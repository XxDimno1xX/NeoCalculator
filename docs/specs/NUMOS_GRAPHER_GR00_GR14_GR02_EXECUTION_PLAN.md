# NumOS — Grapher v2 Batch 1 Execution Plan: GR-00 + GR-14 + GR-02

> Zero-ambiguity implementation pack for the first Grapher Geometry v2 execution batch.
> Audit base: branch `claude/grapher-gr00-gr14-gr02-spec-yyr3w4` @ `ce1b725` (clean tree), 2026-07-02.
> Normative companions written in this batch: `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` (hooks), `NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md` (classification), `NUMOS_GRAPHER_PHASE10_CI_WIRING_PLAN.md` (CI).
> Upstream specs: `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md` (GEOM), `NUMOS_GRAPHER_TRACE_POI_ANALYSIS_SPEC.md` (TPA), `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` (TEST), `NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md` (TICKETS).
> Every `file:line` below was re-verified against the current tree in this session (not copied from the upstream specs).

---

## A. Executive summary

This batch turns the Grapher's currently *untestable* semantic surface into a *tested contract*, then hardens that contract:

1. **GR-00 (CI wiring, zero source changes)** — 18 Phase-10 scripts under `tests/emulator/scripts/grapher_*.numos` exercise implicit curves, inequalities, equal-aspect rendering, mixed relations, trace-domain holes, and stress — and **none of them run in CI** (verified: no stem appears in `.github/workflows/emulator-build.yml`, whose only Grapher gates are the six Phase-9F scripts at `emulator-build.yml:525-530` plus `grapher_input_navigation.numos` at `:407`). GR-00 appends them to a new no-hang/assert gate step and to the candidate generator's `CANDIDATES` list (`scripts/generate-emulator-candidates.py:39-143`).
2. **GR-14 (semantic assert hooks)** — the `.numos` grammar (`NativeHal.cpp:1345-1362` enum, `:1483-1614` parser, `:1703-1849` dispatcher) gains `assert_graph_*` commands backed by `#ifdef NATIVE_SIM` read-only accessors on `GrapherApp`/`GraphModel`, following the exact precedent of `CalculationApp::debugHasResult/debugLastResult` (`CalculationApp.h:95-107`) and `MainMenu::debugFocusedCardId` (`MainMenu.h:58-78`). This makes classification, validity, angle mode, table values, trace state, and no-NaN-UI assertable without pixels or fonts. Full command spec: `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md`.
3. **GR-02 (classification hardening)** — rewrites `GraphModel::preCacheRPN` (`GraphModel.cpp:125-208`) per the contract in `NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md`: first-class `explicitX`, rejection of one-sided relations (today `"x="` silently plots the line x=0 via the missing-side-is-0 rule, `GraphModel.cpp:200-207` + `:247-257`), rejection of multi-relation text (`y=x=2` compiles because the parser passes `=` through, `Parser.cpp:145-149`, then always fails evaluation, `Evaluator.cpp:239-242`), dry-run rejection of unknown identifiers (`xy=1` is a single token, `Tokenizer.cpp:147-169`, that never resolves, `Evaluator.cpp:62-76` — plots nothing while `valid=true`), a 63-char serialization-cap error (`syncASTtoText`, `GrapherApp.cpp:1266-1275`), and angle-mode sync from `vpam::g_angleMode` (Grapher's own `Evaluator` is constructed RAD, `Evaluator.cpp:24-25`, and **no Grapher source ever calls `setAngleMode`** — re-verified by grep: the only `setAngleMode` call sites in the tree are `EquationSolver.cpp:358` and `SystemApp.cpp:96-97` — while the StatusBar badge above the graph shows the vpam mode, `StatusBar.cpp:243-248`).

The batch is deliberately renderer-light: GR-02's only pixel-visible change on currently-blessed goldens is **none** (see §I), and its two expected candidate changes (`grapher_implicit_sideways_smoke`, `grapher_aspect_sideways_parabola_smoke`, which move from marching squares to the y-sweep sampler) are handled by *not blessing those two goldens until after GR-02* (§J, and CI plan doc §E).

## B. Why this batch comes before GeomStore (GR-04)

1. **GeomStore's acceptance gate is "byte-identical rasterization"** (GEOM §D.3 determinism row: cached-explicit rasterization must reproduce streaming byte-for-byte; gate = blessed goldens pass unchanged). That gate is only meaningful if the implicit/inequality/aspect surface is *in* CI first. Today a GeomStore bug that blanks `plotImplicit` (`GrapherApp.cpp:1716-1832`) would pass CI, because every implicit/inequality script is tier T4 "run by nothing" (TEST §A). GR-00 closes that hole before any geometry refactor lands.
2. **GeomStore consumers key off slot kind.** GR-04's build loop, GR-06's intersections and GR-07's trace all branch on ExplicitY/ExplicitX/Implicit (GEOM §D.2 `GeomPolyline.kind`). If the `kind` enum (GR-02) landed *after* GeomStore, every GeomStore switch would need a second churn. TICKETS' dependency graph agrees: `GR-01, GR-02 → GR-04` (TICKETS "Ordering & dependency summary").
3. **Hooks amplify every later ticket.** GR-14 is listed "early, unlocks semantic asserts for everything above" (TICKETS GR-14). GR-03's chain counts, GR-05's POI coverage, GR-13's truncation banner all want `assert_graph_*`-style oracles; writing them before GR-02 lets GR-02's own acceptance scripts be semantic instead of screenshot-only.
4. **Classifier changes are cheap now, expensive later.** `preCacheRPN` is called from exactly one app path (`GrapherApp::preCacheFuncRPN`, `GrapherApp.cpp:1616-1624`, invoked on commit at `:1069` and by template insertion) and consumed by `replot` (`:1886-1893`), trace gating (`firstTraceableFunc`, `:1632-1637`), table (`rebuildTable`, `:2278-2282`), and POIs. After GeomStore, classification also invalidates cache stamps — more coupling, more regression surface.

## C. Exact file-touch list for the later implementation

Legend: **[N]** new file, **[A]** append-only, **[M]** modify.

### PR-1 — GR-00 (no source changes)

| File | Kind | Exact change |
|---|---|---|
| `.github/workflows/emulator-build.yml` | [A] | New step "Grapher Phase-10 relation/aspect/stress guard (headless deterministic, GR-00)" inserted after the Phase-9F artifact upload (`emulator-build.yml:533-539`) and before the candidate-generation step (`:558`). Contains a `run_grapher10()` helper cloned from `run_grapher` (`:501-523`) with log dir `out/grapher_10/`, followed by 18 `run_grapher10 tests/emulator/scripts/<stem>.numos` lines and an `upload-artifact` step (`name: numos-emulator-grapher-10`, `path: out/grapher_10/`). Full step text: CI plan doc §C. |
| `scripts/generate-emulator-candidates.py` | [A] | Append 15 `("stem", "tests/emulator/scripts/<stem>.numos", 1400)` tuples to `CANDIDATES` (insertion point: after line 142, before the closing `]` at line 143). Stems: CI plan doc §D. The 3 gate-only stems (`grapher_breakit_stress`, `grapher_curve_stress`, `grapher_implicit_tabletrace_safe`) are NOT added. |

### PR-2 — GR-14 (NATIVE_SIM-only)

| File | Kind | Exact change |
|---|---|---|
| `src/apps/GrapherApp.h` | [A] | Public `#ifdef NATIVE_SIM` block appended after `getViewport` (`GrapherApp.h:57-60`): `debugRelationCount()`, `debugSlotKind(int)`, `debugSlotValid(int)`, `debugSlotInvalidReason(int)`, `debugSlotRelationOp(int)`, `debugSlotExprText(int)`, `debugAngleMode()`, `debugTableCell(int,int)`, `debugTraceMode()`, `debugTraceFn()`, `debugUiHasNaN()`. Signatures: hooks doc §C. |
| `src/apps/GraphModel.h` | [A] | Public `#ifdef NATIVE_SIM` accessor `AngleMode debugAngleMode() const { return _eval.angleMode(); }` (requires the one-line getter below). |
| `src/math/Evaluator.h` | [A] | One-line getter `AngleMode angleMode() const { return _angleMode; }` next to `setAngleMode` (`Evaluator.h:47`). Unconditional (trivial, firmware-safe, zero data). |
| `src/hal/NativeHal.cpp` | [A] | Append enum values to `ScriptCmdType` (`:1345-1362`), parse branches in `loadScript` (insert before the `else` at `:1612`), dispatch cases in `scriptStepBegin` (insert before the closing brace of the switch at `:1849`), plus the `set_angle_mode` command. Existing branches untouched (append-only CI contract, ground-truth fragile-files list). |
| `docs/emulator-sdl2-quickstart.md` | [A] | Document the new commands in the script-command section. |
| `tests/emulator/scripts/grapher_asserts_smoke.numos` | [N] | Positive control exercising every new command once (hooks doc §F). |
| `tests/emulator/scripts/grapher_negctl_assert_kind.numos` | [N] | Negative control: a deliberately wrong `assert_graph_slot_kind` that must exit 4 (TEST §D negative controls). |
| `.github/workflows/emulator-build.yml` | [A] | Two lines appended to the GR-00 step: `run_grapher10 …grapher_asserts_smoke.numos` and an inverted-exit-code run of the negative control (CI plan doc §C.3). |

### PR-3 — GR-02

| File | Kind | Exact change |
|---|---|---|
| `src/apps/GraphModel.h` | [M] | `CartesianFunction`: add `enum class FnKind : uint8_t { ExplicitY, ExplicitX, Implicit }` + member `kind`; add `enum class InvalidReason : uint8_t { None, Syntax, OneSided, MultiRelation, UnknownIdent, TooLong, ConstantRelation, Unsupported }` + member `invalidReason` + `char reasonDetail[12]`; keep `bool implicit` as a derived shim `bool implicitLike() const`? **No** — keep the *field* `implicit` (`GraphModel.h:73`) in sync with `kind != FnKind::ExplicitY` so the renderer dispatch (`GrapherApp.cpp:1888`) and `isExplicit()` (`GraphModel.h:80`) do not churn; `kind` is the new source of truth, `implicit` is written by `preCacheRPN` only. Add `float evalAtY(CartesianFunction&, float y)` declaration and `void syncAngleModeFromSystem()`. |
| `src/apps/GraphModel.cpp` | [M] | `preCacheRPN` (`:125-208`): implement classifier contract rules R1–R14 (contract doc §B) — relation-operator census, one-sided/empty-side rejection, `isJustX`/`referencesX` (mirrors of `isJustY` `:64-67` / `referencesY` `:95-109`), explicitX branch, constant-relation rejection, dry-run structural validation, reason recording. Add `evalAtY` (mirror of `evalAt` `:212-237` setting `_vars.setVar('y', …)`), `syncAngleModeFromSystem()` (reads `vpam::g_angleMode`, `MathEvaluator.h:76`, maps **by name** to the legacy `AngleMode` — the two enums have different value order: `vpam::AngleMode{DEG,RAD}` `MathEvaluator.h:70-73` vs legacy `AngleMode{RAD=0,DEG=1}` `Evaluator.h:32-35` — and calls `_eval.setAngleMode`, `Evaluator.h:47`). New include: `"MathEvaluator.h"` (already emulator-whitelisted, `platformio.ini` `+<math/MathEvaluator.cpp>`). |
| `src/apps/GrapherApp.cpp` | [M] | (a) `syncASTtoText` (`:1266-1275`): serialize into a 128-byte temp; if the serialized length exceeds 63, mark the slot `TooLong` instead of silently truncating; (b) `serializeNode` default case (`:1260-1263`): set an out-param "unsupported node" flag consumed by (a) → reason `Unsupported`; (c) `replot()` (`:1865-1912`): call `_model.syncAngleModeFromSystem()` before the slot loop; dispatch `kind == ExplicitX` to the new y-sweep sampler; (d) templatize/duplicate `sampleFuncAdaptive`/`adaptSegStream` (`:1645-1706`) over an axis selector per GEOM §E.2 (40 samples over `[yMin,yMax]`, deviation on screen-x via `_view.worldToScreenX`, jump guard `3·(xMax−xMin)`). |
| `src/apps/GrapherApp.h` | [M] | Declarations for the axis-selected sampler (e.g. `sampleFuncAdaptiveY(int fi, uint32_t color)` + `adaptSegStreamY(…)`, or one templated private helper). No layout/state changes. |
| `tests/emulator/scripts/grapher_classifier_*.numos` | [N] | One script per contract row lacking coverage (contract doc §D lists the exact set: `explicitx_sideways`, `explicitx_vline`, `invalid_onesided`, `invalid_multirel`, `invalid_unknown_ident`, `invalid_toolong`, `angle_deg_sine`, `explicit_regression_guard`). |
| `.github/workflows/emulator-build.yml` | [A] | Append the new classifier scripts to the GR-00 gate list (same helper). |
| `scripts/generate-emulator-candidates.py` | [A] | Append candidate stems for the visually-meaningful classifier scripts (`grapher_classifier_explicitx_sideways`, `grapher_classifier_explicitx_vline`, `grapher_classifier_angle_deg_sine`). |
| `tests/emulator/golden/grapher_implicit_sideways_smoke.ppm` (+mask) | [N, human] | Blessed only AFTER GR-02 (post-sampler pixels), via `scripts/promote-emulator-golden.py`. Never blessed in PR-1 (§J). Same for `grapher_aspect_sideways_parabola_smoke`. |

**Explicitly NOT touched in this batch** (do-not-touch boundaries, §H.3): `tests/emulator/golden/**` and `tests/emulator/masks/**` except the human promotion step above; any existing `tests/emulator/scripts/*.numos`; any existing parse branch or assert in `NativeHal.cpp` (`:1483-1614`, `:1703-1849` — append only); `platformio.ini` (all needed TUs are already whitelisted at `platformio.ini:195-266`, including `MathEvaluator.cpp` for `vpam::g_angleMode`); `src/input/KeyCodes.h`; `src/ui/GraphView.*` (no dash work in this batch — that is GR-11); `src/ui/MathRenderer.*`, fonts; `TEMPLATES[]` (`GrapherApp.cpp:82-90`, frozen golden surface per GEOM §B.8); the existing Phase-9F gate step (`emulator-build.yml:495-539`); `scripts/compare-ppm.py`, `scripts/promote-emulator-golden.py`.

## D. Step-by-step implementation sequence

Each numbered step is a commit-sized unit; PR boundaries marked.

**PR-1 (GR-00)**
1. Add the GR-00 workflow step with the 18 `run_grapher10` lines + artifact upload. Do not touch the Phase-9F step.
2. Append the 15 candidate tuples to `CANDIDATES`.
3. Local acceptance (§G.1). Open PR-1; merge on green. From this merge on, the implicit/inequality/aspect surface is hang/assert-gated and produces reviewable candidates on every CI run.
4. (Human, out-of-band, after PR-1) Bless the 13 stable candidates listed in CI plan doc §E.2 — **excluding** the two sideways stems — after visual review on a Linux-generated run.

**PR-2 (GR-14)**
5. Add `Evaluator::angleMode()` getter; add `GraphModel::debugAngleMode()` under `NATIVE_SIM`.
6. Add the `GrapherApp` debug accessor block (hooks doc §C) — pure reads, no LVGL mutation.
7. Append `ScriptCmdType` values + `loadScript` parse branches + `scriptStepBegin` dispatch cases + `set_angle_mode` (hooks doc §B/§D). All asserts follow the `assertPass`/`assertFail` protocol (`NativeHal.cpp:1682-1693`; FAIL ⇒ exit 4, stop replay).
8. Write `grapher_asserts_smoke.numos` (positive control) and `grapher_negctl_assert_kind.numos` (negative control); wire both into the GR-00 step (negative control with inverted exit).
9. Update `docs/emulator-sdl2-quickstart.md`. Local acceptance (§G.2). Open PR-2; merge on green.

**PR-3 (GR-02)**
10. `GraphModel.h`: `FnKind`, `InvalidReason`, `reasonDetail`, decls. Keep the `implicit` field written-in-sync (no call-site churn: 14 `implicit`/`isExplicit` consumers verified across `GrapherApp.cpp`/`GraphModel.cpp`).
11. `GraphModel.cpp`: classifier rewrite per contract rules R1–R14, in the exact rule order of contract doc §B (order is normative — it resolves `x=y`).
12. `evalAtY` + `syncAngleModeFromSystem`; call the sync from `preCacheRPN` entry and `replot()` top.
13. `GrapherApp.cpp`: y-sweep sampler + `replot` dispatch for `ExplicitX`; `syncASTtoText` cap detection; `serializeNode` unsupported flag.
14. `GrapherApp.h` decls.
15. GR-14 hook update: `debugSlotKind` switches from the derivation shim to reading `kind` directly (hooks doc §C.2 defines both phases; token strings are frozen so no script changes).
16. New classifier scripts + CI/candidates wiring (step list append only).
17. Local acceptance (§G.3). Open PR-3; merge on green.
18. (Human, after PR-3) Re-generate candidates; visually review and bless `grapher_implicit_sideways_smoke`, `grapher_aspect_sideways_parabola_smoke`, and the new classifier goldens.

## E. Dependency graph

```
PR-1  GR-00 CI wiring ──────────────┐  (no source; establishes the safety net)
                                    │
PR-2  GR-14 hooks ──────────────────┤  (needs GR-00's step to host its 2 control scripts;
      │                             │   hook *code* is independent of GR-00)
      │ assert_graph_slot_kind      │
      │ assert_graph_slot_invalid_reason
      │ assert_graph_angle_mode + set_angle_mode
      ▼                             ▼
PR-3  GR-02 classifier ── acceptance scripts assert via GR-14 hooks,
      │                   run inside GR-00's gate step
      ▼
      (human) bless sideways pair + classifier goldens
      ▼
      unblocks: GR-04 GeomStore (kind enum), GR-07/08/12 (explicitX), GR-10 (invalidReason UI)
```

Hard edges: GR-02's acceptance is *unverifiable as specified* without GR-14 (`assert_graph_slot_invalid_reason` has no pre-hook substitute — invalid slots are visually indistinguishable from empty ones until GR-10's error UI). GR-14's negative-control wiring wants GR-00's step to exist. Everything else is soft ordering.

## F. Failure modes prevented (by this batch, concretely)

| # | Failure mode (today, verified) | Prevented by |
|---|---|---|
| 1 | A regression blanking `plotImplicit` (`GrapherApp.cpp:1716-1832`) or inequality shading (`:1774-1793`) passes CI silently — zero Phase-10 stems are gated (TEST §A T4) | GR-00 gate step (hang/assert) + candidates (visual drift becomes reviewable, then gating once blessed) |
| 2 | `"x="` plots the line x=0: one compiled side + missing-side-evaluates-0 (`GraphModel.cpp:200-207`, `:247-257`); `"y<"` shades y<0 (`:142-157`) | GR-02 rule R8 (one-sided ⇒ invalid) + negative-control assert that the slot is invalid, not plotted |
| 3 | `xy=1` plots nothing while `valid=true` (single identifier `Tokenizer.cpp:147-169`; resolution fails `Evaluator.cpp:62-76`) — user believes the hyperbola is out of view | GR-02 rule R11 (dry-run unknown-identifier ⇒ invalid, reason `unknown: xy`) |
| 4 | `y=x=2` is a valid slot that never draws (Equal passes the parser `Parser.cpp:145-149`; evaluation always leaves 2 operands `Evaluator.cpp:239-242`) | GR-02 rule R9 (multi-relation ⇒ invalid) |
| 5 | Expressions >63 serialized chars silently truncate and plot the wrong curve (`GrapherApp.cpp:1266-1275`; `CartesianFunction::text[64]` `GraphModel.h:55`) | GR-02 rule R13 (cap ⇒ invalid `TooLong`) |
| 6 | DEG badge over a RAD plot: StatusBar shows `vpam::g_angleMode` (`StatusBar.cpp:243-248`) while Grapher's evaluator is RAD-locked (`Evaluator.cpp:24-25`, no `setAngleMode` call in Grapher sources) | GR-02 angle sync + GR-14 `set_angle_mode`/`assert_graph_angle_mode` + DEG-sine period test |
| 7 | Classification regressions are unobservable (only indirect symptoms: trace refusal text `GrapherApp.cpp:2673-2678`, table `--` `:2349-2353`) | GR-14 `assert_graph_slot_kind` on every corpus expression |
| 8 | A nondeterminism or assert-plumbing regression makes all asserts vacuously pass | GR-14 negative control (must exit 4); GR-00 gate requires an explicit PASS marker (pattern `emulator-build.yml:519-522`) |
| 9 | `x=y^2` renders via 5,136-eval marching squares (`GrapherApp.cpp:1723-1739`) and can never trace/tabulate | GR-02 explicitX classification + 320-eval y-sweep sampler (trace/table follow in GR-07/GR-12) |
| 10 | "nan"/"inf" leaking into pill/info-bar text under future changes (currently guarded ad hoc, `GrapherApp.cpp:1930-1936`, `:1981-1983`) | GR-14 `assert_graph_no_nan_ui` in trace-domain scripts |

## G. Acceptance gate (exact commands)

All from repo root on Linux/CI-equivalent, after `export PLATFORMIO_BUILD_DIR=.pio/build && pio run -e emulator_pc`. `BIN=.pio/build/emulator_pc/program`.

### G.1 PR-1 (GR-00)

```bash
# 1. Every Phase-10 stem passes the gate contract (exit 0, has PASS, no FAIL):
for s in implicit_x implicit_2x implicit_circle implicit_sideways implicit_ycircle \
         ineq_disk ineq_exterior ineq_halfplane ineq_parabola \
         aspect_circle aspect_line aspect_sin aspect_sideways_parabola aspect_small_circle \
         mixed_relations trace_domain implicit_tabletrace_safe explicit_parabola \
         breakit_stress curve_stress; do
  # NOTE: implicit_x/implicit_2x/… name mapping — use the actual filenames:
  f=tests/emulator/scripts/grapher_${s}$( [ -f tests/emulator/scripts/grapher_${s}.numos ] && echo "" || echo "_smoke").numos
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
    --script "$f" --frames 1400 --quiet > /tmp/${s}.log 2>&1 || { echo "FAIL $s"; exit 1; }
  grep -qE '\[ASSERT\].*: PASS -' /tmp/${s}.log || { echo "NO PASS $s"; exit 1; }
done
# 2. Candidate generation still exits 0 with the enlarged list:
python scripts/generate-emulator-candidates.py --bin "$BIN" --out-dir /tmp/cand
# 3. Injected-regression check (local experiment, then revert):
#    comment out the body of GrapherApp::plotImplicit → grapher_implicit_circle candidate
#    changes → after blessing, compare-ppm exits 1; the gate step itself still passes
#    (no-hang only) — this is WHY blessing (step 4 in §D) matters.
```

### G.2 PR-2 (GR-14)

```bash
SDL_VIDEODRIVER=dummy "$BIN" --headless --deterministic \
  --script tests/emulator/scripts/grapher_asserts_smoke.numos --frames 1400 --quiet; echo "exit=$?"   # must be 0
SDL_VIDEODRIVER=dummy "$BIN" --headless --deterministic \
  --script tests/emulator/scripts/grapher_negctl_assert_kind.numos --frames 1400 --quiet; echo "exit=$?" # must be 4
# malformed command → parse error exit 2:
printf 'assert_graph_slot_kind banana explicitY\n' > /tmp/bad.numos
SDL_VIDEODRIVER=dummy "$BIN" --headless --script /tmp/bad.numos --frames 10 --quiet; echo "exit=$?"      # must be 2
# firmware footprint is zero:
grep -rn "debugSlotKind\|assert_graph" src/apps/GrapherApp.h | grep -v NATIVE_SIM  # expect only lines inside the #ifdef block
pio run -e esp32s3_n16r8   # firmware still compiles (hooks compiled out)
```

### G.3 PR-3 (GR-02)

```bash
# classifier corpus (all new scripts green):
for f in tests/emulator/scripts/grapher_classifier_*.numos; do
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
    --script "$f" --frames 1400 --quiet || { echo "FAIL $f"; exit 1; }
done
# no regression on the six blessed Grapher goldens (byte-identical, §I):
python scripts/generate-emulator-candidates.py --bin "$BIN" --out-dir /tmp/cand2
for g in grapher_smoke grapher_expr_smoke grapher_graph_smoke grapher_table_smoke \
         grapher_templates_smoke grapher_trace_smoke; do
  python scripts/compare-ppm.py tests/emulator/golden/$g.ppm /tmp/cand2/$g.ppm \
    --mask-file tests/emulator/masks/$g.mask || { echo "GOLDEN DRIFT $g"; exit 1; }
done
# negative oracle (TEST corpus row 24): "x=" does NOT draw a vertical line at x=0 —
# asserted semantically: assert_graph_slot_valid fails / invalid_reason == one-sided.
# DEG sine period (TEST corpus row 13): grapher_classifier_angle_deg_sine.numos uses
# set_angle_mode deg + assert_graph_angle_mode deg + table-value asserts sin(90)=1 via
# assert_graph_table_value (hooks doc §E.6 shows the exact script).
```

## H. Rollback plan

1. **PR-1**: `git revert` the single commit — two files, no source. CI returns to pre-batch state; candidates already blessed (step 4) remain valid because GR-00 changed no pixels.
2. **PR-2**: `git revert`. Hooks are additive and `#ifdef NATIVE_SIM`; the only consumers are the two control scripts added in the same PR, so a whole-PR revert is self-consistent. Do NOT revert the `Evaluator::angleMode()` getter separately if PR-3 already landed (PR-3's tests don't use it, but GraphModel's debug accessor does — revert whole PRs, never cherry-pick within this batch).
3. **PR-3**: `git revert` the whole PR (classifier + scripts + CI lines in one revert): reverting the classifier alone would leave `grapher_classifier_*` scripts failing in CI. Post-revert state: explicitX routes back to marching squares (the two sideways goldens, if already blessed post-GR-02, must be un-blessed — `git rm` the two PPM+mask pairs in the same revert commit — this is the one rollback with golden interaction, and the reason blessing waits for PR-3 to look stable).
4. **Emergency flag-off** (if a partial hotfix is needed instead of a revert): GR-02's classifier rewrite must be structured so that the explicitX branch and the four rejection rules are independent early-return blocks — a hotfix can delete a single block (e.g. re-permit one-sided) without touching the rest. No compile-time flag is introduced (the batch is small enough to revert; flags are reserved for GR-04/GR-13 per TICKETS).

## I. What must remain byte-identical

Verified inventory: exactly 6 Grapher goldens exist (`tests/emulator/golden/grapher_{smoke,expr_smoke,graph_smoke,table_smoke,templates_smoke,trace_smoke}.ppm`, each with a clock-rect mask).

| Artifact | Must be byte-identical through | Why it is safe |
|---|---|---|
| All 6 blessed Grapher goldens + the 13 non-Grapher goldens | PR-1, PR-2, PR-3 | PR-1/PR-2 touch no renderer. PR-3 changes classification outcomes only for expressions that are today implicit-or-silently-wrong; the golden scripts use `x` (bare), `y=x`, templates — all explicitY under both old (`GraphModel.cpp:161-169,187-198`) and new (contract rules R2/R4/R5) rules. |
| Angle-mode-dependent pixels | PR-3 | `vpam::g_angleMode` is initialized RAD (`MathEvaluator.cpp:53`) and **written nowhere in the tree** (grep: only the definition) — syncing Grapher to it is RAD→RAD, a no-op today. Only `set_angle_mode` (NATIVE_SIM script command) can change it, and no golden script calls it. |
| Existing script exit codes / assert outputs | all PRs | grammar is append-only; existing branches unmodified. |
| `grapher_implicit_sideways_smoke`, `grapher_aspect_sideways_parabola_smoke` candidates | **expected to CHANGE in PR-3** (marching squares → y-sweep sampler; smoother) | handled by not blessing until after PR-3 (§D step 18). All other implicit/inequality/aspect candidates must be byte-identical through PR-3 (their classifications don't change) — CI's golden compare enforces this for the ones blessed at step 4. |

## J. What becomes CI-gated vs warning-only

| Tier after this batch | Contents |
|---|---|
| **CI-gated, assert/no-hang (new GR-00 step)** | all 18 Phase-10 stems + `grapher_asserts_smoke` + `grapher_classifier_*` (8) — exit≠0 or missing/failing `[ASSERT]` marker fails CI |
| **CI-gated, inverted (negative control)** | `grapher_negctl_assert_kind` — exit==4 required |
| **CI-gated, visual (after human blessing)** | step-4 blessings: `grapher_implicit_{x→ already blessed? no — implicit_x/2x were candidates without goldens}`; precisely: 13 stems blessed post-PR-1 (CI plan doc §E.2) + 2 sideways stems and 3 classifier stems blessed post-PR-3 |
| **Warning-only candidates (indefinitely)** | `menu_focus_grapher_smoke`, `grapher_multifn_*`, `grapher_expr_scroll_smoke`, `grapher_implicit_x/2x_smoke` until their own blessing decision (pre-existing backlog, out of batch scope), plus any candidate whose diff review is pending |
| **Gate-only, never candidates** | `grapher_breakit_stress`, `grapher_curve_stress` (stress endpoints, human-review artifacts only), `grapher_implicit_tabletrace_safe` (ends at the launcher — final frame is not a Grapher visual) |
| **Unchanged** | Phase-9F gates (`emulator-build.yml:525-530`), input-parity (`:405-407`), all calc/menu suites |

---

*End of Document 1 of 4.*
