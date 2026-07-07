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
 * Evaluator.h
 * RPN evaluator for calculator expressions, using double precision.
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <vector>
#include "Tokenizer.h"
#include "VariableContext.h"

enum class AngleMode : uint8_t {
    RAD = 0,
    DEG = 1
};

struct EvalResult {
    bool ok = false;
    String errorMessage;
    double value = 0.0;
};

class Evaluator {
public:
    Evaluator();

    void setAngleMode(AngleMode mode) { _angleMode = mode; }
    AngleMode angleMode() const { return _angleMode; }

    EvalResult evaluateRPN(const std::vector<Token> &rpn, VariableContext &vars);

private:
    static constexpr size_t STACK_MAX = 64;

    AngleMode _angleMode;

    bool push(double stack[], size_t &sp, double v);
    bool pop(double stack[], size_t &sp, double &out);

    bool resolveIdentifier(const String &name, VariableContext &vars, double &out, String &err);
    double toRadians(double x) const;
};

