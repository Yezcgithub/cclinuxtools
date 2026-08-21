#!/bin/bash
set -e

echo "============================================"
echo "     ls.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="ls"
SOURCE="ls.c"
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
echo "--- Test 1: Basic listing ---"
./"$OUTPUT" > /dev/null 2>&1
check $? "basic listing"

echo ""
echo "--- Test 2: Long format (-l) ---"
./"$OUTPUT" -l > /dev/null 2>&1
check $? "ls -l (long format)"

echo ""
echo "--- Test 3: Show all (-a) ---"
mkdir -p "$TDIR/sub1"
echo test > "$TDIR/sub1/.hidden"
./"$OUTPUT" -a "$TDIR/sub1" 2>&1 | grep -q "\.hidden"
check $? "ls -a (show dotfiles)"

echo ""
echo "--- Test 4: Almost all (-A) ---"
./"$OUTPUT" -A "$TDIR/sub1" 2>&1 | grep -qv "^\.$" | grep -qv "^\.\.$" > /dev/null 2>&1
check $? "ls -A (skip . and ..)"

echo ""
echo "--- Test 5: Human readable (-h) ---"
./"$OUTPUT" -lh > /dev/null 2>&1
check $? "ls -lh (human readable)"

echo ""
echo "--- Test 6: One per line (-1) ---"
./"$OUTPUT" -1 > /dev/null 2>&1
check $? "ls -1 (one per line)"

echo ""
echo "--- Test 7: Sort by size (-S) ---"
./"$OUTPUT" -S > /dev/null 2>&1
check $? "ls -S (sort by size)"

echo ""
echo "--- Test 8: Sort by time (-t) ---"
./"$OUTPUT" -t > /dev/null 2>&1
check $? "ls -t (sort by time)"

echo ""
echo "--- Test 9: Reverse sort (-r) ---"
./"$OUTPUT" -r > /dev/null 2>&1
check $? "ls -r (reverse sort)"

echo ""
echo "--- Test 10: Classify (-F) ---"
echo "#!/bin/sh" > "$TDIR/test.sh"
chmod +x "$TDIR/test.sh"
./"$OUTPUT" -F "$TDIR" 2>&1 | grep -q "\*"
check $? "ls -F (classify)"

echo ""
echo "--- Test 11: Quote names (-Q) ---"
./"$OUTPUT" -Q 2>&1 | grep -q '"'
check $? "ls -Q (quote names)"

echo ""
echo "--- Test 12: Show inode (-i) ---"
./"$OUTPUT" -i > /dev/null 2>&1
check $? "ls -i (show inode)"

echo ""
echo "--- Test 13: Show blocks (-s) ---"
./"$OUTPUT" -s > /dev/null 2>&1
check $? "ls -s (show blocks)"

echo ""
echo "--- Test 14: Recursive (-R) ---"
mkdir -p "$TDIR/r1/r2"
echo f > "$TDIR/r1/r2/f.txt"
./"$OUTPUT" -R "$TDIR/r1" 2>&1 | grep -q "f.txt"
check $? "ls -R (recursive)"

echo ""
echo "--- Test 15: Directory only (-d) ---"
./"$OUTPUT" -d . > /dev/null 2>&1
check $? "ls -d (directory only)"

echo ""
echo "--- Test 16: Slash indicator (-p) ---"
./"$OUTPUT" -p "$TDIR" 2>&1 | grep -q "/"
check $? "ls -p (slash indicator)"

echo ""
echo "--- Test 17: Group directories first ---"
./"$OUTPUT" --group-directories-first > /dev/null 2>&1
check $? "ls --group-directories-first"

echo ""
echo "--- Test 18: Sort by extension (-X) ---"
./"$OUTPUT" -X > /dev/null 2>&1
check $? "ls -X (sort by extension)"

echo ""
echo "--- Test 19: Sort by version (-v) ---"
./"$OUTPUT" -v > /dev/null 2>&1
check $? "ls -v (version sort)"

echo ""
echo "--- Test 20: Version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "ls --version"

echo ""
echo "--- Test 21: Help ---"
./"$OUTPUT" --help 2>&1 | grep -q "Usage"
check $? "ls --help"

echo ""
echo "--- Test 22: Columnar (-C) ---"
./"$OUTPUT" -C > /dev/null 2>&1
check $? "ls -C (columnar)"

echo ""
echo "--- Test 23: Across (-x) ---"
./"$OUTPUT" -x > /dev/null 2>&1
check $? "ls -x (across)"

echo ""
echo "--- Test 24: Color mode ---"
./"$OUTPUT" --color=never > /dev/null 2>&1
check $? "ls --color=never"

echo ""
echo "--- Test 25: Error on nonexistent file ---"
./"$OUTPUT" "/this/path/should/not/exist_xyz" 2>/dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "ls nonexistent (error)"

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
