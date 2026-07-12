#!/usr/bin/env bash
# Build + run the GIAC-FEAS-01 host feasibility harness.
# Usage: scripts/build-giac-feasibility-host.sh [--build-only]
#
# Compiles the vendored embedded Giac (lib/giac, KhiCAS profile) NATIVELY,
# with the same macro set the esp32s3_n16r8 firmware uses, plus
# -DHAVE_CONFIG_H — which on firmware is injected by the Arduino-ESP32
# platform and whose absence is why previous native attempts died on
# "SIZEOF_INT not declared" (emulator spec risk R3).
#
# Source set mirrors lib/giac/library.json srcFilter (everything except
# kdisplay*/k_*/fx*/ti*/Xcas*/Fl_*/Graph*/hist*/test.cpp) + libtommath.
# Object files are cached in $OUT/obj across runs.
set -euo pipefail
cd "$(dirname "$0")/.."   # repo root
OUT="${GIAC_FEAS_OUT:-/tmp/giac-feas-host}"
mkdir -p "$OUT/obj"

CXX=${CXX:-g++}
CC=${CC:-gcc}
JOBS=${JOBS:-8}

# Macro parity with [env:esp32s3_n16r8] + lib/giac/library.json:
GIAC_DEFS="-DHAVE_CONFIG_H -DIN_GIAC -DGIAC_KHICAS -DNO_GUI -DGIAC_GENERIC -DEMBEDDED -DUSE_GMP_REPLACEMENTS -DUMAP -DDOUBLEVAL"
INC="-Ilib/giac -Ilib/giac/src -Ilib/libtommath -Isrc"
# -ffunction-sections + --gc-sections mirrors the ESP32 link: unreachable
# KhiCAS console/UI code (which references never-defined symbols such as
# ::paste_clipboard and giac::lang) is stripped instead of failing the link.
CXXFLAGS="-std=gnu++17 -O1 -fexceptions -ffunction-sections -fdata-sections -D_USE_MATH_DEFINES $GIAC_DEFS $INC -w"
CFLAGS="-O1 -ffunction-sections -fdata-sections -D_USE_MATH_DEFINES -Ilib/libtommath -w"
LDFLAGS="-Wl,--gc-sections"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    LDFLAGS="$LDFLAGS -static -lpsapi"
    # Giac guards its POSIX access() calls with __MINGW_H, which is only set
    # once a Windows header has been included; predefine it so the guard works
    # in TUs that never touch Windows headers. -fpermissive: one LP64
    # pointer->unsigned long debug print in kusual.cc (harmless on LLP64).
    CXXFLAGS="$CXXFLAGS -D__MINGW_H -fpermissive"
    ;;
esac

# --- collect sources ---
giac_srcs=()
for f in lib/giac/src/*.cc lib/giac/src/*.cpp; do
  base=$(basename "$f")
  case "$base" in
    kdisplay*|k_*|fx*|ti*|Xcas*|Fl_*|Graph*|hist*|test.cpp) continue ;;
  esac
  giac_srcs+=("$f")
done
tommath_srcs=(lib/libtommath/*.c)

compile_one() {
  src="$1"; flags="$2"; compiler="$3"
  obj="$OUT/obj/$(basename "$src").o"
  if [[ -f "$obj" && "$obj" -nt "$src" ]]; then return 0; fi
  echo "CC $src"
  $compiler $flags -c "$src" -o "$obj"
}
export -f compile_one
export OUT

# --- compile (parallel, cached) ---
printf '%s\n' "${tommath_srcs[@]}" | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$CFLAGS' '$CC'"
printf '%s\n' "${giac_srcs[@]}" | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$CXXFLAGS' '$CXX'"

# --- adapter + harness (with warnings on) ---
$CXX -std=gnu++17 -O1 -fexceptions -Wall -Wextra -Wno-unused-parameter \
  -D_USE_MATH_DEFINES -DNUMOS_GIAC_FEASIBILITY $GIAC_DEFS $INC \
  -c src/math/giac/GiacFeasibility.cpp -o "$OUT/obj/GiacFeasibility.cpp.o"
$CXX -std=gnu++17 -O1 -Wall -Wextra -D_USE_MATH_DEFINES \
  -DNUMOS_GIAC_FEASIBILITY -Isrc \
  -c tests/host/giac_feasibility_main.cpp -o "$OUT/obj/giac_feasibility_main.cpp.o"
# Host-only stubs for two kdisplay.cc symbols PE gc-sections cannot strip
# (see the stub file's header comment). Harmless on ELF hosts too.
$CXX -std=gnu++17 -O1 -c tests/host/giac_feasibility_host_stubs.cpp \
  -o "$OUT/obj/giac_feasibility_host_stubs.cpp.o"

# --- link ---
echo "LINK $OUT/giac_feasibility"
$CXX -o "$OUT/giac_feasibility" "$OUT"/obj/*.o $LDFLAGS

[[ "${1:-}" == "--build-only" ]] && exit 0
( cd "$OUT" && exec ./giac_feasibility )
