// keycode_digit_test.cpp — Phase 6F host regression guard for keyCodeDigitValue().
//
// The KeyCode enum lays NUM_0..NUM_9 out in PHYSICAL keypad order, not numeric
// order (NUM_7,8,9 -> NUM_4,5,6 -> NUM_1,2,3 -> NUM_0 last near ENTER), so a
// range test `code >= NUM_0 && code <= NUM_9` is always false and `code - NUM_0`
// is meaningless. Phases 6D/6E replaced every such site with the explicit helper
// keyCodeDigitValue() in src/input/KeyCodes.h. This test pins that helper down:
// it proves the ten digit mappings and that representative non-digits return -1,
// using explicit assertions that do NOT depend on enum ordering.
//
// Standalone host test — KeyCodes.h needs only <stdint.h>, so this compiles with
// any C++17 compiler and is NOT part of any PlatformIO build env:
//
//   g++ -std=c++17 -Wall -Wextra tests/host/keycode_digit_test.cpp -o keycode_digit_test
//   ./keycode_digit_test          # exit 0 = pass, 1 = a mapping regressed
//
// Wired into .github/workflows/emulator-build.yml. See also
// scripts/check-keycode-digit-patterns.py (the static anti-pattern guard).

#include <cstdio>

#include "../../src/input/KeyCodes.h"

static int g_failures = 0;
static int g_checks   = 0;

static void expectEq(int got, int want, const char* what) {
    ++g_checks;
    if (got != want) {
        std::printf("FAIL: %-14s got %d, want %d\n", what, got, want);
        ++g_failures;
    }
}

int main() {
    // ── All ten digit keys map to their numeric value (order-independent) ──
    expectEq(keyCodeDigitValue(KeyCode::NUM_0), 0, "NUM_0 -> 0");
    expectEq(keyCodeDigitValue(KeyCode::NUM_1), 1, "NUM_1 -> 1");
    expectEq(keyCodeDigitValue(KeyCode::NUM_2), 2, "NUM_2 -> 2");
    expectEq(keyCodeDigitValue(KeyCode::NUM_3), 3, "NUM_3 -> 3");
    expectEq(keyCodeDigitValue(KeyCode::NUM_4), 4, "NUM_4 -> 4");
    expectEq(keyCodeDigitValue(KeyCode::NUM_5), 5, "NUM_5 -> 5");
    expectEq(keyCodeDigitValue(KeyCode::NUM_6), 6, "NUM_6 -> 6");
    expectEq(keyCodeDigitValue(KeyCode::NUM_7), 7, "NUM_7 -> 7");
    expectEq(keyCodeDigitValue(KeyCode::NUM_8), 8, "NUM_8 -> 8");
    expectEq(keyCodeDigitValue(KeyCode::NUM_9), 9, "NUM_9 -> 9");

    // ── Representative non-digit keys all return -1 ──
    // Spread across the enum so a future reordering can't accidentally pass:
    // NONE (first), AC/SUB/DOT/ENTER (interleaved with the digit rows),
    // ADD/NEG (adjacent to NUM_0), VAR_X/GRAPH (other rows), FACT (last).
    expectEq(keyCodeDigitValue(KeyCode::NONE),  -1, "NONE -> -1");
    expectEq(keyCodeDigitValue(KeyCode::ADD),   -1, "ADD -> -1");
    expectEq(keyCodeDigitValue(KeyCode::SUB),   -1, "SUB -> -1");
    expectEq(keyCodeDigitValue(KeyCode::DOT),   -1, "DOT -> -1");
    expectEq(keyCodeDigitValue(KeyCode::ENTER), -1, "ENTER -> -1");
    expectEq(keyCodeDigitValue(KeyCode::AC),    -1, "AC -> -1");
    expectEq(keyCodeDigitValue(KeyCode::GRAPH), -1, "GRAPH -> -1");
    expectEq(keyCodeDigitValue(KeyCode::VAR_X), -1, "VAR_X -> -1");
    expectEq(keyCodeDigitValue(KeyCode::FACT),  -1, "FACT -> -1");

    if (g_failures == 0) {
        std::printf("PASS: keycode_digit_test (%d checks)\n", g_checks);
        return 0;
    }
    std::printf("FAILED: %d of %d check(s) in keycode_digit_test\n", g_failures, g_checks);
    return 1;
}
