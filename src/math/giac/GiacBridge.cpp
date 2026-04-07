#include "math/giac/GiacBridge.h"

// If the build enables Giac but the giac headers are not present, provide
// a safe stub so the project still compiles. Prefer real Giac when headers
// exist and __has_include is supported.
#if defined(NUMOS_USE_GIAC) && defined(__has_include)
  #if __has_include(<giac/config.h>) && __has_include(<giac/gen.h>)
    #include <Arduino.h>
    #include <sstream>
    #include <exception>
    #include <giac/config.h>
    #include <giac/gen.h>

    String solveWithGiac(String input) {
        std::string expr = std::string(input.c_str());
        try {
            static giac::context ct;
            giac::gen g(expr, &ct);
            g = giac::eval(g, 1, &ct);
            g = giac::simplify(g, &ct);
            std::ostringstream oss;
            oss << g;
            std::string out = oss.str();
            return String(out.c_str());
        } catch (const std::exception &e) {
            Serial.print("[GiacBridge] exception: ");
            Serial.println(e.what());
            return String("ERROR: ") + String(e.what());
        } catch (...) {
            Serial.println("[GiacBridge] unknown exception");
            return String("ERROR: unknown");
        }
    }
  #else
    // Giac requested but headers missing: compile a runtime-stub that informs
    // the user to add the Giac sources to lib/giac/src.
    #include <Arduino.h>
    String solveWithGiac(String input) {
        Serial.println("[GiacBridge] Giac headers not found; Giac disabled at runtime.");
        return String("Giac headers not found (add Giac sources to lib/giac/src)");
    }
  #endif
#else
  // Giac not enabled: simple stub.
  String solveWithGiac(String input) {
      return String("Giac not enabled in this build");
  }
#endif
