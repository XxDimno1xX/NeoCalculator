# NumOS — StatusBar Truthfulness & System Chrome Specification

> **What "done" means:** the StatusBar never lies. Every indicator it paints — clock, title, angle badge, modifier, battery — either reflects verified live state or visibly declares itself unavailable/simulated. The same rule extends to the two sibling chrome surfaces (the launcher's bar and the legacy TFT header). Companion docs: `NUMOS_SETTINGS_APP_PRODUCT_SPEC.md` (SET), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE), `NUMOS_SYSTEM_CHROME_QA_ORACLE_AND_GOLDEN_PLAN.md` (SCQA), `NUMOS_SYSTEM_CHROME_IMPLEMENTATION_ROADMAP.md` (SCROAD).
>
> Audit base: branch `claude/numos-system-chrome-spec-0gtoxn` @ `5fc5ea3` (2026-07-04). Notation: **[V]** verified with `file:line` · **[P]** proposed · **PD-Bn** product decision · **OQ-Bn** open question.

---

## A. Executive summary — the StatusBar must never lie

1. NumOS has **three independent status-bar implementations** (§B.1): `ui::StatusBar` (apps), `MainMenu::buildStatusBar()` (launcher, fully static), and `SystemApp::drawStatusBar()` (legacy TFT paths). They can — and today do — display contradictory "facts" simultaneously across screens.
2. Current lies, ranked (§D): the battery icon always shows full/green with no ADC behind it (risk HW-4); the launcher bar hard-codes the literal `"rad"`; the legacy header hard-codes `"DEG"` off a frozen member while the app badge says `"RAD"`; the clock claims wall time but is refreshed only on key presses; the modifier indicator goes stale in apps that don't repaint it; the emulator presents all of the above as if it were a real device.
3. The truthfulness contract (§C) is one sentence per indicator: **show live verified state, or show that you don't know.** Its enforcement is one architectural change (single StatusBar implementation + one update policy) plus per-indicator policies (battery: hide-until-real; clock: real or deterministic-declared; angle: read the single global; modifier: read the FSM and repaint on every consumed key; title: set by the active app on load, bounded).
4. Emulator truthfulness (§E): interactive runs may show host wall-clock; deterministic/headless runs get a fixed synthetic clock so the 18 identical clock masks (`4,6,37,13`) become unnecessary for new goldens (SB-03).
5. Rendering contract (§F): no tofu, no clipping, one fixed volatile region, a title length policy — so goldens gate chrome pixels instead of masking them away.

---

## B. Current StatusBar architecture [V]

### B.1 Three implementations, one concept

| Surface | Where shown | Impl | Clock | Title | Angle | Modifier | Battery |
|---|---|---|---|---|---|---|---|
| `ui::StatusBar` | every LVGL app screen (each app owns an instance, e.g. `SettingsApp.h:55`) | `src/ui/StatusBar.h/.cpp` | `time(nullptr)` HH:MM (`StatusBar.cpp:205-225`) | set by app (`setTitle`, `StatusBar.cpp:172-177`) | reads `vpam::g_angleMode` (`StatusBar.cpp:243-248`) | reads `KeyboardManager::indicatorText()` (`StatusBar.cpp:231-237`) | 3-bar icon from `_batLevel`, default 100 (`StatusBar.h:102`, `StatusBar.cpp:254-283`) |
| Launcher bar | MainMenu screen | `MainMenu::buildStatusBar()` (`MainMenu.cpp:321-356`) | **none** | literal `"APPLICATIONS"` (`:342`) | literal `"rad"` (`:334`) | **none** | literal `LV_SYMBOL_BATTERY_FULL` (`:351`) |
| Legacy TFT header | non-LVGL render paths (`renderMenu`, `renderAppView`, steps/graph legacy modes) | `SystemApp::drawStatusBar()` (`SystemApp.cpp:353-375`) | none | param, default "NumOS" | reads `SystemApp::_angleMode` = frozen DEG (`SystemApp.cpp:79,372`) | none | painted-full rectangles (`SystemApp.cpp:366-368`) |

None of the three share state or code beyond `g_angleMode`. The launcher bar is never updated after `create()` — every label is a construction-time literal.

### B.2 `ui::StatusBar` anatomy

- 24 px bar (`HEIGHT=24`, `StatusBar.h:46`), children: clock label at `LEFT_MID+6` (`StatusBar.cpp:61-65`), title at `CENTER` (`:68-72`), modifier label at `RIGHT_MID,-80` in gold (`:75-79`), angle label at `RIGHT_MID,-38` (`:82-86`), 22×11 battery box with three 4×7 bars at `RIGHT_MID,-8` (`:89-111`), plus a 1 px separator parented to the **screen**, not the bar (`:114-122`).
- Firmware clock bootstrap: if epoch < 1000 s, set to 2026-01-01 00:00 via `settimeofday` (`StatusBar.cpp:125-142`). Header comment claims the user can adjust the time from Settings (`StatusBar.cpp:20-23`) — **no such setting exists** (SET §D.3, OQ-S2).
- Lifetime: `destroy()`/`resetPointers()` only null pointers; LVGL children die with the parent screen (`StatusBar.cpp:152-166`). Every app must call it before deleting its screen (e.g. `SettingsApp.cpp:90-91`); the contract is documented at `StatusBar.h:61-66`.

### B.3 Update cadence — pull-only, per-app, inconsistent

`update()` refreshes all four dynamic indicators (`StatusBar.cpp:193-199`); **there is no `lv_timer`** — refresh happens only when an app calls it. Verified per-app cadence:

- CalculationApp: on load and on most key paths (`CalculationApp.cpp:133,259-302,513`).
- GrapherApp: on load (`GrapherApp.cpp:236`); SHIFT/ALPHA keys inside Grapher do not necessarily repaint.
- SettingsApp: **only** on load (`SettingsApp.cpp:110`) — clock and modifier freeze for the whole visit (SET §G.6).
- EquationsApp/CalculusApp/IntegralApp: explicitly repaint on SHIFT/ALPHA (`EquationsApp.cpp:1790-1791`, `CalculusApp.cpp:517-518`, `IntegralApp.cpp:388-389`) — the pattern the others lack.

Consequence: the clock is at best key-press-fresh, at worst entry-time-frozen; a modifier lock engaged in one app can display or not depending on which app you're in.

### B.4 Title propagation

Each app sets its own literal at load (`SettingsApp.cpp:79,109`, etc.). No central mapping from mode→title; no length limit; `setTitle` re-centers on every call (`StatusBar.cpp:172-177`). A long title will collide with the modifier label at x-offset −80 — nothing prevents it today.

### B.5 Battery plumbing

`setBatteryLevel(pct)` clamps and repaints (`StatusBar.cpp:183-187`); bars/color thresholds: 3 bars >66 %, 2 >33 %, 1 >5 %, 0 otherwise; green >50 %, yellow >20 %, red ≤20 % (`StatusBar.cpp:254-282`). **All callers pass the constant 100**: `CalculationApp.cpp:153`, `GrapherApp.cpp:278`, `EquationsApp.cpp:571`, `CalculusApp.cpp:247`, `PythonApp.cpp:170`, `NeoLanguageApp.cpp:156`, `IntegralApp.cpp:175`, `MathRenderVisualTestApp.cpp:55`, emulator showcase `NativeHal.cpp:1170`. No ADC/fuel-gauge read exists anywhere (grep `analogRead|adc` — only unrelated hits). Apps that never call it inherit the default 100 (`StatusBar.h:102`).

---

## C. Truthfulness contract [P — normative]

**C.0 Master rule.** An indicator may render a value only if that value is read, at paint time, from the authoritative live source. If the source does not exist on this target (no battery driver, no RTC), the indicator must be absent or visibly marked, never a plausible constant. "Looks right in the demo" is the failure mode this contract exists to kill.

**C.1 Angle badge.** Must equal the evaluator mode of the math the user can run on that screen. Achieved structurally by GS-01: one global, all engines derive from it, badge reads it (`StatusBar.cpp:246` already does). Until GS-01, the badge is *accidentally* truthful for Calculation (both frozen RAD) and *false in spirit* everywhere a legacy engine runs DEG (`SystemApp.cpp:96-97`). The launcher's `"rad"` literal (`MainMenu.cpp:334`) and legacy `"DEG"` (`SystemApp.cpp:372`) both violate C.0 and are retired/re-pointed by SB-01.

**C.2 Battery.** Until a measured source exists (ADC divider or fuel gauge — hardware work, HW-4/P-13): **firmware hides the battery icon** (PD-B1: absence is honest; a full green icon on a draining battery is the worst lie in the product). The API stays; `setBatteryLevel` calls with hard-coded 100 are deleted. When hardware lands, icon returns with measured percent + a distinct charging glyph. **Emulator** shows the icon only in explicit mock mode (`--battery N` CLI, default absent) so goldens can exercise icon states without implying the host has a battery (§E.2).

**C.3 Clock.** Firmware: HH:MM from `time()` is acceptable *as a session clock* only if it is visibly one — since boot resets it to 00:00 (2026-01-01 bootstrap, `StatusBar.cpp:125-142`) with no RTC battery/NTP and no set-UI, a wall-clock-looking `00:07` is a lie of format. PD-B2: until a time source or set-UI exists, firmware renders the same slot as **uptime** `↑H:MM` (unambiguous, useful, honest); revisit when OQ-S2 resolves. Emulator: §E. The stale-refresh half of the problem (B.3) is fixed by the update policy in C.7.

**C.4 SHIFT/ALPHA/STO indicator.** Must equal `KeyboardManager` state at all times on all screens that have a bar. Requires: (a) every app repaints the bar after any key that can mutate the FSM (the EquationsApp pattern, B.3); (b) firmware `returnToMenu()` resets the FSM like the emulator already does (`NativeHal.cpp:1081` vs `SystemApp.cpp:860-883` — divergence; = RT-14/L-7); (c) the launcher, which has no indicator, must not be a screen where a lock can silently persist — satisfied by (b). Indicator strings are the implemented set `"S" "A" "S-LOCK" "A-LOCK" "STO"` (`KeyboardManager.cpp:136-146`); the stale doc claim of `"SL"/"AL"` (`KeyboardManager.h:141`) is a docs fix (UX-02).

**C.5 Title.** Must name the active app, set in `load()`, and match the launcher card name for launchable apps (`MainMenu.cpp:88-113` is the canonical name table). Length policy in §F.3.

**C.6 Emulator hardware humility.** The emulator must not imply hardware facts it cannot know: no battery icon by default (C.2), clock per §E, and — launcher-specific — cards for apps that are not compiled in must not silently no-op on ENTER (today `launchApp` prints `"App %d no implementada"` to stdout and the UI does nothing, `NativeHal.cpp:993-995`). PD-B3: emulator renders non-launchable cards in a dimmed style. (Alternative — hiding them — rejected: it would change card geometry and diverge menu-nav parity scripts, `menu_nav_parity.numos`.)

**C.7 Update policy (mechanism for C.1–C.5).** One rule replaces per-app discretion: the bar repaints (i) on every key event the active app consumes, via a shared call site, and (ii) on a coarse periodic tick for the time field only (60 s granularity; an `lv_timer` owned by the bar is acceptable *only* if it invalidates nothing when the string is unchanged — byte-stable frames when idle, preserving BC-41 and golden determinism).

---

## D. Current inconsistencies — the lie inventory [V]

| # | Lie | Evidence | Severity |
|---|---|---|---|
| 1 | Angle mode unchangeable; two contradictory frozen values displayed (badge RAD / legacy header DEG) | `MathEvaluator.cpp:53` never written; `StatusBar.cpp:246`; `SystemApp.cpp:79,372` | product-breaking (K-1, COR-3) |
| 2 | Launcher bar `"rad"` literal — not even wired to the (frozen) global | `MainMenu.cpp:333-338` | high |
| 3 | Battery always full/green; no measurement exists | `StatusBar.h:102`; nine hard-coded `setBatteryLevel(100)` call sites (§B.5); risk HW-4 | high |
| 4 | Launcher battery is a static full glyph | `MainMenu.cpp:350-355` | high |
| 5 | Legacy header battery is painted-full rectangles | `SystemApp.cpp:366-368` | medium (legacy paths) |
| 6 | Clock looks like wall time but boots at 00:00-of-2026 and refreshes only on key press | `StatusBar.cpp:125-142,205-225`; no timer (`:193-199`) | medium |
| 7 | Comment claims Settings can set the clock — no such UI | `StatusBar.cpp:20-23`; SET §D.3 | doc lie |
| 8 | Modifier indicator stale in Settings (never repainted after load) and inconsistent per app | `SettingsApp.cpp:110` sole update; contrast `EquationsApp.cpp:1790-1791` | medium |
| 9 | Modifier lock survives app exit on firmware but not emulator | `SystemApp.cpp:860-883` (no reset) vs `NativeHal.cpp:1081` (reset) | divergence (C3.9/RT-14) |
| 10 | Indicator doc drift `SL/AL` vs `S-LOCK/A-LOCK` | `KeyboardManager.h:141` vs `KeyboardManager.cpp:141-142` | doc (K-8) |
| 11 | Emulator launcher shows 20 cards; 12 of them do nothing on ENTER (stdout-only refusal) | `MainMenu.cpp:88-113` compiled fully; `NativeHal.cpp:932-996` handles ids {0,1,4,5,6,7,10,100} | medium |
| 12 | Goldens institutionalize the clock lie: all 18 masks are the same clock rect `4,6,37,13` — chrome is exempted from pixel-gating instead of made deterministic | `tests/emulator/masks/*` (18 files, single rect) | test-debt |
| 13 | SettingsApp title/hint promise ("MODE Back") is the only key documented; SETUP/AC undocumented behaviors | `SettingsApp.cpp:179`; SET §F–§G | low |

---

## E. Desired behavior per target [P]

### E.1 Firmware (production `esp32s3_n16r8`)

Angle badge per C.1; battery hidden per C.2 until HW-4 hardware lands; time slot renders uptime per PD-B2; modifier per C.4 with RT-14 reset-on-menu; title per C.5. Launcher bar and any surviving legacy header rebind to the same sources (SB-01) or are deleted with their dead render paths.

### E.2 Emulator interactive (SDL window)

Same code; clock slot may show host wall time (interactive-only concession — a human at a desktop knows what their clock is); battery absent unless `--battery N` given, then icon shows N with a small `SIM` affix in the tooltip-less world: PD-B4 — the affix is the icon style itself (hollow outline variant), not text, to fit 24 px.

### E.3 Emulator headless/deterministic (`--headless --deterministic`)

Clock string derives from the synthetic tick (`g_detTick`, `NativeHal.cpp:189-190,1998`) — e.g. fixed `00:00` at frame 0 — making chrome pixels a pure function of the frame index. New goldens then need **no clock mask**; existing 18 masks stay until their goldens are re-blessed (SB-03). `assert_statusbar_*` hooks read label text, not pixels (SCQA §C).

### E.4 Validation builds (`_validate*`)

Card id 20 exists (`NUMOS_MATH_VISUAL_VERIFY`, `MainMenu.cpp:110-112`) — launcher goldens differ from production by design; validation-build screenshots must never be compared against production goldens (existing ground-truth rule, restated here because chrome is exactly where it bites).

---

## F. StatusBar rendering contract [P]

1. **No tofu.** All bar text renders in `lv_font_montserrat_12` (`StatusBar.cpp:63,70,77,84`), which covers ASCII + the used symbols; any new glyph (charging icon, ↑ uptime marker) must be verified present in the font or drawn as vector (the Phase 7I space-glyph incident, `SettingsApp.cpp:156-160`, is the precedent).
2. **No clipping/overlap.** Fixed x-budget at 320 px: clock ≤44 px from x=6; title centered with **max width 120 px** and `LV_LABEL_LONG_DOT` ellipsis; modifier right block reserves 44 px ending at −80; angle 3-char block at −38; battery 22 px at −8. Today nothing enforces this (§B.4); SB-05 adds the constraints. Worst legal title today is "Math Visual" (11 chars ≈ 77 px in Montserrat-12) — inside budget; the policy is for future names.
3. **App-title length policy.** Canonical titles = launcher card names (`MainMenu.cpp:88-113`), all ≤12 chars; new apps must fit 120 px or define an explicit short title.
4. **Fixed volatile region.** Exactly one rectangle may vary independent of user input: the time slot, fixed at `4,6,37,13` (the historical mask rect — kept as the *declared* volatile region so old masks remain valid and new deterministic goldens can drop them).
5. **Small-screen fallback.** 320×240 is the only target (`Config.h:47-49`); no responsive behavior is specified. If a future panel is narrower, drop order is: battery, clock, modifier — angle and title are last to go (angle is a correctness indicator, not decoration).

---

## G. Failure behavior [P]

| Condition | Behavior |
|---|---|
| Missing battery driver (today, always) | icon absent (C.2); `setBatteryLevel()` without a real source is a no-op in firmware builds — compile-time capability flag, not runtime guess |
| Invalid angle state (corrupt persisted token) | GSTATE §G quarantine → default DEG; badge shows the default; **never** an empty/garbage badge — the badge renders the enum the evaluator will actually use, so it cannot desynchronize |
| Corrupted settings record | bar unaffected except badge shows post-recovery default (SET §H.2) |
| Null/unset app title | render the mode's canonical name from the launcher table; if truly unknown, `"NumOS"` (legacy default, `SystemApp.cpp:353-355`) — never an empty center slot |
| `update()` before `create()` / after `destroy()` | already safe: every helper null-guards (`StatusBar.cpp:194,206,232,244,255`) [V — keep under test] |
| KeyboardManager unavailable | impossible by construction (header-only singleton, `KeyboardManager.cpp` instance) — no policy needed |

---

## H. Implementation tickets (headline; full fields in SCROAD)

- **SB-01 — Truthful angle badge everywhere**: badge reads GS-01 global (already does); retire launcher `"rad"` literal and legacy header `"DEG"`; repaint-on-change.
- **SB-02 — Truthful battery policy**: hide-until-real on firmware; delete the nine `setBatteryLevel(100)` sites; `--battery` mock for emulator goldens (HW-4/P-13 alignment).
- **SB-03 — Deterministic emulator clock**: synthetic-tick clock in `--deterministic`; uptime rendering on firmware (PD-B2); path to unmasked goldens.
- **SB-04 — SHIFT/ALPHA indicator truth**: shared repaint-after-key mechanism (C.7); firmware modifier reset on returnToMenu (with RT-14, not duplicating it); Settings stale-bar fix.
- **SB-05 — Rendering contract enforcement**: title width/ellipsis, x-budget constants, volatile-region declaration (§F).
- **SB-06 — One StatusBar**: launcher bar becomes a `ui::StatusBar` (or a read-only variant of it); dimmed non-launchable cards in emulator (PD-B3); legacy `drawStatusBar` deletion audit.

Open questions: **OQ-B1** — keep a wall clock at all on firmware vs uptime-only (PD-B2 leans uptime; reverses trivially if RTC/NTP arrives). **OQ-B2** — launcher bar variant: full StatusBar (clock+modifier on menu too) vs minimal (angle+title+battery); leaning full, for one code path and C.4(c) visibility.

*End of StatusBar truthfulness & system chrome spec.*
