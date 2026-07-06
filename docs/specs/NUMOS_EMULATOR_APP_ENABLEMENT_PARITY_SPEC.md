# NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC

Status: SPEC-ONLY (P-05 wave). Nothing here is implemented.
Companions: `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md`, `NUMOS_EMULATOR_NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC.md`, `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md`, `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md`.

> **Branch context (read first).** The 16 prerequisite specs (determinism, fixture hygiene, persistence sandbox, CI/golden policy, portability, settings/chrome/QA oracles) exist on remote branch `origin/claude/numos-emulator-determinism-specs-69nso8`, **not on `main`** — `docs/specs/` does not exist on `main` at the audit base (`8662f47`). This spec set is authored against that branch's decisions and ticket namespaces (`EMUDET-*`, `FIX-*`, `CI-*`, `PORT-*`, `QA-*`, `GR-*`, `MR-*`, `CAS-*`, risk register `EMU-*`/`MEM-*`/…, problem inventory `P-01…P-15` where **P-05 = "enable Equations → Calculus → Matrices in emulator"**). Merging this set to `main` without that branch leaves dangling references; the integration order is a merge/rebase decision recorded as Open Decision OD-1.

Naming note: the parity **levels** P0–P7 defined in §E are maturity levels of one app, unrelated to the `P-01…P-15` problem-inventory IDs.

---

## A. Executive summary

**What emulator app parity means.** An app is "emulator-parity" when a `.numos` script can launch it, drive it with the same key semantics firmware users get, observe its state through read-only debug hooks, capture deterministic screenshots gated against human-blessed goldens, and enter/exit it repeatedly without leaks or hangs — with every intentional firmware/emulator difference written down as a shim, not discovered as a surprise.

**What it does not mean.** It does not mean pixel-identity with the physical TFT (different renderer output paths), Giac availability (Giac never builds natively — `platformio.ini:192-194`; the native baseline is "CAS unavailable via Giac"), real NVS/battery/deep-sleep, autorepeat timing, or emulating FreeRTOS. It also does not mean the emulator may grow features firmware lacks (see §I).

**Why enabling an app is more than adding a menu card.** The launcher already shows every card (`src/ui/MainMenu.cpp:88-113` renders all of `APPS[]` unfiltered); the card is the cheap part. Enabling means: (1) a whitelist source-closure in `platformio.ini` `build_src_filter` (`:195-266`) including every transitive TU, each audited for native safety (unguarded `heap_caps_*`, `<LittleFS.h>`, FreeRTOS); (2) NativeHal work — AppMode value, construction, `launchApp` case, dispatch case, teardown case, canonical script name (six touchpoints that historically get half-done; risk EMU-1 "dispatcher drift"); (3) determinism review — timers, `rand()`, wall-clock; (4) persistence review — every LittleFS store the app imports needs a sandbox story (FIX-01/FIX-02) and a parity-register row; (5) QA — scripts, hooks, candidates, goldens per the promotion policy. Grapher's enablement (Phase 8B–8G) needed 6 unguarded ESP-heap fixes, 4 filter files beyond the app itself, 10 NativeHal touchpoints, and an LVGL 9 allocator fix (`platformio.ini:170-176`) — that is the realistic unit of work.

**Why fixture hygiene and determinism are preconditions.** Golden gating assumes output is a pure function of (binary, script, flags). Today `--deterministic` virtualizes only the LVGL tick (`NativeHal.cpp:190, 1988, 2052-2054`); `time()`, `rand()`, and the filesystem are not virtualized, and `emulator_data/vars.dat` is written into the working tree by any STO (`CalculationApp.cpp:900` → `src/hal/FileSystem.cpp:38`). Five of the thirteen enablement candidates autosave to LittleFS and four call `rand()`. Enabling them before FIX-01/FIX-02 (sandbox) and EMUDET-01/-03 (clock seam, rand fence) land would convert every CI run into a dirty tree and every golden into a flake. Hence Wave 0 of the roadmap ships zero new apps.

## B. Current emulator app architecture (verified)

| Surface | Emulator | Firmware |
|---|---|---|
| Mode enum | `AppMode` 10 values: SPLASH, MENU, CALCULATION, SETTINGS, MATH_SHOWCASE, STATISTICS, PROBABILITY, SEQUENCES, REGRESSION, GRAPHER (`NativeHal.cpp:195-206`) | `Mode` 24 values (`SystemApp.h:76-103`) |
| App construction | `transitionToMenu()` news 7 apps + MainMenu (`NativeHal.cpp:867-908`); Calc eager-`begin()`, rest lazy | `SystemApp::begin()` news 20 apps (`SystemApp.cpp:104-126`), all lazy |
| Launch | script `open_app` → `scriptAppNameToId` (`NativeHal.cpp:1414-1428`) → `launchApp` (`:914-987`); card click → same `launchApp` via `MainMenu::setLaunchCallback` (`:899-902`) | card click → `SystemApp::launchApp`/`switchApp` ids 0–20 (`SystemApp.cpp:720-725, 868-896`) |
| Script dispatch | one command per frame, `scriptStepBegin` (`NativeHal.cpp:1687-1840`); exit codes 0/2/3/4 | none (SerialBridge single-char protocol is a different, incompatible surface — `src/input/SerialBridge.cpp:174-233`) |
| MainMenu cards | identical 21-entry `APPS[]` (`MainMenu.cpp:88-113`; id 20 gated by `NUMOS_MATH_VISUAL_VERIFY`) | identical |
| Teardown | deferred 260 ms via `lv_tick` (`NativeHal.cpp:230-233, 1055-1077, 2074-2078`) | deferred 250 ms via `millis` (`SystemApp.cpp:234, 843-866`) |
| Key dispatch | `dispatchKey` per-AppMode (`NativeHal.cpp:565-759`); forwards RELEASE; no global modifier interception | `SystemApp::handleKey` PRESS/REPEAT only (`SystemApp.cpp:451`); global SHIFT/ALPHA + per-app AC-exit/autosave (`:465-483, 569-651`) |

Full divergence table D1–D14 with citations: routing spec §C.

## C. Current enabled app inventory

Eight emulator-launchable surfaces. Detail and citations per app in the matrix doc (Group 1); condensed:

| App | id | Scripts | Goldens | Persistence | Parity gaps | Status |
|---|---|---|---|---|---|---|
| Calculation | 0 | 2 gated + 14 assert-only + extended | 2 + masks | writes `emulator_data/vars.dat` on STO | STO dirty-tree; RELEASE-forwarding (D4) | stable |
| Grapher | 1 | 6 gated + 5 warn + 6 no-hang + 27 unwired | 6 | none | async-POI paths ungated; libm drift (PORT-05) | stable on curated paths |
| Statistics | 4 | 2 gated | 2 | none | digit-entry bug blocks semantics | partial |
| Probability | 5 | 2 gated | 2 | none | same | partial |
| Regression | 6 | 2 gated | 2 | none | no semantic hooks | stable |
| Sequences | 7 | 2 gated | 2 | none | no semantic hooks | stable |
| Settings | 10 | 1 gated | 1 | `setting_*` globals, no store (`NativeHal.cpp:103-105`) | placebo rows (row-efficacy rule PD-S1) | stable |
| Math Showcase | 100 | 1 gated + 1 unwired | 1 | none | emulator-only by design | stable |

## D. Firmware-only / not-whitelisted inventory

Thirteen registered apps are dead cards in the emulator (selecting them prints `[APP] App %d no implementada en simulador`, `NativeHal.cpp:983-985`, and nothing happens): Equations(2), Calculus(3), Python(8), Matrices(9), Chemistry(11), Bridge(12), Circuit(13), Fluid 2D(14), ParticleLab(15), Neural Lab(16), OpticsLab(17), NeoLang(18), Fractals(19), plus validate-only Math Visual(20). Two orphans (`IntegralApp`, `TutorApp`) are registered nowhere. Full per-app dependency/timer/storage/font/memory/lifecycle detail with citations: matrix doc Groups 2–4. Aggregate risk surface they import:

- **LittleFS stores** (need FIX-01 sandbox + fixture + parity-register row): Python `/py/*`, Circuit `/circuits/*` + autosave-on-exit, Fluid2D `/fluid/*` + autosave, ParticleLab `/save.pt`, NeuralLab `/neural.nn`, NeoLang `/neolang.nl`.
- **`lv_timer` sim loops**: Bridge 16 ms, Circuit 16 ms, Fluid2D/ParticleLab/NeuralLab/OpticsLab 33 ms (citations in matrix).
- **`rand()`** (EMUDET-03 blocker class): Equations (`EquationsApp.cpp:106`), Calculus (`:449,453`), NeuralLab (`:372,380`), orphan Integral (`:334`).
- **ESP-heap/FreeRTOS**: Equations `heap_caps_get_free_size` diagnostics (`EquationsApp.cpp:487-516`); PSRAM `heap_caps_malloc(MALLOC_CAP_SPIRAM)` with `new[]` native fallbacks in Bridge/Fluid2D/ParticleLab/NeuralLab; Fractal `xTaskCreate` (`FractalApp.cpp:491-496`) — the one hard blocker.
- **Real hardware**: none — no WiFi/UART/I2C/ADC in any app; Circuit's "sensors" are simulated models (`CircuitCoreApp.cpp:1263-1286`).

## E. Parity definition — levels P0–P7

| Level | Criteria (all required) | Allowed gaps | Disallowed gaps | Required artifacts |
|---|---|---|---|---|
| **P0** card visible | card in `APPS[]`; selection refuses per routing spec §D.3 | everything else | crash/hang on select | — |
| **P1** opens/exits | `open_app` works; MODE returns to menu; one enter/exit clean under `--deterministic` | no scripts/goldens/hooks | LVGL error, teardown hang, dirty tree | filter+NativeHal wiring per ROUTE-01 checklist |
| **P2** deterministic smoke | `<app>_smoke.numos` passes twice with byte-identical screenshots on Linux reference | no semantic asserts beyond `assert_app` | any nondeterministic pixel outside the clock rect | script + candidate stem registered (gated/pending/assert-only per CI-05) |
| **P3** semantic asserts | ≥1 app-specific `debug*` hook + `assert_*` command exercising a real computation | visual gating | hooks that mutate state or force layout | hook + assert-only CI step |
| **P4** golden candidate | candidate generated in CI, warning-only, listed in `PENDING.md` with owner | golden not yet blessed | unexpected-stem (CI-05 violation) | CANDIDATES entry + PENDING row |
| **P5** blessed golden | human-promoted golden; masked only by the clock rect (`4,6,37,13`) or nothing | firmware-only visuals documented | broad/body masks; auto-promotion | golden + (optional) mask + promotion provenance |
| **P6** firmware-parity checklist | per-app checklist passed: key semantics vs firmware dispatcher (incl. D5 AC-exit/autosave), StatusBar title ≤120 px/no tofu, persistence format parity row, settings capability bits updated | documented shims (§I register) | undocumented behavioral divergence | signed checklist in the app's matrix section |
| **P7** stress/teardown | 10× enter/exit loop (LIFE-01), timer-active HOME (LIFE-02 where applicable), object-leak delta 0 | — | leak growth, UAF, hang | stress scripts in CI |

Levels are strictly ordered per app; skipping (e.g., blessing a golden with no semantic hook for a computation screen) is disallowed by the QA plan §G.

## F. Global prerequisites (before any new app reaches P2+)

1. **Fixture sandbox** — FIX-01 settable FS root, FIX-02 sandbox-by-default for scripted runs, FIX-03 untrack `emulator_data/vars.dat`; committed fixtures under `tests/emulator/fixtures/<name>/`.
2. **Dirty-tree guard** — CI-01: CI fails if a run modifies the tree.
3. **Deterministic clock** — EMUDET-01 (`time()` seam + `--epoch`); until it lands, every new golden inherits the clock mask; after it, new goldens carry no mask.
4. **Assert-failure screenshot capture** — CI-04/QA-05: exit-4 paths dump a failure PPM.
5. **Teardown loop support** — LIFE-01 harness (routing spec §G).
6. **NativeHal routing pattern** — ROUTE-01 checklist (routing spec §D), enforced as PR template.
7. **App state reset policy** — one script = one process; cross-run isolation via sandbox only (routing spec §D.1).
8. **Script naming convention** — `<app>_<facet>_smoke.numos` for smokes, `<app>_semantic_*` for assert suites (mirrors the `calc_semantic_*` CI glob, `emulator-build.yml:341`), stress scripts `<app>_relaunch_loop` / `<app>_breakit_*`.
9. **Golden/mask policy** — CI-05 lifecycle (gated / pending / assert-only), human-only promotion via `promote-emulator-golden.py`, clock-rect-only masks, coordinated re-bless waves.

## G. App categories

- **Low-risk UI-only**: Matrices, Chemistry (also Stats/Prob/Regr/Sequences/Settings, already enabled).
- **Math-renderer apps**: Calculation, Grapher (enabled); Equations, Calculus, Math Visual, Sequences.
- **Storage/persistence apps**: Python, Circuit, Fluid2D, ParticleLab, NeuralLab, NeoLang (+ Calculation's vars.dat).
- **Timer/animation apps**: Grapher (async POI), OpticsLab, Bridge, Circuit, Fluid2D, ParticleLab, NeuralLab.
- **Hardware-dependent**: Fractal (FreeRTOS task) — the only true member; PSRAM users all have native fallbacks.
- **Large-memory/simulation**: Fluid2D (11 grids), ParticleLab, Bridge, NeuralLab, Fractal.
- **CAS-dependent (in-tree cas::, not Giac)**: Equations (largest closure), Calculus, orphans Integral/Tutor; Calculation (already linked).
- **Educational/demo**: Math Showcase (emulator-only), Math Visual (validate-only), Chemistry.

## H. Recommended P-05 scope

**Included in P-05:**
- **Wave 0 (no new app)**: inventory freeze, ROUTE-01 checklist, LIFE-01 loop for the 8 already-enabled apps, STORE-01 persistence inventory, wiring the 27 orphaned Grapher/Calc/Showcase scripts. Minimum acceptance: all existing apps at P7-except-storage; zero unexpected candidate stems.
- **Wave 1 — Matrices, Chemistry** (low risk; no timers/storage/rand; acceptance P3 + P4, P5 after soak).
- **Wave 2 — Calculus, then Equations** (medium; CAS closure + heap-caps guards + rand-label rule; acceptance P3 with result hooks, goldens only on static screens). Math Visual optionally, pending OD-3.

**Deferred beyond P-05** (each with reason/risk/prereqs/minimum acceptance in the matrix): OpticsLab, Bridge, ParticleLab, Circuit, Fluid2D (P-06, timer policy + STORE gates); Python, NeoLang (storage + text input); NeuralLab (unseeded rand); Fractal (FreeRTOS — do not enable; needs its own mini-spec); Integral/Tutor (orphans — do not enable).

Rationale for the split: Wave 1 replays the proven Stats/Prob/Regression pattern to de-risk the checklist itself; Wave 2 is where P-05's stated goal (CAS apps) lands, after the cheap wave has validated tooling; everything requiring the determinism/persistence packs stays behind those packs' tickets rather than importing flakes.

## I. Firmware parity philosophy

- **Must match firmware**: key semantics per app (including dispatcher-level AC-exit/autosave, D5), app behavior and computation results, persistence file formats and load/save triggers (backend may differ), launcher layout and card order, teardown ordering contract, StatusBar ownership rules.
- **May legitimately differ (registered shims)**: tick source and `--deterministic`; script/assert surface (`open_app`, `assert_*`, CLI flags); FS backend (`emulator_data/` sandbox vs LittleFS-on-flash); Settings storage (`setting_*` globals vs NVS, until GS-02); Giac absence; Math Showcase existence; teardown 260 vs 250 ms; TEXTINPUT symbol entry (D12); menu GRAPH shortcut (D6 — flagged for review: it is a convenience divergence that a parity purist would remove; OD-2).
- **How shims are documented**: a single "Emulator Shim Register" — the D1–D14 table in the routing spec §C is the seed; every new shim gets a D-number, a citation, and a ruling (`keep`, `close`, `close-when-<ticket>`). PR review rejects divergences without a D-entry.
- **How to prevent the emulator becoming a different product**: (1) apps are enabled only by whitelisting unmodified firmware sources — behavioral `#ifndef ARDUINO` forks inside `src/apps/**` are forbidden; shims live in `src/hal/` (the NativeHal-only safe-edit rule); (2) emulator-only features must be unreachable from the simulated keypad (script/CLI only); (3) the per-app P6 checklist re-verifies key semantics against the firmware dispatcher lines on every enablement.

## J. Open decisions

| ID | Decision | Options / recommendation |
|---|---|---|
| OD-1 | Integration branch for this spec set vs `origin/claude/numos-emulator-determinism-specs-69nso8` | rebase this set onto that branch (recommended) or merge that branch first |
| OD-2 | Menu GRAPH shortcut (D6) | keep as registered shim vs remove for parity; recommendation: remove when Wave 1 lands (cheap, closes a divergence) |
| OD-3 | Math Visual: NativeHal Showcase proxy vs real app under native `NUMOS_MATH_VISUAL_VERIFY` variant | keep Showcase; enable app only if MathRenderer plan (MR-*) needs device-parity captures |
| OD-4 | Dimmed-card rendering for emulator-dead cards (SBAR PD-B3) vs visible-but-refusing | this spec chooses visible-but-refusing (routing §D.3) to keep menu goldens firmware-identical; SBAR's dimming proposal would fork them — needs one ruling |
| OD-5 | `rand()` flavor-text policy for Equations/Calculus | avoid capturing those screens (chosen here) vs EMUDET-03 `srand(1)` fence making them capturable |
| OD-6 | Sim-app float-drift goldens (Fluid2D/Circuit) | warning-only forever vs per-OS goldens; defer to PORT-05 measurements |
| OD-7 | Digit-entry firmware bug in Stats/Prob (blocks their P3) | fix in firmware first vs accept P5-visual-only status |

## K. Answers to the special questions

1. **Not before fixture sandboxing (FIX-01/02/03)**: Python, Circuit, Fluid2D, ParticleLab (save path), NeuralLab, NeoLang — every LittleFS-writing app; also Calculation's P6 (vars.dat).
2. **Enable before deterministic clock (EMUDET-01)**: Matrices, Chemistry (and optionally Calculus/Equations smoke-only) — their goldens just inherit the clock mask like today's 18; nothing else in them reads time.
3. **New filesystem writes**: Python `/py/*` (+seeded hello.py), Circuit `/circuits/*` + `autosave.dat`, Fluid2D `/fluid/*` + autosave, ParticleLab `/save.pt`, NeuralLab `/neural.nn`, NeoLang `/neolang.nl`.
4. **Timers/async LVGL callbacks**: Grapher (template loader + 10 ms POI), Bridge (16 ms), Circuit (16 ms), Fluid2D/ParticleLab/NeuralLab/OpticsLab (33 ms); Fractal uses a FreeRTOS task instead.
5. **Likely nondeterministic pixels**: NeuralLab (unseeded rand dataset), Fluid2D/Circuit (float accumulation over timer steps), Equations/Calculus solving screens (rand flavor text), Grapher POI-async panels, anything showing wall-clock.
6. **New fonts/glyphs**: none — all candidates use the already-linked stix_math_8/12/18 + montserrat (`platformio.ini:264-266`); the standing hazard is stix's missing U+0020 (tofu at spaces), so labels with spaces must use montserrat.
7. **Need MathRenderer v2 before useful goldens**: none strictly; but Equations/Calculus result-screen goldens should wait for the MR-* oracle stack (O-A…O-D before pixels) so renderer churn doesn't force re-bless waves (REN-1).
8. **Smoke without semantic hooks**: all of them — P2 requires only `assert_app` + screenshot (Matrices, Chemistry, OpticsLab, Bridge, ParticleLab initial screens…).
9. **Need hooks before golden promotion**: any golden showing computed content — Equations/Calculus results, Matrices det/inverse screens, Regression coefficients, Sequences terms (QA plan §G rule "no computation golden without a semantic assert").
10. **Visible-but-dead cards today**: ids 2, 3, 8, 9, 11–19 (and 20 in validate builds) — 13 cards (`NativeHal.cpp:983-985`).
11. **Reporting a dead-app selection**: keep refusing, with a stable greppable log line + ROUTE-02 negative control (routing §D.3).
12. **Hidden / disabled / visible-but-refusing**: visible-but-refusing (OD-4; keeps menu goldens firmware-identical).
13. **Interaction with the 20-card MainMenu**: no card changes ever; enablement only converts refusals into launches, so `launcher_smoke`'s byte-exact golden is untouched by the whole program.
14. **State reset between runs**: one script = one process; FS isolation via sandbox; no in-process reset command (routing §D.1).
15. **Firmware-only hardware features in emulator apps**: represent honestly — absent capability degrades to an explicit refusal/error string (the Giac pattern), never a silent fake; PSRAM maps to host heap via existing `new[]` fallbacks; battery/clock chrome follows SBAR hide-until-real.
