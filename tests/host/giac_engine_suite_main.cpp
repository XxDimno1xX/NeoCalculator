// giac_engine_suite_main.cpp — GIAC-A01 host harness for the PRODUCTION
// numos::GiacEngine seam (src/math/giac/GiacEngine.h). Successor of the
// GIAC-FEAS-01 spike harness; the experimental adapter is gone and this
// drives the same TU the firmware and emulator link.
//
// Build + run: scripts/build-giac-host-harness.sh
//
// Output is line-oriented:
//   EVAL|<expr>|status=Ok|us=123|<exact>|approx=<...>
//   CHECK|<label>|PASS
//   SWEEP|<expr>|n=1000|total_ms=..|per_point_us=..|nonfinite=..|rejected=..
//   FOOTER|pass=..|fail=..

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <chrono>

#include "math/giac/GiacEngine.h"
#include "math/AngleModeRuntime.h"

// Defined in host_rss_probe.cpp — its own TU because <windows.h> cannot
// coexist with NumOS headers (INPUT/TokenType collisions).
size_t hostCurrentRssKb();
static size_t currentRssKb() { return hostCurrentRssKb(); }

using Clock = std::chrono::steady_clock;
static long long usSince(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               Clock::now() - t0).count();
}

static int g_pass = 0, g_fail = 0;

static const char* statusName(numos::MathEngineStatus s) {
    switch (s) {
        case numos::MathEngineStatus::Ok:              return "Ok";
        case numos::MathEngineStatus::Undefined:       return "Undefined";
        case numos::MathEngineStatus::ParseError:      return "ParseError";
        case numos::MathEngineStatus::EvaluationError: return "EvaluationError";
        case numos::MathEngineStatus::Unsupported:     return "Unsupported";
        case numos::MathEngineStatus::OutOfMemory:     return "OutOfMemory";
    }
    return "?";
}

static void check(bool cond, const char* label, const std::string& detail = "") {
    (cond ? g_pass : g_fail)++;
    printf("CHECK|%s|%s%s%s\n", label, cond ? "PASS" : "FAIL",
           detail.empty() ? "" : "|", detail.c_str());
}

// Run one expression through the engine in evaluate or simplify mode and
// assert the status (and optionally an exact-text substring).
static void runCase(const char* expr, bool simplifyMode,
                    numos::MathEngineStatus want,
                    const char* expectContains = nullptr) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    auto t0 = Clock::now();
    numos::MathEngineResult r =
        simplifyMode ? eng.simplify(expr) : eng.evaluate(expr);
    long long us = usSince(t0);
    bool pass = (r.status == want);
    if (pass && expectContains &&
        r.exactText.find(expectContains) == std::string::npos)
        pass = false;
    (pass ? g_pass : g_fail)++;
    printf("EVAL|%s%s|status=%s|us=%lld|%s|approx=%s%s\n",
           simplifyMode ? "simplify:" : "", expr, statusName(r.status), us,
           r.ok() ? r.exactText.c_str() : r.diagnostic.c_str(),
           r.approximateText.c_str(), pass ? "" : "|UNEXPECTED");
}

static void sweep(const char* expr, double x0, double x1, int n) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    numos::CompiledExpression ce = eng.compileNumeric(expr);
    if (!ce.valid()) {
        printf("SWEEP|%s|COMPILE_FAIL|%s\n", expr, ce.diagnostic().c_str());
        g_fail++;
        return;
    }
    int nonfinite = 0, rejected = 0;
    double sum = 0;
    auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        double x = x0 + (x1 - x0) * i / (n - 1);
        double y;
        if (!eng.evaluateNumeric(ce, x, y)) { ++rejected; continue; }
        if (std::isnan(y) || std::isinf(y)) ++nonfinite;
        else sum += y;
    }
    long long us = usSince(t0);
    printf("SWEEP|%s|n=%d|total_ms=%.1f|per_point_us=%.2f|nonfinite=%d|rejected=%d|checksum=%.6g\n",
           expr, n, us / 1000.0, (double)us / n, nonfinite, rejected, sum);
    g_pass++;
}

int main() {
    using numos::GiacEngine;
    using numos::MathEngineStatus;
    GiacEngine& eng = GiacEngine::instance();

    printf("RSS_START|rss_kb=%zu\n", currentRssKb());

    // --- lifecycle: initialization exactly once, begin() idempotent ---
    auto t0 = Clock::now();
    bool first = eng.begin();
    long long initUs = usSince(t0);
    bool second = eng.begin();
    printf("INIT|first=%d|second=%d|us=%lld\n", first ? 1 : 0, second ? 1 : 0,
           initUs);
    check(first && second, "begin-idempotent");

    t0 = Clock::now();
    numos::MathEngineResult firstEval = eng.evaluate("2+2");
    printf("FIRST_EVAL|status=%s|us=%lld|%s\n", statusName(firstEval.status),
           usSince(t0), firstEval.exactText.c_str());
    check(firstEval.ok() && firstEval.exactText == "4", "first-eval");

    // --- the 24-expression representative suite (GIAC-FEAS-01 lineage) ---
    // Arithmetic
    runCase("2+2", false, MathEngineStatus::Ok, "4");
    runCase("1/2+1/3", false, MathEngineStatus::Ok, "5/6");
    runCase("2^100", false, MathEngineStatus::Ok,
            "1267650600228229401496703205376");
    // Symbolic. Documented semantics: plain evaluate does NOT collect like
    // terms; explicit simplify mode does.
    runCase("x-2*x", false, MathEngineStatus::Ok, "x-2*x");
    runCase("x-2*x", true, MathEngineStatus::Ok, "-x");
    runCase("regroup(x-2*x)", false, MathEngineStatus::Ok, "-x");
    runCase("simplify((x^2-1)/(x-1))", false, MathEngineStatus::Ok, "x+1");
    runCase("(x^2-1)/(x-1)", true, MathEngineStatus::Ok, "x+1");
    runCase("factor(x^3-6*x^2+11*x-6)", false, MathEngineStatus::Ok,
            "(x-1)*(x-2)*(x-3)");
    // Equations
    runCase("solve(x^2-2=0,x)", false, MathEngineStatus::Ok, "sqrt(2)");
    runCase("solve(ln(x)=1,x)", false, MathEngineStatus::Ok, "exp(1)");
    // Calculus
    runCase("diff(sin(x)^2,x)", false, MathEngineStatus::Ok, "2*cos(x)*sin(x)");
    runCase("integrate(x^2,x)", false, MathEngineStatus::Ok, "x^3");
    // Constants / exactness
    runCase("pi", false, MathEngineStatus::Ok, "pi");
    runCase("sqrt(2)", false, MathEngineStatus::Ok, "sqrt(2)");
    runCase("sin(pi/6)", false, MathEngineStatus::Ok, "1/2");
    // Grapher-oriented
    runCase("simplify(1/x)", false, MathEngineStatus::Ok, "1/x");
    runCase("diff(sin(x),x)", false, MathEngineStatus::Ok, "cos(x)");
    // Failure semantics, reported honestly
    runCase("2+*3", false, MathEngineStatus::ParseError);
    runCase("1/0", false, MathEngineStatus::Ok, "oo");          // Giac infinity
    runCase("0/0", false, MathEngineStatus::Undefined);
    runCase("thisfunctiondoesnotexist(4)", false, MathEngineStatus::Ok,
            "thisfunctiondoesnotexist(4)");                     // symbolic echo
    runCase("idivis(x)", false, MathEngineStatus::EvaluationError);
    runCase("", false, MathEngineStatus::ParseError);

    // approximate companion text
    {
        numos::MathEngineResult r = eng.evaluate("pi");
        check(r.ok() && r.approximateText.rfind("3.14", 0) == 0,
              "pi-approximate-text", r.approximateText);
    }

    // --- variable assignment, persistence, clearing ---
    check(eng.evaluate("giacvar:=5").ok(), "var-assign");
    {
        numos::MathEngineResult r = eng.evaluate("giacvar");
        check(r.ok() && r.exactText == "5", "var-persists", r.exactText);
    }
    check(eng.evaluate("purge(giacvar)").ok(), "var-purge");
    {
        numos::MathEngineResult r = eng.evaluate("giacvar");
        check(r.ok() && r.exactText == "giacvar", "var-cleared", r.exactText);
    }

    // --- reset() forgets engine-side state ---
    check(eng.evaluate("resetvar:=7").ok(), "reset-var-assign");
    eng.reset();
    {
        numos::MathEngineResult r = eng.evaluate("resetvar");
        check(r.ok() && r.exactText == "resetvar", "reset-clears-vars",
              r.exactText);
    }

    // --- DEG/RAD synchronization with AngleModeRuntime ---
    numos::setAngleMode(vpam::AngleMode::DEG);
    {
        numos::MathEngineResult r = eng.evaluate("sin(30)");
        check(r.ok() && r.exactText == "1/2", "deg-sin30", r.exactText);
    }
    numos::setAngleMode(vpam::AngleMode::RAD);
    {
        numos::MathEngineResult r = eng.evaluate("sin(30)");
        check(r.ok() && r.exactText != "1/2", "rad-sin30-differs", r.exactText);
        numos::MathEngineResult r2 = eng.evaluate("sin(pi/6)");
        check(r2.ok() && r2.exactText == "1/2", "rad-sin-pi6", r2.exactText);
    }

    // --- repeated warm evaluation + memory stability (>= 1000 evals) ---
    {
        eng.evaluate("diff(sin(x)^2,x)");  // warm
        size_t rssBefore = currentRssKb();
        auto tw = Clock::now();
        const int N = 1000;
        int okCount = 0;
        for (int i = 0; i < N; ++i)
            if (eng.evaluate("diff(sin(x)^2,x)").ok()) ++okCount;
        long long us = usSince(tw);
        size_t rssAfter = currentRssKb();
        long long deltaKb = (long long)rssAfter - (long long)rssBefore;
        printf("WARM|diff(sin(x)^2,x)|n=%d|ok=%d|avg_us=%.1f|rss_delta_kb=%lld\n",
               N, okCount, (double)us / N, deltaKb);
        check(okCount == N, "warm-1000-all-ok");
        check(deltaKb < 1024, "warm-1000-rss-stable");
    }

    // --- compiled expressions: two retained simultaneously, interleaved ---
    {
        numos::CompiledExpression cSin = eng.compileNumeric("sin(x)");
        numos::CompiledExpression cPoly = eng.compileNumeric("x^2-2");
        check(cSin.valid() && cPoly.valid(), "compile-two-simultaneous");
        double a = 0, b = 0, c = 0, d = 0;
        bool ok = eng.evaluateNumeric(cSin, 0.0, a) &&
                  eng.evaluateNumeric(cPoly, 2.0, b) &&
                  eng.evaluateNumeric(cSin, M_PI / 2.0, c) &&
                  eng.evaluateNumeric(cPoly, -3.0, d);
        check(ok && std::fabs(a) < 1e-12 && std::fabs(b - 2.0) < 1e-12 &&
                  std::fabs(c - 1.0) < 1e-12 && std::fabs(d - 7.0) < 1e-12,
              "compile-interleaved-values");

        // pole behavior: 1/x at exactly 0 -> unsigned infinity -> rejected
        numos::CompiledExpression cInv = eng.compileNumeric("1/x");
        double pole = 0;
        check(cInv.valid() && !eng.evaluateNumeric(cInv, 0.0, pole),
              "pole-rejected-safely");
        // non-numeric residue: unresolved second symbol
        numos::CompiledExpression cXY = eng.compileNumeric("x+unboundsym");
        double resid = 0;
        check(cXY.valid() && !eng.evaluateNumeric(cXY, 1.0, resid),
              "symbolic-residue-rejected");

        // invalid/empty handles
        numos::CompiledExpression empty;
        double dummy = 0;
        check(!empty.valid() && !eng.evaluateNumeric(empty, 1.0, dummy),
              "empty-handle-rejected");
        numos::CompiledExpression bad = eng.compileNumeric("2+*3");
        check(!bad.valid() && !eng.evaluateNumeric(bad, 1.0, dummy),
              "parse-fail-handle-rejected", bad.diagnostic());

        // engine reset orphans previously compiled handles — no stale context
        check(cSin.valid(), "handle-valid-pre-reset");
        eng.reset();
        check(!cSin.valid() && !eng.evaluateNumeric(cSin, 1.0, dummy),
              "handle-invalid-post-reset");
        numos::CompiledExpression cSin2 = eng.compileNumeric("sin(x)");
        double post = 0;
        check(cSin2.valid() && eng.evaluateNumeric(cSin2, 0.0, post) &&
                  std::fabs(post) < 1e-12,
              "recompile-after-reset");
    }

    // --- Grapher sweeps: 1,000 and 10,000 points, no per-point parsing ---
    sweep("sin(x)", -5.0, 5.0, 1000);
    sweep("1/x", -5.0, 5.0, 1000);
    sweep("x^2-2", -5.0, 5.0, 1000);
    sweep("sin(x)", -5.0, 5.0, 10000);
    sweep("1/x", -5.0, 5.0, 10000);
    sweep("x^2-2", -5.0, 5.0, 10000);

    printf("RSS_END|rss_kb=%zu\n", currentRssKb());
    printf("FOOTER|pass=%d|fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
