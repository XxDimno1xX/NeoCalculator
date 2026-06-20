# Accepted golden screenshots — human review gate

This directory holds **accepted, human-reviewed** golden screenshots (PPM P6,
320×240) for the NumOS SDL2 emulator. A file here is ground truth: CI compares
freshly generated candidate screenshots against it byte-for-byte and **fails on
mismatch**.

Because a golden is authoritative, it must only enter the tree through a
deliberate human review — never automatically. This mirrors the existing
acceptance practice in [`docs/math-renderer-acceptance.md`](../../../docs/math-renderer-acceptance.md)
(geometry is *frozen* only after human review).

## Directory & naming

| Role | Location | Committed? |
|:--|:--|:--|
| Replay script | `tests/emulator/scripts/<name>.numos` | yes |
| **Accepted golden** | `tests/emulator/golden/<name>.ppm` | **yes** |
| Ignore-rect mask (optional) | `tests/emulator/masks/<name>.mask` | yes |
| Candidate (machine-generated) | `out/emulator-candidates/<name>.ppm` | no (`out/` is git-ignored) |

The stem is identical across all of them; they differ only by directory and
extension. Current candidate set: `launcher_smoke`, `calc_1_plus_2`,
`calc_fraction_sum`.

A golden may have an optional **ignore-rect mask** (Phase 4B-B) covering a known
nondeterministic region — today only the CalculationApp blinking cursor. With a
mask, CI ignores those pixels; with no mask the compare is byte-exact. Masks are
also human-gated; see [`tests/emulator/masks/README.md`](../masks/README.md).

## How a candidate becomes a golden (the review gate)

1. **Generate** candidates deterministically (ideally on the pinned reference OS
   — Linux + `SDL_VIDEODRIVER=dummy`, which CI uses):

   ```bash
   pio run -e emulator_pc
   python scripts/generate-emulator-candidates.py
   # -> out/emulator-candidates/*.ppm  (320x240, 230415 bytes each)
   ```

2. **Visually inspect** the candidate and confirm it is actually correct
   (e.g. `calc_1_plus_2` really renders `3`, `calc_fraction_sum` really renders
   `5/6`). View a PPM with GIMP / ImageMagick `display` / IrfanView, or convert:
   `convert out/emulator-candidates/calc_1_plus_2.ppm calc.png`.

3. **Promote** — only after a human confirms correctness — by copying the
   candidate here and committing it:

   ```bash
   cp out/emulator-candidates/calc_1_plus_2.ppm tests/emulator/golden/calc_1_plus_2.ppm
   git add tests/emulator/golden/calc_1_plus_2.ppm
   git commit -m "Accept golden: calc_1_plus_2 renders 3"
   ```

   The commit message records the human acceptance. Re-blessing a changed golden
   follows the same path; review the old-vs-new diff in the PR.

### Promoting a golden that needs a mask (cursor nondeterminism)

Some screens (the calc screens) are not byte-stable across separate launches
because the input cursor blinks. To promote one safely:

1. Prove the volatility: regenerate the same candidate twice and diff them —
   `python scripts/compare-ppm.py a.ppm b.ppm --write-diff jitter.ppm`. The
   magenta region is the minimum the mask must cover (it should be only the cursor
   band, nothing else).
2. Visually confirm the candidate is **semantically correct** (e.g. `1+2` really
   renders `3`). Masking the cursor does not relax this judgement.
3. Author a tight `tests/emulator/masks/<name>.mask` (see that directory's README)
   covering the cursor band, with a comment explaining why.
4. Verify the mask closes the gate: `compare-ppm.py a.ppm b.ppm --mask-file
   tests/emulator/masks/<name>.mask` must report IDENTICAL (exit 0).
5. Commit the golden **and** the mask together so review sees both:

   ```bash
   cp out/emulator-candidates/calc_1_plus_2.ppm tests/emulator/golden/calc_1_plus_2.ppm
   git add tests/emulator/golden/calc_1_plus_2.ppm tests/emulator/masks/calc_1_plus_2.mask
   git commit -m "Accept golden+mask: calc_1_plus_2 renders 3 (cursor region masked)"
   ```

## Guarantees

- **No tool writes here.** `scripts/generate-emulator-candidates.py` only writes
  to `out/emulator-candidates/`; CI only *reads* this directory. The only way a
  golden changes is a human `git commit`.
- **CI cannot ossify a bad screenshot.** When a golden is **absent**, CI prints a
  warning, uploads the candidate, and **does not fail** — so an unreviewed image
  can never silently become a passing gate. When a golden is **present**, CI
  compares against it (`scripts/compare-ppm.py`) and fails on mismatch.
- **Recommended:** require PR review for changes under `tests/emulator/golden/**`
  so blessing/altering a golden is always attributable.

## Scope (Phase 4B-A / 4B-B)

Byte-comparison proves a candidate matches an accepted image; it does **not**
itself assert semantic correctness. Semantic correctness is the human's
judgement at promotion time. Phase 4B-B adds *cursor-safe masking* so calc
screenshots can be compared despite blink nondeterminism — it still does not read
the result. OCR / automated result extraction (proving the pixels actually read
`3` without a hand-reviewed golden) is a later phase (Phase 4B-C).
