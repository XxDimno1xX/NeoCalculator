// giac_feasibility_host_stubs.cpp — GIAC-FEAS-01 host-only link stubs.
//
// On KhiCAS calculator firmwares these symbols live in kdisplay.cc, which is
// excluded from every NumOS build (lib/giac/library.json srcFilter). The
// ESP32 link never needs them because -ffunction-sections/--gc-sections on
// ELF strips the unreachable console helper (giac::keytostring) that
// references them. PE/COFF gc-sections cannot strip that function (its
// .pdata/.xdata unwind records pin it), so the host link needs these two
// definitions. They are console/clipboard UI facilities, unrelated to the
// evaluation paths this spike measures.

namespace giac {
int lang = 0;
}

const char* paste_clipboard() { return ""; }
