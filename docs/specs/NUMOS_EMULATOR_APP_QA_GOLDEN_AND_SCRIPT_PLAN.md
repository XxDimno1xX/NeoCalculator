<<<<<<< HEAD
# NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN

Status: SPEC-ONLY (P-05 wave). Test plan for app enablement.
Depends on: parity spec (levels P0–P7, prerequisites), matrix (per-app facts), routing spec (script API), roadmap (tickets). Inherits the T1–T7 test-tier taxonomy from `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` and the promotion/mask rules from `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md`.
=======
# NumOS — Emulator App QA, Golden and Script Plan (P-05)

> Part of the P-05 spec set (see `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md`
> for the parity ladder P0-P7 and scope). This document is the test plan for app
> enablement. Verified claims cite `file:line`; plans are **[PROPOSED]**.
> Contracts reused, not redefined: `.numos` grammar + exit codes 0/2/3/4
> (`NativeHal.cpp:1457-1622, 1682-1688, 1855-1866, 1882-1884`), clock-rect-only
> mask policy (`tests/emulator/masks/README.md:38-54`), human-only promotion
> (`tests/emulator/golden/README.md:115-129`), candidate→pending→gated lifecycle
> (CI/golden policy spec, CI-05), per-invocation ≤30 s wall budget
> (MathRenderer test plan §6).
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## A. Existing inventory (verified)

<<<<<<< HEAD
Counts: **77 `.numos` scripts, 19 goldens, 18 masks** (all goldens except `launcher_smoke` carry the shared clock-rect mask `4,6,37,13`; `launcher_smoke` is byte-exact — `tests/emulator/masks/README.md`).

- **Golden-gated (19)**: `launcher_smoke`, `calc_1_plus_2`, `calc_fraction_sum`, `settings_smoke`, `math_showcase_smoke`, `statistics_smoke`, `probability_smoke`, `statistics_data_smoke`, `probability_edit_smoke`, `sequences_smoke`, `sequences_edit_smoke`, `regression_smoke`, `regression_data_smoke`, `grapher_smoke`, `grapher_expr_smoke`, `grapher_graph_smoke`, `grapher_table_smoke`, `grapher_trace_smoke`, `grapher_templates_smoke`. Gate: `emulator-build.yml:575-608` — masked compare when a mask exists, byte-exact otherwise, missing golden = warning (`:602-604`), mismatch fails (`:598, 607`).
- **Warning-only candidates (6)**: `menu_focus_grapher_smoke`, `grapher_implicit_x_smoke`, `grapher_implicit_2x_smoke`, `grapher_multifn_graph_smoke`, `grapher_multifn_table_smoke`, `grapher_expr_scroll_smoke` (`generate-emulator-candidates.py:129-142`, no goldens).
- **Assert-only CI (25)**: 14 Calculation (`calc_semantic_*` glob + `calc_error_div_by_zero`, `calc_store_variable` — `emulator-build.yml:308-356`), 3 input-parity 9A (`:375-408`), 2 menu-nav 9B (`:429-461`), 6 Grapher no-hang 9F (`:495-531`).
- **Not run anywhere (27)**: 21 tracked (grapher aspect/implicit/inequality/mixed/trace-domain/stress families, `calc_delimiters_smoke`, `calc_logbase_smoke`, `math_showcase_delims_smoke`, `grapher_explicit_parabola_smoke`, `grapher_curve_stress`, `grapher_breakit_stress`) + **6 untracked Phase-10G scripts** (`grapher_calcmenu_implicit_smoke`, `grapher_calcmenu_xfy_smoke`, `grapher_intersect_mixed_smoke`, `grapher_tangent_implicit_smoke`, `grapher_trace_explicit_y_smoke`, `grapher_trace_implicit_circle_smoke`). No glob picks up `grapher_*` — 8C's glob is `calc_semantic_*` only (`emulator-build.yml:341`); 9F is an explicit list. **This orphan pool is the single largest existing QA gap** (APPQA-05).
- **CANDIDATES** is hardcoded (`generate-emulator-candidates.py:39-143`, 25 stems); the generator never writes into `golden/` and validates P6/320×240/230415 bytes (`:32-33, 218-230`).
- **Per-app coverage**: see parity spec §C table. Zero-coverage registered apps: Equations, Calculus, Python, Matrices, Chemistry, Bridge, Circuit, Fluid2D, ParticleLab, NeuralLab, OpticsLab, NeoLang, Fractals, Math Visual.

## B. Per-app smoke script plan (P-05/P-06 candidates)

Deterministic requirements common to all: run under `--deterministic`; fixed `wait` counts derived from `--step-ms`; no screenshot of any screen containing rand text, sim-time-dependent pixels, or POI-async panels; candidate stems registered as *pending* in `PENDING.md` at introduction (CI-05). Failure mode column = what a red run means first.

| Script | Steps (abbrev.) | Asserts | Screenshot/candidate | Mask | Primary failure mode |
|---|---|---|---|---|---|
| `matrices_smoke.numos` | open_app Matrices → wait → shot | assert_app Matrices | `matrices_smoke.ppm` | clock rect | wiring/link regression |
| `matrices_det_smoke.numos` | define 2×2 ((1,2),(3,4)) → det | assert_app + `assert_matrices_result -2` (hook) | `matrices_det_smoke.ppm` | clock | engine or digit-entry regression |
| `chemistry_smoke.numos` | open_app Chemistry → shot table | assert_app Chemistry | `chemistry_smoke.ppm` | clock | wiring |
| `chemistry_element_smoke.numos` | navigate to fixed element → detail | assert_app + `assert_chem_element Fe` (hook) | `chemistry_element_smoke.ppm` | clock | nav-order or DB regression |
| `calculus_smoke.numos` | open_app Calculus → shot entry UI | assert_app Calculus | `calculus_smoke.ppm` | clock | CAS link closure |
| `calculus_derivative_smoke.numos` | enter x^2 → derive → wait past flavor screen → result | `assert_calculus_result 2*x` (hook) | result screen only | clock | SymDiff regression |
| `equations_smoke.numos` | open_app Equations → template list | assert_app Equations | `equations_smoke.ppm` | clock | closure/heap-guard |
| `equations_linear_smoke.numos` | linear template → 2x+4=0 → solve → result | `assert_equations_solution x=-2` (hook) | result screen only (never solving screen — rand text, `EquationsApp.cpp:106`) | clock | solver regression |
| `optics_smoke.numos` (P-06) | open → wait fixed N frames → shot | assert_app OpticsLab | `optics_smoke.ppm` | clock | timer-determinism break |
| `bridge_smoke.numos` (P-06) | open editor, physics NOT started → shot | assert_app Bridge | `bridge_smoke.ppm` | clock | wiring |
| `particlelab_smoke.numos` (P-06) | open → fixed frames → shot (no save) | assert_app ParticleLab | `particlelab_smoke.ppm` | clock | sim-step drift |
| `circuit_smoke.numos` (P-06, after STORE) | open → shot empty canvas → home (autosave fires) | assert_app Circuit + `assert_storage_clean` | initial screen only | clock | autosave/sandbox breach |
| `fluid2d_smoke.numos` (P-06 late) | open → shot pre-sim frame → home | assert_app Fluid2D | initial screen only | clock | grid alloc/native fallback |
| `python_smoke.numos` (deferred) | open → file list (seeded fixture) | assert_app Python | list screen | clock | fixture/FS regression |

Every enabled app additionally gets `<app>_relaunch_loop.numos` (LIFE-01 shape, §E).

## C. Per-app golden plan

Rules: (1) warning-only first, always — a golden is blessed only after ≥2 weeks of stable candidates; (2) mask = clock rect or nothing, never content; (3) **no computation-screen golden before its semantic hook exists** (§D) — a golden alone can ossify a wrong answer that "looks plausible".

| App | Golden? | Warn-first | Mask | Catches | Does NOT catch |
|---|---|---|---|---|---|
| Matrices | yes (grid + det result) | yes | clock | layout/font/theme churn, tofu | wrong-but-well-rendered math (hook's job) |
| Chemistry | yes (table + element detail) — best golden candidate: fully static | yes | clock | DB/render regressions | data errors outside captured element |
| Calculus | entry UI yes; result screen after hook | yes | clock | VPAM layout regressions | simplification quality |
| Equations | template list + result; never solving screen | yes | clock | list/render churn | solver internals |
| OpticsLab | initial scene only | yes, likely long-term warn (libm drift, OD-6) | clock | render path breaks | ray-physics correctness |
| Bridge | editor only | yes | clock | editor layout | physics |
| ParticleLab | fixed-frame sim shot | yes; promote only if PORT-05 shows cross-OS stability | clock | sim determinism regressions | — |
| Circuit / Fluid2D | initial screens only; sim screens warning-only indefinitely (OD-6) | yes | clock | boot/layout | float-accumulation output |
| NeuralLab | none until rand seeding decision (OD-5-adjacent) | — | — | — | — |
| Math Visual | covered via Math Showcase pages (MR-* plan) | — | clock | renderer regressions | device-TFT parity |

## D. Per-app semantic assert plan

Hook pattern (established by `CalculationApp::debugLastResult`, consumed at `NativeHal.cpp:1731-1794`): `NATIVE_SIM`-visible, read-only, allocation-free `debug*()` const accessors + one append-only script command each. Gate column: should a red hook block merge once landed — yes for all (assert-only steps are cheap and reliable).

| App | Hook(s) | Complexity | CI gate |
|---|---|---|---|
| Matrices | `debugResultText()` or `debugResultCell(r,c)` → `assert_matrices_result` | low (engine already stores result) | yes |
| Chemistry | `debugSelectedElementSymbol()` → `assert_chem_element` | low | yes |
| Calculus | `debugResultText()` (serialized MathAST) → `assert_calculus_result` | medium (needs canonical serialization; reuse MR-01 `assert_math_serialized` shape) | yes |
| Equations | `debugSolutionText()` + `debugSolveState()` → `assert_equations_solution` | medium | yes |
| Regression (already enabled) | `debugCoefficients()` → `assert_regression_coeff a 2.0 0.01` (approx form per QA-03 `assert_result_approx` precedent) | low | yes |
| Sequences (enabled) | `debugTermAt(n)` | low | yes |
| Stats/Prob (enabled) | `debugStatValue(name)` — **blocked by digit-entry bug (OD-7)** | low after fix | yes |
| OpticsLab/Bridge/ParticleLab | `debugSimStepCount()` (proves deterministic step count) | low | yes |
| Circuit | `debugNodeVoltage(n)` approx | medium | yes |
| Fluid2D | `debugChecksum()` over density grid (determinism oracle without pixels) | medium | warning-first (cross-OS float drift) |

## E. Stress plan

Per enabled app, in ascending cost:
1. **Repeated open/close** (`<app>_relaunch_loop.numos`): ×10 `open_app` → wait → `home` → wait; final `assert_app Menu` + screenshot; wraps `mark_lvgl_objects`/`assert_lvgl_object_delta 0` when ROUTE-05 lands. (LIFE-01; mirrors QA-08's 10× relaunch standard.)
2. **Timer-active exit** (timer apps): enter → start sim → `home` with zero wait → relaunch → assert alive. (LIFE-02.)
3. **Modal exit**: open modal/submenu → `open_app` other app (exercises `flushPendingTeardown`, `NativeHal.cpp:1040-1045`).
4. **Storage write/read** (storage apps, post-FIX-04): two-process roundtrip `store → exit → relaunch → assert loaded`, sandboxed.
5. **Invalid input**: per-app breakit corpus (Grapher precedent: `grapher_breakit_stress`); unparseable equation, singular matrix, division by zero.
6. **Rapid keying**: 100 alternating keys with `wait 0` — parser/UI queue robustness.
7. **Long expression**: Calc/Equations/Calculus near-capacity inputs (BC-* corpus rows).
8. **App-specific**: matrix 6×6 fill (memory), chemistry full-table traversal, Grapher orphaned stress scripts wired in (APPQA-05).

## F. Artifact policy (app-enablement runs)

Per CI step, uploaded regardless of pass/fail: run logs (existing pattern), all candidate screenshots, **assert-failure screenshot** (CI-04: on exit-4 dump `failure_<script>.ppm`), visual diffs (`compare-ppm.py --write-diff` output on mismatch, CI-03), storage snapshot (tar of sandbox dir after storage-app scripts), dirty-tree report (CI-01 `git status --porcelain` output), object-leak report (ROUTE-05 counts) once available. Retention: default GitHub artifact policy; failure bundles named `<script>-failure`.

## G. Golden promotion policy (app-enablement addendum to CI-05)

- **When**: candidate stable across ≥2 weeks / ≥10 CI runs on the Linux reference, semantic hooks for its content green, app at P3+.
- **Who**: human reviewer via `scripts/promote-emulator-golden.py <stem>` (refuses clobber without `--force`, never automated); PR touching `tests/emulator/golden/**` requires review.
- **Avoid broad masks**: only the clock rect until EMUDET-01; after it, new goldens unmasked. A proposed mask >1% of frame or below y=24 is auto-rejected (CIART §E).
- **Dynamic content**: don't mask it — don't capture it (choose stable screens; rand/sim screens stay warning-only per OD-5/OD-6).
- **Preserve warning-only value**: warn stems are not second-class — CI-05's rule that every stem is gated/pending/assert-only keeps them tracked with an owner in `PENDING.md`; a warn stem that diffs run-to-run is a determinism bug and must be triaged, not deleted.

## H. Negative controls

Run occasionally (or as CI-05 bite-tests) to prove the harness can fail:
1. **Broken assert hook**: temporary script asserting `assert_matrices_result 999` against det=−2 → must exit 4 and upload failure screenshot.
2. **Intentional pixel mismatch**: compare a candidate against a deliberately edited golden copy → comparator exit 1 → job fails.
3. **Missing app route** (ROUTE-02): key-navigate to a dead card, ENTER, assert menu still active + refusal log line present — guards against half-wired null-deref.
4. **Dirty storage control**: script that STOs a variable without sandbox → CI-01 dirty-tree guard must fail the run (until FIX-02 flips the default, at which point the control asserts the sandbox absorbed it).
5. Existing 9F marker-grep controls (`\[ASSERT\].*FAIL -`) remain.

## I. Acceptance gates

An app may claim a parity level only when:
- **P2**: smoke script green twice consecutively in CI, byte-identical candidate self-diff (CI-02 pattern).
- **P3**: hook + assert-only step green and gating; negative control #1 demonstrated once in the PR.
- **P4**: stem in CANDIDATES + PENDING.md row with owner; no unexpected-stem failures.
- **P5**: promotion PR merged with provenance line; mask ⊆ clock rect.
- **P6**: matrix-doc checklist section updated and reviewed; shim register (D-table) has no unresolved entries for the app.
- **P7**: LIFE-01 (and LIFE-02 where applicable) in CI and green for 2 weeks; object-delta 0 once ROUTE-05 lands.

Program-level gate for P-05 completion: Matrices + Chemistry at P5, Calculus + Equations at P3+ (P4 optional), all 27 orphaned scripts wired (gated/pending/assert-only), zero unexpected candidate stems, dirty-tree guard green throughout.
=======
- **Scripts:** 71 `.numos` under `tests/emulator/scripts/`. By prefix:
  calc 19, grapher 36, menu 3, launcher 1, settings 1, statistics 2,
  probability 2, sequences 2, regression 2, math_showcase 2, input 1.
- **Goldens:** 19 under `tests/emulator/golden/`. **Masks:** 18 (every golden
  except `launcher_smoke`, which has no StatusBar and is byte-gated;
  `masks/README.md:13-14`). All 18 masks carry the single shared clock rect
  `4,6,37,13`.
- **Candidate stems:** 25 in `scripts/generate-emulator-candidates.py:39-143`
  (frames 800 for open+shot smokes, 1400 for typing-heavy runs). Six stems are
  **warning-only** (candidate generated, no committed golden):
  `menu_focus_grapher_smoke`, `grapher_implicit_x_smoke`,
  `grapher_implicit_2x_smoke`, `grapher_multifn_graph_smoke`,
  `grapher_multifn_table_smoke`, `grapher_expr_scroll_smoke`.
- **Assert-only suites (no goldens by design):** 12 `calc_semantic_*` +
  `calc_error_div_by_zero` + `calc_store_variable` (CI step "Phase 8C",
  `emulator-build.yml:308-356`), input-parity trio (`emulator-build.yml:375-408`),
  menu parity pair (`emulator-build.yml:429-461`), Grapher no-hang six
  (`emulator-build.yml:495-531`).
- **Command usage across corpus:** `key` 829, `wait` 350, `log` 164,
  `assert_app` 90, `open_app` 62, `screenshot` 54, `assert_no_error` 18,
  `assert_result` 17, `assert_menu_focus` 13, `assert_error` 2,
  `assert_variable` 1. Never used: `keydown`, `keyup`, `assert_result_contains`.
- **Per-app coverage:** full table in parity spec §C. Gaps: no semantic hooks
  outside Calculation/MainMenu; Grapher has 30 scripts without goldens; zero
  coverage for all 13 firmware-only apps.

## B. Per-app smoke script plan **[PROPOSED]**

Common shape (mirrors `settings_smoke.numos`): `wait 200` (splash→menu settles),
`open_app <App>`, `wait 60`, interaction keys, `screenshot out/<stem>.ppm`,
`assert_app <App>`, `log PASS`. One scenario per script. All new stems register
in `generate-emulator-candidates.py` and enter CI as *pending* per CI-05.

### P-05 wave

| Script | Steps | Assertions | Candidate | Deterministic requirements | Mask | Failure mode |
|---|---|---|---|---|---|---|
| `matrices_smoke.numos` | open → wait → shot | `assert_app Matrices` | `matrices_smoke` | none beyond clock | clock rect | exit 4 wrong app; exit ≠0 crash |
| `matrices_edit_smoke.numos` | open → enter editor on M1 → type `1 2 / 3 4` into cells (digit keys + arrows) → shot | `assert_app Matrices` + `assert_matrices_cell 0 0 1` (hook, §D) | `matrices_edit_smoke` | none | clock rect | wrong cell value = hook FAIL exit 4 |
| `matrices_ops_smoke.numos` | edit M1 → run det/inverse via op menu (F-keys per `MatricesApp.cpp:892-903`) | `assert_matrices_result_contains` (hook) | assert-only | none | — | wrong determinant |
| `periodic_smoke.numos` | open → wait → shot (grid view, custom DRAW_MAIN path) | `assert_app Chemistry` | `periodic_smoke` | none (constexpr data) | clock rect | draw-layer regression caught byte-exact |
| `periodic_nav_smoke.numos` | arrows across NAV_LUT → open element modal → shot | `assert_app Chemistry` + `assert_periodic_selected Fe` (hook) | `periodic_nav_smoke` | none | clock rect | wrong nav target |
| `periodic_balance_smoke.numos` | F-tab to balancer → type `H2+O2=H2O` (keyToChar path, `PeriodicTableApp.cpp:630-670`) → SOLVE | `assert_periodic_balanced "2H2+O2=2H2O"` (hook) | assert-only | exact rational ChemCAS is deterministic (`ChemCAS.cpp:481`) | — | wrong coefficients |
| `calculus_smoke.numos` | open → wait → shot of input screen | `assert_app Calculus` | `calculus_smoke` | screenshot BEFORE any compute (spinner captions are rand(), `CalculusApp.cpp:449-453`) | clock rect | — |
| `calculus_diff_smoke.numos` | type `x^2` → ENTER (derivative mode) → wait past compute → assert | `assert_calculus_result "2*x"` (hook; exact string TBD from SymDiff output at implementation) | assert-only first; result-screen golden after EMUDET-03 | never screenshot the spinner frame | — | wrong derivative; hang caught by CI 60 s timeout |
| `calculus_int_smoke.numos` | UP/DOWN to integral mode (`CalculusApp.cpp:628-639`) → `x` → ENTER | `assert_calculus_result_contains "x^2/2"` | assert-only | same | — | wrong integral |
| `equations_smoke.numos` | open → wait → shot of equation list | `assert_app Equations` | `equations_smoke` | requires APPPAR-01 shims landed | clock rect | — |
| `equations_linear_smoke.numos` | new single equation → type `2x+4=0` → solve → wait through staged pipeline (needs ROUTE-04 update tick) | `assert_equations_solution "x=-2"` (hook) | assert-only | spinner caption rand() → no mid-solve shots; pipeline advance needs update() dispatch | — | wrong root; pipeline stall = timeout |
| `equations_steps_leak.numos` | solve → open steps → home → re-open ×3 | `assert_equations_steps_empty_after_end` (hook reading `_stepRenderers.size()`; MEM-6 canary) | assert-only | — | — | leak regression |

### P-06 candidates (sketch level; full plans authored when their wave opens)

`stixgallery_smoke` (glyph page shot — byte-stable, likely maskless like
launcher if no StatusBar), `optics_smoke`/`bridge_smoke` (open → let sim run a
fixed N deterministic frames → shot; pixel stability must be proven by
self-diff before any golden), `fractal_smoke` (needs ROUTE-04; render completion
polled via hook, never wall time), storage apps (`circuit_save_roundtrip` etc.)
only after FIX-02, using the sandbox two-process protocol
(persistence sandbox spec §C).

## C. Per-app golden plan **[PROPOSED]**

| App | Golden? | Warning-only first? | Mask | What the golden catches | What it does not catch |
|---|---|---|---|---|---|
| Matrices | yes — manager + editor screens | yes (pending → bless) | clock rect | lv_table layout, focus styling, cell text | numeric correctness (hooks do that) |
| PeriodicTable | yes — grid + element modal | yes | clock rect | the emulator's first `LV_EVENT_DRAW_MAIN` coverage; cell colors/layout | balancer math |
| Calculus | input screen only in P-05; result screen after seed fence + caption policy | yes | clock rect | MathCanvas layout of typed expression | CAS correctness; spinner frames (excluded by policy) |
| Equations | equation-list screen only | yes | clock rect | list layout, preview canvases | solver output (hooks); steps screens (dynamic height, assert-only until stable) |
| Integral / Tutor | no — apps not enabled (parity spec §H) | — | — | — | — |
| Sim apps (Optics/Bridge/…) | only frames proven byte-stable by two-run self-diff; else assert-only forever | yes | clock rect | layout/palette regressions | simulation dynamics |
| NeuralLab | **no goldens** until a seeded-RNG debug hook exists (weights from `rand()`, `NeuralEngine.cpp:140-141`) | — | — | — | — |

Universal rules: never bless a frame containing a `rand()`-derived caption or an
in-flight spinner; never mask body pixels (mask lint per CI-05: reject rects
intersecting y≥24 or >1% of frame); each new golden lands with a `PENDING.md`
classification first and is promoted in a reviewed wave.

## D. Per-app semantic assert plan **[PROPOSED]**

Hook pattern is fixed: `#ifdef NATIVE_SIM` read-only const accessors
(`CalculationApp.h:95-107` precedent) + one new script command per hook, parse-
validated, append-only, `[ASSERT] <script>:<line>: PASS/FAIL - msg` verdict
grammar. Every hook ships with a negative control (wrong expectation → exit 4).

| App | Hooks needed | Complexity | Gates CI? |
|---|---|---|---|
| Matrices | `debugCellValue(m,r,c)`, `debugLastResultString()` → `assert_matrices_cell`, `assert_matrices_result[_contains]` | low (reads static arrays, `MatrixEngine.h:117`) | yes, from P3 |
| PeriodicTable | `debugSelectedSymbol()`, `debugBalanceResult()` → `assert_periodic_selected`, `assert_periodic_balanced` | low (reads `_balBuf`/selection state) | yes |
| Calculus | `debugResultString()`, `debugMode()` (deriv/integral) → `assert_calculus_result[_contains]`, `assert_calculus_mode` | medium (serialize result AST → string; reuse `cas::` printer, not pixels) | yes |
| Equations | `debugSolutionString(i)`, `debugSolutionCount()`, `debugStepsCount()`, `debugPipelineIdle()` → `assert_equations_*` | medium-high (staged pipeline; `debugPipelineIdle` lets scripts wait deterministically instead of guessing frames) | yes; `assert_equations_steps_empty_after_end` is the MEM-6 canary |
| Grapher (backfill, not P-05-new) | GR-14 `debugSlotKind` per Grapher test plan | owned by GR-14 | — |
| Generic | `assert_app_ready` (ROUTE-06), `assert_statusbar_title` (ROUTE-07), `assert_lvgl_object_count` (LIFE-02, warning-only) | low/medium | ready/title yes; object-count warning-only |

Hook-before-golden rule (answers special Q9): any app whose golden frame depends
on computed content (Calculus result, Equations solutions) must have its
semantic hook land **before** golden promotion, so a golden mismatch can be
triaged as "layout changed" vs "math changed". Apps whose smoke frames are
static (Matrices manager, Periodic grid, list screens) may hold goldens with
only `assert_app` (answers special Q8).

## E. Stress plan **[PROPOSED]** (LIFE-01/LIFE-02/APPQA extensions)

1. **Repeated open/close loop** — `life_<app>_loop.numos` per enabled app:
   `open_app → minimal key → home → wait 20 → assert_app Menu` ×5 in one
   process. Gates: exit 0, all asserts pass. Extension once
   `assert_lvgl_object_count` exists: object-count delta between loop 1 and 5
   reported (warning-only) — the emulator-side substitute for firmware's
   `NUMOS_MEM_PROBE` exit markers (`SystemApp.cpp:213-217`).
2. **Timer-active exit** — for each timer-owning app as enabled: start the
   activity that arms the timer, `home` on the very next frame, relaunch,
   assert. Existing precedent: `grapher_home_return_smoke`,
   `grapher_tan_exit_smoke` (CI Phase 9F, `emulator-build.yml:495-531`).
3. **Modal exit** — Equations steps view / template picker, Matrices editor,
   Periodic element modal: open modal → `home` → relaunch → assert base state.
4. **Storage write/read** — storage-wave apps only: sandbox two-process
   roundtrip (write in run A, assert in run B, isolation in run C — sandbox
   spec §C protocol); plus the BC-37-style 10× relaunch with writes.
5. **Invalid input** — per app: reject/ignore paths (letters in Matrices cells,
   unbalanced equation in ChemCAS balancer with overflow guards
   `ChemCAS.cpp:116,360,484`, division by zero in Calculus) asserted via
   `assert_error`-analogue hooks; app must not crash or wedge.
6. **Rapid keying** — 50+ `key` commands with `wait 0/1` interleave in the
   busiest editor (Equations); guards against event-queue overflow
   (LvglKeypad ring is 8 deep, `LvglKeypad.h:77`).
7. **Long expression** — Calculus/Equations: type to the editor's cap; assert
   no crash; renderer depth guard `MAX_RENDER_DEPTH=12` (`MathRenderer.h`)
   respected (refusal, not overflow).
8. **App-specific** — Equations: solve unsolvable/nonlinear-handover inputs;
   Matrices: singular matrix inverse (expect error path, `MatrixEngine.cpp:165`);
   Periodic: 118-element nav sweep (NAV_LUT bounds).

## F. Artifact policy **[PROPOSED, extends CI/golden policy spec]**

Per app-enablement CI additions: suite logs uploaded as
`numos-emulator-<app>-suite` (matching existing `numos-emulator-*` naming,
`emulator-build.yml:112-571`); candidate PPMs in the existing
`numos-emulator-candidates`; on assert failure, `<stem>.assertfail.ppm` via
CI-04 in `numos-emulator-failure-<step>`; golden mismatches produce
`out/diffs/<stem>.diff.ppm` (CI-03); storage-wave suites add a post-run fixture
snapshot (`tar` of the sandbox dir) on failure and the CI-01 dirty-tree report
unconditionally; object-leak counts (once LIFE-02 lands) appended to suite logs,
warning-only.

## G. Golden promotion policy (restating + P-05 deltas)

Unchanged core: promotion is human-only via `scripts/promote-emulator-golden.py`
(refuses overwrite without `--force`, never commits, never run by CI —
`emulator-build.yml:24-26`); reviewer checklist per `masks/README.md:100-106`;
absent golden = warning (`emulator-build.yml:575-607`).

P-05 deltas **[PROPOSED]**: (1) new-app goldens enter as *pending* rows with
owner+reason (CI-05) — a stem may stay pending at most one wave before either
promotion or demotion to assert-only; (2) promotion happens in **coordinated
waves** because enabling a card also changes the launcher's dimmed-card set
(SB-06) and may invalidate `launcher_smoke`; (3) broad masks are lint-rejected
(no rect touching y≥24, none >768 px); (4) dynamic content (results, spinners,
captions) is handled by *framing* (screenshot static screens only), never by
masking; (5) warning-only value is preserved: a stem that repeatedly self-diffs
byte-stable but lacks review keeps generating candidates — it is never silently
dropped from the generator list.

## H. Negative controls **[PROPOSED]** (ROUTE-02 / APPQA-03)

1. **Broken assert hook control** — for each new hook family, one committed
   control script asserting a deliberately wrong value (e.g.
   `matrices_negctl_cell.numos` asserts cell=99); CI runs it expecting exit 4
   (inverted gate). Proves the hook can fail.
2. **Intentional pixel mismatch control** — CI job flips one body pixel of a
   candidate copy and asserts `compare-ppm.py` exits 1 with the mask applied
   (extends the CI-05 mask negative controls).
3. **Missing app route control** — `route_dead_card_control.numos`: navigate to
   a known-unrouted card (while any remain), ENTER, `assert_app Menu` — passes
   only while the card is dead; the script is retired (or repointed) when that
   app is enabled. Plus a build-time control: ROUTE-05 checker run against a
   deliberately half-wired fixture branch must fail.
4. **Dirty storage control** — after FIX-01: run a storage-writing scenario
   with `--fs-root emulator_data` (direct mode) in a throwaway checkout and
   assert CI-01's `git diff --exit-code emulator_data/` fails; proves the guard
   detects pollution.

## I. Acceptance gates

An app's enablement PR set is accepted when, in order:

1. Build: emulator compiles with the app whitelisted; ROUTE-05 checker green
   (all eight routing sites present).
2. P2: smoke script green twice with identical SHA-256 (self-diff, extends
   `emulator-build.yml:228-237`) under `--deterministic`.
3. P3: semantic hooks green + their negative controls exit 4.
4. Lifecycle: LIFE-01 loop green; timer-exit test green if applicable.
5. Suite integrated: new CI step follows the established per-script contract
   (`--headless --deterministic --frames N --quiet`, 60 s timeout, exit 0 +
   `PASS -` present + `FAIL -` absent, `emulator-build.yml:308-356` pattern);
   candidate stems registered; PENDING.md rows added.
6. Hygiene: CI-01 dirty-tree green; no new mask beyond the clock rect; no
   `time(`/`rand(` additions outside the EMUDET-03 policy (grep gate).
7. P5/P6 (per-wave, not per-PR): goldens promoted in a reviewed wave; parity
   checklist (parity spec §I) recorded.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc
