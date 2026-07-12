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

// GiacHostStubs.cpp — native-host link stubs for two KhiCAS console symbols.
//
// On KhiCAS calculator firmwares `giac::lang` and `::paste_clipboard` live in
// kdisplay.cc, which every NumOS build excludes (lib/giac/library.json
// srcFilter). The ESP32 link never needs them: -ffunction-sections +
// --gc-sections on ELF/xtensa strips their only referencer
// (giac::keytostring, console UI). Native links DO need them — PE/COFF
// gc-sections cannot strip that function (its unwind records pin it), and
// the emulator_pc link does not use gc-sections at all. They are console
// clipboard/localization facilities, unreachable from the evaluation paths.
//
// Host-only by guard; never compiled into firmware.

#ifndef ARDUINO

namespace giac {
int lang = 0;
}

const char* paste_clipboard() { return ""; }

#endif // !ARDUINO
