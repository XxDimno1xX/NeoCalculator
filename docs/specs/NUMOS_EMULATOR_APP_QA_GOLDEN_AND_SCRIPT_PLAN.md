# NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN

Status: SPEC-ONLY (P-05 wave). Test plan for app enablement.
Depends on: parity spec (levels P0–P7, prerequisites), matrix (per-app facts), routing spec (script API), roadmap (tickets). Inherits the T1–T7 test-tier taxonomy from `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` and the promotion/mask rules from `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md`.

---

## A. Existing inventory (verified)

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
