#pragma once

#include <stdint.h>
#include <lvgl.h>

namespace ui {

// Checks critical glyph coverage and reports baseline-related metrics in Serial.
bool runStixGlyphAlignmentDiagnostics(const lv_font_t* font);

// Displays a short, on-device glyph gallery for manual visual validation.
void showStixGlyphGallery(uint32_t holdMs);

} // namespace ui
