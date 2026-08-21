#!/bin/bash
# Build and test script for echo.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    echo.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=echo
SOURCE=echo.c
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
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="gcc" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

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

# ========== T01: Basic echo
echo ""
echo "--- T01: Basic echo ---"
./"$OUTPUT" hello | grep -q "hello"
check $? "basic echo"

# ========== T02: Multiple words
echo ""
echo "--- T02: Multiple words ---"
./"$OUTPUT" hello world | grep -q "hello world"
check $? "multiple words"

# ========== T03: Empty string
echo ""
echo "--- T03: Empty string ---"
./"$OUTPUT" > /dev/null 2>&1
check $? "empty string"

# ========== T04: -n no newline
echo ""
echo "--- T04: -n no newline ---"
result=$(./"$OUTPUT" -n hello; ./"$OUTPUT" world)
echo "$result" | grep -q "helloworld"
check $? "-n no newline"

# ========== T05: -e newline
echo ""
echo "--- T05: -e newline ---"
./"$OUTPUT" -e "a\nb" | grep -q "a"
check $? "-e newline"

# ========== T06: -e tab
echo ""
echo "--- T06: -e tab ---"
./"$OUTPUT" -e "a\tb" | grep -q "a"
check $? "-e tab"

# ========== T07: -e backslash
echo ""
echo "--- T07: -e backslash ---"
result=$(./"$OUTPUT" -e "\\")
[ "$result" = "\\" ] && check 0 "-e backslash" || check 1 "-e backslash"

# ========== T08: -e \c stops output
echo ""
echo "--- T08: -e \c stops ---"
result=$(./"$OUTPUT" -e "hello\c world")
echo "$result" | grep -q "world" && check 1 "-e \c stops" || check 0 "-e \c stops"

# ========== T09: -e octal
echo ""
echo "--- T09: -e octal ---"
./"$OUTPUT" -e "\101\102\103" | grep -q "ABC"
check $? "-e octal"

# ========== T10: -e hex
echo ""
echo "--- T10: -e hex ---"
./"$OUTPUT" -e "\x41\x42\x43" | grep -q "ABC"
check $? "-e hex"

# ========== T11: -E disable escape
echo ""
echo "--- T11: -E disable escape ---"
./"$OUTPUT" -E "hello\nworld" | grep -q "\\n"
check $? "-E disable escape"

# ========== T12: Default -E
echo ""
echo "--- T12: Default -E ---"
./"$OUTPUT" "hello\nworld" | grep -q "\\n"
check $? "default -E"

# ========== T13: -e -n combined
echo ""
echo "--- T13: -e -n combined ---"
result=$(./"$OUTPUT" -e -n "a\nb"; ./"$OUTPUT" c)
echo "$result" | grep -q "ac"
check $? "-e -n combined"

# ========== T14: -ne combined
echo ""
echo "--- T14: -ne combined ---"
result=$(./"$OUTPUT" -ne "a\nb"; ./"$OUTPUT" c)
echo "$result" | grep -q "ac"
check $? "-ne combined"

# ========== T15: -en combined
echo ""
echo "--- T15: -en combined ---"
result=$(./"$OUTPUT" -en "a\nb"; ./"$OUTPUT" c)
echo "$result" | grep -q "ac"
check $? "-en combined"

# ========== T16: --help
echo ""
echo "--- T16: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help"

# ========== T17: --version
echo ""
echo "--- T17: --version ---"
./"$OUTPUT" --version | grep -q "9.7"
check $? "--version"

# ========== T18: -e \b backspace
echo ""
echo "--- T18: -e \b backspace ---"
./"$OUTPUT" -e "abcd\b" | grep -q "abc"
check $? "-e \b backspace"

# ========== T19: -e \r carriage return
echo ""
echo "--- T19: -e \r carriage return ---"
./"$OUTPUT" -e "hello\rworld" | grep -q "world"
check $? "-e \r carriage return"

# ========== T20: -e \a bell
echo ""
echo "--- T20: -e \a bell ---"
./"$OUTPUT" -e "hello\a" > /dev/null 2>&1
check $? "-e \a bell"

# ========== T21: -e \e escape
echo ""
echo "--- T21: -e \e escape ---"
./"$OUTPUT" -e "\e[31mred\e[0m" | grep -q "red"
check $? "-e \e escape"

# ========== T22: -- separator
echo ""
echo "--- T22: -- treated as string ---"
./"$OUTPUT" -- -n | grep -q "-n"
check $? "-- treated as string"

# ========== T23: Invalid option
echo ""
echo "--- T23: Invalid option treated as string ---"
./"$OUTPUT" -X > /dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid option"

# ========== T24: -e -E order (last wins)
echo ""
echo "--- T24: -e -E last wins ---"
./"$OUTPUT" -e -E "a\nb" | grep -q "\\n"
check $? "-e -E last wins"

# ========== T25: -E -e order (last wins)
echo ""
echo "--- T25: -E -e last wins ---"
lines=$(./"$OUTPUT" -E -e "a\nb" | wc -l)
[ "$lines" -ge 2 ] && check 0 "-E -e last wins" || check 1 "-E -e last wins"

# ========== T26: -e \f form feed
echo ""
echo "--- T26: -e \f form feed ---"
./"$OUTPUT" -e "a\fb" | grep -q "a"
check $? "-e \f form feed"

# ========== T27: -e \v vertical tab
echo ""
echo "--- T27: -e \v vertical tab ---"
./"$OUTPUT" -e "a\vb" | grep -q "a"
check $? "-e \v vertical tab"

# ========== T28: -e octal with 1 digit
echo ""
echo "--- T28: -e octal 1 digit ---"
./"$OUTPUT" -e "\65" | grep -q "5"
check $? "-e octal 1 digit"

# ========== T29: -e hex with 1 digit
echo ""
echo "--- T29: -e hex 1 digit ---"
./"$OUTPUT" -e "\x41" | grep -q "A"
check $? "-e hex 1 digit"

# ========== T30: -e \c stops with -n
echo ""
echo "--- T30: -e \c stops with -n ---"
result=$(./"$OUTPUT" -ne "hello\c world"; ./"$OUTPUT" c)
echo "$result" | grep -q "world" && check 1 "-e \c stops with -n" || check 0 "-e \c stops with -n"

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
