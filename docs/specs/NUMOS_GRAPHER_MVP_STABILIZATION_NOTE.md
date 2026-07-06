# NUMOS_GRAPHER_MVP_STABILIZATION_NOTE

Implementation note (2026-07-06) for the Grapher MVP stabilization batch — the
working-tree realization of GR-00 (CI wiring) + GR-14 (assert hooks, subset) +
GR-02 (classifier validity rules) from the Grapher spec branches. Recorded
deviations from `NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md` / the GR-02 execution
plan, chosen to keep all 19 blessed goldens byte-identical:

1. **Kind is derived, not stored.** No `FnKind` field was added; the script
   token (`explicitY`/`explicitX`/`implicit`/`ineqStrict`/`ineqNonStrict`/
   `invalid`/`empty`) is derived in `GrapherApp::debugSlotKind()` (the GHOOK
   "Phase-1 derivation", permanent for now) from `valid/len/implicit/relation/
   explicitY`. `InvalidReason` + `reasonDetail` + `serializeVerdict` ARE stored
   on `CartesianFunction` (GraphModel.h) because derivation cannot reconstruct
   them.
2. **ExplicitX plots via the existing marching-squares contour**, not the
   contract's y-sweep sampler — pixels are unchanged by design (the sampler is
   deferred with GR-07/GR-12; `grapher_implicit_sideways_smoke` /
   `grapher_aspect_sideways_parabola_smoke` therefore remain bless-able as-is).
3. **Minimal visible-invalid UI shipped ahead of GR-10**: red row border +
   "Invalid: <reason>" hint line on the focused invalid slot + a
   `[GRAPH] invalid slot=N reason=...` telemetry line. Full GR-10 error UI
   remains open.
4. **R10/R11 order swapped**: the structural dry-run runs before the
   constant-relation check so `xy=1` reports `unknown: xy` (the contract's own
   oracle row) instead of `constant relation`; `1=2`/`pi<e` still report
   `constant relation`.
5. **Undefined single-letter variables invalidate** (`y=a*x` with unset `a` →
   `unknown: a`), diverging from the contract's "vars default 0" oracle row:
   `VariableContext::getVariable` returns false for never-set names, so the
   old behavior was a silent blank plot, which the product bar forbids.
6. **Angle-mode sync (GR-02's `syncAngleModeFromSystem`) deferred** with the
   SettingsState work, as the task brief allows. `set_angle_mode` /
   `assert_graph_angle_mode` hooks are NOT yet implemented; the implemented
   GR-14 subset is: `assert_graph_relation_count`, `assert_graph_slot_kind`,
   `assert_graph_slot_valid`, `assert_graph_slot_invalid_reason`,
   `assert_graph_relation_op`, `assert_graph_expr_text`,
   `assert_graph_trace_state` (all append-only in NativeHal).
7. **CI wiring** merges the CIWIRE plan into one workflow step
   ("Grapher Phase-10 relation/aspect/classifier guard"): 15 orphaned scripts
   gate + become warning-only candidates; breakit/curve_stress/tabletrace_safe
   and the six Phase-10G trace/calc-menu smokes gate only; the
   `grapher_classifier_*` glob auto-gates future scripts;
   `grapher_negctl_assert_kind` is inverted (must exit 4).
8. **Implicit multiplication of juxtaposed single-letter variables**
   (follow-up fix). The shared `Tokenizer` reads a run of letters as ONE
   identifier, so `xx`/`xxx` were unknown-identifier refusals instead of
   `x·x`/`x·x·x`. `GraphModel` now expands a bare multi-letter *Identifier*
   token into a product (`GraphModel::tokenizeExpanded` + `expandImplicitVarMul`,
   applied in `compileExpr`/`referencesX`/`referencesY`), preserving reserved
   words `pi`/`ans`/`preans` and function calls. This is **Grapher-local** — the
   `Tokenizer` and `CalculationApp` are untouched (calc suite + all 19 goldens
   verified unchanged). **Consequence for deviation-oriented rule R11:** `xy=1`
   is no longer "unknown: xy" but a valid `implicit` relation `x·y=1` (a
   hyperbola) — this deliberately overrides the classifier contract's
   `xy=1 → invalid` oracle row, which assumed `xy` stays one identifier; the
   product reading matches user expectation. Genuine unknowns now surface only
   as unset single variables (`a` → `unknown: a`, per deviation #5). Test:
   `grapher_classifier_implicitmul_smoke.numos` (replaces the old
   `grapher_classifier_unknown_smoke.numos`).
