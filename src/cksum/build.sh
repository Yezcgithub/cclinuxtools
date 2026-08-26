#!/bin/bash
# Build and test script for cksum.c (Unix/Linux/macOS/BSD)

# NOTE: Do NOT use 'set -e' — cksum --check returns 1 on FAILED/missing
#       files, which would abort the script on those test cases.

echo "============================================"
echo "    cksum.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT=cksum
SOURCE=cksum.c
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

# ========== T01: CRC (POSIX) abc
echo ""
echo "--- T01: CRC (POSIX) abc ---"
out=$(./"$OUTPUT" abc.txt)
[ "$out" = "1219131554 3 abc.txt" ]
check $? "CRC abc"

# ========== T02: CRC empty file
echo ""
echo "--- T02: CRC empty file ---"
out=$(./"$OUTPUT" empty.txt)
[ "$out" = "4294967295 0 empty.txt" ]
check $? "CRC empty"

# ========== T03: CRC stdin
echo ""
echo "--- T03: CRC stdin ---"
out=$(printf 'abc' | ./"$OUTPUT" -)
[ "$out" = "1219131554 3" ]
check $? "CRC stdin"

# ========== T04: CRC32B abc
echo ""
echo "--- T04: CRC32B abc ---"
out=$(./"$OUTPUT" -a crc32b abc.txt)
[ "$out" = "891568578 3 abc.txt" ]
check $? "CRC32B abc"

# ========== T05: SYSV abc
echo ""
echo "--- T05: SYSV abc ---"
out=$(./"$OUTPUT" -a sysv abc.txt)
[ "$out" = "294 1 abc.txt" ]
check $? "SYSV abc"

# ========== T06: BSD abc
echo ""
echo "--- T06: BSD abc ---"
out=$(./"$OUTPUT" -a bsd abc.txt)
[ "$out" = "16556     1 abc.txt" ]
check $? "BSD abc"

# ========== T07: MD5 abc
echo ""
echo "--- T07: MD5 abc ---"
out=$(./"$OUTPUT" -a md5 abc.txt)
[ "$out" = "MD5 (abc.txt) = 900150983cd24fb0d6963f7d28e17f72" ]
check $? "MD5 abc"

# ========== T08: MD5 empty
echo ""
echo "--- T08: MD5 empty ---"
out=$(./"$OUTPUT" -a md5 empty.txt)
[ "$out" = "MD5 (empty.txt) = d41d8cd98f00b204e9800998ecf8427e" ]
check $? "MD5 empty"

# ========== T09: SHA1 abc
echo ""
echo "--- T09: SHA1 abc ---"
out=$(./"$OUTPUT" -a sha1 abc.txt)
[ "$out" = "SHA1 (abc.txt) = a9993e364706816aba3e25717850c26c9cd0d89d" ]
check $? "SHA1 abc"

# ========== T10: SHA224 abc
echo ""
echo "--- T10: SHA224 abc ---"
out=$(./"$OUTPUT" -a sha224 abc.txt)
[ "$out" = "SHA224 (abc.txt) = 23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7" ]
check $? "SHA224 abc"

# ========== T11: SHA256 abc
echo ""
echo "--- T11: SHA256 abc ---"
out=$(./"$OUTPUT" -a sha256 abc.txt)
[ "$out" = "SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "SHA256 abc"

# ========== T12: SHA256 empty
echo ""
echo "--- T12: SHA256 empty ---"
out=$(./"$OUTPUT" -a sha256 empty.txt)
[ "$out" = "SHA256 (empty.txt) = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ]
check $? "SHA256 empty"

# ========== T13: SHA384 abc
echo ""
echo "--- T13: SHA384 abc ---"
out=$(./"$OUTPUT" -a sha384 abc.txt)
[ "$out" = "SHA384 (abc.txt) = cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7" ]
check $? "SHA384 abc"

# ========== T14: SHA512 abc
echo ""
echo "--- T14: SHA512 abc ---"
out=$(./"$OUTPUT" -a sha512 abc.txt)
[ "$out" = "SHA512 (abc.txt) = ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" ]
check $? "SHA512 abc"

# ========== T15: SHA512 empty
echo ""
echo "--- T15: SHA512 empty ---"
out=$(./"$OUTPUT" -a sha512 empty.txt)
[ "$out" = "SHA512 (empty.txt) = cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" ]
check $? "SHA512 empty"

# ========== T16: BLAKE2b abc
echo ""
echo "--- T16: BLAKE2b abc ---"
out=$(./"$OUTPUT" -a blake2b abc.txt)
[ "$out" = "BLAKE2b (abc.txt) = ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" ]
check $? "BLAKE2b abc"

# ========== T17: BLAKE2b empty
echo ""
echo "--- T17: BLAKE2b empty ---"
out=$(./"$OUTPUT" -a blake2b empty.txt)
[ "$out" = "BLAKE2b (empty.txt) = 786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce" ]
check $? "BLAKE2b empty"

# ========== T18: BLAKE2b -l 256 abc
echo ""
echo "--- T18: BLAKE2b -l 256 abc ---"
out=$(./"$OUTPUT" -a blake2b -l 256 abc.txt)
[ "$out" = "BLAKE2b (abc.txt) = bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319" ]
check $? "BLAKE2b 256 abc"

# ========== T19: --untagged SHA256
echo ""
echo "--- T19: --untagged SHA256 ---"
out=$(./"$OUTPUT" -a sha256 --untagged abc.txt)
[ "$out" = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt" ]
check $? "untagged"

# ========== T20: --base64 SHA256
echo ""
echo "--- T20: --base64 SHA256 ---"
out=$(./"$OUTPUT" -a sha256 --base64 abc.txt)
[ "$out" = "SHA256 (abc.txt) = ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=" ]
check $? "base64"

# ========== T21: --check tagged OK
echo ""
echo "--- T21: --check tagged OK ---"
printf 'SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n' > check_ok.txt
out=$(./"$OUTPUT" -a sha256 --check check_ok.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check tagged OK"

# ========== T22: --check untagged OK
echo ""
echo "--- T22: --check untagged OK ---"
printf 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt\n' > check_untag.txt
out=$(./"$OUTPUT" -a sha256 --check check_untag.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check untagged OK"

# ========== T23: --check FAILED
echo ""
echo "--- T23: --check FAILED ---"
printf 'SHA256 (abc.txt) = 0000000000000000000000000000000000000000000000000000000000000000\n' > check_bad.txt
out=$(./"$OUTPUT" -a sha256 --check check_bad.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: FAILED" ] && [ $code -eq 1 ]
check $? "check FAILED"

# ========== T24: --check --status OK
echo ""
echo "--- T24: --check --status OK ---"
./"$OUTPUT" -a sha256 --check --status check_ok.txt >/dev/null 2>&1
[ $? -eq 0 ]
check $? "check status OK"

# ========== T25: --check --status FAILED
echo ""
echo "--- T25: --check --status FAILED ---"
./"$OUTPUT" -a sha256 --check --status check_bad.txt >/dev/null 2>&1
[ $? -eq 1 ]
check $? "check status FAILED"

# ========== T26: --check --quiet
echo ""
echo "--- T26: --check --quiet ---"
out=$(./"$OUTPUT" -a sha256 --check --quiet check_ok.txt 2>/dev/null)
code=$?
[ -z "$out" ] && [ $code -eq 0 ]
check $? "check quiet"

# ========== T27: multiple files
echo ""
echo "--- T27: multiple files ---"
out=$(./"$OUTPUT" -a md5 abc.txt empty.txt)
echo "$out" | grep -q '900150983cd24fb0d6963f7d28e17f72' && echo "$out" | grep -q 'd41d8cd98f00b204e9800998ecf8427e'
check $? "multiple files"

# ========== T28: --help exits 0
echo ""
echo "--- T28: --help exits 0 ---"
out=$(./"$OUTPUT" --help 2>/dev/null)
code=$?
[ $code -eq 0 ] && echo "$out" | grep -q 'Usage:'
check $? "help"

# ========== T29: --version exits 0
echo ""
echo "--- T29: --version exits 0 ---"
out=$(./"$OUTPUT" --version 2>/dev/null)
code=$?
[ $code -eq 0 ] && echo "$out" | grep -q 'cksum'
check $? "version"

# ========== T30: big file CRC
echo ""
echo "--- T30: big file CRC ---"
out=$(./"$OUTPUT" big.bin)
echo "$out" | grep -q ' 100000 '
check $? "big file size"

# ========== T31: --check --ignore-missing
echo ""
echo "--- T31: --check --ignore-missing ---"
printf 'SHA256 (nonexistent.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n' > check_missing.txt
./"$OUTPUT" -a sha256 --check --ignore-missing check_missing.txt >/dev/null 2>&1
[ $? -eq 0 ]
check $? "ignore-missing"

# ========== T32: --check missing file fails
echo ""
echo "--- T32: --check missing file fails ---"
./"$OUTPUT" -a sha256 --check check_missing.txt >/dev/null 2>&1
[ $? -ne 0 ]
check $? "missing file fails"

# ========== T33: CRC large file consistency
echo ""
echo "--- T33: CRC large file consistency ---"
out1=$(./"$OUTPUT" big.bin)
out2=$(./"$OUTPUT" big.bin)
[ "$out1" = "$out2" ]
check $? "CRC consistency"

# ========== T34: --algorithm=crc syntax
echo ""
echo "--- T34: --algorithm=crc syntax ---"
out=$(./"$OUTPUT" --algorithm=crc abc.txt)
[ "$out" = "1219131554 3 abc.txt" ]
check $? "algorithm=crc"

# ========== T35: --algorithm sha256 syntax
echo ""
echo "--- T35: --algorithm sha256 syntax ---"
out=$(./"$OUTPUT" --algorithm sha256 abc.txt)
[ "$out" = "SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "algorithm sha256"

# ========== T36: MD5 --base64
echo ""
echo "--- T36: MD5 --base64 ---"
out=$(./"$OUTPUT" -a md5 --base64 abc.txt)
[ "$out" = "MD5 (abc.txt) = kAFQmDzST7DWlj99KOF/cg==" ]
check $? "MD5 base64"

# ========== T37: BLAKE2b -l 128
echo ""
echo "--- T37: BLAKE2b -l 128 ---"
out=$(./"$OUTPUT" -a blake2b -l 128 abc.txt)
case "$out" in
    "BLAKE2b (abc.txt) = "*) [ ${#out} -gt 40 ]; check $? "BLAKE2b 128" ;;
    *) check 1 "BLAKE2b 128" ;;
esac

# ========== T38: --check base64 tagged
echo ""
echo "--- T38: --check base64 tagged ---"
printf 'SHA256 (abc.txt) = ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=\n' > check_b64.txt
out=$(./"$OUTPUT" -a sha256 --check check_b64.txt 2>/dev/null)
code=$?
[ "$out" = "abc.txt: OK" ] && [ $code -eq 0 ]
check $? "check base64"

# ========== T39: CRC32B empty
echo ""
echo "--- T39: CRC32B empty ---"
out=$(./"$OUTPUT" -a crc32b empty.txt)
[ "$out" = "0 0 empty.txt" ]
check $? "CRC32B empty"

# ========== T40: SHA256 stdin
echo ""
echo "--- T40: SHA256 stdin ---"
out=$(printf 'abc' | ./"$OUTPUT" -a sha256 -)
[ "$out" = "SHA256 = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]
check $? "SHA256 stdin"

# Cleanup
rm -f abc.txt empty.txt big.bin check_ok.txt check_untag.txt check_bad.txt check_missing.txt check_b64.txt

echo ""
echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi
