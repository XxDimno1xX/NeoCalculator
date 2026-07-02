# NumOS — Grapher Classifier Contract (GR-02)

> The exact, exhaustive classification contract for `GraphModel::preCacheRPN` after GR-02, with per-rule oracles for plot/table/trace/error/assert/visual.
> Audit base: `ce1b725`, 2026-07-02. Normative for the GR-02 implementation; consumed by `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` (kind/reason vocabulary) and by the acceptance scripts in `NUMOS_GRAPHER_GR00_GR14_GR02_EXECUTION_PLAN.md` §C/PR-3.
> Classification lives in exactly one function — `GraphModel::preCacheRPN` (`GraphModel.cpp:125-208`) — and that stays true after GR-02 (GEOM §B preamble). The one exception: the >63-char and unsupported-node rules are detected in the serializer (`GrapherApp::syncASTtoText`, `GrapherApp.cpp:1266-1275`; `serializeNode` default case, `:1260-1263`) because the model never sees the untruncated text; they *record* their verdict through the same slot fields.

---

## A. Vocabulary (frozen)

### A.1 Kinds
`FnKind ∈ { ExplicitY, ExplicitX, Implicit }` stored per slot; script-visible kind tokens additionally include the derived `ineqStrict` / `ineqNonStrict` (kind Implicit + `relation` strict/non-strict), `invalid` (`valid==false`, `len>0`), and `empty` (`len==0`). The legacy `bool implicit` field (`GraphModel.h:73`) is kept and written as `kind != ExplicitY`, so `isExplicit()` (`GraphModel.h:80`) continues to mean **traceable/tabulable as y=f(x)** — explicitX is deliberately NOT `isExplicit()` in this batch (trace = GR-07, table = GR-12).

### A.2 Invalid reasons (canonical strings, substring-assertable)
| `InvalidReason` | Canonical string | Set by |
|---|---|---|
| `Syntax` | `syntax` | tokenize/parse/compile failure of a required side, incl. structural dry-run failures other than unknown identifier |
| `OneSided` | `one-sided relation` | R8 |
| `MultiRelation` | `multiple relations` | R9 |
| `UnknownIdent` | `unknown: <ident>` (`reasonDetail` carries the identifier, truncated to 11 chars) | R11 |
| `TooLong` | `expression too long` | R13 |
| `ConstantRelation` | `constant relation` | R10 |
| `Unsupported` | `unsupported node` | R14 |

### A.3 Definitions (verified semantics of the primitives)
- *references y*: tokenization yields an `Identifier` token exactly `y`/`Y` (`referencesY`, `GraphModel.cpp:95-109`; identifiers are maximal `[A-Za-z_][A-Za-z_0-9]*` runs, `Tokenizer.cpp:54-60,147-169` — note **digits are identifier-parts**, so `x2` is ONE identifier). *references x* is the new mirror.
- *isJustY / isJustX*: trimmed side is exactly the single letter (`GraphModel.cpp:64-67` + mirror).
- *compiles*: `normalizeLeadingUnary` (`GraphModel.cpp:38-52`) → tokenize (`Tokenizer.cpp:247-296`) → shunting-yard (`Parser.cpp:56-175`) succeeds (`compileExpr`, `GraphModel.cpp:83-93`).
- *dry-run*: one evaluation of each compiled side with `x=1, y=1` set via `_vars.setVar` (`VariableContext.h:54`); classify the `errorMessage`: **structural** = {`Variable no definida` (`Evaluator.cpp:69-76`), `Operador sin operando previo` (`:117,180,240`), `Función no soportada` (`:159`), `Operador desconocido` (`:211`), `Token inesperado` (`:234`), `Error: pila llena` (`:95,108,…`)}; **value-dependent (tolerated)** = {`Error: Dominio` (`:133,140,146,152`), `Error: Div por 0` (`:197,205`), `Error: Resultado no finito` (`:165,217,246`)}. Structural ⇒ invalid (`unknown: <ident>` when the message is the variable one, else `syntax`); value-dependent ⇒ slot stays valid (the probe point x=1,y=1 may simply be off-domain, e.g. `ln(x-5)`).

## B. Classification rules (normative, in evaluation order)

Rule order IS the contract; earlier rules win. Input = the serialized slot text (`text[64]`, `GraphModel.h:55`).

| # | Rule | Result |
|---|---|---|
| R0 | `len == 0` | not classified; `valid=false`, kind token `empty` (current behavior `GraphModel.cpp:134`) |
| R1 | **Serializer verdicts first**: serialization overflowed 63 chars ⇒ `TooLong`; dropped an unsupported node ⇒ `Unsupported` | invalid (recorded before `preCacheRPN` runs; `preCacheRPN` early-outs on a pre-set serializer verdict) |
| R2 | **Relation-operator census** over raw text: count `=`, `<`, `>` where `<=`/`>=` count as ONE operator each. Count == 0 → R4/R5. Count ≥ 2 → R9. Count == 1 → R3 |  |
| R3 | The single operator is `<`,`>`,`<=`,`>=` ⇒ inequality path (R6-R8 apply to its sides); `=` ⇒ equality path (R5-R12). Operator position splits lhs/rhs exactly as today (`GraphModel.cpp:142-148,159,179-180`) |  |
| R4 | **Bare expression** (no operator): does not reference y ⇒ **ExplicitY**, f = expr (compile fail ⇒ `syntax`); references y ⇒ **Implicit**, G = expr (=0) (unchanged, `GraphModel.cpp:161-176`) — bare `x` plots y=x, bare `y` plots the x-axis (G=y=0) |  |
| R5 | **ExplicitY forms** (equality): `isJustY(lhs) && !referencesY(rhs)` ⇒ ExplicitY f=rhs; else `isJustY(rhs) && !referencesY(lhs)` ⇒ ExplicitY f=lhs (unchanged order, `GraphModel.cpp:187-198`) — tested BEFORE explicitX, so **`x=y` is ExplicitY** (both readings are the same line; explicitY has richer trace — GEOM §B.3 precedence) |  |
| R6 | **ExplicitX forms** (equality, new): `isJustX(lhs) && !referencesX(rhs)` ⇒ ExplicitX f(y)=rhs; else `isJustX(rhs) && !referencesX(lhs)` ⇒ ExplicitX f(y)=lhs. `x=3` is ExplicitX (constant in y ⇒ vertical line) |  |
| R7 | **Both-sides compile requirement** (equality & inequality): each *non-empty* side must compile; the pair (lhs,rhs) feeds G = lhs − rhs (`evalImplicit`, `GraphModel.cpp:241-261`) |  |
| R8 | **One-sided ⇒ invalid** (replaces the silent-0 hole): a relation whose lhs or rhs is empty OR fails to compile ⇒ `valid=false`, reason `one-sided relation` (if the side is empty) or `syntax` (if present but uncompilable). Kills: `x=` (today plots x=0 — `okL&&!okR` still sets valid, `GraphModel.cpp:200-207`, missing side evaluates 0.0, `:247-257`), `=x` (today plots x=0 the same way), `y<` (today shades y<0, `:149-156`), `<x`, `0<x<1`'s split remnant |  |
| R9 | **Multi-relation ⇒ invalid**: R2 count ≥ 2 ⇒ reason `multiple relations`. Kills: `y=x=2` (today a valid slot that never draws — Equal passes the parser, `Parser.cpp:145-149`, evaluation always ends with 2 stack operands, `Evaluator.cpp:239-242`), chained `0<x<1`, `y=(x=2)`. No chained-inequality semantics are designed in v2 — rejection is the contract |  |
| R10 | **Constant relation ⇒ invalid**: equality/inequality where NEITHER side references x nor y (e.g. `1=2`, `2=2`, `pi<e`) ⇒ reason `constant relation`. Decision (GEOM left this open): rejected, not plotted — a universally-true relation would mean "shade everything" (marching squares draws nothing for G≡0 anyway: all-corner-zero cells give case index 0, `GrapherApp.cpp:1802-1804`) and a universally-false one plots nothing; both are user errors on a graphing device |  |
| R11 | **Dry-run structural validation** (equality, inequality, and bare-expr paths, after successful compilation): per §A.3 — unknown identifier ⇒ `unknown: <ident>`; other structural error ⇒ `syntax`. Kills: `xy=1`, `x2`, `abc`, `sin()` (function with empty argument compiles to an operand-less RPN). Cost: ≤ 2 evaluations per compile (one per side) — matches TICKETS GR-02 budget "+1 dry-run eval per compile" per side |  |
| R12 | **Residual equality** ⇒ **Implicit**, G = lhs − rhs (unchanged marching squares, `GrapherApp.cpp:1716-1832`) |  |
| R13 | *(location note for R1)* cap detection: serialize to a 128-byte scratch; `len > 63` ⇒ `TooLong` — never truncate-and-plot (today `syncASTtoText` hard-caps at 63, `GrapherApp.cpp:1271`) |  |
| R14 | *(location note for R1)* `serializeNode` default case (`GrapherApp.cpp:1260-1263`) sets the unsupported flag (defense for `DefIntegral`/`Summation`/…, currently uninsertable from `handleExprEdit`, `:2555-2626`) |  |

**Angle-mode rule (orthogonal to classification)**: at `preCacheRPN` entry and at the top of `replot()` (`GrapherApp.cpp:1865-1873`, before the slot loop), `GraphModel::syncAngleModeFromSystem()` maps `vpam::g_angleMode` (`MathEvaluator.h:70-76`; the value StatusBar displays, `StatusBar.cpp:243-248`) **by name** onto the legacy enum (`Evaluator.h:32-35` — value orders differ!) and calls `_eval.setAngleMode` (`Evaluator.h:47`). Graph, table, trace, POI, and Calculate-menu evaluations all flow through `_eval` (`GraphModel.h:172`), so one sync point covers all consumers. No intentional-override UI is designed in v2: the Grapher follows the global, period (GEOM §C.14). Today the global is RAD forever (written nowhere — verified), so production pixels are unchanged; the NATIVE_SIM `set_angle_mode` command (hooks doc §D.4) makes DEG testable.

## C. Rule-by-rule oracle table

Columns: plotted / table / trace / error state / script assertion / visual. "refused (msg)" = info-bar text `Trace n/a: no y=f(x) (implicit/inequality)` (`GrapherApp.cpp:2673-2678,2731-2735`). Table `--` per `GrapherApp.cpp:2349-2353` (via `evalAt` NAN for non-ExplicitY, `GraphModel.cpp:212-216` + new kind gate). POI/menu behavior for non-explicitY is out of batch scope (GR-05..08) and listed only where it changes.

| Input | Kind | Valid | Plotted | Table | Trace | Error text | Script assertion | Visual expectation |
|---|---|---|---|---|---|---|---|---|
| `x`, `2x`, `sin(x)` | explicitY | ✔ | adaptive x-sweep (`sampleFuncAdaptive`, `GrapherApp.cpp:1685-1706`) | values `%.6g` | full | — | `assert_graph_slot_kind i explicitY` | unchanged; goldens `grapher_graph_smoke` etc. byte-pass |
| `y=x^2`, `sin(x)=y` | explicitY | ✔ | idem | values | full | — | idem | unchanged (`grapher_explicit_parabola_smoke` candidate byte-stable) |
| `x=y` | explicitY (R5 precedence) | ✔ | line y=x | values | full | — | `assert_graph_slot_kind 0 explicitY` | 45° line under equal aspect |
| `x=y^2`, `y^2=x` | **explicitX** (was implicit) | ✔ | **y-sweep sampler** (new; 40 samples over [yMin,yMax], deviation on screen-x, jump guard 3·xRange — GEOM §E.2) | `--` (kind gate; y-table is GR-12) | refused (msg) — trace is GR-07 | — | `assert_graph_slot_kind 0 explicitX` | sideways parabola, smoother than the 3-px marching cells; **candidate changes** ⇒ bless `grapher_implicit_sideways_smoke` only post-GR-02 |
| `x=3` | explicitX (constant) | ✔ | vertical line x=3 | `--` | refused | — | `assert_graph_slot_kind 0 explicitX` | new capability: today `x=3` is implicit marching squares; post-GR-02 a clean 1-px vertical line |
| `x=sin(y)` | explicitX | ✔ | y-sweep | `--` | refused | — | idem | new candidate `grapher_classifier_explicitx_*` |
| `x^2+y^2=1`, `y=x^2+y^2`, bare `y` | implicit | ✔ | marching squares (unchanged, `GrapherApp.cpp:1716-1832`) | `--` | refused | — | `assert_graph_slot_kind 0 implicit` | `grapher_implicit_circle/ycircle` candidates byte-identical through GR-02 |
| `x*y=1` | implicit | ✔ | two hyperbola branches | `--` | refused | — | kind implicit + `assert_graph_slot_valid` | byte-identical; positive twin of the `xy=1` negative oracle |
| `y>x^2`, `x^2+y^2<1` | ineqStrict | ✔ | stipple region + solid boundary (unchanged; dash is GR-11) | `--` | refused | — | `assert_graph_slot_kind 0 ineqStrict` + `assert_graph_relation_op 0 gt/lt` | `grapher_ineq_*` candidates byte-identical |
| `y<=2x` (typed `lt` then `=`) | ineqNonStrict | ✔ | identical rendering to strict (GEOM §B.5) | `--` | refused | — | `assert_graph_relation_op 0 le` — the ONLY oracle until GR-11 | byte-identical to `y<2x` golden by design |
| `x=` , `=x`, `y<`, `<x` | invalid | ✖ | **nothing** (today: x=0 line / y<0 shading!) | excluded (`rebuildTable` filters on valid, `GrapherApp.cpp:2278-2282`) | refused | reason `one-sided relation` (UI rendering of reasons is GR-10; this batch records it) | `assert_graph_slot_kind 0 invalid` + `assert_graph_slot_invalid_reason 0 one-sided`; **negative control: the graph must NOT contain the x=0 overdraw** — semantic assert + (post-blessing) golden of an empty-axes frame | axes-only frame |
| `y=x=2`, `0<x<1`, `y=(x=2)` | invalid | ✖ | nothing (today: valid-but-empty / one-sided remnant) | excluded | refused | `multiple relations` | `assert_graph_slot_invalid_reason 0 multiple` | axes-only |
| `1=2`, `2=2`, `pi<e` | invalid | ✖ | nothing (today: valid, silently empty) | excluded | refused | `constant relation` | `assert_graph_slot_invalid_reason 0 constant` | axes-only |
| `xy=1`, `abc=1`, `x2` | invalid | ✖ | nothing (today: `xy=1` valid + empty plot — THE flagship silent wrong answer, TEST corpus #9) | excluded | refused | `unknown: xy` / `unknown: abc` / `unknown: x2` | `assert_graph_slot_invalid_reason 0 unknown` | axes-only |
| `((x`, `y=+`, `!x` | invalid | ✖ | nothing (already invalid today: paren balance `Parser.cpp:157-171`, unknown symbol `Tokenizer.cpp:216-218`) | excluded | refused | `syntax` | `assert_graph_slot_invalid_reason 0 syntax` | axes-only (unchanged pixels) |
| `sin()` | invalid (was valid-but-dead) | ✖ | nothing | excluded | refused | `syntax` (structural dry-run: operand-less Function) | idem | axes-only |
| 64+-char nested expression | invalid | ✖ | nothing (today: silently truncated plot of the WRONG curve) | excluded | refused | `expression too long` | `assert_graph_slot_invalid_reason 0 too long` | axes-only |
| `y=a*x` (single-letter var, unset) | explicitY | ✔ | y=0 line (variables default to 0: `VariableContext` zero-init `VariableContext.cpp:23-26`, case-insensitive `indexFromName` `:28-32`) | values (0) | full | — | kind explicitY (documented, unchanged — single letters are always defined) | line on the x-axis |
| templates 1-6 (`TEMPLATES[]`, `GrapherApp.cpp:82-90`) | explicitY | ✔ | unchanged | values | full | — | `assert_graph_expr_text` matches the inserted string; kind explicitY | `grapher_templates_smoke` golden byte-passes |
| DEG mode + `y=sin(x)` | explicitY | ✔ | period 360 world units (`Evaluator::toRadians`, `Evaluator.cpp:41-45`, now fed DEG) | sin(90°)=1 at x=90 | full | — | `set_angle_mode deg` + `assert_graph_angle_mode deg` + `assert_graph_table_value` (hooks doc §E.6) | near-flat ramp on default viewport; candidate `grapher_classifier_angle_deg_sine` |

### C.1 Implicit-multiplication rules (documented, unchanged by GR-02)

Verified against `Tokenizer::appendImplicitMulIfNeeded` (`Tokenizer.cpp:223-245`): `*` is inserted between a value-like token (Number/Identifier/RParen, not Function) and a following Number/Identifier/LParen.

| Input | Tokenization | Verdict |
|---|---|---|
| `2x` | `2 * x` | ✔ explicitY (corpus #2) |
| `2(x+1)` | `2 * ( x + 1 )` | ✔ **supported today** — Number→LParen inserts `*` |
| `(x+1)(x-2)` | `… ) * ( …` | ✔ supported (RParen→LParen) |
| `x(x+1)` | `x` is followed by `(` ⇒ lookahead marks it a **Function** (`Tokenizer.cpp:157-167`) ⇒ eval fails `Función no soportada: x` | ✖ invalid post-GR-02 (`syntax` via dry-run) — **adversarial case**: today it is valid-but-dead |
| `xy` | ONE identifier (maximal run) | ✖ `unknown: xy` — implicit multiplication is NOT inserted inside identifier runs (GEOM §C.13 decision); users type `x*y` |
| `x2` | ONE identifier (digits are ident-parts, `Tokenizer.cpp:58-60`) | ✖ `unknown: x2` — adversarial; users type `x*2` or `2x` |
| `2pi`, `2e` | `2 * pi` / `2 * e` (`pi`/`e` are multi-char identifiers, but resolve as constants, `Evaluator.cpp:49-56` — the dry-run does not flag them) | ✔ explicitY constant |
| `x y` (space-separated) | two identifiers ⇒ `x * y` | parser-level curiosity only: no space key exists in `handleExprEdit` (`GrapherApp.cpp:2555-2626`) and the serializer emits none — unreachable from the UI, documented for completeness |

### C.2 Regression floor (must NOT change)

The following classifications and behaviors are byte-frozen by this contract (acceptance = existing goldens + candidates byte-pass, execution plan §I): corpus rows #1-4, 7-8, 10-12, 13-23 of TEST §B keep their [V] classification; `y=x` stays explicitY; bare-`y`-referencing expressions stay implicit; inequality shading/boundary pixels untouched; template insertion round-trip (`grapher_template_all_smoke` gate, `emulator-build.yml:530`) untouched; `normalizeLeadingUnary` rescue of leading `-x` untouched (`GraphModel.cpp:38-52`); NAN-skip semantics (`GraphModel.cpp:221-224`, `GrapherApp.cpp:1697-1698`) untouched.

## D. Acceptance script set (new, PR-3)

| Script | Asserts |
|---|---|
| `grapher_classifier_explicitx_sideways.numos` | `x=y^2` ⇒ kind explicitX, valid, plots (screenshot candidate), table col `--`, trace refused, no-nan |
| `grapher_classifier_explicitx_vline.numos` | `x=3` ⇒ kind explicitX; screenshot candidate (vertical line) |
| `grapher_classifier_invalid_onesided.numos` | `x=` and `y<` (two slots) ⇒ invalid + reasons; relation_count 2; graph screenshot = axes-only (negative visual oracle vs the old x=0 line) |
| `grapher_classifier_invalid_multirel.numos` | `y=x=2` ⇒ `multiple relations` |
| `grapher_classifier_invalid_unknown_ident.numos` | `xy=1` invalid `unknown: xy`; second slot `x*y=1` valid implicit (positive twin) |
| `grapher_classifier_invalid_toolong.numos` | 64+-char input ⇒ `expression too long` (built from repeated `+1` keys) |
| `grapher_classifier_angle_deg_sine.numos` | hooks doc §E.6 |
| `grapher_classifier_explicit_regression_guard.numos` | `x`, `y=x^2`, `x=y` in three slots ⇒ all explicitY; templates row untouched |

Each script: header comment with purpose + Run line (house style, e.g. `grapher_curve_stress.numos` header), `--frames 1400`, ends `assert_app Grapher`. All are CI-gated in the GR-00 step from PR-3 onward; the three visually-novel ones also become candidates (execution plan §C/PR-3).

## E. Boundaries

This contract deliberately does NOT cover (later tickets): explicitX trace/table/POI/menu semantics (GR-07/08/12, TPA §A.3/§C), error-state UI (GR-10 — this batch only *records* reasons), dashed boundaries (GR-11), contour chaining (GR-03), GeomStore (GR-04), time budgets (GR-13). Any implementer tempted to "also fix" those must stop: the byte-identical set in execution plan §I is the guardrail.

---

*End of Document 3 of 4.*
