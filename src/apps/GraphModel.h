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
 * GraphModel.h — Grapher MVC: Model layer
 *
 * Defines CartesianFunction (a single graphable function with its compiled
 * RPN cache) and GraphModel (the evaluation engine).
 *
 * This module knows nothing about pixels, LVGL, or screen coordinates.
 * It is the mathematical core of the Grapher (analogous to Poincaré in
 * NumWorks' architecture).
 */
#pragma once

#include <vector>
#include <cmath>
#include "../math/Tokenizer.h"
#include "../math/Parser.h"
#include "../math/Evaluator.h"
#include "../math/VariableContext.h"

namespace grapher {

/**
 * Relation — how the two sides of a graphed expression are compared.
 *
 * Equal is the default (an equation / explicit function). The four inequality
 * variants drive region shading: the satisfied region is where G = lhs - rhs is
 * negative (Less/LessEqual) or positive (Greater/GreaterEqual). The strict vs
 * non-strict distinction is informational here (boundary styling); the rendered
 * region is the open/closed half either way.
 */
enum class Relation : uint8_t { Equal, Less, Greater, LessEqual, GreaterEqual };

/**
 * CartesianFunction — one graphable function slot.
 *
 * Holds the serialised expression text, its compiled RPN cache for fast
 * evaluation, the display colour and validity flag.
 */
struct CartesianFunction {
    char     text[64];   ///< Expression string (e.g. "y=2*x+3")
    int      len;        ///< strlen(text)
    bool     valid;      ///< Expression is parsed and evaluable
    uint32_t color;      ///< Plot colour (RGB888)
    bool     visible;    ///< User visibility toggle

    // Cached RPN — avoids re-tokenising on every evalAt() call.
    // Explicit y=f(x): rpn holds f(x), evaluated with x only (evalAt).
    // Implicit F(x,y)=0: rpn holds the RHS and rpnL the LHS, both evaluated
    //   with x AND y; the plotted contour is G(x,y) = lhs - rhs = 0 (evalImplicit).
    std::vector<Token> rpn;
    bool               rpnValid;

    // ── Implicit-equation / inequality support ────────────────────────────
    // `implicit` is true for any relation that is NOT a single-valued y=f(x):
    // general implicit equations (G=0 contour) AND inequalities (G≷0 region).
    // It gates the contour/region renderer and makes evalAt() return NAN so the
    // y=f(x) consumers (trace, table, auto-fit) gracefully skip the slot.
    bool               implicit;   ///< true ⇒ plot via G(x,y)=lhs-rhs (contour/region)
    std::vector<Token> rpnL;       ///< LHS RPN (implicit only); empty ⇒ lhs = 0
    bool               rpnLValid;  ///< LHS RPN compiled OK
    Relation           relation;   ///< Equal (equation) or an inequality variant

    bool isInequality() const { return relation != Relation::Equal; }
    /// True when this slot can be traced/tabulated as a single-valued y=f(x).
    bool isExplicit()   const { return valid && !implicit; }

    CartesianFunction()
        : len(0), valid(false), color(0xFF0000), visible(true), rpnValid(false),
          implicit(false), rpnLValid(false), relation(Relation::Equal)
    { text[0] = '\0'; }
};

/**
 * regionHolds — does the inequality region include the point whose implicit
 * residual is `g` (= lhs - rhs, as returned by GraphModel::evalImplicit)?
 *
 * Less/LessEqual  ⇒ lhs < rhs ⇒ g < 0.
 * Greater/Greater­Equal ⇒ lhs > rhs ⇒ g > 0.
 * A NAN residual (domain hole) is never inside the region. Equal relations have
 * no region (false). The strict/non-strict boundary itself is sub-pixel-thin, so
 * shading uses the open test either way; the boundary is drawn separately.
 */
inline bool regionHolds(Relation rel, float g) {
    if (std::isnan(g)) return false;
    switch (rel) {
        case Relation::Less:
        case Relation::LessEqual:    return g < 0.0f;
        case Relation::Greater:
        case Relation::GreaterEqual: return g > 0.0f;
        default:                     return false;
    }
}

/**
 * Point of Interest: root, extremum, or intersection.
 */
struct PointOfInterest {
    float x, y;
    char  label[24];  // "Root", "Min", "Intersection", etc.
};

/**
 * GraphModel — mathematical state and evaluation engine.
 *
 * Owns the Tokenizer/Parser/Evaluator/VariableContext required to compile
 * and evaluate CartesianFunction expressions.  The function array itself
 * lives in GrapherApp (the controller) so the UI can access text/color
 * without going through the model; GrapherApp passes individual
 * CartesianFunction references into the model's methods.
 */
class GraphModel {
public:
    GraphModel() = default;

    /**
     * Compile the RHS of fn.text into fn.rpn.
     * Sets fn.rpnValid and fn.valid accordingly.
     */
    void preCacheRPN(CartesianFunction& fn);

    /**
     * Evaluate fn at x using its cached RPN.
     * Falls back to a full parse if the cache is missing.
     * Returns NAN on any error.
     */
    float evalAt(CartesianFunction& fn, float x);

    /**
     * Evaluate the implicit residual G(x,y) = lhs(x,y) - rhs(x,y) for an
     * implicit equation (fn.implicit == true). The plotted curve is the zero
     * contour G(x,y) = 0. Returns NAN on any error (e.g. domain violation).
     * For an explicit function this returns NAN (use evalAt instead).
     */
    float evalImplicit(CartesianFunction& fn, float x, float y);

    /**
     * Find intersection points of two functions in the given range.
     * Uses bisection to refine roots of (f1(x) - f2(x)) = 0.
     *
     * @param fn1, fn2: The two functions to intersect
     * @param xMin, xMax: Search range
     * @param maxPoints: Max number of intersections to find
     * @param maxIterations: Bisection refinement iterations
     * @return Vector of intersection points
     */
    std::vector<PointOfInterest> findIntersections(
        CartesianFunction& fn1,
        CartesianFunction& fn2,
        float xMin, float xMax,
        int maxPoints = 10,
        int maxIterations = 50
    );

private:
    Tokenizer      _tok;
    Parser         _par;
    Evaluator      _eval;
    VariableContext _vars;

    /// Return pointer to the RHS of expr (after '='), or nullptr.
    static const char* getExprRHS(const char* expr);

    /// Compile a (normalized) sub-expression string into RPN.
    /// Returns true on success, filling outRpn; false otherwise.
    bool compileExpr(const char* expr, std::vector<Token>& outRpn);

    /// True if the tokenized expression references the variable 'y'/'Y'.
    bool referencesY(const char* expr);

    /// Compute f1(x) - f2(x)
    float diffAt(CartesianFunction& fn1, CartesianFunction& fn2, float x);

    /// Bisection root finder: find x in [a, b] such that f(x) ≈ 0
    float bisect(CartesianFunction& fn1, CartesianFunction& fn2,
                 float a, float b, int maxIter);
};

} // namespace grapher
