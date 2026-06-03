/*
 * NumOS — STIX Two Math Glyph Variant & Assembly Tables
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4: Static lookup tables mapping base delimiter/operator glyphs to
 * their size variants and vertical assembly parts.
 *
 * All values are in font design units (UPM = 1000 for STIX Two Math).
 * Scale to pixels by multiplying by (fontSizePx / upm).
 *
 * Reference: OpenType 1.9.1, MATH table — MathVariants sub-table.
 */

#pragma once

#include <cstdint>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// GlyphPartRecord — Single piece in a vertical glyph assembly
//
// OpenType stores assembly parts bottom-to-top. The DelimiterAssembler FSM
// reorders them top-to-bottom for execution in the renderer.
//
// Parts overlap by startConnector / endConnector lengths to eliminate
// visual seams (rasterisation gaps) on low-resolution displays.
// ════════════════════════════════════════════════════════════════════════════
struct GlyphPartRecord {
    uint32_t glyphCp;            ///< Unicode codepoint
    int16_t  fullAdvance;        ///< Full glyph height in design units
    int16_t  glyphWidth;         ///< Horizontal advance width in design units
    int16_t  startConnector;     ///< Connector length at glyph start (DU)
    int16_t  endConnector;       ///< Connector length at glyph end (DU)
    bool     isExtender;         ///< Repeatable piece that fills the middle
};

// ════════════════════════════════════════════════════════════════════════════
// SizeVariantRecord — Discrete larger variant of a base glyph
//
// Used when the target height fits within the variant's height without
// needing assembly. Variants are listed in increasing height order.
// ════════════════════════════════════════════════════════════════════════════
struct SizeVariantRecord {
    uint32_t glyphCp;            ///< Unicode codepoint of the variant
    int16_t  height;             ///< Glyph height in design units
    int16_t  glyphWidth;         ///< Horizontal advance width in design units
};

// ════════════════════════════════════════════════════════════════════════════
// MathVariantTable — Complete variant data for one delimiter/operator
//
// Each table covers one base glyph and enumerates:
//   1. The base glyph's natural height and width
//   2. Size variants (increasing height) — used before falling back to assembly
//   3. Assembly parts (stored bottom-to-top per OpenType) — used for tall content
// ════════════════════════════════════════════════════════════════════════════
struct MathVariantTable {
    uint32_t baseCodepoint;          ///< Base glyph (e.g. U+0028 for '(')
    int16_t  baseHeight;             ///< Base glyph height in design units
    int16_t  baseWidth;              ///< Base glyph horizontal advance in design units
    const SizeVariantRecord* sizeVariants;
    uint8_t  sizeVariantCount;
    const GlyphPartRecord*  assemblyParts;
    uint8_t  assemblyPartCount;
    int16_t  italicsCorrection;      ///< MathItalicsCorrection in design units
};

// ════════════════════════════════════════════════════════════════════════════
// Static Variant Tables — STIX Two Math data
// ════════════════════════════════════════════════════════════════════════════

// ── Left Parenthesis '(' (U+0028) ──────────────────────────────────────────
// Assembly glyphs (Misc. Technical block):
//   U+239B  LEFT PARENTHESIS UPPER HOOK
//   U+239C  LEFT PARENTHESIS EXTENSION
//   U+239D  LEFT PARENTHESIS LOWER HOOK
//
// Connector lengths are conservatively 0 for STIX Two Math —
// the glyphs are designed to abut seamlessly at integer alignment.

inline constexpr GlyphPartRecord kLeftParenAssemblyParts[] = {
    // cp      fullAdv  glyphW  startC  endC  ext
    { 0x239C,   404,     250,    0,      0,    true  },  // extender (middle)
    { 0x239D,   406,     250,    0,      0,    false },  // lower hook
    { 0x239B,   406,     250,    0,      0,    false },  // upper hook
};

inline constexpr SizeVariantRecord kLeftParenSizeVariants[] = {
    // Height increases: base glyph first, then larger variants
    { 0x0028,  700,  250 },   // base '(' at natural size
    { 0x239B,  900,  250 },   // upper hook as standalone large paren
};

inline constexpr MathVariantTable kLeftParenVariantTable = {
    0x0028,          // baseCodepoint
    700,             // baseHeight (DU)
    250,             // baseWidth  (DU)
    kLeftParenSizeVariants,
    2,               // sizeVariantCount
    kLeftParenAssemblyParts,
    3,               // assemblyPartCount (stored bottom-to-top per OpenType)
    0,               // italicsCorrection
};

// ── Right Parenthesis ')' (U+0029) ─────────────────────────────────────────
// Assembly glyphs:
//   U+239E  RIGHT PARENTHESIS UPPER HOOK
//   U+239F  RIGHT PARENTHESIS EXTENSION
//   U+23A0  RIGHT PARENTHESIS LOWER HOOK

inline constexpr GlyphPartRecord kRightParenAssemblyParts[] = {
    { 0x239F,   404,     250,    0,      0,    true  },  // extender
    { 0x23A0,   406,     250,    0,      0,    false },  // lower hook
    { 0x239E,   406,     250,    0,      0,    false },  // upper hook
};

inline constexpr SizeVariantRecord kRightParenSizeVariants[] = {
    { 0x0029,  700,  250 },   // base ')' at natural size
    { 0x239E,  900,  250 },   // upper hook as standalone large paren
};

inline constexpr MathVariantTable kRightParenVariantTable = {
    0x0029,
    700, 250,
    kRightParenSizeVariants, 2,
    kRightParenAssemblyParts, 3,
    0,
};

// ── Summation ∑ (U+2211) ──────────────────────────────────────────────────
// STIX Two Math has size variants and assembly for large operators.
// At display style with limits above/below, we use the largest variant
// or assembly. At text style, limits go to the right as sub/superscripts.

inline constexpr GlyphPartRecord kSummationAssemblyParts[] = {
    { 0x2211,   778,     778,    0,      0,    true  },  // ∑ as extender
    { 0x2211,   778,     778,    0,      0,    false },  // lower
    { 0x2211,   778,     778,    0,      0,    false },  // upper
};

inline constexpr SizeVariantRecord kSummationSizeVariants[] = {
    { 0x2211,  778,  778 },   // size variant 0: base ∑
};

inline constexpr MathVariantTable kSummationVariantTable = {
    0x2211,
    778, 778,
    kSummationSizeVariants, 1,
    kSummationAssemblyParts, 3,
    0,
};

// ── N-ary Product ∏ (U+220F) — for NodeBigOp ─────────────────────────────

inline constexpr GlyphPartRecord kProductAssemblyParts[] = {
    { 0x220F,   778,     778,    0,      0,    true  },  // ∏ as extender
    { 0x220F,   778,     778,    0,      0,    false },
    { 0x220F,   778,     778,    0,      0,    false },
};

inline constexpr SizeVariantRecord kProductSizeVariants[] = {
    { 0x220F,  778,  778 },
};

inline constexpr MathVariantTable kProductVariantTable = {
    0x220F,
    778, 778,
    kProductSizeVariants, 1,
    kProductAssemblyParts, 3,
    0,
};

// ── N-ary Intersection ⋂ (U+22C2) — for NodeBigOp ────────────────────────

inline constexpr GlyphPartRecord kIntersectionAssemblyParts[] = {
    { 0x22C2,   778,     778,    0,      0,    true  },
    { 0x22C2,   778,     778,    0,      0,    false },
    { 0x22C2,   778,     778,    0,      0,    false },
};

inline constexpr SizeVariantRecord kIntersectionSizeVariants[] = {
    { 0x22C2,  778,  778 },
};

inline constexpr MathVariantTable kIntersectionVariantTable = {
    0x22C2,
    778, 778,
    kIntersectionSizeVariants, 1,
    kIntersectionAssemblyParts, 3,
    0,
};

// ── N-ary Union ⋃ (U+22C3) — for NodeBigOp ───────────────────────────────

inline constexpr GlyphPartRecord kUnionAssemblyParts[] = {
    { 0x22C3,   778,     778,    0,      0,    true  },
    { 0x22C3,   778,     778,    0,      0,    false },
    { 0x22C3,   778,     778,    0,      0,    false },
};

inline constexpr SizeVariantRecord kUnionSizeVariants[] = {
    { 0x22C3,  778,  778 },
};

inline constexpr MathVariantTable kUnionVariantTable = {
    0x22C3,
    778, 778,
    kUnionSizeVariants, 1,
    kUnionAssemblyParts, 3,
    0,
};

// ── Integral ∫ (U+222B) — for NodeDefIntegral display mode ────────────────
// Integrals typically only have size variants, not assembly.

inline constexpr SizeVariantRecord kIntegralSizeVariants[] = {
    { 0x222B,  990,  444 },   // base ∫
};

inline constexpr MathVariantTable kIntegralVariantTable = {
    0x222B,
    990, 444,
    kIntegralSizeVariants, 1,
    nullptr, 0,  // no assembly — integrals grow via size variants
    0,
};

// ════════════════════════════════════════════════════════════════════════════
// Lookup helper — returns the variant table for a known base codepoint
// ════════════════════════════════════════════════════════════════════════════
inline const MathVariantTable* lookupVariantTable(uint32_t baseCp) {
    switch (baseCp) {
        case 0x0028: return &kLeftParenVariantTable;
        case 0x0029: return &kRightParenVariantTable;
        case 0x2211: return &kSummationVariantTable;
        case 0x220F: return &kProductVariantTable;
        case 0x22C2: return &kIntersectionVariantTable;
        case 0x22C3: return &kUnionVariantTable;
        case 0x222B: return &kIntegralVariantTable;
        default:     return nullptr;
    }
}

}  // namespace vpam
