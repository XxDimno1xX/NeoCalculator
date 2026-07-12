// host_rss_probe.cpp — process working-set probe for host harnesses.
//
// Lives in its own TU because <windows.h> cannot coexist with NumOS headers
// (hal/ArduinoCompat.h INPUT/OUTPUT pin macros vs winuser.h's INPUT struct;
// winnt.h's TokenType enumerator vs math/Tokenizer.h's TokenType).

#include <cstddef>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

size_t hostCurrentRssKb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (size_t)(pmc.WorkingSetSize / 1024);
    return 0;
}

#else
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

size_t hostCurrentRssKb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            size_t kb = 0;
            sscanf(line.c_str() + 6, " %zu", &kb);
            return kb;
        }
    }
    return 0;
}

#endif
