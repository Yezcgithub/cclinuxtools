#!/bin/bash
# Build and test script for b2sum.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — b2sum --check returns 1 for failed
#       checksums, which would abort the script on those test cases.

echo "============================================"
echo "    b2sum.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=b2sum
SOURCE=b2sum.c
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
printf '' > test_empty.txt
printf 'abc' > test_abc.txt
printf 'xyz' > test_xyz.txt
printf '%0.sA' {1..1000} > test_large.txt

# ========== T01: empty file hash
echo ""
echo "--- T01: empty file hash ---"
h=$(./"$OUTPUT" test_empty.txt | awk '{print $1}')
[ "$h" = "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce" ]
check $? "empty file BLAKE2b-512"

# ========== T02: abc file hash
echo ""
echo "--- T02: abc file hash ---"
h=$(./"$OUTPUT" test_abc.txt | awk '{print $1}')
[ "$h" = "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" ]
check $? "abc file BLAKE2b-512"

# ========== T03: -l 256
echo ""
echo "--- T03: -l 256 ---"
h=$(./"$OUTPUT" -l 256 test_abc.txt | awk '{print $1}')
[ "$h" = "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319" ]
check $? "abc file BLAKE2b-256"

# ========== T04: -l 128
echo ""
echo "--- T04: -l 128 ---"
h=$(./"$OUTPUT" -l 128 test_abc.txt | awk '{print $1}')
[ "$h" = "cf4ab791c62b8d2b2109c90275287816" ]
check $? "abc file BLAKE2b-128"

# ========== T05: --tag
echo ""
echo "--- T05: --tag ---"
./"$OUTPUT" --tag test_abc.txt | grep -q "BLAKE2b (test_abc.txt)"
check $? "--tag format"

# ========== T06: -b binary mode
echo ""
echo "--- T06: -b binary mode ---"
./"$OUTPUT" -b test_abc.txt | grep -q "\*test_abc.txt"
check $? "-b binary separator"

# ========== T07: --check OK
echo ""
echo "--- T07: --check OK ---"
./"$OUTPUT" test_abc.txt > chk.txt
./"$OUTPUT" -c chk.txt >/dev/null 2>&1
check $? "--check valid checksums"

# ========== T08: --check FAILED
echo ""
echo "--- T08: --check FAILED ---"
echo "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  test_abc.txt" > bad.txt
./"$OUTPUT" -c bad.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "--check failed checksum exits 1"

# ========== T09: --check --quiet
echo ""
echo "--- T09: --check --quiet ---"
out=$(./"$OUTPUT" --check --quiet chk.txt 2>/dev/null)
[ -z "$out" ]
check $? "--quiet produces no output"

# ========== T10: --check --status
echo ""
echo "--- T10: --check --status ---"
out=$(./"$OUTPUT" --check --status chk.txt 2>/dev/null)
[ -z "$out" ]
check $? "--status produces no output"

# ========== T11: --help
echo ""
echo "--- T11: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help contains Usage"

# ========== T12: --version
echo ""
echo "--- T12: --version ---"
./"$OUTPUT" --version | grep -q "b2sum"
check $? "--version contains b2sum"

# ========== T13: large file (multi-block)
echo ""
echo "--- T13: large file (1000 bytes) ---"
h=$(./"$OUTPUT" test_large.txt | awk '{print $1}')
[ "$h" = "ffc91d5b8c0451522646f640b093e6d0ba10cad123c5d1cf39a1b43fce76d51ebbe529f908571e141118adad4554769f0f3b8323174c07f94e7d333e28d334df" ]
check $? "large file multi-block hash"

# ========== T14: invalid -l value (not multiple of 8)
echo ""
echo "--- T14: invalid -l 100 (not multiple of 8) ---"
./"$OUTPUT" -l 100 test_abc.txt >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid -l 100 exits non-zero"

# ========== T15: -l 0 (invalid)
echo ""
echo "--- T15: -l 0 (invalid) ---"
./"$OUTPUT" -l 0 test_abc.txt >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid -l 0 exits non-zero"

# ========== T16: -l 520 (invalid, > 512)
echo ""
echo "--- T16: -l 520 (invalid, > 512) ---"
./"$OUTPUT" -l 520 test_abc.txt >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid -l 520 exits non-zero"

# ========== T17: --check tag format
echo ""
echo "--- T17: --check tag format ---"
h=$(./"$OUTPUT" test_abc.txt | awk '{print $1}')
echo "BLAKE2b (test_abc.txt) = $h" > tagchk.txt
./"$OUTPUT" -c tagchk.txt >/dev/null 2>&1
check $? "--check tag format"

# ========== T18: multi-file check
echo ""
echo "--- T18: multi-file check ---"
./"$OUTPUT" test_abc.txt test_xyz.txt > multi.txt
./"$OUTPUT" -c multi.txt >/dev/null 2>&1
check $? "multi-file check all OK"

# ========== T19: --check --ignore-missing
echo ""
echo "--- T19: --check --ignore-missing ---"
echo "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  nonexistent_file" > miss.txt
./"$OUTPUT" -c --ignore-missing miss.txt >/dev/null 2>&1
[ $? -eq 0 ]
check $? "--ignore-missing exits 0"

# ========== T20: --check missing file without --ignore-missing
echo ""
echo "--- T20: --check missing file (no --ignore-missing) ---"
./"$OUTPUT" -c miss.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "missing file exits 1"

# ========== T21: stdin hash
echo ""
echo "--- T21: stdin hash ---"
h=$(printf 'abc' | ./"$OUTPUT" - | awk '{print $1}')
[ "$h" = "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" ]
check $? "stdin hash matches"

# ========== T22: -z zero terminator
echo ""
echo "--- T22: -z zero terminator ---"
./"$OUTPUT" -z test_abc.txt | od -An -tx1 | tail -1 | grep -q "00"
check $? "-z ends with NUL"

# ========== T23: -l 8 (minimum valid)
echo ""
echo "--- T23: -l 8 (minimum valid) ---"
./"$OUTPUT" -l 8 test_abc.txt >/dev/null 2>&1
check $? "-l 8 succeeds"

# ========== T24: -l 512 (maximum valid)
echo ""
echo "--- T24: -l 512 (maximum valid) ---"
./"$OUTPUT" -l 512 test_abc.txt >/dev/null 2>&1
check $? "-l 512 succeeds"

# ========== T25: -l 512 equals default
echo ""
echo "--- T25: -l 512 equals default ---"
h1=$(./"$OUTPUT" test_abc.txt | awk '{print $1}')
h2=$(./"$OUTPUT" -l 512 test_abc.txt | awk '{print $1}')
[ "$h1" = "$h2" ]
check $? "-l 512 matches default"

# Cleanup
rm -f test_empty.txt test_abc.txt test_xyz.txt test_large.txt
rm -f chk.txt bad.txt tagchk.txt multi.txt miss.txt

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
