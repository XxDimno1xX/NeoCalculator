# NumOS — Global Settings State & Persistence Specification

> **What "done" means:** every global setting has exactly one authoritative in-memory home (`SettingsState`), one persistence record with version+CRC, one recovery policy, and zero divergent copies. Angle mode is the flagship case. Companion docs: `NUMOS_SETTINGS_APP_PRODUCT_SPEC.md` (SET), `NUMOS_STATUSBAR_TRUTHFULNESS_AND_SYSTEM_CHROME_SPEC.md` (SBAR), `NUMOS_SYSTEM_CHROME_QA_ORACLE_AND_GOLDEN_PLAN.md` (SCQA), `NUMOS_SYSTEM_CHROME_IMPLEMENTATION_ROADMAP.md` (SCROAD). Persistence rules inherit and must not contradict `NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md` (RT-13 / invariant R-3) and `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md` (`[FS]` tag grammar).
>
> Audit base: branch `claude/numos-system-chrome-spec-0gtoxn` @ `5fc5ea3` (2026-07-04). Notation: **[V]** verified with `file:line` · **[P]** proposed · **PD-Gn/OQ-Gn** would collide with the Grapher spec's namespace, so this doc uses **PD-GSn / OQ-GSn**.

---

## A. Current global state map [V]

### A.1 Angle mode — three stores, two enums, zero writers

| State | Type | Default | Written by | Read by |
|---|---|---|---|---|
| `vpam::g_angleMode` | `vpam::AngleMode {DEG=0, RAD=1}` (`MathEvaluator.h:70-73`) | `RAD` (`MathEvaluator.cpp:53`) — contradicting the enum's own "DEG por defecto" comment (`MathEvaluator.h:71`) | **nobody** (grep-verified: zero assignments in the tree) | trig eval (`MathEvaluator.cpp:849-896`), StatusBar badge (`StatusBar.cpp:246`) |
| `SystemApp::_angleMode` | legacy `AngleMode {RAD=0, DEG=1}` (`Evaluator.h:32-35`) | `DEG` (`SystemApp.cpp:79`) | ctor only | pushed once into `_evaluator` + `_equationSolver` (`SystemApp.cpp:96-97`); legacy TFT header (`SystemApp.cpp:372`) |
| `Evaluator::_angleMode` (per instance) | legacy enum | `RAD` (`Evaluator.cpp:25`) | `setAngleMode()` (`Evaluator.h:47`) — called only from `SystemApp.cpp:96` and `EquationSolver.cpp:358`; **never** for the Grapher's `GraphModel::_eval` (`GraphModel.h:172`, grep-verified) | `toRadians` (`Evaluator.cpp:42`) |

**The two enums have inverted numeric values.** `vpam::AngleMode::DEG == 0 == legacy AngleMode::RAD`. Any integer/`static_cast` bridge silently flips the mode. This is why every sync in this spec is **by name** (§D.2).

Effective behavior today: VPAM math and the badge are frozen RAD; the Grapher is frozen RAD by a *different* accident (unset per-instance default); SystemApp's legacy evaluator and EquationSolver are frozen DEG; nothing is user-changeable. (SET §E.2 has the per-surface table.)

### A.2 The `setting_*` trio

Declared `Config.h:77-79`; defined twice — firmware `main.cpp:31-33`, emulator `NativeHal.cpp:125-127` (defaults kept equal only by comment discipline, `NativeHal.cpp:117-124`). Consumers: `setting_edu_steps` → `CalculationApp.cpp:578`; `setting_complex_enabled` → `SingleSolver.cpp:330` (firmware-only — not in emulator whitelist `platformio.ini:195-266`); `setting_decimal_precision` → **none** (SET §C.1). Written only by SettingsApp (`SettingsApp.cpp:241-315`). Never persisted (fault-containment C6.7 concurs).

### A.3 Input-modifier state (adjacent, for completeness)

`vpam::KeyboardManager` singleton FSM (`KeyboardManager.h:47-54`, transitions `KeyboardManager.cpp:42-130`) is correctly single-instance, but SystemApp keeps parallel dead booleans `_shiftActive/_alphaActive` (`SystemApp.h:171-172`, toggled `SystemApp.cpp:484-493`, read nowhere meaningful). Divergence: emulator `returnToMenu()` resets the FSM (`NativeHal.cpp:1081`); firmware's does not (`SystemApp.cpp:860-883`) → a lock engaged in an app persists invisibly on the firmware launcher. Owned by UX-02 + RT-14; SettingsState does **not** absorb modifier state (input state ≠ setting), but SB-04 depends on the reset landing.

### A.4 SystemApp / MainMenu state relevant to chrome

`SystemApp::_apps` legacy vector (`SystemApp.cpp:162-179`) is the stale non-LVGL menu model (comment "11-18 hidden" at `:175` is misleading — the real launcher is `MainMenu::APPS[]`, `MainMenu.cpp:88-113`). Launcher focus lives in the LVGL group (`MainMenu.cpp:159-197`), exposed read-only to scripts via `debugFocusedCardId()` (`MainMenu.h:58-79`). No settings live here; listed to mark them out of SettingsState scope.

### A.5 Variable stores (persistence neighbors, not settings)

- `vpam::VariableManager` — singleton, `ExactVal`, LittleFS `/vars.dat`, magic `VR01`, no CRC, non-atomic `"w"` writes; loaded at boot (`SystemApp.cpp:144-156` firmware; `NativeHal.cpp:2171-2179` emulator); saved on STO (`CalculationApp.cpp:900`).
- `VariableContext` — per-instance `double` store, NVS namespace `calcVars`; `saveToNVS()` has zero callers (write-dead).
These stay under CA-05/RT-13/P-04 ownership. SettingsState must **not** merge with them; but it must share the RT-13 write discipline.

### A.6 Emulator-only globals

`g_detTick` synthetic clock (`NativeHal.cpp:189-190`, tick source selection `:1998`), `g_opts` CLI state (`:164-177`), `g_mode` app mode (`:215`), teardown latches (`:230-233`). None persist; none are settings. The settings globals in the emulator are process-lifetime — each `.numos` run starts from compile-time defaults [V], which is the deterministic property §F.1 preserves.

---

## B. Persistence model

### B.1 Current state [V]

**Settings: nothing is persisted.** Every boot re-initializes the three globals (`main.cpp:31-33`). There is no settings file, no NVS namespace for settings, no save call. Adjacent hazards that a settings store must not repeat: format-on-fail mounts (`SystemApp.cpp:144`, `NativeHal.cpp:2172` — both `LittleFS.begin(true)`), unvalidated record bodies and non-atomic writes in `/vars.dat` (fault-containment C6.1-C6.4), and a **git-tracked emulator store** (`emulator_data/vars.dat` is committed; `git ls-files` confirms) that is loaded at every emulator boot and rewritten by any STO — §F.3.

### B.2 Desired state [P → GS-02]

One record, one writer, one loader:

- **Record content (v1)**: `version:u8`, `angle:token(deg|rad)`, `complex:bool`, `precision:u8∈{6,8,10,12}`, `edu_steps:bool`, `crc32` over all prior bytes. Stored as key-value text or packed struct — packed struct chosen (PD-GS1) for CRC simplicity and to match the `VR02` direction RT-13 sets for vars.
- **Load** once at boot, before the first app `load()` (firmware: in `SystemApp::begin()` before app construction uses settings; emulator: in `nativeFS_init()` path, `NativeHal.cpp:2171-2179` region). Loader validates version, CRC, and every field's domain; any failure → whole-record rejection (§G).
- **Save** via SettingsState commit (SET §G.4 debounce), write-temp-then-rename where the backend is a file, or per-key transactional writes where the backend is NVS.

### B.3 Storage backend & layout (OQ-S4 resolution proposal)

**Firmware: NVS (`Preferences`), namespace `numos_set`**, keys `ver`, `angle`, `cplx`, `prec`, `edu`, `crc` (PD-GS2, ratification human):
- NVS writes are internally atomic/wear-leveled — the atomic-write rule (R-3/C6.3) is satisfied by the backend instead of hand-rolled temp-rename.
- Settings survive the existing LittleFS format-on-fail hazard and any future `!fs format`.
- Cost: a second storage API in the settings path; NVS is already linked (VariableContext uses `Preferences`).
- Rejected alternative (a LittleFS `/settings.dat`): couples settings survival to the store with the reformat foot-gun; only advantage is one backend for vars+settings, which P-04/RT-13 may deliver later anyway.

**Emulator**: `hal/FileSystem` file `/settings.dat` under `./emulator_data/` — but **only** when `--persist-settings` is passed (default: no load, no save). §F.1 explains why default-off.

### B.4 Reset behavior

`Reset settings` row (SET §D.2) and `!settings reset CONFIRM` diag command (extends the RT-04 console grammar; two-step confirm like `!vars reset`): write defaults to SettingsState, then persist. Never touches `/vars.dat`, NVS `calcVars`, or any app file (R-3: user data is not settings). Safe-mode "reset settings" (safe-mode spec §6) becomes non-vacuous once this exists.

### B.5 Migration / versioning

`version` byte starts at 1. Unknown *higher* version → treat as corrupt (quarantine §G; a downgrade may not half-read a future record). Adding a field = bump version + loader accepts N and N−1 (one generation of migration, defaults for missing fields). Removing/repurposing a field = new version, no reuse of old byte offsets within two generations. Enum-valued fields are stored as **name tokens** so C++ enum reorderings can never corrupt meaning (§D.4).

---

## C. Single source of truth — `SettingsState` [P → GS-02]

### C.1 Shape

`src/system/SettingsState.h/.cpp` (new; namespace `numos`). A small static singleton owning the canonical values; the legacy `extern` globals remain as **read-compatibility aliases** during migration (references, or accessors the old names forward to), then are retired. Zero LVGL/app includes — apps depend on it, never the reverse.

### C.2 Read API

`SettingsState::angleMode()`, `complexEnabled()`, `decimalPrecision()`, `eduSteps()` — trivial inline reads. Hot paths (trig eval) keep reading `vpam::g_angleMode` directly; GS-02 makes that global *owned* by SettingsState (defined in its TU, written only through it) rather than replacing every read site — minimal churn, same authority.

### C.3 Write API

`SettingsState::setAngleMode(vpam::AngleMode)`, `setComplexEnabled(bool)`, `setDecimalPrecision(int)`, `setEduSteps(bool)`. Each: validates domain → mutates → runs the sync fan-out (C.5) → marks dirty for the debounced persist (B.2). Writers: SettingsApp rows, `.numos` `set_*` hooks, diag console. Nothing else assigns the underlying storage (CI grep enforces, SCQA §G.4).

### C.4 Subscription vs polling

**Polling/sync-points, no observer framework** (PD-GS3). NumOS is single-threaded around `lv_timer_handler`; the consumers are few and have natural sync points:
- StatusBar: repaints on the C.7 update policy (SBAR) — reads fresh state every `update()`.
- Grapher: `syncAngleModeFromSystem()` at `preCacheRPN`/`replot()` (GR-02).
- EquationSolver: sync at solve entry (replacing `SystemApp.cpp:96-97`).
- SettingsApp: reads on `updateValues()`.
A callback registry would add lifetime hazards (dangling subscriber on app teardown — exactly the UAF class the deferred-teardown work fought) for no observable latency win.

### C.5 Evaluator sync model — the fan-out

`setAngleMode` immediately: writes `vpam::g_angleMode`; nothing else. Legacy engines pull at their sync points via a single shared helper `numos::legacyAngleModeFromVpam()` that maps **by name** (§D.2). SystemApp's `_angleMode` member and its `begin()` pushes are deleted (GS-01). Rationale for pull-at-use over push-to-all: push requires SettingsState to know every engine instance (Grapher's evaluator is app-owned and lazily constructed, `GraphModel.h:172`); pull needs one line at each entry point that already exists.

---

## D. Angle mode — detailed contract [P → GS-01]

1. **Enum values**: `vpam::AngleMode{DEG,RAD}` stays the authoritative type (`MathEvaluator.h:70-73`). The legacy enum (`Evaluator.h:32-35`) survives only inside the legacy pipeline.
2. **No cast between the enums — ever.** The single mapping point is `legacyAngleModeFromVpam()` (switch on enumerators, exhaustive, no default-passthrough). CI guard greps for cross-namespace `static_cast` on `AngleMode` (SCQA §G.4). This rule is permanent, not transitional: the enums' inverted values (`DEG=0` vs `RAD=0`) make any integral bridge a silent mode flip.
3. **Default**: DEG (SET PD-S3; reversible constant, human ratifies at GS-01 review).
4. **Persistence**: token `deg|rad` in the settings record (B.2); unknown token → whole-record quarantine (§G).
5. **Tests**: SCQA §E.1 semantic scripts (sin(30) DEG/RAD; Grapher period; badge-equals-engine), §C hooks (`set_angle_mode`, `assert_angle_mode`, `assert_statusbar_angle`), §G.4 static guards, plus a host unit test for `legacyAngleModeFromVpam()` round-trip (name-level, both directions) modeled on `tests/host/keycode_digit_test.cpp`.

## E. Display mode — detailed contract

- **[V]** `vpam::ResultMode {Symbolic, Periodic, Extended}` (`MathEvaluator.h:81-85`) is per-CalculationApp *session* display state cycled by FREE_EQ; mode changes never mutate the stored value (CALC §E.3, CAS D.20.3). It is **not** a global and not persisted.
- **[P → PD-GS4]** It stays out of SettingsState this cycle: it is a view-state of one app, resets to Symbolic per expression flow, and persisting it would surprise (calculator boots into Extended). If a future "default display mode" setting is wanted, it enters as a *separate* SettingsState field feeding CalculationApp's initial state — never sharing storage with the live cycle state.
- `setting_decimal_precision` interaction: if OQ-S1 chooses to wire it, its consumers are the Periodic 12-sig-digit and Extended digit-count paths (`MathEvaluator.cpp:1198-1359`) — a CALC-owned change; GSTATE only guarantees the value's storage/validation.

---

## F. Emulator / headless behavior [P]

1. **Deterministic defaults.** Headless/deterministic runs boot with compile-time defaults, always — settings persistence is **opt-in** (`--persist-settings`, B.3). A `.numos` script's observable behavior must be a function of the script + binary, never of a previous run. (This is exactly the property `emulator_data/vars.dat` already violates for variables — see F.3.)
2. **Script commands.** `set_angle_mode deg|rad`, `set_setting_value <name> <value>`, `assert_setting_value <name> <value>`, `assert_angle_mode deg|rad` (grammar/naming in SCQA §C; NativeHal grammar is append-only, `NativeHal.cpp:1317-1362` region). All route through SettingsState — a hook that pokes a raw global would bypass the sync fan-out and test a state the UI can't produce.
3. **Reset between scripts.** Each script is a fresh process (the runner execs the binary per script; candidate generator too, `generate-emulator-candidates.py`), so in-memory settings reset for free. The flake source is on disk: `emulator_data/vars.dat` is git-tracked, loaded every boot (`NativeHal.cpp:2171-2179`), and rewritten by STO scripts (`CalculationApp.cpp:900`) — a store-script run leaves the working tree dirty and can alter later `assert_variable` outcomes. **Rule [P → QA-05]:** deterministic runs treat committed `emulator_data/` as read-only fixture — either `--fs-sandbox <tmpdir>` (copy-on-boot) or CI-enforced `git diff --exit-code emulator_data/` after suites. Settings persistence, being opt-in, never joins this problem.
4. **No hidden persistence, generalized.** Every emulator store must be: absent by default, or explicitly declared in the run's flags. Capability honesty (SET §H.4): the build defines which settings have live consumers; SettingsApp renders dead ones disabled rather than pretending.

---

## G. Corruption handling [P — inherits RT-13 verbatim where applicable]

1. **Checksum/version**: CRC32 + version byte on the record (B.2); NVS backend still stores `crc` over the value-set to catch partial key writes.
2. **Safe defaults**: any validation failure → compile-time defaults for **all** fields (whole-record rejection, SET PD-S6); boot continues; SettingsApp fully usable.
3. **No destructive auto-format**: never `begin(true)`; never overwrite the bad record in place. File backend: rename to `/settings.bad` (one generation — a second corruption overwrites the previous `.bad`). NVS backend: leave keys, write fresh values only on next commit.
4. **Diagnostics**: one line per event in the RT tag grammar — `[FS] settings=loaded ver=1`, `[FS] settings=corrupt action=quarantine`, `[FS] settings=missing action=defaults`, `[FS] settings=write-fail stage=commit`. Repeated corruption across ≥2 consecutive boots additionally logs `[SAFE] hint=settings` (informational; safe-mode *entry* remains governed by RT-03's own triggers — settings corruption alone must not brick into safe mode).
5. **Interaction with safe mode**: in safe mode, settings load is skipped like other persistence (R-5); SettingsState runs defaults; the Reset row still works (writes are allowed — resetting settings is a recovery action).

---

## H. Implementation tickets (headline; full fields in SCROAD)

- **GS-01** — Angle-mode single source of truth (§A.1, §C.5, §D). Refines/implements UX-01+CA-06; unblocks GR-02 rows.
- **GS-02** — SettingsState + persistence schema (§B, §C, §G; OQ-S4/PD-GS2 backend decision).
- **GS-03** — Emulator settings determinism & FS sandbox rule (§F.1, §F.3) — shared with QA-05.
- **GS-04** — Legacy state retirement: delete `SystemApp::_angleMode` + pushes (`SystemApp.cpp:79,96-97`), dead `_shiftActive/_alphaActive` booleans (`SystemApp.h:171-172`) after UX-02, stale `_apps` "hidden" comment (`SystemApp.cpp:175`).

Open: **OQ-GS1** — should `set_setting_value` accept *any* field generically vs one command per setting? Leaning generic name+token (one parser, append-only-friendly); consequence: value validation must live in SettingsState, which C.3 already requires.

*End of global settings state & persistence spec.*
