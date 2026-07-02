# NumOS — Grapher Phase-10 CI Wiring Plan (GR-00)

> Script-by-script disposition of every `grapher_*.numos` fixture, plus the exact CI step, policies, and transition plan.
> Audit base: `ce1b725`, 2026-07-02. Verified against `.github/workflows/emulator-build.yml` (Grapher gates only at `:407` and `:525-530`), `scripts/generate-emulator-candidates.py` (`CANDIDATES`, `:39-143`), `tests/emulator/golden/` (6 Grapher goldens), `tests/emulator/masks/` (6 matching masks), and all 37 `grapher_*`/`menu_focus_grapher*` scripts under `tests/emulator/scripts/`.

---

## A. Full script inventory and classification

Status legend — **G9F**: gated by the Phase-9F step (`emulator-build.yml:525-530`); **G9A**: gated by input-parity (`:405-407`); **CAND**: in `CANDIDATES` (line cited); **GOLD**: blessed golden+mask exist; **ORPH**: tier-T4 orphan (screenshot script run by nothing in CI).

| # | Script | Today | Disposition (this batch) |
|---|---|---|---|
| 1 | `grapher_smoke` | CAND(:82)+GOLD | keep — already visual-gated |
| 2 | `grapher_expr_smoke` | CAND(:88)+GOLD | keep |
| 3 | `grapher_graph_smoke` | CAND(:96)+GOLD | keep |
| 4 | `grapher_table_smoke` | CAND(:104)+GOLD | keep |
| 5 | `grapher_trace_smoke` | CAND(:112)+GOLD | keep |
| 6 | `grapher_templates_smoke` | CAND(:120)+GOLD | keep |
| 7 | `menu_focus_grapher_smoke` | CAND(:129), no golden | keep warning-only (launcher visual; blessing is a launcher-team decision, out of scope) |
| 8 | `grapher_implicit_x_smoke` | CAND(:138), no golden | keep; **also add to the new gate step** (assert-only cost is one more run; closes the "candidate generation is not a hang gate" gap for bare-expr) → bless post-PR-1 |
| 9 | `grapher_implicit_2x_smoke` | CAND(:139), no golden | idem #8 |
| 10 | `grapher_multifn_graph_smoke` | CAND(:140), no golden | keep warning-only; bless post-PR-1 (stable, covered by TEST §D "expr_row/table_multifn") |
| 11 | `grapher_multifn_table_smoke` | CAND(:141), no golden | idem |
| 12 | `grapher_expr_scroll_smoke` | CAND(:142), no golden | keep warning-only (scroll frame is stable; low priority to bless) |
| 13 | `grapher_home_return_smoke` | G9F | keep — untouched |
| 14 | `grapher_tan_exit_smoke` | G9F | keep |
| 15 | `grapher_functions_smoke` | G9F | keep |
| 16 | `grapher_logbase_smoke` | G9F | keep |
| 17 | `grapher_template_insert_smoke` | G9F | keep |
| 18 | `grapher_template_all_smoke` | G9F | keep |
| 19 | `grapher_input_navigation` | G9A | keep |
| 20 | `grapher_implicit_circle_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 21 | `grapher_implicit_sideways_smoke` | **ORPH** | gate + candidate → **bless only post-GR-02** (pixels change: marching squares → y-sweep sampler) |
| 22 | `grapher_implicit_ycircle_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 23 | `grapher_ineq_disk_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 24 | `grapher_ineq_exterior_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 25 | `grapher_ineq_halfplane_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 26 | `grapher_ineq_parabola_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 27 | `grapher_aspect_circle_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 28 | `grapher_aspect_line_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 29 | `grapher_aspect_sin_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 30 | `grapher_aspect_sideways_parabola_smoke` | **ORPH** | gate + candidate → **bless only post-GR-02** (same reason as #21) |
| 31 | `grapher_aspect_small_circle_smoke` | **ORPH** | gate + candidate → bless post-PR-1 |
| 32 | `grapher_mixed_relations_smoke` | **ORPH** (2 in-script shots: `out/grapher_mixed_relations_{graph,table}.ppm`) | gate + candidate (candidate PPM = final `--screenshot` frame, the Table view); the two in-script shots ride the artifact upload for human review. Stem split into two scripts (TEST §D) is deferred to GR-15 — **not** done here (no script edits in this batch) |
| 33 | `grapher_trace_domain_smoke` | **ORPH** (2 in-script shots) | gate + candidate (final frame) |
| 34 | `grapher_explicit_parabola_smoke` | **ORPH** | gate + candidate → bless post-PR-1 (regression guard for the classifier change) |
| 35 | `grapher_implicit_tabletrace_safe` | **ORPH** (assert-only, 0 shots, ends at launcher) | **gate only** — final frame is the launcher, not a Grapher visual; never a candidate |
| 36 | `grapher_breakit_stress` | **ORPH** | **gate only** — stress endpoint; screenshot uploaded as artifact, never a candidate/golden (a truncated/stressed frame must never be blessed — TEST §D mask policy & GR-13 rule) |
| 37 | `grapher_curve_stress` | **ORPH** (header: "DELIBERATELY NOT wired into CI" for *goldens* — Phase-9D wording predates the T4-gap finding; TEST §E.1 now explicitly lists it for the no-hang gate) | **gate only**; screenshot stays human-review artifact |

**Deleted/merged: none.** Every script has a live purpose; merging `mixed_relations` splits is deferred (row 32).

**Missing scripts to add** (not in this batch's PR-1; PR-2/PR-3 add them per the execution plan): `grapher_asserts_smoke`, `grapher_negctl_assert_kind` (PR-2, hooks doc §E.1/E.2); `grapher_classifier_*` ×8 (PR-3, contract doc §D). Missing-but-later (GR-15 backlog, recorded for traceability): intersect-markers script, calc-menu goldens, T5 self-diff step, `grapher_negctl_wrong_expr` comparator control (TEST §D), table empty-state.

## B. Proposed CI step

- **Step name**: `Grapher Phase-10 relation/aspect/stress guard (headless deterministic, GR-00)`
- **Insertion point**: after the Phase-9F artifact upload (`emulator-build.yml:533-539`), before "Generate candidate screenshots (Phase 4B-A)" (`:558`).
- **Contract** (identical to Phase-9F, `emulator-build.yml:501-523`): per-script `timeout 60s`, `--headless --deterministic --frames 1400 --quiet`, require exit 0, require ≥1 `[ASSERT] … PASS -` marker, forbid `[ASSERT] … FAIL -`.

### B.1 Exact step text

```yaml
      # Grapher Phase-10 relation/aspect/stress guard (GR-00) — golden-free.
      #
      # Same assert-only contract as the Phase 9F step above. Closes the tier-T4
      # gap (TEST plan §A): until this step, every implicit / inequality /
      # equal-aspect / mixed-relation / trace-domain / stress behavior shipped in
      # Phase 10 was executed by NOTHING in CI. Scripts stay screenshot-writing
      # (images uploaded for human review); gating here is hang/assert only —
      # pixel gating arrives when a human blesses the corresponding candidates
      # (see NUMOS_GRAPHER_PHASE10_CI_WIRING_PLAN.md §E).
      - name: Grapher Phase-10 relation/aspect/stress guard (headless deterministic, GR-00)
        run: |
          set -euo pipefail
          BIN=.pio/build/emulator_pc/program
          mkdir -p out out/grapher_10

          run_grapher10() {
            script="$1"
            name="$(basename "$script" .numos)"
            log="out/grapher_10/${name}.log"
            echo "=== ${name} ==="
            rc=0
            SDL_VIDEODRIVER=dummy timeout 60s "$BIN" \
              --headless --deterministic --script "$script" \
              --frames 1400 --quiet > "$log" 2>&1 || rc=$?
            cat "$log"
            if [ "$rc" -ne 0 ]; then
              echo "::error::grapher-10 guard FAILED for ${name} (exit $rc -- hang/timeout or assertion failure)"
              return 1
            fi
            if grep -qE '\[ASSERT\].*: FAIL -' "$log"; then
              echo "::error::grapher-10 guard: [ASSERT] FAIL present for ${name}"
              return 1
            fi
            if ! grep -qE '\[ASSERT\].*: PASS -' "$log"; then
              echo "::error::grapher-10 guard: no [ASSERT] PASS marker for ${name}"
              return 1
            fi
          }

          run_grapher10 tests/emulator/scripts/grapher_implicit_x_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_implicit_2x_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_implicit_circle_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_implicit_sideways_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_implicit_ycircle_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_ineq_disk_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_ineq_exterior_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_ineq_halfplane_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_ineq_parabola_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_aspect_circle_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_aspect_line_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_aspect_sin_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_aspect_sideways_parabola_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_aspect_small_circle_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_mixed_relations_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_trace_domain_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_explicit_parabola_smoke.numos
          run_grapher10 tests/emulator/scripts/grapher_implicit_tabletrace_safe.numos
          run_grapher10 tests/emulator/scripts/grapher_breakit_stress.numos
          run_grapher10 tests/emulator/scripts/grapher_curve_stress.numos
          echo "OK: 20 Phase-10 Grapher relation/aspect/stress guards passed (assert-only, NO gating golden)."

      - name: Upload GR-00 Grapher Phase-10 guard logs + review screenshots
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: numos-emulator-grapher-10
          path: |
            out/grapher_10/
            out/grapher_implicit_*.ppm
            out/grapher_ineq_*.ppm
            out/grapher_aspect_*.ppm
            out/grapher_mixed_relations_*.ppm
            out/grapher_trace_domain_*.ppm
            out/grapher_explicit_parabola_smoke.ppm
            out/grapher_breakit_stress.ppm
            out/grapher_curve_stress.ppm
          if-no-files-found: warn
```

Count note: the batch adds implicit_x/implicit_2x (already candidates, previously assert-ungated) to reach **20 gated runs**; the "18 orphans" of the ticket are rows 20-37 of §A.

### B.2 PR-2 / PR-3 appends to this same step

```yaml
          # PR-2 (GR-14):
          run_grapher10 tests/emulator/scripts/grapher_asserts_smoke.numos
          echo "=== grapher_negctl_assert_kind (must exit 4) ==="
          rc=0
          SDL_VIDEODRIVER=dummy timeout 60s "$BIN" \
            --headless --deterministic \
            --script tests/emulator/scripts/grapher_negctl_assert_kind.numos \
            --frames 1400 --quiet > out/grapher_10/negctl_assert_kind.log 2>&1 || rc=$?
          if [ "$rc" -ne 4 ]; then
            echo "::error::negative control did NOT fail as required (exit $rc, expected 4)"
            exit 1
          fi
          # PR-3 (GR-02):
          for s in tests/emulator/scripts/grapher_classifier_*.numos; do run_grapher10 "$s"; done
```

## C. Candidate-generator additions (PR-1)

Append after `generate-emulator-candidates.py:142` (inside `CANDIDATES`, before the closing `]`), with the house-style comment block:

```python
    # GR-00 (Phase 10 wiring): implicit / inequality / equal-aspect / mixed /
    # trace-domain / explicit-regression candidates. Each script already asserts
    # the active app (exit 4 -> FAIL here). No golden is blessed by this change —
    # the compare step warns until a human promotes. grapher_implicit_sideways and
    # grapher_aspect_sideways_parabola must NOT be promoted before GR-02 lands
    # (their renderer changes from marching squares to the explicitX sampler).
    ("grapher_implicit_circle_smoke",  "tests/emulator/scripts/grapher_implicit_circle_smoke.numos", 1400),
    ("grapher_implicit_sideways_smoke","tests/emulator/scripts/grapher_implicit_sideways_smoke.numos", 1400),
    ("grapher_implicit_ycircle_smoke", "tests/emulator/scripts/grapher_implicit_ycircle_smoke.numos", 1400),
    ("grapher_ineq_disk_smoke",        "tests/emulator/scripts/grapher_ineq_disk_smoke.numos", 1400),
    ("grapher_ineq_exterior_smoke",    "tests/emulator/scripts/grapher_ineq_exterior_smoke.numos", 1400),
    ("grapher_ineq_halfplane_smoke",   "tests/emulator/scripts/grapher_ineq_halfplane_smoke.numos", 1400),
    ("grapher_ineq_parabola_smoke",    "tests/emulator/scripts/grapher_ineq_parabola_smoke.numos", 1400),
    ("grapher_aspect_circle_smoke",    "tests/emulator/scripts/grapher_aspect_circle_smoke.numos", 1400),
    ("grapher_aspect_line_smoke",      "tests/emulator/scripts/grapher_aspect_line_smoke.numos", 1400),
    ("grapher_aspect_sin_smoke",       "tests/emulator/scripts/grapher_aspect_sin_smoke.numos", 1400),
    ("grapher_aspect_sideways_parabola_smoke", "tests/emulator/scripts/grapher_aspect_sideways_parabola_smoke.numos", 1400),
    ("grapher_aspect_small_circle_smoke", "tests/emulator/scripts/grapher_aspect_small_circle_smoke.numos", 1400),
    ("grapher_mixed_relations_smoke",  "tests/emulator/scripts/grapher_mixed_relations_smoke.numos", 1400),
    ("grapher_trace_domain_smoke",     "tests/emulator/scripts/grapher_trace_domain_smoke.numos", 1400),
    ("grapher_explicit_parabola_smoke","tests/emulator/scripts/grapher_explicit_parabola_smoke.numos", 1400),
]
```

Frame budget rationale: 1400 matches the typing-heavy precedent (`generate-emulator-candidates.py:136-142` comment "the typing-heavy scripts need ~1400 frames"); all Phase-10 scripts finish their command lists well inside 1400 deterministic frames (longest is `breakit_stress` at 64 lines ≈ ≤ 700 frames of commands+waits; not a candidate anyway).

## D. Expected runtime & output checks

| Item | Value | Basis |
|---|---|---|
| New gate step wall time | ~90-150 s (20 runs × ~4-7 s: 1400 deterministic frames headless; same per-run cost class as the six 9F runs) | matches TICKETS GR-00 budget "+~2 min" |
| Candidate step delta | +15 runs ≈ +60-105 s | same per-run cost |
| Output checks per gated run | exit 0 ∧ `[ASSERT]…PASS` present ∧ `[ASSERT]…FAIL` absent | §B.1 helper |
| Output checks per candidate | generator validates P6 header + exact 230415 bytes (`generate-emulator-candidates.py:29-33,218-230`); compare step: golden present ⇒ byte/mask compare gates (`emulator-build.yml:575-607`), absent ⇒ `::warning` |
| Log upload path | `out/grapher_10/*.log` → artifact `numos-emulator-grapher-10`; candidate PPMs → existing artifact `numos-emulator-candidates` (`emulator-build.yml:567-573`) |
| Failure behavior | any gated run red ⇒ job fails (step `set -euo pipefail` + `return 1`); candidate mismatch vs a blessed golden ⇒ job fails; missing golden ⇒ warning only |

## E. Policies

1. **No-golden policy** (unchanged, restated as binding): a stem without a blessed golden NEVER gates pixels; the compare step emits `::warning` (`emulator-build.yml:602-604`). An unreviewed image can never become a passing gate.
2. **Candidate/golden policy**: promotion is human-only via `scripts/promote-emulator-golden.py` after visual review of a **Linux-generated** candidate (ground truth constraint #6; TEST §E.7 OS-drift rule). Mask policy: clock rect only; never mask the 320×143 canvas, info bar, or pill (TEST §D). Post-PR-1 blessing list (13): implicit_{x,2x,circle,ycircle}, ineq_{disk,exterior,halfplane,parabola}, aspect_{circle,line,sin,small_circle}, explicit_parabola. Deferred to post-GR-02 (2): implicit_sideways, aspect_sideways_parabola. Reviewer checklist per image: curve/region matches the script header's stated math; equal-aspect circles round; no truncation/stress frame; StatusBar clock is the only volatile region.
3. **Stress policy**: `breakit_stress` / `curve_stress` are gate-only forever; their frames are human-review artifacts (no golden may be captured from a stress state — GR-13 rule anticipated).
4. **Grammar policy**: the `.numos` grammar and this step's helper contract are append-only; new scripts append `run_grapher10` lines, never edit prior ones.
5. **Determinism policy**: everything runs `--deterministic`; the T5 self-diff automation stays a GR-15 deliverable, but any flake observed in the new step is a bug to fix, never a mask/retry (TEST §D).

## F. Transition plan

| Stage | State | Gate strength |
|---|---|---|
| Today | 18 orphans, 0 implicit/ineq pixels gated | none |
| PR-1 merged | 20 stems hang/assert-gated; 15 new candidates warn | no-hang + assert_app |
| Post-PR-1 blessing (human) | 13 goldens gate byte-exact | full visual for implicit/ineq/aspect |
| PR-2 merged | + hooks, positive & negative controls gated | assert plumbing proven fail-capable |
| PR-3 merged | + 8 classifier scripts gated semantically | classification contract enforced |
| Post-PR-3 blessing (human) | + sideways pair + explicitx/deg-sine goldens | full batch surface visual+semantic |
| GR-15 (out of batch) | stem splits, self-diff step, comparator negative control, corpus completion | — |

## G. Rollback

- PR-1: revert the commit (workflow step + candidate tuples). Blessed goldens from stage 3 remain valid but stop being generated/compared — acceptable during a short revert window; delete them only if the revert is permanent.
- Any single flaky script (emergency): comment out its ONE `run_grapher10` line + its `CANDIDATES` tuple in a revert-labeled commit; never weaken the helper's contract.

## H. Master PR checklist (all three PRs)

**PR-1 (GR-00)**
- [ ] `emulator-build.yml`: new step + artifact upload inserted between `:539` and `:558`-equivalent anchors; Phase-9F step byte-identical
- [ ] `generate-emulator-candidates.py`: 15 tuples appended; no existing tuple touched
- [ ] `git diff --stat` shows exactly 2 files
- [ ] Local: all 20 gated scripts pass (execution plan §G.1); candidate generator exits 0
- [ ] No file under `tests/emulator/golden/**`, `masks/**`, `src/**` changed
- [ ] PR body links this doc + execution plan; notes the two do-not-bless stems

**PR-2 (GR-14)**
- [ ] `GrapherApp.h`/`GraphModel.h` additions strictly inside `#ifdef NATIVE_SIM`; `Evaluator.h` gains only the const getter
- [ ] `NativeHal.cpp`: new enum values appended after `AssertMenuFocus`; new parse branches before the terminal `else` (`:1612`); new dispatch cases appended; zero edits to existing lines (verify: `git diff` hunks are pure insertions)
- [ ] `ScriptCmd` gains only the defaulted `waitN2` field
- [ ] Positive control exits 0; negative control exits 4; malformed command exits 2 (execution plan §G.2)
- [ ] `pio run -e esp32s3_n16r8` compiles (hooks compiled out)
- [ ] `docs/emulator-sdl2-quickstart.md` documents every new command incl. `set_angle_mode`
- [ ] All existing CI steps green (grammar untouched ⇒ existing scripts unaffected)

**PR-3 (GR-02)**
- [ ] Classifier implements contract rules R0-R14 in order; kind/reason vocabulary matches hooks doc §C exactly
- [ ] `implicit` field kept in sync (`kind != ExplicitY`); no `isExplicit`/`firstTraceableFunc`/`rebuildTable` call-site edits
- [ ] Angle sync maps enums **by name** (vpam `{DEG,RAD}` vs legacy `{RAD,DEG}`)
- [ ] 6 blessed Grapher goldens byte-pass; 13 stage-3 goldens byte-pass (execution plan §G.3 loop)
- [ ] 8 classifier scripts green in the gate step; negative oracle: `x=` frame contains no curve overdraw
- [ ] Corpus regression rows (contract §C.2) all assert green
- [ ] Firmware compiles; device smoke `x=y^2` traces… **not required this batch** (trace is GR-07) — device smoke = plot renders, no watchdog reset during replot
- [ ] `[GRAPH] invalid slot=N reason=…` log line emitted per GEOM F.1/F.2 grammar (telemetry spec §2.8)
- [ ] Post-merge human task filed: bless sideways pair + 3 classifier goldens

---

*End of Document 4 of 4.*
