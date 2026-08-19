#!/bin/bash
# Build and test script for df.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    df.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=df
SOURCE=df.c
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
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo ""
echo "[1/3] Cleaning previous build..."
if [ -f "$OUTPUT" ]; then
    rm -f "$OUTPUT"
    echo "  Removed $OUTPUT"
fi

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE

if [ ! -f "$OUTPUT" ]; then
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

TMP_OUT="/tmp/df_out_$$"

# ========== T01: Basic df
echo ""
echo "--- T01: Basic df ---"
./"$OUTPUT" > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T02: -h human readable
echo ""
echo "--- T02: -h human readable ---"
./"$OUTPUT" -h > "$TMP_OUT" 2>&1
if grep -q "Size" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T03: -H SI
echo ""
echo "--- T03: -H SI ---"
./"$OUTPUT" -H > "$TMP_OUT" 2>&1
if grep -q "Size" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T04: -T print type
echo ""
echo "--- T04: -T print type ---"
./"$OUTPUT" -T > "$TMP_OUT" 2>&1
if grep -q "Type" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T05: --total
echo ""
echo "--- T05: --total ---"
./"$OUTPUT" --total > "$TMP_OUT" 2>&1
if grep -q "total" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T06: -i inodes
echo ""
echo "--- T06: -i inodes ---"
./"$OUTPUT" -i > "$TMP_OUT" 2>&1
if grep -q "Inodes" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T07: -k 1K blocks
echo ""
echo "--- T07: -k 1K blocks ---"
./"$OUTPUT" -k > "$TMP_OUT" 2>&1
if grep -q "1K-blocks" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T08: --help
echo ""
echo "--- T08: --help ---"
./"$OUTPUT" --help > "$TMP_OUT" 2>/dev/null
if grep -q "Usage:" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T09: --version
echo ""
echo "--- T09: --version ---"
./"$OUTPUT" --version > "$TMP_OUT" 2>/dev/null
if grep -q "9.11" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T10: -B 1M block size
echo ""
echo "--- T10: -B 1M block size ---"
./"$OUTPUT" -B 1M > "$TMP_OUT" 2>&1
if grep -q "1M-blocks" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T11: --output with fields
echo ""
echo "--- T11: --output=size,used,avail,target ---"
./"$OUTPUT" --output=size,used,avail,target > "$TMP_OUT" 2>&1
if grep -q "size" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T12: -hT combined
echo ""
echo "--- T12: -hT combined ---"
./"$OUTPUT" -hT > "$TMP_OUT" 2>&1
if grep -q "Type" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T13: -h --total combined
echo ""
echo "--- T13: -h --total combined ---"
./"$OUTPUT" -h --total > "$TMP_OUT" 2>&1
if grep -q "total" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T14: -l local only
echo ""
echo "--- T14: -l local only ---"
./"$OUTPUT" -l > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T15: -a all
echo ""
echo "--- T15: -a all ---"
./"$OUTPUT" -a > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T16: -x exclude type (tmpfs)
echo ""
echo "--- T16: -x tmpfs ---"
./"$OUTPUT" -x tmpfs > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T17: --output without value (all fields)
echo ""
echo "--- T17: --output (all fields) ---"
./"$OUTPUT" --output > "$TMP_OUT" 2>&1
if grep -q "source" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T18: -hT --total
echo ""
echo "--- T18: -hT --total ---"
./"$OUTPUT" -hT --total > "$TMP_OUT" 2>&1
if grep -q "Type" "$TMP_OUT" && grep -q "total" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T19: -B 1 byte block
echo ""
echo "--- T19: -B 1 block ---"
./"$OUTPUT" -B 1 > "$TMP_OUT" 2>&1
if grep -q "1B-blocks" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T20: -P portability
echo ""
echo "--- T20: -P portability ---"
./"$OUTPUT" -P > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T21: -t type filter
echo ""
echo "--- T21: -t ext4 ---"
./"$OUTPUT" -t ext4 > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T22: -v ignored
echo ""
echo "--- T22: -v (ignored) ---"
./"$OUTPUT" -v > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T23: file argument (root path)
echo ""
echo "--- T23: file argument (/) ---"
./"$OUTPUT" / > "$TMP_OUT" 2>&1
if grep -q "Filesystem" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T24: --output=source,fstype,target
echo ""
echo "--- T24: --output=source,fstype,target ---"
./"$OUTPUT" --output=source,fstype,target > "$TMP_OUT" 2>&1
if grep -q "source" "$TMP_OUT" && grep -q "fstype" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T25: -iT combined
echo ""
echo "--- T25: -iT combined ---"
./"$OUTPUT" -iT > "$TMP_OUT" 2>&1
if grep -q "Type" "$TMP_OUT" && grep -q "Inodes" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T26: --output mutual exclusivity with -i
echo ""
echo "--- T26: --output incompatible with -i ---"
./"$OUTPUT" --output -i > "$TMP_OUT" 2>&1
if grep -q "incompatible" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T27: --total + --output
echo ""
echo "--- T27: --total --output=source,size,used ---"
./"$OUTPUT" --total --output=source,size,used > "$TMP_OUT" 2>&1
if grep -q "total" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T28: --output split usage
echo ""
echo "--- T28: --output=target --output=pcent ---"
./"$OUTPUT" --output=target --output=pcent > "$TMP_OUT" 2>&1
if grep -q "target" "$TMP_OUT" && grep -q "pcent" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T29: DF_BLOCK_SIZE env var
echo ""
echo "--- T29: DF_BLOCK_SIZE=1M ---"
DF_BLOCK_SIZE=1M ./"$OUTPUT" > "$TMP_OUT" 2>&1
if grep -q "1M-blocks" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T30: POSIXLY_CORRECT env var
echo ""
echo "--- T30: POSIXLY_CORRECT ---"
POSIXLY_CORRECT=1 ./"$OUTPUT" > "$TMP_OUT" 2>&1
if grep -q "512-blocks" "$TMP_OUT"; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# Cleanup
rm -f "$TMP_OUT"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo "  All tests passed!"
else
    echo "  Some tests failed!"
    exit 1
fi

exit 0
