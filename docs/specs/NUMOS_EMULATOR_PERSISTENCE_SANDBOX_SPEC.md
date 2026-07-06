# NumOS — Emulator Persistence Sandbox Specification

> **What "done" means:** every byte the emulator persists has exactly one declared root directory chosen by the invoker, deterministic runs start from a declared filesystem state, no test run can dirty the git tree, and a two-process roundtrip test proves persistence actually persists. Companion docs: `NUMOS_EMULATOR_DETERMINISM_AND_FIXTURE_HYGIENE_SPEC.md` (EMUDET), `NUMOS_EMULATOR_CI_ARTIFACT_AND_GOLDEN_POLICY_SPEC.md` (CIART), `NUMOS_EMULATOR_HOST_PORTABILITY_SPEC.md` (PORT), `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md` (EMUROAD). Persistence-format and corruption rules inherit `NUMOS_GLOBAL_SETTINGS_STATE_AND_PERSISTENCE_SPEC.md` (GSTATE §B/§G) and must not contradict `NUMOS_SAFE_MODE_AND_RECOVERY_SPEC.md` invariant R-3 or the `[FS]` tag grammar in `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md`.
>
> Audit base: branch `claude/numos-emulator-determinism-specs-69nso8` (2026-07-04). **[V]** verified `file:line` · **[P]** proposed. SPEC-ONLY.

---

## A. Current persistence map [V]

### A.1 The emulator filesystem shim

`hal/FileSystem.{h,cpp}` emulates the Arduino LittleFS API for `!ARDUINO` builds (`FileSystem.h:37`). Properties that matter here:

- **Base directory is a hardcoded relative path**: `static constexpr const char* EMULATOR_DATA_DIR = "./emulator_data"` (`FileSystem.cpp:38`); `fullPath()` prefixes it (`FileSystem.cpp:44-49`). Everything is therefore **CWD-relative**: run the binary from anywhere other than the repo root and you get a new, empty `emulator_data/` there.
- `begin()` just `mkdir`s the base dir and always returns true; `formatOnFail` is ignored (`FileSystem.cpp:52-56`, `FileSystem.h:115-119`).
- API surface: `exists/open/remove`, `File` = thin `FILE*` wrapper, binary mode forced (`FileSystem.cpp:70-82`). No directories-in-paths handling beyond what `fopen` gives; no atomic rename; no fsync.

### A.2 VariableManager (the only live emulator store)

- Singleton, 11 `ExactVal` slots A–F, x, y, z, Ans, PreAns (`VariableManager.cpp:62-77`, `VariableManager.h:139`).
- File format `/vars.dat`: magic `VR01` (`MAGIC = 0x56523031`, `VariableManager.h:154`) written as a host-endian u32 — on-disk bytes are `10RV` on little-endian [V, `od` of the committed file] — + 1 count byte + N×34-byte records (`VariableManager.cpp:19-25,143-163,169-190`). **No checksum, no version negotiation beyond the magic, non-atomic `"w"` overwrite** (`VariableManager.cpp:170`). Loader tolerates short files by breaking early and ignores extra records (`VariableManager.cpp:216-222`).
- **Load:** emulator boot, unconditionally: `nativeFS_init()` → `LittleFS.begin(true)` → `loadFromFlash()` (`NativeHal.cpp:2171-2179`). Firmware boot: `SystemApp.cpp:144-156` (which also pre-creates a 1-byte `0x00` file on first boot to silence a VFS log, `SystemApp.cpp:145-148` — that stub then fails the magic check on the next load and reads as "empty", `VariableManager.cpp:202-207`; the emulator does **not** replicate this pre-creation).
- **Save:** exactly one caller — STO: `CalculationApp::executeStore()` → `vm.saveToFlash()` (`CalculationApp.cpp:894-901`). Exercised by `tests/emulator/scripts/calc_store_variable.numos` (STO flow documented in the script header) which is a **gating CI step** (`emulator-build.yml:343`).
- **Read for asserts:** `assert_variable` reads the singleton directly (`NativeHal.cpp:1805-1820`).

### A.3 Settings (future store)

Nothing persisted today: the `setting_*` trio are process-lifetime globals (`Config.h:77-79`; firmware defs `main.cpp:31-33`; emulator defs `NativeHal.cpp:125-127`), written only by SettingsApp in memory (`SettingsApp.cpp:241-266`). GS-02 (GSTATE §B) will add: firmware NVS namespace `numos_set`; emulator file `/settings.dat` under the emulator FS root, **opt-in via `--persist-settings`** (GSTATE §B.3, §F.1). This spec supplies the sandbox those rules assume.

### A.4 LittleFS/NVS analogs

| Firmware store | Emulator analog | Status [V] |
|---|---|---|
| LittleFS `/vars.dat` | `./emulator_data/vars.dat` via `hal/FileSystem` | live, tracked in git |
| LittleFS NeoLang scripts (`NeoLanguageApp.cpp:897,924`, F2/F3 save/load `:393-394`) | would land in `./emulator_data/` | dormant — app not whitelisted (`platformio.ini:195-266`) |
| NVS `calcVars` (`VariableContext`, legacy doubles) | none — NVS path is `#ifdef ARDUINO` only (`platformio.ini:227-229` comment; store is write-dead anyway, GSTATE §A.5) | dormant |
| NVS `numos_set` (future settings, GSTATE §B.3) | `/settings.dat` file, opt-in | future |

### A.5 emulator_data/ inventory

Exactly one file exists and it is **git-tracked**: `emulator_data/vars.dat` (379 bytes = 4+1+11×34, all-zero values, [V] `git ls-files emulator_data/`). `.gitignore` does **not** exclude `emulator_data/` [V — reviewed `.gitignore`; it ignores `out/`, `.pio/`, `*piobuild*`, dot-paths, but not `emulator_data`].

### A.6 Script side effects

- STO scripts mutate `vars.dat` (A.2). After a local `calc_store_variable` run the tracked file's x-slot becomes 4 → permanently dirty tree until manually reverted.
- Every boot *reads* the file, so run N's stores are visible to run N+1 in the same checkout — `assert_variable` on a never-stored variable is only correct today because no bundled script stores into the variables other scripts assert, a discipline nothing enforces.
- Screenshot writes (`out/…`) are covered by EMUDET §E.1; they are artifacts, not persistence, and `out/` is git-ignored.

---

## B. Desired sandbox model [P → FIX-01/FIX-02]

### B.1 One resolved FS root per process

`hal/FileSystem` gains a settable root (single point: replace the constant at `FileSystem.cpp:38` with a `setRoot()` applied before `begin()`; `NativeHal` resolves it from CLI flags before `nativeFS_init()`, `NativeHal.cpp:2019-2023`). Resolution precedence:

1. `--fs-sandbox-dir <path>` — use `<path>` as the root, create if absent, read-write, **never deleted** by the emulator (caller owns lifecycle). This is the shared-sandbox / roundtrip primitive (§C).
2. `--fixture-dir <path>` — create a fresh per-run temp sandbox, copy `<path>/*` into it, run against the copy; the fixture itself is opened read-only and its content is never modified (verified by AT-FIX-2, EMUDET §H).
3. `--fs-sandbox` — fresh empty per-run temp sandbox.
4. `--fs-root <path>` — use `<path>` directly (read-write, no copy). `--fs-root emulator_data` reproduces today's behavior explicitly.
5. No flag: **interactive** runs default to (4) with `emulator_data` (unchanged developer experience); **`--deterministic` or `--script` runs default to (3)** once FIX-02 lands (staged: FIX-01 ships the flags inert-by-default; FIX-02 flips the scripted default after CI and bundled scripts are updated).

Mutually exclusive flags → exit 2 at parse time (same fail-fast contract as `NativeHal.cpp:1876-1884`). The resolved mode + absolute path is printed in the deterministic banner (EMUDET AT-DET-5) and tagged `[FS] root=<path> mode=<sandbox|fixture|shared|direct>`.

### B.2 Per-run temp dir placement

- Root: `TMPDIR`/`TEMP` per platform (POSIX `mkdtemp`, Windows `GetTempPath`+unique suffix), pattern `numos-emu-<pid>-XXXXXX`. Never inside the repo (keeps every dirty-tree guard trivially true) and never under `C:/.piobuild` (PORT §D).
- Cleanup: removed on exit code 0; **retained** on any nonzero exit, path printed on stderr, so failure state is inspectable and CI can upload it (CIART §F.6).

### B.3 Per-script clean dir

One process = one script = one sandbox already holds (the runner and the candidate generator exec the binary per script, `generate-emulator-candidates.py:296-310`), so "per-script clean dir" falls out of B.1(3) with no runner changes.

### B.4 Explicit persistent fixture dir

Committed fixtures live under `tests/emulator/fixtures/<name>/` (new, e.g. `tests/emulator/fixtures/vars_x7/vars.dat`), consumed only via `--fixture-dir`. Rules: fixtures are committed, human-authored/blessed like masks (`tests/emulator/masks/README.md:88-98` authoring discipline applies); no tool writes into `tests/emulator/fixtures/`; each fixture dir carries a `README` line stating what it encodes and which scripts use it.

### B.5 No writes to repo root by default; no tracked runtime files

Consequences of B.1: the binary writes only inside its resolved root plus explicitly named screenshot paths. The tracked `emulator_data/vars.dat` is removed from tracking (§E). After that, **zero tracked files are writable by any emulator code path** — which is the invariant the CI dirty-tree guard (CIART §H) enforces forever after.

---

## C. Two-process persistence test protocol [P → FIX-04; = SCQA §E.3 QA-05 made concrete]

Runner-level (bash in `emulator-build.yml`, mirrored in a local script), not `.numos` grammar — the emulator is one process per script, so "reboot" = a second invocation sharing a sandbox.

```
T=$(mktemp -d)                                   # deterministic, runner-owned location
# Process A — write
$BIN --headless --deterministic --frames 1400 --quiet \
     --fs-sandbox-dir "$T" \
     --script tests/emulator/scripts/persist_store_x4.numos      # STO x=4; exit 0
# Process B — verify persistence
$BIN --headless --deterministic --frames 800 --quiet \
     --fs-sandbox-dir "$T" \
     --script tests/emulator/scripts/persist_assert_x4.numos     # assert_variable x 4; exit 0
# Process C — verify isolation (fresh sandbox ⇒ default state)
$BIN --headless --deterministic --frames 800 --quiet \
     --fs-sandbox \
     --script tests/emulator/scripts/persist_assert_x0.numos     # assert_variable x 0; exit 0
rm -rf "$T"
```

Contract details:

- **Scripts.** Two tiny new `.numos` fixtures per store under test. For variables they are refactorings of `calc_store_variable.numos` (store half) plus an assert-only half. For settings (post-GS-02): run A adds `--persist-settings` and uses the QA-01 `set_*` hooks; run B asserts via `assert_setting_value`; run C omits `--persist-settings` to prove opt-in is honored (GSTATE §F.1).
- **Deterministic file location.** The only cross-process channel is `$T`; the runner chooses it, so the test is host-portable (mktemp on Linux/macOS, `$env:TEMP` on Windows runners) and parallel-safe (unique per job).
- **Cleanup.** `$T` deleted by the runner on success; on failure the runner uploads `$T` (tiny — vars.dat is 379 bytes) plus all three logs as a failure bundle (CIART §F.6).
- **Failure artifacts.** Each process already emits `[ASSERT] … FAIL` lines and exit 4 on mismatch (`NativeHal.cpp:1682-1693`); the runner additionally prints `sha256 vars.dat` between processes so a corrupt-write failure is distinguishable from a load failure.
- **What it proves.** Save path (`VariableManager.cpp:169-190`), load path (`:196-226`), and sandbox sharing — end to end, byte-real, on the CI OS. This is the test the firmware cannot get from CI (firmware roundtrip stays a manual/HIL row, SCQA §E.3).

---

## D. Corruption testing [P → FIX-05; formats and quarantine semantics from GSTATE §G]

Runner-level steps that plant a bad file in a sandbox, boot, and assert graceful degradation. Today's loader behavior [V] gives the baseline expectations:

| Case | Planted state | Expected today [V] | Expected after RT-13/GS-02 hardening [P] |
|---|---|---|---|
| Corrupt magic | 4 random bytes + valid tail | `loadFromFlash()` returns false at magic check (`VariableManager.cpp:202-207`); all vars 0 | same + one `[FS] vars=corrupt action=defaults` line |
| Truncated record | valid header, half a record | partial read breaks loop; earlier vars loaded, later default (`VariableManager.cpp:216-222`) | whole-record rejection (no half-loaded state), quarantine per GSTATE §G.3 |
| Missing file | none | `open` fails → false (`VariableManager.cpp:199-200`); defaults | same + `[FS] vars=missing action=defaults` |
| Firmware 1-byte stub | single `0x00` (mirrors `SystemApp.cpp:145-148`) | magic read fails → defaults | same; documents emulator/firmware first-boot parity (§F) |
| Old/newer version | magic `VR99` | fails magic → defaults | explicit version policy once `VR02` exists (GSTATE §B.5 pattern) |
| Invalid checksum | n/a today (no CRC, `VariableManager.cpp:19-25`) | not testable | mandatory case the moment a CRC lands (settings first: garbage `/settings.dat` → boot → defaults + `[FS] settings=corrupt`, SCQA §F) |

Acceptance for every case: process exits 0 (corruption must never kill a boot), asserts on default values pass, the diagnostic line is grepped by the runner, and the planted file is either untouched or quarantined (`.bad` rename) — never silently overwritten in place before the next explicit save (GSTATE §G.3 no-destructive-auto-format rule; note today's `begin(true)` call sites `NativeHal.cpp:2172`, `SystemApp.cpp:144` are format-on-fail on firmware only — the emulator shim ignores the flag, `FileSystem.cpp:52-56`).

---

## E. Git hygiene [P → FIX-03]

1. **Must be gitignored:** `emulator_data/` (whole directory), `out/` (already, `.gitignore`), any `numos-emu-*` sandbox should one ever be created in-repo by mistake, `*.piobuild*`/`.pio/` (already), `C:/` artifacts (already via `*piobuild*` — verify a plain `C:/` entry too when PORT-04 resolves).
2. **Must be committed:** `tests/emulator/scripts/**`, `tests/emulator/golden/**`, `tests/emulator/masks/**`, new `tests/emulator/fixtures/**`, the READMEs. Unchanged policy (`tests/emulator/golden/README.md:14-20`).
3. **Must never be committed:** candidates (`out/emulator-candidates/`), logs, diff PPMs, sandboxes, any runtime-mutated store. After FIX-03 there is no committed runtime-mutable file left.
4. **Deletion from tracking:** `git rm --cached emulator_data/vars.dat` + `.gitignore` entry, in the same commit that (a) lands the sandbox default or (b) at minimum documents `--fs-root emulator_data` as the interactive default — order matters: untracking before the sandbox exists would leave interactive users with an untracked-but-still-mutated file, which is fine (untracked ≠ dirty), so FIX-03 may land first. If a seeded boot state is ever actually wanted, it returns as a **fixture** (`tests/emulator/fixtures/default_vars/`), not as a tracked live file. The committed content today is all-zeros [V §A.5], i.e. equivalent to no file at all (`VariableManager` constructor zeroes, `VariableManager.cpp:54-56,112-116`) — so nothing of value is lost by untracking.
5. **How CI detects a dirty tree:** dedicated final step per job — `git diff --exit-code` (tracked modifications) plus a scoped untracked check (`git status --porcelain` filtered to non-ignored paths) — exact step text in CIART §H. It must run `if: always()` so an earlier red doesn't skip the hygiene signal.
6. **How scripts avoid dirtying the local tree:** sandbox default (B.1); until it lands, the documented workaround is `git update-index --skip-worktree` — **not recommended**, prefer landing FIX-03 quickly. The candidate generator and all runner suites already confine their writes to `out/` [V `generate-emulator-candidates.py:240-268`; `emulator-build.yml:205,264,312,379,436,499`].

---

## F. Emulator vs firmware persistence: what must match, may differ, must be documented

### F.1 Must match (parity contract)

- **File format bytes** for every LittleFS-backed store: same magic, layout, endianness assumptions (`VariableManager.cpp:143-163` serialization is shared code compiled into both targets — parity by construction; keep it single-source).
- **Load/save call sites and triggers:** boot-load (`SystemApp.cpp:149` ↔ `NativeHal.cpp:2173`) and STO-save (`CalculationApp.cpp:900`, shared) must stay behaviorally aligned; an emulator-only save trigger would test a flow firmware doesn't have.
- **Corruption policy** (§D): identical decisions (defaults, quarantine, no destructive format) on both targets once RT-13/GS-02 land.
- **Settings opt-in semantics:** emulator `--persist-settings` off ⇒ behaves like firmware with settings never saved; on ⇒ behaves like firmware steady-state (GSTATE §B.3/§F.1).

### F.2 May differ (declared divergences)

- **Backend:** real LittleFS wear-leveled flash vs host `FILE*` in a directory (`FileSystem.cpp` whole file). Emulator therefore proves *logic*, not flash behavior (power-loss atomicity, wear, mount failures are firmware/HIL-only — consistent with `NUMOS_FABLE_CONTEXT_HEADER.md:86-88`).
- **NVS stores:** no emulator analog (`VariableContext` NVS is `#ifdef ARDUINO`; future `numos_set` NVS maps to a file). Two-process tests of NVS semantics are firmware-only rows.
- **First-boot stub file:** firmware writes a 1-byte `vars.dat` (`SystemApp.cpp:145-148`); the emulator doesn't. Harmless (both read as "empty") but must stay documented — or better, the stub gets deleted from firmware under RT-13 and the divergence disappears.
- **Capacity/latency:** host disk is effectively infinite and instant; no emulator test may assert flash-full or write-latency behavior.

### F.3 Must be explicitly documented (in `docs/emulator-sdl2-quickstart.md` when FIX-01 lands)

The FS root resolution table (B.1), the sandbox default flip (FIX-02), the fixture directory convention (B.4), and the divergences in F.2. Every new store added to either target must add a row to the A.4 table in this spec — the table is the parity register.

---

## G. Implementation tickets (headline — full fields in EMUROAD)

- **FIX-01** — Settable FS root + sandbox/fixture flags in `hal/FileSystem` + `NativeHal` arg parsing (B.1–B.4).
- **FIX-02** — Default flip: `--script`/`--deterministic` runs sandbox by default; bundled scripts/CI updated in the same change.
- **FIX-03** — Untrack `emulator_data/vars.dat`, gitignore `emulator_data/`, add fixture convention + README (E).
- **FIX-04** — Two-process roundtrip runner test for variables now, settings post-GS-02 (C; joint with SCQA QA-05).
- **FIX-05** — Corruption matrix runner tests (D), aligned with RT-13/GS-02 quarantine work.
- **FIX-06** — Parity register upkeep + firmware stub-file cleanup decision (F.2) — doc + one-line firmware change owned by RT-13.

*End of persistence sandbox spec.*
