/*
 * NumOS — Glyph Assembly Engine Implementation
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MathGlyphAssembly.h"
#include <algorithm>

#include "../MathTypography.h"  // for duToPx

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// DU → Pixel conversion helpers
// ════════════════════════════════════════════════════════════════════════════

int16_t DelimiterAssembler::duToPxCeil(int16_t du, int16_t emSizePx) {
    // Ceiling division: (du * emSize + upm - 1) / upm
    int32_t num = static_cast<int32_t>(du) * emSizePx + kStixMathUPM - 1;
    return static_cast<int16_t>(num / kStixMathUPM);
}

int16_t DelimiterAssembler::duToPxRound(int16_t du, int16_t emSizePx) {
    int32_t num = static_cast<int32_t>(du) * emSizePx + kStixMathUPM / 2;
    return static_cast<int16_t>(num / kStixMathUPM);
}

// ════════════════════════════════════════════════════════════════════════════
// 6-State FSM — Validate & Split assembly parts
// ════════════════════════════════════════════════════════════════════════════

DelimiterAssembler::FsmResult
DelimiterAssembler::validateAndSplit(const GlyphPartRecord* parts,
                                     uint8_t count) {
    FsmResult result{};
    result.valid = false;
    result.extenderPart = nullptr;

    if (count == 0 || parts == nullptr) {
        result.valid = true;  // empty assembly → just use base glyph
        return result;
    }

    FsmState state = FsmState::START;
    uint8_t nonExtenderCount = 0;

    for (uint8_t i = 0; i < count; ++i) {
        const GlyphPartRecord& part = parts[i];

        if (part.isExtender) {
            // ── Extender piece ────────────────────────────────────────
            switch (state) {
                case FsmState::START:
                case FsmState::BOTTOM_PIECES:
                    // First extender: this is the repeatable piece
                    if (result.extenderPart != nullptr) {
                        // Already seen an extender → duplicate extender error
                        return result;
                    }
                    result.extenderPart = &part;
                    state = FsmState::EXTENDER_SEEN;
                    break;

                case FsmState::EXTENDER_SEEN:
                case FsmState::TOP_PIECES:
                    // Duplicate extender after already seeing one → error
                    return result;

                default:
                    return result;
            }
        } else {
            // ── Non-extender piece ────────────────────────────────────
            nonExtenderCount++;
            if (nonExtenderCount > 3) {
                // Gecko constraint: max 3 non-extender pieces
                return result;
            }

            switch (state) {
                case FsmState::START:
                case FsmState::BOTTOM_PIECES:
                    // Bottom piece (start of OpenType bottom-to-top list)
                    if (result.bottomCount < 3) {
                        result.bottomParts[result.bottomCount++] = &part;
                    }
                    state = FsmState::BOTTOM_PIECES;
                    break;

                case FsmState::EXTENDER_SEEN:
                case FsmState::TOP_PIECES:
                    // Top piece (after extender in OpenType order)
                    if (result.topCount < 3) {
                        result.topParts[result.topCount++] = &part;
                    }
                    state = FsmState::TOP_PIECES;
                    break;

                default:
                    return result;
            }
        }
    }

    result.valid = true;
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// assemble() — Canonical OpenType 1.9.1 assembly algorithm
// ════════════════════════════════════════════════════════════════════════════

DelimiterAssembler::AssemblyResult
DelimiterAssembler::assemble(int16_t targetHeightPx,
                             uint32_t /*baseCp*/,
                             const MathVariantTable& table,
                             int16_t emSizePx) {
    AssemblyResult result;

    // ── Step 1: Size variant search (Spec §4.4) ───────────────────────────
    // Start from index 0 (base glyph), iterate upward until a variant
    // covers the target height or we exhaust the list.
    {
        int16_t baseH = duToPxCeil(table.baseHeight, emSizePx);
        int16_t bestH = baseH;
        uint32_t bestCp = table.baseCodepoint;
        int16_t bestW = duToPxRound(table.baseWidth, emSizePx);

        for (uint8_t i = 0; i < table.sizeVariantCount; ++i) {
            const auto& sv = table.sizeVariants[i];
            int16_t svH = duToPxCeil(sv.height, emSizePx);
            if (svH >= targetHeightPx) {
                // Found a variant that covers the target height
                bestH = svH;
                bestCp = sv.glyphCp;
                bestW = duToPxRound(sv.glyphWidth, emSizePx);
                break;  // First that fits → use it
            }
            // Track the tallest variant seen (for fallback)
            if (svH > bestH) {
                bestH = svH;
                bestCp = sv.glyphCp;
                bestW = duToPxRound(sv.glyphWidth, emSizePx);
            }
        }

        // If the best variant is tall enough AND we don't need assembly,
        // return a single-glyph result.
        if (bestH >= targetHeightPx) {
            result.pieces[0] = { bestCp, false, bestH };
            result.pieceCount = 1;
            result.widthPx = bestW;
            result.totalHeightPx = bestH;
            result.isAssembly = false;
            result.valid = true;
            return result;
        }
    }

    // ── Step 2: Assemble from parts (Spec §4.2, §4.3) ────────────────────
    if (table.assemblyPartCount == 0 || table.assemblyParts == nullptr) {
        // No assembly data → fall back to largest size variant
        int16_t baseH = duToPxCeil(table.baseHeight, emSizePx);
        result.pieces[0] = { table.baseCodepoint, false, baseH };
        result.pieceCount = 1;
        result.widthPx = duToPxRound(table.baseWidth, emSizePx);
        result.totalHeightPx = baseH;
        result.isAssembly = false;
        result.valid = true;
        return result;
    }

    // Validate and split assembly parts (bottom-to-top → bottom/extender/top)
    FsmResult fsm = validateAndSplit(table.assemblyParts,
                                     table.assemblyPartCount);
    if (!fsm.valid) {
        // FSM validation failed → fallback to base glyph
        int16_t baseH = duToPxCeil(table.baseHeight, emSizePx);
        result.pieces[0] = { table.baseCodepoint, false, baseH };
        result.pieceCount = 1;
        result.widthPx = duToPxRound(table.baseWidth, emSizePx);
        result.totalHeightPx = baseH;
        result.isAssembly = false;
        result.valid = true;
        return result;
    }

    // ── Step 3: Build top-to-bottom output ────────────────────────────────
    // OpenType stores: [bottomNonExt…, extender, topNonExt…]  (bottom→top)
    // We output:       [reverse(topNonExt), extender, bottomNonExt] (top→bottom)
    //
    // Final output order for renderer:
    //   pieces[0..topCount-1]     = top non-extenders (reversed)
    //   pieces[topCount]          = extender (repeated to fill gap)
    //   pieces[topCount+1..end]   = bottom non-extenders

    uint8_t outIdx = 0;

    // Determine assembly glyph width (all parts share same horizontal advance)
    int16_t asmWidthDU = table.assemblyParts[0].glyphWidth;
    int16_t asmWidthPx = duToPxRound(asmWidthDU, emSizePx);

    // 3a. Top pieces (reversed from OpenType order, which is bottom→top)
    for (int8_t i = static_cast<int8_t>(fsm.topCount) - 1; i >= 0; --i) {
        const auto* part = fsm.topParts[i];
        result.pieces[outIdx++] = {
            part->glyphCp, false,
            duToPxCeil(part->fullAdvance, emSizePx)
        };
    }

    // 3b. Extender (the repeatable middle piece)
    if (fsm.extenderPart != nullptr && outIdx < kMaxParts) {
        result.pieces[outIdx++] = {
            fsm.extenderPart->glyphCp, true,
            duToPxCeil(fsm.extenderPart->fullAdvance, emSizePx)
        };
    }

    // 3c. Bottom pieces (in OpenType order = the first pieces in the table)
    for (uint8_t i = 0; i < fsm.bottomCount && outIdx < kMaxParts; ++i) {
        const auto* part = fsm.bottomParts[i];
        result.pieces[outIdx++] = {
            part->glyphCp, false,
            duToPxCeil(part->fullAdvance, emSizePx)
        };
    }

    // ── Compute total height ──────────────────────────────────────────────
    int16_t totalH = 0;
    for (uint8_t i = 0; i < outIdx; ++i) {
        totalH = static_cast<int16_t>(totalH + result.pieces[i].heightPx);
    }
    result.totalHeightPx = totalH;

    result.pieceCount = outIdx;
    result.widthPx = asmWidthPx;
    result.isAssembly = (outIdx > 1);
    result.valid = true;
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// glyphWidthPx() — Quick width lookup for layout
// ════════════════════════════════════════════════════════════════════════════

int16_t DelimiterAssembler::glyphWidthPx(uint32_t baseCp, int16_t emSizePx) {
    const MathVariantTable* table = lookupVariantTable(baseCp);
    if (!table) return 0;
    return duToPxRound(table->baseWidth, emSizePx);
}

}  // namespace vpam
