/*
 * NeoCalculator - NumOS
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
 * CalculusStressTest.cpp — Phase 6 memory & performance stress test.
 *
 * Creates 50 diverse expression trees, differentiates + simplifies each,
 * checks arena memory between iterations. Verifies:
 *   1) No crash across 50 complex differentiations
 *   2) Arena properly resets between iterations (no leak)
 *   3) All results produce valid SymExpr pointers
 *   4) Arena block count stays bounded (≤ 4 blocks)
 *
 * Part of: NumOS CAS — Phase 6 (Calculus & Production Polish)
 */

#include "CalculusStressTest.h"
#include "../src/math/cas/SymExpr.h"
#include "../src/math/cas/SymExprArena.h"
#include "../src/math/cas/SymDiff.h"
#include "../src/math/cas/SymSimplify.h"
#include "../src/math/cas/SymExprToAST.h"
#include "../src/math/MathAST.h"

#include <cmath>
#include <cstdio>

#ifdef ARDUINO
  #include <Arduino.h>
  #define ST_PRINT(...)   Serial.printf(__VA_ARGS__)
  #define ST_PRINTLN(s)   Serial.println(s)
#else
  #define ST_PRINT(...)   printf(__VA_ARGS__)
  #define ST_PRINTLN(s)   printf("%s\n", s)
#endif

namespace cas {

static int _stPass = 0;
static int _stFail = 0;

static void stCheck(const char* name, bool cond) {
    if (cond) {
        _stPass++;
    } else {
        _stFail++;
        ST_PRINT("  FAIL: %s\n", name);
    }
}

// ── Expression builders ────────────────────────────────────────────
// Build diverse expressions using arena factory helpers.

// Build: a*x^n
static SymExpr* buildMonomial(SymExprArena& a, double coeff, int power) {
    SymExpr* x = symVar(a, 'x');
    SymExpr* term;
    if (power == 0) {
        term = symInt(a, (int64_t)coeff);
    } else if (power == 1) {
        if (coeff == 1.0) {
            term = x;
        } else {
            term = symMul(a, symInt(a, (int64_t)coeff), x);
        }
    } else {
        SymExpr* xn = symPow(a, x, symInt(a, power));
        if (coeff == 1.0) {
            term = xn;
        } else {
            term = symMul(a, symInt(a, (int64_t)coeff), xn);
        }
    }
    return term;
}

// Build polynomial: c0 + c1*x + c2*x^2 + ...
// symAddN keeps the terms-array pointer inside the SymAdd node, so the
// array must live in the arena — a stack array would dangle.
static SymExpr* buildPoly(SymExprArena& a, const int* coeffs, int degree) {
    SymExpr* terms[10];
    int count = 0;
    for (int i = 0; i <= degree && count < 10; ++i) {
        if (coeffs[i] != 0) {
            terms[count++] = buildMonomial(a, coeffs[i], i);
        }
    }
    if (count == 0) return symInt(a, 0);
    if (count == 1) return terms[0];
    auto** arr = static_cast<SymExpr**>(a.allocRaw(count * sizeof(SymExpr*)));
    if (!arr) return nullptr;
    for (int i = 0; i < count; ++i) arr[i] = terms[i];
    return symAddN(a, arr, static_cast<uint16_t>(count));
}

// ── Test expressions ───────────────────────────────────────────────
// Returns a new expression for each iteration index (0..49)

static SymExpr* buildTestExpr(SymExprArena& arena, int idx) {
    SymExpr* x = symVar(arena, 'x');

    switch (idx % 25) {
        case 0: {  // x^2
            return symPow(arena, x, symInt(arena, 2));
        }
        case 1: {  // 3x^3 + 2x^2 - x + 5
            int c[] = {5, -1, 2, 3};
            return buildPoly(arena, c, 3);
        }
        case 2: {  // sin(x)
            return symFunc(arena, SymFuncKind::Sin, x);
        }
        case 3: {  // cos(x)
            return symFunc(arena, SymFuncKind::Cos, x);
        }
        case 4: {  // tan(x)
            return symFunc(arena, SymFuncKind::Tan, x);
        }
        case 5: {  // ln(x)
            return symFunc(arena, SymFuncKind::Ln, x);
        }
        case 6: {  // e^x
            return symFunc(arena, SymFuncKind::Exp, x);
        }
        case 7: {  // x^5 - 3x^4 + 2x^3
            int c[] = {0, 0, 0, 2, -3, 1};
            return buildPoly(arena, c, 5);
        }
        case 8: {  // sin(x^2) — chain rule
            return symFunc(arena, SymFuncKind::Sin,
                symPow(arena, x, symInt(arena, 2)));
        }
        case 9: {  // x * sin(x) — product rule
            return symMul(arena, x, symFunc(arena, SymFuncKind::Sin, x));
        }
        case 10: { // e^(x^2) — chain rule
            return symFunc(arena, SymFuncKind::Exp,
                symPow(arena, x, symInt(arena, 2)));
        }
        case 11: { // ln(x^2 + 1)
            return symFunc(arena, SymFuncKind::Ln,
                symAdd(arena,
                    symPow(arena, x, symInt(arena, 2)),
                    symInt(arena, 1)));
        }
        case 12: { // x^10
            return symPow(arena, x, symInt(arena, 10));
        }
        case 13: { // sin(x) * cos(x) — product rule
            return symMul(arena,
                symFunc(arena, SymFuncKind::Sin, x),
                symFunc(arena, SymFuncKind::Cos, x));
        }
        case 14: { // (x^2 + 1)^3 — chain + power rule
            return symPow(arena,
                symAdd(arena,
                    symPow(arena, x, symInt(arena, 2)),
                    symInt(arena, 1)),
                symInt(arena, 3));
        }
        case 15: { // arcsin(x)
            return symFunc(arena, SymFuncKind::ArcSin, x);
        }
        case 16: { // arccos(x)
            return symFunc(arena, SymFuncKind::ArcCos, x);
        }
        case 17: { // arctan(x)
            return symFunc(arena, SymFuncKind::ArcTan, x);
        }
        case 18: { // log10(x)
            return symFunc(arena, SymFuncKind::Log10, x);
        }
        case 19: { // x^(1/2) = sqrt(x)
            return symPow(arena, x, symFrac(arena, 1, 2));
        }
        case 20: { // -x^3 — negation
            return symNeg(arena, symPow(arena, x, symInt(arena, 3)));
        }
        case 21: { // sin(cos(x)) — nested chain rule
            return symFunc(arena, SymFuncKind::Sin,
                symFunc(arena, SymFuncKind::Cos, x));
        }
        case 22: { // e^(sin(x)) — chain rule
            return symFunc(arena, SymFuncKind::Exp,
                symFunc(arena, SymFuncKind::Sin, x));
        }
        case 23: { // x^2 * ln(x) — product rule + log
            return symMul(arena,
                symPow(arena, x, symInt(arena, 2)),
                symFunc(arena, SymFuncKind::Ln, x));
        }
        case 24: { // 7x^6 - 4x^3 + x
            int c[] = {0, 1, 0, -4, 0, 0, 7};
            return buildPoly(arena, c, 6);
        }
    }
    return symInt(arena, 1); // fallback
}

// ════════════════════════════════════════════════════════════════════
// Main stress test
// ════════════════════════════════════════════════════════════════════

void runCalculusStressTest() {
    _stPass = 0;
    _stFail = 0;

    ST_PRINTLN("===========================================");
    ST_PRINTLN(" Phase 6 — Calculus Memory Stress Test");
    ST_PRINTLN(" 50 differentiations in a loop");
    ST_PRINTLN("===========================================");

    SymExprArena arena;
    constexpr int ITERATIONS = 50;
    int successCount = 0;
    int failCount = 0;
    size_t maxUsed = 0;
    int maxBlocks = 0;

    for (int i = 0; i < ITERATIONS; ++i) {
        arena.reset();

        // Verify arena is clean after reset
        size_t usedAfterReset = arena.currentUsed();
        stCheck("arena_clean_after_reset", usedAfterReset == 0);

        // Build expression
        SymExpr* expr = buildTestExpr(arena, i);
        if (!expr) {
            ST_PRINT("  [%2d] SKIP: buildTestExpr returned null\n", i);
            failCount++;
            continue;
        }

        // Differentiate
        SymExpr* raw = SymDiff::diff(expr, 'x', arena);
        if (!raw) {
            ST_PRINT("  [%2d] FAIL: diff returned null for expr: %s\n",
                     i, expr->toString().c_str());
            failCount++;
            continue;
        }

        // Simplify
        SymExpr* simp = SymSimplify::simplify(raw, arena);
        SymExpr* result = simp ? simp : raw;

        // Verify result is valid (can call toString without crash)
        std::string str = result->toString();
        stCheck("result_not_empty", !str.empty());

        // Try SymExprToAST conversion
        vpam::NodePtr ast = SymExprToAST::convert(result);
        stCheck("ast_conversion_ok", ast != nullptr);

        // Track memory
        size_t used = arena.currentUsed();
        int blocks = arena.blockCount();
        if (used > maxUsed) maxUsed = used;
        if (blocks > maxBlocks) maxBlocks = blocks;

        // Verify arena stays bounded
        stCheck("arena_blocks_bounded", blocks <= 4);

        successCount++;

        if (i % 10 == 0) {
            ST_PRINT("  [%2d] OK: d/dx(%s) = %s | arena: %zu bytes, %d blocks\n",
                     i, expr->toString().c_str(), str.c_str(),
                     used, blocks);
        }
    }

    // Final arena reset — verify cleanup. reset() keeps block 0 for warm
    // re-use (MT-04 lazy-first-block contract), so a used arena settles at
    // exactly one block, not zero.
    arena.reset();
    stCheck("final_arena_clean", arena.currentUsed() == 0);
    stCheck("final_arena_one_warm_block", arena.blockCount() <= 1);

    ST_PRINTLN("-------------------------------------------");
    ST_PRINT("  Iterations: %d/%d succeeded\n", successCount, ITERATIONS);
    ST_PRINT("  Peak arena: %zu bytes, %d blocks\n", maxUsed, maxBlocks);
    ST_PRINT("  Unit checks: %d passed, %d failed\n", _stPass, _stFail);
    ST_PRINTLN("-------------------------------------------");

    if (_stFail == 0 && successCount == ITERATIONS) {
        ST_PRINTLN("  ALL STRESS TESTS PASSED");
    } else {
        ST_PRINTLN("  SOME TESTS FAILED");
    }

    ST_PRINTLN("===========================================");
}

} // namespace cas
