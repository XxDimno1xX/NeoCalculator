# NumOS WebAssembly Port — Architecture Specification

Status: SPEC (no implementation). Authored 2026-07-06 against `main` @ `ad536a7`.
Ticket namespace: `WASM-*` (subnamespaces `WASM-ARCH/BUILD/RUN/UI/FS/TEST/DEPLOY/PERF`). This prefix is unused anywhere in `docs/specs/` (namespace census in the app-enablement roadmap and the SCROAD collision note were checked; `WASM-*` and `WEB-*` are both free — we take `WASM-*`).

Companion documents (this pack):
1. This file — architecture and decisions.
2. `NUMOS_WASM_NATIVEHAL_RUNTIME_AND_EVENT_LOOP_SPEC.md` — runtime port.
3. `NUMOS_WASM_BROWSER_UI_INPUT_STORAGE_SPEC.md` — browser product behavior.
4. `NUMOS_WASM_BUILD_DEPLOY_AND_CI_SPEC.md` — build & deployment.
5. `NUMOS_WASM_TESTING_DETERMINISM_AND_PARITY_SPEC.md` — testing & parity.
6. `NUMOS_WASM_IMPLEMENTATION_ROADMAP_AND_PROMPTS.md` — tickets, phases, prompts.

Evidence convention: `file:line` citations refer to the working tree at the commit above. Claims marked **[verified]** were read directly from source for this spec; claims marked **[proposed]** are design, not current behavior. Anything not marked is context inherited from the prerequisite specs listed in §J.

---

## A. Executive summary

**What the WebAssembly port is.** A third *host* for the existing NumOS native emulator: the same C++17 app/runtime/LVGL code that today compiles to `esp32s3_n16r8` (firmware) and `emulator_pc` (SDL2 desktop) is additionally compiled with Emscripten to a `.wasm` module plus a small JS/HTML shell, rendering the 320×240 RGB565 framebuffer into a `<canvas>` and feeding browser keyboard/touch events into the exact same `dispatchKey()` path the desktop emulator uses ([NativeHal.cpp:575](src/hal/NativeHal.cpp#L575) **[verified]**).

**What it is not.** It is not a second emulator, not a TypeScript rewrite, not an ESP32 hardware emulator, not a Giac/CAS-in-the-browser promise, and not a separate product fork. Every behavior the browser build exhibits must be traceable to shared C++ code or to an explicitly declared platform-backend divergence, exactly as the persistence-sandbox spec already requires for firmware↔emulator divergences (parity register, `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` §F).

**Why a port of the emulator, not a rewrite.** The emulator is already the project's behavioral oracle: 86 `.numos` scripts, 19 byte-exact PPM goldens, 18 clock-rect masks, a deterministic tick, a fail-fast script/assert engine with frozen exit codes (0/2/3/4), and a CI gate chain of 13+ steps ([emulator-build.yml](/.github/workflows/emulator-build.yml)). A rewrite would have to re-earn all of that; a port inherits it. The risk register already names dispatcher drift between two hand-written registries as risk EMU-1 — a third hand-written registry (a JS rewrite) would make that risk structural. The port must therefore *reduce* the number of hand-maintained loops (via a backend interface), not add one.

**What "success" means for the first web demo (MVP).**
- The `emulator_pc` source set compiles under Emscripten with zero edits to `src/apps/**`, `src/math/**`, `src/ui/**` (edits confined to `src/hal/**` + new platform files).
- The page boots to the launcher, arrows+ENTER open Calculation, typing `1+2=` shows `3` — on desktop Chrome and Firefox.
- A deterministic scripted run (`launcher_smoke` semantics) executes in a headless browser in CI with the same exit-code contract.
- No persistence, no PWA, no mobile polish required at MVP.

**What "perfect port" means long-term.**
- All 8 currently-enabled emulator apps at their current parity grades (P0–P7 ladder of `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md`) reachable and scripted in the browser.
- The full `.numos` corpus runs in browser CI with byte-identical self-diff; per-stem golden parity with the Linux SDL goldens measured, and shared goldens adopted for every stem proven byte-equal.
- Opt-in persistence (variables, settings) in IndexedDB with export/import and reset, obeying the sandbox spec's "absent by default, explicitly declared" rule.
- Installable offline PWA; virtual keypad usable on touch devices; published versioned builds with commit-hash display and rollback.

---

## B. Current emulator architecture map (verified current state)

All of the following is **current, verified behavior**, cited from source.

**Entry point & guard.** The emulator is a single translation unit `src/hal/NativeHal.cpp` compiled only under `-DNATIVE_SIM` ([NativeHal.cpp:85](src/hal/NativeHal.cpp#L85), [platformio.ini:161](platformio.ini#L161)). Firmware `main.cpp` is entirely `#ifdef ARDUINO` ([main.cpp:23](src/main.cpp#L23)); the emulator has its own `main()` ([NativeHal.cpp:2085](src/hal/NativeHal.cpp#L2085)).

**NativeHal responsibilities** (one file, ~2390 lines):
- CLI parsing: `--frames`, `--run-for-ms`, `--scale`, `--headless`, `--quiet`, `--deterministic`, `--step-ms`, `--screenshot/--dump-frame`, `--script`, `--help` ([NativeHal.cpp:1245-1280](src/hal/NativeHal.cpp#L1245-L1280)).
- SDL window/renderer/streaming-texture creation, logical size 320×240 + integer scale ([NativeHal.cpp:2117-2170](src/hal/NativeHal.cpp#L2117-L2170)).
- LVGL init, tick source selection (`SDL_GetTicks` wall clock vs synthetic `detTickCb`) ([NativeHal.cpp:2207](src/hal/NativeHal.cpp#L2207)), full-frame CPU buffer `g_lvBuf` (320×240×2 bytes RGB565, `LV_DISPLAY_RENDER_MODE_FULL`) ([NativeHal.cpp:271](src/hal/NativeHal.cpp#L271), [2218-2220](src/hal/NativeHal.cpp#L2218-L2220)).
- Input mapping: SDL keysyms for navigation/letters ([NativeHal.cpp:318-400](src/hal/NativeHal.cpp#L318-L400)), `SDL_TEXTINPUT` for layout-resolved printable characters ([415-444](src/hal/NativeHal.cpp#L415-L444), enabled at [2134](src/hal/NativeHal.cpp#L2134)), script key-name table ([456-565](src/hal/NativeHal.cpp#L456-L565)) — all three funnel into one `dispatchKey()` ([575-769](src/hal/NativeHal.cpp#L575-L769)).
- Its own app registry: `AppMode` enum of 10 states ([195-206](src/hal/NativeHal.cpp#L195-L206)), `launchApp(id)` for ids {0,1,4,5,6,7,10,100} ([924-996](src/hal/NativeHal.cpp#L924-L996)), deferred teardown at `TEARDOWN_DELAY_MS = 260` ms of `lv_tick` ([230-233](src/hal/NativeHal.cpp#L230-L233)) mirroring the firmware's 250 ms rule.
- Script replay + semantic asserts: one command per frame, executed before SDL events and tick advance ([1803-1807](src/hal/NativeHal.cpp#L1803-L1807)); assert families for Calculation results, variables, menu focus, and eight Grapher `assert_graph_*` hooks ([1345-1375](src/hal/NativeHal.cpp#L1345-L1375)); fail-fast whole-script validation before SDL init (exit 2, [2091-2093](src/hal/NativeHal.cpp#L2091-L2093)); assert failure → exit 4 ([1788-1794](src/hal/NativeHal.cpp#L1788-L1794)); screenshot write failure → exit 3 ([2064-2075](src/hal/NativeHal.cpp#L2064-L2075)).
- Screenshot: `saveScreenshotPPM()` reads `g_lvBuf` (never the SDL texture/renderer), RGB565→RGB888 by bit replication, P6 PPM, works identically headless and at any scale ([1292-1315](src/hal/NativeHal.cpp#L1292-L1315)).

**SDL2 responsibilities.** Window + accelerated (fallback software) renderer + RGB565 streaming texture ([2136-2155](src/hal/NativeHal.cpp#L2136-L2155)); event pump; wall clock (`SDL_GetTicks`) and sleep (`SDL_Delay(5)` per frame, [155](src/hal/NativeHal.cpp#L155), [2315-2317](src/hal/NativeHal.cpp#L2315-L2317)); `--headless` sets `SDL_VIDEODRIVER=dummy` pre-init ([2098-2100](src/hal/NativeHal.cpp#L2098-L2100)). Crucially SDL is *presentation and timing only* — LVGL composes into the CPU buffer; the flush callback merely copies to the texture and defers `SDL_RenderPresent` to the loop ([293-313](src/hal/NativeHal.cpp#L293-L313)).

**LVGL responsibilities.** All rendering (software renderer into `g_lvBuf`), widget tree, animations/timers driven by `lv_timer_handler()` once per loop iteration ([2279](src/hal/NativeHal.cpp#L2279)). Native builds route LVGL's allocator to libc (`-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB`, [platformio.ini:176](platformio.ini#L176)) because LVGL 9 otherwise falls back to a 64 KB pool that object-heavy screens exhaust.

**Framebuffer / screenshot model.** Single source of truth: the CPU buffer. This is the property that makes screenshots renderer-independent and is the anchor for the whole golden policy; the browser port must preserve it (see companion doc 5 §E).

**Script replay model.** `.numos` grammar is append-only (risk EMU-4 protects 86 scripts); commands: `wait/key/keydown/keyup/screenshot/log/open_app/assert_*` ([1345-1375](src/hal/NativeHal.cpp#L1345-L1375)).

**Filesystem model.** `src/hal/FileSystem.{h,cpp}` shims `LittleFS` over stdio `FILE*` under a hardcoded CWD-relative root `./emulator_data` ([FileSystem.cpp:38](src/hal/FileSystem.cpp#L38)); no env override, no fixture mode. The only live store is `VariableManager` ↔ `/vars.dat` (magic `VR01`, 34-byte records; save at [VariableManager.cpp:170-186](src/math/VariableManager.cpp#L170-L186), load at boot via `nativeFS_init()` [NativeHal.cpp:2380-2388](src/hal/NativeHal.cpp#L2380-L2388)). The sandbox spec (FIX-01..06) defines the target flag surface (`--fs-sandbox`, `--fixture-dir`, `--fs-root`, …) — **not yet implemented**.

**App routing model.** Two independent registries exist (firmware `SystemApp` with 24 `Mode` states, emulator `AppMode` with 10) — risk EMU-1. Only 8 apps are wired natively; all other launcher cards print `[APP] … no implementada` and stay in the menu ([NativeHal.cpp:993-995](src/hal/NativeHal.cpp#L993-L995)). The per-app enablement matrix (Enabled: Calculation 0, Grapher 1, Statistics 4, Probability 5, Regression 6, Sequences 7, Settings 10, MathShowcase 100; everything else Blocked/P-05/P-06) is authoritative.

**Deterministic mode model.** `--deterministic` swaps *only* the LVGL tick for a synthetic counter advanced `+= stepMs` per frame before `lv_timer_handler()` ([2275-2277](src/hal/NativeHal.cpp#L2275-L2277)) and skips the sleep ([2315](src/hal/NativeHal.cpp#L2315)). Known leaks (per EMUDET spec): `time()` in StatusBar clock, `SDL_GetTicks()` in `--run-for-ms` ([2334-2336](src/hal/NativeHal.cpp#L2334-L2336)), `millis()/micros()/delay()` in ArduinoCompat, unseeded `rand()`.

**CI/golden model.** One Linux-only job; deterministic self-diff (run `calc_1_plus_2` twice, compare SHA-256, [emulator-build.yml:221-236](/.github/workflows/emulator-build.yml#L221-L236)); 44 candidate stems generated by `scripts/generate-emulator-candidates.py` (invocation `--headless --deterministic --script … --frames N --screenshot … --quiet`, [generate-emulator-candidates.py:317-325](scripts/generate-emulator-candidates.py#L317-L325)); golden compare via `scripts/compare-ppm.py` (byte-exact, mask union, exit 0/1/2); promotion human-only.

**Current limitations relevant to a browser port.**
1. Blocking infinite `while (!g_quit)` main loop with in-loop sleep — incompatible with the browser event loop as written ([2254-2339](src/hal/NativeHal.cpp#L2254-L2339)).
2. Monolithic NativeHal — platform code (SDL, argv, stdio) and shared harness code (script engine, asserts, screenshot, app registry) live in one TU with file-static state.
3. Hardcoded CWD-relative FS root; no sandbox flags yet (FIX-01 not landed).
4. Wall-clock leaks outside the tick (StatusBar `time()`, `--run-for-ms`).
5. Exit-code semantics assume a process that exits — a browser tab doesn't.
6. No app `update()` tick in the emulator loop (divergence D6) — inherited as-is; not a WASM problem to fix.

---

## C. Browser constraints

These are the environmental facts the design must respect. They are standard platform behavior (not repo claims), stated here as requirements.

1. **No blocking main loop.** A wasm function that never returns freezes the tab and gets killed by the browser's hang detector. The `while (!g_quit)` loop must become a per-frame callback (`emscripten_set_main_loop`) or an Asyncify-transformed loop. `SDL_Delay` inside a frame is a busy-block under Emscripten without Asyncify.
2. **requestAnimationFrame pacing.** The browser calls the frame callback at display refresh (typically 60 Hz, can be 120/144), only while the tab is visible. Frame cadence is neither fixed nor guaranteed — another reason the deterministic tick (frame-indexed, not wall-clock) is the right foundation.
3. **No native filesystem.** Emscripten offers MEMFS (in-memory, lost on reload), IDBFS (IndexedDB-backed, *asynchronous* explicit sync), OPFS/worker FS (needs workers), and the File System Access API (Chromium-only, permission-gated). `./emulator_data` must map onto one of these via a mount decision.
4. **Keyboard focus rules.** Key events go to the focused element; a `<canvas>` is not focusable unless it has `tabindex`. Clicking away silently stops input. Some keys (Tab, F5, Ctrl+…, Backspace-as-back) have browser default actions that must be `preventDefault()`ed only while the emulator has focus.
5. **Touch/pointer events.** Mobile has no hardware keys; input must come from an on-screen keypad. Touch also implies no `keydown` repeats and different focus semantics.
6. **Async startup/assets.** The `.wasm` (and any preloaded data) arrive over the network; instantiation is async. Boot must tolerate "module not ready yet" and show a loading state. There is no synchronous `main()`-before-page-paint.
7. **Canvas rendering.** The framebuffer must be blitted via `putImageData`/texture upload each frame; `devicePixelRatio` scaling, CSS scaling, and image-smoothing defaults can destroy pixel crispness unless explicitly controlled (`image-rendering: pixelated`, integer scale).
8. **Clocks.** `performance.now()` is coarsened (Spectre mitigations) and `Date.now()` is wall clock; neither is stable across engines. Emscripten's `SDL_GetTicks` maps to `performance.now()`. Deterministic runs must not consume either.
9. **Cross-origin isolation.** `SharedArrayBuffer` (required for pthreads) needs COOP/COEP headers; GitHub Pages cannot set custom headers (only workaround: a service-worker shim). This is a strong argument to forbid pthreads (§D, decision Q4).
10. **Memory growth.** `ALLOW_MEMORY_GROWTH` costs a copy on grow and (without threads) is otherwise safe; fixed memory risks OOM aborts. Mobile Safari historically caps wasm memory aggressively (practical budgets ~256–512 MB; NumOS needs far less).
11. **Mobile Safari specifics.** No `beforeinstallprompt`, stricter IndexedDB eviction (7-day ITP eviction for unused sites), audio/vibration restrictions, historical wasm bugs — treat iOS as best-effort tier 2 (companion doc 3 §H).
12. **Deployment/cache/versioning.** Static hosts cache aggressively; `.wasm` must be served with `application/wasm` for streaming compile; cache-busting must be by content hash; a stale HTML + new wasm (or vice versa) must fail loudly (build-metadata check, companion doc 4 §I).

---

## D. Architecture options

### Option 1 — Emscripten SDL2 port of the current NativeHal
Compile `NativeHal.cpp` nearly as-is; Emscripten provides an SDL2 implementation over canvas/DOM events (`-sUSE_SDL=2`).
- **Pros:** Maximum immediate reuse (input mapping, window/texture code all "just work" in principle); smallest diff; fastest first boot; the script engine, asserts, screenshot code compile unchanged.
- **Cons:** Still requires the main-loop rewrite (Emscripten SDL does not remove that); inherits SDL's browser quirks (its own key handling intercepts events, its own canvas management fights a custom shell); text-input path (`SDL_TEXTINPUT`) behaves differently against IME/mobile; harder to build the product shell (virtual keypad, overlays) around SDL's canvas ownership; adds ~100–200 KB of SDL shim JS.
- **Code reuse:** ~95% of NativeHal. **Parity risk:** low for pixels (LVGL still renders to `g_lvBuf`), medium for input. **Maintenance:** medium — monolith stays a monolith. **Testability:** good (same script engine). **Performance:** fine (blit-per-frame). **Deployment complexity:** low.

### Option 2 — Backend split: `IEmulatorPlatform` interface with a browser canvas/JS backend
Extract the platform-touching ~15% of NativeHal (window/present, event pump, clock, argv, process exit) behind a small C++ interface; keep the other ~85% (app registry, dispatchKey, script engine, asserts, screenshot, deterministic tick, teardown) as a shared "emulator core". Implement two backends: the existing SDL code (desktop, behavior-identical) and a browser backend (canvas blit via small JS glue, input queue fed from JS listeners).
- **Pros:** Kills the "second emulator" failure mode by construction — there is exactly one core; desktop emulator keeps working unchanged; the browser shell owns its canvas/DOM (clean product UX); the seam is exactly where EMUDET-01's `numosNow()` and FIX-01's FS-root already want a seam; testable per-backend.
- **Cons:** A refactor of the one file the entire CI chain depends on — must be done behaviorally-neutral and verified by the existing golden/self-diff gates; more upfront design than Option 1.
- **Code reuse:** ~85% shared + SDL backend preserved. **Parity risk:** lowest long-term. **Maintenance:** best. **Testability:** best. **Performance:** same as 1. **Deployment complexity:** low.

### Option 3 — LVGL web backend
LVGL upstream has demo web ports (LVGL compiled with Emscripten + SDL, or the "lvgl web" simulator projects). This is not a distinct architecture: it is Option 1 with LVGL's example glue instead of ours, and it would bypass NativeHal entirely — losing the script engine, asserts, app registry, and screenshot contract.
- **Pros:** Upstream-maintained boot examples to crib from.
- **Cons:** Discards the harness that is the point of the port; parity risk maximal.
- **Verdict:** useful only as reference material. Rejected as an architecture.

### Option 4 — Full TypeScript rewrite of the emulator shell
Reimplement the launcher/apps/renderer in TS, keeping only math in wasm (or nothing).
- **Pros:** Native web UX; no Emscripten.
- **Cons:** A second product. Violates the anti-fork rules of the app-enablement parity spec §I; re-implements MathRenderer/LVGL layout (the TeX-parity work) from scratch; zero reuse of 86 scripts/19 goldens; risk EMU-1 squared. **Rejected.**

### Option 5 — Remote emulator streaming
Run the existing Linux emulator server-side; stream frames (VNC/WebRTC) to the browser; forward keys.
- **Pros:** Perfect parity by definition; zero porting.
- **Cons:** Requires a server (violates "no server dependency for MVP", §G); latency; cost; no offline; not embeddable in docs; doesn't produce a testable browser artifact. **Rejected for the product**, though noted as a cheap interim demo trick if ever needed.

### Recommended: Option 2 as the architecture, executed via Option 1 as the first backend

Phase the work so the risky refactor is validated by the machinery it protects:
1. **WASM-ARCH-01** extracts `IEmulatorPlatform` from NativeHal with the SDL backend as the only implementation — a pure refactor gated on the existing CI (self-diff + all goldens byte-identical, all `.numos` suites green).
2. **First browser backend uses Emscripten's SDL2** (`-sUSE_SDL=2`) behind the same interface — i.e., the browser backend is initially *mostly the SDL backend recompiled*, minimizing new code for first boot (Phase 1).
3. **The canvas-native browser backend** (direct `putImageData`, own input listeners, no SDL shim) replaces the Emscripten-SDL backend once the product shell needs to own the DOM (Phase 3), shrinking the payload and removing SDL-shim input quirks. Both browser backends implement the same interface, so this swap cannot change core behavior.

---

## E. Recommended architecture (normative) **[proposed]**

- **WASM target structure.** One Emscripten build target `numos-emu.wasm` + `numos-emu.js` (MODULARIZE'd ES module) + `index.html` shell + `shell.css` + optional `numos-emu.data` (only if assets ever move out of the binary; today fonts are compiled in as C arrays — `src/fonts/stix_math_*.c` [platformio.ini:264-266](platformio.ini#L264-L266) — so **no data package at MVP**).
- **Browser NativeHal abstraction.** `IEmulatorPlatform` (full signature set in companion doc 2 §D): `pollInput`, `presentFramebuffer`, `nowMillis`, `sleep`, `captureScreenshotSink`, `requestExit`, `log`, plus FS root resolution hooks. The emulator core never touches SDL, Emscripten, or DOM symbols directly.
- **Shared emulator core.** Everything currently in NativeHal that is not SDL/argv/stdio: app registry + `launchApp` + deferred teardown, `dispatchKey`, script loader/stepper/asserts, `saveScreenshotPPM` (writing through a platform sink), deterministic tick, splash/menu lifecycle. File layout **[proposed]**: `src/hal/EmuCore.{h,cpp}` (core), `src/hal/PlatformSdl.cpp` (desktop), `src/hal/PlatformWeb.cpp` + `web/shell/*.{js,html,css}` (browser). `NativeHal.cpp` shrinks to `main()` + wiring, preserving the file the specs cite for as long as practical.
- **Platform backend boundary.** The boundary is *below* input mapping semantics: browser key events are translated JS-side into the **script key-name vocabulary** (`enter`, `left`, `sin`, digits, symbols — the append-only names of [NativeHal.cpp:456-565](src/hal/NativeHal.cpp#L456-L565)) and injected via one exported C ABI function. This reuses the frozen, tested name table instead of inventing a parallel keycode map (kills input drift; details in companion docs 2 §F and 3 §B).
- **JS glue layer.** Hand-written, dependency-free ES module (`numos-shell.js`): module instantiation, canvas management, key listeners, virtual keypad, config→argv translation, error overlay. No framework at MVP (third-party dependency policy, companion doc 3 §G).
- **Canvas/framebuffer ownership.** The wasm side owns `g_lvBuf` (RGB565); `presentFramebuffer` exposes the buffer pointer + dirty flag; JS converts RGB565→RGBA into a reused `ImageData` and `putImageData`s to a 320×240 backing canvas, CSS-scaled with `image-rendering: pixelated`. The screenshot path *never* reads the canvas — it reads `g_lvBuf`, same as today (companion doc 5 §E).
- **Input bridge.** JS → ring buffer (mirroring `LvglKeypad`'s 8-slot pattern, [LvglKeypad.h:77](src/input/LvglKeypad.h#L77)) → drained by `pollInput()` at the top of each frame → `dispatchKey`. Live input is ignored during script runs (EMUDET-04 rule) unless explicitly allowed.
- **Storage bridge.** `./emulator_data` mounts on MEMFS by default (fresh per load — automatically satisfies the sandbox spec's "deterministic runs start clean" rule); an opt-in "persistent session" mounts IDBFS at the same path with explicit sync points (after each `saveToFlash`-triggering event and on `visibilitychange`). Flag surface mirrors FIX-01 (`fsMode: "sandbox" | "persistent" | "fixture"`). Full design in companion doc 3 §C.
- **Deterministic mode.** Identical semantics: synthetic tick, fixed step, one script command per frame, no dependence on rAF timing (each rAF callback runs exactly one frame; in test mode frames can be driven by a loop without rAF at all). `numosNow()` (EMUDET-01) is implemented in the platform interface so the browser never consults `Date.now()` on asserted paths.
- **Script runner.** The same C++ loader; scripts are delivered as strings from JS (fetched or embedded) and handed to the existing `loadScript` path via a MEMFS temp file or a `loadScriptFromString` overload **[proposed]** (companion doc 2 §I).
- **Screenshot/export API.** Exported functions `numos_screenshot_ppm()` (returns pointer+length of a PPM in wasm memory — byte-identical format to [NativeHal.cpp:1292-1315](src/hal/NativeHal.cpp#L1292-L1315)) and a JS helper that wraps it in a downloadable Blob / test-harness byte array.
- **App parity policy.** The browser build compiles the *same* `build_src_filter` allowlist (extracted to a shared file list, companion doc 4 §B) and routes the *same* 8 app ids. Browser exposes no app the desktop emulator doesn't; enablement continues to be governed by the P-05/P-06 matrix, and a newly enabled app reaches the browser by recompilation, not by porting.

---

## F. Layering proposal

| Layer | Contents | Owner files (today → target) | Rules |
|---|---|---|---|
| 1. Core NumOS app/runtime | apps, math, CAS-lite, ui, fonts, LVGL | `src/apps|math|ui|fonts`, `lvgl` | Never edited for the port. `#ifdef ARDUINO` guards only. |
| 2. Emulator platform abstraction | `IEmulatorPlatform`, EmuCore (registry, dispatch, scripts, asserts, screenshots, det-tick) | `src/hal/NativeHal.cpp` → `src/hal/EmuCore.*` + `src/hal/IEmulatorPlatform.h` | Single implementation of all harness logic. Append-only grammar/exit codes. |
| 3. Native SDL backend | SDL window/renderer/texture/events/clock | → `src/hal/PlatformSdl.cpp` | Behavior-identical extraction; gated by existing goldens. |
| 4. Browser/WASM backend | Emscripten main-loop adapter, canvas present, input queue, FS mounts, exports | new `src/hal/PlatformWeb.cpp` (guarded `__EMSCRIPTEN__`) | No DOM logic in C++ beyond EM_JS/exports. |
| 5. JS/TypeScript shell | page, canvas, keypad, config, error overlay, loader | new `web/` directory | Zero third-party runtime deps at MVP. Never interprets NumOS semantics. |
| 6. Asset/FS bridge | MEMFS/IDBFS mounts, script delivery, state export/import | `PlatformWeb.cpp` + shell | Default fresh; persistence opt-in; sandbox-spec flag parity. |
| 7. Test harness | Playwright driver, PPM extraction, exit-code plumbing, golden compare reuse (`compare-ppm.py`) | new `tests/wasm/` | Reuses `.numos` scripts and comparator verbatim. |

---

## G. Non-goals

- **No full firmware emulation.** No ESP32 CPU/peripheral/PSRAM/DMA emulation; the browser build proves UI/math/input logic exactly as far as the desktop emulator does (MEM-5 caveat inherited verbatim).
- **No ESP32 peripheral emulation beyond current app needs.** Battery %, RTC, serial console remain stubs.
- **No CAS/Giac browser promise.** Giac does not build natively today ([platformio.ini:192-194](platformio.ini#L192-L194) `lib_ignore = giac, libtommath`); the browser inherits "CAS unavailable via Giac; in-tree `cas::` only". Compiling Giac to wasm is a separate future investigation, not implied here.
- **No WebGPU/WebGL requirement.** Software blit (`putImageData`) is sufficient for 320×240@60; GPU paths add drift risk for zero benefit.
- **No server dependency for MVP.** Static hosting only; no telemetry backend, no remote persistence.
- **No separate product fork.** No browser-only apps, no browser-only behaviors beyond the declared backend divergences; the anti-fork rules of the parity spec apply to the browser target.
- **No pthreads / no SharedArrayBuffer at MVP** (see Q4, and §C.9).
- **No mobile-first commitment.** Touch support is required to *work*; iOS Safari polish is tier-2.

---

## H. Success criteria

- **MVP web demo (gate G1):** builds reproducibly in CI; boots to launcher < 3 s on a mid-range laptop over broadband; Calculation `1+2=3` via physical keyboard; deterministic scripted `launcher_smoke`-equivalent passes headless in CI; total transfer ≤ 6 MB compressed (budgets in companion doc 4 §G); zero console errors on happy path.
- **Parity (gate G2):** all 8 enabled apps open/exit cleanly; the shared `.numos` semantic suites (calc semantic 8C set, input 9A, menu 9B, grapher 9F/10 assert suites) pass in browser CI with identical exit codes; browser self-diff byte-identical for all candidate stems; SDL-vs-WASM pixel drift measured and published per stem.
- **Testability (gate G3):** browser test harness produces PPMs consumed by the unmodified `compare-ppm.py`; failing runs upload the same artifact classes CI uploads today (logs + PPMs + diffs).
- **Deployment (gate G4):** versioned static deploy with commit hash visible in the UI; old versions rollback-able by re-pointing; cache-safe (hashed assets); PR preview artifacts.
- **Classroom/demo (gate G5):** virtual keypad usable mouse-only and touch; reset button restores factory state; help panel lists key bindings; works offline after first load (PWA).
- **Long-term (gate G6):** golden parity decision executed (shared vs per-host goldens per stem); persistence roundtrip tests (FIX-04 analogue) green in browser; app enablement waves (P-05) flow into the browser build with no browser-specific work beyond recompile + smoke.

---

## I. Open decisions

| ID | Decision | Options | Consequences | Default until decided |
|---|---|---|---|---|
| WASM-OD-1 | Golden sharing | (a) separate `tests/emulator/golden-wasm/`; (b) shared goldens where byte-equal, else per-host; (c) always shared, force `-ffp-contract=off` + libm pinning | (a) doubles blessing work; (b) needs per-stem bookkeeping; (c) may be unattainable (libm differs by design) | (b), measured by WASM-TEST-03 |
| WASM-OD-2 | Emscripten-SDL vs canvas-native browser backend timing | keep Emscripten-SDL permanently vs swap at Phase 3 | keeping: +size, SDL input quirks; swapping: extra backend to maintain until SDL one is deleted | swap at Phase 3, delete Emscripten-SDL backend after parity re-run |
| WASM-OD-3 | Exceptions strategy | `-fwasm-exceptions` (native EH, needs recent browsers) vs Emscripten JS exceptions (slow, bigger) vs `-fno-exceptions` (must verify no throw paths in compiled set) | affects size/perf/browser floor; the emulator whitelist may be exception-free in practice (Giac, the exceptions consumer, is excluded) | audit in Phase 0 (WASM-BUILD-01); provisional `-fwasm-exceptions` |
| WASM-OD-4 | Persistence backend | IDBFS (Emscripten-standard, async sync) vs OPFS (faster, worker plumbing) vs File System Access API (Chromium-only) | IDBFS is lowest-effort and portable; OPFS better long-term | IDBFS; revisit post-MVP |
| WASM-OD-5 | Hosting | GitHub Pages vs Netlify/Vercel | Pages: free, no custom headers (blocks COOP/COEP → cements no-pthreads); Netlify/Vercel: headers + previews | GitHub Pages (consistent with no-pthreads) |
| WASM-OD-6 | Where the browser build's source list lives | parse `platformio.ini` `build_src_filter` at build time vs a checked-in `emulator_sources.txt` consumed by both | parsing: zero drift, fragile parser; shared list: needs a CI check that platformio.ini matches | generate-and-verify: CMake list checked against platformio.ini by a CI script (WASM-BUILD-02) |
| WASM-OD-7 | `NUMOS_WASM` define name & guard style | new `-DNUMOS_WASM` alongside `NATIVE_SIM` vs rely on `__EMSCRIPTEN__` | explicit define is greppable and mirrors `NATIVE_SIM` precedent | both: `NATIVE_SIM` stays on (browser IS a native sim), `__EMSCRIPTEN__` used only inside PlatformWeb |
| WASM-OD-8 | Spanish-language log strings in shared core | keep byte-identical (CI greps depend on `"Launcher cargado"`, [emulator-build.yml:141](/.github/workflows/emulator-build.yml#L141)) vs translate | translating breaks CI greps and script tooling | keep byte-identical; never localize harness logs |
| WASM-OD-9 | NumOS Lab integration surface | plain `<iframe>` embed vs importable ES-module component | iframe: trivial, isolated; component: richer integration | design shell as ES module now, embed via iframe first |

---

## J. Prerequisite specs this document builds on

`NUMOS_EMULATOR_DETERMINISM_AND_FIXTURE_HYGIENE_SPEC.md` (EMUDET), `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` (FIX), `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (CI), `NUMOS_EMULATOR_HOST_PORTABILITY_SPEC.md` (PORT), `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md`, the app-enablement pack (APPPAR/ROUTE/APPQA/STORE/LIFE + per-app matrix), `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GS), `NUMOS_FABLE_CONTEXT_HEADER.md`, `NUMOS_ARCHITECTURE_GROUND_TRUTH.md`, `NUMOS_RISK_REGISTER.md`.

Dependency stance **[proposed]**: the WASM port *wants* FIX-01 (settable FS root), EMUDET-01 (virtual clock), EMUDET-02/03/04 to land first, but only **hard-requires** none of them for Phase 0–1 (first boot uses MEMFS-fresh state, which incidentally satisfies fixture hygiene). Phase 2+ (asserted parity) hard-requires EMUDET-02 semantics (reject wall-clock flags) implemented at least in the browser harness, and Phase 4 (persistence) hard-requires the FS-root seam (FIX-01 or its WASM-FS-01 equivalent implemented at the platform boundary). See roadmap doc §Phase-0.
