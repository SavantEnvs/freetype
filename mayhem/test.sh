#!/usr/bin/env bash
#
# mayhem/test.sh - run the oracle built by mayhem/build.sh (mayhem/build-tests, no sanitizers):
#   * upstream's own tests/varc and tests/outline-reverse programs (assertion-based, exit code), and
#   * known-answer probes through mayhem/kat/kat.c: face metrics, glyph advances, outline control box
#     and point/contour counts, bitmap checksums, for TrueType, WOFF, CFF2, Type 1, BDF and PCF
#     (plain, gzip, bzip2) fonts. The exact output text is compared, so a neutered library fails.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

T="$SRC/mayhem/build-tests"
D="$SRC/mayhem/kat/data"
V="$SRC/tests/data"
passed=0
failed=0

for b in kat test-varc test-outline-reverse varc-static-hvar.ttf varc-static-vvar.ttf; do
  if [ ! -e "$T/$b" ]; then
    echo "missing $T/$b (mayhem/build.sh should have built it)" >&2
    emit_ctrf "freetype-kat" 0 1 0
    exit 1
  fi
done

pass() { passed=$((passed + 1)); echo "PASS $1"; }
fail() { failed=$((failed + 1)); echo "FAIL $1"; }

# expect <name> <expected text> <kat args...>
expect() {
  local name="$1" want="$2" got
  shift 2
  got=$("$T/kat" "$@" 2>&1)
  if [ "$got" = "$want" ]; then
    pass "$name"
  else
    fail "$name"
    printf 'expected:\n%s\ngot:\n%s\n' "$want" "$got" >&2
  fi
}

export FREETYPE_TESTS_DATA_DIR="$V"
cd "$T"

if ./test-varc ./varc-static-hvar.ttf ./varc-static-vvar.ttf; then
  pass "tests/varc"
else
  fail "tests/varc"
fi

if ./test-outline-reverse; then
  pass "tests/outline-reverse"
else
  fail "tests/outline-reverse"
fi

TTF_HEAD='format=TrueType glyphs=3 upem=1000 faces=1 charmaps=2'
TTF_CHARS='char=65 gid=0 adv=2048 points=0 contours=0 cbox=0,0,0,0
char=66 gid=0 adv=2048 points=0 contours=0 cbox=0,0,0,0
char=1 gid=0 adv=2048 points=0 contours=0 cbox=0,0,0,0'
TTF_GIDS='char=0 gid=1 adv=2048 points=3 contours=1 cbox=205,0,1843,2048
char=0 gid=2 adv=2048 points=3 contours=1 cbox=0,0,1638,2048'

expect "truetype glyph lookup" "$TTF_HEAD
$TTF_CHARS" "$V/varc-static-gvar.ttf" 65 66 1
expect "truetype outlines" "$TTF_HEAD
$TTF_GIDS" "$V/varc-static-gvar.ttf" g1 g2
expect "woff outlines" "$TTF_HEAD
$TTF_GIDS" "$D/varc-static-gvar.woff" g1 g2

expect "cff2 glyph lookup" "format=CFF glyphs=9 upem=1000 faces=1 charmaps=2
char=65 gid=0 adv=2624 points=16 contours=4 cbox=327,0,2293,2752
char=66 gid=0 adv=2624 points=16 contours=4 cbox=327,0,2293,2752
char=1 gid=0 adv=2624 points=16 contours=4 cbox=327,0,2293,2752" "$V/varc-static-cff2.otf" 65 66 1
expect "cff2 outlines" "format=CFF glyphs=9 upem=1000 faces=1 charmaps=2
char=0 gid=1 adv=960 points=0 contours=0 cbox=0,0,0,0
char=0 gid=2 adv=3200 points=60 contours=7 cbox=147,0,3015,2707" "$V/varc-static-cff2.otf" g1 g2

PCF_WANT="format=PCF glyphs=3 upem=0 faces=1 charmaps=1
char=65 gid=0 adv=512 bitmap=8x8 sum=15091733063850166396
char=66 gid=0 adv=512 bitmap=8x8 sum=15091733063850166396
char=1 gid=0 adv=512 bitmap=8x8 sum=15091733063850166396"
expect "pcf" "$PCF_WANT" "$D/kat.pcf" 65 66 1
expect "pcf gzip" "$PCF_WANT" "$D/kat.pcf.gz" 65 66 1
expect "pcf bzip2" "$PCF_WANT" "$D/kat.pcf.bz2" 65 66 1
expect "pcf glyph bitmaps" "format=PCF glyphs=3 upem=0 faces=1 charmaps=1
char=0 gid=1 adv=512 bitmap=8x8 sum=15091733063850166396
char=0 gid=2 adv=512 bitmap=8x8 sum=8805807938183317378" "$D/kat.pcf" g1 g2

expect "bdf" "format=BDF glyphs=3 upem=0 faces=1 charmaps=1
char=65 gid=1 adv=512 bitmap=8x8 sum=694207166736
char=66 gid=2 adv=512 bitmap=8x8 sum=3472145465082
char=1 gid=0 adv=512 bitmap=8x8 sum=694207166736" "$D/kat.bdf" 65 66 1

expect "type1" "format=Type 1 glyphs=2 upem=1000 faces=1 charmaps=2
char=65 gid=1 adv=2432 points=4 contours=1 cbox=614,0,2252,2047
char=66 gid=0 adv=2048 points=0 contours=0 cbox=0,0,0,0
char=1 gid=0 adv=2048 points=0 contours=0 cbox=0,0,0,0" "$D/kat.pfa" 65 66 1

cd "$SRC"
emit_ctrf "freetype-kat" "$passed" "$failed" 0
