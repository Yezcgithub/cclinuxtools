#!/bin/bash
# Build and test script for cut.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    cut.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=cut
SOURCE=cut.c
PASS=0
FAIL=0

PLATFORM="unknown"
if [ "$(uname -s)" = "Linux" ]; then PLATFORM="linux"
elif [ "$(uname -s)" = "Darwin" ]; then PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then PLATFORM="netbsd"; fi

echo ""; echo "Detected platform: $PLATFORM"
echo ""; echo "[1/3] Cleaning previous build..."; rm -f "$OUTPUT"
echo ""; echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS -o $OUTPUT $SOURCE"; echo ""
$CC $CFLAGS -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then echo ""; echo "[ERROR] Build failed"; exit 1; fi
echo ""; echo "[3/3] Build succeeded!"; echo "  Output: $(pwd)/$OUTPUT"
echo ""; echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then echo "  [PASS] $2"; PASS=$((PASS + 1))
    else echo "  [FAIL] $2"; FAIL=$((FAIL + 1)); fi
}

# Create test files
printf 'abcdefgh\n' > t_bytes.txt
printf 'a:b:c:d:e\n1:2:3:4:5\nhello:world:foo:bar:baz\n' > t_colon.txt
printf '  alpha  beta  gamma  delta\none two three four five\n  x   y   z\n' > t_ws.txt

# ========== T01: -b 1-3
echo ""; echo "--- T01: -b 1-3 ---"
out=$(./"$OUTPUT" -b 1-3 t_bytes.txt)
[ "$out" = "abc" ]; check $? "byte range 1-3"

# ========== T02: -b 1-3,5-7
echo ""; echo "--- T02: -b 1-3,5-7 ---"
out=$(./"$OUTPUT" -b 1-3,5-7 t_bytes.txt)
[ "$out" = "abcefg" ]; check $? "byte ranges 1-3,5-7"

# ========== T03: -b 1-3,5-7 --output-delimiter=/
echo ""; echo "--- T03: -b with output delimiter ---"
out=$(./"$OUTPUT" -b 1-3,5-7 --output-delimiter=/ t_bytes.txt)
[ "$out" = "abc/efg" ]; check $? "byte output delimiter"

# ========== T04: -b --complement
echo ""; echo "--- T04: -b 1-3,5-7 --complement ---"
out=$(./"$OUTPUT" -b 1-3,5-7 --complement t_bytes.txt)
[ "$out" = "dh" ]; check $? "byte complement"

# ========== T05: -f 1,3 -d:
echo ""; echo "--- T05: -f 1,3 -d: ---"
out=$(./"$OUTPUT" -f 1,3 -d: t_colon.txt | head -1)
[ "$out" = "a:c" ]; check $? "field select 1,3"

# ========== T06: -f 2- -d:
echo ""; echo "--- T06: -f 2- -d: ---"
out=$(./"$OUTPUT" -f 2- -d: t_colon.txt | head -1)
[ "$out" = "b:c:d:e" ]; check $? "field range 2-"

# ========== T07: -f -2 -d: --output-delimiter=-
echo ""; echo "--- T07: -f -2 with output delim ---"
out=$(./"$OUTPUT" -f -2 -d: --output-delimiter=- t_colon.txt | head -1)
[ "$out" = "a-b" ]; check $? "field range -2 with output delim"

# ========== T08: -f 1,3 --complement
echo ""; echo "--- T08: -f 1,3 --complement ---"
out=$(./"$OUTPUT" -f 1,3 -d: --complement t_colon.txt | head -1)
[ "$out" = "b:d:e" ]; check $? "field complement"

# ========== T09: -c 2-4
echo ""; echo "--- T09: -c 2-4 ---"
out=$(./"$OUTPUT" -c 2-4 t_bytes.txt)
[ "$out" = "bcd" ]; check $? "char range 2-4"

# ========== T10: -F 1 (whitespace)
echo ""; echo "--- T10: -F 1 (whitespace) ---"
out=$(./"$OUTPUT" -F 1 t_ws.txt | sed -n '2p')
[ "$out" = "one" ]; check $? "whitespace field 1"

# ========== T11: -F 1,3 (whitespace)
echo ""; echo "--- T11: -F 1,3 (whitespace) ---"
out=$(./"$OUTPUT" -F 1,3 t_ws.txt | sed -n '2p')
[ "$out" = "one three" ]; check $? "whitespace fields 1,3"

# ========== T12: --whitespace-delimited=trimmed -f 1
echo ""; echo "--- T12: trimmed whitespace -f 1 ---"
out=$(./"$OUTPUT" --whitespace-delimited=trimmed -f 1 t_ws.txt | sed -n '1p')
[ "$out" = "alpha" ]; check $? "trimmed whitespace field 1"

# ========== T13: -w -f 1
echo ""; echo "--- T13: -w -f 1 ---"
out=$(./"$OUTPUT" -w -f 1 t_ws.txt | sed -n '2p')
[ "$out" = "one" ]; check $? "whitespace delimited field 1"

# ========== T14: -s (only delimited)
echo ""; echo "--- T14: -s only delimited ---"
printf 'no_delim_here\na:b:c\n' > t_s.txt
out=$(./"$OUTPUT" -f 1 -d: -s t_s.txt)
[ "$out" = "a" ]; check $? "only delimited"

# ========== T15: without -s (print undelimited)
echo ""; echo "--- T15: without -s prints undelimited ---"
out=$(./"$OUTPUT" -f 1 -d: t_s.txt)
echo "$out" | grep -q 'no_delim_here' && echo "$out" | grep -q 'a'
check $? "print undelimited lines"

# ========== T16: -O (output delimiter)
echo ""; echo "--- T16: -O output delimiter ---"
out=$(./"$OUTPUT" -f 1,3 -d: -O SEP t_colon.txt | head -1)
[ "$out" = "aSEPc" ]; check $? "output delimiter string"

# ========== T17: stdin
echo ""; echo "--- T17: stdin ---"
out=$(printf 'abc\n' | ./"$OUTPUT" -b 1-2 -)
[ "$out" = "ab" ]; check $? "stdin input"

# ========== T18: -z (zero terminated)
echo ""; echo "--- T18: -z zero terminated ---"
printf 'abc\0def\0' | ./"$OUTPUT" -b 1 -z > t_z.out
# Check that output contains NUL
if grep -qP '\x00' t_z.out 2>/dev/null; then
    check 0 "zero terminated"
else
    # Fallback: check with od
    od_output=$(od -c t_z.out 2>/dev/null | head -1)
    echo "$od_output" | grep -q '\\0' && check 0 "zero terminated" || check 1 "zero terminated"
fi
rm -f t_z.out

# ========== T19: -b with open range
echo ""; echo "--- T19: -b 3- ---"
out=$(./"$OUTPUT" -b 3- t_bytes.txt)
[ "$out" = "cdefgh" ]; check $? "byte open range 3-"

# ========== T20: -b -3
echo ""; echo "--- T20: -b -3 ---"
out=$(./"$OUTPUT" -b -3 t_bytes.txt)
[ "$out" = "abc" ]; check $? "byte open range -3"

# ========== T21: -n flag (no-op)
echo ""; echo "--- T21: -n (no-op) ---"
out=$(./"$OUTPUT" -b 1-3 -n t_bytes.txt)
[ "$out" = "abc" ]; check $? "no-partial flag"

# ========== T22: --help exits 0
echo ""; echo "--- T22: --help ---"
./"$OUTPUT" --help >/dev/null 2>&1; [ $? -eq 0 ]; check $? "help exits 0"

# ========== T23: --version exits 0
echo ""; echo "--- T23: --version ---"
./"$OUTPUT" --version >/dev/null 2>&1; [ $? -eq 0 ]; check $? "version exits 0"

# ========== T24: no option fails
echo ""; echo "--- T24: no option ---"
./"$OUTPUT" t_bytes.txt >/dev/null 2>&1; [ $? -ne 0 ]; check $? "no option fails"

# ========== T25: -b with --complement open range
echo ""; echo "--- T25: -b 3- --complement ---"
out=$(./"$OUTPUT" -b 3- --complement t_bytes.txt)
[ "$out" = "ab" ]; check $? "complement open range"

# ========== T26: multiple files
echo ""; echo "--- T26: multiple files ---"
out=$(./"$OUTPUT" -b 1 t_bytes.txt t_bytes.txt)
echo "$out" | wc -l | grep -q '2'; check $? "multiple files"

# ========== T27: -f 1-5 -d: (all fields)
echo ""; echo "--- T27: -f 1-5 -d: ---"
out=$(./"$OUTPUT" -f 1-5 -d: t_colon.txt | head -1)
[ "$out" = "a:b:c:d:e" ]; check $? "all fields"

# ========== T28: -O '' (empty output delimiter)
echo ""; echo "--- T28: -O '' empty output delim ---"
out=$(./"$OUTPUT" -f 1,3 -d: --output-delimiter= t_colon.txt | head -1)
[ "$out" = "ac" ]; check $? "empty output delimiter"

# ========== T29: -b 1,1 (duplicate position)
echo ""; echo "--- T29: -b 1,1 duplicate ---"
out=$(./"$OUTPUT" -b 1,1 t_bytes.txt)
[ "$out" = "a" ]; check $? "duplicate position"

# ========== T30: --bytes=N syntax
echo ""; echo "--- T30: --bytes= syntax ---"
out=$(./"$OUTPUT" --bytes=1-3 t_bytes.txt)
[ "$out" = "abc" ]; check $? "long option bytes syntax"

# Cleanup
rm -f t_bytes.txt t_colon.txt t_ws.txt t_s.txt

echo ""; echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then exit 1; else exit 0; fi
