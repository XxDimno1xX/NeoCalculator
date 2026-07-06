# NumOS — PROJECT_BIBLE.md Update Delta

> Patch-style plan to bring `docs/PROJECT_BIBLE.md` (898 lines, "Last Update: April 2026") in line with source as of `main` @ `8662f47` (2026-07-01). **Do not apply without explicit instruction.** Each item cites the Bible line and the contradicting/confirming source. Line numbers refer to the current files on this branch.

Legend: **[CORRECT]** = replace text · **[REMOVE]** = delete stale claim · **[ADD]** = new section/row · **[KEEP]** = verified accurate (listed only where it is surprising).

---

## 1. Header block (PROJECT_BIBLE.md:1-9)

1.1 **[CORRECT]** `PROJECT_BIBLE.md:6` — "**CAS Engine**: Active Production"
- Evidence: README.md Giac row says "Experimental / in progress — not a validated CAS"; zero automated tests exercise Giac or the cas:: suites (no `-DCAS_RUN_TESTS` in `platformio.ini` or `.github/workflows/*`).
- Replacement: `**CAS Engine**: Giac 1.9.0 (firmware-only, experimental) + in-tree cas:: engine (edu-steps/solvers; unit suites compile-gated, not run in CI)`

1.2 **[REMOVE]** `PROJECT_BIBLE.md:9` — "Website … also available as the repo `index.html` and CNAME"
- Evidence: no `index.html` or `CNAME` at repo root (`ls` verified 2026-07-01).

1.3 **[KEEP]** "UI: LVGL 9.5" — accurate: `lvgl/lvgl@^9.2.0` (`platformio.ini:107`) currently resolves to 9.5.0 (`.pio/libdeps/emulator_pc/lvgl/library.json`). Recommend adding "(pinned as ^9.2.0 — floats)".

## 2. §2 Software Architecture (PROJECT_BIBLE.md:48-241)

2.1 **[CORRECT]** `PROJECT_BIBLE.md:104` — "heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_8BIT, 6400) × 2 — DMA"
- Evidence: `src/main.cpp:138-142` — **single** 32 KB buffer, `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`, `lvBuf2 = nullptr`; comment at `main.cpp:135-136` explains double-buffer deadlock.
- Replacement: "4. `heap_caps_malloc(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA, 32768)` — **single** 32 KB internal DMA buffer (double-buffering disabled: LVGL 9.x pipelining deadlock; PSRAM forbidden: S3 SPI-DMA cannot source from OPI-PSRAM)."
- Also update the matching claim "LVGL 9.5.0 double DMA buffer" at `PROJECT_BIBLE.md:529`.

2.2 **[CORRECT]** `PROJECT_BIBLE.md:128-131` — Mode enum listing 12 values
- Evidence: `src/SystemApp.h:76-103` — 24 values including `APP_PERIODIC_TABLE, APP_BRIDGE_DESIGNER, APP_CIRCUIT_CORE, APP_FLUID_2D, APP_PARTICLE_LAB, APP_NEURAL_LAB, APP_OPTICS_LAB, APP_NEO_LANGUAGE, APP_FRACTAL`, conditional `APP_MATH_VISUAL`, plus `STEP_VIEW`; also `APP_TABLE` exists as placeholder.
- Replacement: paste the actual enum from `SystemApp.h:76-103`.

2.3 **[ADD]** after §2.2 — launcher truth: `MainMenu::APPS[]` (`src/ui/MainMenu.cpp:88-115`) defines 20 cards (ids 0-19) + conditional id 20 (`NUMOS_MATH_VISUAL_VERIFY`); `SystemApp::launchApp` dispatch cases 0-20 (`SystemApp.cpp:870-891`). Note explicitly that the `initApps()` "11-18 hidden/experimental" comment (`SystemApp.cpp:174`) applies only to the legacy non-LVGL grid, not to the LVGL launcher.

2.4 **[CORRECT]** `PROJECT_BIBLE.md:159-163` — the `end()` StatusBar rule lists 16 apps
- Evidence: 20 apps exist (§F of ground truth); NeuralLab, OpticsLab, NeoLanguage, Fractal missing from the list.
- Replacement: "…Every app (all 20 registered apps) follows this pattern."

## 3. §3 CAS Engine (PROJECT_BIBLE.md:243-365)

3.1 **[KEEP, clarify]** `PROJECT_BIBLE.md:245` — "canonical symbolic backend now runs on Giac C++ through `src/math/giac/GiacBridge.cpp`" — file exists (441 lines). Add the operating constraints: firmware-only (`#ifdef ARDUINO`, `src/math/GiacBridge.h:19-22`; emulator `lib_ignore` `platformio.ini:192-194`), real mode (`complex_mode(false)`, `GiacBridge.cpp:197`), exact mode, `eval_level=1` (`GiacBridge.cpp:192-211`).

3.2 **[CORRECT]** `PROJECT_BIBLE.md:253-271` module table — states `CASRational.h/.cpp` and `SymToAST.h/.cpp`; verify: `CASRational` is header + `CASNumber.cpp` (no `CASRational.cpp` — `src/math/cas/` listing) and `SymToAST.h/.cpp` exists. Fix filenames against the actual directory listing.

3.3 **[ADD]** BigInt clarification: in-tree `CASInt` promotes to **mbedtls_mpi** (`CASInt.h:54`), Arduino-only (`CAS_HAS_BIGINT=0` native, `CASInt.h:58`); **libtommath serves Giac** via `USE_GMP_REPLACEMENTS` (`lib/giac/library.json`). The Bible currently implies one bignum story.

## 4. §4 Modules — Complete Inventory (PROJECT_BIBLE.md:368-467)

4.1 **[CORRECT]** Apps table (`PROJECT_BIBLE.md:413-431`): missing rows for `NeuralLabApp`, `NeoLanguageApp`, `FractalApp`, `TutorApp` (dead), `IntegralApp` (dead), `MathRenderVisualTestApp` (validate-only). PythonApp row says "Placeholder UI … pending Phase 8" — the app now has a working 3-tab IDE with a **custom** interpreter (`src/apps/PythonEngine.cpp` header: "simulated Python execution environment"); still not MicroPython. Replace status with "custom mini-interpreter (not MicroPython)".

4.2 **[CORRECT]** `PROJECT_BIBLE.md:455` — "`KeyMatrix` | `input/KeyMatrix.cpp/.h` | 6×8 scan, debounce, autorepeat"
- Evidence: KeyMatrix is legacy and never instantiated (`main.cpp:35` comment "no instanciado"); the real driver is `drivers/Keyboard` 5×10 (`main.cpp:36,67`) — and its scanning is currently disabled (`Keyboard.h:82` `CONNECTED_COLS = 0`).
- Replacement row: "`Keyboard` | `drivers/Keyboard.cpp/.h` | 5×10 matrix scan (currently `CONNECTED_COLS=0` — disabled; input via SerialBridge)" and mark KeyMatrix "legacy, unused".

4.3 **[CORRECT]** `PROJECT_BIBLE.md:458` — "`KeyCodes.h` | `KeyCode` enum — 48 keys" → the enum now has 60+ entries incl. `F5, EXE, LN, LOG, LOG_BASE, CONST_PI, CONST_E, ANS, PREANS, STO, ALPHA_A-F, NEGATE, FACT, LESS, GREATER` (`KeyCodes.h:34-117`). Add the **non-contiguous digits** warning and `keyCodeDigitValue()` (`KeyCodes.h:119-141`).

4.4 **[CORRECT]** Tests table (`PROJECT_BIBLE.md:462-467`): lists 3 files with ✅. Reality: 11 root test files; 8 CAS suites gated behind never-set `-DCAS_RUN_TESTS`; `TokenizerTest_temp.cpp` and `MathEnginePhaseRegression.cpp` orphaned; the actively-running tests are `tests/host/keycode_digit_test.cpp` + `tests/emulator/` (71 scripts / 20 goldens / 19 masks) in `emulator-build.yml`. Replace the table accordingly.

## 5. §5 Build Configuration (PROJECT_BIBLE.md:470-519)

5.1 **[CORRECT]** `PROJECT_BIBLE.md:484-485` — "`-DARDUINO_USB_MODE=1`, `-DARDUINO_USB_CDC_ON_BOOT=1`"
- Evidence: `platformio.ini:45-47` — production env sets `ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=0`, `NUMOS_SERIAL_BACKEND_UART0=1`; USB-CDC exists only in `esp32s3_n16r8_validate_usbcdc` (`platformio.ini:139-150`).

5.2 **[CORRECT]** `PROJECT_BIBLE.md:488` — "`-DSPI_FREQUENCY=10000000 ; 10 MHz — without: artifacts`"
- Evidence: `platformio.ini:71-72` — `SPI_FREQUENCY=40000000` (40 MHz) in production. Keep the historical 10 MHz note only as a troubleshooting fallback.

5.3 **[CORRECT]** buffer snippet `PROJECT_BIBLE.md:514-519` (6400-byte example) → same fix as 2.1 (32 KB, INTERNAL|DMA).

5.4 **[ADD]** The five other environments are entirely undocumented: `esp32s3_n16r8_validate` (+`NUMOS_MATH_VISUAL_VERIFY`, `NUMOS_MATH_RENDER_TRACE_ONCE`), `_overlay` (+`NUMOS_MATH_INK_OVERLAY`), `_sup1` (+`NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX=1`), `_usbcdc` (serial backend swap), and `emulator_pc` (SDL2, `lib_ignore giac/libtommath`, whitelist `build_src_filter`) — source `platformio.ini:116-266`.

5.5 **[ADD]** `build_dir = C:/.piobuild/numOS` (`platformio.ini:6`) and the `PLATFORMIO_BUILD_DIR=.pio/build` override used by CI (`emulator-build.yml:54-55`) and non-Windows users.

## 6. §6 Current State (PROJECT_BIBLE.md:523-563)

6.1 **[CORRECT]** Build stats (`PROJECT_BIBLE.md:551-556`, RAM 29.7% / Flash 23.2% / 1,518,269 B) — predates Giac linkage and 8 apps; flash figure cannot be current with a ~185k-line CAS compiled in. Mark "historical (pre-Giac)" or re-measure via `pio run -e esp32s3_n16r8 -v`.

6.2 **[CORRECT]** "✅ 85+ CAS tests — all passing (disabled in production)" (`PROJECT_BIBLE.md:546`) → "compile-gated (`-DCAS_RUN_TESTS`), not executed by any CI; last verified run unknown".

6.3 **[ADD]** to *In production*: the emulator + CI pipeline (arguably the biggest infrastructure change since April): SDL2 emulator, deterministic `.numos` replay, 20 visual goldens with masks, semantic assertion suites, two GitHub workflows. Source: `src/hal/NativeHal.cpp`, `tests/emulator/`, `.github/workflows/emulator-build.yml`.

6.4 **[CORRECT]** *Pending* list: "Table App" still true (`Mode::APP_TABLE` placeholder, `SystemApp.h:80`); "PythonApp scripting engine (Lua/MicroPython — Phase 8)" → reword per 4.1; add "physical keyboard bring-up (`CONNECTED_COLS=0`)".

## 7. §11 Troubleshooting (PROJECT_BIBLE.md:748-766)

7.1 **[KEEP]** — the deferred-teardown and StatusBar rows match implementation (`SystemApp.cpp:181-239`). Add one row: "Digits ignored in an app | unsafe `NUM_0..NUM_9` range/arith on KeyCode | use `keyCodeDigitValue()`; CI guard `scripts/check-keycode-digit-patterns.py`".

## 8. §12 NeoLanguage (PROJECT_BIBLE.md:774-899)

8.1 **[CORRECT]** `PROJECT_BIBLE.md:895` — "The parser produces an AST but there is no interpreter yet (Phase 2)."
- Evidence: `src/apps/NeoInterpreter.cpp` (1129 lines), `NeoStdLib.cpp` (1748 lines: math, plot, diff/integrate/solve hooks), `NeoEnv`, `NeoValue`, `NeoModules` all exist and are wired into `NeoLanguageApp` (persistence `/neolang.nl`, plot overlay).
- Replacement: describe Phase-2+ reality (tree-walk interpreter + stdlib + CAS hooks) and update the file map (`PROJECT_BIBLE.md:791-801`) which omits NeoInterpreter/NeoEnv/NeoValue/NeoStdLib/NeoModules/NeoIO and the domain headers (NeoFinance/NeoPhysics/NeoScientific/NeoStats/NeoUnits/NeoBitwise).

8.2 **[KEEP]** §12.8 App ID 18 — matches `SystemApp.cpp:888` and `MainMenu.cpp:108`.

## 9. Missing sections to ADD (new chapters)

9.1 **Emulator & test architecture** — none of PROJECT_BIBLE mentions the emulator. Summarize §D/§I/§K of `NUMOS_ARCHITECTURE_GROUND_TRUTH.md`; link `docs/emulator-sdl2-quickstart.md` (verified accurate).

9.2 **CI** — describe both workflows and what they do/do not prove (`compile-and-release.yml`, `emulator-build.yml`).

9.3 **Input architecture** — KeyCode invariants, KeyboardManager modifier FSM, SerialBridge map, serial backend selection (`NumosSerialBackend.h`), current hardware-scan status.

9.4 **VPAM math engine** — the Bible's §4 covers only the legacy RPN engine and cas::; `MathAST`/`MathEvaluator`/`ExactVal`/`CursorController`/STIX rendering (the actual CalculationApp stack) are undocumented in the Bible (they are specified in `docs/NUMOS_MATH_ENGINE_SPEC.md`; add a cross-reference at minimum).

## 10. Related non-Bible doc corrections (same sweep)

- `docs/MATH_ENGINE.md` header "Tests passing ✅" → "test suites exist; not run in CI" (evidence §K).
- `docs/ROADMAP.md` — refresh phase statuses; git history shows Phases 8B/8C/9A/9B/9F landed after its "last update March 2026".
- `wokwi.toml` / `diagram.json` — fix env (`esp32s3_n16r8`) and board/pins, or delete.
- Delete or `.gitignore`: `build_output.log`, `pio_error.log` (stale UTF-16 logs of a failure that no longer reproduces — the cited error at `TutorTemplates.cpp:25` does not exist in current source), `compile_commands.json` (23 MB generated).
