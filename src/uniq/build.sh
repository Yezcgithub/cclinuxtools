#!/bin/bash
# Build and test script for uniq.c (Unix/Linux/macOS/BSD)
#
# Re-implements GNU coreutils uniq(1) behavior.  Tested on Linux,
# macOS, FreeBSD, OpenBSD and NetBSD with the cclinuxtools project.
#
# Usage:  ./build.sh

set -u

echo "============================================"
echo "    uniq.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="${CC:-cc}"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="uniq"
SOURCE="uniq.c"
PASS=0
FAIL=0

PLATFORM="unknown"
EXTRA_FLAGS=""

if [ -f /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    PLATFORM="$ID"
elif [ "$(uname -s)" = "Darwin" ]; then
    PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then
    PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then
    PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then
    PLATFORM="netbsd"
elif [ "$(uname -s)" = "Linux" ]; then
    PLATFORM="linux"
fi

case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="${CC:-gcc}" ;;
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
echo "  Platform : $PLATFORM"
echo "  Compiler : $CC $CFLAGS $EXTRA_FLAGS"
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
echo "  Running tests (51 total)..."
echo "============================================"

TDIR=_build_test_sh
rm -rf "$TDIR"
mkdir -p "$TDIR"

UNIQ="./$OUTPUT"

# ---------- helper functions ----------
test_ok() {
    echo "  [PASS] $1"; PASS=$((PASS + 1))
}
test_fail() {
    echo "  [FAIL] $1"; FAIL=$((FAIL + 1))
    shift
    for ln in "$@"; do
        echo "         $ln"
    done
}
test_nonzero() {
    # $1=name, rest=command
    local name="$1"; shift
    if "$@" >/dev/null 2>&1; then
        test_fail "$name" "expected non-zero exit"
    else
        test_ok "$name"
    fi
}
test_contains() {
    # $1=name $2=pattern rest=command
    local name="$1" pattern="$2"; shift 2
    local out
    out=$("$@" 2>&1) || true
    if printf '%s' "$out" | grep -q -- "$pattern"; then
        test_ok "$name"
    else
        test_fail "$name" "pattern=/$pattern/" "got(head)= $(printf '%s' "$out" | head -n 3)"
    fi
}
chk_bytes() {
    # usage: chk_bytes NAME EXPECTED_HEX [ARGS...]
    # EXPECTED_HEX: space-separated 2-digit hex bytes
    local name="$1" expected="$2"; shift 2
    local outf="$TDIR/_${name}_out.bin"
    local rc=0
    "$UNIQ" "$@" "$outf" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -ne 0 ]; then
        test_fail "$name" "exit=$rc"
        return
    fi
    local got_hex
    got_hex=$(od -An -tx1 -v "$outf" | tr -s ' \n' ' ' | sed -e 's/^ *//' -e 's/ *$//')
    if [ "$got_hex" = "$expected" ]; then
        test_ok "$name"
    else
        test_fail "$name" "expect bytes: $expected" "got    bytes: $got_hex"
    fi
}

# ---------- fixtures ----------
printf 'a\na\nb\nb\nb\nc\nd\nd\n'            > "$TDIR/basic.txt"   # T1-T13
printf '  1 foo\n  2 foo\n  3 bar\n  4 bar\n'  > "$TDIR/fc.txt"      # T32 skip-fields
printf 'ab123\nab456\nab999\nAB123\n'         > "$TDIR/ic.txt"      # T44 ignore-case
printf 'xxAlpha\nxxAlpaca\nxxAlgebra\n'       > "$TDIR/sc.txt"      # skip-chars
printf 'apple01\napple02\nbanana\n'           > "$TDIR/wc.txt"      # check-chars
printf ''                                      > "$TDIR/empty.txt"
printf 'solo\n'                                 > "$TDIR/single.txt"
printf 'a\na\nb\nc\nc\nc\nd\n'                 > "$TDIR/in7.txt"
printf '1 a\n2 a\n3 b\nx b\n5 c\n'             > "$TDIR/t33.txt"     # skip-fields all rep
printf 'xx12345\nxx12777\nxx22qqq\n'          > "$TDIR/t18b.txt"
printf 'ax\0bx\0bx\0cx\0dx\0dx\0dx\0'          > "$TDIR/z.bin"       # -z tests
printf 'ay\0by\0cy\0dy\0ey\0'                  > "$TDIR/z_uniq.bin"
printf 'qq\n' > "$TDIR/o.txt"
printf 'ab\ncd\ncd\nef\nef\nef\ngh\ngh\nij\n' > "$TDIR/tr.txt"

echo ""
echo "--- T1 basic uniq (8 input, 5 unique groups) ---"
C=$("$UNIQ" "$TDIR/basic.txt" | wc -l)
[ "$C" = "5" ] && test_ok "T1 basic" || test_fail "T1 basic" "lines=$C (want 5)"

echo "--- T2 -c count prefix ---"
FIRST=$("$UNIQ" -c "$TDIR/basic.txt" | sed -n '1p')
case "$FIRST" in
    *"2 a"*) test_ok "T2 -c first";;
    *) test_fail "T2 -c first" "got='$FIRST'";;
esac

echo "--- T3 -d only first of repeated groups (a,b,d repeated) ---"
C=$("$UNIQ" -d "$TDIR/basic.txt" | wc -l)
[ "$C" = "3" ] && test_ok "T3 -d count" || test_fail "T3 -d count" "lines=$C (want 3)"

echo "--- T4 -D all copies only for repeated groups ---"
OUT=$("$UNIQ" -D "$TDIR/basic.txt" | tr -d '\n')
EXP="aabbbdd"
[ "$OUT" = "$EXP" ] && test_ok "T4 -D content" || test_fail "T4 -D content" "got=$OUT want=$EXP"

echo "--- T5 -u only unique ---"
OUT=$("$UNIQ" -u "$TDIR/basic.txt" | tr -d '\n')
[ "$OUT" = "c" ] && test_ok "T5 -u" || test_fail "T5 -u" "got=$OUT want=c"

echo "--- T6 -f1 skip 1 field ---"
printf '1 foo\n2 foo\n3 bar\n' > "$TDIR/t6.txt"
C=$("$UNIQ" -f 1 "$TDIR/t6.txt" | wc -l)
[ "$C" = "2" ] && test_ok "T6 -f1" || test_fail "T6 -f1" "lines=$C (want 2)"

echo "--- T7 traditional -2 skip 2 fields ---"
printf 'a b 1\na b 2\na c 3\n' > "$TDIR/t7.txt"
C=$("$UNIQ" -2 "$TDIR/t7.txt" | wc -l)
[ "$C" = "2" ] && test_ok "T7 -2" || test_fail "T7 -2" "lines=$C (want 2)"

echo "--- T8 -s2 skip 2 chars ---"
OUT=$("$UNIQ" -s 2 "$TDIR/sc.txt" | wc -l)
[ "$OUT" = "1" ] && test_ok "T8 -s2 all match after skip" || test_fail "T8 -s2" "lines=$OUT (want 1)"

echo "--- T9 -w N check only N chars ---"
OUT=$("$UNIQ" -w 5 "$TDIR/wc.txt" | wc -l)
[ "$OUT" = "2" ] && test_ok "T9 -w5" || test_fail "T9 -w5" "lines=$OUT (want 2)"

echo "--- T10 -i ignore case ---"
OUT=$("$UNIQ" -i "$TDIR/ic.txt" | wc -l)
[ "$OUT" = "2" ] && test_ok "T10 -i" || test_fail "T10 -i" "lines=$OUT (want 2)"

echo "--- T11 positional output file (arg2) ---"
"$UNIQ" "$TDIR/basic.txt" "$TDIR/t11_out.txt"
C=$(wc -l < "$TDIR/t11_out.txt" | tr -d ' ')
[ "$C" = "5" ] && test_ok "T11 positional output" || test_fail "T11 positional output" "lines=$C"

echo "--- T12 stdin when no input file ---"
C=$(printf 'x\nx\ny\ny\ny\nz\n' | "$UNIQ" | wc -l)
[ "$C" = "3" ] && test_ok "T12 stdin" || test_fail "T12 stdin" "lines=$C (want 3)"

echo "--- T13 -c glued count -c3 (should conflict? no, 3=skip-fields traditional) ---"
# Actually -c3 means --count with traditional -3 => both. Make sure it parses.
printf 'a b 1\na b 2\na c 3\n' > "$TDIR/t13.txt"
OUT=$("$UNIQ" -c -3 "$TDIR/t13.txt" | sed -n '1p' | tr -s ' ')
case "$OUT" in
    " 2 a b 1"*) test_ok "T13 -c -3 glued count";;
    *"2 a b 1"*) test_ok "T13 -c -3 glued count";;
    *) test_fail "T13 -c -3" "got='$OUT'";;
esac

echo "--- T14 -f N glued -f1 ---"
printf '1 a\n2 a\n3 b\n' > "$TDIR/t14.txt"
C=$("$UNIQ" -f1 "$TDIR/t14.txt" | wc -l)
[ "$C" = "2" ] && test_ok "T14 -f1 glued" || test_fail "T14 -f1 glued" "lines=$C (want 2)"

echo "--- T15 traditional skip-fields -3 ---"
printf 'a b c 1\na b c 2\n' > "$TDIR/t15.txt"
C=$("$UNIQ" -3 "$TDIR/t15.txt" | wc -l)
[ "$C" = "1" ] && test_ok "T15 -3 skip-fields" || test_fail "T15 -3" "lines=$C (want 1)"

echo "--- T16 --all-repeated positional OUTPUT (std pipe via file) ---"
"$UNIQ" -D "$TDIR/in7.txt" "$TDIR/t16_out.txt"
C=$(wc -l < "$TDIR/t16_out.txt" | tr -d ' ')
[ "$C" = "5" ] && test_ok "T16 -D positional out" || test_fail "T16 -D pos out" "lines=$C (want 5)"

echo "--- T17 -z + -u (NUL items, keep unique only) ---"
# z_uniq.bin: ay by cy dy ey — all unique
"$UNIQ" -z -u "$TDIR/z_uniq.bin" "$TDIR/t17.bin"
HEX=$(od -An -tx1 -v "$TDIR/t17.bin" | tr -s ' \n' ' ' | sed -e 's/^ *//' -e 's/ *$//')
EXP_HEX="61 79 00 62 79 00 63 79 00 64 79 00 65 79 00"
[ "$HEX" = "$EXP_HEX" ] && test_ok "T17 -z -u" || test_fail "T17 -z -u" "exp=$EXP_HEX" "got=$HEX"

echo "--- T18 positional OUTPUT file (single file -> positional in+out) ---"
# INPUT plus OUTPUT both positional
"$UNIQ" "$TDIR/basic.txt" "$TDIR/t18_out.txt"
C=$(wc -l < "$TDIR/t18_out.txt" | tr -d ' ')
[ "$C" = "5" ] && test_ok "T18 in+out pos" || test_fail "T18 in+out pos" "lines=$C"

echo "--- T18b -s2 -w4 (skip 2, compare 4) ---"
# xx12345\nxx12777\nxx22qqq => compare keys: '1234' vs '1277' vs '22qq' => 3 groups
C=$("$UNIQ" -s2 -w4 "$TDIR/t18b.txt" | wc -l)
[ "$C" = "3" ] && test_ok "T18b -s2 -w4 3 groups" || test_fail "T18b s2w4" "lines=$C (want 3)"

echo "--- T19 empty input -> 0 lines ---"
C=$("$UNIQ" "$TDIR/empty.txt" | wc -l)
[ "$C" = "0" ] && test_ok "T19 empty" || test_fail "T19 empty" "lines=$C"

echo "--- T20 single record -> 1 line ---"
C=$("$UNIQ" "$TDIR/single.txt" | wc -l)
[ "$C" = "1" ] && test_ok "T20 single" || test_fail "T20 single" "lines=$C"

echo "--- T21 -z --zero-terminated combined with -c count ---"
"$UNIQ" -z -c "$TDIR/z.bin" "$TDIR/t21.bin"
# ax bx bx cx dx dx dx => groups: ax(1) bx(2) cx(1) dx(3)
# Each prefix "   %lu " + record + 0 byte
# Count leading space bytes; ensure 4 output records
CNT=$(tr '\0' '\n' < "$TDIR/t21.bin" | sed '/^$/d' | wc -l)
# Because 0 term replaced -> \n leaves 4 lines (pre-counted records)
[ "$CNT" = "4" ] && test_ok "T21 -z -c groups=4" || test_fail "T21 -z -c" "count=$CNT (want 4)"

echo "--- T22 -z -D all repeated NUL items ---"
"$UNIQ" -z -D "$TDIR/z.bin" "$TDIR/t22.bin"
# bx(2) dx(3) => 5 items total
N=$(tr '\0' '\n' < "$TDIR/t22.bin" | grep -c . || true)
[ "$N" = "5" ] && test_ok "T22 -z -D count=5" || test_fail "T22 -z -D" "n=$N (want 5)"

echo "--- T23 -z -d single copy of repeated NUL ---"
"$UNIQ" -z -d "$TDIR/z.bin" "$TDIR/t23.bin"
N=$(tr '\0' '\n' < "$TDIR/t23.bin" | grep -c . || true)
[ "$N" = "2" ] && test_ok "T23 -z -d groups=2" || test_fail "T23 -z -d" "n=$N (want 2)"

echo "--- T24 -z -u unique NUL items only ---"
"$UNIQ" -z -u "$TDIR/z.bin" "$TDIR/t24.bin"
HEX=$(od -An -tx1 -v "$TDIR/t24.bin" | tr -s ' \n' ' ' | sed -e 's/^ *//' -e 's/ *$//')
# ax 0 cx 0
[ "$HEX" = "61 78 00 63 78 00" ] && test_ok "T24 -z -u bytes" || test_fail "T24 -z -u" "hex=$HEX"

echo "--- T25 --group default SEPARATE (in7: 4 groups) ---"
chk_bytes "T25 group default" \
  "61 0a 61 0a 0a 62 0a 0a 63 0a 63 0a 63 0a 0a 64 0a" \
  --group "$TDIR/in7.txt"

echo "--- T26 --group=prepend ---"
chk_bytes "T26 group prepend" \
  "0a 61 0a 61 0a 0a 62 0a 0a 63 0a 63 0a 63 0a 0a 64 0a" \
  --group=prepend "$TDIR/in7.txt"

echo "--- T27 --group=append ---"
chk_bytes "T27 group append" \
  "61 0a 61 0a 0a 62 0a 0a 63 0a 63 0a 63 0a 0a 64 0a 0a" \
  --group=append "$TDIR/in7.txt"

echo "--- T28 --group=both ---"
chk_bytes "T28 group both" \
  "0a 61 0a 61 0a 0a 62 0a 0a 63 0a 63 0a 63 0a 0a 64 0a 0a" \
  --group=both "$TDIR/in7.txt"

echo "--- T29 --all-repeated (default none) equals -D content ---"
A=$("$UNIQ" -D "$TDIR/in7.txt" | tr -d '\n')
B=$("$UNIQ" --all-repeated "$TDIR/in7.txt" | tr -d '\n')
[ "$A" = "$B" ] && [ "$A" = "aaccc" ] && test_ok "T29 --all-repeated=-D" || test_fail "T29 --all-repeated" "A=$A B=$B"

echo "--- T30 --all-repeated=prepend ---"
chk_bytes "T30 allrep=prepend" \
  "0a 61 0a 61 0a 0a 63 0a 63 0a 63 0a" \
  --all-repeated=prepend "$TDIR/in7.txt"

echo "--- T31 --all-repeated=separate ---"
chk_bytes "T31 allrep=separate" \
  "61 0a 61 0a 0a 63 0a 63 0a 63 0a" \
  --all-repeated=separate "$TDIR/in7.txt"

echo "--- T32 -c -f1 count+skip 1 field ---"
"$UNIQ" -c -f 1 "$TDIR/fc.txt" "$TDIR/t32.txt"
FIRST=$(sed -n '1p' "$TDIR/t32.txt")
case "$FIRST" in
    *"2"*"foo"*) test_ok "T32 -c -f1";;
    *) test_fail "T32 -c -f1" "line1='$FIRST'";;
esac

echo "--- T33 -D -f1 (all repeated via skip-fields t33.txt) ---"
"$UNIQ" -D -f1 "$TDIR/t33.txt" "$TDIR/t33o.txt"
C=$(wc -l < "$TDIR/t33o.txt" | tr -d ' ')
[ "$C" = "4" ] && test_ok "T33 -D -f1 4 lines" || test_fail "T33 -D -f1" "lines=$C (want 4)"

echo "--- T34 --help ---"
test_contains "T34 --help" "Usage" "$UNIQ" --help

echo "--- T35 --version ---"
test_contains "T35 --version" "1\.0\.0" "$UNIQ" --version

echo "--- T36 unknown long option => non-zero ---"
test_nonzero "T36 unknown long" "$UNIQ" --this-does-not-exist

echo "--- T37 unknown short option => non-zero ---"
test_nonzero "T37 unknown short" "$UNIQ" -Q

echo "--- T38 incompatible -c -d ---"
test_nonzero "T38 -c -d" "$UNIQ" -c -d /dev/null

echo "--- T39 -cdiu multi mode conflict ---"
test_nonzero "T39 -cdiu conflict" "$UNIQ" -c -d -i -u /dev/null

echo "--- T40 --skip-fields requires argument ---"
test_nonzero "T40 --skip-fields noarg" "$UNIQ" --skip-fields /dev/null

echo "--- T41 --skip-chars invalid number ---"
test_nonzero "T41 --skip-chars nan" "$UNIQ" --skip-chars=abc /dev/null

echo "--- T42 extra positional operand ---"
test_nonzero "T42 extra operand" "$UNIQ" /dev/null /dev/null /dev/null

echo "--- T43 -w2 short glued ---"
printf 'ab12\nab34\nac56\n' > "$TDIR/t43.txt"
C=$("$UNIQ" -w2 "$TDIR/t43.txt" | wc -l)
[ "$C" = "2" ] && test_ok "T43 -w2 glued" || test_fail "T43 -w2 glued" "lines=$C (want 2)"

echo "--- T44 combined -if1 (ignore case + skip 1 field) ---"
printf '1 apple\n2 Apple\n3 banana\n' > "$TDIR/t44.txt"
C=$("$UNIQ" -i -f1 "$TDIR/t44.txt" | wc -l)
[ "$C" = "2" ] && test_ok "T44 -if1" || test_fail "T44 -if1" "lines=$C (want 2)"

echo "--- T45 non-existent input file => non-zero ---"
test_nonzero "T45 missing input" "$UNIQ" "$TDIR/does_not_exist_xyz.txt"

echo "--- T46 single all-repeated group via -D ---"
printf 'x\nx\nx\n' > "$TDIR/t46.txt"
C=$("$UNIQ" -D "$TDIR/t46.txt" | wc -l)
[ "$C" = "3" ] && test_ok "T46 -D single group all 3" || test_fail "T46 -D" "lines=$C (want 3)"

echo "--- T47 -d on all-unique input => empty ---"
printf 'a\nb\nc\nd\n' > "$TDIR/t47.txt"
OUT=$("$UNIQ" -d "$TDIR/t47.txt")
[ -z "$OUT" ] && test_ok "T47 -d all-unique empty" || test_fail "T47 -d not empty" "'$OUT'"

echo "--- T48 -u on all-unique input => same lines ---"
printf 'a\nb\nc\nd\n' > "$TDIR/t48.txt"
C=$("$UNIQ" -u "$TDIR/t48.txt" | wc -l)
[ "$C" = "4" ] && test_ok "T48 -u all-unique 4 lines" || test_fail "T48 -u" "lines=$C (want 4)"

echo "--- T49 --all-repeated invalid method => non-zero ---"
test_nonzero "T49 allrep bad method" "$UNIQ" --all-repeated=bogus /dev/null

echo "--- T50 --group invalid method => non-zero ---"
test_nonzero "T50 group bad method" "$UNIQ" --group=totally-bogus /dev/null

echo "--- T51 --check-chars --skip-chars --skip-fields long options accepted ---"
"$UNIQ" --skip-fields=1 --skip-chars=1 --check-chars=1 "$TDIR/fc.txt" >/dev/null 2>&1 \
  && test_ok "T51 long opts accepted" || test_fail "T51 long opts"

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