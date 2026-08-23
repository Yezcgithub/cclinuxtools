#!/bin/bash
# Build and test script for less.c (Unix/Linux/macOS/BSD)
#
# Re-implements GNU less(1) behavior.  Tested on Linux, macOS, FreeBSD,
# OpenBSD and NetBSD with the cclinuxtools project.
#
# Usage:  ./build.sh

set -u

echo "============================================"
echo "    less.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="${CC:-cc}"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="less"
SOURCE="less.c"
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

LESS="./$OUTPUT"

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

# chk NAME EXPECTED_FILE LESS_ARGS...
# Runs $LESS with args, captures stdout to a tmp file, compares bytes
# with EXPECTED_FILE via cmp.
chk() {
    local name="$1" expf="$2"; shift 2
    local outf="$TDIR/_${name}.out"
    if "$LESS" "$@" >"$outf" 2>/dev/null; then
        if cmp -s "$outf" "$expf"; then
            test_ok "$name"
        else
            test_fail "$name" \
                "got=$(wc -c <"$outf") exp=$(wc -c <"$expf")"
        fi
    else
        local rc=$?
        test_fail "$name" "exit=$rc"
    fi
}

# chk_nonzero NAME LESS_ARGS...
# Runs $LESS and expects a non-zero exit status.
chk_nonzero() {
    local name="$1"; shift
    if "$LESS" "$@" >/dev/null 2>&1; then
        test_fail "$name" "expected non-zero exit"
    else
        test_ok "$name"
    fi
}

# chk_contains NAME PATTERN LESS_ARGS...
# Runs $LESS and checks if stdout contains PATTERN (grep -E).
chk_contains() {
    local name="$1" pattern="$2"; shift 2
    local out
    out=$("$LESS" "$@" 2>&1) || true
    if printf '%s' "$out" | grep -qE -- "$pattern"; then
        test_ok "$name"
    else
        test_fail "$name" "pattern=/$pattern/"
    fi
}

# ---------- fixtures ----------
printf 'line1\nline2\nline3\nline4\n'           > "$TDIR/in7.txt"
printf 'a\n\n\n\n\nb\nc\n'                       > "$TDIR/blanks.txt"
printf 'a\t01\t12345678\tEND\n'                  > "$TDIR/tabbed.txt"
printf 'one\ntwo\nthree\n'                       > "$TDIR/multi.txt"
printf 'header\n'                                > "$TDIR/f2.txt"
printf 'skip1\nskip2\nfind ME here\nkeep1\nkeep2\n' > "$TDIR/needle.txt"
printf ''                                        > "$TDIR/empty.txt"
printf 'only-line\n'                             > "$TDIR/single.txt"
printf '\xe4\xbd\xa0\xe5\xa5\xbd\n\xe4\xb8\xad\xe6\x96\x87\n\xe6\xb5\x8b\xe8\xaf\x95\n' > "$TDIR/cn.txt"
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ\n0123456789\n' > "$TDIR/long.txt"
printf 'A\x1b[31mB\x1b[0mC\n'                    > "$TDIR/ansi.txt"
printf 'ABC'                                      > "$TDIR/tail.bin"

# 100-line fixture
: > "$TDIR/h.txt"
i=0; while [ $i -lt 100 ]; do printf 'L%d\n' "$i" >> "$TDIR/h.txt"; i=$((i+1)); done

# 8192-line fixture
: > "$TDIR/big.txt"
i=0; while [ $i -lt 8192 ]; do printf 'x\n' >> "$TDIR/big.txt"; i=$((i+1)); done

# ---------- expected output files ----------
printf 'line1\nline2\nline3\nline4\n'                                                         > "$TDIR/e01.bin"
printf '      1 line1\n      2 line2\n      3 line3\n      4 line4\n'                       > "$TDIR/e02.bin"
printf 'line1\nline2\nline3\nline4\n'                                                         > "$TDIR/e04.bin"
printf 'a\n\nb\nc\n'                                                                          > "$TDIR/e05.bin"
printf 'a   01  12345678    END\n'                                                            > "$TDIR/e07.bin"
printf 'a       01      12345678        END\n'                                               > "$TDIR/e08.bin"
printf 'header\none\ntwo\nthree\n'                                                          > "$TDIR/e09.bin"
printf 'only-line\n'                                                                          > "$TDIR/e13.bin"
printf ' line1\n line2\n line3\n line4\n'                                                    > "$TDIR/e16.bin"
printf '       1 line1\n       2 line2\n       3 line3\n       4 line4\n'                    > "$TDIR/e17.bin"
printf 'find ME here\nkeep1\nkeep2\n'                                                        > "$TDIR/e18.bin"
printf ''                                                                                     > "$TDIR/e_empty.bin"
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ\n0123456789\n'                                            > "$TDIR/e25.bin"
printf 'line1\nline2\nline3\nline4\n'                                                         > "$TDIR/e_basic.bin"
printf '      1 line1\n      2 line2\n      3 line3\n      4 line4\n'                       > "$TDIR/e_lno.bin"

# ---------- tests ----------
echo ""
echo "--- T01 basic -E ---"
chk "T01 basic -E"   "$TDIR/e01.bin" -E "$TDIR/in7.txt"

echo "--- T02 -N numbers ---"
chk "T02 -N"         "$TDIR/e02.bin" -N -E "$TDIR/in7.txt"

echo "--- T03 --LINE-NUMBERS ---"
chk "T03 --LINE-NUMBERS" "$TDIR/e02.bin" --LINE-NUMBERS -E "$TDIR/in7.txt"

echo "--- T04 -n default (no numbers in streaming) ---"
chk "T04 -n default" "$TDIR/e04.bin" -n -E "$TDIR/in7.txt"

echo "--- T05 -s squeeze ---"
chk "T05 -s"         "$TDIR/e05.bin" -s -E "$TDIR/blanks.txt"

echo "--- T06 --squeeze-blank-lines ---"
chk "T06 --squeeze-blank-lines" "$TDIR/e05.bin" --squeeze-blank-lines -E "$TDIR/blanks.txt"

echo "--- T07 -x4 tabs ---"
chk "T07 -x4"        "$TDIR/e07.bin" -x 4 -E "$TDIR/tabbed.txt"

echo "--- T08 --tabs=8 ---"
chk "T08 --tabs=8"   "$TDIR/e08.bin" --tabs=8 -E "$TDIR/tabbed.txt"

echo "--- T09 cat two ---"
chk "T09 cat two"   "$TDIR/e09.bin" -E "$TDIR/f2.txt" "$TDIR/multi.txt"

echo "--- T10 -e ---"
chk "T10 -e"         "$TDIR/e_basic.bin" -e "$TDIR/in7.txt"

echo "--- T11 -F ---"
chk "T11 -F"         "$TDIR/e_basic.bin" -F "$TDIR/in7.txt"

echo "--- T12 empty ---"
chk "T12 empty"      "$TDIR/e_empty.bin" -E "$TDIR/empty.txt"

echo "--- T13 single ---"
chk "T13 single"     "$TDIR/e13.bin" -E "$TDIR/single.txt"

echo "--- T14 --help ---"
chk_contains "T14 --help" "Usage: less" --help

echo "--- T15 --version ---"
chk_contains "T15 --version" "less" --version

echo "--- T16 -J ---"
chk "T16 -J"         "$TDIR/e16.bin" -J -E "$TDIR/in7.txt"

echo "--- T17 -J -N ---"
chk "T17 -J -N"      "$TDIR/e17.bin" -J -N -E "$TDIR/in7.txt"

echo "--- T18 -p find ME ---"
chk "T18 -p find ME" "$TDIR/e18.bin" -p "find ME" -E "$TDIR/needle.txt"

echo "--- T19 -p case default (no match) ---"
chk "T19 -p case default" "$TDIR/e_empty.bin" -p "find me" -E "$TDIR/needle.txt"

echo "--- T20 -i soft ---"
chk "T20 -i soft"   "$TDIR/e18.bin" -i -p "find me" -E "$TDIR/needle.txt"

echo "--- T21 -I hard ---"
chk "T21 -I hard"   "$TDIR/e18.bin" -I -p "Find Me" -E "$TDIR/needle.txt"

echo "--- T22 -i + upper exact ---"
chk "T22 -i + upper exact" "$TDIR/e18.bin" -i -p "find ME" -E "$TDIR/needle.txt"

echo "--- T23 -p no match ---"
chk "T23 -p no match" "$TDIR/e_empty.bin" -p "NOPE" -E "$TDIR/needle.txt"

echo "--- T24 100 lines ---"
NLS=$("$LESS" -E "$TDIR/h.txt" | tr -cd '\n' | wc -c)
[ "$NLS" = "100" ] && test_ok "T24 100 lines" || test_fail "T24 100 lines" "nls=$NLS (want 100)"

echo "--- T25 -S large cols ---"
chk "T25 -S"        "$TDIR/e25.bin" -S -E "$TDIR/long.txt"

echo "--- T26 -Ns -x2 (5 segments after split on LF) ---"
N=$("$LESS" -Ns -x 2 -E "$TDIR/blanks.txt" 2>/dev/null | awk 'END{print NR}')
[ "$N" = "5" ] && test_ok "T26 -Ns -x2" || test_fail "T26 -Ns -x2" "segments=$N (want 5)"

echo "--- T27 CJK byte exact ---"
chk "T27 CJK exact" "$TDIR/cn.txt" -E "$TDIR/cn.txt"

echo "--- T28 invalid option ---"
chk_nonzero "T28 invalid option" --not-a-real-option-xyz -E "$TDIR/in7.txt"

echo "--- T29 -f ---"
chk "T29 -f"        "$TDIR/e_basic.bin" -f -E "$TDIR/in7.txt"

echo "--- T30 -R ANSI ---"
chk "T30 -R ANSI"   "$TDIR/ansi.txt" -R -E "$TDIR/ansi.txt"

echo "--- T31 -r raw ---"
chk "T31 -r raw"    "$TDIR/ansi.txt" -r -E "$TDIR/ansi.txt"

echo "--- T32 -~ ---"
chk "T32 -~"        "$TDIR/e_basic.bin" -~ -E "$TDIR/in7.txt"

echo "--- T33 +F ---"
chk "T33 +F"        "$TDIR/e_basic.bin" +F -E "$TDIR/in7.txt"

echo "--- T34 +N ---"
chk "T34 +N"        "$TDIR/e_lno.bin" +N -E "$TDIR/in7.txt"

echo "--- T35 missing file ---"
chk_nonzero "T35 missing file" -E "$TDIR/MISSING_NOPE.txt"

echo "--- T36 -o log ---"
rm -f "$TDIR/log.bin"
if "$LESS" -o "$TDIR/log.bin" -E "$TDIR/in7.txt" >"$TDIR/_t36.out" 2>/dev/null; then
    if cmp -s "$TDIR/_t36.out" "$TDIR/in7.txt" && cmp -s "$TDIR/log.bin" "$TDIR/in7.txt"; then
        test_ok "T36 -o log"
    else
        test_fail "T36 -o log" "out/log mismatch"
    fi
else
    test_fail "T36 -o log" "exit non-zero"
fi

echo "--- T37 -o refuse overwrite ---"
printf 'A\n' > "$TDIR/log.bin"
if "$LESS" -o "$TDIR/log.bin" -E "$TDIR/in7.txt" >/dev/null 2>&1; then
    test_fail "T37 -o refuse" "expected non-zero exit"
else
    test_ok "T37 -o refuse"
fi

echo "--- T38 -O overwrite ---"
printf 'XXX' > "$TDIR/log_over.bin"
if "$LESS" -O "$TDIR/log_over.bin" -E "$TDIR/in7.txt" >/dev/null 2>&1; then
    if cmp -s "$TDIR/log_over.bin" "$TDIR/in7.txt"; then
        test_ok "T38 -O overwrite"
    else
        test_fail "T38 -O overwrite" "log content mismatch"
    fi
else
    test_fail "T38 -O overwrite" "exit non-zero"
fi

echo "--- T39 --quit-at-eof ---"
chk "T39 --quit-at-eof" "$TDIR/e_basic.bin" --quit-at-eof "$TDIR/in7.txt"

echo "--- T40 --QUIT-AT-EOF ---"
chk "T40 --QUIT-AT-EOF" "$TDIR/e_basic.bin" --QUIT-AT-EOF "$TDIR/in7.txt"

echo "--- T41 --quit-if-one-screen ---"
chk "T41 --quit-if-one-screen" "$TDIR/e_basic.bin" --quit-if-one-screen "$TDIR/in7.txt"

echo "--- T42 -Rr ---"
chk "T42 -Rr"       "$TDIR/e_basic.bin" -R -r -E "$TDIR/in7.txt"

echo "--- T43 8192 lines ---"
chk "T43 8192 lines" "$TDIR/big.txt" -E "$TDIR/big.txt"

echo "--- T44 no trailing newline ---"
chk "T44 no trailing nl" "$TDIR/tail.bin" -E "$TDIR/tail.bin"

echo "--- T45 --force ---"
chk "T45 --force"   "$TDIR/e_basic.bin" --force -E "$TDIR/in7.txt"

echo "--- T46 -K stub ---"
chk "T46 -K stub"   "$TDIR/e_basic.bin" -K -E "$TDIR/in7.txt"

echo "--- T47 -L stub ---"
chk "T47 -L stub"   "$TDIR/e_basic.bin" -L -E "$TDIR/in7.txt"

echo "--- T48 -gG stubs ---"
chk "T48 -gG stubs" "$TDIR/e_basic.bin" -gG -E "$TDIR/in7.txt"

echo "--- T49 --buffers=64 ---"
chk "T49 --buffers=64" "$TDIR/e_basic.bin" --buffers=64 -E "$TDIR/in7.txt"

echo "--- T50 --max-back-scroll=2 ---"
chk "T50 --max-back-scroll=2" "$TDIR/e_basic.bin" --max-back-scroll=2 -E "$TDIR/in7.txt"

echo "--- T51 -P prompt ---"
chk "T51 -P prompt" "$TDIR/e_basic.bin" -P "?f%f .?ltlines %ltL?ln (END)" "$TDIR/in7.txt"

echo ""
echo "============================================"
echo "  PASS: $PASS   FAIL: $FAIL"
echo "============================================"
rm -rf "$TDIR"
rm -f less.dSYM
if [ "$FAIL" -gt 0 ]; then exit 1; else exit 0; fi
