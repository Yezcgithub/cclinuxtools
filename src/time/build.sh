#!/bin/bash
# Build and test script for time.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    time.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=time
SOURCE=time.c
PASS=0
FAIL=0

PLATFORM="unknown"
if [ "$(uname -s)" = "Linux" ]; then PLATFORM="linux"
elif [ "$(uname -s)" = "Darwin" ]; then PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then PLATFORM="netbsd"; fi

# Platform-specific POSIX macro
POSIX_MACRO=""
if [ "$PLATFORM" = "linux" ]; then POSIX_MACRO="-D_POSIX_C_SOURCE=200809L"
elif [ "$PLATFORM" = "macos" ]; then POSIX_MACRO="-D_DARWIN_C_SOURCE"
elif [ "$PLATFORM" = "netbsd" ]; then POSIX_MACRO="-D_NETBSD_SOURCE"; fi

echo ""; echo "Detected platform: $PLATFORM"
echo ""; echo "[1/3] Cleaning previous build..."; rm -f "$OUTPUT"
echo ""; echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $POSIX_MACRO -o $OUTPUT $SOURCE"; echo ""
$CC $CFLAGS $POSIX_MACRO -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then echo ""; echo "[ERROR] Build failed"; exit 1; fi
echo ""; echo "[3/3] Build succeeded!"; echo "  Output: $(pwd)/$OUTPUT"
echo ""; echo "============================================"
echo "  Running functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then echo "  [PASS] $2"; PASS=$((PASS + 1))
    else echo "  [FAIL] $2"; FAIL=$((FAIL + 1)); fi
}

# ========== T01: --help exits 0
echo ""; echo "--- T01: --help exits 0 ---"
./"$OUTPUT" --help >/dev/null 2>&1
check $? "--help exits 0"

# ========== T02: --version exits 0
echo ""; echo "--- T02: --version exits 0 ---"
./"$OUTPUT" --version >/dev/null 2>&1
check $? "--version exits 0"

# ========== T03: missing command is error
echo ""; echo "--- T03: missing command is error ---"
out=$(./"$OUTPUT" 2>&1)
rc=$?
[ $rc -eq 1 ]; check $? "missing command exits 1 ($rc)"

# ========== T04: default output format contains 'user'
echo ""; echo "--- T04: default output format ---"
out=$(./"$OUTPUT" true 2>&1)
echo "$out" | grep -q 'user'; check $? "default output contains 'user'"
echo "$out" | grep -q 'system'; check $? "default output contains 'system'"
echo "$out" | grep -q 'elapsed'; check $? "default output contains 'elapsed'"

# ========== T05: -p portable format
echo ""; echo "--- T05: -p portable format ---"
out=$(./"$OUTPUT" -p true 2>&1)
echo "$out" | grep -q '^real '; check $? "-p output contains 'real'"
echo "$out" | grep -q '^user '; check $? "-p output contains 'user'"
echo "$out" | grep -q '^sys '; check $? "-p output contains 'sys'"

# ========== T06: exit code propagation
echo ""; echo "--- T06: exit code propagation ---"
./"$OUTPUT" true >/dev/null 2>&1
check $? "time true returns 0"
./"$OUTPUT" sh -c "exit 42" >/dev/null 2>&1
rc=$?
[ $rc -eq 42 ]; check $? "time 'exit 42' returns 42 ($rc)"

# ========== T07: --portability long option
echo ""; echo "--- T07: --portability long option ---"
out1=$(./"$OUTPUT" -p true 2>&1)
out2=$(./"$OUTPUT" --portability true 2>&1)
[ "$out1" = "$out2" ]; check $? "--portability matches -p"

# ========== T08: -f custom format
echo ""; echo "--- T08: -f custom format ---"
out=$(./"$OUTPUT" -f "elapsed=%e user=%U sys=%S" true 2>&1)
echo "$out" | grep -q '^elapsed='; check $? "-f custom format works"
echo "$out" | grep -q 'user='; check $? "-f output contains user="
echo "$out" | grep -q 'sys='; check $? "-f output contains sys="

# ========== T09: --format= long option
echo ""; echo "--- T09: --format= long option ---"
out1=$(./"$OUTPUT" -f "TEST=%e" true 2>&1)
out2=$(./"$OUTPUT" --format="TEST=%e" true 2>&1)
[ "$out1" = "$out2" ]; check $? "--format= matches -f"

# ========== T10: -o output file
echo ""; echo "--- T10: -o output file ---"
./"$OUTPUT" -o /tmp/time_test_out.txt true >/dev/null 2>&1
[ -f /tmp/time_test_out.txt ]; check $? "-o creates output file"
rm -f /tmp/time_test_out.txt

# ========== T11: -o -a append
echo ""; echo "--- T11: -a append mode ---"
./"$OUTPUT" -o /tmp/time_append.txt true >/dev/null 2>&1
./"$OUTPUT" -a -o /tmp/time_append.txt true >/dev/null 2>&1
lines=$(wc -l < /tmp/time_append.txt 2>/dev/null)
[ "$lines" -ge 2 ]; check $? "-a appends to file ($lines lines)"
rm -f /tmp/time_append.txt

# ========== T12: -v verbose output
echo ""; echo "--- T12: -v verbose output ---"
out=$(./"$OUTPUT" -v true 2>&1)
echo "$out" | grep -q 'Command being timed'; check $? "-v contains command info"
echo "$out" | grep -q 'User time'; check $? "-v contains user time"
echo "$out" | grep -q 'System time'; check $? "-v contains system time"
echo "$out" | grep -q 'Exit status'; check $? "-v contains exit status"

# ========== T13: %C format specifier
echo ""; echo "--- T13: %C format specifier ---"
out=$(./"$OUTPUT" -f "%C" echo hello world 2>&1)
echo "$out" | grep -q 'echo hello world'; check $? "%C prints command line"

# ========== T14: %x exit status format
echo ""; echo "--- T14: %x exit status format ---"
out=$(./"$OUTPUT" -f "exit=%x" sh -c "exit 7" 2>&1)
echo "$out" | grep -q 'exit=7'; check $? "%x prints exit code"

# ========== T15: %Z page size
echo ""; echo "--- T15: %Z page size ---"
out=$(./"$OUTPUT" -f "pagesize=%Z" true 2>&1)
echo "$out" | grep -qE 'pagesize=[0-9]+'; check $? "%Z prints page size"

# ========== T16: %P CPU percentage
echo ""; echo "--- T16: %P CPU percentage ---"
out=$(./"$OUTPUT" -f "cpu=%P" true 2>&1)
echo "$out" | grep -q 'cpu=.*%'; check $? "%P prints CPU percentage"

# ========== T17: %E elapsed time format
echo ""; echo "--- T17: %E elapsed time format ---"
out=$(./"$OUTPUT" -f "elapsed=%E" sleep 0.1 2>&1)
echo "$out" | grep -qE 'elapsed=[0-9]+:[0-9]+\.[0-9]+'; check $? "%E prints MM:SS.cc format"

# ========== T18: real time is positive
echo ""; echo "--- T18: real time is positive ---"
out=$(./"$OUTPUT" -p sleep 0.1 2>&1)
real_val=$(echo "$out" | grep '^real ' | awk '{print $2}')
[ -n "$real_val" ] && awk -v v="$real_val" 'BEGIN { exit !(v > 0) }'; check $? "real time > 0 ($real_val)"

# ========== T19: invalid option exits 1
echo ""; echo "--- T19: invalid option exits 1 ---"
./"$OUTPUT" -Q true >/dev/null 2>&1
rc=$?
[ $rc -eq 1 ]; check $? "invalid option -Q exits 1 ($rc)"

# ========== T20: nonexistent command
echo ""; echo "--- T20: nonexistent command ---"
./"$OUTPUT" nosuchcommand_xyz123 >/dev/null 2>&1
rc=$?
[ $rc -eq 127 ]; check $? "nonexistent command returns 127 ($rc)"

echo ""; echo "============================================"
echo "  Test Results: $PASS passed, $FAIL failed"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
