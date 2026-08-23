#!/bin/bash
# Build and test script for head.c (Unix/Linux/macOS/BSD)
#
# Re-implements GNU head(1) behavior.  Tested on Linux, macOS, FreeBSD,
# OpenBSD and NetBSD with the cclinuxtools project.

set -e

echo "============================================"
echo "     head.c Build Script for Unix"
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
OUTPUT="head"
SOURCE="head.c"

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
echo "  Running tests (57 cases)..."
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

# Run head with given args, optionally piped stdin
# Usage: run_head "stdin_data" arg1 arg2 ...
run_head() {
    local stdin_data=""
    if [ "$1" = "--stdin" ]; then
        stdin_data="$2"
        shift 2
    fi
    if [ -n "$stdin_data" ]; then
        printf '%s' "$stdin_data" | "./$OUTPUT" "$@" 2>stderr.tmp
    else
        "./$OUTPUT" "$@" 2>stderr.tmp
    fi
    RC=$?
    cat stderr.tmp
}

# Compare output byte-for-byte
# Usage: chk "name" "expected_file" args...
chk() {
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
    local exp
    exp=$(cat "$expected")
    if [ "$out" = "$exp" ]; then
        test_ok "$name"
    else
        test_fail "$name" "mismatch"
    fi
}

# Compare raw bytes (for -z tests)
chk_bytes() {
    local name="$1"
    local expected="$2"
    shift 2
    local out
    out=$("./$OUTPUT" "$@" 2>/dev/null | od -An -tx1 | tr -d ' \n')
    local exp_hex
    exp_hex=$(od -An -tx1 < "$expected" | tr -d ' \n')
    if [ "$out" = "$exp_hex" ]; then
        test_ok "$name"
    else
        test_fail "$name" "byte mismatch"
    fi
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

# fc.txt: 4 lines c1..c4
FC="$TMPDIR_TEST/fc.txt"
printf 'c1\nc2\nc3\nc4\n' > "$FC"

# empty.txt
EMPTY="$TMPDIR_TEST/empty.txt"
: > "$EMPTY"

# single.txt: one line
SINGLE="$TMPDIR_TEST/single.txt"
printf 'only-line\n' > "$SINGLE"

# nonl.txt: no trailing newline
NONL="$TMPDIR_TEST/nonl.txt"
printf 'no newline at end' > "$NONL"

# z.txt: x\0y\0z\0
ZFILE="$TMPDIR_TEST/z.txt"
printf 'x\000y\000z\000' > "$ZFILE"

# long.txt: 50 lines line1..line50
LONG="$TMPDIR_TEST/long.txt"
for i in $(seq 1 50); do echo "line$i"; done > "$LONG"

# Expected output files
EXP="$TMPDIR_TEST/exp"
exp_lines() { printf '%s\n' "$@"; }

# --- T01 default 10 lines ---
echo "--- T01 default 10 lines ---"
out=$("./$OUTPUT" "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n')
[ "$out" = "$exp" ] && test_ok "T01 default" || test_fail "T01 default" "len=${#out}"

# --- T02 -n 3 ---
echo "--- T02 -n 3 ---"
out=$("./$OUTPUT" -n 3 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n')
[ "$out" = "$exp" ] && test_ok "T02 -n3" || test_fail "T02 -n3"

# --- T03 -n 0 ---
echo "--- T03 -n 0 ---"
out=$("./$OUTPUT" -n 0 "$IN12" 2>/dev/null)
[ -z "$out" ] && test_ok "T03 -n0" || test_fail "T03 -n0" "len=${#out}"

# --- T04 -n -3 (all but last 3) ---
echo "--- T04 -n-3 ---"
out=$("./$OUTPUT" -n -3 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n')
[ "$out" = "$exp" ] && test_ok "T04 -n-3" || test_fail "T04 -n-3"

# --- T05 -n +4 (first 4) ---
echo "--- T05 -n+4 ---"
out=$("./$OUTPUT" -n +4 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n')
[ "$out" = "$exp" ] && test_ok "T05 -n+4" || test_fail "T05 -n+4"

# --- T06 -n 15 (more than file) ---
echo "--- T06 -n15 ---"
out=$("./$OUTPUT" -n 15 "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T06 -n15" || test_fail "T06 -n15"

# --- T07 --lines=3 ---
echo "--- T07 --lines=3 ---"
out=$("./$OUTPUT" --lines=3 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n')
[ "$out" = "$exp" ] && test_ok "T07 --lines=3" || test_fail "T07 --lines=3"

# --- T08 --lines=-2 ---
echo "--- T08 --lines=-2 ---"
out=$("./$OUTPUT" --lines=-2 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n')
[ "$out" = "$exp" ] && test_ok "T08 --lines=-2" || test_fail "T08 --lines=-2"

# --- T09 -c 5 ---
echo "--- T09 -c5 ---"
out=$("./$OUTPUT" -c 5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3')
[ "$out" = "$exp" ] && test_ok "T09 -c5" || test_fail "T09 -c5"

# --- T10 -c 0 ---
echo "--- T10 -c0 ---"
out=$("./$OUTPUT" -c 0 "$IN12" 2>/dev/null)
[ -z "$out" ] && test_ok "T10 -c0" || test_fail "T10 -c0"

# --- T11 -c -5 (all but last 5 bytes) ---
echo "--- T11 -c-5 ---"
out=$("./$OUTPUT" -c -5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n1')
[ "$out" = "$exp" ] && test_ok "T11 -c-5" || test_fail "T11 -c-5"

# --- T12 -c +3 (first 3 bytes) ---
echo "--- T12 -c+3 ---"
out=$("./$OUTPUT" -c +3 "$IN12" 2>/dev/null)
exp=$(printf '1\n2')
[ "$out" = "$exp" ] && test_ok "T12 -c+3" || test_fail "T12 -c+3"

# --- T13 --bytes=4 ---
echo "--- T13 --bytes=4 ---"
out=$("./$OUTPUT" --bytes=4 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n')
[ "$out" = "$exp" ] && test_ok "T13 --bytes=4" || test_fail "T13 --bytes=4"

# --- T14 -c 1b (512, clamped) ---
echo "--- T14 -c1b ---"
out=$("./$OUTPUT" -c 1b "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T14 -c1b" || test_fail "T14 -c1b"

# --- T15 -c 1c (1 byte) ---
echo "--- T15 -c1c ---"
out=$("./$OUTPUT" -c 1c "$IN12" 2>/dev/null)
exp=$(printf '1')
[ "$out" = "$exp" ] && test_ok "T15 -c1c" || test_fail "T15 -c1c"

# --- T16 -c 1w (2 bytes) ---
echo "--- T16 -c1w ---"
out=$("./$OUTPUT" -c 1w "$IN12" 2>/dev/null)
exp=$(printf '1\n')
[ "$out" = "$exp" ] && test_ok "T16 -c1w" || test_fail "T16 -c1w"

# --- T17 -c 2kB (2000, clamped) ---
echo "--- T17 -c2kB ---"
out=$("./$OUTPUT" -c 2kB "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T17 -c2kB" || test_fail "T17 -c2kB"

# --- T18 -c 1K (1024, clamped) ---
echo "--- T18 -c1K ---"
out=$("./$OUTPUT" -c 1K "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T18 -c1K" || test_fail "T18 -c1K"

# --- T19 obsolete -5 (first 5 lines) ---
echo "--- T19 obs -5 ---"
out=$("./$OUTPUT" -5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n')
[ "$out" = "$exp" ] && test_ok "T19 obs -5" || test_fail "T19 obs -5"

# --- T20 obsolete -5c (first 5 bytes) ---
echo "--- T20 obs -5c ---"
out=$("./$OUTPUT" -5c "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3')
[ "$out" = "$exp" ] && test_ok "T20 obs -5c" || test_fail "T20 obs -5c"

# --- T21 obsolete -5q (quiet first 5) ---
echo "--- T21 obs -5q ---"
out=$("./$OUTPUT" -5q "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n')
[ "$out" = "$exp" ] && test_ok "T21 obs -5q" || test_fail "T21 obs -5q"

# --- T22 obsolete -5v (verbose single) ---
echo "--- T22 obs -5v ---"
out=$("./$OUTPUT" -5v "$IN12" 2>/dev/null)
exp=$(printf '==> %s <==\n1\n2\n3\n4\n5\n' "$IN12")
[ "$out" = "$exp" ] && test_ok "T22 obs -5v" || test_fail "T22 obs -5v"

# --- T23 two files headers ---
echo "--- T23 two files headers ---"
out=$("./$OUTPUT" "$FA" "$FB" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n==> %s <==\nb1\nb2\n' "$FA" "$FB")
[ "$out" = "$exp" ] && test_ok "T23 two files" || test_fail "T23 two files"

# --- T24 -q two files (no headers) ---
echo "--- T24 -q two ---"
out=$("./$OUTPUT" -q "$FA" "$FB" 2>/dev/null)
exp=$(printf 'a1\na2\na3\nb1\nb2\n')
[ "$out" = "$exp" ] && test_ok "T24 -q two" || test_fail "T24 -q two"

# --- T25 --quiet two files ---
echo "--- T25 --quiet two ---"
out=$("./$OUTPUT" --quiet "$FA" "$FB" 2>/dev/null)
exp=$(printf 'a1\na2\na3\nb1\nb2\n')
[ "$out" = "$exp" ] && test_ok "T25 --quiet two" || test_fail "T25 --quiet two"

# --- T26 -v single (force header) ---
echo "--- T26 -v single ---"
out=$("./$OUTPUT" -v "$FA" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n' "$FA")
[ "$out" = "$exp" ] && test_ok "T26 -v single" || test_fail "T26 -v single"

# --- T27 --verbose single ---
echo "--- T27 --verbose ---"
out=$("./$OUTPUT" --verbose "$FA" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n' "$FA")
[ "$out" = "$exp" ] && test_ok "T27 --verbose" || test_fail "T27 --verbose"

# --- T28 three files headers ---
echo "--- T28 three files headers ---"
out=$("./$OUTPUT" "$FA" "$FB" "$FC" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n==> %s <==\nb1\nb2\n==> %s <==\nc1\nc2\nc3\nc4\n' "$FA" "$FB" "$FC")
[ "$out" = "$exp" ] && test_ok "T28 three files" || test_fail "T28 three files"

# --- T29 stdin via - ---
echo "--- T29 stdin via - ---"
out=$(printf 'x1\nx2\nx3\n' | "./$OUTPUT" -n 2 - 2>/dev/null)
exp=$(printf 'x1\nx2\n')
[ "$out" = "$exp" ] && test_ok "T29 stdin -" || test_fail "T29 stdin -"

# --- T30 stdin default ---
echo "--- T30 stdin default ---"
out=$(printf 'a\nb\n' | "./$OUTPUT" - 2>/dev/null)
exp=$(printf 'a\nb\n')
[ "$out" = "$exp" ] && test_ok "T30 stdin default" || test_fail "T30 stdin default"

# --- T31 -z first 2 records ---
echo "--- T31 -z n2 ---"
"./$OUTPUT" -z -n 2 "$ZFILE" 2>/dev/null > "$TMPDIR_TEST/t31.out"
printf 'x\000y\000' > "$TMPDIR_TEST/t31.exp"
cmp -s "$TMPDIR_TEST/t31.out" "$TMPDIR_TEST/t31.exp" && test_ok "T31 -z n2" || test_fail "T31 -z n2"

# --- T32 --zero-terminated n1 ---
echo "--- T32 --zero n1 ---"
"./$OUTPUT" --zero-terminated -n 1 "$ZFILE" 2>/dev/null > "$TMPDIR_TEST/t32.out"
printf 'x\000' > "$TMPDIR_TEST/t32.exp"
cmp -s "$TMPDIR_TEST/t32.out" "$TMPDIR_TEST/t32.exp" && test_ok "T32 --zero n1" || test_fail "T32 --zero n1"

# --- T33 empty file ---
echo "--- T33 empty ---"
out=$("./$OUTPUT" "$EMPTY" 2>/dev/null)
[ -z "$out" ] && test_ok "T33 empty" || test_fail "T33 empty"

# --- T34 single line ---
echo "--- T34 single ---"
out=$("./$OUTPUT" "$SINGLE" 2>/dev/null)
exp=$(printf 'only-line\n')
[ "$out" = "$exp" ] && test_ok "T34 single" || test_fail "T34 single"

# --- T35 -n 1 single ---
echo "--- T35 -n1 single ---"
out=$("./$OUTPUT" -n 1 "$SINGLE" 2>/dev/null)
exp=$(printf 'only-line\n')
[ "$out" = "$exp" ] && test_ok "T35 -n1 single" || test_fail "T35 -n1 single"

# --- T36 no trailing newline ---
echo "--- T36 nonl ---"
out=$("./$OUTPUT" "$NONL" 2>/dev/null)
exp=$(printf 'no newline at end')
[ "$out" = "$exp" ] && test_ok "T36 nonl" || test_fail "T36 nonl"

# --- T37 -c 5 nonl ---
echo "--- T37 -c5 nonl ---"
out=$("./$OUTPUT" -c 5 "$NONL" 2>/dev/null)
exp=$(printf 'no ne')
[ "$out" = "$exp" ] && test_ok "T37 -c5 nonl" || test_fail "T37 -c5 nonl"

# --- T38 -n -1 (all but last 1) ---
echo "--- T38 -n-1 ---"
out=$("./$OUTPUT" -n -1 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n')
[ "$out" = "$exp" ] && test_ok "T38 -n-1" || test_fail "T38 -n-1"

# --- T39 -c -1 (all but last byte) ---
echo "--- T39 -c-1 ---"
out=$("./$OUTPUT" -c -1 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12')
[ "$out" = "$exp" ] && test_ok "T39 -c-1" || test_fail "T39 -c-1"

# --- T40 -n 50 long file (all) ---
echo "--- T40 -n50 long ---"
out=$("./$OUTPUT" -n 50 "$LONG" 2>/dev/null)
exp=$(cat "$LONG")
[ "$out" = "$exp" ] && test_ok "T40 -n50 long" || test_fail "T40 -n50 long"

# --- T41 -n -10 long (all but last 10) ---
echo "--- T41 -n-10 long ---"
out=$("./$OUTPUT" -n -10 "$LONG" 2>/dev/null)
exp=$(head -40 "$LONG")
[ "$out" = "$exp" ] && test_ok "T41 -n-10 long" || test_fail "T41 -n-10 long"

# --- T42 attached -n5 ---
echo "--- T42 -n5 attached ---"
out=$("./$OUTPUT" -n5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n')
[ "$out" = "$exp" ] && test_ok "T42 -n5 attached" || test_fail "T42 -n5 attached"

# --- T43 attached -c5 ---
echo "--- T43 -c5 attached ---"
out=$("./$OUTPUT" -c5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3')
[ "$out" = "$exp" ] && test_ok "T43 -c5 attached" || test_fail "T43 -c5 attached"

# --- T44 combined -qn5 ---
echo "--- T44 -qn5 ---"
out=$("./$OUTPUT" -qn5 "$FA" "$FB" 2>/dev/null)
exp=$(printf 'a1\na2\na3\nb1\nb2\n')
[ "$out" = "$exp" ] && test_ok "T44 -qn5" || test_fail "T44 -qn5"

# --- T45 -n5q (invalid, should error) ---
echo "--- T45 -n5q (invalid) ---"
"./$OUTPUT" -n5q "$IN12" >/dev/null 2>&1
rc=$?
[ $rc -ne 0 ] && test_ok "T45 -n5q err" || test_fail "T45 -n5q err" "exit=$rc (expected non-zero)"

# --- T46 -- ---
echo "--- T46 -- ---"
out=$("./$OUTPUT" -- "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n')
[ "$out" = "$exp" ] && test_ok "T46 -- file" || test_fail "T46 -- file"

# --- T47 -c 1KB ---
echo "--- T47 -c1KB ---"
out=$("./$OUTPUT" -c 1KB "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T47 -c1KB" || test_fail "T47 -c1KB"

# --- T48 -c 1MiB (clamped) ---
echo "--- T48 -c1MiB ---"
out=$("./$OUTPUT" -c 1MiB "$IN12" 2>/dev/null)
exp=$(cat "$IN12")
[ "$out" = "$exp" ] && test_ok "T48 -c1MiB" || test_fail "T48 -c1MiB"

# --- T49 multiple -n (last wins) ---
echo "--- T49 multi -n last wins ---"
out=$("./$OUTPUT" -n 2 -n 5 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n3\n4\n5\n')
[ "$out" = "$exp" ] && test_ok "T49 multi -n last wins" || test_fail "T49 multi -n last wins"

# --- T50 -n 1 -c 4 (last unit wins) ---
echo "--- T50 -n1 -c4 last wins ---"
out=$("./$OUTPUT" -n 1 -c 4 "$IN12" 2>/dev/null)
exp=$(printf '1\n2\n')
[ "$out" = "$exp" ] && test_ok "T50 -n1 -c4 last wins" || test_fail "T50 -n1 -c4 last wins"

# --- T51 --help ---
echo "--- T51 --help ---"
out=$("./$OUTPUT" --help 2>/dev/null)
echo "$out" | grep -q "Usage: head" && test_ok "T51 --help" || test_fail "T51 --help"

# --- T52 --version ---
echo "--- T52 --version ---"
out=$("./$OUTPUT" --version 2>&1)
echo "$out" | grep -qi "head" && test_ok "T52 --version" || test_fail "T52 --version"

# --- T53 missing file ---
echo "--- T53 missing file ---"
"./$OUTPUT" does_not_exist_zzz.txt >"$TMPDIR_TEST/t53.out" 2>"$TMPDIR_TEST/t53.err"
rc=$?
if [ $rc -eq 1 ] && [ ! -s "$TMPDIR_TEST/t53.out" ] && grep -q "cannot open" "$TMPDIR_TEST/t53.err"; then
    test_ok "T53 missing"
else
    test_fail "T53 missing" "exit=$rc"
fi

# --- T54 -q -v (verbose overrides quiet, single file) ---
echo "--- T54 -q -v single ---"
out=$("./$OUTPUT" -q -v "$FA" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n' "$FA")
[ "$out" = "$exp" ] && test_ok "T54 -q -v single" || test_fail "T54 -q -v single"

# --- T55 -v two files ---
echo "--- T55 -v two ---"
out=$("./$OUTPUT" -v "$FA" "$FB" 2>/dev/null)
exp=$(printf '==> %s <==\na1\na2\na3\n==> %s <==\nb1\nb2\n' "$FA" "$FB")
[ "$out" = "$exp" ] && test_ok "T55 -v two" || test_fail "T55 -v two"

# --- T56 -c 0 (empty output) ---
echo "--- T56 -c0 ---"
out=$("./$OUTPUT" -c 0 "$IN12" 2>/dev/null)
[ -z "$out" ] && test_ok "T56 -c0" || test_fail "T56 -c0"

# --- T57 -n 0 (empty output) ---
echo "--- T57 -n0 ---"
out=$("./$OUTPUT" -n 0 "$IN12" 2>/dev/null)
[ -z "$out" ] && test_ok "T57 -n0" || test_fail "T57 -n0"

echo
echo "============================================"
echo "  Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"

rm -f stderr.tmp
if [ "$FAIL" -gt 0 ]; then
    exit 1
else
    exit 0
fi
