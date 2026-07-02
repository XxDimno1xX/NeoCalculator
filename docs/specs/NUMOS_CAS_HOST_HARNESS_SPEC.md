# NumOS — CAS Host Test Harness Specification (CAS-01)

> Execution-ready design for reviving the eight dormant CAS suites on a host (Linux CI) build.
> Date: 2026-07-02 · Branch: `claude/cas-01-02-execution-semantics-i3ngue` (clean, on `main` @ `5a03ac2`).
> Companion: `NUMOS_CAS_CAS01_CAS02_EXECUTION_PLAN.md` (PR sequencing), `NUMOS_CAS_CORPUS_100_SPEC.md` (CAS-02 rows), `NUMOS_CAS_SEMANTICS_DECISION_RECORDS.md` (D-1/D-2/D-3).
>
> **This spec is empirically grounded**: every dormant suite was trial-compiled and (where it compiles) trial-executed with `g++ 13 -std=c++17` on Linux during this audit, in a scratch directory, with zero repo modifications. Results in §B are measured, not predicted. This is the first time any of these suites has ever executed on any platform (dormancy proof: `src/main.cpp:56-64,105-115` gate on `CAS_RUN_TESTS`, defined nowhere; `platformio.ini:102-103` `build_src_filter = +<*>` never compiles `tests/*.cpp`).

---

## A. Executive summary

- Six of the eight suites compile unmodified on host and four of those six pass or nearly pass. Two suites (`CASTest.cpp`, `CalculusStressTest.cpp`) have **API bitrot** — they were written against older `cas::` APIs and no longer compile (§B.2). The oracle spec's claim that "revival is harness work, not API archaeology" (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md:29`) is **wrong for these two files**.
- First execution found **two previously unknown engine bugs**, one of them severe: `ln(x) = 1` — the most basic inverse equation — crashes with a stack overflow via unguarded mutual recursion `OmniSolver::solve` ↔ log/exp tutor templates (§B.3, finding NB-1). This path is reachable from EquationsApp on firmware (`EquationsApp.cpp:2197-2198`), where it would exhaust the 64 KB loopTask stack (`platformio.ini:44`).
- Consequence for harness design: **suites must run in isolated child processes with timeouts** so one crashing suite cannot mask the others' results (§E.2).
- The harness is a standalone `g++` build driven by one shell script, mirroring the existing `tests/host/keycode_digit_test.cpp` pattern (CI step `.github/workflows/emulator-build.yml:96-104`). No PlatformIO env, no source edits, no behavior change.

---

## B. Dormant test inventory — with first-run results

### B.1 Inventory and measured status

Dormancy mechanism (all eight): includes gated by `#ifdef CAS_RUN_TESTS` (`src/main.cpp:56-65`), invocations gated the same way (`src/main.cpp:105-115`), flag defined in no `platformio.ini` env, and `tests/*.cpp` never in any `build_src_filter` (`platformio.ini:102-103`). Every suite already has a dual output path — `Serial` under `ARDUINO`, `printf` otherwise (`CASTest.cpp:39-48`, `SymExprTest.cpp:42-51`, `SymDiffTest.cpp:47-54`, `ASTFlatExprTest.cpp:40-47`, `OmniSolverTest.cpp:47-54`, `BigIntTest.cpp:36-41`, `CalculusStressTest.cpp:40-47`, `TutorTemplateTest.cpp:30-39`) — so they were *designed* for host execution and never run there.

| Suite | Entry (all `void`, no exit code — headers `tests/*.h`) | Host compile | First-run result (this audit, g++ 13, `-O1`, Linux) |
|---|---|---|---|
| `tests/BigIntTest.cpp` | `cas::runBigIntTests()` | ✅ clean | **30/30 PASS** (int64 subset; promotion cases compiled out via `CAS_HAS_BIGINT=0`, `CASInt.h:53-59`) |
| `tests/SymExprTest.cpp` | `cas::runSymExprTests()` | ✅ clean | **37/39 — 2 FAIL**: `Arena: blockCount == 1 after init` and `Arena: totalAllocated == 1024` (`SymExprTest.cpp:96-99`) — stale expectations vs. the arena's lazy first block (`SymExprArena.h:97` allocates block 0 on first use, not in the ctor). Test bug, not engine bug. |
| `tests/SymDiffTest.cpp` | `cas::runSymDiffTests()` | ✅ clean | **61/61 PASS** (banner says 32 tests; actual check count is 61) |
| `tests/ASTFlatExprTest.cpp` | `cas::runASTFlatExprTests()` | ✅ clean | **90/90 PASS**, also clean under `-fsanitize=address` `-O0` |
| `tests/OmniSolverTest.cpp` | `cas::runOmniSolverTests()` | ✅ clean | **CRASH** (SIGSEGV / ASAN `stack-overflow`) at case C `ln(x) = 1` (`OmniSolverTest.cpp:185-190`); also 1 real FAIL before the crash: `B2 two solutions` (`OmniSolverTest.cpp:145`) — `x²−2=0` classified "Mixed transcendental", returns only `x1 = 1.414213562` (missing −√2). See §B.3. |
| `tests/TutorTemplateTest.cpp` | `cas::runTutorTests()` | ✅ clean | **2 FAIL then CRASH**: `QuadTutor keeps x² term before -5x term`, `QuadTutor keeps constant after variable terms` (`TutorTemplateTest.cpp:124,128`) — step-phrasing/order expectations; crash on reaching the log-tutor section (same recursion as OmniSolverTest, §B.3). |
| `tests/CASTest.cpp` | `cas::runCASTests()` | ❌ **11 compile errors** | All one pattern: `t.coeff.num == 3` where `CASRational::num()`/`den()` are now accessor functions returning `const CASInt&` (`CASRational.h`; first error at `CASTest.cpp:83-84`). Mechanical fix: `t.coeff.num().toInt64() == 3`. |
| `tests/CalculusStressTest.cpp` | `cas::runCalculusStressTest()` | ❌ **64 compile errors** | Written against an older arena API: `arena.symInt(...)`/`arena.symFunc(...)` member calls (now free functions `cas::symInt(arena, …)` etc., `SymExpr.h`), plus 3 × `'ExactVal' has not been declared` (missing `vpam::` qualification or include). Mechanical but extensive. |

Not in CAS-01 scope: `tests/MathEnginePhaseRegression.cpp` (render-geometry, own `main()` at `:2041`, reads `platformio.ini` at runtime `:76-77` — separate ticket per oracle spec §B), `tests/TokenizerTest_temp.cpp` (throwaway, CAS-00 deletes it), `tests/HardwareTest.cpp` (hardware-only).

### B.2 API-bitrot detail (input to PR-2)

- `CASTest.cpp`: 8 × `coeff.num` and 3 × `coeff.den` used as public fields. Exact fix per site: compare through `.num().toInt64()` / `.den().toInt64()` (int64 range is safe — the tests use single-digit constants). Zero logic changes.
- `CalculusStressTest.cpp`: 20 × `arena.symFunc`, 14 × `arena.symInt`, 11 × `arena.symPow`, 5 × `arena.symMul`, 4 × `arena.symNum`, 3 × `arena.symAdd`, 2 × `arena.symVar` → rewrite as free-function builders `symFunc(arena, …)` etc. (current API: `SymExpr.h`); 3 × `ExactVal` → `vpam::ExactVal` (or add `#include "../src/math/ExactVal.h"`). Zero logic changes.
- Rule: these edits are **test-file-only**, reviewed as "port to current API", and must not alter any assertion's expected value. Any expectation that then fails at runtime is triaged per §F, never silently adjusted.

### B.3 New engine findings from first execution (register these; do NOT fix in CAS-01)

| ID | Finding | Evidence | Severity |
|---|---|---|---|
| **NB-1** | Unguarded mutual recursion: `OmniSolver::solve` calls `solveLogarithmicTutor` / `solveExponentialTutor` first (`OmniSolver.cpp:209-215`); each tutor recursively constructs a *new* `OmniSolver` and calls `solve` on the transformed equation (`TutorTemplates.cpp:929-931` log→exp, `TutorTemplates.cpp:1064-1066` exp→log). For `ln(x)=1` the pair ping-pongs until stack overflow (ASAN trace cycles `OmniSolver.cpp:213-214` ↔ `TutorTemplates.cpp:931,1065` via `PedagogicalLogger::logAction`). Reachable from EquationsApp (`EquationsApp.cpp:2197-2198`) → on firmware this is a 64 KB loopTask stack overflow (`platformio.ini:44`), i.e. a device crash/reboot from typing a textbook equation. | first-run ASAN trace; code sites cited | **Critical** |
| **NB-2** | `x²−2=0` reaches OmniSolver classified as "Mixed transcendental" and returns a single numeric root `1.414213562`, dropping `−√2` (`OmniSolverTest.cpp:141-149` expectations vs. observed output). | first-run log | High (wrong answer, silent) |
| NB-3 | SymExprTest arena-lifecycle expectations stale (lazy block 0) — test bug. | §B.1 row 2 | Low |
| NB-4 | TutorTemplateTest quadratic step-ordering expectations diverge from current `PedagogicalLogger` phrasing — test bug or intended phrasing change; triage per §F. | §B.1 row 6 | Low |

NB-1/NB-2 become corpus rows `EQ-006`/`EQ-008` (XFAIL) in `NUMOS_CAS_CORPUS_100_SPEC.md` and require a new fix ticket (proposed **CAS-06b**, see execution plan §C.4); the harness ships with them XFAIL'd, proving the safety-net works before any engine fix lands.

---

## C. Build design

### C.1 Translation-unit list (empirically verified link closure)

The following exact set compiles and links on host with no repo modifications (verified this audit):

```
tests/host/cas_suite_main.cpp          # NEW driver (PR-1)
tests/SymExprTest.cpp  tests/SymDiffTest.cpp  tests/ASTFlatExprTest.cpp
tests/OmniSolverTest.cpp  tests/BigIntTest.cpp  tests/TutorTemplateTest.cpp
# PR-2 adds: tests/CASTest.cpp  tests/CalculusStressTest.cpp
src/math/cas/*.cpp                     # all 24 (glob is safe; RuleBasedTutor.cpp links dead until CAS-00 deletes it)
src/math/MathAST.cpp                   # SymExprToAST/SymToAST target trees
src/math/MathEvaluator.cpp             # vpam::ExactVal implementation (SystemSolver dep)
src/math/VariableManager.cpp           # MathEvaluator dep
src/math/font/MathGlyphAssembly.cpp    # MathAST NodeRoot/Delimiter layout dep (vpam::DelimiterAssembler)
src/hal/FileSystem.cpp                 # native LittleFS shim (VariableManager dep; guarded #ifndef ARDUINO, hal/FileSystem.h:37)
```

Flags: `g++ -std=c++17 -O1 -I src` (test files include project headers via relative `../src/...` paths, e.g. `CASTest.cpp:30-36`, so only `-I src` is needed for `src/`-internal includes). No LVGL, no SDL, no `NATIVE_SIM` define needed — none of these TUs touches LVGL.

**Driver must define the three settings globals** (owned by `src/main.cpp:31-33` on firmware and `src/hal/NativeHal.cpp:125` in the emulator, both excluded here): `bool setting_complex_enabled = true; int setting_decimal_precision = 10; bool setting_edu_steps = false;` — `SingleSolver.cpp:330` reads `setting_complex_enabled` at link time (this is also D-2 evidence, see decision records). Keep `true` so quadratic complex-root tests exercise the implemented path.

### C.2 Arduino / ESP-only isolation

All Arduino/ESP dependencies inside the linked set are already `#ifdef ARDUINO`-guarded — no shims or stubs required beyond the two files above:

- `CASInt.h:53-59` — mbedtls bignum only under `ARDUINO`; host gets `CAS_HAS_BIGINT=0` (overflow → error, `CASInt.h:213-221`).
- `SymExprArena.h:50,125,187,210-245`, `ConsTable.h:45,194,212`, `CasMemory.h:81,325,339`, `PSRAMAllocator.h:37,72,91` — `heap_caps` PSRAM allocation under `ARDUINO`, plain `malloc`/`new` otherwise.
- `RuleEngine.cpp:39-51`, `SystemTutor.cpp:26-38` — Arduino includes guarded.
- `VariableManager.cpp:29-35` — `<LittleFS.h>` under `ARDUINO`, else `hal/FileSystem.h` (native wrapper writing under `./emulator_data/`, `hal/FileSystem.h:17-33`). Harness consequence: the binary may create `./emulator_data/` if a test path calls `saveToFlash()`; the runner script executes from a scratch working directory so the repo stays clean.

### C.3 Giac / libtommath / mbedtls / PSRAM policy

- **Giac: excluded, permanently.** No dormant suite includes any Giac header (verified: includes list §B). Native Giac is known-broken (`platformio.ini:179-185` — `gen.h: 'SIZEOF_INT' not declared`) and `lib_ignore = giac, libtommath` is deliberate (`platformio.ini:192-194`). The harness must never add `lib/giac` or `lib/libtommath` to its compile line; Giac testing is hardware-only (CAS-15) plus the advisory host-distro differential (CAS-04).
- **libtommath**: used only by Giac (`lib/giac/src/config.h:47` `USE_GMP_REPLACEMENTS`; no `src/` file includes tommath) → irrelevant to the host harness.
- **mbedtls**: only behind `CAS_HAS_BIGINT` (`CASInt.h:53-59`). Host runs the int64+error subset; the driver must print an explicit `SKIPPED (firmware-only): BigInt promotion/demotion cases` line so absence is visible, not silent (BigIntTest's promotion sections compile out with the flag).
- **PSRAM-only code**: arena/pool fall back to libc heap natively (§C.2). This means host results validate arena *logic* (offsets, reset, lifetime — ASAN-checkable) but say nothing about `MALLOC_CAP_SPIRAM` routing, fragmentation, or exhaustion order — those stay firmware-scope (memory audit `NUMOS_MEMORY_AND_PSRAM_AUDIT.md:163`).

### C.4 Why standalone g++, not a PlatformIO env

Owner: **standalone `g++` invoked by `scripts/build-cas-host-tests.sh`**, exactly like the keycode host test (`emulator-build.yml:102-104` runs `g++ -std=c++17 -Wall -Wextra tests/host/keycode_digit_test.cpp`). Rejected alternative — a `[env:host_tests]` PlatformIO native env — because: (a) LDF would try to resolve `lib/` private libraries and needs the same `lib_ignore` dance as `emulator_pc` (`platformio.ini:186-194`); (b) `build_dir = C:/.piobuild/numOS` (`platformio.ini:6`) requires the `PLATFORMIO_BUILD_DIR` workaround on every non-Windows machine (context header constraint 8); (c) an explicit TU list in one script is the whole point — the emulator whitelist precedent shows implicit filters are where coverage silently disappears. The script is the single source of truth for what links.

---

## D. Exact commands

### D.1 `scripts/build-cas-host-tests.sh` (NEW in PR-1 — content specified here, file not created by this spec)

```bash
#!/usr/bin/env bash
# Build + run the CAS host suites (CAS-01). Usage: scripts/build-cas-host-tests.sh [--asan] [--build-only]
set -euo pipefail
cd "$(dirname "$0")/.."                      # repo root
OUT="${CAS_HOST_OUT:-/tmp/cas-host-tests}"   # all artifacts out-of-tree
mkdir -p "$OUT"

CXX=${CXX:-g++}
FLAGS="-std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -I src"
[[ "${1:-}" == "--asan" ]] && FLAGS="$FLAGS -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all" && shift

$CXX $FLAGS -o "$OUT/cas_host_tests" \
  tests/host/cas_suite_main.cpp \
  tests/SymExprTest.cpp tests/SymDiffTest.cpp tests/ASTFlatExprTest.cpp \
  tests/OmniSolverTest.cpp tests/BigIntTest.cpp tests/TutorTemplateTest.cpp \
  src/math/cas/*.cpp \
  src/math/MathAST.cpp src/math/MathEvaluator.cpp src/math/VariableManager.cpp \
  src/math/font/MathGlyphAssembly.cpp src/hal/FileSystem.cpp

[[ "${1:-}" == "--build-only" ]] && exit 0
( cd "$OUT" && exec ./cas_host_tests --all --xfail "$OLDPWD/tests/host/cas_xfail.list" )
```

(PR-2 appends `tests/CASTest.cpp tests/CalculusStressTest.cpp` to the list and their runners to the driver.)

### D.2 Driver contract — `tests/host/cas_suite_main.cpp` (NEW in PR-1)

The `run*Tests()` functions return `void` and print heterogeneous summaries (`CASTest.cpp:1022` "`%d passed, %d failed`"; `SymExprTest.cpp:369-374` "`%d/%d PASSED`"/"`*** %d FAILED ***`"; `SymDiffTest.cpp:608` "`%d PASS, %d FAIL (total %d)`"; `ASTFlatExprTest.cpp:693-698`; `OmniSolverTest.cpp:290`; `BigIntTest.cpp:305`; `CalculusStressTest.cpp:301-307`; `TutorTemplateTest.cpp:373-374`). The driver therefore:

1. **Runs each suite in a forked child process** (`--suite <name>` self-exec) with a **60 s `alarm()` timeout**. Rationale: measured reality — OmniSolverTest and TutorTemplateTest crash (NB-1); in-process sequencing would lose every suite after the first crash *and* lose buffered stdout of already-passed suites (observed: ASTFlatExpr output truncated mid-line by the later crash under full buffering). Child stdout/stderr go to `$OUT/<suite>.log`.
2. **Parses per-case failure markers**, not summaries: regex `\[FAIL\]|FAIL: ` matches every suite's per-case failure line (`CASTest.cpp:61`, `SymExprTest.cpp:64`, `SymDiffTest.cpp:66`, `ASTFlatExprTest.cpp:59`, `OmniSolverTest.cpp:66`, `BigIntTest.cpp:55`, `CalculusStressTest.cpp:59,257`, `TutorTemplateTest.cpp:52`) and — deliberately — does **not** match the always-printed SymDiff summary "`0 FAIL (total`" (no colon/bracket). Suite outcome = {PASS, FAIL(n), CRASH(signal), TIMEOUT}.
3. **Applies the XFAIL list** `tests/host/cas_xfail.list` (NEW): one entry per line, format `SUITE<TAB>KIND<TAB>match-substring<TAB>ticket/reason`, `KIND ∈ {case, crash, timeout}`. An XFAIL'd failure is reported `XFAIL` and does not affect exit code; an XFAIL entry that *passes* is **XPASS and fails the run** (stale entries must be pruned — this is how bug-fix PRs visibly flip rows). Initial contents: see execution plan §H.
4. **Prints a machine-parsable footer** and exits with `count(unexpected FAIL) + count(unexpected CRASH/TIMEOUT)`:
   ```
   [CAS-HOST] suite=BigIntTest        status=pass   cases_failed=0
   [CAS-HOST] suite=SymExprTest       status=xfail  cases_failed=2 xfail=2
   [CAS-HOST] suite=SymDiffTest       status=pass   cases_failed=0
   [CAS-HOST] suite=ASTFlatExprTest   status=pass   cases_failed=0
   [CAS-HOST] suite=OmniSolverTest    status=xfail-crash signal=SIGSEGV xfail=1 pre_crash_failed=1
   [CAS-HOST] suite=TutorTemplateTest status=xfail-crash signal=SIGSEGV xfail=1 pre_crash_failed=2
   [CAS-HOST] total suites=6 pass=3 xfail=3 unexpected=0 exit=0
   ```
   (This exact footer is the PR-1 acceptance output; see execution plan §F.)
5. Defines the three settings globals (§C.1) and runs from the current working directory (the script already cd's to `$OUT`, keeping `emulator_data/` out of the repo).

### D.3 Local usage

```bash
scripts/build-cas-host-tests.sh            # build + run, exit 0 expected (with initial XFAIL list)
scripts/build-cas-host-tests.sh --asan     # ASAN/UBSAN variant (catches NB-1 as stack-overflow deterministically)
/tmp/cas-host-tests/cas_host_tests --suite OmniSolverTest   # one suite, in-process (for debugging; may crash your shell's exit code — expected)
```

Expected wall time (measured): compile ≈ 28 s serial `-O1` (one-shot, no incrementality needed); all suites execute in < 1 s combined. ASAN variant compile ≈ similar, run still < 5 s. Total CI budget ≤ 90 s for both variants — within the CAS-01 ticket's < 60 s target for the gating (non-ASAN) step alone.

---

## E. CI placement

- **Workflow**: `.github/workflows/emulator-build.yml` (host CI — same runner already has g++; the firmware workflow `compile-and-release.yml` executes nothing and stays untouched). Emulator CI is correct because this is host-vehicle testing per the oracle layer table (`NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` §A layer 1); no separate workflow is warranted until the extended/nightly lane exists (CAS-04+).
- **Position**: new step **"CAS host suites (CAS-01)"** inserted immediately after the keycode guard step (after `emulator-build.yml:104`) and before "Build emulator" — CAS regressions then fail in ~30 s instead of after the multi-minute emulator build. Step body (documentation — CI edit happens in PR-1, not this spec):
  ```yaml
  - name: CAS host suites (CAS-01)
    run: scripts/build-cas-host-tests.sh
  - name: CAS host suites (ASAN variant)
    run: scripts/build-cas-host-tests.sh --asan
  - name: Upload CAS host test logs
    if: always()
    uses: actions/upload-artifact@v4
    with:
      name: cas-host-test-logs
      path: /tmp/cas-host-tests/*.log
      if-no-files-found: warn
  ```
  Artifact pattern mirrors `emulator-build.yml:109-114`.
- **Gating**: both steps hard-gate PRs (exit code per §D.2.4). XFAIL rows report but never gate; XPASS gates (forces list hygiene).

## F. Triage policy for first-green-run archaeology

Per the CAS-01 ticket invariant (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:29`): no suite assertion is edited in PR-1. Every non-passing case lands in exactly one bucket, recorded in `cas_xfail.list` with a reason:

1. **Stale test expectation** (NB-3, NB-4): XFAIL in PR-1 → fixed in PR-2 as a reviewed test-only edit (each fix quotes the source line proving current behavior is intended, e.g. lazy block 0 `SymExprArena.h:97`).
2. **Real engine bug** (NB-1, NB-2): XFAIL/xfail-crash permanently until its fix ticket closes; also pinned as a corpus row so the fix flips two visible indicators.
3. **Environment-dependent** (none observed; candidate class: OmniSolver Newton tolerances on different libm, oracle spec §B risk note): XFAIL with `env` reason + issue link; resolved by pinning tolerances in a follow-up.

## G. Flakiness prevention

- No randomness in any suite (verified: no `rand`/`random_device` in `tests/*.cpp`); no time-based assertions (CalculusStressTest asserts arena **block counts** ≤ 4, `CalculusStressTest.cpp:16-26`, not wall time). Determinism threats are: (a) libm variance across runners for Newton-converged values — mitigated by GitHub's fixed runner image and, if it ever flakes, bucket F.3 (never auto-retry, per oracle spec §G flake policy); (b) cross-suite global state (ConsTable/static counters) — eliminated structurally by per-suite process isolation (§D.2.1); (c) address-dependent hash iteration order — `symCanonicalLess` falls back to hash order (`SymExpr.h:499-504`), which is derived from *values*, not pointers (`SymExpr.h:198-206`), so stable across runs.
- Single-threaded everywhere; timeout is per-suite (60 s) not per-case, and generous by 3 orders of magnitude over measured runtime.

## H. Failure artifacts

Per run, uploaded always (§E): `<suite>.log` (full stdout/stderr per suite), `summary.txt` (the `[CAS-HOST]` footer), and on crash the signal + last 30 log lines echoed into the step output. ASAN reports land in the crashing suite's log (ASAN writes to stderr).

## I. Minimal first PR (runs, changes no CAS behavior)

Exactly three files, all additive — full checklist in the execution plan:

1. `tests/host/cas_suite_main.cpp` (driver per §D.2, six suites),
2. `scripts/build-cas-host-tests.sh` (per §D.1) + `tests/host/cas_xfail.list` (initial entries per execution-plan §H),
3. `.github/workflows/emulator-build.yml` (three steps per §E).

Zero `src/**` edits ⇒ firmware and emulator binaries byte-identical; goldens untouched. Acceptance: CI green with footer `total suites=6 pass=3 xfail=3 unexpected=0` (§D.2.4), ASAN variant green with the two crashes reported as `xfail-crash`.

---

*End. PR sequencing, acceptance gates, rollback: `NUMOS_CAS_CAS01_CAS02_EXECUTION_PLAN.md`.*
