# NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX

Status: SPEC-ONLY (P-05 wave). Per-app enablement detail for every app in the repository.
Companions: parity spec (levels P0–P7, categories), routing spec (checklist §D, divergences D1–D14), QA plan (scripts/goldens), roadmap (tickets).

Audit result: **23 app-class file pairs** exist in `src/apps/` — 21 registered (firmware `Mode` + `launchApp` branch + launcher card) and 2 orphaned (`IntegralApp`, `TutorApp`, registered nowhere). The launcher table is `MainMenu::APPS[]` (`src/ui/MainMenu.cpp:88-113`): ids 0–19 always, id 20 "Math Visual" only under `NUMOS_MATH_VISUAL_VERIFY`. **"StixGlyphGallery" does not exist in this repository** (no such .h/.cpp; requested by the task brief but absent from source — the closest artifact is the glyph-heavy Math Showcase in `src/hal/NativeHal.cpp:1093-1182`). The emulator build links 7 app pairs (`platformio.ini:195-266`) plus the NativeHal-resident Math Showcase.

Parity levels (defined in parity spec §E): P0 card-visible … P7 stress-passed.

Legend used below: "canonical script name" = the token for `open_app`/`assert_app` (`NativeHal.cpp:1414-1428, 1656-1668`).

---

## Group 1 — Currently emulator-enabled apps

### 1.1 Calculation
- **Source**: `src/apps/CalculationApp.h` (184 L) / `.cpp` (1126 L).
- **Firmware**: Mode `APP_CALCULATION` (`SystemApp.h:78`); launchApp id 0 (`SystemApp.cpp:727-731`); handleKey case (`:489-496`, owns SHIFT/ALPHA per `:474-483`); card id 0 (`MainMenu.cpp:90`).
- **Emulator**: FULLY WIRED. Built (`platformio.ini:197`); AppMode CALCULATION; eager `begin()` (`NativeHal.cpp:872-873`); launchApp case 0 (`:923`); dispatch (`:615-632`); canonical name `Calculation`.
- **Persistence**: `vm.saveToFlash()` after STO (`CalculationApp.cpp:900`) → `emulator_data/vars.dat` via FS shim (`src/hal/FileSystem.cpp:38-88`). **Writes a non-gitignored-by-fixture file — the known fixture-hygiene offender** (STORE-01).
- **Timers**: none. **MathRenderer**: yes. **CAS**: in-tree `cas::` for edu-steps (`CalculationApp.cpp:36-38`), not Giac. **Fonts**: stix_math + montserrat.
- **Determinism risk**: cursor blink (frame-indexed stable under `--deterministic`, only StatusBar clock varies per `tests/emulator/masks/README.md`).
- **Existing coverage**: richest — 2 golden-gated scripts + 14 assert-only `calc_semantic_*`/error/store scripts + input-extended (see QA plan §A).
- **Semantic hooks**: `debugHasResult()`/`debugLastResult()` — the only app with result hooks (`NativeHal.cpp:1733-1794`).
- **Status / phase**: **P5–P6 achieved. Risk low.** Remaining gap to P6: STO persistence sandboxing; P7 needs LIFE-01 loop. No P-05 work except STORE/LIFE tickets.

### 1.2 Grapher
- **Source**: `src/apps/GrapherApp.h` (395 L) / `.cpp` (3929 L, largest app) + `GraphModel.cpp`, `ui/GraphView.cpp`.
- **Firmware**: Mode `APP_GRAPHER` (`SystemApp.h:79`); id 1 (`SystemApp.cpp:732-736`); dedicated `handleKeyGraph()` with AC-at-tab-level exit (`:1002-1016`); card id 1.
- **Emulator**: FULLY WIRED (`platformio.ini:233-247`); lazy begin (`NativeHal.cpp:896`); case 1 (`:929`); canonical name `Grapher`.
- **Timers/async**: `_tplLoadTimer` (`GrapherApp.cpp:1530`) and **async POI timer, 10 ms** (`GrapherApp.cpp:3518`) — the async-POI replot is the known nondeterminism trap (safe golden path = default Expressions tab).
- **MathRenderer**: heaviest user. **CAS**: none (numeric RPN pipeline). **Persistence**: none direct (shared VariableContext).
- **Existing coverage**: 6 golden-gated + 5 warn-only + 9F no-hang suite + 21 tracked-but-unwired scripts + 6 untracked Phase-10G scripts (QA plan §A/§B).
- **Status / phase**: **P5 achieved on curated paths; risk medium** (async POI, libm portability per host-portability spec). P-05 work: wire the orphaned scripts (APPQA tickets), no enablement work.

### 1.3 Settings
- **Source**: `src/apps/SettingsApp.h` (70 L) / `.cpp` (321 L, smallest).
- **Firmware**: Mode `APP_SETTINGS` (`SystemApp.h:101`); id 10 (`SystemApp.cpp:777-781`); card id 10.
- **Emulator**: FULLY WIRED (`platformio.ini:200`); case 10 (`NativeHal.cpp:969`); values are `setting_*` globals defined in `NativeHal.cpp:103-105` (no NVS) — a documented emulator-only shim; the global-settings spec owns its future persistence.
- **Risks**: none material. **Coverage**: `settings_smoke` golden-gated.
- **Status / phase**: **P5. Risk low.** P6 blocked on global-settings persistence decisions (see `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md`).

### 1.4 Statistics / 1.5 Probability
- **Source**: `StatisticsApp` (125/673 L) + `StatsEngine.cpp`; `ProbabilityApp` (108/507 L) + `ProbEngine.cpp`.
- **Firmware**: Modes at `SystemApp.h:81-82`; ids 4/5 (`SystemApp.cpp:747-756`); cards 4/5.
- **Emulator**: FULLY WIRED (`platformio.ini:206-209`); cases 4/5 (`NativeHal.cpp:937, 945`). Pure `<cmath>` engines, no persistence/timers/CAS.
- **Known parity gap**: digit-entry bug — both apps ignore all digit keys (broken `NUM_0..NUM_9` range test on a non-contiguous enum; see memory `stats-prob-digit-entry-bug`); this blocks scripted numeric entry and therefore semantic asserts.
- **Coverage**: 2 golden-gated scripts each. **Status**: **P5 visually, but P3 unreachable until the digit bug is fixed (firmware bug, not emulator divergence). Risk low.**

### 1.6 Regression / 1.7 Sequences
- **Source**: `RegressionApp` (135/877 L) + `RegressionEngine.cpp`; `SequencesApp` (113/482 L, self-contained sscanf recompute).
- **Firmware**: Modes `SystemApp.h:86-87`; ids 6/7 (`SystemApp.cpp:757-766`); cards 6/7.
- **Emulator**: FULLY WIRED (`platformio.ini:215, 222-223`); cases 6/7 (`NativeHal.cpp:953, 961`). No persistence/timers/CAS.
- **Coverage**: 2 golden-gated scripts each. **Status**: **P5. Risk low.** P6 gap: no semantic hooks (fit coefficients / sequence terms unverifiable).

### 1.8 Math Showcase (emulator-only, not an `src/apps` module)
- **Source**: `src/hal/NativeHal.cpp:1093-1182` (curated cases from `math/MathRenderVisualCases.cpp`, built natively per `platformio.ini:250`).
- **Emulator**: AppMode `MATH_SHOWCASE`, id 100 (`NativeHal.cpp:250-251`), no launcher card, `open_app MathShowcase` only; cursor blink disabled for determinism (`:1133-1134`); caption uses `lv_font_montserrat_14` (stix fonts lack U+0020).
- **Firmware**: no counterpart (firmware equivalent is MathRenderVisualTestApp, §3.1). Documented emulator-only shim.
- **Coverage**: golden-gated `math_showcase_smoke`; `math_showcase_delims_smoke` exists but is unwired.
- **Status**: **P5. Risk low.** Not a parity target; excluded from firmware-parity checklists by design.

---

## Group 2 — P-05 candidate apps (not yet emulator-enabled)

Template fields apply to each; the routing checklist (routing spec §D) is implied and not repeated.

### 2.1 Matrices — **recommended P-05, Wave 1**
- **Source**: `src/apps/MatricesApp.h` (160 L) / `.cpp` (927 L) + `MatrixEngine.cpp`.
- **Firmware**: Mode `APP_MATRICES` (`SystemApp.h:85`); id 9 (`SystemApp.cpp:772-776`); handleKey (`:545-551`, MODE-only exit); card id 9 (`MainMenu.cpp:99`).
- **Emulator today**: card visible, dead (`NativeHal.cpp:983-985`). P0.
- **Required work**: build_src_filter `+<apps/MatricesApp.cpp>` `+<apps/MatrixEngine.cpp>`; AppMode `MATRICES`; lazy construct; launchApp case 9; standard dispatch case (MODE-only, matches firmware); names `Matrices`.
- **Persistence**: none. **Timers**: none. **MathRenderer**: no. **CAS/Giac**: none. **Fonts**: stix_math (14 hits) — beware missing-space-glyph tofu in labels.
- **Native-safety check required**: grep for unguarded ESP-heap/Arduino calls in MatricesApp/MatrixEngine before wiring (pattern that bit Grapher in Phase 8B; verification is part of the APPPAR-04 ticket, not assumed).
- **Determinism risk**: low (no timers, no rand).
- **Proposed QA**: `matrices_smoke.numos` (open → assert_app → screenshot), then `matrices_edit_smoke` (enter a 2×2, compute det) once digit entry is verified; semantic hook `debugResultCell(r,c)` before golden promotion of the result screen.
- **Firmware parity checklist**: MODE exit; no AC-exit in firmware (`SystemApp.cpp:545-551`) — emulator must NOT add one.
- **Risk: low. Phase: P-05 Wave 1.** Rationale: same shape as Stats/Prob/Regression, which enabled cleanly.

### 2.2 Periodic Table ("Chemistry") — **recommended P-05, Wave 1**
- **Source**: `PeriodicTableApp.h` (153 L) / `.cpp` (1121 L) + `ChemDatabase.h`, `ChemExtraData.h`, `ChemCAS.cpp`.
- **Firmware**: Mode `APP_PERIODIC_TABLE` (`SystemApp.h:89`); id 11 (`SystemApp.cpp:782-786`); **handleKey exits on MODE or AC** (`:569-575`) — divergence class D5; card id 11 "Chemistry" (`MainMenu.cpp:101`).
- **Emulator today**: dead card. P0.
- **Required work**: filter entries (incl. `ChemCAS.cpp` — verify it has no Giac/ESP deps); AppMode; case 11; dispatch case **replicating the AC-exit** (cite `SystemApp.cpp:569-575` in a comment); canonical name `Chemistry` (matches card so `assert_menu_focus` and `assert_app` tokens agree).
- **Persistence/timers/CAS(math)/random**: none. **Fonts**: heaviest static-font user (24 hits) — element-cell rendering; static content is golden-friendly.
- **Determinism risk**: low.
- **Proposed QA**: `chemistry_smoke` (open → screenshot table view), `chemistry_element_smoke` (navigate to a fixed element, screenshot detail). Golden candidate early: pure static content.
- **Risk: low. Phase: P-05 Wave 1.**

### 2.3 Equations — **recommended P-05, Wave 2 (math-renderer/CAS wave)**
- **Source**: `EquationsApp.h` (284 L) / `.cpp` (3350 L).
- **Firmware**: Mode `APP_EQUATIONS` (`SystemApp.h:83`); id 2 (`SystemApp.cpp:737-741`); handleKey (`:503-509`) **plus a cooperative `update()` tick** (`SystemApp.cpp:255-259`); card id 2.
- **Emulator today**: dead card; only referenced as a nav token (`menu_nav_parity.numos:42`). P0.
- **Required work**: largest link closure of any candidate — pulls `math/cas/` TUs not yet in the native filter: `SingleSolver`, `SystemSolver`, `OmniSolver`, `RuleEngine`, `TutorTemplates`, `AlgebraicRules`, `CasToVpam`, `PersistentAST`, `CasMemory`, `SystemHeuristics`, `SystemTutor`, `SymToAST` (includes at `EquationsApp.cpp:36-43`, `.h:39-51`); each must be audited for native safety. Plus AppMode/case 2/dispatch/**update() tick wiring** — NativeHal's main loop must call the app's `update()` (new requirement; no current emulator app needs a per-frame tick from the dispatcher).
- **Blockers to audit before wiring**:
  - `heap_caps_get_free_size`/`largest_free_block` diagnostics (`EquationsApp.cpp:487, 513, 516`) — ESP-IDF-only; need `#ifdef ARDUINO` guard or an ArduinoCompat shim (LIFE/PORT ticket).
  - `rand() % NUM_SOLVING_MSGS` flavor text (`EquationsApp.cpp:106`) — nondeterministic label ⇒ either avoid capturing the solving screen, mask the label, or (firmware-acceptable) index deterministically. Golden policy forbids broad masks, so the smoke must screenshot only pre/post-solve screens.
- **Persistence**: none. **Timers**: none (staged solve is cooperative via update()). **Fonts**: stix_math_18 pervasive (29 hits).
- **Proposed QA**: `equations_smoke` (open → template list screenshot); `equations_linear_smoke` (pick linear template, enter 2x+4=0, solve, `assert_result`-class hook `debugSolutionText()`); golden only for the list + result screens, never the animated solving screen.
- **Risk: medium-high** (link closure size, ESP-heap guards, rand label). **Phase: P-05 Wave 2**, after the Wave 1 pattern is proven.

### 2.4 Calculus — **recommended P-05, Wave 2**
- **Source**: `CalculusApp.h` (187 L) / `.cpp` (1130 L).
- **Firmware**: Mode `APP_CALCULUS` (`SystemApp.h:84`); id 3 (`SystemApp.cpp:742-746`); handleKey (`:510-516`; owns SHIFT/ALPHA like Calc, `:474-483`); card id 3.
- **Emulator today**: dead card, zero script references. P0.
- **Required work**: filter additions `SymDiff.cpp`, `SymIntegrate.cpp`, `SymToAST.cpp` (includes at `CalculusApp.h:54-60`, `.cpp:31`) — smaller closure than Equations since `ASTFlattener/SymSimplify/SymExprToAST/SymExpr/ConsTable/CASStepLogger/SymPoly` are already native (`platformio.ini:252-258`); AppMode; case 3; dispatch case forwarding SHIFT/ALPHA (app-owned modifiers).
- **Blockers to audit**: `rand()` flavor messages (`CalculusApp.cpp:449, 453`) — same golden restriction as Equations.
- **Persistence/timers**: none. **MathRenderer**: yes.
- **Proposed QA**: `calculus_smoke` (open, screenshot); `calculus_derivative_smoke` (d/dx of x^2 → hook `debugResultText()` → assert `2*x`-form); result-screen golden after hook exists.
- **Risk: medium.** SymDiff/SymIntegrate are pure in-tree C++ (no Giac), so closure risk is bounded. **Phase: P-05 Wave 2.**

### 2.5 Math Visual (MathRenderVisualTestApp) — **P-05 Wave 2, emulator-define variant**
- **Source**: `MathRenderVisualTestApp.h` (64 L) / `.cpp` (725 L).
- **Firmware**: only in validate builds — Mode gated by `NUMOS_MATH_VISUAL_APP_ENABLED` ⇐ `NUMOS_MATH_VISUAL_VERIFY` (`SystemApp.h:62-65, 98-100`; `platformio.ini:116-121`); id 20 (`SystemApp.cpp:828-832`); MODE/AC exit (`:650-656`); card 20 (`MainMenu.cpp:110-112`).
- **Emulator today**: excluded; the NativeHal Math Showcase (§1.8) reuses the same `MathRenderVisualCases.cpp` fixture data.
- **Decision required (Open Decision OD-3 in parity spec)**: keep Showcase as the emulator's renderer fixture (recommended) vs. also enabling the real app under a native define for 1:1 parity with validate firmware. Recommendation: **enable the app natively gated by the same `NUMOS_MATH_VISUAL_VERIFY` define in a dedicated CI variant only if the MathRenderer golden plan (`NUMOS_MATHRENDERER_TEST_ORACLE_AND_GOLDEN_PLAN.md`) demands parity with device captures; otherwise defer.** The Showcase already covers the renderer-golden need.
- **Risk: low-medium** (26 MathRenderer call sites; static content). **Phase: P-05 Wave 2 optional / P-06.**

---

## Group 3 — Timer/simulation/storage apps (P-06 wave)

Common facts: all use `lv_timer_create` sim loops, several use PSRAM with `new[]` native fallbacks, several autosave to LittleFS. Enabling any of them before the fixture sandbox (STORE-01) and timer-determinism policy land would violate the determinism spec preconditions. All are P0 today (dead cards).

### 3.1 Optics Lab — **first of Wave 3 (P-06)**
- `OpticsLabApp.h` (124)/. cpp (601) + `OpticsEngine.cpp`, `OpticsRenderer.h`. Mode `SystemApp.h:95`; id 17 (`SystemApp.cpp:812-816`); card 17.
- Timer: 33 ms sim (`OpticsLabApp.cpp:253`). **No persistence, no PSRAM, no rand** — the least-entangled sim app.
- Determinism: ray-trace is pure float math; with the deterministic tick, N frames ⇒ fixed sim steps ⇒ stable pixels (host libm variance caveat applies, see portability spec).
- QA: `optics_smoke` (open, wait fixed frames, screenshot); golden warning-only first.
- **Risk: medium. Phase: P-06.** Rationale: proves the timer-app pattern with zero storage entanglement.

### 3.2 Bridge Designer — Wave 3 (P-06)
- `BridgeDesignerApp.h` (175)/.cpp (903). Mode `SystemApp.h:90`; id 12 (`SystemApp.cpp:787-791`); card 12 "Bridge".
- PSRAM node/beam arrays with native `new[]` fallback (`BridgeDesignerApp.cpp:98-102`); 16 ms physics timer (`:836`). No persistence, no rand.
- QA: `bridge_smoke` (open, screenshot editor; do NOT run physics for the golden), `bridge_sim_steps` later with fixed-frame sim run.
- **Risk: medium. Phase: P-06.**

### 3.3 Particle Lab — Wave 3 (P-06, storage-gated)
- `ParticleLabApp.h` (131)/.cpp (776). Mode `SystemApp.h:93`; id 15 (`SystemApp.cpp:802-806`); card 15.
- PSRAM render buffer + grid buffers w/ fallbacks (`ParticleLabApp.cpp:108-110, 411, 434`); 33 ms timer (`:203`); **LittleFS `/save.pt`** (`:416, 429-431`). Deterministic sim (no rand found).
- **Blocked on STORE-01** for the save/load path; smoke without save can precede it.
- **Risk: medium. Phase: P-06.**

### 3.4 Circuit — Wave 3 (P-06, storage- and dispatcher-gated)
- `CircuitCoreApp.h` (304)/.cpp (2067, 2nd largest) + MNA solver stack. Mode `SystemApp.h:91`; id 13 (`SystemApp.cpp:792-796`); card 13.
- **Firmware dispatcher autosaves on MODE/AC exit** (`SystemApp.cpp:585-596`) — divergence D5 must be replicated. LittleFS `/circuits` + `autosave.dat` (`CircuitCoreApp.cpp:1364-1374, 1483-1488, 1712-1721`); 16 ms sim timer (`:781`). "Sensors" are simulated component models (`:1263-1286`), not hardware — no HW blocker.
- **Blocked on STORE-01** (autosave fires on every exit ⇒ guaranteed dirty tree without sandbox).
- **Risk: high** (size, autosave-on-exit, MNA float accumulation across timer steps). **Phase: P-06 late.**

### 3.5 Fluid 2D — Wave 3 (P-06 late)
- `Fluid2DApp.h` (256)/.cpp (1523). Mode `SystemApp.h:92`; id 14 (`SystemApp.cpp:797-801`, autosave on MODE `:598-605`); card 14.
- 11 PSRAM grids w/ native fallbacks (`Fluid2DApp.cpp:128-152`); 33 ms timer (`:257`); LittleFS `/fluid` + autosave (`:1455-1462, 1490, 1518`).
- Float-accumulation nondeterminism across hosts is the worst of any app ⇒ goldens likely warning-only forever except the initial (pre-sim) screen.
- **Risk: high. Phase: P-06 late** (same gates as Circuit).

### 3.6 Neural Lab — Wave 4 / future
- `NeuralLabApp.h` (172)/.cpp (1197) + `NeuralEngine.cpp`. Mode `SystemApp.h:94`; id 16 (`SystemApp.cpp:807-811`); card 16.
- **`rand()`-generated spiral dataset** (`NeuralLabApp.cpp:372, 380`) — unseeded ⇒ training screens nondeterministic by construction; PSRAM buffers w/ fallbacks (`:45, 942-975`); 33 ms train timer (`:230`); LittleFS `/neural.nn` (`:950, 965-967`).
- Enablement is possible (smoke of the initial screen only), but semantic/golden value is near zero until a seeding policy exists (firmware change — out of emulator scope).
- **Risk: high. Phase: future** (defer; revisit after a deterministic-seed decision).

---

## Group 4 — Blocked / firmware-only for now

### 4.1 Fractal — **do not enable yet (hard blocker)**
- `FractalApp.h` (166)/.cpp (961). Mode `SystemApp.h:97`; id 19 (`SystemApp.cpp:822-826`; AC-exit + `consumeExitRequest()` polling `:639-648`; `update()` state machine `:280-282`); card 19 "Fractals".
- **Blocker: FreeRTOS render task — `xTaskCreate(renderTaskWrapper, ...)` (`FractalApp.cpp:491-496`, deleted `:515`).** The native platform has no FreeRTOS; ArduinoCompat has no task shim. Options: (a) std::thread shim (imports real thread nondeterminism — contradicts determinism spec), (b) synchronous strip-render path under `NATIVE_SIM` (a firmware-source modification with divergent scheduling — needs its own mini-spec). Neither is P-05/P-06 material.
- **Risk: block. Phase: do not enable yet.** Ticket APPPAR-09 records the deferral and the two options.

### 4.2 Python — **defer (P-06 at earliest)**
- `PythonApp.h` (160)/.cpp (928) + `PythonEngine` (simulated exec, not real CPython). Mode `SystemApp.h:88`; id 8 (`SystemApp.cpp:767-771`); card 8.
- LittleFS file manager under `/py` incl. seeding `/py/hello.py`, mkdir/open/remove (`PythonApp.cpp:340, 657-696`) ⇒ **blocked on STORE-01**; shell text entry needs either alpha-key sequences or a new `text` script command (routing spec §E.2) ⇒ **blocked on input-parity work**. No timers/CAS.
- **Risk: medium (storage + input). Phase: defer to P-06; APPPAR-06 = explicit deferral ticket.**

### 4.3 NeoLanguage — future
- `NeoLanguageApp.h` (168)/.cpp (958) + full language stack (Lexer/Parser/Interpreter/StdLib/domain libs). Mode `SystemApp.h:96`; id 18 (`SystemApp.cpp:817-821`); card 18 "NeoLang".
- LittleFS `/neolang.nl` session load at boot + F2/F3 save/load (`NeoLanguageApp.cpp:116, 393-394, 897-931`); code-editor free-text input; includes `math/cas/SymExprArena.h` (`.h:54`). Link closure (entire language stack) unaudited for native safety.
- **Risk: high. Phase: future.**

### 4.4 Orphaned apps — do not enable
- **IntegralApp** (`IntegralApp.h` 159/.cpp 879): registered nowhere (no Mode, no launchApp, no card); superseded by CalculusApp; compiles on device only via the firmware `+<*>` glob. **APPPAR-03 resolves to "mark orphaned + defer"** — enabling it would add an app that firmware users cannot reach, violating the parity philosophy. If Calculus enablement (2.4) surfaces gaps in integral UI coverage, the fix is in CalculusApp, not resurrecting IntegralApp.
- **TutorApp** (`TutorApp.h` 130/.cpp 558): registered nowhere; CAS tutor prototype. Same ruling.

### 4.5 Pseudo-modes — not apps
- `APP_TABLE` (`SystemApp.h:80`) → generic "Coming Soon" placeholder (`SystemApp.cpp:902-953`); `STEP_VIEW` (`SystemApp.h:102`) → step viewer (`:958, :974`). Neither has an app class nor a launcher card; excluded from enablement. STEP_VIEW emulator support would ride on CalculationApp edu-steps work, tracked separately if ever needed.

---

## Summary matrix

| App | Card id | Emulator today | Risk | Phase | Blocking prerequisites |
|---|---|---|---|---|---|
| Calculation | 0 | P5–P6 | low | enabled | STORE-01 for P6 |
| Grapher | 1 | P5 (curated) | med | enabled | wire orphan scripts |
| Equations | 2 | P0 | med-high | **P-05 W2** | heap-caps guards, CAS closure, rand-label rule, update() tick |
| Calculus | 3 | P0 | med | **P-05 W2** | SymDiff/SymIntegrate closure, rand-label rule |
| Statistics | 4 | P5 | low | enabled | digit-entry firmware fix for P3+ |
| Probability | 5 | P5 | low | enabled | same |
| Regression | 6 | P5 | low | enabled | hooks for P6 |
| Sequences | 7 | P5 | low | enabled | hooks for P6 |
| Python | 8 | P0 | med | defer→P-06 | STORE-01, text input |
| Matrices | 9 | P0 | **low** | **P-05 W1** | native-safety grep only |
| Settings | 10 | P5 | low | enabled | settings-persistence spec |
| Chemistry | 11 | P0 | **low** | **P-05 W1** | ChemCAS native audit; AC-exit shim (D5) |
| Bridge | 12 | P0 | med | P-06 | timer policy |
| Circuit | 13 | P0 | high | P-06 late | STORE-01, D5 autosave shim |
| Fluid 2D | 14 | P0 | high | P-06 late | STORE-01, float-drift ruling |
| ParticleLab | 15 | P0 | med | P-06 | STORE-01 for save path |
| Neural Lab | 16 | P0 | high | future | rand-seed decision |
| OpticsLab | 17 | P0 | med | **P-06 first** | timer policy |
| NeoLang | 18 | P0 | high | future | STORE-01, text input, stack audit |
| Fractals | 19 | P0 | **block** | do not enable | FreeRTOS task — needs mini-spec |
| Math Visual | 20 (validate) | P0 (Showcase proxy at P5) | low-med | P-05 W2 opt. | OD-3 decision |
| Math Showcase | — (id 100) | P5 | low | enabled | none |
| IntegralApp / TutorApp | none | orphaned | — | do not enable | — |
| StixGlyphGallery | — | **does not exist** | — | — | — |
