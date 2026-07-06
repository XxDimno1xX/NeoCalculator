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
 * InvalidReason — WHY a committed expression was rejected by the classifier
 * (GraphModel::preCacheRPN). The canonical user/script-visible strings are
 * frozen by the classifier contract (NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md §A.2):
 *   Syntax           → "syntax"
 *   OneSided         → "one-sided relation"      (e.g. "x=", "=x", "y<")
 *   MultiRelation    → "multiple relations"      (e.g. "y=x=2", "0<x<1")
 *   UnknownIdent     → "unknown: <ident>"        (e.g. "xy=1" → "unknown: xy")
 *   TooLong          → "expression too long"     (serialised form exceeds 63 chars)
 *   ConstantRelation → "constant relation"       (e.g. "1=2", "pi<e")
 *   Unsupported      → "unsupported node"        (AST node the pipeline can't serialise)
 * An invalid slot NEVER plots, never traces, never tabulates — visible refusal
 * instead of a silent wrong picture (e.g. the old "x=" ⇒ line x=0 bug).
 */
enum class InvalidReason : uint8_t {
    None, Syntax, OneSided, MultiRelation, UnknownIdent,
    TooLong, ConstantRelation, Unsupported
};

/// Canonical base string for an InvalidReason (UnknownIdent's "<ident>" detail
/// lives in CartesianFunction::reasonDetail and is appended by the caller).
inline const char* invalidReasonText(InvalidReason r) {
    switch (r) {
        case InvalidReason::Syntax:           return "syntax";
        case InvalidReason::OneSided:         return "one-sided relation";
        case InvalidReason::MultiRelation:    return "multiple relations";
        case InvalidReason::UnknownIdent:     return "unknown";
        case InvalidReason::TooLong:          return "expression too long";
        case InvalidReason::ConstantRelation: return "constant relation";
        case InvalidReason::Unsupported:      return "unsupported node";
        default:                              return "";
    }
}

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

    // ── x = f(y) explicit-in-y support ─────────────────────────────────────
    // True when the relation is exactly "x = f(y)" — one side is the bare
    // variable x, the other side has no x (e.g. "x=y^2", "x=sin(y)"). The slot
    // still PLOTS as an implicit contour (G = lhs - rhs, marching squares), but
    // the tracer can parametrise it by y and report an exact x = f(y), walking
    // the cursor along y. rpnY holds that single-valued f(y), evaluated with y.
    bool               explicitY;  ///< true ⇒ x = f(y); trace parametrised by y
    std::vector<Token> rpnY;       ///< f(y) RPN (explicitY only), evaluated with y
    bool               rpnYValid;  ///< rpnY compiled OK

    // ── Rejection bookkeeping (classifier contract) ────────────────────────
    // invalidReason is set by preCacheRPN whenever valid == false && len > 0.
    // reasonDetail carries the offending identifier for UnknownIdent ("xy"),
    // truncated to 11 chars. serializeVerdict is a PRE-classification verdict
    // recorded by the AST→text serialiser (GrapherApp::syncASTtoText): the model
    // never sees untruncated text, so overflow (TooLong) and non-serialisable
    // nodes (Unsupported) must be flagged before preCacheRPN runs; preCacheRPN
    // honours a pending verdict by early-returning invalid.
    InvalidReason      invalidReason;    ///< why valid==false (None when valid)
    char               reasonDetail[12]; ///< UnknownIdent: offending identifier
    InvalidReason      serializeVerdict; ///< serialiser verdict (None/TooLong/Unsupported)

    bool isInequality() const { return relation != Relation::Equal; }
    /// True when this slot can be traced/tabulated as a single-valued y=f(x).
    bool isExplicit()   const { return valid && !implicit; }
    /// True when this slot is a curve the tracer can follow: an explicit y=f(x),
    /// an explicit x=f(y), or a general implicit *equation*. Inequalities
    /// (relation != Equal) are half-plane regions, not curves, so never traceable.
    bool isTraceable()  const { return valid && relation == Relation::Equal; }

    CartesianFunction()
        : len(0), valid(false), color(0xFF0000), visible(true), rpnValid(false),
          implicit(false), rpnLValid(false), relation(Relation::Equal),
          explicitY(false), rpnYValid(false),
          invalidReason(InvalidReason::None), serializeVerdict(InvalidReason::None)
    { text[0] = '\0'; reasonDetail[0] = '\0'; }
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
     * Evaluate the x = f(y) function side at the given y, for a slot detected as
     * explicit-in-y (fn.explicitY == true). Returns the world x on the curve, or
     * NAN if the slot is not explicit-in-y or the evaluation fails (domain hole).
     */
    float evalAtY(CartesianFunction& fn, float y);

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

    /// True if the tokenized expression references the variable 'x'/'X'.
    bool referencesX(const char* expr);

    /**
     * Tokenize `expr` and expand juxtaposed single-letter variables into an
     * explicit product ("xx" → x*x, "xy" → x*y), preserving reserved multi-char
     * words (pi/ans/preans) and function calls. Grapher-local: the shared
     * Tokenizer (and CalculationApp) is not modified. Returns false only when
     * the tokenizer rejects the input.
     */
    bool tokenizeExpanded(const char* expr, std::vector<Token>& outTokens);

    /**
     * Structural dry-run of a compiled RPN at the probe point x=1, y=1
     * (classifier contract R11). Returns true when the expression is
     * structurally sound. Structural evaluator errors (unknown identifier,
     * malformed operator/token/stack states) mark the slot invalid and record
     * the reason (UnknownIdent + reasonDetail, or Syntax) — this is what turns
     * "xy=1" from a valid-but-blank plot into a visible refusal. Value-dependent
     * errors at the probe point (domain, div-by-zero, non-finite) are tolerated:
     * "ln(x-5)" stays valid even though ln(-4) fails.
     */
    bool dryRunStructural(const std::vector<Token>& rpn, CartesianFunction& fn);

    /// Compute f1(x) - f2(x)
    float diffAt(CartesianFunction& fn1, CartesianFunction& fn2, float x);

    /// Bisection root finder: find x in [a, b] such that f(x) ≈ 0
    float bisect(CartesianFunction& fn1, CartesianFunction& fn2,
                 float a, float b, int maxIter);
};

} // namespace grapher
