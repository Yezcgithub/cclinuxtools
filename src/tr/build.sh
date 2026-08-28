#!/bin/bash
# Build and test script for tr.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    tr.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=tr
SOURCE=tr.c
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

# ========== T01: basic translation
echo ""; echo "--- T01: basic translation ---"
out=$(echo "abc" | ./"$OUTPUT" 'abc' 'xyz')
[ "$out" = "xyz" ]; check $? "basic translation"

# ========== T02: range translation
echo ""; echo "--- T02: range translation ---"
out=$(echo "hello" | ./"$OUTPUT" 'a-z' 'A-Z')
[ "$out" = "HELLO" ]; check $? "range a-z to A-Z"

# ========== T03: delete chars
echo ""; echo "--- T03: delete chars ---"
out=$(echo "hello123" | ./"$OUTPUT" -d '0-9')
[ "$out" = "hello" ]; check $? "delete digits"

# ========== T04: squeeze repeats
echo ""; echo "--- T04: squeeze repeats ---"
out=$(echo "aaabbbccc" | ./"$OUTPUT" -s 'abc')
[ "$out" = "abc" ]; check $? "squeeze repeats"

# ========== T05: complement
echo ""; echo "--- T05: complement ---"
out=$(echo "abc123" | ./"$OUTPUT" -c -d '0-9')
[ "$out" = "123" ]; check $? "complement delete"

# ========== T06: translate with complement
echo ""; echo "--- T06: complement translate ---"
out=$(printf 'a1b2c3' | ./"$OUTPUT" -c 'a-z' 'X')
[ "$out" = "aXbXcX" ]; check $? "complement translate"

# ========== T07: truncate set1
echo ""; echo "--- T07: truncate set1 ---"
out=$(echo "abcdef" | ./"$OUTPUT" -t 'abcde' 'xyz')
[ "$out" = "xyzdef" ]; check $? "truncate set1"

# ========== T08: set2 extended by last char
echo ""; echo "--- T08: set2 extended ---"
out=$(echo "abcde" | ./"$OUTPUT" 'abcde' 'xy')
[ "$out" = "xyyyy" ]; check $? "set2 extended by last char"

# ========== T09: character class lower to upper
echo ""; echo "--- T09: lower to upper ---"
out=$(echo "hello" | ./"$OUTPUT" '[:lower:]' '[:upper:]')
[ "$out" = "HELLO" ]; check $? "lower to upper class"

# ========== T10: delete with class
echo ""; echo "--- T10: delete with class ---"
out=$(echo "hello world 123" | ./"$OUTPUT" -d '[:digit:]')
[ "$out" = "hello world " ]; check $? "delete digit class"

# ========== T11: squeeze with class
echo ""; echo "--- T11: squeeze with class ---"
out=$(echo "hello   world" | ./"$OUTPUT" -s '[:space:]')
[ "$out" = "hello world" ]; check $? "squeeze space class"

# ========== T12: escape sequences
echo ""; echo "--- T12: escape sequences ---"
out=$(printf 'a\tb' | ./"$OUTPUT" '\t' ' ')
[ "$out" = "a b" ]; check $? "tab to space escape"

# ========== T13: octal escape
echo ""; echo "--- T13: octal escape ---"
out=$(printf 'a\tb' | ./"$OUTPUT" '\011' ' ')
[ "$out" = "a b" ]; check $? "octal tab to space"

# ========== T14: delete + squeeze
echo ""; echo "--- T14: delete + squeeze ---"
out=$(echo "aaabbbccc123" | ./"$OUTPUT" -ds '0-9' 'abc')
[ "$out" = "abc" ]; check $? "delete and squeeze"

# ========== T15: translate + squeeze
echo ""; echo "--- T15: translate + squeeze ---"
out=$(echo "aaabbb" | ./"$OUTPUT" -s 'ab' 'xy')
[ "$out" = "xy" ]; check $? "translate and squeeze"

# ========== T16: backslash escape
echo ""; echo "--- T16: backslash escape ---"
out=$(printf 'a\\b' | ./"$OUTPUT" '\\' '/')
[ "$out" = "a/b" ]; check $? "backslash to slash"

# ========== T17: bell escape
echo ""; echo "--- T17: bell escape ---"
out=$(printf '\a' | ./"$OUTPUT" '\a' 'B')
[ "$out" = "B" ]; check $? "bell escape"

# ========== T18: newline escape
echo ""; echo "--- T18: newline escape ---"
out=$(printf 'a\nb' | ./"$OUTPUT" '\n' '_')
[ "$out" = "a_b" ]; check $? "newline to underscore"

# ========== T19: [:alnum:] class
echo ""; echo "--- T19: alnum class ---"
out=$(echo "a1!b2@" | ./"$OUTPUT" -d '[:alnum:]')
[ "$out" = "!@" ]; check $? "delete alnum"

# ========== T20: [:punct:] class
echo ""; echo "--- T20: punct class ---"
out=$(echo "hello, world!" | ./"$OUTPUT" -d '[:punct:]')
[ "$out" = "hello world" ]; check $? "delete punct"

# ========== T21: [c*] repeat in set2
echo ""; echo "--- T21: [c*] repeat ---"
out=$(echo "abcdef" | ./"$OUTPUT" 'abcdef' '[x*]')
[ "$out" = "xxxxxx" ]; check $? "repeat construct [x*]"

# ========== T22: [c*n] repeat in set2
echo ""; echo "--- T22: [c*n] repeat ---"
out=$(echo "abc" | ./"$OUTPUT" 'abc' '[x*3]y')
[ "$out" = "xxx" ]; check $? "repeat construct [x*3]"

# ========== T23: --help
echo ""; echo "--- T23: --help ---"
./"$OUTPUT" --help >/dev/null 2>&1; [ $? -eq 0 ]; check $? "help exits 0"

# ========== T24: --version
echo ""; echo "--- T24: --version ---"
./"$OUTPUT" --version >/dev/null 2>&1; [ $? -eq 0 ]; check $? "version exits 0"

# ========== T25: missing operand
echo ""; echo "--- T25: missing operand ---"
./"$OUTPUT" >/dev/null 2>&1; [ $? -ne 0 ]; check $? "missing operand fails"

# ========== T26: translate missing second operand
echo ""; echo "--- T26: translate missing operand ---"
echo "test" | ./"$OUTPUT" 'a' >/dev/null 2>&1; [ $? -ne 0 ]; check $? "missing second operand"

# ========== T27: -- separator
echo ""; echo "--- T27: -- separator ---"
out=$(echo "abc" | ./"$OUTPUT" -- 'abc' 'xyz')
[ "$out" = "xyz" ]; check $? "double dash separator"

# ========== T28: reverse range error
echo ""; echo "--- T28: reverse range error ---"
echo "test" | ./"$OUTPUT" 'z-a' 'a-z' >/dev/null 2>&1; [ $? -ne 0 ]; check $? "reverse range error"

# ========== T29: mixed range and literal
echo ""; echo "--- T29: mixed range and literal ---"
out=$(echo "abc123" | ./"$OUTPUT" 'a-c1-3' 'xyzABC')
[ "$out" = "xyzABC" ]; check $? "mixed ranges"

# ========== T30: complement squeeze
echo ""; echo "--- T30: complement squeeze ---"
out=$(echo "aaa111bbb" | ./"$OUTPUT" -c -s '0-9')
[ "$out" = "a111b" ]; check $? "complement squeeze"

echo ""; echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then exit 1; else exit 0; fi
