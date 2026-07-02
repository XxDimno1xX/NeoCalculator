# NumOS — CAS/Giac Integration and Boundary Specification

> Companion to `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md`. Same audit basis and markers ([V] verified `file:line`, [P] proposed, [D-n] open decision).
> Audit date: 2026-07-02 · Branch: `claude/numos-cas-spec-frmkeg` @ `a0794b0`.

---

## A. Current Giac integration (verified from source)

### A.1 What is vendored

- `lib/giac/` is a **real Giac/KhiCAS source tree**, 154 files (~6.7 MB), 41 compilable units — genuine core (`lib/giac/src/gen.h`, `global.h`, `input_parser.cc` 235 KB, `derive.h`, `desolve.h`, `series.h`, …). `library.json`: name `giac`, version `1.9.0`; build flags `-DGIAC_KHICAS -DNO_GUI -DGIAC_GENERIC -DEMBEDDED -DUSE_GMP_REPLACEMENTS -DUMAP -fexceptions -Isrc` (`lib/giac/library.json:6-14`); `srcFilter` strips calculator-hardware frontends (`:16-32`); depends on `libtommath` (`:35`). `lib/giac/src/config.h:47` `#define USE_GMP_REPLACEMENTS` routes bignums to libtommath.
- `lib/libtommath/` = libtommath 1.2.0, 171 files (`lib/libtommath/library.json:8-13`). **Used only by Giac** — no `src/` file includes a tommath header; the in-tree `cas::CASInt` uses `mbedtls_mpi` (`CASInt.h:54,145,152,449`). Any doc claiming cas:: uses libtommath is wrong.

### A.2 What compiles it

- Firmware env `esp32s3_n16r8` and its four `_validate*` descendants: Giac defines at `platformio.ini:85-92` (`-DNUMOS_USE_GIAC=1 -DGIAC_KHICAS -DNO_GUI -DGIAC_GENERIC -DEMBEDDED -UCASIO -DUMAP -fexceptions`), include paths `:76,94-96`, `lib_deps file://lib/giac` + `file://lib/libtommath` (`:109-110`), `-DDOUBLEVAL` (`:41`), exceptions/RTTI restored via `build_unflags` (`:36-37`), `build_src_filter = +<*>` (`:103`) compiles `src/math/giac/`.
- The effective runtime gate is **`ARDUINO`** at call sites plus **`NUMOS_USE_GIAC` + `BOARD_HAS_PSRAM`** for the allocator (`GiacAlloc.cpp:24`). There is no `NUMOS_ENABLE_GIAC` macro.

### A.3 Bridge files

- `src/math/giac/GiacBridge.h` — internal API: single symbol `String solveWithGiac(String input)` (`:29`).
- `src/math/giac/GiacBridge.cpp` (448 lines) — implementation:
  - `static giac::context global_context;` (`:45`) — one global context for the device lifetime (grandfathered monotonic cache, `NUMOS_MEMORY_ALLOCATION_POLICY.md:95`).
  - `initGiac()` (`:193-212`): `xcas_mode(0)`, `approx_mode(false)` (exact), `complex_mode(false)` + `complex_variables(false)` (real), `eval_level=1`, English, `withsqrt(true)`.
  - Normalizers: `applyDisplaySymbolMap` (`:79`), `mapPresentationCommandAliases` (`:108-143`), `normalizeDesolvePrimeNotation` (`:260`), `prettifyRootofIfNeeded` (`:151-191`, normal→radsimp→evalf cascade), `diagonalToVector` (`:375-388`).
  - `solveWithGiac` (`:390-448`): strip leading `:` (`:401-403`) → capture `cout/cerr/logptr` into a step buffer (`:411-414`) → `giac::gen(expr, ctx)` + `giac::eval` (`:416-417`) → print → append `[STEP_OUTPUT]` → `try/catch(std::exception)/catch(...)` → `"Error: …"` (`:441-447`). `[MEM] giac-pre/post/post-ex` probes at `:394,439,442,445`.
- `src/math/GiacBridge.h` (a **second, distinct file**) — app-side class shim `GiacBridge::evaluate()` (`:25-40`); native stub returns `"Error: Giac available only on Arduino target"` (`:38`). **Zero callers** (reliability spec `:8`).
- `src/math/giac/GiacAlloc.cpp` — **global** `operator new/delete` override (§A.6).

### A.4 What calls it / what does not

| Path | Status | Evidence |
|---|---|---|
| `SerialBridge::processChar` `:`-prefixed line → `solveWithGiac` | **LIVE (the only path)** | `src/input/SerialBridge.cpp:148-160` (call at `:155`) |
| `MathEvaluator::evaluateWithGiac` | dead — zero callers | `MathEvaluator.cpp:509-513`; `MathEvaluator.h:99` |
| `GiacBridge` class shim | dead — zero callers | `src/math/GiacBridge.h:25-40` |
| CalculationApp | never — uses `_evaluator.evaluate` only | `CalculationApp.cpp:529` |
| EquationsApp / CalculusApp / IntegralApp / TutorApp | never — in-tree cas:: only | `EquationsApp.h:39-51`; `CalculusApp.h:54-60` |
| GrapherApp / GraphModel | never — RPN pipeline | `GraphModel.h:170-173` |

Note: `SerialBridge` lives in `src/input/`, not `src/hal/` (the task brief's `src/hal/SerialBridge.*` path does not exist).

### A.5 SerialBridge protocol facts [V]

Line-buffered `static std::string`, CR/LF-terminated, 50 ms CR+LF debounce (`SerialBridge.cpp:110-133`); **input cap 255 chars, silently dropped beyond** (`:116`); keyword commands HOME/AC/DEL/ENTER/EXE/F1/F2 (`:165-171`); single-char key map (`:174-233`); event ring `BUF_SIZE=32`, overflow drops (`SerialBridge.h:63`; `SerialBridge.cpp:74-75`). SerialBridge is **not compiled into the emulator** (absent from the whitelist, `platformio.ini:195-266`; diagnostics spec `:230`).

### A.6 Memory relationships [V]

- `GiacAlloc.cpp` overrides the **entire process's** `operator new/new[]/delete` (all C++17 variants incl. aligned/sized/nothrow, `:42-143`), enabled by `NUMOS_USE_GIAC && ARDUINO && BOARD_HAS_PSRAM` (`:24`): `heap_caps_malloc(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)` with fallback to `MALLOC_CAP_8BIT` (may take internal RAM) (`:27-32`); throwing forms `throw std::bad_alloc()` on total exhaustion (`:44,55`); `delete` uses plain `free()` (`:48-51`, ESP-IDF-only-correct, `NUMOS_MEMORY_ALLOCATION_POLICY.md:66`).
- Ratified policy: all C++ `new` PSRAM-first (`NUMOS_MEMORY_ALLOCATION_POLICY.md:5`); Giac working set "unbounded; must never fall on internal first" (`:26`) — the `MALLOC_CAP_8BIT` fallback violates the spirit under pressure (audit finding, `NUMOS_MEMORY_AND_PSRAM_AUDIT.md:142`).
- PSRAM relationship: Giac shares the 8 MB PSRAM with LVGL >512 B allocs, MathAST, cas:: arenas, sim apps (`NUMOS_MEMORY_AND_PSRAM_AUDIT.md:52`). Giac `bad_alloc` is caught at the bridge (`GiacBridge.cpp:441-447`) but transient pressure pushes *other* allocators into internal-RAM fallback (risk C1.3/C4.2).

### A.7 Execution/timeout facts [V]

- Runs synchronously on the shared 64 KB `loopTask` stack (`platformio.ini:44`; comment `GiacBridge.cpp:391-392`). No task isolation (only FractalApp spawns a task, `FractalApp.cpp:490`).
- **No timeout, no interrupt**: `caseval_maxtime` machinery compiled out; nothing sets `giac::ctrl_c`; Giac's own recursion cap `MAX_RECURSION_LEVEL=100` (`lib/giac/src/kglobal.cc:1679`) throws too late for the shared stack (reliability spec C4.1, `RELIABILITY…SPEC.md:123`). A `:factor(<hard input>)` freezes the device indefinitely; no watchdog is armed in `src/` [V per C4.1].

### A.8 Firmware vs emulator [V]

`emulator_pc` excludes Giac three ways: `lib_deps` omission with rationale comment ("`gen.h: 'SIZEOF_INT' not declared`" — native build known-broken, `platformio.ini:177-185`), `lib_ignore = giac, libtommath` (`:192-194`), and the allow-list `build_src_filter` never including `src/math/giac/` (`:195-266`, note `:189-191`). The whitelisted cas:: subset (7 files, `:251-258`) **is** in the emulator. Native `solveWithGiac` is never linked; the shim string J.10 is the only native surface.

---

## B. Giac boundary contract [P]

This section is normative for any future code that routes app traffic to Giac (decisions D-4/H.4 in Document 1) and for hardening the existing serial path. It extends, and must stay consistent with, reliability-spec requirements H-2, M-2 and RT-09 (`RELIABILITY…SPEC.md:217,224-227`; `DIAGNOSTICS…SPEC.md` §2.6-2.7).

### B.1 Answer routing

| Situation | Engine | Rationale |
|---|---|---|
| Anything ExactVal-representable (rational arithmetic, single radical, π/e monomials, special angles) | VPAM MathEvaluator | already CI-gated; deterministic; no Giac risk |
| Edu-steps, polynomial solve ≤ cubic, 2×2/3×3 systems, diff of supported forms, table/u-sub/parts integrals | in-tree cas:: | test-first per CAS-01..CAS-09 before any routing change |
| Free-form console math; anything both native engines refuse (factorization, desolve, series, big factorials) | Giac (firmware only) | only after B.2-B.6 guards exist |
| Giac must NOT be called | (a) when preflight fails (B.3); (b) input exceeds B.4 caps; (c) from any ISR/timer context; (d) re-entrantly (single global context, `GiacBridge.cpp:45`); (e) on native builds (link-level impossible today — keep it that way, `platformio.ini:192-194`) | |

### B.2 Timeout budget

Adopt H-2 verbatim: cancellation deadline **10 s** enforced by an `esp_timer` callback setting `giac::ctrl_c`/`interrupted`; expiry surfaces `Error: timeout` through the existing catch path (`GiacBridge.cpp:441-447`); ctrl_c cleared after; context assumed reusable **pending hardware verification** — if interrupt poisons the global context, the contract escalates to "Giac disabled until reboot" + `[GIAC] disabled` log (reliability spec F.3, `:323`). Hardware acceptance = reliability injection test I-H5 (`:435`).

### B.3 Memory preflight

Adopt M-2: refuse the query unless PSRAM **largest free block ≥ 512 KB** (`heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)`); refusal message `Error: low memory`, log `[GIAC] refused reason=mem`, no evaluation attempted (reliability I-H6, `:436`). Post-query, the existing `[MEM] giac-post` probe (`GiacBridge.cpp:439`) records the delta; per-query high-water via `heap_caps_get_minimum_free_size` is the RT-09 measurement plan (`NUMOS_MEMORY_AND_PSRAM_AUDIT.md:158`).

### B.4 Size limits

- **Input ≤ 2048 chars** checked at bridge entry before `gen()` (H-2). Today the SerialBridge 255-char buffer (`SerialBridge.cpp:116`) is the accidental limiter — the bridge check must exist independently so future callers are bounded.
- **Output ≤ 8192 chars [P]**: result string longer than 8 KB is truncated with a `…[truncated N chars]` tail and `[GIAC] output-cap` log. Rationale: `print(ctx)` on huge results (e.g. `2^100000`) can allocate multi-MB Strings on the loop stack path; 8 KB covers every legitimate calculator display and serial console line.
- **Step buffer** (`:411-414` capture) shares the output cap; overflow drops oldest.
- **Recursion**: Giac-internal cap 100 stays; the stack budget (B.5) is the real constraint.

### B.5 Stack budget

Giac evaluation must fit in the 64 KB loopTask stack minus caller overhead. [P] Instrument `uxTaskGetStackHighWaterMark` before/after `solveWithGiac` (audit measurement plan, `NUMOS_MEMORY_AND_PSRAM_AUDIT.md:160`); alarm threshold: high-water < 8 KB remaining → `[GIAC] stack-low water=<N>` and refuse further queries until reboot. **[D-5]** Dedicated Giac task with larger stack: rejected by the reliability spec for now (context not re-entrant/killable, `RELIABILITY…SPEC.md:397`); revisit only with hardware evidence that ctrl_c cancellation is insufficient.

### B.6 Cancellation, error mapping, logging

- Cancellation: cooperative via ctrl_c only (B.2). No task kill.
- Error mapping to the Document-1 §J model: parse failure inside `gen()` → J.1 (`Error: <giac what()>`), timeout → J.6, `bad_alloc` → J.7, other exceptions → J.11 with `what()` text. All pass through the single catch site (`GiacBridge.cpp:441-447`) — keep it the only exit.
- Logging: RT-09 `[GIAC]` line per query: `status ∈ ok|timeout|oom|error|refused`, `ms`, input length (never the input text in release builds — privacy rule, `DIAGNOSTICS…SPEC.md:239`).

### B.7 Determinism

[P] Contract: same input + same context state ⇒ byte-identical output. Giac in exact real mode is deterministic per query, but the **global context accretes state** (symbol assignments via `:a:=5`, caches) — so cross-query determinism holds only per session prefix. Consequences for testing: the golden harness (F below) must (a) run queries in a fixed order from a fresh boot, or (b) reset assigned symbols between cases (`purge(a)`), and must treat any output containing addresses/timing as a bug. Known nondeterminism sources to watch on hardware: rootof prettify fallback to `evalf` (`GiacBridge.cpp:151-191`) yields float text whose formatting must be pinned by goldens.

---

## C. Input/output normalization

### C.1 Current [V]

There is **no AST→Giac or Giac→AST converter anywhere in the tree**. The bridge is string→string: serial text in, display text out (`SerialBridge.cpp:155-157` prints raw). The alias map rewrites presentation-command names (`mapPresentationCommandAliases`, `GiacBridge.cpp:108-143`); display-symbol substitution post-processes output (`applyDisplaySymbolMap`, `:79`); rootof prettify and diagonal→vector normalize specific result shapes (`:151-191,375-388`).

### C.2 AST → Giac string [P] (prerequisite for any app routing; ticket CAS-04 depends)

Serializer from VPAM MathAST with these rules:
- Explicit `*` for every implicit multiplication (Giac's parser has its own juxtaposition rules; never rely on them).
- Fractions → `(num)/(den)` fully parenthesized; Power → `(base)^(exp)`; Root degree n → `surd(x,n)` (NOT `x^(1/n)` — preserves D-3 odd-root semantics); LogBase → `log(x,b)` Giac form; PeriodicDecimal → exact fraction; DefIntegral → `integrate(f,x,a,b)`; Summation → `sum(f,k,a,b)`.
- Reject (refuse to serialize): Empty nodes, PlusMinus, CoeffAssign, any node whose serialization is not in this table. Refusal = J.2, never a best-effort string.
- Length check per B.4 **after** serialization.

### C.3 Giac string → AST / display [P]

Giac output is parsed into a *restricted result grammar*, not general Giac syntax: signed rationals, decimals, `sqrt(n)`/`n^(m/k)`, `pi`, `e`/`exp(1)`, `i` (rejected in real mode), lists `[a,b,…]`, equations `x=…`, and symbolic residue (unparsed tail). Parser rules:
- Exact result: fraction/radical forms map to ExactVal when representable; otherwise keep as display string tagged symbolic (no ExactVal round-trip through double — that would violate NC-1).
- Approximate result: any output containing `.` or `e±NN` maps to approximate-with-marker.
- Lists/vectors: solution lists map to the H.2 ordered-solutions contract; `diagonalToVector` already canonicalizes one matrix shape (`GiacBridge.cpp:375-388`).
- Equations: `x=3` forms split on the top-level `=`.
- Units: none exist in NumOS input; any unit token in output (from a stray Giac command) → unsupported-output error J.2.
- Unsupported outputs (matrices beyond the vector case, `rootof(…)` that survived prettify, `desolve` structures): display raw string in console context; **refuse** to import into app context (error J.2). Never partially parse.
- Hard rule: the output parser must be total — every possible byte string terminates in accept/refuse without UB (fuzz target, §F.6; ticket CAS-14).

### C.4 Complex numbers

Real mode: an output containing `i` outside an identifier means the mode contract broke → J.11 internal (corpus row GIAC-ADV-*). If D-2(c) later enables per-query complex mode, `a+b*i` parses to the display-only complex form.

---

## D. Security / reliability (local-only, still enforced)

1. **Expression size limits**: B.4 input/output caps; SerialBridge 255-char line cap stays as defense-in-depth (`SerialBridge.cpp:116`).
2. **Recursion limits**: Giac internal 100; NumOS-side serializer/parser depth cap 64 (matches J.8).
3. **No unbounded string growth**: output/step caps (B.4); the capture buffer (`GiacBridge.cpp:411-414`) capped.
4. **No uncontrolled allocation**: B.3 preflight + `bad_alloc` catch; bulk-block internal-fallback prohibition is policy M-3/MT-05 (`NUMOS_MEMORY_ALLOCATION_POLICY.md:42`).
5. **Watchdog policy**: per reliability H-1 — TWDT fed only from `loop()`; never fed inside Giac to mask a hang (`RELIABILITY…SPEC.md:224,395`).
6. **Crash containment**: the bridge try/catch is the only exit (B.6); exceptions must not escape into LVGL/app code (nothing else catches, reliability spec `:42`).
7. **Safe failure**: every refusal/error leaves UI + serial responsive; follow-up `:1+1` must return `2` (reliability I-H5 acceptance, `:435`).
8. **Serial command restrictions [P]**: the `:` console remains the only Giac surface; commands that mutate persistent state through Giac (file ops — Giac has none compiled in KhiCAS/NO_GUI mode, but e.g. `:a:=…` mutates the context) are permitted but session-scoped; **[D-6]** whether to blocklist context-mutating commands (`assume`, `:=`, `purge`) in release builds — options: (a) allow (power-user console), (b) block and require a fresh context per query. Recommendation: (a) with the B.7 testing consequence documented; (b) costs context-rebuild time that the reliability spec calls expensive (`:397`).
9. Safe mode never initializes Giac (reliability R-5, `:237`).

---

## E. Firmware/emulator divergence

| Concern | Emulator | Firmware | Test consequence |
|---|---|---|---|
| Giac | absent (link-excluded) | linked | Giac behavior testable **only** on hardware or a future host-Giac differential rig (§F.7) |
| Giac error string | `"Error: Giac available only on Arduino target"` (`src/math/GiacBridge.h:38`) | live results | corpus GIAC-ENV-1 pins the native string |
| cas:: BigInt | `CAS_HAS_BIGINT=0` → overflow errors (`CASInt.h:53-59,216-218`) | mbedtls promotion (with aliasing bug A.4-1) | host tests CANNOT validate BigInt promotion/demotion; `BigIntTest.cpp` promotion cases are firmware-only |
| Allocator | libc malloc; no GiacAlloc override, no heap_caps (`NUMOS_MEMORY_AND_PSRAM_AUDIT.md:163`) | PSRAM-first global new | OOM behavior not host-testable; arena-lifetime logic IS (ASAN, policy MT-09) |
| SerialBridge | not compiled (`platformio.ini:195-266`) | live | `:`-console tests are hardware-only; emulator gets DiagConsole via planned `.numos` `diag` command (diagnostics spec `:230`) |
| cas:: subset | 7 files whitelisted (`platformio.ini:251-258`) | all 60 | host testing of solvers/diff/integrate requires a separate g++ target (CAS-01), not the emulator build |

**What can be validated on host**: all cas:: algebra/simplify/diff/integrate/solver logic within int64 range; ExactVal semantics; converters; parser hardening with a mocked Giac-output corpus. **What must never be mocked**: Giac's actual output text (mock only the *transport*, feed recorded real outputs); heap_caps domain routing; stack high-water; timing.

**What requires hardware**: Giac correctness/timeout/memory (F.1-F.5), BigInt promotion paths, PSRAM exhaustion behavior, ctrl_c context-health verification (B.2).

---

## F. Giac test matrix

All rows execute over serial on a `esp32s3_n16r8_validate` build via a host-driver script (ticket CAS-15); pass/fail is parsed from the `: => ` response lines (`SerialBridge.cpp:156-157`). Corpus schema/rows in `NUMOS_CAS_TEST_ORACLE_AND_CORPUS_SPEC.md` (§D-E, categories GIAC-*).

### F.1 Smoke (≥10 rows)
`:1+1` → `2`; `:0.1+0.2` exact mode → `3/10`; `:sqrt(8)` → `2*sqrt(2)`; `:pi` stays symbolic; follow-up query health after each error row.

### F.2 Correctness goldens (≥50 rows — ground-truth P-02)
factor/expand/solve/diff/integrate/desolve/series canonical pairs, e.g. `:factor(x^2-1)` → `(x-1)*(x+1)`; `:solve(x^2-5x+6=0,x)` → `list[2,3]`; `:integrate(1/x,x)` → `ln(abs(x))` (pin actual output text on hardware first — goldens are *recorded*, human-reviewed, then frozen, same policy as PPM goldens).

### F.3 Adversarial (≥15 rows)
`:factor(10^60+7)` (timeout path); `:sum(1/k^2,k,1,inf)` (symbolic infinity handling); deep parens `:((((…1…))))` ×200; `:x^(1/3)` at negatives; unicode/π glyph input; empty `:`; `:` + 2048 chars (cap boundary), 2049 (refusal); output-cap probe `:2^100000`.

### F.4 Memory stress
Repeated heavy queries watching `[MEM] giac-pre/post` deltas (`GiacBridge.cpp:394,439`): 20× `:factor(2^128-1)`; assert post-minus-pre stabilizes (no monotonic leak beyond context warm-up) and largest-block recovery; preflight refusal under a scratch PSRAM pin (reliability I-H6).

### F.5 Timeout stress
With H-2 implemented: measured-slow input must return `Error: timeout` ≤ 10.5 s with heartbeat continuity and healthy follow-up (I-H5). Until H-2 lands, these rows are **expected-hang documentation** and must NOT run unattended.

### F.6 Output parser stress (host-side, ticket CAS-14)
Fuzz the C.3 parser with: recorded real Giac outputs, mutations (truncations, sign flips, nested `rootof`), random bytes, 1 MB strings. Oracle: total-parser property (accept/refuse, no crash, no allocation blow-up beyond input size ×4).

### F.7 Differential vs host Giac [D-7]
Options: (a) build desktop Giac (distro package `giac`/`xcas`) in CI and compare normalized outputs for the F.2 corpus — catches KhiCAS-build divergence but risks version skew (host giac ≠ 1.9.0 KhiCAS); (b) skip, rely on recorded-golden discipline. Recommendation: (a) as a **non-gating advisory job** with normalization (strip whitespace, canonical function names, resimplify both sides through the same normalizer) and a curated ignore-list for known formatting skew; promote to gating only if skew proves manageable. Acceptable differences: term order, equivalent radical forms (both sides re-checked by numeric sampling O4); unacceptable: numeric value mismatch, solution-set mismatch.

---

*End of Document 2.*
