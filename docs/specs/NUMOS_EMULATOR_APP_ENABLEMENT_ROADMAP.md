<<<<<<< HEAD
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
=======
# NumOS — Emulator App Enablement Roadmap (P-05)

> Ordered, ticketed implementation roadmap for the P-05 spec set. Read
> `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` first (scope, P0-P7 ladder),
> then `..._NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC.md` (ROUTE-*),
> `..._PER_APP_ENABLEMENT_MATRIX.md` (per-app evidence),
> `..._APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (APPQA-*/LIFE-* test contracts).
>
> Namespaces: **APPPAR-** app parity, **ROUTE-** NativeHal/SystemApp routing,
> **APPQA-** scripts/goldens/asserts, **STORE-** persistence, **LIFE-**
> lifecycle/teardown, **PORT-** host portability. External dependencies use
> their home namespaces (FIX-*, EMUDET-*, CI-*, GS-*, SB-* — see the
> determinism roadmap `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md`).
> Owner tiers per the risk-register convention (Human / Fable-Opus / Sonnet /
> Haiku-class).

---

## Wave plan

- **Wave 0 — no new app**: inventory freeze + harness + guards
  (APPPAR-00, ROUTE-01, ROUTE-05, ROUTE-03, APPQA-01, LIFE-01, STORE-01,
  ROUTE-02). External prereqs pulled in: FIX-01, FIX-03, CI-01, EMUDET-03
  (before first rand()-carrying app), CI-04.
- **Wave 1 — low-risk static/UI apps**: APPPAR-04 Matrices, APPPAR-05
  PeriodicTable (+APPQA-02 candidates, APPQA-03 hooks for both).
- **Wave 2 — math-renderer/CAS apps**: APPPAR-02 Calculus, then ROUTE-04 +
  APPPAR-01 Equations (+LIFE-02, APPQA-03 extensions, APPQA-04 first golden
  promotion wave; APPPAR-07/APPPAR-08 ride along as low-cost renderer items).
- **Wave 3 — storage/timer/simulation apps**: requires FIX-02 (sandbox
  default). APPPAR-09a Optics+Bridge (timers, no FS), APPPAR-09b
  Circuit/Fluid2D/ParticleLab (FS + timers), APPPAR-09d Fractal (needs
  ROUTE-04). STORE-02 roundtrips.
- **Wave 4 — hardware/CAS/Giac-sensitive**: APPPAR-09c NeuralLab (seeded-RNG
  hook), APPPAR-06 Python (product decision first). Giac-dependent behavior:
  never (native Giac impossible, `platformio.ini:179-194`) — firmware-only
  harness owns it (P-02).
- **Future / not planned**: Integral, Tutor (dead on firmware — P-07 owns
  deletion; APPPAR-03 resolves against it), NeoLang (after script grammar can
  drive an editor), Math Visual id 20 (emulator keeps MathShowcase instead).

Ticket ordering inside waves is the listing order below.

---

## Wave 0

### APPPAR-00 — App inventory freeze
- **Motivation:** every downstream ticket keys off a fixed app list; the launcher
  table, firmware dispatch and emulator dispatch must be snapshotted so drift is
  detectable.
- **Evidence:** `MainMenu.cpp:88-113` (20 cards), `SystemApp.cpp:885-913`
  (dispatch 0-20), `NativeHal.cpp:924-997` (ids {0,1,4,5,6,7,10,100}),
  dead apps Tutor/Integral (ground truth §F).
- **Files:** `docs/specs/NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (already
  authored — this ticket ratifies it), plus a machine-readable
  `tests/emulator/app_inventory.json` (id, name, firmware mode, emulator mode
  or null, persistence paths, timer list).
- **Desired behavior:** inventory file committed; adding/removing an app without
  updating it fails ROUTE-05.
- **Non-goals:** no code behavior change.
- **Dependencies:** none. **Tests:** ROUTE-05 consumes it. **Golden impact:**
  none. **Persistence impact:** none. **Firmware impact:** none.
- **Rollback:** delete the JSON. **Risk:** low. **Owner:** Sonnet.

### ROUTE-01 — Generic app enablement checklist
- **Motivation:** enabling an app touches 8 hand-maintained NativeHal sites +
  whitelist + scripts; missed sites are today's silent-drift engine (EMU-1).
- **Evidence:** routing spec §A.4 (sites list with lines); Settings as the
  worked template (`NativeHal.cpp:102,199,979-985,1016-1018,647-663,1409,1434`).
- **Files:** `src/hal/NativeHal.cpp` (doc comment block near AppMode),
  `.github/pull_request_template.md` (checklist section), routing spec §D.1
  (normative text — already written).
- **Desired behavior:** every enablement PR carries the filled checklist; each
  touched site gets an `// APP-ENABLE(<Name>)` marker.
- **Non-goals:** no registry refactor (a table-driven registry is a possible
  later ticket; P-05 keeps append-only switches to protect the CI contract).
- **Dependencies:** APPPAR-00. **Tests:** ROUTE-05 mechanizes. **Golden:** none.
  **Persistence:** none. **Firmware:** none. **Rollback:** doc-only.
- **Risk:** low. **Owner:** Sonnet.

### ROUTE-05 — Route-sync checker
- **Motivation:** mechanize ROUTE-01; a half-wired app (e.g. whitelisted but no
  teardown case) must fail CI, not soak as UB.
- **Evidence:** the eight sites (routing spec §A.4); precedent guard style
  `scripts/check-keycode-digit-patterns.py` + `emulator-build.yml:96-104`.
- **Files:** new `scripts/check-emulator-app-routes.py`; `emulator-build.yml`
  (new early step); reads `tests/emulator/app_inventory.json`.
- **Desired behavior:** for every app marked emulator-enabled in the inventory,
  assert presence of: whitelist entry, AppMode enumerator, launchApp case,
  teardown case, dispatchKey case, name-map triple, exit-cleanup entry; fail
  with a per-site diagnostic.
- **Non-goals:** no semantic verification of the cases' bodies.
- **Dependencies:** APPPAR-00, ROUTE-01. **Tests:** self-test with a synthetic
  bad fixture (negative control, part of ROUTE-02 acceptance). **Golden:** none.
  **Persistence:** none. **Firmware:** none (script only).
- **Rollback:** remove CI step. **Risk:** low. **Owner:** Sonnet.

### ROUTE-03 — Symmetric exit cleanup (fix D10)
- **Motivation:** `g_grapherApp` is never `end()`-ed/deleted at process exit —
  breaks the symmetric-teardown invariant and poisons future leak accounting.
- **Evidence:** `NativeHal.cpp:2144-2151` (grapher absent from cleanup block).
- **Files:** `src/hal/NativeHal.cpp` (one block).
- **Desired behavior:** every constructed app appears in cleanup; ROUTE-05
  checks it thereafter.
- **Non-goals:** no teardown-order redesign.
- **Dependencies:** none. **Tests:** full emulator suite (behavioral no-op at
  runtime); LIFE-02 later relies on it. **Golden:** none. **Persistence:** none.
  **Firmware:** none.
- **Rollback:** revert one hunk. **Risk:** low. **Owner:** Haiku-class.

### APPQA-01 — Per-app smoke script harness
- **Motivation:** every wave needs the same smoke shape; encode it once.
- **Evidence:** existing shape `settings_smoke.numos` /
  `math_showcase_smoke.numos`; CI per-script contract
  (`emulator-build.yml:308-356`); candidate generator registration
  (`generate-emulator-candidates.py:39-143`).
- **Files:** `tests/emulator/scripts/` (new stems per wave),
  `scripts/generate-emulator-candidates.py` (stem entries),
  `.github/workflows/emulator-build.yml` (one new suite step per wave, same
  run_semantic pattern).
- **Desired behavior:** adding a smoke = script + generator entry + suite-list
  line; QA plan §B is the per-app content authority.
- **Non-goals:** no new script grammar in this ticket.
- **Dependencies:** ROUTE-01. **Tests:** the scripts are the tests; two-run
  SHA self-diff per new stem. **Golden impact:** candidates only (pending).
  **Persistence:** none. **Firmware:** none.
- **Rollback:** remove stems/steps. **Risk:** low. **Owner:** Sonnet.

### LIFE-01 — Repeated app enter/exit loop
- **Motivation:** teardown bugs (UAF/hang class — the Phase 9F incident) only
  surface on re-entry; nothing loops today.
- **Evidence:** deferred teardown windows (`NativeHal.cpp:230-233,2088-2092`;
  `SystemApp.cpp:243-250`); idempotent-end contract (`NativeHal.cpp:1005`).
- **Files:** `tests/emulator/scripts/life_<app>_loop.numos` (one per enabled
  app, starting with the 7 already enabled), `emulator-build.yml` (LIFE suite).
- **Desired behavior:** open→key→home→wait≥17 frames→assert Menu, ×5, exit 0.
- **Non-goals:** leak quantification (LIFE-02).
- **Dependencies:** APPQA-01. **Tests:** self. **Golden:** none.
  **Persistence:** none (loop apps in wave 0 don't write). **Firmware:** none.
- **Rollback:** drop suite. **Risk:** low (may *reveal* real bugs — that is the
  point; triage as app bugs, not harness bugs). **Owner:** Sonnet.

### STORE-01 — Per-app persistence inventory guard
- **Motivation:** each enabled app must declare its storage surface before it
  can pollute the fixture root; the sandbox spec's §A.4 parity register is the
  ledger, this ticket wires enforcement.
- **Evidence:** app write sites and their **native behavior class** (matrix doc
  §cross-app table): ARDUINO-gated → native no-op today:
  `PythonApp.cpp:657-698` (`/py`), `CircuitCoreApp.cpp:1363,1707-1708`
  (`/circuits`), `Fluid2DApp.cpp:1453,1484,1516` (`/fluid`),
  `ParticleLabApp.cpp:409,427` (`/save.pt`). **Writes natively already**:
  `NeuralLabApp.cpp:950-967` (`/neural.nn`), `NeoLanguageApp.cpp:897-944`
  (`/neolang.nl`), plus NeoIO's raw `std::fopen` cwd bypass
  (`NeoIO.cpp:62-119`). Live store: `VariableManager` `/vars.dat`
  (`VariableManager.cpp:169-190`); FS root `hal/FileSystem.cpp:38`. The
  inventory must record which class each surface is in, because "gated no-op"
  apps need an explicit un-gate-or-register-divergence decision at enablement.
- **Files:** `tests/emulator/app_inventory.json` (persistence field),
  `scripts/check-emulator-app-routes.py` (cross-check: an enabled app whose
  sources match `LittleFS.open(...,"w")` must have inventory rows), sandbox
  spec §A.4 table updates per wave.
- **Desired behavior:** enabling an FS-writing app without inventory rows and
  without FIX-01 sandbox flags available fails CI.
- **Non-goals:** implementing the sandbox itself (FIX-01/02/03 own that).
- **Dependencies:** APPPAR-00; FIX-01 for the runtime half. **Tests:** negative
  control = synthetic app with undeclared write. **Golden:** none.
  **Persistence impact:** ledger only. **Firmware:** none.
- **Rollback:** disable the cross-check. **Risk:** low. **Owner:** Sonnet.

### ROUTE-02 — App route negative controls
- **Motivation:** prove the harness can fail: dead-card refusal, broken hooks,
  half-wired routes.
- **Evidence:** dead-card path `NativeHal.cpp:993-995`; QA plan §H.
- **Files:** `tests/emulator/scripts/route_dead_card_control.numos` (+ retired/
  repointed as cards come alive), CI step running ROUTE-05 against a bad
  fixture, hook negative controls per APPQA-03.
- **Desired behavior:** structured `[APP] launch-refused id=<n>` log line added
  to the default branch of `launchApp`; control script asserts Menu-after-ENTER
  on a dead card; inverted-gate wiring in CI (expect exit 4 / expect fail).
- **Non-goals:** changing dead-card UX (SB-06 owns dimming).
- **Dependencies:** ROUTE-05, APPQA-01. **Tests:** self. **Golden:** none.
  **Persistence:** none. **Firmware:** none (NativeHal-only log line).
- **Rollback:** drop step. **Risk:** low. **Owner:** Sonnet.

---
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

## Wave 1 — low-risk static/UI apps

### APPPAR-04 — Enable Matrices
<<<<<<< HEAD
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
=======
- **Motivation:** cleanest candidate: pure LVGL + `<cmath>`, no CAS/Arduino/FS/
  rand/timers; same profile as already-enabled Stats/Prob; establishes the
  wave-1 template.
- **Evidence:** includes only lvgl/StatusBar/KeyCodes/KeyboardManager/
  MatrixEngine (`MatricesApp.h:31-35`); static `double data[5][5]`
  (`MatrixEngine.h:41`); `end()` order StatusBar→screen
  (`MatricesApp.cpp:118-141`); no persistence/timers/rand (matrix doc §
  Matrices).
- **Files:** `platformio.ini` (+`apps/MatricesApp.cpp`, `apps/MatrixEngine.cpp`),
  `src/hal/NativeHal.cpp` (8 sites, card id 9, AppMode `MATRICES`),
  scripts `matrices_smoke/edit/ops`, generator entries, CI suite line,
  `docs/emulator-sdl2-quickstart.md`.
- **Desired behavior:** P3 at merge (smoke + `assert_matrices_cell` +
  `assert_matrices_result`), P5 in APPQA-04 wave.
- **Non-goals:** matrix math extensions; SHIFT/ALPHA additions (app doesn't use
  the modifier FSM today).
- **Dependencies:** Wave 0 complete. **Tests:** QA plan §B rows + LIFE-01 loop
  + hook negative control. **Golden impact:** 2 new pending stems.
  **Persistence:** none. **Firmware impact:** none (app source untouched except
  the NATIVE_SIM hook block).
- **Rollback:** remove whitelist lines + NativeHal cases (scripts become
  load-errors — delete with them).
- **Risk:** low. **Owner:** Sonnet.

### APPPAR-05 — Enable Periodic Table
- **Motivation:** second zero-engine app; first emulator coverage of the
  `LV_EVENT_DRAW_MAIN` custom-draw path; exact-rational ChemCAS balancer gets
  its first automated oracle.
- **Evidence:** constexpr DBs (`ChemDatabase.h:51,172,186`; `ChemExtraData.h:46,
  167`; no PROGMEM reads); draw callback (`PeriodicTableApp.cpp:204,515-599`);
  extra `resetPointers()` teardown nuance (`PeriodicTableApp.cpp:106-111`);
  firmware exits on MODE **or AC** (`SystemApp.cpp:579-585`) — emulator must
  replicate; SOLVE key consumed (`PeriodicTableApp.cpp:870`).
- **Files:** `platformio.ini` (+`apps/PeriodicTableApp.cpp`, `apps/ChemCAS.cpp`),
  NativeHal 8 sites (card id 11, name "Chemistry"), `solve` script token
  (ROUTE-08, consumer-cited), scripts per QA plan §B, generator/CI entries.
- **Desired behavior:** P3 at merge (selection + balancer hooks); grid + modal
  goldens pending.
- **Non-goals:** ChemCAS correctness expansion (its corpus is future QA).
- **Dependencies:** Wave 0. **Tests:** QA plan rows; AC-exit parity script.
  **Golden impact:** 2 pending stems. **Persistence:** none.
  **Firmware impact:** none beyond hook block.
- **Rollback:** as APPPAR-04. **Risk:** low. **Owner:** Sonnet.

### APPQA-02 — Golden candidate generation for wave-1/2 apps
- **Motivation:** register all new stems; keep the pending ledger honest.
- **Evidence:** generator list structure (`generate-emulator-candidates.py:
  39-143`); warning-only compare (`emulator-build.yml:575-607`); CI-05 pending
  classification.
- **Files:** `scripts/generate-emulator-candidates.py`,
  `tests/emulator/golden/PENDING.md` (new, per CI-05).
- **Desired behavior:** every new stem generates in CI, classified
  gated/pending/assert-only; screenshot framing policy (no spinner/rand-caption
  frames) enforced by review.
- **Dependencies:** APPPAR-04/05 (and later -02/-01). **Tests:** generator run
  + two-run self-diff. **Golden impact:** candidates only. **Rollback:** remove
  entries. **Risk:** low. **Owner:** Sonnet.

### APPQA-03 — Semantic assert hooks (wave-1 + generic)
- **Motivation:** hook-before-golden rule; only Calculation/MainMenu have hooks
  today (`CalculationApp.h:95-107`, `MainMenu.h:58-79`).
- **Evidence:** hook table QA plan §D; script parser append points
  (`NativeHal.cpp:1457-1622`).
- **Files:** app headers (`#ifdef NATIVE_SIM` blocks), `NativeHal.cpp` (new
  ScriptCmdType arms), negative-control scripts.
- **Desired behavior:** `assert_matrices_*`, `assert_periodic_*`, generic
  `assert_app_ready` (ROUTE-06); each with a negative control; grammar stays
  append-only, parse-validated.
- **Dependencies:** APPPAR-04/05. **Tests:** controls exit 4. **Golden:** none.
  **Persistence:** none. **Firmware impact:** none (NATIVE_SIM-stripped).
- **Rollback:** hooks are additive; remove arms+blocks. **Risk:** low-medium
  (parser is a CI contract — append-only discipline). **Owner:** Sonnet, parser
  arms reviewed by Opus-tier.

---

## Wave 2 — math-renderer/CAS apps

### APPPAR-02 — Enable Calculus
- **Motivation:** first CAS UI in the emulator; unblocks CAS corpus rows tagged
  "once Calculus is emulator-enabled per P-05"
  (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:94`).
- **Evidence:** Arduino-clean (no Arduino/ESP includes — full-source audit,
  matrix doc); needs `SymDiff.cpp`, `SymIntegrate.cpp`, `SymToAST.cpp` +
  link-verified transitive closure (`CalculusApp.h:52-60`, `CalculusApp.cpp:31`
  vs `platformio.ini:252-258`); `rand()` captions (`CalculusApp.cpp:449-453`);
  inline `lv_timer_handler()` in `showComputing()` (`CalculusApp.cpp:464`) —
  legal in the emulator loop but must not re-enter (verify once running);
  `_stepRenderers` not cleared in `end()` (`CalculusApp.cpp:129-169` — cleared
  per-compute at `:786,1031`) — watch in LIFE-01 loop.
- **Files:** `platformio.ini` (app + closure; closure fixed by `g++ -M`/link
  iteration per ground truth §Q), NativeHal 8 sites (card id 3), scripts per
  QA plan §B, hooks per §D, EMUDET-03 seed fence must be green first.
- **Desired behavior:** P3 at merge (result hooks); input-screen golden pending;
  spinner frames never captured.
- **Non-goals:** SymDiff/SymIntegrate correctness fixes (CAS tickets own);
  Giac anything.
- **Dependencies:** Wave 0, EMUDET-03, APPQA-03 pattern. **Tests:** diff/int
  semantic scripts + negative controls + LIFE-01 + long-expression stress.
  **Golden impact:** 1 pending stem initially. **Persistence:** none.
  **Firmware impact:** none beyond hook block.
- **Rollback:** remove whitelist+cases; closure files stay harmless if unused
  (prefer full revert to avoid dead weight).
- **Risk:** medium (closure size unknown until linked; cas:: arena lifetime
  MEM-2 now exercisable under host sanitizers — an upside). **Owner:** Sonnet
  build/routing, Opus-tier review on cas:: closure and hooks.

### ROUTE-04 — App update() dispatch in emulator loop
- **Motivation:** firmware ticks Equations (staged solve pipeline) and Fractal;
  without a tick, Equations' solve never advances in the emulator.
- **Evidence:** `SystemApp.cpp:265-269, 290-292`; emulator loop has no app tick
  (`NativeHal.cpp:2045-2130`); Equations pipeline `EquationsApp.cpp:1059-1631`.
- **Files:** `src/hal/NativeHal.cpp` (main loop, one guarded block per ticked
  app).
- **Desired behavior:** tick exactly the apps firmware ticks, same guard
  conditions (`isActive()`), after `lv_timer_handler()`.
- **Non-goals:** inventing ticks for un-ticked apps.
- **Dependencies:** none hard; lands with APPPAR-01. **Tests:**
  `equations_linear_smoke` is the proof. **Golden:** none. **Persistence:**
  none. **Firmware:** none. **Rollback:** remove block. **Risk:** low-medium
  (loop ordering vs deferred teardown — tick must not run for a mode pending
  teardown). **Owner:** Sonnet, Opus review.

### APPPAR-01 — Enable Equations
- **Motivation:** highest-value enablement: 3350-line untested CAS UI
  (EMU-5/PRD-3); MEM-6 leak canary becomes automatable.
- **Evidence:** unconditional `Arduino.h`/`esp_heap_caps.h`/FreeRTOS includes +
  `heap_caps_*` step budget (`EquationsApp.cpp:34,44-46,487-518`) — none
  shimmed (`ArduinoCompat.h` audit); largest cas:: closure (SingleSolver/
  SystemSolver/OmniSolver/SymToAST/RuleEngine/AlgebraicRules/CasToVpam/
  CasMemory/SystemHeuristics/SystemTutor/PersistentAST/TutorTemplates/
  PedagogicalLogger + transitive; `EquationsApp.h:37-52`); `rand()` caption
  (`EquationsApp.cpp:106`); `end()` clears steps + arena
  (`EquationsApp.cpp:400-465`); firmware forwards keys only when `isActive()`
  (`SystemApp.cpp:513-519`).
- **Files:** `src/apps/EquationsApp.cpp` (**guard blocks only**: `#ifdef
  ARDUINO` around the three includes and `canAllocStep()/logStepBudget()` with
  a fixed synthetic native budget — open decision 2 in the parity spec),
  `platformio.ini` (largest closure), NativeHal 8 sites + ROUTE-04 tick
  (card id 2), scripts/hooks per QA plan (incl. `assert_equations_*`,
  `debugPipelineIdle`, steps-leak canary).
- **Desired behavior:** P3 at merge; equation-list golden pending; solve flows
  assert-only; enable complex-settings honesty flip (SET §D.2 "P-05 makes it
  real") in coordination with ST-03.
- **Non-goals:** solver correctness (CAS corpus owns); tutor UX changes.
- **Dependencies:** ROUTE-04, EMUDET-03, FIX-01 (recommended), APPPAR-02 landed
  (closure overlap reduces risk). **Tests:** QA plan §B/§D/§E rows.
  **Golden impact:** 1 pending stem. **Persistence:** none. **Firmware
  impact:** the `#ifdef ARDUINO` guards touch firmware-compiled code — firmware
  build must be compile-verified in the same PR (P-09 compile check helps).
- **Rollback:** revert guards + whitelist + cases; guards are semantically
  no-op for ARDUINO so firmware rollback risk is minimal.
- **Risk:** medium-high (guard correctness on firmware; closure link size;
  pipeline-vs-teardown interaction). **Owner:** Opus-tier implementation,
  Human sign-off on the native budget semantics.

### APPPAR-03 — Integral: resolve as do-not-enable
- **Motivation:** the ticket list requires disposition; evidence says dead code.
- **Evidence:** no SystemApp member/dispatch/no card (`SystemApp.h:138-160`,
  `SystemApp.cpp:885-913`, `MainMenu.cpp:88-113`); ground truth §F "dead
  (merged into CalculusApp)".
- **Desired behavior:** no emulator enablement (would create emulator-only
  reachability, violating parity philosophy §I). Disposition: P-07 dead-code
  sweep deletes or firmware re-wires; only if firmware ever re-wires does an
  enablement ticket reopen.
- **Files:** matrix doc row + P-07 cross-reference; no code.
- **Dependencies:** P-07. **Risk:** none. **Owner:** Human decision recorded;
  Haiku-class doc update.

### APPPAR-07 — Math Visual / Showcase disposition
- **Motivation:** required disposition; avoid a duplicate route.
- **Evidence:** emulator MathShowcase (id 100) already reuses accepted
  `MathRenderVisualCases` (`NativeHal.cpp:1090-1222`); firmware Math Visual is
  validate-env-only (`SystemApp.h:62-65`, `platformio.ini:116-121`).
- **Desired behavior:** keep both as-is; extend showcase case list only via
  `MathRenderVisualCases.cpp` (already whitelisted, `platformio.ini:250`);
  add `math_showcase_delims_smoke` candidate registration (script exists,
  no golden) if delimiter coverage is wanted.
- **Files:** generator entry only. **Dependencies:** none. **Risk:** low.
  **Owner:** Haiku-class.

### APPPAR-08 — Enable StixGlyphGallery (diagnostic route)
- **Motivation:** REN-2 (missing-glyph tofu) has no automated oracle; the
  gallery renders the font inventory.
- **Evidence:** diag-only behind `NUMOS_STIX_DIAGNOSTICS`
  (`main.cpp:49-52,187-194` per ground truth §F); `ui/StixGlyphGallery.*` not
  whitelisted; not a launcher card; **not an app class** — two free functions
  (`StixGlyphGallery.h:24,27`). Blockers: unconditional `#include <Arduino.h>`
  (`StixGlyphGallery.cpp:18`, no native Arduino.h exists) and the gallery's
  blocking `millis()` busy-loop (`:165-169`).
- **Files:** `src/ui/StixGlyphGallery.cpp` (guard the include to
  ArduinoCompat), `platformio.ini` (+`ui/StixGlyphGallery.cpp`), NativeHal:
  showcase-style wrapper mode that builds the panel without the busy-wait
  (`open_app StixGallery`, no launcher card — mirrors id-100 pattern with a
  new non-card id, e.g. 101), `stixgallery_smoke` script + candidate.
- **Desired behavior:** P2 + pending golden per gallery page; additionally
  wire `runStixGlyphAlignmentDiagnostics()` (`StixGlyphGallery.cpp:57-112`)
  as a text-assert CI check (pass/fail line). Explicitly an emulator
  diagnostic (documented emulator-only surface, allowed because it is harness,
  not product UI — parity spec §I).
- **Non-goals:** launcher card on either target.
- **Dependencies:** Wave 0. **Tests:** smoke + self-diff. **Golden impact:**
  pending stems. **Risk:** low. **Owner:** Sonnet.

### APPQA-04 — Golden promotion wave 1
- **Motivation:** convert accumulated pending stems (Matrices ×2, Periodic ×2,
  Calculus ×1, Equations ×1, gallery) to blessed goldens in one reviewed batch.
- **Evidence:** promotion mechanics `golden/README.md:31-113`,
  `promote-emulator-golden.py` behavior (refuse-without-force, never commits).
- **Files:** `tests/emulator/golden/*.ppm`, `masks/*.mask` (clock rect only),
  `PENDING.md` rows closed.
- **Desired behavior:** human reviews each candidate against semantic
  correctness; masks limited to `4,6,37,13`; coordinated with any SB-06
  launcher re-bless.
- **Dependencies:** APPQA-02/03, apps landed. **Golden impact:** the wave
  itself. **Risk:** medium (EMU-2 — bad bless normalizes regressions; mitigated
  by review checklist). **Owner:** Human (mandatory), prepared by Sonnet.

### LIFE-02 — Timer-active teardown stress + object-leak probe
- **Motivation:** teardown while `lv_timer`s live is the highest-severity
  lifecycle class; and enter/exit loops need a leak signal.
- **Evidence:** timer sites (`GrapherApp.cpp:1508,2994`; wave-3 apps
  `BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `OpticsLabApp.cpp:253`,
  `ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `Fluid2DApp.cpp:257`);
  firmware MEM probe precedent (`SystemApp.cpp:213-217`).
- **Files:** `NativeHal.cpp` (`assert_lvgl_object_count[_between]` command,
  warning-only mode first), `tests/emulator/scripts/life_*_timer_exit.numos`,
  CI suite.
- **Desired behavior:** per timer-owning app: arm timer → immediate home →
  relaunch → assert; loop object-count delta reported.
- **Dependencies:** LIFE-01, ROUTE-03; extends per wave-3 app as enabled.
  **Golden:** none. **Risk:** medium (count brittleness — keep warning-only
  until stable). **Owner:** Sonnet.

---

## Wave 3 — storage/timer/simulation apps (gated on FIX-02)

### APPPAR-09 — Sim/lab apps: split enablement
Disposition ticket splitting into sub-tickets; each reuses the ROUTE-01
checklist and QA plan patterns. Order and gates:

- **APPPAR-09a — Optics + Bridge** (timers only, no FS — grep-clean audit):
  Optics first — its 33 ms timer only re-renders a static scene
  (`OpticsLabApp.cpp:587`), making it the best sim golden candidate; Bridge's
  16 ms Verlet stepper is fixed-DT deterministic (`BridgeDesignerApp.h:98`).
  Smoke at fixed deterministic frame counts; goldens only if two-run self-diff
  proves byte-stability of the chosen frame; otherwise assert-only.
  Risk medium. Owner Sonnet.
- **APPPAR-09b — Circuit, Fluid2D, ParticleLab** (timers + LittleFS
  autosaves): their FS code is **ARDUINO-gated (native no-op today)**
  (`CircuitCoreApp.cpp:1707-1708`, `Fluid2DApp.cpp:1453`,
  `ParticleLabApp.cpp:409`) — each enablement must decide: un-gate (needs
  `File::printf` shim for Circuit's text format + FIX-02 sandbox) or register
  a no-persistence divergence in the sandbox §A.4 table. Emulator must
  replicate the SystemApp-side autoSave-on-exit hooks
  (`SystemApp.cpp:595-615`) — divergence class D8 becomes real here. Circuit
  also carries the LuaVM-stub product question (ground truth §F); Fluid2D's
  timer is always-on from createUI (`Fluid2DApp.cpp:257`). ParticleLab
  additionally needs a NATIVE_SIM `resetRng()` hook — its engine xorshift is
  fixed-seed but never re-seeded across relaunch
  (`ParticleEngine.cpp:84-95,147`). Risk medium-high. Owner Sonnet + Opus
  review on exit hooks.
- **APPPAR-09c — NeuralLab** (rand()-core + **native FS write**): its
  `/neural.nn` save/load is NOT gated and writes `emulator_data/` natively
  today (`NeuralLabApp.cpp:950-967`) — hard FIX-01 dependency before the app
  is even compiled in. Pixel goldens blocked on a seeded-RNG debug hook
  (weights `NeuralEngine.cpp:140-141`, UI jitter `NeuralLabApp.cpp:372-380`);
  assert-only until seeding lands. Risk high for pixels, medium for asserts.
  Owner Sonnet.
- **APPPAR-09d — Fractal**: needs ROUTE-04 tick (`SystemApp.cpp:290-292`) and
  the AC-exit + `consumeExitRequest()` contract (`SystemApp.cpp:649-658`)
  replicated. Risk medium. Owner Sonnet.
- **NeoLang**: explicitly out of APPPAR-09; future wave. Three gates: it
  saves/loads `/neolang.nl` natively already (`NeoLanguageApp.cpp:897-944`);
  its NeoIO `file()` builtin uses raw cwd-relative `std::fopen` on native,
  escaping the LittleFS root entirely (`NeoIO.cpp:62-119`) — a sandbox bypass
  that must be routed through `hal/FileSystem` first; and its text-editor
  input surface makes scripts unwieldy.

Common fields: **Tests** QA plan §B/§E sim rows; **Golden impact** self-diff-
gated; **Persistence impact** sandbox §A.4 rows per app (STORE-01 enforced);
**Firmware impact** none (plus exit-hook parity code in NativeHal);
**Rollback** per-app revert.

### STORE-02 — Storage roundtrip suites for wave-3 apps
- **Motivation:** persistence parity is behavior, not just paths.
- **Evidence:** two-process protocol (sandbox spec §C); app formats
  (`CircuitCoreApp.cpp:1360-1500`, `Fluid2DApp.cpp:1446-1518`,
  `ParticleLabApp.cpp:406-431`, `NeuralLabApp.cpp:950-967`).
- **Files:** runner scripts + CI steps; fixtures under
  `tests/emulator/fixtures/<name>/` (committed, human-blessed).
- **Desired behavior:** write→read roundtrip per app in sandbox; corruption rows
  align with FIX-05 matrix.
- **Dependencies:** FIX-02/FIX-04, APPPAR-09b. **Risk:** medium.
  **Owner:** Sonnet.

---

## Wave 4 — deferred/decision-gated

### APPPAR-06 — Python: defer with explicit decision gate
- **Motivation:** required disposition. Two blockers: (1) every boot seeds
  `/py/hello.py` and the app lists/creates/deletes scripts
  (`PythonApp.cpp:340,470,657-698`) — needs sandbox + directory listing which
  `hal/FileSystem` doesn't implement (no `openNextFile`/`mkdir`,
  `FileSystem.cpp:52-88` audit) — a real HAL work item; (2) the engine is a
  custom mini-interpreter, not MicroPython (ground truth §F) and its product
  fate is undecided.
- **Desired behavior:** DEFER until Human decides the interpreter question and
  FIX-02 + FileSystem directory-API extension land. Then enable at P3 with
  interpreter-output hooks (`assert_python_output`), goldens for the IDE chrome
  only.
- **Files (when opened):** `hal/FileSystem.*` (dir ops), whitelist, NativeHal,
  scripts. **Risk:** medium. **Owner:** Human decision → Sonnet.

### PORT-06 — Cross-OS drift re-measure after CAS-app goldens
- **Motivation:** CAS UIs render double-math-derived layouts; PORT-05's drift
  measurement must be re-run once wave-2 goldens exist, before any non-Linux
  golden claims.
- **Evidence:** Linux-reference-only policy (CI/golden policy spec §I);
  PORT-05 definition (host portability spec §G).
- **Files:** portability workflow matrix. **Dependencies:** APPQA-04, PORT-05.
  **Risk:** low. **Owner:** Sonnet.

---

## Dependency graph (summary)

```
FIX-01/FIX-03/CI-01/EMUDET-03/CI-04 (external, Wave-0 pull-ins)
        │
APPPAR-00 → ROUTE-01 → ROUTE-05 → ROUTE-02
        │       │
        │   APPQA-01 → LIFE-01
        │       │
        │   STORE-01
        ▼
APPPAR-04 ─┬→ APPQA-02 → APPQA-03 → APPQA-04 (Human)
APPPAR-05 ─┘
        ▼
APPPAR-02 → ROUTE-04 → APPPAR-01 → LIFE-02
        │                        (APPPAR-03/-07/-08 parallel, low-cost)
        ▼
FIX-02 → APPPAR-09a → APPPAR-09b → STORE-02 → APPPAR-09c/09d
        ▼
APPPAR-06 (Human gate) · PORT-06
```

ROUTE-03 lands any time (independent, trivial).
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc
