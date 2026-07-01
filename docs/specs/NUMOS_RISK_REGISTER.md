# NumOS — Risk Register

> Standalone register derived from the 2026-07-01 architecture audit (`NUMOS_ARCHITECTURE_GROUND_TRUTH.md`). Likelihood/Impact: L/M/H. "Owner" = suggested responsible party class (Human = repo owner; Fable/Opus/Sonnet = model tier for delegated work). "Tests" = concrete guard that detects or should detect the risk.

## 1. Memory risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| MEM-1 | LVGL draw buffer moved to PSRAM or double-buffered → black screen / StoreProhibited / pipelining deadlock | L (guarded by comments) | H (boot failure) | hardware boot log `[BOOT] Draw buffer:` (`main.cpp:148-150`) | keep single 32 KB `MALLOC_CAP_INTERNAL\|MALLOC_CAP_DMA` (`main.cpp:138-142`); document in context header | Human | none automated (HIL gap) |
| MEM-2 | cas:: arena misuse — pointers outliving `SymExprArena::reset()` / `CasMemoryPool::reset()` | M | H (UAF, corruption) | ASAN on emulator build; crash on device | enforce lifecycle contract (`CasMemory.h:301-307`); soft-reset pattern like NeoLanguage (>70%, `NeoLanguageApp.h:86`) | Opus | port CAS suites to host + ASAN (P-01) |
| MEM-3 | PSRAM exhaustion by sim apps (Fluid2D ~200 KB, ParticleLab ~MBs, Python 1 MB heap, Giac unbounded) when combined with CAS sessions | M | M (alloc failures mid-session) | heartbeat heap log (`main.cpp:233-236`, internal only — PSRAM not logged) | add `ESP.getFreePsram()` to heartbeat; per-app budget notes | Sonnet (logging) | on-device soak (manual) |
| MEM-4 | LVGL heap exhaustion if many apps `begin()` eagerly | L (lazy-init in place) | H | boot hang | keep lazy `load()->begin()` (`SystemApp.cpp:101-102` rationale) | — | boot smoke (emu Phase 3A) |
| MEM-5 | Emulator allocator ≠ firmware allocator (libc vs heap_caps cutoff) masks fragmentation/PSRAM bugs | H (by design) | M (false confidence) | n/a | treat emulator green as UI/logic signal only (`platformio.ini:170-176`) | doc-only | — |
| MEM-6 | `EquationsApp::end()` forgetting `steps.clear()` → PSRAM leak across sessions | M | M | PSRAM trend over app open/close cycles | keep the documented rule (PROJECT_BIBLE §3.5); grep-guard in review | Sonnet | emu semantic test once EquationsApp emulator-enabled (P-05) |

## 2. Correctness risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| COR-1 | Giac output wrong/regressed — zero tests | M | H (flagship feature) | user reports only | golden-answer serial harness on validate firmware (P-02) | Human+Fable | to be created |
| COR-2 | cas:: regressions invisible (suites never run) | H | H | none today | host-port suites into CI (P-01) | Sonnet/Opus | 8 suites + MathEnginePhaseRegression |
| COR-3 | Engine semantic divergence: same expression, different result in MathEvaluator vs legacy Evaluator vs Giac (angle mode, log base, implicit mult) | M | M-H | cross-checking only | single source of truth for angle mode; cross-engine differential tests | Opus | new: evaluate N expressions through both engines, assert equal |
| COR-4 | `CASNumber→ExactVal` demotes promoted BigInt to double silently | M | M (wrong exact display) | none | flag approximation in UI (P-11) | Opus | BigIntTest extension |
| COR-5 | Grapher grammar gap: VPAM-editable expression serializes to something legacy parser mishandles → NAN plot or wrong curve | M | M | Phase 9F guards catch hangs, not wrong curves | document grammar; per-function value asserts | Sonnet | extend `.numos` with `assert`-style table checks |
| COR-6 | Dual variable stores drift (`VariableManager` vs `VariableContext`) | H (structural) | M | user confusion | unify (P-04) | Opus | `assert_variable` both paths |
| COR-7 | KeyCode misuse recurrence in new code | L (CI-guarded) | M | CI static scan | append-only enum; `keyCodeDigitValue` | — | `check-keycode-digit-patterns.py` + host test (gating, `emulator-build.yml:96-104`) |

## 3. Rendering risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| REN-1 | Geometry/font/metric change silently alters layout → goldens fail (or worse: no golden covers it) | H (any renderer work) | M-H | golden compare in CI | candidate→review→promote workflow only | Fable/Opus | 20 goldens + 43-candidate generation |
| REN-2 | Missing STIX glyph → tofu for new symbols (subsetted fonts) | M | M | visual only | extend font-gen ranges + regenerate (`scripts/generate_stix_math_font.sh`, `extract_stix_math.py`) | Sonnet | `calc_delimiters_smoke`; StixGlyphGallery (manual, `NUMOS_STIX_DIAGNOSTICS`) |
| REN-3 | Deep nesting exceeds `MAX_RENDER_DEPTH=12` → clipped/placeholder rendering | L | L | stress fixtures | `MathStressExpressions` + `NUMOS_MATH_STRESS_DIAGNOSTICS` run | — | validate env manual pass |
| REN-4 | LVGL version float (`^9.2.0` → 9.5.0 today) changes rendering/behavior on rebuild | M | M | goldens break "for no reason" | pin exact LVGL version | Sonnet | CI golden compare (would catch) |
| REN-5 | Validate-env flag bitrot (`NUMOS_MATH_*` code paths not compiled by CI) | M | M | none | add compile-check job (P-09) | Sonnet | new CI matrix entry |

## 4. Emulator / CI risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| EMU-1 | NativeHal/SystemApp registry drift — app behavior differs between targets | H (two hand-written dispatchers) | M | 9A/9B parity scripts (partial) | update-both rule; parity checklist in PR template | Sonnet | extend parity suite per app |
| EMU-2 | Golden re-bless without review normalizes a regression | M | H (quality bar erodes) | human diff review only | `promote-emulator-golden.py` stays manual; require diff PPM inspection | Human | process, not code |
| EMU-3 | `build_dir` Windows path breaks local tooling on mac/Linux (literal `C:/` dir observed in this audit) | H (every non-Windows clone) | L-M | immediate | export `PLATFORMIO_BUILD_DIR=.pio/build`; consider removing `platformio.ini:6` (P-08) | Sonnet | CI already overrides |
| EMU-4 | Script vocabulary change breaks 71 `.numos` scripts | L | M | CI replay | append-only command grammar (`NativeHal.cpp:1447-1612`) | — | full suite in CI |
| EMU-5 | Firmware-only apps have zero automated coverage (13 apps) | H (structural) | M | none | emulator enablement wave (P-05) starting with Equations/Calculus/Matrices | Sonnet | new scripts+goldens per app |
| EMU-6 | CAS-flag builds untested: `-DCAS_RUN_TESTS` may no longer even compile | M | L-M | none | include in P-01/P-09 | Sonnet | compile check |

## 5. Hardware risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| HW-1 | Keyboard bring-up blocked/regressed: driver compiled off (`Keyboard.h:82` `CONNECTED_COLS=0`), `Config.h:68` contradicts it | certain (current state) | H for device usability | code inspection | staged enable (3 cols) + `HardwareTest.cpp` interactive test | Human | on-device matrix test |
| HW-2 | `powerOff()` deep-sleep wake pin references legacy matrix constant → device that can't wake | M | H | on-device only | audit `SystemApp::powerOff` vs current wiring (P-14) before shipping sleep | Human | manual sleep/wake cycle |
| HW-3 | GPIO conflicts on future wiring changes (history: GPIO 4/5 TFT vs keyboard, fixed 2026-03-02 `Config.h:55-57`) | M | M | boot artifacts | keep `Config.h` as single pin registry; cross-check `platformio.ini` TFT pins | Human | HardwareTest |
| HW-4 | Battery/charging absent while UI implies it (StatusBar icon) | certain | L (cosmetic) → M (product) | — | implement ADC or hide icon (P-13) | Human | — |
| HW-5 | SPI 40 MHz margin: older docs claim artifacts above 10 MHz; current build runs 40 MHz (`platformio.ini:71`) | L-M | M | visual artifacts on device | keep per-panel validation note; fall back to lower f if a new panel batch artifacts | Human | manual |

## 6. Onboarding / DX risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| DX-1 | PROJECT_BIBLE materially stale (≥12 claims) misleads new contributors/agents | H | M | this audit | apply `NUMOS_PROJECT_BIBLE_DELTA.md` (P-10) | Sonnet | doc review |
| DX-2 | Committed cruft (23 MB `compile_commands.json`, stale UTF-16 error logs) suggests broken build to newcomers | H | L | repo browse | delete + gitignore (P-07) | Haiku-class | build stays green |
| DX-3 | Two same-named `MathTypography.h` headers | M | L | include confusion | rename/merge (P-15) | Sonnet | build |
| DX-4 | Spanish/English mixed comments raise cost for non-Spanish contributors | H | L | — | glossary in context header; no mass rewrite (churn risk) | doc-only | — |
| DX-5 | `run-emulator.sh -t exec` cannot forward program args (surprising vs linux script) | M | L | user friction | document; prefer `run-emulator-linux.sh` | doc-only | — |

## 7. Product / release risks

| ID | Risk | Likelihood | Impact | Detection | Mitigation | Owner | Tests |
|---|---|---|---|---|---|---|---|
| PRD-1 | Release binary is auto-published from every `main` push with zero runtime verification (`compile-and-release.yml:94-103`) | H | M-H (users flash unvetted builds) | user reports | gate release on emulator workflow success; manual release tag | Sonnet (CI) | workflow dependency |
| PRD-2 | Flagship claim ("Giac CAS") experimental while marketed (README badges) | M | M (trust) | — | keep README honesty (already present); align Bible (P-10) | Human | — |
| PRD-3 | 13 launcher-visible apps unreachable by tests → shipped regressions in Chemistry/Circuit/etc. | H | M | none | either hide unfinished cards behind a flag or add coverage (P-05) | Human decision | — |
| PRD-4 | YH4F submission doc (`YH4F_SUBMISSION.md`) diverging from repo state over time | M | L-M | periodic re-read | date-stamp claims; link to ground-truth doc | doc-only | — |
| PRD-5 | Single-maintainer bus factor; Windows-only local firmware flow | H | H (long-term) | — | this ground-truth doc set; CI as executable documentation | Human | — |
