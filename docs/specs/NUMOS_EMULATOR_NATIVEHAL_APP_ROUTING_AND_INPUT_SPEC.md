<<<<<<< HEAD
# NUMOS_EMULATOR_NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC

Status: SPEC-ONLY (P-05 wave). No code in this document is implemented.
Scope: how the native PC emulator (`src/hal/NativeHal.cpp`, compiled only under `NATIVE_SIM`) routes apps, dispatches input, and exposes script/assert APIs — current verified state, divergences from firmware, and the required pattern for enabling new apps.
Companions: `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` (architecture/product), `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (per-app detail), `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (QA), `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md` (tickets).

Evidence convention: every current-state claim cites `file:line`. Sections marked **[PROPOSED]** are design, not current behavior.
=======
# NumOS — Emulator NativeHal App Routing and Input Spec (P-05)

> Part of the P-05 emulator app-enablement spec set. Companion documents:
> `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` (architecture/product),
> `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (per-app detail),
> `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (test plan),
> `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md` (tickets).
>
> Audited against the working tree of branch `claude/emulator-app-parity-p05-spec-1lf7bu`
> (based on `main`, 2026-07-04). Every current-state claim cites `file:line`.
> Sections marked **[PROPOSED]** are design, not current behavior.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## A. Current NativeHal routing (verified)

### A.1 AppMode enum

<<<<<<< HEAD
`NativeHal.cpp:195-206` defines the emulator's own mode enum, independent of firmware's `Mode`:

```
SPLASH, MENU, CALCULATION, SETTINGS, MATH_SHOWCASE,
STATISTICS, PROBABILITY, SEQUENCES, REGRESSION, GRAPHER
```

This is a strict subset of firmware `Mode` (`src/SystemApp.h:76-103`), which additionally has `APP_TABLE`, `APP_EQUATIONS`, `APP_CALCULUS`, `APP_MATRICES`, `APP_PYTHON`, `APP_PERIODIC_TABLE`, `APP_BRIDGE_DESIGNER`, `APP_CIRCUIT_CORE`, `APP_FLUID_2D`, `APP_PARTICLE_LAB`, `APP_NEURAL_LAB`, `APP_OPTICS_LAB`, `APP_NEO_LANGUAGE`, `APP_FRACTAL`, `APP_MATH_VISUAL` (gated by `NUMOS_MATH_VISUAL_APP_ENABLED`), `APP_SETTINGS`, `STEP_VIEW`.

### A.2 App construction

`transitionToMenu()` (`NativeHal.cpp:867-908`), invoked from the main loop after the splash (`NativeHal.cpp:2044-2047`), constructs:

| App | Construction | begin() timing |
|---|---|---|
| CalculationApp | `NativeHal.cpp:872-873` | eager (`begin()` at construction) |
| SettingsApp | `NativeHal.cpp:877` | lazy on `load()` |
| StatisticsApp / ProbabilityApp | `NativeHal.cpp:882-883` | lazy |
| SequencesApp | `NativeHal.cpp:887` | lazy |
| RegressionApp | `NativeHal.cpp:891` | lazy |
| GrapherApp | `NativeHal.cpp:896` (deliberately no `begin()` to spare heap) | lazy |
| MainMenu | `NativeHal.cpp:899-902`, `setLaunchCallback(launchApp)` | — |

Keyboard group wiring: `lv_indev_set_group(LvglKeypad::indev(), g_menu->group())` (`NativeHal.cpp:905`).

### A.3 open_app / launchApp

- Script `open_app NAME` → `scriptAppNameToId()` (`NativeHal.cpp:1414-1428`) → `launchApp(id)` at execution (`NativeHal.cpp:1717-1719`). Unknown names are rejected **at parse time** (returns −1 → whole script load fails, exit 2).
- `launchApp(appId)` (`NativeHal.cpp:914-987`): calls `flushPendingTeardown()` first (`:920`), then switches on id: `0`→CALCULATION (`:923`), `1`→GRAPHER (`:929`), `4`→STATISTICS (`:937`), `5`→PROBABILITY (`:945`), `6`→REGRESSION (`:953`), `7`→SEQUENCES (`:961`), `10`→SETTINGS (`:969`), `APPID_MATH_SHOWCASE = 100` (`:251`) → showcase (`:977`).
- **Dead cards**: any other id (2, 3, 8, 9, 11–19, 20) hits `default:` → prints `"[APP] App %d no implementada en simulador"` and returns without changing `g_mode` (`NativeHal.cpp:983-985`). The launcher keeps rendering all `APPS[]` cards (`src/ui/MainMenu.cpp:88-113`) — the grid is not filtered by emulator availability, so those cards are focusable and clickable but inert.

### A.4 assert_app

`assert_app NAME` compares the canonical name from `activeAppName()` (derived from `g_mode`, `NativeHal.cpp:1656-1668`) against the token (`NativeHal.cpp:1722-1730`).

### A.5 Home/MODE handling

Every app-mode case of `dispatchKey` intercepts `MODE` on the down edge → `returnToMenu()` (`NativeHal.cpp:615, 641, 658, 675, 692, 709, 727, 745`). MODE is produced by SDL `h`/Home (`:325-326`) or script `home`/`mode` (`:494`).

### A.6 Deferred teardown

Emulator replicates firmware SAFE_HOME_EXIT:

- State: `g_teardownPending` / `g_teardownMode` / `g_teardownStartTick`, `TEARDOWN_DELAY_MS = 260` (`NativeHal.cpp:230-233`).
- `returnToMenu()` (`NativeHal.cpp:1055-1077`): records the departing mode, timestamps with `lv_tick_get()` (`:1064-1068`), resets modifiers via `KeyboardManager::instance().reset()` (`:1071`), loads the menu with fade while the old screen stays alive as fade source (`:1073`), sets `g_mode = MENU`.
- Main loop fires the teardown once `lv_tick_elaps(...) >= TEARDOWN_DELAY_MS` (`NativeHal.cpp:2074-2078`) → `performAppTeardown()` (`:997-1036`), which calls the departing app's `end()`; for CALCULATION it immediately re-`begin()`s (`:1000-1005`).
- `flushPendingTeardown()` (`NativeHal.cpp:1040-1045`) forces the teardown before launching another app.

### A.7 Key dispatch model

`dispatchKey(kc, action, isDown)` (`NativeHal.cpp:565-759`):

- SPLASH: ignored.
- MENU: down-edge only; arrows → `g_menu->moveFocusByDelta()` (`:588-598`); `GRAPH` → `launchApp(1)` (`:602-605`, emulator-only shortcut); everything else synthesized as PRESS+RELEASE into `LvglKeypad::pushKey` (`:608-609`).
- App modes: build `KeyEvent{code, action, row=-1, col=-1}` and forward to the active app's `handleKey` after the MODE intercept. RELEASE events are forwarded too (apps filter internally; comment at `NativeHal.cpp:620-621`).

### A.8 Script command parser

`loadScript()` (`NativeHal.cpp:1447-1612`); `ScriptCmdType` (`:1335-1352`); execution one command per frame in `scriptStepBegin()` (`:1687-1840`), scheduled at loop top before SDL events so a key is visible to the same frame's `lv_timer_handler` (`:2036-2039`). Whole script is validated at load (fail-fast). Exit codes: parse fail = 2 (`:1873`), screenshot write fail = 3 (`:1853`), assert fail = 4 (`:1676`).

Full command set (parse-line cites):

| Command | Parse | Semantics |
|---|---|---|
| `#`/blank | `:1464-1465` | ignored |
| `wait N` | `:1473-1481` | wait N frames |
| `key NAME` | `:1482-1492` | PRESS+RELEASE |
| `keydown NAME` / `keyup NAME` | `:1482-1492` | PRESS only / RELEASE only |
| `screenshot PATH` | `:1493-1499` | deferred PPM capture after render (`:1708-1711`, `:1845-1861`); no spaces in path |
| `log "msg"` | `:1500-1510` | print |
| `open_app NAME` | `:1511-1522` | launch by canonical name |
| `assert_app NAME` | `:1523-1532` | active app check |
| `assert_result TEXT` / `assert_result_contains TEXT` | `:1533-1547` | Calc-only result check (`:1731-1755`) |
| `assert_no_error` | `:1548-1552` | Calc result non-error (`:1756-1765`) |
| `assert_error [TEXT]` | `:1553-1565` | Calc result is error (`:1768-1794`) |
| `assert_variable NAME VALUE` | `:1566-1585` | `VariableManager` global check (`:1795-1810`) |
| `assert_menu_focus NAME\|ID` | `:1586-1601` | launcher focus check (`:1813-1838`) |
| anything else | `:1602-1603` | load failure |

Assert-hook coupling (important for enablement): `assert_result*` / `assert_error` / `assert_no_error` require `g_mode == CALCULATION && g_calcApp` (`NativeHal.cpp:1733, 1757, 1769`) and read `CalculationApp::debugHasResult()/debugLastResult()` formatted by `formatExactVal` (`:1623-1653`). **No other app exposes semantic hooks today.** `assert_variable` reads the global `VariableManager` regardless of app (`:1799-1800`). `assert_menu_focus` reads `MainMenu::debugFocusedCardId()` (`src/ui/MainMenu.cpp:212-217`), with token resolution against the real `APPS[]` table via `debugResolveCardToken` (`src/ui/MainMenu.cpp:245-266`).
=======
The emulator has its own mode enum, disjoint from the firmware `Mode` enum:

```cpp
enum class AppMode : uint8_t {
    SPLASH, MENU, CALCULATION, SETTINGS, MATH_SHOWCASE,
    STATISTICS, PROBABILITY, SEQUENCES, REGRESSION, GRAPHER
};
```
(`src/hal/NativeHal.cpp:195-206`.) Ten states; eight app-bearing states. Firmware `Mode`
has 24 states (`src/SystemApp.h:76-103`) including `STEP_VIEW`, `APP_TABLE` and all 13
firmware-only apps. There is no shared header; the two enums are hand-maintained.

### A.2 App instances and construction

- All emulator app instances are file-scope statics: `g_calcApp`, `g_settingsApp`,
  `g_statsApp`, `g_probApp`, `g_seqApp`, `g_regApp`, `g_grapherApp`
  (`NativeHal.cpp:249-255`), plus the MathShowcase object cluster
  (`g_showcaseScreen/Bar/Canvas/Caption/Root/Index`, `NativeHal.cpp:262-267`).
- Construction happens in `transitionToMenu()` after the splash completes
  (`NativeHal.cpp:877-918`). CalculationApp is eagerly `begin()`-ed
  (`NativeHal.cpp:882-883`); every other app is constructed only, relying on the
  `load() → if (!_screen) begin()` lazy pattern (`NativeHal.cpp:887-906`), mirroring
  the firmware rationale of `SystemApp.cpp:100-104` (eager begin() of many apps
  exhausts the LVGL heap on device).

### A.3 launchApp and the dead-card path

`launchApp(int appId)` (`NativeHal.cpp:924-997`) switches on the **launcher card id**
(same ids as `MainMenu::APPS[]`): 0 Calculation, 1 Grapher, 4 Statistics,
5 Probability, 6 Regression, 7 Sequences, 10 Settings, plus the emulator-only
`APPID_MATH_SHOWCASE = 100` (`NativeHal.cpp:261`, not a launcher card).
Any other id — i.e. cards 2, 3, 8, 9, 11-19, which the launcher **does render**
in the emulator because `ui/MainMenu.cpp` is whitelisted (`platformio.ini:263`)
and `APPS[]` has 20 unconditional entries (`src/ui/MainMenu.cpp:88-113`) — falls to:

```cpp
default:
    std::printf("[APP] App %d no implementada en simulador\n", appId);
```
(`NativeHal.cpp:993-995`.) The menu stays loaded, `g_mode` stays `MENU`, no error
surfaces to a script. This is the "visible-but-dead card" behavior; see §C.2 and
special-question answers in the parity spec §J.

### A.4 open_app / assert_app command plumbing

- `open_app NAME` is parsed at script load; `scriptAppNameToId()` maps name → launcher
  id (`NativeHal.cpp:1424-1438`). Unknown names fail script load with exit 2
  (`NativeHal.cpp:1521-1532`). The command executes by calling the same
  `launchApp(id)` as a menu ENTER (`NativeHal.cpp:1727-1729`).
- `assert_app NAME` compares against `activeAppName()`, a switch over `g_mode`
  (`NativeHal.cpp:1666-1678`); name canonicalization in `canonicalAppName()`
  (`NativeHal.cpp:1402-1418`).
- Enabling one new app therefore requires touching, today, **five hand-maintained
  switch/map sites** in NativeHal.cpp alone: the `AppMode` enum (195-206), instance
  pointer + construction (249-255, 877-918), `launchApp` (924-997),
  `performAppTeardown` (1007-1046), `dispatchKey` (575-769) — plus
  `canonicalAppName` (1402-1418), `scriptAppNameToId` (1424-1438) and
  `activeAppName` (1666-1678). Eight sites total. This is the drift engine behind
  risk EMU-1 (`NUMOS_RISK_REGISTER.md` §4).

### A.5 Home/MODE handling and deferred teardown

- In every app-mode case of `dispatchKey`, `MODE` on the down-edge calls
  `returnToMenu()` (e.g. `NativeHal.cpp:625-627, 651-654, 737-740, 755-758`).
- `returnToMenu()` (`NativeHal.cpp:1065-1087`): flushes any prior pending teardown,
  records `g_teardownMode = g_mode` + `g_teardownStartTick = lv_tick_get()`, resets
  keyboard modifiers via `vpam::KeyboardManager::instance().reset()`
  (`NativeHal.cpp:1081`), loads the menu (fade-in starts with the app screen still
  alive), and sets `g_mode = MENU`.
- The deferred `end()` runs in the main loop once `lv_tick_elaps(g_teardownStartTick)
  >= TEARDOWN_DELAY_MS` (260 ms > the 200 ms menu fade; `NativeHal.cpp:233,
  2088-2092`), via `performAppTeardown(mode)` (`NativeHal.cpp:1007-1046`).
  CalculationApp is special-cased: `end()` then immediate `begin()` re-create
  (`NativeHal.cpp:1010-1015`); all others rely on lazy re-begin in `load()`.
- `flushPendingTeardown()` resolves a pending teardown synchronously before
  launching another app (`NativeHal.cpp:1050-1055`, called at `launchApp` entry
  `NativeHal.cpp:930`), mirroring firmware `flushPendingTeardownNow`
  (`SystemApp.cpp:220-232`, called at `SystemApp.cpp:735`).
- The delay is measured with `lv_tick`, so it is deterministic under
  `--deterministic` (comment `NativeHal.cpp:218-229`).

### A.6 Key dispatch model

`dispatchKey(KeyCode, KeyAction, bool isDown)` (`NativeHal.cpp:575-769`) is the single
dispatch point for both SDL input and script replay (extracted for exactly that
parity, comment 567-574). Per mode:

- `SPLASH`: keys ignored (579-580).
- `MENU`: arrows go to `MainMenu::moveFocusByDelta()` — the same 2D grid model the
  firmware uses (586-608; firmware equivalent `SystemApp.cpp:687-707`); `GRAPH`
  ('g') launches app id 1 directly (612-615); all other keys are synthesized as
  LVGL PRESS+RELEASE pairs through `LvglKeypad::pushKey` so ENTER fires
  `LV_EVENT_CLICKED` on the focused card (617-619).
- Every app mode: `MODE`+down-edge → `returnToMenu()`; everything else (including
  RELEASE) is forwarded to `<App>::handleKey(KeyEvent)` with `row=col=-1`
  (623-749). MathShowcase is inline-handled (751-767): LEFT/UP previous case,
  RIGHT/DOWN/ENTER next case, no app object.

SDL translation: `mapSdlToKeyCode` for navigation/control/letter shortcuts
(`NativeHal.cpp:318-400`); `SDL_TEXTINPUT` + `mapTextChar` for digits/symbols with
OS keyboard layout applied (`NativeHal.cpp:415-444, 792-803`). Script translation:
`scriptNameToKeyCode` (`NativeHal.cpp:456-565`).

### A.7 Script command parser

`.numos` grammar (parse: `loadScript`, `NativeHal.cpp:1457-1622`; execute:
`scriptStepBegin`, 1697-1850):

| Command | Semantics | Source |
|---|---|---|
| `wait N` | consume N frames | 1483-1491, 1705-1707 |
| `key NAME` | PRESS+RELEASE via dispatchKey | 1492-1502, 1708-1711 |
| `keydown NAME` / `keyup NAME` | PRESS only / RELEASE only | 1492-1502, 1712-1717 |
| `screenshot PATH` | deferred PPM capture after this frame's render | 1503-1509, 1718-1721; capture 1855-1871 |
| `log "msg"` | stdout | 1510-1520 |
| `open_app NAME` | launchApp(id) | 1521-1532, 1727-1729 |
| `assert_app NAME` | active mode name equality | 1533-1542, 1732-1740 |
| `assert_result TEXT` / `assert_result_contains TEXT` | CalculationApp-only, reads `debugLastResult()` | 1543-1557, 1741-1765 |
| `assert_no_error` | CalculationApp result `.ok` | 1558-1562, 1766-1775 |
| `assert_error [TEXT]` | CalculationApp result is error | 1563-1575, 1778-1804 |
| `assert_variable NAME VALUE` | reads `VariableManager` singleton | 1576-1595, 1805-1820 |
| `assert_menu_focus NAME\|ID` | `MainMenu::debugFocusedCardId()` | 1596-1611, 1823-1848 |

Contract properties (CI-relied-upon, append-only per the context header's fragile-file
list): whole-script validation before execution, exit 2 on parse error
(`NativeHal.cpp:1882-1884`); assertion failure exit 4 (`assertFail`, 1682-1688);
screenshot write failure exit 3 (1855-1866); one command per frame, executed before
SDL events and `lv_timer_handler` (1697-1701, 2049).

Notable asymmetries:

- `assert_result*`, `assert_no_error`, `assert_error` are **hardwired to
  CalculationApp** (`g_mode != AppMode::CALCULATION` → assert fail, 1743-1747,
  1779-1783). No other app exposes result state.
- Only CalculationApp (`debugHasResult/debugLastResult`,
  `src/apps/CalculationApp.h:95-107`) and MainMenu (`debugFocusedCardId`,
  `debugResolveCardToken`, `debugCardNameById`, `src/ui/MainMenu.h:58-79`) have
  NATIVE_SIM debug hooks. Grapher, Settings, Statistics, Probability, Sequences,
  Regression have **none** — they are pixel-opaque to the assert harness.

### A.8 Exit cleanup

At process exit, apps are `end()`-ed and deleted (`NativeHal.cpp:2142-2152`).
**Verified divergence:** `g_grapherApp` is missing from this cleanup block — calc,
settings, stats, prob, seq, reg are deleted; Grapher is neither ended nor deleted
(`NativeHal.cpp:2144-2149`). Harmless today (process exit + `lv_deinit()`), but it
breaks the "every app has a symmetric end" invariant and would corrupt any future
LVGL leak accounting. Recorded as ticket ROUTE-03 in the roadmap.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## B. Current firmware routing (verified)

<<<<<<< HEAD
- **Launcher path**: `SystemApp::begin()` news 20 apps (`src/SystemApp.cpp:104-126`); `MainMenu` fires `_launchCb(appId)` on card click (`src/ui/MainMenu.cpp:1023-1034`); `switchApp` maps ids 0–20 to modes (`src/SystemApp.cpp:868-896`); `launchApp` runs `lv_obj_update_layout(lv_scr_act())` + `flushPendingTeardownNow` before switching (`src/SystemApp.cpp:720-725`).
- **Construction/destruction**: lazy `begin()` inside each app's `load()`; deferred teardown 250 ms via `_pendingTeardownMode` / `_teardownStartMs` (`src/SystemApp.cpp:234, 854`; contract in `src/SystemApp.h:180-186`).
- **Keyboard dispatch**: `SystemApp::handleKey` filters to PRESS/REPEAT at entry (`src/SystemApp.cpp:451`); physical events come from `_keypad.pollEvent` in `update()` (`:242-247`); serial injection via `injectKey` (`:299-301`). Global SHIFT+AC power hook (`:465-471`, currently disabled) and global SHIFT/ALPHA toggling (`:474-483`, except Calc/Calculus which own modifiers).
- **MODE/HOME**: each app case calls `returnToMenu()` on `MODE`; `returnToMenu` (`src/SystemApp.cpp:843-866`) does **not** reset KeyboardManager.
- **AC behavior is app-specific in firmware**: several apps get AC-to-exit and save-on-exit at the dispatcher level — PeriodicTable (`src/SystemApp.cpp:570`), CircuitCore with autosave (`:585-596`), Fluid2D (`:598-605`), Fractal (`:640`), MathVisual (`:651`); Grapher exits on AC at tab-level/Expressions (`:1002-1016`).
- **Menu keys**: `handleKeyMenu` (`src/SystemApp.cpp:669-711`) handles UP/DOWN/LEFT/RIGHT/ENTER/AC/DEL/F1/F2 only; GRAPH is ignored in the firmware menu.

## C. Divergences (verified, numbered for reference by other docs)

| # | Divergence | Emulator | Firmware |
|---|---|---|---|
| D1 | App coverage | 8 modes wired (ids 0,1,4,5,6,7,10,100) | 20 apps, ids 0–20 (`SystemApp.cpp:868-896`) |
| D2 | Dead-card behavior | silent console line, no UI feedback (`NativeHal.cpp:983-985`) | no dead cards (all wired) |
| D3 | Global modifier interception | none; SHIFT/ALPHA forwarded to app | dispatcher-level toggle (`SystemApp.cpp:474-483`) + SHIFT+AC hook (`:465-471`) |
| D4 | PRESS/REPEAT filter | forwards RELEASE too (`NativeHal.cpp:620-621`) | filtered at entry (`SystemApp.cpp:451`) |
| D5 | App-specific AC exit / autosave | only what the app itself implements | dispatcher-level for PeriodicTable/Circuit/Fluid2D/Fractal/MathVisual (`SystemApp.cpp:570-651`) |
| D6 | Menu GRAPH shortcut | launches Grapher (`NativeHal.cpp:602-605`) | ignored (`SystemApp.cpp:677-709`) |
| D7 | Teardown delay | 260 ms via `lv_tick` (`NativeHal.cpp:233`) | 250 ms via `millis` (`SystemApp.cpp:234`) |
| D8 | returnToMenu modifier reset | `KeyboardManager::reset()` (`NativeHal.cpp:1071`) | no reset (`SystemApp.cpp:843-866`) |
| D9 | Tick source | SDL wall clock or `--deterministic` synthetic tick (`NativeHal.cpp:190, 1988, 2052-2054`) | Arduino `millis()` |
| D10 | Persistence boot path | `nativeFS_init()` shim → `emulator_data/` (`NativeHal.cpp:2148-2156`, `src/hal/FileSystem.cpp:38-88`) | `LittleFS.begin` + proactive `/vars.dat` create (`SystemApp.cpp:143-155`) |
| D11 | open_app / assert hooks | emulator-only additions | none |
| D12 | Symbol entry | OS TEXTINPUT delivers `* ( ) < >` directly (`NativeHal.cpp:395-403, 773-793`) | physical SHIFT combos |
| D13 | Status bar content | static "rad"/battery in menu (`MainMenu.cpp:321-356`); no live modifier indicator | same LVGL menu bar, plus TFT legacy paths (`SystemApp.cpp:343-365`) |
| D14 | STEP_VIEW / APP_TABLE modes | absent | present (mostly placeholder) |

Parity rulings for these divergences (which must be closed vs. documented as emulator-only shims) live in `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` §I.
=======
### B.1 SystemApp launcher path

- `SystemApp::begin()` news all 20 apps (21 with `NUMOS_MATH_VISUAL_APP_ENABLED`)
  without calling `begin()` (`SystemApp.cpp:105-127`), creates the LVGL MainMenu,
  binds `launchApp` as the card callback (`SystemApp.cpp:135-138`), then mounts
  LittleFS and loads `/vars.dat` (`SystemApp.cpp:144-156`).
- `MainMenu::APPS[]` (`src/ui/MainMenu.cpp:88-113`) is the card source of truth:
  ids 0-19 unconditional + id 20 under `NUMOS_MATH_VISUAL_VERIFY`. ENTER on a card →
  `launchApp(id)` → `switchApp(id)` sets `_mode` (`SystemApp.cpp:885-913`) and
  `<app>->load()` (`SystemApp.cpp:744-855`).
- The separate `_apps` vector (`initApps()`, `SystemApp.cpp:162-180`) feeds only the
  legacy TFT `renderMenu()`; its "11-18 are hidden/experimental" comment
  (`SystemApp.cpp:175`) does **not** apply to the LVGL launcher.

### B.2 App construction/destruction

- Deferred teardown: `returnToMenu()` records `_pendingTeardownMode` + `millis()`
  (`SystemApp.cpp:860-883`); `update()` calls `teardownModeNow()` after 250 ms
  (`SystemApp.cpp:243-250`); `launchApp` flushes any pending teardown first
  (`SystemApp.cpp:735`).
- `teardownModeNow(Mode)` is the firmware analogue of `performAppTeardown`: one
  `end()` case per app (`SystemApp.cpp:182-218`), plus a memory-probe exit marker
  under `NUMOS_MEM_PROBE_ENABLE` (`SystemApp.cpp:213-217`) — a leak canary the
  emulator has no equivalent of.

### B.3 Keyboard dispatch

- `SystemApp::handleKey` filters to PRESS/REPEAT only (`SystemApp.cpp:461`) — note
  the emulator forwards RELEASE into apps and relies on each app to ignore it
  (`NativeHal.cpp:629-644`), whereas firmware apps never see RELEASE at all.
- Global modifier handling: SHIFT/ALPHA toggle `KeyboardManager` **except** when
  the active mode is APP_CALCULATION or APP_CALCULUS, which own their modifier
  handling (`SystemApp.cpp:484-493`). The emulator has no such global layer —
  every key goes straight to the app.
- SHIFT+AC power-off is intercepted globally and currently disabled
  (`SystemApp.cpp:475-481`, `powerOff()` stub `SystemApp.cpp:1052-1057`).

### B.4 MODE/HOME and app-specific exit behavior

Per-app exit rules in `SystemApp::handleKey` (`SystemApp.cpp:495-671`):

| App | Exit keys | Extra behavior |
|---|---|---|
| Most apps | MODE | plain `returnToMenu()` |
| PeriodicTable | MODE **or AC** (`SystemApp.cpp:580`) | — |
| CircuitCore | MODE; also AC when toolbar focused | `autoSave()` before exit (`SystemApp.cpp:595-606`) |
| Fluid2D | MODE | `autoSave()` before exit (`SystemApp.cpp:608-615`) |
| Fractal | **AC only** (no MODE case) | plus app-initiated exit via `consumeExitRequest()` (`SystemApp.cpp:649-658`) |
| MathVisual | MODE or AC (`SystemApp.cpp:660-666`) | — |
| Grapher | MODE; AC at tab level on Expressions tab also exits (`SystemApp.cpp:1019-1033`) | emulator does **not** replicate the AC-exit path — GrapherApp receives AC and handles internal focus retreat only (`NativeHal.cpp:733-749`) |
| Equations | MODE, but keys forwarded only `if (_equationsApp->isActive())` (`SystemApp.cpp:513-519`) | Equations also gets a cooperative `update()` tick (`SystemApp.cpp:265-269`) |
| Fractal | — | gets `update()` tick (`SystemApp.cpp:290-292`) |

The emulator main loop calls **no app `update()` at all** — there is no per-frame
app tick in `NativeHal.cpp`'s loop (2045-2130). Any app whose behavior depends on
`update()` (EquationsApp staged solve pipeline, FractalApp render state machine)
cannot be enabled without adding an update dispatch (ROUTE-04).

---

## C. Divergences (verified inventory)

| # | Divergence | Emulator evidence | Firmware evidence | Impact on P-05 |
|---|---|---|---|---|
| D1 | Two hand-written mode enums/dispatchers | `NativeHal.cpp:195-206` | `SystemApp.h:76-103` | every new app touches 8 NativeHal sites (§A.4) |
| D2 | Dead launcher cards: 13 cards render but only print a console line on ENTER | `NativeHal.cpp:993-995`; cards from `MainMenu.cpp:88-113` | all 20 ids launch (`SystemApp.cpp:744-855`) | scripts can't detect a failed launch; see F.3 |
| D3 | RELEASE events forwarded to apps (apps must self-filter) | `NativeHal.cpp:629-644` | PRESS/REPEAT filter at dispatcher (`SystemApp.cpp:461`) | new apps must tolerate RELEASE; add to checklist |
| D4 | No global SHIFT/ALPHA layer in emulator | absent from `dispatchKey` (575-769) | `SystemApp.cpp:484-493` | apps relying on SystemApp toggling modifiers behave differently |
| D5 | Modifier reset on returnToMenu exists in both, but emulator resets **only** there | `NativeHal.cpp:1081` | firmware also resets on SHIFT+AC (`SystemApp.cpp:477-478`) | minor |
| D6 | No app `update()` tick in emulator loop | loop body `NativeHal.cpp:2045-2130` | `SystemApp.cpp:238-304` | blocks Equations (staged pipeline) and Fractal (state machine) until ROUTE-04 |
| D7 | Grapher AC-at-tab-level exit path missing in emulator | `NativeHal.cpp:733-749` | `SystemApp.cpp:1024-1030` | existing parity gap; scripts use `home` instead |
| D8 | CircuitCore/Fluid2D autoSave-on-exit is SystemApp-side, not app-side | n/a in emulator | `SystemApp.cpp:595-615` | if these apps are ever enabled, the emulator must replicate the exit hook or storage behavior diverges |
| D9 | CalculationApp teardown re-begins immediately in emulator | `NativeHal.cpp:1010-1015` | firmware relies on lazy `load()` re-begin | screen pre-exists on relaunch; cosmetic timing difference |
| D10 | `g_grapherApp` missing from exit cleanup | `NativeHal.cpp:2144-2151` | firmware has no process exit | breaks symmetric-teardown invariant (ROUTE-03) |
| D11 | Settings/vars globals defined twice | `NativeHal.cpp:125-127` | `main.cpp:31-33` (per header comment `NativeHal.cpp:119-124`) | values match today; drift possible |
| D12 | Emulator ships a committed persistent store: `emulator_data/vars.dat` is git-tracked and loaded at boot | `NativeHal.cpp:2171-2179`; `git ls-files` shows `emulator_data/vars.dat` | firmware starts from device LittleFS | fixture-hygiene precondition; see persistence sandbox spec |
| D13 | `VariableContext` NVS persistence silently no-ops natively | `VariableContext.h:36-50` (native stubs) | NVS `calcVars` on device | Grapher/legacy vars are session-only in emulator |
| D14 | Firmware MEM probe on app exit; emulator has none | — | `SystemApp.cpp:213-217` | emulator teardown-loop tests can't watch heap trend without a new probe |
| D15 | Heartbeat log collapses all app modes to "CALC" | `NativeHal.cpp:2113-2118` | n/a | cosmetic; misleading logs for new apps |
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## D. Desired routing pattern for new apps **[PROPOSED]**

<<<<<<< HEAD
Adding one app to the emulator MUST touch exactly this checklist (ticket ROUTE-01 turns it into a PR template). Deviating from the list (e.g., forgetting the teardown case) is the #1 historical source of emulator hangs (see memory of the returnToMenu hang; determinism spec §fixtures).

1. **platformio.ini**: add the app's `.cpp` (and engine companions) to `[env:emulator_pc] build_src_filter` as individual files, never globs (`platformio.ini:195-266` convention), with a comment naming the phase and the dependency-closure argument (why each new TU is native-safe).
2. **AppMode**: add one enum value (`NativeHal.cpp:195-206`).
3. **Globals + construction**: add `g_<app>` pointer; construct in `transitionToMenu()` with **lazy** `begin()` (only CalculationApp is eager; new apps must be lazy — LVGL heap constraint, `CLAUDE.md` Key Constraint 1).
4. **launchApp**: add the `case <id>:` matching the app's `MainMenu::APPS[]` id (`src/ui/MainMenu.cpp:88-113`), calling `flushPendingTeardown()`-protected `load()` and setting `g_mode`.
5. **dispatchKey**: add the app-mode case with the standard shape: MODE-on-down-edge → `returnToMenu()`, else forward `KeyEvent{kc, action, -1, -1}` to `handleKey`. If firmware gives the app dispatcher-level AC-exit/autosave (divergence D5), replicate that here and cite the firmware line in a comment.
6. **performAppTeardown**: add the case calling `end()`. Verify the app's `end()` calls `_statusBar.destroy()` before `lv_obj_delete(_screen)` (Key Constraint 2) — if it doesn't, the app is NOT enablement-ready; file a LIFE-* ticket instead.
7. **activeAppName / scriptAppNameToId**: add the canonical name (single spelling, no spaces; matches the matrix doc's "canonical script name" column) to both (`NativeHal.cpp:1656-1668, 1414-1428`).
8. **assert_app support** comes free from step 7.
9. **State reset**: define what `end()` clears vs. persists (see §D.1). If the app holds cross-launch state (e.g., autosaved documents), the STORE-* persistence inventory must be updated first.
10. **Smoke script**: land `tests/emulator/scripts/<app>_smoke.numos` in the same PR (APPQA-01 harness shape), and register its candidate stem per the CI candidate policy.

### D.1 App state reset between scripts **[PROPOSED]**

Policy: **one script = one process**. The emulator process is single-shot per `.numos` run (`--script` CLI, `NativeHal.cpp:1235-1270`), so in-RAM state resets naturally. The only cross-run state is the filesystem sandbox (`emulator_data/`, `src/hal/FileSystem.cpp:38`). Therefore:

- No in-process "reset app" command is added.
- Cross-run isolation is delegated to the fixture sandbox defined in `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` / determinism spec (fresh or fixture-seeded data dir per script). Apps that write files may not be golden-gated until that lands (STORE-01 gate).

### D.2 open_app semantics for newly enabled apps **[PROPOSED]**

- `open_app` remains a direct `launchApp(id)` call (no synthetic menu navigation). Scripts that need to test menu navigation itself use `key` + `assert_menu_focus`.
- Parse-time rejection of unknown app names is kept (fail-fast is a feature), but the error message must list the wired names so a stale script fails legibly.

### D.3 Dead-app selection reporting **[PROPOSED]** (answers Special Question 11/12)

- Cards stay **visible-but-refusing** (hiding them would fork the launcher layout from firmware and invalidate menu goldens; disabling styles would also diverge visually).
- The `default:` branch in `launchApp` keeps the console line but with a stable, greppable prefix: `[APP] unsupported-in-emulator id=<n> name=<card>`; scripts get a deterministic observable via the existing behavior that `g_mode` stays MENU (`assert_app Menu` after `open_app` is impossible since parse rejects; after a *click path* via keys, `assert_app`-style verification uses `assert_menu_focus`).
- ROUTE-02 adds a negative-control script asserting exactly this refusal behavior so an accidental half-wiring (case added, app not constructed → null deref) fails CI instead of crashing users.
=======
### D.1 The per-app enablement checklist (ROUTE-01)

Adding app X to the emulator is exactly this closed list; a PR that skips a line is
incomplete:

1. `platformio.ini` `build_src_filter`: add `+<apps/XApp.cpp>` and every new
   transitive `.cpp` (engines, cas:: files, math files) — individual files only, no
   globs (existing policy, `platformio.ini:232`).
2. `NativeHal.cpp` `AppMode`: append one enumerator (append-only, never reorder).
3. Instance pointer + lazy construction in `transitionToMenu()` (constructor only;
   never eager `begin()` unless the app is the boot-critical CalculationApp).
4. `launchApp()`: one `case <launcher-id>:` calling `x->load(); g_mode = ...;`.
5. `performAppTeardown()`: one case calling `x->end()` (no re-begin unless justified).
6. `dispatchKey()`: one case with the standard contract — MODE+down → returnToMenu,
   else forward KeyEvent with `row=col=-1`. App-specific exit keys (AC, EXE) are
   routed **inside the app**, not in NativeHal, unless firmware SystemApp itself
   special-cases them (D7/D8); if it does, replicate the SystemApp behavior and cite
   it in a comment.
7. `canonicalAppName()` + `scriptAppNameToId()` + `activeAppName()`: add the name
   triple. Names must equal the `MainMenu::APPS[]` card name modulo
   case/spaces, and `assert_app` output must match `open_app` input.
8. Exit cleanup block: `end()` + `delete` (fixes the D10 class by construction).
9. If the app needs a per-frame `update()` (Equations, Fractal): add to the
   ROUTE-04 update dispatch (§D.4).
10. Scripts: at minimum `<app>_smoke.numos` per the QA plan; candidate stem
    registered in `scripts/generate-emulator-candidates.py`.
11. `docs/emulator-sdl2-quickstart.md` command table updated.

A single greppable marker comment (`// APP-ENABLE(<Name>)`) at each touched site is
required so `scripts/check-emulator-deps.py`-style tooling can later verify the
eight sites stay in sync (ROUTE-05 proposes automating this check).

### D.2 open_app / assert_app for new apps **[PROPOSED]**

- `open_app` keeps calling `launchApp(id)` — same code path as a menu ENTER; no
  bypass constructors.
- `open_app` on a compiled-out app must become a **script-load error** (exit 2),
  which it already is (unknown name); the failure mode to fix is the *menu* path
  (§F.3).
- `assert_app` gains no new semantics; each new app adds its canonical name.

### D.3 Home/MODE for new apps **[PROPOSED]**

Uniform: MODE on down-edge → `returnToMenu()` with deferred teardown, per §A.5.
Apps with firmware-side AC-exit or autosave-on-exit semantics (PeriodicTable,
CircuitCore, Fluid2D, Fractal, Grapher-AC, MathVisual) must replicate the exact
SystemApp conditions cited in §B.4, with the SystemApp line cited in a comment.
Divergence here is a firmware-parity checklist failure (parity spec §I).

### D.4 App update ticks (ROUTE-04) **[PROPOSED]**

Add to the NativeHal main loop, after `lv_timer_handler()`:

```cpp
// Firmware parity: SystemApp::update() gives some apps a cooperative tick.
if (g_mode == AppMode::EQUATIONS && g_equationsApp && g_equationsApp->isActive())
    g_equationsApp->update();
if (g_mode == AppMode::FRACTAL && g_fractalApp)
    g_fractalApp->update();
```

Scope: only apps that firmware ticks (`SystemApp.cpp:265-269, 290-292`). Do not
invent ticks for apps firmware doesn't tick.

### D.5 App state reset between scripts **[PROPOSED]**

Each CI script invocation is a fresh process (`generate-emulator-candidates.py`
runs the binary per stem), so in-RAM state resets by construction. The two
carriers of cross-run state are:

1. `emulator_data/` (LittleFS root, `src/hal/FileSystem.cpp:38`) — governed by the
   persistence sandbox spec; P-05 apps that write files must run under the
   sandboxed fixture root before their scripts are gated (STORE-01).
2. The committed `emulator_data/vars.dat` loaded at boot
   (`NativeHal.cpp:2171-2179`) — scripts must treat it as a **fixture** with
   defined content, or the sandbox spec's per-run scratch copy must land first.

Within one script, state reset across app switches is *deliberately* not provided:
firmware keeps app state alive across menu round-trips too (objects persist,
`end()` destroys screens, not engines). Scripts asserting "fresh app" semantics
must encode the app's own clear action (e.g. AC) rather than assuming reset.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## E. Input parity

<<<<<<< HEAD
### E.1 Current alias tables (verified)

- Script names: `scriptNameToKeyCode` (`NativeHal.cpp:446-555`) — symbols `+ - * / ^ . ( ) = < >`; digits; `enter/left/right/up/down/backspace|del|delete/ac|esc|escape/home|mode/graph`; `x|varx`, `y|vary`, `pow`, `frac|fraction|div`, `sqrt/sin/cos/tan/ln/log/pi/e/negate|neg`, `ans/preans/shift/alpha/sto/freeeq|sd`, `exe/table/f1..f5` (`:531-537`), `logbase|log_n/zoom/lt|less/gt|greater` (`:547-552`).
- SDL keysyms: `mapSdlToKeyCode` (`NativeHal.cpp:308-390`) — nav/control/modifiers plus letter shortcuts (`s`→SIN, `c`→COS, `t`→TAN, `l`→LN, `g`→GRAPH, `m`→LOG, `b`→LOG_BASE, `r`→SQRT, `p`→POW, `o`→CONST_PI, `e`→CONST_E, `x/y/a/f/n`, F5→FREE_EQ, `h`/Home→MODE, Tab→ALPHA, Insert→STO); printable chars deliberately NONE (`:388`) and instead resolved via `mapTextChar` on SDL_TEXTINPUT (`:405-434`).
- Firmware SerialBridge uses a **different** single-char table (`src/input/SerialBridge.cpp:174-233`; e.g. serial `s`=DOWN, `t`=SIN, `<`=EXE) — scripts are NOT SerialBridge-compatible and never were.

### E.2 Policy **[PROPOSED]**

- **Key alias policy**: one canonical script name per KeyCode; aliases are append-only and never repurposed. New apps that need a KeyCode not yet script-reachable (e.g., app-specific F-row semantics) extend `scriptNameToKeyCode` in the same PR, documented in the script-API table of this spec.
- **Unsupported keys**: KeyCodes with no script name are unsupported by policy until a test needs them; scripts must not encode raw numbers.
- **SHIFT/ALPHA**: emulator keeps forwarding modifiers to apps (divergence D3). For apps that rely on firmware's dispatcher-level toggling, parity requires replicating that in `dispatchKey` per-app — this is decided per-app in the matrix; the default for P-05 apps is "app owns modifiers" (true for Calc-style apps).
- **SETUP/secondary legends**: not script-reachable today; out of scope for P-05 (no P-05 app requires it).
- **Text input**: only via per-key entry. Apps with free-text editors (Python, NeoLanguage) would need either a `text "..."` script command or per-char alpha sequences — one reason they are deferred (see matrix).
- **Repeat/autorepeat**: SDL repeat produces `REPEAT` actions (`NativeHal.cpp:822-824`); scripts cannot express repeat except as repeated `key` lines. Scripts must not depend on autorepeat timing.

---

## F. Assertions and script APIs **[PROPOSED additions]**

Existing commands stay frozen (backward compatibility with all blessed scripts). Additions, in priority order:

| Command | Semantics | Feasibility |
|---|---|---|
| `assert_app NAME` | exists (`NativeHal.cpp:1722-1730`) | — |
| `assert_menu_focus` | exists (`:1813-1838`) | — |
| `assert_no_crash` | implicit today (process exit code); make explicit as a final-line marker that asserts the script reached EOF with `g_mode` valid; cheap | trivial |
| `assert_statusbar_title TEXT` | read `ui::StatusBar` current title; needs a `debugTitle()` getter on StatusBar (single string, no LVGL walk) | low |
| `assert_app_ready` | asserts active app's screen == `lv_scr_act()` and teardown not pending; guards against asserting during fade | low |
| `assert_lvgl_object_count N` / leak check | count descendants of `lv_scr_act()`; useful as a **delta** check around enter/exit loops, not absolute counts (theme changes would churn goldens) — expose as `assert_lvgl_object_delta 0` bracketed by `mark_lvgl_objects` | medium; feasible |
| `assert_storage_clean` | asserts `emulator_data/` matches the fixture manifest (defers to persistence sandbox spec's manifest format) | blocked on STORE-01 |
| `assert_app_dirty_state` | per-app "unsaved changes" getter; only meaningful for autosaving apps (Circuit, NeoLang) — deferred with those apps | deferred |
| App-specific hooks | pattern: `debug*()` const getters on the app (like `CalculationApp::debugLastResult`), one `assert_<app>_<fact>` script command each; every hook must be read-only and allocation-free | per-app, see QA plan |

Rule: **assert commands never mutate state** and must be usable between any two script lines without changing subsequent rendering (hooks that force layout are forbidden).
=======
### E.1 Current key alias surface (verified)

- SDL live-input map: `NativeHal.cpp:318-400` (navigation, control, modifiers,
  letter shortcuts) + `SDL_TEXTINPUT` chars `NativeHal.cpp:415-444`.
- Script name map: `scriptNameToKeyCode`, `NativeHal.cpp:456-565`. Covered tokens:
  symbols `+ - * / ^ . ( ) = < >`, digits, `enter/left/right/up/down`,
  `backspace|del|delete`, `ac|esc|escape`, `home|mode`, `graph`, `x|varx`,
  `y|vary`, `pow`, `frac|fraction|div`, `dot/add/sub/mul/lparen/rparen`,
  `sqrt/sin/cos/tan/ln/log/pi/e`, `negate|neg`, `ans`, `preans`, `shift`,
  `alpha`, `sto`, `freeeq|sd`, `exe`, `table`, `f1..f5`, `logbase|log_n`,
  `zoom`, `lt|less`, `gt|greater`.
- `input_alias_surface.numos` exists as an alias-coverage script
  (`tests/emulator/scripts/`).

### E.2 KeyCodes with **no** script token today (verified against KeyCodes.h)

`SETUP` (KeyCodes.h:41), `ON` (48), `TRACE` (63), `SHOW_STEPS` (64), `SOLVE` (65),
`NEG` physical (77 — distinct from virtual `NEGATE`), `ALPHA_A..ALPHA_F` (97-102),
`FACT` (108), `F3`/`F4` have tokens (`NativeHal.cpp:545-546`) but `F3/F4` are
consumed by no current emulator app.

**[PROPOSED]** Alias policy for P-05:

- Add tokens only when an enabled app's handler consumes the KeyCode (the Phase
  8B/9A precedent, comments `NativeHal.cpp:537-558`): tokens never invent
  KeyCodes, and unused aliases are allowed only as documented forward stubs.
- P-05 wave needs, per the app matrix: `solve` (EquationsApp/MatricesApp solve
  actions if their handlers consume SOLVE), `alpha_a..alpha_f` (Equations variable
  entry if ALPHA_x codes are consumed directly), `setup` only if an enabled app
  consumes SETUP. Each addition cites the consuming `handleKey` line in a comment.
- SHIFT/ALPHA in scripts drive the same `KeyboardManager` FSM as firmware
  (`src/input/KeyboardManager.cpp:42-92`); scripts should prefer modifier
  sequences over direct ALPHA_x tokens when testing firmware-realistic flows, and
  may use direct tokens for terseness where the app consumes them identically.

### E.3 Physical-key mapping caveats

- Physical scanning is disabled on hardware (`src/drivers/Keyboard.h:82`,
  `CONNECTED_COLS = 0`); firmware input arrives via SerialBridge. Emulator parity
  target is therefore the **KeyCode stream**, not a physical matrix.
- OS autorepeat produces REPEAT actions in the emulator (`NativeHal.cpp:826-834`);
  scripts have no `repeat` primitive — `keydown`/`keyup` pairs cannot synthesize
  REPEAT actions. Apps whose UX depends on autorepeat (scroll-heavy apps) can only
  be REPEAT-tested live, not by script. Accepted limitation for P-05; a
  `keyrepeat NAME` token is a candidate future command (kept out of scope).

### E.4 Text input

No enabled or P-05-candidate app uses LVGL textarea free-text input; app text
entry (Python IDE, NeoLang editor) is one reason those apps are deferred (see
matrix). The `SDL_TEXTINPUT` path exists solely to resolve keyboard-layout symbols
(`NativeHal.cpp:783-803`).

---

## F. Assertions and script APIs **[PROPOSED unless cited]**

### F.1 Existing generic commands (verified)

`assert_app`, `assert_menu_focus`, `assert_result[_contains]`, `assert_no_error`,
`assert_error`, `assert_variable` — see §A.7 table.

### F.2 Proposed generic commands for P-05

| Command | Semantics | Feasibility |
|---|---|---|
| `assert_app_ready` | active app's `debugIsReady()` hook returns true (screen built, first layout done) | needs a per-app NATIVE_SIM hook; cheap — most apps can report `_screen != nullptr` |
| `assert_no_crash` | not a runtime command: the *absence* of exit≠0 plus a final `assert_app` already encodes it; document as an idiom, do not implement | idiom only |
| `assert_statusbar_title TEXT` | reads the active `ui::StatusBar` title string via a NATIVE_SIM accessor | requires StatusBar debug hook + a registry of "current bar"; medium |
| `assert_lvgl_object_count N` / `assert_lvgl_object_count_between A B` | walks `lv_obj` tree of active screen | feasible via LVGL child traversal; brittle to UI evolution — use for leak deltas (LIFE-02), not absolute values, and keep warning-only |
| `assert_app_dirty_state ...` | per-app dirty flag | only meaningful per app; defer to app-specific hooks |
| `assert_storage_clean` | fixture root matches its pre-run snapshot | belongs to the sandbox spec's dirty-tree guard, exposed to scripts once STORE-01/sandbox lands; until then implemented as a CI-side directory diff, not a script command |

Per-app hooks follow the CalculationApp precedent exactly: `#ifdef NATIVE_SIM`
read-only const accessors in the app header (`CalculationApp.h:95-107` pattern),
consumed by new `case` arms in `scriptStepBegin`. Hook proposals per app live in
the QA plan (Document 4 §D).

### F.3 Dead-app selection reporting (answers special question 11)

Current: menu-ENTER on an unrouted card prints to stdout and silently stays in
MENU (`NativeHal.cpp:993-995`). Proposed:

- Keep the menu behavior (visible-but-refusing, see parity spec §J for the
  decision rationale), but make it **script-detectable**: after `launchApp` on an
  unsupported id, a following `assert_app Menu` already passes today — that is the
  wrong polarity (silent failure looks like success). Add a
  `[APP] launch-refused id=<n> name=<name>` structured log line and a negative
  control script (`route_dead_card_control.numos`) that asserts refusal via
  `assert_app Menu` **by design** and is documented as such (ROUTE-02).
- `open_app` for unrouted apps stays a load-time error (exit 2) — scripts cannot
  even express launching them, which is the stronger guarantee.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## G. Teardown and lifecycle tests **[PROPOSED]**

<<<<<<< HEAD
Grounded in the verified lifecycle: deferred teardown (`NativeHal.cpp:1055-1077, 2074-2078`), `flushPendingTeardown` on relaunch (`:920`), and the firmware Key Constraints (status-bar-destroy ordering, 250 ms gap).

1. **Repeated enter/exit loop** (LIFE-01): for each enabled app, a script doing `open_app X` → `wait 30` → `key home` → `wait 30`, ×10, ending `assert_menu_focus 0`-style sanity + final screenshot. Catches `end()` ordering bugs and re-`begin()` misfires (Key Constraint 2). Wait ≥ 17 frames must cover TEARDOWN_DELAY_MS at the deterministic step size — compute from `--step-ms`, don't hardcode.
2. **HOME while timers active** (LIFE-02): for timer-using apps (Grapher async POI; simulations), enter, trigger the timer-heavy state, HOME immediately (no wait), then relaunch. Catches use-after-free in timer callbacks referencing the torn-down screen.
3. **App switch while modal open**: open an app's modal/submenu, then `open_app` another app directly (exercises `flushPendingTeardown` path `:1040-1045`).
4. **AC/MODE matrix**: per app, assert AC does what firmware's dispatcher does for it (divergence D5 must be resolved for that app first).
5. **Destructor safety**: not testable in-process (apps are never `delete`d, only `end()`d — both sides); out of scope.
6. **LVGL object leak detection**: the `mark/assert_lvgl_object_delta` pair from §F run around LIFE-01 loops; a leaking app fails with a count, not a hang.
=======
Verified baseline: teardown correctness is protected today only by the
grapher home-return scripts (e.g. `grapher_home_return_smoke.numos`,
`grapher_tan_exit_smoke.numos`) and Phase 9F design (§A.5). There is no repeated
enter/exit loop test, no timer-active-exit test, no leak detection.

Planned tests (details and gates in the QA plan; tickets LIFE-01/LIFE-02):

1. **Repeated enter/exit loop** (`life_enter_exit_loop.numos` per app): open app →
   minimal interaction → `home` → `assert_app Menu` → repeat ×5 within one process.
   Catches non-idempotent `begin()`, dangling StatusBar pointers, double-delete.
   The 260 ms teardown delay must be respected between iterations
   (`wait` ≥ 17 frames at 16 ms/frame).
2. **HOME while timers active**: enter Grapher, trigger `_poiAsyncTimer`
   (`GrapherApp.cpp:2994`) or template load timer (`GrapherApp.cpp:1508`), `home`
   immediately, then relaunch. Extends to every timer-owning app as it is enabled
   (Bridge 16 ms, Circuit 16 ms, Optics/Particle/Neural/Fluid 33 ms timers —
   `BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `OpticsLabApp.cpp:253`,
   `ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `Fluid2DApp.cpp:257`). The
   app's `end()` must delete its timers; the test's oracle is process exit 0 +
   `assert_app` after relaunch.
3. **App switch while modal open**: for apps with modal overlays (Equations step
   view, Matrices editors), open modal → `home` → relaunch → assert base state.
4. **AC/MODE contract**: per-app script asserting the §B.4 exit table behavior.
5. **Destructor safety**: process-exit path already runs `end()`+`delete`+
   `lv_deinit()` (`NativeHal.cpp:2142-2156`); every enabled app must be in that
   block (fixes D10).
6. **LVGL object leak detection**: only after `assert_lvgl_object_count` (F.2)
   exists; run enter/exit loop and compare object count at menu between iteration
   1 and N — delta > 0 is a leak. Warning-only until proven stable.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc

---

## H. Implementation tickets

<<<<<<< HEAD
Defined in full (motivation/evidence/files/rollback/risk) in `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md`. Routing-owned subset:

- **ROUTE-01** — Generic app enablement checklist (§D) as PR template + reviewer checklist.
- **ROUTE-02** — Dead-card negative control script + stable refusal log line (§D.3).
- **ROUTE-03** — `assert_app_ready`, `assert_no_crash`, `assert_statusbar_title` script commands (§F).
- **ROUTE-04** — Per-app dispatcher parity shims for firmware AC-exit/autosave apps (divergence D5), one sub-ticket per app, blocked on that app's enablement ticket.
- **ROUTE-05** — `mark_lvgl_objects` / `assert_lvgl_object_delta` (§F), consumed by LIFE-01/LIFE-02.
=======
Routing tickets defined here; full ticket bodies (motivation/evidence/files/tests/
rollback) live in `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md`:

- **ROUTE-01** — Generic app enablement checklist (§D.1) landed as a doc block in
  `NativeHal.cpp` + PR template item.
- **ROUTE-02** — App-route negative controls: dead-card refusal script + broken
  route control (§F.3).
- **ROUTE-03** — Add `g_grapherApp` (and every future app) to the exit cleanup
  block (§A.8 / D10).
- **ROUTE-04** — App `update()` dispatch in the emulator main loop for Equations/
  Fractal parity (§D.4 / D6).
- **ROUTE-05** — Route-sync checker: script that greps the eight NativeHal sites +
  whitelist and fails on a partial enablement (mechanizes §D.1).
- **ROUTE-06** — `assert_app_ready` + per-app ready hooks (§F.2).
- **ROUTE-07** — `assert_statusbar_title` (§F.2), needed for showcase-class apps
  whose only cheap semantic signal is the bar title.
- **ROUTE-08** — Script-token additions for newly consumed KeyCodes (§E.2),
  strictly consumer-cited.
>>>>>>> b62382e9c486561b4df415f88a54cb7d1e0df8fc
