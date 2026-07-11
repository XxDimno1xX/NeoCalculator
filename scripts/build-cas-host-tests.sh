#!/usr/bin/env bash
# Build + run the CAS host suites (CAS-01). Usage: scripts/build-cas-host-tests.sh [--asan] [--build-only]
#
# Standalone g++ harness — deliberately NOT a PlatformIO env (LDF/lib_ignore
# dance, Windows build_dir workaround; see NUMOS_CAS_HOST_HARNESS_SPEC.md §C.4).
# The TU list below is the empirically verified link closure (spec §C.1) and is
# the single source of truth for what the host lane compiles. Giac/libtommath
# must NEVER be added here (native Giac is known-broken; spec §C.3).
# PR-2 appends tests/CASTest.cpp and tests/CalculusStressTest.cpp.
set -euo pipefail
cd "$(dirname "$0")/.."                      # repo root
OUT="${CAS_HOST_OUT:-/tmp/cas-host-tests}"   # all artifacts out-of-tree
mkdir -p "$OUT"

CXX=${CXX:-g++}
# -D_USE_MATH_DEFINES: MinGW math.h hides M_PI/M_E under -std=c++17 strict
# ANSI (MathEvaluator.cpp uses both); no-op on glibc.
FLAGS="-std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -D_USE_MATH_DEFINES -I src"
# Windows/MSYS local runs: link the runtime statically — otherwise the binary
# resolves Git Bash's mismatched libstdc++-6.dll at runtime and segfaults in
# iostreams. Linux CI is unaffected.
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) FLAGS="$FLAGS -static" ;; esac
[[ "${1:-}" == "--asan" ]] && FLAGS="$FLAGS -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all" && shift

$CXX $FLAGS -o "$OUT/cas_host_tests" \
  tests/host/cas_suite_main.cpp \
  tests/SymExprTest.cpp tests/SymDiffTest.cpp tests/ASTFlatExprTest.cpp \
  tests/OmniSolverTest.cpp tests/BigIntTest.cpp tests/TutorTemplateTest.cpp \
  src/math/cas/*.cpp \
  src/math/MathAST.cpp src/math/MathEvaluator.cpp src/math/VariableManager.cpp \
  src/math/font/MathGlyphAssembly.cpp src/hal/FileSystem.cpp

[[ "${1:-}" == "--build-only" ]] && exit 0
# Run from $OUT so any emulator_data/ a test creates stays out of the repo.
( cd "$OUT" && exec ./cas_host_tests --all --xfail "$OLDPWD/tests/host/cas_xfail.list" )
