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

// GiacFeasibilityNoGiacStub.cpp — GIAC-FEAS-01 measurement aid ONLY.
//
// Link-time stand-in for solveWithGiac() used by the
// esp32s3_n16r8_giacfeas_baseline environment, which strips lib/giac +
// lib/libtommath out of the firmware so the flash/static-RAM cost of Giac
// can be measured as (full build) - (this build). Never compiled in any
// shipping environment.

#if defined(ARDUINO) && defined(NUMOS_GIAC_FEAS_NOGIAC)

#include <Arduino.h>
#include "math/giac/GiacBridge.h"

String solveWithGiac(String input) {
    (void)input;
    return String("Error: Giac excluded from this measurement build");
}

#endif
