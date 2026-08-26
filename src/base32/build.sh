#!/bin/bash
# Build and test script for base32.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — base32 -d returns 1 for invalid input,
#       which would abort the script on those test cases.

echo "============================================"
echo "    base32.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=base32
SOURCE=base32.c
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
printf '' > empty.bin
printf 'f' > f.bin
printf 'fo' > fo.bin
printf 'foo' > foo.bin
printf 'foob' > foob.bin
printf 'fooba' > fooba.bin
printf 'foobar' > foobar.bin
printf '%0.sA' {1..1000} > big.bin

# ========== T01: encode empty
echo ""
echo "--- T01: encode empty ---"
out=$(./"$OUTPUT" empty.bin)
[ -z "$out" ]
check $? "encode empty"

# ========== T02: encode f
echo ""
echo "--- T02: encode f ---"
out=$(./"$OUTPUT" f.bin)
[ "$out" = "MY======" ]
check $? "encode f"

# ========== T03: encode fo
echo ""
echo "--- T03: encode fo ---"
out=$(./"$OUTPUT" fo.bin)
[ "$out" = "MZXQ====" ]
check $? "encode fo"

# ========== T04: encode foo
echo ""
echo "--- T04: encode foo ---"
out=$(./"$OUTPUT" foo.bin)
[ "$out" = "MZXW6===" ]
check $? "encode foo"

# ========== T05: encode foob
echo ""
echo "--- T05: encode foob ---"
out=$(./"$OUTPUT" foob.bin)
[ "$out" = "MZXW6YQ=" ]
check $? "encode foob"

# ========== T06: encode fooba
echo ""
echo "--- T06: encode fooba ---"
out=$(./"$OUTPUT" fooba.bin)
[ "$out" = "MZXW6YTB" ]
check $? "encode fooba"

# ========== T07: encode foobar
echo ""
echo "--- T07: encode foobar ---"
out=$(./"$OUTPUT" foobar.bin)
[ "$out" = "MZXW6YTBOI======" ]
check $? "encode foobar"

# ========== T08: decode f
echo ""
echo "--- T08: decode f ---"
out=$(echo "MY======" | ./"$OUTPUT" -d)
[ "$out" = "f" ]
check $? "decode f"

# ========== T09: decode foobar
echo ""
echo "--- T09: decode foobar ---"
out=$(echo "MZXW6YTBOI======" | ./"$OUTPUT" -d)
[ "$out" = "foobar" ]
check $? "decode foobar"

# ========== T10: roundtrip foobar
echo ""
echo "--- T10: roundtrip foobar ---"
out=$(./"$OUTPUT" foobar.bin | ./"$OUTPUT" -d)
[ "$out" = "foobar" ]
check $? "roundtrip foobar"

# ========== T11: base32hex encode f
echo ""
echo "--- T11: base32hex encode f ---"
out=$(./"$OUTPUT" --base32hex f.bin)
[ "$out" = "CO======" ]
check $? "base32hex encode f"

# ========== T12: base32hex encode foobar
echo ""
echo "--- T12: base32hex encode foobar ---"
out=$(./"$OUTPUT" --base32hex foobar.bin)
[ "$out" = "CPNMUOJ1E8======" ]
check $? "base32hex encode foobar"

# ========== T13: base32hex roundtrip
echo ""
echo "--- T13: base32hex roundtrip ---"
out=$(./"$OUTPUT" --base32hex foobar.bin | ./"$OUTPUT" -d --base32hex)
[ "$out" = "foobar" ]
check $? "base32hex roundtrip"

# ========== T14: -w 0 (no wrap)
echo ""
echo "--- T14: -w 0 (no wrap) ---"
lines=$(./"$OUTPUT" -w 0 big.bin | wc -l)
[ "$lines" -eq 1 ]
check $? "wrap 0 single line"

# ========== T15: -w 10
echo ""
echo "--- T15: -w 10 ---"
lines=$(./"$OUTPUT" -w 10 big.bin | wc -l)
[ "$lines" -gt 1 ]
check $? "wrap 10 multi line"

# ========== T16: ignore-garbage
echo ""
echo "--- T16: ignore-garbage ---"
echo "MY@======" | ./"$OUTPUT" -d >/dev/null 2>&1
[ $? -ne 0 ]
check $? "garbage without -i fails"
out=$(echo "MY@======" | ./"$OUTPUT" -d -i)
[ "$out" = "f" ]
check $? "garbage with -i decodes"

# ========== T17: invalid input
echo ""
echo "--- T17: invalid input ---"
echo "@@@@@@@@@" | ./"$OUTPUT" -d >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid input fails"

# ========== T18: no padding decode
echo ""
echo "--- T18: no padding decode ---"
out=$(echo "MZXW6YTBOI" | ./"$OUTPUT" -d)
[ "$out" = "foobar" ]
check $? "no padding decode"

# ========== T19: --help
echo ""
echo "--- T19: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "help"

# ========== T20: --version
echo ""
echo "--- T20: --version ---"
./"$OUTPUT" --version | grep -q "base32"
check $? "version"

# ========== T21: default wrap 76
echo ""
echo "--- T21: default wrap 76 ---"
maxlen=$(./"$OUTPUT" big.bin | awk '{ if (length > m) m = length } END { print m }')
[ "$maxlen" -le 76 ]
check $? "default wrap 76"

# ========== T22: lowercase decode
echo ""
echo "--- T22: lowercase decode ---"
out=$(echo "mzxw6ytboi======" | ./"$OUTPUT" -d)
[ "$out" = "foobar" ]
check $? "lowercase decode"

# ========== T23: multi-file encode
echo ""
echo "--- T23: multi-file encode ---"
lines=$(./"$OUTPUT" f.bin fo.bin | wc -l)
[ "$lines" -eq 2 ]
check $? "multi-file encode 2 lines"

# ========== T24: invalid -w value
echo ""
echo "--- T24: invalid -w -1 ---"
./"$OUTPUT" -w -1 f.bin >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid -w -1"

# ========== T25: stdin encode
echo ""
echo "--- T25: stdin encode ---"
out=$(./"$OUTPUT" < foo.bin)
[ "$out" = "MZXW6===" ]
check $? "stdin encode"

# ========== T26: stdin decode
echo ""
echo "--- T26: stdin decode ---"
out=$(./"$OUTPUT" -d < foobar.bin 2>/dev/null)
[ "$out" = "foobar" ]
check $? "stdin decode via redirection"

# ========== T26b: stdin decode via pipe (same as T09)
echo ""
echo "--- T26b: stdin decode via pipe ---"
out=$(echo "MZXW6YTBOI======" | ./"$OUTPUT" -d)
[ "$out" = "foobar" ]
check $? "stdin decode via pipe"

# ========== T27: file not found
echo ""
echo "--- T27: file not found ---"
./"$OUTPUT" nonexistent_file >/dev/null 2>&1
[ $? -ne 0 ]
check $? "file not found fails"

# Cleanup
rm -f empty.bin f.bin fo.bin foo.bin foob.bin fooba.bin foobar.bin big.bin

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
