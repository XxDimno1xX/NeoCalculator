# NumOS — Emulator Determinism Implementation Roadmap

> Ordered, self-contained tickets for the four spec packs: `NUMOS_EMULATOR_DETERMINISM_AND_FIXTURE_HYGIENE_SPEC.md` (EMUDET), `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` (SANDBOX), `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (CIART), `NUMOS_EMULATOR_HOST_PORTABILITY_SPEC.md` (PORT). Cross-links: EMUDET-01 = SBAR SB-03 (emulator half); FIX-01/02 = GSTATE GS-03 / SCQA QA-05 groundwork; CI-04 = SCQA core-QA-05.
>
> Namespaces: **EMUDET-\*** determinism · **FIX-\*** persistence/fixture hygiene · **CI-\*** artifacts/golden policy · **PORT-\*** host portability. Owner tiers follow the risk-register convention (Human / Fable / Opus / Sonnet). The NativeHal CLI + script grammar is **append-only** (`NUMOS_FABLE_CONTEXT_HEADER.md:67`): every ticket below adds flags/commands, never renames or repurposes.
>
> Audit base: branch `claude/numos-emulator-determinism-specs-69nso8` (2026-07-04). SPEC-ONLY: nothing here is implemented yet.

---

## 0. Recommended order & waves

**Wave 1 — stop the bleeding (independent, small):** FIX-01 → FIX-03 → CI-01 → EMUDET-02.
**Wave 2 — determinism core:** EMUDET-01 → CI-02 → (golden re-bless wave, mask retirement begins) → EMUDET-03 → EMUDET-04.
**Wave 3 — sandbox default + roundtrip:** FIX-02 → FIX-04 → FIX-05.
**Wave 4 — CI evidence & governance:** CI-03 → CI-04 → CI-05 → CI-06.
**Wave 5 — portability:** PORT-03 → PORT-02 → PORT-01 → PORT-05 → PORT-04 → FIX-06.

Rationale: FIX-01/03 + CI-01 remove the tracked-file hazard with tiny diffs and make every later change tree-guarded; EMUDET-01 unlocks unmasked goldens which CI-02's self-diff then enforces; the sandbox default flip (FIX-02) waits until all bundled scripts/CI are proven under explicit flags; portability items are independent and can interleave.

---

## EMUDET-01 — Deterministic clock injection (virtual `time()`)

- **Motivation:** `--deterministic` covers only LVGL ticks; the StatusBar clock reads host wall time, forcing 18 identical masks and blocking any "byte-identical without masks" claim. EMUDET §B.5/§D.1; SBAR SB-03.
- **Evidence:** `StatusBar.cpp:205-225` (`time(nullptr)`); tick swap `NativeHal.cpp:1998`; masks all `4,6,37,13` (`tests/emulator/masks/README.md:36-44`); prior real CI failure from mask under-coverage (`masks/README.md:46-54`).
- **Exact files:** `src/ui/StatusBar.cpp` (clock read via new seam), new `src/hal/Clock.h` (or extension of the HAL boundary), `src/hal/NativeHal.cpp` (implement seam: deterministic → `epoch0 + g_detTick`, else host; `--epoch` flag; banner line), firmware side trivially forwards to `time()` (no behavior change).
- **Desired behavior:** in `--deterministic`, HH:MM is a pure function of frame index and epoch; default epoch 2026-01-01 00:00 (mirrors firmware bootstrap `StatusBar.cpp:125-142`); `--epoch <unix>` overrides.
- **Non-goals:** firmware uptime rendering (SBAR PD-B2, firmware ticket); periodic clock refresh policy (SBAR C.7); mask deletion (separate re-bless wave).
- **Tests:** EMUDET AT-DET-1 (cross-minute run pair, unmasked compare); AT-DET-5 banner grep; existing suites stay green (masks are supersets — a now-constant clock still passes masked compares).
- **Golden impact:** none immediately (masks tolerate constants); enables mask-free re-blessing per CIART §D.3.
- **Firmware impact:** none (seam compiles to `time()` under `ARDUINO`).
- **Rollback:** revert the seam; masks still in tree, everything passes as before.
- **Risk:** low; touches a fragile-listed file's *read* path only.
- **Owner tier:** Opus.

## EMUDET-02 — Deterministic flag hygiene + banner

- **Motivation:** `--run-for-ms` silently mixes wall clock into deterministic runs; unknown flags only warn; no single log line states the full deterministic configuration. EMUDET §B.3/§D.1/AT-DET-5; PORT §E arg-tightening.
- **Evidence:** `NativeHal.cpp:2044,2125-2129` (wall-clock exit); `:1277` (unknown option = warning); `:1999-2006` (partial banner).
- **Exact files:** `src/hal/NativeHal.cpp` (`parseArgs`, main-loop banner).
- **Desired behavior:** `--deterministic` + `--run-for-ms` → exit 2 naming both flags; unknown option → exit 2; one greppable banner: tick source, step-ms, epoch, seed, fs mode + resolved root.
- **Non-goals:** changing `--run-for-ms` semantics in wall-clock mode (Phase-3A smoke keeps it, `emulator-build.yml:132-141`).
- **Tests:** AT-DET-3; CI greps the banner in every gating step (extends `emulator-build.yml:172-173`).
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** revert; prior lenient parsing returns.
- **Risk:** low; exit-code contract 0/2/3/4 untouched (2 reused for its documented "invalid input" class).
- **Owner tier:** Sonnet.

## EMUDET-03 — Seed fence + time-API guard

- **Motivation:** no whitelisted code uses `rand()`/`millis()` today, but the shim makes both one `#include` away from reintroducing nondeterminism invisibly. EMUDET §B.6/§D.2/§D.7.
- **Evidence:** `ArduinoCompat.h:176-180` (`millis` = `SDL_GetTicks`); `rand()` in non-whitelisted apps only (`EquationsApp.cpp:106`, `IntegralApp.cpp:334`, `NeuralLabApp.cpp:372-380`); no `srand` in `src/` [V].
- **Exact files:** `src/hal/NativeHal.cpp` (`srand(1)` in deterministic init + banner field), new `scripts/check-emulator-time-apis.py` (static scan of the whitelist file set from `platformio.ini:195-266` for `time(`, `millis(`, `SDL_GetTicks`, `system_clock`, `rand(` outside HAL seams), `.github/workflows/emulator-build.yml` (guard step alongside the keycode guard `:96-104`).
- **Desired behavior:** guard passes today (proving the whitelist is clean); fails when a future whitelisted TU adopts a raw time/rand API.
- **Non-goals:** virtualizing `millis()` (do it when a whitelisted consumer actually appears); seeding policy for firmware.
- **Tests:** scan `--selftest` (planted bad idiom detected), CI green on current tree.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** drop the CI step.
- **Risk:** low; false positives possible → scan is comment-aware like `check-keycode-digit-patterns.py`.
- **Owner tier:** Sonnet.

## EMUDET-04 — Ignore live SDL input during script replay

- **Motivation:** a windowed scripted run can be perturbed by a human key press; scripted output must be a function of the script alone. EMUDET §D.6.
- **Evidence:** `processSdlEvents()` runs unconditionally each frame (`NativeHal.cpp:2051`); script and live input share `dispatchKey` (`:575`).
- **Exact files:** `src/hal/NativeHal.cpp` (in `processSdlEvents`: when `g_scriptActive && !allowLiveInput`, drop key/text events after logging once; `SDL_QUIT` still honored; new `--allow-live-input` flag).
- **Desired behavior:** AT-DET-4 — windowed scripted run byte-equals headless run.
- **Non-goals:** blocking window events (resize/expose), which don't reach `dispatchKey` anyway.
- **Tests:** AT-DET-4 (manual QA row until an SDL_PushEvent harness exists); regression: full CI suite (headless — unaffected).
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** revert; watch-the-window discipline returns.
- **Risk:** low.
- **Owner tier:** Sonnet.

## FIX-01 — Filesystem sandbox CLI flags (incl. explicit persistent fixture mode)

- **Motivation:** the FS root is a hardcoded CWD-relative `./emulator_data` — the single cause of tracked-file mutation, cross-run leakage, and CWD fragility. SANDBOX §B.
- **Evidence:** `FileSystem.cpp:38,44-49`; boot load `NativeHal.cpp:2171-2179`; STO write `CalculationApp.cpp:894-901`.
- **Exact files:** `src/hal/FileSystem.{h,cpp}` (settable root), `src/hal/NativeHal.cpp` (flags `--fs-sandbox`, `--fs-sandbox-dir <p>`, `--fixture-dir <p>`, `--fs-root <p>`; resolution precedence SANDBOX B.1; temp-dir creation/cleanup B.2; banner field), `docs/emulator-sdl2-quickstart.md` (table), new `tests/emulator/fixtures/README.md` (convention B.4).
- **Desired behavior:** flags work, defaults unchanged (inert without flags — that's FIX-02); `--fixture-dir` copies read-only fixture into a fresh sandbox; conflicting flags exit 2; sandbox retained + path printed on nonzero exit.
- **Non-goals:** flipping any default; settings persistence itself (GS-02); NVS emulation.
- **Tests:** AT-FIX-2 (fixture seeding + fixture-unchanged SHA); unit-style runner checks for each flag; full CI suite green with no flags.
- **Golden impact:** none. **Firmware impact:** none (`hal/` is `!ARDUINO`).
- **Rollback:** flags ignored / revert; hardcoded constant returns.
- **Risk:** low-medium (touches every persistence open); mitigated by the inert default and AT-FIX suite.
- **Owner tier:** Opus.

## FIX-02 — Default clean temp sandbox for scripted/deterministic runs

- **Motivation:** determinism-by-default: `--script`/`--deterministic` runs must not need a flag to be isolated. EMUDET §G.3; SANDBOX §B.1(5).
- **Evidence:** every CI invocation is `--headless --deterministic --script … ` (`emulator-build.yml:167,210,270,320,387,441,507`, generator `generate-emulator-candidates.py:296-304`) — all become sandboxed by this flip.
- **Exact files:** `src/hal/NativeHal.cpp` (default resolution), `docs/emulator-sdl2-quickstart.md`, `.github/workflows/emulator-build.yml` (remove any now-redundant explicit flags; keep them where documentation value is higher).
- **Desired behavior:** scripted+deterministic ⇒ fresh temp sandbox unless an explicit fs flag was given; interactive ⇒ `emulator_data` as today.
- **Non-goals:** changing interactive behavior.
- **Tests:** AT-FIX-1 (STO run leaves tree clean, no cross-run leakage; add `assert_variable x 0` bookend script); CI-01 guard goes green-by-construction.
- **Golden impact:** none expected — no bundled visual script depends on pre-seeded variables [V: candidate scripts store nothing before their screenshots]; self-diff (CI-02) verifies.
- **Firmware impact:** none.
- **Rollback:** revert default; explicit flags keep working.
- **Risk:** medium (behavioral default change) — staged after FIX-01 soak.
- **Owner tier:** Opus.

## FIX-03 — `emulator_data/vars.dat` git hygiene

- **Motivation:** a git-tracked file that test runs rewrite is a standing dirty-tree + state-leak + poisoned-commit hazard. SANDBOX §A.5/§E.
- **Evidence:** `git ls-files emulator_data/` → `emulator_data/vars.dat`; content all-zeros (379 B, header per `VariableManager.h:139,148,154-155`); rewritten via `CalculationApp.cpp:900`; CI runs the writer (`emulator-build.yml:343`); `.gitignore` lacks `emulator_data/` [V].
- **Exact files:** `.gitignore` (+`emulator_data/`), `git rm --cached emulator_data/vars.dat`, `tests/emulator/fixtures/README.md` (where a seeded state now lives if ever needed).
- **Desired behavior:** no tracked runtime-mutable file; local `emulator_data/` becomes untracked scratch; committed zero-state loses nothing (equals constructor defaults, `VariableManager.cpp:54-56,112-116`).
- **Non-goals:** deleting the directory concept; changing load/save code.
- **Tests:** `git status` clean after STO run even pre-FIX-02 (file untracked); fresh-clone boot works (dir auto-created, `FileSystem.cpp:52-56`).
- **Golden impact:** none (no golden depends on variable state [V]).
- **Firmware impact:** none.
- **Rollback:** re-add the file.
- **Risk:** low.
- **Owner tier:** Sonnet.

## FIX-04 — Two-process persistence roundtrip test (vars now, settings post-GS-02)

- **Motivation:** persistence has zero cross-process verification; GS-02's settings store must not ship untested. SANDBOX §C; SCQA §E.3 (QA-05).
- **Evidence:** save path `VariableManager.cpp:169-190`, load path `:196-226`; single-process STO test only (`emulator-build.yml:343`); one-process-per-script model (`generate-emulator-candidates.py:296-310`).
- **Exact files:** new `tests/emulator/scripts/persist_store_x4.numos`, `persist_assert_x4.numos`, `persist_assert_x0.numos`; `.github/workflows/emulator-build.yml` (runner step per SANDBOX §C, incl. between-process sha256 logging + failure bundle upload); optional local wrapper `scripts/run-persistence-roundtrip.sh`.
- **Desired behavior:** A writes via `--fs-sandbox-dir $T`; B asserts in `$T`; C asserts defaults in a fresh sandbox; settings variant added with `--persist-settings` when GS-02 lands.
- **Non-goals:** firmware/NVS roundtrip (HIL row); crash-during-write atomicity (firmware/RT-13).
- **Tests:** is one (AT-FIX-3); negative check: corrupt `$T/vars.dat` between A and B → B exits 0 with defaults + `[FS]` line (bridges to FIX-05).
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** delete step + scripts.
- **Risk:** low. Depends on FIX-01.
- **Owner tier:** Sonnet.

## FIX-05 — Corruption-matrix runner tests

- **Motivation:** loader behavior on bad files is undefined-by-test; RT-13/GS-02 quarantine semantics need executable oracles. SANDBOX §D.
- **Evidence:** magic/short-read handling `VariableManager.cpp:202-222`; no CRC (`:19-25`); firmware 1-byte stub `SystemApp.cpp:145-148`; emulator `begin(true)` ignored (`FileSystem.cpp:52-56`).
- **Exact files:** `.github/workflows/emulator-build.yml` (matrix step planting: corrupt magic / truncated record / missing / 1-byte stub / future-version), reusing FIX-04's sandbox mechanics; new assert-only `.numos` fixtures.
- **Desired behavior:** every case boots (exit 0), asserts defaults, greps the `[FS]` diagnostic, and the planted file is untouched or quarantined — per SANDBOX §D table (today-column first; tighten to the RT-13 column when that lands).
- **Non-goals:** implementing CRC/quarantine (RT-13/GS-02 own the code).
- **Tests:** is one.
- **Golden impact:** none. **Firmware impact:** none (documents parity, F-table).
- **Rollback:** delete step.
- **Risk:** low. Depends on FIX-01, FIX-04.
- **Owner tier:** Sonnet.

## FIX-06 — Emulator/firmware persistence parity register + stub cleanup decision

- **Motivation:** divergences (backend, NVS absence, firmware stub file) must stay enumerated, not tribal. SANDBOX §F.
- **Evidence:** `SystemApp.cpp:145-148` stub; NVS gaps (`platformio.ini:227-229`; GSTATE §A.5).
- **Exact files:** SANDBOX §A.4/§F tables (living register); one firmware line (`SystemApp.cpp:145-148` removal) *proposed to* RT-13, not done here.
- **Desired behavior:** every new store adds a register row; stub divergence resolved (delete or replicate — recommend delete, it creates a magic-failing file).
- **Non-goals:** any emulator code change.
- **Tests:** review checklist item.
- **Golden impact:** none. **Firmware impact:** one-line, deferred to RT-13.
- **Rollback:** n/a (doc).
- **Risk:** none.
- **Owner tier:** Sonnet (doc), Human (ratify stub deletion).

## CI-01 — Dirty-tree CI guard

- **Motivation:** the tree-mutation class must be structurally impossible to reintroduce. CIART §H; EMUDET §E.3.
- **Evidence:** STO step dirties tracked file today (`emulator-build.yml:343` + `CalculationApp.cpp:900`); no guard exists [V].
- **Exact files:** `.github/workflows/emulator-build.yml` (final `if: always()` step per CIART §H).
- **Desired behavior:** `git diff --exit-code` + untracked check (excluding `out/`) after all suites; red job on any leak.
- **Non-goals:** guarding the firmware workflow (no runtime steps there).
- **Tests:** AT-CI-1 incl. a one-off scratch-branch proof that the guard bites pre-sandbox.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** remove step.
- **Risk:** low; ordering: land with/after FIX-01 (or FIX-03 alone also makes it green — untracked `emulator_data` is filtered only if we scope the untracked check; decide in review: recommended scope is *fail* on untracked `emulator_data/` mutations too once FIX-02 lands).
- **Owner tier:** Sonnet.

## CI-02 — Deterministic self-diff CI phase (all candidates)

- **Motivation:** run-twice reproducibility is proven for one fixture only; every other screen's determinism is assumed. CIART §G.4/§J.2.
- **Evidence:** `emulator-build.yml:228-237` (single fixture); candidate list `generate-emulator-candidates.py:39-143`.
- **Exact files:** `.github/workflows/emulator-build.yml` (Phase B step: generate into `out/run1` and `out/run2`, unmasked byte-compare, `out/selfdiff/report.txt`, upload); optionally a `--runs 2` mode in `generate-emulator-candidates.py`.
- **Desired behavior:** any nondeterministic pixel fails with both PPMs + `--write-diff` image uploaded. Pre-EMUDET-01: clock rect may differ across a minute rollover *within* the job — mitigate by comparing masked pre-EMUDET-01, unmasked after (the step flips with EMUDET-01; sequencing note in wave 2).
- **Non-goals:** cross-OS comparison (PORT-05).
- **Tests:** AT-DET-2; deliberate-nondeterminism canary once (scratch branch, e.g. reintroduce wall tick) to prove it fails.
- **Golden impact:** none (self-compare). **Firmware impact:** none.
- **Rollback:** remove step.
- **Risk:** low-medium (runtime cost ~2× candidate generation; budget note CIART §J).
- **Owner tier:** Sonnet.

## CI-03 — Visual diff artifact generation + failure bundles

- **Motivation:** golden mismatches ship no image evidence; reviewers must repro locally. CIART §F/§G.1.
- **Evidence:** compare step without `--write-diff` (`emulator-build.yml:589,593`); comparator supports it (`compare-ppm.py:263-294,303-308`).
- **Exact files:** `.github/workflows/emulator-build.yml` (add `--write-diff out/diffs/<stem>.diff.ppm` on compare, upload `numos-emulator-golden-diffs`; per-step failure bundles incl. retained sandboxes).
- **Desired behavior:** every red visual/assert step is diagnosable from artifacts alone.
- **Non-goals:** PNG conversion (PPM is the repo's dependency-free contract; viewers exist, `golden/README.md:44-45`).
- **Tests:** scratch-branch forced mismatch produces the diff artifact.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** remove flags.
- **Risk:** low.
- **Owner tier:** Sonnet.

## CI-04 — Assert-failure screenshot capture

- **Motivation:** exit-4 failures are text-only; pixel state at failure is lost. CIART §F/§G.2; = SCQA core-QA-05 (one implementation).
- **Evidence:** `assertFail` sets exit 4 + quits (`NativeHal.cpp:1682-1688`); `saveScreenshotPPM` is renderer-independent (`:1286-1315`).
- **Exact files:** `src/hal/NativeHal.cpp` (on first assert failure, write `<script-stem>.assertfail.ppm` into `out/` — or the sandbox artifact dir — before `g_quit`; log the path), `.github/workflows/emulator-build.yml` (upload pattern in every suite's artifact step).
- **Desired behavior:** every FAIL line is accompanied by the frame it failed on.
- **Non-goals:** screenshots on exit 2/3 (no meaningful frame yet / write path already failing).
- **Tests:** Phase D deliberate-failure run asserts the file exists and is a valid 320×240 P6.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** revert.
- **Risk:** low; append-only behavior (new output on a path that previously produced none).
- **Owner tier:** Sonnet.

## CI-05 — Golden/mask governance mechanization (pending inventory, mask lint, negative controls)

- **Motivation:** warn-only candidates age invisibly; mask discipline is prose; comparator correctness is untested in CI. CIART §C/§D/§E/§J.4.
- **Evidence:** 6 golden-less stems warn (`emulator-build.yml:602-604` + golden/candidate inventory [V CIART §B]); mask rules are README-only (`masks/README.md:88-106`); no negative-control step [V].
- **Exact files:** new `tests/emulator/golden/PENDING.md`; `.github/workflows/emulator-build.yml` (inventory summary + `K>0` failure; mask-size/region lint via a small stdlib script `scripts/check-emulator-masks.py`; Phase D negative controls per CIART §E.3).
- **Desired behavior:** every stem classified gated/pending/assert-only; oversized or body-region masks fail; comparator exit codes 0/1/2 each exercised every run.
- **Non-goals:** auto-promotion (never); changing the warn-not-fail semantics for *listed* pending stems.
- **Tests:** is one; scratch-branch: unlisted candidate → red; 2%-frame mask → red.
- **Golden impact:** none to bytes; process tightens.
- **Firmware impact:** none.
- **Rollback:** remove steps/lint.
- **Risk:** low.
- **Owner tier:** Sonnet.

## CI-06 — Pin exact LVGL version

- **Motivation:** `^9.2.0` float means two machines (or two CI days) can build different renderers under byte-exact goldens — the "goldens break for no reason" class. CIART §I; risk REN-4.
- **Evidence:** `platformio.ini:107,178`; header notes 9.5.0 currently resolves (`NUMOS_FABLE_CONTEXT_HEADER.md:7`).
- **Exact files:** `platformio.ini` (both `lib_deps` entries → exact `lvgl/lvgl@9.5.0` or the verified current), full re-candidate + compare run.
- **Desired behavior:** LVGL version is a build constant; upgrades become deliberate re-bless waves.
- **Non-goals:** upgrading LVGL.
- **Tests:** CI green with identical goldens post-pin (proves the pin matched reality).
- **Golden impact:** none if pinned to the resolving version; otherwise a full re-bless wave (that would itself be the evidence the float was dangerous).
- **Firmware impact:** same pin applies to the firmware env — needs one firmware compile check (P-09 job or manual).
- **Rollback:** restore `^`.
- **Risk:** low-medium (firmware build coupling).
- **Owner tier:** Sonnet (change), Human (ratify version).

## PORT-01 — macOS run verification workflow

- **Motivation:** macOS is documented but unproven; "expected to work" is not evidence. PORT §A/§G.
- **Evidence:** `docs/emulator-sdl2-quickstart.md:325-329`; single-OS CI (`emulator-build.yml:47-50`).
- **Exact files:** new `.github/workflows/emulator-macos.yml` (`macos-14`; brew sdl2 pkg-config platformio; `PLATFORMIO_BUILD_DIR=.pio/build`; deps check; build; headless deterministic smoke + `calc_1_plus_2` semantic run; `workflow_dispatch` + weekly cron, not per-push).
- **Desired behavior:** a green run converts the quickstart caveat into a dated fact; failures are portability signal, never blocking `main` (non-required check).
- **Non-goals:** macOS golden gating (PORT-05 first measures drift).
- **Tests:** is one.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** delete workflow.
- **Risk:** low (isolated job; macOS runner cost is why it's cron/dispatch).
- **Owner tier:** Sonnet.

## PORT-02 — Host portability docs/task cleanup

- **Motivation:** the quickstart/tasks must absorb this series' new flags and keep the existing traps documented (PIO Debug, Electron Main, `C:/.piobuild`, `pio run` bare). PORT §C/§D/§F.4.
- **Evidence:** `quickstart:81-97,92-94,98-133,316-320`; `tasks.json:6`; no `default_envs` (`platformio.ini:5-7`).
- **Exact files:** `docs/emulator-sdl2-quickstart.md`, `.vscode/tasks.json` (non-Windows `options.env` `PLATFORMIO_BUILD_DIR`), `docs/es/` mirror where applicable.
- **Desired behavior:** every doc command copy-pastes clean on all three OSes; sandbox flags + banner documented; macOS caveat removed only when PORT-01 is green.
- **Non-goals:** platformio.ini changes (PORT-04).
- **Tests:** doc review; a fresh-clone Linux walkthrough of every quickstart command block.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** n/a.
- **Risk:** none.
- **Owner tier:** Sonnet.

## PORT-03 — `check-emulator-deps.py` improvements

- **Motivation:** the diagnostic should see everything the build and runtime see, and emit a paste-ready issue report. PORT §B (macOS dylib gap) / §F.3.
- **Evidence:** current checks `check-emulator-deps.py:52-152`; Linux-only runtime preflight lives in `run-emulator-linux.sh:69-97`; report shape prescribed at `quickstart:355-393`.
- **Exact files:** `scripts/check-emulator-deps.py` (macOS `otool -L`, Linux `ldconfig` runtime check, `--report` block, bare-`pio run` warning, FS-root echo post-FIX-01).
- **Desired behavior:** exit 0/1 semantics unchanged; new checks additive; `--report` prints §F.1 fields verbatim.
- **Non-goals:** making the script a build dependency (stays optional/read-only, `check-emulator-deps.py:6-13`).
- **Tests:** run on CI Linux (already a step, `emulator-build.yml:77-78`) + PORT-01 macOS job.
- **Golden impact:** none. **Firmware impact:** none.
- **Rollback:** revert.
- **Risk:** none.
- **Owner tier:** Sonnet.

## PORT-04 — `build_dir` portability decision

- **Motivation:** the Windows MAX_PATH workaround creates literal `./C:/` dirs on other OSes; the mitigation is env-var folklore. PORT §D (options analysis).
- **Evidence:** `platformio.ini:6`; `quickstart:98-133`; CI overrides (`emulator-build.yml:54-55`, `compile-and-release.yml:15-19,29-39`); risk EMU-3.
- **Exact files:** decision record in PORT §D; then `.vscode/tasks.json` env (option 1, now) and — only after Windows measurement — `platformio.ini` (option 2: short relative `build_dir` + `default_envs`).
- **Desired behavior:** option 1 immediately (no ini change, docs/tasks only); option 2 evaluated on the maintainer's machine with the deep Giac include tree before any ini edit.
- **Non-goals:** breaking the Windows firmware build (the whole reason the line exists).
- **Tests:** Windows firmware build green post-change (Human); Linux fresh clone produces no `C:/` dir under option 2.
- **Golden impact:** none. **Firmware impact:** build-system only — Human-verified.
- **Rollback:** restore `build_dir` line.
- **Risk:** medium if option 2 proceeds unmeasured — hence the measurement gate.
- **Owner tier:** Human (decision + Windows verification), Sonnet (mechanics).

## PORT-05 — Cross-OS pixel drift measurement

- **Motivation:** whether goldens are portable across libm/compiler is unknown; claims either way are currently unfounded. CIART §I; PORT §E.
- **Evidence:** Linux-reference blessing rule (`golden/README.md:33-35`); double-math Grapher pipeline (`platformio.ini:236-240`).
- **Exact files:** extend PORT-01's macOS workflow (+ optionally a `windows-latest` variant) to run the full candidate set and `compare-ppm.py` each against the Linux goldens (masked), publishing a drift report artifact; if drift is found, a follow-up decides `-ffp-contract=off`/`-fno-fast-math`-class flags on `[env:emulator_pc]` vs per-OS non-gating status.
- **Desired behavior:** a dated, per-stem drift matrix; CIART §I position upgraded from "unmeasured" to facts.
- **Non-goals:** making non-Linux comparisons gating.
- **Tests:** is one.
- **Golden impact:** none directly; may motivate an FP-flag + re-bless wave.
- **Firmware impact:** none (emulator env flags only).
- **Rollback:** delete steps.
- **Risk:** low.
- **Owner tier:** Sonnet (harness), Fable (drift analysis if found).

---

## Ticket → prior-spec cross-reference

| This series | Equals / implements | Source |
|---|---|---|
| EMUDET-01 | SB-03 (emulator half) | SBAR §E/§H |
| FIX-01/02 | GS-03 / QA-05 groundwork (`--fs-sandbox`) | GSTATE §F.1/§F.3; SCQA §E.3-4 |
| FIX-03 | GSTATE §F.3 tracked-fixture rule | GSTATE §F.3 |
| FIX-04 | QA-05 roundtrip | SCQA §E.3 |
| CI-01 | GSTATE §F.3 tree-clean guard | GSTATE §F.3; SCQA §E.4 |
| CI-02 | SCQA §F deterministic idle/self-diff gate | SCQA §F |
| CI-04 | core-QA-05 / SCQA §G.6 failure screenshots | SCQA §G |
| PORT-04 | P-07/P-08 build_dir portability item | `NUMOS_FABLE_CONTEXT_HEADER.md:81`; risk EMU-3 |

*End of implementation roadmap.*
