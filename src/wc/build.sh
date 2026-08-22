#!/bin/bash
# Build and test script for wc.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    wc.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="wc"
SOURCE="wc.c"
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
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE" 2>&1

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed!"
    exit 1
fi

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"
echo ""
echo "============================================"
echo "  Running basic tests..."
echo "============================================"

# Setup test directory
TDIR=_build_test
rm -rf "$TDIR"
mkdir -p "$TDIR"

# Helper: check command exit code
test_exit_zero() {
    local name="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"
        FAIL=$((FAIL + 1))
    fi
}

# Helper: check output contains pattern
test_contains() {
    local name="$1"
    local pattern="$2"
    shift 2
    local result
    result=$("$@" 2>&1)
    if echo "$result" | grep -q "$pattern"; then
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"
        echo "        expected pattern: '$pattern'"
        echo "        got: '$result'"
        FAIL=$((FAIL + 1))
    fi
}

# Helper: check exit code is non-zero
test_exit_nonzero() {
    local name="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        echo "  [FAIL] $name (expected non-zero exit)"
        FAIL=$((FAIL + 1))
    else
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    fi
}

# Helper: check field value in command output
# Usage: test_field "name" field_num expected command...
test_field() {
    local name="$1"
    local field_num="$2"
    local expected="$3"
    shift 3
    local result
    result=$("$@" 2>&1 | awk -v f="$field_num" '{print $f}')
    if [ "$result" = "$expected" ]; then
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"
        echo "        expected field $field_num: '$expected'"
        echo "        got: '$result'"
        FAIL=$((FAIL + 1))
    fi
}

# Create test files
printf "hello world\n" > "$TDIR/f1.txt"
printf "line1\nline2\nline3\n" > "$TDIR/f2.txt"
printf "" > "$TDIR/empty.txt"
printf "no newline" > "$TDIR/nonl.txt"
printf "a b c d e\n" > "$TDIR/f5.txt"
printf "\xc3\xa9\xc3\xa8\xc3\xaa\n" > "$TDIR/utf8.txt"
printf "aaaaaaaaaaaaaaaaaaaaaaaa\n" > "$TDIR/longline.txt"

echo ""
echo "--- Test 1: Basic stdin run ---"
test_exit_zero "basic stdin runs" sh -c "echo hello | ./$OUTPUT"

echo ""
echo "--- Test 2: --help mentions Usage ---"
test_contains "--help has Usage" "Usage" ./$OUTPUT --help

echo ""
echo "--- Test 3: --version contains 1.0.0 ---"
test_contains "--version has 1.0.0" "1.0.0" ./$OUTPUT --version

echo ""
echo "--- Test 4: -l counts 1 line ---"
test_field "-l on f1.txt" 1 "1" ./$OUTPUT -l "$TDIR/f1.txt"

echo ""
echo "--- Test 5: -l counts 3 lines ---"
test_field "-l on f2.txt is 3" 1 "3" ./$OUTPUT -l "$TDIR/f2.txt"

echo ""
echo "--- Test 6: -w counts 2 words ---"
test_field "-w on f1.txt is 2" 1 "2" ./$OUTPUT -w "$TDIR/f1.txt"

echo ""
echo "--- Test 7: -w counts 5 words ---"
test_field "-w on f5.txt is 5" 1 "5" ./$OUTPUT -w "$TDIR/f5.txt"

echo ""
echo "--- Test 8: -c counts 12 bytes ---"
test_field "-c on f1.txt is 12" 1 "12" ./$OUTPUT -c "$TDIR/f1.txt"

echo ""
echo "--- Test 9: -c on empty file is 0 ---"
test_field "-c on empty.txt is 0" 1 "0" ./$OUTPUT -c "$TDIR/empty.txt"

echo ""
echo "--- Test 10: -c on no-newline file ---"
test_field "-c on nonl.txt is 10" 1 "10" ./$OUTPUT -c "$TDIR/nonl.txt"

echo ""
echo "--- Test 11: -L max line length ---"
test_field "-L on longline.txt is 24" 1 "24" ./$OUTPUT -L "$TDIR/longline.txt"

echo ""
echo "--- Test 12: -L on f2.txt ---"
test_field "-L on f2.txt is 5" 1 "5" ./$OUTPUT -L "$TDIR/f2.txt"

echo ""
echo "--- Test 13: Default mode (no option) has 4 columns ---"
COLS=$(./$OUTPUT "$TDIR/f1.txt" | awk '{print NF}')
if [ "$COLS" = "4" ]; then
    echo "  [PASS] default mode 4 columns"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] default mode 4 columns (got $COLS)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 14: Default mode lines=1 ---"
test_field "default f1.txt lines=1" 1 "1" ./$OUTPUT "$TDIR/f1.txt"

echo ""
echo "--- Test 15: Default mode words=2 ---"
test_field "default f1.txt words=2" 2 "2" ./$OUTPUT "$TDIR/f1.txt"

echo ""
echo "--- Test 16: Default mode bytes=12 ---"
test_field "default f1.txt bytes=12" 3 "12" ./$OUTPUT "$TDIR/f1.txt"

echo ""
echo "--- Test 17: Multiple files show total ---"
test_contains "multiple files has total" "total" ./$OUTPUT "$TDIR/f1.txt" "$TDIR/f2.txt"

echo ""
echo "--- Test 18: Combined -lw ---"
COLS=$(./$OUTPUT -lw "$TDIR/f1.txt" | awk '{print NF}')
if [ "$COLS" = "3" ]; then
    echo "  [PASS] -lw shows 3 columns"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -lw shows 3 columns (got $COLS)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 19: Combined -lcw ---"
COLS=$(./$OUTPUT -lcw "$TDIR/f1.txt" | awk '{print NF}')
if [ "$COLS" = "4" ]; then
    echo "  [PASS] -lcw shows 4 columns"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -lcw shows 4 columns (got $COLS)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 20: -clmwL all options ---"
COLS=$(./$OUTPUT -clmwL "$TDIR/f1.txt" | awk '{print NF}')
if [ "$COLS" = "6" ]; then
    echo "  [PASS] -clmwL shows 6 columns"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -clmwL shows 6 columns (got $COLS)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 21: --lines long option ---"
test_field "--lines on f2.txt is 3" 1 "3" ./$OUTPUT --lines "$TDIR/f2.txt"

echo ""
echo "--- Test 22: --words long option ---"
test_field "--words on f5.txt is 5" 1 "5" ./$OUTPUT --words "$TDIR/f5.txt"

echo ""
echo "--- Test 23: --bytes long option ---"
test_field "--bytes on f1.txt is 12" 1 "12" ./$OUTPUT --bytes "$TDIR/f1.txt"

echo ""
echo "--- Test 24: --chars on ASCII equals bytes ---"
BYTES=$(./$OUTPUT -c "$TDIR/f1.txt" | awk '{print $1}')
CHARS=$(./$OUTPUT -m "$TDIR/f1.txt" | awk '{print $1}')
if [ "$BYTES" = "$CHARS" ]; then
    echo "  [PASS] -m equals -c on ASCII"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -m ($CHARS) != -c ($BYTES) on ASCII"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 25: --max-line-length long option ---"
test_field "--max-line-length on longline.txt is 24" 1 "24" ./$OUTPUT --max-line-length "$TDIR/longline.txt"

echo ""
echo "--- Test 26: - reads from stdin ---"
RESULT=$(echo "test stdin" | ./$OUTPUT -c - | awk '{print $1}')
EXPECTED="11"
if [ "$RESULT" = "$EXPECTED" ]; then
    echo "  [PASS] - reads from stdin"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] - reads from stdin (expected $EXPECTED, got $RESULT)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 27: -m on UTF-8 multibyte ---"
RESULT=$(./$OUTPUT -m "$TDIR/utf8.txt" | awk '{print $1}')
EXPECTED="4"
if [ "$RESULT" = "$EXPECTED" ]; then
    echo "  [PASS] -m counts multibyte chars correctly"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -m multibyte (expected $EXPECTED, got $RESULT)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 28: -c on UTF-8 equals byte count ---"
RESULT=$(./$OUTPUT -c "$TDIR/utf8.txt" | awk '{print $1}')
EXPECTED="7"
if [ "$RESULT" = "$EXPECTED" ]; then
    echo "  [PASS] -c counts UTF-8 bytes correctly"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] -c UTF-8 (expected $EXPECTED, got $RESULT)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 29: --files0-from works ---"
printf "%s\0%s\0" "$TDIR/f1.txt" "$TDIR/f2.txt" > "$TDIR/files0.txt"
test_contains "--files0-from has total" "total" ./$OUTPUT --files0-from="$TDIR/files0.txt"

echo ""
echo "--- Test 30: --files0-from counts ---"
RESULT=$(./$OUTPUT --files0-from="$TDIR/files0.txt" | grep total | awk '{print $1}')
EXPECTED="4"
if [ "$RESULT" = "$EXPECTED" ]; then
    echo "  [PASS] --files0-from total lines correct"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] --files0-from total lines (expected $EXPECTED, got $RESULT)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- Test 31: Unknown long option is error ---"
test_exit_nonzero "unknown long option errors" ./$OUTPUT --unknown

echo ""
echo "--- Test 32: Unknown short option is error ---"
test_exit_nonzero "unknown short option errors" ./$OUTPUT -Z

echo ""
echo "--- Test 33: Empty file lines is 0 ---"
test_field "-l on empty.txt is 0" 1 "0" ./$OUTPUT -l "$TDIR/empty.txt"

echo ""
echo "--- Test 34: Multiple files total lines ---"
RESULT=$(./$OUTPUT -l "$TDIR/f1.txt" "$TDIR/f2.txt" "$TDIR/f5.txt" | grep total | awk '{print $1}')
EXPECTED="5"
if [ "$RESULT" = "$EXPECTED" ]; then
    echo "  [PASS] total lines = 5"
    PASS=$((PASS + 1))
else
    echo "  [FAIL] total lines (expected $EXPECTED, got $RESULT)"
    FAIL=$((FAIL + 1))
fi

# Cleanup
rm -rf "$TDIR"

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
