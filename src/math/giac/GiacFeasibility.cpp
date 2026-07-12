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

// GiacFeasibility.cpp — GIAC-FEAS-01 spike adapter (see header). Deliberately
// independent of GiacBridge.cpp: no display-symbol mapping, no desolve
// normalization, no rootof prettifying. Raw engine truth only.

#ifdef NUMOS_GIAC_FEASIBILITY

#include <cmath>
#include <exception>

#include "config.h"
#include "gen.h"
#include "global.h"
#include "prog.h"    // cas_setup
#include "subst.h"
#include "solve.h"   // has_num_coeff
#include "usual.h"

#include "math/giac/GiacFeasibility.h"

namespace giac {
// Not exposed by this snapshot's headers; defined in the lexer TU (same
// forward-declaration GiacBridge.cpp uses).
void check_browser_functions();
void lexer_localization(int lang, const context* contextptr);
}

namespace numos {
namespace giacfeas {

// Own the context by pointer so resetContext() can destroy and rebuild it.
static giac::context* s_ctx = nullptr;

// Mirrors GiacBridge.cpp initGiac() so the spike measures the same engine
// configuration the shipping UART path uses.
static void configureContext(giac::context* ctx) {
    giac::xcas_mode(0, ctx);
    giac::approx_mode(false, ctx);
    giac::complex_mode(false, ctx);
    giac::complex_variables(false, ctx);
    giac::i_sqrt_minus1(1, ctx);
    giac::withsqrt(true, ctx);
    giac::eval_level(ctx) = 1;
    giac::step_infolevel(ctx) = 0;
    giac::language(0, ctx);
    giac::check_browser_functions();
    giac::lexer_localization(0, ctx);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), ctx);
}

bool initRuntime() {
    if (s_ctx) return true;
    try {
        s_ctx = new giac::context;
        configureContext(s_ctx);
        return true;
    } catch (...) {
        s_ctx = nullptr;
        return false;
    }
}

void resetContext() {
    delete s_ctx;
    s_ctx = nullptr;
    initRuntime();
}

GiacEvalResult evaluate(const char* expression) {
    GiacEvalResult r;
    if (!initRuntime()) {
        r.error = "giac runtime init failed";
        return r;
    }
    try {
        giac::gen parsed(std::string(expression), s_ctx);
        giac::gen out = giac::eval(parsed, giac::eval_level(s_ctx), s_ctx);
        if (giac::is_undef(out)) {
            r.error = out.print(s_ctx);
            return r;
        }
        r.ok = true;
        r.text = out.print(s_ctx);
        r.approximate = giac::has_num_coeff(out);
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = e.what();
    } catch (...) {
        r.ok = false;
        r.error = "unknown giac exception";
    }
    return r;
}

struct CompiledExpr::Impl {
    giac::gen expr;      // parsed + evaluated once, retained
    giac::gen var;       // the sample variable as a gen(identificateur)
    bool ok = false;
    std::string error;
};

CompiledExpr::CompiledExpr(const char* expression, const char* variable)
    : _impl(new Impl) {
    if (!initRuntime()) {
        _impl->error = "giac runtime init failed";
        return;
    }
    try {
        _impl->var = giac::gen(giac::identificateur(variable));
        giac::gen parsed(std::string(expression), s_ctx);
        // One symbolic normalization up front; samples then reuse the DAG.
        _impl->expr = giac::eval(parsed, giac::eval_level(s_ctx), s_ctx);
        if (giac::is_undef(_impl->expr)) {
            _impl->error = _impl->expr.print(s_ctx);
            return;
        }
        _impl->ok = true;
    } catch (const std::exception& e) {
        _impl->error = e.what();
    } catch (...) {
        _impl->error = "unknown giac exception";
    }
}

CompiledExpr::~CompiledExpr() { delete _impl; }

bool CompiledExpr::ok() const { return _impl->ok; }
const std::string& CompiledExpr::error() const { return _impl->error; }

double CompiledExpr::evalAt(double x) const {
    if (!_impl->ok) return NAN;
    try {
        giac::gen sub = giac::subst(_impl->expr, _impl->var, giac::gen(x),
                                    false, s_ctx);
        giac::gen num = giac::evalf_double(sub, 1, s_ctx);
        if (num.type == giac::_DOUBLE_) return num._DOUBLE_val;
        return NAN;  // non-real / symbolic residue / undef
    } catch (...) {
        return NAN;
    }
}

} // namespace giacfeas
} // namespace numos

#endif // NUMOS_GIAC_FEASIBILITY
