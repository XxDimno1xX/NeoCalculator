#pragma once

#include <lvgl.h>

namespace ui {

// Global math style used by equation-oriented widgets only.
extern lv_style_t style_math_primary;

// Initialize math typography styles once (call after lv_init).
void initMathTypography();

// Canonical font for VPAM/MathRenderer equation drawing.
const lv_font_t* mathPrimaryFont();

} // namespace ui
