# NumOS — Emulator CI Artifact & Golden Policy Specification

> **What "done" means:** every CI failure ships the evidence needed to diagnose it without a local repro (logs, screenshots, visual diffs, sandbox state); every golden/mask/candidate has an explicit lifecycle state; masks stay a single-purpose clock exception until the clock is deterministic, then retire; and CI proves the tree stays clean. Companion docs: `NUMOS_EMULATOR_DETERMINISM_AND_FIXTURE_HYGIENE_SPEC.md` (EMUDET), `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` (SANDBOX), `NUMOS_EMULATOR_HOST_PORTABILITY_SPEC.md` (PORT), `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md` (EMUROAD). Golden governance builds on `tests/emulator/golden/README.md` and `tests/emulator/masks/README.md` (both authoritative today) and on SCQA §D/§G; it must not contradict them.
>
> Audit base: branch `claude/numos-emulator-determinism-specs-69nso8` (2026-07-04). **[V]** verified `file:line` · **[P]** proposed. SPEC-ONLY.

---

## A. Current CI flow [V]

Two workflows, deliberately separate (`emulator-build.yml:3-12`):

- **`compile-and-release.yml`** — firmware only: sed-strips the Windows `build_dir` (`compile-and-release.yml:29-39`), sets `PLATFORMIO_BUILD_DIR=.pio/build` (`:15-19`), builds `esp32s3_n16r8` (`:51-53`), uploads a renamed `firmware.bin` as an auto-release on every `main` push (`:94-103`). Executes nothing; no emulator involvement.
- **`emulator-build.yml`** — one Linux job (`ubuntu-latest`, `:50`), `PLATFORMIO_BUILD_DIR: .pio/build` job env (`:54-55`), triggered by paths covering `src/**`, `tests/emulator/**`, the comparator/generator/guard scripts, and itself (`:14-43`; `promote-emulator-golden.py` deliberately excluded, `:24-27`). Steps in order:

| # | Step | Gate? | Artifacts |
|---|---|---|---|
| 1 | apt SDL2 + PlatformIO + `check-emulator-deps.py` (`:61-78`) | fail on deps | — |
| 2 | KeyCode digit guard: static scan `--selftest` + scan + host unit test (`:96-104`) | fail | — |
| 3 | `pio run -e emulator_pc` (`:106-107`) | fail | binary `numos-emulator-linux` (`:109-114`) |
| 4 | Wall-clock boot smoke `--run-for-ms 5000`, grep `Launcher cargado` (`:132-141`) | fail | `emu-smoke.log` (`:143-149`) |
| 5 | Deterministic smoke `--frames 600` + screenshot header check (`:162-177`) | fail | log+`smoke.ppm` (`:179-187`) |
| 6 | Scripted replay: launcher + calc_1_plus_2; **run-twice SHA-256 self-diff for calc_1_plus_2 only** (`:201-237`) | fail | logs+PPMs (`:239-249`) |
| 7 | Semantic asserts 4B-C: two fixtures, exit-0 + PASS-marker + no-FAIL grep (`:260-286`) | fail | logs (`:288-296`) |
| 8 | Calc semantic suite 8C: glob `calc_semantic_*` + error/store fixtures — **includes the STO write** (`:308-356`, store at `:343`) | fail | `out/semantic_8c/` logs (`:358-364`) |
| 9 | Input parity 9A (3 scripts) (`:375-408`) | fail | logs (`:410-416`) |
| 10 | Menu parity 9B (2 scripts) (`:429-461`) | fail | logs (`:463-469`) |
| 11 | Grapher no-hang guards 9F (6 scripts; some write screenshots under `out/`, uploaded never gated, `:471-531`) | fail | logs (`:533-539`) |
| 12 | Candidate generation: 25 stems, deterministic+headless (`:558-564`; list `generate-emulator-candidates.py:39-143`) | fail on generation error | `numos-emulator-candidates` (`:567-573`) |
| 13 | Golden compare: per candidate, masked if `masks/<stem>.mask` exists; **missing golden ⇒ warning; mismatch ⇒ fail** (`:575-607`) | fail on mismatch only | none beyond step-12 upload |

Per-script contract everywhere: `--headless --deterministic --frames N --quiet`, 60 s `timeout`, exit 0 + `PASS -` present + `FAIL -` absent (e.g. `:314-336`).

## B. Current golden/mask inventory [V]

- **Goldens (19 + README):** launcher_smoke, calc_1_plus_2, calc_fraction_sum, settings_smoke, math_showcase_smoke, statistics_smoke, statistics_data_smoke, probability_smoke, probability_edit_smoke, sequences_smoke, sequences_edit_smoke, regression_smoke, regression_data_smoke, grapher_smoke, grapher_expr_smoke, grapher_graph_smoke, grapher_table_smoke, grapher_trace_smoke, grapher_templates_smoke [V `ls tests/emulator/golden/`].
- **Masks (18 + README):** one per golden except `launcher_smoke` (static bar, no clock — `MainMenu.cpp:321-356`); every mask is the identical single clock rect `4,6,37,13` (`tests/emulator/masks/README.md:36-44`).
- **Candidates without goldens (6):** menu_focus_grapher_smoke, grapher_implicit_x_smoke, grapher_implicit_2x_smoke, grapher_multifn_graph_smoke, grapher_multifn_table_smoke, grapher_expr_scroll_smoke — generated (`generate-emulator-candidates.py:121-143`), uploaded, compare-warned (`emulator-build.yml:602-604`).
- **Scripts:** 71 `.numos` (`ls tests/emulator/scripts | wc -l`); ~46 run as assert-only CI gates (steps 7–11), 25 feed the visual pipeline, some overlap.
- **Comparator:** `compare-ppm.py` — dependency-free P6 parse (`:82-143`), rect masks with clipping (`:194-218`), metrics + bbox (`:221-260`), optional `--write-diff` magenta/teal/grayscale diff image (`:263-294`), exit 0/1/2 (`:33-37`). **CI never passes `--write-diff`** (`emulator-build.yml:589,593`).

## C. Warning-only candidates policy

**Current [V]:** absent golden ⇒ `::warning`, candidate uploaded, job green (`emulator-build.yml:602-604`). This is the designed anti-ossification property — CI can never bless an unreviewed image (`tests/emulator/golden/README.md:124-127`).

**Gap:** warn-forever is unbounded. The 6 stems above have been warning since their phases landed; nothing distinguishes "review pending" from "abandoned".

**Policy [P → CI-05]:**
1. Warning-only is legitimate **only** as a transition state. Every candidate stem must be, at any time, one of: *gated* (golden exists), *pending* (listed in a committed `tests/emulator/golden/PENDING.md` with owner + reason), or *assert-only* (deliberately never gated visually — e.g. the 9F no-hang scripts, `emulator-build.yml:477-480`).
2. CI emits one summary line: `N gated, M pending, K unexpected`. `K > 0` (a candidate neither gated nor listed pending) fails the job — that's the "forgotten" detector, and it is cheap (a list diff).
3. Promotion out of pending remains human-only via `promote-emulator-golden.py` (`:6-12`), unchanged.

## D. Golden promotion policy

**Current [V], kept verbatim:** candidates generated deterministically on the reference OS (`tests/emulator/golden/README.md:33-39`); human visually confirms semantics; `promote-emulator-golden.py <stem>` does a validated byte-exact copy, refuses overwrite without `--force` (`promote-emulator-golden.py:147-152`), never commits (`:238-244`); the git commit is the acceptance record; CI never runs the tool (`emulator-build.yml:24-27`).

**Additions [P → CI-05]:**
1. **Re-bless waves, not dribbles:** any change that alters shared chrome pixels (SB-01/02/03/06) invalidates all 19 goldens at once; re-bless in one coordinated PR (inherited from SCQA §D blessing rule).
2. **Provenance line:** each promotion commit message records generator binary provenance — commit SHA of the tree that built the emulator + `sha256` of the golden (the tool already prints the sha, `promote-emulator-golden.py:171-173`; the requirement is to paste it).
3. **Post-EMUDET-01 rule:** a *new* golden may not be committed with a clock mask; the clock must be deterministic first. Existing masked goldens re-bless mask-free opportunistically (SBAR SB-03 path).

## E. Mask policy

**Current [V]:** masks are hand-authored, human-committed, single-purpose: the wall-clock `HH:MM` rect `4,6,37,13`, justified per file with reproduced-jitter evidence (`tests/emulator/masks/README.md:88-98`; e.g. `masks/calc_1_plus_2.mask` header). Comparator unions `--ignore-rect` flags and `--mask-file` lines (`compare-ppm.py:324-335`).

**Policy [P → CI-05], formalizing the READMEs into checkable rules:**
1. **Clock-only rect.** Until EMUDET-01, the only permissible mask content is the shared clock rect (optionally per-golden justified sub-rects of it). After EMUDET-01, no new masks at all; existing ones retire with their goldens' next re-bless.
2. **Forbidden: broad/body masks.** A CI lint computes each mask's ignored-pixel count (the comparator already reports it, `compare-ppm.py:354`): any mask ignoring > 1 % of the frame (768 px; the clock rect is 481 px) or intersecting the app body region (y ≥ 24, below the StatusBar separator, `StatusBar.cpp:114-122`) fails the job. This mechanizes the README's "keep masks tight / reject large masks" reviewer rules (`masks/README.md:93-98,100-106`).
3. **Negative controls.** CI proves masks and the comparator still *bite*: (a) compare a golden against a copy with one body pixel flipped → must exit 1; (b) compare a golden against itself with its mask → must exit 0; (c) feed a malformed mask line → must exit 2 (`compare-ppm.py:33-37` codes). Today the comparator's correctness is assumed, never exercised in CI [V — no such step in `emulator-build.yml`].
4. **When masks can change:** only in a commit that also touches the corresponding golden or cites a reproduced jitter diff; mask-only diffs otherwise fail review by convention (and the §E.2 lint bounds the blast radius).

## F. Artifact policy [P → CI-03; current state cited]

| Artifact | Today [V] | Contract [P] |
|---|---|---|
| Candidates | uploaded always (`emulator-build.yml:567-573`) | unchanged; retention 90 d default |
| Logs | per-suite uploads, `if: always()` (`:143-149,179-187,239-249,288-296,358-364,410-416,463-469,533-539`) | unchanged; add the deterministic banner line (EMUDET AT-DET-5) to every log |
| Screenshots (script-written, non-gating) | 9F images under `out/` uploaded with logs (`:494,533-539`) | unchanged |
| Diff images | **never produced** (`--write-diff` unused, `:589,593`) | on every golden mismatch: `compare-ppm.py … --write-diff out/diffs/<stem>.diff.ppm`, upload `out/diffs/` in a `numos-emulator-golden-diffs` bundle |
| Self-diff reports | one fixture's run-twice SHA check, logs only (`:228-237`) | full-suite self-diff phase (§J.2) writes a machine-readable `out/selfdiff/report.txt` (stem, sha1, sha2, verdict), uploaded always |
| Assert-failure screenshots | none — a failed assert exits 4 with text only (`NativeHal.cpp:1682-1688`) | emulator dumps `<stem>.assertfail.ppm` (final framebuffer via the existing `saveScreenshotPPM`, `NativeHal.cpp:1292-1315`) into the run's `out/` next to the log before exiting 4 (= SCQA §G.6 / core-QA-05, one implementation); runner uploads it |
| Failure bundles | ad-hoc per step | one `numos-emulator-failure-<step>` bundle per red step: log + assert screenshot + retained sandbox (SANDBOX §B.2) + diff PPMs |

## G. Missing artifact gaps (the delta F closes) [V]

1. **Visual diff PNGs/PPMs:** a golden mismatch today reports counts and a bbox in the log (`compare-ppm.py:385-391`) but reviewers must regenerate locally to *see* it.
2. **Assert-failure screenshots:** exit-4 failures ship no pixels; for layout-dependent failures (e.g. Grapher tab asserts) the log alone under-determines the cause.
3. **Negative-control logs:** no CI record that the comparator/masks/gates can still fail (E.3).
4. **Deterministic self-diff outputs:** only `calc_1_plus_2` is self-diffed (`emulator-build.yml:228-237`); a nondeterminism regression in any other screen surfaces — if at all — as a confusing golden mismatch at a different wall minute.

## H. CI dirty-tree guard [P → CI-01]

Final step of the emulator job, `if: always()`:

```
- name: Working tree must be clean after all suites
  if: always()
  run: |
    git diff --exit-code                      # tracked modifications (vars.dat class)
    UNTRACKED=$(git status --porcelain | grep -v '^?? out/' || true)
    test -z "$UNTRACKED" || { echo "$UNTRACKED"; echo "::error::suite created untracked files outside out/"; exit 1; }
```

Scope notes: today this step would fail on `emulator_data/vars.dat` after step 8 (`emulator-build.yml:343` runs the STO script; write path `CalculationApp.cpp:900`) — that is the point; it lands together with SANDBOX FIX-01/FIX-03 so it lands green. It permanently guards: runtime artifacts leaking outside `out/`, `emulator_data` mutations, any generated file a build step drops into tracked paths (e.g. a stray `C:/` dir if `PLATFORMIO_BUILD_DIR` were ever lost, `platformio.ini:6` + `emulator-build.yml:54-55`). The `out/`-exclusion keeps the intended artifact area free.

## I. Cross-platform drift (what the golden gate does and does not pin) [V+P]

| Source | Exposure | Position |
|---|---|---|
| Fonts | none at runtime — STIX subsets are compiled C arrays (`platformio.ini:264-266`), Montserrat ships inside the pinned LVGL source; no host font stack is consulted | goldens portable across OSes w.r.t. fonts |
| libm | Grapher curves/table values go through libc `double` math (legacy `Evaluator`, `platformio.ini:236-240`); glibc vs Apple libm vs mingw can differ in last-ulp rounding → single-pixel curve drift is *possible* | goldens are **Linux-reference only** (`tests/emulator/golden/README.md:33-35`); cross-OS byte-equality is explicitly *not* a contract until measured (PORT-05 runs the measurement) |
| SDL | none for pixels (CPU buffer capture, `NativeHal.cpp:1286-1315`); SDL version affects only windowing/input | no golden exposure |
| Compiler | gcc vs clang vs mingw optimization of FP expressions (fma contraction) — same class as libm | same position as libm; if drift is measured, options are per-OS goldens (rejected: triple review burden) or `-ffp-contract=off` on the emulator env (preferred candidate, decision in PORT-05) |
| Time | wall clock leaks via the StatusBar (EMUDET §B.5) — the one *proven* cross-anything drift | EMUDET-01 removes it |
| Filesystem path | CWD-relative `emulator_data`/`out` (`FileSystem.cpp:38`; `NativeHal.cpp:1294`) and `C:/.piobuild` (`platformio.ini:6`) | SANDBOX B.1 + PORT §D |
| LVGL version float | `lvgl@^9.2.0` (`platformio.ini:107,178`) resolves per-machine at install time — two hosts can legitimately render differently (risk REN-4) | pin exact version = PORT/CI shared ticket CI-06; until then, goldens implicitly pin "whatever CI's lockfile resolved" |

## J. Proposed CI phases [P → CI-02…CI-06]

Restructure the single job's tail into named phases (same job, explicit ordering; steps 1–3 unchanged):

1. **Phase A — assert-only:** current steps 4–11 unchanged (wall-clock boot smoke stays, it is intentionally wall-clock, `emulator-build.yml:127-131`). Add: assert-failure screenshot capture wiring (F).
2. **Phase B — deterministic self-diff:** run the full candidate list **twice** into `out/run1|run2`, byte-compare unmasked, write `out/selfdiff/report.txt`; any mismatch fails with both PPMs + a `--write-diff` image uploaded. Subsumes the single-fixture check (`:228-237`). Post-EMUDET-01 this is the strongest determinism gate (SCQA §F "self-diff without clock mask").
3. **Phase C — golden compare:** current step 13 + `--write-diff out/diffs/<stem>.diff.ppm` on mismatch + the §C pending-inventory summary + the §E.2 mask lint.
4. **Phase D — negative controls:** the three §E.3 comparator/mask bite-tests + one deliberate assert-failure run (a 2-line script asserting a wrong result must exit 4 and produce the failure screenshot) — proving the whole gate chain can fail.
5. **Phase E — artifact upload + hygiene:** failure bundles (F), dirty-tree guard (H), both `if: always()`.

Runtime budget: Phase B doubles candidate generation (~25 short deterministic runs, each bounded by `--frames ≤ 1400` and no sleep in deterministic mode, `NativeHal.cpp:2103-2108`) — acceptable on ubuntu-latest; measure and, if needed, self-diff a rotating subset daily + full on `main`.

## K. Implementation tickets (headline — full fields in EMUROAD)

- **CI-01** — Dirty-tree guard step (H) — lands with SANDBOX FIX-01/03.
- **CI-02** — Deterministic self-diff phase for all candidates + report artifact (J.2).
- **CI-03** — Mismatch visual-diff artifacts + failure bundles (F).
- **CI-04** — Assert-failure screenshot capture in NativeHal + runner upload (F; = core-QA-05).
- **CI-05** — Golden/mask governance mechanization: pending inventory, mask lint, negative controls (C, D, E, J.4).
- **CI-06** — Pin exact LVGL version for both envs (I; risk REN-4) — one-line `platformio.ini` change + full re-candidate/verify wave.

*End of CI artifact & golden policy spec.*
