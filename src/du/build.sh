#!/bin/bash
# Build and test script for du.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    du.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=du
SOURCE=du.c
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

# Test directory structure
TD="/tmp/dutest_$$"
mkdir -p "$TD/subdir"
printf 'hello\n' > "$TD/file1.txt"
printf 'world!\n' > "$TD/file2.txt"
printf 'test\n' > "$TD/subdir/file3.txt"
printf 'data\n' > "$TD/subdir/file4.txt"

# ========== T01: Basic du
echo ""
echo "--- T01: Basic du ---"
./"$OUTPUT" "$TD" 2>/dev/null | grep -q "$TD"
check $? "basic du"

# ========== T02: -h human readable
echo ""
echo "--- T02: -h human readable ---"
./"$OUTPUT" -h "$TD" 2>/dev/null | grep -qE '[0-9][KMGT]'
check $? "-h human readable"

# ========== T03: -a all files
echo ""
echo "--- T03: -a all files ---"
./"$OUTPUT" -a "$TD" 2>/dev/null | grep -q "file1.txt"
check $? "-a shows file1.txt"

# ========== T04: -s summarize
echo ""
echo "--- T04: -s summarize ---"
result=$(./"$OUTPUT" -s "$TD" 2>/dev/null)
echo "$result" | grep -q "$TD"
check $? "-s shows top-level"
! echo "$result" | grep -q "subdir"
check $? "-s hides subdirectories"

# ========== T05: -c total
echo ""
echo "--- T05: -c total ---"
./"$OUTPUT" -c "$TD" 2>/dev/null | grep -q "total"
check $? "-c shows total"

# ========== T06: -k 1K blocks
echo ""
echo "--- T06: -k 1K blocks ---"
./"$OUTPUT" -k "$TD" 2>/dev/null | grep -qE '^[0-9]+'
check $? "-k produces numeric output"

# ========== T07: -d 0 max-depth=0
echo ""
echo "--- T07: -d 0 max-depth=0 ---"
result=$(./"$OUTPUT" -d 0 "$TD" 2>/dev/null)
echo "$result" | grep -q "$TD"
check $? "-d 0 shows top-level"
! echo "$result" | grep -q "subdir"
check $? "-d 0 hides subdirectories"

# ========== T08: -S separate-dirs
echo ""
echo "--- T08: -S separate-dirs ---"
./"$OUTPUT" -S "$TD" 2>/dev/null | grep -q "subdir"
check $? "-S shows subdirectory separately"

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

# ========== T11: -b bytes
echo ""
echo "--- T11: -b bytes ---"
./"$OUTPUT" -b -a "$TD" 2>/dev/null | grep -q "file1.txt"
check $? "-b shows files"

# ========== T12: non-existent directory
echo ""
echo "--- T12: non-existent directory ---"
./"$OUTPUT" "/nonexistent_dir_xyz" 2>/dev/null
[ $? -ne 0 ]
check $? "non-existent directory returns error"

# ========== T13: --exclude pattern
echo ""
echo "--- T13: --exclude pattern ---"
result=$(./"$OUTPUT" -a --exclude='*.txt' "$TD" 2>/dev/null)
! echo "$result" | grep -q "file1.txt"
check $? "--exclude removes .txt files"

# ========== T14: -d 1 max-depth=1
echo ""
echo "--- T14: -d 1 max-depth=1 ---"
result=$(./"$OUTPUT" -d 1 "$TD" 2>/dev/null)
echo "$result" | grep -q "subdir"
check $? "-d 1 shows first level"

# ========== T15: multiple arguments
echo ""
echo "--- T15: multiple arguments ---"
result=$(./"$OUTPUT" "$TD/file1.txt" "$TD/file2.txt" 2>/dev/null)
echo "$result" | grep -q "file1.txt"
check $? "multiple args: file1"
echo "$result" | grep -q "file2.txt"
check $? "multiple args: file2"

# ========== T16: no arguments (current dir)
echo ""
echo "--- T16: no arguments (current dir) ---"
cd "$TD"
"$(pwd)/$OUTPUT" 2>/dev/null | grep -q "."
check $? "no args runs on current dir"
cd - > /dev/null

# ========== T17: -B 1 custom block size
echo ""
echo "--- T17: -B 1 custom block size ---"
./"$OUTPUT" -B 1 -a "$TD" 2>/dev/null | grep -q "file1.txt"
check $? "-B 1 shows files"

# ========== T18: --apparent-size
echo ""
echo "--- T18: --apparent-size ---"
./"$OUTPUT" --apparent-size -a "$TD" 2>/dev/null | grep -q "file1.txt"
check $? "--apparent-size shows files"

# ========== T19: --si
echo ""
echo "--- T19: --si ---"
./"$OUTPUT" --si "$TD" 2>/dev/null | grep -qE '[0-9][kMGT]'
check $? "--si produces human output"

# ========== T20: -m 1M blocks
echo ""
echo "--- T20: -m 1M blocks ---"
./"$OUTPUT" -m "$TD" 2>/dev/null | grep -q "$TD"
check $? "-m shows top-level"

# ========== T21: -x one-file-system (POSIX only)
echo ""
echo "--- T21: -x one-file-system ---"
./"$OUTPUT" -x "$TD" 2>/dev/null | grep -q "$TD"
check $? "-x runs without error"

# ========== T22: -l count-links
echo ""
echo "--- T22: -l count-links ---"
./"$OUTPUT" -l "$TD" 2>/dev/null | grep -q "$TD"
check $? "-l runs without error"

# ========== T23: -L dereference
echo ""
echo "--- T23: -L dereference ---"
ln -sf "$TD/file1.txt" "$TD/link_to_file" 2>/dev/null || true
./"$OUTPUT" -L -a "$TD" 2>/dev/null | grep -q "file1.txt"
check $? "-L dereference runs"

# ========== T24: --block-size=1K
echo ""
echo "--- T24: --block-size=1K ---"
./"$OUTPUT" --block-size=1K "$TD" 2>/dev/null | grep -q "$TD"
check $? "--block-size=1K"

# ========== T25: -sh combined
echo ""
echo "--- T25: -sh combined ---"
./"$OUTPUT" -sh "$TD" 2>/dev/null | grep -qE '[0-9]'
check $? "-sh combined works"

# Cleanup
rm -rf "$TD"

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
