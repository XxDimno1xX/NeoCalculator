# NumOS — CAS Implementation Tickets

> Ordered roadmap implementing the CAS spec set. References: **Doc 1** = `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md`, **Doc 2** = `NUMOS_CAS_GIAC_INTEGRATION_AND_BOUNDARY_SPEC.md`, **Doc 3** = `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md`. Reliability/diagnostics tickets referenced: H-2/H-4/M-2 (`NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md:217-227`), RT-09 (`NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md`), MT-04/MT-05 (`NUMOS_MEMORY_IMPLEMENTATION_TICKETS.md`).
> Audit date: 2026-07-02 · Branch: `claude/numos-cas-spec-frmkeg` @ `a0794b0`.
>
> Ordering rationale: tests-first (CAS-00..CAS-02 create the harness), then oracles (CAS-03..CAS-09), then user-visible contracts (CAS-10..CAS-13), then hardening (CAS-14..CAS-15). No engine-behavior change ships before the harness that would catch its regressions.
>
> **Model owner key** — Sonnet: mechanical, well-specified, low blast radius. Opus: multi-file design work, medium risk. Fable: cross-cutting contracts, fragile files, anything touching `lib/giac` or arena lifetimes. Human: hardware-in-the-loop, golden blessing, D-n decision ratification.

---

## CAS-00 — Inventory cleanup / remove misleading claims

- **Motivation**: dead code and stale banners actively mislead future sessions (Doc 1 §A.2); the audit itself was slowed by them.
- **Source evidence**: `RuleBasedTutor.cpp:141` (zero callers); `SymIntegrate.h:27,54` (unimplemented Strategy-5 claim); `SymSimplify.cpp:26` (stale `MAX_PASSES=8` banner vs `SymSimplify.h:60` =10); `ConsTable.h:26` ("Robin Hood" vs linear probing `ConsTable.cpp:150-159`); `SymSimplify.h:43` (unenforced weight claim); `tests/TokenizerTest_temp.cpp:20-23` (self-described throwaway); `SequencesApp.cpp:326-370` (silent term-dropping evaluator — document, don't fix here).
- **Files**: the above + `docs/specs/NUMOS_FABLE_CONTEXT_HEADER.md` (add CAS-spec pointer).
- **Design**: comment/banner corrections only; delete `RuleBasedTutor.{h,cpp}` and `TokenizerTest_temp.cpp` after porting its one case into the Doc 3 corpus (PARSE row); no behavior change anywhere.
- **Invariants**: firmware + emulator builds stay green; zero object-code diff outside deleted TUs.
- **Acceptance tests**: `pio run -e emulator_pc` and firmware CI green; `grep -r "RuleBasedTutor" src/` empty.
- **CI**: existing suites only. **Firmware tests**: none. **Budget**: n/a. 
- **Rollback**: revert commit. **Risk**: Low. **Owner**: Sonnet.

## CAS-01 — Host test harness for the dormant suites (= ground-truth P-01)

- **Motivation**: 6.6k lines of CAS assertions never execute (Doc 3 §B); every later ticket needs this safety net.
- **Source evidence**: `main.cpp:56-64,105-113` (flag-gated includes); `platformio.ini:102-103` (tests/ never compiled); cas:: compiles natively (whitelist proof `platformio.ini:251-258`); host-test template `tests/host/keycode_digit_test.cpp:14` + CI step `emulator-build.yml:103-104`.
- **Files**: new `tests/host/cas_suite_main.cpp` (driver `main()` calling the eight `cas::run*Tests()`, exit code = failure count); new `scripts/build-cas-host-tests.sh` (g++ compile of `tests/*.cpp` + `src/math/cas/*.cpp` + `src/math/MathAST.cpp` + `src/math/MathEvaluator.cpp` deps); `.github/workflows/emulator-build.yml` (new step after `:104`).
- **Design**: replace `Serial`-style output with the suites' existing printf paths (they already print `[PASS]/[FAIL]`, `CASTest.cpp:55`); driver greps its own counters. BigIntTest promotion cases compile out on host (`CAS_HAS_BIGINT=0`, `CASInt.h:53-59`) — driver must report them SKIPPED, not silently absent (Doc 3 §B risk). Build with `-fsanitize=address,undefined` in a second CI variant.
- **Invariants**: no edits to suite assertions in this ticket (first green run may reveal stale expectations → each becomes a `known-bug` corpus row or a reviewed assertion fix in a follow-up commit, never silently adjusted).
- **Acceptance tests**: all eight suites compile, run, exit 0 on host (or documented XFAIL list); ASAN variant clean.
- **CI**: gating step in `emulator-build.yml` fast lane. **Firmware**: none. **Budget**: <60 s CI runtime.
- **Rollback**: remove the CI step. **Risk**: Medium (first execution of 6.6k lines of assertions — expect archaeology). **Owner**: Sonnet (harness) + Opus (triage of first-run failures).

## CAS-02 — Corpus schema + first 100 rows

- **Motivation**: Doc 3 §D/§E is the single source of truth for expected behavior; oracles are useless without rows.
- **Source evidence**: Doc 3 §D schema; live `.numos` asserts (`NativeHal.cpp:1543-1611`) as the emulator projection target.
- **Files**: new `tests/corpus/cas_corpus.jsonl`; new `tests/host/corpus_runner.cpp` (loads rows with `input_form: symexpr|string`, executes O1/O2/O4/O8); new `scripts/corpus-to-numos.py` (projects `input_form: keys` rows to `.numos` scripts under `tests/emulator/scripts/corpus/`); workflow step.
- **Design**: first 100 rows = E.1, E.2, E.5, E.6, E.7, E.8 subsets (pure-arithmetic classes with no D-n dependency) + E.17 known-bug pins. Rows blocked on decisions (D-1, D-2, D-3) enter with `tags:["blocked-decision"]` and no gating.
- **Invariants**: schema validated by the runner at load (unknown field = error); ids immutable once merged.
- **Acceptance**: runner exits 0 with ≥100 rows executed, XFAIL report listing exactly the E.17 pins; projected `.numos` rows pass in the emulator locally.
- **CI**: fast lane, gating (XFAIL excepted). **Firmware**: none. **Budget**: <30 s.
- **Rollback**: step removal. **Risk**: Low. **Owner**: Sonnet (runner) + Fable (row review — rows ARE the contract).

## CAS-03 — Canonical AST/string normalizer

- **Motivation**: O1/O3/O6 need one shared normal form; today formatting is scattered (resultToAST `MathEvaluator.cpp:1053-1058`; SymExprToAST; Giac display map `GiacBridge.cpp:79`).
- **Source evidence**: Doc 1 §D.20, §E.6; Doc 2 §C.3.
- **Files**: new `src/math/cas/CanonicalForm.{h,cpp}` (SymExpr → canonical text: sorted terms per `symCanonicalLess` then display-order pass, normalized rationals/signs per D.2, radical form per D.5); host runner adoption; NO changes to app display paths in this ticket.
- **Design**: pure function, no allocation outside caller arena; also provides `parseRestricted(text) → SymExpr` for the Doc 2 §C.3 result grammar (shared with CAS-04/CAS-14).
- **Invariants**: `canonical(parseRestricted(canonical(e))) == canonical(e)` (round-trip property, O7); zero behavior change to displayed results.
- **Acceptance**: property test over 10k generated expressions (fixed seed); all corpus O1 rows re-expressed via the normalizer produce identical verdicts.
- **CI**: fast lane. **Budget**: <5 s for 10k round-trips host-side; ≤64 KB arena per expression.
- **Rollback**: unused module — delete. **Risk**: Low-Medium. **Owner**: Opus.

## CAS-04 — Differential Giac oracle (host advisory)

- **Motivation**: only oracle strong enough for BigInt and hard algebra (Doc 3 §C O6); catches KhiCAS drift.
- **Source evidence**: Doc 2 §F.7 [D-7 recommendation (a)]; vendored version pin `lib/giac/library.json` (1.9.0).
- **Files**: new `scripts/giac-differential.py` (drives distro `giac` binary over corpus projections, normalizes via a Python port of the restricted grammar, emits report); nightly workflow job (report-only).
- **Design**: input set = E.4/E.5/E.6/E.9/E.10/E.11 rows with `engine: giac` projections; ignore-list file for known formatting skew, every entry commented with an example.
- **Invariants**: advisory — never gates; never runs on PR lane.
- **Acceptance**: nightly report artifact produced; seeded with ≥50 rows; at least one deliberately-wrong test row proves the comparator detects mismatches (comparator self-test).
- **CI**: nightly extended lane. **Firmware**: none (hardware differential is CAS-15). **Budget**: ≤5 min.
- **Rollback**: job removal. **Risk**: Low (advisory). **Owner**: Sonnet.

## CAS-05 — Timeout/memory guard for Giac (implements reliability H-2 + M-2 for the bridge)

- **Motivation**: `:factor(hard)` freezes the device forever (C4.1, `RELIABILITY…SPEC.md:123`); NC-8.
- **Source evidence**: `GiacBridge.cpp:390-448` (single entry, single catch); `SerialBridge.cpp:155` (caller); no `ctrl_c` setter anywhere [V]; H-2 text (`RELIABILITY…SPEC.md:225`); probes `GiacBridge.cpp:394,439,442,445`.
- **Files**: `src/math/giac/GiacBridge.cpp` (input-length check ≤2048 before `gen()`; PSRAM largest-block ≥512 KB preflight; `esp_timer` arming `giac::ctrl_c` at 10 s; output cap 8192 per Doc 2 §B.4; `[GIAC]` RT-09 log line); no changes inside `lib/giac/**`.
- **Design**: per Doc 2 §B.2-B.6 verbatim. Fragile-file rule applies (`GiacBridge.cpp` is on the do-not-touch-casually list, ground truth §N) — bridge remains the only exit; catch-all preserved.
- **Invariants**: `:1+1` behavior unchanged; every abnormal path returns a `Error: …` string, never throws past the bridge; ctrl_c cleared on every exit.
- **Acceptance tests**: firmware I-H5 (timeout ≤10.5 s, healthy follow-up) and I-H6 (mem refusal) per `RELIABILITY…SPEC.md:435-436`; input-cap boundary rows (2048 ok / 2049 refused); output-cap row `:2^100000`.
- **CI**: firmware compile only (CI executes nothing on-device). **Firmware tests**: CAS-15 harness rows; until then a manual serial checklist committed under `docs/specs/`. **Budget**: guard overhead <1 ms/query; no added allocation on the happy path.
- **Rollback**: guards are additive early-returns — revert cleanly. **Risk**: Medium (ctrl_c context-health unknown until hardware test; escalation path defined in Doc 2 §B.2). **Owner**: Fable (bridge) + Human (hardware verification).

## CAS-06 — Simplify idempotence + termination tests (and the two rule fixes they force)

- **Motivation**: NC-3/NC-4; unconditional `ln a+ln b` merge (R-A2) and the R-F3 pow guard are provable soundness holes.
- **Source evidence**: `SymSimplify.cpp:193-203` (fixed-point loop), `:454-471` (ln merge), `:689-699` (pow-of-pow), `:663-664` (0^0 order, pending D-1), `:635` (8-term cap); unchecked allocs `:228-234,247-253,483-485`.
- **Files**: `src/math/cas/SymSimplify.cpp` (restrict R-A2 to literal-positive operands; tighten R-F3 to integer-outer-exponent-or-literal-positive-base; null-check the three alloc sites → error propagation per NC-9; status reporting ok/passcap/growth-cap per Doc 1 §E.7-E.8); property tests in the host runner (idempotence + evaluate-preservation over 10k generated expressions, O7).
- **Invariants**: every corpus POLY/EXP/POW/TRIG/LOG row keeps its verdict except `known-bug` flips to PASS; no new rule additions in this ticket.
- **Acceptance**: `simplify(simplify(e))==simplify(e)` pointer-identity for 10k seeds ×2 generators; R-A2 adversarial row (symbolic operands do NOT merge); R-F3 row `((x)^2)^(1/2)` sampled at x=−2 unchanged; null-injection test (failing allocator shim) returns clean error.
- **CI**: fast lane property subset (1k seeds), extended full. **Budget**: 10k simplifies <10 s host; arena ≤256 KB/expression.
- **Rollback**: per-rule revert; property tests stay. **Risk**: Medium (edu-steps output may change wording paths — EquationsApp/CalculationApp snapshot review required). **Owner**: Opus.

## CAS-07 — Derivative oracle

- **Motivation**: NC-5; Abs currently nullifies whole derivatives silently (`SymDiff.cpp:281-283`).
- **Source evidence**: Doc 1 §F; `CalculusApp.cpp:835` (nullptr consumer to audit).
- **Files**: host runner O5-numeric-check module (central difference per Doc 1 §F.6); `src/math/cas/SymDiff.cpp` (replace bare nullptr for Abs/unknown with a typed unsupported result or a documented nullptr contract consumed by CalculusApp as J.2); depth cap 64 per §F.5; corpus E.9 rows activated.
- **Invariants**: all 17 legacy SymDiffTest expectations preserved (or reviewed); every DIFF row passes both O2 (pinned tree) and numeric check.
- **Acceptance**: E.9 green incl. `d/dx|x|` → explicit J.2 error surfaced in CalculusApp UI (emulator script once Calculus is emulator-enabled per P-05; until then host-level assert).
- **CI**: fast lane. **Budget**: numeric check ≤7 evals ×2 per row; <1 s total.
- **Rollback**: oracle stays; SymDiff edits revert independently. **Risk**: Low-Medium. **Owner**: Sonnet (oracle) + Opus (SymDiff error surface).

## CAS-08 — Integration oracle

- **Motivation**: NC-6; nullptr-on-failure is invisible to users (Doc 1 §G.2).
- **Source evidence**: `SymIntegrate.cpp:41-536,81`; `SymIntegrate.h:27,54`; `SymExprToAST.h:56-66` (+C display).
- **Files**: host runner derivative-of-integral module (O5, per-sign sampling for `ln|u|`); `src/math/cas/SymIntegrate.cpp` (implement the unevaluated-fallback wrapper the header promises: failure → `symFunc(Integral, f)`); CalculusApp handling audit (`CalculusApp.cpp:879-1005`).
- **Invariants**: every current success case unchanged; failure never returns nullptr to apps.
- **Acceptance**: E.10 green; `∫e^(x²)` renders `∫e^(x²)dx` unevaluated in CalculusApp; O5 passes for all table/u-sub/parts rows; depth-boundary rows report status via O8.
- **CI**: fast lane. **Budget**: H-4 5 s deadline per integrate (once H-4 lands; until then wall-clock corpus timeout). 
- **Rollback**: wrapper is additive. **Risk**: Low. **Owner**: Sonnet.

## CAS-09 — Equation-solving oracle

- **Motivation**: NC-7; Newton paths can return unverified or miss roots (Doc 1 §H.2).
- **Source evidence**: `SingleSolver.cpp:458-495`; `HybridNewton.h:87-92`, `HybridNewton.cpp:50-60,97-98`; `SystemSolver.cpp:665,883`; `OmniSolver.h:98-102`.
- **Files**: host runner substitution-verifier (every returned root back into the original equation, exact-zero for exact roots, |f|<1e-8 for numeric); ordering + exact/approx tagging per H.2 in `OmniSolver`/`SingleSolver`/`HybridNewton` result structs; corpus E.11/E.12 activation.
- **Invariants**: exact linear/quadratic paths return bit-identical results pre/post ticket; approximate results carry the approx tag end-to-end into EquationsApp display (NC-1).
- **Acceptance**: E.11+E.12 green; EQ-ADV-2 documents the Newton-miss honestly (result wording "solutions found"); complex-metadata rows per D-2(a).
- **CI**: fast lane. **Budget**: solver rows ≤5 s each (H-4 alignment); 16-seed Newton ≤200 ms host.
- **Rollback**: verifier is harness-side; struct tags additive. **Risk**: Medium (EquationsApp display strings change → its screens have no goldens yet, P-05 dependency noted). **Owner**: Opus.

## CAS-10 — Exact/approx display contract in Calculation (NC-1)

- **Motivation**: the flagship honesty fix — `√2+√3` and `sin(1)` currently display as exact-looking fractions (Doc 1 §A.4-2, §D.4).
- **Source evidence**: `MathEvaluator.cpp:107-149` (`fromDouble` leaves `approximate=false`), `:309-311` (radical-sum degrade), `:393-399,418-422` (pow paths); `CalculationApp.cpp:532-549` (display-mode logic); `ExactVal.h:51-56`.
- **Files**: `src/math/MathEvaluator.cpp` (every `fromDouble` result from a non-exact source sets `approximate=true`; audit each of the ~15 call sites individually — typed-decimal parsing stays exact per D.3); `src/math/ExactVal.h` docs; `CalculationApp.cpp` + `MathEvaluator.cpp` resultToAST (`≈` prefix rendering per D.20.1); `VariableManager.h/.cpp` (persist `approximate/approxVal` — extend `/vars.dat` record with versioned magic per Doc 1 A.4-10).
- **Design**: distinguish *sources*: exact constructors (fromFrac/fromInt/fromRadical/fromPi/fromE, digit parsing) vs approximation events (transcendental fallback, unlike-radical arithmetic, non-integer pow, overflow). The `approximate` flag becomes "not exactly equal to the true value", not "too big for int64".
- **Invariants**: all currently-exact CI rows (`1+2`, `5/6`, …) show NO marker (regression guard: forbidden-output `≈` on those rows); Periodic mode refuses approximate values (D.20 unacceptable row).
- **Acceptance**: E.18 DISP rows green; BUG rows √2+√3 and VariableManager round-trip flip to PASS; goldens: `calc_1_plus_2.ppm`/`calc_fraction_sum.ppm` byte-identical (no visual change for exact results); new golden for an `≈` result blessed by human.
- **CI**: fast lane + golden compare. **Firmware**: `/vars.dat` migration test (old-format file loads, VR01→VR02). **Budget**: zero per-eval overhead beyond a flag write.
- **Rollback**: flag-source revert; display revert; keep the persistence version bump (forward-compatible). **Risk**: Medium-High (touches result formatting = golden surface + user trust). **Owner**: Fable.

## CAS-11 — Graph/CAS evaluator parity tests

- **Motivation**: NC-10; DV-1..DV-10 register (Doc 1 §C.8) is currently untested folklore.
- **Source evidence**: grapher spec GR-02/GR-14 (`NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md:22-61`, unimplemented [V]); `GraphModel.cpp:218-236`; angle-mode findings (`MathEvaluator.cpp:53`; grapher spec `:226-228`).
- **Files**: depends on GR-14 hooks (or minimal subset: `assert_graph_table_value`, `set_angle_mode`); new `.numos` scripts under `tests/emulator/scripts/parity/` from E.21 projections; host differential module (MathEvaluator vs RPN evaluator over shared-function inputs, Doc 3 §F row 3).
- **Design**: parity failures that reflect *documented* gaps (DV-3/DV-4 function sets) assert the documented rejection; genuine value mismatches gate.
- **Invariants**: no engine behavior changes in this ticket — tests only; angle-mode *fix* itself belongs to GR-02 (grapher scope) + a small Calculation-side ticket ratifying D.19 (SettingsApp writes `g_angleMode`).
- **Acceptance**: E.21 rows executable; DV register fully covered by a test or an explicit "untestable until <ticket>" annotation.
- **CI**: fast lane (emulator scripts) + host module. **Budget**: ≤10 scripts × existing per-script timeout.
- **Rollback**: tests only. **Risk**: Low. **Owner**: Sonnet (after GR-14 lands; coordinate with grapher ticket owner).

## CAS-12 — Complex-mode contract (executes D-2 decision)

- **Motivation**: `setting_complex_enabled` exists but nothing implements complex arithmetic; quadratic complex roots have display metadata only (Doc 1 §D.15).
- **Source evidence**: `Config.h:77-79`; `MathAST.h:196-200` + `MathEvaluator.cpp:988` (Imag → error); `SingleSolver.h:73-80`; `GiacBridge.cpp:193-212` (real mode).
- **Files** (per recommendation D-2(a)): EquationsApp complex-root display path (render `a±bi` from metadata); SettingsApp: either remove the complex toggle or bind it to "show complex roots" semantics only; corpus E.13 activation; docs.
- **Invariants**: no engine gains complex arithmetic; Giac stays `complex_mode(false)`; every native engine's `√(−1)` remains J.3.
- **Acceptance**: E.13 rows green; `x²+2x+5=0` shows `no real solutions — x = −1 ± 2i`; settings toggle semantics documented and asserted.
- **CI**: fast lane once Equations is emulator-enabled (P-05); host-level solver asserts before that. **Budget**: display-only. 
- **Rollback**: display revert. **Risk**: Low after D-2 ratified (Human sign-off required on the decision itself). **Owner**: Sonnet + Human (decision).

## CAS-13 — Step-by-step correctness harness

- **Motivation**: fake steps are the worst tutor failure (Doc 1 §I.2); dedup window and caps unimplemented (`CASStepLogger.cpp:87-94`; no count cap [V]).
- **Source evidence**: `RuleEngine.h:361-380`; `CASStepLogger.h:83-108`; `CalculationApp.cpp:919-964`; `EquationsApp.cpp:1354,3193`; reliability F.2 steps-cap contract (`RELIABILITY…SPEC.md:301`).
- **Files**: host harness replaying corpus equations through RuleEngine/AlgebraicRules/SystemTutor asserting Doc 1 §I.6 (a)-(e); `src/math/cas/CASStepLogger.{h,cpp}` (100-step cap + truncation record + full-log dedup per I.3/I.5); phrasing snapshots as revisable golden text files (not gating correctness).
- **Invariants**: every step value/solution-set-preserving (I.2 oracle); tutor final answers equal non-tutor solver answers.
- **Acceptance**: harness green over E.11 linear/quadratic rows + 20 tutor-specific rows; steps-cap adversarial row (>100 steps) truncates with `[CAS] steps-cap` line; no arena-lifetime violation under ASAN (steps read after reset = the bug class in `CasMemory.h:302-306`).
- **CI**: fast lane. **Budget**: steps ≤100/solve; step generation inside H-4 deadline; StepVec ≤64 KB/solve.
- **Rollback**: cap constants revertible. **Risk**: Medium (PedagogicalLogger phrasing churn — keep phrasing goldens separate from correctness). **Owner**: Opus.

## CAS-14 — Output parser hardening (Giac → NumOS)

- **Motivation**: any future app-routing of Giac (D-4/G.2) requires a total parser; the console path already displays raw output unbounded (Doc 2 §C.3, §B.4).
- **Source evidence**: `GiacBridge.cpp:79,151-191,375-388` (existing ad-hoc normalizers); `SerialBridge.cpp:156-157` (raw echo).
- **Files**: new `src/math/giac/GiacResultParser.{h,cpp}` implementing the Doc 2 §C.3 restricted grammar (shares `parseRestricted` with CAS-03); fuzz target `tests/host/giac_parser_fuzz.cpp` (libFuzzer-compatible `main` fallback for CI); recorded-output corpus `tests/corpus/giac_outputs/*.txt` (populated from CAS-15 hardware runs, human-reviewed).
- **Invariants**: total function (accept/refuse, never crash/UB); memory ≤ input×4; refuse is loss-free (raw string preserved for console display).
- **Acceptance**: Doc 3 §H.9 fuzz clean for 10⁷ execs under ASAN; all recorded real outputs parse or refuse per expectations; comparator self-test.
- **CI**: extended lane fuzz (bounded 5 min) + fast-lane corpus replay. **Budget**: parse <1 ms for ≤8 KB inputs.
- **Rollback**: module unused until wired — delete. **Risk**: Low until wired; Medium at wiring time. **Owner**: Sonnet (parser) + Fable (wiring decision).

## CAS-15 — Hardware Giac stress + golden script (= ground-truth P-02)

- **Motivation**: the flagship feature has zero tests (Doc 2 §F); CAS-04/CAS-05/BigInt rows all terminate here.
- **Source evidence**: serial protocol (`SerialBridge.cpp:110-160`); probes (`GiacBridge.cpp:394-445`); validate env (`platformio.ini:116-121`); reliability I-H5/I-H6 (`RELIABILITY…SPEC.md:435-436`).
- **Files**: new `scripts/giac-hardware-harness.py` (pyserial driver: flash-assumed device, send corpus GIAC-* rows, parse `: => ` responses + `[GIAC]/[MEM]` lines, emit JSONL report + recorded-golden candidates); new `tests/corpus/giac_goldens/` (human-blessed expected outputs, promotion script mirroring `promote-emulator-golden.py` policy); `docs` runbook.
- **Design**: phases per Doc 2 §F.1-F.5: smoke → 50+ correctness goldens → adversarial → memory stress (probe deltas) → timeout stress (only after CAS-05; before it, timeout rows are skipped with a hard warning, never run unattended).
- **Invariants**: harness never brick-risks the device (no flash writes; power-cycle recovery documented); goldens recorded-then-frozen, human-reviewed like PPM goldens.
- **Acceptance**: full run on real hardware producing ≥50 blessed goldens + a memory report with stable post-warm-up deltas; re-run reproduces goldens byte-identically (B.7 determinism, fresh-boot ordering).
- **CI**: not in CI (hardware); release-checklist item per Doc 3 §G. **Budget**: ≤30 min attended run.
- **Rollback**: n/a (tooling). **Risk**: Medium (first real Giac data — expect golden churn during recording). **Owner**: Human (device) + Fable (harness + triage).

---

## Dependency graph

```
CAS-00 → CAS-01 → CAS-02 → CAS-03 ─┬→ CAS-04 (advisory)
                    │               └→ CAS-14
                    ├→ CAS-06 → CAS-07 → CAS-08
                    ├→ CAS-09 → CAS-12, CAS-13
                    └→ CAS-10
CAS-05 (independent after 00) → CAS-15 (needs 05 for timeout rows; can record goldens before)
CAS-11 blocked on GR-14 (grapher ticket) + CAS-02
Decisions D-1, D-2, D-3 must be ratified (Human) before CAS-06/CAS-12/corpus RAD-ODD rows gate.
```

---

*End of Document 4.*
