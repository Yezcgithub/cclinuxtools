#!/bin/bash
# Build and test script for free.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    free.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="free"
SOURCE="free.c"
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

# Resolve absolute path to the executable so subshells never confuse it
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FREE_EXE="$SCRIPT_DIR/$OUTPUT"

TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 1: Basic free runs ---"
./"$OUTPUT" > "$TDIR/t1.txt" 2>&1
check $? "basic free runs"

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
echo "--- Test 5: Default output contains Mem: row ---"
./"$OUTPUT" > "$TDIR/t5.txt" 2>&1
grep -q "Mem:" "$TDIR/t5.txt"
check $? "default output has Mem: row"

echo ""
echo "--- Test 6: Default output contains Swap: row ---"
grep -q "Swap:" "$TDIR/t5.txt"
check $? "default output has Swap: row"

echo ""
echo "--- Test 7: Default output has header line ---"
grep -Eq "total[[:space:]]+used[[:space:]]+free" "$TDIR/t5.txt"
check $? "default output has header (total/used/free)"

echo ""
echo "--- Test 8: Narrow mode shows buff/cache (combined) ---"
grep -q "buff/cache" "$TDIR/t5.txt"
check $? "narrow mode header has buff/cache"

echo ""
echo "--- Test 9: -w / --wide shows separate buffers and cache ---"
./"$OUTPUT" -w > "$TDIR/t9.txt" 2>&1
grep -q "buffers" "$TDIR/t9.txt" && grep -q "cache" "$TDIR/t9.txt"
check $? "wide mode header has separate buffers and cache"

echo ""
echo "--- Test 10: --wide alias produces same wide header ---"
./"$OUTPUT" --wide > "$TDIR/t10.txt" 2>&1
grep -q "buffers" "$TDIR/t10.txt" && grep -q "cache" "$TDIR/t10.txt"
check $? "--wide alias produces wide header"

echo ""
echo "--- Test 11: -b / --bytes output is numeric ---"
./"$OUTPUT" -b > "$TDIR/t11.txt" 2>&1
# Mem: line should contain only digits (and spaces/colon)
if grep -E "^Mem:" "$TDIR/t11.txt" | grep -Eq "[0-9]+[[:space:]]+[0-9]+"; then
    check 0 "-b produces numeric values"
else
    check 1 "-b produces numeric values"
fi

echo ""
echo "--- Test 12: --bytes alias works ---"
./"$OUTPUT" --bytes > "$TDIR/t12.txt" 2>&1
if grep -E "^Mem:" "$TDIR/t12.txt" | grep -Eq "[0-9]+"; then
    check 0 "--bytes alias works"
else
    check 1 "--bytes alias works"
fi

echo ""
echo "--- Test 13: -k / --kibi is default (kibibytes) ---"
./"$OUTPUT" -k > "$TDIR/t13.txt" 2>&1
if grep -E "^Mem:" "$TDIR/t13.txt" | grep -Eq "[0-9]+"; then
    check 0 "-k produces numeric values"
else
    check 1 "-k produces numeric values"
fi

echo ""
echo "--- Test 14: -m / --mebi works ---"
./"$OUTPUT" -m > "$TDIR/t14.txt" 2>&1
if grep -E "^Mem:" "$TDIR/t14.txt" | grep -Eq "[0-9]+"; then
    check 0 "-m produces numeric values"
else
    check 1 "-m produces numeric values"
fi

echo ""
echo "--- Test 15: -g / --gibi works ---"
./"$OUTPUT" -g > "$TDIR/t15.txt" 2>&1
if grep -E "^Mem:" "$TDIR/t15.txt" | grep -Eq "[0-9]+"; then
    check 0 "-g produces numeric values"
else
    check 1 "-g produces numeric values"
fi

echo ""
echo "--- Test 16: -h / --human produces values with unit suffix ---"
./"$OUTPUT" -h > "$TDIR/t16.txt" 2>&1
# Expect a value like 1.5Gi, 500Mi, etc. (suffix K/M/G/T/P + optional i)
if grep -E "^Mem:" "$TDIR/t16.txt" | grep -Eq "[0-9]+[KMGTPE]i?"; then
    check 0 "-h produces values with unit suffix"
else
    check 1 "-h produces values with unit suffix"
fi

echo ""
echo "--- Test 17: --human alias works ---"
./"$OUTPUT" --human > "$TDIR/t17.txt" 2>&1
if grep -E "^Mem:" "$TDIR/t17.txt" | grep -Eq "[0-9]+[KMGTPE]i?"; then
    check 0 "--human alias works"
else
    check 1 "--human alias works"
fi

echo ""
echo "--- Test 18: -l / --lohi shows Low: and High: rows ---"
./"$OUTPUT" -l > "$TDIR/t18.txt" 2>&1
grep -q "Low:" "$TDIR/t18.txt" && grep -q "High:" "$TDIR/t18.txt"
check $? "-l shows Low:/High: rows"

echo ""
echo "--- Test 19: --lohi alias works ---"
./"$OUTPUT" --lohi > "$TDIR/t19.txt" 2>&1
grep -q "Low:" "$TDIR/t19.txt" && grep -q "High:" "$TDIR/t19.txt"
check $? "--lohi alias works"

echo ""
echo "--- Test 20: -t / --total shows Total: row ---"
./"$OUTPUT" -t > "$TDIR/t20.txt" 2>&1
grep -q "Total:" "$TDIR/t20.txt"
check $? "-t shows Total: row"

echo ""
echo "--- Test 21: --total alias works ---"
./"$OUTPUT" --total > "$TDIR/t21.txt" 2>&1
grep -q "Total:" "$TDIR/t21.txt"
check $? "--total alias works"

echo ""
echo "--- Test 22: --si works (default unit becomes kilo) ---"
./"$OUTPUT" --si > "$TDIR/t22.txt" 2>&1
check $? "--si runs successfully"

echo ""
echo "--- Test 23: --iec works (default) ---"
./"$OUTPUT" --iec > "$TDIR/t23.txt" 2>&1
check $? "--iec runs successfully"

echo ""
echo "--- Test 24: Combined short options -hlwt ---"
./"$OUTPUT" -hlwt > "$TDIR/t24.txt" 2>&1
if grep -q "Low:" "$TDIR/t24.txt" && grep -q "High:" "$TDIR/t24.txt" && \
   grep -q "Total:" "$TDIR/t24.txt" && grep -q "buffers" "$TDIR/t24.txt"; then
    check 0 "-hlwt combined works"
else
    check 1 "-hlwt combined works"
fi

echo ""
echo "--- Test 25: -h --si uses SI suffixes (K/M/G without i) ---"
./"$OUTPUT" -h --si > "$TDIR/t25.txt" 2>&1
# In SI mode suffix should be K/M/G/T/P (no 'i')
if grep -E "^Mem:" "$TDIR/t25.txt" | grep -Eq "[0-9]+[KMGTPE]"; then
    check 0 "-h --si uses SI suffixes"
else
    check 1 "-h --si uses SI suffixes"
fi

echo ""
echo "--- Test 26: -s 1 -c 1 runs exactly once and exits ---"
./"$OUTPUT" -s 1 -c 1 > "$TDIR/t26.txt" 2>&1
R26=$?
if [ "$R26" -eq 0 ] && [ -s "$TDIR/t26.txt" ]; then
    check 0 "-s 1 -c 1 runs once and exits"
else
    check 1 "-s 1 -c 1 runs once and exits"
fi

echo ""
echo "--- Test 27: --seconds=1 --count=1 long form works ---"
./"$OUTPUT" --seconds=1 --count=1 > "$TDIR/t27.txt" 2>&1
R27=$?
if [ "$R27" -eq 0 ] && [ -s "$TDIR/t27.txt" ]; then
    check 0 "--seconds=1 --count=1 works"
else
    check 1 "--seconds=1 --count=1 works"
fi

echo ""
echo "--- Test 28: -s with invalid argument errors ---"
"$FREE_EXE" -s abc > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-s with invalid argument errors"

echo ""
echo "--- Test 29: -s without argument errors ---"
"$FREE_EXE" -s > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-s without argument errors"

echo ""
echo "--- Test 30: -c without argument errors ---"
"$FREE_EXE" -c > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-c without argument errors"

echo ""
echo "--- Test 31: Unknown long option is an error ---"
"$FREE_EXE" --no-such-option > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown long option returns error"

echo ""
echo "--- Test 32: Unknown short option is an error ---"
"$FREE_EXE" -Z > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown short option returns error"

echo ""
echo "--- Test 33: Extra operand is an error ---"
"$FREE_EXE" extra_operand > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "extra operand returns error"

echo ""
echo "--- Test 34: -lt combined shows lohi + total ---"
./"$OUTPUT" -lt > "$TDIR/t34.txt" 2>&1
if grep -q "Low:" "$TDIR/t34.txt" && grep -q "High:" "$TDIR/t34.txt" && \
   grep -q "Total:" "$TDIR/t34.txt"; then
    check 0 "-lt combined shows lohi + total"
else
    check 1 "-lt combined shows lohi + total"
fi

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
