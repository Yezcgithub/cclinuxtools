#!/bin/bash
# Build and test script for touch.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    touch.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=touch
SOURCE=touch.c
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

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR_TEST="/tmp/touch_test_$$"
mkdir -p "$TMPDIR_TEST"
cd "$TMPDIR_TEST"

# Helper: check exit code
check_exit() {
    if [ "$1" -eq 0 ]; then
        PASS=$((PASS + 1)); echo "  [PASS]"
    else
        FAIL=$((FAIL + 1)); echo "  [FAIL]"
    fi
}

# Helper: check file exists
check_exists() {
    if [ -f "$1" ]; then
        PASS=$((PASS + 1)); echo "  [PASS]"
    else
        FAIL=$((FAIL + 1)); echo "  [FAIL]"
    fi
}

# Helper: check file does NOT exist
check_not_exists() {
    if [ ! -f "$1" ]; then
        PASS=$((PASS + 1)); echo "  [PASS]"
    else
        FAIL=$((FAIL + 1)); echo "  [FAIL]"
    fi
}

# ========== T01: Basic create
echo ""
echo "--- T01: Basic create ---"
"$SCRIPT_DIR/$OUTPUT" test1.txt
check_exists test1.txt

# ========== T02: -c no create
echo ""
echo "--- T02: -c no create ---"
"$SCRIPT_DIR/$OUTPUT" -c test2_nonexist.txt
check_not_exists test2_nonexist.txt

# ========== T03: -t stamp
echo ""
echo "--- T03: -t 202401011200.00 ---"
"$SCRIPT_DIR/$OUTPUT" -t 202401011200.00 test3.txt
check_exists test3.txt

# ========== T04: -d date
echo ""
echo "--- T04: -d '2024-06-15 10:30:00' ---"
"$SCRIPT_DIR/$OUTPUT" -d "2024-06-15 10:30:00" test4.txt
check_exists test4.txt

# ========== T05: -r reference
echo ""
echo "--- T05: -r test3.txt ---"
"$SCRIPT_DIR/$OUTPUT" -r test3.txt test5.txt
check_exists test5.txt

# ========== T06: --help
echo ""
echo "--- T06: --help ---"
"$SCRIPT_DIR/$OUTPUT" --help > /dev/null 2>&1
check_exit $?

# ========== T07: --version
echo ""
echo "--- T07: --version ---"
"$SCRIPT_DIR/$OUTPUT" --version > /tmp/touch_out_$$ 2>/dev/null
if grep -q "9.11" /tmp/touch_out_$$; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T08: -a access only
echo ""
echo "--- T08: -a access only ---"
"$SCRIPT_DIR/$OUTPUT" -a test1.txt
check_exit $?

# ========== T09: -m modify only
echo ""
echo "--- T09: -m modify only ---"
"$SCRIPT_DIR/$OUTPUT" -m test1.txt
check_exit $?

# ========== T10: multiple files
echo ""
echo "--- T10: multiple files ---"
"$SCRIPT_DIR/$OUTPUT" test7.txt test8.txt test9.txt
if [ -f test7.txt ] && [ -f test8.txt ] && [ -f test9.txt ]; then
    PASS=$((PASS + 1)); echo "  [PASS]"
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# ========== T11: --time=atime
echo ""
echo "--- T11: --time=atime ---"
"$SCRIPT_DIR/$OUTPUT" --time=atime test1.txt
check_exit $?

# ========== T12: --time=mtime
echo ""
echo "--- T12: --time=mtime ---"
"$SCRIPT_DIR/$OUTPUT" --time=mtime test1.txt
check_exit $?

# ========== T13: -d @epoch
echo ""
echo "--- T13: -d @epoch ---"
"$SCRIPT_DIR/$OUTPUT" -d "@1700000000" test10.txt
check_exists test10.txt

# ========== T14: -d now
echo ""
echo "--- T14: -d now ---"
"$SCRIPT_DIR/$OUTPUT" -d now test11.txt
check_exists test11.txt

# ========== T15: -d today
echo ""
echo "--- T15: -d today ---"
"$SCRIPT_DIR/$OUTPUT" -d today test12.txt
check_exists test12.txt

# ========== T16: -d yesterday
echo ""
echo "--- T16: -d yesterday ---"
"$SCRIPT_DIR/$OUTPUT" -d yesterday test13.txt
check_exists test13.txt

# ========== T17: -d tomorrow
echo ""
echo "--- T17: -d tomorrow ---"
"$SCRIPT_DIR/$OUTPUT" -d tomorrow test14.txt
check_exists test14.txt

# ========== T18: -- delimiter
echo ""
echo "--- T18: -- delimiter ---"
"$SCRIPT_DIR/$OUTPUT" -- test15.txt
check_exists test15.txt

# ========== T19: -d ISO date only
echo ""
echo "--- T19: -d '2024-03-15' ---"
"$SCRIPT_DIR/$OUTPUT" -d "2024-03-15" test16.txt
check_exists test16.txt

# ========== T20: -t short stamp (MMDDhhmm)
echo ""
echo "--- T20: -t 01011200 ---"
"$SCRIPT_DIR/$OUTPUT" -t 01011200 test17.txt
check_exists test17.txt

# ========== T21: -d "HH:MM:SS"
echo ""
echo "--- T21: -d '10:30:00' ---"
"$SCRIPT_DIR/$OUTPUT" -d "10:30:00" test18.txt
check_exists test18.txt

# ========== T22: -am both
echo ""
echo "--- T22: -am both ---"
"$SCRIPT_DIR/$OUTPUT" -am test19.txt
check_exists test19.txt

# ========== T23: -f ignored
echo ""
echo "--- T23: -f ignored ---"
"$SCRIPT_DIR/$OUTPUT" -f test20.txt
check_exists test20.txt

# ========== T24: -t 2-digit year (YYMMDDhhmm)
echo ""
echo "--- T24: -t 2401011200 ---"
"$SCRIPT_DIR/$OUTPUT" -t 2401011200 test21.txt
check_exists test21.txt

# ========== T25: verify -t sets correct time
echo ""
echo "--- T25: verify -t time ---"
"$SCRIPT_DIR/$OUTPUT" -t 202401011200.00 test22.txt
if [ -f test22.txt ]; then
    # Check the file's modify time is close to Jan 1 2024 12:00
    MTIME=$(stat -c %Y test22.txt 2>/dev/null || stat -f %m test22.txt 2>/dev/null || echo "0")
    if [ "$MTIME" -ge 1704067200 ] && [ "$MTIME" -le 1704110400 ]; then
        PASS=$((PASS + 1)); echo "  [PASS]"
    else
        FAIL=$((FAIL + 1)); echo "  [FAIL] (mtime=$MTIME)"
    fi
else
    FAIL=$((FAIL + 1)); echo "  [FAIL]"
fi

# Cleanup
rm -f /tmp/touch_out_$$
cd /
rm -rf "$TMPDIR_TEST"

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
