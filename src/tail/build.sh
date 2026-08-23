#!/bin/bash
# Build and test script for tail.c (Unix/Linux/macOS/BSD)
#
# Re-implements GNU tail(1) behavior.  Tested on Linux, macOS, FreeBSD,
# OpenBSD and NetBSD with the cclinuxtools project.

set -e

echo "============================================"
echo "     tail.c Build Script for Unix"
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
OUTPUT="tail"
SOURCE="tail.c"

# --- Platform detection (POSIX feature macro) ---
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

EXTRA_FLAGS=""
case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

echo
echo "Detected platform: $PLATFORM"
echo
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo
echo "[2/3] Compiling..."
echo "  Compiler: $CC"
echo "  CFLAGS:   $CFLAGS $EXTRA_FLAGS"

$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE" 2>build_err.log
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
echo "  Running tests..."
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

# --- Fixtures ---
TMPDIR_TEST=$(mktemp -d)
trap 'rm -rf "$TMPDIR_TEST" stderr.tmp' EXIT

# in12.txt: 12 lines "1".."12"
IN12="$TMPDIR_TEST/in12.txt"
for i in $(seq 1 12); do echo "$i"; done > "$IN12"

# fa.txt: 3 lines a1,a2,a3
FA="$TMPDIR_TEST/fa.txt"
printf 'a1\na2\na3\n' > "$FA"

# fb.txt: 2 lines b1,b2
FB="$TMPDIR_TEST/fb.txt"
printf 'b1\nb2\n' > "$FB"

# empty.txt
EMPTY="$TMPDIR_TEST/empty.txt"
: > "$EMPTY"

# nonl.txt: no trailing newline
NONL="$TMPDIR_TEST/nonl.txt"
printf 'no newline at end' > "$NONL"

# z.txt: x\0y\0z\0  (zero-terminated records)
ZFILE="$TMPDIR_TEST/z.txt"
printf 'x\000y\000z\000' > "$ZFILE"

# long.txt: a single 100000-byte line with a newline in the middle
LONG="$TMPDIR_TEST/long.txt"
{ head -c 50000 < /dev/zero | tr '\0' 'a'; printf '\n'; head -c 49999 < /dev/zero | tr '\0' 'a'; } > "$LONG"

# bytetest.txt: 'helloworld' (no BOM)
BYTE10="$TMPDIR_TEST/bytetest.txt"
printf 'helloworld' > "$BYTE10"

# --- T01 default 10 lines ---
echo "--- T01 default 10 lines ---"
out=$("./$OUTPUT" "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n')
[ "$out" = "$exp" ] && test_ok "T01 default" || test_fail "T01 default" "len=${#out}"

# --- T02 -n 3 ---
echo "--- T02 -n 3 ---"
out=$("./$OUTPUT" -n 3 "$IN12" 2>/dev/null)
exp=$(printf '10\n11\n12\n')
[ "$out" = "$exp" ] && test_ok "T02 -n3" || test_fail "T02 -n3"

# --- T03 -n 0 ---
echo "--- T03 -n 0 ---"
out=$("./$OUTPUT" -n 0 "$IN12" 2>/dev/null)
[ -z "$out" ] && test_ok "T03 -n0" || test_fail "T03 -n0" "not empty"

# --- T04 -n +3 (start at line 3) ---
echo "--- T04 -n +3 ---"
out=$("./$OUTPUT" -n +3 "$FA" 2>/dev/null)
exp=$(printf 'a3\n')
[ "$out" = "$exp" ] && test_ok "T04 -n+3" || test_fail "T04 -n+3"

# --- T05 -n +1 on long line (chunk-spanning bug regression) ---
echo "--- T05 -n +1 on 100k long line ---"
bytes=$("./$OUTPUT" -n +1 "$LONG" 2>/dev/null | wc -c)
[ "$bytes" = "100000" ] && test_ok "T05 long line intact" || test_fail "T05 long line intact" "bytes=$bytes"

# --- T06 -c 5 ---
echo "--- T06 -c 5 ---"
out=$("./$OUTPUT" -c 5 "$BYTE10" 2>/dev/null)
exp=$(printf 'world')
[ "$out" = "$exp" ] && test_ok "T06 -c5" || test_fail "T06 -c5"

# --- T07 -c +6 ---
echo "--- T07 -c +6 ---"
out=$("./$OUTPUT" -c +6 "$BYTE10" 2>/dev/null)
exp=$(printf 'world')
[ "$out" = "$exp" ] && test_ok "T07 -c+6" || test_fail "T07 -c+6"

# --- T08 -c 1K suffix ---
echo "--- T08 -c 1K suffix ---"
bytes=$("./$OUTPUT" -c 1K "$LONG" 2>/dev/null | wc -c)
[ "$bytes" = "1024" ] && test_ok "T08 -c1K" || test_fail "T08 -c1K" "bytes=$bytes"

# --- T09 obsolete -3 (last 3 lines) ---
echo "--- T09 obsolete -3 ---"
out=$("./$OUTPUT" -3 "$IN12" 2>/dev/null)
exp=$(printf '10\n11\n12\n')
[ "$out" = "$exp" ] && test_ok "T09 obsolete -3" || test_fail "T09 obsolete -3"

# --- T10 obsolete +2l (start at line 2) ---
echo "--- T10 obsolete +2l ---"
out=$("./$OUTPUT" +2l "$FA" 2>/dev/null)
exp=$(printf 'a2\na3\n')
[ "$out" = "$exp" ] && test_ok "T10 obsolete +2l" || test_fail "T10 obsolete +2l"

# --- T11 obsolete -5c (last 5 bytes) ---
echo "--- T11 obsolete -5c ---"
out=$("./$OUTPUT" -5c "$BYTE10" 2>/dev/null)
exp=$(printf 'world')
[ "$out" = "$exp" ] && test_ok "T11 obsolete -5c" || test_fail "T11 obsolete -5c"

# --- T12 obsolete -3c (last 3 bytes, x1) ---
echo "--- T12 obsolete -3c ---"
out=$("./$OUTPUT" -3c "$BYTE10" 2>/dev/null)
exp=$(printf 'rld')
[ "$out" = "$exp" ] && test_ok "T12 obsolete -3c" || test_fail "T12 obsolete -3c"

# --- T13 stdin ---
echo "--- T13 stdin ---"
out=$(printf 'p1\np2\np3\n' | "./$OUTPUT" 2>/dev/null)
exp=$(printf 'p1\np2\np3\n')
[ "$out" = "$exp" ] && test_ok "T13 stdin" || test_fail "T13 stdin"

# --- T14 stdin via - ---
echo "--- T14 stdin via - ---"
out=$(printf 'x\n' | "./$OUTPUT" - 2>/dev/null)
exp=$(printf 'x\n')
[ "$out" = "$exp" ] && test_ok "T14 stdin dash" || test_fail "T14 stdin dash"

# --- T15 multi-file headers ---
echo "--- T15 multi-file headers ---"
out=$("./$OUTPUT" -n 1 "$FA" "$FB" 2>/dev/null)
exp=$(printf '==> %s <==\na3\n\n==> %s <==\nb2\n' "$FA" "$FB")
[ "$out" = "$exp" ] && test_ok "T15 multi-file" || test_fail "T15 multi-file" "header mismatch"

# --- T16 -q suppresses headers ---
echo "--- T16 -q quiet ---"
out=$("./$OUTPUT" -q -n 1 "$FA" "$FB" 2>/dev/null)
exp=$(printf 'a3\nb2\n')
[ "$out" = "$exp" ] && test_ok "T16 -q" || test_fail "T16 -q"

# --- T17 -v forces headers even single file ---
echo "--- T17 -v verbose ---"
out=$("./$OUTPUT" -v -n 1 "$FA" 2>/dev/null)
exp=$(printf '==> %s <==\na3\n' "$FA")
[ "$out" = "$exp" ] && test_ok "T17 -v" || test_fail "T17 -v"

# --- T18 empty file ---
echo "--- T18 empty file ---"
out=$("./$OUTPUT" "$EMPTY" 2>/dev/null)
[ -z "$out" ] && test_ok "T18 empty" || test_fail "T18 empty" "not empty"

# --- T19 no trailing newline ---
echo "--- T19 no trailing newline ---"
out=$("./$OUTPUT" "$NONL" 2>/dev/null)
exp=$(printf 'no newline at end')
[ "$out" = "$exp" ] && test_ok "T19 nonl" || test_fail "T19 nonl"

# --- T20 -z zero-terminated ---
echo "--- T20 -z zero-terminated ---"
ob=$("./$OUTPUT" -z -n 1 "$ZFILE" 2>/dev/null | od -An -tx1 | tr -d ' \n')
eb=$(printf 'z\000' | od -An -tx1 | tr -d ' \n')
[ "$ob" = "$eb" ] && test_ok "T20 -z" || test_fail "T20 -z" "byte mismatch"

# --- T21 --help ---
echo "--- T21 --help ---"
"./$OUTPUT" --help 2>/dev/null | grep -q "Usage: tail" && test_ok "T21 --help" || test_fail "T21 --help"

# --- T22 --version ---
echo "--- T22 --version ---"
"./$OUTPUT" --version 2>/dev/null | grep -q "tail" && test_ok "T22 --version" || test_fail "T22 --version"

# --- T23 nonexistent file error ---
echo "--- T23 nonexistent file ---"
"./$OUTPUT" "$TMPDIR_TEST/no_such_file" >/dev/null 2>stderr.tmp
rc=$?
[ $rc -ne 0 ] && test_ok "T23 errcode" || test_fail "T23 errcode" "exit=$rc"
grep -q "cannot open" stderr.tmp && test_ok "T23 errmsg" || test_fail "T23 errmsg"

# --- T24 invalid number rejected ---
echo "--- T24 invalid number rejected ---"
"./$OUTPUT" -n abc "$IN12" >/dev/null 2>stderr.tmp
rc=$?
[ $rc -ne 0 ] && test_ok "T24 invalid -n" || test_fail "T24 invalid -n" "exit=$rc"

echo
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ $FAIL -eq 0 ]; then
    echo "  All tests passed!"
else
    echo "  Some tests failed!"
fi
rm -f stderr.tmp
exit 0
