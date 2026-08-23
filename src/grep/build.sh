#!/bin/bash
# Build and test script for grep.c (Unix/Linux/macOS/BSD)

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=grep
SOURCE=grep.c
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
echo "============================================"
echo "    grep.c Build Script for Unix"
echo "============================================"
echo ""
echo "Detected platform: $PLATFORM"

EXTRA_FLAGS=""
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

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed!"
    exit 1
fi

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

echo ""
echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

check() {
    if [ "$1" -eq 0 ]; then
        echo "  [PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

# ---- fixtures ----
printf 'hello world\nfoo bar\nhello again\nend\n'     > "$WORKDIR/f1.txt"
printf 'one\ntwo\nthree\n42\nx9y\n'                   > "$WORKDIR/nums.txt"
printf 'a\nb\nc\nMATCH\nd\ne\nf\ng\nMATCH2\nh\n'      > "$WORKDIR/ctx.txt"
printf 'aaa\nabab\nword\nx123x\nhello world\n'        > "$WORKDIR/re.txt"
printf 'a.c\nabc\naxc\n'                              > "$WORKDIR/met.txt"
printf 'hit1\nmiss\nhit2\nhit3\n'                     > "$WORKDIR/m.txt"
touch "$WORKDIR/empty.txt"
printf 'hello\nend\n'                                 > "$WORKDIR/pats.txt"
printf 'AB\000C\nhi\n'                                > "$WORKDIR/bin.dat"

mkdir -p "$WORKDIR/t35/sub"
printf 'nothing here\n'                               > "$WORKDIR/t35/top.txt"
printf 'needle deep\n'                                > "$WORKDIR/t35/sub/deep.txt"

mkdir -p "$WORKDIR/t36"
printf 'needle in txt\n'                              > "$WORKDIR/t36/root.txt"
printf 'needle in log\n'                              > "$WORKDIR/t36/root.log"

mkdir "$WORKDIR/t38"

cd "$WORKDIR" || exit 1

# ========== T01: Basic match (exit 0 + output)
echo ""
echo "--- T01: Basic match ---"
./"$OUTPUT" hello f1.txt > got.txt 2>&1
rc=$?
grep -q "hello world" got.txt && [ $rc -eq 0 ]
check $? "basic match"

# ========== T02: No match exits 1
echo ""
echo "--- T02: No match exit 1 ---"
./"$OUTPUT" zebra f1.txt > /dev/null 2>&1
[ $? -eq 1 ]
check $? "no match exit 1"

# ========== T03: Missing file exits 2
echo ""
echo "--- T03: Missing file exit 2 ---"
./"$OUTPUT" hello nosuchfile.txt > /dev/null 2>&1
[ $? -eq 2 ]
check $? "missing file exit 2"

# ========== T04: -n line numbers
echo ""
echo "--- T04: -n line numbers ---"
./"$OUTPUT" -n foo f1.txt | grep -q "^2:foo bar$"
check $? "-n line numbers"

# ========== T05: -c count
echo ""
echo "--- T05: -c count ---"
[ "$(./"$OUTPUT" -c hello f1.txt)" = "2" ]
check $? "-c count"

# ========== T06: -v invert count
echo ""
echo "--- T06: -v invert ---"
[ "$(./"$OUTPUT" -vc e f1.txt)" = "1" ]
check $? "-v invert"

# ========== T07: -i ignore case
echo ""
echo "--- T07: -i ignore case ---"
[ "$(./"$OUTPUT" -ic HELLO f1.txt)" = "2" ]
check $? "-i ignore case"

# ========== T08: -w whole word
echo ""
echo "--- T08: -w whole word ---"
[ "$(./"$OUTPUT" -cw word re.txt)" = "1" ]
check $? "-w whole word"

# ========== T09: -x whole line
echo ""
echo "--- T09: -x whole line ---"
[ "$(./"$OUTPUT" -cx end f1.txt)" = "1" ] && [ "$(./"$OUTPUT" -cx en f1.txt)" = "0" ]
check $? "-x whole line"

# ========== T10: -E alternation
echo ""
echo "--- T10: -E alternation ---"
[ "$(./"$OUTPUT" -Ec "hello|foo" f1.txt)" = "3" ]
check $? "-E alternation"

# ========== T11: -F fixed strings
echo ""
echo "--- T11: -F fixed strings ---"
[ "$(./"$OUTPUT" -Fc "a.c" met.txt)" = "1" ] && [ "$(./"$OUTPUT" -Ec "a.c" met.txt)" = "3" ]
check $? "-F fixed strings"

# ========== T12: BRE interval
echo ""
echo "--- T12: BRE interval ---"
[ "$(./"$OUTPUT" -c 'a\{2,\}' re.txt)" = "1" ]
check $? "BRE interval"

# ========== T13: BRE backreference
echo ""
echo "--- T13: BRE backreference ---"
[ "$(./"$OUTPUT" -c '\(ab\)\1' re.txt)" = "1" ]
check $? "BRE backreference"

# ========== T14: ERE class plus
echo ""
echo "--- T14: ERE class plus ---"
[ "$(./"$OUTPUT" -Ec '[0-9]+' nums.txt)" = "2" ]
check $? "ERE class plus"

# ========== T15: POSIX :digit: class
echo ""
echo "--- T15: POSIX class ---"
[ "$(./"$OUTPUT" -c '[[:digit:]]' nums.txt)" = "2" ]
check $? "POSIX :digit: class"

# ========== T16: anchors
echo ""
echo "--- T16: Anchors ^ \$ ---"
[ "$(./"$OUTPUT" -c '^hello' f1.txt)" = "2" ] && [ "$(./"$OUTPUT" -c 'world$' f1.txt)" = "1" ]
check $? "anchors"

# ========== T17: -o only matching
echo ""
echo "--- T17: -o only matching ---"
lines=$(printf 'abc abc' | ./"$OUTPUT" -o abc | wc -l)
[ "$lines" = "2" ]
check $? "-o only matching"

# ========== T18: -b byte offset
echo ""
echo "--- T18: -b byte offset ---"
./"$OUTPUT" -b foo f1.txt | grep -q "^12:foo bar$"
check $? "-b byte offset"

# ========== T19: -q quiet exit codes
echo ""
echo "--- T19: -q quiet ---"
./"$OUTPUT" -q hello f1.txt 2>/dev/null; a=$?
./"$OUTPUT" -q zebra f1.txt 2>/dev/null; b=$?
[ $a -eq 0 ] && [ $b -eq 1 ]
check $? "-q quiet exit codes"

# ========== T20: -l files-with-matches
echo ""
echo "--- T20: -l list ---"
./"$OUTPUT" -l hello f1.txt nums.txt > got.txt
grep -q "f1.txt" got.txt && ! grep -q "nums.txt" got.txt
check $? "-l files-with-matches"

# ========== T21: -L files-without-match
echo ""
echo "--- T21: -L list ---"
./"$OUTPUT" -L hello f1.txt nums.txt > got.txt
grep -q "nums.txt" got.txt && ! grep -q "f1.txt" got.txt
check $? "-L files-without-match"

# ========== T22: -m max-count
echo ""
echo "--- T22: -m max-count ---"
[ "$(./"$OUTPUT" -cm2 hit m.txt)" = "2" ]
check $? "-m max-count"

# ========== T23: -e multiple patterns
echo ""
echo "--- T23: -e multiple ---"
[ "$(./"$OUTPUT" -c -e hello -e foo f1.txt)" = "3" ]
check $? "-e multiple patterns"

# ========== T24: -f pattern file
echo ""
echo "--- T24: -f pattern file ---"
[ "$(./"$OUTPUT" -cf pats.txt f1.txt)" = "3" ]
check $? "-f pattern file"

# ========== T25: -A after-context
echo ""
echo "--- T25: -A after-context ---"
./"$OUTPUT" -n -A1 MATCH ctx.txt > got.txt
grep -q "^5-d$" got.txt && grep -q "^10-h$" got.txt
check $? "-A after-context"

# ========== T26: -B before-context
echo ""
echo "--- T26: -B before-context ---"
./"$OUTPUT" -B1 MATCH ctx.txt | grep -q "^3-c$"
check $? "-B before-context"

# ========== T27: -C context with -- separator
echo ""
echo "--- T27: -C context ---"
./"$OUTPUT" -n -C1 MATCH ctx.txt > got.txt
grep -qx "\-\-" got.txt && grep -q "^4:MATCH$" got.txt && grep -q "^9:MATCH2$" got.txt
check $? "-C context with --"

# ========== T28: --group-separator
echo ""
echo "--- T28: --group-separator ---"
./"$OUTPUT" -n -C1 --group-separator=%% MATCH ctx.txt | grep -qx "%%"
check $? "--group-separator"

# ========== T29: stdin input
echo ""
echo "--- T29: stdin ---"
printf 'hello\nbye\n' | ./"$OUTPUT" hello | grep -qx "hello"
check $? "stdin input"

# ========== T30: multiple files show labels
echo ""
echo "--- T30: filename labels ---"
./"$OUTPUT" hello f1.txt nums.txt > got.txt
[ "$(grep -c "^./f1.txt:" got.txt)" = "2" ]
check $? "multiple file labels"

# ========== T31: -h suppress labels
echo ""
echo "--- T31: -h no labels ---"
./"$OUTPUT" -h hello f1.txt nums.txt > got.txt
grep -qx "hello world" got.txt
check $? "-h suppress labels"

# ========== T32: -H force labels
echo ""
echo "--- T32: -H force labels ---"
./"$OUTPUT" -H hello f1.txt | grep -q "^./f1.txt:hello world$"
check $? "-H force labels"

# ========== T33: binary file report
echo ""
echo "--- T33: Binary file matches ---"
./"$OUTPUT" hi bin.dat > got.txt 2>&1
rc=$?
grep -q "Binary file" got.txt && [ $rc -eq 0 ]
check $? "binary file report"

# ========== T34: -a text mode on binary
echo ""
echo "--- T34: -a text mode ---"
./"$OUTPUT" -a hi bin.dat | grep -qx "hi"
check $? "-a text mode"

# ========== T35: -r recursion
echo ""
echo "--- T35: -r recursion ---"
./"$OUTPUT" -r needle t35 > got.txt 2>&1
grep -q "deep.txt:needle deep" got.txt
check $? "-r recursion"

# ========== T36: --include glob
echo ""
echo "--- T36: --include ---"
./"$OUTPUT" -r --include=*.txt needle t36 > got.txt 2>&1
grep -q "root.txt" got.txt && ! grep -q "root.log" got.txt
check $? "--include glob"

# ========== T37: --exclude glob
echo ""
echo "--- T37: --exclude ---"
./"$OUTPUT" -r --exclude=*.log needle t36 > got.txt 2>&1
grep -q "root.txt" got.txt && ! grep -q "root.log" got.txt
check $? "--exclude glob"

# ========== T38: directory operand error
echo ""
echo "--- T38: Is a directory ---"
./"$OUTPUT" hello t38 > /dev/null 2> got.txt
rc=$?
grep -q "Is a directory" got.txt && [ $rc -eq 2 ]
check $? "directory operand error"

# ========== T39: -d skip directories
echo ""
echo "--- T39: -d skip ---"
./"$OUTPUT" -d skip hello t38 > got.txt 2> got.err
rc=$?
[ $rc -eq 1 ] && [ ! -s got.txt ] && [ ! -s got.err ]
check $? "-d skip directories"

# ========== T40: --help
echo ""
echo "--- T40: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help"

# ========== T41: --version
echo ""
echo "--- T41: --version ---"
./"$OUTPUT" --version | grep -q "v1.0.0"
check $? "--version"

# ========== T42: invalid regex exits 2
echo ""
echo "--- T42: invalid regex ---"
./"$OUTPUT" "[" f1.txt > /dev/null 2>&1
[ $? -eq 2 ]
check $? "invalid regex exit 2"

# ========== T43: invalid option exits 2
echo ""
echo "--- T43: invalid option ---"
./"$OUTPUT" -Z hello f1.txt > /dev/null 2>&1
[ $? -eq 2 ]
check $? "invalid option exit 2"

# ========== T44: empty pattern matches all
echo ""
echo "--- T44: empty pattern ---"
lines=$(printf 'a\nb\n' | ./"$OUTPUT" "" | wc -l)
[ "$lines" = "2" ]
check $? "empty pattern matches all"

# ========== T45: -s suppresses errors
echo ""
echo "--- T45: -s suppress ---"
./"$OUTPUT" -s hello nosuchfile.txt > /dev/null 2> got.err
rc=$?
[ $rc -eq 2 ] && [ ! -s got.err ]
check $? "-s suppresses errors"

# ========== T46: -o -b ascending offsets
echo ""
echo "--- T46: -o -b offsets ---"
printf 'abc abc' | ./"$OUTPUT" -ob abc > got.txt
grep -q "^0:abc$" got.txt && grep -q "^4:abc$" got.txt
check $? "-o -b ascending offsets"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ $FAIL -eq 0 ]; then
    echo "  All tests passed!"
else
    echo "  Some tests failed!"
fi

exit 0
