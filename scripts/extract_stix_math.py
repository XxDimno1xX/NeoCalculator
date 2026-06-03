#!/usr/bin/env python3
"""
extract_stix_math.py — Build-time STIX Two Math font data extraction.

Extracts the 56 OpenType MATH table constants from STIXTwoMath-Regular.otf/.ttf
and generates a C/C++ header for the NumOS ESP32-S3 calculator firmware.

Requirements:
    pip install fonttools

Usage:
    python scripts/extract_stix_math.py [path-to-STIXTwoMath-Regular.ttf]

Output:
    src/math/font/stix_math_constants.h   — constexpr int16_t kStixMathConstants[56]

Future extensions (Phase 3+):
    src/math/font/stix_math_variants.h    — GlyphAssembly tables for delimiters
    src/math/font/stix_math_italics.h     — MathItalicsCorrectionInfo table
    src/math/font/stix_math_glyph_map.h   — codepoint → glyphID sparse map
"""

import os
import sys
from datetime import datetime
from pathlib import Path

# ── Find fontTools ──────────────────────────────────────────────────────────
try:
    from fontTools.ttLib import TTFont
except ImportError:
    print("ERROR: fontTools is not installed.", file=sys.stderr)
    print("Install it with:  pip install fonttools", file=sys.stderr)
    sys.exit(1)


# ═══════════════════════════════════════════════════════════════════════════════
# The 56 MathConstants in OpenType spec serialization order.
# Index → (C++ name, OpenType spec name, storage type)
#
# Storage types:
#   "i16"  = direct int16_t  (constants 0–1: scale percentages)
#   "u16"  = direct uint16_t (constants 2–3: min heights, constant 55: raise %)
#   "mvr"  = MathValueRecord (constants 4–54: {int16_t Value; Offset16 DeviceTable})
#            We extract only the .Value — DeviceTable is always NULL in STIX Two Math
#            and we have no runtime device-table interpreter on ESP32.
# ═══════════════════════════════════════════════════════════════════════════════

CONSTANT_SPEC = [
    # idx,  C++ name,                               storage
    ( 0,    "ScriptPercentScaleDown",                "i16"),
    ( 1,    "ScriptScriptPercentScaleDown",          "i16"),
    ( 2,    "DelimitedSubFormulaMinHeight",          "u16"),
    ( 3,    "DisplayOperatorMinHeight",              "u16"),
    ( 4,    "MathLeading",                           "mvr"),
    ( 5,    "AxisHeight",                            "mvr"),
    ( 6,    "AccentBaseHeight",                      "mvr"),
    ( 7,    "FlattenedAccentBaseHeight",             "mvr"),
    ( 8,    "SubscriptShiftDown",                    "mvr"),
    ( 9,    "SubscriptTopMax",                       "mvr"),
    (10,    "SubscriptBaselineDropMin",              "mvr"),
    (11,    "SuperscriptShiftUp",                    "mvr"),
    (12,    "SuperscriptShiftUpCramped",             "mvr"),
    (13,    "SuperscriptBottomMin",                  "mvr"),
    (14,    "SuperscriptBaselineDropMax",            "mvr"),
    (15,    "SubSuperscriptGapMin",                  "mvr"),
    (16,    "SuperscriptBottomMaxWithSubscript",     "mvr"),
    (17,    "SpaceAfterScript",                      "mvr"),
    (18,    "UpperLimitGapMin",                      "mvr"),
    (19,    "UpperLimitBaselineRiseMin",             "mvr"),
    (20,    "LowerLimitGapMin",                      "mvr"),
    (21,    "LowerLimitBaselineDropMin",             "mvr"),
    (22,    "StackTopShiftUp",                       "mvr"),
    (23,    "StackTopDisplayStyleShiftUp",           "mvr"),
    (24,    "StackBottomShiftDown",                  "mvr"),
    (25,    "StackBottomDisplayStyleShiftDown",      "mvr"),
    (26,    "StackGapMin",                           "mvr"),
    (27,    "StackDisplayStyleGapMin",               "mvr"),
    (28,    "StretchStackTopShiftUp",                "mvr"),
    (29,    "StretchStackBottomShiftDown",           "mvr"),
    (30,    "StretchStackGapAboveMin",               "mvr"),
    (31,    "StretchStackGapBelowMin",               "mvr"),
    (32,    "FractionNumeratorShiftUp",              "mvr"),
    (33,    "FractionNumeratorDisplayStyleShiftUp",  "mvr"),
    (34,    "FractionDenominatorShiftDown",          "mvr"),
    (35,    "FractionDenominatorDisplayStyleShiftDown","mvr"),
    (36,    "FractionNumeratorGapMin",               "mvr"),
    (37,    "FractionNumDisplayStyleGapMin",         "mvr"),
    (38,    "FractionRuleThickness",                 "mvr"),
    (39,    "FractionDenominatorGapMin",             "mvr"),
    (40,    "FractionDenomDisplayStyleGapMin",       "mvr"),
    (41,    "SkewedFractionHorizontalGap",           "mvr"),
    (42,    "SkewedFractionVerticalGap",             "mvr"),
    (43,    "OverbarVerticalGap",                    "mvr"),
    (44,    "OverbarRuleThickness",                  "mvr"),
    (45,    "OverbarExtraAscender",                  "mvr"),
    (46,    "UnderbarVerticalGap",                   "mvr"),
    (47,    "UnderbarRuleThickness",                 "mvr"),
    (48,    "UnderbarExtraDescender",                "mvr"),
    (49,    "RadicalVerticalGap",                    "mvr"),
    (50,    "RadicalDisplayStyleVerticalGap",        "mvr"),
    (51,    "RadicalRuleThickness",                  "mvr"),
    (52,    "RadicalExtraAscender",                  "mvr"),
    (53,    "RadicalKernBeforeDegree",               "mvr"),
    (54,    "RadicalKernAfterDegree",                "mvr"),
    (55,    "RadicalDegreeBottomRaisePercent",       "u16"),
]

EXPECTED_COUNT = 56  # Sanity check


def extract_constants(font_path: str) -> tuple[list[int], int]:
    """
    Extract all 56 MathConstants from the font's MATH table.

    Returns:
        (values[56], unitsPerEm) — list of int16_t-compatible values + font UPM.
    """
    font = TTFont(font_path)

    if "MATH" not in font:
        raise ValueError(f"Font '{font_path}' does not contain a MATH table. "
                         f"Is this STIX Two Math?")

    mc = font["MATH"].table.MathConstants
    upm = font["head"].unitsPerEm

    values = []
    for idx, name, storage in CONSTANT_SPEC:
        try:
            raw = getattr(mc, name, None)
        except Exception:
            raw = None

        if raw is None:
            print(f"  [WARN] Constant {idx:2d} '{name}' missing — falling back to 0")
            values.append(0)
            continue

        if storage == "mvr":
            # MathValueRecord — extract .Value (int16_t)
            if hasattr(raw, "Value"):
                val = raw.Value
            else:
                print(f"  [WARN] Constant {idx:2d} '{name}' is not a "
                      f"MathValueRecord — using raw value as fallback")
                val = int(raw)
        else:
            val = int(raw)

        # Clamp to int16_t range (OpenType spec guarantees these fit, but be safe)
        if val < -32768 or val > 32767:
            print(f"  [WARN] Constant {idx:2d} '{name}' = {val} exceeds int16_t "
                  f"range — clamping")
            val = max(-32768, min(32767, val))

        values.append(val)

    if len(values) != EXPECTED_COUNT:
        raise RuntimeError(f"Internal error: extracted {len(values)} constants, "
                           f"expected {EXPECTED_COUNT}")

    font.close()
    return values, upm


def generate_header(values: list[int], upm: int, output_path: str,
                    source_font: str) -> None:
    """Generate C/C++ header with the extracted constants array."""

    # Build per-constant comments
    constant_lines = []
    for i, (val, (_, name, _)) in enumerate(zip(values, CONSTANT_SPEC)):
        constant_lines.append(f"    {val:4d},  // [{i:2d}] {name}")

    header_content = f"""\
/*
 * NumOS — STIX Two Math Font Constants
 * Auto-generated by scripts/extract_stix_math.py on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 * Source font: {source_font}
 * Font UPM (unitsPerEm): {upm}
 *
 * This file is part of NumOS.
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DO NOT EDIT MANUALLY — regenerated at build time.
 */

#pragma once

#include <cstdint>

namespace vpam {{

/// OpenType MATH table constants extracted from STIX Two Math.
///
/// Array index matches the OpenType spec serialization order (56 entries).
/// All values are in font design units (UPM = {upm}).
/// Scale to pixels by multiplying by (fontSizePx / upm).
///
/// Access patterns (matching Mozilla Gecko's GetMathConstant dispatch):
///   [ 0– 1]  int16  — ScriptPercentScaleDown, ScriptScriptPercentScaleDown
///   [ 2– 3]  uint16 — DelimitedSubFormulaMinHeight, DisplayOperatorMinHeight
///   [ 4–54]  int16  — MathValueRecord.Value (DeviceTable offsets omitted)
///   [55]     uint16 — RadicalDegreeBottomRaisePercent
///
/// Reference: OpenType 1.9.1 spec, MATH table — MathConstants sub-table.
constexpr int16_t kStixMathConstants[{EXPECTED_COUNT}] = {{
{chr(10).join(constant_lines)}
}};

/// Enumeration index into kStixMathConstants for type-safe access.
/// Values match the OpenType MATH table serialization order.
enum class MathConstant : uint8_t {{
    // ── int16 direct (0–1) ──
    ScriptPercentScaleDown                 = 0,
    ScriptScriptPercentScaleDown           = 1,

    // ── uint16 direct (2–3) ──
    DelimitedSubFormulaMinHeight           = 2,
    DisplayOperatorMinHeight               = 3,

    // ── MathValueRecord int16 values (4–54) ──
    MathLeading                            = 4,
    AxisHeight                             = 5,
    AccentBaseHeight                       = 6,
    FlattenedAccentBaseHeight              = 7,
    SubscriptShiftDown                     = 8,
    SubscriptTopMax                        = 9,
    SubscriptBaselineDropMin               = 10,
    SuperscriptShiftUp                     = 11,
    SuperscriptShiftUpCramped              = 12,
    SuperscriptBottomMin                   = 13,
    SuperscriptBaselineDropMax             = 14,
    SubSuperscriptGapMin                   = 15,
    SuperscriptBottomMaxWithSubscript      = 16,
    SpaceAfterScript                       = 17,
    UpperLimitGapMin                       = 18,
    UpperLimitBaselineRiseMin              = 19,
    LowerLimitGapMin                       = 20,
    LowerLimitBaselineDropMin              = 21,
    StackTopShiftUp                        = 22,
    StackTopDisplayStyleShiftUp            = 23,
    StackBottomShiftDown                   = 24,
    StackBottomDisplayStyleShiftDown       = 25,
    StackGapMin                            = 26,
    StackDisplayStyleGapMin                = 27,
    StretchStackTopShiftUp                 = 28,
    StretchStackBottomShiftDown            = 29,
    StretchStackGapAboveMin                = 30,
    StretchStackGapBelowMin                = 31,
    FractionNumeratorShiftUp               = 32,
    FractionNumeratorDisplayStyleShiftUp   = 33,
    FractionDenominatorShiftDown           = 34,
    FractionDenominatorDisplayStyleShiftDown = 35,
    FractionNumeratorGapMin                = 36,
    FractionNumDisplayStyleGapMin          = 37,
    FractionRuleThickness                  = 38,
    FractionDenominatorGapMin              = 39,
    FractionDenomDisplayStyleGapMin        = 40,
    SkewedFractionHorizontalGap            = 41,
    SkewedFractionVerticalGap              = 42,
    OverbarVerticalGap                     = 43,
    OverbarRuleThickness                   = 44,
    OverbarExtraAscender                   = 45,
    UnderbarVerticalGap                    = 46,
    UnderbarRuleThickness                  = 47,
    UnderbarExtraDescender                 = 48,
    RadicalVerticalGap                     = 49,
    RadicalDisplayStyleVerticalGap         = 50,
    RadicalRuleThickness                   = 51,
    RadicalExtraAscender                   = 52,
    RadicalKernBeforeDegree                = 53,
    RadicalKernAfterDegree                 = 54,

    // ── uint16 standalone (55) ──
    RadicalDegreeBottomRaisePercent        = 55,

    COUNT                                  = 56
}};

}}  // namespace vpam
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header_content)

    print(f"  Generated: {output_path}")
    print(f"  Constants: {EXPECTED_COUNT}")
    print(f"  Font UPM:  {upm}")


def main() -> int:
    # ── Resolve font path ────────────────────────────────────────────────────
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    if len(sys.argv) > 1:
        font_path = Path(sys.argv[1])
    else:
        # Default: look in assets/fonts/
        font_path = repo_root / "assets" / "fonts" / "STIXTwoMath-Regular.ttf"

    font_path = font_path.resolve()

    if not font_path.exists():
        print(f"ERROR: Font file not found: {font_path}", file=sys.stderr)
        print(f"Usage: {sys.argv[0]} [path-to-STIXTwoMath-Regular.ttf]",
              file=sys.stderr)
        return 1

    # ── Output path ──────────────────────────────────────────────────────────
    output_dir = repo_root / "src" / "math" / "font"
    output_path = output_dir / "stix_math_constants.h"

    # ── Extract ──────────────────────────────────────────────────────────────
    print(f"Source font:  {font_path}")
    print(f"Output:       {output_path}")
    print()

    try:
        values, upm = extract_constants(str(font_path))
    except Exception as e:
        print(f"ERROR extracting constants: {e}", file=sys.stderr)
        return 1

    # ── Generate header ──────────────────────────────────────────────────────
    try:
        generate_header(values, upm, str(output_path), str(font_path))
    except Exception as e:
        print(f"ERROR generating header: {e}", file=sys.stderr)
        return 1

    # ── Summary ──────────────────────────────────────────────────────────────
    print()
    print("Key constants for reference:")
    key_indices = {
        "AxisHeight": 5, "FractionRuleThickness": 38,
        "SuperscriptShiftUp": 11, "SubscriptShiftDown": 8,
        "RadicalRuleThickness": 51, "RadicalVerticalGap": 49,
        "ScriptPercentScaleDown": 0, "ScriptScriptPercentScaleDown": 1,
    }
    for name, idx in key_indices.items():
        print(f"  {name:35s} = {values[idx]:5d} du  "
              f"({values[idx] * 18 // upm:3d} px @ 18pt)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
