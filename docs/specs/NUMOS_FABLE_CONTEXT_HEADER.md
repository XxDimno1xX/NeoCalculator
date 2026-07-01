# NumOS — Context Header for Fable/Opus/Sonnet sessions

> Paste this at the top of future agent prompts. Verified against `main` @ `8662f47` (2026-07-01). Full evidence: `docs/specs/NUMOS_ARCHITECTURE_GROUND_TRUTH.md`.

## What NumOS is

C++17 scientific-calculator OS for **ESP32-S3 N16R8** (16 MB QIO flash, 8 MB OPI PSRAM), ILI9341 320×240 (rotation 1), **LVGL 9.5.0** (pinned loosely as `lvgl@^9.2.0`). GPL-3 licensed, single-maintainer, Windows-first dev machine, Spanish+English comments. Repo = `El-EnderJ/NeoCalculator`.

**Two entry points, one app codebase:**
- Firmware: `src/main.cpp` (`#ifdef ARDUINO`) → `SystemApp` (`src/SystemApp.h/.cpp`) — owns app lifecycle, launcher dispatch, deferred teardown.
- Desktop emulator: `src/hal/NativeHal.cpp` (SDL2, `-DNATIVE_SIM`) — its **own** app registry/dispatch, deterministic tick, `.numos` script replay, PPM screenshots, semantic `assert_*` commands, exit codes 0/2/3/4.

**Apps** (launcher ids, `src/ui/MainMenu.cpp:88-115`): 0 Calculation, 1 Grapher, 2 Equations, 3 Calculus, 4 Statistics, 5 Probability, 6 Regression, 7 Sequences, 8 Python, 9 Matrices, 10 Settings, 11 Chemistry/PeriodicTable, 12 Bridge, 13 Circuit, 14 Fluid2D, 15 ParticleLab, 16 NeuralLab, 17 OpticsLab, 18 NeoLang, 19 Fractals, [20 Math Visual — only with `NUMOS_MATH_VISUAL_VERIFY`]. All 0-19 are launcher-reachable on firmware. `TutorApp` and `IntegralApp` exist in `src/apps/` but are **dead** (not instantiated/dispatched).

**Math stack (three engines — do not conflate):**
1. **VPAM** (`vpam::`, namespace of `src/math/MathAST.h`): structural expression tree (17 node types, `unique_ptr` children, raw parent pointers, PSRAM `operator new`), edited by `CursorController`, rendered by `ui/MathRenderer` (MathCanvas LVGL widget, baseline-oriented, STIX Two Math fonts + vector-fallback delimiters), evaluated by `MathEvaluator` → `ExactVal` (int64 rational + radical + π/e; S⇔D result modes Symbolic/Periodic/Extended).
2. **Legacy RPN** (`Tokenizer`→`Parser`→`Evaluator`, doubles only): used by **GraphModel/GrapherApp** and `EquationSolver`. Different grammar and its own angle-mode member — semantic divergence from VPAM is a real risk.
3. **CAS**: in-tree `cas::` (59 files, ~20k lines: SymExpr hash-consed arena DAG, SymSimplify/Diff/Integrate, Omni/Single/System solvers, tutor/step stack) for edu-steps + Equations/Calculus; plus **Giac 1.9.0** (`lib/giac`, KhiCAS build) — **firmware-only**, real+exact mode, reached solely via `src/math/giac/GiacBridge.cpp::solveWithGiac()` under `#ifdef ARDUINO`.

## Non-negotiable constraints

1. **Emulator compiles a whitelist**, not the tree: `platformio.ini:195-266`. Emulator apps = Calculation, Settings, Statistics, Probability, Sequences, Regression, Grapher, MathShowcase. Adding an app to the emulator requires: whitelist entries + `NativeHal.cpp` AppMode/dispatch + scripts. Firmware builds everything (`+<*>`).
2. **Giac never builds natively.** `lib_ignore = giac, libtommath` in `emulator_pc` (`platformio.ini:192-194`). Native shim returns an error string. Don't "fix" this casually.
3. **KeyCode enum is append-only** and digits are **non-contiguous** (`src/input/KeyCodes.h`). Never write `code - NUM_0` or `>= NUM_0 && <= NUM_9`; use `keyCodeDigitValue()`. CI statically scans for violations and runs a host unit test (`emulator-build.yml:96-104`).
4. **LVGL draw buffer**: single 32 KB, `MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA` (`main.cpp:138-142`). No PSRAM (silent black screen/crash), no double buffer (LVGL 9 deadlock).
5. **Deferred teardown**: never `lv_obj_delete` an app screen synchronously on exit. `returnToMenu()` records the mode; `end()` runs ~250 ms later (`SystemApp.cpp:181-239`; mirrored in NativeHal "Phase 9F"). Every `end()` destroys StatusBar **before** the screen and nulls LVGL pointers.
6. **Goldens are byte-exact** P6 PPMs (320×240) under `tests/emulator/golden/`, with optional pixel masks (`masks/*.mask`, `x,y,w,h` lines). Golden mismatch **fails CI**; missing golden warns. Promotion is human-only via `scripts/promote-emulator-golden.py`. Never hand-edit PPMs/masks; never promote without visually reviewing the diff.
7. **Two variable stores exist and do not sync**: `VariableManager` (ExactVal, LittleFS `/vars.dat`) vs `VariableContext` (double, NVS `calcVars`). Don't add a third; unification is a planned ticket.
8. **`build_dir = C:/.piobuild/numOS`** (`platformio.ini:6`): on Linux/macOS/CI set `PLATFORMIO_BUILD_DIR=.pio/build` or you get a literal `C:/` directory in the repo.
9. **Physical keyboard scanning is OFF**: `src/drivers/Keyboard.h:82` `CONNECTED_COLS = 0`. All firmware input flows through `SerialBridge`. (`Config.h:68` saying 3 is stale.)
10. **CAS unit tests do not run anywhere** (gated behind `-DCAS_RUN_TESTS`, never set). Do not claim "tests pass" for cas:: changes; either port them to a host build or state the gap.
11. **Docs may be stale** — PROJECT_BIBLE.md has known wrong claims (buffer sizes, USB mode, SPI freq, app/mode lists, "Active Production" CAS, "no NeoLang interpreter"). Trust source + `docs/specs/*` over other docs.

## Build & test commands

```bash
# Emulator (Linux/macOS; needs libsdl2-dev; ~1 min clean build)
export PLATFORMIO_BUILD_DIR=.pio/build
pio run -e emulator_pc
# Run a script headless + deterministic:
SDL_VIDEODRIVER=dummy .pio/build/emulator_pc/program --headless --deterministic \
  --script tests/emulator/scripts/calc_1_plus_2.numos --frames 400 \
  --screenshot /tmp/out.ppm --quiet   # exit 0 = all asserts passed

# Golden pipeline
python scripts/generate-emulator-candidates.py          # 43 candidate PPMs
python scripts/compare-ppm.py CAND.ppm tests/emulator/golden/NAME.ppm \
  --mask-file tests/emulator/masks/NAME.mask            # exit 0 = identical (after mask)
python scripts/promote-emulator-golden.py <stem>        # HUMAN-ONLY blessing

# Invariant guards
python scripts/check-keycode-digit-patterns.py          # must print OK
g++ -std=c++17 tests/host/keycode_digit_test.cpp -o t && ./t

# Firmware (needs espressif32 toolchain; not runnable in most sandboxes)
pio run -e esp32s3_n16r8            # production (Giac linked)
pio run -e esp32s3_n16r8_validate   # + MathRenderVisualTestApp + render trace
```

CI: `compile-and-release.yml` (firmware build + auto-release on `main` push — no runtime checks) and `emulator-build.yml` (keycode guard → build → boot/determinism smokes → semantic calc suites → Grapher no-hang gates → candidate/golden compare). Both green at audit time.

## Fragile files (require justification + specific verification)

- `src/input/KeyCodes.h` — append-only; run keycode guard + full emulator suite.
- `src/ui/MathRenderer.*`, `src/math/MathAST.h` (FontMetrics/layout), `src/math/font/*`, `src/fonts/stix_*.c` (generated) — any change = re-candidate + review + re-bless goldens.
- `src/SystemApp.cpp` teardown block + every app `end()` — hang/UAF class; run Phase 9F + home-return scripts.
- `src/hal/NativeHal.cpp` script runner — exit-code and command grammar are CI contracts; append-only.
- `platformio.ini` emulator filter / `lib_ignore` — a wrong line links Giac natively or silently drops CI coverage.
- `src/math/giac/GiacBridge.cpp`, `lib/giac/**` — vendored-frozen; global context; needs `-fexceptions`.
- `main.cpp:127-164` — buffer + LVGL + indev init order (indev must be created after display).
- `src/math/cas/{SymExprArena.h,CasMemory.h,PSRAMAllocator.h}` — arena lifetime contracts.
- `tests/emulator/golden/**`, `masks/**`, `.github/workflows/emulator-build.yml` — CI ground truth.

## Current priority roadmap (from the audit's problem inventory)

1. **P-01** Port the 8 cas:: unit suites to a host g++ target and gate them in `emulator-build.yml` (today they never run).
2. **P-02** Giac golden-answer harness (≥50 input→output pairs via serial on validate firmware) — the flagship feature has zero tests.
3. **P-05** Enable Equations → Calculus → Matrices in the emulator (whitelist + NativeHal modes + scripts/goldens) so CAS UIs become testable.
4. **P-09** CI compile-check for `esp32s3_n16r8_validate` (flag bitrot).
5. **P-10** Apply `NUMOS_PROJECT_BIBLE_DELTA.md`.
6. **P-07/P-08** Dead-code & cruft sweep (`TutorApp`, `IntegralApp`, `src/lua`+LuaVM stub decision, `lib/*.ts`, stale logs, wokwi files, `compile_commands.json`); make `build_dir` portable.
7. **P-03/P-14** Hardware: keyboard bring-up (`CONNECTED_COLS`), verify `powerOff()` wake pin — human/HIL required.

## How to interpret emulator vs firmware results

- Emulator green ⇒ UI logic, math semantics (VPAM + whitelisted cas::), input flow, and pixel layout are correct **under libc malloc, SDL timing, no Giac, no DMA/PSRAM**. It says nothing about: DMA buffer rules, PSRAM cache/allocation behavior, Giac output or memory, physical keyboard, deep-sleep, real serial backends.
- Firmware CI green ⇒ everything (incl. Giac) **compiles and links** for the S3. It executes nothing.
- A bug reproducible in the emulator is safe to fix+verify there; a bug that only manifests on hardware (display artifacts, boot hang, PSRAM crash, Giac) needs the device — do not claim verification from the emulator for these.
- Determinism: always pass `--deterministic` for anything compared or asserted; wall-clock runs are for interactive use only.
- Emulator app ids: launch via `open_app <Name>`; internal ids differ from firmware in one case (MathShowcase = 100, emulator-only app reusing `MathRenderVisualCases`).

*(≈1,100 words. Full detail: `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` §A-§P; risks: `NUMOS_RISK_REGISTER.md`; doc corrections: `NUMOS_PROJECT_BIBLE_DELTA.md`.)*
