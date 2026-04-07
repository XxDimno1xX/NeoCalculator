// GiacBridge.h
#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
#endif

// Public API: evaluate a textual expression using Giac and return result as Arduino String
// If Giac is not available, returns an explanatory message.
String solveWithGiac(String input);
