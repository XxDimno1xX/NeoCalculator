// giac_feasibility_main.cpp — GIAC-FEAS-01 host harness.
//
// Drives the spike adapter (src/math/giac/GiacFeasibility.h) through the
// representative expression suite, failure-behavior probes, repeated-eval
// stability, and Grapher-shaped 1000-sample numeric sweeps.
// Build: scripts/build-giac-feasibility-host.sh
//
// Output is line-oriented and machine-readable:
//   EVAL|<expr>|ok=1|approx=0|us=123|<result text>
//   SWEEP|<expr>|n=1000|total_ms=..|per_point_us=..|nan=..
//   STAB|iter=..|rss_kb=..
//   FOOTER|pass=..|fail=..

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>

#include "math/giac/GiacFeasibility.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
static size_t currentRssKb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (size_t)(pmc.WorkingSetSize / 1024);
    return 0;
}
static size_t peakRssKb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (size_t)(pmc.PeakWorkingSetSize / 1024);
    return 0;
}
#else
#include <fstream>
static size_t readStatusKb(const char* key) {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind(key, 0) == 0) {
            size_t kb = 0;
            sscanf(line.c_str() + strlen(key), " %zu", &kb);
            return kb;
        }
    }
    return 0;
}
static size_t currentRssKb() { return readStatusKb("VmRSS:"); }
static size_t peakRssKb() { return readStatusKb("VmHWM:"); }
#endif

using Clock = std::chrono::steady_clock;
static long long usSince(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               Clock::now() - t0).count();
}

static int g_pass = 0, g_fail = 0;

// expectOk: run expression, require success; expectContains (optional):
// substring that must appear in the printed result.
static void runCase(const char* expr, bool expectOk,
                    const char* expectContains = nullptr) {
    auto t0 = Clock::now();
    numos::giacfeas::GiacEvalResult r = numos::giacfeas::evaluate(expr);
    long long us = usSince(t0);
    bool pass = (r.ok == expectOk);
    if (pass && r.ok && expectContains &&
        r.text.find(expectContains) == std::string::npos)
        pass = false;
    (pass ? g_pass : g_fail)++;
    printf("EVAL|%s|ok=%d|approx=%d|us=%lld|%s%s%s\n", expr, r.ok ? 1 : 0,
           r.approximate ? 1 : 0, us,
           r.ok ? r.text.c_str() : r.error.c_str(),
           pass ? "" : "|EXPECT_FAIL wanted=",
           pass ? "" : (expectContains ? expectContains : (expectOk ? "ok" : "error")));
}

static void sweep(const char* expr, double x0, double x1, int n) {
    numos::giacfeas::CompiledExpr ce(expr, "x");
    if (!ce.ok()) {
        printf("SWEEP|%s|COMPILE_FAIL|%s\n", expr, ce.error().c_str());
        g_fail++;
        return;
    }
    int nanCount = 0;
    double sum = 0;
    auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        double x = x0 + (x1 - x0) * i / (n - 1);
        double y = ce.evalAt(x);
        if (std::isnan(y) || std::isinf(y)) ++nanCount;
        else sum += y;
    }
    long long us = usSince(t0);
    printf("SWEEP|%s|n=%d|total_ms=%.1f|per_point_us=%.1f|nonfinite=%d|checksum=%.6g\n",
           expr, n, us / 1000.0, (double)us / n, nanCount, sum);
    g_pass++;
}

// Reference: what re-parsing per sample would cost (the thing CompiledExpr
// exists to avoid).
static void sweepReparse(const char* exprFmt, double x0, double x1, int n) {
    char buf[128];
    auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        double x = x0 + (x1 - x0) * i / (n - 1);
        snprintf(buf, sizeof(buf), exprFmt, x);
        numos::giacfeas::evaluate(buf);
    }
    long long us = usSince(t0);
    printf("SWEEP_REPARSE|%s|n=%d|total_ms=%.1f|per_point_us=%.1f\n", exprFmt,
           n, us / 1000.0, (double)us / n);
}

int main(int argc, char** argv) {
    // Optional override of sweep sample count (scaling sanity checks).
    int sweepN = (argc > 1) ? atoi(argv[1]) : 1000;
    if (sweepN < 2) sweepN = 1000;
    printf("RSS_START|rss_kb=%zu\n", currentRssKb());

    // --- init + cold timings ---
    auto t0 = Clock::now();
    bool init = numos::giacfeas::initRuntime();
    printf("INIT|ok=%d|us=%lld\n", init ? 1 : 0, usSince(t0));
    if (!init) { printf("FOOTER|pass=0|fail=1\n"); return 1; }
    printf("RSS_AFTER_INIT|rss_kb=%zu\n", currentRssKb());

    t0 = Clock::now();
    numos::giacfeas::GiacEvalResult first = numos::giacfeas::evaluate("2+2");
    printf("FIRST_EVAL|ok=%d|us=%lld|%s\n", first.ok ? 1 : 0, usSince(t0),
           first.text.c_str());

    // --- representative suite ---
    // Arithmetic
    runCase("2+2", true, "4");
    runCase("1/2+1/3", true, "5/6");
    runCase("2^100", true, "1267650600228229401496703205376");
    // Symbolic. NOTE: with the production context settings (eval_level=1,
    // exact mode) Giac does not auto-collect like terms on plain eval;
    // Calculation would need an explicit regroup/simplify pass for display.
    runCase("x-2*x", true);
    runCase("regroup(x-2*x)", true, "-x");
    runCase("simplify((x^2-1)/(x-1))", true, "x+1");
    runCase("factor(x^3-6*x^2+11*x-6)", true);
    // Equations
    runCase("solve(x^2-2=0,x)", true, "sqrt(2)");
    runCase("solve(ln(x)=1,x)", true);
    // Calculus
    runCase("diff(sin(x)^2,x)", true);
    runCase("integrate(x^2,x)", true, "x^3");
    // Constants / exactness
    runCase("pi", true, "pi");
    runCase("sqrt(2)", true, "sqrt(2)");
    runCase("sin(pi/6)", true, "1/2");
    // Grapher-oriented symbolic prep
    runCase("simplify(1/x)", true, "1/x");
    runCase("diff(sin(x),x)", true, "cos(x)");

    // --- warm timing: 100x of a mid-weight symbolic call ---
    {
        numos::giacfeas::evaluate("diff(sin(x)^2,x)");  // ensure warm
        auto tw = Clock::now();
        const int N = 100;
        for (int i = 0; i < N; ++i)
            numos::giacfeas::evaluate("diff(sin(x)^2,x)");
        printf("WARM|diff(sin(x)^2,x)|n=%d|avg_us=%.1f\n", N,
               (double)usSince(tw) / N);
    }

    // --- failure behavior ---
    runCase("2+*3", false);                 // syntax error -> undef
    runCase("1/0", true, "oo");             // Giac semantics: unsigned infinity
    runCase("thisfunctiondoesnotexist(4)", true); // unknown fn: giac keeps symbolic
    runCase("idivis(x)", false);            // integer op on symbol

    // --- context reset ---
    numos::giacfeas::evaluate("feasvar:=42");
    numos::giacfeas::GiacEvalResult before = numos::giacfeas::evaluate("feasvar");
    numos::giacfeas::resetContext();
    numos::giacfeas::GiacEvalResult after = numos::giacfeas::evaluate("feasvar");
    bool resetOk = before.ok && before.text == "42" && after.ok &&
                   after.text == "feasvar";
    printf("CTX_RESET|before=%s|after=%s|%s\n", before.text.c_str(),
           after.text.c_str(), resetOk ? "PASS" : "FAIL");
    (resetOk ? g_pass : g_fail)++;

    // --- repeated-evaluation stability: full mini-suite x30, RSS trend ---
    {
        const char* stab[] = {"1/2+1/3", "factor(x^3-6*x^2+11*x-6)",
                              "solve(x^2-2=0,x)", "integrate(x^2,x)",
                              "diff(sin(x)^2,x)", "simplify((x^2-1)/(x-1))"};
        size_t rssBefore = currentRssKb();
        for (int it = 0; it < 30; ++it) {
            for (const char* e : stab) numos::giacfeas::evaluate(e);
            if (it % 10 == 9)
                printf("STAB|iter=%d|rss_kb=%zu\n", it + 1, currentRssKb());
        }
        size_t rssAfter = currentRssKb();
        printf("STAB_DELTA|before_kb=%zu|after_kb=%zu|delta_kb=%lld\n",
               rssBefore, rssAfter,
               (long long)rssAfter - (long long)rssBefore);
    }

    // --- Grapher: parse-once, 1000-sample numeric sweeps ---
    sweep("sin(x)", -5.0, 5.0, sweepN);
    sweep("1/x", -5.0, 5.0, sweepN);
    sweep("x^2-2", -5.0, 5.0, sweepN);
    // reference: cost of the naive reparse-per-sample approach (200 samples
    // is enough to extrapolate; full 1000 would just be slow)
    sweepReparse("evalf(sin(%.6f))", -5.0, 5.0, 200);

    printf("RSS_END|rss_kb=%zu|peak_kb=%zu\n", currentRssKb(), peakRssKb());
    printf("FOOTER|pass=%d|fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
