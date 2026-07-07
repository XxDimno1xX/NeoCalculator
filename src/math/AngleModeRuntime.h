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
 * AngleModeRuntime.h — runtime single source of truth for DEG/RAD.
 *
 * The runtime angle mode IS vpam::g_angleMode (defined in MathEvaluator.cpp,
 * boot default RAD — the value every LVGL StatusBar badge has always shown).
 * Every badge and evaluator path must go through these helpers instead of
 * keeping a private AngleMode copy that can drift.
 *
 * The two AngleMode enums have INVERTED numeric values:
 *   vpam::AngleMode { DEG=0, RAD=1 }   (MathEvaluator.h)
 *   ::AngleMode     { RAD=0, DEG=1 }   (Evaluator.h, legacy numeric pipeline)
 * so mapping is done BY NAME below — never by cast.
 *
 * Persistence is deliberately out of scope here (GS-02).
 */

#pragma once

#include "MathEvaluator.h"  // vpam::AngleMode + vpam::g_angleMode (the truth)
#include "Evaluator.h"      // legacy ::AngleMode for the numeric RPN pipeline

namespace numos {

inline vpam::AngleMode angleMode()          { return vpam::g_angleMode; }
inline void setAngleMode(vpam::AngleMode m) { vpam::g_angleMode = m; }
inline bool angleModeIsDeg() { return vpam::g_angleMode == vpam::AngleMode::DEG; }

/// Name-based mapping to the legacy enum: vpam DEG ↔ legacy DEG, RAD ↔ RAD.
inline ::AngleMode legacyAngleMode() {
    return angleModeIsDeg() ? ::AngleMode::DEG : ::AngleMode::RAD;
}
inline void setAngleModeFromLegacy(::AngleMode m) {
    setAngleMode(m == ::AngleMode::DEG ? vpam::AngleMode::DEG
                                       : vpam::AngleMode::RAD);
}

/// Lowercase token used by the .numos script hooks ("deg"/"rad").
inline const char* angleModeName() { return angleModeIsDeg() ? "deg" : "rad"; }

} // namespace numos
