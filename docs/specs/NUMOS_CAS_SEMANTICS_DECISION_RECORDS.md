# NumOS — CAS Semantics Decision Records (D-1, D-2, D-3)

> ADR-style records for the three open semantics decisions from `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md` (§D.7, §D.15, §D.18).
> Date: 2026-07-02 · Branch: `claude/cas-01-02-execution-semantics-i3ngue` (on `main` @ `5a03ac2`).
> All "current behavior" claims below were re-verified against source during this audit; **two corrections to Document 1 were found** (D-2 and D-3 contexts) and are marked ⚠ CORRECTION.
> Ratification is a Human decision (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:193`); these records supply the evidence and a recommendation. No web sources were consulted; calculator-precedent claims cite repo documents only.

---

## D-1 — Semantics of `0^0`

**Status**: proposed (recommendation: Option B — literal `0^0` is a domain error in every engine; symbolic `x^0 → 1` retained).

### Context

`0^0` is convention-dependent (combinatorial convention: 1; analysis convention: undefined). NumOS today gives **different answers in different engines for the same keystrokes' worth of math**, which violates NC-10 (declared engine parity, `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md` §A.5) and is divergence-register row DV-6 (§C.8).

### Current behavior per engine [all verified this audit]

| Engine | `0^0` result | Evidence |
|---|---|---|
| VPAM (`exactPow`) | `"Math ERROR"` | `src/math/MathEvaluator.cpp:405-407` — `if (e == 0) { if (base.isZero()) return ExactVal::makeError("Math ERROR"); // 0^0` |
| VPAM `0^(−n)` (adjacent case) | `"Math ERROR"` | `src/math/MathEvaluator.cpp:411-414` |
| cas:: symbolic simplify | **`1`** — the `x^0 → 1` rule fires before any base check | `src/math/cas/SymSimplify.cpp:663-664` (`if (isZero(p->exponent)) return symInt(arena, 1);` is the *first* rule in `simplifyPow`) |
| cas:: numeric evaluation | **`1`** — `SymPow::evaluate` delegates to `std::pow(0.0, 0.0)`, which returns 1 | `src/math/cas/SymExpr.cpp:252-256` |
| Legacy RPN | **`1`** — the `Pow` case guards only `a==0 && b<0` (div-by-zero error), then calls `pow(0,0)` = 1 | `src/math/Evaluator.cpp:202-209` |
| Giac (firmware serial console) | expected `1` (Giac's convention) — **unverified in-repo**; must be pinned by a recorded hardware golden (CAS-15), not assumed | `lib/giac` vendored source; no test exists (`NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md` §F) |

So today three engine paths say `1` and the flagship Calculation app says error.

### Options

- **(A) `0^0 = 1` everywhere** (combinatorial convention). Requires changing VPAM `exactPow` (`MathEvaluator.cpp:405-407`) — the *only* CI-gated engine — and accepts that `0^0=1` while `0^x` at `x→0⁺` is 0-for-x>0, which users of a "Math ERROR"-style calculator do not expect.
- **(B) literal `0^0` = domain error everywhere; symbolic `x^0 → 1` retained** (analysis convention at the value level, algebraic convention at the rewrite level). VPAM already complies. cas:: and RPN change.
- **(C) keep the divergence, document it.** Rejected: it is exactly the NC-10 violation this spec set exists to eliminate, and it makes edu-steps (cas::) contradict the result line (VPAM) inside the *same* CalculationApp screen (`CalculationApp.cpp:529` result path vs `:919-964` edu-steps path).

### Tradeoffs / precedent

- Calculator precedent available in-repo: Document 1's D-1 note records that Casio/TI-class calculators report an error for `0^0` (`NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md:330`). NumOS is styled after that class of device (S⇔D, Math ERROR strings).
- The subtlety in (B): for **symbolic** `x`, `x^0 → 1` must still fire or the simplifier loses most of its utility (polynomial normalization depends on it). This makes the rule assumption-dependent (`x ≠ 0` presumed), which is consistent with the already-ratified variable contract: "all symbolic variables are assumed real and unconstrained" but rewrites are judged per §E tables — `x^0→1` is explicitly retained as R-A1 with the literal-zero carve-out (`NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md:433`).

### Recommended decision

**Option B.**

- **User-visible behavior**: typing `0^0` (any app, any engine) → domain-error class J.3/J.4 message for that engine (`"Math ERROR"` in Calculation today; text unification is CAS-10/J-table scope, not D-1 scope). `x^0` with symbolic `x` displays `1`. `0^x` stays: `0` for literal positive `x`, error for literal negative `x` (`MathEvaluator.cpp:411-414`; `SymSimplify.cpp:670-674`), unevaluated for symbolic `x`.
- **Internal behavior**: 
  - `cas::SymSimplify::simplifyPow` (`src/math/cas/SymSimplify.cpp:663-664`): insert a literal-base-zero check **before** the exponent-zero rule: if `isZero(p->base) && isZero(p->exponent)` → return an error-carrying form (the `CASNumber::Error`-style path per §J; exact mechanism chosen in the fix ticket — `simplifyPow` currently has no error channel, returning `p` unchanged is the minimal-viable fallback that at least stops the wrong `1`).
  - `cas::SymPow::evaluate` (`src/math/cas/SymExpr.cpp:252-256`): return `NaN` for `b==0 && e==0` so numeric oracles (O4) NaN-skip instead of silently sampling 1.
  - RPN `Evaluator` `Pow` case (`src/math/Evaluator.cpp:202-209`): extend the guard to `a == 0.0 && b == 0.0` → `"Error: Dominio"`.
  - VPAM: no change (`MathEvaluator.cpp:405-407` already correct).
  - Giac: **no change** — console is a power-user surface; its `0^0` result is recorded verbatim as a hardware golden and documented as Giac-convention (divergence *declared*, satisfying NC-10's "declared, not accidental").
- **Error behavior**: J.3 domain-error class; recoverable; per-engine message pinned by corpus rows until CAS-10 unifies text.
- **Tests unlocked**: corpus rows `POW-002` (VPAM, passes already), `POW-003` (cas::, XFAIL→PASS on fix), `POW-004` (RPN, XFAIL→PASS on fix); a Giac golden row `GIAC-POW-1` (record-then-freeze).
- **Risks**: low. The cas:: reorder touches the first rule of `simplifyPow` — property tests (CAS-06 idempotence/soundness suite) must run over the change; the "return unchanged" fallback can leave `0^0` unevaluated in edu-steps, which is acceptable (unevaluated ≠ wrong).
- **Migration**: no stored-data impact (no persisted results). One-PR change per engine site; each site independently revertible.
- **Tickets affected**: CAS-06 (simplify rule fixes — absorb the `simplifyPow` reorder there), CAS-02 (rows), CAS-15 (Giac golden). Corpus rows blocked until ratified: `POW-003`, `POW-004` gate as XFAIL either way, so **nothing hard-blocks on D-1** — the rows just can't flip to gating-PASS expectations until the human signs off.

---

## D-2 — Real-only vs complex-number policy

**Status**: proposed (recommendation: Option A — ratify NumOS as real-arithmetic-only, with the *already-implemented* complex-conjugate-root display as the sole complex surface).

### Context

`Config.h` exposes `setting_complex_enabled` (`src/Config.h:77`, comment: "true = show complex roots, false = 'No real solutions'"). Document 1 §D.15 assumed this toggle was consumed "only in settings UI". ⚠ **CORRECTION**: it is live engine-adjacent logic — the solver reads it.

### Current behavior [verified this audit]

| Surface | Behavior | Evidence |
|---|---|---|
| Quadratic solve, Δ<0, toggle **on** (default) | Exact complex parts computed with CASNumber arithmetic: `Re = −b/2a`, `|Im| = √|Δ|/2a`; result carries `hasComplexRoots/complexReal/complexImagMag`; steps log "Complex conjugate roots" | `src/math/cas/SingleSolver.cpp:316-360` (computation), `SingleSolver.h:73-80` (fields); defaults `src/main.cpp:31`, `src/hal/NativeHal.cpp:125` |
| Quadratic solve, Δ<0, toggle **off** | `result.error = "No real solutions"`, solve reports not-solved | `src/math/cas/SingleSolver.cpp:330-336` |
| EquationsApp display | "Complex roots:" title + `a ± bi` rendered from the metadata via `SymToAST::fromExactVal`; step list renders "Complex conjugate roots:" | `src/apps/EquationsApp.cpp:2374-2390,2775-2783` |
| Settings UI | toggle exists and flips the flag | `src/apps/SettingsApp.cpp:213,244` |
| VPAM arithmetic | no complex: `ConstKind::Imag` exists in the AST (`src/math/MathAST.h:196-200`) but `evalConst` has no case for it → `"Math ERROR"` | `src/math/MathEvaluator.cpp:984-988` |
| RPN arithmetic | doubles only, no complex | `src/math/Evaluator.cpp` (whole file; `Tokenizer.h:60`) |
| cas:: arithmetic | none beyond the quadratic metadata above; `√(−n)` errors (`CASNumber` radical path is nonnegative-only, `CASNumber.h:159-165`) | — |
| Giac | configured real: `complex_mode(false)`, `complex_variables(false)`; **but** `i_sqrt_minus1(1)` is also set, so the console's treatment of a literal `i` token is not obviously inert — must be pinned on hardware | `src/math/giac/GiacBridge.cpp:198-201` |

So D-2 option (a) from Document 1 ("display quadratic complex roots from metadata") is **not a proposal — it is the shipped behavior**. What is missing is: ratification, tests, coverage for the *OmniSolver* route (only `EquationsApp.cpp:2374` handles Omni results; parity for SingleSolver-direct paths must be asserted), and a decision on the toggle's future.

### Options

- **(A) Real-only, formalized.** Arithmetic in every engine is real; `√(−1)`, `ln(−1)`, even roots of negatives are J.3 domain errors; the *only* complex output is the quadratic (and, if OmniSolver classifies there, quadratic-shaped) conjugate-pair display, controlled by `setting_complex_enabled`, whose meaning is ratified as "show complex roots (display-only)".
- **(B) Implement complex arithmetic** (ExactVal/CASNumber gain an imaginary component). Large multi-quarter project touching every evaluator, renderer, and persistence format; nothing in the app roadmap needs it.
- **(C) Route complex queries to Giac with per-query `complex_mode(true)`.** Firmware-only, requires the hardened boundary (Doc 2 §B, CAS-05/CAS-14) that does not exist yet, and leaves emulator/native semantics divergent.

### Recommended decision

**Option A now**; revisit (C) only after CAS-05 + CAS-14 + CAS-15 exist. This matches Document 1's recommendation (`NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md:376`) and — decisive point — is already what the code does; ratifying it costs zero engine changes.

- **User-visible behavior** (exact): `√(−4)` → domain error in Calculation. `x² + 2x + 5 = 0` in Equations → toggle on: title "Complex roots:", value `−1 ± 2i`; toggle off: "No real solutions". The settings item is relabeled in meaning (docs/spec only): "Show complex roots of quadratics".
- **Internal behavior**: no engine gains complex arithmetic; `SingleSolver.cpp:316-360` stays the only producer; `hasComplexRoots` metadata is the only channel; Giac stays `complex_mode(false)` (`GiacBridge.cpp:198-199`).
- **Error behavior**: `√(−1)` and friends: J.3 per engine (VPAM `MathEvaluator.cpp:219-222` radical guard; RPN `Evaluator.cpp:138-143` "Error: Dominio"). With toggle off, "No real solutions" is a *result-classification message*, not an error dialog — pinned as-is (`SingleSolver.cpp:333`).
- **Tests unlocked**: corpus `CPLX-001..003` (this pack), later E.13 rows (`x²+1=0` Giac real-mode empty list; `setting_complex_enabled` semantics row); a hardware adversarial row for the `i_sqrt_minus1(1)` question (`GIAC-ADV` class, Doc 2 §C.4).
- **Risks**: the Omni route returns complex metadata only when its internal quadratic path does (`OmniSolver.h:98-102`); non-quadratic equations with complex roots (e.g. `x⁴+1=0`) will report "no solutions found (numeric)" — H.2.7 honesty wording applies; a corpus row should pin that this is a stated limitation, not a bug.
- **Migration**: none (behavior unchanged). Documentation delta: PROJECT_BIBLE via `NUMOS_PROJECT_BIBLE_DELTA.md` process; Document 1 §D.15 evidence correction (this record supersedes its "settings-UI-only" claim).
- **Tickets affected**: CAS-12 **shrinks** from "implement complex display" to "ratify + test + document + Omni-path parity" (Sonnet-sized); CAS-02 rows CPLX-*; CAS-15 Giac rows.

Blocked corpus rows if not ratified: `CPLX-002`, `CPLX-003`, `EQ-003` stay `blocked-d2` (tagged, non-gating).

---

## D-3 — Odd roots / principal roots of negative numbers

**Status**: proposed (recommendation: Option A′ — ratify the root-glyph/power-form split that the code already implements, then fix its NC-1 marking gap).

### Context

For negative `x`: the real n-th root exists for odd `n` (`³√−8 = −2`), while the principal value of `x^(1/n)` is complex. School calculators typically make the root *glyph* return the real root and the *power form* follow principal-value/error semantics. Document 1 §D.18 claimed NumOS errors on `³√−8`. ⚠ **CORRECTION**: it does not.

### Current behavior [verified this audit]

| Path | Input | Result | Evidence |
|---|---|---|---|
| VPAM `NodeRoot`, degree 2 | `√x`, x<0 | domain error (radical simplify rejects negative inner) | `src/math/MathEvaluator.cpp:782-784` (routes to `exactSqrt`), `:219-222` (`simplifyRadical` inner<0 → "Math ERROR") |
| VPAM `NodeRoot`, odd integer degree, negative radicand | `³√(−8)` | **real root, sign-aware**: `−std::pow(−base, 1.0/n)` → `ExactVal::fromDouble` → `−2` (integer-detected, displays exact) | `src/math/MathEvaluator.cpp:788-796` (odd-root branch is explicit: "Raíces impares de negativos") |
| — same, non-perfect radicand | `³√(−2)` | real root as an **unmarked** ≤10-digit fraction (`fromDouble` leaves `approximate=false`) | `src/math/MathEvaluator.cpp:794` → `:107-149`; NC-1 violation class (Doc 1 §A.4-2) |
| VPAM `NodeRoot`, even degree, negative radicand | `⁴√(−16)` | `"Math ERROR"` | `src/math/MathEvaluator.cpp:797` (falls through when `ni` even) |
| VPAM power form | `(−8)^(1/3)` | `"Math ERROR"` — non-integer exponent path calls `std::pow(−8, 0.333…)` → NaN → error | `src/math/MathEvaluator.cpp:393-399` |
| cas:: numeric | `Pow(−8, 1/3)` | NaN from `std::pow` (propagates to callers/oracles as NaN) | `src/math/cas/SymExpr.cpp:252-256` |
| cas:: symbolic | `Pow(−8, 1/3)` | stays unevaluated (const-fold requires integer exponents) | `src/math/cas/SymSimplify.cpp:702-716` |
| RPN | `sqrt(−1)` | `"Error: Dominio"` | `src/math/Evaluator.cpp:138-143` |
| RPN | `(−8)^(1/3)` | NaN → `"Error: Resultado no finito"` | `src/math/Evaluator.cpp:208-219` |
| Giac | `(−8)^(1/3)` vs `surd(−8,3)` | expected principal-complex vs `−2` per Giac semantics — **unverified in-repo**, hardware row required; the planned AST→Giac serializer maps Root degree n to `surd(x,n)` precisely to preserve this split | `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md:119` (C.2 serializer rule); adversarial row Doc 3 §H.6 |

So the glyph/power split **already exists in VPAM**; it is undocumented, untested, inconsistent across engines (cas:: numeric yields NaN where VPAM yields −2), and its non-integer results violate NC-1 (unmarked approximation).

### Options

- **(A) Root glyph = real odd root; power form = error/principal.** (Document 1's recommendation.) VPAM already implements the value semantics; remaining work is honesty (approx marker), exactness for perfect powers, and cross-engine consistency.
- **(B) Everything errors for negative radicands.** Regresses shipped, mathematically-correct behavior (`³√−8 = −2` is the school-math answer) and would need a deliberate code *removal* (`MathEvaluator.cpp:788-796`).
- **(C) Root glyph and power form both return the real root for odd n.** Makes `(x^(1/3))` diverge from `((x)^(1/3))`-via-Giac and from every principal-value CAS; breaks the R-F3 exponent-algebra guard rationale (`(x²)^(1/2)` ≠ x precisely because power forms are principal/real-domain, §E.4).

### Recommended decision

**Option A′** = Option A, ratified as follows:

- **User-visible behavior** (exact):
  - `ⁿ√x` glyph, odd integer n ≥ 3, x < 0: real root. Perfect odd powers display exact (`³√(−8)` → `−2`, `⁵√(−32)` → `−2`); otherwise a decimal **with the `≈` marker** (once CAS-10 lands; until then current unmarked display is pinned as the known-bug row `RAD-008`).
  - `ⁿ√x` glyph, even n, x < 0: domain error (unchanged).
  - `x^(1/n)` typed as a power: domain error for negative literal base with non-integer exponent (unchanged, every native engine). The glyph and the power form are **defined as different operators**; this sentence is the normative statement.
  - Root glyph with non-integer degree and negative radicand: domain error (unchanged, `MathEvaluator.cpp:792` — `ni == 0` guard).
- **Internal behavior**:
  - VPAM: keep `MathEvaluator.cpp:788-796`; upgrade in CAS-10-adjacent work: (1) perfect-power detection for odd degrees (integer-cube-root check) so `³√(−8)` is exact *by construction*, not by `fromDouble` luck; (2) set `approximate=true` on the non-perfect path (NC-1).
  - cas::: no new capability required now; cas:: has no Root node (roots arrive as `Pow(x, 1/n)`, which stays unevaluated symbolically — acceptable). `SymPow::evaluate`'s NaN for negative-base-fractional-exponent is *correct* under the power-form-is-principal ruling; document it at `SymExpr.cpp:252-256` (comment-only, CAS-00 class).
  - ASTFlattener: must NOT rewrite `NodeRoot(n, x)` into `Pow(x, 1/n)` for odd n without preserving the sign semantics — audit item for the fix ticket (flattener behavior for NodeRoot is an evidence gap; the corpus row `RAD-006` in the cas:: projection will surface it).
  - Giac serializer (future, CAS-04/CAS-14 dependency): Root degree n → `surd(x,n)`, never `x^(1/n)` (`NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md:119` — already specified; this record ratifies the reason).
- **Error behavior**: even-root-of-negative and power-form cases → J.3 domain error, per-engine text as today; corpus pins each.
- **Tests unlocked**: `RAD-004` (√(−4) error), `RAD-006` (³√(−8) → −2), `RAD-007` ((−8)^(1/3) → error), `RAD-008` (³√(−2) approx-marked — XFAIL until NC-1 fix); Giac hardware differential `surd` vs `^` row (Doc 2 §F.3).
- **Risks**: low for ratification (no behavior change). The perfect-power exactness upgrade touches `evalRoot` — golden-surface adjacent (result rendering), needs the CAS-10 review discipline. The `fromDouble` path for large odd roots can mis-round (`³√` of a 15+-digit perfect cube may miss integer detection at `MathEvaluator.cpp:126` `1e15` cliff) — corpus stress row candidate for the ≥300 corpus, out of the first 100.
- **Migration**: none.
- **Tickets affected**: CAS-02 (rows), CAS-10 (≈-marker on the non-perfect path), CAS-03 (canonical form for `k·ⁿ√m`), CAS-15 (Giac `surd` goldens). New small ticket recommended: **CAS-10b "odd-root exactness + marker"** if CAS-10 scope is kept display-only.

Blocked corpus rows if not ratified: `RAD-006`, `RAD-007`, `RAD-008` stay `blocked-d3` (tagged, non-gating).

---

## Summary table

| Decision | Recommendation | Engine changes required | Corpus rows blocked until human ratification |
|---|---|---|---|
| D-1 `0^0` | error for literal `0^0` everywhere; `x^0→1` symbolic retained | `SymSimplify.cpp:663-664` reorder; `SymExpr.cpp:252-256` NaN; `Evaluator.cpp:202-209` guard | POW-002/003/004 |
| D-2 complex | real-only ratified; existing metadata display is the complex surface | **none** (tests + docs only) | CPLX-002/003, EQ-003 |
| D-3 odd roots | glyph = real odd root (as shipped); power form = principal/error; NC-1 marker fix later | none for ratification; `MathEvaluator.cpp:788-796` exactness+marker upgrade later | RAD-006/007/008 |

*End of decision records.*
