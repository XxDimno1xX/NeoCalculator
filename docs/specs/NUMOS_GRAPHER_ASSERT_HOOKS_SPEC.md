# NumOS — Grapher Semantic Assert Hooks Specification (GR-14)

> NATIVE_SIM-only `.numos` script commands for asserting Grapher state without pixels, OCR, or fonts.
> Audit base: `ce1b725`, 2026-07-02. Companion of `NUMOS_GRAPHER_GR00_GR14_GR02_EXECUTION_PLAN.md` (sequencing) and `NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md` (the kind/validity vocabulary these hooks expose).
> This document is SPEC-ONLY; it defines the exact grammar, accessors, and failure semantics an implementer must produce.

---

## A. Framework contract (existing, reused verbatim)

- **Grammar host**: `loadScript` in `src/hal/NativeHal.cpp:1457-1622` parses one command per line, validates the whole script at load, and fails fast with **exit 2** on any parse error (`NativeHal.cpp:1882-1884`). New commands add `else if` branches **before** the terminal `else { return scriptErr(...); }` at `NativeHal.cpp:1612-1614`. Existing branches are an append-only CI contract (ground truth fragile-files list) — never edited.
- **Dispatch**: `scriptStepBegin` (`NativeHal.cpp:1697-1850`) executes one command per deterministic frame, before SDL/LVGL tick. New `case` labels append to the `switch`.
- **Verdicts**: `assertPass` prints `[ASSERT] <script>:<line>: PASS - <msg>` (`NativeHal.cpp:1689-1693`); `assertFail` prints `FAIL -`, sets **exit 4**, and stops the replay (`NativeHal.cpp:1682-1688`). CI greps for these markers (`emulator-build.yml:515-522`).
- **App gating precedent**: `assert_menu_focus` fails (not no-ops) when the wrong app is active (`NativeHal.cpp:1826-1830`). All `assert_graph_*` commands follow this: if `g_mode != AppMode::GRAPHER || !g_grapherApp` (`g_grapherApp` defined at `NativeHal.cpp:255`) ⇒ `assertFail("assert_graph_* requiere Grapher activo (app actual: '<name>')")`.
- **Hook precedent**: `#ifdef NATIVE_SIM` public read-only accessors — `CalculationApp::debugHasResult/debugLastResult` (`CalculationApp.h:95-107`), `MainMenu::debugFocusedCardId/debugResolveCardToken/debugCardNameById` (`MainMenu.h:58-78`). `NATIVE_SIM` is defined exclusively by `[env:emulator_pc]`, so firmware footprint is zero.
- **Struct storage**: reuse `ScriptCmd` fields (`NativeHal.cpp:1364-1373`): integer args in `waitN` (pack two small ints as `waitN = a*1000 + b` is FORBIDDEN — instead add one new field `long waitN2 = 0;` to `ScriptCmd`; adding a defaulted field to the private struct is not a grammar change), string arg in `strArg`.

### A.1 Timing discipline (applies to every command below)

State inspected is the state after the previous frame's `lv_timer_handler`. Scripts must `wait` after the triggering key before asserting, exactly like `assert_result` ("pulsa ENTER y deja asentar con `wait`", `NativeHal.cpp:1749-1750`). Recommended settle waits: 30 frames after expression commit; 80 after `key graph` (deferred first replot, `GrapherApp.cpp:836-842`); 60 after table switch.

## B. Command grammar (append to `loadScript`)

New `ScriptCmdType` values (append after `AssertMenuFocus`, `NativeHal.cpp:1361`):
`SetAngleMode, AssertGraphRelationCount, AssertGraphSlotKind, AssertGraphSlotValid, AssertGraphSlotInvalidReason, AssertGraphAngleMode, AssertGraphTableValue, AssertGraphTraceState, AssertGraphNoNanUi, AssertGraphRelationOp, AssertGraphExprText`.

Parse-time validation table (all violations ⇒ `scriptErr` ⇒ exit 2, nothing executed):

| Command syntax | Args validated at load |
|---|---|
| `set_angle_mode <deg\|rad>` | exactly 1 token, case-insensitive `deg`/`rad` |
| `assert_graph_relation_count <n>` | exactly 1 token, integer `0..6` (`MAX_FUNCS`, `GrapherApp.h:64`) |
| `assert_graph_slot_kind <slot> <kind>` | slot integer `0..5`; kind ∈ `{explicitY, explicitX, implicit, ineqStrict, ineqNonStrict, invalid, empty}` (case-sensitive, frozen vocabulary — contract doc §A) |
| `assert_graph_slot_valid <slot>` | slot integer `0..5`, no extra args |
| `assert_graph_slot_invalid_reason <slot> <reason-substring…>` | slot integer; rest-of-line non-empty (quote-trimmed like `assert_result`, `NativeHal.cpp:1544-1553`) |
| `assert_graph_angle_mode <deg\|rad>` | as `set_angle_mode` |
| `assert_graph_table_value <row> <col> <text…>` | row `0..20` (`TBL_ROWS=21`, `GrapherApp.h:201`), col `0..6` (`TBL_COLS=1+MAX_FUNCS`, `GrapherApp.h:202`); rest-of-line non-empty |
| `assert_graph_trace_state <idle\|navigate\|trace> [slot]` | mode token from the fixed set; optional slot integer `0..5` (only meaningful with `trace`) |
| `assert_graph_no_nan_ui` | no args |
| `assert_graph_relation_op <slot> <eq\|lt\|gt\|le\|ge>` | slot integer; op token from the fixed set |
| `assert_graph_expr_text <slot> <text…>` | slot integer; rest-of-line non-empty (exact match, quote-trimmed) |

`set_angle_mode` is a **mutator**, not an assert: it exists because `vpam::g_angleMode` is written nowhere in the tree (grep: only its definition `MathEvaluator.cpp:53`), so no key sequence can reach DEG. It is still NATIVE_SIM-only and still gated on nothing (works from any app — it writes a global). It must also call no LVGL API; the StatusBar badge refreshes itself on its periodic update (`StatusBar.cpp:197`).

## C. Debug accessors (the app state each command inspects)

All in a single public `#ifdef NATIVE_SIM` block appended to `GrapherApp.h` after `getViewport` (`GrapherApp.h:57-60`). All are `const`, allocation-free, and never mutate LVGL state.

| Accessor | Returns | Reads (exact members) |
|---|---|---|
| `int debugRelationCount() const` | `_numFuncs` | `GrapherApp.h:222` |
| `const char* debugSlotKind(int i) const` | canonical kind token or `nullptr` if `i<0\|\|i>=MAX_FUNCS`; `i>=_numFuncs` ⇒ `"empty"` | Phase 1 (pre-GR-02) derivation from `_funcs[i]` (`GrapherApp.h:221`): `!valid ⇒ "invalid"` **unless** `len==0 ⇒ "empty"`; `valid && !implicit ⇒ "explicitY"`; `implicit && relation==Equal ⇒ "implicit"`; `relation∈{Less,Greater} ⇒ "ineqStrict"`; `relation∈{LessEqual,GreaterEqual} ⇒ "ineqNonStrict"` (fields: `GraphModel.h:57,73,76`; `Relation` enum `GraphModel.h:46`). Phase 2 (post-GR-02): read `kind`/`invalidReason` directly; adds `"explicitX"`. Token strings are frozen across phases. |
| `bool debugSlotValid(int i) const` | `_funcs[i].valid` (out-of-range ⇒ false) | `GraphModel.h:57` |
| `const char* debugSlotInvalidReason(int i) const` | Phase 1: always `""` (reason tracking doesn't exist yet — the command FAILs with "reason tracking requires GR-02" until then); Phase 2: canonical reason string from `invalidReason` + `reasonDetail` (contract doc §A.2 vocabulary: `syntax`, `one-sided relation`, `multiple relations`, `unknown: <ident>`, `expression too long`, `constant relation`, `unsupported node`) | new fields (GR-02) |
| `const char* debugSlotRelationOp(int i) const` | `"eq"/"lt"/"gt"/"le"/"ge"` from `_funcs[i].relation` | `GraphModel.h:46,76` |
| `const char* debugSlotExprText(int i) const` | `_funcs[i].text` | `GraphModel.h:55` |
| `const char* debugAngleMode() const` | `"deg"/"rad"` from `_model.debugAngleMode()` → new `GraphModel::debugAngleMode()` → new `Evaluator::angleMode()` getter beside `setAngleMode` (`Evaluator.h:47`) | `Evaluator.h:54` `_angleMode` |
| `const char* debugTableCell(int r, int c) const` | `lv_table_get_cell_value(_tblTable, r, c)` or `nullptr` when `!_tblTable` | `_tblTable` (`GrapherApp.h:203`), populated by `rebuildTable` (`GrapherApp.cpp:2334-2358`) |
| `const char* debugTraceMode() const` | `"idle"/"navigate"/"trace"` from `_grMode` | `GrapherApp.h:227` (`GrMode` enum `GrapherApp.h:96`) |
| `int debugTraceFn() const` | `_traceFn` | `GrapherApp.h:231` |
| `bool debugUiHasNaN() const` | true iff the *text* of `_infoLabel`, `_tracePillLabel`, or any current `_tblTable` cell contains `"nan"` or `"inf"` (case-insensitive substring via `lv_label_get_text` / `lv_table_get_cell_value`; null widgets skipped) | `GrapherApp.h:178,176,203` |

Design constraint: `debugSlotKind`'s Phase-1 derivation makes GR-14 land-able before GR-02 with identical script-visible behavior for every kind except `explicitX` (which no Phase-1 expression produces — `x=y^2` correctly asserts `implicit` until GR-02 flips it, which is exactly what the GR-02 acceptance scripts assert as a *flip*).

## D. Per-command specification

Common FAIL cases for every `assert_graph_*`: (a) Grapher not active (§A gating); (b) out-of-range slot at runtime is impossible (validated at parse). PASS/FAIL messages follow the house style: PASS echoes the assertion + actual value; FAIL prints expected AND actual (`NativeHal.cpp:1735-1738` pattern).

### D.1 `assert_graph_relation_count <n>`
- **Inspects**: number of committed slots `_numFuncs` (incremented by `addFunction`, decremented by `removeFunction`, `GrapherApp.cpp:1030`). NOT the number of *valid* slots.
- **PASS** iff `debugRelationCount() == n`.
- **Limitations**: counts empty-but-added rows too (a slot exists after `key down`+`key enter` even before typing). Scripts should assert after commits, not mid-edit.
- **Why font-independent**: reads an `int`; the expression list may render with any font.
- **Complements goldens**: a golden shows 2 rows visually; this asserts the model agrees — catching “row rendered but slot not committed” bugs a screenshot can miss when rows overlap identically.

### D.2 `assert_graph_slot_kind <slot> <kind>`
- **Inspects**: classification result of `GraphModel::preCacheRPN` (`GraphModel.cpp:125-208`) via the §C derivation.
- **PASS** iff `strcmp(debugSlotKind(slot), kind)==0`.
- **Parse errors**: unknown kind token, slot outside 0..5 (exit 2 at load, mirroring `assert_menu_focus`'s parse-time resolution, `NativeHal.cpp:1596-1611`).
- **Limitations**: cannot distinguish *why* something is implicit (bare-expr-with-y vs equation) — that is `assert_graph_expr_text`'s job. Pre-GR-02 there is no `explicitX`.
- **Example**: §E.1. **Complements goldens**: the classifier is the semantic heart (TICKETS GR-02 risk note); goldens only see its pixel shadow — `x=y^2` drawn by marching squares vs sampler can look near-identical while trace/table behavior differs completely.

### D.3 `assert_graph_slot_valid <slot>` / `assert_graph_slot_invalid_reason <slot> <substr>`
- **Inspects**: `CartesianFunction::valid` (`GraphModel.h:57`); reason from GR-02's `invalidReason`/`reasonDetail` (contract doc §A.2).
- **PASS**: `_valid` true for the first; for the second: slot is invalid AND canonical reason string *contains* `<substr>` (substring, like `assert_error` `NativeHal.cpp:1795-1798`, so scripts can assert `unknown` without spelling the identifier).
- **FAIL** additionally when the slot is *valid* (invalid_reason asserted on a valid slot is a FAIL with message "slot N is valid").
- **Limitations**: Phase 1 has no reason channel — `assert_graph_slot_invalid_reason` FAILs with an explicit "requires GR-02" message (scripts using it are written in PR-3, so this never fires in CI; it exists so a human running the command early gets a diagnosis, not a silent pass).
- **Why font-independent / complements goldens**: invalid slots are deliberately *visually silent* today (reliability spec F.2 row 1; `GrapherApp.cpp:1886-1887` just skips them) — there is literally no pixel to golden until GR-10's error UI. This hook is the ONLY oracle for failure-modes F.1/F.2 (GEOM §F) in this batch.

### D.4 `set_angle_mode <deg|rad>` / `assert_graph_angle_mode <deg|rad>`
- **Mutator**: writes `vpam::g_angleMode` (`MathEvaluator.h:76`) — `vpam::AngleMode::DEG` or `::RAD` (`MathEvaluator.h:70-73`).
- **Assert inspects**: the Grapher evaluator's actual `_angleMode` (`Evaluator.h:54`) through the accessor chain (§C) — NOT the vpam global. This is the point: pre-GR-02 the assert exposes the divergence (global DEG, Grapher still RAD); post-GR-02 it proves the sync (`GraphModel::syncAngleModeFromSystem` called from `preCacheRPN` and `replot()` top).
- **PASS** iff the Grapher evaluator mode matches. **Enum-mapping hazard the implementer must encode**: `vpam::AngleMode{DEG=0,RAD=1}` vs legacy `AngleMode{RAD=0,DEG=1}` (`Evaluator.h:32-35`) — map by name, never by cast.
- **Limitations**: `set_angle_mode` affects every vpam consumer (MathEvaluator trig, `MathEvaluator.cpp:849-896`; StatusBar badge). Scripts must reset to `rad` before their final screenshot if that frame is a candidate, or the badge pixel-diffs.
- **Complements goldens**: a DEG-vs-RAD sine plot differs subtly at 320×143 (period 360 world units puts less than 3% of a period on the default view — the curve is a near-flat ramp); the semantic assert plus table-value asserts (§E.6) are decisive where pixels are ambiguous.

### D.5 `assert_graph_table_value <row> <col> <text>`
- **Feasibility: YES.** Table cells are plain strings written by `rebuildTable`: x column `%.4g` (`GrapherApp.cpp:2340-2342`), value columns `%.6g` with NAN/Inf ⇒ `--` (`:2344-2354`), empty-set column `--` (`:2355-2357`). `lv_table_get_cell_value` returns exactly those strings — no rendering involved.
- **PASS** iff exact string equality with the quote-trimmed rest-of-line. Exact (not substring): `--` must not match `-1`... substring would; and `%.6g` output is deterministic for a given double.
- **FAIL** when `_tblTable` is null (Table tab never opened) — message "table not built (switch to Table tab and wait)".
- **Parse errors**: row/col out of the fixed 21×7 grid.
- **Limitations**: row indices are *display* rows; the x value of row r is `_tblStart + (_tblRow + r)·_tblStep` (`GrapherApp.cpp:2337`) — scripts must account for scroll state (`key up/down` changes `_tblRow`, `GrapherApp.cpp:2871-2882`). Column c≥1 maps to the c-th *valid* slot (`activeFuncs`, `:2276-2282`), not the slot index.
- **Complements goldens**: the value `1` for `log_2(2)` is one glyph among thousands of pixels; the string assert pins the math while the golden pins the layout. (This generalizes `grapher_logbase_smoke`'s current indirect approach.)

### D.6 `assert_graph_trace_state <mode> [slot]`
- **Feasibility: YES.** `_grMode` and `_traceFn` are plain members (`GrapherApp.h:227,231`) mutated at well-defined points: TRACE entry (`GrapherApp.cpp:2679-2689`, `:2737-2744`, tab-switch auto-enter `:823-857`), AC exit to NAVIGATE (`:2846`), NAVIGATE→IDLE (`:2748-2753`).
- **PASS** iff `debugTraceMode()==mode` and, when `[slot]` given, `debugTraceFn()==slot`.
- **Limitations**: does not expose `_traceX` (float comparison in a line-oriented grammar invites tolerance bikeshedding; the pill/info-bar text asserted via table/no-nan hooks plus goldens cover position). If a later ticket needs it, add `assert_graph_trace_x <val> <tol>` as a NEW command — never extend this one (append-only).
- **Complements goldens**: proves the trace *refusal* path (`firstTraceableFunc<0` ⇒ stays NAVIGATE, `GrapherApp.cpp:2731-2735`) — pixels show an unchanged graph either way.

### D.7 `assert_graph_no_nan_ui`
- **Feasibility: YES** with the §C definition (label/text sweep, not pixels).
- **Inspects**: current text of `_infoLabel` (written by `updateInfoBar`, `GrapherApp.cpp:1975-2009`, which deliberately prints `y=---  (off domain)` instead of nan, `:1981-1983`), `_tracePillLabel` (`drawTraceCursor`, `:1962-1972`), and all populated table cells (`--` rule, `:2349-2353`).
- **PASS** iff no scanned string contains `nan`/`inf` (case-insensitive).
- **Limitations**: substring scan means a hypothetical label "Finance" would false-positive on `nan` — no current or planned Grapher string does; the implementer must scan with word-boundary awareness OR document this (decision: plain substring, documented, because the UI vocabulary is fixed and English "nan" collisions don't exist in it). Does not scan pixel buffers: a NaN that reaches a *coordinate* (not text) is covered by `drawFunctionSegment`'s guard (`GraphView.cpp:198-202`) and goldens.
- **Complements goldens**: goldens freeze one frame; this asserts the invariant at *any* scripted moment (e.g. after each of 6 trace steps across a tan pole in `grapher_trace_domain_smoke`).

### D.8 `assert_graph_relation_op <slot> <op>`
- **Inspects**: `CartesianFunction::relation` (`GraphModel.h:76`), set by the inequality scan (`GraphModel.cpp:142-157`: first `<`/`>` wins; `p[1]=='='` upgrades to LessEqual/GreaterEqual).
- **PASS** iff mapping matches: Equal⇒`eq`, Less⇒`lt`, Greater⇒`gt`, LessEqual⇒`le`, GreaterEqual⇒`ge`.
- **Limitations**: meaningful only on valid inequality/equality slots; asserting `eq` on an invalid slot passes (relation resets to Equal on invalidation, `GraphModel.cpp:129,153`) — scripts should pair with `assert_graph_slot_valid`.
- **Complements goldens**: strict vs non-strict inequalities render identically today (GEOM §B.5) — this is the ONLY way to test `<=` input (`key lt` + `key eq`… precisely `key lt` then FREE_EQ `=`, serializing `<=`) until GR-11's dashed boundary exists.

### D.9 `assert_graph_expr_text <slot> <text>`
- **Reliability: reliable, included.** `_funcs[slot].text` is the exact serializer output (`syncASTtoText`, `GrapherApp.cpp:1266-1275`) — the very string the classifier consumes. It is deterministic (pure function of the AST) and font-free.
- **PASS** iff exact match.
- **Limitations**: asserts the *serialized* form, which is not the visual form: fractions serialize as `(a)/(b)` (`GrapherApp.cpp:1156-1170`), log-base as `(log(x)/log(2))` (`:1240-1254`), π as `pi` (`:1147-1150`). Scripts must use serialized spellings; the command doc must show the mapping table. This is a feature: it pins the exact classifier input, catching serializer regressions (the Phase-9F `OpKind::Eq` bug class, `GrapherApp.cpp:1129-1135`) semantically.
- **Complements goldens**: `grapher_template_insert_smoke` currently proves serialization only via "it plotted"; this asserts the string itself.

## E. Example scripts

### E.1 Classification (usable pre-GR-02, Phase-1 kinds)
```numos
# grapher_asserts_smoke.numos — positive control for every assert_graph_* command
wait 200
open_app Grapher
wait 60
key down
key enter
key x
key enter
wait 30
assert_graph_relation_count 1
assert_graph_slot_kind 0 explicitY
assert_graph_slot_valid 0
assert_graph_expr_text 0 x
assert_graph_relation_op 0 eq
assert_graph_angle_mode rad
key graph
wait 80
assert_graph_trace_state trace 0
assert_graph_no_nan_ui
key table
wait 60
assert_graph_table_value 0 0 -10
assert_app Grapher
```
(Row-0 x value: table starts at `_tblStart` — the implementer verifies the default and pins the exact literal when writing the script; `%.4g` of the default start.)

### E.2 Negative control (must exit 4)
```numos
# grapher_negctl_assert_kind.numos — proves assert plumbing can fail
wait 200
open_app Grapher
wait 60
key down
key enter
key x
key enter
wait 30
assert_graph_slot_kind 0 implicit
```

### E.3–E.5 GR-02 acceptance sketches
The script token for the `=` key is `key =` (`NativeHal.cpp:467` maps `"="` → `KeyCode::FREE_EQ`); inequality tokens are `key lt` / `key gt` (`NativeHal.cpp:561-563`).
```numos
# invalid one-sided ("x="):   key x, key =, key enter  then:
assert_graph_slot_kind 0 invalid
assert_graph_slot_invalid_reason 0 one-sided
# unknown identifier ("xy=1"): key x, key y, key =, key 1, key enter  then:
#   assert_graph_slot_kind 0 invalid
#   assert_graph_slot_invalid_reason 0 unknown
# multi-relation ("y=x=2"):    key y, key =, key x, key =, key 2, key enter  then:
#   assert_graph_slot_invalid_reason 0 multiple
```

### E.6 DEG-mode sine (GR-02 acceptance, corpus row 13)
```numos
# grapher_classifier_angle_deg_sine.numos
wait 200
set_angle_mode deg
open_app Grapher
wait 60
key down
key enter
key sin
key x
key right
key enter
wait 30
assert_graph_slot_kind 0 explicitY
assert_graph_angle_mode deg          # FAILs pre-GR-02 (Grapher locked RAD) — lands with GR-02
key table
wait 60
# with a table window covering x=90: sin(90°)=1; exact row/col pinned at implementation
# assert_graph_table_value <r> 1 1
set_angle_mode rad
```

## F. Why the suite is font-independent (summary)

Every accessor reads model integers/enums (`_numFuncs`, `kind`, `relation`, `_grMode`, `_traceFn`, `_angleMode`) or pre-render strings (`text[64]`, lv_label/lv_table cell text set via `snprintf`). Nothing touches `_graphBuf` pixels, glyph tables, or layout. Font swaps (montserrat↔STIX churn like `GrapherApp.cpp:2970-2973`) and any future AA/theming change zero assert outcomes — exactly the property TEST §C demands ("font-independent asserts").

## G. How the suite complements visual goldens (summary)

| Concern | Golden | assert_graph_* |
|---|---|---|
| Curve pixels, layout, colors, stipple | ✔ byte-exact | ✖ |
| Classification / validity / reason | ✖ (invalid = invisible) | ✔ |
| Table math values | weak (glyph diff) | ✔ exact string |
| Trace mode transitions & refusals | ✖ (same pixels) | ✔ |
| Angle-mode truth | ambiguous at 320×143 | ✔ |
| Invariants at every step (no-nan) | one frame only | ✔ any frame |
| Nondeterminism tripwire | ✔ (self-diff, TEST T5) | ✖ |

Rule of thumb encoded in the CI plan: every new behavior gets one T1 assert AND (when it has stable pixels) one golden — never only one of the two (TEST §C "Failure/disabled" row).

## H. Boundaries, rollback, PR checklist (GR-14 slice)

- **Do not touch**: existing `ScriptCmdType` values/order, existing parse branches, `formatExactVal`, exit-code meanings (0/2/3/4, `NativeHal.cpp:1317-1344` doc block — extend the comment, don't rewrite it), any firmware-visible header content outside `#ifdef NATIVE_SIM` (sole exception: the one-line `Evaluator::angleMode()` getter).
- **Rollback**: revert the PR; hooks have no consumers outside the two control scripts shipped in the same PR.
- **PR checklist**: see execution plan §D steps 5-9 and the master checklist in `NUMOS_GRAPHER_PHASE10_CI_WIRING_PLAN.md` §H.

---

*End of Document 2 of 4.*
