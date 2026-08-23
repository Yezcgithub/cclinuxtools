#!/bin/bash
# Build and test script for sort.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    sort.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="sort"
SOURCE="sort.c"
PASS=0
FAIL=0

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
echo "  Running tests..."
echo "============================================"

TDIR=_build_test
rm -rf "$TDIR"
mkdir -p "$TDIR"

test_contains() {
    local name="$1"; local pattern="$2"; shift 2
    local result
    result=$("$@" 2>&1)
    if echo "$result" | grep -q "$pattern"; then
        echo "  [PASS] $name"; PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"; FAIL=$((FAIL + 1))
        echo "         expected: '$pattern'"; echo "         got: '$result'"
    fi
}
test_exit_zero() {
    local name="$1"; shift
    if "$@" >/dev/null 2>&1; then
        echo "  [PASS] $name"; PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"; FAIL=$((FAIL + 1))
    fi
}
test_exit_nonzero() {
    local name="$1"; shift
    if "$@" >/dev/null 2>&1; then
        echo "  [FAIL] $name (expected non-zero)"; FAIL=$((FAIL + 1))
    else
        echo "  [PASS] $name"; PASS=$((PASS + 1))
    fi
}

printf "banana\napple\ncherry\n"               > "$TDIR/words.txt"
printf "10\n2\n1\n20\n3\n"                      > "$TDIR/nums.txt"
printf "2K\n1G\n500\n3M\n"                     > "$TDIR/human.txt"
printf "apple\nApple\nbanana\nBanana\n"        > "$TDIR/mixed_case.txt"
printf "3 1\n1 5\n2 3\n1 2\n"                  > "$TDIR/two_fields.txt"
printf ""                                       > "$TDIR/empty.txt"
printf "single\n"                               > "$TDIR/single.txt"
printf "aaa\nbbb\nccc\n"                        > "$TDIR/sorted.txt"
printf "ccc\nbbb\naaa\n"                        > "$TDIR/reverse_sorted.txt"
printf "MAR\nJAN\nDEC\nAUG\nxxx\n"             > "$TDIR/months.txt"
printf "v1.10\nv1.2\nv2.0\nv1.0.1\nv1.0\n"    > "$TDIR/versions.txt"

echo ""
echo "--- T1: Basic sort first ---"
R=$(./$OUTPUT "$TDIR/words.txt" | head -1)
[ "$R" = "apple" ] && { echo "  [PASS] T1"; PASS=$((PASS+1)); } || { echo "  [FAIL] T1 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T2: Basic sort last ---"
R=$(./$OUTPUT "$TDIR/words.txt" | tail -1)
[ "$R" = "cherry" ] && { echo "  [PASS] T2"; PASS=$((PASS+1)); } || { echo "  [FAIL] T2 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T3: -r reverse ---"
R=$(./$OUTPUT -r "$TDIR/words.txt" | head -1)
[ "$R" = "cherry" ] && { echo "  [PASS] T3"; PASS=$((PASS+1)); } || { echo "  [FAIL] T3 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T4: -n numeric first ---"
R=$(./$OUTPUT -n "$TDIR/nums.txt" | head -1)
[ "$R" = "1" ] && { echo "  [PASS] T4"; PASS=$((PASS+1)); } || { echo "  [FAIL] T4 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T5: -n numeric last ---"
R=$(./$OUTPUT -n "$TDIR/nums.txt" | tail -1)
[ "$R" = "20" ] && { echo "  [PASS] T5"; PASS=$((PASS+1)); } || { echo "  [FAIL] T5 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T6: -h human first ---"
R=$(./$OUTPUT -h "$TDIR/human.txt" | head -1)
[ "$R" = "500" ] && { echo "  [PASS] T6"; PASS=$((PASS+1)); } || { echo "  [FAIL] T6 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T7: -h human last ---"
R=$(./$OUTPUT -h "$TDIR/human.txt" | tail -1)
[ "$R" = "1G" ] && { echo "  [PASS] T7"; PASS=$((PASS+1)); } || { echo "  [FAIL] T7 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T8: -f ignore case ---"
R=$(./$OUTPUT -f "$TDIR/mixed_case.txt" | head -1)
{ [ "$R" = "apple" ] || [ "$R" = "Apple" ]; } && { echo "  [PASS] T8"; PASS=$((PASS+1)); } || { echo "  [FAIL] T8 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T9: -k field 2 ---"
R=$(./$OUTPUT -k2,2 "$TDIR/two_fields.txt" | head -1)
[ "$R" = "3 1" ] && { echo "  [PASS] T9"; PASS=$((PASS+1)); } || { echo "  [FAIL] T9 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T10: -t with explicit sep ---"
R=$(./$OUTPUT -t' ' -k2,2 "$TDIR/two_fields.txt" | head -1)
[ "$R" = "3 1" ] && { echo "  [PASS] T10"; PASS=$((PASS+1)); } || { echo "  [FAIL] T10 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T11: -u unique ---"
printf "a\na\nb\nb\nc\n" > "$TDIR/dups.txt"
C=$(./$OUTPUT -u "$TDIR/dups.txt" | wc -l)
[ "$C" = "3" ] && { echo "  [PASS] T11"; PASS=$((PASS+1)); } || { echo "  [FAIL] T11 ($C)"; FAIL=$((FAIL+1)); }

echo "--- T12: -o output file ---"
./$OUTPUT "$TDIR/words.txt" -o "$TDIR/out12.txt"
R=$(head -1 "$TDIR/out12.txt")
[ "$R" = "apple" ] && { echo "  [PASS] T12"; PASS=$((PASS+1)); } || { echo "  [FAIL] T12 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T13: -c check sorted ---"
test_exit_zero "T13 -c sorted" ./$OUTPUT -c "$TDIR/sorted.txt"

echo "--- T14: -c check unsorted ---"
test_exit_nonzero "T14 -c unsorted" ./$OUTPUT -c "$TDIR/reverse_sorted.txt"

echo "--- T15: -C silent sorted ---"
test_exit_zero "T15 -C sorted" ./$OUTPUT -C "$TDIR/sorted.txt"

echo "--- T16: -C silent unsorted ---"
test_exit_nonzero "T16 -C unsorted" ./$OUTPUT -C "$TDIR/reverse_sorted.txt"

echo "--- T17: --help Usage ---"
test_contains "T17 --help" "Usage" ./$OUTPUT --help

echo "--- T18: --version ---"
test_contains "T18 --version" "1.0.0" ./$OUTPUT --version

echo "--- T19: empty file ---"
C=$(./$OUTPUT "$TDIR/empty.txt" | wc -l)
[ "$C" = "0" ] && { echo "  [PASS] T19"; PASS=$((PASS+1)); } || { echo "  [FAIL] T19 ($C)"; FAIL=$((FAIL+1)); }

echo "--- T20: single line ---"
R=$(./$OUTPUT "$TDIR/single.txt")
[ "$R" = "single" ] && { echo "  [PASS] T20"; PASS=$((PASS+1)); } || { echo "  [FAIL] T20 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T21: stdin ---"
R=$(printf "c\na\nb\n" | ./$OUTPUT | head -1)
[ "$R" = "a" ] && { echo "  [PASS] T21"; PASS=$((PASS+1)); } || { echo "  [FAIL] T21 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T22: -rn combined ---"
R=$(./$OUTPUT -rn "$TDIR/nums.txt" | head -1)
[ "$R" = "20" ] && { echo "  [PASS] T22"; PASS=$((PASS+1)); } || { echo "  [FAIL] T22 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T23: -k 1n ---"
printf "10 a\n2 b\n1 c\n" > "$TDIR/keynum.txt"
R=$(./$OUTPUT -k1n "$TDIR/keynum.txt" | head -1)
[ "$R" = "1 c" ] && { echo "  [PASS] T23"; PASS=$((PASS+1)); } || { echo "  [FAIL] T23 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T24: multiple files ---"
C=$(./$OUTPUT "$TDIR/sorted.txt" "$TDIR/reverse_sorted.txt" | wc -l)
[ "$C" = "6" ] && { echo "  [PASS] T24"; PASS=$((PASS+1)); } || { echo "  [FAIL] T24 ($C)"; FAIL=$((FAIL+1)); }

echo "--- T25: -s stable ---"
printf "b 1\na 1\na 2\nb 2\n" > "$TDIR/stable.txt"
R=$(./$OUTPUT -s -k1,1 "$TDIR/stable.txt" | head -2 | tail -1)
[ "$R" = "a 2" ] && { echo "  [PASS] T25"; PASS=$((PASS+1)); } || { echo "  [FAIL] T25 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T26: multi keys ---"
printf "b 2\na 2\na 1\nb 1\n" > "$TDIR/mk.txt"
R=$(./$OUTPUT -k1,1 -k2,2n "$TDIR/mk.txt" | head -1)
[ "$R" = "a 1" ] && { echo "  [PASS] T26"; PASS=$((PASS+1)); } || { echo "  [FAIL] T26 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T27: unknown long option ---"
test_exit_nonzero "T27 unknown long" ./$OUTPUT --unknown

echo "--- T28: unknown short option ---"
test_exit_nonzero "T28 unknown short" ./$OUTPUT -Z

echo "--- T29: -k per-key reverse ---"
printf "3 a\n1 c\n2 b\n" > "$TDIR/kr.txt"
R=$(./$OUTPUT -k2,2r "$TDIR/kr.txt" | head -1)
[ "$R" = "1 c" ] && { echo "  [PASS] T29"; PASS=$((PASS+1)); } || { echo "  [FAIL] T29 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T30: -m merge sorted files ---"
C=$(./$OUTPUT -m "$TDIR/sorted.txt" "$TDIR/sorted.txt" | wc -l)
[ "$C" = "6" ] && { echo "  [PASS] T30"; PASS=$((PASS+1)); } || { echo "  [FAIL] T30 ($C)"; FAIL=$((FAIL+1)); }

echo "--- T31: -z zero-terminated ---"
printf "c\0a\0b\0" > "$TDIR/zero.txt"
R=$(./$OUTPUT -z "$TDIR/zero.txt" | tr '\0' '\n' | head -1)
[ "$R" = "a" ] && { echo "  [PASS] T31"; PASS=$((PASS+1)); } || { echo "  [FAIL] T31 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T32: -c -u strict order ---"
printf "a\na\nb\n" > "$TDIR/dup_sorted.txt"
test_exit_nonzero "T32 -c -u strict" ./$OUTPUT -c -u "$TDIR/dup_sorted.txt"

echo "--- T33: -M month sort ---"
R=$(./$OUTPUT -M "$TDIR/months.txt" | head -1)
[ "$R" = "xxx" ] && { echo "  [PASS] T33"; PASS=$((PASS+1)); } || { echo "  [FAIL] T33 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T34: -M month sort DEC last ---"
R=$(./$OUTPUT -M "$TDIR/months.txt" | tail -1)
[ "$R" = "DEC" ] && { echo "  [PASS] T34"; PASS=$((PASS+1)); } || { echo "  [FAIL] T34 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T35: -V version sort ---"
R=$(./$OUTPUT -V "$TDIR/versions.txt" | tail -1)
[ "$R" = "v2.0" ] && { echo "  [PASS] T35"; PASS=$((PASS+1)); } || { echo "  [FAIL] T35 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T36: -V version 1.2 < 1.10 ---"
R=$(./$OUTPUT -V "$TDIR/versions.txt" | sed -n '3p')
[ "$R" = "v1.2" ] && { echo "  [PASS] T36"; PASS=$((PASS+1)); } || { echo "  [FAIL] T36 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T37: --sort=numeric ---"
R=$(./$OUTPUT --sort=numeric "$TDIR/nums.txt" | head -1)
[ "$R" = "1" ] && { echo "  [PASS] T37"; PASS=$((PASS+1)); } || { echo "  [FAIL] T37 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T38: --sort=month ---"
R=$(./$OUTPUT --sort=month "$TDIR/months.txt" | tail -1)
[ "$R" = "DEC" ] && { echo "  [PASS] T38"; PASS=$((PASS+1)); } || { echo "  [FAIL] T38 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T39: option order independence -k then -r ---"
A=$(printf "b 3\na 1\nc 2\n" | ./$OUTPUT -k2,2n -r | md5sum)
B=$(printf "b 3\na 1\nc 2\n" | ./$OUTPUT -r -k2,2n | md5sum)
[ "$A" = "$B" ] && { echo "  [PASS] T39 order indep"; PASS=$((PASS+1)); } || { echo "  [FAIL] T39"; FAIL=$((FAIL+1)); }

echo "--- T40: --files0-from ---"
printf "%s\0%s\0" "$TDIR/sorted.txt" "$TDIR/sorted.txt" > "$TDIR/flist.0"
C=$(./$OUTPUT --files0-from="$TDIR/flist.0" | wc -l)
[ "$C" = "6" ] && { echo "  [PASS] T40 files0-from"; PASS=$((PASS+1)); } || { echo "  [FAIL] T40 ($C)"; FAIL=$((FAIL+1)); }

echo "--- T41: -k MV key options accepted ---"
printf "1 v1.10\n2 v1.2\n3 v1.0\n" > "$TDIR/vkey.txt"
R=$(./$OUTPUT -k2V "$TDIR/vkey.txt" | head -1)
[ "$R" = "3 v1.0" ] && { echo "  [PASS] T41 -k with V flag"; PASS=$((PASS+1)); } || { echo "  [FAIL] T41 ($R)"; FAIL=$((FAIL+1)); }

echo "--- T42: invalid --sort ---"
test_exit_nonzero "T42 invalid --sort" ./$OUTPUT --sort=foobar

echo "--- T43: -n implied leading blank skip ---"
printf "   5\n  10\n    1\n" > "$TDIR/leadb.txt"
R=$(./$OUTPUT -n "$TDIR/leadb.txt" | head -1)
[ "$R" = "    1" ] && { echo "  [PASS] T43 -n implied -b"; PASS=$((PASS+1)); } || { echo "  [FAIL] T43 ($R)"; FAIL=$((FAIL+1)); }

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