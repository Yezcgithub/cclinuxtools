#!/bin/bash
# Build and test script for top.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    top.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="top"
SOURCE="top.c"
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
echo "Detected platform: $PLATFORM"

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
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

# Resolve absolute path to the executable
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOP_EXE="$SCRIPT_DIR/$OUTPUT"

TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 1: Basic batch mode runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 > "$TDIR/t1.txt" 2>&1
check $? "batch mode -b -n 1 runs"

echo ""
echo "--- Test 2: Output is non-empty ---"
if [ -s "$TDIR/t1.txt" ]; then check 0 "output non-empty"; else check 1 "output non-empty"; fi

echo ""
echo "--- Test 3: --help mentions Usage ---"
./"$OUTPUT" --help > "$TDIR/t3.txt" 2>&1
grep -q "Usage" "$TDIR/t3.txt"
check $? "--help mentions Usage"

echo ""
echo "--- Test 4: --version contains v1.0.0 ---"
./"$OUTPUT" --version > "$TDIR/t4.txt" 2>&1
grep -q "v1.0.0" "$TDIR/t4.txt"
check $? "--version contains v1.0.0"

echo ""
echo "--- Test 5: Output has 'top -' header line ---"
grep -q "^top -" "$TDIR/t1.txt"
check $? "output has 'top -' header line"

echo ""
echo "--- Test 6: Output has Tasks: line ---"
grep -q "Tasks:" "$TDIR/t1.txt"
check $? "output has Tasks: line"

echo ""
echo "--- Test 7: Output has Cpu line ---"
grep -Eq "Cpu" "$TDIR/t1.txt"
check $? "output has Cpu line"

echo ""
echo "--- Test 8: Output has Mem: line ---"
grep -q "MiB Mem" "$TDIR/t1.txt"
check $? "output has MiB Mem line"

echo ""
echo "--- Test 9: Output has column header PID ---"
grep -q "PID" "$TDIR/t1.txt"
check $? "output has PID column header"

echo ""
echo "--- Test 10: Output has column header COMMAND ---"
grep -q "COMMAND" "$TDIR/t1.txt"
check $? "output has COMMAND column header"

echo ""
echo "--- Test 11: -b -n 2 -d 0.1 runs two iterations ---"
./"$OUTPUT" -b -n 2 -d 0.1 > "$TDIR/t11.txt" 2>&1
check $? "batch mode -n 2 runs"

echo ""
echo "--- Test 12: -o %MEM sorts by memory ---"
./"$OUTPUT" -b -n 1 -d 0.1 -o %MEM > "$TDIR/t12.txt" 2>&1
check $? "-o %MEM runs"

echo ""
echo "--- Test 13: -o %CPU sorts by CPU ---"
./"$OUTPUT" -b -n 1 -d 0.1 -o %CPU > "$TDIR/t13.txt" 2>&1
check $? "-o %CPU runs"

echo ""
echo "--- Test 14: -o TIME+ sorts by time ---"
./"$OUTPUT" -b -n 1 -d 0.1 -o TIME+ > "$TDIR/t14.txt" 2>&1
check $? "-o TIME+ runs"

echo ""
echo "--- Test 15: -o PID sorts by PID ---"
./"$OUTPUT" -b -n 1 -d 0.1 -o PID > "$TDIR/t15.txt" 2>&1
check $? "-o PID runs"

echo ""
echo "--- Test 16: -c toggle runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 -c > "$TDIR/t16.txt" 2>&1
check $? "-c toggle runs"

echo ""
echo "--- Test 17: --batch --iterations=1 runs ---"
./"$OUTPUT" --batch --iterations=1 --delay=0.1 > "$TDIR/t17.txt" 2>&1
check $? "--batch --iterations=1 runs"

echo ""
echo "--- Test 18: -H thread mode runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 -H > "$TDIR/t18.txt" 2>&1
check $? "-H thread mode runs"

echo ""
echo "--- Test 19: -S cumulative mode runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 -S > "$TDIR/t19.txt" 2>&1
check $? "-S cumulative mode runs"

echo ""
echo "--- Test 20: -1 single CPU toggle runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 -1 > "$TDIR/t20.txt" 2>&1
check $? "-1 single CPU toggle runs"

echo ""
echo "--- Test 21: -d with float delay works ---"
./"$OUTPUT" -b -n 1 -d 0.5 > "$TDIR/t21.txt" 2>&1
check $? "-d 0.5 float delay works"

echo ""
echo "--- Test 22: -d with invalid argument errors ---"
"$TOP_EXE" -b -d abc > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-d invalid argument errors"

echo ""
echo "--- Test 23: -d without argument errors ---"
"$TOP_EXE" -b -d > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-d without argument errors"

echo ""
echo "--- Test 24: -n without argument errors ---"
"$TOP_EXE" -b -n > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-n without argument errors"

echo ""
echo "--- Test 25: -o without argument errors ---"
"$TOP_EXE" -b -o > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-o without argument errors"

echo ""
echo "--- Test 26: Unknown long option is an error ---"
"$TOP_EXE" --no-such-option > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown long option returns error"

echo ""
echo "--- Test 27: Unknown short option is an error ---"
"$TOP_EXE" -Z > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown short option returns error"

echo ""
echo "--- Test 28: Extra operand is an error ---"
"$TOP_EXE" extra_operand > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "extra operand returns error"

echo ""
echo "--- Test 29: -p with own PID runs ---"
MY_PID=$$
./"$OUTPUT" -b -n 1 -d 0.1 -p "$MY_PID" > "$TDIR/t29.txt" 2>&1
check $? "-p with PID filter runs"

echo ""
echo "--- Test 30: -u with current user runs ---"
CURR_USER=$(id -un 2>/dev/null || echo "root")
./"$OUTPUT" -b -n 1 -d 0.1 -u "$CURR_USER" > "$TDIR/t30.txt" 2>&1
check $? "-u with user filter runs"

echo ""
echo "--- Test 31: Combined -bcH runs ---"
./"$OUTPUT" -bcH -n 1 -d 0.1 > "$TDIR/t31.txt" 2>&1
check $? "-bcH combined runs"

echo ""
echo "--- Test 32: -w width option runs ---"
./"$OUTPUT" -b -n 1 -d 0.1 -w 120 > "$TDIR/t32.txt" 2>&1
check $? "-w 120 width runs"

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
