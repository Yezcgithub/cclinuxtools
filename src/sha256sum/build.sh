#!/bin/bash
# Build and test script for sha256sum.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — sha256sum -c returns 1 on FAILED/missing
#       files, which would abort the script on those test cases.

echo "============================================"
echo "    sha256sum.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=sha256sum
SOURCE=sha256sum.c
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

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"

echo ""
echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed"
    exit 1
fi

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

# Create test files
printf 'abc' > abc.txt
: > empty.txt
awk 'BEGIN{for(i=0;i<100000;i++)printf "%c",i%256}' > big.bin

# ========== T01: abc default
echo ""
echo "--- T01: abc default (untagged text mode) ---"
out=$(./"$OUTPUT" abc.txt)
[ "$out" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt" ]
check $? "abc default"

# ========== T02: empty file
echo ""
echo "--- T02: empty file ---"
out=$(./"$OUTPUT" empty.txt)
[ "$out" = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  empty.txt" ]
check $? "empty default"

# ========== T03: stdin
echo ""
echo "--- T03: stdin ---"
out=$(printf 'abc' | ./"$OUTPUT" -)
[ "$out" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  -" ]
check $? "stdin"

# ========== T04: -b binary mode
echo ""
echo "--- T04: -b binary mode ---"
out=$(./"$OUTPUT" -b abc.txt)
[ "$out" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad *abc.txt" ]
check $? "binary mode"

# ========== T05: --tag mode
echo ""
echo "--- T05: --tag mode ---"
out=$(./"$OUTPUT" --tag abc.txt)
[ "$out" = "SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "tag mode"

# ========== T06: --tag stdin
echo ""
echo "--- T06: --tag stdin ---"
out=$(printf 'abc' | ./"$OUTPUT" --tag -)
[ "$out" = "SHA256 (stdin) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "tag stdin"

# ========== T07: --check untagged OK
echo ""
echo "--- T07: --check untagged OK ---"
printf 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt\n' > check_ok.txt
out=$(./"$OUTPUT" -c check_ok.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check untagged OK"

# ========== T08: --check tagged OK
echo ""
echo "--- T08: --check tagged OK ---"
printf 'SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n' > check_tag.txt
out=$(./"$OUTPUT" -c check_tag.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check tagged OK"

# ========== T09: --check FAILED
echo ""
echo "--- T09: --check FAILED ---"
printf '0000000000000000000000000000000000000000000000000000000000000000  abc.txt\n' > check_bad.txt
out=$(./"$OUTPUT" -c check_bad.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: FAILED" ] && [ $code -eq 1 ]
check $? "check FAILED"

# ========== T10: --check --status OK
echo ""
echo "--- T10: --check --status OK ---"
./"$OUTPUT" -c --status check_ok.txt >/dev/null 2>&1
[ $? -eq 0 ]
check $? "check status OK"

# ========== T11: --check --status FAILED
echo ""
echo "--- T11: --check --status FAILED ---"
./"$OUTPUT" -c --status check_bad.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "check status FAILED"

# ========== T12: --check --quiet OK
echo ""
echo "--- T12: --check --quiet OK ---"
out=$(./"$OUTPUT" -c --quiet check_ok.txt 2>/dev/null)
code=$?
[ -z "$out" ] && [ $code -eq 0 ]
check $? "check quiet OK"

# ========== T13: --check --ignore-missing
echo ""
echo "--- T13: --check --ignore-missing ---"
printf 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  nonexistent.txt\n' > check_missing.txt
./"$OUTPUT" -c --ignore-missing check_missing.txt >/dev/null 2>&1
[ $? -eq 0 ]
check $? "ignore-missing"

# ========== T14: --check missing file fails
echo ""
echo "--- T14: --check missing file fails ---"
./"$OUTPUT" -c check_missing.txt >/dev/null 2>&1
[ $? -ne 0 ]
check $? "missing file fails"

# ========== T15: multiple files
echo ""
echo "--- T15: multiple files ---"
out=$(./"$OUTPUT" abc.txt empty.txt)
echo "$out" | grep -q 'ba7816bf' && echo "$out" | grep -q 'e3b0c442'
check $? "multiple files"

# ========== T16: --help
echo ""
echo "--- T16: --help exits 0 ---"
out=$(./"$OUTPUT" --help 2>/dev/null)
code=$?
[ $code -eq 0 ] && echo "$out" | grep -q 'Usage:'
check $? "help"

# ========== T17: --version
echo ""
echo "--- T17: --version exits 0 ---"
out=$(./"$OUTPUT" --version 2>/dev/null)
code=$?
[ $code -eq 0 ] && echo "$out" | grep -q 'sha256sum'
check $? "version"

# ========== T18: big file
echo ""
echo "--- T18: big file ---"
out=$(./"$OUTPUT" big.bin)
echo "$out" | grep -q 'big.bin'
check $? "big file exists"

# ========== T19: big file consistency
echo ""
echo "--- T19: big file consistency ---"
out1=$(./"$OUTPUT" big.bin)
out2=$(./"$OUTPUT" big.bin)
[ "$out1" = "$out2" ]
check $? "big file consistency"

# ========== T20: --check binary format
echo ""
echo "--- T20: --check binary format (with *) ---"
printf 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad *abc.txt\n' > check_bin.txt
out=$(./"$OUTPUT" -c check_bin.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check binary format"

# ========== T21: -w warn on bad line
echo ""
echo "--- T21: -w warn on bad line ---"
printf 'not_a_valid_line\nba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt\n' > check_warn.txt
err=$(./"$OUTPUT" -c -w check_warn.txt 2>&1 1>/dev/null)
code=$?
echo "$err" | grep -q 'improperly formatted' && [ $code -eq 0 ]
check $? "warn bad line"

# ========== T22: --strict bad line fails
echo ""
echo "--- T22: --strict bad line fails ---"
./"$OUTPUT" -c --strict check_warn.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "strict bad line fails"

# ========== T23: -t text mode
echo ""
echo "--- T23: -t text mode (explicit) ---"
out=$(./"$OUTPUT" -t abc.txt)
[ "$out" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt" ]
check $? "text mode"

# ========== T24: --check empty checkfile fails
echo ""
echo "--- T24: --check empty checkfile fails ---"
: > check_empty.txt
./"$OUTPUT" -c check_empty.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "empty checkfile fails"

# ========== T25: --check no valid lines
echo ""
echo "--- T25: --check no valid lines ---"
printf 'invalid line 1\ninvalid line 2\n' > check_novalid.txt
err=$(./"$OUTPUT" -c check_novalid.txt 2>&1 1>/dev/null)
code=$?
[ $code -eq 1 ] && echo "$err" | grep -q 'no properly formatted'
check $? "no valid lines"

# ========== T26: --tag with -b
echo ""
echo "--- T26: --tag with -b (tag overrides binary display) ---"
out=$(./"$OUTPUT" --tag -b abc.txt)
[ "$out" = "SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "tag + binary"

# ========== T27: --zero output
echo ""
echo "--- T27: --zero output ---"
out=$(./"$OUTPUT" --zero abc.txt | tr '\0' 'Z')
case "$out" in
    *Z*) check 0 "zero output" ;;
    *)   check 1 "zero output" ;;
esac

# ========== T28: unrecognized option fails
echo ""
echo "--- T28: unrecognized option fails ---"
err=$(./"$OUTPUT" --foobar 2>&1)
code=$?
[ $code -ne 0 ] && echo "$err" | grep -q 'unrecognized'
check $? "unrecognized option"

# ========== T29: nonexistent file fails
echo ""
echo "--- T29: nonexistent file fails ---"
err=$(./"$OUTPUT" nonexistent.txt 2>&1)
code=$?
[ $code -ne 0 ] && echo "$err" | grep -q 'No such file'
check $? "nonexistent file"

# ========== T30: --check --quiet FAILED still prints FAILED
echo ""
echo "--- T30: --check --quiet FAILED still prints FAILED ---"
out=$(./"$OUTPUT" -c --quiet check_bad.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: FAILED" ] && [ $code -eq 1 ]
check $? "quiet FAILED prints"

# Cleanup
rm -f abc.txt empty.txt big.bin check_ok.txt check_tag.txt check_bad.txt check_missing.txt check_bin.txt check_warn.txt check_empty.txt check_novalid.txt

echo ""
echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi
