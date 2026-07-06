# NumOS — Emulator Host Portability Specification

> **What "done" means:** a developer on Windows, macOS (Apple Silicon or Intel), or Linux can build, run, script, and screenshot the emulator with copy-paste commands; every host-specific quirk (`C:/.piobuild`, SDL2 discovery, binary location) is either removed or fenced behind one documented, diagnosable seam; and a beginner's failed attempt produces a report a maintainer can act on. Companion docs: `NUMOS_EMULATOR_DETERMINISM_AND_FIXTURE_HYGIENE_SPEC.md` (EMUDET), `NUMOS_EMULATOR_PERSISTENCE_SANDBOX_SPEC.md` (SANDBOX), `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (CIART), `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md` (EMUROAD). The user-facing narrative lives in `docs/emulator-sdl2-quickstart.md` (1447 lines, current and detailed) — this spec is the *contract* those docs implement.
>
> Audit base: branch `claude/numos-emulator-determinism-specs-69nso8` (2026-07-04). **[V]** verified `file:line` · **[P]** proposed. SPEC-ONLY.

---

## A. Current host support [V]

| Host | Build | Run | Proven? |
|---|---|---|---|
| **Windows** (maintainer's machine) | SDL2 MinGW dev root via env vars or `C:/SDL2/x86_64-w64-mingw32` default (`sdl2_env.py:63-68`); `build_dir = C:/.piobuild/numOS` is an absolute path here (`platformio.ini:6`) | `run-emulator-windows.ps1` resolves `program.exe` + `SDL2.dll` (`run-emulator-windows.ps1:17-26,39-48,72-116`) | yes — primary dev platform (`NUMOS_FABLE_CONTEXT_HEADER.md:7`); Phase-3A smoke "verified equivalent locally on Windows" (`emulator-build.yml:127-129`) |
| **Linux** | system SDL2 via pkg-config → sdl2-config → bare `-lSDL2` (`sdl2_env.py:140-162`) | `run-emulator.sh` (pio `-t exec`) or `run-emulator-linux.sh` (direct binary + ldd sanity check, `run-emulator-linux.sh:69-97`) | yes — CI is the proof (`emulator-build.yml:47-50`, whole job) |
| **macOS (Apple Silicon / Intel)** | same unix path as Linux; Homebrew `sdl2 pkg-config platformio` (`docs/emulator-sdl2-quickstart.md:285-291`) | `run-emulator.sh`; quickstart flags the `/opt/homebrew/bin` PATH gotcha (`quickstart:305-311`) | **no** — "expected to work, pending first confirmation" (`quickstart:325-329`); no macOS CI job exists [V — `emulator-build.yml` has one `ubuntu-latest` job] |
| **CI Linux** | ubuntu-latest, apt `libsdl2-dev`, `PLATFORMIO_BUILD_DIR=.pio/build` (`emulator-build.yml:54-64`) | headless `SDL_VIDEODRIVER=dummy` everywhere (`:137,167,210`, …); `--headless` also sets it in-process (`NativeHal.cpp:1889-1892`) | yes — gating |

## B. SDL2 discovery [V]

One seam: `scripts/sdl2_env.py`, a PlatformIO `pre:` extra_script wired only into `[env:emulator_pc]` (`platformio.ini:155-159`; firmware envs untouched by design, `sdl2_env.py:28-29`).

- **Windows:** dev-root resolution order `NUMOS_SDL2_ROOT` → `SDL2_DIR` → `SDL2_ROOT` → `C:/SDL2/x86_64-w64-mingw32` (`sdl2_env.py:63-68`); validates `include/`+`lib/` (`:87-95`); appends `-I<root>/include{,SDL2}`, `-L<root>/lib`, `-lmingw32 -lSDL2main -lSDL2` (`:96-100`); warns if `bin/SDL2.dll` is absent since it's needed at *runtime* (`:103-110`). Runtime DLL resolution is separately handled by `run-emulator-windows.ps1:72-116` (exe-adjacent → env roots → historical default → PATH) and mirrored for subprocesses by `generate-emulator-candidates.py:184-207`. Hard failure with instructions if no root found (`sdl2_env.py:77-85`).
- **Linux/macOS:** `pkg-config --cflags/--libs sdl2` (`:143-148`) → `sdl2-config --cflags --libs` (`:151-154`) → last-resort bare `-lSDL2` with an install hint (`:157-162`). For every `-I<dir>` the parent is also added so both `<SDL2/SDL.h>` and `<SDL.h>` include styles resolve under nonstandard prefixes — which is exactly the Homebrew case (`:124-137`).
- **PATH/DYLD/LIBRARY issues:** Linux relies on the system loader (`generate-emulator-candidates.py:187-189` returns None deliberately); `run-emulator-linux.sh` pre-flights with `ldd`/`ldconfig` and prints per-distro install lines (`run-emulator-linux.sh:69-97`). macOS: Homebrew SDL2 is linked via the pkg-config `-L`, and the install-name resolution normally needs no `DYLD_LIBRARY_PATH`; **gap [P]:** nothing pre-flights the macOS dylib the way `ldd` does on Linux (`run-emulator-linux.sh:70-79` is Linux-shaped; `otool -L` equivalent is absent) — folded into PORT-03.
- **Diagnostic mirror:** `check-emulator-deps.py` re-implements the same discovery read-only (`check-emulator-deps.py:69-114`) so a user can see what the build will see.

## C. PlatformIO build/run model [V]

- `pio run -e emulator_pc` — compile only (`quickstart:316-320`). **`pio run` with no `-e` builds *every* env including firmware** because `[platformio]` sets no `default_envs` (`platformio.ini:5-7`) — on a machine without the espressif toolchain this downloads/attempts the ESP32 platform. Documented trap; candidate fix in PORT-04 (set `default_envs`) must weigh the maintainer's firmware-first habit.
- `pio run -e emulator_pc -t exec` — build + launch from wherever PlatformIO built it; sidesteps binary-location questions entirely (`run-emulator.sh:8-14`); **cannot forward program arguments** (`run-emulator.sh:23-27`), so scripted/headless invocations need the direct binary.
- Direct binary: `<build_dir>/emulator_pc/program[.exe]`; resolution ladder implemented three times — `generate-emulator-candidates.py:146-181`, `run-emulator-linux.sh:27-48`, `run-emulator-windows.ps1:39-48` — all: explicit → `PLATFORMIO_BUILD_DIR` → ini `build_dir` (non-Windows skips `C:` paths) → `.pio/build`.
- **VS Code tasks** (`.vscode/tasks.json`): "Run NumOS Emulator" (`-t exec`, `:4-18`), "Build NumOS Emulator (compile only)" (`:19-28`), legacy alias (`:29-38`). The run task's `detail` explicitly warns off F5/"PIO Debug" (`:6`).
- **PIO Debug is not for the emulator:** it targets ESP32 JTAG and fails without a board (`tasks.json:6`; `quickstart:81-97`).
- **"Electron Main" is not part of NumOS:** a generic VS Code debug template users must not pick (`quickstart:92-94`). Both warnings already exist; the contract is they stay (docs are part of the append-only surface for this).

## D. `build_dir = C:/.piobuild/numOS` [V + P]

- **Why it exists:** Windows 260-char `MAX_PATH` — the default `.pio/build` tree under a long user path plus Giac/LVGL's deep include folders breaks the *firmware* compile; a short absolute root keeps paths under the limit (`quickstart:100-110`).
- **Why it confuses macOS/Linux:** no `C:` drive ⇒ PlatformIO treats the string as *relative* and creates a literal `./C:/.piobuild/numOS/` inside the repo (`quickstart:107-113`; risk EMU-3 in `NUMOS_RISK_REGISTER.md` — observed in the original audit). `.gitignore` neutralizes it for commits (`*piobuild*` pattern [V]), and the three resolution ladders skip `C:` paths on non-Windows (§C), but the binary still lands in the weird directory unless `PLATFORMIO_BUILD_DIR` is set.
- **Why CI overrides it:** both workflows force `PLATFORMIO_BUILD_DIR=.pio/build` (`emulator-build.yml:52-55`; `compile-and-release.yml:15-19`), and the firmware workflow *additionally* sed-strips the line for clean logs (`compile-and-release.yml:29-39`).
- **PlatformIO cannot scope `build_dir` per environment** (`quickstart:116-119`), so it can't be "emulator-portable, firmware-short" in one ini.
- **How docs should present it [P]:** exactly as the quickstart already does (`quickstart:98-133`) — named section, "harmless but confusing", two clean options (`-t exec`, or `PLATFORMIO_BUILD_DIR=.pio/build`). Contract addition: every command block in docs/specs that builds the emulator on macOS/Linux carries the `export PLATFORMIO_BUILD_DIR=.pio/build` prefix (the context header already models this, `NUMOS_FABLE_CONTEXT_HEADER.md:37-39`).
- **Future portable alternative [P → PORT-04, decision ticket]:** options, in preference order:
  1. **Conditional via env var default** — keep the ini line but document `PLATFORMIO_BUILD_DIR` as *the* mechanism; add it to `.vscode/tasks.json` task `options.env` on non-Windows (tasks are cross-platform-parameterizable). Zero risk to the Windows firmware build.
  2. **`default_envs = esp32s3_n16r8` + a short *relative* `build_dir`** (e.g. `.pb`) — needs measurement that `<repo>/.pb/...` stays under MAX_PATH on the maintainer's checkout path; if it does, the `C:` weirdness disappears for everyone. Requires Windows verification before merge (human).
  3. Status quo forever — acceptable; the ladders + docs already contain it.
  PORT-04 picks 1 now (docs/tasks only, spec-compatible with this SPEC-ONLY series) and leaves 2 as a measured follow-up.

## E. Running direct binaries [V + P]

- **Path differences:** `program` vs `program.exe`; `<build>/emulator_pc/` under whichever build dir won (§C ladder). The canonical cross-platform locator is `generate-emulator-candidates.py:146-181`; any new runner script must reuse its order, not invent a fourth.
- **Args:** the full CLI surface is `--frames`, `--run-for-ms`, `--scale`, `--headless`, `--quiet`, `--deterministic`, `--step-ms`, `--screenshot/--dump-frame`, `--script`, `--help` (`NativeHal.cpp:1227-1280`) — identical on all hosts; unknown options only warn today (`NativeHal.cpp:1277`) [P → tighten to exit 2 alongside EMUDET-02 so a typo'd flag can't silently produce a wrong-mode run].
- **Headless:** `--headless` sets `SDL_VIDEODRIVER=dummy` in-process pre-init (`NativeHal.cpp:1889-1892`); exporting the env var works too and is what CI does belt-and-suspenders (`emulator-build.yml:137`). Works identically on all three OSes (the dummy driver ships with SDL2).
- **Scripts:** `.numos` paths are passed as given and opened with `std::ifstream` (`NativeHal.cpp:1459`); CRLF tolerated (`:1469-1470`). **CWD contract:** run from the repo root — script-relative screenshot paths like `out/x.ppm` and the FS root `./emulator_data` (`FileSystem.cpp:38`) resolve against CWD. The generator pins `cwd=REPO_ROOT` and deliberately passes ASCII *relative* paths because Windows narrow-CRT `fopen` mangles non-ASCII absolute paths (real case: a `Juan Ramón` home dir, `generate-emulator-candidates.py:286-294`). SANDBOX B.1 removes the FS-root half of the CWD dependence; screenshot paths keep the repo-root convention.
- **Screenshots:** pixel-identical across hosts by construction (CPU RGB565→RGB888, `NativeHal.cpp:1286-1315`) *modulo* the libm/compiler drift class (CIART §I) — cross-OS byte-equality is unmeasured [V] and is exactly what PORT-05 measures before anyone claims it.

## F. Beginner support contract [P → PORT-02/PORT-03; current pieces cited]

1. **What an issue report must include** (the quickstart already prescribes this shape, `quickstart:355-393`): OS + arch; `pio --version`; `python scripts/check-emulator-deps.py` full output; the exact command run; the full first error (not the tail); on run failures, the `[SCALE]`/`[LVGL]` boot lines (`NativeHal.cpp:1966-2006`) — these are the load-bearing diagnostics because they encode renderer backend, tick mode, and geometry.
2. **Diagnostics command:** `python scripts/check-emulator-deps.py` — stdlib-only, read-only, exit 0/1 (`check-emulator-deps.py:4-13,155-166`). Checks today: pio/compiler presence (`:52-66`), per-OS SDL2 (`:69-114`), build_dir sanity incl. the non-Windows `C:` warning (`:117-152`), built-binary presence (`:144-152`).
3. **`check-emulator-deps.py` role, extended [P → PORT-03]:** add (a) macOS dylib check (`otool -L` on the built binary when present — closes the §B gap); (b) SDL2 *runtime* presence on Linux via `ldconfig` (mirroring `run-emulator-linux.sh:80-86`); (c) a `--report` flag that prints the §F.1 issue-report block verbatim, machine-copy-pasteable; (d) a check that `pio run` without `-e` is about to build firmware (warn + suggest `-e emulator_pc`); (e) sandbox-flag awareness once FIX-01 lands (print the resolved FS root).
4. **Docs required:** quickstart stays the single beginner entry (already covers Windows/Linux/macOS/TL;DR/troubleshooting, `quickstart:1-200` structure); this spec adds the requirement that the quickstart's macOS section drops its "pending first confirmation" caveat (`quickstart:325-329`) only when PORT-01's verification actually runs — never by editorial optimism.

## G. Implementation tickets (headline — full fields in EMUROAD)

- **PORT-01** — macOS run verification workflow: a manual/dispatch CI job (`macos-14` runner: brew sdl2 + platformio, build, headless deterministic smoke + one candidate) to convert "expected to work" into evidence; optionally periodic, not per-push (cost).
- **PORT-02** — Docs/task cleanup wave: quickstart deltas from this series (sandbox flags, banner line, issue-report block), `.vscode/tasks.json` non-Windows `PLATFORMIO_BUILD_DIR` env, retire any stale claims.
- **PORT-03** — `check-emulator-deps.py` improvements (F.3).
- **PORT-04** — `build_dir` portability decision (D): adopt option 1 now; measure option 2 on Windows as follow-up.
- **PORT-05** — Cross-OS pixel-drift measurement: run the candidate set on macOS + Windows runners, byte-compare against Linux goldens, record the verdict (drives the CIART §I libm/compiler position; includes the `-ffp-contract` decision if drift is found).

*End of host portability spec.*
