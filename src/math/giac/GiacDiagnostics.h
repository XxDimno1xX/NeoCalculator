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
 * GiacDiagnostics.h — GIAC-A01 opt-in on-device engine diagnostics.
 *
 * Compiled to a no-op unless BOTH -DNUMOS_GIAC_DIAGNOSTICS and ARDUINO are
 * set (use `pio run -e esp32s3_n16r8_giacdiag`). Prints grep-stable
 * [GIACDIAG] serial lines at boot: init/first/warm eval timings, a
 * 1000-sample retained-expression sweep, internal-heap/PSRAM free +
 * largest-block before/after, stack high-water, and a compact
 * representative-suite pass/fail summary. Never part of normal firmware.
 */

#pragma once

namespace numos {

#if defined(NUMOS_GIAC_DIAGNOSTICS) && defined(ARDUINO)
void runGiacDiagnostics();
#else
inline void runGiacDiagnostics() {}
#endif

} // namespace numos
