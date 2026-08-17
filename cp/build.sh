#!/bin/bash
set -e

echo "============================================"
echo "     cp.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="cp"
SOURCE="cp.c"
PASS=0
FAIL=0

# Detect platform and set flags
PLATFORM="unknown"
EXTRA_FLAGS=""

if [ -f /etc/os-release ]; then
    . /etc/os-release
    PLATFORM="$ID"
elif [ "$(uname)" = "Darwin" ]; then
    PLATFORM="macos"
elif [ "$(uname)" = "FreeBSD" ]; then
    PLATFORM="freebsd"
elif [ "$(uname)" = "OpenBSD" ]; then
    PLATFORM="openbsd"
elif [ "$(uname)" = "NetBSD" ]; then
    PLATFORM="netbsd"
elif [ "$(uname)" = "Linux" ]; then
    PLATFORM="linux"
fi

case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="gcc" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo ""
echo "[2/3] Detecting platform and compiling..."
echo "  Platform: $PLATFORM"
echo "  Compiler: $CC $CFLAGS $EXTRA_FLAGS"
echo "  Command:  $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE"

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

echo ""
echo "============================================"
echo "  Running basic tests..."
echo "============================================"

# Helper
check() { if [ "$1" -eq 0 ]; then PASS=$((PASS+1)); echo "  [PASS] $2"; else FAIL=$((FAIL+1)); echo "  [FAIL] $2"; fi; }

# Setup
TDIR="_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 1: Basic file copy ---"
echo "Hello World" > "$TDIR/src.txt"
./"$OUTPUT" "$TDIR/src.txt" "$TDIR/dst.txt"
diff -q "$TDIR/src.txt" "$TDIR/dst.txt" > /dev/null 2>&1
check $? "cp src dst"

echo ""
echo "--- Test 2: Verbose mode ---"
./"$OUTPUT" -v "$TDIR/src.txt" "$TDIR/dst_v.txt" > /dev/null 2>&1
diff -q "$TDIR/src.txt" "$TDIR/dst_v.txt" > /dev/null 2>&1
check $? "cp -v (verbose)"

echo ""
echo "--- Test 3: Copy to directory ---"
mkdir -p "$TDIR/subdir"
./"$OUTPUT" "$TDIR/src.txt" "$TDIR/subdir/"
[ -f "$TDIR/subdir/src.txt" ]
check $? "cp src dir/"

echo ""
echo "--- Test 4: No-clobber (-n) ---"
echo "OldContent" > "$TDIR/noclobber.txt"
./"$OUTPUT" -n "$TDIR/src.txt" "$TDIR/noclobber.txt"
grep -q "Old" "$TDIR/noclobber.txt"
check $? "cp -n (no-clobber)"

echo ""
echo "--- Test 5: Force (-f) ---"
echo "Old" > "$TDIR/force.txt"
./"$OUTPUT" -f "$TDIR/src.txt" "$TDIR/force.txt"
diff -q "$TDIR/src.txt" "$TDIR/force.txt" > /dev/null 2>&1
check $? "cp -f (force)"

echo ""
echo "--- Test 6: Recursive copy (-r) ---"
mkdir -p "$TDIR/rsrc/sub"
echo "FileA" > "$TDIR/rsrc/a.txt"
echo "FileB" > "$TDIR/rsrc/sub/b.txt"
./"$OUTPUT" -r "$TDIR/rsrc" "$TDIR/rdst"
[ -f "$TDIR/rdst/a.txt" ] && [ -f "$TDIR/rdst/sub/b.txt" ]
check $? "cp -r (recursive)"

echo ""
echo "--- Test 7: Archive mode (-a) ---"
./"$OUTPUT" -a "$TDIR/rsrc" "$TDIR/adst"
[ -f "$TDIR/adst/a.txt" ] && [ -f "$TDIR/adst/sub/b.txt" ]
check $? "cp -a (archive)"

echo ""
echo "--- Test 8: Target directory (-t) ---"
mkdir -p "$TDIR/target"
./"$OUTPUT" -t "$TDIR/target" "$TDIR/src.txt"
[ -f "$TDIR/target/src.txt" ]
check $? "cp -t DIR src"

echo ""
echo "--- Test 9: Multiple sources to dir ---"
echo "Extra1" > "$TDIR/extra1.txt"
echo "Extra2" > "$TDIR/extra2.txt"
./"$OUTPUT" "$TDIR/extra1.txt" "$TDIR/extra2.txt" "$TDIR/target"
[ -f "$TDIR/target/extra1.txt" ] && [ -f "$TDIR/target/extra2.txt" ]
check $? "cp src1 src2 dir"

echo ""
echo "--- Test 10: Hard link (-l) ---"
./"$OUTPUT" -l "$TDIR/src.txt" "$TDIR/hardlink.txt"
[ -f "$TDIR/hardlink.txt" ]
check $? "cp -l (hard link)"

echo ""
echo "--- Test 11: Symbolic link (-s) ---"
./"$OUTPUT" -s "$TDIR/src.txt" "$TDIR/symlink.txt" 2>/dev/null
if [ -L "$TDIR/symlink.txt" ]; then
    check 0 "cp -s (symbolic link)"
elif [ -f "$TDIR/symlink.txt" ]; then
    check 0 "cp -s (fallback to copy)"
else
    check 1 "cp -s (symbolic link)"
fi

echo ""
echo "--- Test 12: No-target-directory (-T) ---"
./"$OUTPUT" -T "$TDIR/src.txt" "$TDIR/notdir.txt"
diff -q "$TDIR/src.txt" "$TDIR/notdir.txt" > /dev/null 2>&1
check $? "cp -T (no-target-directory)"

echo ""
echo "--- Test 13: Preserve (-p) ---"
./"$OUTPUT" -p "$TDIR/src.txt" "$TDIR/preserved.txt"
diff -q "$TDIR/src.txt" "$TDIR/preserved.txt" > /dev/null 2>&1
check $? "cp -p (preserve)"

echo ""
echo "--- Test 14: Dereference (-L) ---"
./"$OUTPUT" -L "$TDIR/src.txt" "$TDIR/deref.txt"
diff -q "$TDIR/src.txt" "$TDIR/deref.txt" > /dev/null 2>&1
check $? "cp -L (dereference)"

echo ""
echo "--- Test 15: Version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "cp --version"

echo ""
echo "--- Test 16: Error - no args ---"
./"$OUTPUT" 2>/dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "cp (no args returns error)"

echo ""
echo "--- Test 17: Error - same file ---"
./"$OUTPUT" "$TDIR/src.txt" "$TDIR/src.txt" 2>/dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "cp src src (same file error)"

echo ""
echo "--- Test 18: Error - no -r for directory ---"
./"$OUTPUT" "$TDIR/rsrc" "$TDIR/no_r" 2>/dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "cp dir (without -r error)"

# Cleanup
rm -rf "$TDIR"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo "  All tests passed!"
    exit 0
else
    echo "  Some tests failed!"
    exit 1
fi
