# NumOS — System Chrome QA Oracle & Golden Plan

> **What "done" means:** every system-chrome contract in SET/SBAR/GSTATE has an executable oracle — a semantic assert, a golden, or an explicitly-manual checklist row — and the chrome stops being the part of the screen CI masks away. Companion docs: `NUMOS_SETTINGS_APP_PRODUCT_SPEC.md` (SET), `NUMOS_STATUSBAR_TRUTHFULNESS_AND_SYSTEM_CHROME_SPEC.md` (SBAR), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE), `NUMOS_SYSTEM_CHROME_IMPLEMENTATION_ROADMAP.md` (SCROAD). This plan extends — never contradicts — `NUMOS_CORE_APPS_QA_ORACLE_AND_STRESS_PLAN.md` (core-QA) and reuses hook names already reserved by the Grapher hooks spec (GHOOK: `set_angle_mode`, `assert_graph_angle_mode`) and core-QA-03 (`assert_status_indicator`).
>
> Audit base: branch `claude/numos-system-chrome-spec-0gtoxn` @ `5fc5ea3` (2026-07-04). **Namespace note:** QA-* ticket numbers in this pack are the *system-chrome* QA series (SCROAD §0); the core-apps plan's QA-01…QA-08 are cited as core-QA-0n.

---

## A. Existing inventory [V]

### A.1 Scripts, goldens, masks touching chrome

- **Settings**: exactly one script — `tests/emulator/scripts/settings_smoke.numos` (`open_app Settings` → screenshot → `assert_app Settings`), one golden `settings_smoke.ppm`, mask = the standard clock rect.
- **Launcher**: `launcher_smoke.numos` (screenshot only, no asserts; the only maskless golden — the menu has no live clock), `menu_focus_grapher_smoke.numos` (+golden), `menu_enter_launch.numos`, `menu_nav_parity.numos` (2D-grid parity walk mirroring `MainMenu::moveFocusByDelta`, `MainMenu.cpp:159-197`).
- **Home-return**: `grapher_home_return_smoke.numos` (7 HOME cycles, `assert_app` alternation) — CI-gated Phase 9F (`emulator-build.yml:495-531`). No Calculation/Settings equivalent (core-QA notes the calc gap; Settings gap is ours).
- **StatusBar in goldens**: every app golden includes the bar; all 18 masks contain the single identical rect `4,6,37,13` — the live `HH:MM` clock (`StatusBar.cpp:205-225` reads wall time even under `--deterministic`, whose synthetic tick covers only LVGL, `NativeHal.cpp:1998`). No mask covers angle/modifier/battery/title — those are byte-gated today, i.e. gated as **frozen constants** ("RAD", "", full battery), which passively locks in the SBAR §D lies.
- **Candidates**: 25 stems in `scripts/generate-emulator-candidates.py:39-143`, including `settings_smoke` (L47), `menu_focus_grapher_smoke` (L129), `launcher_smoke` (L40).
- **CI**: menu parity gate (`emulator-build.yml:429-461`), Grapher no-hang gate (`:495-531`), candidate generation (`:558-565`), golden compare — mismatch outside mask fails, missing golden warns (`:575-607`).

### A.2 Existing `.numos` grammar (append-only CI contract)

`wait, key, keydown, keyup, screenshot, log, open_app, assert_app, assert_result, assert_result_contains, assert_no_error, assert_error, assert_variable, assert_menu_focus` (`NativeHal.cpp:1317-1362`); exit codes 0/2/3/4; one command per deterministic frame; whole-script parse validation up front. `assert_menu_focus` reads `MainMenu::debugFocusedCardId()` (`NativeHal.cpp:1826-1848`, `MainMenu.h:58-79`).

## B. Gaps [V]

1. **No angle-mode oracle of any kind** — no `set_angle_mode`, no DEG/RAD assert, no trig-vs-badge cross-check (the K-1 bug ships silently under 71 green scripts).
2. **No battery truth test** — icon is byte-frozen at full in every golden; no mock, no absence test.
3. **No SETUP test** — the keycode has no `.numos` name (`NativeHal.cpp` scriptNameToKeyCode: no `setup`, grep-verified), so its future handler can't be scripted until the name is added.
4. **No corrupted-settings test** — nothing is persisted, so nothing can corrupt; the moment GS-02 lands, corruption/quarantine paths (GSTATE §G) ship untested unless QA-04 lands with it.
5. **No persistence roundtrip test** — no way to assert "value X survives restart" (needs either two-process scripting or `--persist-settings` + sequential runs, §E.3).
6. **No SHIFT/ALPHA indicator test** — the FSM (`KeyboardManager.cpp:42-130`) has zero automated coverage; the firmware-vs-emulator reset divergence (`SystemApp.cpp:860-883` vs `NativeHal.cpp:1081`) is invisible to CI by construction (CI only runs the emulator side).
7. **No StatusBar text oracle** — asserts read app debug state only; nothing reads bar labels (title/badge/indicator), so a truthfulness regression can't fail a semantic script today.
8. **No settings-value oracle** — `settings_smoke` proves the screen opens, not that ENTER toggles anything (`SettingsApp.cpp:241-266` is untested).
9. **Hidden persistence flake** — `emulator_data/vars.dat` is git-tracked and mutable by store scripts (GSTATE §F.3); no CI guard notices the dirtied tree.
10. **No stale-indicator test** — Settings' frozen bar (SET §G.6) is inside the masked clock rect for the clock, and byte-frozen "" for the modifier, so the bug reads as "passing".

## C. Needed `.numos` hooks [P → QA-01]

All append-only additions to the grammar (`NativeHal.cpp:1345-1362` enum + parser + executor), NATIVE_SIM-only debug accessors following the `debugLastResult`/`debugFocusedCardId` precedent (`CalculationApp.h:95-107`, `MainMenu.h:58-79`); no OCR, no pixel reads. Every new command ships with a negative control (wrong expectation ⇒ exit 4) per core-QA Stage-2 discipline.

| Command | Reads / writes | Notes |
|---|---|---|
| `set_angle_mode deg\|rad` | writes via SettingsState (GSTATE §F.2) | name reserved by GHOOK §D.4 — one implementation serves both plans |
| `assert_angle_mode deg\|rad` | reads `vpam::g_angleMode` | global engine truth |
| `assert_statusbar_angle DEG\|RAD` | reads the active screen's bar `_angleLabel` text via a `StatusBar` debug accessor | badge truth ≠ engine truth: asserting both catches paint-path bugs GS-01 can't |
| `assert_statusbar_title <text>` | reads `_titleLabel` text | validates SBAR §C.5/§F.3 |
| `assert_setting_value <name> <value>` | reads SettingsState by field token (`angle,complex,precision,edu_steps`) | generic (OQ-GS1) |
| `set_setting_value <name> <value>` | writes via SettingsState | rejects unknown name/domain at parse where static, else exit 4 |
| `assert_shift_state none\|shift\|shift_lock` and `assert_alpha_state none\|alpha\|alpha_lock` | read `KeyboardManager::state()` | split by axis for readable scripts; `sto` accepted by either as the shared terminal state — or a single `assert_modifier <token>`; pick one at QA-01 review, then frozen |
| `assert_launcher_focus <name\|id>` | — | **already exists** as `assert_menu_focus` (`NativeHal.cpp:1596-1607`); no new command — the SET/SBAR docs use the existing name |
| `assert_statusbar_clock <HH:MM>` | reads `_clockLabel` text | only meaningful post-SB-03 (deterministic clock); until then unusable and therefore not added earlier (append-only ≠ add-early) |
| `key setup` | script key name → `KeyCode::SETUP` | prerequisite for ST-02 tests |

Battery: no assert hook — battery truth is visual; covered by mock-mode goldens (§D) plus the firmware-absence golden. A `--battery N` CLI flag (SBAR §E.2) is the injection mechanism.

## D. Visual goldens [P → QA-03; all promotions human-only via `scripts/promote-emulator-golden.py`]

| Golden stem | Content | Depends on | Mask |
|---|---|---|---|
| `settings_initial` | Settings freshly opened, row 0 focused, default values | exists as `settings_smoke` — re-bless only when rows change (ST-01/ST-03 **will** change it: new angle row + placebo resolution) | clock rect until SB-03, then none |
| `settings_angle_deg` / `settings_angle_rad` | angle row in each state, badge matching | ST-01 | ditto |
| `menu_focus_grapher_smoke` | launcher focus ring on card 1 | exists — unchanged | none (menu has no clock) |
| `calc_deg_badge` | Calculation with DEG badge after `set_angle_mode deg` | GS-01, SB-01 | clock/SB-03 |
| `grapher_deg_badge` | Grapher graph view, DEG badge, sin(x) period-360 curve | GS-01, GR-02 | ditto |
| `statusbar_shift` / `statusbar_alpha_lock` | Calculation with `S` / `A-LOCK` painted | SB-04 | ditto |
| `statusbar_battery_mock_20` | bar with `--battery 20` (1 red bar) | SB-02 | ditto |
| `statusbar_no_battery` | firmware-policy default: icon absent | SB-02 | ditto |
| `launcher_dimmed_cards` | emulator launcher with non-launchable cards dimmed | SB-06/PD-B3 — **invalidates** `launcher_smoke` + `menu_focus_grapher_smoke` (re-bless) | none |

Blessing rule (inherited): any SB-01/02/03/06 change to bar pixels invalidates **all 18** existing goldens → one coordinated re-bless wave per SCROAD ordering, not per-ticket dribble.

## E. Semantic tests [P → QA-02]

### E.1 Angle-mode scripts (the K-1 kill set)

- `angle_calc_deg_sin30.numos`: `set_angle_mode deg` → open Calculation → type `sin(30)` → ENTER → `assert_result_contains 0.5` → `assert_statusbar_angle DEG` → `assert_angle_mode deg`.
- `angle_calc_rad_sin30.numos`: RAD twin (`assert_result_contains -0.98`).
- `angle_badge_equals_engine.numos`: flip mode mid-session in Settings via keys (not the hook) → walk Calculation → Grapher → Settings asserting `assert_statusbar_angle` in each — badge equals engine on every screen (SET §E.4).
- `angle_grapher_deg_period.numos`: DEG + `sin(x)` → table oracle `assert_graph_table_value` at 90/270 once GR-14 hooks exist; until then graph golden `grapher_deg_badge` carries it (GC-137).
- `angle_ans_invariant.numos`: evaluate, flip mode, `assert_variable ans` unchanged (CC-58).
- Settings-UI path: `settings_toggle_angle.numos`: open Settings → navigate to angle row → ENTER → `assert_setting_value angle rad` → ENTER → `assert_setting_value angle deg` (proves the UI writes, not just the hook).

### E.2 Settings-value scripts

`settings_toggle_edu.numos` (toggle → `assert_setting_value edu_steps true` → evaluate `2+2` in Calculation → steps exist via existing `assert_result` + F2 flow), `settings_precision_cycle.numos` (cycle wraps 12→6; LEFT reverses — `SettingsApp.cpp:247-315` finally under test).

### E.3 Persistence roundtrip (post GS-02) [→ QA-05]

The emulator is one process per script, so "restart" = two invocations sharing a sandbox: a **runner-level** test (shell/python in CI, not `.numos` grammar): run A `--persist-settings --fs-sandbox $T` sets angle=rad and exits; run B same sandbox asserts `assert_angle_mode rad`; run C without `--persist-settings` asserts default (opt-in honored). Firmware roundtrip is a manual/HIL row (§H).

### E.4 Emulator reset behavior

`determinism_settings_default.numos`: boot → `assert_setting_value angle deg` (default) — run twice in CI self-diff (T5) to prove no cross-run bleed; plus the `git diff --exit-code emulator_data/` tree-clean guard (GSTATE §F.3).

## F. Stress tests [P → QA-06 within this pack; feeds core-QA-08 suite]

- `settings_enter_exit_x10.numos`: 10× (open Settings → MODE) — teardown loop; asserts `assert_app` alternation; memory canary is the existing MT-01 probe pattern on firmware.
- `settings_keymash.numos`: 200 random-ish (scripted, fixed) UP/DOWN/ENTER/LEFT/RIGHT — values stay in-domain (`assert_setting_value` after), no crash; exercises the §G.5 domain guarantee.
- Corrupted settings record (post GS-02): CI step writes a garbage `/settings.dat` into the sandbox → boot → `assert_setting_value angle deg` + expect `[FS] settings=corrupt` in stdout (runner greps; the diag line is part of the contract, GSTATE §G.4).
- `home_mode_transitions.numos`: interleave HOME from Settings/Calculation/Grapher ×5 with `assert_shift_state none` after each return (RT-14 acceptance, emulator side).
- Deterministic idle (BC-41 heir): 2000-frame idle post-SB-03 → screenshot self-diff byte-identical **without** clock mask — the strongest "chrome is deterministic" gate.

## G. CI plan [P → QA-06/QA-07 tickets in SCROAD]

1. **Assert-only scripts** (no goldens): all §E scripts gate as a new "Chrome semantics" step in `emulator-build.yml`, same contract as the menu-parity step (`emulator-build.yml:441-457`: rc + PASS-marker + FAIL-grep).
2. **Warning-only candidates**: `launcher_dimmed_cards`, battery-mock goldens enter as candidates (missing-golden warns, `emulator-build.yml:600-604`) until human blessing promotes them to gated.
3. **Gated goldens**: the §D set after blessing; re-bless waves coordinated with SB-01/02/03/06 landings.
4. **Static guards** (extend the keycode-guard pattern, `emulator-build.yml:96-104`): (a) forbid cross-namespace `AngleMode` casts (GSTATE §D.2); (b) forbid direct assignment to `vpam::g_angleMode`/`setting_*` outside SettingsState (GSTATE §C.3); (c) `git diff --exit-code emulator_data/` after all suites.
5. **Negative controls**: per new hook, one inverted script asserted to exit 4 (core-QA T6 discipline); for goldens, the existing wrong-golden control pattern.
6. **Failure artifacts**: on assert fail, auto-screenshot to the artifacts dir (shared need with core-QA-05 — one implementation).
7. **Host unit**: `tests/host/angle_mode_map_test.cpp` for `legacyAngleModeFromVpam()` (GSTATE §D.5), wired next to `keycode_digit_test.cpp` in CI.

## H. Manual QA checklist (firmware/HIL — CI cannot see these)

1. Badge flip on device: Settings → angle row → badge changes on the same screen and in Calculation/Grapher after switch.
2. `sin(30)` on device in DEG = 0.5; Grapher sin period 360.
3. Settings persist across a real power cycle (GS-02); then across a `!fs format CONFIRM` (NVS backend survival, PD-GS2's key promise).
4. Corrupt NVS/`settings.bad` path: forced via diag console; boots to defaults with `[FS]` line, no hang.
5. Battery icon absent on firmware (SB-02) until ADC lands; when it lands: percent sanity at 3 charge levels + charging glyph.
6. Clock/uptime slot renders per PD-B2; no tofu in any bar glyph on device fonts.
7. SHIFT lock → exit to menu → indicator clear on next app entry (RT-14 on hardware).
8. SETUP key (post ST-02 + keyboard bring-up HW-1): opens Settings from every app; idempotent inside Settings.
9. Title never collides with modifier at worst-case app names (SBAR §F.2).
10. Safe-mode boot: Settings usable, Reset row works, settings load skipped (GSTATE §G.5).

*End of system chrome QA oracle & golden plan.*
