#!/bin/bash
# Build and test script for dirname.c (Unix/Linux/macOS/BSD)
#
# Re-implements GNU coreutils dirname(1) behavior.  Tested on Linux, macOS,
# FreeBSD, OpenBSD and NetBSD with the cclinuxtools project.

set -e

echo "============================================"
echo "     dirname.c Build Script for Unix"
echo "============================================"

# --- Detect compiler ---
CC=""
for c in gcc cc clang; do
    if command -v "$c" >/dev/null 2>&1; then
        CC="$c"
        break
    fi
done
if [ -z "$CC" ]; then
    echo "[ERROR] No C compiler found (gcc, cc, or clang). Exiting."
    exit 1
fi

CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="dirname"
SOURCE="dirname.c"

echo
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo
echo "[2/3] Compiling..."
echo "  Compiler: $CC"
echo "  CFLAGS:   $CFLAGS"

$CC $CFLAGS -o "$OUTPUT" "$SOURCE" 2>build_err.log
BERR=$?
if [ "$BERR" -ne 0 ]; then
    echo "[ERROR] Build failed!"
    cat build_err.log
    exit 1
fi

# Check for warnings
if [ -s build_err.log ]; then
    echo "[WARN] Compiler produced output:"
    cat build_err.log
fi
rm -f build_err.log

echo
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"
echo
echo "============================================"
echo "  Running tests (40 cases)..."
echo "============================================"
echo

# --- Test harness ---
PASS=0
FAIL=0

test_ok() {
    echo "  [PASS] $1"
    PASS=$((PASS + 1))
}

test_fail() {
    echo "  [FAIL] $1  $2"
    FAIL=$((FAIL + 1))
}

# Compare string output (default: trailing newline added by dirname)
# Usage: chk_str "name" "expected" args...
chk_str() {
    local name="$1"
    local expected="$2"
    shift 2
    local out
    out=$("./$OUTPUT" "$@" 2>/dev/null)
    local rc=$?
    if [ $rc -ne 0 ]; then
        test_fail "$name" "exit=$rc"
        return
    fi
    if [ "$out" = "$expected" ]; then
        test_ok "$name"
    else
        test_fail "$name" "got='$out' exp='$expected'"
    fi
}

# Compare raw bytes (for -z tests). Uses od hex dump comparison.
# Usage: chk_bytes "name" "expected_bytes" args...
chk_bytes() {
    local name="$1"
    local expected="$2"
    shift 2
    local out
    out=$("./$OUTPUT" "$@" 2>/dev/null | od -An -tx1 | tr -d ' \n')
    local exp_hex
    exp_hex=$(printf '%s' "$expected" | od -An -tx1 | tr -d ' \n')
    if [ "$out" = "$exp_hex" ]; then
        test_ok "$name"
    else
        test_fail "$name" "byte mismatch"
    fi
}

# Expect a non-zero exit code (error path)
# Usage: chk_err "name" args...
chk_err() {
    local name="$1"
    shift
    "./$OUTPUT" "$@" >/dev/null 2>&1
    local rc=$?
    if [ $rc -ne 0 ]; then
        test_ok "$name"
    else
        test_fail "$name" "expected non-zero exit, got 0"
    fi
}

# --- Fixtures ---
TMPDIR_TEST=$(mktemp -d)
trap 'rm -rf "$TMPDIR_TEST"' EXIT

# --- T01 documented example: /usr/bin/sort ---
echo "--- T01 /usr/bin/sort ---"
chk_str "T01 usr-bin-sort" "/usr/bin" "/usr/bin/sort"

# --- T02 documented example: stdio.h (no slash -> .) ---
echo "--- T02 stdio.h ---"
chk_str "T02 stdio.h" "." "stdio.h"

# --- T03 documented example: two names ---
echo "--- T03 dir1/str dir2/str ---"
out=$("./$OUTPUT" dir1/str dir2/str 2>/dev/null)
exp=$(printf 'dir1\ndir2\n')
[ "$out" = "$exp" ] && test_ok "T03 two names" || test_fail "T03 two names" "got='$out'"

# --- T04 root / ---
echo "--- T04 / ---"
chk_str "T04 root" "/" "/"

# --- T05 double slash // ---
echo "--- T05 // ---"
chk_str "T05 dbl-slash" "/" "//"

# --- T06 triple slash /// ---
echo "--- T06 /// ---"
chk_str "T06 tpl-slash" "/" "///"

# --- T07 four slash //// ---
echo "--- T07 //// ---"
chk_str "T07 quad-slash" "/" "////"

# --- T08 no slash ---
echo "--- T08 a ---"
chk_str "T08 a" "." "a"

# --- T09 a/b ---
echo "--- T09 a/b ---"
chk_str "T09 a-b" "a" "a/b"

# --- T10 a/b/c ---
echo "--- T10 a/b/c ---"
chk_str "T10 a-b-c" "a/b" "a/b/c"

# --- T11 trailing slash a/b/c/ ---
echo "--- T11 a/b/c/ ---"
chk_str "T11 trailing" "a/b" "a/b/c/"

# --- T12 double trailing slash a/b/c// ---
echo "--- T12 a/b/c// ---"
chk_str "T12 dbl-trail" "a/b" "a/b/c//"

# --- T13 /a (single component under root) ---
echo "--- T13 /a ---"
chk_str "T13 slash-a" "/" "/a"

# --- T14 /a/b ---
echo "--- T14 /a/b ---"
chk_str "T14 slash-a-b" "/a" "/a/b"

# --- T15 /a/b/ ---
echo "--- T15 /a/b/ ---"
chk_str "T15 slash-a-b-trail" "/a" "/a/b/"

# --- T16 d/f ---
echo "--- T16 d/f ---"
chk_str "T16 d-f" "d" "d/f"

# --- T17 /d/f ---
echo "--- T17 /d/f ---"
chk_str "T17 slash-d-f" "/d" "/d/f"

# --- T18 d/f/ ---
echo "--- T18 d/f/ ---"
chk_str "T18 d-f-trail" "d" "d/f/"

# --- T19 dot . ---
echo "--- T19 . ---"
chk_str "T19 dot" "." "."

# --- T20 dotdot .. ---
echo "--- T20 .. ---"
chk_str "T20 dotdot" "." ".."

# --- T21 empty string "" ---
echo "--- T21 empty ---"
chk_str "T21 empty" "." ""

# --- T22 foo/bar ---
echo "--- T22 foo/bar ---"
chk_str "T22 foo-bar" "foo" "foo/bar"

# --- T23 foo/bar/ ---
echo "--- T23 foo/bar/ ---"
chk_str "T23 foo-bar-trail" "foo" "foo/bar/"

# --- T24 foo/bar// ---
echo "--- T24 foo/bar// ---"
chk_str "T24 foo-bar-dbl-trail" "foo" "foo/bar//"

# --- T25 -z NUL-terminated ---
echo "--- T25 -z /a/b ---"
chk_bytes "T25 -z" "/a" "-z" "/a/b"

# --- T26 --zero long form ---
echo "--- T26 --zero /a/b ---"
chk_bytes "T26 --zero" "/a" "--zero" "/a/b"

# --- T27 --help ---
echo "--- T27 --help ---"
out=$("./$OUTPUT" --help 2>/dev/null)
rc=$?
if [ $rc -eq 0 ] && printf '%s' "$out" | grep -q "Usage: dirname"; then
    test_ok "T27 --help"
else
    test_fail "T27 --help" "exit=$rc"
fi

# --- T28 --version ---
echo "--- T28 --version ---"
out=$("./$OUTPUT" --version 2>/dev/null)
rc=$?
if [ $rc -eq 0 ] && printf '%s' "$out" | grep -qi "dirname"; then
    test_ok "T28 --version"
else
    test_fail "T28 --version" "exit=$rc"
fi

# --- T29 no operand (error) ---
echo "--- T29 no operand ---"
chk_err "T29 no operand"

# --- T30 unknown option (error) ---
echo "--- T30 unknown option ---"
chk_err "T30 unknown opt" "--thisopt"

# --- T31 invalid short option (error) ---
echo "--- T31 invalid short ---"
chk_err "T31 invalid short" "-X"

# --- T32 -- end-of-options sentinel ---
echo "--- T32 -- /a/b ---"
chk_str "T32 dashdash" "/a" -- "/a/b"

# --- T33 single dash as name ---
echo "--- T33 - ---"
chk_str "T33 dash" "." "-"

# --- T34 -- followed by dash ---
echo "--- T34 -- - ---"
chk_str "T34 dashdash-dash" "." -- "-"

# --- T35 three names ---
echo "--- T35 a/b a/c a/d ---"
out=$("./$OUTPUT" a/b a/c a/d 2>/dev/null)
exp=$(printf 'a\na\na\n')
[ "$out" = "$exp" ] && test_ok "T35 three names" || test_fail "T35 three names" "got='$out'"

# --- T36 -z with two names ---
echo "--- T36 -z a/b c/d ---"
out=$("./$OUTPUT" -z a/b c/d 2>/dev/null | od -An -tx1 | tr -d ' \n')
exp=$(printf 'a\0c\0' | od -An -tx1 | tr -d ' \n')
[ "$out" = "$exp" ] && test_ok "T36 -z two" || test_fail "T36 -z two" "byte mismatch"

# --- T37 /usr/bin/sort /usr/lib/libc.so ---
echo "--- T37 two abs paths ---"
out=$("./$OUTPUT" /usr/bin/sort /usr/lib/libc.so 2>/dev/null)
exp=$(printf '/usr/bin\n/usr/lib\n')
[ "$out" = "$exp" ] && test_ok "T37 two abs" || test_fail "T37 two abs" "got='$out'"

# --- T38 a/. (dot as last component) ---
echo "--- T38 a/. ---"
chk_str "T38 a-dot" "a" "a/."

# --- T39 a/.. (dotdot as last component) ---
echo "--- T39 a/.. ---"
chk_str "T39 a-dotdot" "a" "a/.."

# --- T40 ./a (relative leading) ---
echo "--- T40 ./a ---"
chk_str "T40 dot-slash-a" "." "./a"

echo
echo "============================================"
echo "  Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
else
    exit 0
fi
