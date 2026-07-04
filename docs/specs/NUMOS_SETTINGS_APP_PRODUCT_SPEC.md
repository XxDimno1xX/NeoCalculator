# NumOS — SettingsApp Product Specification

> **What "done" means:** SettingsApp is the single, honest control panel for every global NumOS setting — every row it shows changes real behavior, every value it shows is the value the engines actually use, and (once GS-02 lands) every change survives a reboot. Companion docs: `NUMOS_STATUSBAR_TRUTHFULNESS_AND_SYSTEM_CHROME_SPEC.md` (SBAR), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE), `NUMOS_SYSTEM_CHROME_QA_ORACLE_AND_GOLDEN_PLAN.md` (SCQA), `NUMOS_SYSTEM_CHROME_IMPLEMENTATION_ROADMAP.md` (SCROAD).
>
> Audit base: branch `claude/numos-system-chrome-spec-0gtoxn` @ `5fc5ea3` (2026-07-04); all `file:line` citations verified against this tree.
>
> Notation: **[V]** = verified current behavior with `file:line` evidence · **[P]** = proposed normative behavior · **PD-Sn** = product decision recorded here · **OQ-Sn** = open decision with options and consequences. Ticket prefixes: ST-* (Settings), SB-* (StatusBar), GS-* (global settings state), QA-* (this spec pack's QA series — see SCROAD §0 namespace note). Existing core-apps tickets (UX-01, CA-06, GR-02, RT-13, RT-14, …) are cited by their own IDs.

---

## A. Executive summary — what SettingsApp must be

1. **Today** (§B–§C): SettingsApp is a 3-row LVGL panel ("Complex numbers", "Decimal precision", "Step-by-step mode") that mutates three process-global variables and persists nothing. Two of its three rows are partially or entirely **placebo**: `setting_decimal_precision` has **zero consumers** anywhere in the tree, and `setting_complex_enabled` has exactly one consumer (`cas::SingleSolver`) that is not even compiled into the emulator. There is no angle-mode row, no reset row, no system-info row, and no persistence.
2. **The product contract** (§D–§G): SettingsApp becomes the write-surface for a single `SettingsState` (GSTATE §C). Every visible row must satisfy the **efficacy rule**: a row may only be shown if changing it observably changes behavior on that build target, or if it is explicitly labeled unavailable. Angle mode (DEG/RAD) is the first new row and the highest-priority one (UX-01/CA-06/GS-01).
3. **Angle mode** (§E): one authoritative global — `vpam::g_angleMode` — written only by SettingsApp (and the test hook `set_angle_mode`), read by MathEvaluator, StatusBar, and synced by-name into the legacy `Evaluator` instances (Grapher, EquationSolver). The two `AngleMode` enums have **inverted numeric values**; casting between them is forbidden forever.
4. **SETUP key** (§F): currently a dead keycode. Product decision PD-S4: SETUP opens SettingsApp from any mode (quick, reversible, matches Casio muscle memory); until implemented it stays inert and documented as such.
5. **Failure honesty** (§H): corrupted or missing persisted settings must degrade to compile-time defaults with a diagnostic — never a crash, never a silent half-load, never a destructive reformat (inherits RT-13/R-3).

Out of scope this cycle: theme/contrast and language rows (nothing in the codebase backs them — see §D.3), clock-setting UI (SBAR §C.3 owns the clock contract), Diagnostics app (RT-15 owns it).

---

## B. Current architecture and code-path map [V]

### B.1 Class shape and lifecycle

- `SettingsApp` is an LVGL-native app with the standard `begin()/end()/load()/handleKey()` lifecycle (`src/apps/SettingsApp.h:34-45`). It owns a screen, a `ui::StatusBar` instance (`SettingsApp.h:54-55`), 3 rows of label/value pairs (`SettingsApp.h:47,59-61`), a hint label, and a focus index.
- `begin()` creates the screen and StatusBar with title "Settings" (`SettingsApp.cpp:70-82`); `load()` lazily calls `begin()`, resets focus to row 0, refreshes values, and fades the screen in over 200 ms (`SettingsApp.cpp:107-115`).
- `end()` calls `_statusBar.destroy()` **before** `lv_obj_delete(_screen)` and nulls all widget pointers (`SettingsApp.cpp:88-101`) — compliant with the deferred-teardown/StatusBar-before-screen contract (`SystemApp.cpp:181-208,238-250`).

### B.2 Firmware dispatch path

- Instantiated (not begun) at boot: `SystemApp.cpp:109`. Launcher card id 10 (`MainMenu.cpp:100`); `launchApp(10)` → `switchApp` → `_settingsApp->load()` (`SystemApp.cpp:794-798`).
- Key routing: SystemApp intercepts `KeyCode::MODE` and calls `returnToMenu()`; everything else forwards to `SettingsApp::handleKey` (`SystemApp.cpp:527-532`). SHIFT/ALPHA are consumed globally by SystemApp before Settings sees them (`SystemApp.cpp:484-493`) — they toggle the `KeyboardManager` FSM but SettingsApp never refreshes its StatusBar afterwards (see §G.6).
- Teardown: `returnToMenu()` records `APP_SETTINGS` as `_pendingTeardownMode` (`SystemApp.cpp:860-883`); `update()` calls `_settingsApp->end()` ≥250 ms later (`SystemApp.cpp:238-250,188`).

### B.3 Emulator dispatch path

- `SettingsApp` is in the emulator whitelist (`platformio.ini` emulator `build_src_filter`; include at `NativeHal.cpp:102`). Constructed lazily at splash→menu transition (`NativeHal.cpp:887`).
- `AppMode::SETTINGS` (`NativeHal.cpp:199`); `launchApp(10)` loads it (`NativeHal.cpp:979-985`); `dispatchKey` mirrors the firmware contract — MODE → `returnToMenu()`, rest forwarded (`NativeHal.cpp:647-663`).
- Scriptable: `open_app Settings` maps to id 10 (`NativeHal.cpp:1434`), `assert_app Settings` is a canonical name (`NativeHal.cpp:1409`). Deferred teardown mirrored (Phase 9F, `NativeHal.cpp:1065-1085`).
- Emulator `returnToMenu()` resets the modifier FSM (`NativeHal.cpp:1081`); **firmware `returnToMenu()` does not** (`SystemApp.cpp:860-883`) — a real firmware/emulator behavior divergence (RT-14/L-7 is the planned fix; GSTATE §A.3).

### B.4 Rendering

- UI built once in `createUI()` (`SettingsApp.cpp:121-186`): a container below the 24 px StatusBar, 3 rows (44 px each), Montserrat-14 text (STIX has no U+0020 glyph — Phase 7I comments `SettingsApp.cpp:156-160,173-177`), hint label `"Navigate   ENTER Toggle   MODE Back"` (`SettingsApp.cpp:179`).
- `updateValues()` reads the three globals and paints ON/OFF/`"%d digits"` (`SettingsApp.cpp:211-235`). `updateFocus()` paints the focus ring (`SettingsApp.cpp:192-205`).

---

## C. Current settings inventory [V]

### C.1 Visible settings (the 3 rows)

| # | Row label | Backing variable | Type / default | Mutation path | Real consumers | Firmware effect | Emulator effect | Persistence |
|---|---|---|---|---|---|---|---|---|
| 0 | "Complex numbers" | `setting_complex_enabled` | `bool` / `true` (`main.cpp:31`, `NativeHal.cpp:125`) | ENTER toggles (`SettingsApp.cpp:243-245`) | **exactly one**: `cas::SingleSolver.cpp:330` (gates complex-root output vs "No real solutions") | real (SingleSolver reachable via EquationsApp) | **none** — `SingleSolver.cpp` is not in the emulator whitelist (`platformio.ini:195-266`); toggle changes a variable nothing reads | none |
| 1 | "Decimal precision" | `setting_decimal_precision` | `int` / `10` (`main.cpp:32`, `NativeHal.cpp:126`) | ENTER/RIGHT cycles 6→8→10→12 (`SettingsApp.cpp:247-258,310-315`); LEFT cycles backward (`SettingsApp.cpp:294-308`) | **zero consumers** — grep of the whole tree finds reads only inside `SettingsApp.cpp` itself (`:223,250,299`) and the declarations (`Config.h:78`) | **placebo** | **placebo** | none |
| 2 | "Step-by-step mode" | `setting_edu_steps` | `bool` / `false` (`main.cpp:33`, `NativeHal.cpp:127`) | ENTER toggles (`SettingsApp.cpp:260-262`) | `CalculationApp.cpp:578` (gates edu-steps generation after evaluate) | real | real (CalculationApp + cas::SymSimplify are whitelisted) | none |

The placebo rows are the central honesty defect of the current SettingsApp: the UI paints a confident colored ON/OFF or "10 digits" value that provably changes nothing (row 1 anywhere; row 0 in the emulator). Fix owned by ST-03; the row-efficacy rule is normative in §D.1.

### C.2 Hidden / global settings not surfaced in Settings

| Global | Where | Default | Writers today | Readers | Should it be a Settings row? |
|---|---|---|---|---|---|
| `vpam::g_angleMode` | `MathEvaluator.h:70-76`, defined `MathEvaluator.cpp:53` | `AngleMode::RAD` | **none — never assigned anywhere in the tree** (grep-verified) | `MathEvaluator.cpp:849-896` (trig conversion), `StatusBar.cpp:246` (badge) | **Yes** — §E, ST-01 |
| `SystemApp::_angleMode` (legacy enum) | `SystemApp.h:173` | `AngleMode::DEG` (`SystemApp.cpp:79`) | ctor only | pushed once into `_evaluator`/`_equationSolver` (`SystemApp.cpp:96-97`); legacy TFT header string (`SystemApp.cpp:372`) | No — **retire** (GS-01) |
| `Evaluator::_angleMode` (per instance) | `Evaluator.h:54`, default RAD (`Evaluator.cpp:25`) | `setAngleMode()` (`Evaluator.h:47`) — never called for the Grapher's instance (`GraphModel.h:172`, grep-verified) | Grapher trig (`Evaluator.cpp:42`) | No — becomes a **sync target** of the vpam global (GS-01) |
| `EquationSolver::_angleMode` | `EquationSolver.h:89`, default DEG (`EquationSolver.cpp:25`) | `SystemApp.cpp:97` | applied at solve time (`EquationSolver.cpp:358`) | No — sync target (GS-01) |
| `vpam::ResultMode` (S⇔D Symbolic/Periodic/Extended) | `MathEvaluator.h:81-85` | per-CalculationApp session state, cycled by FREE_EQ | CalculationApp display | Display-mode contract in GSTATE §E; **not** a Settings row this cycle (PD-S2) |
| `KeyboardManager` modifier FSM | `KeyboardManager.h:47-54` singleton | — | apps + StatusBar indicator (`StatusBar.cpp:231-237`) | No — input state, not a setting (UX-02/RT-14 own it) |

### C.3 The `setting_*` definition split

The three globals are declared `extern` in `Config.h:77-79` and defined **twice**: firmware `main.cpp:31-33` and emulator `NativeHal.cpp:125-127` (with a comment asserting the defaults must match — `NativeHal.cpp:117-124`). There is no compile-time guard that keeps the two definition sites in sync; GS-02 replaces both with a single `SettingsState` definition.

### C.4 Persistence paths — none

- No save/load call exists for any `setting_*` global (grep-verified; concurs with fault-containment spec C6.7 and safe-mode spec §6: "Settings are never persisted").
- Firmware storage that *does* exist and is adjacent: `VariableManager` → LittleFS `/vars.dat` (loaded `SystemApp.cpp:144-156`, **`begin(true)` = format-on-fail**), `VariableContext` → NVS `calcVars` (load-only; `saveToNVS()` has zero callers). Neither stores settings.
- Emulator storage: `hal/FileSystem` maps LittleFS onto `./emulator_data/` (`FileSystem.cpp:38`); `emulator_data/vars.dat` is **committed to git** and loaded at every emulator boot (`NativeHal.cpp:2171-2179`) — a hidden-persistence hazard documented in GSTATE §F.3.

---

## D. Desired settings inventory [P]

### D.1 Normative row-efficacy rule (PD-S1)

**A SettingsApp row may exist only if, on the build target showing it, changing the value observably changes behavior — otherwise the row must either be removed or rendered in a disabled style with an explicit "(no effect on this build)" affordance.** Rationale: a settings panel is a promise; placebo rows teach the user the panel lies. This rule retroactively condemns the current rows 0 (emulator) and 1 (everywhere) — resolution in ST-03.

### D.2 Target inventory (post GS-01/GS-02/ST-01/ST-03)

| Row | Values | Default | Backing state | Consumers | Persisted | Ticket |
|---|---|---|---|---|---|---|
| **Angle mode** | `Degrees` / `Radians` | **Degrees** (PD-S3, §E.1) | `vpam::g_angleMode` via SettingsState | MathEvaluator, Grapher (sync), EquationSolver (sync), StatusBar badge | yes | ST-01/GS-01 |
| **Complex numbers** | ON / OFF | ON | `setting_complex_enabled` → SettingsState | `SingleSolver.cpp:330` | yes | ST-03 (emulator: disabled style until SingleSolver is whitelisted — P-05 makes it real) |
| **Decimal precision** | 6 / 8 / 10 / 12 digits | 10 | `setting_decimal_precision` → SettingsState | **must gain a real consumer** (approx/decimal formatting in `MathEvaluator`/CalculationApp) **or be removed** — OQ-S1 | yes (if kept) | ST-03 |
| **Step-by-step mode** | ON / OFF | OFF | `setting_edu_steps` → SettingsState | `CalculationApp.cpp:578` | yes | (already honest) |
| **System info** (read-only) | build version / commit / free RAM+PSRAM / storage state | — | read-only probes | — | n/a | ST-04 (thin row; deep diagnostics stay in RT-15's DiagnosticsApp) |
| **Reset settings** | action row, two-step confirm | — | SettingsState reset-to-defaults + persist | — | writes defaults | ST-04; complements RT-13's `!vars reset` (variables) — settings-reset only resets settings |
| *(future)* Diagnostics entry | action row → DiagnosticsApp | — | — | — | n/a | after RT-15; not this cycle |
| *(future)* Safe-mode request | action row (`flags.bit0`) | — | CrashRecord | — | RTC | after RT-02/RT-03; not this cycle |

### D.3 Explicit non-rows

- **Theme/contrast**: no theme state exists anywhere (colors are per-file constants, e.g. `SettingsApp.cpp:30-39`, `Theme.h` is static). Not a row until a theme system exists.
- **Language**: comments are bilingual but every UI string is a hard-coded English literal (e.g. `SettingsApp.cpp:132-136,179`). No i18n substrate; not a row.
- **Clock set**: `StatusBar.cpp:21-23` claims "el usuario puede ajustarla desde Settings" — **false today** (no such row, grep-verified). OQ-S2 decides whether to build it or fix the comment; SBAR §C.3.

---

## E. Angle mode contract

### E.1 Single source of truth [P → GS-01]

- **Authoritative state**: `vpam::g_angleMode` (`MathEvaluator.h:70-76`), wrapped by SettingsState accessors (GSTATE §C). It is the *only* stored angle mode. Everything else is a **derived copy synced by name** (`DEG`↔`DEG`, `RAD`↔`RAD`), never by numeric cast: `vpam::AngleMode` is `DEG=0,RAD=1` (`MathEvaluator.h:70-73`) while the legacy enum is `RAD=0,DEG=1` (`Evaluator.h:32-35`) — **a `static_cast` between them silently inverts the mode**. This prohibition is normative and permanent (GSTATE §D.2).
- **Default: Degrees** (PD-S3). Rationale: this is a student-market scientific calculator (Casio/NumWorks default DEG); the vpam enum comment already documents DEG as intended default (`MathEvaluator.h:71` "Grados (por defecto)") — the shipped RAD initializer (`MathEvaluator.cpp:53`) contradicts the design comment. Consequence: goldens showing the badge change at flip time (SCQA §D). If human review prefers RAD, only the default constant and the affected goldens change; the architecture is identical (recorded as reversible in GS-01).

### E.2 Current verified behavior per surface [V]

| Surface | Reads | Effective mode today | Evidence |
|---|---|---|---|
| CalculationApp (VPAM trig) | `vpam::g_angleMode` | **RAD, permanently** (global never written) | `MathEvaluator.cpp:53,849-896` |
| Grapher (RPN trig) | its own `Evaluator _eval` | **RAD, permanently** (ctor default; `setAngleMode` never called on it) | `GraphModel.h:172`, `Evaluator.cpp:25,42` |
| StatusBar badge | `vpam::g_angleMode` | **"RAD", permanently** | `StatusBar.cpp:243-248` |
| Launcher (MainMenu) bar | nothing — hard-coded literal | **"rad", permanently** | `MainMenu.cpp:333-338` |
| SystemApp legacy TFT header | `SystemApp::_angleMode` | **"DEG", permanently** — contradicts every other surface | `SystemApp.cpp:79,372` |
| SystemApp legacy Evaluator + EquationSolver | `_angleMode` pushed at begin | **DEG, permanently** | `SystemApp.cpp:96-97`, `EquationSolver.cpp:358` |
| SettingsApp | — | **no angle row at all** | `SettingsApp.cpp:132-136` (grep-verified) |

Net: the calculator's angle mode is unchangeable by any user action, and the firmware carries two contradictory frozen answers (VPAM/StatusBar say RAD; SystemApp's legacy engines say DEG). This is risk COR-3 realized and UX conflict K-1.

### E.3 Behavior contract per surface [P]

1. **Calculation**: trig evaluates in the current mode via the existing conversion machinery (`MathEvaluator.cpp:849-896` already handles DEG correctly when the global is DEG). Mode changes never mutate stored results, Ans, or history (CC-58 rule).
2. **Grapher**: `GraphModel` gains `syncAngleModeFromSystem()` (by-name mapping) invoked at `preCacheRPN` and `replot()` (per UX §D.4 ruling / GR-02). A mode flip while a plot is on screen takes effect on the next replot — it does **not** force an immediate implicit replot (PD-S5: replot-on-next-action; avoids surprise redraw cost mid-trace). The Grapher's table regenerates on the same rule.
3. **StatusBar**: badge continues to read `vpam::g_angleMode` and is therefore truthful for VPAM by construction; it becomes truthful for *all* engines only because all engines now derive from that global (SBAR §C.1).
4. **Settings UI**: row 0 (top) "Angle mode", value text `Degrees`/`Radians`, ENTER/LEFT/RIGHT toggles. Value takes effect immediately (write-through to SettingsState); the StatusBar badge inside SettingsApp must repaint on the same key event (this forces the §G.6 fix: `handleKey` refreshes `_statusBar.update()` after any mutation).
5. **EquationSolver**: syncs by name at solve entry (replacing `SystemApp.cpp:96-97` push-once).
6. **Serialization/persistence**: stored in the settings record as a **string-named token** (`angle=deg|rad`), never a raw enum integer — survives future enum reorderings (GSTATE §D.4). Loaded before the first app `load()`; on corrupt/missing record → default DEG + `[FS]` diagnostic (§H).
7. **Emulator/scripts**: `set_angle_mode deg|rad` script command (name already reserved by GHOOK §D.4 — reuse, don't fork) writes through the same SettingsState API as the UI; `assert_angle_mode deg|rad` reads the global (SCQA §C). Headless boot default equals firmware default (deterministic).

### E.4 Acceptance criteria (exact)

- `sin(30)` in DEG displays `1/2`-adjacent decimal; in RAD displays ≈ `-0.988…` (corpus CC-56; script SCQA §E.1).
- Grapher `sin(x)` in DEG has period 360 in graph units (GC-137), in RAD period 2π (GC-25).
- After `set_angle_mode deg`: `assert_angle_mode deg` passes **and** `assert_statusbar_angle DEG` passes in Calculation, Grapher, and Settings (badge equals engine — the K-1 kill shot).
- Casting between the two enums is absent: CI grep forbids `static_cast<AngleMode>` / `static_cast<vpam::AngleMode>` across namespaces (SCQA §G.4).
- Mode flip does not alter `Ans` (CC-58) and does not invalidate Grapher expression slots.

### E.5 Failure behavior (exact)

- Persisted token unrecognized (e.g. `angle=grad`) → whole-record quarantine per §H.2, default DEG, one `[FS] settings=corrupt` line. No per-field salvage (PD-S6: a record that fails one field validation is untrusted in full — simpler, and settings are cheap to re-enter).
- If a sync target is missing (Grapher not built — emulator variants): sync call compiles to nothing for absent apps; no dangling behavior.

---

## F. SETUP key contract

### F.1 Current evidence [V]

- `KeyCode::SETUP` exists (`KeyCodes.h:41`) and is mapped in both physical keyboard drivers (`Keyboard.cpp:44,53` — top row `SHIFT ALPHA MODE SETUP …`; `KeyMatrix.cpp:34`).
- **No handler anywhere**: zero `KeyCode::SETUP` matches in `SystemApp.cpp`, any app, or `NativeHal.cpp` (grep-verified). No SerialBridge character maps to it (`SerialBridge.cpp:142-224` — grep-verified). No `.numos` key name maps to it (`NativeHal.cpp` `scriptNameToKeyCode` — grep-verified).
- Physical scanning is itself disabled (`Keyboard.h:82` `CONNECTED_COLS = 0`), so today the key is doubly unreachable on hardware.
- Comment-only drift: `PeriodicTableApp.cpp:882` mentions a "SETUP/EXE key mapping" that does not exist as code — the block handles character input only.

### F.2 Product decision (PD-S4, resolves OQ-U3)

**SETUP opens SettingsApp from any mode.** This adopts option (b) of OQ-U3 (the UX spec's recorded leaning) and makes it normative:

- In `SystemApp::handleKey`, before the per-mode switch: `SETUP` → if `_mode != APP_SETTINGS`, behave exactly like launching card 10 (flush pending teardown, `launchApp(10)`); if already in Settings, ignore (PD-S7: idempotent, not a toggle — a toggle would make SETUP-mash exit to menu unpredictably).
- Return path: MODE from Settings returns to the **launcher** (existing behavior, `SystemApp.cpp:528-531`) — *not* to the interrupted app. PD-S8: no "return to previous app" stack this cycle; consequence: SETUP mid-Grapher costs the user their Grapher screen state only to the same degree MODE already does (deferred teardown destroys it). OQ-S3 records the richer alternative.
- Emulator parity: same interception in `dispatchKey` for every AppMode; `.numos` gains key name `setup` (append-only grammar). SerialBridge gains a character mapping (proposed `q`, currently unmapped — verified free in `SerialBridge.cpp:177-224`).
- **No quick-settings popover** this cycle (PD-S9): a popover is a second settings surface that can disagree with the first; build it only after SettingsState exists and only if HIL shows the full app is too slow to reach.
- Until ST-02 lands, SETUP remains inert and is documented as inert (KEYBOARD_LAYOUT.md note; no UI hint may reference SETUP before the handler exists).

---

## G. UX behavior [V current / P where marked]

### G.1 Navigation [V]

UP/DOWN move focus with clamping at both ends, no wrap (`SettingsApp.cpp:276-288`). [P] Keep no-wrap for a 3-6 row list (wrap adds misclick risk with no reach benefit); revisit if the list exceeds one screen — then the container must scroll (`LV_OBJ_FLAG_SCROLLABLE` currently removed, `SettingsApp.cpp:130`).

### G.2 Selection & editing [V]

ENTER toggles/cycles the focused row (`SettingsApp.cpp:290-292`); LEFT cycles precision backward, RIGHT forward, both only on row 1 (`SettingsApp.cpp:294-315`). [P] Generalize: LEFT/RIGHT cycle values on **every** enumerated row (angle row included); ENTER = same as RIGHT for cycle rows, activate for action rows (Reset). Digits/other keys remain ignored (current `default:` at `SettingsApp.cpp:317-318`).

### G.3 AC / MODE / HOME behavior

- MODE (= HOME; there is no separate HOME key — `SerialBridge.cpp:165,188` maps `h`/`HOME` to `KeyCode::MODE`) exits to launcher, handled by SystemApp/NativeHal before the app (`SystemApp.cpp:528-531`, `NativeHal.cpp:651-654`) [V]. Keep.
- AC currently does nothing in Settings (falls through the switch) [V `SettingsApp.cpp:317-318`]. [P → PD-S10]: AC = **revert focused row to its default value** (cheap, discoverable, consistent with "AC clears" idiom) — *not* exit-to-menu (that's MODE's job; PeriodicTable's `AC=exit` at `SystemApp.cpp:580` is the outlier, not the pattern).
- SHIFT+AC power-off chord is intercepted globally and currently disabled (`SystemApp.cpp:472-481`) [V]. Unchanged by this spec.

### G.4 Persistence confirmation [P → ST-04]

Write-through with debounced commit: value applies to SettingsState immediately; the persist happens on **exit from Settings** (returnToMenu / SETUP-away) or after 2 s of no changes, whichever first — bounds flash wear under key-mash (GSTATE §B.4). UI: transient "Saved" toast only on persist failure ("Save failed — using session value"), silence on success (PD-S11: success toast on every toggle is noise).

### G.5 Invalid-setting recovery [P]

`updateValues()` currently assumes `setting_decimal_precision ∈ {6,8,10,12}`; an out-of-range value (possible once persistence exists) silently displays as-is and the cycle logic snaps to index 0 (`SettingsApp.cpp:247-258` defaults `idx=0`). Normative: SettingsState validates on load (§H); by the time SettingsApp reads a value it is guaranteed in-domain, so the UI needs no per-row salvage logic.

### G.6 StatusBar refresh inside Settings [V bug → P]

`_statusBar.update()` is called only in `load()` (`SettingsApp.cpp:110`); no key event refreshes it (grep: the only `_statusBar.update()` in `SettingsApp.cpp` is line 110). Consequences today: the clock freezes at entry time for the whole visit, and SHIFT/ALPHA presses (consumed by SystemApp into the FSM, `SystemApp.cpp:484-493`) change modifier state **invisibly** while in Settings. [P] `handleKey` ends with `_statusBar.update()` (matching CalculationApp's pattern, e.g. `CalculationApp.cpp:513`), and the angle row mutation path repaints the badge in the same frame (§E.3.4). Clock cadence beyond key events is SBAR §F's problem, not Settings'.

---

## H. Error / recovery behavior [P — nothing exists today; inherits RT-13/R-3]

### H.1 Missing storage

LittleFS mount fails → SettingsApp runs on compile-time defaults, fully functional for the session; persist attempts are skipped; one `[FS] mount=fail action=disabled` diagnostic (RT-13 wording). **Never** `begin(true)` reformat for settings' sake (the two existing format-on-fail sites `SystemApp.cpp:144` / `NativeHal.cpp:2172` are RT-13's to fix; GS-02 must not add a third).

### H.2 Corrupted settings record

Version/CRC mismatch or failed field validation → quarantine the file (rename `/settings.bad`, one generation), load defaults, `[FS] settings=corrupt action=quarantine` (GSTATE §G). SettingsApp shows defaults; no modal, no boot delay. A subsequent persist writes a fresh valid record.

### H.3 Persistence write failure

Temp-write or rename fails → session value stays live, `[FS] write-fail file=/settings.dat` diagnostic, §G.4 failure toast. Retry only on the next natural commit point (no retry loop).

### H.4 Emulator-unsupported settings

Rows whose consumer isn't compiled into the emulator (today: Complex numbers → `SingleSolver`) render disabled-style with "(firmware only)" per PD-S1 — the emulator must not imply an effect it cannot have. Compile-time: `#if` on the same whitelist knowledge is brittle; prefer a SettingsState capability bit set by the build (GSTATE §F.4).

---

## I. Open decisions

| ID | Question | Options | Leaning / consequence |
|---|---|---|---|
| **OQ-S1** | Decimal precision row: wire or remove? | (a) wire into decimal formatting (Periodic 12-sig-digit and Extended paths, `MathEvaluator.cpp:1198-1359`) — real work, touches CALC display contract; (b) remove the row until a consumer exists | ⭢ (b) short-term inside ST-03 (honesty now), (a) as a follow-up ticket once CALC owners define which formatter obeys it. Human call because (a) changes math display goldens. |
| **OQ-S2** | Clock-set UI in Settings? | (a) build HH:MM editor row (makes `StatusBar.cpp:21-23` comment true); (b) fix the comment, no UI until RTC/NTP story exists | ⭢ (b) — a settable clock that resets every boot (no RTC battery/NTP) is another honesty trap. SBAR §C.3. |
| **OQ-S3** | SETUP return semantics | (a) always return to launcher (PD-S8); (b) return to interrupted app (needs app-state preservation across Settings visit — conflicts with deferred teardown destroying the old screen) | ⭢ (a) now; (b) requires a "suspend without end()" lifecycle NumOS does not have — out of scope. |
| **OQ-S4** | Where do settings persist on firmware? | (a) NVS (`Preferences`) — atomic per-key, wear-leveled, survives LittleFS reformat; (b) LittleFS `/settings.dat` — same store as vars, one code path with RT-13 v2 CRC format | ⭢ (a) NVS; consequences in GSTATE §B.3 (it also keeps settings alive through the existing format-on-fail hazard). Human ratification in GS-02. |

---

## J. Implementation tickets (headline; full fields in SCROAD)

- **GS-01** — Angle-mode single source of truth (implements §E; = UX-01/CA-06 refined; feeds GR-02).
- **GS-02** — SettingsState + persistence schema (implements §C.3, §D, §H, OQ-S4).
- **ST-01** — Settings angle-mode row (implements §E.3.4, §G).
- **ST-02** — SETUP key disposition (implements §F, PD-S4/S7/S8).
- **ST-03** — Row-efficacy honesty pass: complex-row capability gating + precision-row resolution per OQ-S1 (implements §C.1, §D.1).
- **ST-04** — System-info + Reset rows, persistence confirmation UX (implements §D.2, §G.4).
- **SB-01/SB-04** (StatusBar side of §E.3.3 and §G.6) and **QA-01/QA-02/QA-04/QA-05** (hooks, semantic scripts, corruption, roundtrip) — see their own docs.

Ordering and dependencies: SCROAD §1.

*End of SettingsApp product spec.*
