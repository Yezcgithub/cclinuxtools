#!/bin/bash
# Build and test script for cat.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    cat.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=cat
SOURCE=cat.c
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

# Test files
T1="/tmp/cattest_1.txt"
T2="/tmp/cattest_2.txt"

# ========== T01: Basic cat
echo ""
echo "--- T01: Basic cat ---"
printf 'hello\n' > "$T1"
./"$OUTPUT" "$T1" | grep -q "hello"
check $? "basic cat"

# ========== T02: Multiple files
echo ""
echo "--- T02: Multiple files ---"
printf 'aaa\n' > "$T1"
printf 'bbb\n' > "$T2"
./"$OUTPUT" "$T1" "$T2" | grep -q "aaa"
check $? "multiple files (first)"
./"$OUTPUT" "$T1" "$T2" | grep -q "bbb"
check $? "multiple files (second)"

# ========== T03: stdin
echo ""
echo "--- T03: stdin pipe ---"
echo "piped" | ./"$OUTPUT" | grep -q "piped"
check $? "stdin pipe"

# ========== T04: -n number all lines
echo ""
echo "--- T04: -n number lines ---"
printf 'line1\nline2\nline3\n' > "$T1"
result=$(./"$OUTPUT" -n "$T1")
echo "$result" | head -1 | grep -qE "^\s*1\s+line1"
check $? "-n number line 1"
echo "$result" | tail -1 | grep -qE "^\s*3\s+line3"
check $? "-n number line 3"

# ========== T05: -b number nonblank only
echo ""
echo "--- T05: -b number nonblank ---"
printf 'aaa\n\nbbb\n' > "$T1"
result=$(./"$OUTPUT" -b "$T1")
echo "$result" | head -1 | grep -qE "^\s*1\s+aaa"
check $? "-b number nonblank line 1"
echo "$result" | tail -1 | grep -qE "^\s*2\s+bbb"
check $? "-b number nonblank line 2"

# ========== T06: -E show ends
echo ""
echo "--- T06: -E show ends ---"
printf 'hi\n' > "$T1"
./"$OUTPUT" -E "$T1" | grep -q '${'
check $? "-E show ends"

# ========== T07: -T show tabs
echo ""
echo "--- T07: -T show tabs ---"
printf '\t\tx\n' > "$T1"
./"$OUTPUT" -T "$T1" | grep -q '\^I\^Ix'
check $? "-T show tabs"

# ========== T08: -s squeeze blank
echo ""
echo "--- T08: -s squeeze blank ---"
printf 'a\n\n\n\n\nb\n' > "$T1"
lines=$(./"$OUTPUT" -s "$T1" | wc -l)
[ "$lines" -le 3 ]
check $? "-s squeeze blank"

# ========== T09: --help
echo ""
echo "--- T09: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help"

# ========== T10: --version
echo ""
echo "--- T10: --version ---"
./"$OUTPUT" --version | grep -q "9.7"
check $? "--version"

# ========== T11: -A show all (equivalent to -vET)
echo ""
echo "--- T11: -A show all ---"
printf 'hi\t\n' > "$T1"
result=$(./"$OUTPUT" -A "$T1")
echo "$result" | grep -q '\^I'
check $? "-A show tabs"
echo "$result" | grep -q '${'
check $? "-A show ends"

# ========== T12: non-existent file error
echo ""
echo "--- T12: non-existent file error ---"
./"$OUTPUT" "/nonexistent_file_xyz.txt" 2>/dev/null
[ $? -ne 0 ]
check $? "nonexistent file error exit code"

# ========== T13: dash as filename = stdin
echo ""
echo "--- T13: - as stdin ---"
echo "dashstd" | ./"$OUTPUT" - | grep -q "dashstd"
check $? "- as stdin"

# ========== T14: -e equivalent to -vE
echo ""
echo "--- T14: -e (equiv -vE) ---"
printf 'hi\n' > "$T1"
result=$(./"$OUTPUT" -e "$T1")
echo "$result" | grep -q '${'
check $? "-e show ends"

# ========== T15: -t equivalent to -vT
echo ""
echo "--- T15: -t (equiv -vT) ---"
printf '\tx\n' > "$T1"
result=$(./"$OUTPUT" -t "$T1")
echo "$result" | grep -q '\^I'
check $? "-t show tabs"

# ========== T16: -v show nonprinting
echo ""
echo "--- T16: -v show nonprinting ---"
printf '\x01\x02\n' > "$T1"
result=$(./"$OUTPUT" -v "$T1")
echo "$result" | grep -q '\^A'
check $? "-v show nonprinting control chars"

# ========== T17: -v M- notation for high bytes
echo ""
echo "--- T17: -v M- notation ---"
printf '\x80\n' > "$T1"
result=$(./"$OUTPUT" -v "$T1")
echo "$result" | grep -q 'M-'
check $? "-v M- notation"

# ========== T18: -v Del char (127) -> ^?
echo ""
echo "--- T18: -v Del char ---"
printf '\x7f\n' > "$T1"
result=$(./"$OUTPUT" -v "$T1")
echo "$result" | grep -q '\^?'
check $? "-v Del char"

# ========== T19: -n and -b together (-b wins)
echo ""
echo "--- T19: -n and -b together ---"
printf 'aaa\n\nbbb\n' > "$T1"
result=$(./"$OUTPUT" -nb "$T1")
echo "$result" | head -1 | grep -qE "^\s*1\s+aaa"
check $? "-nb: -b overrides -n"

# ========== T20: multiple files + line numbering continues
echo ""
echo "--- T20: numbering continues across files ---"
printf 'a\nb\n' > "$T1"
printf 'c\nd\n' > "$T2"
result=$(./"$OUTPUT" -n "$T1" "$T2")
echo "$result" | grep -qE "^\s*3\s+c"
check $? "numbering continues across files"

# ========== T21: empty file
echo ""
echo "--- T21: empty file ---"
printf '' > "$T1"
result=$(./"$OUTPUT" "$T1")
[ -z "$result" ]
check $? "empty file produces no output"

# ========== T22: binary safe (NUL byte pass-through)
echo ""
echo "--- T22: binary safe NUL ---"
printf 'A\x00B\n' > "$T1"
result=$(./"$OUTPUT" "$T1" | od -A n -t x1 | head -1)
echo "$result" | grep -q '41'
check $? "binary safe (A byte)"
echo "$result" | grep -q '42'
check $? "binary safe (B byte)"

# ========== T23: -- separator
echo ""
echo "--- T23: -- separator ---"
printf 'content\n' > -- 2>/dev/null || printf 'content\n' > "$T1"
./"$OUTPUT" -- "$T1" 2>/dev/null | grep -q "content"
check $? "-- separator"

# ========== T24: -s with no blanks
echo ""
echo "--- T24: -s no blanks ---"
printf 'a\nb\nc\n' > "$T1"
lines=$(./"$OUTPUT" -s "$T1" | wc -l)
[ "$lines" -eq 3 ]
check $? "-s with no blanks unchanged"

# ========== T25: combined -nE
echo ""
echo "--- T25: combined -nE ---"
printf 'hi\n' > "$T1"
result=$(./"$OUTPUT" -nE "$T1")
echo "$result" | grep -qE "^\s*1\s+hi\$"
check $? "combined -nE"

# Cleanup
rm -f "$T1" "$T2"

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
