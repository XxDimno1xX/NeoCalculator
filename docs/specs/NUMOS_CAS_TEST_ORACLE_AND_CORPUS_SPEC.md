# NumOS — CAS Test Oracle and Corpus Specification

> Companion to `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md` (semantics, §D-§J contracts) and `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md` (boundary, test matrix §F).
> Audit date: 2026-07-02 · Branch: `claude/numos-cas-spec-frmkeg` @ `a0794b0`. Markers: [V]/[P]/[D-n] as in Document 1.

---

## A. Test strategy overview

Layered strategy, cheapest-first. Every layer names its execution vehicle.

| Layer | Vehicle | What it proves | Status |
|---|---|---|---|
| 1. Host unit tests | `g++ -std=c++17` direct compile (pattern: `tests/host/keycode_digit_test.cpp:14`, CI `emulator-build.yml:103-104`) | cas::/ExactVal algebra correctness, int64 range only (`CASInt.h:53-59`) | keycode test only; 8 CAS suites dormant (§B) |
| 2. Host corpus runner | new host binary driving the §D corpus through cas::/MathEvaluator | semantics contract §D-§H per row | does not exist (CAS-02) |
| 3. Emulator semantic `.numos` tests | `NativeHal` script runner, `assert_result/…` (`NativeHal.cpp:1543-1611`), exit codes 0/2/3/4 (`:1317-1344`) | end-to-end keypress→display through the real app | live, CI-gated (`emulator-build.yml:260-356`) |
| 4. Golden text tests | recorded expected-output files compared byte-wise (pattern: PPM goldens, promotion human-only per `tests/emulator/golden/README.md:117-123`) | display formatting, Giac output text | PPM only today; text goldens proposed for Giac (Doc 2 §F.2) |
| 5. Differential tests | native CAS vs Giac vs MathEvaluator vs graph evaluator (§F) | cross-engine parity (NC-10) | none exist |
| 6. Property-based tests | host runner generating random expressions within a grammar | idempotence, termination, evaluate-preservation (NC-3/NC-4) | none exist (CAS-06) |
| 7. Visual tests | PPM goldens (43 candidates, byte-exact + masks; `generate-emulator-candidates.py:39-88`, `compare-ppm.py:33-37`) | pixel layout of math rendering | live for calc/grapher |
| 8. Firmware tests | serial-driven harness on `esp32s3_n16r8_validate` hardware | Giac, BigInt promotion, PSRAM/stack behavior | none exist (CAS-15); manual only |

What CI proves today for math: `1+2=3`, `1/2+1/3=5/6`, `2+3*4=14`, power/negation/paren cases, `1/0` errors, `x` stores `4` (`emulator-build.yml:260-356` globbing `calc_semantic_*.numos`). **No symbolic differentiation, integration, equation solving, BigInt, step logging, or Giac output is exercised by any automated job** [V].

---

## B. Current test inventory

Dormancy root causes [V]: (1) `CAS_RUN_TESTS` defined by no build env (grep: only `src/main.cpp:56,105` and docs); (2) `build_src_filter = +<*>` compiles `src/` only, never `tests/*.cpp` (`platformio.ini:102-103`) — so the `run*Tests()` bodies would not link even with the flag. All eight suites' target APIs still exist in `src/math/cas/` (verified against current headers) — revival is harness work, not API archaeology.

| File | Tests | Runs? | Why dormant | Revival | Risk |
|---|---|---|---|---|---|
| `tests/CASTest.cpp` (1027 ln) | ~53: SymPoly ops, GCD reduction, SymEquation, ASTFlattener, CASStepLogger, SingleSolver (linear/quadratic/identity/contradiction), SystemSolver 2×2/3×3 (`CASTest.cpp:16-27`; runner `:66`) | No | doubly dormant (above); no `main()`, Serial output | host driver `main()` calling `cas::runCASTests()`; link `src/math/cas/*.cpp` + `MathAST.cpp` (natively compilable — proven by emulator whitelist `platformio.ini:251-258`) | Low; assertions may encode stale expected strings — review on first green run |
| `tests/SymExprTest.cpp` (390 ln) | 31: arena, node eval, clone, toSymPoly (`SymExprTest.cpp:16-35`) | No | same | same driver | Low |
| `tests/SymDiffTest.cpp` (618 ln) | 32: 17 diff, 10 simplify, 5 SymExprToAST (`SymDiffTest.cpp:16-30`) | No | same | same | Low |
| `tests/ASTFlatExprTest.cpp` (702 ln) | 91 checks: flattenToExpr dual-mode (`ASTFlatExprTest.cpp:16-27`) | No | same | same | Low |
| `tests/OmniSolverTest.cpp` (294 ln) | 13: classification, `x²−2=0`, `ln x=1`, HybridNewton (`OmniSolverTest.cpp:16-33`) | No | same | same | Newton rows may be tolerance-flaky on different libm — pin tolerances |
| `tests/BigIntTest.cpp` (309 ln) | 30: CASInt arith, overflow **promotion/demotion (ESP32-only)**, CASRational (`BigIntTest.cpp:16-25`) | No | same + promotion paths need ARDUINO | host runs the int64+error subset; promotion rows firmware-only (CAS-15) | Medium: host run gives false confidence about firmware BigInt unless split is explicit |
| `tests/CalculusStressTest.cpp` (313 ln) | 50-expression diff+simplify memory/perf, arena block count ≤4 (`CalculusStressTest.cpp:16-26`) | No | same | same driver; block-count asserts stay valid (arena logic is target-independent) | Low |
| `tests/TutorTemplateTest.cpp` (377 ln) | 55 checks: tutor templates over solvers (`TutorTemplateTest.cpp:16-17`) | No | same | same | Low |
| `tests/MathEnginePhaseRegression.cpp` (2124 ln) | ~90 MathAST layout/geometry tests, real `main()` (`:2041`), reads `platformio.ini` at runtime (`:76-77`) | No | referenced by nothing | one CI step: `g++ -I. tests/MathEnginePhaseRegression.cpp src/math/… && ./mathreg` | Medium: compiles many render-adjacent TUs; may be sensitive to font-table regeneration |
| `tests/TokenizerTest_temp.cpp` (89 ln) | implicit-mul + power token order (`:44-81`); self-described throwaway (`:20-23`) | No | Arduino String/Serial, no main, never referenced | port the one test into the host corpus runner (PARSE-* rows), then delete file | none |
| `tests/HardwareTest.cpp` (64 ln) | keypad/display bring-up; duplicate of compiled-but-uncalled `src/HardwareTest.cpp` (`src/HardwareTest.cpp:85-94`) | No | Arduino-only, unreferenced | hardware-only; out of CAS scope (P-07 dead-code sweep decides) | none |
| `tests/host/keycode_digit_test.cpp` | keycode digits | **Yes** (CI `emulator-build.yml:103-104`) | — | — (the template to copy) | — |
| `tests/emulator/scripts/calc_*.numos` (19 calc + others) | exact int/frac semantics, div-0 error, variable store | **Yes** (`emulator-build.yml:260-356`) | — | extend with §E rows | — |

Also relevant: grapher assert-hook vocabulary (GR-14) is **spec-only** — `assert_graph_*`/`set_angle_mode` not yet in `src/` [V: repo grep, `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md:5`]. Parity tests (CAS-11) depend on it or on `assert_result`-style additions.

---

## C. Oracle design

Eight oracle layers. Each corpus row names its oracle(s); a row passes only if all its oracles pass.

### O1 — Canonical string equality
- **What**: `format(result) == expected_canonical` byte-wise, after display formatting.
- **Valid where**: exact arithmetic, pinned display forms (D.1-D.7, D.20), error messages (§J).
- **False positives** (test passes, code wrong): none for value; can mask internal-representation bugs that format identically.
- **False negatives** (test fails, code right): any legitimate formatting change (spacing, fraction glyph policy) breaks all O1 rows — mitigate by keeping O1 rows minimal-form (`3`, `5/6`, `2√2`) and pushing rich layout to visual goldens.
- **Example**: `1/2+1/3` → `5/6` (already CI, `emulator-build.yml:260-286`).
- **Implementation**: exists (`.numos assert_result`, `NativeHal.cpp:1543-1557`); host runner reuses `MathEvaluator::resultToAST` + a text serializer.

### O2 — Structural AST equality
- **What**: expected tree equals actual tree; for cas:: this is **pointer equality after consing** (`SymExpr.h:107-139`) — build the expected tree in the same arena and compare pointers.
- **Valid where**: simplify outputs, derivative shapes, converter round-trips.
- **False positives**: BigInt aliasing (A.4-1) makes *distinct* values pointer-equal — O2 must never be the sole oracle for values that can exceed int64 (pair with O4/O6).
- **False negatives**: mathematically equal but structurally different results (`x+x` vs `2x` pre-simplify) — only compare post-simplify per NC-3.
- **Example**: `simplify(x·x²)` pointer-equals `symPow(x,3)`.
- **Implementation**: trivial on host once CAS-01 links cas:: natively.

### O3 — Mathematical equivalence by simplification
- **What**: `simplify(a − b)` is the zero node (or `simplify(a/b)==1` for nonzero b).
- **Valid where**: comparing alternate acceptable forms (D.5 radical sums, factor-order permutations).
- **False positives**: a simplifier bug that wrongly cancels makes unequal things "equal" — pair with O4 on every O3 row.
- **False negatives**: simplifier too weak to close the gap (e.g. no rational-function cancellation, §D.10) — row then escalates to O4 as the deciding oracle.
- **Implementation**: after CAS-01/CAS-03.

### O4 — Numerical sampling
- **What**: evaluate both sides at N sample points; relative tolerance 1e-6 (absolute 1e-9 for |expected|<1e-6); NaN-skip out-of-domain points; require ≥⌈N/2⌉ valid points; N=7 default over a per-row range.
- **Valid where**: everything with a numeric interpretation; the backstop for O2/O3.
- **False positives**: polynomials agreeing on sampled points while differing elsewhere (need degree+1 points — row must set N accordingly); functions differing only outside the sampled range; branch-sensitive forms (sample both signs per G.5).
- **False negatives**: catastrophic cancellation near roots — use relative-or-absolute tolerance rule above.
- **Implementation**: `SymExpr::evaluate` (`SymExpr.h:117`) host-side; doubles only, so it can NOT distinguish exact-vs-approximate (that is O8's job).

### O5 — Derivative-of-integral check
- **What**: NC-6 — `diff(integrate(f)) ≍ f` via O3-then-O4; `+C` stripped (G.3); `ln|x|` sampled per-sign (G.5).
- **Valid where**: every SymIntegrate success.
- **False positives**: both diff and integrate wrong in inverse ways — astronomically unlikely for independent rule sets but pair table-integral rows with O2 pinned forms.
- **False negatives**: diff unsupported on integral output (contains Abs → nullptr, `SymDiff.cpp:281-283`) — the harness must special-case `ln|u|` differentiation or those rows will falsely fail; this is a known harness prerequisite (CAS-08).

### O6 — Differential against Giac
- **What**: same input to native engine and Giac (hardware serial, or host Giac per D-7); normalize both (C.3 restricted grammar + resimplify), compare via O3/O4.
- **Valid where**: firmware validation of anything Giac also computes; the only oracle strong enough for BigInt (A.4-1) and hard integrals.
- **False positives**: normalization bugs equating different values; version skew (host giac ≠ vendored 1.9.0 KhiCAS).
- **False negatives**: formatting skew (mitigated by the ignore-list, Doc 2 §F.7); Giac exact forms the normalizer can't parse (rootof) — refuse-and-flag, not fail.
- **Implementation**: CAS-04 (host advisory) + CAS-15 (hardware).

### O7 — Metamorphic properties
- **What**: relations that must hold without knowing the answer: commutativity `eval(a+b)==eval(b+a)`; substitution consistency `eval(f, x=c) == eval(f[x:=c])`; scaling `solve(f)==solve(k·f)` for k≠0; translation `roots(f(x−c)) == roots(f)+c`; idempotence NC-3; angle-mode coherence `sinDEG(x)==sinRAD(x·π/180)`.
- **Valid where**: property-based random generation (layer 6); cheap wide coverage.
- **False positives**: property too weak to pin the value (they check consistency, not correctness) — metamorphic rows never replace O1/O4, they add breadth.
- **False negatives**: legitimate representation asymmetries (hash-order display differences) — compare via O3/O4, never string.

### O8 — Timeout/memory/flag oracle
- **What**: asserts *meta*-behavior: run completes within `timeout_ms`; status ∈ expected set (`ok|deadline|oom|passcap|growth-cap` per the RT-09 `[CAS]`/`[GIAC]` line formats, `DIAGNOSTICS…SPEC.md` §2.6-2.7); `approximate`/exactness flags match `expected_kind` (NC-1); arena bytes ≤ `memory_budget`; step count ≤ cap (I.5).
- **Valid where**: stress/adversarial rows; every NC-1 display row.
- **False positives**: generous budgets hide regressions — budgets are set from measured baseline ×1.5 and ratcheted.
- **False negatives**: host-vs-device timing differences — `timeout_ms` applies per-vehicle (host budgets separate from firmware budgets; the schema has one field, multiplied by a per-vehicle factor: host×1, emulator×2, firmware×10).

---

## D. Corpus format

Machine-readable, one JSON object per line (`.jsonl`), stored at `tests/corpus/cas_corpus.jsonl` [P]. Schema (all fields; `?` = optional):

```json
{
  "id": "RAT-001",
  "category": "rational",
  "input": "1/2+1/3",
  "input_form": "keys|string|symexpr",
  "mode": {"angle": "rad|deg", "result": "symbolic|periodic|extended", "complex": false},
  "engine": "vpam|rpn|cas|giac|graph",
  "variables": {"x": "4"},
  "assumptions": [],
  "expected_kind": "exact|approx|error|unevaluated|timeout",
  "expected_canonical": "5/6",
  "accepted_alternates": [],
  "forbidden_outputs": ["0.8333333333"],
  "oracle": ["O1"],
  "sample_range": [-5, 5],
  "sample_points": 7,
  "timeout_ms": 1000,
  "memory_budget": 65536,
  "notes": "free text",
  "source": "spec D.2 | bug #NN | SymDiffTest case 12",
  "tags": ["ci-fast", "firmware-only", "adversarial"]
}
```

Field semantics:
- `input_form`: `keys` = `.numos` key sequence (emulator vehicle); `string` = RPN/Giac text; `symexpr` = host-runner builder DSL. One row may be *projected* to several vehicles; projection tooling keeps ids stable with suffixes (`RAT-001.emu`, `RAT-001.host`).
- `assumptions`: reserved, must be `[]` until D.16 changes (schema future-proofing, not a feature).
- `expected_kind` is checked by O8 before any value oracle — an exact-looking approximate result fails here even if the digits match (NC-1 enforcement).
- `forbidden_outputs`: substrings that must NOT appear (catches known-bad legacy behavior, e.g. unmarked decimals).
- `memory_budget`: arena bytes for cas:: rows (readable from the `[CAS] arena=` line, `DIAGNOSTICS…SPEC.md:88-94`); ignored where not measurable.
- `tags` drive suite selection (§G): `ci-fast`, `ci-extended`, `firmware-only`, `adversarial`, `property-seed`, `known-bug` (expected-fail until its ticket closes — never silently skipped, reported as XFAIL).

---

## E. Required corpus (≥300 rows; counts are minimums)

Per-category: representative examples, expected result, oracle, rationale. Totals: **330 rows** planned.

### E.1 Arithmetic exactness (25 rows, `ARITH-*`)
Examples: `2+3*4` → `14` (O1; precedence — already CI); `7/7` → `1`; `(-2)^3` → `-8`; `2^10` → `1024`; `12345*6789` → `83810205`; `1-1` → `0`; `10^15+1` → exact int64 boundary; `2^62+2^62` → `expected_kind: approx` on VPAM (O8 flag row) / exact on Giac (GIAC row). Why: NC-2; the int64 cliff (D.1) is where silent wrongness starts.

### E.2 Rational simplification (20 rows, `RAT-*`)
`1/2+1/3`→`5/6`; `10/12`→`5/6`; `-1/-2`→`1/2` (sign normalization); `0.1+0.2`→`3/10` (D.3); `1/3+1/6`→`1/2`; `(2/3)/(4/9)`→`3/2`; `0.125`→`1/8`; 10-decimal-digit boundary row (D.3 cap). Oracle O1. Why: the core exactness promise; den≥10^5 auto-Periodic row (O8 display-mode assert, `CalculationApp.cpp:541-549`).

### E.3 Polynomial simplification (20 rows, `POLY-*`)
`2x+3x`→`5x` (O2); `x·x²`→`x³`; `x+x+x`→`3x`; `2xy+3xy` → **known-bug row** (stays unmerged, §B.2 risk note; tag `known-bug` → XFAIL until fixed or ruled acceptable); `x²+2x+1−(x+1)²`→`0` after expand (O3+O4); degree-descending display order (O1 display row). Why: like-term collection is pointer-identity-based (`SymSimplify.cpp:388-428`) — fragile exactly here.

### E.4 Expansion / factoring (15 rows, `EXP-*`)
`(x+1)^2` expand → `x²+2x+1` (O2/O4, 4 sample points min for degree 2 → N=5); `(x+1)(x−1)`→`x²−1`; 8-term distribution cap boundary: `(a)(b₁+…+b₈)` distributes, `(a)(b₁+…+b₉)` does not (O8 status row; `SymSimplify.cpp:635`); factoring rows are Giac-only (D.9): `factor(x²−1)` with expand-back O3 check. Why: pins the E.5 expansion policy and the ≤8 cap as contract.

### E.5 Powers (20 rows, `POW-*`)
`x^0` (symbolic) → `1`; `0^0` → per D-1 ruling (one row per engine, initially `known-bug` on the divergent one); `0^(-1)` → J.4 error; `2^(-2)`→`1/4`; `(2/3)^2`→`4/9`; `(x^2)^3`→`x^6`; `((x)^2)^(1/2)` adversarial (R-F3 guard, sampled at x=−2 via O4 — must NOT simplify to x); const-fold boundary `2^10` folds, `2^11` folds?, `2^15` beyond |exp|≤10 cap stays symbolic (O8/O2; `SymSimplify.cpp:707`). Why: D.6/D.7 + the R-F3 domain trap.

### E.6 Radicals (20 rows, `RAD-*`)
`√8`→`2√2` (O1); `√(4/9)`→`2/3`; `√2·√3`→`√6` (`MathEvaluator.cpp:333-334`); `√0`→`0`; `√(−4)`→ J.3 error; `√2+√3` → `expected_kind: approx` + forbidden unmarked fraction (O8; the NC-1 flagship row); `(√2)²`→`2`; `³√(−8)` → per D-3 ruling (`RAD-ODD-*`); `√(x²)` stays unevaluated (R-A3). Why: D.5 ceiling + display honesty.

### E.7 Trig (25 rows, `TRIG-*`)
Special-angle table RAD: sin/cos/tan at 0, π/6, π/4, π/3, π/2 (exact O1 per D.11); `tan(π/2)` → J.3; `sin(π)` → exactly `0` (adversarial float-leak row); `sin²x+cos²x`→`1` (O2; `SymSimplify.cpp:430-452`); `sin(−x)`→`−sin x`; `tan x → sin x/cos x` (O2); non-special `sin(1)` → approx-marked (O8+O4). Why: D.11 table is the most user-visible exactness claim.

### E.8 Log/ln/logBase (20 rows, `LOG-*`)
`ln(1)`→`0`; `ln(e)`→`1`; `ln(e³)`→`3`; `log(1000)`→`3` (log10, `MathEvaluator.cpp:918-925`); `log_2(8)`→`3` (LogBase node, `:938-973`); `ln(0)`/`ln(−1)` → J.3; `log_1(5)` → J.3 (base validation `:950-951`); `ln a+ln b → ln(ab)` restriction rows (R-A2: literal-positive merges; symbolic operands after the fix must NOT merge — `known-bug` until CAS-06); `e^(ln 5)`→`5`. Why: D.12 + the assumption-dependent-rewrite fix.

### E.9 Differentiation (30 rows, `DIFF-*`)
Port the 17 SymDiffTest cases as O2 rows + O5-style numeric check per F.6: `d/dx c`→`0`; `d/dx x`→`1`; power/product/quotient-shape/chain: `d/dx x³`→`3x²`; `d/dx (x·sin x)`→`sin x + x·cos x`; `d/dx sin(x²)`→`2x·cos(x²)`; `d/dx eˣ`; `d/dx ln x`→`1/x`; `d/dx arctan x`→`1/(1+x²)`; `d/dx x^x` (general-power rule, sample x>0 only, F.4); `d/dx |x|` → J.2 explicit unsupported (currently nullptr — `known-bug` until CAS-07); nested depth-10 chain (recursion row, O8 timeout). Why: NC-5.

### E.10 Integration (25 rows, `INT-*`)
Table rows: `∫x^n` incl. `n=−1`→`ln|x|` (`SymIntegrate.cpp:120`); `∫sin`, `∫eˣ`, `∫tan`→`−ln|cos x|`, `∫ln x`→`x ln x − x`; linearity `∫(3x²+2x)`; u-sub `∫2x·cos(x²)`→`sin(x²)`; parts `∫x·eˣ`→`(x−1)eˣ`; failure honesty: `∫e^(x²)` → `expected_kind: unevaluated` (G.2 wrapper; currently nullptr — `known-bug` until CAS-08); u-sub depth-5 boundary; parts depth-3 boundary (O8 status rows). Oracle: O5 primary. Why: NC-6.

### E.11 Equations (30 rows, `EQ-*`)
Linear `3x+5=11`→`x=2` exact; fraction root `2x=1`→`1/2`; quadratic two-root ordered `x²−5x+6=0`→`x=2, x=3` (H.2 ordering); repeated root; `x²+1=0`→ no real solutions + `±i` display (D-2a); identity `x=x`→ infinite; contradiction `x+1=x`→ none; inverse `ln x=1`→`x=e`; transcendental `eˣ+x=5` (HybridNewton, `expected_kind: approx`, O8 flag + substitution O4 |f(root)|<1e-8); domain-filter `ln(x)=ln(2x−1)` rejecting arguments ≤0 (`EQ-DOM-*`); `(x−1000)(x−1001)=0` via forced-Newton route (`EQ-ADV-2`, H.2.7 honesty row). Why: NC-7; ports the dormant CASTest/OmniSolverTest cases.

### E.12 Systems (15 rows, `SYS-*`)
2×2 substitution/reduction exact incl. fraction solutions; incompatible → none; dependent → infinite; 3×3 Gauss with pivot swap; singular 3×3; nonlinear 2×2 via resultant (`x²+y²=25, y=x+1`) with substitution O4 verification of every returned pair; 3×3 nonlinear resultant row (O8 timeout budget — resultant blowup risk). Why: SystemSolver exact paths are ExactVal-based (`SystemSolver.h:150-208`) — int64 overflow inside elimination is a live risk (ARITH boundary rows reused with system inputs).

### E.13 Complex (8 rows, `CPLX-*`)
Per D-2(a): `√(−1)` → J.3 in every native engine; `x²+2x+5=0` → no-real + `−1±2i` display metadata (`SingleSolver.h:73-80`); Giac real-mode row: `:solve(x^2+1=0,x)` → empty list (real mode) — pins `complex_mode(false)` (`GiacBridge.cpp:193-212`); `setting_complex_enabled` no-op documentation row. Why: D-2 enforcement and its user-visible surface.

### E.14 Edge cases (15 rows, `EDGE-*`)
Empty expression → J.1; lone `−` → J.1; `--5`→`5`; deeply-nested parens ×50; `1/(1/(1/x))` → stays or normalizes per §E rules; whitespace/unicode in RPN strings; `xy=1` grapher silent-invalid parity row (grapher spec §C.13); max-arity Add approach (256-term sum; O8 memory). Why: parser ambiguity + converter seams.

### E.15 Malformed input (12 rows, `ERR-SYN-*`)
Unbalanced parens (`Parser.cpp:157-171`); unknown token `!` (`Tokenizer.cpp:216-219`); `2++3`; trailing operator; empty function args `sin()`; 63-char grapher limit boundary (grapher spec B.6); Giac `:` empty / `:(`. Oracle O1 on error class per §J. Why: every parser must fail closed.

### E.16 Memory/time stress (12 rows, `STRESS-*`, tags `ci-extended`/`firmware-only`)
`(a+b)^8` repeated-squaring expansion growth (O8 arena budget); simplify pass-cap input (constructed oscillator if one exists — else document non-reproducibility); 1 MB arena exhaustion row → J.7 clean error (currently null-write risk A.4-5 — `known-bug`); ConsTable 32k saturation row (`ConsTable.cpp:161,190`); Giac `:factor(10^60+7)` timeout (firmware, Doc 2 §F.5); CalculusStressTest's 50-expression sweep imported as one row-set with block-count ≤4 assert (`CalculusStressTest.cpp:16-26`). Why: NC-8/NC-9.

### E.17 Known bugs / regressions (10 rows, `BUG-*`)
One row per Document-1 A.4 finding, tagged `known-bug`, XFAIL until its ticket closes: BigInt aliasing (firmware differential row: `:...` vs cas:: on `2^70`-scale rationals); √2+√3 unmarked-approx; 0^0 divergence; Abs-diff nullptr; step-log unbounded (O8 cap row); VariableManager approximate-field persistence loss (`VariableManager.h:153-155` — store huge Ans, reload, assert not-garbage); SequencesApp `n^2+n` term drop (`SequencesApp.cpp:326-370`); GraphModel one-sided relation `x=` plotting x=0 (grapher GR-02); angle-mode badge lie (DV-1). Why: regressions need pins *before* fixes so the fix flips XFAIL→PASS visibly.

### E.18 Display formatting (12 rows, `DISP-*`)
`5/6` vertical fraction (visual golden exists: `calc_fraction_sum.ppm`); `2√2` radical layout; `≈` marker presence for every D.4 row class (O8+O1); Periodic vinculum for `1/3`; Extended 200-digit π row (`CalculationApp.cpp:714-724`); scientific notation for approximate-flag; S⇔D round-trip preserves exact value (D.20.3, O1 after toggle-toggle). Why: D.20 contract; display is where NC-1 is user-visible.

### E.19 Variable storage (10 rows, `VARS-*`)
`4→x` then `x+1`→`5` (extends CI `calc_store_variable.numos`); Ans/PreAns rotation (`VariableManager.h:83`); exact `1/3→A` recall stays exact; cross-store isolation documentation rows (DV-7: `A` in calc invisible to grapher — asserted as *current* contract until P-04); persistence round-trip via save/load (`saveToFlash/loadFromFlash`, `VariableManager.h:101-107`). Why: variables are state — corruption compounds.

### E.20 Angle mode (8 rows, `ANGLE-*`)
After the D.19 sync rule lands: DEG `sin(30)`→`1/2` exact; RAD `sin(π/6)`→`1/2`; DEG grapher `sin(x)` period-360 (grapher spec test plan §B); arcsin output in DEG → `90` for arg 1; metamorphic O7 row `sinDEG(x)==sinRAD(πx/180)` sampled. Until then: `known-bug` rows pinning today's stuck-RAD behavior (DV-1/DV-2). Why: highest user-visible wrongness risk.

### E.21 Graph/evaluator parity (8 rows, `PARITY-*`)
For each DV row that is testable in the emulator: `1/3` VPAM-vs-graph-table value (needs GR-14 `assert_graph_table_value`); `arctan(1)` VPAM ok vs grapher slot-invalid (needs `assert_graph_slot_*`); root-vs-solver agreement (H.3); `abs(-3)` RPN-only. Vehicle note: blocked on GR-14 hooks or equivalent — rows exist now, `tags: ["blocked-gr14"]`. Why: NC-10.

**Total: 25+20+20+15+20+20+25+20+30+25+30+15+8+15+12+12+10+12+10+8+8 = 330 rows.**

---

## F. Differential testing plan

| Pair | Inputs | Normalization | Acceptable differences | Unacceptable |
|---|---|---|---|---|
| cas:: vs Giac (firmware serial; primary) | E.5-E.11 projections + BUG BigInt rows | C.3 restricted grammar both sides → resimplify → O3/O4 | term order; equivalent radical/log forms (O4-verified); Giac solving more than cas:: (cas:: returns unevaluated) | value mismatch; solution-set mismatch; cas:: "exact" ≠ Giac exact |
| MathEvaluator vs cas:: (host) | variable-free E.1-E.8 rows via ASTFlattener | ExactVal→SymNum lift; compare `evaluate()` doubles (O4) + exactness kind (O8) | representation (ExactVal single-radical vs SymAdd) | numeric divergence > tol; exact-flag disagreement on rationals |
| Graph evaluator vs MathEvaluator (emulator) | E.21 rows | double vs `ExactVal::toDouble()` at sampled x | |Δ| ≤ 1e-9·max(1,|v|); documented function-set gaps (DV-3/DV-4) rejected-vs-error asymmetry | silent wrong plot values; angle-mode mismatch post-GR-02 |
| Native vs host Giac (advisory, D-7) | Doc 2 §F.7 | same C.3 normalizer | formatting skew (curated ignore-list) | normalized value mismatch |
| External oracle (later, e.g. SymPy offline) | corpus authoring aid only — expected values generated offline, human-reviewed into `expected_canonical`; never a CI dependency | — | — | — |

---

## G. CI plan

| Suite | Content | Vehicle | Gate | Runtime target |
|---|---|---|---|---|
| **fast** (every PR) | host CAS suites (CAS-01) + corpus `ci-fast` rows (~200) + existing `.numos` semantic suites | `g++` binaries + `emulator_pc` (existing `emulator-build.yml` job, new steps after `:104`) | hard fail; XFAIL rows report but don't fail | ≤ 3 min added (host binaries are seconds; emulator suite already bounded by 30-60 s per-script timeouts, `emulator-build.yml` timeout usage) |
| **extended** (nightly / label-triggered) | `ci-extended` stress rows, property-based generation (10k random exprs, fixed seed + date seed), MathEnginePhaseRegression, host-Giac advisory differential | host | hard fail except advisory differential (report-only) | ≤ 20 min |
| **hardware-only** (manual, releases) | GIAC-* firmware rows, BigInt promotion rows, timeout/memory stress (Doc 2 §F.1-F.5) via CAS-15 driver | human + `esp32s3_n16r8_validate` device | release checklist item; results committed as recorded goldens | ~30 min attended |

Policies:
- **Failure artifacts**: corpus runner emits a JSONL result file (row id, status, actual, expected, oracle detail, `[CAS]` log lines); uploaded via the existing `actions/upload-artifact` pattern (`emulator-build.yml:109-114`, `if: always()`).
- **Flake policy**: zero tolerated flake for O1/O2/O8 (deterministic); O4/O6 rows that flake get their tolerance/sample-range fixed within 48 h or move to `ci-extended` with a linked issue — never auto-retry to green.
- **PR gating**: fast suite gates all PRs touching `src/math/**` or `tests/**` (extend the path filter at `emulator-build.yml:14-44`). Golden-text promotion (Giac goldens) is human-only, same policy as PPM goldens (`golden/README.md:117-123`).
- **Remains manual**: everything hardware-only; visual review of new display goldens; D-n decision ratification.

---

## H. Adversarial tests

Designed-to-break rows (tag `adversarial`), targeting each fragile mechanism:

1. **Simplifier termination**: inputs engineered to oscillate between distribution (grows) and collection (shrinks), e.g. `2(x+y)+2(x+y)` variants at the 8-term cap; assert O8 status ∈ {ok, passcap} and NC-3 on the result — never wall-clock timeout.
2. **Expression explosion**: `(a+b+c+d)^4`-class inputs sized to approach the E.8 growth cap; assert growth-cap status, arena ≤ budget, no null-deref (A.4-5).
3. **Recursion depth**: 200-deep Neg chains and nested Func into diff/flatten/SymExprToAST — currently unguarded (C4.3); expected J.8 after CAS tickets, `known-bug` crash-documentation rows until then (host-only, ASAN).
4. **Exact/approx conversion**: values straddling the int64 cliff (`(2^31)²`, `10^18+1`); `fromDouble` 10-digit boundary (`MathEvaluator.cpp:139,144`); assert O8 kind on every one.
5. **Domain logic**: `√(x²−2x+1)` (perfect square of possibly-negative `x−1`); `ln(x²)`; `(x²)^(1/2)` — assert *absence* of the forbidden rewrites (§E.3-E.4), O4 at negative samples.
6. **Branch cuts / odd roots**: `³√(−8)` per D-3; `(−8)^(1/3)` power form (distinct semantics); Giac `surd` vs `^` differential row.
7. **Parser ambiguity**: `2x^2` (is it `2(x²)`? pin it); `sin x + 1` vs `sin(x+1)` in RPN grammar; implicit-mul chains `2(3)(4)`; `xy` single-identifier trap (grapher spec §C.13).
8. **Allocator/PSRAM**: host ASAN sweep of arena reset-with-live-pointers (`CasMemory.h:302-306` contract); firmware arena-exhaustion row (E.16); CasMemoryPool 256 KB overflow heap-spill detection row (`CasMemory.h:184-187` — assert the spill is at least logged once RT-09 lands).
9. **Output parser** (CAS-14): Doc 2 §F.6 fuzz — truncated Giac outputs, `rootof(` prefix bombs, 1 MB strings, invalid UTF-8.
10. **Step logger**: solver run engineered for >100 steps → truncation + cap log, no PSRAM runaway (I.5); duplicate-step storm (repeated identical rewrites) → dedup effectiveness row.

False-positive risks for the whole adversarial class: rows that "pass" because the input never reached the targeted mechanism (e.g. simplify folded the bomb before distribution). Every adversarial row must therefore also assert a *reachability witness* — the `[CAS]` log line proving the targeted phase ran (pass-count, rule name, or arena delta), not just the final output.

---

*End of Document 3.*
