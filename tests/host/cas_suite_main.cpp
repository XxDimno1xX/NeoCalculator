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
 * cas_suite_main.cpp — CAS host suite driver (CAS-01, PR-1).
 *
 * Runs the dormant CAS test suites on a host build. Each suite executes in
 * an ISOLATED child process with a 60 s timeout: OmniSolverTest and
 * TutorTemplateTest are known to crash (NB-1, unguarded OmniSolver ↔
 * TutorTemplates mutual recursion), and in-process sequencing would lose
 * every suite after the first crash plus the buffered stdout of suites that
 * already passed.
 *
 * Failure detection parses per-case markers ("[FAIL]" / "FAIL: ") from each
 * suite's log — the run*Tests() entry points return void and print
 * heterogeneous summaries. The always-printed SymDiff summary line
 * "... 0 FAIL (total ..." deliberately does NOT match (no colon/bracket).
 *
 * XFAIL policy (tests/host/cas_xfail.list, tab-separated
 * SUITE\tKIND\tmatch-substring\treason, KIND ∈ {case, crash, timeout}):
 * an XFAIL'd failure reports as XFAIL and does not affect the exit code;
 * an XFAIL entry that passes is XPASS and FAILS the run, forcing stale
 * entries to be pruned in the same PR that fixes the underlying bug.
 *
 * Exit code = count(unexpected FAIL) + count(unexpected CRASH/TIMEOUT)
 *           + count(XPASS).
 *
 * Contract: docs/specs/NUMOS_CAS_HOST_HARNESS_SPEC.md §D.2.
 * Build/run: scripts/build-cas-host-tests.sh (never compiled by PlatformIO).
 *
 * Usage:
 *   cas_host_tests --all --xfail <path/to/cas_xfail.list>
 *   cas_host_tests --suite <SuiteName>     # one suite, in-process (debug)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "../ASTFlatExprTest.h"
#include "../BigIntTest.h"
#include "../OmniSolverTest.h"
#include "../SymDiffTest.h"
#include "../SymExprTest.h"
#include "../TutorTemplateTest.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// ── Settings globals ────────────────────────────────────────────────
// Owned by src/main.cpp:31-33 (firmware) and src/hal/NativeHal.cpp:136-138
// (emulator); neither TU is linked here. setting_complex_enabled stays true
// so quadratic complex-root tests exercise the implemented path
// (SingleSolver.cpp reads it at link time).
bool setting_complex_enabled   = true;
int  setting_decimal_precision = 10;
bool setting_edu_steps         = false;

namespace {

struct Suite {
    const char* name;
    void (*run)();
};

// PR-2 appends CASTest / CalculusStressTest here once their API bitrot is
// fixed (they do not compile today — harness spec §B.2).
const Suite kSuites[] = {
    {"BigIntTest",        &cas::runBigIntTests},
    {"SymExprTest",       &cas::runSymExprTests},
    {"SymDiffTest",       &cas::runSymDiffTests},
    {"ASTFlatExprTest",   &cas::runASTFlatExprTests},
    {"OmniSolverTest",    &cas::runOmniSolverTests},
    {"TutorTemplateTest", &cas::runTutorTests},
};
constexpr int kSuiteCount = static_cast<int>(sizeof(kSuites) / sizeof(kSuites[0]));
constexpr int kTimeoutSec = 60;

struct XfailEntry {
    std::string suite;
    std::string kind;   // "case" | "crash" | "timeout"
    std::string match;  // substring
    std::string reason;
    bool used = false;
};

enum class Outcome { Completed, Crash, Timeout };

struct SpawnResult {
    Outcome outcome = Outcome::Completed;
    std::string signalName;  // set for Crash
};

bool isFailureLine(const std::string& line) {
    return line.find("[FAIL]") != std::string::npos ||
           line.find("FAIL: ") != std::string::npos;
}

std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path.c_str());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// Sanitizer runtimes report the underlying fault in the log and _exit()
// non-zero instead of dying by signal; recover the crash class from the
// report so the same xfail entry ("SIGSEGV") covers plain and --asan runs.
std::string crashNameFromLog(const std::vector<std::string>& logLines, long exitCode) {
    for (const std::string& l : logLines) {
        if (l.find("stack-overflow") != std::string::npos ||
            l.find("SEGV") != std::string::npos)
            return "SIGSEGV";
    }
    for (const std::string& l : logLines) {
        if (l.find("AddressSanitizer") != std::string::npos) return "ASAN";
        if (l.find("runtime error:") != std::string::npos) return "UBSAN";
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "EXIT%ld", exitCode);
    return buf;
}

#ifndef _WIN32
std::string signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        default: {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "SIG%d", sig);
            return buf;
        }
    }
}
#endif

// Run one suite in an isolated child (self-exec `--suite <name>`), stdout+
// stderr redirected to logPath, kTimeoutSec timeout.
SpawnResult spawnSuite(const std::string& selfExe, const char* suiteName,
                       const std::string& logPath) {
    SpawnResult r;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE log = CreateFileA(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                             &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si;
    std::memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    std::string cmd = "\"" + selfExe + "\" --suite " + suiteName;
    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, 0, nullptr,
                        nullptr, &si, &pi)) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        r.outcome = Outcome::Crash;
        r.signalName = "SPAWN-FAILED";
        return r;
    }
    DWORD wait = WaitForSingleObject(pi.hProcess, kTimeoutSec * 1000);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        r.outcome = Outcome::Timeout;
    } else {
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        if (code == 0) {
            r.outcome = Outcome::Completed;
        } else {
            r.outcome = Outcome::Crash;
            if (code == 0xC00000FDu /*stack overflow*/ ||
                code == 0xC0000005u /*access violation*/) {
                r.signalName = "SIGSEGV";
            } else if ((code & 0xC0000000u) == 0xC0000000u) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(code));
                r.signalName = buf;
            } else {
                r.signalName = crashNameFromLog(readLines(logPath),
                                                static_cast<long>(code));
            }
        }
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
#else
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(logPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        execl(selfExe.c_str(), selfExe.c_str(), "--suite", suiteName,
              static_cast<char*>(nullptr));
        _exit(127);
    }
    if (pid < 0) {
        r.outcome = Outcome::Crash;
        r.signalName = "SPAWN-FAILED";
        return r;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGALRM) {  // child arms alarm(kTimeoutSec) on itself
            r.outcome = Outcome::Timeout;
        } else {
            r.outcome = Outcome::Crash;
            r.signalName = signalName(sig);
        }
    } else if (WEXITSTATUS(status) != 0) {
        r.outcome = Outcome::Crash;
        r.signalName = crashNameFromLog(readLines(logPath), WEXITSTATUS(status));
    }
#endif
    return r;
}

bool loadXfailList(const std::string& path, std::vector<XfailEntry>& out) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) {
        std::fprintf(stderr, "[CAS-HOST] ERROR: cannot open xfail list: %s\n",
                     path.c_str());
        return false;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t t1 = line.find('\t');
        size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
        size_t t3 = (t2 == std::string::npos) ? std::string::npos : line.find('\t', t2 + 1);
        if (t3 == std::string::npos) {
            std::fprintf(stderr,
                         "[CAS-HOST] ERROR: malformed xfail entry (need 4 "
                         "tab-separated fields) at %s:%d\n",
                         path.c_str(), lineNo);
            return false;
        }
        XfailEntry e;
        e.suite = line.substr(0, t1);
        e.kind = line.substr(t1 + 1, t2 - t1 - 1);
        e.match = line.substr(t2 + 1, t3 - t2 - 1);
        e.reason = line.substr(t3 + 1);
        if (e.kind != "case" && e.kind != "crash" && e.kind != "timeout") {
            std::fprintf(stderr,
                         "[CAS-HOST] ERROR: unknown KIND '%s' at %s:%d "
                         "(must be case|crash|timeout)\n",
                         e.kind.c_str(), path.c_str(), lineNo);
            return false;
        }
        out.push_back(e);
    }
    return true;
}

void echoLogTail(const char* suiteName, const std::vector<std::string>& logLines) {
    size_t start = logLines.size() > 30 ? logLines.size() - 30 : 0;
    std::printf("---- %s log tail (last %d lines) ----\n", suiteName,
                static_cast<int>(logLines.size() - start));
    for (size_t i = start; i < logLines.size(); i++)
        std::printf("  | %s\n", logLines[i].c_str());
    std::printf("---- end %s log tail ----\n", suiteName);
}

int childMain(const char* name) {
    // Unbuffered so a crash cannot swallow already-produced output.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#ifdef _WIN32
    // No fault dialogs — a crashing suite must terminate, not hang CI/local runs.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
#else
    alarm(kTimeoutSec);  // SIGALRM default action kills us → parent reports TIMEOUT
#endif
    for (const Suite& s : kSuites) {
        if (std::strcmp(s.name, name) == 0) {
            s.run();
            return 0;
        }
    }
    std::fprintf(stderr, "[CAS-HOST] ERROR: unknown suite '%s'\n", name);
    return 2;
}

std::string selfExecutable(const char* argv0) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return buf;
#endif
    return argv0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string xfailPath;
    const char* singleSuite = nullptr;
    bool runAll = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--all") == 0) {
            runAll = true;
        } else if (std::strcmp(argv[i], "--suite") == 0 && i + 1 < argc) {
            singleSuite = argv[++i];
        } else if (std::strcmp(argv[i], "--xfail") == 0 && i + 1 < argc) {
            xfailPath = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s --all --xfail <list> | --suite <name>\n",
                         argv[0]);
            return 2;
        }
    }
    if (singleSuite) return childMain(singleSuite);
    if (!runAll) {
        std::fprintf(stderr, "usage: %s --all --xfail <list> | --suite <name>\n",
                     argv[0]);
        return 2;
    }

    std::vector<XfailEntry> xfails;
    if (!xfailPath.empty() && !loadXfailList(xfailPath, xfails)) return 3;

    const std::string self = selfExecutable(argv[0]);
    std::vector<std::string> footer;
    int passCount = 0, xfailCount = 0, unexpected = 0;

    for (const Suite& s : kSuites) {
        std::string logPath = std::string(s.name) + ".log";
        std::printf("[CAS-HOST] running suite=%s ...\n", s.name);
        std::fflush(stdout);
        SpawnResult sr = spawnSuite(self, s.name, logPath);
        std::vector<std::string> logLines = readLines(logPath);

        std::vector<std::string> failedLines;
        for (const std::string& l : logLines)
            if (isFailureLine(l)) failedLines.push_back(l);

        // Match per-case failures against `case` xfail entries.
        int matchedCases = 0, unmatchedCases = 0;
        for (const std::string& fl : failedLines) {
            bool matched = false;
            for (XfailEntry& e : xfails) {
                if (e.kind == "case" && e.suite == s.name &&
                    fl.find(e.match) != std::string::npos) {
                    e.used = true;
                    matched = true;
                }
            }
            if (matched) matchedCases++;
            else unmatchedCases++;
        }

        char line[256];
        if (sr.outcome == Outcome::Completed) {
            if (failedLines.empty()) {
                std::snprintf(line, sizeof(line),
                              "[CAS-HOST] suite=%-17s status=%-6s cases_failed=0",
                              s.name, "pass");
                passCount++;
            } else if (unmatchedCases == 0) {
                std::snprintf(line, sizeof(line),
                              "[CAS-HOST] suite=%-17s status=%-6s cases_failed=%d xfail=%d",
                              s.name, "xfail", static_cast<int>(failedLines.size()),
                              matchedCases);
                xfailCount++;
            } else {
                std::snprintf(line, sizeof(line),
                              "[CAS-HOST] suite=%-17s status=%-6s cases_failed=%d unexpected=%d",
                              s.name, "fail", static_cast<int>(failedLines.size()),
                              unmatchedCases);
                unexpected += unmatchedCases;
                echoLogTail(s.name, logLines);
            }
        } else {
            const bool isCrash = (sr.outcome == Outcome::Crash);
            const char* wantKind = isCrash ? "crash" : "timeout";
            bool covered = false;
            for (XfailEntry& e : xfails) {
                if (e.kind == wantKind && e.suite == s.name &&
                    (!isCrash || sr.signalName.find(e.match) != std::string::npos)) {
                    e.used = true;
                    covered = true;
                }
            }
            unexpected += unmatchedCases;  // unexpected pre-crash case failures still count
            if (isCrash) {
                std::snprintf(line, sizeof(line),
                              "[CAS-HOST] suite=%-17s status=%s signal=%s xfail=%d pre_crash_failed=%d",
                              s.name, covered ? "xfail-crash" : "crash",
                              sr.signalName.c_str(), covered ? 1 : 0,
                              static_cast<int>(failedLines.size()));
            } else {
                std::snprintf(line, sizeof(line),
                              "[CAS-HOST] suite=%-17s status=%s timeout=%ds xfail=%d pre_crash_failed=%d",
                              s.name, covered ? "xfail-timeout" : "timeout",
                              kTimeoutSec, covered ? 1 : 0,
                              static_cast<int>(failedLines.size()));
            }
            if (covered) xfailCount++;
            else unexpected += 1;
            std::printf("[CAS-HOST] %s: %s (%s)\n", s.name,
                        isCrash ? "CRASH" : "TIMEOUT",
                        isCrash ? sr.signalName.c_str() : "60s");
            echoLogTail(s.name, logLines);
        }
        footer.push_back(line);

        if (std::strcmp(s.name, "BigIntTest") == 0) {
            // Host builds compile with CAS_HAS_BIGINT=0 (no mbedtls) — make the
            // reduced coverage visible, never silent (harness spec §C.3).
            std::printf("[CAS-HOST] note suite=BigIntTest SKIPPED (firmware-only): "
                        "BigInt promotion/demotion cases (CAS_HAS_BIGINT=0 on host)\n");
        }
        std::fflush(stdout);
    }

    // XPASS discipline: every unused xfail entry is stale and fails the run.
    for (const XfailEntry& e : xfails) {
        if (!e.used) {
            std::printf("[CAS-HOST] XPASS suite=%s kind=%s match=\"%s\" — entry no "
                        "longer fails; prune it from cas_xfail.list (reason was: %s)\n",
                        e.suite.c_str(), e.kind.c_str(), e.match.c_str(),
                        e.reason.c_str());
            unexpected++;
        }
    }

    int exitCode = unexpected > 125 ? 125 : unexpected;
    char total[128];
    std::snprintf(total, sizeof(total),
                  "[CAS-HOST] total suites=%d pass=%d xfail=%d unexpected=%d exit=%d",
                  kSuiteCount, passCount, xfailCount, unexpected, exitCode);
    footer.push_back(total);

    std::printf("\n");
    std::ofstream summary("summary.txt");
    for (const std::string& l : footer) {
        std::printf("%s\n", l.c_str());
        if (summary.is_open()) summary << l << "\n";
    }
    return exitCode;
}
