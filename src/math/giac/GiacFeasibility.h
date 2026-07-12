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

// GiacFeasibility.h — GIAC-FEAS-01 spike adapter.
//
// PROVISIONAL API. This is a feasibility probe for using Giac as the primary
// in-process engine under Calculation and as the semantic frontend for
// Grapher. It is NOT the final engine abstraction and must not be consumed
// by apps. Compiled only when NUMOS_GIAC_FEASIBILITY is defined (host
// feasibility harness / opt-in firmware env); an empty TU otherwise.

#pragma once

#ifdef NUMOS_GIAC_FEASIBILITY

#include <string>

namespace numos {
namespace giacfeas {

struct GiacEvalResult {
    bool ok = false;
    std::string text;    // printed result when ok
    std::string error;   // diagnostic when !ok
    bool approximate = false;  // result carries floating-point coefficients
};

// Idempotent runtime/context initialization (exact mode, real domain,
// radians). Returns false if Giac could not be initialized.
bool initRuntime();

// Destroy and recreate the evaluation context (failure-behavior probe:
// verifies state does not leak across a reset).
void resetContext();

// One-shot textual evaluation — the Calculation-shaped path.
// Never throws; failures are reported through the result struct.
GiacEvalResult evaluate(const char* expression);

// Grapher-shaped path: parse + symbolically prepare an expression of one
// variable ONCE, then evaluate it numerically many times with no string
// parsing per sample.
class CompiledExpr {
public:
    CompiledExpr(const char* expression, const char* variable);
    ~CompiledExpr();
    CompiledExpr(const CompiledExpr&) = delete;
    CompiledExpr& operator=(const CompiledExpr&) = delete;

    bool ok() const;
    const std::string& error() const;
    // Numeric value at x. Returns NaN on domain error / non-real result.
    double evalAt(double x) const;

private:
    struct Impl;
    Impl* _impl;
};

} // namespace giacfeas
} // namespace numos

#endif // NUMOS_GIAC_FEASIBILITY
