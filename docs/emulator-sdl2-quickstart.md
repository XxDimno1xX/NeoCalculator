# NumOS SDL2 Desktop Emulator — Quickstart

How to build and run the **native desktop emulator** (`pio run -e emulator_pc`) on
Windows and Linux. This is a desktop *port* of NumOS (the real LVGL/UI/math code
compiled against an SDL2 desktop HAL) — **not** a cycle-accurate ESP32 emulator.

For the full design, see
[emulator-sdl2-implementation-spec.md](emulator-sdl2-implementation-spec.md).
This quickstart covers **Phase 2** (reproducible, portable build & run with no
machine-specific SDL2 paths), **Phase 3A** (stable 320×240 logical scaling,
key press/release, and a headless auto-exit mode for CI — see
[Run modes](#run-modes-phase-3a) and [Keyboard map](#keyboard-map-phase-3a)), and
**Phase 3B** (a deterministic fixed-step tick and a dependency-free PPM screenshot
dump for reproducible CI artifacts — see
[Deterministic mode & screenshots](#deterministic-mode--screenshots-phase-3b)).

> The firmware build is completely separate and unchanged:
> `pio run -e esp32s3_n16r8`. Nothing here affects it.

---

## TL;DR

```bash
# Windows (PowerShell)
pio run -e emulator_pc
./scripts/run-emulator-windows.ps1

# Linux
sudo apt-get install -y libsdl2-dev
PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc
./scripts/run-emulator-linux.sh

# Timed / headless smoke (auto-exit; no manual window close — see Run modes)
./scripts/run-emulator-windows.ps1 --headless --run-for-ms 5000 --quiet          # Windows
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh --run-for-ms 5000 --quiet   # Linux
```

Diagnose your setup any time:

```bash
python scripts/check-emulator-deps.py
```

---

## How SDL2 is discovered (no hardcoded paths)

SDL2 include/library/link flags are injected at build time by
[`scripts/sdl2_env.py`](../scripts/sdl2_env.py), a PlatformIO `pre:` extra_script
wired **only** into `[env:emulator_pc]`. There are no `-IC:/SDL2/...` paths in
`platformio.ini` anymore.

| OS | Discovery method |
|:--|:--|
| **Windows** | An SDL2 *development* root (folder containing `include/`, `lib/`, `bin/`). Resolved from `NUMOS_SDL2_ROOT` → `SDL2_DIR` → `SDL2_ROOT` → the historical default `C:/SDL2/x86_64-w64-mingw32`. Links `-lmingw32 -lSDL2main -lSDL2`. |
| **Linux / macOS** | System SDL2 via `pkg-config sdl2`, falling back to `sdl2-config`, falling back to a bare `-lSDL2`. |

At runtime the **Windows** binary additionally needs `SDL2.dll`;
[`run-emulator-windows.ps1`](../scripts/run-emulator-windows.ps1) finds it and
extends `PATH` for the child process only (it never edits your global `PATH`).

---

## Windows

### Prerequisites
- **PlatformIO Core** (`pip install -U platformio`).
- **A C++ compiler** — MinGW-w64 GCC (the project is built/tested with
  `x86_64-...-posix-seh` GCC).
- **SDL2 MinGW development package** — download `SDL2-devel-2.x.y-mingw` from
  <https://github.com/libsdl-org/SDL/releases> and extract it. You want the
  `x86_64-w64-mingw32` subfolder (it contains `include/`, `lib/`, `bin/`).

  > ABI note: use a **MinGW** SDL2 dev package (not the MSVC one). The project's
  > known-good combo is the SDL2 `x86_64-w64-mingw32` prebuilt with MinGW-w64 GCC.

### Point the build at SDL2
Either drop it at the default location `C:\SDL2\x86_64-w64-mingw32`, **or** set an
environment variable (per-shell):

```powershell
$env:NUMOS_SDL2_ROOT = 'C:\path\to\SDL2-2.30.x\x86_64-w64-mingw32'
```

`SDL2_DIR` / `SDL2_ROOT` work too.

### Build & run
```powershell
pio run -e emulator_pc
./scripts/run-emulator-windows.ps1
```

The run script locates the executable (honoring the project's custom `build_dir`,
see [Build directory](#build-directory)), resolves `SDL2.dll`
(next to the exe → SDL2 root → `PATH`), then launches. Pass-through args are
forwarded: `./scripts/run-emulator-windows.ps1 <args>`.

---

## Linux

### Prerequisites
- **PlatformIO Core** (`pip install -U platformio`).
- **g++** (or clang++).
- **SDL2 dev package:**
  - Debian/Ubuntu: `sudo apt-get install -y libsdl2-dev`
  - Fedora: `sudo dnf install -y SDL2-devel`
  - Arch: `sudo pacman -S sdl2`

### Build & run
```bash
PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc
./scripts/run-emulator-linux.sh
```

Why `PLATFORMIO_BUILD_DIR=.pio/build`? See [Build directory](#build-directory).

The run script locates the binary, sanity-checks the SDL2 runtime (`ldd` /
`ldconfig`), then runs it.

> **Status:** the Linux path is designed and the discovery logic is in place, but
> a Linux build has **not** been executed on the author's machine (Windows-only).
> The additive CI workflow
> ([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml))
> is the mechanism that will actually prove the Linux build; treat Linux as
> "expected to work, pending first green CI run" until then.

### macOS
Untested/future. `brew install sdl2` provides `pkg-config`/`sdl2-config`, so the
same discovery path *should* apply; no macOS guarantee is made.

---

## Run modes (Phase 3A)

By default the emulator is interactive (the window stays open until you close
it). Phase 3A adds a few command-line flags — **emulator-only; the firmware is
unaffected** — mainly so CI (or you) can launch it briefly without a human:

| Flag | Effect |
|:--|:--|
| `--frames N` | Run N main-loop iterations, then exit cleanly (liveness / no-crash smoke). |
| `--run-for-ms N` | Run ~N ms of wall-clock time, then exit cleanly. Prefer this for "did it reach the menu" checks — the splash is ~2 s, so use ≥ 4000. |
| `--scale N` | Integer window scale 1..8 (default 2). The logical surface is always 320×240; SDL scales it with crisp integer scaling. |
| `--headless` | Force `SDL_VIDEODRIVER=dummy` (no visible window) for CI / no-display hosts. You can also `export SDL_VIDEODRIVER=dummy` yourself. |
| `--quiet` | Silence the per-key / per-iteration debug logs (lifecycle logs remain). |
| `--deterministic` | **(Phase 3B)** Drive LVGL from a synthetic fixed-step clock instead of the wall clock. Animation/timer state becomes a function of the *frame index*, so a given `--frames N` is reproducible across runs and machines. Pair it with `--frames N`. See [Deterministic mode & screenshots](#deterministic-mode--screenshots-phase-3b). |
| `--step-ms N` | **(Phase 3B)** Virtual milliseconds advanced per frame in `--deterministic` mode, 1..1000 (default 16 ≈ 60 fps). No effect without `--deterministic`. |
| `--screenshot P` / `--dump-frame P` | **(Phase 3B)** After the final frame (before shutdown), dump the **320×240 logical** framebuffer to PPM (P6) at path `P`. Dependency-free; works under `--headless` and at any `--scale`. |
| `--help` | Print usage and exit. |

The run scripts forward all flags, e.g.:

```bash
# Windows
./scripts/run-emulator-windows.ps1 --headless --run-for-ms 5000 --quiet
# Linux
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh --run-for-ms 5000 --quiet
```

A healthy timed run prints a `[SCALE] …` geometry line, reaches
`[MENU] Launcher cargado`, then `[SIM] auto-exit: …` and `[SIM] Bye!` (exit 0).
That is exactly what the additive CI workflow asserts
([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml)).

### Display scaling

The renderer presents a fixed **320×240 logical** surface
(`SDL_RenderSetLogicalSize`) with **integer scaling**
(`SDL_RenderSetIntegerScale`) and nearest-neighbor sampling, so pixels stay
crisp at ×2/×3/×4. This is *output* scaling only — the device geometry and the
math renderer are untouched. The startup `[SCALE]` log reports logical size,
window size, renderer output size, scale factor, integer-scale state, and the
selected backend (e.g. `direct3d(accel)` or `software`).

### Deterministic mode & screenshots (Phase 3B)

Phase 3B adds a **reproducible** headless mode plus a **screenshot** dump, so CI
can produce a stable visual artifact. Both are **emulator-only** and **opt-in**;
without the flags the runtime is byte-for-byte the Phase 3A behavior.

```bash
# Deterministic headless run that writes a 320x240 PPM screenshot
SDL_VIDEODRIVER=dummy ./scripts/run-emulator-linux.sh \
  --headless --frames 600 --deterministic --screenshot smoke.ppm --quiet   # Linux
./scripts/run-emulator-windows.ps1 `
  --headless --frames 600 --deterministic --screenshot smoke.ppm --quiet   # Windows
```

- **Deterministic tick.** Normally LVGL reads the wall clock
  (`lv_tick_set_cb(SDL_GetTicks)`), so the same `--frames N` samples animations at
  different real-time instants each run. With `--deterministic`, LVGL instead reads
  a synthetic counter the loop advances by `--step-ms` (default **16 ms**) **per
  frame**, *before* `lv_timer_handler()`. The visual state then depends only on the
  frame index — reproducible across runs/machines, with no `SDL_Delay`/VSYNC jitter
  (the per-frame sleep is skipped in this mode). The boot log states which clock is
  active: `[LVGL] lv_init() OK — tick = determinista (paso fijo por frame)` vs
  `… = SDL_GetTicks() (reloj de pared)`. At 16 ms/frame the ~2 s splash finishes by
  ~frame 125, so `--frames 600` lands well inside the launcher.
- **Screenshot.** `--screenshot PATH` (alias `--dump-frame PATH`) dumps the final
  frame just before shutdown. The source is the CPU-side full-screen LVGL buffer
  (`g_lvBuf`, RGB565, `LV_DISPLAY_RENDER_MODE_FULL`), so the image is always the
  **exact 320×240 logical display** — independent of `--scale` and valid under
  `--headless`. Output is **PPM P6** (a 3-line ASCII header + raw RGB), chosen to
  avoid any new dependency (no PNG/libpng). RGB565 is expanded to RGB888 by bit
  replication. A success line prints `[SHOT] PPM 320x240 escrito: <path>`.

> View a PPM with most image tools (GIMP, ImageMagick `display`/`convert`, IrfanView
> with plugins). To convert to PNG: `convert smoke.ppm smoke.png`.

**CI artifacts.** The additive emulator workflow
([`.github/workflows/emulator-build.yml`](../.github/workflows/emulator-build.yml))
runs this exact deterministic command and uploads, under the
**`numos-emulator-smoke`** artifact, both the run log (`emu-det-smoke.log`) and the
screenshot (`smoke.ppm`). Download it from the workflow run's *Artifacts* section.
The job only asserts the launcher was reached, the deterministic tick was used, and
the PPM exists and is 320×240 — it makes **no pixel-correctness claim yet** (that is
a later phase).

**Limitations.** Determinism covers LVGL's tick-driven animation/timer state; it
does not freeze any wall-clock reads elsewhere (there are none on the render path
today). The screenshot is the logical framebuffer, not the scaled host window. This
is single-shot capture at exit — no per-frame capture and no scripted input yet
(**next: Phase 4, scripted input replay** — see [Known limitations](#known-limitations-phase-3a)).

## Keyboard map (Phase 3A)

PC keys map directly to calculator `KeyCode`s in
[`NativeHal.cpp`](../src/hal/NativeHal.cpp) (`mapSdlToKeyCode`). SDL **KEYDOWN**
produces a press (or `REPEAT` on OS auto-repeat); SDL **KEYUP** now produces a
release. In the launcher (MENU) only navigation/confirm keys do anything; in
CalculationApp every mapped key is forwarded.

| PC key | Calculator | PC key | Calculator |
|:--|:--|:--|:--|
| `Enter` / KP Enter | ENTER | `0`–`9` (top row + keypad) | NUM_0..NUM_9 |
| Arrow keys | UP / DOWN / LEFT / RIGHT | `+ - * /` | ADD / SUB / MUL / DIV |
| `Esc` | AC | `( )` | LPAREN / RPAREN |
| `Backspace` / `Delete` | DEL | `^` | POW |
| `Home` / `m` | MODE (back to launcher) | `.` | DOT |
| `LShift` / `RShift` | SHIFT | `s` `c` `t` | SIN / COS / TAN |
| `Tab` | ALPHA | `l` `g` `r` | LN / LOG / SQRT |
| `Insert` | STO (Store) | `p` `e` | π / e |
| `f` / `F5` / `=` | FREE_EQ (S⇔D) | `x` `y` | VAR_X / VAR_Y |
| `n` | NEGATE | | |

Notes:
- **SHIFT / ALPHA / STO** are resolved by `KeyboardManager` inside
  CalculationApp (they do nothing in the launcher). `Insert`→STO is new in
  Phase 3A.
- **OS key-repeat** is forwarded as `KeyAction::REPEAT` (a held key repeats),
  matching the hardware keyboard driver; it is distinct from a fresh PRESS.
- Unmapped keys are logged as `[KEY] sin-mapear SDL=<name>` unless `--quiet`.
- This is **not** the full hardware 5×10 matrix; it is a direct desktop keymap.

---

## Build directory

This repo sets `build_dir = C:/.piobuild/numOS` in `platformio.ini` (top
`[platformio]` section) to keep Windows build paths short (MinGW + deep
PlatformIO paths can hit the Windows path-length limit). Consequences:

- On **Windows** the emulator builds to
  `C:/.piobuild/numOS/emulator_pc/program.exe`. The run script finds it
  automatically.
- On **Linux/CI**, that `C:/...` value is not a sensible path. Override it with
  `PLATFORMIO_BUILD_DIR=.pio/build` (recommended), which is what the commands and
  CI above do. The firmware CI also strips the line with `sed`.
- The run scripts resolve the build dir in this order:
  `PLATFORMIO_BUILD_DIR` → `build_dir` from `platformio.ini` (ignored on
  non-Windows if it is a `C:\` path) → `.pio/build`.

---

## Known limitations (Phase 3A)

- **Native Giac is unavailable.** All Giac entry points are `#ifdef ARDUINO`
  ([GiacBridge.h:19,29](../src/math/giac/GiacBridge.h#L19)); the emulator does not
  compile or call Giac. The Giac library is intentionally excluded from the native
  build (`lib_ignore = giac libtommath`, [platformio.ini:185](../platformio.ini#L185)).
- **BigInt parity is not guaranteed.** The in-tree `cas::` engine builds natively
  but with `CAS_HAS_BIGINT 0` ([CASInt.h:53-59](../src/math/cas/CASInt.h#L53)) —
  int64 only; on overflow it returns an error state rather than promoting to
  arbitrary precision like the device. Large factorials / huge exact fractions can
  differ from hardware. Small results (e.g. `1/2 + 1/3 = 5/6`) are fine.
- **Not a cycle-accurate ESP32 emulator.** No Xtensa CPU emulation, no ILI9341 /
  SPI / DMA timing, no real PSRAM/`heap_caps` behavior (PSRAM is plain `malloc`).
- **Hardware keyboard matrix is not emulated.** The physical 5×10 GPIO scan
  (`drivers/Keyboard`) is not compiled natively; PC keys are mapped directly to
  `KeyCode` in [NativeHal.cpp](../src/hal/NativeHal.cpp) (`mapSdlToKeyCode`).
  As of Phase 3A, `KEYUP` produces `RELEASE` and `Insert`→`STO` is mapped (see
  [Keyboard map](#keyboard-map-phase-3a)); full matrix / serial-text input is
  still future. SHIFT/ALPHA/STO are only meaningful inside CalculationApp, not
  in the launcher.
- **Only CalculationApp is wired** in the emulator; other apps print
  "no implementada" ([NativeHal.cpp](../src/hal/NativeHal.cpp)).
- **Deterministic screenshots exist; scripted input does not yet.** Phase 3A adds
  `--frames` / `--run-for-ms` / `--headless` for unattended boot smokes, and Phase
  3B adds `--deterministic` + `--screenshot` for a reproducible 320×240 PPM artifact
  (see [Deterministic mode & screenshots](#deterministic-mode--screenshots-phase-3b)).
  Still future (**Phase 4**): scripted-input replay and value assertions like
  `1+2 → 3`. The interactive window still runs until you close it; capture is
  single-shot at exit, and no pixel-level correctness is asserted yet.

---

## Troubleshooting

### "SDL2.dll could not be located" (Windows, at runtime)
The build succeeded but the DLL isn't reachable. Do one of:
- copy `SDL2.dll` next to `program.exe`, **or**
- set `NUMOS_SDL2_ROOT` to your SDL2 dev folder (the one with `bin\SDL2.dll`), **or**
- add the SDL2 `bin` folder to `PATH` for your shell.

The run script searches all three; run it after fixing one.

### "SDL2 development root not found" / "cannot find SDL headers/libs" (build)
- **Windows:** the extra_script couldn't find an SDL2 dev root. Set
  `NUMOS_SDL2_ROOT` (see [Windows](#windows)) and rebuild.
- **Linux:** install `libsdl2-dev` (or distro equivalent) so `pkg-config sdl2`
  succeeds. Verify with `pkg-config --modversion sdl2`.
- Run `python scripts/check-emulator-deps.py` for a full diagnosis.

### Emulator builds but the window does not open
- **Linux headless / SSH:** there is no display. Either run on a desktop session,
  or run with `--headless` (or `export SDL_VIDEODRIVER=dummy`). Pair it with
  `--run-for-ms N` so it auto-exits instead of running forever, e.g.
  `./scripts/run-emulator-linux.sh --headless --run-for-ms 5000 --quiet`.
- Check stdout: a healthy boot prints `[SDL2] Ventana ... creada OK`, a
  `[SCALE] ...` line, and reaches `[MENU] Launcher cargado`.
- If `SDL_CreateRenderer` fails (e.g. no GPU under the dummy driver), the code
  already falls back to the software renderer
  ([NativeHal.cpp](../src/hal/NativeHal.cpp), `SDL_RENDERER_SOFTWARE`); the
  `[SCALE]` log shows `backend=software(software)` in that case.

### The window opens but no keys work
- **Focus:** the SDL window must have OS keyboard focus — click it once.
- **Mode matters:** in the launcher (MENU) only arrows, `Enter`, `Esc`, and
  `Backspace` do anything (LVGL grid navigation); digits/operators are ignored
  there by design. Press `Enter` on the *Calculation* card first, then type.
- **Check the log (omit `--quiet`):** each accepted key prints
  `[KEY] SDL=<name> code=<n> action=PRESS|REPEAT|RELEASE mode=<MODE>`. If you
  see `[KEY] sin-mapear SDL=<name>` the key has no calculator mapping — consult
  the [Keyboard map](#keyboard-map-phase-3a) (note `(` / `)` / `*` need the same
  Shift combos as on a US keyboard).
- **STO:** press `Insert` (the only key bound to `KeyCode::STO`); it is
  meaningful only inside CalculationApp.

### The firmware build is separate
The emulator and firmware are independent PlatformIO environments. The emulator
work never changes firmware behavior:

```bash
pio run -e esp32s3_n16r8        # production firmware (unchanged by Phase 2)
```

If a firmware build misbehaves, it is not related to the emulator scripts/config.
