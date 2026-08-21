#!/usr/bin/env bash
# build.sh for awk (Linux / macOS / FreeBSD / OpenBSD / NetBSD)
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

rm -f awk
rm -rf _awkt
mkdir -p _awkt

echo "=== BUILD ==="
echo "$CC $CFLAGS -o awk awk.c -lm"
$CC $CFLAGS -o awk awk.c -lm || exit 1

T=_awkt

pass() { printf "\x1b[32mPASS\x1b[0m %s\n" "$1"; }
fail() { printf "\x1b[31mFAIL\x1b[0m %s (got: %s)\n" "$1" "$2"; exit 1; }
check() { local name="$1" expected="$2" actual="$3";
  if [ "$expected" = "$actual" ]; then pass "$name"; else fail "$name" "$actual"; fi; }

echo "=== TEST 1: --version ==="
./awk --version | head -n1 > "$T/v.txt"
grep -q "awk" "$T/v.txt" || fail "version" "$(cat $T/v.txt)"
pass "version"

echo "=== TEST 2: --help ==="
./awk --help | grep -q "Usage:" || fail "help" ""
pass "help"

echo "=== TEST 3: print \$0 stdin ==="
out=$(echo "hello world" | ./awk '{ print }')
check "print \$0" "hello world" "$out"

echo "=== TEST 4: print \$1,\$3 ==="
out=$(echo "a b c d" | ./awk '{ print $1, $3 }')
check "print 1,3" "a c" "$out"

echo "=== TEST 5: -F: + \$3 >= 1000 ==="
cat > "$T/pw.txt" <<'EOF'
root:x:0:0:root:/root:/bin/bash
alice:x:1001:1000:alice:/home/alice:/bin/bash
bob:x:500:100:bob:/home/bob:/bin/sh
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
EOF
out=$(./awk -F: '$3 >= 1000 { print $1 }' "$T/pw.txt")
echo "$out" | grep -qx "alice" || fail "uid>=1000 alice" "$out"
echo "$out" | grep -qx "nobody" || fail "uid>=1000 nobody" "$out"
pass "-F + compare"

echo "=== TEST 6: NR NF FILENAME ==="
out=$(./awk -F: '{print NR, NF, FILENAME}' "$T/pw.txt" | tail -n1)
check "last NR/NF/FN" "4 7 $T/pw.txt" "$out"

echo "=== TEST 7: BEGIN + END accum ==="
cat > "$T/n.txt" <<'EOF'
10
20
30
EOF
out=$(./awk 'BEGIN{s=0} {s+=$1} END{print s}' "$T/n.txt")
check "sum 60" "60" "$out"

echo "=== TEST 8: /regex/ pattern ==="
out=$(./awk '/ro/' "$T/pw.txt")
echo "$out" | grep -q "root" || fail "regex ro" "$out"
pass "regex pattern"

echo "=== TEST 9: ~ with RSTART RLENGTH ==="
out=$(echo "hello world" | ./awk '{ if ($0 ~ /wo/) print RSTART, RLENGTH }')
check "match" "7 2" "$out"

echo "=== TEST 10: substr + index ==="
out=$(echo "foobar" | ./awk '{ print substr($0,2,3) }')
check "substr 2,3" "oob" "$out"
out=$(echo "abcdef" | ./awk '{ print index($0,"de") }')
check "index de=4" "4" "$out"

echo "=== TEST 11: printf %03d %0.2f %s ==="
out=$(./awk 'BEGIN{printf "%03d %0.2f %s\n", 5, 3.14159, "ok"}')
check "printf" "005 3.14 ok" "$out"

echo "=== TEST 12: ternary ==="
out=$(./awk '{ print ($1>=20 ? "big" : "small") }' "$T/n.txt")
expected=$(printf "small\nbig\nbig")
check "ternary" "$expected" "$out"

echo "=== TEST 13: -v N=3 ==="
out=$(./awk -v N=3 'BEGIN{print N, N+1}')
check "-v assign" "3 4" "$out"

echo "=== TEST 14: unknown option exits 2 ==="
set +e
./awk --we-dont-have-this-option 2>/dev/null
rc=$?
set -e
[ "$rc" = "2" ] || fail "unknown option rc" "$rc"
pass "unknown option rc=2"

echo "=== TEST 15: -f progfile ==="
echo '{ print $2, $1 }' > "$T/swp.awk"
out=$(echo "a b c" | ./awk -f "$T/swp.awk")
check "-f prog" "b a" "$out"

echo "=== TEST 16: s+=2 s*=3 ==="
out=$(echo "5" | ./awk '{ s=$1; s+=2; s*=3; print s }')
check "compound assign" "21" "$out"

echo "=== TEST 17: FS regex /[,-]/ ==="
out=$(echo "a,b-c,d" | ./awk -F'[,-]' '{print NF, $2}')
check "regex FS" "4 b" "$out"

echo "=== TEST 18: OFS ORS ==="
out=$(echo "1 2 3" | ./awk 'BEGIN{OFS="|"; ORS="-"} {print $1,$2,$3}')
check "OFS ORS" "1|2|3-" "$out"

echo "=== TEST 19: sub first match ==="
out=$(echo "ababa" | ./awk '{ sub(/aba/, "X"); print }')
check "sub first" "Xba" "$out"

echo "=== TEST 20: gsub global ==="
out=$(echo "aabbaa" | ./awk '{ gsub(/aa/, "Z"); print }')
check "gsub global" "ZbbZ" "$out"

echo "=== TEST 21: pattern with && ==="
out=$(./awk -F: '$1 ~ /o/ && $3 >= 0 {print $1}' "$T/pw.txt" | head -n1)
check "&& and ~" "root" "$out"

echo "=== TEST 22: ! negation ==="
out=$(./awk -F: '!($3 == 0){print $1}' "$T/pw.txt" | head -n1)
check "! filter" "alice" "$out"

echo "=== TEST 23: tolower toupper length ==="
out=$(echo "Hello" | ./awk '{ print tolower($0), toupper($0), length($0) }')
check "str builtins" "hello HELLO 5" "$out"

rm -rf _awkt
echo
echo "ALL TESTS PASSED."
