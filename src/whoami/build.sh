#!/bin/bash
# Build and test script for whoami.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "   whoami.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="whoami"
SOURCE="whoami.c"
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

check() { if [ "$1" -eq 0 ]; then PASS=$((PASS+1)); echo "  [PASS] $2"; else FAIL=$((FAIL+1)); echo "  [FAIL] $2"; fi; }

# Resolve absolute path to the executable so subshells never confuse it
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WHOAMI_EXE="$SCRIPT_DIR/$OUTPUT"

TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 1: Basic whoami ---"
./"$OUTPUT" > "$TDIR/t1.txt" 2>&1
check $? "basic whoami runs"

echo ""
echo "--- Test 2: Output is non-empty ---"
if [ -s "$TDIR/t1.txt" ]; then check 0 "output non-empty"; else check 1 "output non-empty"; fi

echo ""
echo "--- Test 3: Output matches 'id -un' ---"
OUT=$(tr -d '\n' < "$TDIR/t1.txt")
IDUN=$(id -un 2>/dev/null | tr -d '\n')
if [ -n "$IDUN" ] && [ "$OUT" = "$IDUN" ]; then
    check 0 "matches id -un"
else
    check 1 "matches id -un"
fi

echo ""
echo "--- Test 4: Single-line output ---"
LINES=$(wc -l < "$TDIR/t1.txt" | awk '{print $1}')
if [ "$LINES" -eq 1 ]; then check 0 "single line output"; else check 1 "single line output"; fi

echo ""
echo "--- Test 5: --help ---"
./"$OUTPUT" --help > "$TDIR/t5.txt" 2>&1
grep -q "Usage" "$TDIR/t5.txt"
check $? "--help mentions Usage"

echo ""
echo "--- Test 6: --version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "--version contains 1.0.0"

echo ""
echo "--- Test 7: '--' alone is accepted ---"
./"$OUTPUT" -- > "$TDIR/t7.txt" 2>&1
check $? "'--' alone runs"

echo ""
echo "--- Test 8: Unknown long option is an error ---"
"$WHOAMI_EXE" --thisoptiondoesnotexist >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown long option returns error"

echo ""
echo "--- Test 9: Unknown short option is an error ---"
"$WHOAMI_EXE" -Z >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown short option returns error"

echo ""
echo "--- Test 10: Extra operand is an error ---"
"$WHOAMI_EXE" foo >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "extra operand returns error"

echo ""
echo "--- Test 11: --help takes precedence over a following operand ---"
"$WHOAMI_EXE" --help foo > "$TDIR/t11.txt" 2>&1 && rc=0 || rc=1
if [ "$rc" -eq 0 ] && grep -q "Usage" "$TDIR/t11.txt"; then
    check 0 "--help exits 0 even with following operand"
else
    check 1 "--help exits 0 even with following operand"
fi

echo ""
echo "--- Test 12: '--' makes following token an extra operand error ---"
"$WHOAMI_EXE" -- foo >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "'-- foo' is an extra operand error"

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
