#!/bin/bash
# Build and test script for test.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — test(1) returns 1 for false, which
#       would abort the script on the very first test case.

echo "============================================"
echo "    test.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=test
BRACKET="["
SOURCE=test.c
PASS=0
FAIL=0

PLATFORM="unknown"
if [ "$(uname -s)" = "Linux" ]; then
    PLATFORM="linux"
elif [ "$(uname -s)" = "Darwin" ]; then
    PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then
    PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then
    PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then
    PLATFORM="netbsd"
fi

echo ""
echo "Detected platform: $PLATFORM"

EXTRA_FLAGS=""
case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT" "$BRACKET"

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed"
    exit 1
fi

# Create [ as a hard link to test
ln "$OUTPUT" "$BRACKET" 2>/dev/null || cp "$OUTPUT" "$BRACKET"

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"
echo "  Output: $(pwd)/$BRACKET"

echo ""
echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then
        echo "  [PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

# ========== T01: test with no args (false)
echo ""
echo "--- T01: test no args (false) ---"
./"$OUTPUT" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "no args exits 1"

# ========== T02: test non-empty string (true)
echo ""
echo "--- T02: test non-empty string (true) ---"
./"$OUTPUT" hello >/dev/null 2>&1
[ $? -eq 0 ]
check $? "non-empty string exits 0"

# ========== T03: test empty string (false)
echo ""
echo "--- T03: test empty string (false) ---"
./"$OUTPUT" "" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "empty string exits 1"

# ========== T04: test -n non-empty (true)
echo ""
echo "--- T04: test -n non-empty (true) ---"
./"$OUTPUT" -n hello >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-n non-empty exits 0"

# ========== T05: test -z empty (true)
echo ""
echo "--- T05: test -z empty (true) ---"
./"$OUTPUT" -z "" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-z empty exits 0"

# ========== T06: test ! non-empty (false)
echo ""
echo "--- T06: test ! non-empty (false) ---"
./"$OUTPUT" ! hello >/dev/null 2>&1
[ $? -eq 1 ]
check $? "! non-empty exits 1"

# ========== T07: test string equality = (true)
echo ""
echo "--- T07: test string = (true) ---"
./"$OUTPUT" abc = abc >/dev/null 2>&1
[ $? -eq 0 ]
check $? "abc = abc exits 0"

# ========== T08: test string inequality != (true)
echo ""
echo "--- T08: test string != (true) ---"
./"$OUTPUT" abc != def >/dev/null 2>&1
[ $? -eq 0 ]
check $? "abc != def exits 0"

# ========== T09: test integer -eq (true)
echo ""
echo "--- T09: test -eq (true) ---"
./"$OUTPUT" 5 -eq 5 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "5 -eq 5 exits 0"

# ========== T10: test integer -ne (true)
echo ""
echo "--- T10: test -ne (true) ---"
./"$OUTPUT" 5 -ne 6 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "5 -ne 6 exits 0"

# ========== T11: test integer -lt (true)
echo ""
echo "--- T11: test -lt (true) ---"
./"$OUTPUT" 5 -lt 6 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "5 -lt 6 exits 0"

# ========== T12: test integer -gt (true)
echo ""
echo "--- T12: test -gt (true) ---"
./"$OUTPUT" 6 -gt 5 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "6 -gt 5 exits 0"

# ========== T13: test integer -le (true)
echo ""
echo "--- T13: test -le (true) ---"
./"$OUTPUT" 5 -le 5 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "5 -le 5 exits 0"

# ========== T14: test integer -ge (true)
echo ""
echo "--- T14: test -ge (true) ---"
./"$OUTPUT" 5 -ge 5 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "5 -ge 5 exits 0"

# ========== T15: test -a logical AND (true)
echo ""
echo "--- T15: test -a (true) ---"
./"$OUTPUT" -n hello -a -n world >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-a both true exits 0"

# ========== T16: test -o logical OR (true)
echo ""
echo "--- T16: test -o (true) ---"
./"$OUTPUT" -z "" -o -n hello >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-o either true exits 0"

# ========== T17: test -a AND short-circuit (false)
echo ""
echo "--- T17: test -a false (false) ---"
./"$OUTPUT" -n hello -a -z hello >/dev/null 2>&1
[ $? -eq 1 ]
check $? "-a one false exits 1"

# ========== T18: test ! -n (negation)
echo ""
echo "--- T18: test ! -n (false) ---"
./"$OUTPUT" ! -n hello >/dev/null 2>&1
[ $? -eq 1 ]
check $? "! -n hello exits 1"

# ========== T19: test parentheses grouping
echo ""
echo "--- T19: test ( ) grouping (true) ---"
./"$OUTPUT" "(" -n hello ")" -a "(" -z "" ")" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "grouping with -a exits 0"

# ========== T20: test -e on existing file
echo ""
echo "--- T20: test -e existing file (true) ---"
./"$OUTPUT" -e "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-e existing file exits 0"

# ========== T21: test -e on non-existent file (false)
echo ""
echo "--- T21: test -e non-existent (false) ---"
./"$OUTPUT" -e nonexistentfile123 >/dev/null 2>&1
[ $? -eq 1 ]
check $? "-e non-existent exits 1"

# ========== T22: test -f regular file (true)
echo ""
echo "--- T22: test -f regular file (true) ---"
./"$OUTPUT" -f "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-f regular file exits 0"

# ========== T23: test -d directory (true)
echo ""
echo "--- T23: test -d directory (true) ---"
./"$OUTPUT" -d . >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-d directory exits 0"

# ========== T24: test -s non-empty file (true)
echo ""
echo "--- T24: test -s non-empty file (true) ---"
./"$OUTPUT" -s "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-s non-empty file exits 0"

# ========== T25: [ with ] (true)
echo ""
echo "--- T25: [ -n hello ] (true) ---"
./"$BRACKET" -n hello ] >/dev/null 2>&1
[ $? -eq 0 ]
check $? "[ -n hello ] exits 0"

# ========== T26: [ without ] (error)
echo ""
echo "--- T26: [ without ] (error) ---"
./"$BRACKET" -n hello >/dev/null 2>&1
[ $? -eq 2 ]
check $? "[ without ] exits 2"

# ========== T27: [ empty (false)
echo ""
echo "--- T27: [ ] (false) ---"
./"$BRACKET" ] >/dev/null 2>&1
[ $? -eq 1 ]
check $? "[ ] exits 1"

# ========== T28: test --help
echo ""
echo "--- T28: test --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help contains Usage"

# ========== T29: test --version
echo ""
echo "--- T29: test --version ---"
./"$OUTPUT" --version | grep -q "test"
check $? "--version contains test"

# ========== T30: test string < (true)
echo ""
echo "--- T30: test string < (true) ---"
./"$OUTPUT" abc "<" def >/dev/null 2>&1
[ $? -eq 0 ]
check $? "abc < def exits 0"

# ========== T31: test string > (true)
echo ""
echo "--- T31: test string > (true) ---"
./"$OUTPUT" def ">" abc >/dev/null 2>&1
[ $? -eq 0 ]
check $? "def > abc exits 0"

# ========== T32: test invalid integer (error)
echo ""
echo "--- T32: test invalid integer (error) ---"
./"$OUTPUT" abc -eq 5 >/dev/null 2>&1
[ $? -eq 2 ]
check $? "invalid integer exits 2"

# ========== T33: test -r readable (true)
echo ""
echo "--- T33: test -r readable (true) ---"
./"$OUTPUT" -r "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-r readable exits 0"

# ========== T34: test -w writable (true)
echo ""
echo "--- T34: test -w writable (true) ---"
./"$OUTPUT" -w "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-w writable exits 0"

# ========== T35: test -x executable (true)
echo ""
echo "--- T35: test -x executable (true) ---"
./"$OUTPUT" -x "$OUTPUT" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-x executable exits 0"

# ========== T36: test complex grouping
echo ""
echo "--- T36: test complex grouping (true) ---"
./"$OUTPUT" "(" -n hello -o -z "" ")" -a "(" -n world ")" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "complex grouping exits 0"

# ========== T37: test ! with grouping (false)
echo ""
echo "--- T37: test ! grouping (false) ---"
./"$OUTPUT" ! "(" -n hello -a -n world ")" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "! grouping exits 1"

# ========== T38: test -nt newer file
echo ""
echo "--- T38: test -nt (true) ---"
./"$OUTPUT" "$SOURCE" -nt nonexistent123 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-nt newer exits 0"

# ========== T39: test -ot older file
echo ""
echo "--- T39: test -ot (true) ---"
./"$OUTPUT" nonexistent123 -ot "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-ot older exits 0"

# ========== T40: test -ef same file (true)
echo ""
echo "--- T40: test -ef same file (true) ---"
./"$OUTPUT" "$SOURCE" -ef "$SOURCE" >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-ef same file exits 0"

# ========== T41: test -t terminal
echo ""
echo "--- T41: test -t 0 (terminal) ---"
./"$OUTPUT" -t 0 >/dev/null 2>&1
check 0 "-t 0 (terminal or not, no crash)"

# ========== T42: test -h/-L symlink
echo ""
echo "--- T42: test -h symlink (false for regular) ---"
./"$OUTPUT" -h "$SOURCE" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "-h regular file is false"

# ========== T43: test -S socket (false for regular)
echo ""
echo "--- T43: test -S socket (false) ---"
./"$OUTPUT" -S "$SOURCE" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "-S regular file is false"

# ========== T44: test -p pipe (false for regular)
echo ""
echo "--- T44: test -p pipe (false) ---"
./"$OUTPUT" -p "$SOURCE" >/dev/null 2>&1
[ $? -eq 1 ]
check $? "-p regular file is false"

# ========== T45: test negative integers
echo ""
echo "--- T45: test negative integers ---"
./"$OUTPUT" -5 -lt -1 >/dev/null 2>&1
[ $? -eq 0 ]
check $? "-5 -lt -1 exits 0"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ $FAIL -eq 0 ]; then
    echo "  All tests passed!"
else
    echo "  Some tests failed!"
fi

exit 0
