#!/bin/bash
# Build and test script for mkdir.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    mkdir.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=mkdir
SOURCE=mkdir.c
PASS=0
FAIL=0

# Platform detection
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

# Platform-specific flags
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

TDIR="/tmp/mkdir_ftest_$$"
rm -rf "$TDIR"
mkdir -p "$TDIR"

check() {
    # args: exit_code test_name
    if [ "$1" -eq 0 ]; then
        echo "  [PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

# ========== T01: Basic directory
echo ""
echo "--- T01: Basic directory ---"
./"$OUTPUT" "$TDIR/d1" > /dev/null 2>&1
[ -d "$TDIR/d1" ]
check $? "basic directory"

# ========== T02: Multiple directories
echo ""
echo "--- T02: Multiple directories ---"
./"$OUTPUT" "$TDIR/m1" "$TDIR/m2" "$TDIR/m3" > /dev/null 2>&1
[ -d "$TDIR/m1" ] && [ -d "$TDIR/m2" ] && [ -d "$TDIR/m3" ]
check $? "multiple directories"

# ========== T03: Existing directory errors
echo ""
echo "--- T03: Existing dir errors ---"
./"$OUTPUT" "$TDIR/d1" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "existing dir errors"

# ========== T04: -p parents
echo ""
echo "--- T04: -p parents ---"
./"$OUTPUT" -p "$TDIR/a/b/c/d" > /dev/null 2>&1
[ -d "$TDIR/a/b/c/d" ]
check $? "-p parents"

# ========== T05: -p no error if exists
echo ""
echo "--- T05: -p no error if exists ---"
./"$OUTPUT" -p "$TDIR/a/b/c/d" > /dev/null 2>&1
[ $? -eq 0 ]
check $? "-p no error if exists"

# ========== T06: --parents long
echo ""
echo "--- T06: --parents long ---"
./"$OUTPUT" --parents "$TDIR/x/y/z" > /dev/null 2>&1
[ -d "$TDIR/x/y/z" ]
check $? "--parents long"

# ========== T07: -v verbose
echo ""
echo "--- T07: -v verbose ---"
./"$OUTPUT" -v "$TDIR/vd1" > /dev/null 2>&1
rc=$?
grep -q "created directory" "$TDIR/vd1" 2>/dev/null || true
# Check stderr/stdout for verbose message
./"$OUTPUT" -v "$TDIR/vd2" 2>&1 | grep -q "created directory"
check $? "-v verbose"

# ========== T08: --verbose long
echo ""
echo "--- T08: --verbose long ---"
./"$OUTPUT" --verbose "$TDIR/vd3" 2>&1 | grep -q "created directory"
check $? "--verbose long"

# ========== T09: -pv combined
echo ""
echo "--- T09: -pv combined ---"
out=$(./"$OUTPUT" -pv "$TDIR/pvd1/pvd2" 2>&1)
rc=$?
echo "$out" | grep -q "created directory" && [ -d "$TDIR/pvd1/pvd2" ]
check $? "-pv combined"

# ========== T10: -m 755 mode
echo ""
echo "--- T10: -m 755 ---"
./"$OUTPUT" -m 755 "$TDIR/m755" > /dev/null 2>&1
[ -d "$TDIR/m755" ]
check $? "-m 755"

# ========== T11: -m 700 mode
echo ""
echo "--- T11: -m 700 ---"
./"$OUTPUT" -m 700 "$TDIR/m700" > /dev/null 2>&1
[ -d "$TDIR/m700" ]
check $? "-m 700"

# ========== T12: -m 777 mode
echo ""
echo "--- T12: -m 777 ---"
./"$OUTPUT" -m 777 "$TDIR/m777" > /dev/null 2>&1
[ -d "$TDIR/m777" ]
check $? "-m 777"

# ========== T13: --mode=755 long
echo ""
echo "--- T13: --mode=755 ---"
./"$OUTPUT" --mode=755 "$TDIR/ml755" > /dev/null 2>&1
[ -d "$TDIR/ml755" ]
check $? "--mode=755"

# ========== T14: --mode 755 separate
echo ""
echo "--- T14: --mode 755 separate ---"
./"$OUTPUT" --mode 755 "$TDIR/mls755" > /dev/null 2>&1
[ -d "$TDIR/mls755" ]
check $? "--mode 755 separate"

# ========== T15: -m with -p
echo ""
echo "--- T15: -m with -p ---"
./"$OUTPUT" -p -m 700 "$TDIR/mp/a/b" > /dev/null 2>&1
[ -d "$TDIR/mp/a/b" ]
check $? "-m with -p"

# ========== T16: -m with -v
echo ""
echo "--- T16: -m with -v ---"
./"$OUTPUT" -m 755 -v "$TDIR/mv1" 2>&1 | grep -q "created directory"
rc=$?
[ -d "$TDIR/mv1" ] && [ $rc -eq 0 ]
check $? "-m with -v"

# ========== T17: -p -v -m combined
echo ""
echo "--- T17: -p -v -m combined ---"
out=$(./"$OUTPUT" -p -v -m 755 "$TDIR/pvm/a/b/c" 2>&1)
echo "$out" | grep -q "created directory" && [ -d "$TDIR/pvm/a/b/c" ]
check $? "-p -v -m combined"

# ========== T18: File exists as file
echo ""
echo "--- T18: File exists as file ---"
echo "data" > "$TDIR/file1.txt"
./"$OUTPUT" "$TDIR/file1.txt" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "file exists as file"

# ========== T19: -p file exists as file
echo ""
echo "--- T19: -p file exists as file ---"
echo "data" > "$TDIR/file2.txt"
./"$OUTPUT" -p "$TDIR/file2.txt" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "-p file exists as file"

# ========== T20: --version
echo ""
echo "--- T20: --version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "--version"

# ========== T21: --help
echo ""
echo "--- T21: --help ---"
./"$OUTPUT" --help 2>&1 | grep -q "Usage:"
check $? "--help"

# ========== T22: -h help
echo ""
echo "--- T22: -h help ---"
./"$OUTPUT" -h 2>&1 | grep -q "Usage:"
check $? "-h help"

# ========== T23: Missing operand
echo ""
echo "--- T23: Missing operand ---"
./"$OUTPUT" 2>/dev/null
[ $? -ne 0 ]
check $? "missing operand"

# ========== T24: Invalid option
echo ""
echo "--- T24: Invalid option ---"
./"$OUTPUT" -Q "$TDIR/x" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid option"

# ========== T25: -- separator
echo ""
echo "--- T25: -- separator ---"
./"$OUTPUT" -- "$TDIR/dashdir" > /dev/null 2>&1
[ -d "$TDIR/dashdir" ]
check $? "-- separator"

# ========== T26: -Z SELinux (ignored)
echo ""
echo "--- T26: -Z SELinux ignored ---"
./"$OUTPUT" -Z "$TDIR/zdir" > /dev/null 2>&1
[ -d "$TDIR/zdir" ]
check $? "-Z SELinux ignored"

# ========== T27: Parent missing without -p
echo ""
echo "--- T27: Parent missing without -p ---"
./"$OUTPUT" "$TDIR/noexist/sub" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "parent missing without -p"

# ========== T28: Deep nested -p
echo ""
echo "--- T28: Deep nested -p ---"
./"$OUTPUT" -p "$TDIR/deep/d1/d2/d3/d4/d5" > /dev/null 2>&1
[ -d "$TDIR/deep/d1/d2/d3/d4/d5" ]
check $? "deep nested -p"

# ========== T29: -p -v verbose each component
echo ""
echo "--- T29: -p -v each component ---"
out=$(./"$OUTPUT" -p -v "$TDIR/vpc/vpb" 2>&1)
cnt=$(echo "$out" | grep -c "created directory")
[ "$cnt" -ge 2 ]
check $? "-p -v each component"

# ========== T30: -m invalid mode
echo ""
echo "--- T30: -m invalid mode ---"
./"$OUTPUT" -m abc "$TDIR/mdir" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "-m invalid mode"

# ========== T31: -m 0755 (leading zero)
echo ""
echo "--- T31: -m 0755 leading zero ---"
./"$OUTPUT" -m 0755 "$TDIR/m0755" > /dev/null 2>&1
[ -d "$TDIR/m0755" ]
check $? "-m 0755 leading zero"

# ========== T32: -m requires argument
echo ""
echo "--- T32: -m requires argument ---"
./"$OUTPUT" -m > /dev/null 2>&1
[ $? -ne 0 ]
check $? "-m requires argument"

# ========== T33: --mode requires argument
echo ""
echo "--- T33: --mode requires argument ---"
./"$OUTPUT" --mode > /dev/null 2>&1
[ $? -ne 0 ]
check $? "--mode requires argument"

# ========== T34: Multiple -p dirs
echo ""
echo "--- T34: Multiple -p dirs ---"
./"$OUTPUT" -p "$TDIR/mp1/a/b" "$TDIR/mp2/c/d" > /dev/null 2>&1
[ -d "$TDIR/mp1/a/b" ] && [ -d "$TDIR/mp2/c/d" ]
check $? "multiple -p dirs"

# ========== T35: -p with trailing slash
echo ""
echo "--- T35: -p trailing slash ---"
./"$OUTPUT" -p "$TDIR/tsdir/" > /dev/null 2>&1
[ -d "$TDIR/tsdir" ]
check $? "-p trailing slash"

# ========== T36: -m symbolic a+rwx
echo ""
echo "--- T36: -m a+rwx ---"
./"$OUTPUT" -m a+rwx "$TDIR/marwx" > /dev/null 2>&1
[ -d "$TDIR/marwx" ]
check $? "-m a+rwx"

# ========== T37: -m symbolic u=rwx,go=rx
echo ""
echo "--- T37: -m u=rwx,go=rx ---"
./"$OUTPUT" -m u=rwx,go=rx "$TDIR/msym" > /dev/null 2>&1
[ -d "$TDIR/msym" ]
check $? "-m u=rwx,go=rx"

# ========== T38: -m mode out of range
echo ""
echo "--- T38: -m mode out of range ---"
./"$OUTPUT" -m 9999 "$TDIR/mout" > /dev/null 2>&1
[ $? -ne 0 ]
check $? "-m mode out of range"

# ========== T39: Verify -m 755 actual permissions (POSIX)
echo ""
echo "--- T39: -m 755 actual permissions ---"
./"$OUTPUT" -m 755 "$TDIR/pm755" > /dev/null 2>&1
pm=$(stat -c "%a" "$TDIR/pm755" 2>/dev/null || stat -f "%Lp" "$TDIR/pm755" 2>/dev/null)
[ "$pm" = "755" ]
check $? "-m 755 permissions"

# ========== T40: Verify -m 700 actual permissions (POSIX)
echo ""
echo "--- T40: -m 700 actual permissions ---"
./"$OUTPUT" -m 700 "$TDIR/pm700" > /dev/null 2>&1
pm=$(stat -c "%a" "$TDIR/pm700" 2>/dev/null || stat -f "%Lp" "$TDIR/pm700" 2>/dev/null)
[ "$pm" = "700" ]
check $? "-m 700 permissions"

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
