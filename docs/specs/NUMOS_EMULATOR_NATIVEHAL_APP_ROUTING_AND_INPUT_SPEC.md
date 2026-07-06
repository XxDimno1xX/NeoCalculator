# NUMOS_EMULATOR_NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC

Status: SPEC-ONLY (P-05 wave). No code in this document is implemented.
Scope: how the native PC emulator (`src/hal/NativeHal.cpp`, compiled only under `NATIVE_SIM`) routes apps, dispatches input, and exposes script/assert APIs — current verified state, divergences from firmware, and the required pattern for enabling new apps.
Companions: `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` (architecture/product), `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (per-app detail), `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (QA), `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md` (tickets).

Evidence convention: every current-state claim cites `file:line`. Sections marked **[PROPOSED]** are design, not current behavior.

---

## A. Current NativeHal routing (verified)

### A.1 AppMode enum

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

---

## B. Current firmware routing (verified)

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

---

## D. Desired routing pattern for new apps **[PROPOSED]**

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

---

## E. Input parity

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

---

## G. Teardown and lifecycle tests **[PROPOSED]**

Grounded in the verified lifecycle: deferred teardown (`NativeHal.cpp:1055-1077, 2074-2078`), `flushPendingTeardown` on relaunch (`:920`), and the firmware Key Constraints (status-bar-destroy ordering, 250 ms gap).

1. **Repeated enter/exit loop** (LIFE-01): for each enabled app, a script doing `open_app X` → `wait 30` → `key home` → `wait 30`, ×10, ending `assert_menu_focus 0`-style sanity + final screenshot. Catches `end()` ordering bugs and re-`begin()` misfires (Key Constraint 2). Wait ≥ 17 frames must cover TEARDOWN_DELAY_MS at the deterministic step size — compute from `--step-ms`, don't hardcode.
2. **HOME while timers active** (LIFE-02): for timer-using apps (Grapher async POI; simulations), enter, trigger the timer-heavy state, HOME immediately (no wait), then relaunch. Catches use-after-free in timer callbacks referencing the torn-down screen.
3. **App switch while modal open**: open an app's modal/submenu, then `open_app` another app directly (exercises `flushPendingTeardown` path `:1040-1045`).
4. **AC/MODE matrix**: per app, assert AC does what firmware's dispatcher does for it (divergence D5 must be resolved for that app first).
5. **Destructor safety**: not testable in-process (apps are never `delete`d, only `end()`d — both sides); out of scope.
6. **LVGL object leak detection**: the `mark/assert_lvgl_object_delta` pair from §F run around LIFE-01 loops; a leaking app fails with a count, not a hang.

---

## H. Implementation tickets

Defined in full (motivation/evidence/files/rollback/risk) in `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md`. Routing-owned subset:

- **ROUTE-01** — Generic app enablement checklist (§D) as PR template + reviewer checklist.
- **ROUTE-02** — Dead-card negative control script + stable refusal log line (§D.3).
- **ROUTE-03** — `assert_app_ready`, `assert_no_crash`, `assert_statusbar_title` script commands (§F).
- **ROUTE-04** — Per-app dispatcher parity shims for firmware AC-exit/autosave apps (divergence D5), one sub-ticket per app, blocked on that app's enablement ticket.
- **ROUTE-05** — `mark_lvgl_objects` / `assert_lvgl_object_delta` (§F), consumed by LIFE-01/LIFE-02.
