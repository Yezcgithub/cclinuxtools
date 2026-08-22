#!/bin/bash
# Build and test script for uname.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    uname.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=uname
SOURCE=uname.c
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
echo "Detected platform: $PLATFORM"

EXTRA_FLAGS=""
case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE" ;;
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

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

echo ""
echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then
        echo "  [PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

# Expected OS string per platform (for -o verification)
case "$PLATFORM" in
    linux)   EXP_OS="GNU/Linux" ;;
    macos)   EXP_OS="Darwin" ;;
    freebsd) EXP_OS="FreeBSD" ;;
    openbsd) EXP_OS="OpenBSD" ;;
    netbsd)  EXP_OS="NetBSD" ;;
    *)       EXP_OS="" ;;
esac

# Expected kernel-name per platform (for -s verification)
case "$PLATFORM" in
    linux)   EXP_SYS="Linux" ;;
    macos)   EXP_SYS="Darwin" ;;
    freebsd) EXP_SYS="FreeBSD" ;;
    openbsd) EXP_SYS="OpenBSD" ;;
    netbsd)  EXP_SYS="NetBSD" ;;
    *)       EXP_SYS="" ;;
esac

# ========== T01: default (no option == -s)
echo ""
echo "--- T01: default (== -s) ---"
out=$(./"$OUTPUT")
[ -n "$out" ]
check $? "default non-empty"

# ========== T02: -s kernel name
echo ""
echo "--- T02: -s kernel name ---"
out=$(./"$OUTPUT" -s)
[ -n "$out" ]
check $? "-s non-empty"
if [ -n "$EXP_SYS" ]; then
    [ "$out" = "$EXP_SYS" ]
    check $? "-s equals $EXP_SYS"
fi

# ========== T03: -n nodename
echo ""
echo "--- T03: -n nodename ---"
out=$(./"$OUTPUT" -n)
[ -n "$out" ]
check $? "-n non-empty"
# nodename should match system hostname (uname -n)
[ "$out" = "$(uname -n)" ]
check $? "-n matches system hostname"

# ========== T04: -r kernel release
echo ""
echo "--- T04: -r kernel release ---"
out=$(./"$OUTPUT" -r)
[ -n "$out" ]
check $? "-r non-empty"
[ "$out" = "$(uname -r)" ]
check $? "-r matches system release"

# ========== T05: -v kernel version
echo ""
echo "--- T05: -v kernel version ---"
out=$(./"$OUTPUT" -v)
[ -n "$out" ]
check $? "-v non-empty"
[ "$out" = "$(uname -v)" ]
check $? "-v matches system version"

# ========== T06: -m machine
echo ""
echo "--- T06: -m machine ---"
out=$(./"$OUTPUT" -m)
[ -n "$out" ]
check $? "-m non-empty"
[ "$out" = "$(uname -m)" ]
check $? "-m matches system machine"

# ========== T07: -o operating system
echo ""
echo "--- T07: -o operating system ---"
out=$(./"$OUTPUT" -o)
[ -n "$out" ]
check $? "-o non-empty"
if [ -n "$EXP_OS" ]; then
    [ "$out" = "$EXP_OS" ]
    check $? "-o equals $EXP_OS"
fi

# ========== T08: -a all information
echo ""
echo "--- T08: -a all information ---"
out=$(./"$OUTPUT" -a)
[ -n "$out" ]
check $? "-a non-empty"
# -a should contain the kernel name as the first token
echo "$out" | grep -q "^$EXP_SYS"
check $? "-a starts with kernel name"
# -a should contain at least 5 space-separated fields (snrvm...o)
fields=$(echo "$out" | wc -w)
[ "$fields" -ge 5 ]
check $? "-a has >=5 fields (got $fields)"

# ========== T09: -srm combined short options
echo ""
echo "--- T09: -srm combined ---"
out=$(./"$OUTPUT" -srm)
fields=$(echo "$out" | wc -w)
[ "$fields" -eq 3 ]
check $? "-srm has 3 fields (got $fields)"

# ========== T10: -sr -m equivalent (order check)
echo ""
echo "--- T10: -sr vs -s -r ---"
a=$(./"$OUTPUT" -sr)
b=$(./"$OUTPUT" -s -r)
[ "$a" = "$b" ]
check $? "-sr equals -s -r"

# ========== T11: --help
echo ""
echo "--- T11: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help contains Usage"

# ========== T12: --version
echo ""
echo "--- T12: --version ---"
./"$OUTPUT" --version | grep -q "uname"
check $? "--version contains uname"

# ========== T13: long options --kernel-name / --machine
echo ""
echo "--- T13: long options ---"
a=$(./"$OUTPUT" --kernel-name)
b=$(./"$OUTPUT" -s)
[ "$a" = "$b" ]
check $? "--kernel-name equals -s"
a=$(./"$OUTPUT" --machine)
b=$(./"$OUTPUT" -m)
[ "$a" = "$b" ]
check $? "--machine equals -m"

# ========== T14: -p processor (may be unknown but non-empty)
echo ""
echo "--- T14: -p processor ---"
out=$(./"$OUTPUT" -p)
[ -n "$out" ]
check $? "-p non-empty"

# ========== T15: -i hardware platform
echo ""
echo "--- T15: -i hardware platform ---"
out=$(./"$OUTPUT" -i)
[ -n "$out" ]
check $? "-i non-empty"
# On most POSIX systems -i equals -m
a=$(./"$OUTPUT" -i)
b=$(./"$OUTPUT" -m)
[ "$a" = "$b" ]
check $? "-i equals -m (POSIX)"

# ========== T16: invalid option exits non-zero
echo ""
echo "--- T16: invalid option ---"
./"$OUTPUT" -Z >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid option exits non-zero"

# ========== T17: extra operand exits non-zero
echo ""
echo "--- T17: extra operand ---"
./"$OUTPUT" -s extra >/dev/null 2>&1
[ $? -ne 0 ]
check $? "extra operand exits non-zero"

# ========== T18: -a output order = snrvmio (verify first 5 tokens)
echo ""
echo "--- T18: -a field order ---"
out=$(./"$OUTPUT" -a)
s=$(echo "$out" | awk '{print $1}')
r=$(echo "$out" | awk '{print $3}')
m=$(echo "$out" | awk '{print $5}')
[ "$s" = "$(./"$OUTPUT" -s)" ]
check $? "-a field 1 == -s"
[ "$r" = "$(./"$OUTPUT" -r)" ]
check $? "-a field 3 == -r"
[ "$m" = "$(./"$OUTPUT" -m)" ]
check $? "-a field 5 == -m"

# ========== T19: -o last field of -a
echo ""
echo "--- T19: -a last field == -o ---"
out=$(./"$OUTPUT" -a)
last=$(echo "$out" | awk '{print $NF}')
o=$(./"$OUTPUT" -o)
[ "$last" = "$o" ]
check $? "-a last field == -o"

# ========== T20: -asnrvmpio long+short mix
echo ""
echo "--- T20: -asnrvmpio mix ---"
out=$(./"$OUTPUT" -asnrvmpio)
[ -n "$out" ]
check $? "-asnrvmpio non-empty"

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
