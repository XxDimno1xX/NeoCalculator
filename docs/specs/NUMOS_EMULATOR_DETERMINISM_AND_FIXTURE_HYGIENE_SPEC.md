# NumOS — Emulator Determinism & Fixture Hygiene Specification

> **What "done" means:** a `.numos` run's observable output (pixels, assert results, exit code, files written) is a pure function of *(binary, script, CLI flags)* — never of the host's wall clock, a previous run's leftovers, the host keyboard layout, or the checkout's dirty state — and every file a test run creates has a declared home and lifecycle. Companion docs: `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` (SANDBOX), `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (CIART), `NUMOS_EMULATOR_HOST_PORTABILITY_SPEC.md` (PORT), `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md` (EMUROAD). This spec refines — and never contradicts — `NUMOS_STATUSBAR_TRUTHFULNESS_AND_SYSTEM_CHROME_SPEC.md` (SBAR, esp. SB-03), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE, esp. §F/GS-03), and `NUMOS_SYSTEM_CHROME_QA_ORACLE_AND_GOLDEN_PLAN.md` (SCQA, esp. QA-05).
>
> Audit base: branch `claude/numos-emulator-determinism-specs-69nso8` (2026-07-04), working tree clean at audit start. Notation: **[V]** verified with `file:line` evidence · **[P]** proposed (desired behavior; does not exist yet). SPEC-ONLY: no code changed.

---

## A. Executive summary

The SDL2 emulator (`src/hal/NativeHal.cpp`, compiled only under `NATIVE_SIM`, `NativeHal.cpp:85`) has a genuinely good determinism core: `--deterministic` replaces LVGL's clock with a synthetic fixed-step counter (`NativeHal.cpp:189-190,1998,2066-2068`), scripts inject exactly one command per frame through the same dispatch path as live input (`NativeHal.cpp:1697-1701,2049`), and screenshots are read from the CPU framebuffer, independent of renderer/scale/headless (`NativeHal.cpp:1292-1315`). CI has empirically proven run-to-run byte-reproducibility for one fixture (`emulator-build.yml:228-237`).

But the determinism boundary is narrower than the flag name implies, and the fixture story has real holes:

1. **`--deterministic` virtualizes LVGL time only.** `time()` — the StatusBar clock (`StatusBar.cpp:205-225`) — and `SDL_GetTicks()` outside the tick callback (`--run-for-ms`, `NativeHal.cpp:2044,2125-2129`; `millis()` in `ArduinoCompat.h:176-180`) still read the host. Result: every StatusBar screen is *cross-launch* nondeterministic in exactly one rect, papered over by 18 identical clock masks (`tests/emulator/masks/README.md:5-14`).
2. **Persistent state crosses runs.** `emulator_data/vars.dat` is git-tracked, loaded at every boot (`NativeHal.cpp:2171-2179`), and rewritten by any STO (`CalculationApp.cpp:894-901`) — including in CI (`emulator-build.yml:343`). A test run mutates a committed fixture, dirties the tree, and leaks state into later runs.
3. **Fixture lifecycle is inconsistent.** Scripts write screenshots into `out/` relative to CWD (`tests/emulator/scripts/calc_1_plus_2.numos` `screenshot out/calc_1_plus_2.ppm`), candidates go to `out/emulator-candidates/` (`generate-emulator-candidates.py:240-242`), goldens/masks are committed, `emulator_data/` is half-fixture-half-scratch, and nothing verifies the tree is clean after a suite.
4. **Host-coupled paths** (`build_dir = C:/.piobuild/numOS`, `platformio.ini:6`) and host-coupled input (SDL_TEXTINPUT layout resolution, `NativeHal.cpp:783-803`) are already mitigated for CI and scripts but remain live traps for local interactive use (PORT owns these).

This spec defines the current determinism model (§B), the exact boundary (§C), a formal deterministic-run contract (§D), fixture hygiene rules (§E), the concrete hazards (§F), four named run modes (§G), acceptance tests (§H), and tickets (§I).

---

## B. Current emulator determinism model [V]

### B.1 LVGL tick behavior

- Default (interactive): `lv_tick_set_cb(SDL_GetTicks)` — wall time (`NativeHal.cpp:1998`, policy comment `:151-155`). Loop sleeps `FRAME_DELAY_MS = 5` per iteration (`NativeHal.cpp:155,2106-2108`).
- `--deterministic`: `lv_tick_set_cb(detTickCb)` where `detTickCb` returns `g_detTick` (`NativeHal.cpp:189-190,1998`). The main loop advances `g_detTick += stepMs` once per frame, *before* `lv_timer_handler()` (`NativeHal.cpp:2066-2068`), so every LVGL animation/timer state is a function of the frame index. `--step-ms N` sets the step, default 16, clamped 1..1000 (`NativeHal.cpp:172,1267-1271`).
- In deterministic mode the loop does **not** sleep (`NativeHal.cpp:2103-2108`) — runs complete as fast as the host allows; wall pacing is irrelevant to the virtual clock.

### B.2 `--deterministic` scope

The flag changes exactly two things: the tick source (`:1998`) and the sleep policy (`:2106`). It does **not** touch `time()`, `SDL_GetTicks()` call sites outside the tick callback, `rand()`, the filesystem, or app state. There is no logged banner of *fixture* state — only `tick = determinista` (`NativeHal.cpp:1999-2006`), which CI greps (`emulator-build.yml:172-173`).

### B.3 Frame count and auto-exit

- `--frames N` counts main-loop iterations (`loopCount`, `NativeHal.cpp:2043,2110,2121-2124`) — frame-indexed, deterministic.
- `--run-for-ms N` compares `SDL_GetTicks() - startTicks` (`NativeHal.cpp:2044,2125-2129`) — **wall clock, even under `--deterministic`**. The two flags can be combined; whichever fires first exits. CI's Phase-3A smoke uses `--run-for-ms 5000` deliberately as a wall-clock boot check (`emulator-build.yml:136-138`); everything comparable uses `--frames`.

### B.4 Screenshot timing

- Final screenshot (`--screenshot P`): written after the loop exits, before teardown, from `g_lvBuf` (`NativeHal.cpp:2132-2140`).
- In-script `screenshot RUTA`: the command marks a deferred capture on its frame (`NativeHal.cpp:1718-1721`); the capture executes after that frame's render (`scriptCaptureIfPending`, `NativeHal.cpp:2080-2082,1855-1871`). A write failure is fatal, exit 3 (`NativeHal.cpp:1860-1865`).
- `saveScreenshotPPM` converts the full-frame RGB565 CPU buffer (`LV_DISPLAY_RENDER_MODE_FULL`, `NativeHal.cpp:2009-2011`) to P6 RGB888 with fixed bit-replication (`NativeHal.cpp:1292-1315`) — no GPU readback, so identical under `--headless`, any `--scale`, and any renderer backend. Frame contract: `P6\n320 240\n255\n` + 320·240·3 bytes = 230 415 bytes (`generate-emulator-candidates.py:29-33`).

### B.5 StatusBar `time()`

`StatusBar::updateClock()` reads `time(nullptr)` → `localtime` → `"%02d:%02d"` (`StatusBar.cpp:205-225`). This is the host wall clock on both targets; the deterministic tick does not cover it. Refresh is event-driven, not periodic: CalculationApp calls `_statusBar.update()` on load and on key handling (`CalculationApp.cpp:133,259-302,513`); SettingsApp only on `load()` (`SettingsApp.cpp:110`) — so the pixels freeze at whatever wall minute the last update ran (SBAR §B.3 owns the truthfulness aspect). The launcher's bar is a *different*, static implementation — literal `"rad"`, `"APPLICATIONS"`, full-battery glyph, **no clock** (`MainMenu.cpp:321-356`) — which is why `launcher_smoke` is byte-stable with no mask (`tests/emulator/masks/README.md:24-26`).

Consequence [V]: within one run, two frames in the same wall minute render identical clocks; across runs launched in different minutes the clock rect differs. All 18 masks exist solely for this (`tests/emulator/masks/README.md:36-39`, every mask file is the single rect `4,6,37,13`, e.g. `tests/emulator/masks/settings_smoke.mask`).

### B.6 App timers and async timers

All whitelisted-app timers are LVGL timers, therefore virtualized under `--deterministic`:

- MathCanvas cursor blink: `lv_timer_create(cursorTimerCb, BLINK_PERIOD, …)` (`MathRenderer.cpp:669,694`). Under the synthetic tick the blink phase is a function of frame index — the masks README documents that the old "cursor mask" theory was a misdiagnosis (`tests/emulator/masks/README.md:46-54`).
- Grapher lazy template load: `_tplLoadTimer` (`GrapherApp.cpp:1508`); Grapher async POI: `_poiAsyncTimer`, 10 ms (`GrapherApp.cpp:2994`), both deleted on teardown (`GrapherApp.cpp:170,188-189`). Empirically frame-deterministic (`generate-emulator-candidates.py:105-112,113-120`).
- Deferred teardown latches measure elapsed time with `lv_tick_elaps` (`NativeHal.cpp:2088-2101`, `TEARDOWN_DELAY_MS = 260`, `:233`) — same clock as the animations, correct in both modes (comment `:222-229`).

Hazards that are compiled but *not* virtualized: `millis()`/`micros()`/`delay()` in the native shim are `SDL_GetTicks`-based wall time (`ArduinoCompat.h:176-180`). No emulator-whitelisted app (`platformio.ini:195-266`) calls them today [V — grep found `millis()` only in non-whitelisted apps and `main.cpp`], but any future whitelist addition that uses `millis()` silently reintroduces wall time. Same for `rand()`: used by `EquationsApp.cpp:106`, `IntegralApp.cpp:334`, `NeuralLabApp.cpp:372-380`, `NeuralEngine.cpp:140-141` — none whitelisted, no `srand()` call anywhere in `src/` [V], so the day one of these is emulator-enabled it will use the libc default seed (deterministic per libc but unpinned across platforms).

### B.7 Input event timing

- Script replay: one command per frame, executed at the top of the loop *before* `processSdlEvents()` and the tick advance (`NativeHal.cpp:1341-1343,2045-2051`), through the same `dispatchKey()` as SDL input (`NativeHal.cpp:575-769`). `key` synthesizes PRESS+RELEASE in the same frame (`NativeHal.cpp:1708-1711`). Scripts are validated entirely at load; any error exits 2 before SDL init (`NativeHal.cpp:1457-1622,1876-1884`).
- Live SDL input: `SDL_KEYDOWN/KEYUP` → PRESS/REPEAT/RELEASE (`NativeHal.cpp:805-849`); printable characters arrive via `SDL_TEXTINPUT` already resolved by the host keyboard layout (`NativeHal.cpp:783-803,415-444`). Live input is inherently wall-timed and layout-dependent — fine, because nothing compared/asserted is allowed to come from live input (`NUMOS_FABLE_CONTEXT_HEADER.md:89`).

### B.8 Fade/animation timing

Splash fade (~2 s, `NativeHal.cpp:2026-2030`), launcher fade-in 200 ms (`showcaseLoad` example `NativeHal.cpp:1200`; MainMenu load), and app-screen `LV_SCREEN_LOAD_ANIM_FADE_IN` 200 ms (`SettingsApp.cpp:114`) are all `lv_anim`-driven → frame-deterministic under the synthetic tick. Scripts budget for them in frames (`calc_1_plus_2.numos` comments: splash done ~frame 125 at step-ms 16).

---

## C. Determinism boundary

### C.1 Deterministic today (proven or structural) [V]

| Element | Why | Evidence |
|---|---|---|
| LVGL animations, timers, blink phases | synthetic tick advanced per frame | `NativeHal.cpp:1998,2066-2068` |
| Script command timing | 1 command/frame, frame-indexed waits | `NativeHal.cpp:1697-1721` |
| Key dispatch path | shared with SDL path, no OS layout in script names | `NativeHal.cpp:456-565,575` |
| Screenshot pixels vs renderer/scale/display | CPU buffer read | `NativeHal.cpp:1286-1315` |
| Run-to-run byte identity (same process env, same minute or masked) | CI runs `calc_1_plus_2` twice and compares SHA-256 | `emulator-build.yml:228-237` |
| Launcher screen | static bar, no clock | `MainMenu.cpp:321-356` |
| Frame budget exits | `--frames` counts iterations | `NativeHal.cpp:2121-2124` |

### C.2 Not deterministic today [V]

| Element | Failure shape | Evidence |
|---|---|---|
| StatusBar clock pixels | differ across launches in different minutes | `StatusBar.cpp:205-225`; masks README `:22-39` |
| `--run-for-ms` exit point | frame count at exit varies with host load | `NativeHal.cpp:2125-2129` |
| Variable state at boot | `emulator_data/vars.dat` loaded from whatever the last run (or the commit) left | `NativeHal.cpp:2171-2179`; `FileSystem.cpp:38` |
| Working tree after STO scripts | `vars.dat` rewritten in place | `CalculationApp.cpp:900`; `VariableManager.cpp:169-190` |
| Live keyboard input | OS layout, autorepeat, wall-time arrival | `NativeHal.cpp:783-849` |
| `millis()/micros()/delay()` shim | wall clock if any whitelisted code adopts them | `ArduinoCompat.h:176-180` |

### C.3 Deterministic only by masking [V]

Exactly one class: the StatusBar `HH:MM` rect `4,6,37,13`, in all 18 masks (`tests/emulator/masks/README.md:36-44`). Everything else in every golden is byte-gated. This is honest, documented masking — but it institutionalizes the clock lie (SBAR §D row 12) and it means "golden passes" currently proves "deterministic except where we agreed not to look."

### C.4 Host-dependent [V]

- Binary location: `build_dir = C:/.piobuild/numOS` (`platformio.ini:6`) vs `PLATFORMIO_BUILD_DIR` override (CI: `emulator-build.yml:54-55`) vs `.pio/build` default — resolution ladders exist in `generate-emulator-candidates.py:146-181`, `run-emulator-linux.sh:27-48`, `run-emulator-windows.ps1:39-48` (PORT §D).
- SDL2 discovery and runtime library (PORT §B; `sdl2_env.py:63-162`).
- Pixel content across OSes: fonts are compiled into the binary (`platformio.ini:264-266`; LVGL Montserrat is in-tree), so no host-font dependency; the known cross-host drift risks are libm (`sin/cos/log` double rounding feeding Grapher curves via the legacy `Evaluator`) and the loosely pinned `lvgl@^9.2.0` (`platformio.ini:107,178`; risk REN-4, `NUMOS_RISK_REGISTER.md`). No cross-OS golden equality has ever been measured — goldens are blessed on the "pinned reference OS — Linux" (`tests/emulator/golden/README.md:33-35`) and only Linux CI gates them (CIART §I).
- Script file line endings: CRLF tolerated explicitly (`NativeHal.cpp:1469-1470`).

---

## D. Deterministic run contract [P → EMUDET-01…04; states the target, current gaps cited]

A run invoked with `--deterministic` SHALL satisfy, for every observable output:

1. **Clock.** All time-derived UI state derives from one virtual clock. LVGL tick: already `g_detTick` [V `:1998`]. **Gap:** `time(nullptr)` must be virtualized — a `numosNow()` HAL seam returning `epoch0 + g_detTick` in deterministic mode (default epoch fixed, e.g. 2026-01-01 00:00, matching the firmware bootstrap `StatusBar.cpp:125-142`; overridable via `--epoch <unix-seconds>` for clock-rollover tests). This is SB-03 (SBAR §E), restated here as EMUDET-01. `--run-for-ms` SHALL be rejected (exit 2) in deterministic mode instead of silently mixing clocks (EMUDET-02).
2. **Random seed.** Deterministic mode seeds `srand(1)` at startup and logs it; any future PRNG use in whitelisted apps must route through a seedable helper. Today no whitelisted consumer exists [V §B.6], so this is a cheap fence, not a behavior change (EMUDET-03).
3. **Filesystem state.** The run starts from a *declared* filesystem: by default an empty per-run sandbox; with `--fixture-dir <d>` a read-only copy of `<d>` (SANDBOX §B/§C). It never reads or writes the repo's `emulator_data/` unless `--fs-root emulator_data` is explicitly passed (interactive convenience only). Current behavior violates this: hardcoded `./emulator_data` (`FileSystem.cpp:38`), CWD-relative (`FileSystem.cpp:44-49`).
4. **Variable state.** Follows filesystem state: default = all-zero `VariableManager` (constructor default, `VariableManager.cpp:54-56,112-116`) because the sandbox is empty; fixture mode = exactly the fixture's `vars.dat`. `assert_variable` semantics unchanged (`NativeHal.cpp:1805-1820`).
5. **Settings state.** Compile-time defaults every run (`NativeHal.cpp:125-127`); settings persistence, when GS-02 lands, is opt-in `--persist-settings` and sandboxed (GSTATE §F.1, §B.3). No change needed today; the contract forbids regressing it.
6. **SDL events.** Scripted runs must not depend on live events. Today live SDL events are still processed during script replay (`NativeHal.cpp:2051`) — harmless headless (dummy driver produces none), but a windowed scripted run can be perturbed by a human key. Contract: with `--script`, live key/text events are ignored (logged once) unless `--allow-live-input` is passed (EMUDET-04).
7. **LVGL timers / app timers.** All timing through `lv_tick`/`lv_timer` only. New-code rule (enforced by review + a CI grep, CIART §H): whitelisted-app code must not call `time()`, `millis()`, `SDL_GetTicks()`, `clock()`, `std::chrono::system_clock` directly — HAL seams only.
8. **Screenshot time.** Screenshots occur at frame boundaries only (already true, §B.4) and their pixels contain no wall-clock-derived text once (1) lands. Post-EMUDET-01, **new goldens carry no clock mask**, and existing masks are retired golden-by-golden at re-bless time (SBAR SB-03; CIART §E).

**Failure behavior.** Violations detectable at parse/startup (flag conflicts, missing fixture dir) exit 2 with a one-line reason on stderr, before SDL init — same fail-fast contract as script validation (`NativeHal.cpp:1876-1884`). Violations detectable only at runtime (fixture write attempt in read-only mode) exit 3 (same class as screenshot-write failure) with `[FS]`-tagged diagnostics per the RT tag grammar (`NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md`).

---

## E. Fixture hygiene

### E.1 What files a test run may create, and where [P — with current state cited]

| Class | Today [V] | Contract [P] |
|---|---|---|
| Persistence (vars/settings/app saves) | `./emulator_data/` relative to CWD (`FileSystem.cpp:38,44-49`), git-tracked file inside | per-run sandbox under the OS temp dir, or `--fs-root`/`--fixture-dir` explicit (SANDBOX §B) |
| Script screenshots | any path the script names; convention `out/…` (`calc_1_plus_2.numos`); generator pre-creates `out/` (`generate-emulator-candidates.py:266-268`) | unchanged location (`out/` is git-ignored, `.gitignore` `out/`), but paths must stay repo-relative and inside `out/` for bundled scripts; CI treats them as artifacts (CIART §F) |
| Candidates | `out/emulator-candidates/` (`generate-emulator-candidates.py:240-242`) | unchanged |
| Goldens / masks | committed, human-promoted only (`tests/emulator/golden/README.md:115-129`; `promote-emulator-golden.py`) | unchanged |
| Logs | CI redirects stdout to `out/**/*.log` (`emulator-build.yml:272,322,388`) | unchanged; local runs may log anywhere under `out/` |

### E.2 Cleanup policy [P]

- Per-run sandboxes are created under the OS temp dir and deleted on clean exit; on failure they are *kept* and their path printed, so the post-mortem state is inspectable (and CI uploads it, CIART §F.6).
- `out/` is never auto-cleaned by the emulator; `generate-emulator-candidates.py` overwrites per-stem files idempotently (`:285`). A `--clean` convenience on the generator MAY be added; it must only ever delete `out/emulator-candidates/`.
- Nothing under `tests/emulator/` is ever written by any tool (`promote-emulator-golden.py:6-12` is the sole, human-run exception for goldens).

### E.3 Dirty-tree policy [P → FIX-03/CI-01]

After any scripted suite (local or CI), `git status --porcelain` restricted to tracked paths SHALL be empty. Today this fails: running `calc_store_variable.numos` rewrites the tracked `emulator_data/vars.dat` (write path `CalculationApp.cpp:900` → `VariableManager.cpp:169-190` → `FileSystem.cpp:70-82`). CI does not notice because it never checks and the runner is ephemeral; local developers get a permanently dirty file. Contract: (a) the sandbox removes the cause; (b) CI adds `git diff --exit-code` as a gate anyway (defense in depth, exact step in CIART §H) — the guard is what keeps the *next* hazard from shipping.

### E.4 CI policy vs local policy

- **CI:** every gating step runs `--headless --deterministic --frames N` (`emulator-build.yml:167-168,210-212,270-272` and siblings); artifacts under `out/` are uploaded (`emulator-build.yml:239-249` etc.); the dirty-tree guard (E.3) and sandbox-by-default make the checkout state irrelevant.
- **Local:** same commands work verbatim (`NUMOS_FABLE_CONTEXT_HEADER.md:40-43`); interactive runs (no `--deterministic`) are for eyeballs only and carry no hygiene guarantees beyond "writes stay in sandbox/`out/`". The one local-only concession: `--fs-root emulator_data` for developers who *want* durable local variables across interactive sessions.

---

## F. Current hazards (ranked) [V]

1. **`emulator_data/vars.dat` is a tracked, mutable, cross-run store.** Tracked (`git ls-files` shows `emulator_data/vars.dat`); loaded every boot (`NativeHal.cpp:2171-2179`); rewritten by STO (`CalculationApp.cpp:900`); STO script runs in CI (`emulator-build.yml:343`). Committed content is 11 zero-valued vars (`VR01` magic little-endian on disk reads `10RV`; header 4+1 bytes + 11×34 = 379 bytes, matching `VariableManager.h:139,148,154-155`). Three distinct failure modes: dirty tree; state leakage into later runs' `assert_variable`/renders; and a future contributor committing a mutated `vars.dat` silently changing every subsequent run's boot state.
2. **Wall-clock StatusBar under `--deterministic`.** `StatusBar.cpp:205-225`; forced the 18-mask regime (`tests/emulator/masks/README.md`); already caused a real CI failure when a mask under-covered the clock's left edge (`tests/emulator/masks/README.md:46-54`).
3. **`C:/.piobuild` on non-Windows** creates a literal `./C:/` directory in the repo (`platformio.ini:6`; quickstart `docs/emulator-sdl2-quickstart.md:98-133`; risk EMU-3). Ignored by `.gitignore` (`*piobuild*`) but breaks binary-discovery expectations (PORT §D owns the decision).
4. **Warning-only candidates.** 25 candidates are generated (`generate-emulator-candidates.py:39-143`) but only 19 goldens exist (`tests/emulator/golden/`), so 6 stems (`menu_focus_grapher_smoke`, `grapher_implicit_x/2x`, `grapher_multifn_graph/table`, `grapher_expr_scroll`) compare-warn forever (`emulator-build.yml:602-604`). Warn-not-fail is the *designed* anti-ossification property (`tests/emulator/golden/README.md:124-127`) — the hazard is that there is no inventory/aging policy, so "pending human review" is indistinguishable from "forgotten" (CIART §C).
5. **Script screenshots under `out/`** rely on the CWD being the repo root (`FileSystem.cpp`-style relative opens also apply to `fopen` in `saveScreenshotPPM`, `NativeHal.cpp:1294`); the generator enforces `cwd=REPO_ROOT` (`generate-emulator-candidates.py:307-308`) but a hand-run from another directory writes `out/` wherever you stand, or exits 3 if the directory doesn't exist.
6. **Persistent app saves:** today only vars (emulator) — NeoLanguageApp saves editor sessions via its own `saveToFlash/loadFromFlash` (`NeoLanguageApp.cpp:116,393-394,897,924`) but is firmware-only (not in the whitelist `platformio.ini:195-266`). The hazard is the *pattern*: any app enabled into the emulator later (P-05 wave) brings its LittleFS writes into `emulator_data/` with no sandbox (SANDBOX §A).
7. **`--run-for-ms` mixing wall clock into "deterministic" invocations** (`NativeHal.cpp:2125-2129`) — currently only used by the intentionally wall-clock Phase-3A smoke; a footgun for script authors.

---

## G. Proposed deterministic modes [P]

Four named modes; flags compose rather than multiply. All are backwards-compatible: today's invocations keep today's meaning until the sandbox default flips (staged in EMUROAD).

### G.1 Interactive mode (default; exists today)

`program` with no flags: wall clock, window, live input, sleeps 5 ms/frame (`NativeHal.cpp:151-155`). Persistence: today repo `./emulator_data` [V]; target: still `./emulator_data` via explicit *documented* default `--fs-root emulator_data` semantics, so interactive users keep durable variables (SANDBOX §B.4). Never used for anything compared or asserted (`NUMOS_FABLE_CONTEXT_HEADER.md:89`).

### G.2 Deterministic-headless mode (exists today, minus clock/sandbox)

`--headless --deterministic --script S --frames N [--screenshot P] --quiet` — the CI shape (`emulator-build.yml:167-168` etc.). Target additions: virtual `time()` (§D.1), live-input ignore (§D.6), `srand(1)` (§D.2), and the sandbox default (§G.3). Exit codes stay the append-only contract 0/2/3/4 (`NativeHal.cpp:1382,1687,1863,1883`; +1 for SDL/init failure `:1900-1903`).

### G.3 Deterministic-with-clean-fixtures mode (target default for G.2)

Everything in G.2 plus: fresh empty FS sandbox per process (no `vars.dat` read/write outside it), tree provably untouched. Flags: `--fs-sandbox` (explicit today → default later), `--fixture-dir <d>` to seed the sandbox read-only from a committed fixture directory (e.g. a future `tests/emulator/fixtures/vars_x4/`). Exact mechanics, path resolution, and Windows/macOS temp-dir behavior: SANDBOX §B.

### G.4 Deterministic-persistence-roundtrip mode

For two-process tests (Settings GS-02, vars today): two or more invocations *sharing* one named sandbox: `--fs-sandbox-dir $T` (create-if-absent, no copy, read-write). Run A mutates state and exits; run B boots from $T and asserts. Protocol, cleanup, and failure artifacts: SANDBOX §C; roundtrip test content: SCQA §E.3 (QA-05). This mode is the only sanctioned way for one test run to see another's writes.

---

## H. Acceptance tests

Each maps to a ticket in EMUROAD; "runner-level" = shell/python steps in CI, not `.numos` grammar.

1. **AT-DET-1 (clock virtualization, EMUDET-01).** Runner: generate `settings_smoke` candidate twice with the host `TZ` forced to two different values and with `faketime`-style start minutes differing (or simply: two runs straddling a real minute rollover — run, `sleep 61`, run). Compare with `compare-ppm.py` **without** the mask → exit 0. Today this fails inside rect `4,6,37,13`.
2. **AT-DET-2 (self-diff without masks).** Runner: for every stem in the candidate list, run twice, byte-compare unmasked → identical. Generalizes `emulator-build.yml:228-237` from one fixture to all (CI phase T-selfdiff, CIART §J.2).
3. **AT-DET-3 (`--run-for-ms` rejection, EMUDET-02).** `program --deterministic --run-for-ms 100` exits 2 with a message naming both flags; `--deterministic --frames 100` still exits 0.
4. **AT-DET-4 (live-input isolation, EMUDET-04).** Windowed scripted run with synthetic SDL key injection (SDL_PushEvent harness or manual QA row): final screenshot equals the headless run's byte-for-byte.
5. **AT-FIX-1 (sandbox isolation, FIX-01/FIX-02).** Runner: run `calc_store_variable.numos` (STO writes x=4, `emulator-build.yml:343`); assert exit 0; assert `git status --porcelain` empty; assert repo `emulator_data/vars.dat` SHA unchanged; then run `calc_1_plus_2.numos` and assert it cannot observe x=4 (add an `assert_variable x 0` bookend variant).
6. **AT-FIX-2 (fixture seeding).** Runner: prepare fixture dir with a `vars.dat` where x=7; run with `--fixture-dir` + script asserting `assert_variable x 7`; assert the fixture file's SHA unchanged after the run (writes went to the sandbox copy).
7. **AT-FIX-3 (roundtrip, FIX-04).** Runner: run A `--fs-sandbox-dir $T` stores x=4 and exits 0; run B same `$T` asserts `assert_variable x 4`; run C fresh sandbox asserts `assert_variable x 0`. Mirrors SCQA §E.3 for variables now, settings post-GS-02.
8. **AT-CI-1 (dirty-tree guard, CI-01).** CI: after all suites, `git diff --exit-code && git status --porcelain | (! grep .)` (untracked-file check scoped to non-`out/` paths). Must pass with the sandbox; must have *failed* before it (proof the guard bites: run once with sandbox disabled in a scratch branch).
9. **AT-DET-5 (deterministic banner).** Deterministic runs log a single machine-greppable line enumerating: tick source, step-ms, epoch, seed, fs mode + resolved sandbox path. CI greps it in every gating step (extends `emulator-build.yml:172-173`).

---

## I. Implementation tickets (headline — full fields in EMUROAD)

- **EMUDET-01** — Virtual `time()`/clock injection for deterministic mode (= SBAR SB-03 emulator half). Unblocks unmasked goldens.
- **EMUDET-02** — Flag hygiene: reject `--run-for-ms` under `--deterministic`; deterministic banner line (AT-DET-5).
- **EMUDET-03** — Seed fence: `srand(1)` + no-direct-time/CI grep guard for whitelisted sources.
- **EMUDET-04** — Ignore live SDL input during `--script` replay unless `--allow-live-input`.
- **FIX-01** — FS sandbox flags (`--fs-sandbox`, `--fs-sandbox-dir`, `--fixture-dir`, `--fs-root`) in `hal/FileSystem` (SANDBOX §B).
- **FIX-02** — Flip default: scripted+deterministic runs sandbox by default; interactive keeps `emulator_data/`.
- **FIX-03** — `emulator_data/vars.dat` git hygiene: untrack, ignore, provide fixture path (SANDBOX §E).
- **FIX-04** — Two-process persistence roundtrip runner test (SANDBOX §C; SCQA QA-05).
- **CI-01** — Dirty-tree CI guard (CIART §H).
- **CI-02…CI-06, PORT-01…PORT-05** — see CIART §K and PORT §G.

*End of determinism & fixture hygiene spec.*
