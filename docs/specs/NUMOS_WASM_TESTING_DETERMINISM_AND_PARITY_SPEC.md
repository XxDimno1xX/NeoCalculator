# NumOS WASM — Testing, Determinism, and Parity Specification

Status: SPEC (no implementation). Part 5 of the WASM port pack.
Convention: **[verified]** = read from source at `main` @ `ad536a7`; **[proposed]** = design.

---

## A. Test philosophy

The WASM port is **another emulator backend, not a separate product fork**. Its behavioral truth is defined by the same artifacts that define the desktop emulator's truth: the `.numos` script corpus (86 scripts **[verified count]**), the semantic assert grammar with frozen `PASS -`/`FAIL -` markers and exit codes 0/2/3/4 ([NativeHal.cpp:1788-1799](src/hal/NativeHal.cpp#L1788-L1799), [2092](src/hal/NativeHal.cpp#L2092), [2069-2074](src/hal/NativeHal.cpp#L2069-L2074) **[verified]**), the 230,415-byte P6 PPM frame contract ([generate-emulator-candidates.py:32-33](scripts/generate-emulator-candidates.py#L32-L33) **[verified]**), and the golden/mask policy (byte-exact, clock-rect-only masks, missing-golden-warns/mismatch-fails, [emulator-build.yml:682-714](/.github/workflows/emulator-build.yml#L682-L714) **[verified]**).

Corollaries:
- A test that exists for the desktop emulator and *can* run in the browser *must* run in the browser (or be listed with a reason in the parity matrix, §H).
- The browser never gets a *weaker* variant of an existing assertion; browser-specific tests are *additive* (§D).
- All comparison tooling is reused, not re-implemented: `compare-ppm.py` consumes browser-produced PPMs unchanged.

---

## B. Parity levels **[proposed]**

| Level | Meaning | Gate |
|---|---|---|
| **W0 builds** | wasm target compiles the full emulator allowlist, zero edits outside `src/hal` + `web/` | CI build job green |
| **W1 boots** | splash → launcher in a headless browser; first frame non-blank; zero console errors | boot smoke |
| **W2 opens core apps** | all 8 enabled apps open/exit via `open_app` + MODE; no hang, no abort | app-cycle script |
| **W3 deterministic scripts** | shared semantic `.numos` suites pass with identical exit codes and PASS/FAIL lines; self-diff byte-identical | script suite + self-diff |
| **W4 screenshots** | PPM extraction works; WASM golden set blessed and gated; masks reused | golden compare job |
| **W5 parity with SDL emulator** | per-stem SDL-vs-WASM drift report published; every stem classified identical / masked-identical / diverged-with-cause | parity measurement job |
| **W6 persistence** | sandbox/fixture/persistent modes tested incl. two-instantiation roundtrip and reload-survival | storage suite |
| **W7 public demo readiness** | cross-browser smoke, input/UX tests, budgets, PWA offline, release checklist | release gate |

Levels are cumulative; the roadmap phases (companion doc 6) map onto them.

---

## C. Existing tests to reuse **[verified inventory → proposed mapping]**

| Asset | Current state | WASM reuse |
|---|---|---|
| 86 `.numos` scripts (`tests/emulator/scripts/`) | append-only grammar ([NativeHal.cpp:1345-1375](src/hal/NativeHal.cpp#L1345-L1375)) | run verbatim via `numos_load_script`; `screenshot <path>` paths become harvest labels (same relative strings) |
| Assert hooks (`assert_result/_variable/_menu_focus/_graph_*` etc.) | semantic, no OCR | unchanged — they read C++ state, which is host-independent |
| 19 golden PPMs (`tests/emulator/golden/`) | Linux-reference, byte-exact | **not gated against WASM initially** — used as the comparison *reference* for the drift report (§E); shared per-stem only after proven byte-equal (WASM-OD-1(b)) |
| 18 masks (`tests/emulator/masks/*.mask`, all clock-rect `4,6,37,13`) | union-applied by `compare-ppm.py` | reused verbatim for WASM goldens (the StatusBar clock leak exists identically in wasm until EMUDET-01) |
| `generate-emulator-candidates.py` (44 stems, frame budgets) | drives desktop binary | stem list + frame budgets extracted to a shared manifest **[proposed]** consumed by both the desktop generator and the browser harness, so candidate inventories cannot drift |
| `compare-ppm.py` | byte-exact + masks, exit 0/1/2 ([compare-ppm.py:33-37](scripts/compare-ppm.py#L33-L37)) | consumed unchanged |
| Deterministic self-diff (run twice, sha256 equal, [emulator-build.yml:221-236](/.github/workflows/emulator-build.yml#L221-L236)) | `calc_1_plus_2` only | browser harness runs self-diff for **all** candidate stems (adopting the CI-02 ambition on the new host from day one) |
| App-enablement plans (P0–P7 ladder, PENDING.md lifecycle) | per-app QA governance | inherited: a stem's WASM golden follows the same pending→blessed lifecycle |

---

## D. Browser-specific tests **[proposed]**

Additive suite (Playwright), not expressible in `.numos`:

1. **Keyboard focus**: click canvas → type `1+2=` → result; click outside → keys ignored; re-focus → keys work; held key across blur → RELEASE synthesized (no stuck key).
2. **Virtual keypad**: pointer taps produce PRESS/RELEASE; long-press produces REPEAT cadence; SHIFT latching indicator matches `numos_modifier_state()`.
3. **Touch**: Playwright touch emulation of keypad flow on a mobile viewport.
4. **Resize/fullscreen**: integer-scale recomputation; framebuffer bytes unchanged by any resize (assert screenshot before/after identical — the "presentation cannot affect state" invariant).
5. **Background tab pause**: hide tab N seconds → return → no abort, no assert failure, animations consistent (interactive-mode behavioral check, not byte-gated).
6. **Storage persistence**: persistent mode: write var (STO flow) → `FS.syncfs` settles → new browser context, same origin → variable present (`assert_variable` script).
7. **Reset data**: erase-data flow → fresh defaults.
8. **Reload page**: sandbox mode: state gone (expected); persistent mode: state present.
9. **Offline PWA** (phase 6): load once → go offline → reload → boots from cache.
10. **Cross-browser smoke**: W1+W3-subset on Chromium + Firefox in CI; Safari (WebKit via Playwright) boot-only, non-gating initially (§H caveats).

---

## E. Screenshot strategy **[proposed, anchored in verified invariants]**

- **Capture source**: always the CPU framebuffer `g_lvBuf` via the shared `buildPpm()` path — never `canvas.toDataURL`, never `drawImage` readback. Rationale **[verified]**: the desktop screenshot is renderer-independent for exactly this reason ([NativeHal.cpp:1287-1291](src/hal/NativeHal.cpp#L1287-L1291) comment, [1292-1315](src/hal/NativeHal.cpp#L1292-L1315)); canvas readback would add premultiplication/color-management/DPR variables for nothing.
- **Pixel format**: RGB565 → RGB888 by the same bit-replication ([1303-1309](src/hal/NativeHal.cpp#L1303-L1309)); P6 header `P6\n320 240\n255\n`; 230,415 bytes. Any deviation is a harness bug, caught by the same `validate()` size/header check the candidate generator applies ([generate-emulator-candidates.py:239-251](scripts/generate-emulator-candidates.py#L239-L251)).
- **Scaling/devicePixelRatio**: irrelevant by construction (capture happens before presentation). A one-time test asserts capture identity across `?scale=` and DPR settings.
- **Font/rendering determinism**: fonts are compiled-in bitmaps; LVGL software renderer is integer-dominant → within one wasm binary, rendering is deterministic across runs (self-diff enforces). Across *engines* the wasm spec guarantees identical numeric semantics for the wasm code itself; residual drift can only enter through libm-in-libc (musl-derived, compiled *into* the wasm — therefore engine-independent too). Expected result: **cross-engine byte-identity is likely** — but it is asserted by measurement (WASM-TEST-06), never assumed.
- **Comparison against SDL goldens**: the drift report (W5) classifies each stem. Divergence causes to hunt in order: `-ffp-contract` (neutralized at compile, companion doc 4 §C), libm differences (glibc vs Emscripten-musl `sin/cos/pow` — plausible in Grapher curve stems), `long double` (x87 80-bit on Linux-x86 vs 64-bit in wasm — audit for `long double` use), uninitialized-read luck.
- **Separate vs shared goldens**: start with `tests/emulator/golden-wasm/` **[proposed dir]**; per-stem promotion to "shared" (delete the wasm copy, gate on the SDL golden) only when the drift report shows sustained byte-equality (≥10 CI runs, mirroring the golden maturation rule). Bookkeeping lives in the golden governance file (PENDING.md pattern).
- **Mask policy**: unchanged — clock rect only; the CIART mask lint (>1% frame or body region fails) applies to any wasm-side mask verbatim. No new mask classes for the browser; dynamic content is handled by framing, never masking.
- **False-positive risks**: harness must fail on *missing* screenshots (a lost capture invalidates the run — same philosophy as exit 3 [NativeHal.cpp:2062-2074](src/hal/NativeHal.cpp#L2062-L2074) **[verified]**); byte-compare of a stale artifact from a previous test is prevented by per-test module instances and per-run output dirs.

---

## F. Deterministic mode **[proposed, same contract as EMUDET]**

- **Virtual time**: synthetic tick only (`detTick += stepMs` per frame); `nowMillis()` never consulted; `Date.now()/performance.now()` never feed emulator state. The StatusBar `time()` leak persists until EMUDET-01 lands in shared code — until then, wasm goldens use the same clock mask, keeping the two hosts' mask stories identical.
- **Fixed seed**: adopt EMUDET-03 semantics on this host: harness asserts the seed banner once EMUDET-03 lands; until then, no candidate stem may depend on `rand()` (already true of the desktop stem set — rand-caption apps are excluded from goldens by the app-QA plan).
- **Filesystem fixture**: deterministic runs always `sandbox` or `fixture` mode (companion doc 3 §D); `persistent` + script is a config error.
- **Script runner**: identical one-command-per-frame scheduler ([NativeHal.cpp:1803-1807](src/hal/NativeHal.cpp#L1803-L1807) **[verified]**), whole-script fail-fast validation (exit-2 parity before any frame runs).
- **No live input**: enforced at `pollInput` (EMUDET-04 semantics — the browser implements this from day one even though the desktop hasn't landed EMUDET-04 yet; it is strictly stricter, therefore safe).
- **Screenshot frame**: captures happen only at frame boundaries after render, as today.
- **Reproducibility across engines**: self-diff within each engine is a hard gate; cross-engine byte-equality is measured (WASM-TEST-06) and, once proven, promoted to a gate on the two CI engines.

---

## G. CI browser testing **[proposed]**

- **Runner**: Playwright (pinned) — chosen over Puppeteer for first-class Firefox/WebKit coverage; headless Chromium + Firefox gate, WebKit informational.
- **Flow per test**: static-serve the built site → `page.goto` → wait `numosReady` → drive via exported functions (`numos_load_script`, `numos_run_frames`) — *not* via synthesized DOM keys, except in the input-specific tests (§D) where DOM-level realism is the point.
- **Artifact capture on failure**: PPMs + `compare-ppm.py --diff` PNGs, console log (with `[ASSERT]` lines), page screenshot (the shell, for UX bugs), state envelope, `build-info.json`.
- **Console errors**: any `console.error` or `pageerror` during gated runs fails the test (zero-tolerance keeps the shell honest).
- **WASM traps**: `onAbort` → test failure with reason + symbolized stack (debug-build rerun job available via workflow_dispatch).
- **Performance metrics**: boot time, mean/p95 frame time, wasm size recorded per run into a JSON artifact (trend tracking; budget gate in companion doc 4 §F/G).

---

## H. App parity matrix **[proposed statuses over verified current enablement]**

Currently emulator-enabled apps (per the enablement matrix and [NativeHal.cpp:924-996](src/hal/NativeHal.cpp#L924-L996) **[verified]** routing):

| App (id) | Expected WASM status | Risks | Required tests |
|---|---|---|---|
| Calculation (0) | Full — pure integer/exact math paths dominate | decimal path uses doubles (low risk) | calc semantic suite, `calc_1_plus_2`/`calc_fraction_sum` goldens, STO roundtrip (sandbox) |
| Grapher (1) | Full expected; **highest FP-drift risk** (curve rasterization via libm) | libm drift vs SDL goldens; async-POI timing already a known desktop trap (frame-indexed, so OK deterministically) | grapher script suites (9F + Phase-10 assert set incl. negative control), grapher goldens (wasm set first) |
| Statistics (4) | Full visual; semantic blocked same as desktop (digit-entry bug) | none browser-specific | smoke + golden |
| Probability (5) | same as Statistics | none | smoke + golden |
| Regression (6) | Full visual | FP in fit math → golden drift possible | smoke + golden |
| Sequences (7) | Full visual | none | smoke + golden |
| Settings (10) | Full | settings persistence arrives with GS-02/03 | smoke + golden; later `assert_setting_value` suite |
| MathShowcase (100) | Full — cursor-off deterministic by design | none | smoke + golden |

P-05/P-06 future apps: **not blocked by the browser** — they are blocked by their existing blockers (native-safety closure, update()-tick ROUTE-04, FS sandbox, rand seeding). Policy: an app reaches the browser **only after** it reaches the desktop emulator; the browser adds one risk column to their enablement checklists: *browser-specific risk* = FP-heavy sims (Fluid2D worst, per the matrix's cross-host float-drift note) and timer-driven sims (fine deterministically, wall-clock-paced interactively). NeuralLab (unseeded `rand()`) and NeoLang (raw `std::fopen` sandbox bypass) are the two the browser must specifically refuse until their FIX/seed dependencies land.

---

## I. Acceptance gates **[proposed]**

- **MVP demo gate (G1/W1-W3):** W0–W3 green on Chromium+Firefox; boot ≤ budget; zero console errors; demo page manually smoke-tested on Windows+desktop Safari once.
- **Public demo gate (G5/W7):** W4–W6 green; cross-browser matrix executed with results recorded in this pack; budgets green 2 consecutive weeks; release checklist (companion doc 6, prompt 6) signed by a human.
- **PR gate (ongoing):** wasm build + W1 smoke + W3 script suite on every PR touching `src/**`, `web/**`, or the wasm workflow; golden compare gates only stems with blessed wasm goldens; dirty-tree guard.
- **Release gate:** PR gate + full candidate self-diff + parity report generated + budgets + manual checklist.

---

## J. Known risks

1. **Browser FP/libm drift** vs Linux SDL goldens (Grapher/Regression stems most exposed) — mitigated by `-ffp-contract=off`, per-host goldens until measured, drift report.
2. **SDL2-Emscripten backend differences** (input focus, text input, canvas ownership) during Phase 1–2 — bounded by the backend interface; retired with the canvas-native backend.
3. **Input focus differences** (browser focus model vs SDL window focus) — explicit focus tests (§D.1).
4. **Async storage** (IDBFS sync races: state lost if tab dies pre-sync) — debounced sync + pagehide flush + documented best-effort; deterministic tests never depend on IDBFS.
5. **Mobile Safari memory/eviction** — tier-2 policy, export/import as durability story.
6. **WASM size** (font arrays + LVGL) — budgets + symbol-map measurement in Phase 0.
7. **Canvas scaling artifacts** (non-integer DPR) — capture-before-present makes this cosmetic-only; explicit invariant test (§D.4).
8. **Font rendering outside the framebuffer** — none: all NumOS text renders inside LVGL; shell text is not part of any golden. Keep it that way (never composite shell UI into captures).
9. **Two-backend era drift** (SDL desktop vs web backend behavior differences hiding in platform code) — every behavior in `EmuCore`, platform backends kept dumb; parity suite runs on both.
10. **rAF throttling in background tabs breaking long local test runs** — batched `numos_run_frames` + setTimeout slices; CI headless unaffected.
11. **LVGL version float** (`^9.2.0` **[verified]** [platformio.ini:178](platformio.ini#L178)) — a rebuild can shift pixels on both hosts; CI-06 (exact pin) becomes a prerequisite for the parity report being meaningful; wasm build records the resolved version.
12. **Committed `emulator_data/vars.dat` fixture leak** — desktop issue (FIX-03) with a browser analogue: never seed the persistent store from tests; enforced by mode-validation.
13. **Harness divergence from `.numos` semantics** (e.g. harness "helpfully" retrying) — forbidden: the harness is a transport, all semantics stay in C++.
14. **Exit-code erosion** in a no-process world — `numos_exit_code()` is part of the frozen contract; tests assert exact codes (0/2/3/4), never just truthiness.
15. **Spanish log-string drift** — harness greps reuse the exact strings CI greps today (`Launcher cargado`, `tick = determinista` **[verified]** [emulator-build.yml:141](/.github/workflows/emulator-build.yml#L141), [:173](/.github/workflows/emulator-build.yml#L173)); WASM-OD-8 freezes them.

---

## K. Explicit answers (testing subset of the Special Questions)

- **Q6 (same PPM goldens?):** Same *format and comparator*, separate `golden-wasm/` gating initially; per-stem promotion to shared goldens on proven byte-equality (§E).
- **Q7 (browser screenshot capture):** exported CPU-framebuffer PPM (`buildPpm` bytes), never canvas readback (§E).
- **Q9 (deterministic time):** synthetic frame-indexed tick; browser clocks quarantined behind `nowMillis()`; clock mask retained until EMUDET-01 (§F).
- **Q10 (script replay):** same engine, same one-command-per-frame contract, batched execution as a pure speed optimization (§F, companion doc 2).
- **Q16 (must remain identical):** framebuffer geometry + pixel format; PPM contract; script grammar + exit codes + assert markers; app whitelist and launcher routing; deterministic-mode semantics; KeyCode dispatch semantics; log grammar; `/vars.dat` (and future `/settings.dat`) byte formats.
- **Q17 (may legitimately differ):** pacing source (rAF vs SDL_Delay), presentation path (canvas vs SDL texture), persistence backend (IDBFS vs host dirs), input *transport* (DOM vs SDL events), interactive-only conveniences (pause on hidden tab), golden *sets* until parity is proven, and the absence of `--run-for-ms`/wall-clock auto-exit.
