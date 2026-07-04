# NumOS — System Chrome Implementation Roadmap

> Ordered, fully-fielded tickets implementing `NUMOS_SETTINGS_APP_PRODUCT_SPEC.md` (SET), `NUMOS_STATUSBAR_TRUTHFULNESS_AND_SYSTEM_CHROME_SPEC.md` (SBAR), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE), `NUMOS_SYSTEM_CHROME_QA_ORACLE_AND_GOLDEN_PLAN.md` (SCQA).
>
> Audit base: `5fc5ea3` (2026-07-04). Owner tiers: **Sonnet** = mechanical/bounded · **Opus** = multi-file feature with regression risk · **Fable** = cross-cutting semantics · **Human** = judgment, hardware, golden blessing.

## 0. Namespace & collision note

- **GS-\*** (global settings state), **ST-\*** (SettingsApp), **SB-\*** (StatusBar) are new namespaces introduced by this pack.
- **QA-\*** below is the *system-chrome QA series* mandated by this pack's charter. It numerically collides with the core-apps roadmap's QA-01…QA-08; when both packs meet in one tracker, cite this series as **QA(SC)-nn** and the core series as **core-QA-nn**. No scope overlaps: shared needs are explicitly delegated (failure artifacts → core-QA-05; `set_angle_mode` hook is implemented once, here, satisfying GHOOK §D.4).
- Relationship to existing tickets: **GS-01 subsumes UX-01/CA-06** (same change, now with a full contract — the roadmap that schedules UX-01 in its Wave 2 should execute GS-01's field list). **SB-04 depends on RT-14** (modifier reset) but does not re-implement it. **GS-02 coordinates with RT-13** (storage discipline) and **CA-05** (vars format v2) without touching `/vars.dat`. **SB-02 executes P-13** (risk HW-4 mitigation, software half).

## 1. Recommended order

```
Wave A (unblockers):      QA-01 → GS-01 → ST-01 → SB-01 → QA-02
Wave B (truth pass):      SB-04 → SB-03 → SB-02 → ST-03 → QA-03 (bless wave 1)
Wave C (persistence):     GS-02 → ST-04 → QA-04 → QA-05 → GS-03
Wave D (consolidation):   ST-02 → SB-05 → SB-06 → GS-04 → QA-06 → QA-07 (bless wave 2)
```

Rationale: hooks first (QA-01) so GS-01 lands with its oracle in the same PR window; the angle-mode chain (GS-01→ST-01→SB-01→QA-02) is the product-critical path; pixel-visible bar changes are batched into two golden re-bless waves (B and D) because any bar change invalidates all 18 goldens at once (SCQA §D blessing rule).

---

## 2. GS-* — Global settings state

### GS-01 — Single source of truth for angle mode
- **Motivation**: angle mode is frozen and self-contradictory: three stores, two enums with inverted values, zero writers (GSTATE §A.1; SET §E.2; risk COR-3; conflict K-1). Subsumes UX-01/CA-06.
- **Evidence**: `MathEvaluator.cpp:53` (never assigned), `MathEvaluator.h:70-73` vs `Evaluator.h:32-35` (inverted enums), `SystemApp.cpp:79,96-97,372`, `Evaluator.cpp:25`, `GraphModel.h:172` (no `setAngleMode` call), `StatusBar.cpp:243-248`.
- **Files**: `src/math/MathEvaluator.cpp` (default → DEG per PD-S3), new `src/system/AngleSync.h` (`legacyAngleModeFromVpam()` by-name map), `src/apps/GraphModel.cpp/.h` (`syncAngleModeFromSystem()` at `preCacheRPN`/`replot` — GR-02's shared half), `src/math/EquationSolver.cpp` (sync at solve entry), `src/SystemApp.cpp` (delete `_angleMode` init/pushes; leave member deletion to GS-04 if `drawStatusBar` still reads it — SB-01 re-points that read first).
- **Desired behavior**: SET §E.3 per-surface contract; `vpam::g_angleMode` sole store; by-name sync only.
- **Non-goals**: Settings UI row (ST-01); persistence (GS-02); special-angle exactness (CAS D.11).
- **Tests**: SCQA §E.1 scripts; host unit `angle_mode_map_test.cpp`; static cast-guard (SCQA §G.4).
- **Golden impact**: none by itself if the default ships RAD; **with PD-S3 (DEG default) all 18 goldens' badge changes** → land inside bless-wave B, or ship RAD-default first and flip with SB-01 (executor's choice, human ratifies default at review).
- **Firmware impact**: behavior change — legacy engines (EquationSolver path) move DEG→global default; VPAM trig follows the default. Giac path unaffected (its angle handling is out of scope).
- **Rollback**: revert the sync call sites; the global's default is one constant.
- **Risk**: medium (semantics of every trig result). **Owner**: Fable; default-value ratification Human.

### GS-02 — Settings persistence schema (SettingsState)
- **Motivation**: settings are process-lifetime; the trio is defined twice with comment-enforced equality (GSTATE §A.2, §B.1; fault-containment C6.7).
- **Evidence**: `Config.h:77-79`, `main.cpp:31-33`, `NativeHal.cpp:117-127`; no save/load path (grep).
- **Files**: new `src/system/SettingsState.h/.cpp`; `main.cpp` + `NativeHal.cpp` (retire dual definitions → SettingsState-owned); `src/SystemApp.cpp` (load-at-boot before app use); `NativeHal.cpp` (`--persist-settings`, `/settings.dat` via `hal/FileSystem`); firmware NVS backend per PD-GS2 (`Preferences`, namespace `numos_set`).
- **Desired behavior**: GSTATE §B–§C, §G (v1 record, CRC+version, whole-record quarantine, `[FS] settings=*` diag lines, debounced commit per SET §G.4).
- **Non-goals**: `/vars.dat` changes (CA-05/RT-13); observer/callback framework (PD-GS3); safe-mode entry logic (RT-03).
- **Tests**: QA-04 (corruption), QA-05 (roundtrip), `assert_setting_value` scripts; `[FS]` line greps.
- **Golden impact**: none (no pixels).
- **Firmware impact**: +NVS namespace; flash wear bounded by debounce; boot adds one NVS read.
- **Rollback**: SettingsState falls back to compile-time defaults if the load call is stubbed; delete the namespace.
- **Risk**: medium (boot path + storage). **Owner**: Opus; backend choice (OQ-S4) Human.

### GS-03 — Emulator settings determinism & FS sandbox rule
- **Motivation**: deterministic runs must be a function of script+binary; `emulator_data/vars.dat` is git-tracked, boot-loaded, and STO-mutated — the existing hidden-persistence flake settings must not join (GSTATE §F).
- **Evidence**: `git ls-files emulator_data` → `vars.dat`; `NativeHal.cpp:2171-2179`; `CalculationApp.cpp:900`.
- **Files**: `NativeHal.cpp` (`--fs-sandbox <dir>` copy-on-boot or equivalent), `.github/workflows/emulator-build.yml` (`git diff --exit-code emulator_data/` step), `docs/emulator-sdl2-quickstart.md` note.
- **Desired behavior**: GSTATE §F.1/F.3; default runs never read or write persisted settings; committed `emulator_data/` treated as read-only fixture in CI.
- **Non-goals**: changing VariableManager semantics.
- **Tests**: SCQA §E.4 self-diff + tree-clean guard.
- **Golden impact**: none. **Firmware impact**: none.
- **Rollback**: remove the CI step/flag. **Risk**: low. **Owner**: Sonnet.

### GS-04 — Legacy divergent-state retirement
- **Motivation**: dead/divergent copies invite re-divergence (GSTATE §A.3-A.4).
- **Evidence**: `SystemApp.h:171-173` (`_shiftActive/_alphaActive/_angleMode`), `SystemApp.cpp:484-493` (dead toggles), `SystemApp.cpp:175` (misleading "hidden" comment), `SystemApp.cpp:353-375` (legacy header — deletion audit with SB-06).
- **Files**: `src/SystemApp.h/.cpp`.
- **Desired behavior**: members deleted; reads re-pointed to `KeyboardManager`/SettingsState; comment corrected.
- **Non-goals**: RT-14 itself (reset-on-menu lands with SB-04/RT-14).
- **Tests**: full emulator suite green (behavior-neutral change); grep guards.
- **Golden impact**: none. **Firmware impact**: none functional.
- **Rollback**: revert. **Risk**: low. **Owner**: Sonnet (after SB-01/SB-04 merge).

## 3. ST-* — SettingsApp

### ST-01 — Settings angle-mode row
- **Motivation**: no UI can change angle mode (SET §C.2, §E.3.4).
- **Evidence**: `SettingsApp.cpp:132-136` (3 rows, no angle), `SettingsApp.h:47`.
- **Files**: `src/apps/SettingsApp.h/.cpp` (NUM_ITEMS→4, row 0 = "Angle mode", `Degrees|Radians` value, LEFT/RIGHT/ENTER cycle per SET §G.2; `_statusBar.update()` after mutation per SET §G.6).
- **Desired behavior**: SET §E.3.4; badge repaints same frame; writes via SettingsState.
- **Non-goals**: SETUP key (ST-02); persistence (GS-02 — row works session-only until then).
- **Tests**: `settings_toggle_angle.numos`, `angle_badge_equals_engine.numos` (SCQA §E.1).
- **Golden impact**: `settings_smoke` invalidated (row count changes) → re-bless as `settings_initial` + `settings_angle_deg/rad` (bless-wave B).
- **Firmware impact**: UI only. **Rollback**: revert (rows are data-driven-ish; small diff).
- **Risk**: low-medium (golden churn). **Owner**: Sonnet; goldens Human. **Depends**: GS-01, QA-01.

### ST-02 — SETUP key disposition
- **Motivation**: dead keycode on the physical top row; product decision PD-S4 = open Settings from anywhere (SET §F; resolves OQ-U3).
- **Evidence**: `KeyCodes.h:41`; `Keyboard.cpp:53`; zero handlers (grep); no serial char (`SerialBridge.cpp:142-224`); no script name (grep `NativeHal.cpp`).
- **Files**: `src/SystemApp.cpp` (global intercept before per-mode switch), `src/hal/NativeHal.cpp` (dispatchKey intercept + `key setup` name — append-only), `src/input/SerialBridge.cpp` (char `q`), `docs/KEYBOARD_LAYOUT.md`.
- **Desired behavior**: SET §F.2 (idempotent open; MODE-from-Settings returns to launcher; PD-S7/S8).
- **Non-goals**: quick-settings popover (PD-S9); return-to-previous-app (OQ-S3b).
- **Tests**: `setup_opens_settings.numos` (from Menu/Calc/Grapher → `assert_app Settings`); idempotence script; negative control.
- **Golden impact**: none. **Firmware impact**: new global key intercept — verify it can't shadow app keys (SETUP is unused by all apps, grep-verified).
- **Rollback**: delete the intercepts. **Risk**: low. **Owner**: Sonnet.

### ST-03 — Row-efficacy honesty pass (placebo resolution)
- **Motivation**: "Decimal precision" changes nothing anywhere; "Complex numbers" changes nothing in the emulator (SET §C.1, PD-S1).
- **Evidence**: grep — `setting_decimal_precision` read only in `SettingsApp.cpp:223,250,299`; `setting_complex_enabled` sole consumer `SingleSolver.cpp:330`, absent from `platformio.ini:195-266` whitelist.
- **Files**: `src/apps/SettingsApp.cpp` (remove precision row per OQ-S1(b), or wire per (a) if Human chooses; disabled-style "(firmware only)" for complex row on emulator via SettingsState capability bit — GSTATE §F.4), `src/system/SettingsState.*`.
- **Desired behavior**: every visible row passes PD-S1 on its build target.
- **Non-goals**: implementing a precision consumer (separate CALC-owned ticket if OQ-S1(a)).
- **Tests**: `settings_precision_cycle.numos` retired-or-kept per decision; golden re-bless.
- **Golden impact**: `settings_initial` re-bless (row set changes) — batch with ST-01 in bless-wave B.
- **Firmware impact**: UI only. **Rollback**: revert. **Risk**: low. **Owner**: Sonnet; OQ-S1 decision Human.

### ST-04 — System-info + Reset rows, persistence UX
- **Motivation**: SET §D.2 (info/reset), §G.4 (commit/confirm), §H (recovery surfacing).
- **Evidence**: no reset path exists anywhere (fault-containment C6.8).
- **Files**: `src/apps/SettingsApp.*`, `src/system/SettingsState.*` (resetToDefaults), diag console hook (`!settings reset CONFIRM`, extends RT-04 grammar when that lands).
- **Desired behavior**: GSTATE §B.4; two-step confirm; failure toast per PD-S11.
- **Non-goals**: variables reset (RT-13's `!vars reset`); DiagnosticsApp (RT-15).
- **Tests**: `settings_reset_row.numos`; persistence-failure path via sandbox read-only trick (emulator).
- **Golden impact**: `settings_initial` re-bless (bless-wave D). **Firmware impact**: UI + one NVS write path.
- **Rollback**: hide rows. **Risk**: low. **Owner**: Sonnet. **Depends**: GS-02.

## 4. SB-* — StatusBar

### SB-01 — Truthful angle badge on every chrome surface
- **Motivation**: launcher hard-codes `"rad"`; legacy header shows frozen `"DEG"`; app badge is truthful only by accident (SBAR §C.1, §D.1-2).
- **Evidence**: `MainMenu.cpp:333-338`; `SystemApp.cpp:372`; `StatusBar.cpp:243-248`.
- **Files**: `src/ui/MainMenu.cpp` (badge reads `vpam::g_angleMode`; store the label pointer; refresh in `load()`), `src/SystemApp.cpp` (`drawStatusBar` reads the global or is deleted per SB-06 audit), `src/ui/StatusBar.cpp` (no change expected — verify).
- **Desired behavior**: one truth on all three surfaces; badge repaints on mode change (Settings same-frame per ST-01; other apps at their update-policy points).
- **Non-goals**: unifying the bar implementations (SB-06).
- **Tests**: `assert_statusbar_angle` across screens (`angle_badge_equals_engine.numos`).
- **Golden impact**: launcher goldens (`launcher_smoke`, `menu_focus_grapher_smoke`) re-bless if the literal case changes ("rad"→"RAD"/"DEG"); app goldens re-bless if the default flips (GS-01 note) — bless-wave B.
- **Firmware impact**: none beyond pixels. **Rollback**: revert. **Risk**: low-medium (golden coordination). **Owner**: Sonnet; blessing Human. **Depends**: GS-01.

### SB-02 — Truthful battery policy
- **Motivation**: full/green battery with no measurement is the flagship chrome lie (SBAR §C.2, §D.3-5; risk HW-4; executes P-13 software half).
- **Evidence**: `StatusBar.h:102` default 100; nine `setBatteryLevel(100)` sites (SBAR §B.5 list); `MainMenu.cpp:350-355`; `SystemApp.cpp:366-368`; no ADC (grep).
- **Files**: `src/ui/StatusBar.cpp/.h` (capability-gated icon; hidden by default on firmware), the nine caller files (delete the constant calls), `src/ui/MainMenu.cpp` (glyph removal/gating), `src/hal/NativeHal.cpp` (`--battery N` mock flag), `src/SystemApp.cpp` (legacy rects).
- **Desired behavior**: SBAR §C.2 + §E.2 (hide-until-real; explicit mock in emulator, hollow-outline mock style PD-B4).
- **Non-goals**: ADC/fuel-gauge bring-up (hardware, HW-4 proper); charging detection.
- **Tests**: `statusbar_no_battery` + `statusbar_battery_mock_20` goldens; negative control.
- **Golden impact**: **all 18 goldens** (icon disappears) — anchor of bless-wave B.
- **Firmware impact**: pixels only. **Rollback**: `-DNUMOS_BATTERY_ICON_LEGACY=1` compile flag restoring the old constant icon (kept one cycle).
- **Risk**: low logic / high golden-churn. **Owner**: Sonnet; blessing Human.

### SB-03 — Deterministic emulator clock policy
- **Motivation**: clock reads wall time even under `--deterministic` (synthetic tick covers LVGL only), forcing the identical `4,6,37,13` mask in all 18 masks; firmware clock is a format-lie (boots 00:00-of-2026, key-press refresh only) (SBAR §C.3, §D.6-7, §E.3; PD-B2).
- **Evidence**: `StatusBar.cpp:205-225,125-142`; `NativeHal.cpp:189-190,1998`; `tests/emulator/masks/*` single-rect uniformity; false comment `StatusBar.cpp:20-23`.
- **Files**: `src/ui/StatusBar.cpp` (time-source seam: deterministic source when NATIVE_SIM+deterministic; uptime rendering on firmware per PD-B2; fix the comment), `src/hal/NativeHal.cpp` (expose det-tick to the seam).
- **Desired behavior**: SBAR §E.3 (chrome pixels = f(frame index)); §C.3 firmware honesty; new goldens maskless; old masks valid until re-bless.
- **Non-goals**: RTC/NTP; clock-set UI (OQ-S2).
- **Tests**: `assert_statusbar_clock` (post-seam); 2000-frame idle self-diff without mask (SCQA §F); existing goldens must still pass with their masks pre-re-bless.
- **Golden impact**: none forced immediately; enables maskless future goldens; firmware uptime rendering changes the slot's *shape* → include in bless-wave B if PD-B2 ships then.
- **Firmware impact**: uptime vs fake-wall-clock display change. **Rollback**: seam defaults back to `time()`.
- **Risk**: low-medium. **Owner**: Opus (touches golden determinism machinery); PD-B2 ratification Human.

### SB-04 — SHIFT/ALPHA indicator truth
- **Motivation**: indicator staleness is per-app luck (Settings never repaints; Equations does); firmware keeps locks across app exit while the emulator resets — CI-invisible divergence (SBAR §C.4, §D.8-9; C3.9).
- **Evidence**: `SettingsApp.cpp:110`; `EquationsApp.cpp:1790-1791`; `SystemApp.cpp:860-883` vs `NativeHal.cpp:1081`; `KeyboardManager.cpp:136-146`; doc drift `KeyboardManager.h:141`.
- **Files**: `src/SystemApp.cpp` (repaint-after-key shared mechanism per SBAR §C.7(i); modifier reset in `returnToMenu` — **this is RT-14; if RT-14 has landed, only verify**), `src/apps/SettingsApp.cpp` (`handleKey` tail update), per-app audit of SHIFT/ALPHA paths, `KeyboardManager.h` comment fix (with UX-02).
- **Desired behavior**: indicator equals FSM on every bar-bearing screen at all times; no lock survives returnToMenu on either target.
- **Non-goals**: modifier FSM semantics changes (UX-02 owns); launcher indicator slot (SB-06/OQ-B2).
- **Tests**: `assert_shift_state`/`assert_alpha_state` scripts (BC-08/BC-09 pattern); `statusbar_shift`/`statusbar_alpha_lock` goldens; `home_mode_transitions.numos`.
- **Golden impact**: two new goldens; existing unaffected (indicator is empty in all current goldens).
- **Firmware impact**: behavior change (lock no longer survives exit) — matches emulator, closes divergence.
- **Rollback**: revert reset call. **Risk**: low. **Owner**: Sonnet. **Depends**: coordinates with UX-02/RT-14.

### SB-05 — Rendering contract enforcement
- **Motivation**: nothing bounds title width or declares the volatile region (SBAR §F).
- **Evidence**: `StatusBar.cpp:68-79` (unbounded centered title next to `-80` mod label).
- **Files**: `src/ui/StatusBar.cpp/.h` (120 px title max + `LV_LABEL_LONG_DOT`, x-budget constants, volatile-region constant `4,6,37,13`).
- **Desired behavior**: SBAR §F.1-F.4. **Non-goals**: responsive layouts (§F.5 records drop order only).
- **Tests**: `assert_statusbar_title` truncation script (long synthetic title via debug setter); goldens unchanged for current names.
- **Golden impact**: none (current titles fit). **Firmware impact**: none. **Rollback**: revert. **Risk**: low. **Owner**: Sonnet.

### SB-06 — One StatusBar (chrome consolidation)
- **Motivation**: three implementations is why lies multiply (SBAR §B.1, §D.11; PD-B3 dimmed cards).
- **Evidence**: `MainMenu.cpp:321-356`; `SystemApp.cpp:353-375`; `NativeHal.cpp:932-996` (silent non-launchable cards).
- **Files**: `src/ui/MainMenu.cpp` (adopt `ui::StatusBar` per OQ-B2 resolution; dimmed style for emulator-non-launchable cards keyed off the NativeHal-supported id set), `src/SystemApp.cpp` (delete `drawStatusBar`+legacy render paths if the APP_TABLE placeholder is retired — audit), `src/hal/NativeHal.cpp` (expose launchable-id predicate).
- **Desired behavior**: one bar implementation on all screens; emulator launcher visually honest about launchability.
- **Non-goals**: menu redesign; app additions to the emulator whitelist (P-05).
- **Tests**: `launcher_dimmed_cards` golden; `menu_nav_parity.numos` must stay green (focus model untouched).
- **Golden impact**: launcher goldens re-bless (bless-wave D).
- **Firmware impact**: launcher gains clock/modifier if OQ-B2 chooses full bar. **Rollback**: keep old builders behind a flag one cycle.
- **Risk**: medium (touches the launcher everyone boots into). **Owner**: Opus; OQ-B2 + blessing Human.

## 5. QA-* — tests & oracles (system-chrome series; see §0)

### QA-01 — Settings/chrome assert hooks
- **Motivation**: zero chrome oracles exist (SCQA §B.1-8).
- **Evidence**: grammar ends at `AssertMenuFocus` (`NativeHal.cpp:1345-1362`); no `set_angle_mode`/`assert_setting*`/`assert_shift*`/`assert_statusbar*` anywhere in `tests/emulator/scripts/` (grep).
- **Files**: `src/hal/NativeHal.cpp` (append-only grammar per SCQA §C), NATIVE_SIM debug accessors in `src/ui/StatusBar.h`, `src/system/SettingsState.h`, `src/input/KeyboardManager.h`.
- **Desired behavior**: SCQA §C table, incl. reusing GHOOK's reserved `set_angle_mode` (one implementation) and the existing `assert_menu_focus` (no duplicate launcher-focus command).
- **Non-goals**: pixel/OCR asserts; Grapher-specific `assert_graph_*` (GR-14).
- **Tests**: negative control per new command (exit 4).
- **Golden impact**: none. **Firmware impact**: none (NATIVE_SIM-only). **Rollback**: commands are additive. **Risk**: low. **Owner**: Sonnet.

### QA-02 — Angle-mode semantic scripts
- **Motivation**: K-1 must be un-shippable once fixed (SCQA §E.1).
- **Evidence**: no angle assertion in any of the 71 scripts (grep).
- **Files**: `tests/emulator/scripts/angle_*.numos`, `settings_toggle_angle.numos`; `emulator-build.yml` "Chrome semantics" gate step (pattern of `:441-457`).
- **Desired behavior**: SCQA §E.1 six-script set, CI-gated.
- **Non-goals**: Grapher table oracles (needs GR-14).
- **Tests**: are the tests. **Golden impact**: none. **Firmware impact**: none.
- **Rollback**: remove gate step. **Risk**: low. **Owner**: Sonnet. **Depends**: QA-01, GS-01, ST-01.

### QA-03 — StatusBar & settings goldens (bless-wave management)
- **Motivation**: chrome pixels must be gated, not masked away (SCQA §A.1, §D).
- **Evidence**: 18/18 masks are the clock rect; no chrome-state golden exists.
- **Files**: `tests/emulator/golden/*`, `masks/*`, `scripts/generate-emulator-candidates.py` (new stems), scripts for §D captures.
- **Desired behavior**: SCQA §D table; two coordinated bless waves aligned to SB landings; wrong-golden negative control retained.
- **Non-goals**: auto-promotion (forbidden — human-only, `promote-emulator-golden.py`).
- **Tests**: golden compare gates. **Golden impact**: is the ticket. **Firmware impact**: none.
- **Rollback**: goldens are versioned. **Risk**: low mechanics / medium coordination. **Owner**: Sonnet prep, **Human blessing mandatory**.

### QA-04 — Settings corruption tests
- **Motivation**: GSTATE §G ships untested otherwise (SCQA §B.4).
- **Evidence**: n/a (feature is new with GS-02).
- **Files**: CI runner step (garbage `/settings.dat` into sandbox), `determinism`-family scripts, stdout `[FS]` greps.
- **Desired behavior**: SCQA §F corrupted-record row: boot→defaults→quarantine line→no crash; higher-version record treated as corrupt (GSTATE §B.5).
- **Non-goals**: vars corruption (RT-13's I-E tests).
- **Tests**: are the tests. **Golden impact**: none. **Firmware impact**: none (manual/HIL rows for NVS in SCQA §H.4).
- **Rollback**: remove step. **Risk**: low. **Owner**: Sonnet. **Depends**: GS-02, GS-03.

### QA-05 — Persistence roundtrip tests
- **Motivation**: "survives restart" is the whole point of GS-02 and needs a two-process oracle (SCQA §B.5, §E.3).
- **Evidence**: one-process-per-script runner (`NativeHal.cpp` main loop; candidate generator).
- **Files**: CI runner step (three-run sandbox protocol A/B/C per SCQA §E.3), `emulator-build.yml`.
- **Desired behavior**: value persists opt-in, absent by default; tree-clean guard co-lands (GS-03).
- **Non-goals**: firmware power-cycle (manual SCQA §H.3).
- **Tests**: are the tests. **Golden impact**: none. **Firmware impact**: none.
- **Rollback**: remove step. **Risk**: low. **Owner**: Sonnet. **Depends**: GS-02, GS-03, QA-01.

### QA-06 — Chrome stress & idle-determinism suite
- **Motivation**: teardown loops, key-mash domain safety, and the maskless idle gate (SCQA §F).
- **Files**: `tests/emulator/scripts/settings_enter_exit_x10.numos`, `settings_keymash.numos`, `home_mode_transitions.numos`, idle self-diff step.
- **Desired behavior**: SCQA §F; idle 2000 frames byte-identical without clock mask (post SB-03).
- **Non-goals**: core-apps stress (core-QA-08).
- **Tests/Golden/Firmware**: none beyond the scripts. **Rollback**: remove. **Risk**: low. **Owner**: Sonnet. **Depends**: SB-03 for the maskless gate; rest immediately after QA-01.

### QA-07 — Chrome static guards
- **Motivation**: the enum-cast and rogue-writer bugs are grep-preventable forever (GSTATE §C.3, §D.2; SCQA §G.4).
- **Evidence**: precedent `scripts/check-keycode-digit-patterns.py` + `emulator-build.yml:96-104`.
- **Files**: new `scripts/check-chrome-invariants.py` (cross-namespace `AngleMode` cast; direct writes to `g_angleMode`/`setting_*` outside SettingsState; `emulator_data` tree-clean), `emulator-build.yml`, host unit `tests/host/angle_mode_map_test.cpp`.
- **Desired behavior**: guards gate like the keycode guard.
- **Non-goals**: general lint. **Golden/Firmware impact**: none. **Rollback**: remove step. **Risk**: low. **Owner**: Sonnet.

## 6. Cross-reference to required-items checklist

| Required item | Ticket(s) |
|---|---|
| Single source of truth for angle mode | GS-01 (+SB-01 badge, ST-01 UI, QA-02 oracle) |
| Settings persistence schema | GS-02 (+QA-04/QA-05) |
| Settings angle-mode UI | ST-01 |
| SETUP key disposition | ST-02 (PD-S4) |
| Truthful angle badge | SB-01 |
| Truthful battery policy | SB-02 (HW-4/P-13) |
| Deterministic emulator clock | SB-03 |
| SHIFT/ALPHA indicator truth | SB-04 (+RT-14) |
| Settings assert hooks | QA-01 |
| Angle-mode semantic scripts | QA-02 |
| StatusBar goldens | QA-03 |
| Settings corruption tests | QA-04 |
| Persistence roundtrip tests | QA-05 |
| Shared UX behavior (SETUP/AC/MODE in Settings, honesty rows) | ST-02, ST-03, SET §G rulings (PD-S10) |

*End of system chrome implementation roadmap.*
