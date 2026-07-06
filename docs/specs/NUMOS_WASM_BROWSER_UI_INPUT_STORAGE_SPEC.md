# NumOS WASM — Browser UI, Input, and Storage Specification

Status: SPEC (no implementation). Part 3 of the WASM port pack.
Convention: **[verified]** = read from source at `main` @ `ad536a7`; everything else in this document is **[proposed]** product/design behavior (this doc is mostly design by nature; current-state anchors are cited inline).

---

## A. Web demo UX

**Page layout.** Single-page shell, three regions:
1. **Screen** — the 320×240 canvas, centered, dark bezel frame styled like the device.
2. **Keypad** — on-screen virtual keypad below/beside the screen (collapsible on desktop, always-on for touch).
3. **Status strip** — commit hash + version label, focus indicator ("keyboard captured / click to type"), buttons: Reset, Screenshot, Help, Fullscreen, (if persistence enabled) Storage menu.

**Canvas size/scaling.** Backing canvas is always exactly 320×240 (the logical geometry is frozen; [NativeHal.cpp:147-148](src/hal/NativeHal.cpp#L147-L148) **[verified]** — "NUNCA cambian"). Display scaling is pure CSS: integer scale chosen as `floor(min(availW/320, availH/240))` clamped ≥1, `image-rendering: pixelated`, mirroring the desktop emulator's logical-size + integer-scale policy ([NativeHal.cpp:2169-2170](src/hal/NativeHal.cpp#L2169-L2170) **[verified]**). Non-integer responsive scaling is allowed only below the 1× floor (tiny phones), accepting blur there. `devicePixelRatio` is compensated so 1 emulator pixel = N device pixels exactly.

**Pixel-perfect vs responsive:** pixel-perfect wins whenever ≥320×240 CSS px are available; responsive shrink-to-fit is the fallback, never the default.

**Keyboard focus.** Canvas has `tabindex="0"`; clicking the screen or keypad focuses it; a visible focus ring + the status-strip indicator always tells the user whether keys are captured. While focused, `keydown` handlers `preventDefault()` only for keys the emulator consumes (so browser shortcuts like Ctrl+L still work); Tab is consumed (it is ALPHA on desktop SDL, [NativeHal.cpp:341](src/hal/NativeHal.cpp#L341) **[verified]**) only when focused, with Escape+Tab documented as the focus-escape path for keyboard-only users (accessibility, §B).

**On-screen key overlay.** Pressing and holding `?` (or the Help button) overlays the physical-key map on the keypad (the SDL letter shortcuts: s=SIN, c=COS, t=TAN, l=LN, m=LOG, b=log_n, r=√, p=^, o=π, e=e, x, y, a=Ans, f=fraction, n=NEGATE, g=Graph, h=Home — the table at [NativeHal.cpp:36-65](src/hal/NativeHal.cpp#L36-L65) **[verified]** is the single source; the shell renders it from a generated JSON, never a second hand-written copy).

**Mobile/touch.** Keypad-only input; canvas taps do nothing at MVP (no touchscreen semantics exist on device). Layout stacks vertically; keypad sized to thumb reach; `touch-action: manipulation` to kill double-tap zoom.

**Fullscreen.** Fullscreen API on the shell container (screen + keypad), same integer-scaling rule.

**Help/shortcut panel.** Modal listing: key map, script/demo links, "what is NumOS" blurb, browser-support note, privacy note (§G).

**Reset button.** Two-stage: "Reset calculator" (reload module with fresh sandbox FS — always available) and, only when persistence is on, "Erase saved data" (clears the IDBFS store; confirm dialog; §C).

**Shareable links (future).** Reserved URL-param namespace `#s=` for a future serialized-state or script link; MVP ships only `?app=<name>` (open a launcher app by the same canonical names as `open_app` — Calculation|Grapher|Statistics|Probability|Sequences|Regression|Settings|MathShowcase, [NativeHal.cpp:1437-1449](src/hal/NativeHal.cpp#L1437-L1449) **[verified]**) and `?det=1&script=<url>` for the test/demo harness. URL params are the browser's argv (host-portability spec: CLI surface must stay reachable; mapping table in companion doc 4 §C).

**Loading/error UI.** Deterministic sequence: splash placeholder (pure HTML/CSS) → progress (wasm fetch) → canvas appears on first presented frame. Failures show a diagnostic card (HTTP status / abort reason / commit hash / "copy report" button) — never a blank page (failure model, companion doc 2 §H).

---

## B. Input model

**Design rule.** The browser translates DOM events into the **existing script key-name vocabulary** and injects them through `numos_inject_key` → `scriptNameToKeyCode` ([NativeHal.cpp:456-565](src/hal/NativeHal.cpp#L456-L565) **[verified]**). No parallel keycode table exists in JS. This makes the browser input path provably a subset of the already-CI-tested grammar.

**Physical keyboard → NumOS keys.** Two-tier translation mirroring the desktop split ([NativeHal.cpp:344-349](src/hal/NativeHal.cpp#L344-L349) **[verified]**: keysyms for navigation/control/letters, TEXTINPUT for printable characters):
- `keydown` with **non-printable** `e.key` (`ArrowLeft/Right/Up/Down`, `Enter`, `Escape`→AC, `Backspace/Delete`→DEL, `Home`→MODE, `Shift`→SHIFT, `Tab`→ALPHA, `Insert`→STO, `F5`→S⇔D) → inject named keys with PRESS/REPEAT (from `e.repeat`)/RELEASE on `keyup`.
- `keydown` with **printable single-char** `e.key` — the DOM already applies layout/Shift/AltGr, exactly like `SDL_TEXTINPUT`: digits, `+ - * / ^ = ( ) . < >` → the symbol names; letters → the letter-shortcut names (s→`sin`, etc.). Characters outside the map are dropped with a console note (parity with [NativeHal.cpp:813-823](src/hal/NativeHal.cpp#L813-L823) **[verified]**).
- Dead keys/IME (`e.key === "Dead"`, `isComposing`) are ignored; NumOS consumes no composed text.

**Virtual keypad.** Buttons laid out like the device keypad; each button carries a `data-key="<script name>"`; pointerdown → PRESS, pointerup/pointerleave → RELEASE; long-press → REPEAT at the same cadence class the OS would give (initial 400 ms, then 80 ms — tunable; REPEAT is a distinct action apps rely on, [NativeHal.cpp:826-834](src/hal/NativeHal.cpp#L826-L834) **[verified]**). SHIFT/ALPHA buttons are plain keys — the modifier state machine lives in `KeyboardManager` C++ ([KeyboardManager.cpp:42-92](src/input/KeyboardManager.cpp#L42-L92) **[verified]**) and the keypad merely reflects it (see indicator note below).

**Modifier display.** The keypad SHIFT/ALPHA buttons show a "latched" style when the calculator's modifier state is active. Exposure **[proposed]**: read-only export `numos_modifier_state()` returning the `indicatorText()` token ([KeyboardManager.cpp:136-146](src/input/KeyboardManager.cpp#L136-L146) **[verified]**) — a NATIVE_SIM-style read-only debug accessor, consistent with the hook rules of the app-QA plan (read-only, no mutation surface).

**Touch/pointer.** Pointer Events only (covers mouse+touch+pen); no synthetic mouse fallbacks. Multi-touch: each pointer id may hold one key (chords allowed, e.g. SHIFT+key).

**Mobile keyboard suppression.** No `<input>`/`<textarea>` anywhere in the shell; the canvas+buttons never summon the soft keyboard.

**Repeat behavior.** Hardware keyboard: trust `e.repeat` (OS cadence, same as SDL). Virtual keypad: shell-generated cadence above.

**Shift/Alpha behavior.** Identical to desktop: they are ordinary KeyCodes consumed by the C++ state machine; the shell adds zero modifier logic. Browser Shift held while typing a symbol is naturally handled by the printable-char tier (layout-resolved), same as `SDL_TEXTINPUT` today.

**Text paste policy.** MVP: disabled. Future (behind a dev-tools toggle): paste decomposes into per-char injections through the same printable map, one char per frame (script-like pacing), rejecting any unmapped char with a visible warning. Never a hidden second input semantics.

**Scripted input policy.** During `--script`-equivalent runs, live input is discarded at the platform boundary (EMUDET-04 enforcement; companion doc 2 §I `pollInput`). The demo page's "run demo script" feature uses the same rule; a visible "demo running — press Stop to take over" banner is required.

**Accessibility basics.** Keypad buttons are real `<button>`s with labels (screen-reader friendly); focus order: screen → keypad rows; the canvas gets `role="img"` + `aria-label` "NumOS calculator screen" (full screen-reader access to screen contents is out of scope and honestly documented as such); all shell controls keyboard-reachable; prefers-reduced-motion respected by the shell (no bezel animations).

---

## C. Storage model

**Baseline facts [verified].** The emulator's only FS root is the hardcoded CWD-relative `./emulator_data` ([FileSystem.cpp:38](src/hal/FileSystem.cpp#L38)); the only live store is `/vars.dat` written by `VariableManager::saveToFlash()` ([VariableManager.cpp:170-186](src/math/VariableManager.cpp#L170-L186)) and loaded at boot ([NativeHal.cpp:2380-2388](src/hal/NativeHal.cpp#L2380-L2388)); the settings spec (GS-02/03) adds `/settings.dat` opt-in via `--persist-settings`, default off; the sandbox spec (FIX-01/02) defines the flag surface and the "deterministic runs start from a fresh sandbox" default.

**Browser mapping [proposed].** Three FS modes, selected at module init (`fsMode` in the init config; URL param `?fs=`):

| Mode | Mount | Lifetime | Default for |
|---|---|---|---|
| `sandbox` | MEMFS at the emulator root | one page load; lost on reload | deterministic/test runs, and the public demo's default |
| `persistent` | IDBFS at the emulator root | across reloads, per origin | user opt-in ("Remember my variables") |
| `fixture` | MEMFS, pre-populated by copying a fixture bundle (fetched JSON/base64 or supplied by the test harness) before boot | one page load | tests needing seeded state (`--fixture-dir` analogue) |

- **Tradeoffs considered.** IDBFS: standard Emscripten, async explicit `FS.syncfs`, portable — chosen. OPFS: faster/sync-in-worker but requires worker plumbing (and we forbid workers at MVP) — future (WASM-OD-4). `localStorage`: 5 MB string-only — rejected. File System Access API: Chromium-only, permission prompts — reserved for export/import convenience only (§E).
- **Sync points (persistent mode).** `FS.syncfs(false, cb)` after each frame in which any file was written (dirty flag from the `LittleFSClass` shim — one added hook), debounced to ≥1 s; plus on `visibilitychange→hidden` and `pagehide`. Writes are tiny (vars.dat ≈ dozens of bytes, header + 34-byte records [VariableManager.cpp:175-186](src/math/VariableManager.cpp#L175-L186) **[verified]**).
- **Settings persistence.** Follows GS-02/03 verbatim: `/settings.dat` with version+CRC+name-token record, written only when `persistSettings` is explicitly enabled; deterministic runs always boot compile-time defaults. The browser adds a third backend row to the GS parity register (NVS | file | IDBFS-file) — same bytes, different mount.
- **Variables persistence / app save files.** Whatever the compiled apps write under the root persists or not per mode; the STORE-01 rule (every FS-writing app declares its store before enablement) is enforced unchanged — the browser build must not enable NeuralLab/NeoLang (the "writes natively today" offenders in the enablement matrix) before the sandbox mode is proven.
- **Reset/clear data.** "Reset calculator" reloads with `sandbox`/fresh `persistent` state intact; "Erase saved data" unmounts + deletes the IndexedDB database, then reloads. Both accessible from the status strip; erase requires confirmation.
- **Export/import state.** Export: zip-less single-file dump — a versioned JSON envelope `{formatVersion, commit, files: {"/vars.dat": base64, ...}}` downloaded via Blob. Import: file picker → validate envelope → populate fixture mount → reload in `fixture` mode. This doubles as the bug-report state attachment.
- **No repo-dirtying equivalent.** The browser cannot write the repo — but the analogous hazard is **polluting the persistent store from test/demo runs**. Rule: test harness and demo scripts always run `sandbox`/`fixture`; `persistent` is never combined with script execution (config validation error, mirroring the exit-2 flag-conflict style, sandbox spec §B).
- **Privacy implications.** All state is origin-local (IndexedDB); nothing leaves the machine (§G). The privacy note in Help states this and documents Safari's ~7-day eviction of unused-site storage (data may vanish; export is the durable path).

---

## D. Deterministic fixtures in browser **[proposed]**

- **Clean session mode** = `sandbox`: fresh MEMFS each load; the deterministic default (satisfies EMUDET/FIX "declared FS state" with zero machinery).
- **Persistent session mode** = `persistent`: interactive only; excluded from all asserted runs.
- **Test fixture mode** = `fixture`: harness passes a fixture bundle; the mount is populated before `numos_init` returns; the fixture source is read-only (copy-on-boot, like `--fixture-dir`).
- **Two-process equivalent.** The sandbox spec's roundtrip (run A writes, run B reads) maps to **two module instantiations**: run A in `fixture-or-sandbox` mode with a post-run **state harvest** (harness reads the mount into an envelope via `numos_export_state` or direct `FS` reads), run B instantiated with that envelope as its fixture. Two full `createNumos()` instantiations = two processes; no shared JS state (fresh module, fresh memory). Persistent-mode roundtrip (reload-survival) is a separate browser-specific test: instantiate → write → sync → destroy page context → new context, same origin → assert (companion doc 5 §D).
- **State isolation between tests.** One module instance per test, never reused after exit/abort; `persistent` tests use a per-test unique IndexedDB name torn down after.
- **Artifact capture.** On failure the harness captures: exit code, screenshot PPMs, console log ring, and the state envelope — the browser analogue of CI-03/CI-04 failure bundles.

---

## E. Browser file APIs

- **File System Access API (future):** optional "open/save state file on disk" convenience on Chromium; never required; feature-detected.
- **Downloading screenshots/state:** Blob + `<a download>`; screenshots offered as PPM (canonical) and PNG (human); state as the JSON envelope (§C).
- **Uploading state files:** `<input type=file>` behind the Storage menu → import path (§C).
- **Drag/drop scripts (future, dev mode only):** dropping a `.numos` onto the canvas loads it through `numos_load_script` and runs in sandbox mode with the "demo running" banner; parse failure shows the same `[SCRIPT] file:line` diagnostics ([NativeHal.cpp:1397-1401](src/hal/NativeHal.cpp#L1397-L1401) **[verified]** style) in the dev console panel.

---

## F. Public deployment modes **[proposed]**

| Mode | Entry | FS | Input | Extras |
|---|---|---|---|---|
| **Demo** (default public page) | `/` | sandbox (opt-in persistent) | full | help, reset, screenshot |
| **Classroom** | `/?mode=class` | sandbox, persistence UI hidden | full | bigger default scale, keypad always visible, no dev tools |
| **Developer debug** | `/?mode=dev` | any | full + paste + drag-drop scripts | log panel, frame counter, det-mode toggle, state export, source-mapped build |
| **CI test** | harness-driven, no UI | sandbox/fixture | scripted only | batched frames, artifact capture (companion doc 5 §G) |
| **Embedded docs** | iframe embed `/?mode=embed&app=Grapher` | sandbox | full, keypad optional | minimal chrome, postMessage "ready" event |
| **Offline PWA** | installed app | persistent by default (explicit) | full | service worker, cache-first, update toast |

All modes are the same build; modes are shell configuration only — never compile-time forks (anti-fork rule).

---

## G. Security/privacy **[proposed]**

- **No network by default.** After asset load, the page performs zero network requests (demo mode). Script/fixture fetches happen only for explicit `?script=`/embed parameters and are same-origin-only by default.
- **No arbitrary filesystem access.** Only the Emscripten mounts; File System Access only via explicit user gesture (§E).
- **User data stays local.** No analytics, no cookies, no third-party requests at MVP (analytics policy: none until a deliberate decision; if ever added, it must be a separate consented build flag, and never in classroom/embed modes).
- **CSP.** `default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; img-src 'self' data: blob:; style-src 'self'; connect-src 'self'; frame-ancestors *` (embed allowed) — `'wasm-unsafe-eval'` is required for wasm compilation on some engines; no inline scripts (the shell is external files), no eval.
- **COOP/COEP:** not set (no pthreads/SAB); if OD-5 ever moves hosting to a header-capable host and pthreads become needed, this section must be revisited together.
- **Third-party dependency policy.** Runtime: zero third-party JS. Build/test-time: Emscripten SDK + Playwright only, version-pinned (companion doc 4).
- **Safe share links.** `#s=` state links (future) must embed only calculator state (validated envelope), never executable content; length-capped; parsed with strict validation.

---

## H. Browser support matrix **[proposed targets; to be verified by WASM-TEST cross-browser smoke]**

| Browser | Expected support | Risks / caveats |
|---|---|---|
| Chrome desktop (last 2) | Tier 1 — full | Baseline for CI (Playwright Chromium). |
| Edge desktop | Tier 1 — full (Chromium) | Same engine; test occasionally. |
| Firefox desktop | Tier 1 — full | Second CI engine; historically stricter about `putImageData` perf — fine at 320×240; watch wasm-exceptions support if OD-3 picks `-fwasm-exceptions` (supported in current releases). |
| Safari desktop | Tier 2 — expected working | Later wasm feature adoption (exceptions, memory limits); IndexedDB eviction (7-day ITP) — persistence caveat in Help; no CI runner at MVP (manual smoke per release). |
| Chrome Android | Tier 2 — working with keypad | Performance fine; keypad ergonomics primary risk; memory pressure on low-end devices — growth cap matters. |
| Safari iOS/iPadOS | Tier 2/3 — best effort | Strictest: wasm memory caps, ITP eviction, no install prompt (manual Add-to-Home-Screen), historical audio/rAF quirks (audio unused), keyboard events unavailable without external keyboard → keypad-only. Known highest-risk row; never a release blocker at MVP. |

Storage caveats: all Safari rows — eviction; all mobile — quota prompts unlikely at our sizes (<1 MB state). Input caveats: mobile rows keypad-only; Firefox `e.key` for dead keys differs (we ignore dead keys). Performance: none expected to matter at 320×240×60 blit (~0.3 MB/frame RGBA).

---

## I. Non-goals and limitations

- No screen-reader access to calculator screen contents (the framebuffer is pixels; documenting, not solving).
- No touchscreen gestures on the canvas (pan/zoom in Grapher stays key-driven, as on device).
- No multi-instance on one page at MVP (one module instance; file-static core state — companion doc 2 §E notes the `EmuCore` struct opens the door later).
- No cloud sync, accounts, or server-side anything.
- No localization of shell beyond English/Spanish static strings; harness log strings remain byte-frozen Spanish (WASM-OD-8).
- No guarantee of byte-identical pixels across browser *engines* until measured (companion doc 5 §J); the demo never claims it.
- Persistence is best-effort under browser eviction policies; export is the durability story.

---

## J. Explicit answers (UI/storage subset of the Special Questions)

- **Q8 (storage mapping):** `./emulator_data` root mounted per-mode (MEMFS sandbox default / IDBFS opt-in persistent / MEMFS fixture); `/vars.dat` + future `/settings.dat` unchanged file formats; GS/FIX flag semantics mirrored in init config (§C).
- **Q11 (focus changes):** explicit focus model with visible indicator; on blur, synthesize RELEASE for held keys but do not clear calculator modifier state (§A, companion doc 2 §C).
- **Q12 (mobile/touch):** on-screen keypad with pointer events, long-press repeat, no soft-keyboard summons (§B).
- **Q13 (which apps):** exactly the emulator-enabled set — same launcher, same dead-card refusals; `?app=` uses the `open_app` canonical names (§A).
- **Q15 (public deploy):** static hosting, modes-as-config (§F; hosting decision in companion doc 4 §E).
- **Q19 (NumOS Lab):** embed mode + ES-module shell is the integration surface (architecture doc WASM-OD-9).
