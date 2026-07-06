# NumOS WASM — Implementation Roadmap and Prompts

Status: SPEC (no implementation). Part 6 of the WASM port pack.
Ticket namespaces (all new, collision-checked against the docs/specs census): `WASM-ARCH-*`, `WASM-BUILD-*`, `WASM-RUN-*`, `WASM-UI-*`, `WASM-FS-*`, `WASM-TEST-*`, `WASM-DEPLOY-*`, `WASM-PERF-*`.
Owner tiers follow the determinism-roadmap convention: **Human** (decisions/blessing), **Fable** (spec/adversarial review), **Opus** (complex implementation), **Sonnet** (mechanical implementation).

Common ticket defaults (stated once; every ticket inherits unless overridden):
- **Firmware impact:** none — no ticket may touch `esp32s3_n16r8` sources, flags, or behavior; app sources are edited by zero tickets in this pack.
- **Golden impact:** none unless stated; **no desktop golden may ever be re-blessed by a WASM ticket** (a diff means the refactor changed behavior — fix the code).
- **Persistence impact:** none unless stated.
- **Rollback:** every ticket lands behind either a new build target or `#ifdef __EMSCRIPTEN__`/new files; rollback = revert the commit; desktop emulator CI is the regression net.
- **Evidence anchors:** [NativeHal.cpp](src/hal/NativeHal.cpp), [platformio.ini:153-266](platformio.ini#L153-L266), [emulator-build.yml](/.github/workflows/emulator-build.yml) as cited in companion docs 1–5.

---

## Phase 0 — Feasibility, no source fork (W0 groundwork)

### WASM-BUILD-01 — Emscripten feasibility audit & compile-blocker inventory
- **Motivation:** know every compile/link blocker and size number before designing around guesses.
- **Evidence:** allowlist [platformio.ini:195-266](platformio.ini#L195-L266); exceptions unknown (WASM-OD-3); font-array size unknown.
- **Files:** new `web/feasibility/` scratch CMakeLists + report `docs/specs/NUMOS_WASM_FEASIBILITY_REPORT.md`; touches NO existing files.
- **Desired:** attempt `emcmake` compile of the exact allowlist + LVGL with the emulator defines; produce: blocker list (per-file), `throw/try/longjmp` audit, `long double` audit, size-by-section (symbol map), recommendation for OD-3.
- **Non-goals:** making it link/run; editing any source.
- **Dependencies:** none. **Tests:** report reviewed (Fable). **Risk:** low. **Owner:** Opus (audit) + Fable (review).

### WASM-ARCH-01 — Define `IEmulatorPlatform` boundary (header only)
- **Motivation:** freeze the seam before the refactor; the interface is the anti-fork instrument.
- **Evidence:** platform-touching sites enumerated in companion doc 2 §A/§E.
- **Files:** new `src/hal/IEmulatorPlatform.h` (+ doc comments mapping each method to current NativeHal lines).
- **Desired:** compilable header matching companion doc 2 §D; no callers yet.
- **Non-goals:** moving any code. **Dependencies:** none. **Tests:** compiles in both envs (header-only include test). **Risk:** low. **Owner:** Fable (design) / Sonnet (typing).

### WASM-BUILD-02 — Source-list generator (`emit-emulator-sources.py`)
- **Motivation:** one authority for what compiles (WASM-OD-6); prevents allowlist drift between PlatformIO and CMake.
- **Evidence:** allowlist format [platformio.ini:195-266](platformio.ini#L195-L266).
- **Files:** new `scripts/emit-emulator-sources.py`, generated `web/emulator_sources.cmake`.
- **Desired:** parse `build_src_filter` of `[env:emulator_pc]` → emit CMake list; `--check` mode exits nonzero on mismatch (CI hook later).
- **Non-goals:** general PlatformIO parsing. **Dependencies:** none. **Tests:** self-test comparing against a fixture ini snippet; run against real ini. **Risk:** low. **Owner:** Sonnet.

## Phase 1 — First boot in browser (W1)

### WASM-ARCH-02 — Split NativeHal: `EmuCore` + `PlatformSdl` (behavior-frozen refactor)
- **Motivation:** the load-bearing refactor; everything else stacks on it.
- **Evidence:** monolith structure companion doc 2 §A; CI grep strings [emulator-build.yml:141](/.github/workflows/emulator-build.yml#L141),[173](/.github/workflows/emulator-build.yml#L173).
- **Files:** `src/hal/NativeHal.cpp` (shrinks to main+wiring), new `src/hal/EmuCore.{h,cpp}`, `src/hal/PlatformSdl.cpp`; `platformio.ini` `build_src_filter` gains the new hal files (still `+<hal/*>` — likely **zero ini change**; verify).
- **Desired:** byte-identical behavior: all `.numos` suites green, self-diff identical, **all 19 goldens byte-identical without re-bless**, `--help` text and log strings unchanged, exit codes unchanged.
- **Non-goals:** any web code; any grammar/flag change; fixing D6/D10 divergences (do NOT sneak fixes in).
- **Dependencies:** WASM-ARCH-01. **Tests:** the entire existing emulator CI, run locally + in a PR. **Golden impact:** must be zero (hard gate). **Risk:** medium-high (touches everything); mitigated by mechanical-move discipline. **Owner:** Opus, Fable adversarial review mandatory.

### WASM-BUILD-03 — CMake/emsdk build target
- **Motivation:** produce `numos-emu.{wasm,js}`.
- **Files:** new `web/CMakeLists.txt`, `web/toolchain-notes.md`; consumes `web/emulator_sources.cmake`.
- **Desired:** release+debug configs per companion doc 4 §C (incl. `-ffp-contract=off`, `LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB`, exceptions per WASM-BUILD-01 outcome, `-sUSE_SDL=2` for now); links clean.
- **Dependencies:** WASM-BUILD-01/02, WASM-ARCH-02. **Tests:** artifact exists; `node`-free (ENVIRONMENT=web) load test in Playwright later. **Risk:** medium. **Owner:** Opus.

### WASM-RUN-01 — `PlatformWeb` backend + browser main loop (Emscripten-SDL flavor)
- **Motivation:** first boot.
- **Evidence:** loop order companion doc 2 §C (normative).
- **Files:** new `src/hal/PlatformWeb.cpp` (guarded `__EMSCRIPTEN__`), exported ABI subset (`numos_init/_frame/_run_frames/_fb_ptr/_fb_dirty/_exit_code`).
- **Desired:** `emscripten_set_main_loop` interactive mode; frame-step order identical to desktop; splash→launcher boots.
- **Non-goals:** input beyond arrows/enter via SDL shim; persistence; shell polish.
- **Dependencies:** WASM-BUILD-03. **Tests:** manual boot + scripted W1 smoke (WASM-TEST-01). **Risk:** medium. **Owner:** Opus.

### WASM-UI-01 — Minimal HTML shell (canvas + loader + error card)
- **Files:** new `web/index.html`, `web/numos-shell.js`, `web/shell.css`.
- **Desired:** loads module, blits framebuffer (RGB565→RGBA per companion doc 2 §F), integer scaling, `image-rendering: pixelated`, loading + error states; zero third-party deps.
- **Dependencies:** WASM-RUN-01. **Tests:** W1 smoke; console-error-free. **Risk:** low. **Owner:** Sonnet.

### WASM-TEST-01 — Playwright harness skeleton + W1 boot smoke
- **Files:** new `tests/wasm/` (package.json pinned, `boot.spec.ts`, static server util).
- **Desired:** headless Chromium+Firefox: page loads, `numosReady`, first frame non-blank (read `numos_fb_ptr` bytes), zero console errors.
- **Dependencies:** WASM-UI-01. **Risk:** low. **Owner:** Sonnet.

## Phase 2 — Deterministic script parity (W2–W3, plus W4 seed)

### WASM-RUN-02 — Deterministic mode + script runner exports
- **Motivation:** the port's testability spine.
- **Evidence:** det tick [NativeHal.cpp:2275-2277](src/hal/NativeHal.cpp#L2275-L2277); scheduler [1803-1807](src/hal/NativeHal.cpp#L1803-L1807); fail-fast [2091-2093](src/hal/NativeHal.cpp#L2091-L2093).
- **Files:** `EmuCore` (add `loadScriptFromString` overload — append-only), `PlatformWeb.cpp` (`numos_load_script`, batched `numos_run_frames`, screenshot store + `numos_screenshot_ppm`), harness plumbing.
- **Desired:** shared `.numos` scripts execute with identical exit codes/PASS-FAIL lines; screenshots harvested as byte-exact PPMs; live input suppressed during scripts (EMUDET-04 semantics at `pollInput`).
- **Non-goals:** golden blessing; new grammar.
- **Dependencies:** WASM-RUN-01. **Tests:** WASM-TEST-02. **Risk:** medium. **Owner:** Opus.

### WASM-TEST-02 — Shared semantic suite + self-diff in browser CI
- **Files:** `tests/wasm/scripts.spec.ts`, shared stem manifest `tests/emulator/candidates.json` **[proposed]** + generator update to consume it (`generate-emulator-candidates.py` reads, list unchanged).
- **Desired:** calc-semantic set, input 9A, menu 9B, grapher 9F/10 suites (incl. the inverted negative control, exit 4 expected) green; all candidate stems self-diff byte-identical.
- **Golden impact:** none yet (candidates only). **Persistence impact:** sandbox mode only.
- **Dependencies:** WASM-RUN-02. **Risk:** medium (first real parity contact). **Owner:** Opus.

### WASM-TEST-03 — SDL-vs-WASM drift report (measurement, non-gating)
- **Desired:** per-stem classification vs Linux SDL goldens (identical / masked-identical / diverged), published CI artifact + summary table checked into `docs/specs/NUMOS_WASM_PARITY_REPORT.md` per milestone; feeds WASM-OD-1.
- **Dependencies:** WASM-TEST-02. **Risk:** low. **Owner:** Sonnet + Human review.

### WASM-TEST-04 — WASM golden set: bless wave 1
- **Desired:** `tests/emulator/golden-wasm/` + reuse masks; pending→blessed lifecycle per app-QA plan; gate in wasm CI (missing warns / mismatch fails, mirroring [emulator-build.yml:682-714](/.github/workflows/emulator-build.yml#L682-L714)).
- **Golden impact:** creates the wasm set; desktop set untouched. **Dependencies:** WASM-TEST-03; **Human blesses.** **Risk:** low. **Owner:** Human + Sonnet tooling.

## Phase 3 — Browser product shell (W7 groundwork)

### WASM-UI-02 — Virtual keypad + keyboard input bridge (script-name vocabulary)
- **Evidence:** name table [NativeHal.cpp:456-565](src/hal/NativeHal.cpp#L456-L565); dual-tier input [344-349](src/hal/NativeHal.cpp#L344-L349).
- **Files:** shell JS/CSS; `PlatformWeb.cpp` (`numos_inject_key`, `numos_modifier_state`); generated `web/keymap.json` (+ its generator script from the C++ table — single-source).
- **Desired:** full physical-keyboard map, keypad with press/release/repeat, focus model + indicator, modifier latching display per companion doc 3 §B.
- **Dependencies:** WASM-RUN-02. **Tests:** WASM-TEST-05 input suite. **Risk:** medium. **Owner:** Opus.

### WASM-RUN-03 — Canvas-native browser backend; drop `-sUSE_SDL=2`
- **Desired:** replace Emscripten-SDL presentation/input with direct canvas + listeners behind the same interface; re-run full W3 suite + drift report — **zero classification changes allowed**; measure size win.
- **Dependencies:** WASM-UI-02. **Risk:** medium. **Owner:** Opus. **Rollback:** flag to rebuild with SDL flavor until deleted.

### WASM-UI-03 — Product shell: reset, help/keymap overlay, screenshot download, fullscreen, modes (`?mode=`, `?app=`)
- **Dependencies:** WASM-UI-02. **Risk:** low. **Owner:** Sonnet.

### WASM-TEST-05 — Browser-specific input/UX test suite
- **Desired:** companion doc 5 §D items 1–5 (focus, keypad, touch, resize invariance, background tab).
- **Dependencies:** WASM-UI-02/03. **Risk:** low. **Owner:** Sonnet.

## Phase 4 — Persistence (W6)

### WASM-FS-01 — FS modes: sandbox/fixture mounts + init-config plumbing
- **Evidence:** hardcoded root [FileSystem.cpp:38](src/hal/FileSystem.cpp#L38); sandbox-spec flag semantics.
- **Files:** `PlatformWeb.cpp` (mount setup), `EmuCore` init (root prefix hook — the FIX-01 seam implemented at the platform boundary; coordinate with FIX-01 if it lands first so flag/mode names match).
- **Desired:** sandbox default; fixture bundle load; mode/script conflict validation (config error, exit-2 style).
- **Persistence impact:** defines it. **Dependencies:** WASM-RUN-02. **Risk:** medium. **Owner:** Opus.

### WASM-FS-02 — Persistent mode (IDBFS) + storage UI + export/import
- **Desired:** IDBFS mount, debounced `syncfs` + pagehide flush, erase-data flow, JSON state envelope export/import (companion doc 3 §C/§E).
- **Dependencies:** WASM-FS-01, WASM-UI-03. **Risk:** medium (async storage). **Owner:** Opus.

### WASM-TEST-06 — Storage suite + roundtrips + cross-engine byte-equality measurement
- **Desired:** two-instantiation roundtrip, reload-survival (persistent), erase, sandbox-isolation tests; plus the cross-engine (Chromium vs Firefox) candidate byte-compare — promote to gate if identical.
- **Dependencies:** WASM-FS-02. **Risk:** low. **Owner:** Sonnet.

## Phase 5 — CI & deployment (W4 gate + G4)

### WASM-DEPLOY-01 — `wasm-build.yml` workflow (build, smoke, suites, goldens, budgets, artifacts, dirty-guard)
- **Desired:** companion doc 4 §F jobs; PR gate wiring; failure bundles (CI-03/04 classes).
- **Dependencies:** WASM-TEST-02/04/05. **Risk:** medium. **Owner:** Opus.

### WASM-DEPLOY-02 — GitHub Pages deploy: versioned dirs, `latest/`, build-info, rollback runbook
- **Desired:** companion doc 4 §E/§I; manual `workflow_dispatch` publish; post-deploy live smoke.
- **Dependencies:** WASM-DEPLOY-01. **Risk:** low. **Owner:** Sonnet + Human (first publish).

### WASM-PERF-01 — Size/perf measurement & budget calibration
- **Desired:** symbol-map size report, `-Oz`/emmalloc/closure experiments, boot+frame-time metrics job; recalibrate §G budgets with real numbers (spec update PR).
- **Dependencies:** WASM-BUILD-03. **Risk:** low. **Owner:** Sonnet.

## Phase 6 — PWA & public demo (W7/G5)

### WASM-UI-04 — PWA: manifest, service worker, offline, update toast
- **Dependencies:** WASM-DEPLOY-02. **Risk:** low. **Owner:** Sonnet.

### WASM-TEST-07 — Cross-browser matrix execution + public-demo gate
- **Desired:** run companion doc 3 §H matrix incl. manual Safari/iOS passes; record results in the spec; execute release checklist (prompt 6). **Owner:** Human + Sonnet.

### WASM-DEPLOY-03 — Public launch: docs page, README quickstart, embed mode for NumOS Lab
- **Owner:** Sonnet + Human.

**Recommended order (dependency spine):** BUILD-01 → ARCH-01 → BUILD-02 → **ARCH-02** → BUILD-03 → RUN-01 → UI-01 → TEST-01 → RUN-02 → TEST-02 → TEST-03 → UI-02 → RUN-03 → UI-03 → TEST-05 → TEST-04 → FS-01 → FS-02 → TEST-06 → DEPLOY-01 → PERF-01 → DEPLOY-02 → UI-04 → TEST-07 → DEPLOY-03.

---

## Ready-to-paste prompts

### Prompt 1 — Fable/Opus: WASM feasibility audit (WASM-BUILD-01)

```
You are auditing WebAssembly build feasibility for the NumOS emulator. SPEC pack:
docs/specs/NUMOS_WEBASSEMBLY_PORT_ARCHITECTURE_SPEC.md and NUMOS_WASM_BUILD_DEPLOY_AND_CI_SPEC.md — read both first, plus platformio.ini [env:emulator_pc].

TASK: Attempt to compile (not link, then link) the exact emulator_pc source allowlist
(platformio.ini:195-266) plus LVGL with emcc, using the defines from
NUMOS_WASM_BUILD_DEPLOY_AND_CI_SPEC.md §C. Produce
docs/specs/NUMOS_WASM_FEASIBILITY_REPORT.md with: per-file blocker list (exact errors),
throw/try/catch/longjmp audit over the allowlist, long double audit, symbol-map size
breakdown, and a recommendation for the exceptions decision WASM-OD-3.

ALLOWED FILES: create web/feasibility/** and docs/specs/NUMOS_WASM_FEASIBILITY_REPORT.md only.
FORBIDDEN: any edit to src/**, tests/**, scripts/**, platformio.ini, .github/**, lv_conf.h, goldens/masks. Do not commit.
COMMANDS: emsdk install/activate (pinned latest LTS), emcmake/emmake or direct emcc; grep/audit commands.
STOP CONDITIONS: stop if you find yourself wanting to edit any source file — record the needed edit in the report instead. Stop after the report is written.
ACCEPTANCE: report exists; every blocker has file:line + exact compiler error; size table totals stated compressed+uncompressed; OD-3 recommendation argued from the audit data.
FINAL REPORT: blockers count, top 5 size contributors, exceptions verdict, go/no-go for Phase 1.
```

### Prompt 2 — Opus/Sonnet: first browser boot prototype (WASM-ARCH-02 + BUILD-03 + RUN-01 + UI-01)

```
Implement the first NumOS browser boot per docs/specs/NUMOS_WASM_NATIVEHAL_RUNTIME_AND_EVENT_LOOP_SPEC.md (§C frame order is normative) and NUMOS_WASM_BUILD_DEPLOY_AND_CI_SPEC.md §C.

STAGES (separate commits, in order):
1. WASM-ARCH-02: split src/hal/NativeHal.cpp into EmuCore.{h,cpp} + PlatformSdl.cpp + IEmulatorPlatform.h with ZERO behavior change. Mechanical moves only; log strings, --help text, exit codes, flag handling byte-identical.
2. WASM-BUILD-02/03: scripts/emit-emulator-sources.py + web/CMakeLists.txt (emsdk).
3. WASM-RUN-01 + UI-01: PlatformWeb.cpp (__EMSCRIPTEN__ only) + web/index.html + web/numos-shell.js: emscripten_set_main_loop, RGB565→RGBA blit, integer scaling, loading/error card.

ALLOWED FILES: src/hal/** (new files + NativeHal.cpp reduction), web/**, scripts/emit-emulator-sources.py, platformio.ini ONLY if the hal glob no longer covers new files (prefer zero change).
FORBIDDEN: src/apps/**, src/math/**, src/ui/**, src/input/**, src/display/**, tests/**, .github/**, goldens, masks, lv_conf.h.
COMMANDS: pio run -e emulator_pc; the full local script suite:
  for s in tests/emulator/scripts/*.numos, run per the CI per-script contract (--headless --deterministic --frames per candidates list --quiet);
  python scripts/generate-emulator-candidates.py; python scripts/compare-ppm.py per golden; emcmake/cmake build; a local static server for manual boot check.
STOP CONDITIONS: STOP stage 1 immediately if any golden differs or any script's exit code/PASS-FAIL lines change — do not re-bless, do not mask, fix the refactor. STOP if you need to edit a forbidden file.
ACCEPTANCE: (a) desktop: all 19 goldens byte-identical, all script suites green, self-diff identical; (b) web: page boots splash→launcher in Chromium and Firefox, zero console errors, first frame non-blank.
FINAL REPORT: diffstat per stage; proof-of-acceptance command outputs; wasm+js sizes; deviations from spec (if any) with justification.
```

### Prompt 3 — Fable: adversarial review of the boot prototype

```
Adversarially review the WASM boot prototype against docs/specs/NUMOS_WASM_NATIVEHAL_RUNTIME_AND_EVENT_LOOP_SPEC.md and the architecture spec. You are trying to prove the refactor changed behavior or planted future drift.

CHECKS (minimum): frame-step order vs §C (line-by-line against old NativeHal loop); any log string / exit code / flag drift (diff --help output and grep the CI marker strings "Launcher cargado", "tick = determinista"); hidden platform calls inside EmuCore (grep SDL_/emscripten_/time(/getenv outside Platform*); file-static state that breaks two-instance future; determinism seams (nowMillis usage in deterministic paths); goldens actually byte-identical (rerun candidates + compare, do not trust the PR's claim); Emscripten flags vs BUILD spec §C; memory-view invalidation on growth in the shell blit.

ALLOWED: read everything; run desktop emulator suites and the wasm build; write review notes to a PR comment / review file docs/specs/reviews/WASM_BOOT_REVIEW.md.
FORBIDDEN: fixing code yourself; editing specs.
STOP: after publishing findings ranked by severity with file:line evidence and a repro per finding.
ACCEPTANCE: every finding has a repro command or a concrete failure scenario; explicit verdict list: MERGE-BLOCKING / SHOULD-FIX / NOTE.
FINAL REPORT: verdict summary + the single highest-risk latent defect.
```

### Prompt 4 — Opus/Sonnet: deterministic browser script runner (WASM-RUN-02 + TEST-01/02)

```
Implement deterministic script execution in the browser per docs/specs/NUMOS_WASM_TESTING_DETERMINISM_AND_PARITY_SPEC.md §F and the runtime spec §B/§I.

SCOPE: EmuCore::loadScriptFromString (append-only; file-path loader unchanged); exports numos_load_script/_run_frames/_screenshot_ppm/_exit_code; screenshot store keyed by the script's screenshot paths; pollInput suppression during scripts; tests/wasm/ Playwright harness running: calc semantic suite, input 9A set, menu 9B set, grapher 9F + Phase-10 suites INCLUDING the negative control (must exit 4), and per-stem self-diff (run twice, byte-compare).
ALLOWED FILES: src/hal/EmuCore.*, src/hal/PlatformWeb.cpp, web/**, tests/wasm/**, tests/emulator/candidates.json (+ generate-emulator-candidates.py ONLY to read the shared manifest with identical resulting behavior).
FORBIDDEN: script grammar changes; new assert commands; src/apps|math|ui|input/**; .github/**; goldens/masks; exit-code changes.
COMMANDS: desktop suite re-run (regression net); npm test in tests/wasm (pinned Playwright); both engines.
STOP CONDITIONS: any shared script producing a different exit code or PASS/FAIL line set than desktop → STOP and file a parity finding (do not "fix" the script or weaken the assert). Any self-diff mismatch → STOP, it's a determinism bug.
ACCEPTANCE: all listed suites green on Chromium+Firefox; negative control exits 4; all candidate stems self-diff identical; desktop CI untouched and green; PPMs validate (P6, 320x240, 230415 bytes).
FINAL REPORT: suite-by-suite results table, any parity findings, wall-clock runtime of the browser suite vs budget (≤10 min).
```

### Prompt 5 — Fable: SDL-vs-WASM parity review (WASM-TEST-03 gatekeeping)

```
Review the first SDL-vs-WASM parity report per docs/specs/NUMOS_WASM_TESTING_DETERMINISM_AND_PARITY_SPEC.md §E and decide golden policy per stem (WASM-OD-1).

TASK: For every candidate stem, verify the drift classification (rerun, don't trust): identical / masked-identical / diverged. For each divergence: localize (compare-ppm.py --diff), attribute a cause hypothesis (libm call chain? contraction? long double? uninitialized?), and test the hypothesis where cheap (e.g. same stem with a constant-function variant). Then recommend per stem: share desktop golden / keep wasm golden / block pending fix.
ALLOWED: read everything; run both emulators; write docs/specs/NUMOS_WASM_PARITY_REPORT.md.
FORBIDDEN: blessing goldens (Human-only); editing sources, masks, scripts.
STOP: after the report; escalate to Human for the blessing wave.
ACCEPTANCE: every stem classified with evidence artifact paths; every divergence has a cause hypothesis and a falsification attempt; explicit per-stem recommendation table.
FINAL REPORT: counts by class, riskiest divergence, recommendation for WASM-OD-1.
```

### Prompt 6 — Human release checklist: public web demo (WASM-TEST-07 / G5 gate)

```
NumOS Web Demo — release checklist (complete top to bottom; any NO blocks release):
[ ] wasm-build.yml green on the release commit; desktop emulator-build.yml green.
[ ] W0–W6 parity levels green (testing spec §B); parity report current for this commit.
[ ] Budgets green (build/deploy spec §G) — record actual numbers here: wasm __ MB, total __ MB, boot __ s.
[ ] Manual smoke: Chrome, Firefox, Edge, Safari desktop — boot, 1+2=3, Grapher opens, keypad works, reset works.
[ ] Mobile smoke: one Android Chrome + one iOS Safari device — keypad usable, no zoom fights, no crash.
[ ] Persistence: opt-in flow, reload survival, erase-data, export/import — all verified by hand.
[ ] Demo-script banner + stop works; live input ignored during script.
[ ] Error paths: kill network mid-load → error card; forced abort (dev hook) → overlay with copyable report.
[ ] Privacy/security: no network requests after load (devtools check); CSP header/meta present; no third-party JS.
[ ] Commit hash visible in UI and matches deployed build-info.json.
[ ] Rollback rehearsed: previous version re-deployed to a staging path and boots.
[ ] Docs: README quickstart section + known-limitations list (incl. Safari eviction note) published.
[ ] Sign-off recorded in docs/specs/NUMOS_WASM_PARITY_REPORT.md with date + commit.
```

---

## Explicit answers — consolidated (Special Questions I.1–I.20)

1. **Emscripten SDL2 or new backend?** Both, staged: Emscripten-SDL first boot behind `IEmulatorPlatform`, canvas-native backend at Phase 3, SDL flavor then deleted (arch doc §D).
2. **Split NativeHal?** Yes — WASM-ARCH-01/02, zero-behavior-change gated by byte-identical goldens.
3. **PlatformIO owns the WASM build?** No — CMake/emsdk owns it; PlatformIO stays the authority on *what* compiles via the generated source list (build doc §B).
4. **pthreads?** No (MVP hard rule; Pages hosting cements it).
5. **Asyncify?** No; the frame body never blocks by design.
6. **Same PPM goldens?** Same format/comparator/masks; separate `golden-wasm/` gating until the drift report proves per-stem byte-equality, then promote to shared per stem (testing doc §E).
7. **Browser screenshots?** Exported CPU-framebuffer PPM (`buildPpm`), never canvas readback (testing doc §E).
8. **Storage mapping?** `./emulator_data` mounted per mode — MEMFS sandbox (default, deterministic), IDBFS persistent (opt-in), MEMFS fixture; file formats unchanged; GS/FIX semantics mirrored (UI doc §C).
9. **Deterministic time?** Frame-indexed synthetic tick unchanged; browser clocks quarantined behind `nowMillis()`; clock mask retained until EMUDET-01 (runtime doc §B, testing doc §F).
10. **Script replay?** Same C++ engine, one command per frame; scripts delivered as strings; batched frame execution for speed (runtime doc §I).
11. **Keyboard focus changes?** Explicit focus model with visible indicator; RELEASE synthesized for held keys on blur; calculator modifier state untouched (UI doc §A/§B).
12. **Mobile/touch?** On-screen keypad (pointer events, long-press repeat), no soft-keyboard summons; canvas taps do nothing (UI doc §B).
13. **All apps or stable apps?** Exactly the emulator-enabled set (same launcher, same dead-card refusals); browser never leads desktop enablement (testing doc §H).
14. **Crashes/traps?** `onAbort` → error overlay + copyable structured report; script assert failures remain clean exit-4; harness records exact codes (runtime doc §H).
15. **Public deployment?** GitHub Pages, versioned dirs + mutable `latest/`, PR artifacts, manual-then-auto publish, rollback by redeploy (build doc §E/§I).
16. **Must remain identical:** framebuffer geometry/format, PPM contract, script grammar + assert markers + exit codes, app whitelist/routing, deterministic semantics, KeyCode dispatch, log grammar, persistence byte formats.
17. **May differ:** pacing source, presentation path, persistence backend, input transport, pause-on-hidden, golden sets until proven, absence of wall-clock auto-exit.
18. **Top blockers before public demo:** ARCH-02 refactor risk; emsdk build bring-up; exceptions decision; script-runner parity (RUN-02); golden strategy execution (TEST-03/04); input focus/keypad UX (UI-02); FS sandbox modes (FS-01); size budget; CI workflow (DEPLOY-01); cross-browser verification.
19. **NumOS Lab interaction?** Embed mode (`?mode=embed`) + ES-module shell is the integration surface; iframe first (arch doc WASM-OD-9).
20. **Minimal realistic first milestone?** End of Phase 1: browser boots to launcher, arrows+ENTER open Calculation, `1+2=` shows `3`, W1 smoke green in CI — with the ARCH-02 split already landed and desktop CI proven unchanged.
