#!/bin/bash
# Build and test script for xargs.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    xargs.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=xargs
SOURCE=xargs.c
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

# ========== T01: basic echo
echo ""; echo "--- T01: basic echo ---"
out=$(echo "hello world" | ./"$OUTPUT")
[ "$out" = "hello world" ]; check $? "basic echo default command"

# ========== T02: multiple items
echo ""; echo "--- T02: multiple items ---"
out=$(printf 'a\nb\nc\n' | ./"$OUTPUT" echo)
[ "$out" = "a b c" ]; check $? "multiple items to single command"

# ========== T03: -n max args
echo ""; echo "--- T03: -n max args ---"
out=$(printf 'a\nb\nc\n' | ./"$OUTPUT" -n 1 echo)
expected=$(printf 'a\nb\nc')
[ "$out" = "$expected" ]; check $? "-n 1 one arg per command"

# ========== T04: -n 2
echo ""; echo "--- T04: -n 2 ---"
out=$(printf 'a\nb\nc\nd\n' | ./"$OUTPUT" -n 2 echo)
expected=$(printf 'a b\nc d')
[ "$out" = "$expected" ]; check $? "-n 2 two args per command"

# ========== T05: -0 null mode
echo ""; echo "--- T05: -0 null mode ---"
out=$(printf 'a\0b\0c\0' | ./"$OUTPUT" -0 echo)
[ "$out" = "a b c" ]; check $? "-0 NUL-terminated input"

# ========== T06: -d delimiter
echo ""; echo "--- T06: -d delimiter ---"
out=$(printf 'a,b,c' | ./"$OUTPUT" -d , echo)
[ "$out" = "a b c" ]; check $? "-d comma delimiter"

# ========== T07: -r no-run-if-empty
echo ""; echo "--- T07: -r no-run-if-empty ---"
out=$(printf '' | ./"$OUTPUT" -r echo)
[ -z "$out" ]; check $? "-r with empty input runs nothing"

# ========== T08: default empty runs once
echo ""; echo "--- T08: default empty runs once ---"
out=$(printf '' | ./"$OUTPUT" echo)
[ "$out" = "" ]; check $? "empty input runs command once (no args)"

# ========== T09: -I replace
echo ""; echo "--- T09: -I replace ---"
out=$(printf 'foo\nbar\n' | ./"$OUTPUT" -I {} echo file-{}.txt)
expected=$(printf 'file-foo.txt\nfile-bar.txt')
[ "$out" = "$expected" ]; check $? "-I {} replacement"

# ========== T10: -L max lines
echo ""; echo "--- T10: -L max lines ---"
out=$(printf 'a\nb\nc\nd\n' | ./"$OUTPUT" -L 2 echo)
expected=$(printf 'a b\nc d')
[ "$out" = "$expected" ]; check $? "-L 2 two lines per command"

# ========== T11: -t verbose
echo ""; echo "--- T11: -t verbose ---"
out=$(echo "test" | ./"$OUTPUT" -t echo 2>&1 >/dev/null)
echo "$out" | grep -q "echo test"; check $? "-t prints command to stderr"

# ========== T12: -E eof string
echo ""; echo "--- T12: -E eof string ---"
out=$(printf 'a\nb\nEOF\nc\n' | ./"$OUTPUT" -E EOF echo)
[ "$out" = "a b" ]; check $? "-E stops at EOF string"

# ========== T13: quoted strings
echo ""; echo "--- T13: quoted strings ---"
out=$(printf '"hello world" foo\n' | ./"$OUTPUT" echo)
[ "$out" = "hello world foo" ]; check $? "double quotes preserve spaces"

# ========== T14: single quotes
echo ""; echo "--- T14: single quotes ---"
out=$(printf "'hello world' foo\n" | ./"$OUTPUT" echo)
[ "$out" = "hello world foo" ]; check $? "single quotes preserve spaces"

# ========== T15: backslash escape
echo ""; echo "--- T15: backslash escape ---"
out=$(printf 'hello\ world foo\n' | ./"$OUTPUT" echo)
[ "$out" = "hello world foo" ]; check $? "backslash escapes space"

# ========== T16: --help
echo ""; echo "--- T16: --help ---"
./"$OUTPUT" --help >/dev/null 2>&1; [ $? -eq 0 ]; check $? "help exits 0"

# ========== T17: --version
echo ""; echo "--- T17: --version ---"
./"$OUTPUT" --version >/dev/null 2>&1; [ $? -eq 0 ]; check $? "version exits 0"

# ========== T18: custom command
echo ""; echo "--- T18: custom command ---"
out=$(echo "test" | ./"$OUTPUT" printf '[%s]\n')
[ "$out" = "[test]" ]; check $? "custom command printf"

# ========== T19: -s max chars
echo ""; echo "--- T19: -s max chars ---"
out=$(printf 'a\nb\nc\n' | ./"$OUTPUT" -s 10 -n 1 echo 2>&1)
# Should still work (single arg fits in 10 chars)
echo "$out" | grep -q "^a$"; check $? "-s with small limit works"

# ========== T20: whitespace handling
echo ""; echo "--- T20: whitespace handling ---"
out=$(printf '  a   b\tc\n' | ./"$OUTPUT" echo)
[ "$out" = "a b c" ]; check $? "multiple whitespace types handled"

# ========== T21: blank lines
echo ""; echo "--- T21: blank lines ---"
out=$(printf 'a\n\nb\n' | ./"$OUTPUT" echo)
[ "$out" = "a b" ]; check $? "blank lines skipped"

# ========== T22: -I with multiple replacements
echo ""; echo "--- T22: -I multiple replacements ---"
out=$(printf 'x\n' | ./"$OUTPUT" -I {} echo {}--{}--{})
[ "$out" = "x--x--x" ]; check $? "-I replaces all occurrences"

# ========== T23: --arg-file (via stdin simulation)
echo ""; echo "--- T23: arg file via -a ---"
printf 'alpha\nbeta\n' > /tmp/xargs_test_input.txt 2>/dev/null || printf 'alpha\nbeta\n' > xargs_test_input.txt
if [ -f /tmp/xargs_test_input.txt ]; then
    out=$(./"$OUTPUT" -a /tmp/xargs_test_input.txt echo)
    rm -f /tmp/xargs_test_input.txt
else
    out=$(./"$OUTPUT" -a xargs_test_input.txt echo)
    rm -f xargs_test_input.txt
fi
[ "$out" = "alpha beta" ]; check $? "-a reads from file"

# ========== T24: --long option forms
echo ""; echo "--- T24: long option --null ---"
out=$(printf 'a\0b\0' | ./"$OUTPUT" --null echo)
[ "$out" = "a b" ]; check $? "--null long option"

# ========== T25: --max-args
echo ""; echo "--- T25: --max-args ---"
out=$(printf 'a\nb\n' | ./"$OUTPUT" --max-args=1 echo)
expected=$(printf 'a\nb')
[ "$out" = "$expected" ]; check $? "--max-args long option"

# ========== T26: --no-run-if-empty
echo ""; echo "--- T26: --no-run-if-empty ---"
out=$(printf '' | ./"$OUTPUT" --no-run-if-empty echo)
[ -z "$out" ]; check $? "--no-run-if-empty long option"

# ========== T27: --verbose
echo ""; echo "--- T27: --verbose ---"
out=$(echo "x" | ./"$OUTPUT" --verbose echo 2>&1 >/dev/null)
echo "$out" | grep -q "echo x"; check $? "--verbose long option"

# ========== T28: exit code on command failure
echo ""; echo "--- T28: exit code ---"
echo "test" | ./"$OUTPUT" sh -c 'exit 0' >/dev/null 2>&1
[ $? -eq 0 ]; check $? "exit 0 propagates"

# ========== T29: initial args
echo ""; echo "--- T29: initial args ---"
out=$(printf 'b\nc\n' | ./"$OUTPUT" echo a)
[ "$out" = "a b c" ]; check $? "initial args prepended"

# ========== T30: -n with remainder
echo ""; echo "--- T30: -n with remainder ---"
out=$(printf 'a\nb\nc\n' | ./"$OUTPUT" -n 2 echo)
expected=$(printf 'a b\nc')
[ "$out" = "$expected" ]; check $? "-n 2 with remainder of 1"

echo ""; echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then exit 1; else exit 0; fi
