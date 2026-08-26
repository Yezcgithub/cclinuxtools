#!/bin/bash
# Build and test script for arch.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    arch.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=arch
SOURCE=arch.c
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

# ========== T01: default (no option)
echo ""
echo "--- T01: default (no option) ---"
out=$(./"$OUTPUT")
[ -n "$out" ]
check $? "default non-empty"
./"$OUTPUT" >/dev/null 2>&1
check $? "default exits 0"

# ========== T02: output matches uname -m
echo ""
echo "--- T02: output equals uname -m ---"
out=$(./"$OUTPUT")
[ "$out" = "$(uname -m)" ]
check $? "output matches uname -m"

# ========== T03: --help
echo ""
echo "--- T03: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help contains Usage"
./"$OUTPUT" --help >/dev/null 2>&1
check $? "--help exits 0"

# ========== T04: --version
echo ""
echo "--- T04: --version ---"
./"$OUTPUT" --version | grep -q "arch"
check $? "--version contains arch"
./"$OUTPUT" --version >/dev/null 2>&1
check $? "--version exits 0"

# ========== T05: invalid short option exits non-zero
echo ""
echo "--- T05: invalid short option ---"
./"$OUTPUT" -x >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid short option exits non-zero"

# ========== T06: extra operand exits non-zero
echo ""
echo "--- T06: extra operand ---"
./"$OUTPUT" extra >/dev/null 2>&1
[ $? -ne 0 ]
check $? "extra operand exits non-zero"

# ========== T07: unknown long option exits non-zero
echo ""
echo "--- T07: unknown long option ---"
./"$OUTPUT" --bogus >/dev/null 2>&1
[ $? -ne 0 ]
check $? "unknown long option exits non-zero"

# ========== T08: output is single line
echo ""
echo "--- T08: output is single line ---"
lines=$(./"$OUTPUT" | wc -l)
[ "$lines" -eq 1 ]
check $? "output is single line (got $lines)"

# ========== T09: - (dash alone) is error
echo ""
echo "--- T09: dash alone is error ---"
./"$OUTPUT" - >/dev/null 2>&1
[ $? -ne 0 ]
check $? "dash alone exits non-zero"

# ========== T10: -- alone is error
echo ""
echo "--- T10: double dash alone is error ---"
./"$OUTPUT" -- >/dev/null 2>&1
[ $? -ne 0 ]
check $? "double dash alone exits non-zero"

# ========== T11: multiple operands is error
echo ""
echo "--- T11: multiple operands is error ---"
./"$OUTPUT" foo bar >/dev/null 2>&1
[ $? -ne 0 ]
check $? "multiple operands exits non-zero"

# ========== T12: stderr message on error
echo ""
echo "--- T12: stderr message on error ---"
err=$(./"$OUTPUT" --bogus 2>&1 >/dev/null)
echo "$err" | grep -q "unrecognized"
check $? "error message on stderr"

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
