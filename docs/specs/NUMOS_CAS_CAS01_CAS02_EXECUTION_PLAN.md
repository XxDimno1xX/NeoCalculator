# NumOS — CAS-01 + CAS-02 Execution Plan

> Zero-ambiguity execution pack for **CAS-01** (host harness for the dormant CAS suites) and **CAS-02** (JSONL corpus schema + first 100 rows), per `NUMOS_CAS_IMPLEMENTATION_TICKETS.md:23-43`.
> Date: 2026-07-02 · Branch: `claude/cas-01-02-execution-semantics-i3ngue` (clean, on `main` @ `5a03ac2`).
> Companion deliverables of this pack: `NUMOS_CAS_HOST_HARNESS_SPEC.md` (Doc H — harness design, **empirically validated by trial compile+run this audit**), `NUMOS_CAS_CORPUS_100_SPEC.md` (Doc C — the 100 rows, schema-validated), `NUMOS_CAS_SEMANTICS_DECISION_RECORDS.md` (Doc R — D-1/D-2/D-3 ADRs).
> This pack is SPEC-ONLY: no source, test, CI, or config file was modified; the trial builds ran in a scratch directory.

---

## A. Executive summary

CAS-01/CAS-02 turn 6.6k lines of never-executed CAS assertions plus a 100-row semantics corpus into a gating host CI lane, without changing a single engine behavior. During this planning audit the harness design was **actually executed** (scratch build, zero repo changes), which converted the plan from prediction to measurement and immediately produced results:

1. Six of eight dormant suites compile unmodified; two (`CASTest.cpp`, `CalculusStressTest.cpp`) have mechanical API bitrot (11 + 64 compile errors, catalogued in Doc H §B.2) — the oracle spec's "no API archaeology" claim (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md:29`) is corrected.
2. First-ever execution found **two unknown engine bugs**: **NB-1** — `solve ln(x)=1` stack-overflows via unguarded mutual recursion `OmniSolver.cpp:209-215` ↔ `TutorTemplates.cpp:929-931,1064-1066`, reachable from EquationsApp (`EquationsApp.cpp:2197-2198`), i.e. a probable on-device crash from a textbook input; **NB-2** — `x²−2=0` via OmniSolver returns one root, misclassified "Mixed transcendental" (Doc H §B.3).
3. Measured green baseline: BigIntTest 30/30, SymDiffTest 61/61, ASTFlatExprTest 90/90 (ASAN-clean); SymExprTest 37/39 (2 stale arena expectations); OmniSolverTest and TutorTemplateTest crash (NB-1) — hence the harness's per-suite process isolation requirement.
4. Two evidence corrections feeding the decision records: complex-root display is already implemented end-to-end (`SingleSolver.cpp:316-360`, `EquationsApp.cpp:2374-2390`), and VPAM already computes real odd roots of negatives (`MathEvaluator.cpp:788-796`) — D-2 and D-3 are largely ratifications, not projects (Doc R).

The work ships as three PRs (§D). PR-1 is deliberately tiny (3 new files + 3 CI steps) and proves the lane end-to-end with the crashes contained as XFAIL.

## B. Why CAS-01/CAS-02 must precede CAS behavior fixes

- **No safety net exists.** The only executed math tests are `.numos` semantic asserts over VPAM arithmetic (`emulator-build.yml:260-356`); zero symbolic diff/integrate/solve/BigInt/step tests run anywhere (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md:23`). Every ticket CAS-05..CAS-13 edits engine code; without CAS-01/02 first, each fix is unverifiable and each regression invisible. This is the ticket file's explicit ordering rationale (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:6`).
- **The audit just proved the point empirically.** Thirty minutes of harness execution found a device-crash-class bug (NB-1) in code that has shipped for the project's lifetime. Fixing NB-1 *before* the harness merges would itself be an untested engine change; fixing it after means the fix flips a visible XFAIL→PASS (`EQ-006`) plus an xfail-crash→pass suite line — the regression-net working as designed.
- **Decisions need pins.** D-1/D-2/D-3 corpus rows (`POW-002..004`, `CPLX-*`, `RAD-006..008`) must exist as tagged, non-gating rows *before* ratification so the decision's implementation flips them observably (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` §E.17 rationale).
- **Golden-surface protection.** CAS-10 (display honesty) touches result formatting — the highest-trust user surface. Its invariant "currently-exact rows show no marker" is only enforceable if those rows are already executing (corpus rows ARITH/RAT with `forbidden_outputs: ["≈"]`).

## C. Exact file-touch list (future implementation; nothing touched by this pack)

### C.1 PR-1 — "CAS-01a: host lane, six suites" (all additive)

| File | Action | Content contract |
|---|---|---|
| `tests/host/cas_suite_main.cpp` | **NEW** | driver per Doc H §D.2: per-suite fork + 60 s alarm, failure regex `\[FAIL\]|FAIL: `, XFAIL/XPASS logic, `[CAS-HOST]` footer, defines `setting_complex_enabled/setting_decimal_precision/setting_edu_steps` (globals owned by `src/main.cpp:31-33` / `src/hal/NativeHal.cpp:125` in real builds) |
| `scripts/build-cas-host-tests.sh` | **NEW** | exact content Doc H §D.1 (TU list is the empirically verified closure: 6 suite files + `src/math/cas/*.cpp` + `src/math/MathAST.cpp` + `src/math/MathEvaluator.cpp` + `src/math/VariableManager.cpp` + `src/math/font/MathGlyphAssembly.cpp` + `src/hal/FileSystem.cpp`) |
| `tests/host/cas_xfail.list` | **NEW** | initial entries per §H |
| `.github/workflows/emulator-build.yml` | **EDIT** (append-only steps) | 3 steps after line 104 per Doc H §E: run, ASAN run, artifact upload |

### C.2 PR-2 — "CAS-01b: revive the two bitrotted suites" (test-files only)

| File | Action | Exact edits |
|---|---|---|
| `tests/CASTest.cpp` | EDIT | 11 sites: `coeff.num`/`coeff.den` field syntax → `coeff.num().toInt64()`/`coeff.den().toInt64()` (first sites `:83-84,93,99-100`; full list = the 11 compile errors, Doc H §B.2) |
| `tests/CalculusStressTest.cpp` | EDIT | 64 sites: `arena.symX(…)` member-call syntax → free builders `cas::symX(arena, …)` (20 symFunc, 14 symInt, 11 symPow, 5 symMul, 4 symNum, 3 symAdd, 2 symVar); 3 × qualify `vpam::ExactVal` |
| `tests/SymExprTest.cpp` | EDIT (2 asserts) | update the two stale arena-lifecycle expectations (`:96-99`) to the lazy-block-0 contract (`SymExprArena.h:97`): after init `blockCount == 0`, `totalAllocated == 0`; each edit's commit message cites the source line proving current behavior is intended |
| `tests/TutorTemplateTest.cpp` | EDIT (2 asserts) or XFAIL | triage NB-4 (`:124,128`): if current step order is intended, fix expectations with evidence; else leave XFAIL'd and file an engine ticket |
| `tests/host/cas_suite_main.cpp`, `scripts/build-cas-host-tests.sh`, `tests/host/cas_xfail.list` | EDIT | add the two suites; prune revived XFAILs |

**Assertion-edit rule** (inherited from `NUMOS_CAS_IMPLEMENTATION_TICKETS.md:29`): PR-2 may change *syntax* freely but an *expected value* only with a cited source-line justification in the diff; anything contested stays XFAIL.

### C.3 PR-3 — "CAS-02: corpus + runner" (all additive)

| File | Action | Content contract |
|---|---|---|
| `tests/corpus/cas_corpus.jsonl` | **NEW** | the 100 rows of Doc C §D, byte-for-byte; §F validation script must pass |
| `tests/host/corpus_runner.cpp` | **NEW** | loads/validates schema (unknown field = load error); executes `symexpr` rows via the Doc C §C micro-grammar against cas:: (O1/O2/O4/O8 subset; O5 for INT rows incl. the `ln|u|` special case); executes `string`+`engine:rpn` rows via Tokenizer/Parser/Evaluator; skips (with counted SKIP report) `keys`, `giac`, `graph`, `display-only`, `blocked-gr14` vehicles; honors `status:` tags (xfail/blocked = report-only); child-process sandbox for rows tagged `crash`; exit = unexpected failures |
| `scripts/corpus-to-numos.py` | **NEW** | projects `input_form: keys` + `engine: vpam` rows to `.numos` scripts under `tests/emulator/scripts/corpus/` (id-stable names `corpus_<id>.numos`, key vocabulary per existing scripts; rows tagged `host-first` skipped); idempotent regeneration |
| `.github/workflows/emulator-build.yml` | EDIT (append-only) | corpus-runner step after the CAS host suites step; projected corpus scripts join the existing `.numos` glob execution; artifact = runner's JSONL result file |

### C.4 Explicitly out of scope for all three PRs (each needs its own ticket)

- **NB-1 fix** — proposed ticket **CAS-06b "solver re-entry guard"**: depth/re-entry cap in `OmniSolver::solve` (`src/math/cas/OmniSolver.cpp:209-215`) or tutor-side guard at `TutorTemplates.cpp:929-931,1064-1066`; acceptance = `EQ-006` XFAIL→PASS + OmniSolver/Tutor suites lose `xfail-crash`.
- **NB-2 fix** — same ticket or follow-up: OmniSolver classification of pure polynomials (`OmniSolver.h:67-71` path); acceptance = `EQ-008` flip.
- Any `src/**` change whatsoever, `platformio.ini`, goldens/masks, D-1/D-2/D-3 implementation (Doc R), `MathEnginePhaseRegression.cpp` revival (separate ticket per oracle spec §B).

## D. Implementation sequence

**PR-1** (Sonnet-sized, ~half day): write driver + script + xfail list exactly per Doc H; run `scripts/build-cas-host-tests.sh` locally → expect footer `total suites=6 pass=3 xfail=3 unexpected=0`; run `--asan` variant → same but crashes reported as ASAN stack-overflow (still xfail-crash); add the 3 CI steps; push; verify CI wall-time delta ≤ 90 s.

**PR-2** (Sonnet mechanical + Opus review, ~half day): apply §C.2 edits; local run → expect `total suites=8` with CASTest and CalculusStressTest green (CalculusStressTest asserts arena block count ≤ 4, `CalculusStressTest.cpp:16-26` — if it fails on host, triage per Doc H §F, likely XFAIL bucket 1); SymExprTest → 39/39; prune those xfail entries (XPASS discipline enforces this automatically).

**PR-3** (Sonnet runner + Fable row review, ~1 day): land corpus + runner + projector; local acceptance per §F; wire CI step. May land in parallel with PR-2 (dependency is only on PR-1's script/driver files).

## E. Dependency graph

```
this spec pack (docs only, no deps)
   └→ PR-1 (CAS-01a: driver+script+xfail+CI)          ── blocks → every CAS-05..CAS-13 engine change
        ├→ PR-2 (CAS-01b: revive CASTest + CalculusStressTest)
        └→ PR-3 (CAS-02: corpus + runner + projector)
              ├→ CAS-03 (normalizer replaces the §C micro-grammar serializer)
              ├→ CAS-06b (NB-1/NB-2 fixes flip EQ-006/EQ-008 + suite crash lines)
              └→ D-1/D-2/D-3 ratification (Human) → blocked-d* rows begin gating
PR-2 ∥ PR-3 (independent of each other)
Giac rows (EXP-005/006, BUG-001 oracle O6) → CAS-15 hardware lane (unchanged by this pack)
BUG-004 → GR-14 assert hooks (grapher ticket, unchanged)
```

## F. Acceptance gates

| Gate | PR | Check | Expected |
|---|---|---|---|
| G1 | PR-1 | `scripts/build-cas-host-tests.sh` local + CI | exit 0; footer exactly `total suites=6 pass=3 xfail=3 unexpected=0` (BigInt, SymDiff, ASTFlat pass; SymExpr xfail ×2 cases; Omni + Tutor xfail-crash) |
| G2 | PR-1 | `--asan` variant | exit 0; ASAN reports appear only inside xfail-crash suites' logs |
| G3 | PR-1 | CI wall time | added steps ≤ 90 s total (measured baseline: 28 s compile + <1 s run per variant) |
| G4 | PR-1 | byte-identity | `pio run -e emulator_pc` output binary unchanged vs main (no `src/**` diff ⇒ guaranteed; spot-check `git diff --stat main -- src platformio.ini` is empty) |
| G5 | PR-2 | full run | `total suites=8`, CASTest + CalculusStressTest pass, SymExprTest 39/39, xfail list contains only NB-1-related crash entries (+ NB-4 if triaged as engine issue) |
| G6 | PR-3 | corpus validation | Doc C §F script passes (100 rows, schema-exact, unique ids, every row has a `status:` tag) |
| G7 | PR-3 | runner | executes ≥ the 45 `native-cas-only` + RPN rows; report shows 21 XFAIL (exactly the `known-bug` set), 0 unexpected failures, explicit SKIP count for keys/giac/graph/display vehicles; `crash`-tagged rows sandboxed (runner survives) |
| G8 | PR-3 | projector | `scripts/corpus-to-numos.py` emits deterministic scripts for the keys+vpam rows (minus `host-first`); each passes locally in the emulator (`--headless --deterministic`); re-run produces byte-identical files |
| G9 | all | XPASS discipline | flipping any XFAIL to pass without pruning the list fails CI (proved once in PR-1 review by a deliberate dry-run) |

## G. What must remain byte-identical

- **Everything under `src/`** (all three PRs) ⇒ firmware image and emulator binary functionally identical; `compile-and-release.yml` artifacts unchanged.
- `platformio.ini` — untouched (the harness is deliberately outside PlatformIO, Doc H §C.4).
- `tests/emulator/golden/**`, `tests/emulator/masks/**` — untouched; no golden is generated, promoted, or compared differently. Existing `.numos` scripts untouched (PR-3 only *adds* under `tests/emulator/scripts/corpus/`).
- Existing CI steps in `emulator-build.yml` — untouched; new steps append-only (the NativeHal script-runner grammar and exit codes are CI contracts, context header fragile-files list).
- `lib/giac/**`, `lib/libtommath/**` — never in the host compile line.
- PR-2 exception, stated for honesty: `tests/CASTest.cpp`, `tests/CalculusStressTest.cpp`, `tests/SymExprTest.cpp`(2 lines), `tests/TutorTemplateTest.cpp`(≤2 lines) change — these files are currently compiled by **nothing** (`platformio.ini:102-103`), so no build artifact anywhere can change.

## H. Initial XFAIL inventory (`tests/host/cas_xfail.list`, PR-1)

| Suite | Kind | Match | Reason / ticket |
|---|---|---|---|
| SymExprTest | case | `Arena: blockCount == 1 after init` | stale vs lazy block 0 (`SymExprArena.h:97`); fixed in PR-2 |
| SymExprTest | case | `Arena: totalAllocated == 1024` | same |
| OmniSolverTest | case | `B2 two solutions` | NB-2 misclassification → CAS-06b |
| OmniSolverTest | crash | `SIGSEGV` (during/after case C `ln(x) = 1`) | NB-1 mutual recursion → CAS-06b |
| TutorTemplateTest | case | `QuadTutor keeps x² term before -5x term` | NB-4 phrasing/order triage → PR-2 |
| TutorTemplateTest | case | `QuadTutor keeps constant after variable terms` | NB-4 |
| TutorTemplateTest | crash | `SIGSEGV` (log-tutor section) | NB-1 |

Corpus-side XFAIL (PR-3) = exactly the 21 `status:xfail` rows of Doc C (list in Doc C §E). Nothing else may be XFAIL'd without updating this table's successor in the PR description.

## I. Rollback plan

- **PR-1 / PR-3 rollback**: delete the appended workflow steps (single-commit revert); the new files are inert without CI wiring (nothing includes or executes them). No state, no migrations. Partial rollback option: mark a step `continue-on-error: true` for one cycle instead of deleting, if flake-triage time is needed (then Doc 3 §G's 48 h flake policy applies).
- **PR-2 rollback**: revert the test-file edits; suites return to not-compiling; driver's suite list reverts with the same commit (the driver references the suites explicitly, so a partial revert fails the build loudly rather than silently dropping coverage).
- **Emergency CI unblock** (any PR): `git revert` of the workflow-file commit alone restores the exact prior CI behavior because all steps are append-only.
- Not needed: golden re-blessing, firmware reflash, data migration — nothing in this scope touches them.

## J. PR checklists

### PR-1
- [ ] `git diff --stat main -- src platformio.ini tests/emulator/golden tests/emulator/masks` is empty
- [ ] New files: `tests/host/cas_suite_main.cpp`, `scripts/build-cas-host-tests.sh` (`chmod +x`), `tests/host/cas_xfail.list`; workflow diff is append-only (3 steps)
- [ ] Local: `scripts/build-cas-host-tests.sh` → exit 0, footer `total suites=6 pass=3 xfail=3 unexpected=0`
- [ ] Local: `--asan` → exit 0
- [ ] Deliberate-failure proof: temporarily remove one xfail entry → run exits nonzero (XPASS/regression detection works); restore
- [ ] CI green; runtime delta ≤ 90 s; `cas-host-test-logs` artifact present
- [ ] PR body: paste the footer + link Doc H; state NB-1/NB-2 as pre-existing bugs found, explicitly NOT fixed here
- [ ] No commit touches `src/**`

### PR-2
- [ ] Edits confined to the four test files + harness files (§C.2)
- [ ] Every expected-value change cites a source line in the commit message; syntax-only changes marked as such
- [ ] Local: footer `total suites=8 … unexpected=0`; SymExprTest 39/39; CASTest & CalculusStressTest green or triaged per Doc H §F
- [ ] `cas_xfail.list` pruned; XPASS check re-proved
- [ ] CI green both variants

### PR-3
- [ ] `tests/corpus/cas_corpus.jsonl` byte-equal to Doc C §D block; Doc C §F validator passes in CI (add as part of the runner's load or a one-line step)
- [ ] Runner: 0 unexpected failures, 21 XFAIL, SKIPs enumerated with reasons, crash rows sandboxed
- [ ] Projector output deterministic (run twice, `diff -r` empty); projected scripts pass locally in the emulator with `--deterministic`
- [ ] No changes to existing `.numos` scripts or goldens
- [ ] PR body: coverage table from Doc C §E; note the 9 `blocked-d*` rows await Doc R ratification
- [ ] CI green; total added runtime for corpus step ≤ 30 s (ticket budget, `NUMOS_CAS_IMPLEMENTATION_TICKETS.md:42`)

---

*End of execution plan. Harness detail: Doc H. Rows: Doc C. Decisions: Doc R.*
