#!/usr/bin/env bash
#
# mayhem/build.sh — build FreeType twice and the fuzz harnesses:
#   1) mayhem/build-fuzz   : libfreetype.a with $SANITIZER_FLAGS + SanCov + DWARF 3, linked into every harness
#   2) mayhem/build-tests  : libfreetype.a with the project's normal flags, for the oracle (test.sh)
# Both build dirs are gitignored (mayhem/.gitignore), so `git clean -ffdX` removes them.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
M="$SRC/mayhem"

CMAKE_COMMON=(
  -G Ninja
  -DBUILD_SHARED_LIBS=OFF
  -DFT_DISABLE_HVF=ON
  -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_BZIP2=ON -DFT_REQUIRE_PNG=ON -DFT_REQUIRE_BROTLI=ON
  -DCMAKE_BUILD_TYPE=
)
EXTRA_LIBS=(-lpng16 -lz -lbz2 -lbrotlidec -lbrotlicommon -lm)

# 1) sanitized + coverage-instrumented library
#    pointer-overflow is relaxed: FT_ARRAY_COPY/ftstroke.c do "NULL + 0" on every empty border (benign
#    in practice), which would halt every input otherwise. All other ASan/UBSan checks stay halting.
#    The pre-Adobe Type 1 and CFF charstring engines are compiled in, as in upstream's own fuzz build;
#    fuzz_ftface selects them with the `hinting-engine` property.
FUZZ_CFLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fno-sanitize=pointer-overflow -fsanitize=fuzzer-no-link"
FUZZ_CFLAGS="$FUZZ_CFLAGS -DT1_CONFIG_OPTION_OLD_ENGINE -DCFF_CONFIG_OPTION_OLD_ENGINE"
cmake -S "$SRC" -B "$M/build-fuzz" "${CMAKE_COMMON[@]}" -DFT_DISABLE_HARFBUZZ=ON \
  -DCMAKE_C_FLAGS="$FUZZ_CFLAGS" >/dev/null
cmake --build "$M/build-fuzz" -j"$MAYHEM_JOBS" --target freetype

INC=(-I"$M/build-fuzz/include" -I"$SRC/include")
LIB="$M/build-fuzz/libfreetype.a"

#    The OpenType and TrueType GX table validators are optional modules the CMake library does not
#    build. Compile them with the same flags; fuzz_ftvalidate registers them with FT_Add_Module.
VALID_OBJS=()
for v in otvalid gxvalid; do
  $CC $FUZZ_CFLAGS -DFT2_BUILD_LIBRARY "${INC[@]}" -c "$SRC/src/$v/$v.c" -o "$M/build-fuzz/$v.o"
  VALID_OBJS+=("$M/build-fuzz/$v.o")
done

# 2) harnesses: libFuzzer binary + standalone reproducer, both with leak detection compiled out
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$M/lsan_off.cc" -o "$M/build-fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$M/build-fuzz/standalone_main.o"

for t in ftface ftvar ftstroke ftcache ftvalidate; do
  objs=()
  if [ "$t" = ftvalidate ]; then objs=("${VALID_OBJS[@]}"); fi
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 "${INC[@]}" $LIB_FUZZING_ENGINE \
      "$M/fuzz_$t.c" "${objs[@]}" "$M/build-fuzz/lsan_off.o" "$LIB" "${EXTRA_LIBS[@]}" -o "$SRC/fuzz_$t"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 "${INC[@]}" \
      "$M/fuzz_$t.c" "${objs[@]}" "$M/build-fuzz/standalone_main.o" "$M/build-fuzz/lsan_off.o" "$LIB" \
      "${EXTRA_LIBS[@]}" -o "$SRC/fuzz_$t-standalone"
done

# 3) oracle build: normal flags, no sanitizers, HarfBuzz linked in (tests/issue-1441 needs it)
cmake -S "$SRC" -B "$M/build-tests" "${CMAKE_COMMON[@]}" \
  -DFT_DISABLE_HARFBUZZ=OFF -DFT_DYNAMIC_HARFBUZZ=OFF -DFT_REQUIRE_HARFBUZZ=ON \
  -DCMAKE_C_FLAGS="-O2 $COVERAGE_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" \
  >/dev/null
cmake --build "$M/build-tests" -j"$MAYHEM_JOBS" --target freetype

TINC=(-I"$M/build-tests/include" -I"$SRC/include")
TLIBS=("$M/build-tests/libfreetype.a" -lharfbuzz "${EXTRA_LIBS[@]}")
# every program upstream's tests/meson.build registers, plus our known-answer probe
for t in issue-1063 varc outline-reverse issue-1469 issue-1441; do
  $CC -O2 $COVERAGE_FLAGS "${TINC[@]}" "$SRC/tests/$t/main.c" "${TLIBS[@]}" -o "$M/build-tests/test-$t"
done
$CC -O2 $COVERAGE_FLAGS "${TINC[@]}" "$M/kat/kat.c" "${TLIBS[@]}" -o "$M/build-tests/kat"
python3 "$SRC/tests/varc/make-hvvar-fonts.py" "$SRC/tests/data/varc-static-gvar.ttf" \
    "$M/build-tests/varc-static-hvar.ttf" "$M/build-tests/varc-static-vvar.ttf"
python3 "$SRC/tests/issue-1469/make-fonts.py" "$M"/build-tests/issue-1469-{32764,32765,32766,32767}.ttf
python3 "$SRC/tests/issue-1441/make-fonts.py" \
    "$M/build-tests/issue-1441-latin.ttf" "$M/build-tests/issue-1441-mixed.ttf"
