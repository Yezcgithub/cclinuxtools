#!/bin/bash
# Build and test script for basenc.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — basenc returns 1 for invalid input,
#       which would abort the script on those test cases.

echo "============================================"
echo "    basenc.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=basenc
SOURCE=basenc.c
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
printf '\376\117\202\000' > z85.bin
printf '%0.sA' {1..1000} > big.bin

# ========== T01: base64
echo ""
echo "--- T01: base64 ---"
out=$(./"$OUTPUT" --base64 test.bin)
[ "$out" = "/k+C" ]
check $? "base64"

# ========== T02: base64url
echo ""
echo "--- T02: base64url ---"
out=$(./"$OUTPUT" --base64url test.bin)
[ "$out" = "_k-C" ]
check $? "base64url"

# ========== T03: base32
echo ""
echo "--- T03: base32 ---"
out=$(./"$OUTPUT" --base32 test.bin)
[ "$out" = "7ZHYE===" ]
check $? "base32"

# ========== T04: base32hex
echo ""
echo "--- T04: base32hex ---"
out=$(./"$OUTPUT" --base32hex test.bin)
[ "$out" = "VP7O4===" ]
check $? "base32hex"

# ========== T05: base16
echo ""
echo "--- T05: base16 ---"
out=$(./"$OUTPUT" --base16 test.bin)
[ "$out" = "FE4F82" ]
check $? "base16"

# ========== T06: base2lsbf
echo ""
echo "--- T06: base2lsbf ---"
out=$(./"$OUTPUT" --base2lsbf test.bin)
[ "$out" = "011111111111001001000001" ]
check $? "base2lsbf"

# ========== T07: base2msbf
echo ""
echo "--- T07: base2msbf ---"
out=$(./"$OUTPUT" --base2msbf test.bin)
[ "$out" = "111111100100111110000010" ]
check $? "base2msbf"

# ========== T08: z85
echo ""
echo "--- T08: z85 ---"
out=$(./"$OUTPUT" --z85 z85.bin)
[ "$out" = "@.FaC" ]
check $? "z85"

# ========== T09-T16: roundtrip tests
RT_TESTS=(
    "base64:--base64:test.bin:3"
    "base32:--base32:test.bin:3"
    "base16:--base16:test.bin:3"
    "base2msbf:--base2msbf:test.bin:3"
    "base2lsbf:--base2lsbf:test.bin:3"
    "z85:--z85:z85.bin:4"
    "base64url:--base64url:test.bin:3"
    "base32hex:--base32hex:test.bin:3"
)

tnum=9
for rt in "${RT_TESTS[@]}"; do
    IFS=':' read -r name enc_opt in_file exp_len <<< "$rt"
    echo ""
    echo "--- T$(printf '%02d' $tnum): roundtrip $name ---"
    ./"$OUTPUT" -w 0 $enc_opt "$in_file" > rt.txt
    ./"$OUTPUT" -d $enc_opt rt.txt > rt_dec.bin 2>/dev/null
    actual_len=$(wc -c < rt_dec.bin)
    if [ "$actual_len" -eq "$exp_len" ] && cmp -s test.bin rt_dec.bin 2>/dev/null; then
        check 0 "roundtrip $name"
    elif [ "$exp_len" -eq 4 ] && [ "$actual_len" -eq 4 ] && cmp -s z85.bin rt_dec.bin 2>/dev/null; then
        check 0 "roundtrip $name"
    else
        check 1 "roundtrip $name"
    fi
    rm -f rt.txt rt_dec.bin
    tnum=$((tnum + 1))
done

# ========== T17: no encoding
echo ""
echo "--- T17: no encoding ---"
./"$OUTPUT" test.bin >/dev/null 2>&1
[ $? -ne 0 ]
check $? "no encoding fails"

# ========== T18: --help
echo ""
echo "--- T18: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "help"

# ========== T19: --version
echo ""
echo "--- T19: --version ---"
./"$OUTPUT" --version | grep -q "basenc"
check $? "version"

# ========== T20: z85 wrong length
echo ""
echo "--- T20: z85 wrong length ---"
./"$OUTPUT" --z85 test.bin >/dev/null 2>&1
[ $? -ne 0 ]
check $? "z85 wrong length fails"

# ========== T21: -w 0 no wrap
echo ""
echo "--- T21: -w 0 no wrap ---"
lines=$(./"$OUTPUT" -w 0 --base64 big.bin | wc -l)
[ "$lines" -eq 1 ]
check $? "wrap 0 single line"

# ========== T22: default wrap 76
echo ""
echo "--- T22: default wrap 76 ---"
maxlen=$(./"$OUTPUT" --base64 big.bin | awk '{ if (length > m) m = length } END { print m }')
[ "$maxlen" -le 76 ]
check $? "default wrap 76"

# ========== T23: invalid input
echo ""
echo "--- T23: invalid input ---"
echo "@@@" | ./"$OUTPUT" -d --base64 >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid input fails"

# ========== T24: ignore-garbage
echo ""
echo "--- T24: ignore-garbage ---"
echo "/k@+C" | ./"$OUTPUT" -d --base64 >/dev/null 2>&1
[ $? -ne 0 ]
check $? "garbage without -i fails"
out=$(echo "/k@+C" | ./"$OUTPUT" -d -i --base64 2>/dev/null | xxd -p)
[ "$out" = "fe4f82" ]
check $? "garbage with -i decodes"

# ========== T25: multiple encodings
echo ""
echo "--- T25: multiple encodings ---"
./"$OUTPUT" --base64 --base32 test.bin >/dev/null 2>&1
[ $? -ne 0 ]
check $? "multiple encodings fails"

# ========== T26: empty file encode
echo ""
echo "--- T26: empty file encode ---"
printf '' > empty.bin
out=$(./"$OUTPUT" --base64 empty.bin)
[ -z "$out" ]
check $? "empty file encode"

# ========== T27: stdin encode
echo ""
echo "--- T27: stdin encode ---"
out=$(./"$OUTPUT" --base16 < test.bin)
[ "$out" = "FE4F82" ]
check $? "stdin encode"

# ========== T28: file not found
echo ""
echo "--- T28: file not found ---"
./"$OUTPUT" --base64 nonexistent_file >/dev/null 2>&1
[ $? -ne 0 ]
check $? "file not found fails"

# Cleanup
rm -f test.bin z85.bin big.bin empty.bin

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
