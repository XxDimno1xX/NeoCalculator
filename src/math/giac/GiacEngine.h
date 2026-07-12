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
 * GiacEngine.h — GIAC-A01 production seam over the in-process Giac engine.
 *
 * This is the ONE owner of the giac::context. Future Calculation and Grapher
 * migrations call this API; nothing outside src/math/giac/ may touch Giac
 * types directly (this header exposes none). The legacy GiacBridge UART path
 * borrows the same context through GiacEngineInternal.h during the
 * transition, so there is exactly one symbolic state in the system.
 *
 * Contract:
 *  - Single-threaded, NON-reentrant. Calls made while another engine call is
 *    on the stack are rejected with Status::Unsupported (never queued).
 *  - begin() is idempotent; every public entry point self-initializes.
 *  - reset() destroys and rebuilds the context: all Giac-side variable
 *    assignments (a:=5) are forgotten, and every CompiledExpression from
 *    before the reset becomes invalid (evaluateNumeric returns false — no
 *    silent stale-context evaluation).
 *  - Angle mode: synchronized from AngleModeRuntime (vpam::g_angleMode) at
 *    every evaluate/simplify/compileNumeric/evaluateNumeric entry. The legacy
 *    UART path pins radians instead (its historical behavior).
 *  - Semantics are Giac's, reported honestly: 1/0 -> "infinity"/oo is Ok
 *    (not an error); plain evaluate() does NOT collect like terms
 *    (x-2*x stays x-2*x — use simplify() for -x); undef and parser
 *    diagnostics come back as non-Ok statuses, never as ordinary values.
 *  - Variable policy: Giac-side assignments persist in the context until
 *    reset(); the engine does not mirror NumOS VariableManager (a later
 *    migration decides that mapping).
 */

#pragma once

#include <cstdint>
#include <string>

namespace numos {

enum class MathEngineStatus : uint8_t {
    Ok,
    Undefined,        // evaluation produced Giac undef without a diagnosis
    ParseError,       // input never became a valid expression
    EvaluationError,  // Giac raised/diagnosed an error during eval
    Unsupported,      // rejected by the engine contract (e.g. reentrancy)
    OutOfMemory
};

struct MathEngineResult {
    MathEngineStatus status = MathEngineStatus::Unsupported;
    std::string exactText;        // printed result (engine print settings)
    std::string approximateText;  // evalf of the result when it adds info
    std::string diagnostic;       // parser/engine messages, never in exactText
    bool ok() const { return status == MathEngineStatus::Ok; }
};

/**
 * Opaque handle to a parse-once/evaluate-many numeric expression
 * (Grapher-shaped). Movable, non-copyable; owns its Giac-side state
 * privately. Invalidated by GiacEngine::reset() (generation check).
 */
class CompiledExpression {
public:
    CompiledExpression();                    // empty/invalid handle
    ~CompiledExpression();
    CompiledExpression(CompiledExpression&& other) noexcept;
    CompiledExpression& operator=(CompiledExpression&& other) noexcept;
    CompiledExpression(const CompiledExpression&) = delete;
    CompiledExpression& operator=(const CompiledExpression&) = delete;

    /// Parsed successfully AND not orphaned by an engine reset.
    bool valid() const;
    /// Parse/compile diagnostic when !valid() at compile time.
    const std::string& diagnostic() const;

private:
    friend class GiacEngine;
    struct Impl;
    Impl* _impl;  // null for the empty handle
};

class GiacEngine {
public:
    /// The single engine (and single giac::context) in the system.
    static GiacEngine& instance();

    /// Idempotent. Returns false only if the context could not be created.
    bool begin();

    /// Destroy + rebuild the context. See header contract.
    void reset();

    /// Plain Giac eval of one expression. No implicit simplification.
    MathEngineResult evaluate(const char* expression);

    /// Explicit simplify mode (giac _simplify on the parsed input).
    MathEngineResult simplify(const char* expression);

    /**
     * Parse + evaluate once for repeated numeric sampling in `variable`
     * (default "x"). Check .valid() on the returned handle.
     */
    CompiledExpression compileNumeric(const char* expression,
                                      const char* variable = "x");

    /**
     * Numeric value at x without any string parsing. Returns true and sets
     * `out` for real samples, including signed infinities at poles
     * (discontinuities are preserved as non-finite values). Returns false
     * (out = NaN) for invalid/stale handles and non-real/symbolic residues.
     */
    bool evaluateNumeric(const CompiledExpression& expr, double x,
                         double& out);

    GiacEngine(const GiacEngine&) = delete;
    GiacEngine& operator=(const GiacEngine&) = delete;

private:
    GiacEngine() = default;
    struct State;
    State* _state = nullptr;   // created by begin()
    bool _inCall = false;      // reentrancy rejection
    uint32_t _generation = 0;  // bumped by reset(); stamps compiled handles
};

} // namespace numos
