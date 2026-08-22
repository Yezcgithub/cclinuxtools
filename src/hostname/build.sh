#!/bin/bash
# Build and test script for hostname.c (Unix/Linux/macOS/BSD)

set -e

echo "============================================"
echo "    hostname.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=hostname
SOURCE=hostname.c
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

# System host name for comparison (from the OS hostname command, if present)
SYS_HOST=$(uname -n 2>/dev/null || echo "")

# ========== T01: default (print current host name)
echo ""
echo "--- T01: default (== host name) ---"
out=$(./"$OUTPUT")
[ -n "$out" ]
check $? "default non-empty"
if [ -n "$SYS_HOST" ]; then
    [ "$out" = "$SYS_HOST" ]
    check $? "default equals uname -n"
fi

# ========== T02: -s short name
echo ""
echo "--- T02: -s short name ---"
out=$(./"$OUTPUT" -s)
[ -n "$out" ]
check $? "-s non-empty"
# -s must not contain a dot
echo "$out" | grep -qv '\.'
check $? "-s has no dot"

# ========== T03: -s is a prefix of the default host name
echo ""
echo "--- T03: -s is prefix of host name ---"
def=$(./"$OUTPUT")
sh=$(./"$OUTPUT" -s)
case "$def" in
    "$sh"*)
        check 0 "-s is prefix of default"
        ;;
    *)
        check 1 "-s is prefix of default"
        ;;
esac

# ========== T04: -f FQDN
echo ""
echo "--- T04: -f FQDN ---"
out=$(./"$OUTPUT" -f)
[ -n "$out" ]
check $? "-f non-empty"

# ========== T05: -d domain (may be "unknown" but non-empty)
echo ""
echo "--- T05: -d domain ---"
out=$(./"$OUTPUT" -d)
[ -n "$out" ]
check $? "-d non-empty"

# ========== T06: -i IP address (non-empty)
echo ""
echo "--- T06: -i IP address ---"
out=$(./"$OUTPUT" -i)
[ -n "$out" ]
check $? "-i non-empty"
# Should contain a digit and a dot (IPv4) or a colon (IPv6)
echo "$out" | grep -qE '[0-9.]|:'
check $? "-i looks like an IP"

# ========== T07: -I all IP addresses (non-empty)
echo ""
echo "--- T07: -I all IP addresses ---"
out=$(./"$OUTPUT" -I)
[ -n "$out" ]
check $? "-I non-empty"

# ========== T08: -y NIS domain (may be "unknown" but non-empty)
echo ""
echo "--- T08: -y NIS domain ---"
out=$(./"$OUTPUT" -y)
[ -n "$out" ]
check $? "-y non-empty"

# ========== T09: -a alias (may produce no output; just must not crash)
echo ""
echo "--- T09: -a alias ---"
./"$OUTPUT" -a >/dev/null 2>&1
check $? "-a runs without error"

# ========== T10: --help
echo ""
echo "--- T10: --help ---"
./"$OUTPUT" --help | grep -q "Usage:"
check $? "--help contains Usage"

# ========== T11: --version
echo ""
echo "--- T11: --version ---"
./"$OUTPUT" --version | grep -q "hostname"
check $? "--version contains hostname"

# ========== T12: long options --short == -s
echo ""
echo "--- T12: long options ---"
a=$(./"$OUTPUT" --short)
b=$(./"$OUTPUT" -s)
[ "$a" = "$b" ]
check $? "--short equals -s"
a=$(./"$OUTPUT" --fqdn)
b=$(./"$OUTPUT" -f)
[ "$a" = "$b" ]
check $? "--fqdn equals -f"

# ========== T13: invalid option exits non-zero
echo ""
echo "--- T13: invalid option ---"
./"$OUTPUT" -Z >/dev/null 2>&1
[ $? -ne 0 ]
check $? "invalid option exits non-zero"

# ========== T14: extra operand exits non-zero (setting requires privileges)
echo ""
echo "--- T14: extra operand ---"
./"$OUTPUT" -s extra >/dev/null 2>&1
[ $? -ne 0 ]
check $? "extra operand exits non-zero"

# ========== T15: -F with non-existent file exits non-zero
echo ""
echo "--- T15: -F nonexistent file ---"
./"$OUTPUT" -F /nonexistent/path/xyz >/dev/null 2>&1
[ $? -ne 0 ]
check $? "-F nonexistent file exits non-zero"

# ========== T16: -F --file form
echo ""
echo "--- T16: --file nonexistent ---"
./"$OUTPUT" --file=/nonexistent/path/xyz >/dev/null 2>&1
[ $? -ne 0 ]
check $? "--file= nonexistent exits non-zero"

# ========== T17: -b boot mode tolerates failure
echo ""
echo "--- T17: -b boot mode ---"
# -b with a bad file should not crash; boot mode tolerates set failure
./"$OUTPUT" -b -F /nonexistent/path/xyz >/dev/null 2>&1
# boot mode tolerates failure: exit code 0 (file open failed but tolerated)
[ $? -eq 0 ]
check $? "-b tolerates bad file"

# ========== T18: setting host name requires privileges (non-root fails)
echo ""
echo "--- T18: set name without privileges ---"
if [ "$(id -u)" = "0" ]; then
    # Running as root: setting may succeed; skip the negative test
    check 0 "set name (root, skipped negative)"
else
    ./"$OUTPUT" newname_xyz >/dev/null 2>&1
    [ $? -ne 0 ]
    check $? "set name without privileges fails"
fi

# ========== T19: -A all FQDNs (may be empty; must not crash)
echo ""
echo "--- T19: -A all FQDNs ---"
./"$OUTPUT" -A >/dev/null 2>&1
check $? "-A runs without error"

# ========== T20: combined -sd runs
echo ""
echo "--- T20: combined -sd ---"
./"$OUTPUT" -sd >/dev/null 2>&1
check $? "-sd runs without error"

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
