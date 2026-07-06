# NumOS WASM — Build, Deploy, and CI Specification

Status: SPEC (no implementation). Part 4 of the WASM port pack.
Convention: **[verified]** = read from source at `main` @ `ad536a7`; **[proposed]** = design.

---

## A. Current build system **[verified]**

- **PlatformIO envs.** Two: `esp32s3_n16r8` (Arduino/espressif32, [platformio.ini:9-12](platformio.ini#L9-L12)) and `emulator_pc` (`platform = native`, [platformio.ini:153-154](platformio.ini#L153-L154)). No `make`/`npm`/`cmake` anywhere in the build path today.
- **emulator_pc flags.** `-DNATIVE_SIM -D_USE_MATH_DEFINES -std=c++17 -DLV_CONF_INCLUDE_SIMPLE -I src -DLV_USE_DRAW_SW_ASM=0 … -DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB` ([platformio.ini:160-176](platformio.ini#L160-L176)). `lib_deps = lvgl/lvgl@^9.2.0` only ([177-178](platformio.ini#L177-L178)); `lib_ignore = giac, libtommath` ([192-194](platformio.ini#L192-L194)).
- **SDL2 discovery.** `extra_scripts = pre:scripts/sdl2_env.py` ([platformio.ini:159](platformio.ini#L159)); Windows env-var chain `NUMOS_SDL2_ROOT → SDL2_DIR → SDL2_ROOT → C:/SDL2/x86_64-w64-mingw32` ([sdl2_env.py:63-68](scripts/sdl2_env.py#L63-L68)); Unix `pkg-config → sdl2-config → -lSDL2` ([sdl2_env.py:140-162](scripts/sdl2_env.py#L140-L162)).
- **CI build flow.** `emulator-build.yml`: single `ubuntu-latest` job, `PLATFORMIO_BUILD_DIR=.pio/build` ([emulator-build.yml:54-55](/.github/workflows/emulator-build.yml#L54-L55)), apt SDL2, `pio run -e emulator_pc`, then the smoke/script/golden chain (mapped in architecture doc §B).
- **build_dir quirk.** `[platformio] build_dir = C:/.piobuild/numOS` ([platformio.ini:6](platformio.ini#L6)) — Windows MAX_PATH workaround; CI and Linux scripts override with `PLATFORMIO_BUILD_DIR`; `compile-and-release.yml` even `sed`s the line out ([compile-and-release.yml:29-39](/.github/workflows/compile-and-release.yml#L29-L39)).
- **Source filters.** `emulator_pc` compiles an explicit allowlist (`build_src_filter`, [platformio.ini:195-266](platformio.ini#L195-L266)): `hal/*`, 8 apps + engines, RPN math pipeline, selected `math/cas/*`, ui, three STIX font TUs. Firmware compiles everything (`+<*>`, [platformio.ini:102-103](platformio.ini#L102-L103)).
- **Native-only macros.** `NATIVE_SIM` gates NativeHal ([NativeHal.cpp:85](src/hal/NativeHal.cpp#L85)); app sources use `#ifdef ARDUINO`/`#else` with `hal/ArduinoCompat.h` fallbacks.
- **Firmware env contrasts.** `-DDOUBLEVAL`, `-DBOARD_HAS_PSRAM`, `-DARDUINO_LOOP_STACK_SIZE=65536`, `-fexceptions`, Giac defines ([platformio.ini:40-92](platformio.ini#L40-L92)) — none of these apply to native or wasm builds.

---

## B. WASM build options **[proposed analysis]**

1. **PlatformIO custom env** (`platform = native` + emcc as compiler). PlatformIO has no first-class Emscripten platform; forcing `CC=emcc` through `native` fights the toolchain wrapper (linker flags, `.wasm`+`.js` dual outputs, `-sXXX` settings, ports system). High friction, ongoing breakage risk. **Rejected as primary.**
2. **Standalone CMake + Emscripten toolchain** (`emcmake cmake … && cmake --build`). Idiomatic Emscripten; clean place for `-sXXX` link settings, debug/release configs, and the JS shell packaging; testable locally and in CI with `mymindstorm/setup-emsdk`. Cost: a second build description that could drift from `build_src_filter`. **Recommended (MVP and long-term)** with drift neutralized per WASM-OD-6: a small script `scripts/emit-emulator-sources.py` **[proposed]** parses `platformio.ini`'s `build_src_filter` allowlist and emits/verifies `web/emulator_sources.cmake`; CI fails if regeneration differs (same pattern as the KeyCode digit guard step, [emulator-build.yml:96-104](/.github/workflows/emulator-build.yml#L96-L104)).
3. **Makefile/script wrapping emcc.** Workable but reinvents CMake's config/dependency handling; worse IDE/debug ergonomics. Rejected.
4. **Hybrid via compile database.** Export `compile_commands.json` from PlatformIO and replay with emcc — fragile (native flags leak through, e.g. `-msse` from LVGL asm settings). Rejected.
5. **PlatformIO custom platform (write a `platform-emscripten`).** Highest effort, benefits only if many Emscripten targets appear. Rejected for now; revisit only if the CMake island becomes a maintenance sore.

**Decision (Q3): PlatformIO does NOT own the WASM build.** CMake+emsdk owns it; PlatformIO remains the single owner of firmware + desktop emulator; the source list is generated from platformio.ini to keep one authority for *what* compiles.

---

## C. Emscripten configuration **[proposed]**

Recommended release settings (link step):

```
emcc <objects> -o numos-emu.js
  -sWASM=1
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createNumos
  -sENVIRONMENT=web                       # no node shims in the shipped artifact
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sMAXIMUM_MEMORY=536870912
  -sSTACK_SIZE=1048576
  -sUSE_PTHREADS=0                        # Q4: forbidden
  # no -sASYNCIFY                         # Q5: forbidden
  -sFILESYSTEM=1 -lidbfs.js               # MEMFS default + IDBFS for persistent mode
  -sEXPORTED_FUNCTIONS=_numos_init,_numos_frame,_numos_run_frames,_numos_inject_key,\
_numos_load_script,_numos_screenshot_ppm,_numos_exit_code,_numos_reset,_numos_fb_ptr,\
_numos_fb_dirty,_numos_export_state,_numos_modifier_state,_main
  -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,HEAPU8,HEAPU16,FS
  -O2 -flto                               # -Oz evaluated by size measurement (WASM-PERF-01)
  --closure 0                             # closure only if shell stays closure-safe; measure first
```

Compile flags (all TUs): `-std=c++17 -DNATIVE_SIM -D_USE_MATH_DEFINES -DLV_CONF_INCLUDE_SIMPLE -DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB -Isrc` — byte-matching the emulator_pc defines ([platformio.ini:160-176](platformio.ini#L160-L176)) so the preprocessed sources are identical to the desktop emulator's. Determinism-relevant addition: `-ffp-contract=off` from day one on the wasm target (removes one drift class preemptively; PORT-05/CIART identify FP contraction as the cross-host pixel-drift mechanism).

- **SDL2 flag (Phase 1 only):** `-sUSE_SDL=2` while the browser backend is Emscripten-SDL; dropped when the canvas-native backend lands (architecture doc WASM-OD-2), which also drops its JS shim weight.
- **Exception handling (WASM-OD-3):** Phase 0 audits `throw`/`try` in the compiled allowlist (Giac, the known exceptions consumer, is excluded [platformio.ini:192-194](platformio.ini#L192-L194); LVGL is C). If the set is exception-free → `-fno-exceptions` (smallest); else `-fwasm-exceptions` (modern engines) with `-sSUPPORT_LONGJMP=wasm` alignment. Emscripten JS-exceptions mode is the fallback only if very old Safari must be supported.
- **Assertions/debug vs release:** debug config adds `-O1 -g -gsource-map -sASSERTIONS=2 -sSTACK_OVERFLOW_CHECK=2 -sSAFE_HEAP=1 -fsanitize=undefined` (UBSan-in-wasm for local runs); release strips all.
- **Preloaded assets:** none (fonts are compiled C arrays, [platformio.ini:264-266](platformio.ini#L264-L266)).
- **LVGL:** compiled from the same `lvgl` source tree/version as the desktop build; the exact-version pin (CI-06) becomes a hard prerequisite for any golden-parity claim — the wasm build must record the LVGL version in build metadata (§D).

---

## D. Build artifacts **[proposed]**

| Artifact | Notes |
|---|---|
| `numos-emu.wasm` | content-hashed filename in deploy (`numos-emu.<hash>.wasm`) |
| `numos-emu.js` | ES6 module glue, content-hashed |
| worker files | none (no pthreads) |
| data package | none at MVP |
| `index.html` | shell entry; references hashed assets; contains inline **no** scripts (CSP) |
| `numos-shell.js`, `shell.css` | hand-written shell, hashed |
| `keymap.json` | generated from the C++ key-name table (single-source rule, companion doc 3 §A) |
| `manifest.webmanifest`, `sw.js` | PWA phase only |
| `*.map` | source maps: uploaded as CI artifacts always; deployed only in dev-mode builds |
| `build-info.json` | `{commit, branch, date?, emsdkVersion, lvglVersion, flags, mode}` — `date` omitted in deterministic-build mode (reproducibility, §I); shell displays `commit`; test harness asserts it matches the checkout |

---

## E. Deployment **[proposed]**

- **Primary: GitHub Pages** (decision WASM-OD-5), deployed by CI from `main` via `actions/deploy-pages`. Consequence accepted: no custom headers → no COOP/COEP → pthreads stay impossible on this host (consistent with Q4); MIME for `.wasm` is correct on Pages.
- **Layout:** `https://<user>.github.io/NeoCalculator/` serving `latest/` plus versioned `v/<short-sha>/` directories; `latest/index.html` is the only mutable object; all hashed assets `Cache-Control: immutable`-equivalent (Pages sets long max-age for hashed names by convention; the hash in the filename is what actually guarantees correctness).
- **Dev/staging:** PR builds are **artifacts, not deployments** (a zip of the site directory downloadable from the Actions run + a bot comment with size numbers). If preview URLs become important, move previews (only) to Netlify — revisit OD-5 then.
- **Own website / Vercel/Netlify:** compatible by construction (static dir); nothing in the build may assume the Pages URL (all asset paths relative).
- **Rollback:** re-point `latest/` to a previous `v/<sha>/` by re-running the deploy job at that commit; versioned dirs are never deleted within a retention window (last ~20 builds).

---

## F. CI **[proposed]**

New workflow `wasm-build.yml`, additive, never touching `emulator-build.yml` (whose job/greps are load-bearing **[verified]**, e.g. [emulator-build.yml:141](/.github/workflows/emulator-build.yml#L141)):

1. **build**: checkout → `setup-emsdk` (pinned emsdk version) → verify `web/emulator_sources.cmake` regeneration is clean → CMake release build → upload site artifact + `build-info.json` + maps.
2. **smoke (headless browser)**: Playwright (pinned) Chromium + Firefox: load page from a local static server, wait for `numosReady`, assert first presented frame non-blank, assert zero console errors; deterministic boot script (launcher smoke) via the harness → exit 0.
3. **script/parity suite**: run the shared `.numos` semantic suites (calc semantic set, input 9A, menu 9B, grapher assert suites — same scripts from `tests/emulator/scripts/`), assert exit codes + `PASS -`/no-`FAIL -` grammar, matching the per-script contract of the desktop job.
4. **screenshot/golden compare**: harness extracts PPMs (byte-identical format) → `python scripts/compare-ppm.py` against **WASM goldens** (`tests/emulator/golden-wasm/` **[proposed]**, lifecycle per companion doc 5 §E) with the same mask files; plus the **self-diff** step (each candidate twice, SHA-256 equal) mirroring [emulator-build.yml:221-236](/.github/workflows/emulator-build.yml#L221-L236).
5. **parity measurement (non-gating)**: compare WASM candidates against the **SDL Linux goldens**, publish a per-stem drift report artifact (identical / masked-identical / N pixels) — the WASM-OD-1 evidence stream.
6. **budgets**: fail if compressed `wasm+js+shell` exceeds budget (§G); warn at 90%.
7. **artifacts**: on failure upload failing PPMs, diff PNGs (from `compare-ppm.py --diff`), console logs, state envelopes — CI-03/CI-04 artifact classes.
8. **dirty-tree guard**: `git diff --exit-code` + scoped untracked check at job end (adopting CI-01 for this workflow from day one, since the generator script writes into the tree only under `web/` regeneration checks).

Deploy job (separate, `main` only, after all gates): build → deploy Pages → post-deploy smoke against the live URL.

---

## G. Size/performance budgets **[proposed initial — recalibrate after first real build]**

| Budget | Target | Hard fail |
|---|---|---|
| `.wasm` compressed (brotli/gzip as served) | ≤ 4 MB | 6 MB |
| JS glue + shell compressed | ≤ 250 KB | 500 KB |
| Total initial transfer | ≤ 4.5 MB | 6.5 MB |
| Time to interactive (mid-laptop, fast 3G-ish 5 Mbps) | ≤ 5 s | 10 s |
| Frame time (interactive, desktop) | ≤ 8 ms p95 | 16 ms sustained |
| Steady-state memory | ≤ 128 MB | 256 MB |
| Full browser screenshot-test suite runtime in CI | ≤ 10 min | 20 min |

The three STIX font arrays plus LVGL are the expected size drivers; measure per-section with `--emit-symbol-map`/twiggy in Phase 0 (WASM-BUILD-01 feasibility numbers).

---

## H. Debugging **[proposed]**

- **Debug build config** (§C flags) with source maps; DWARF (`-g`) build available locally for Chrome's C++ DevTools debugging.
- **Console logs:** the existing `[SIM]/[SCRIPT]/[ASSERT]/[KEY]` prefixed lines verbatim; `--quiet` maps to config `quiet`.
- **Crash report:** overlay's "copy report" bundles `build-info.json`, last 200 log lines, abort reason, browser UA.
- **Assertion messages:** unchanged C++ strings (they are the CI grammar).
- **Replay scripts:** dev mode records injected keys as a `.numos`-syntax log ("input tape") downloadable for bug reports — keys already flow through the script-name vocabulary, so the tape is replayable on the *desktop* emulator too. This is a genuinely new debugging capability the port enables.
- **Deterministic traces:** dev mode can dump per-frame `{frame, tick, cmd}` lines (bounded), matching the desktop's per-frame log style.
- **Symbolized stacks:** feasible in debug builds via source maps; release builds ship a symbol map artifact in CI for post-hoc symbolication.

---

## I. Release policy **[proposed]**

- **When to publish:** manual `workflow_dispatch` at first (human gate, mirroring the human-only golden-promotion philosophy); auto-publish from `main` only after the parity gates have been stable ≥2 weeks (same maturation rule the golden plan uses for blessing).
- **Version label:** `numos-web-<date>-<short-sha>` shown in the status strip; no semver until the product stabilizes.
- **Commit hash display:** required in UI and `build-info.json` (test harness asserts it).
- **Cache invalidation:** hashed filenames + tiny mutable `index.html`; service worker (PWA phase) uses versioned caches keyed by commit and shows an "update available — reload" toast.
- **Rollback:** §E; a rollback is a deploy of an old commit, never a manual file edit.
- **Compatibility warnings:** shell feature-detects (wasm, exceptions mode if used, IndexedDB) and shows a friendly unsupported-browser card instead of failing cryptically. Reproducible-build note: the deterministic-build mode (no timestamp in `build-info.json`, `-frandom-seed=` fixed, sorted inputs) is the analogue of the firmware reproducibility finding (whole-file hashes shift on trivial changes) — we record inputs precisely rather than promising bit-identical wasm across emsdk versions.

---

## J. Explicit answers (build/deploy subset of the Special Questions)

- **Q3 (PlatformIO owns WASM build?):** No — standalone CMake/emsdk with a generated-and-verified source list keeping `platformio.ini` the single authority on what compiles (§B).
- **Q4 (pthreads):** No (`-sUSE_PTHREADS=0`); GitHub Pages hosting makes this structural (§C, §E).
- **Q5 (Asyncify):** No (§C; runtime rationale in companion doc 2 §B).
- **Q15 (public deployment):** GitHub Pages, versioned dirs + mutable `latest/`, PR artifacts not previews, manual-then-automatic publish (§E, §I).
- **Q18 (top blockers, build view):** emsdk toolchain bring-up, source-list generation, exceptions decision, size budget of font arrays, browser test harness — sequenced in the roadmap doc Phase 0–2.
