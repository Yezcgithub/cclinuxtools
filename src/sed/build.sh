#!/usr/bin/env bash
# build.sh for sed (Linux / macOS / FreeBSD / OpenBSD / NetBSD)
#
set -u
case "$(uname -s | tr '[:upper:]' '[:lower:]')" in
  linux*)   CC="${CC:-gcc}" CFLAGS="${CFLAGS:--O2 -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra}" ;;
  darwin*)  CC="${CC:-gcc}" CFLAGS="${CFLAGS:--O2 -std=c99 -D_DARWIN_C_SOURCE -Wall -Wextra}" ;;
  freebsd*) CC="${CC:-cc}"  CFLAGS="${CFLAGS:--O2 -std=c99 -Wall -Wextra}" ;;
  openbsd*) CC="${CC:-cc}"  CFLAGS="${CFLAGS:--O2 -std=c99 -Wall -Wextra}" ;;
  netbsd*)  CC="${CC:-cc}"  CFLAGS="${CFLAGS:--O2 -std=c99 -D_NETBSD_SOURCE -Wall -Wextra}" ;;
  *)        CC="${CC:-cc}"  CFLAGS="${CFLAGS:--O2 -std=c99 -Wall -Wextra}" ;;
esac

rm -f sed
rm -rf _sedt
mkdir -p _sedt

echo "=== BUILD ==="
echo "$CC $CFLAGS -o sed sed.c"
$CC $CFLAGS -o sed sed.c || exit 1

T=_sedt

pass() { printf "\x1b[32mPASS\x1b[0m %s\n" "$1"; }
fail() { printf "\x1b[31mFAIL\x1b[0m %s (got: %s)\n" "$1" "$2"; exit 1; }
check() { local name="$1" expected="$2" actual="$3";
  if [ "$expected" = "$actual" ]; then pass "$name"; else fail "$name" "$actual"; fi; }

echo "=== TEST 1: --version ==="
./sed --version | head -n1 > "$T/v.txt"
grep -q "sed" "$T/v.txt" || fail "version" "$(cat "$T/v.txt")"
pass "version"

echo "=== TEST 2: --help ==="
./sed --help | grep -q "Usage:" || fail "help" ""
pass "help"

echo "=== TEST 3: basic s/// ==="
out=$(echo "hello world" | ./sed 's/world/sed/')
check "basic s" "hello sed" "$out"

echo "=== TEST 4: s///g global ==="
out=$(echo "aaa" | ./sed 's/a/b/g')
check "s g" "bbb" "$out"

echo "=== TEST 5: s///2 nth ==="
out=$(echo "aaa" | ./sed 's/a/X/2')
check "s nth" "aXa" "$out"

echo "=== TEST 6: s///p flag ==="
out=$(echo "abc" | ./sed -n 's/b/B/p')
check "s p" "aBc" "$out"

echo "=== TEST 7: s///i flag ==="
out=$(echo "HELLO" | ./sed 's/hello/world/i')
check "s i" "world" "$out"

echo "=== TEST 8: d delete ==="
out=$(printf "a\nb\nc\n" | ./sed '2d')
check "d" "$(printf "a\nc")" "$out"

echo "=== TEST 9: -n p ==="
out=$(printf "a\nb\nc\n" | ./sed -n '2p')
check "-n p" "b" "$out"

echo "=== TEST 10: line range ==="
out=$(printf "1\n2\n3\n4\n5\n" | ./sed -n '2,4p')
check "range" "$(printf "2\n3\n4")" "$out"

echo "=== TEST 11: regex address ==="
out=$(printf "foo\nbar\nbaz\n" | ./sed -n '/b/p')
check "regex addr" "$(printf "bar\nbaz")" "$out"

echo "=== TEST 12: \$ last line ==="
out=$(printf "a\nb\nc\n" | ./sed -n '$p')
check "dollar" "c" "$out"

echo "=== TEST 13: a append ==="
out=$(echo "hello" | ./sed 'a\appended')
check "a" "$(printf "hello\nappended")" "$out"

echo "=== TEST 14: i insert ==="
out=$(echo "hello" | ./sed 'i\inserted')
check "i" "$(printf "inserted\nhello")" "$out"

echo "=== TEST 15: c change ==="
out=$(printf "a\nb\nc\n" | ./sed '2c\changed')
check "c" "$(printf "a\nchanged\nc")" "$out"

echo "=== TEST 16: y translit ==="
out=$(echo "abc" | ./sed 'y/abc/ABC/')
check "y" "ABC" "$out"

echo "=== TEST 17: h g hold ==="
out=$(printf "a\nb\n" | ./sed -n 'h;1!G;p')
check "hG" "$(printf "a\nb\na")" "$out"

echo "=== TEST 18: x exchange ==="
out=$(printf "a\nb\n" | ./sed 'x')
check "x" "$(printf "\na")" "$out"

echo "=== TEST 19: N multi-line ==="
out=$(printf "foo\nbar\n" | ./sed 'N;s/\n/ /')
check "N" "foo bar" "$out"

echo "=== TEST 20: b branch ==="
out=$(printf "1\n2\n3\n" | ./sed '2b skip; s/./X/; :skip')
check "b" "$(printf "X\n2\nX")" "$out"

echo "=== TEST 21: t branch ==="
out=$(printf "1\n2\n3\n" | ./sed 's/./X/; t end; s/X/Y/; :end')
check "t" "$(printf "X\nX\nX")" "$out"

echo "=== TEST 22: {} block ==="
out=$(printf "1\n2\n3\n" | ./sed '{s/1/one/;s/3/three/}')
check "block" "$(printf "one\n2\nthree")" "$out"

echo "=== TEST 23: = line number ==="
out=$(printf "a\nb\nc\n" | ./sed '=')
check "=" "$(printf "1\na\n2\nb\n3\nc")" "$out"

echo "=== TEST 24: -E extended regex ==="
out=$(echo "hello world" | ./sed -E 's/(o)/\1\1/g')
check "-E" "helloo woorld" "$out"

echo "=== TEST 25: backreference \\1 ==="
out=$(echo "hello" | ./sed 's/\(l\(o\)\)/X\1Y/')
check "backref" "helXloY" "$out"

echo "=== TEST 26: -f script file ==="
echo 's/foo/FOO/' > "$T/script.sed"
out=$(echo "foo bar" | ./sed -f "$T/script.sed")
check "-f" "FOO bar" "$out"

echo "=== TEST 27: -e multiple expressions ==="
out=$(echo "abc" | ./sed -e 's/a/A/' -e 's/c/C/')
check "-e" "AbC" "$out"

echo "=== TEST 28: P print first line ==="
out=$(printf "foo\nbar\n" | ./sed -n 'N;P')
check "P" "foo" "$out"

echo "=== TEST 29: D delete first line ==="
out=$(printf "a\nb\nc\n" | ./sed 'N;D')
check "D" "c" "$out"

echo "=== TEST 30: addr~step ==="
out=$(printf "1\n2\n3\n4\n5\n6\n" | ./sed -n '0~2p')
check "step" "$(printf "2\n4\n6")" "$out"

echo "=== TEST 31: r read file ==="
echo "INSERTED" > "$T/rfile.txt"
out=$(printf "line1\nline2\nline3\n" | ./sed '2r '"$T"'/rfile.txt')
check "r" "$(printf "line1\nline2\nINSERTED\nline3")" "$out"

echo "=== TEST 32: w write file ==="
printf "a\nb\nc\n" | ./sed -n '1,2w '"$T"'/wout.txt'
out=$(cat "$T/wout.txt")
check "w" "$(printf "a\nb")" "$out"

echo "=== TEST 33: l list ==="
out=$(echo "abc" | ./sed -n 'l')
check "l" 'abc$' "$out"

echo "=== TEST 34: q quit ==="
out=$(printf "1\n2\n3\n" | ./sed '2q')
check "q" "$(printf "1\n2")" "$out"

echo "=== TEST 35: Q quit no print ==="
out=$(printf "1\n2\n3\n" | ./sed '2Q')
check "Q" "1" "$out"

echo "=== TEST 36: tac simulate ==="
out=$(printf "a\nb\nc\n" | ./sed -n '1!G;h;$p')
check "tac" "$(printf "c\nb\na")" "$out"

echo "=== TEST 37: join lines ==="
out=$(printf "a\nb\nc\nd\n" | ./sed ':a;N;$!ba;s/\n/-/g')
check "join" "a-b-c-d" "$out"

echo "=== TEST 38: addr,+N range ==="
out=$(printf "1\n2\n3\n4\n5\n" | ./sed -n '2,+1p')
check "addr+N" "$(printf "2\n3")" "$out"

echo "=== TEST 39: ! negation ==="
out=$(printf "a\nb\nc\n" | ./sed -n '2!p')
check "neg" "$(printf "a\nc")" "$out"

echo "=== TEST 40: -i in-place ==="
printf "foo\nbar\n" > "$T/ip.txt"
./sed -i 's/foo/FOO/' "$T/ip.txt"
out=$(cat "$T/ip.txt")
check "-i" "$(printf "FOO\nbar")" "$out"

echo "=== TEST 41: -i with backup ==="
printf "foo\nbar\n" > "$T/ipb.txt"
./sed -i'.bak' 's/foo/FOO/' "$T/ipb.txt"
out=$(cat "$T/ipb.txt")
bak=$(cat "$T/ipb.txt.bak")
check "-i backup" "$(printf "FOO\nbar")" "$out"
check "-i bak content" "$(printf "foo\nbar")" "$bak"

echo "=== TEST 42: multiple files ==="
printf "a\n" > "$T/f1.txt"
printf "b\n" > "$T/f2.txt"
out=$(./sed 's/./X/' "$T/f1.txt" "$T/f2.txt")
check "multi-file" "$(printf "X\nX")" "$out"

echo "=== TEST 43: character class ==="
out=$(echo "Hello123" | ./sed 's/[0-9]//g')
check "class" "Hello" "$out"

echo "=== TEST 44: s with & ==="
out=$(echo "abc" | ./sed 's/b/[&]/')
check "ampersand" "a[b]c" "$out"

echo "=== TEST 45: s with \n in replacement ==="
out=$(echo "abc" | ./sed 's/b/\n/')
check "repl newline" "$(printf "a\nc")" "$out"

rm -rf _sedt
echo
echo "ALL TESTS PASSED."
