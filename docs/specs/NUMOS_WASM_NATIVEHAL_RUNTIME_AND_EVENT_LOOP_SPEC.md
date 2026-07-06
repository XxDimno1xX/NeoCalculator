# NumOS WASM — NativeHal Runtime and Event-Loop Specification

Status: SPEC (no implementation). Part 2 of the WASM port pack; architecture context in `NUMOS_WEBASSEMBLY_PORT_ARCHITECTURE_SPEC.md`.
Convention: **[verified]** = read from source at `main` @ `ad536a7`; **[proposed]** = design.

---

## A. Current NativeHal runtime model **[verified]**

**Main loop.** One blocking loop ([NativeHal.cpp:2254-2339](src/hal/NativeHal.cpp#L2254-L2339)) with a strict per-iteration order:
1. `scriptStepBegin()` — executes exactly one script command (or decrements a `wait`) *before* any input/tick work ([2258](src/hal/NativeHal.cpp#L2258), scheduler at [1803-1807](src/hal/NativeHal.cpp#L1803-L1807)).
2. `processSdlEvents()` — drains the SDL queue; `SDL_QUIT` sets `g_quit` ([2260](src/hal/NativeHal.cpp#L2260), [774-851](src/hal/NativeHal.cpp#L774-L851)).
3. Deferred Splash→Menu transition outside LVGL callbacks ([2263-2270](src/hal/NativeHal.cpp#L2263-L2270)).
4. Deterministic tick advance `g_detTick += stepMs` (only under `--deterministic`) ([2275-2277](src/hal/NativeHal.cpp#L2275-L2277)).
5. `lv_timer_handler()` — all LVGL rendering/animation/timers ([2279](src/hal/NativeHal.cpp#L2279)).
6. Deferred present: if the flush callback set `g_needsPresent`, clear/copy/present the SDL texture ([2282-2287](src/hal/NativeHal.cpp#L2282-L2287); the flush cb itself never presents, [286-313](src/hal/NativeHal.cpp#L286-L313)).
7. `scriptCaptureIfPending()` — deferred screenshot after render ([2291](src/hal/NativeHal.cpp#L2291), [2064-2080](src/hal/NativeHal.cpp#L2064-L2080)).
8. Deferred app teardown when `lv_tick_elaps(start) >= 260` ms ([2297-2301](src/hal/NativeHal.cpp#L2297-L2301)); same for splash teardown ([2306-2310](src/hal/NativeHal.cpp#L2306-L2310)).
9. `SDL_Delay(5)` — skipped entirely in deterministic mode ([2315-2317](src/hal/NativeHal.cpp#L2315-L2317)).
10. Auto-exit checks: `--frames` against `loopCount`, `--run-for-ms` against wall-clock `SDL_GetTicks()` ([2330-2338](src/hal/NativeHal.cpp#L2330-L2338)).

**SDL window/renderer/texture.** 320×240 logical size, integer scale, RGB565 streaming texture; accelerated renderer with VSYNC, software fallback ([2117-2170](src/hal/NativeHal.cpp#L2117-L2170)).

**Framebuffer.** Static `g_lvBuf[320*240*2]` in RGB565, registered as the single full-mode LVGL buffer ([271](src/hal/NativeHal.cpp#L271), [2218-2220](src/hal/NativeHal.cpp#L2218-L2220)). Always contains the current composed frame — the property every screenshot depends on.

**LVGL tick.** `lv_tick_set_cb(deterministic ? detTickCb : SDL_GetTicks)` ([2207](src/hal/NativeHal.cpp#L2207)); synthetic counter at [189-190](src/hal/NativeHal.cpp#L189-L190).

**Input polling.** Three sources converge on `dispatchKey(kc, action, isDown)` ([575](src/hal/NativeHal.cpp#L575)): SDL keysyms (navigation/letter shortcuts, [318-400](src/hal/NativeHal.cpp#L318-L400)), `SDL_TEXTINPUT` layout-resolved characters ([415-444](src/hal/NativeHal.cpp#L415-L444)), and script key names ([456-565](src/hal/NativeHal.cpp#L456-L565)).

**Script replay.** Whole-file validation at load (exit 2 before SDL init, [2091-2093](src/hal/NativeHal.cpp#L2091-L2093)); one command per frame; `screenshot` is captured after that frame's render; asserts read app debug state and set exit 4 + quit on failure ([1788-1794](src/hal/NativeHal.cpp#L1788-L1794)).

**Screenshot capture.** `saveScreenshotPPM(path)` — P6 header, RGB565→RGB888 bit-replication, from `g_lvBuf` only ([1292-1315](src/hal/NativeHal.cpp#L1292-L1315)). Also invoked once at exit for `--screenshot` ([2344-2349](src/hal/NativeHal.cpp#L2344-L2349)).

**App lifecycle.** `transitionToMenu()` constructs app objects (Calculation eagerly `begin()`s; others lazy) ([877-918](src/hal/NativeHal.cpp#L877-L918)); `launchApp(id)` routes {0,1,4,5,6,7,10,100} ([924-996](src/hal/NativeHal.cpp#L924-L996)); `returnToMenu()` schedules deferred teardown and resets `KeyboardManager` ([1065-1087](src/hal/NativeHal.cpp#L1065-L1087)).

**Deferred teardown.** 260 ms measured on `lv_tick` (deterministic-safe by construction) ([230-233](src/hal/NativeHal.cpp#L230-L233), [2297-2301](src/hal/NativeHal.cpp#L2297-L2301)).

**Deterministic mode.** Tick-only virtualization; no sleep; frame-indexed. Leaks: StatusBar `time()`, `--run-for-ms` wall clock, `millis()/delay()` shims ([ArduinoCompat.h:176-180](src/hal/ArduinoCompat.h#L176-L180)), unseeded `rand()` (per EMUDET spec §current-state).

**Process exit.** Cleanup ends all apps, `lv_deinit()`, SDL teardown, `return g_exitCode` ([2351-2373](src/hal/NativeHal.cpp#L2351-L2373)). Exit codes: 0 ok / 1 SDL init / 2 parse / 3 screenshot write / 4 assert — append-only contract.

---

## B. Emscripten/browser runtime model **[proposed]**

**Main-loop rewrite.** The loop body (steps 1–8 above) becomes a function `emuFrameStep()`. The `while` + sleep + auto-exit shell becomes browser-driven:

- **Interactive mode:** `emscripten_set_main_loop(frame_cb, 0 /* = use rAF */, /*simulate_infinite_loop=*/false)`. `fps=0` means one `emuFrameStep()` per `requestAnimationFrame`. `simulate_infinite_loop=false` + returning from `main()` avoids Asyncify/unwind tricks; all post-loop cleanup moves out of `main()` (see Failure/Exit model, §H).
- **Deterministic/test mode:** rAF pacing is *irrelevant to correctness* because state is a function of frame index, not time — but it is *slow* (a 1400-frame script at 60 Hz = 23 s wall). Test mode therefore runs **batched frames**: the driver calls an exported `numos_run_frames(n)` that executes up to `n` frame steps in a tight loop (with a time-slice guard, e.g. yield to the event loop every ~50 ms so the tab stays responsive and IDBFS callbacks can run). This is the browser analogue of "deterministic mode doesn't sleep" ([NativeHal.cpp:2315](src/hal/NativeHal.cpp#L2315)).

- **Asyncify: forbidden.** Nothing in the frame body blocks: `SDL_Delay` is deleted from the browser backend (pacing is rAF's job), the FS is synchronous (MEMFS; IDBFS syncs are explicit and outside the frame), and scripts are pre-loaded. Asyncify would add ~size, ~speed, and re-entrancy hazards for zero need. If a future feature genuinely needs blocking semantics, that feature is redesigned, not Asyncify'd. (Decision Q5.)
- **pthreads: forbidden for MVP.** The compiled set is single-threaded (FreeRTOS use is ARDUINO-only), and COOP/COEP is unavailable on GitHub Pages. `-sUSE_PTHREADS=0`. Revisit only if profiling proves a need (none expected at 320×240). (Decision Q4.)
- **LVGL tick.** Same selection logic: deterministic → `detTickCb`; interactive → the platform's `nowMillis()`, which the web backend implements as Emscripten's `emscripten_get_now()` (monotonic ms, same shape as `SDL_GetTicks`). The `lv_tick_get_cb_t` signature (`uint32_t(void)`) is unchanged.
- **Frame pacing.** Interactive: rAF (typically 60 Hz; on 120 Hz displays the emulator simply runs more frames — acceptable because interactive mode already has no fixed-rate contract on desktop either, where the cap is ~200 fps [NativeHal.cpp:155](src/hal/NativeHal.cpp#L155)). Deterministic: batched, unpaced.
- **`--run-for-ms` mapping.** Not mapped. EMUDET-02 already targets rejecting it under `--deterministic`; the browser harness supports only `frames`-bounded runs. Interactive browser sessions have no auto-exit at all (a page doesn't "exit"); `maxFrames` remains available for smoke automation.
- **Screenshots.** Same `saveScreenshotPPM` logic writing to a platform sink instead of `fopen` (§D); in the browser the sink is a wasm-memory byte vector retrievable from JS (`numos_last_screenshot_ptr/len`) and, for script-driven multi-shot runs, a MEMFS path table the harness reads out after completion.
- **Headless/CI mapping.** "Headless" in the browser means: no visible canvas requirement — the frame step and `g_lvBuf` are canvas-independent (present is a no-op if no canvas is attached), exactly mirroring how desktop headless works because capture reads the CPU buffer, not the renderer ([NativeHal.cpp:1287-1291](src/hal/NativeHal.cpp#L1287-L1291) comment block). CI runs Playwright-headless Chromium/Firefox; the emulator itself needs no special headless flag beyond "don't attach a canvas".

---

## C. Event-loop contract **[proposed]**

**One frame step** (normative, shared across all modes and backends):

```
frameStep():
  1. scriptStepBegin()                       # 0 or 1 script command
  2. platform.pollInput() → dispatchKey()*   # 0..N queued input events (empty during scripts unless allowLiveInput)
  3. deferred splash→menu transition
  4. if deterministic: detTick += stepMs
  5. lv_timer_handler()
  6. platform.presentFramebuffer(g_lvBuf, needsPresent)
  7. scriptCaptureIfPending()
  8. deferred teardowns (app, splash) on lv_tick
  9. frameIndex += 1
  10. if scriptDone or frameLimitReached: platform.requestExit(code)
```

Ordering is byte-identical to the desktop loop; the only changes are (a) sleep removed (pacing external), (b) exit is a callback, not a `while` condition.

- **Deterministic frame stepping:** the driver (JS test harness or `numos_run_frames`) calls `frameStep()` exactly `N` times. State after frame `k` must be independent of wall time, rAF cadence, batching size, and browser engine (modulo the drift classes in companion doc 5 §J).
- **Interactive frame stepping:** one `frameStep()` per rAF callback. Multiple rAFs never coalesce into multiple LVGL ticks beyond the real elapsed time because interactive mode uses the real clock — same as desktop.
- **Script frame stepping:** identical to deterministic; live input suppressed (EMUDET-04 semantics enforced in `pollInput`, not in each app).
- **requestAnimationFrame behavior:** rAF stops when the tab is hidden — see below; rAF timestamp is *never* fed into the emulator (the tick comes from `nowMillis()`/detTick only), so rAF jitter cannot enter emulator state.
- **Background tab:** interactive sessions simply pause (no rAF → no frames → LVGL animations freeze consistently, because the tick is read per frame). On return, `nowMillis()` will have jumped — LVGL handles large tick deltas by completing animations; this matches minimizing the desktop window. Deterministic batched runs must NOT use rAF (they'd starve in background tabs); `numos_run_frames` runs on `setTimeout(0)` slices, which throttle in background tabs but do not stop — acceptable for CI (headless browsers don't throttle) and documented as "don't background long local test runs".
- **pause/resume:** shell-level `numos_pause()` / `numos_resume()` simply stop/start scheduling `frameStep()`. No emulator state is touched; a paused emulator is byte-frozen.
- **visibilitychange:** interactive shell pauses on hidden (saves CPU/battery), resumes on visible; if persistence is enabled, an IDBFS sync is kicked on hidden (companion doc 3 §C).
- **focus/blur:** on canvas blur, the shell synthesizes RELEASE for any keys it is holding down and clears its own held-key set; it does NOT reset `KeyboardManager` modifiers (SHIFT/ALPHA state is calculator state, not host state — parity with desktop, where alt-tabbing does not clear modifiers). Documented divergence candidate if stuck-modifier reports appear.

---

## D. Backend abstraction **[proposed]**

Concrete interface (C++17, no virtual cost concerns at this scale):

```cpp
// src/hal/IEmulatorPlatform.h  [proposed]
struct EmuInputEvent {          // exactly what dispatchKey consumes
    KeyCode   code;
    KeyAction action;           // PRESS / REPEAT / RELEASE
};

class IEmulatorPlatform {
public:
    virtual ~IEmulatorPlatform() = default;

    // -- lifecycle --------------------------------------------------------
    virtual bool init(const EmuOptions& opts) = 0;      // window/canvas, indev host
    virtual void shutdown() = 0;
    virtual void requestExit(int exitCode) = 0;         // desktop: g_quit; web: onExit JS cb

    // -- per-frame --------------------------------------------------------
    // Drain host input into out; returns count. MUST be empty while a script
    // is active unless opts.allowLiveInput (EMUDET-04 enforcement point).
    virtual size_t pollInput(EmuInputEvent* out, size_t max) = 0;
    // Present the composed CPU framebuffer (RGB565, 320x240) if dirty.
    virtual void presentFramebuffer(const uint8_t* rgb565, bool dirty) = 0;

    // -- time (single seam; EMUDET-01 numosNow() lives behind this) --------
    virtual uint32_t nowMillis() = 0;                   // wall ms; NEVER read in deterministic mode

    // -- files (FIX-01 seam; roots resolved once at init) -------------------
    virtual bool readFile (const char* path, std::vector<uint8_t>& out) = 0;
    virtual bool writeFile(const char* path, const uint8_t* data, size_t len) = 0;
    virtual bool listFiles(const char* dir, std::vector<std::string>& out) = 0;

    // -- capture / diagnostics ---------------------------------------------
    // Sink for saveScreenshotPPM output; desktop writes path, web stores bytes.
    virtual bool storeScreenshot(const char* label, const uint8_t* ppm, size_t len) = 0;
    virtual void log(const char* tag, const char* msg) = 0;   // [SIM]/[SCRIPT]/[ASSERT] lines

    // -- mode ---------------------------------------------------------------
    virtual void setDeterministicMode(bool on, long stepMs) = 0;  // lets backend disable pacing/sleep
};
```

Notes:
- `pollInput` deals in `KeyCode` already — SDL→KeyCode and DOM→KeyCode mapping is backend-internal, but the **browser backend maps via the script key-name table** (see §F) so both backends' vocabularies stay provably aligned with the `.numos` grammar.
- The existing `LittleFSClass` shim keeps its API; its `fopen` calls are *not* rerouted through `readFile/writeFile` at MVP (Emscripten's MEMFS implements stdio natively). The interface FS methods exist for the shell/test harness (state export/import, script delivery) and for a future FIX-01 root-resolution implementation. This avoids touching `VariableManager` serialization (single-source parity rule, sandbox spec).
- `storeScreenshot` replaces the raw `fopen` in `saveScreenshotPPM` **at the sink level only** — the PPM bytes are produced by the same code, so format parity is by construction.

---

## E. Native SDL backend migration **[proposed]**

Split `NativeHal.cpp` without behavior change (WASM-ARCH-01/02):

1. `src/hal/EmuCore.{h,cpp}`: everything from `AppMode` through the script engine, asserts, `saveScreenshotPPM` (bytes-producing part), `dispatchKey`, launch/teardown, `frameStep()`. File-static globals become members of a single `EmuCore` struct (still one instance; no API change to apps).
2. `src/hal/PlatformSdl.cpp`: SDL init/teardown ([2108-2192](src/hal/NativeHal.cpp#L2108-L2192), [2367-2370](src/hal/NativeHal.cpp#L2367-L2370)), event pump ([774-851](src/hal/NativeHal.cpp#L774-L851)) reduced to producing `EmuInputEvent`s, flush-present pair, `SDL_GetTicks`, `SDL_Delay` pacing, file-writing screenshot sink.
3. `NativeHal.cpp` remains as `main()` + argv parsing + wiring (`EmuCore core(platform)` + `while (!exitRequested) { core.frameStep(); platform.pace(); }`).

Acceptance for the split (hard gate, from companion doc 5): full CI chain green; deterministic self-diff byte-identical; all 19 goldens byte-identical (no re-bless allowed — a re-bless means the refactor changed behavior); `--help` output unchanged; exit codes unchanged. Log strings must remain byte-identical (CI greps `Launcher cargado`, `tick = determinista` — [emulator-build.yml:141](/.github/workflows/emulator-build.yml#L141), [:173](/.github/workflows/emulator-build.yml#L173)).

---

## F. Browser backend **[proposed]**

- **JS/C++ boundary.** Minimal exported C ABI (`EMSCRIPTEN_KEEPALIVE`, listed in companion doc 4 §C): `numos_init(configJsonPtr)`, `numos_frame()`, `numos_run_frames(n)`, `numos_inject_key(namePtr, action)`, `numos_load_script(textPtr) -> int`, `numos_screenshot_ppm(&ptr,&len)`, `numos_exit_code()`, `numos_reset()`, `numos_fb_ptr()`, `numos_fb_dirty()`. JS→C++ strings via `allocateUTF8`; C++→JS via return pointers into linear memory. No Embind at MVP (size).
- **Canvas framebuffer upload.** `presentFramebuffer` sets a dirty flag + the shell reads `numos_fb_ptr()` as a `Uint16Array(HEAP, ptr, 320*240)` view, converts RGB565→RGBA8888 into a persistent `ImageData` (same bit-replication constants as [NativeHal.cpp:1303-1309](src/hal/NativeHal.cpp#L1303-L1309) so on-screen pixels match screenshot pixels), `putImageData` to the 320×240 backing canvas. Alternative (EM_JS pushing bytes) rejected: the pull model keeps all DOM code in JS.
- **Input queue.** JS listeners translate DOM events → script key names (§C of companion doc 3 has the full map) → `numos_inject_key("sin", PRESS)`. Inside, the name goes through the existing `scriptNameToKeyCode` ([NativeHal.cpp:456](src/hal/NativeHal.cpp#L456)) into a ring buffer drained by `pollInput`. Unknown names are logged and dropped (same as unknown SDL keys today, [813-823](src/hal/NativeHal.cpp#L813-L823)).
- **Timer source.** `nowMillis()` = `emscripten_get_now()` truncated to uint32 — used only in interactive mode.
- **Logging.** `log()` → `console.log`/`console.error` with the existing `[TAG]` prefixes preserved verbatim; the shell also appends to a bounded in-page ring buffer (crash reports, companion doc 3 §A).
- **Error reporting.** Module `onAbort`/`onExit` hooks → shell error overlay + structured `{phase, exitCode, lastLogLines, commitHash}` object; test harness reads the same object.
- **Screenshot API.** As in §B; plus shell helper `downloadScreenshot()` wrapping the PPM (or a PNG re-encode for humans — the PPM stays the canonical artifact).

---

## G. Memory model **[proposed, with verified anchors]**

- **Linear memory.** `-sALLOW_MEMORY_GROWTH=1`, `-sINITIAL_MEMORY=64MB`, `-sMAXIMUM_MEMORY=512MB`. Rationale: the desktop emulator runs comfortably in tens of MB (320×240×2 framebuffer = 150 KB [NativeHal.cpp:270-271](src/hal/NativeHal.cpp#L270-L271); fonts compiled-in; LVGL on libc malloc [platformio.ini:176](platformio.ini#L176)); growth is a safety valve, not a plan. Growth events invalidate JS heap views — the shell must re-acquire `HEAPU8`/`Uint16Array` views each frame rather than caching them (standard Emscripten discipline; cheap).
- **Stack.** `-sSTACK_SIZE=1MB` initial proposal. Evidence for caution: firmware needs a 64 KB loop stack ([platformio.ini:44](platformio.ini#L44)) and the Grapher/renderer recursion depth is nontrivial; Emscripten default (64 KB) is exactly the firmware floor with no margin. 1 MB is cheap in wasm. Verify with `-sSTACK_OVERFLOW_CHECK=2` in debug builds.
- **Heap.** libc `malloc` (dlmalloc default; `emmalloc` considered for size in release — decide by measurement, WASM-PERF-01).
- **PSRAM simulation.** None needed: the native build already routes `ps_malloc` → `malloc` ([ArduinoCompat.h:198-205](src/hal/ArduinoCompat.h#L198-L205)) and all `heap_caps` sites in compiled TUs are `#ifdef ARDUINO`-guarded (verified sweep in the HAL evidence: `MathAST.cpp:41-67`, `cas/ConsTable.h:45-48`, `SymExprArena.h:50-53`, `PSRAMAllocator.h:37-39`, all guarded). The MEM-5 caveat (emulator allocator ≠ hardware) applies to wasm identically and is inherited, not worsened.
- **LVGL heap.** Same `LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB` define must be set in the wasm build — omitting it reproduces the 64 KB-pool hang class ([platformio.ini:170-176](platformio.ini#L170-L176) documents the failure mode).
- **Fonts/assets.** STIX fonts are C arrays compiled into the binary ([platformio.ini:264-266](platformio.ini#L264-L266)) — they inflate the `.wasm` (size budget in companion doc 4 §G) but need no async loading and cannot rendering-drift. No `--preload-file` at MVP.
- **Giac/CAS.** Giac is excluded from native builds ([platformio.ini:192-194](platformio.ini#L192-L194)) and stays excluded in wasm. The in-tree `cas::` subset compiles (it's in the allowlist, [platformio.ini:252-258](platformio.ini#L252-L258)) with its `#else` std-malloc branches. If Giac-in-wasm is ever attempted it gets its own spec; its memory (tens of MB, exceptions, longjmp) is out of scope here.

---

## H. Failure model **[proposed]**

| Failure | Detection | Behavior |
|---|---|---|
| OOM (malloc null / memory.grow fail) | Emscripten `onAbort("OOM")` | Fatal overlay with "reload" + state note; test harness maps to exit-code-like `abort:oom`. Never silently continue. |
| Failed asset load (.wasm/.js fetch) | loader promise rejection | Shell shows retry UI with HTTP status; never a blank page. |
| Lost canvas context | `contextlost` event (2D contexts: rare) | Re-create canvas + force full present next frame; emulator state untouched (framebuffer is CPU-side — this is the payoff of CPU-buffer ownership). |
| Browser kills tab / navigation | `visibilitychange`/`pagehide` | If persistence on: best-effort IDBFS sync on `pagehide`. Data loss beyond that is accepted and documented. |
| Script timeout | harness-side: frame budget `N` exhausted with script not done | Harness fails the run (analogue of the 60 s `timeout` in CI today, [emulator-build.yml] per-script contract); captures screenshot + logs. |
| Infinite loop inside a frame | watchdog in shell: `frameStep()` wall-time > 2 s | Cannot preempt wasm; the shell can only detect after return — for true hangs the harness's outer Playwright timeout is the backstop. Document: same exposure as desktop (a hang hangs). |
| Uncaught C++ abort/assert (LVGL `LV_ASSERT`, `abort()`) | `onAbort` | Overlay with stack (symbolized in debug builds, `-gsource-map`), log ring, commit hash; harness records `abort`. |
| Assert failure (script `assert_*`) | existing path: exit code 4 via `requestExit(4)` | NOT an abort: clean stop, screenshot-on-assert-failure (CI-04 analogue) captured by harness. |

Exit-code contract in a tab: `requestExit(code)` stops scheduling frames and resolves a JS promise `Module.numosExited` with the code; the page stays alive (shell may show "run finished, exit N"). The 0/2/3/4 meanings are preserved verbatim; "1 = SDL init failure" generalizes to "1 = platform init failure".

---

## I. Pseudocode **[proposed]**

**Browser main loop (interactive):**
```js
const mod = await createNumos({ canvas });          // MODULARIZE factory
mod._numos_init(cfgPtr);                            // parses config → EmuOptions
function tick() {
  if (!paused && !exited) {
    mod._numos_frame();
    if (mod._numos_fb_dirty()) blitFramebuffer(mod);
    raf = requestAnimationFrame(tick);
  }
}
raf = requestAnimationFrame(tick);
```

**Deterministic script runner (test mode):**
```js
const mod = await createNumos({});                  // no canvas needed
mod._numos_init(cfg({ deterministic: true, stepMs: 16, frames: 1400 }));
if (mod._numos_load_script(str(scriptText)) !== 0) fail(2);   // fail-fast parse, exit 2 parity
while (!mod._numos_exited()) {
  mod._numos_run_frames(512);                       // batched; yields between batches
  await microYield();                               // keep event loop alive
}
const code = mod._numos_exit_code();                // 0 | 3 | 4
const shots = readScreenshots(mod);                 // label → PPM bytes (byte-identical format)
```

**Frame step (C++, shared core — normative order):**
```cpp
void EmuCore::frameStep() {
    scriptStepBegin();
    EmuInputEvent ev[16];
    size_t n = _platform.pollInput(ev, 16);
    for (size_t i = 0; i < n; ++i) dispatchKey(ev[i].code, ev[i].action, ev[i].action != KeyAction::RELEASE);
    maybeTransitionSplashToMenu();
    if (_opts.deterministic) _detTick += _opts.stepMs;
    lv_timer_handler();
    _platform.presentFramebuffer(_lvBuf, std::exchange(_needsPresent, false));
    scriptCaptureIfPending();          // -> buildPpm() -> _platform.storeScreenshot(label, bytes, len)
    serviceDeferredTeardowns();        // lv_tick-based, unchanged
    ++_frameIndex;
    if (frameLimitReached() || _quit) _platform.requestExit(_exitCode);
}
```

**Screenshot capture (shared bytes, per-backend sink):**
```cpp
// Byte-for-byte the algorithm of NativeHal.cpp:1292-1315, minus fopen:
std::vector<uint8_t> EmuCore::buildPpm() const;                // P6 header + RGB888
// SDL backend: storeScreenshot -> fwrite(path)   (path = label)
// Web backend: storeScreenshot -> _shots[label] = bytes; also latest-shot slot for numos_screenshot_ppm()
```

**Platform read/write (web backend):**
```cpp
bool WebPlatform::writeFile(const char* p, const uint8_t* d, size_t n) {
    // MEMFS/IDBFS via stdio; root prefix resolved at init (fsMode):
    //   sandbox   -> /numos/run-<nonce>/emulator_data   (MEMFS, fresh)
    //   persistent-> /numos/persist/emulator_data       (IDBFS mount, explicit sync)
    //   fixture   -> /numos/fixture copied into a fresh sandbox at init (read-only source)
    ...
}
```

**Input enqueue/dequeue (web backend):**
```cpp
extern "C" EMSCRIPTEN_KEEPALIVE
void numos_inject_key(const char* name, int action /*0=press 1=release 2=repeat*/) {
    KeyCode kc = scriptNameToKeyCode(name);          // reuse the frozen table
    if (kc == KeyCode::NONE) { logUnknown(name); return; }
    g_webQueue.push({kc, toKeyAction(action)});      // fixed-size ring, drop+log on overflow
}
size_t WebPlatform::pollInput(EmuInputEvent* out, size_t max) {
    if (core().scriptActive() && !_opts.allowLiveInput) { g_webQueue.clear(); return 0; }  // EMUDET-04
    return g_webQueue.drain(out, max);
}
```

---

## J. Explicit answers (runtime subset of the Special Questions)

- **Q1 (SDL vs new backend):** both, staged — Emscripten SDL2 behind `IEmulatorPlatform` for first boot, canvas-native backend at Phase 3 (architecture doc §D).
- **Q2 (split NativeHal):** yes — WASM-ARCH-01/02, gated on byte-identical goldens (§E).
- **Q4 (pthreads):** forbidden at MVP (§B).
- **Q5 (Asyncify):** forbidden (§B).
- **Q9 (deterministic time):** unchanged synthetic tick + `numosNow()` seam behind `IEmulatorPlatform::nowMillis`; browser clocks never reach deterministic state (§B, §D).
- **Q10 (script replay):** same C++ engine; scripts delivered as strings from JS; one command per frame preserved; batched execution for speed (§B, §I).
- **Q14 (crash surfacing):** `onAbort`/`requestExit` split — aborts get an overlay + structured report; assert failures remain clean exit-4 semantics (§H).
