#!/bin/bash
# Build and test script for realpath.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    realpath.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=realpath
SOURCE=realpath.c
PASS=0
FAIL=0

PLATFORM="unknown"
if [ "$(uname -s)" = "Linux" ]; then PLATFORM="linux"
elif [ "$(uname -s)" = "Darwin" ]; then PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then PLATFORM="netbsd"; fi

echo ""; echo "Detected platform: $PLATFORM"
echo ""; echo "[1/3] Cleaning previous build..."; rm -f "$OUTPUT"
echo ""; echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS -o $OUTPUT $SOURCE"; echo ""
$CC $CFLAGS -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then echo ""; echo "[ERROR] Build failed"; exit 1; fi
echo ""; echo "[3/3] Build succeeded!"; echo "  Output: $(pwd)/$OUTPUT"
echo ""; echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then echo "  [PASS] $2"; PASS=$((PASS + 1))
    else echo "  [FAIL] $2"; FAIL=$((FAIL + 1)); fi
}

CWD=$(pwd)
TEST_DIR="$CWD/_test_dir"
mkdir -p "$TEST_DIR/sub/deep"
echo "hello" > "$TEST_DIR/file.txt"
ln -sf "$TEST_DIR/file.txt" "$TEST_DIR/link.txt" 2>/dev/null
echo "content" > "$TEST_DIR/sub/deep/nested.txt"

# ========== T01: basic resolution of existing file
echo ""; echo "--- T01: basic resolution ---"
out=$(./"$OUTPUT" "$TEST_DIR/file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "basic resolution"

# ========== T02: -e canonicalize-existing
echo ""; echo "--- T02: -e canonicalize-existing ---"
out=$(./"$OUTPUT" -e "$TEST_DIR/file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "canonicalize-existing"

# ========== T03: -e fails on missing file
echo ""; echo "--- T03: -e fails on missing ---"
./"$OUTPUT" -e "$TEST_DIR/nonexistent" >/dev/null 2>&1
[ $? -ne 0 ]; check $? "canonicalize-existing fails on missing"

# ========== T04: -m canonicalize-missing
echo ""; echo "--- T04: -m canonicalize-missing ---"
out=$(./"$OUTPUT" -m "$TEST_DIR/nonexistent")
[ "$out" = "$TEST_DIR/nonexistent" ]; check $? "canonicalize-missing"

# ========== T05: -m with ../ in path
echo ""; echo "--- T05: -m with ../ ---"
out=$(./"$OUTPUT" -m "$TEST_DIR/sub/../nonexistent")
[ "$out" = "$TEST_DIR/nonexistent" ]; check $? "canonicalize-missing with .."

# ========== T06: default mode allows missing last component
echo ""; echo "--- T06: default allows missing last ---"
out=$(./"$OUTPUT" "$TEST_DIR/nonexistent")
[ "$out" = "$TEST_DIR/nonexistent" ]; check $? "default allows missing last"

# ========== T07: default mode fails on missing parent
echo ""; echo "--- T07: default fails on missing parent ---"
./"$OUTPUT" "$TEST_DIR/nodir/nonexistent" >/dev/null 2>&1
[ $? -ne 0 ]; check $? "default fails on missing parent"

# ========== T08: symlink resolution
echo ""; echo "--- T08: symlink resolution ---"
out=$(./"$OUTPUT" "$TEST_DIR/link.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "symlink resolution"

# ========== T09: -s no-symlinks
echo ""; echo "--- T09: -s no-symlinks ---"
out=$(./"$OUTPUT" -s "$TEST_DIR/link.txt")
[ "$out" = "$TEST_DIR/link.txt" ]; check $? "no-symlinks keeps link path"

# ========== T10: -s with -m
echo ""; echo "--- T10: -s -m ---"
out=$(./"$OUTPUT" -s -m "$TEST_DIR/sub/../nonexistent")
[ "$out" = "$TEST_DIR/nonexistent" ]; check $? "strip with missing"

# ========== T11: -q quiet
echo ""; echo "--- T11: -q quiet ---"
err=$(./"$OUTPUT" -q "$TEST_DIR/nodir/file" 2>&1 >/dev/null)
[ -z "$err" ]; check $? "quiet suppresses errors"

# ========== T12: relative path .
echo ""; echo "--- T12: relative path . ---"
out=$(./"$OUTPUT" -m ".")
[ "$out" = "$CWD" ]; check $? "relative dot resolves to cwd"

# ========== T13: relative path with ..
echo ""; echo "--- T13: relative path with .. ---"
out=$(./"$OUTPUT" -m "$TEST_DIR/sub/../file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "relative path with .."

# ========== T14: --relative-to
echo ""; echo "--- T14: --relative-to ---"
out=$(./"$OUTPUT" --relative-to="$TEST_DIR" "$TEST_DIR/file.txt")
[ "$out" = "file.txt" ]; check $? "relative-to basic"

# ========== T15: --relative-to with subdirectory
echo ""; echo "--- T15: --relative-to subdirectory ---"
out=$(./"$OUTPUT" --relative-to="$TEST_DIR" "$TEST_DIR/sub/deep/nested.txt")
[ "$out" = "sub/deep/nested.txt" ]; check $? "relative-to subdir"

# ========== T16: --relative-to with ..
echo ""; echo "--- T16: --relative-to with .. ---"
out=$(./"$OUTPUT" --relative-to="$TEST_DIR/sub" "$TEST_DIR/file.txt")
[ "$out" = "../file.txt" ]; check $? "relative-to parent"

# ========== T17: --relative-base
echo ""; echo "--- T17: --relative-base ---"
out=$(./"$OUTPUT" --relative-base="$TEST_DIR" "$TEST_DIR/file.txt")
[ "$out" = "file.txt" ]; check $? "relative-base below base"

# ========== T18: --relative-base prints absolute when outside
echo ""; echo "--- T18: --relative-base outside ---"
out=$(./"$OUTPUT" --relative-base="$TEST_DIR/sub/deep" "$TEST_DIR/file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "relative-base outside prints absolute"

# ========== T19: multiple files
echo ""; echo "--- T19: multiple files ---"
out=$(./"$OUTPUT" "$TEST_DIR/file.txt" "$TEST_DIR/sub/deep/nested.txt")
echo "$out" | wc -l | grep -q '2'; check $? "multiple files"

# ========== T20: -z zero terminated
echo ""; echo "--- T20: -z zero terminated ---"
printf '%s\0' "$TEST_DIR/file.txt" | ./"$OUTPUT" -z "$TEST_DIR/file.txt" > t_z.out 2>/dev/null
if od -c t_z.out 2>/dev/null | head -1 | grep -q '\\0'; then
    check 0 "zero terminated"
else
    check 1 "zero terminated"
fi
rm -f t_z.out

# ========== T21: --help exits 0
echo ""; echo "--- T21: --help ---"
./"$OUTPUT" --help >/dev/null 2>&1; [ $? -eq 0 ]; check $? "help exits 0"

# ========== T22: --version exits 0
echo ""; echo "--- T22: --version ---"
./"$OUTPUT" --version >/dev/null 2>&1; [ $? -eq 0 ]; check $? "version exits 0"

# ========== T23: no operand fails
echo ""; echo "--- T23: no operand ---"
./"$OUTPUT" >/dev/null 2>&1; [ $? -ne 0 ]; check $? "no operand fails"

# ========== T24: -- separator
echo ""; echo "--- T24: -- separator ---"
out=$(./"$OUTPUT" -- "$TEST_DIR/file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "double dash separator"

# ========== T25: -e with directory
echo ""; echo "--- T25: -e with directory ---"
out=$(./"$OUTPUT" -e "$TEST_DIR/sub")
[ "$out" = "$TEST_DIR/sub" ]; check $? "canonicalize-existing directory"

# ========== T26: double slash cleanup
echo ""; echo "--- T26: double slash cleanup ---"
out=$(./"$OUTPUT" -m "$TEST_DIR//file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "double slash cleanup"

# ========== T27: ./ prefix cleanup
echo ""; echo "--- T27: ./ prefix cleanup ---"
out=$(./"$OUTPUT" -m "$TEST_DIR/./file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "dot-slash cleanup"

# ========== T28: trailing slash on directory
echo ""; echo "--- T28: trailing slash ---"
out=$(./"$OUTPUT" -m "$TEST_DIR/sub/")
[ "$out" = "$TEST_DIR/sub" ]; check $? "trailing slash removed"

# ========== T29: -e -s combined
echo ""; echo "--- T29: -e -s combined ---"
out=$(./"$OUTPUT" -e -s "$TEST_DIR/file.txt")
[ "$out" = "$TEST_DIR/file.txt" ]; check $? "combined -e -s"

# ========== T30: --relative-to with -m
echo ""; echo "--- T30: --relative-to with -m ---"
out=$(./"$OUTPUT" -m --relative-to="$TEST_DIR" "$TEST_DIR/nonexistent")
[ "$out" = "nonexistent" ]; check $? "relative-to with missing"

# Cleanup
rm -rf "$TEST_DIR"

echo ""; echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then exit 1; else exit 0; fi
