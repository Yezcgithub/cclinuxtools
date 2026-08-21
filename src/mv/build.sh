#!/bin/bash
# Build and test script for mv.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    mv.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=mv
SOURCE=mv.c
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
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="gcc" ;;
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

TDIR="/tmp/mv_ftest_$$"
rm -rf "$TDIR"
mkdir -p "$TDIR"

check() {
    if [ "$1" -eq 0 ]; then
        echo "  [PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

# ========== T01: Basic file move (rename)
echo ""
echo "--- T01: Basic file move ---"
echo "test" > "$TDIR/f1.txt"
./"$OUTPUT" "$TDIR/f1.txt" "$TDIR/f2.txt" > /dev/null 2>&1
[ ! -f "$TDIR/f1.txt" ] && [ -f "$TDIR/f2.txt" ]
check $? "basic file move"

# ========== T02: Multiple files to directory
echo ""
echo "--- T02: Multiple files to directory ---"
echo "a" > "$TDIR/a.txt"
echo "b" > "$TDIR/b.txt"
mkdir -p "$TDIR/d1"
./"$OUTPUT" "$TDIR/a.txt" "$TDIR/b.txt" "$TDIR/d1" > /dev/null 2>&1
[ -f "$TDIR/d1/a.txt" ] && [ -f "$TDIR/d1/b.txt" ]
check $? "multiple files to directory"

# ========== T03: Directory move
echo ""
echo "--- T03: Directory move ---"
mkdir -p "$TDIR/srcdir"
echo "c" > "$TDIR/srcdir/c.txt"
mkdir -p "$TDIR/destdir"
./"$OUTPUT" "$TDIR/srcdir" "$TDIR/destdir" > /dev/null 2>&1
[ -f "$TDIR/destdir/srcdir/c.txt" ] && [ ! -d "$TDIR/srcdir" ]
check $? "directory move"

# ========== T04: -v verbose
echo ""
echo "--- T04: -v verbose ---"
echo "v" > "$TDIR/vf.txt"
./"$OUTPUT" -v "$TDIR/vf.txt" "$TDIR/vf2.txt" 2>&1 | grep -q "moving"
check $? "-v verbose"

# ========== T05: --verbose long
echo ""
echo "--- T05: --verbose long ---"
echo "v2" > "$TDIR/v2f.txt"
./"$OUTPUT" --verbose "$TDIR/v2f.txt" "$TDIR/v2f2.txt" 2>&1 | grep -q "moving"
check $? "--verbose long"

# ========== T06: -f force overwrite
echo ""
echo "--- T06: -f force overwrite ---"
echo "old" > "$TDIR/over.txt"
echo "new" > "$TDIR/newf.txt"
./"$OUTPUT" -f "$TDIR/newf.txt" "$TDIR/over.txt" > /dev/null 2>&1
[ -f "$TDIR/over.txt" ] && [ ! -f "$TDIR/newf.txt" ]
check $? "-f force overwrite"

# ========== T07: --force long
echo ""
echo "--- T07: --force long ---"
echo "old2" > "$TDIR/over2.txt"
echo "new2" > "$TDIR/newf2.txt"
./"$OUTPUT" --force "$TDIR/newf2.txt" "$TDIR/over2.txt" > /dev/null 2>&1
[ -f "$TDIR/over2.txt" ] && [ ! -f "$TDIR/newf2.txt" ]
check $? "--force long"

# ========== T08: -n no clobber
echo ""
echo "--- T08: -n no clobber ---"
echo "keep" > "$TDIR/keep.txt"
echo "replace" > "$TDIR/repl.txt"
./"$OUTPUT" -n "$TDIR/repl.txt" "$TDIR/keep.txt" > /dev/null 2>&1
[ -f "$TDIR/keep.txt" ] && [ -f "$TDIR/repl.txt" ]
check $? "-n no clobber"

# ========== T09: --no-clobber long
echo ""
echo "--- T09: --no-clobber long ---"
echo "keep2" > "$TDIR/keep2.txt"
echo "replace2" > "$TDIR/repl2.txt"
./"$OUTPUT" --no-clobber "$TDIR/repl2.txt" "$TDIR/keep2.txt" > /dev/null 2>&1
[ -f "$TDIR/keep2.txt" ] && [ -f "$TDIR/repl2.txt" ]
check $? "--no-clobber long"

# ========== T10: -u update (dest older)
echo ""
echo "--- T10: -u update dest older ---"
echo "oldcontent" > "$TDIR/oldf.txt"
sleep 1
echo "newcontent" > "$TDIR/newf.txt"
./"$OUTPUT" -u "$TDIR/newf.txt" "$TDIR/oldf.txt" > /dev/null 2>&1
[ -f "$TDIR/oldf.txt" ] && [ ! -f "$TDIR/newf.txt" ]
check $? "-u update dest older"

# ========== T11: -u update (dest newer - skip)
echo ""
echo "--- T11: -u update dest newer ---"
echo "oldcontent" > "$TDIR/oldf2.txt"
sleep 1
echo "newcontent" > "$TDIR/newf2.txt"
./"$OUTPUT" -u "$TDIR/oldf2.txt" "$TDIR/newf2.txt" > /dev/null 2>&1
[ -f "$TDIR/oldf2.txt" ] && [ -f "$TDIR/newf2.txt" ]
check $? "-u update dest newer"

# ========== T12: --update long
echo ""
echo "--- T12: --update long ---"
echo "old3" > "$TDIR/oldf3.txt"
sleep 1
echo "new3" > "$TDIR/newf3.txt"
./"$OUTPUT" --update "$TDIR/newf3.txt" "$TDIR/oldf3.txt" > /dev/null 2>&1
[ -f "$TDIR/oldf3.txt" ] && [ ! -f "$TDIR/newf3.txt" ]
check $? "--update long"

# ========== T13: -b backup
echo ""
echo "--- T13: -b backup ---"
echo "orig" > "$TDIR/orig.txt"
echo "new" > "$TDIR/newb.txt"
./"$OUTPUT" -b "$TDIR/newb.txt" "$TDIR/orig.txt" > /dev/null 2>&1
[ -f "$TDIR/orig.txt" ] && [ -f "$TDIR/orig.txt~" ]
check $? "-b backup"

# ========== T14: --backup long
echo ""
echo "--- T14: --backup long ---"
echo "orig2" > "$TDIR/orig2.txt"
echo "new2" > "$TDIR/newb2.txt"
./"$OUTPUT" --backup "$TDIR/newb2.txt" "$TDIR/orig2.txt" > /dev/null 2>&1
[ -f "$TDIR/orig2.txt" ] && [ -f "$TDIR/orig2.txt~" ]
check $? "--backup long"

# ========== T15: --help
echo ""
echo "--- T15: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help"

# ========== T16: --version
echo ""
echo "--- T16: --version ---"
./"$OUTPUT" --version | grep -q "1.0.0"
check $? "--version"

# ========== T17: -h help
echo ""
echo "--- T17: -h help ---"
./"$OUTPUT" -h | grep -q "Usage:"
check $? "-h help"

# ========== T18: Missing operand
echo ""
echo "--- T18: Missing operand ---"
./"$OUTPUT" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "missing operand"

# ========== T19: Invalid option
echo ""
echo "--- T19: Invalid option ---"
./"$OUTPUT" -X "$TDIR/x" "$TDIR/y" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid option"

# ========== T20: -- separator
echo ""
echo "--- T20: -- separator ---"
echo "sep" > "$TDIR/sep.txt"
./"$OUTPUT" -- "$TDIR/sep.txt" "$TDIR/sep2.txt" > /dev/null 2>&1
[ -f "$TDIR/sep2.txt" ]
check $? "-- separator"

# ========== T21: Cannot move to itself
echo ""
echo "--- T21: Cannot move to itself ---"
echo "self" > "$TDIR/self.txt"
./"$OUTPUT" "$TDIR/self.txt" "$TDIR/self.txt" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "cannot move to itself"

# ========== T22: Cannot move dir to file
echo ""
echo "--- T22: Cannot move dir to file ---"
mkdir -p "$TDIR/dir2file"
echo "content" > "$TDIR/targetfile.txt"
./"$OUTPUT" "$TDIR/dir2file" "$TDIR/targetfile.txt" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "cannot move dir to file"

# ========== T23: Deep directory move
echo ""
echo "--- T23: Deep directory move ---"
mkdir -p "$TDIR/deep/a/b/c"
echo "d" > "$TDIR/deep/a/b/c/d.txt"
mkdir -p "$TDIR/destdeep"
./"$OUTPUT" "$TDIR/deep" "$TDIR/destdeep" > /dev/null 2>&1
[ -f "$TDIR/destdeep/deep/a/b/c/d.txt" ] && [ ! -d "$TDIR/deep" ]
check $? "deep directory move"

# ========== T24: -f cancels -i
echo ""
echo "--- T24: -f cancels -i ---"
echo "fi" > "$TDIR/fi.txt"
echo "newfi" > "$TDIR/newfi.txt"
./"$OUTPUT" -fi "$TDIR/newfi.txt" "$TDIR/fi.txt" > /dev/null 2>&1
[ -f "$TDIR/fi.txt" ] && [ ! -f "$TDIR/newfi.txt" ]
check $? "-f cancels -i"

# ========== T25: -n cancels -f
echo ""
echo "--- T25: -n cancels -f ---"
echo "nf" > "$TDIR/nf.txt"
echo "newnf" > "$TDIR/newnf.txt"
./"$OUTPUT" -fn "$TDIR/newnf.txt" "$TDIR/nf.txt" > /dev/null 2>&1
[ -f "$TDIR/nf.txt" ] && [ -f "$TDIR/newnf.txt" ]
check $? "-n cancels -f"

# ========== T26: File does not exist
echo ""
echo "--- T26: File does not exist ---"
./"$OUTPUT" "$TDIR/nonexistent.txt" "$TDIR/dest.txt" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "file does not exist"

# ========== T27: -f ignores nonexistent
echo ""
echo "--- T27: -f ignores nonexistent ---"
./"$OUTPUT" -f "$TDIR/nonexistent2.txt" "$TDIR/dest2.txt" > /dev/null 2>&1
[ $? -eq 0 ]
check $? "-f ignores nonexistent"

# ========== T28: -u with missing dest
echo ""
echo "--- T28: -u with missing dest ---"
echo "umiss" > "$TDIR/umiss.txt"
./"$OUTPUT" -u "$TDIR/umiss.txt" "$TDIR/unew.txt" > /dev/null 2>&1
[ -f "$TDIR/unew.txt" ] && [ ! -f "$TDIR/umiss.txt" ]
check $? "-u with missing dest"

# Cleanup
rm -rf "$TDIR"

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
