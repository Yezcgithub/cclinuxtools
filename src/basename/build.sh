#!/bin/bash
# Build and test script for basename.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — basename returns 1 for error cases,
#       which would abort the script on those test cases.

echo "============================================"
echo "    basename.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=basename
SOURCE=basename.c
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
if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed"
    exit 1
fi

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

# ========== T01: basic
echo ""
echo "--- T01: basic ---"
out=$(./"$OUTPUT" /usr/bin/sort)
[ "$out" = "sort" ]
check $? "basic"

# ========== T02: suffix
echo ""
echo "--- T02: suffix ---"
out=$(./"$OUTPUT" include/stdio.h .h)
[ "$out" = "stdio" ]
check $? "suffix"

# ========== T03: -s suffix
echo ""
echo "--- T03: -s suffix ---"
out=$(./"$OUTPUT" -s .h include/stdio.h)
[ "$out" = "stdio" ]
check $? "-s suffix"

# ========== T04: -a multiple
echo ""
echo "--- T04: -a multiple ---"
out=$(./"$OUTPUT" -a any/str1 any/str2)
[ "$out" = $'str1\nstr2' ]
check $? "-a multiple"

# ========== T05: trailing slash
echo ""
echo "--- T05: trailing slash ---"
out=$(./"$OUTPUT" /usr/bin/)
[ "$out" = "bin" ]
check $? "trailing slash"

# ========== T06: root
echo ""
echo "--- T06: root ---"
out=$(./"$OUTPUT" /)
[ "$out" = "/" ]
check $? "root"

# ========== T07: no args
echo ""
echo "--- T07: no args ---"
./"$OUTPUT" >/dev/null 2>&1
[ $? -ne 0 ]
check $? "no args fails"

# ========== T08: extra args
echo ""
echo "--- T08: extra args ---"
./"$OUTPUT" a b c >/dev/null 2>&1
[ $? -ne 0 ]
check $? "extra args fails"

# ========== T09: --help
echo ""
echo "--- T09: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "help"

# ========== T10: --version
echo ""
echo "--- T10: --version ---"
./"$OUTPUT" --version | grep -q "basename"
check $? "version"

# ========== T11: just slashes
echo ""
echo "--- T11: just slashes ---"
out=$(./"$OUTPUT" ///)
[ "$out" = "/" ]
check $? "just slashes"

# ========== T12: -s with -a
echo ""
echo "--- T12: -s with -a ---"
out=$(./"$OUTPUT" -a -s .txt file1.txt file2.txt)
[ "$out" = $'file1\nfile2' ]
check $? "-s with -a"

# ========== T13: no suffix match
echo ""
echo "--- T13: no suffix match ---"
out=$(./"$OUTPUT" hello.txt .c)
[ "$out" = "hello.txt" ]
check $? "no suffix match"

# ========== T14: single char
echo ""
echo "--- T14: single char ---"
out=$(./"$OUTPUT" a)
[ "$out" = "a" ]
check $? "single char"

# ========== T15: no suffix single name
echo ""
echo "--- T15: no suffix single name ---"
out=$(./"$OUTPUT" hello.txt)
[ "$out" = "hello.txt" ]
check $? "no suffix single name"

# ========== T16: --suffix= form
echo ""
echo "--- T16: --suffix= form ---"
out=$(./"$OUTPUT" --suffix=.h include/stdio.h)
[ "$out" = "stdio" ]
check $? "--suffix= form"

# ========== T17: -z zero separator
echo ""
echo "--- T17: -z zero separator ---"
out=$(./"$OUTPUT" -z any/str1 any/str2 2>/dev/null | xxd -p | head -1)
echo "$out" | grep -q "7300"
check $? "zero separator"

# ========== T18: --multiple form
echo ""
echo "--- T18: --multiple form ---"
out=$(./"$OUTPUT" --multiple any/str1 any/str2)
[ "$out" = $'str1\nstr2' ]
check $? "--multiple form"

# ========== T19: --zero form
echo ""
echo "--- T19: --zero form ---"
out=$(./"$OUTPUT" --zero any/str1 any/str2 2>/dev/null | xxd -p | head -1)
echo "$out" | grep -q "7300"
check $? "--zero form"

# ========== T20: relative path
echo ""
echo "--- T20: relative path ---"
out=$(./"$OUTPUT" ./src/main.c)
[ "$out" = "main.c" ]
check $? "relative path"

# ========== T21: double dot
echo ""
echo "--- T21: double dot ---"
out=$(./"$OUTPUT" ../dir/file.txt)
[ "$out" = "file.txt" ]
check $? "double dot"

# ========== T22: suffix longer than name
echo ""
echo "--- T22: suffix longer than name ---"
out=$(./"$OUTPUT" ab .abcdef)
[ "$out" = "ab" ]
check $? "suffix longer than name"

# ========== T23: suffix equals name
echo ""
echo "--- T23: suffix equals name ---"
out=$(./"$OUTPUT" .txt .txt)
[ "$out" = ".txt" ]
check $? "suffix equals name"

# ========== T24: empty string
echo ""
echo "--- T24: empty string ---"
out=$(./"$OUTPUT" "")
[ -z "$out" ]
check $? "empty string"

# ========== T25: multiple names with --suffix
echo ""
echo "--- T25: multiple names with --suffix ---"
out=$(./"$OUTPUT" --suffix=.txt a.txt b.txt c.txt)
[ "$out" = $'a\nb\nc' ]
check $? "multiple names with --suffix"

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
