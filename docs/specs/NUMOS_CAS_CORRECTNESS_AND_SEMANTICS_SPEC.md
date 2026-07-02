# NumOS — CAS Correctness and Semantics Specification

> Authoritative correctness contract for all symbolic/exact math in NumOS.
> Audit date: 2026-07-02 · Branch: `claude/numos-cas-spec-frmkeg` (clean tree, based on `main` @ `a0794b0`).
> Evidence basis: static audit of `src/math/**`, `src/math/cas/**`, `src/math/giac/**`, `lib/giac/**`, `lib/libtommath/**`, `src/apps/**`, `src/input/SerialBridge.*`, `platformio.ini`, `.github/workflows/*`, `tests/**`; cross-checked against `NUMOS_ARCHITECTURE_GROUND_TRUTH.md`, `NUMOS_MEMORY_AND_PSRAM_AUDIT.md`, `NUMOS_MEMORY_ALLOCATION_POLICY.md`, `NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md`, `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md`, `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md`.
>
> Markers: **[V]** verified in source (with `file:line`), **[P]** proposed normative contract (not yet implemented), **[D-n]** open decision with options and consequences.
>
> Ground-truth correction adopted from the reliability spec (`NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md:8`): there is **no** VPAM→Giac escalation. `MathEvaluator::evaluateWithGiac` (`src/math/MathEvaluator.cpp:509-513`, decl `MathEvaluator.h:99`) and the `GiacBridge` class shim (`src/math/GiacBridge.h:25-40`) have **zero callers**. The only runtime path into Giac is `SerialBridge.cpp:155` → `solveWithGiac()` (`src/math/giac/GiacBridge.cpp:390-448`).

Companion documents (this spec set):
- `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md` — Giac boundary, normalization, security.
- `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` — oracle layers, corpus schema, ≥300-test corpus plan, CI plan.
- `NUMOS_CAS_IMPLEMENTATION_TICKETS.md` — ordered CAS-00…CAS-15 roadmap.

---

## A. Executive summary

### A.1 What symbolic math currently exists

NumOS ships **four evaluation engines plus two app-local mini-evaluators** (full taxonomy §C):

1. **VPAM exact evaluator** — `vpam::MathEvaluator` over the VPAM `MathAST`, producing `ExactVal = (num/den)·outer√inner·π^piMul·e^eMul` over `int64` (`src/math/ExactVal.h:39-56`). This is the CalculationApp engine (`CalculationApp.cpp:529`) and the only engine gated by CI semantic asserts today.
2. **Legacy RPN double evaluator** — `Tokenizer`→`Parser`→`Evaluator` (`Evaluator.cpp:79-253`, 64-slot double stack). Used by GraphModel/GrapherApp (`GraphModel.h:170-173`), SystemApp (`SystemApp.h:32-36`), and the numeric `EquationSolver` (`EquationSolver.h:86-88`).
3. **In-tree `cas::` symbolic engine** — 60 files / ~20.3k lines under `src/math/cas/`: hash-consed `SymExpr` DAG, `SymSimplify`/`SymDiff`/`SymIntegrate`, `Omni/Single/System` solvers, RuleEngine tutor stack. Used by CalculationApp edu-steps, EquationsApp, CalculusApp, IntegralApp (dead app), TutorApp (dead app), NeoStdLib.
4. **Giac 1.9.0** (KhiCAS build, `lib/giac`, firmware-only) — reachable **only** via the serial `:` console (`SerialBridge.cpp:148-160` → `GiacBridge.cpp:390-448`).

Mini-evaluators: `MathAnalysis` (numeric root/extremum/Simpson over an `EvalFunc`, `MathAnalysis.h:47-113`) and SequencesApp's `sscanf` pattern matcher (`SequencesApp.cpp:326-370`).

### A.2 Real vs aspirational

| Claim | Status | Evidence |
|---|---|---|
| "CAS Engine: Active Production" (PROJECT_BIBLE) | **Aspirational** — README says "Experimental / in progress" | `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` §A |
| VPAM results are "exact" | **Partially real**: exact only for rationals, one radical term, π/e monomials; everything else silently degrades to a ≤10-digit decimal fraction **without** setting `approximate` | `MathEvaluator.cpp:107-149` (`fromDouble`), `:309-311` (unlike-radical sums) |
| Giac escalation from the calculator | **Not real** — zero callers | `MathEvaluator.cpp:509-513`; reliability spec `:8` |
| cas:: "infinite precision" BigInt | **Firmware-only**: `CAS_HAS_BIGINT=0` off-device; native overflow = error | `CASInt.h:53-59,216-218` |
| SymIntegrate "Strategy 5: unevaluated fallback" | **Aspirational** — engine returns `nullptr`; header claims otherwise | `SymIntegrate.h:27,54` vs `SymIntegrate.cpp:81` |
| ConsTable "Robin Hood probing" | **Aspirational** — plain linear probing | `ConsTable.h:26` vs `ConsTable.cpp:150-159` |
| SymSimplify "weight can only decrease" | **Not enforced** — distribution and tan-rewrite grow trees | `SymSimplify.h:43` vs `SymSimplify.cpp:599-604,793-799` |
| CAS unit tests | **Exist but never run** (doubly dormant: flag never set, `tests/` never compiled) | `main.cpp:56-64,105-113`; `platformio.ini:102-103` |
| `RuleBasedTutor::solveEquationStepByStep` | **Dead code** — zero callers | `RuleBasedTutor.cpp:141` |

### A.3 Engines and how they differ (summary; details §C)

- **Numbers**: ExactVal (int64 rational+radical+π/e) vs double (RPN) vs `CASRational`/`CASNumber` (int64→mbedtls bignum on ARDUINO) vs Giac `gen` (libtommath bignum via `USE_GMP_REPLACEMENTS`, `lib/giac/src/config.h:47`).
- **Function sets disjoint**: VPAM has arcsin/arccos/arctan + logBase but no `sqrt`/`abs` *functions* (`MathAST.h:117-126`); RPN has `sqrt`/`abs` but no inverse trig or logBase (`Evaluator.cpp:125-161`); cas:: adds `Exp`, `Abs`, `Integral` (`SymExpr.h:73-85`); Giac has everything.
- **Angle mode is four independent states** (§D.19): VPAM global stuck at RAD (`MathEvaluator.cpp:53`, never assigned), GraphModel's Evaluator stuck at RAD (never calls `setAngleMode`; `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md:226`), SystemApp/EquationSolver set DEG (`SystemApp.cpp:96`, `EquationSolver.cpp:25`), Giac uses its own context default.
- **Error text differs**: `"Math ERROR"` (VPAM, `MathEvaluator.cpp:355`) vs Spanish `"Error: Div por 0"`/`"Error: Dominio"` (RPN, `Evaluator.cpp:197,133`) vs Giac exception `what()` strings (`GiacBridge.cpp:441-447`).

### A.4 Current biggest correctness risks (ranked)

1. **BigInt hash-consing aliasing (silent wrong answers)**: `ConsTable::structurallyEqual` and `SymNum::computeHashStatic` compare/hash `CASInt::toInt64()` which returns **0 for promoted BigInts** — two different huge exact numbers cons to the same node (`ConsTable.cpp:56-57`; `SymExpr.h:202-203`; `CASInt.h:513`). Breaks exactness precisely in the advertised big-number regime, firmware-only, no test can currently catch it.
2. **Exact-looking approximate results**: `ExactVal::fromDouble` builds a ≤10-digit fraction with `approximate=false` (`MathEvaluator.cpp:107-149`); unlike-radical sums route through it (`:309-311`). `√2+√3` displays as a fraction indistinguishable from an exact result.
3. **Angle-mode incoherence**: `vpam::g_angleMode` is initialized RAD and **never assigned anywhere** (`MathEvaluator.cpp:53`; repo-wide grep), yet StatusBar can display "DEG" (`ui/StatusBar.cpp:246`). `sin(30)` in "DEG" mode returns sin(30 rad).
4. **Zero executed CAS tests**: 8 suites (6.6k lines) doubly dormant (`main.cpp:56-64,105-113`; `platformio.ini:102-103`); Giac has zero tests of any kind.
5. **Unchecked arena allocation in the simplifier**: `SymSimplify::simplifyPass` indexes `allocRaw` results without null checks (`SymSimplify.cpp:228-234,247-253,483-485`); arena hard cap is 1 MB (`SymExprArena.h:63-64`) — PSRAM exhaustion ⇒ null write.
6. **Giac unbounded**: no timeout, no interrupt, shared 64 KB loopTask stack, Giac recursion cap 100 arrives too late (reliability spec C4.1, `:123`; `GiacBridge.cpp:391-392`; `platformio.ini:44`).
7. **0^0 divergence**: VPAM = `Math ERROR` (`MathEvaluator.cpp:404-408`); cas:: `simplifyPow` fires x^0→1 before checking the base ⇒ 0^0→1 (`SymSimplify.cpp:663-664`).
8. **ConsTable saturation**: at `MAX_CAPACITY=32768` full-table `find` returns nullptr and `insert` silently no-ops (`ConsTable.cpp:161,190`) — breaks the pointer-identity invariant the simplifier depends on.
9. **Abs poisons differentiation**: `SymDiff` returns nullptr for `Abs` and nullptr propagates through every combinator (`SymDiff.cpp:281-283`; `:78,93,181,286`) — any |·| anywhere fails the whole derivative.
10. **Persistence round-trip loss**: `VariableManager` serialization omits `approximate/approxVal` (34-byte layout, `VariableManager.h:153-155`) — a stored large Ans reloads as garbage/zero.

### A.5 Top 10 non-negotiable CAS contracts [P]

These are the invariants every future CAS change must preserve. Each maps to acceptance tests in `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md`.

1. **NC-1 No exact-looking lies.** Any displayed result not mathematically equal to the true value MUST carry an approximation marker (flag → `≈` in display). No path may produce an unmarked `fromDouble`-style fraction. (Fixes A.4-2; ticket CAS-10.)
2. **NC-2 Rational arithmetic is exact or an explicit error.** For inputs whose exact value is representable (rational within engine range), the result is bit-exact; overflow either promotes (bignum) or errors — never silently truncates. (`CASInt.h:216-218` native behavior is the acceptable floor; ExactVal's silent `approximate` flip must be surfaced per NC-1.)
3. **NC-3 Simplify is idempotent and terminating.** `simplify(simplify(e)) == simplify(e)` (pointer identity in cas::) and `simplify` completes within its pass cap without oscillation for every corpus input. (§E.13-E.15; ticket CAS-06.)
4. **NC-4 Simplify is sound over ℚ.** Every rewrite fired on a variable-free rational expression preserves exact value (checkable by `evaluate()` before/after). Domain-changing rewrites are only permitted per the §E.4 table.
5. **NC-5 Derivatives verify numerically.** For every supported `d/dx f`, sampled `(f(x+h)−f(x−h))/2h` matches the symbolic derivative within oracle tolerance at all valid sample points. (§F.6; ticket CAS-07.)
6. **NC-6 Integrals verify by differentiation.** `simplify(diff(integrate(f)))` is equivalent to `simplify(f)` (structural or numeric-sampling equivalence). Failure to integrate returns an explicit "unevaluated" outcome, never a wrong closed form. (§G; ticket CAS-08.)
7. **NC-7 Solver results verify by substitution.** Every root returned satisfies |f(root)| within the oracle tolerance; exact roots substitute to exactly 0. No-solution/identity outcomes are asserted as such, not silently empty. (§H; ticket CAS-09.)
8. **NC-8 Bounded execution.** Every CAS entry point (simplify/diff/integrate/solve/Giac) respects the reliability-spec budgets: CAS cooperative deadline 5 s (H-4), Giac 10 s + 2048-char input cap (H-2), 512 KB PSRAM preflight (M-2) (`NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md:217,224-227`). Timeout is a defined error, not a hang.
9. **NC-9 OOM is an error, not corruption.** Arena/pool/table exhaustion produces a defined error result (§J); no null-deref, no silent no-op consing, no partial trees reaching converters (`SymExpr.cpp:149,327,331` unchecked clone chains must be fixed).
10. **NC-10 Engine parity is declared, not accidental.** For every input class both a) accepted by two engines, the expected relation between their outputs is written in the parity table (§C.8) and tested; or b) accepted by only one, the other's rejection is tested. No silent divergence.

---

## B. Current CAS architecture

Component map. "Tests" refers to *executed* tests; all 8 CAS suites are dormant (§B.14).

### B.1 `cas::SymExpr` DAG + `SymExprArena` + `ConsTable`

- **Purpose**: immutable, hash-consed expression DAG; pointer identity = structural identity (`SymExpr.h:107-139`).
- **Files**: `SymExpr.h/.cpp`, `SymExprArena.h`, `ConsTable.h/.cpp`.
- **Representation**: node types `Num, Var, Neg, Add, Mul, Pow, Func, Paren, PlusMinus, Subscript, CoeffAssign` (`SymExpr.h:93-105`); functions `Sin, Cos, Tan, ArcSin, ArcCos, ArcTan, Ln, Log10, Exp, Abs, Integral` (`SymExpr.h:73-85`). `SymNum` holds `CASRational _coeff` + `int64 _outer,_inner` + `int8 _piMul,_eMul` (`SymExpr.h:145-210`) — i.e. `a·√b·π^m·e^n` exactly; doubles never live in the DAG. N-ary Add/Mul, arity ≤ 65535 (`SymExpr.h:266,296`). Canonical child order via `symCanonicalLess` (non-Num before Num, then by hash — `SymExpr.h:499-504`).
- **Callers**: CalculationApp edu-steps (`CalculationApp.cpp:919-964`), EquationsApp, CalculusApp, IntegralApp, NeoStdLib (§B.13).
- **Memory**: bump arena, 64 KB blocks × 16 = 1 MB cap (`SymExprArena.h:63-64`); lazy first block (`:97`); SPIRAM-first with any-heap fallback (`:219-223`, policy violation MT-05); `allocRaw` → nullptr on exhaustion (`:103`); `reset()` keeps block 0, clears ConsTable (`:143-157`). ConsTable: 4096→32768 buckets, load 0.70 (`ConsTable.h:90-92`).
- **Timeout**: none.
- **Tests**: `tests/SymExprTest.cpp` (31 tests) — dormant.
- **Risks**: BigInt consing aliasing (A.4-1); table saturation (A.4-8); hash mixer written for 64-bit `size_t` truncates to 32-bit on ESP32-S3 (`ConsTable.h:63`) — weaker distribution, correctness preserved by structural compare *except* in the BigInt case.

### B.2 `SymSimplify`

- **Purpose**: fixed-point bottom-up rewriter. Entry `simplify()` loops ≤ `MAX_PASSES=10` breaking on pointer identity (`SymSimplify.h:60`; `SymSimplify.cpp:193-203`; note the `.cpp` banner stale-says 8, `SymSimplify.cpp:26`).
- **Rules** (all [V], see §E for the contract): Neg folding/distribution (`SymSimplify.cpp:295-343`); Add flatten/zero-drop/const-fold/like-terms/Pythagorean sin²+cos²→1/ln-merge (`:357-487`); Mul flatten/zero/one/const-fold/power-base-merge/distribution (`:501-607`, distribution guard ≤8 terms `:616-649`); Pow identities + const fold for integer exponents in [−10,10] (`:662-717`); Func identities incl. tan→sin·cos⁻¹ (`:736-831`).
- **Callers**: CalculationApp edu-steps (`CalculationApp.cpp:935`), CalculusApp (`CalculusApp.cpp:848`), SymIntegrate (pre/post, `SymIntegrate.cpp:41-53`), NeoStdLib.
- **Memory**: arena; **unchecked `allocRaw` at `SymSimplify.cpp:228-234,247-253,483-485`** (A.4-5).
- **Timeout**: none (H-4 deadline is proposed, not implemented).
- **Tests**: 10 cases inside `tests/SymDiffTest.cpp` — dormant.
- **Risks**: no size-growth cap; like-term collection cannot merge ≥3-factor monomials (`2xy+3xy` stays split); 0^0→1 rule-order bug (A.4-7).

### B.3 `SymPoly` / `SymPolyMulti`

- **Purpose**: `SymPoly` = univariate `SymTerm{CASRational coeff, char var, int16 power}` vector with normalize/add/sub/mul(FOIL)/divScalar (`SymPoly.h:50-83`; `SymPoly.cpp:186-400`). `SymPolyMulti` = univariate view with SymExpr-tree coefficients + **Sylvester resultant via Bareiss** for nonlinear elimination (`SymPolyMulti.h:76-101`).
- **Callers**: ASTFlattener poly path; SingleSolver; SystemSolver nonlinear (`SystemSolver.cpp:693-694,725,916-975`).
- **No general factoring exists** — only the cubic tutor's rational-root Ruffini (`TutorTemplates.cpp:755+`).
- **Tests**: CASTest Phase A/B — dormant.

### B.4 `SymDiff`

12-rule recursive differentiator (`SymDiff.cpp:33-290`): const/var/neg/sum/n-ary product/power(3 cases)/chain for 9 function kinds. `Abs` → nullptr, and nullptr propagates (`:281-283; :78,93,181,286`). No depth guard. Null-checks its own arena allocs (`:89,111,117`). Tests: SymDiffTest (17 cases) — dormant. Contract: §F.

### B.5 `SymIntegrate`

simplify → cascade (table → linearity → u-sub ≤ depth 5 → parts ≤ depth 3, LIATE) → **nullptr on failure** (`SymIntegrate.cpp:41-536`; caps `SymIntegrate.h:46-49`). Header's unevaluated-fallback claim is aspirational (`SymIntegrate.h:27,54`). Callers: CalculusApp (`CalculusApp.cpp:879`), IntegralApp (dead), NeoStdLib. Tests: none executed. Contract: §G.

### B.6 Solvers (`OmniSolver`, `SingleSolver`, `SystemSolver`, `SystemHeuristics`, `HybridNewton`)

- OmniSolver classifies Polynomial/SimpleInverse/MixedTranscendental (`OmniSolver.h:67-71`); analytic isolation depth-capped at 20 (`OmniSolver.cpp:638`); complex-root metadata `hasComplexRoots/complexReal/complexImagMag` (`OmniSolver.h:98-102`).
- SingleSolver: linear + quadratic exact (incl. complex-conjugate metadata, `SingleSolver.h:73-80`), Newton for degree ≥3 (50 iter, tol 1e-12, accept |f|<1e-8, `SingleSolver.cpp:458-495`); max 2 exact solutions (`SingleSolver.h:56`).
- SystemSolver: exact 2×2 (substitution/reduction) and 3×3 Gauss over `vpam::ExactVal` (`SystemSolver.h:150-208`); nonlinear 2×2/3×3 via resultant (`SystemSolver.cpp:665,883`).
- HybridNewton: 16 fixed seeds, symbolic Jacobian via SymDiff; `maxRoots` parameter ignored (`HybridNewton.cpp:50-60`); converged roots are **rationalized doubles** stored as `ExactVal::fromDouble` (`:97-98`) — not exact (NC-1 applies).
- Callers: EquationsApp (`EquationsApp.cpp:2197,2252-2329`), NeoStdLib. Tests: CASTest/OmniSolverTest — dormant. Contract: §H.

### B.7 Step/tutor stack (`CASStepLogger`, `PedagogicalLogger`, `RuleEngine`, `AlgebraicRules`, `TutorTemplates`, `SystemTutor`, `RuleBasedTutor`)

- `CASStepLogger`: `CASStep` records with non-owning `SymExpr*` into the arena (`CASStepLogger.h:83-105`); PSRAM `StepVec` (`:108`); dedup only vs the immediately previous step (`CASStepLogger.cpp:87-94`); **no count cap** (risk: unbounded PSRAM growth).
- `RuleEngine`: TRS over the *separate* `PersistentAST` shared_ptr/COW tree family, allocated from `CasMemoryPool` (256 KB PMR slab, overflow silently spills to heap — `CasMemory.h:184-187,263`); `applyToFixedPoint(maxSteps=100)` with cycle/no-progress halts (`RuleEngine.h:372-380`).
- `AlgebraicRules`: 10 rules, 4 phases (`AlgebraicRules.h:25-42,78`).
- `TutorTemplates`: quadratic/cubic/log/exp/radical tutors; `SOLUTION_TOLERANCE=1e-6` (`TutorTemplates.cpp:40-42`).
- `SystemTutor`: 2×2 sessions, called from EquationsApp (`EquationsApp.cpp:1354,3193`).
- `RuleBasedTutor`: **dead code**, zero callers (`RuleBasedTutor.cpp:141`).
- Contract: §I.

### B.8 Converters (`ASTFlattener`, `SymExprToAST`, `SymToAST`, `CasToVpam`, `AstDiff`, `PersistentAST`)

- ASTFlattener: VPAM → SymPoly/SymEquation (poly path) or SymExpr (transcendental, `flattenToExpr`) (`ASTFlattener.h:58-174`).
- SymExprToAST: SymExpr → VPAM for rendering; std-heap `unique_ptr`, not arena (`SymExprToAST.h:42-66`); **no size cap** on the VPAM tree built (reliability risk C5.8).
- SymToAST: SymPoly/SymEquation/ExactVal → VPAM (`SymToAST.h:43-63`).
- CasToVpam: PersistentAST → VPAM with highlight tracking (`CasToVpam.h:52-89`); `MAX_INTEGER_DISPLAY=1e9` (`CasToVpam.cpp:40`).
- AstDiff: smallest-changed-subtree via shared-pointer identity (`AstDiff.cpp:29-104`), used only by `RuleEngine.cpp:711`.
- Risks: two independent tree families (SymExpr vs PersistentAST) with independent converters = double the round-trip surface; every converter is a place where semantics can drift (implicit-mul, sign, precedence).

### B.9 Giac bridge

See `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md` §A. Summary: `solveWithGiac` (`GiacBridge.cpp:390-448`), single global context configured exact/real mode (`:45,193-212`), try/catch containment (`:441-447`), `[MEM] giac-*` probes (`:394,439,442,445`), serial-only caller, no timeout, no tests.

### B.10 `MathEvaluator` / `ExactVal` (VPAM engine)

- Entry `evaluate(const MathNode*)` (`MathEvaluator.h:109`; dispatch `MathEvaluator.cpp:527`). Per-node evaluators `MathEvaluator.h:151-160`.
- ExactVal fields and predicates: `ExactVal.h:39-105`. Arithmetic: `exactAdd/Sub/Mul/Div/Neg/Pow/Sqrt` (`ExactVal.h:125-143`; impls in `MathEvaluator.cpp:250-460`).
- Display modes Symbolic/Periodic/Extended (`MathEvaluator.h:81-85`); result-AST builders (`MathEvaluator.cpp:1053-1058,1202,1297,1367`).
- Memory: `MathAST operator new` is PSRAM-first (`MathAST.cpp:50-60`); on firmware **all** `new` is PSRAM-first via the GiacAlloc global override (`GiacAlloc.cpp:24-31`).
- Timeout: none (single-threaded, but tree-recursive — depth bounded only by editor structure).
- Tests: **the only executed math tests in the repo** — `.numos` semantic asserts (`emulator-build.yml:260-356`), covering exact integer/fraction arithmetic, one div-by-zero error, one variable store.
- Risks: NC-1 violations via `fromDouble` (A.4-2); int64 overflow flips `approximate` (correct per NC-1 only if displayed as approximate — currently is, via scientific notation, `MathEvaluator.cpp:1058`); persistence loss (A.4-10).

### B.11 Legacy RPN pipeline

Files `Tokenizer.* / Parser.* / Evaluator.* / ExprNode.h / VariableContext.*`. §C.3 for taxonomy. Tests: none executed except indirectly through Grapher `.numos` scripts (which assert UI state, not numeric values — `emulator-build.yml:495-531`).

### B.12 VPAM `MathAST` + `CursorController`

Structural editor/display tree, 17 node types (`MathAST.h:72-90`); **no evaluation methods** (grep: none). CursorController mutates it from keys (`CalculationApp.cpp:293,326-398`). Not an engine, but the *input contract* for both MathEvaluator and ASTFlattener — divergences between what CursorController can build and what each consumer handles are input-space gaps.

### B.13 App evaluation paths

| App | Path | Evidence |
|---|---|---|
| CalculationApp | keys → CursorController → MathAST → `_evaluator.evaluate` → ExactVal → mode-select → resultToAST | `CalculationApp.cpp:326-398,479-567,707-737` |
| CalculationApp edu-steps | MathAST → ASTFlattener → SymExpr → `SymSimplify::simplifyPass` per step → SymExprToAST | `CalculationApp.cpp:919-964,935,1065` |
| EquationsApp | MathAST → ASTFlattener → SymPoly/SymExpr → Single/System/Omni solvers + tutors → SymToAST/CasToVpam | `EquationsApp.cpp:1318-1354,2197-2329` |
| CalculusApp | MathAST → ASTFlattener → SymDiff/SymIntegrate → SymSimplify → SymExprToAST | `CalculusApp.cpp:793-1005` |
| GrapherApp | string → cached RPN → double per sample; MathAnalysis for calc menu | `GraphModel.cpp:83-89,218-236`; `MathAnalysis.h:47-113` |
| SystemApp legacy menu | string → RPN (DEG) + numeric EquationSolver | `SystemApp.h:32-36`; `SystemApp.cpp:96-97` |
| Serial console | `:expr` → Giac | `SerialBridge.cpp:148-160` |
| SequencesApp | `sscanf` on 4 hardcoded patterns | `SequencesApp.cpp:326-370` |

### B.14 Test reality

Eight cas:: suites (`CASTest, SymExprTest, ASTFlatExprTest, SymDiffTest, OmniSolverTest, BigIntTest, CalculusStressTest, TutorTemplateTest`) are included by `main.cpp:57-64` and invoked at `main.cpp:106-113` **only under `CAS_RUN_TESTS`**, which no environment defines; additionally `build_src_filter = +<*>` compiles `src/` only, never `tests/*.cpp` (`platformio.ini:102-103`) — so even with the flag the `run*Tests()` bodies would not link. `MathEnginePhaseRegression.cpp` (2124 lines, real `main()` at `:2041`) is referenced by nothing. Full inventory: `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` §B.

---

## C. Engine taxonomy

### C.1 VPAM exact engine (`vpam::MathEvaluator` + `ExactVal`)

| Property | Value | Evidence |
|---|---|---|
| Input | pre-built VPAM MathAST (no string parser) | `MathEvaluator.h:109` |
| Numbers | `(num/den)·outer√inner·π^piMul·e^eMul`, int64; `approximate` double escape | `ExactVal.h:39-56` |
| Variables | `A–F, x, y, z, Ans(#), PreAns($)` via VariableManager singleton (ExactVal, LittleFS `/vars.dat`) | `MathEvaluator.cpp:995-1004`; `VariableManager.h:138-158` |
| Functions | Sin/Cos/Tan/ArcSin/ArcCos/ArcTan/Ln/Log + LogBase node + Root node | `MathAST.h:117-126`; `MathEvaluator.cpp:845-973,769-808` |
| Exactness | exact for rationals / single radical / π-e monomials; else `fromDouble` ≤10-digit fraction (unmarked) | `MathEvaluator.cpp:107-149,309-311` |
| Complex | none; `ConstKind::Imag` exists but unhandled → error | `MathAST.h:196-200`; `MathEvaluator.cpp:988` |
| Angle mode | global `g_angleMode`, init RAD, never assigned | `MathEvaluator.cpp:53` |
| Domain/error | `ExactVal{ok=false,error="Math ERROR"}` | §J.1 |
| Memory | PSRAM-first MathAST nodes; tree recursion, no depth guard | `MathAST.cpp:50-60` |
| Test coverage | `.numos` semantic asserts (exact int/frac only) | `emulator-build.yml:260-356` |

### C.2 Legacy numeric RPN engine

| Property | Value | Evidence |
|---|---|---|
| Input | text; implicit multiplication inserted; any `ident(` becomes a Function token | `Tokenizer.h:22-24,98`; `Tokenizer.cpp:157-167` |
| Numbers | double only; 64-slot stack | `Tokenizer.h:60`; `Evaluator.cpp:85` |
| Variables | `A–Z` (case-insensitive) + `PI/E/ANS` via per-instance VariableContext (double, NVS `calcVars`) | `Evaluator.cpp:47-77`; `VariableContext.h:61-70` |
| Functions | sin, cos, tan, sqrt, log(=log10), ln, abs; others → `"Función no soportada"` | `Evaluator.cpp:125-161` |
| Exactness | none (IEEE double) | — |
| Complex | none | — |
| Angle mode | per-instance `_angleMode`, default RAD; DEG only when a caller sets it | `Evaluator.cpp:24-25`; `Evaluator.h:47` |
| Domain/error | Spanish messages; every result isfinite-sanitized | `Evaluator.cpp:130-167,196-219,244-248` |
| Memory | fixed stack; token vectors on heap | `Evaluator.h:49-52` |
| Tests | none direct | — |
| Divergence | function set, exactness, variables store, angle default, error text — all differ from C.1 | §C.8 |

### C.3 `cas::` symbolic engine

| Property | Value | Evidence |
|---|---|---|
| Input | SymExpr DAG (from ASTFlattener) or PersistentAST (tutor stack) | `ASTFlattener.h:115-174`; `RuleEngine.h:97-98` |
| Numbers | `CASRational` (CASInt int64→mbedtls bignum ARDUINO-only) + radical/π/e fields in SymNum | `SymExpr.h:145-210`; `CASInt.h:53-59` |
| Variables | single-char SymVar | `SymExpr.h:145` (SymVar) |
| Functions | Sin..ArcTan, Ln, Log10, Exp, Abs, Integral | `SymExpr.h:73-85` |
| Exactness | exact within representation; native builds error on int64 overflow | `CASInt.h:216-218` |
| Complex | quadratic-root metadata only (real part + imag magnitude), no complex arithmetic | `SingleSolver.h:73-80`; `OmniSolver.h:98-102` |
| Angle mode | **none** — trig simplification rules are mode-agnostic (sin(0)=0 is valid in both) but any numeric fold assumes radians | `SymSimplify.cpp:736-831` |
| Domain/error | nullptr returns + `CASNumber::Error` form; inconsistent (§J) | `SymDiff.cpp:281-283`; `CASNumber.h:67-72` |
| Memory | SymExprArena 1 MB cap / CasMemoryPool 256 KB + heap spill / PSRAMAllocator throws | §B.1, `CasMemory.h:184-187,263`, `PSRAMAllocator.h:84` |
| Tests | 8 dormant suites | §B.14 |

### C.4 Giac-backed engine

Full contract in the integration spec. Taxonomy row: string input (serial only, ≤255 chars by SerialBridge buffer `SerialBridge.cpp:116`); Giac `gen` numbers (libtommath bignum); exact + real mode configured (`GiacBridge.cpp:193-212`); complex mode **off** (`complex_mode(false)`, `:197` area); no angle-mode plumbing from NumOS; errors = exception strings; memory = global PSRAM-first new; **no tests**.

### C.5 Graph numeric evaluator (GraphModel + MathAnalysis)

Engine C.2 with cached RPN per function slot (`GraphModel.h:65,74`), `evalAt` per-sample (`GraphModel.cpp:218-236`), implicit residual `G=lhs−rhs` (`:241-261`), NAN-skip semantics (grapher spec §C.9). MathAnalysis: bisection/golden-section/Simpson, `MAX_ITER=100`, tol 1e-5 (`MathAnalysis.h:50-53`). Angle: stuck RAD (grapher spec `:226-228` proposes the sync rule, ticket GR-02).

### C.6 App-specific mini evaluators

- SequencesApp pattern matcher (`SequencesApp.cpp:326-370`): 4 hardcoded shapes; `n^2+n` matches the `n^2` branch and **drops** `+n` — this is a live wrong-answer generator, must be either replaced by MathEvaluator or corpus-tested (ticket CAS-00 scope note).
- Numeric EquationSolver (`EquationSolver.cpp:347-519`): black-box polynomial identification by sampling at x∈{−2..2} + finite differences (tolerance 1e-9), Newton fallback (20 iters). DEG default (`:25`).

### C.7 Engine feature matrix

| Feature | VPAM | RPN | cas:: | Giac | Graph |
|---|---|---|---|---|---|
| Exact rationals | ✔ | ✘ | ✔ | ✔ | ✘ |
| Radicals | 1 term | ✘ | per-SymNum | full | ✘ |
| π/e symbolic | ✔ (monomial) | value only | ✔ (monomial) | full | value only |
| BigInt | ✘ (→approx) | ✘ | ARDUINO only | ✔ | ✘ |
| sqrt as function | node | ✔ | via Pow | ✔ | ✔ |
| abs | ✘ | ✔ | ✔ (diff fails) | ✔ | ✔ |
| inverse trig | ✔ | ✘ | ✔ | ✔ | ✘ |
| logBase | ✔ | ✘ | via ln ratio | ✔ | ✘ |
| exp(x) | via e^x | ✘ | ✔ | ✔ | ✘ |
| complex | ✘ | ✘ | metadata | off by config | ✘ |
| angle-mode honored | stuck RAD | caller-set | n/a | ctx default | stuck RAD |

### C.8 Divergence register (each row becomes parity tests, ticket CAS-11)

| # | Input class | Engines | Current divergence | Evidence |
|---|---|---|---|---|
| DV-1 | `sin(30)` in DEG UI | VPAM vs StatusBar claim | computes RAD under a DEG badge | `MathEvaluator.cpp:53`; `StatusBar.cpp:246` |
| DV-2 | `sin(x)` plot in DEG | Graph vs VPAM intent | RAD curve under DEG badge | grapher spec `:226` |
| DV-3 | `arctan(1)` | VPAM ok, RPN error | "Función no soportada" in Grapher | `Evaluator.cpp:159` |
| DV-4 | `abs(-3)` | RPN ok, VPAM impossible (no node) | function gap | `MathAST.h:117-126` |
| DV-5 | `1/3` | VPAM exact vs RPN 0.333… | representation | `ExactVal.h` vs `Evaluator.cpp:85` |
| DV-6 | `0^0` | VPAM error vs cas:: 1 | rule ordering | `MathEvaluator.cpp:404-408` vs `SymSimplify.cpp:663-664` |
| DV-7 | `A` variable | VariableManager vs VariableContext disjoint stores | value invisible across apps | `VariableManager.h:138-158`; `VariableContext.h:61-70` |
| DV-8 | div-by-zero text | "Math ERROR" vs "Error: Div por 0" vs Giac string | user-visible inconsistency | `MathEvaluator.cpp:355`; `Evaluator.cpp:197` |
| DV-9 | `√2+√3` squared | VPAM ≈decimal vs cas:: symbolic Add vs Giac `5+2√6` | exactness ceiling | `MathEvaluator.cpp:309-311` |
| DV-10 | huge integers | VPAM approx-flag vs cas:: native error vs cas:: ARDUINO bignum-with-aliasing vs Giac exact | four different behaviors | `ExactVal.h:51-56`; `CASInt.h:216-218,513` |

---

## D. Symbolic semantics contract

Normative target semantics for NumOS. Each class states: current verified behavior, intended contract [P], examples with canonical/acceptable/unacceptable forms, the oracle that checks it (oracle IDs O1–O8 defined in `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` §C), and display expectation. "Canonical form" always refers to the engine-native canonical result; display strings are separate (§D.20).

### D.1 Integers

- [V] VPAM: int64; beyond range → `approximate=true` double shown in scientific notation (`ExactVal.h:51-56`; `MathEvaluator.cpp:1058`). cas::: CASInt int64→bignum (ARDUINO) or error (native) (`CASInt.h:53-59,216-218`). Giac: exact bignum.
- [P] Contract: integer add/sub/mul/pow with integer result is exact while representable. `12345·6789 = 83810205` exactly. Beyond engine range: VPAM must display with `≈` (NC-1); cas:: native must error; cas:: firmware must be exact **after the aliasing fix (CAS ticket)** — until then firmware BigInt results are untrusted.
- Examples: `2^62 + 2^62` → VPAM: `≈9.223372037e18` (approximate marked); Giac: `9223372036854775808` exact. Unacceptable: an unmarked exact-looking `9223372036854775807`-adjacent wrong digit string.
- Oracle: O1 canonical string (small), O6 differential vs Giac (large). Display: no decimal point, no `≈` when exact.

### D.2 Rationals

- [V] Normalization: gcd-reduced, den>0, sign in num (`ExactVal.h:112-113` simplify contract; `CASRational.h:22-24`).
- [P] Canonical: `p/q` with gcd(|p|,q)=1, q≥1; q=1 renders as integer. `1/2+1/3` → `5/6` (CI-gated today, `emulator-build.yml:260-286`). Acceptable alternate: none at engine level. Unacceptable: `10/12`, `-1/-2`, decimal rendering when mode=Symbolic.
- Oracle: O1. Display: vertical fraction (VPAM NodeFraction); Periodic mode shows repeating decimal via long division (`MathEvaluator.h:81-85`).

### D.3 Decimals (user-typed)

- [V] `NodeNumber` text parsed in `evalNumber` (`MathEvaluator.cpp:663-731`); decimal → fraction. RPN parses to double.
- [P] Contract: a typed terminating decimal is **exactly** the rational it denotes: `0.1 + 0.2 = 3/10` (not 0.30000000000000004). Typed decimals with >10 significant fraction digits: current `fromDouble` truncates at 10 (`MathEvaluator.cpp:139,144`) — intended: parse digit string directly as num/den without a double round-trip; until then inputs beyond 10 decimals are out of the exactness contract and must be marked approximate (NC-1).
- Oracle: O1. Display: Symbolic mode shows the reduced fraction; S⇔D toggles decimal.

### D.4 Floats / approximations

- [P] Contract: an approximate value is a tagged type, never a disguised rational. Sources of approximation (non-special-angle trig, non-integer powers, overflow, Newton roots — `MathEvaluator.cpp:393-399,418-422`; `HybridNewton.cpp:97-98`) must set `approximate=true` (or the cas::Floating form) and display with `≈`. **This is the single largest current contract violation** (`fromDouble` leaves `approximate=false`, `MathEvaluator.cpp:107-149`).
- Oracle: O8 (flag assertions) + O4 numeric sampling for value. Unacceptable: `sin(1)` displayed as `8414709848/10000000000` with no marker.

### D.5 Radicals

- [V] VPAM: single `outer√inner`, perfect-square extraction (`ExactVal.h:115-116`; `simplifyRadical` errors on negative radicand `MathEvaluator.cpp:219-223`); `√2·√3=√6` (`:333-334`); unlike-radical sums degrade (`:309-311`). cas::: per-SymNum radical, `CASNumber::sqrt` extracts square factors (`CASNumber.h:159-165`). Root node: degree 2 exact, degree n → double (`MathEvaluator.cpp:775-793`).
- [P] Canonical: `k√m` with m square-free, k rational. `√8 → 2√2`; `√(4/9) → 2/3`; `√0 → 0`; `√(−4)` → domain error (real mode). Sums of unlike radicals: engine-level result may be an unevaluated sum (cas:: SymAdd) — acceptable; VPAM's decimal degrade is acceptable **only** with the ≈ marker. Unacceptable: `√8` displayed as `2.828…` in Symbolic mode; unmarked decimal for `√2+√3`.
- Oracle: O1 (single radicals), O4 (sums), O6 (vs Giac `withsqrt(true)` forms, `GiacBridge.cpp:193-212`).
- Display: radical glyph via NodeRoot; never `^0.5`.

### D.6 Powers

- [V] `exactPow`: integer exponents exact for rational bases (binary exp via repeated mult); non-integer exponent or non-rational base → double (`MathEvaluator.cpp:388-423`). cas:: `simplifyPow` identities + const fold |exp|≤10 (`SymSimplify.cpp:662-717`).
- [P] Canonical: `2^10 → 1024`; `(2/3)^2 → 4/9`; `2^(1/2) → √2` (currently VPAM Root-only; cas:: keeps `Pow(2,1/2)` — acceptable alternate); `x^1 → x`; `x^0 → 1` for x≠0.
- Unacceptable: `2^10 → 1024.0`; `(a^b)^c → a^(bc)` for non-numeric b,c (invalid over ℝ: `((-1)^2)^(1/2) ≠ (-1)^1`; current cas:: guards this by requiring numeric exponents, `SymSimplify.cpp:689-699` — the guard must additionally require integer c or positive base, see §E.4 row R-F3).
- Oracle: O1/O3 + O4 sampling.

### D.7 Negative powers

- [V] `exactPow` inverts base for negative integer exponents (`MathEvaluator.cpp:425-428`); `0^neg` → error (`:413`); RPN `0^neg` → "Error: Div por 0" (`Evaluator.cpp:204-207`).
- [P] Contract: `x^(−n) = 1/x^n` for x≠0; `2^(−2) → 1/4` exact; `0^(−n)` → division-by-zero error in every engine. cas:: `SymPoly` supports negative powers (`SymPoly.h:50-53` int16 power) — normalization must keep them (no forced common-denominator rewrite).
- **0^0 [D-1]**: VPAM errors (`MathEvaluator.cpp:404-408`); cas:: returns 1 (`SymSimplify.cpp:663-664` rule order). Options: (a) define 0^0=1 everywhere (combinatorial convention; requires VPAM change); (b) define 0^0=error everywhere (analysis convention; requires reordering `simplifyPow` to check base-zero before x^0→1 — but note for *symbolic* x, x^0→1 must still fire since x is presumed nonzero, making the rule assumption-dependent); (c) keep divergence documented. **Recommendation: (b) for literal 0^0, keeping x^0→1 for symbolic x with the assumption noted** — matches calculator user expectations (Casio/TI report error) and keeps the simplifier useful. Consequence: `simplifyPow` needs a literal-zero-base check before the exponent-zero rule; corpus rows DV-6 pin whichever is chosen.

### D.8 Polynomial expansion

- [V] cas:: distribution only when Mul has exactly 2 factors and the Add has ≤8 terms (`SymSimplify.cpp:616-649`); SymPoly::mul FOIL (`SymPoly.cpp:278-400`).
- [P] Contract: `(x+1)^2` flattened via ASTFlattener poly path → `x^2+2x+1` with descending powers (SymPoly `normalize()` sorts desc, combines, drops zeros — `SymPoly.cpp:186-232`). Expansion policy: §E.5. Unacceptable: dropped terms, unnormalized `x^2+x+x+1`.
- Oracle: O3 structural after normalize + O4 sampling at 5 points (degree-d poly needs d+1 points; use max-degree+1).

### D.9 Factorization

- [V] No general factorization exists in cas:: (only cubic-tutor Ruffini, `TutorTemplates.cpp:755+`). Giac `factor` works via alias mapping (`GiacBridge.cpp:108-143`).
- [P] Contract: NumOS-native factor is **out of scope** until a ticket exists; UI must not offer "factor" backed by cas::. Giac `factor(x^2-1)` → `(x-1)*(x+1)`; canonical ordering of factors follows Giac output (do not re-sort; oracle O6 accepts factor-order permutations via O5-style resimplification).
- Oracle: expand(factored) ≡ original via O3/O4.

### D.10 Simplification / cancellation

Contract is §E in full. Headline: `(x^2−1)/(x−1)` — cas:: has no rational-function cancellation [V: no such rule in `SymSimplify.cpp:209-831`]; contract: leave unevaluated rather than cancel, because cancellation changes the domain (x=1). If/when implemented, result must be `x+1` **with domain annotation** or the rewrite is forbidden (§E.4 row R-F1).

### D.11 Trigonometric functions

- [V] VPAM special angles snap to 0/±1/±½ (`MathEvaluator.cpp:848-856`); tan at |cos|<eps errors (`:874`); cas:: sin(0)/cos(0)/tan(0), parity rules, sin²+cos²→1, tan→sin/cos (`SymSimplify.cpp:736-831,430-452`).
- [P] Contract (RAD, exact): `sin(0)=0`, `cos(0)=1`, `sin(π/6)=1/2`, `sin(π/4)=√2/2`, `sin(π/3)=√3/2`, `sin(π/2)=1`, `tan(π/4)=1`, `tan(π/2)=domain error`. Non-special arguments: approximate with marker (D.4). In DEG mode the same table applies to 0/30/45/60/90. Current VPAM covers a subset [V: `:852-856` — full table membership is corpus work, rows TRIG-*].
- Unacceptable: `sin(π)` ≠ exactly 0 (floating sin(3.14159…) ≈ 1.2e-16 leaking to display); any special angle rendered as a 10-digit fraction.
- Oracle: O1 exact table; O4 for approximations (|Δ| ≤ 1e-12 relative at double precision).

### D.12 Logarithms (ln / log / logBase)

- [V] VPAM: `ln(1)=0, ln(e)=1, ln(e^n)=n` (`MathEvaluator.cpp:902-909`); `log10(10^n)=n` (`:918-925`); `logBase = ln x / ln n` with base/arg validation (`:938-973,950-951`). cas::: ln(1), ln(e^x)→x, e^(ln x)→x, log10(1) (`SymSimplify.cpp:736-831`); ln-merge ln a + ln b → ln(ab) (`:454-471`).
- [P] Contract: `ln: (0,∞)→ℝ`, `ln(x≤0)` = domain error (real mode). `log_b(x)`: b>0, b≠1, x>0. Exact hits: `log_2(8)=3`, `log_10(1000)=3`, `ln(e^3)=3`. ln-merge is valid only for a,b>0 — the current unconditional merge (`:454-471`) is an **assumption-dependent rewrite**; §E.4 row R-A2 restricts it to literal-positive or assumption-tagged operands.
- Unacceptable: `ln(−1)` returning a value in real mode; `ln(x²)→2ln(x)` (loses x<0 half) — currently absent [V], must stay forbidden without assumptions.
- Oracle: O1 exact hits; O4 with domain-restricted sampling.

### D.13 Absolute value

- [V] cas:: `Abs` in SymFuncKind; simplify folds |c| and |−x|→|x| (`SymSimplify.cpp:814-827`); SymDiff fails on Abs (nullptr, `SymDiff.cpp:281-283`). RPN `abs` numeric (`Evaluator.cpp:125-161`). VPAM: no abs node.
- [P] Contract: `|c| = c or −c` exactly for literals; `|x|` stays symbolic; `√(x²) → |x|` (not x) if that rewrite is ever added. `d/dx|x|`: **defined as unsupported** → explicit "unsupported operation" error, not a silent nullptr collapse (§F.2, ticket CAS-07 acceptance).
- Oracle: O4 sampling both signs.

### D.14 Equations and inequalities

- [V] SymEquation (LHS/RHS SymPoly pair, `SymEquation.h`); OmniSolver/SingleSolver/SystemSolver per §B.6. Inequalities: only the Grapher's relational plotting (`OpKind::Lt/Gt/Le/Ge`, `MathAST.h:95-112`; region semantics in grapher spec §C) — **no symbolic inequality solving exists** [V: no solver consumes relational OpKinds].
- [P] Contract: §H. Inequality solving remains out of scope; the Grapher's numeric region semantics are the only inequality semantics, per the grapher spec.

### D.15 Complex numbers

- [V] Giac: `complex_mode(false)` — real mode (`GiacBridge.cpp:193-212`). cas::: quadratic Δ<0 reports `hasComplexRoots + complexReal + complexImagMag` (`SingleSolver.h:73-80`) — display metadata, not arithmetic. VPAM: `ConstKind::Imag` unhandled → error (`MathEvaluator.cpp:988`). `setting_complex_enabled` global exists (`Config.h:77-79`) but no engine reads it for arithmetic [V: grep — consumed only in settings UI].
- **[D-2] Complex-mode contract**: options: (a) declare NumOS real-only, display quadratic complex roots as `a ± bi` from the metadata, remove/repurpose `setting_complex_enabled`; (b) implement complex ExactVal/CASNumber arithmetic (large project); (c) route complex work to Giac with `complex_mode(true)` per-query. **Recommendation: (a) now** — it matches every implemented engine; (c) is a follow-up once the Giac boundary (integration spec §B) is hardened. Consequence of (a): corpus rows assert `√(−1)` = domain error in every engine and `x²+1=0` → "no real solutions" + complex display from metadata. Ticket CAS-12.

### D.16 Variables and assumptions

- [V] Variables: single-char; two disjoint stores (DV-7). No assumption system exists anywhere in cas::/VPAM [V: no assumption type in `src/math/cas/*`].
- [P] Contract: all symbolic variables are assumed **real and unconstrained**. Rewrites requiring positivity/nonzero-ness are forbidden unless operands are literal constants satisfying the condition (§E.4). An assumption system is out of scope; the word "assumes" in any future rule comment must cite this section.
- Store unification is P-04 (ground truth §M) — out of CAS-spec scope but corpus rows (VARS-*) pin current per-store behavior including the `approximate` persistence loss (`VariableManager.h:153-155`).

### D.17 Undefined values, division by zero, NaN/Inf

- [V] Per-engine: `exactDiv` zero-denominator → "Math ERROR" (`MathEvaluator.cpp:355-357`); `fromDouble` rejects NaN/Inf (`:109-111`); RPN div-0 and non-finite sanitization (`Evaluator.cpp:196-200,164-167,216-219,244-248`); graph NAN-skip (grapher spec §C.9); CASRational error sentinel den==0 (`CASRational.h:87-92`).
- [P] Contract: NaN/Inf must never appear in a displayed result or a stored variable. Division by literal zero = domain error. Division by a symbolic expression is allowed unevaluated; **no engine may simplify `x/x → 1`** (x=0) unless x is a literal nonzero constant (§E.4 row R-F2). `1/0`, `0/0`, `tan(π/2)`, `ln(0)`: all → domain error class (§J.3), each with its per-engine message pinned by corpus rows ERR-*.

### D.18 Branch cuts

- [P] In real mode (D-2 option a) there are no branch cuts to define for NumOS-native engines: even roots of negatives, logs of non-positives, and arcsin/arccos outside [−1,1] are domain errors ([V] `MathEvaluator.cpp:797,882,889,910,927`). Odd roots of negatives: `NodeRoot` degree-3 of −8 currently routes through `pow(−8, 1/3)` double → NaN → error [V: `MathEvaluator.cpp:787-793` general case uses `std::pow`, which returns NaN for negative base with fractional exponent]. **[D-3]**: define `ⁿ√x` for odd integer n and x<0 as the real root (`³√−8 = −2`) — options: (a) implement sign-aware odd-root handling; (b) keep error. **Recommendation (a)**: it is the school-math expectation for a root *glyph* (distinct from `x^(1/3)` power form, which may remain principal-value/error). Corpus rows RAD-ODD-* pin the choice; Giac differential oracle must normalize (Giac returns the principal complex value for `(−8)^(1/3)` but `surd(−8,3)=−2`).

### D.19 Angle mode

- [V] Four independent states (§A.3). VPAM applies DEG→rad on trig input and rad→DEG on arc output when `g_angleMode==DEG` (`MathEvaluator.cpp:849-896`) — the machinery works; nothing ever sets the mode.
- [P] Contract: **one authoritative mode**: `vpam::g_angleMode`, set by SettingsApp, displayed by StatusBar, and *synced* into every RPN `Evaluator` instance at evaluation entry (extends the grapher spec's GR-02 sync rule, `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md:228`). cas:: trig *numeric folds* are RAD-only by definition; ASTFlattener must convert DEG literals at the boundary or refuse numeric trig folds in DEG mode. Giac: mode is not forwarded — Giac console is defined as RAD-only and documented as such (integration spec §C.7).
- Exact table in DEG: `sin(30°)=1/2` exact — requires special-angle recognition on the *degree* grid, not conversion-then-float. Acceptance: corpus rows ANGLE-*.

### D.20 Exact vs approximate result selection and display

- [V] CalculationApp: Symbolic default; auto-switch to Periodic when rational with den ≥ 10^5 (`CalculationApp.cpp:532-549`); S⇔D cycles Symbolic⇄Periodic⇄Extended (`:485-489`); Extended = 200-digit expansion (`:714-724`); `approximate` results render scientific (`MathEvaluator.cpp:1058`).
- [P] Contract:
  1. Result carrying `approximate=true` (or Floating form) displays with a leading `≈` in every mode.
  2. Symbolic mode shows the canonical exact form (D.1-D.13); Periodic shows the repeating decimal with vinculum (`resultToPeriodicAST`); Extended shows ≥200 digits with the stated digit count.
  3. The den≥10^5 auto-Periodic heuristic is retained but MUST NOT change the stored exact value — S⇔D back to Symbolic recovers the exact fraction [V today: `_lastResult` retained, `CalculationApp.cpp:529-549`].
  4. Formatting round-trip: `format(parse(format(v))) == format(v)` for every displayable exact value (oracle O1 on the display layer; corpus rows DISP-*).
- Unacceptable: mode toggles that mutate the result; `≈`-less display of any D.4 value; Periodic display of an approximate value (period detection on a truncated fraction fabricates a false period).

---

## E. Simplification contract

Applies to `cas::SymSimplify` (primary), `ExactVal::simplify/simplifyRadical` (VPAM leaf-level), and `RuleEngine/AlgebraicRules` (tutor). Rule IDs are normative for tickets/corpus.

### E.1 Always-valid over ℚ (unconditional; must preserve `evaluate()` exactly)

| ID | Rewrite | Current | Evidence |
|---|---|---|---|
| R-U1 | flatten nested Add/Mul | [V] | `SymSimplify.cpp:358,502` |
| R-U2 | drop additive 0 / multiplicative 1 | [V] | `:372-386,521-537` |
| R-U3 | fold pure-constant subexpressions | [V] | `:372-386,521-537` |
| R-U4 | `−(−x)→x`, sign push into numeric factor, distribute `−` over Add | [V] | `:295-343` |
| R-U5 | like-term collection `2x+3x→5x` | [V] (pointer-identity terms only) | `:388-428` |
| R-U6 | power-base merge `x·x²→x³` | [V] | `:539-582` |
| R-U7 | `x^1→x`; `1^n→1`; `(−1)^int` fold | [V] | `:668-686` |
| R-U8 | `0·anything→0` | [V] | `:516` |
| R-U9 | integer/rational const^int fold, |exp|≤10 | [V] | `:707` |

### E.2 Valid only under assumptions (forbidden today per D.16)

| ID | Rewrite | Needs | Status |
|---|---|---|---|
| R-A1 | `x^0→1` | x≠0 | [V] fired unconditionally — retained for symbolic x per D-1, forbidden for literal 0 |
| R-A2 | `ln a + ln b → ln(ab)` | a>0 ∧ b>0 | [V] fired unconditionally (`:454-471`) — **must be restricted** to literal-positive operands (ticket CAS-06 scope) |
| R-A3 | `√(x²)→x` | x≥0 | not present; stays forbidden (write `|x|` if added) |
| R-A4 | `ln(e^x)→x` | always valid over ℝ (e^x>0) | [V] `:736-831` — OK |
| R-A5 | `e^(ln x)→x` | x>0 | [V] fired unconditionally — acceptable only because ln(x) already errors/undefined for x≤0; document as domain-preserving-by-composition |

### E.3 Forbidden without assumptions

`x/x→1`; `(x^a)^b→x^(ab)` for non-integer b with possibly-negative base; `√(ab)→√a·√b` for possibly-negative a,b; `ln(x^2)→2ln(x)`; `arcsin(sin x)→x`. None are currently implemented [V: absent from `SymSimplify.cpp`] — this section exists to keep them out.

### E.4 Domain-changing rewrites

| ID | Rewrite | Domain effect | Ruling |
|---|---|---|---|
| R-F1 | rational-function cancellation `(x²−1)/(x−1)→x+1` | adds x=1 | forbidden (leave unevaluated) until a domain-annotation mechanism exists |
| R-F2 | `x/x→1` | adds x=0 | forbidden |
| R-F3 | `(a^b)^c→a^(bc)` | wrong over ℝ for even-root cases | allowed only when b,c numeric AND (c integer OR a literal >0). Current guard requires numeric only (`SymSimplify.cpp:689-699`) — **must be tightened** (corpus row SIMP-POW-7 is the adversarial test: `((x)^2)^(1/2)` with x=−2) |
| R-F4 | `tan u → sin u/cos u` | none (same domain) | allowed [V `:793-799`], but see E.6 growth note |

### E.5 Expansion vs factoring policy

Simplify is **mildly expanding**: distribution fires only for 2-factor Mul with ≤8-term Add (`:616-649`). [P] Policy: `simplify` never factors; `expand` semantics = current distribution + SymPoly path; a future `factor` is a separate verb (D.9). The ≤8-term cap is normative (matches reliability H-4's AC-match cap of 8, `RELIABILITY…SPEC.md:227`).

### E.6 Ordering, sorting, normalization

- [V] Canonical child order: `symCanonicalLess` — non-Num before Num, then hash order (`SymExpr.h:499-504`). **Hash order is stable within a process but not human-predictable**; [P] display layer (SymExprToAST) owns human ordering (degree-descending for polynomials via SymPoly normalize, `SymPoly.cpp:186-232`).
- Coefficient normalization: CASRational invariants gcd=1, den>0 (`CASRational.h:22-24`). Sign normalization: sign lives in the numerator/coefficient, `Neg` nodes eliminated where a numeric factor exists (R-U4).
- Zero/one identities: R-U2/R-U7/R-U8. Power identities: R-U6/R-U7/R-U9/R-F3. Trig/log identities: D.11/D.12 + Pythagorean (`:430-452`).

### E.7 Idempotence requirement [P → NC-3]

`simplify(simplify(e))` must equal `simplify(e)` by pointer identity for every corpus input. Current code *assumes* this via the fixed-point loop but never verifies across the pass cap: an input that still changes at pass 10 returns a non-fixed-point [V: loop exits on cap, `SymSimplify.cpp:193-203`]. [P] Hitting `MAX_PASSES` without fixed point must be logged (`[CAS] simplify status=passcap`, diagnostics spec §2.6 format) and is a corpus failure unless the row is tagged `expect:passcap`.

### E.8 Termination and size growth [P]

- Termination: guaranteed today only by pass cap + pointer identity. [P] Add a node-count budget: a pass whose output exceeds `max(2×input_nodes, 256)` nodes aborts simplification and returns the input unchanged with status `growth-cap`. Rationale: tan→sin/cos plus distribution can grow trees; 1 MB arena (A.4-5) is the hard backstop and must never be the *first* line of defense.
- The docstring "weight can only stay equal or decrease" (`SymSimplify.h:43`) is rewritten by this contract: growth is allowed within the cap, monotonicity is not claimed.

---

## F. Differentiation contract

### F.1 Supported [V] (`SymDiff.cpp:33-290`)

Constants→0; var→1/0; `−u→−u'`; sum; n-ary product (Σ replace-one-factor); powers: `u^n` (n const) → `n·u^(n−1)·u'`, `a^v` → `a^v·ln a·v'`, `u^v` → `u^v·(v' ln u + v·u'/u)`; chain rule for sin, cos, tan, ln, exp, log10, arcsin, arccos, arctan.

### F.2 Unsupported [V]

`Abs` → nullptr (`SymDiff.cpp:281-283`); `Integral` nodes; display-only nodes (Paren is transparent; PlusMinus/Subscript/CoeffAssign are not differentiable). Implicit differentiation: **does not exist** (no entry point takes an equation). [P] Contract: unsupported constructs must surface as error J.2 ("unsupported operation") at the CalculusApp layer, with the offending sub-node named — never a blank result. Current CalculusApp behavior on nullptr must be audited during CAS-07 (evidence gap: `CalculusApp.cpp:835` result handling).

### F.3 Simplification of results [P]

`diff` output is passed through `simplify` (CalculusApp does this, `CalculusApp.cpp:835-848`). Contract: the *unsimplified* derivative must already be correct (oracle applies pre-simplify); simplify must not change its value (NC-4).

### F.4 General-power rule domain note

`u^v → u^v(v' ln u + v u'/u)` is only valid for u>0. [P] Ruling: acceptable as the implementation for the general case (standard CAS practice), but corpus adversarial row DIFF-ADV-3 documents behavior at u<0 with integer v (e.g. `d/dx (x^x)` sampled only at x>0; `d/dx((−2)^x)` expected unsupported/domain-error, not a NaN-bearing tree).

### F.5 No depth guard [V]

Recursion bounded by tree depth only. [P] Add depth cap aligned with reliability C4.3 remediation (H-4); depth > 64 → error J.8.

### F.6 Test oracle (ticket CAS-07)

Primary: **O5-style numeric check** — central difference `(f(x+h)−f(x−h))/2h`, h=1e-5 scaled by |x|, at ≥7 sample points inside the shared domain of f and f′, relative tolerance 1e-6 (absolute 1e-9 near 0), NaN-skipping points where f is undefined and requiring ≥4 valid points. Secondary: O3 structural equality against a pinned expected tree for the 17 SymDiffTest cases. Equivalence of alternate forms (e.g. `cos²−sin²` vs `cos 2x` — no double-angle rules exist [V], so not currently reachable) resolved by O4.

---

## G. Integration contract

### G.1 Supported now [V] (`SymIntegrate.cpp:92-536`)

Table: `c→cx`, `x→x²/2`, `x^n→x^(n+1)/(n+1)` (n≠−1), `x^(−1)→ln|x|` (`:120`), sin/cos/exp/tan(→−ln|cos|)/ln(→x ln x−x)/arcsin/arccos/arctan of the bare variable. Linearity: sum split, constant extraction (`:262-300,592-631`). U-substitution: `f(g)·g'` and `c·g^n·g'` shapes, depth ≤5 (`:311-466`). Parts: LIATE, depth ≤3 (`:476-536,643-675`). Failure → nullptr (`:81`).

### G.2 Planned/aspirational

Header's Strategy-5 unevaluated wrapper (`SymIntegrate.h:27,54`): [P] adopt it — on failure the *caller-visible* result is `SymFunc(Integral, f)` rendered as `∫f dx` ("unevaluated"), never a blank. Definite integrals: numeric only, via MathAnalysis Simpson (`MathAnalysis.h:113`) or the VPAM `DefIntegral` node path — symbolic definite integration is out of scope. Giac delegation: **[D-4]** — options: (a) keep native-only in apps, Giac stays serial-only; (b) CalculusApp falls back to Giac on nullptr (firmware only). Recommendation: (a) until the Giac boundary contract (integration spec §B) is implemented; then (b) behind the H-2 budget. Consequence of (b) without the boundary: unbounded-hang risk C4.1 lands in a UI app.

### G.3 Constant of integration [P]

Indefinite results display `+C` — implemented in the converter (`SymExprToAST::convertIntegral` appends "+C", `SymExprToAST.h:56-66`). Contract: `+C` is display-only; the engine tree never contains C; oracles strip it.

### G.4 Step logging

Integration steps ride CASStepLogger (§I). [P] Each strategy transition (table hit, u-sub choice with the chosen g, parts split u/dv) is one step; internal retries are not steps.

### G.5 Equivalence checking by differentiation [P → NC-6, ticket CAS-08]

For every corpus integral: compute `d = diff(integrate(f))`, then assert `simplify(d − f)` evaluates to 0 at ≥7 sampled points (O4; structural zero O3 preferred when reachable). Absolute-value forms: `ln|x|` differentiates piecewise — sample only x>0 and x<0 separately, both must pass. Timeout: each integrate call inherits the H-4 5 s deadline; corpus stress rows assert timeout status, not a hang.

---

## H. Equation solving contract

### H.1 Classes and routes [V]

Linear/quadratic → SingleSolver exact; degree ≥3 → Newton (numeric); classification+isolation → OmniSolver (depth 20); inverse forms `ln(x)=1 → x=e`; transcendental → HybridNewton (16 seeds); systems: exact 2×2/3×3 linear, resultant-based nonlinear 2×2/3×3 (§B.6).

### H.2 Result contract [P]

1. **Exactness declaration**: each solution is tagged exact (from closed form) or approximate (Newton). HybridNewton results are approximate by definition (they are rationalized doubles, `HybridNewton.cpp:97-98`) and must display `≈` (NC-1).
2. **Ordering**: real solutions sorted ascending by numeric value; the quadratic pair as `(−b−√Δ)/2a` then `(−b+√Δ)/2a`. Newton root sets: sorted ascending after dedup (`DEDUP_TOL=1e-8`, `HybridNewton.h:90`).
3. **Verification**: NC-7 — substitution check before returning; a "solution" failing `|f(root)| < VERIFY_TOL=1e-8` (`HybridNewton.h:89`) is dropped, and if all drop the result is "no solution found (numeric)", distinct from the proven "no real solution" of a negative quadratic discriminant.
4. **Domain filtering**: roots outside the domain of the original equation (e.g. `ln(x)=ln(2x−1)` candidate x making an argument ≤0) are rejected; log/exp/radical tutors already carry `SOLUTION_TOLERANCE=1e-6` checks (`TutorTemplates.cpp:40-42`) — corpus rows EQ-DOM-* pin this.
5. **Complex roots**: per D-2(a) — reported as "no real solutions" + `a ± bi` display from the metadata (`SingleSolver.h:73-80`). Never silently empty.
6. **No-solution / identity**: contradictions (`0=1`) → "no solution"; identities (`x=x`) → "infinite solutions"; both are existing CASTest cases (dormant) and become corpus rows EQ-EDGE-*.
7. **Exhaustiveness honesty**: Newton with 16 fixed seeds does not find all roots (e.g. roots outside [−100,100] grid or clustered). The UI wording must be "solutions found", not "the solutions", for approximate paths. Corpus adversarial row EQ-ADV-2: `(x−1000)(x−1001)=0` via transcendental route — document expected miss or seed-grid escalation.

### H.3 Graphing relationship

Solver results and graph x-intercepts must agree within graph tolerance: parity rows (CAS-11) plot `f(x)` and compare `assert`-read root positions vs solver output. The Grapher never calls cas:: [V: grapher spec `:40` keeps the split] — parity is test-level only.

### H.4 Giac delegation

None today [V]. Same decision gate as D-4/G.2: only after the boundary contract exists.

---

## I. Step-by-step contract

### I.1 Definitions

A **step** is one semantically meaningful transformation of the working expression/equation: one rule application (RuleEngine `applyOneStep`, `RuleEngine.h:361`), one simplify sub-rewrite chosen for pedagogy (CalculationApp edu-steps via `simplifyPass`, `CalculationApp.cpp:935`), or one solver phase (isolate, divide, substitute — PedagogicalLogger `SolveAction`, `PedagogicalLogger.h:58-120`).

### I.2 Correctness requirement [P]

**Every step's "after" state must be mathematically implied by its "before" state** under the rewrite's validity conditions (§E tables). Oracle: for variable-free steps, `evaluate(before) == evaluate(after)`; for equations, solution-set preservation checked by substituting the final solutions into every intermediate equation (each must hold within 1e-9). A step that fails this is a **fake step** — the highest-severity tutor bug class.

### I.3 Granularity

Acceptable: one rule per step; combining commutative reorder with the rule that needed it. Forbidden: steps that skip a sign change; "…and simplifying we get" jumps that hide >1 rule application unless tagged as a summary step; duplicate consecutive steps (dedup exists but only vs the immediately-previous hash, `CASStepLogger.cpp:87-94` — [P] extend dedup window to the full log for identical snapshots).

### I.4 Logging format

`CASStep{description, SymEquation snapshot, MethodId, StepKind, mathExpr*, highlightExpr*, reason}` (`CASStepLogger.h:83-105`). Contract: `mathExpr`/`highlightExpr` are arena-non-owning — **no step may be read after its arena resets** (`CasMemory.h:302-306` analogue; CalculationApp clears at `CalculationApp.cpp:114-122,575-576,758-759,914-915` [V]).

### I.5 UI + memory/time limits [P]

Steps rendered as per-step MathCanvas widgets (`CalculationApp.h:149-153`). Budgets (aligns with reliability F.2 "steps-cap" contract, `RELIABILITY…SPEC.md:301`): max 100 retained steps per solve; beyond that truncate with "… (more steps omitted)" and `[CAS] steps-cap shown=N total=M` (diagnostics spec §2.6). Step generation shares the H-4 5 s deadline. RuleEngine `maxSteps=100` [V `RuleEngine.h:372`] is the engine-side cap.

### I.6 Test strategy (ticket CAS-13)

Host harness replays corpus equations through the tutor stack, asserting per I.2: (a) every step value/solution-set-preserving; (b) no consecutive duplicates; (c) final state equals the non-tutor solver result; (d) step count ≤ cap; (e) Spanish/English phrasing snapshots (golden text, revisable) separate from correctness asserts.

---

## J. Error model

Closed set of error categories. "User msg" = calculator display; "log" = serial line per diagnostics-spec closed tag set (`NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md:34`). Recoverability: all errors leave the app usable (reliability F.3); none may reboot except uncontained internal bugs.

| # | Category | Trigger examples | Current behavior [V] | Contract [P]: user msg / log / recoverable / corpus row |
|---|---|---|---|---|
| J.1 | Syntax error | unparseable graph string; malformed serial | RPN: parse fail → slot invalid silently (grapher spec F.1); VPAM: structurally impossible | "Syntax error" per grapher GR-02 slot states / `[GRAPH] invalid slot=N reason=syntax` / yes / ERR-SYN-* |
| J.2 | Unsupported operation | `abs` in VPAM; `arctan` in RPN; `d/dx |x|` | "Función no soportada" (`Evaluator.cpp:159`); silent nullptr (SymDiff) | "Not supported: <op>" / `[CAS] unsupported op=<kind>` / yes / ERR-UNS-* |
| J.3 | Domain error | `ln(−1)`, `arcsin(2)`, `tan(π/2)`, `√(−4)` | "Math ERROR" / "Error: Dominio" (§D.17) | "Domain error" (unify text per engine table) / `[CAS] domain fn=<kind>` / yes / ERR-DOM-* |
| J.4 | Division by zero | `1/0`, `0^(−1)` | "Math ERROR" (`MathEvaluator.cpp:355-357`); "Error: Div por 0" (`Evaluator.cpp:196-200`) | "Division by zero" / same tags / yes / ERR-DIV-* (CI already gates `calc_error_div_by_zero.numos`) |
| J.5 | Overflow | int64 exceeded | VPAM → approximate flag (silent, NC-1 violation); cas:: native → error; cas:: fw → bignum (aliasing bug) | `≈` display or "Overflow" per D.1 / `[CAS] overflow` / yes / ERR-OVF-* |
| J.6 | Timeout | Giac hard input; CAS deadline | **none — hangs** (C4.1) | "Timeout" / `[GIAC]…status=timeout` `[CAS]…status=deadline` (RT-09 formats) / yes, engine context stays usable (verify on hw per reliability F.3) / ERR-TMO-* |
| J.7 | Memory exhausted | arena/pool/ConsTable full; Giac bad_alloc | mixed: nullptr / heap-spill / throw / caught string (§B, A.4-5) | "Out of memory" with op aborted wholesale / `[CAS] solve status=oom`, `[MEM]` probes / yes; arena reset on next op / ERR-OOM-* |
| J.8 | Recursion depth | deep AST into diff/flatten/convert | unguarded (C4.3) except OmniSolver 20, Giac 100 | "Expression too complex" / `[CAS] depth-cap` / yes / ERR-REC-* |
| J.9 | Invalid assumption | rewrite requiring unavailable assumption | n/a (no assumptions, D.16) | must be unreachable; any occurrence = J.11 / — / — / static review |
| J.10 | Giac unavailable | `:cmd` on emulator/native | `"Error: Giac available only on Arduino target"` (`src/math/GiacBridge.h:38`) | keep exact string (it is a de-facto API) / `[GIAC] unavailable` / yes / GIAC-ENV-* |
| J.11 | Internal bug | null child in cons'd tree; aliasing wrong answer | silent corruption / wrong display | never silent: assert in debug builds, "Internal error" + `[CAS] internal` in release; panic-record path per reliability spec §C1 / partially / ADV-* rows are designed to hunt these |

Every category gets ≥3 corpus rows including one adversarial variant (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` §E.15/§H).

---

*End of Document 1. Boundary details: `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md`. Oracles/corpus: `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md`. Roadmap: `NUMOS_CAS_IMPLEMENTATION_TICKETS.md`.*
