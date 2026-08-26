#!/bin/bash
# Build and test script for base16.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — base16 -d returns 1 for invalid input,
#       which would abort the script on those test cases.

echo "============================================"
echo "    base16.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=base16
SOURCE=base16.c
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

# Create test files
printf '\376\117\202' > test.bin
printf '%0.sA' {1..1000} > big.bin

# ========== T01: encode
echo ""
echo "--- T01: encode ---"
out=$(./"$OUTPUT" test.bin)
[ "$out" = "FE4F82" ]
check $? "encode"

# ========== T02: decode
echo ""
echo "--- T02: decode ---"
echo "FE4F82" | ./"$OUTPUT" -d | xxd -p
out=$(echo "FE4F82" | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "decode"

# ========== T03: lowercase decode
echo ""
echo "--- T03: lowercase decode ---"
out=$(echo "fe4f82" | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "lowercase decode"

# ========== T04: roundtrip
echo ""
echo "--- T04: roundtrip ---"
out=$(./"$OUTPUT" -w 0 test.bin | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "roundtrip"

# ========== T05: -w 0 (no wrap)
echo ""
echo "--- T05: -w 0 (no wrap) ---"
lines=$(./"$OUTPUT" -w 0 big.bin | wc -l)
[ "$lines" -eq 1 ]
check $? "wrap 0 single line"

# ========== T06: -w 10
echo ""
echo "--- T06: -w 10 ---"
lines=$(./"$OUTPUT" -w 10 big.bin | wc -l)
[ "$lines" -gt 1 ]
check $? "wrap 10 multi line"

# ========== T07: default wrap 76
echo ""
echo "--- T07: default wrap 76 ---"
maxlen=$(./"$OUTPUT" big.bin | awk '{ if (length > m) m = length } END { print m }')
[ "$maxlen" -le 76 ]
check $? "default wrap 76"

# ========== T08: --help
echo ""
echo "--- T08: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "help"

# ========== T09: --version
echo ""
echo "--- T09: --version ---"
./"$OUTPUT" --version | grep -q "base16"
check $? "version"

# ========== T10: odd length fails
echo ""
echo "--- T10: odd length fails ---"
echo "FE4" | ./"$OUTPUT" -d >/dev/null 2>&1
[ $? -ne 0 ]
check $? "odd length fails"

# ========== T11: ignore-garbage
echo ""
echo "--- T11: ignore-garbage ---"
echo "FE 4F-82" | ./"$OUTPUT" -d >/dev/null 2>&1
[ $? -ne 0 ]
check $? "garbage without -i fails"
out=$(echo "FE 4F-82" | ./"$OUTPUT" -d -i 2>/dev/null | xxd -p)
[ "$out" = "fe4f82" ]
check $? "garbage with -i decodes"

# ========== T12: invalid input
echo ""
echo "--- T12: invalid input ---"
echo "XYZ" | ./"$OUTPUT" -d >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid input fails"

# ========== T13: empty file encode
echo ""
echo "--- T13: empty file encode ---"
printf '' > empty.bin
out=$(./"$OUTPUT" empty.bin)
[ -z "$out" ]
check $? "empty file encode"

# ========== T14: multi-file encode
echo ""
echo "--- T14: multi-file encode ---"
printf 'f' > f1.bin
printf 'o' > f2.bin
out=$(./"$OUTPUT" f1.bin f2.bin)
lines=$(echo "$out" | wc -l)
[ "$lines" -eq 2 ]
check $? "multi-file encode 2 lines"

# ========== T15: invalid -w value
echo ""
echo "--- T15: invalid -w -1 ---"
./"$OUTPUT" -w -1 f1.bin >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid -w -1"

# ========== T16: stdin encode
echo ""
echo "--- T16: stdin encode ---"
out=$(./"$OUTPUT" < test.bin)
[ "$out" = "FE4F82" ]
check $? "stdin encode"

# ========== T17: no newline decode
echo ""
echo "--- T17: no newline decode ---"
printf 'FE4F82' | ./"$OUTPUT" -d | xxd -p
out=$(printf 'FE4F82' | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "no newline decode"

# ========== T18: file not found
echo ""
echo "--- T18: file not found ---"
./"$OUTPUT" nonexistent_file >/dev/null 2>&1
[ $? -ne 0 ]
check $? "file not found fails"

# ========== T19: binary roundtrip all 256 byte values
echo ""
echo "--- T19: binary roundtrip ---"
python3 -c "import sys; sys.stdout.buffer.write(bytes(range(256)))" > allbytes.bin 2>/dev/null
if [ $? -eq 0 ]; then
    ./"$OUTPUT" -w 0 allbytes.bin > allbytes_enc.txt
    ./"$OUTPUT" -d allbytes_enc.txt > allbytes_dec.bin 2>/dev/null
    if cmp -s allbytes.bin allbytes_dec.bin; then
        check 0 "binary roundtrip all 256 byte values"
    else
        check 1 "binary roundtrip all 256 byte values"
    fi
    rm -f allbytes.bin allbytes_enc.txt allbytes_dec.bin
else
    head -c 256 /dev/urandom > allbytes.bin
    ./"$OUTPUT" -w 0 allbytes.bin > allbytes_enc.txt
    ./"$OUTPUT" -d allbytes_enc.txt > allbytes_dec.bin 2>/dev/null
    if cmp -s allbytes.bin allbytes_dec.bin; then
        check 0 "binary roundtrip 256 random bytes"
    else
        check 1 "binary roundtrip 256 random bytes"
    fi
    rm -f allbytes.bin allbytes_enc.txt allbytes_dec.bin
fi

# ========== T20: mixed case decode
echo ""
echo "--- T20: mixed case decode ---"
out=$(echo "Fe4F82" | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "mixed case decode"

# ========== T21: stdin decode via pipe
echo ""
echo "--- T21: stdin decode via pipe ---"
out=$(echo "FE4F82" | ./"$OUTPUT" -d | xxd -p)
[ "$out" = "fe4f82" ]
check $? "stdin decode via pipe"

# ========== T22: --wrap=COLS long form
echo ""
echo "--- T22: --wrap=0 ---"
lines=$(./"$OUTPUT" --wrap=0 big.bin | wc -l)
[ "$lines" -eq 1 ]
check $? "--wrap=0"

# Cleanup
rm -f test.bin big.bin empty.bin f1.bin f2.bin

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
