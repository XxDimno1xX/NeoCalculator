#!/usr/bin/env bash
set -euo pipefail

# Generate STIX Two Math font for LVGL.
# Requirements:
#   npm i -g lv_font_conv
# Usage:
#   ./scripts/generate_stix_math_font.sh [path-to-STIXTwoMath-Regular.ttf]
#
# Default output:
#   src/fonts/stix_math_18.c

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FONT_FILE="${1:-${ROOT_DIR}/assets/fonts/STIXTwoMath-Regular.ttf}"
OUT_FILE="${ROOT_DIR}/src/fonts/stix_math_18.c"

if [[ ! -f "${FONT_FILE}" ]]; then
  echo "Missing font file: ${FONT_FILE}" >&2
  echo "Pass STIXTwoMath-Regular.ttf path as first argument." >&2
  exit 1
fi

# Requested broad coverage for math rendering.
BASE_RANGES="0x20-0x7E,0xA0-0xFF,0x370-0x3FF,0x2000-0x206F,0x2070-0x209F,0x2100-0x214F,0x2190-0x21FF,0x2200-0x22FF,0x2300-0x23FF,0x2460-0x24FF,0x25A0-0x25FF,0x27C0-0x27EF,0x27F0-0x27FF,0x2900-0x297F,0x2980-0x29FF,0x2A00-0x2AFF,0x2B00-0x2BFF,0x1D400-0x1D7FF"
OPTIONAL_RANGE="0x1F00-0x1FFF"

# Extra explicit symbols for auditability.
SYMBOLS="∫∑√∞ℝℂαβγ⊂⊆→←↔≤≥≠±×÷∂∇∈∉∪∩∅πℤℚℕ"

run_conv() {
  local ranges="$1"
  lv_font_conv \
    --font "${FONT_FILE}" \
    --size 18 \
    --bpp 4 \
    --format lvgl \
    --range "${ranges}" \
    --symbols "${SYMBOLS}" \
    -o "${OUT_FILE}"
}

echo "[stix] Generating ${OUT_FILE}"
if ! run_conv "${BASE_RANGES},${OPTIONAL_RANGE}"; then
  echo "[stix] Optional range ${OPTIONAL_RANGE} not present in this TTF, retrying without it"
  run_conv "${BASE_RANGES}"
fi

# PlatformIO compatibility include path.
sed -i 's|#include "lvgl/lvgl.h"|#include "lvgl.h"|g' "${OUT_FILE}"

echo "[stix] Done: ${OUT_FILE}"
