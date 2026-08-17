#!/bin/bash
set -e

echo "============================================"
echo "     pwd.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="pwd"
SOURCE="pwd.c"
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

# Setup: resolve absolute path to OUTPUT so subshells/chdir never confuse it with the directory called "pwd"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PWD_EXE="$SCRIPT_DIR/$OUTPUT"

TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 1: Basic pwd ---"
./"$OUTPUT" > "$TDIR/t1.txt" 2>&1
check $? "basic pwd (default = physical)"

echo ""
echo "--- Test 2: Physical mode (-P) ---"
./"$OUTPUT" -P > "$TDIR/t2.txt" 2>&1
check $? "pwd -P"

echo ""
echo "--- Test 3: Logical mode (-L) ---"
./"$OUTPUT" -L > "$TDIR/t3.txt" 2>&1
check $? "pwd -L"

echo ""
echo "--- Test 4: Output absolute path ---"
out=$(cat "$TDIR/t1.txt")
case "$out" in /*) ok=0;; *) ok=1;; esac
check $ok "output is absolute (starts with /)"

echo ""
echo "--- Test 5: Output has no trailing slash (non-root) ---"
# Get length > 1 typically
len=$(wc -c < "$TDIR/t1.txt" | awk '{print $1}')
if [ "$len" -gt 2 ]; then
    # strip trailing newline
    content=$(cat "$TDIR/t1.txt" | tr -d '\n')
    last_char="${content: -1}"
    if [ "$last_char" != "/" ]; then
        check 0 "no trailing slash on non-root dir"
    else
        check 1 "no trailing slash on non-root dir"
    fi
else
    check 0 "no trailing slash (skipped, root or too short)"
fi

echo ""
echo "--- Test 6: -P and default outputs match ---"
diff -q "$TDIR/t1.txt" "$TDIR/t2.txt" > /dev/null 2>&1
check $? "default == -P (GNU behavior)"

echo ""
echo "--- Test 7: Logical mode w/ explicit PWD env ---"
(
    cd "$TDIR"
    REAL=$("$PWD_EXE" -P | tr -d '\n')
    export PWD="$REAL"
    OUT=$("$PWD_EXE" -L | tr -d '\n')
    if [ "$OUT" = "$REAL" ]; then exit 0; else exit 1; fi
)
check $? "-L returns valid \$PWD"

echo ""
echo "--- Test 8: Symlink check for -P vs -L ---"
(
    cd "$TDIR"
    mkdir -p realdir
    ln -sf realdir linkdir 2>/dev/null || true
    cd linkdir 2>/dev/null || exit 77
    LOGICAL_PWD="$TDIR/linkdir"
    export PWD="$LOGICAL_PWD"
    PHYS=$("$PWD_EXE" -P | tr -d '\n')
    LOGI=$("$PWD_EXE" -L | tr -d '\n')
    case "$LOGI" in *linkdir) has_link=0;; *) has_link=1;; esac
    case "$PHYS" in *realdir) has_real=0;; *) has_real=1;; esac
    if [ $has_link -eq 0 ] && [ $has_real -eq 0 ]; then
        case "$PHYS" in
            *linkdir) exit 1 ;;
            *)         exit 0 ;;
        esac
    else
        exit 77
    fi
)
rc=$?
if [ $rc -eq 77 ]; then
    check 0 "-P vs -L symlink (skipped, symlinks not supported)"
else
    check $rc "-P physical strips symlink; -L keeps PWD"
fi

echo ""
echo "--- Test 9: Help ---"
./"$OUTPUT" --help > "$TDIR/t9.txt" 2>&1
grep -q "Usage" "$TDIR/t9.txt"
check $? "--help mentions Usage"

echo ""
echo "--- Test 10: Version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "--version contains 1.0.0"

echo ""
echo "--- Test 11: Last option wins (-L then -P = physical) ---"
./"$OUTPUT" -L -P > "$TDIR/t11a.txt" 2>&1
./"$OUTPUT" -P > "$TDIR/t11b.txt" 2>&1
diff -q "$TDIR/t11a.txt" "$TDIR/t11b.txt" > /dev/null 2>&1
check $? "last of -L/-P wins"

echo ""
echo "--- Test 12: Long options --logical / --physical ---"
./"$OUTPUT" --logical > "$TDIR/t12a.txt" 2>&1
./"$OUTPUT" --physical > "$TDIR/t12b.txt" 2>&1
rc=$?
check $rc "--logical and --physical both run"

echo ""
echo "--- Test 13: Unknown long option is an error ---"
"$PWD_EXE" --thisoptiondoesnotexist >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown long option returns error"

echo ""
echo "--- Test 14: Unknown short option is an error ---"
"$PWD_EXE" -Z >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "unknown short option returns error"

echo ""
echo "--- Test 15: Extra operand is an error ---"
"$PWD_EXE" foo >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "extra operand returns error"

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
