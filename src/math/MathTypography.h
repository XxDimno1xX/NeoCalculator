/*
 * NumOS — Math Typography Engine
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * MathTypography.h — OpenType MATH Table-Driven Layout Engine
 *
 * Phase 2+3: Provides MathConstantsProvider (singleton wrapping the 56
 * STIX Two Math design-unit constants), the TeX 8-class atom system,
 * inter-atom spacing table, and design-unit-to-pixel conversion.
 *
 * This is the PURE MATH layer — no LVGL, no Arduino dependencies.
 * It sits between the font data (stix_math_constants.h) and the AST
 * layout system (MathAST.h).
 *
 * Reference: OpenType 1.9.1 MATH table spec, TeXbook Appendix G.
 */

#pragma once

#include <cstdint>
#include <algorithm>

#include "font/stix_math_constants.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// MathStyle — Current math typesetting context
//
// Determines which MATH constant variant to use (display vs inline),
// large operator limit placement, and mu-to-pixel conversion.
// ════════════════════════════════════════════════════════════════════════════
enum class MathStyle : uint8_t {
    DISPLAY_STYLE = 0,  // Display math (centered equations) — renamed to avoid Arduino DISPLAY macro
    TEXT          = 1,  // Inline math (same em-size as display, tighter spacing)
    SCRIPT        = 2,  // First-level sub/superscripts (~70% of display)
    SCRIPTSCRIPT  = 3,  // Second-level sub/superscripts (~55% of display)
};

/// Returns true if style is a script level (SCRIPT or SCRIPTSCRIPT).
constexpr bool isScriptStyle(MathStyle s) {
    return s == MathStyle::SCRIPT || s == MathStyle::SCRIPTSCRIPT;
}

/// Returns true for display or text (non-script) styles.
constexpr bool isDisplayOrText(MathStyle s) {
    return s == MathStyle::DISPLAY_STYLE || s == MathStyle::TEXT;
}

// ════════════════════════════════════════════════════════════════════════════
// MathClass — TeX 8-class atom classification
//
// Every AST node is assigned one of these classes. The spacing table
// (kSpacingTable) determines the inter-atom space between adjacent classes.
//
// Maps to LyX MathClass.h and TeXbook pp.170–171.
// ════════════════════════════════════════════════════════════════════════════
enum class MathClass : uint8_t {
    ORD    = 0,  // Ordinary: variables, digits, constants, functions
    OP     = 1,  // Large operator: ∑, ∫, ∏
    BINARY = 2,  // Binary operator: +, −, ×, ±  (renamed from BIN to avoid Arduino macro)
    REL    = 3,  // Relation: =, <, >, ≤, ≥, ≠
    OPEN   = 4,  // Opening delimiter: (, [, {
    CLOSE  = 5,  // Closing delimiter: ), ], }
    PUNCT  = 6,  // Punctuation: ,, ;
    INNER  = 7,  // Inner sub-formula (fractions, radicals, etc.)
    COUNT  = 8   // Sentinel
};

// ════════════════════════════════════════════════════════════════════════════
// TeX Inter-Atom Spacing Table
//
// kSpacingTable[left][right] = spacing code:
//    0 = no space
//    1 = thin space  (3 mu)
//    2 = medium space (4 mu)  — "2*" entries in TeXbook
//    3 = thick space (5 mu)
//    9 = impossible (error)
//   -1/-2/-3 = absolute space in display/text only, zero in script/scriptscript
//
// Verified against LyX 2.3.2 MathClass.h (Debian sources).
// Starred entries (*) from the spec are simplified per LyX's embedded model.
// ════════════════════════════════════════════════════════════════════════════
constexpr int8_t kSpacingTable[8][8] = {
    //           ORD   OP   BIN  REL  OPEN CLOSE PUNCT INNER
    /* ORD    */ { 0,   1,   2,   3,   0,   0,    0,    1   },
    /* OP     */ { 1,   1,   9,   3,   0,   0,    0,    1   },
    /* BINARY */ { 2,   9,   9,   9,   2,   9,    9,    2   },
    /* REL    */ { 3,   3,   9,   0,   3,   0,    0,    3   },
    /* OPEN   */ { 0,   0,   9,   0,   0,   0,    0,    0   },
    /* CLOSE  */ { 0,   1,   2,   3,   0,   0,    0,    1   },
    /* PUNCT  */ { 1,   1,   9,   1,   1,   1,    1,    1   },
    /* INNER  */ { 1,   1,   2,   3,   1,   0,    1,    1   }
};

/// Lookup inter-atom spacing code between two MathClass values.
/// Returns 0 for invalid classes (COUNT sentinel).
constexpr int8_t getInterAtomSpace(MathClass left, MathClass right) {
    auto li = static_cast<uint8_t>(left);
    auto ri = static_cast<uint8_t>(right);
    if (li >= 8 || ri >= 8) return 0;
    return kSpacingTable[li][ri];
}

// ════════════════════════════════════════════════════════════════════════════
// Design-unit conversion utilities
//
// STIX Two Math has UPM (units-per-em) = 1000.
// Mu conversion: 18 mu = 1 em (TeXbook p.169).
// ════════════════════════════════════════════════════════════════════════════

/// Convert font design units to pixels at the given em-size.
/// duToPx(value, emSize) = (value * emSize) / kStixMathUPM
constexpr int16_t duToPx(int16_t valueDu, int16_t emSizePx) {
    return static_cast<int16_t>((static_cast<int32_t>(valueDu) * emSizePx) / kStixMathUPM);
}

/// Convert math units (mu) to pixels.
/// 18 mu = 1 em. At script level, the mu unit scales with the font.
/// @param mu  Raw math-unit count (e.g. 3 for thin space, 4 for medium, 5 for thick).
constexpr int16_t muToPx(int8_t mu, int16_t emSizePx) {
    return static_cast<int16_t>((static_cast<int32_t>(mu) * emSizePx) / 18);
}

/// Map a TeX spacing-table code to actual math units (mu).
///
/// kSpacingTable codes:
///   0 = no space (0 mu)
///   1 = thin space  (3 mu)
///   2 = medium space (4 mu)
///   3 = thick space (5 mu)
///
/// This MUST be called BEFORE muToPx() when the value comes from
/// getInterAtomSpace() / kSpacingTable.
constexpr int8_t spacingCodeToMu(int8_t code) {
    // Map code → mu: 0→0, 1→3, 2→4, 3→5
    // For negative codes (absolute spacing), keep the magnitude as-is
    // (they're already in mu, just with sign to indicate style-dependent).
    if (code < 0) return static_cast<int8_t>(-code);  // abs(code) mu
    if (code == 0) return 0;
    if (code == 1) return 3;   // thin space
    if (code == 2) return 4;   // medium space
    if (code == 3) return 5;   // thick space
    return 0;  // code 9 (impossible) → no space
}

/// Round design units up to at least minPx pixels (ceiling division).
/// Use for rule thickness and other critical elements that must never
/// collapse to zero at small font sizes.
constexpr int16_t duToPxMin(int16_t valueDu, int16_t emSizePx, int16_t minPx) {
    int16_t px = duToPx(valueDu, emSizePx);
    return px < minPx ? minPx : px;
}

// ════════════════════════════════════════════════════════════════════════════
// MathConstantsProvider — Zero-overhead access to the 56 MATH constants
//
// Accesses the compile-time kStixMathConstants[] array directly.
// All methods are static and branch-free — the compiler can inline
// every call to a single array load.
//
// Usage:
//   int16_t axis = MathConstantsProvider::get(MathConstant::AxisHeight);
//   int16_t shift = MathConstantsProvider::fractionNumeratorShiftUp(true);
//   int16_t px = MathConstantsProvider::duToPx(68, 18);
// ════════════════════════════════════════════════════════════════════════════
class MathConstantsProvider {
public:
    // ── Raw constant access (O(1), zero branches) ──
    static constexpr int16_t get(MathConstant c) {
        return kStixMathConstants[static_cast<uint8_t>(c)];
    }

    // ── Typed accessors by name ──

    /// Font scaling percentage for script level (constant 0).
    static constexpr int16_t scriptPercentScaleDown() {
        return get(MathConstant::ScriptPercentScaleDown);
    }

    /// Font scaling percentage for scriptscript level (constant 1).
    static constexpr int16_t scriptScriptPercentScaleDown() {
        return get(MathConstant::ScriptScriptPercentScaleDown);
    }

    /// Height of the math axis above the baseline (constant 5).
    static constexpr int16_t axisHeight() {
        return get(MathConstant::AxisHeight);
    }

    /// Minimum delimiter sub-formula height (constant 2).
    static constexpr int16_t delimitedSubFormulaMinHeight() {
        return get(MathConstant::DelimitedSubFormulaMinHeight);
    }

    /// Minimum display operator height for limit placement (constant 3).
    static constexpr int16_t displayOperatorMinHeight() {
        return get(MathConstant::DisplayOperatorMinHeight);
    }

    // ── Subscript / Superscript ──

    static constexpr int16_t subscriptShiftDown() {
        return get(MathConstant::SubscriptShiftDown);
    }

    static constexpr int16_t subscriptTopMax() {
        return get(MathConstant::SubscriptTopMax);
    }

    static constexpr int16_t subscriptBaselineDropMin() {
        return get(MathConstant::SubscriptBaselineDropMin);
    }

    static constexpr int16_t superscriptShiftUp() {
        return get(MathConstant::SuperscriptShiftUp);
    }

    static constexpr int16_t superscriptShiftUpCramped() {
        return get(MathConstant::SuperscriptShiftUpCramped);
    }

    static constexpr int16_t superscriptBottomMin() {
        return get(MathConstant::SuperscriptBottomMin);
    }

    static constexpr int16_t superscriptBaselineDropMax() {
        return get(MathConstant::SuperscriptBaselineDropMax);
    }

    static constexpr int16_t subSuperscriptGapMin() {
        return get(MathConstant::SubSuperscriptGapMin);
    }

    static constexpr int16_t superscriptBottomMaxWithSubscript() {
        return get(MathConstant::SuperscriptBottomMaxWithSubscript);
    }

    static constexpr int16_t spaceAfterScript() {
        return get(MathConstant::SpaceAfterScript);
    }

    // ── Limits (∑, ∫ above/below) ──

    static constexpr int16_t upperLimitGapMin() {
        return get(MathConstant::UpperLimitGapMin);
    }

    static constexpr int16_t upperLimitBaselineRiseMin() {
        return get(MathConstant::UpperLimitBaselineRiseMin);
    }

    static constexpr int16_t lowerLimitGapMin() {
        return get(MathConstant::LowerLimitGapMin);
    }

    static constexpr int16_t lowerLimitBaselineDropMin() {
        return get(MathConstant::LowerLimitBaselineDropMin);
    }

    // ── Stack (for limits above/below operators) ──

    static constexpr int16_t stackTopShiftUp(bool display) {
        return get(display ? MathConstant::StackTopDisplayStyleShiftUp
                           : MathConstant::StackTopShiftUp);
    }

    static constexpr int16_t stackBottomShiftDown(bool display) {
        return get(display ? MathConstant::StackBottomDisplayStyleShiftDown
                           : MathConstant::StackBottomShiftDown);
    }

    static constexpr int16_t stackGapMin(bool display) {
        return get(display ? MathConstant::StackDisplayStyleGapMin
                           : MathConstant::StackGapMin);
    }

    // ── Fraction ──

    /// Shift of the numerator baseline above the fraction bar.
    static constexpr int16_t fractionNumeratorShiftUp(bool display) {
        return get(display ? MathConstant::FractionNumeratorDisplayStyleShiftUp
                           : MathConstant::FractionNumeratorShiftUp);
    }

    /// Shift of the denominator baseline below the fraction bar.
    static constexpr int16_t fractionDenominatorShiftDown(bool display) {
        return get(display ? MathConstant::FractionDenominatorDisplayStyleShiftDown
                           : MathConstant::FractionDenominatorShiftDown);
    }

    /// Minimum gap between numerator baseline and fraction bar.
    static constexpr int16_t fractionNumeratorGapMin(bool display) {
        return get(display ? MathConstant::FractionNumDisplayStyleGapMin
                           : MathConstant::FractionNumeratorGapMin);
    }

    /// Minimum gap between denominator baseline and fraction bar.
    static constexpr int16_t fractionDenominatorGapMin(bool display) {
        return get(display ? MathConstant::FractionDenomDisplayStyleGapMin
                           : MathConstant::FractionDenominatorGapMin);
    }

    /// Thickness of the fraction bar in design units.
    static constexpr int16_t fractionRuleThickness() {
        return get(MathConstant::FractionRuleThickness);
    }

    // ── Radical ──

    static constexpr int16_t radicalVerticalGap(bool display) {
        return get(display ? MathConstant::RadicalDisplayStyleVerticalGap
                           : MathConstant::RadicalVerticalGap);
    }

    static constexpr int16_t radicalRuleThickness() {
        return get(MathConstant::RadicalRuleThickness);
    }

    static constexpr int16_t radicalExtraAscender() {
        return get(MathConstant::RadicalExtraAscender);
    }

    static constexpr int16_t radicalKernBeforeDegree() {
        return get(MathConstant::RadicalKernBeforeDegree);
    }

    static constexpr int16_t radicalKernAfterDegree() {
        return get(MathConstant::RadicalKernAfterDegree);
    }

    static constexpr int16_t radicalDegreeBottomRaisePercent() {
        return get(MathConstant::RadicalDegreeBottomRaisePercent);
    }

    // ── Overbar / Underbar ──

    static constexpr int16_t overbarVerticalGap() {
        return get(MathConstant::OverbarVerticalGap);
    }

    static constexpr int16_t overbarRuleThickness() {
        return get(MathConstant::OverbarRuleThickness);
    }

    static constexpr int16_t overbarExtraAscender() {
        return get(MathConstant::OverbarExtraAscender);
    }

    static constexpr int16_t underbarVerticalGap() {
        return get(MathConstant::UnderbarVerticalGap);
    }

    static constexpr int16_t underbarRuleThickness() {
        return get(MathConstant::UnderbarRuleThickness);
    }

    static constexpr int16_t underbarExtraDescender() {
        return get(MathConstant::UnderbarExtraDescender);
    }

    // ── Accent / Miscellaneous ──

    static constexpr int16_t mathLeading() {
        return get(MathConstant::MathLeading);
    }

    static constexpr int16_t accentBaseHeight() {
        return get(MathConstant::AccentBaseHeight);
    }

    static constexpr int16_t flattenedAccentBaseHeight() {
        return get(MathConstant::FlattenedAccentBaseHeight);
    }

    static constexpr int16_t skewedFractionHorizontalGap() {
        return get(MathConstant::SkewedFractionHorizontalGap);
    }

    static constexpr int16_t skewedFractionVerticalGap() {
        return get(MathConstant::SkewedFractionVerticalGap);
    }

    // ── Stretch stack ──

    static constexpr int16_t stretchStackTopShiftUp() {
        return get(MathConstant::StretchStackTopShiftUp);
    }

    static constexpr int16_t stretchStackBottomShiftDown() {
        return get(MathConstant::StretchStackBottomShiftDown);
    }

    static constexpr int16_t stretchStackGapAboveMin() {
        return get(MathConstant::StretchStackGapAboveMin);
    }

    static constexpr int16_t stretchStackGapBelowMin() {
        return get(MathConstant::StretchStackGapBelowMin);
    }

    // ── Design-unit-to-pixel helpers (delegates to free functions) ──

    /// Convert design units to pixels at the given em-size.
    static constexpr int16_t duToPx(int16_t valueDu, int16_t emSizePx) {
        return vpam::duToPx(valueDu, emSizePx);
    }

    /// Convert design units to pixels, guaranteed at least minPx.
    static constexpr int16_t duToPxMin(int16_t valueDu, int16_t emSizePx,
                                       int16_t minPx) {
        return vpam::duToPxMin(valueDu, emSizePx, minPx);
    }

    /// Convert math units (mu) to pixels.
    static constexpr int16_t muToPx(int8_t mu, int16_t emSizePx) {
        return vpam::muToPx(mu, emSizePx);
    }

private:
    MathConstantsProvider() = delete;  // Pure static — never instantiated
};

}  // namespace vpam
