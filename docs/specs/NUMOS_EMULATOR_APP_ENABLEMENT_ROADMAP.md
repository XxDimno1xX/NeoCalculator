# NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP

Status: SPEC-ONLY (P-05 wave). Ordered, staged ticket plan.
Namespaces: `APPPAR-*` (app parity), `ROUTE-*` (NativeHal/SystemApp routing), `APPQA-*` (scripts/goldens/asserts), `STORE-*` (persistence), `LIFE-*` (lifecycle/teardown), `PORT-*` (host portability — **existing namespace on the determinism branch already uses PORT-01…PORT-05; new IDs here start at PORT-06**). External dependencies referenced: `FIX-*`, `EMUDET-*`, `CI-*` (see `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md`).

Owner tiers follow the risk register convention: Human / Fable / Opus / Sonnet.

Ticket field template: **Motivation · Evidence · Files · Desired behavior · Non-goals · Deps · Tests · Golden impact · Persistence impact · Firmware impact · Rollback · Risk · Owner.**

---

## Wave 0 — no new app: inventory + harness (all gates for later waves)

### APPPAR-00 — App inventory freeze
- **Motivation**: every later ticket assumes a fixed app census; drift (new apps, renamed cards) silently invalidates the matrix.
- **Evidence**: 23 app pairs incl. 2 orphans; 21 cards (`src/ui/MainMenu.cpp:88-113`); emulator wires 8 surfaces (`src/hal/NativeHal.cpp:914-987`); "StixGlyphGallery" confirmed nonexistent.
- **Files**: `docs/specs/NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (this set) becomes the frozen census; add a CI grep (docs-adjacent script) asserting `APPS[]` entry count and `AppMode` member count match the matrix header numbers.
- **Desired**: any PR changing `MainMenu::APPS[]`, `Mode`, or `AppMode` must update the matrix in the same PR.
- **Non-goals**: no behavior change. **Deps**: none. **Tests**: the count-guard script self-test.
- **Golden/persistence/firmware impact**: none. **Rollback**: delete guard. **Risk**: low. **Owner**: Sonnet.

### ROUTE-01 — Generic app enablement checklist (PR template)
- **Motivation**: enabling an app touches ≥6 NativeHal points + platformio; half-wired apps hang or null-deref (risk EMU-1 dispatcher drift).
- **Evidence**: touchpoint list verified at `NativeHal.cpp:195-206, 867-908, 914-987, 565-759, 997-1036, 1414-1428, 1656-1668`; Grapher's Phase 8B needed 10 touchpoints + 6 heap guards.
- **Files**: `.github/PULL_REQUEST_TEMPLATE/app-enablement.md` (new), routing spec §D as source of truth.
- **Desired**: checklist covering filter closure, AppMode, construction (lazy), launchApp case, dispatch case (incl. D5 AC/autosave parity), teardown case, canonical names, smoke script, candidate registration, native-safety grep (`heap_caps_|xTaskCreate|<LittleFS.h>|rand\(`).
- **Non-goals**: no automation of the wiring itself. **Deps**: APPPAR-00. **Tests**: n/a (process). **Impacts**: none. **Rollback**: remove template. **Risk**: low. **Owner**: Sonnet.

### APPQA-01 — Per-app smoke script harness
- **Motivation**: today only hand-listed scripts run (glob covers `calc_semantic_*` only, `emulator-build.yml:341`); 27 scripts are orphaned.
- **Evidence**: inventory in QA plan §A; CANDIDATES hardcoded (`scripts/generate-emulator-candidates.py:39-143`).
- **Files**: `scripts/generate-emulator-candidates.py`, `.github/workflows/emulator-build.yml`, new `tests/emulator/golden/PENDING.md`.
- **Desired**: every stem classified gated/pending/assert-only (CI-05 rule); unexpected stems fail; a manifest (not code edits) declares new scripts.
- **Non-goals**: no new scripts here. **Deps**: CI-05 (or subsumes it for scripts). **Tests**: negative control — add an unclassified stem, CI must fail.
- **Golden impact**: none (classification only). **Persistence**: none. **Firmware**: none. **Rollback**: revert workflow step. **Risk**: low. **Owner**: Opus.

### LIFE-01 — Repeated app enter/exit loop
- **Motivation**: teardown bugs (StatusBar-before-screen, dangling anim) only surface on relaunch; the returnToMenu hang shipped because no smoke exercised exit-to-launcher.
- **Evidence**: deferred teardown `NativeHal.cpp:1055-1077, 2074-2078`; Key Constraints 2–3; QA-08 precedent (10× relaunch).
- **Files**: `tests/emulator/scripts/<app>_relaunch_loop.numos` ×8 enabled apps; one CI step.
- **Desired**: ×10 open/home per app under `--deterministic`, final `assert_app Menu`, exit 0; gating.
- **Non-goals**: object-leak counting (ROUTE-05). **Deps**: APPQA-01. **Tests**: the scripts themselves + one deliberately broken loop as bite-test.
- **Golden impact**: none (no screenshots or final-screen only, warning). **Persistence**: none. **Firmware**: none. **Rollback**: drop step. **Risk**: low. **Owner**: Sonnet.

### STORE-01 — Per-app persistence inventory guard
- **Motivation**: each storage app imports a new `emulator_data/` write; without an inventory + sandbox gate, enabling one dirties every CI run.
- **Evidence**: stores: `/py/*` (`PythonApp.cpp:340, 657-696`), `/circuits/*`+autosave (`CircuitCoreApp.cpp:1364-1374, 1712-1721`), `/fluid/*` (`Fluid2DApp.cpp:1455-1518`), `/save.pt` (`ParticleLabApp.cpp:416-431`), `/neural.nn` (`NeuralLabApp.cpp:950-967`), `/neolang.nl` (`NeoLanguageApp.cpp:116, 897-931`), `/vars.dat` (`CalculationApp.cpp:900`); FS shim root hardcoded (`src/hal/FileSystem.cpp:38`).
- **Files**: parity-register table (extend FIX-06's register); CI grep that fails when a whitelisted TU includes `<LittleFS.h>`/`hal/FileSystem.h` without a register row.
- **Desired**: no app with an unregistered store can be whitelisted.
- **Non-goals**: implementing the sandbox (FIX-01/02). **Deps**: FIX-06 register format. **Tests**: guard self-test. **Impacts**: none. **Rollback**: remove grep. **Risk**: low. **Owner**: Sonnet.

### ROUTE-02 — App route negative controls
- **Motivation**: prove dead-card refusal and detect half-wiring (case added, app never constructed → null deref).
- **Evidence**: `default:` refusal `NativeHal.cpp:983-985`; dead ids 2,3,8,9,11-19.
- **Files**: `tests/emulator/scripts/menu_dead_card_refusal.numos` (key-navigate to a dead card, ENTER, assert menu alive); stable refusal log line in `NativeHal.cpp` (routing spec §D.3) — **emulator-only edit**.
- **Desired**: gating assert-only step; log line greppable `[APP] unsupported-in-emulator id=<n>`.
- **Non-goals**: hiding/dimming cards (OD-4). **Deps**: APPQA-01. **Tests**: itself. **Golden**: none. **Persistence**: none. **Firmware**: none (NativeHal only). **Rollback**: revert. **Risk**: low. **Owner**: Sonnet.

### APPQA-05 — Wire the 27 orphaned scripts *(added; not in the mandated minimum but blocking honest coverage)*
- **Motivation**: 21 tracked + 6 untracked scripts (incl. the Phase-10G grapher_* set in git status) run nowhere — written value earning nothing.
- **Evidence**: QA plan §A; no glob matches `grapher_*_smoke`.
- **Files**: CANDIDATES/manifest, `emulator-build.yml` 9F list or new glob, `git add` of the 6 untracked scripts.
- **Desired**: every script classified and running (assert-only at minimum). **Deps**: APPQA-01. **Golden impact**: new pending stems. **Risk**: low-medium (some scripts may be stale and fail — triage, don't delete). **Owner**: Opus.

## Wave 1 — low-risk static/UI apps

### APPPAR-04 — Enable Matrices
- **Motivation**: lowest-risk unenabled app; proves the ROUTE-01 checklist on fresh code.
- **Evidence**: no persistence/timers/CAS/rand (matrix §2.1); firmware routing `SystemApp.cpp:545-551, 772-776`.
- **Files**: `platformio.ini` (+MatricesApp.cpp, +MatrixEngine.cpp), `src/hal/NativeHal.cpp` (6 touchpoints), `tests/emulator/scripts/matrices_smoke.numos`, `matrices_relaunch_loop.numos`.
- **Desired**: P1→P3 in one PR series (P3 needs `debugResultText()` hook — coordinate with APPQA-03); MODE-only exit (firmware has no AC-exit here — do not add one).
- **Non-goals**: golden blessing (APPQA-04). **Deps**: ROUTE-01, APPQA-01, LIFE-01 pattern; native-safety grep must pass first.
- **Tests**: smoke + det + relaunch. **Golden**: new pending stems. **Persistence**: none. **Firmware impact**: none if grep is clean; if guards are needed in MatricesApp they are `#ifdef`-reviewed (firmware-identical binary asserted by region-diff, not whole-file hash).
- **Rollback**: remove filter lines + NativeHal cases (self-contained). **Risk**: low. **Owner**: Opus.

### APPPAR-05 — Enable Periodic Table (Chemistry)
- Same shape as APPPAR-04. **Extra requirements**: audit `ChemCAS.cpp` for native safety; replicate firmware's **AC-exit** (`SystemApp.cpp:569-575`) in the dispatch case (divergence class D5); canonical name `Chemistry`. **Files**: + ChemCAS.cpp/data headers as link demands; `chemistry_smoke/_element_smoke/_relaunch_loop`. **Golden**: best early candidate (static content). **Risk**: low. **Owner**: Opus.

### ROUTE-03 — Script API additions: `assert_app_ready`, `assert_no_crash`, `assert_statusbar_title`
- **Motivation**: Wave 1 asserts need to avoid racing the 260 ms teardown fade and to verify chrome truthfulness (SB-* rules).
- **Evidence**: teardown race window `NativeHal.cpp:2074-2078`; StatusBar title contract (SBAR §F.3).
- **Files**: `NativeHal.cpp` parser/exec; `src/ui/StatusBar.*` gains read-only `debugTitle()` (native-visible).
- **Desired**: per routing spec §F; append-only grammar. **Deps**: none hard. **Tests**: unit scripts + negative control. **Impacts**: none. **Rollback**: commands unused → remove. **Risk**: low. **Owner**: Opus.

## Wave 2 — math-renderer / CAS apps

### APPPAR-02 — Enable Calculus
- **Motivation**: first CAS-closure app; smaller closure than Equations (SymDiff/SymIntegrate/SymToAST on top of already-native cas:: TUs, `platformio.ini:252-258`).
- **Evidence**: includes `CalculusApp.h:54-60`; rand flavor text `CalculusApp.cpp:449, 453`; firmware routing `SystemApp.cpp:510-516, 742-746`.
- **Files**: platformio (+SymDiff.cpp, +SymIntegrate.cpp, +SymToAST.cpp, + whatever link demands — incremental-closure method per arch §P), NativeHal touchpoints, `calculus_smoke/_derivative_smoke/_relaunch_loop`.
- **Desired**: P3 with `debugResultText()`; goldens: entry UI only until hook lands; never capture flavor-text screen (OD-5).
- **Non-goals**: integral parity with orphan IntegralApp. **Deps**: Wave 1 complete; EMUDET-03 rand-fence desirable but not blocking (screens avoided instead).
- **Tests**: semantic derivative rows (reuse CAS corpus DIFF-* expectations). **Golden**: pending stems. **Persistence**: none. **Firmware**: none (unmodified sources). **Rollback**: self-contained. **Risk**: medium. **Owner**: Opus, Fable review on closure audit.

### APPPAR-01 — Enable Equations
- **Motivation**: P-05's headline app ("highest-value emulator enablement" per arch §F).
- **Evidence**: closure list `EquationsApp.cpp:36-43`/`.h:39-51`; **blockers**: `heap_caps_get_free_size` diagnostics (`EquationsApp.cpp:487-516`), rand flavor (`:106`), `update()` cooperative tick (`SystemApp.cpp:255-259`), `end()` must `steps.clear()` (risk MEM-6).
- **Files**: platformio (~11 new cas:: TUs), NativeHal (touchpoints + per-frame `update()` call for the active app — new dispatcher capability), heap-caps guard (`#ifdef ARDUINO` or ArduinoCompat shim — smallest possible diff, firmware-region-diff verified), scripts `equations_smoke/_linear_smoke/_relaunch_loop`.
- **Desired**: P3 with `debugSolutionText()`; goldens list+result screens only.
- **Non-goals**: Giac path (native baseline is Giac-absent); tutor flows. **Deps**: APPPAR-02 (proves CAS closure method), ROUTE-03. **Tests**: EQ-* corpus rows as `.numos` asserts; ASAN run for arena lifetime (MEM-2). **Golden**: pending. **Persistence**: none. **Firmware impact**: the heap-caps guard touches firmware-shared source — requires explicit review + region-diff proof. **Rollback**: filter/NativeHal revert; guard stays (harmless). **Risk**: medium-high. **Owner**: Fable.

### APPPAR-07 — Math Visual / Showcase parity
- **Motivation**: decide OD-3 and close the "Math Visual dead card in validate builds" gap.
- **Evidence**: app gated `SystemApp.h:62-65, 98-100`; Showcase proxy `NativeHal.cpp:1093-1182` shares `MathRenderVisualCases.cpp`.
- **Desired**: default = keep Showcase as renderer fixture; extend its pages per MR-* plan; enable the real app natively **only** if MR plan requires device-parity captures. Document ruling in shim register.
- **Deps**: MR-01/MR-02 direction. **Risk**: low. **Owner**: Opus.

### APPPAR-08 — StixGlyphGallery
- **Ruling: no such app exists in source** (matrix §Summary). Ticket resolves to: (a) confirm with a repo-wide grep at execution time; (b) if the intent was a glyph-coverage fixture, file it as a Math Showcase page set under APPPAR-07 (glyph sweep pages), not a new app. **Risk**: none. **Owner**: Sonnet.

### APPQA-02 — Golden candidate generation for Waves 1–2
- **Motivation**: every new script needs a candidate stem, frames count, and PENDING row.
- **Files**: candidates manifest, PENDING.md. **Desired**: warning-only candidates for matrices/chemistry/calculus/equations stems; self-diff (CI-02 pattern) green. **Deps**: per-app tickets. **Golden impact**: pending only. **Risk**: low. **Owner**: Sonnet.

### APPQA-03 — Semantic assert hooks (Waves 1–2 + enabled-app backfill)
- **Motivation**: no computation golden without a hook (QA plan §C/§D); backfills Regression/Sequences and unblocks Stats/Prob post-digit-fix.
- **Files**: per-app `debug*` accessors (NATIVE_SIM-visible, read-only) + `NativeHal.cpp` commands; append-only grammar.
- **Deps**: per-app enablement; OD-7 for Stats/Prob. **Tests**: negative control #1 per hook. **Firmware impact**: header-only debug accessors compiled on device too — keep allocation-free and `const`. **Risk**: low-medium. **Owner**: Opus.

### APPQA-04 — Golden promotion wave (Waves 1–2)
- **Motivation**: convert stable pending candidates to gated goldens in one coordinated, human-reviewed wave (CIART re-bless discipline).
- **Desired**: promote matrices/chemistry (+ calculus/equations static screens if stable) after the soak window; provenance lines; masks ⊆ clock rect (none if EMUDET-01 landed first).
- **Deps**: APPQA-02/-03 green ≥2 weeks. **Golden impact**: the point. **Risk**: medium (EMU-2 blind re-bless — mitigated by review rule). **Owner**: Human.

## Wave 3 — storage/timer/simulation apps (P-06 preview; specified now, executed after FIX/EMUDET waves)

### APPPAR-09 — Sim/tool app family: split ruling
- **Split**: OpticsLab first (no storage/PSRAM/rand — proves timer pattern) → Bridge → ParticleLab (save path gated on FIX-04 roundtrip) → Circuit (D5 autosave shim ROUTE-04; STORE gates) → Fluid2D (same + OD-6 float ruling). **Deferred out of the family**: NeuralLab (unseeded rand — blocked until a seeding decision; EMUDET-03 fence is emulator-side only and does not make training screens meaningful), Fractal (**do not enable**: FreeRTOS `xTaskCreate`, `FractalApp.cpp:491-496`; requires its own mini-spec — options: std::thread shim vs synchronous NATIVE_SIM render path, both out of P-05/P-06 scope), NeoLang (storage + free-text input + unaudited language-stack closure). Each sub-enablement follows ROUTE-01 with its own APPPAR sub-ID (APPPAR-09a…e).
- **Deps**: FIX-01/02/03, CI-01, EMUDET-01/02, LIFE-02, ROUTE-04. **Risk**: medium→high per matrix. **Owner**: Opus/Fable.

### APPPAR-06 — Python: explicit deferral
- **Ruling: defer.** Blocked on FIX-01/02 (imports `/py/*` writes incl. boot-seeded `hello.py`, `PythonApp.cpp:340, 657-696`) and on a text-entry decision (routing spec §E.2: `text` command vs alpha sequences). Also pending arch §F's product question (toy interpreter vs MicroPython) — enabling a UI whose engine may be replaced is wasted golden churn. Revisit at P-06 planning with those three inputs. **Owner**: Human (product call) + Opus.

### ROUTE-04 — Dispatcher parity shims for D5 apps
- **Motivation**: firmware gives PeriodicTable/Circuit/Fluid2D/Fractal/MathVisual dispatcher-level AC-exit/autosave (`SystemApp.cpp:569-651`); emulator dispatch must replicate per app or key semantics diverge.
- **Files**: `NativeHal.cpp` dispatch cases, one sub-ticket per app bound to its APPPAR ticket. **Tests**: AC-exit scripts. **Risk**: low each. **Owner**: Sonnet.

### LIFE-02 — Timer-active teardown stress
- **Motivation**: HOME during an active `lv_timer` sim is the classic UAF; must be proven per timer app before its golden work.
- **Evidence**: timers at `GrapherApp.cpp:3518`, `BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `Fluid2DApp.cpp:257`, `ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `OpticsLabApp.cpp:253`.
- **Files**: `<app>_timer_exit_stress.numos` per timer app; CI step. **Desired**: enter → sim running → immediate HOME → relaunch ×5 → exit 0. **Deps**: LIFE-01, per-app enablement. **Risk**: medium (will find real bugs — that's the point). **Owner**: Opus.

### ROUTE-05 — LVGL object-count marks (`mark_lvgl_objects` / `assert_lvgl_object_delta`)
- **Motivation**: turn leak suspicion into a number; wraps LIFE-01/02 loops. **Files**: NativeHal parser + a descendant-count walk of `lv_scr_act()`. **Risk**: low. **Owner**: Sonnet.

### STORE-02 — Per-app fixture sets *(added)*
- **Motivation**: storage apps need committed fixtures (`tests/emulator/fixtures/<name>/`) for load-path tests (FIX spec §prereq). **Files**: fixtures + manifest rows per app (python/, circuit/, fluid/, particlelab/, neural/, neolang/ as enabled). **Deps**: FIX-01. **Owner**: Opus.

### PORT-06 — Cross-OS soak for new-app goldens *(new ID; extends PORT-05)*
- **Motivation**: every new golden is Linux-reference-only; sim apps are float-heavy (worst drift candidates). **Desired**: fold new stems into PORT-05's drift measurement before any cross-OS gating claims; feed OD-6. **Owner**: Sonnet.

## Wave 4 — hardware/CAS/Giac-sensitive — future
NeuralLab seeding decision; Fractal mini-spec; NeoLang audit; native Giac (explicitly out of scope; the emulator baseline remains Giac-absent); STEP_VIEW/edu-steps surface if ever needed.

---

## Ordered execution summary

| # | Ticket | Wave | Gate it clears |
|---|---|---|---|
| 1 | APPPAR-00 | 0 | census frozen |
| 2 | ROUTE-01 | 0 | enablement pattern |
| 3 | APPQA-01 | 0 | stem governance |
| 4 | APPQA-05 | 0 | orphan scripts earning |
| 5 | LIFE-01 | 0 | teardown baseline |
| 6 | STORE-01 | 0 | storage gate |
| 7 | ROUTE-02 | 0 | refusal control |
| 8 | ROUTE-03 | 1 | assert API |
| 9 | APPPAR-04 Matrices | 1 | first new app |
| 10 | APPPAR-05 Chemistry | 1 | D5 pattern proven |
| 11 | APPQA-02 | 1–2 | candidates |
| 12 | APPQA-03 | 1–2 | hooks |
| 13 | APPPAR-02 Calculus | 2 | CAS closure method |
| 14 | APPPAR-01 Equations | 2 | P-05 headline |
| 15 | APPPAR-07 / APPPAR-08 | 2 | OD-3 ruling / nonexistence ruling |
| 16 | APPQA-04 | 2 | promotion wave |
| 17 | ROUTE-04, LIFE-02, ROUTE-05, STORE-02 | 3 | sim-app gates |
| 18 | APPPAR-09a…e | 3 | sim apps |
| 19 | APPPAR-06 (defer ruling), PORT-06 | 3 | — |

External prerequisite spine (not owned here): FIX-01→FIX-03→CI-01→EMUDET-02 before Wave 1 goldens; EMUDET-01→re-bless before unmasked goldens; FIX-02→FIX-04 before Wave 3 storage apps.
