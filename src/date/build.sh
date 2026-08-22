#!/bin/bash
# Build and test script for date.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    date.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="date"
SOURCE="date.c"
PASS=0
FAIL=0

# Detect platform and set flags
PLATFORM="unknown"
EXTRA_FLAGS=""

if [ -f /etc/os-release ]; then
    . /etc/os-release
    PLATFORM="$ID"
elif [ "$(uname)" = "Darwin" ]; then
    PLATFORM="macos"
elif [ "$(uname)" = "FreeBSD" ]; then
    PLATFORM="freebsd"
elif [ "$(uname)" = "OpenBSD" ]; then
    PLATFORM="openbsd"
elif [ "$(uname)" = "NetBSD" ]; then
    PLATFORM="netbsd"
elif [ "$(uname)" = "Linux" ]; then
    PLATFORM="linux"
fi

case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="gcc" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo ""
echo "[2/3] Detecting platform and compiling..."
echo "  Platform: $PLATFORM"
echo "  Compiler: $CC $CFLAGS $EXTRA_FLAGS"
echo "  Command:  $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE"

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

echo ""
echo "============================================"
echo "  Running basic tests..."
echo "============================================"

check() { if [ "$1" -eq 0 ]; then PASS=$((PASS+1)); echo "  [PASS] $2"; else FAIL=$((FAIL+1)); echo "  [FAIL] $2"; fi; }

# Resolve absolute path to the executable so subshells never confuse it
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DATE_EXE="$SCRIPT_DIR/$OUTPUT"

TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

# A reference file with known mtime for -r tests
REF_FILE="$TDIR/ref.txt"
echo "hello" > "$REF_FILE"

echo ""
echo "--- Test 1: Basic date runs ---"
./"$OUTPUT" > "$TDIR/t1.txt" 2>&1
check $? "basic date runs"

echo ""
echo "--- Test 2: Output is non-empty ---"
if [ -s "$TDIR/t1.txt" ]; then check 0 "output non-empty"; else check 1 "output non-empty"; fi

echo ""
echo "--- Test 3: --help mentions Usage ---"
./"$OUTPUT" --help > "$TDIR/t3.txt" 2>&1
grep -q "Usage" "$TDIR/t3.txt"
check $? "--help mentions Usage"

echo ""
echo "--- Test 4: --version contains 1.0.0 ---"
./"$OUTPUT" --version > "$TDIR/t4.txt" 2>&1
grep -q "1.0.0" "$TDIR/t4.txt"
check $? "--version contains 1.0.0"

echo ""
echo "--- Test 5: +%Y returns a 4-digit year ---"
OUT=$(./"$OUTPUT" +%Y 2>/dev/null)
if echo "$OUT" | grep -Eq '^[0-9]{4}$'; then
    check 0 "+%Y is 4-digit year"
else
    check 1 "+%Y is 4-digit year"
fi

echo ""
echo "--- Test 6: +%s matches epoch seconds ---"
OUT=$(./"$OUTPUT" +%s 2>/dev/null)
NOW=$(date +%s 2>/dev/null)
if [ -n "$NOW" ] && [ "$OUT" = "$NOW" ]; then
    check 0 "+%s matches system date +%s"
else
    # allow 1-second skew
    DIFF=$(( OUT - NOW ))
    if [ "$DIFF" -ge -1 ] && [ "$DIFF" -le 1 ]; then
        check 0 "+%s matches system date +%s (within 1s)"
    else
        check 1 "+%s matches system date +%s"
    fi
fi

echo ""
echo "--- Test 7: -u / --utc / --universal all accepted ---"
./"$OUTPUT" -u > /dev/null 2>&1
R1=$?
./"$OUTPUT" --utc > /dev/null 2>&1
R2=$?
./"$OUTPUT" --universal > /dev/null 2>&1
R3=$?
if [ "$R1" -eq 0 ] && [ "$R2" -eq 0 ] && [ "$R3" -eq 0 ]; then
    check 0 "all three UTC flags accepted"
else
    check 1 "all three UTC flags accepted"
fi

echo ""
echo "--- Test 8: -u +%H:%M output matches system 'date -u +%H:%M' ---"
OURS=$(./"$OUTPUT" -u +%H:%M 2>/dev/null)
SYS=$(date -u +%H:%M 2>/dev/null)
if [ -n "$SYS" ] && [ "$OURS" = "$SYS" ]; then
    check 0 "-u +%H:%M matches system date -u"
else
    check 1 "-u +%H:%M matches system date -u"
fi

echo ""
echo "--- Test 9: -d @0 +%s yields 0 ---"
OUT=$(./"$OUTPUT" -d @0 +%s 2>/dev/null)
if [ "$OUT" = "0" ]; then
    check 0 "-d @0 +%s == 0"
else
    check 1 "-d @0 +%s == 0"
fi

echo ""
echo "--- Test 10: -d '2020-01-01' +%Y-%m-%d yields 2020-01-01 ---"
OUT=$(./"$OUTPUT" -d '2020-01-01' +%Y-%m-%d 2>/dev/null | tr -d '\n')
if [ "$OUT" = "2020-01-01" ]; then
    check 0 "-d '2020-01-01' +%Y-%m-%d"
else
    check 1 "-d '2020-01-01' +%Y-%m-%d (got '$OUT')"
fi

echo ""
echo "--- Test 11: -d '1970-01-01 00:00:00 UTC' +%s yields 0 (UTC) ---"
OUT=$(./"$OUTPUT" -u -d '1970-01-01 00:00:00' +%s 2>/dev/null | tr -d '\n')
if [ "$OUT" = "0" ]; then
    check 0 "-u -d '1970-01-01 00:00:00' +%s == 0"
else
    check 1 "-u -d '1970-01-01 00:00:00' +%s == 0 (got '$OUT')"
fi

echo ""
echo "--- Test 12: -Idefault outputs ISO 8601 date ---"
OUT=$(./"$OUTPUT" -I 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}$'; then
    check 0 "-I outputs YYYY-MM-DD"
else
    check 1 "-I outputs YYYY-MM-DD (got '$OUT')"
fi

echo ""
echo "--- Test 13: -Iseconds outputs ISO 8601 with time ---"
OUT=$(./"$OUTPUT" -Iseconds 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}'; then
    check 0 "-Iseconds outputs date+T+time"
else
    check 1 "-Iseconds outputs date+T+time (got '$OUT')"
fi

echo ""
echo "--- Test 14: -Ins includes nanoseconds ---"
OUT=$(./"$OUTPUT" -Ins 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]+[+-][0-9]{2}:?[0-9]{2}$'; then
    check 0 "-Ins contains nanoseconds + offset"
else
    check 1 "-Ins contains nanoseconds + offset (got '$OUT')"
fi

echo ""
echo "--- Test 15: -R / --rfc-email produces a weekday name ---"
OUT=$(./"$OUTPUT" -R 2>/dev/null)
if echo "$OUT" | grep -Eq '^(Mon|Tue|Wed|Thu|Fri|Sat|Sun),'; then
    check 0 "-R produces a weekday-prefixed RFC 5322 date"
else
    check 1 "-R produces a weekday-prefixed RFC 5322 date (got '$OUT')"
fi

echo ""
echo "--- Test 16: --rfc-email alias ---"
OUT2=$(./"$OUTPUT" --rfc-email 2>/dev/null)
if [ -n "$OUT2" ] && echo "$OUT2" | grep -Eq '^(Mon|Tue|Wed|Thu|Fri|Sat|Sun),'; then
    check 0 "--rfc-email alias works"
else
    check 1 "--rfc-email alias works"
fi

echo ""
echo "--- Test 17: --rfc-3339=seconds ---"
OUT=$(./"$OUTPUT" --rfc-3339=seconds 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}'; then
    check 0 "--rfc-3339=seconds outputs 'YYYY-MM-DD HH:MM:SS'"
else
    check 1 "--rfc-3339=seconds outputs 'YYYY-MM-DD HH:MM:SS' (got '$OUT')"
fi

echo ""
echo "--- Test 18: --rfc-3339=date ---"
OUT=$(./"$OUTPUT" --rfc-3339=date 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}$'; then
    check 0 "--rfc-3339=date outputs YYYY-MM-DD"
else
    check 1 "--rfc-3339=date outputs YYYY-MM-DD (got '$OUT')"
fi

echo ""
echo "--- Test 19: -r/--reference uses file mtime ---"
./"$OUTPUT" -r "$REF_FILE" +%Y > "$TDIR/t19a.txt" 2>&1
R19=$?
./"$OUTPUT" --reference="$REF_FILE" +%Y > "$TDIR/t19b.txt" 2>&1
R19B=$?
if [ "$R19" -eq 0 ] && [ "$R19B" -eq 0 ] && [ -s "$TDIR/t19a.txt" ] && [ -s "$TDIR/t19b.txt" ]; then
    check 0 "-r / --reference file mtime"
else
    check 1 "-r / --reference file mtime"
fi

echo ""
echo "--- Test 20: -d 'yesterday' yields a valid 4-digit year ---"
OUT=$(./"$OUTPUT" -d yesterday +%Y 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}$'; then
    check 0 "-d yesterday parses"
else
    check 1 "-d yesterday parses (got '$OUT')"
fi

echo ""
echo "--- Test 21: -d 'tomorrow' yields a valid 4-digit year ---"
OUT=$(./"$OUTPUT" -d tomorrow +%Y 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}$'; then
    check 0 "-d tomorrow parses"
else
    check 1 "-d tomorrow parses (got '$OUT')"
fi

echo ""
echo "--- Test 22: -d '2 days ago' +%s is ~ 2*86400 before now ---"
PAST=$(./"$OUTPUT" -d '2 days ago' +%s 2>/dev/null)
NOW=$(date +%s 2>/dev/null)
if [ -n "$PAST" ] && [ -n "$NOW" ]; then
    DIFF=$(( NOW - PAST ))
    # expect ~172800 ± 2h slack
    if [ "$DIFF" -ge 170000 ] && [ "$DIFF" -le 176000 ]; then
        check 0 "-d '2 days ago' is ~2*86400 before now"
    else
        check 1 "-d '2 days ago' is ~2*86400 before now (diff=$DIFF)"
    fi
else
    check 1 "-d '2 days ago' parses"
fi

echo ""
echo "--- Test 23: -d '2020-02-29' +%Y-%m-%d (leap day) ---"
OUT=$(./"$OUTPUT" -d '2020-02-29' +%Y-%m-%d 2>/dev/null | tr -d '\n')
if [ "$OUT" = "2020-02-29" ]; then
    check 0 "leap-day 2020-02-29 parses"
else
    check 1 "leap-day 2020-02-29 parses (got '$OUT')"
fi

echo ""
echo "--- Test 24: -d 'next Monday' +%A yields 'Monday' ---"
OUT=$(./"$OUTPUT" -d 'next Monday' +%A 2>/dev/null | tr -d '\n')
if [ "$OUT" = "Monday" ]; then
    check 0 "-d 'next Monday' +%A == Monday"
else
    check 1 "-d 'next Monday' +%A == Monday (got '$OUT')"
fi

echo ""
echo "--- Test 25: +%A matches system date +%A ---"
OURS=$(./"$OUTPUT" +%A 2>/dev/null | tr -d '\n')
SYS=$(date +%A 2>/dev/null | tr -d '\n')
if [ -n "$SYS" ] && [ "$OURS" = "$SYS" ]; then
    check 0 "+%A matches system date +%A"
else
    check 1 "+%A matches system date +%A (ours=$OURS sys=$SYS)"
fi

echo ""
echo "--- Test 26: +%F format ---"
OUT=$(./"$OUTPUT" +%F 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}$'; then
    check 0 "+%F outputs YYYY-MM-DD"
else
    check 1 "+%F outputs YYYY-MM-DD (got '$OUT')"
fi

echo ""
echo "--- Test 27: +%z yields a timezone offset ---"
OUT=$(./"$OUTPUT" +%z 2>/dev/null | tr -d '\n')
if echo "$OUT" | grep -Eq '^[+-][0-9]{4}$'; then
    check 0 "+%z yields +-HHMM"
else
    check 1 "+%z yields +-HHMM (got '$OUT')"
fi

echo ""
echo "--- Test 28: -u +%z is +0000 ---"
OUT=$(./"$OUTPUT" -u +%z 2>/dev/null | tr -d '\n')
if [ "$OUT" = "+0000" ]; then
    check 0 "-u +%z == +0000"
else
    check 1 "-u +%z == +0000 (got '$OUT')"
fi

echo ""
echo "--- Test 29: Unknown long option is an error ---"
"$DATE_EXE" --no-such-option >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown long option returns error"

echo ""
echo "--- Test 30: Unknown short option is an error ---"
"$DATE_EXE" -Z >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown short option returns error"

echo ""
echo "--- Test 31: -d without argument is an error ---"
"$DATE_EXE" -d >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-d without argument errors"

echo ""
echo "--- Test 32: -s without argument is an error ---"
"$DATE_EXE" -s >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-s without argument errors"

echo ""
echo "--- Test 33: Mutually exclusive -d and -s error ---"
"$DATE_EXE" -d now -s now >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-d and -s mutually exclusive"

echo ""
echo "--- Test 34: Multiple output formats error (-I -R) ---"
"$DATE_EXE" -I -R >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "multiple output formats (-I -R) error"

# Cleanup
rm -rf "$TDIR"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo "  All tests passed!"
    exit 0
else
    echo "  Some tests failed!"
    exit 1
fi
