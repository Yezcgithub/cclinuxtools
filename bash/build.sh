#!/usr/bin/env bash
# build.sh for mini-bash (Linux / macOS / POSIX)
# Cross-platform bash build and smoke-test runner.

set -e

BASH=./bash
TD=_basht

[ -f bash ] && rm -f bash
[ -d "$TD" ] && rm -rf "$TD"
mkdir -p "$TD/tmp"

echo "=== BUILD ==="
UNAME_S="$(uname -s 2>/dev/null || echo Linux)"
case "$UNAME_S" in
  Linux)   gcc -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -o bash bash.c ;;
  Darwin)  gcc -O2 -std=c99 -D_DARWIN_C_SOURCE    -Wall -Wextra -o bash bash.c ;;
  NetBSD)  gcc -O2 -std=c99 -D_NETBSD_C_SOURCE     -Wall -Wextra -o bash bash.c ;;
  FreeBSD|OpenBSD) gcc -O2 -std=c99 -Wall -Wextra -o bash bash.c ;;
  *)       gcc -O2 -std=c99 -Wall -Wextra -o bash bash.c ;;
esac

PASS=0

# -------- fixture files ---------
printf 'hello world\n'        > "$TD/tmp/a.txt"
printf 'foo\nbar\nbaz\n'     > "$TD/tmp/b.txt"
printf '1\n2\n3\n'           > "$TD/tmp/n.txt"

chk() { PASS=$((PASS+1)); echo "=== TEST $PASS: $1 ==="; shift; "$@"; }
assert_eq() {
  local actual="$1" expect="$2"
  [ "$actual" = "$expect" ] || { echo "FAIL: expected '$expect' got '$actual'"; exit 1; }
}

# T1 version
chk "--version" $BASH --version > "$TD/v.txt"
grep -qi bash "$TD/v.txt"

# T2 help
chk "--help" $BASH --help > "$TD/help.txt"
grep -q "Usage:" "$TD/help.txt"

# T3 echo
chk "echo builtin"
out="$($BASH -c 'echo hello world')"
assert_eq "$out" "hello world"

# T4 pwd
chk "pwd builtin"
out="$($BASH -c 'pwd')"
[ -n "$out" ] || { echo "FAIL pwd empty"; exit 1; }

# T5 var assign
chk "var assign + \$var"
out="$($BASH -c 'x=42; echo $x')"
assert_eq "$out" "42"

# T6 arithmetic
chk 'arithmetic $(( ))'
out="$($BASH -c 'echo $((2 + 3 * 4))')"
assert_eq "$out" "14"

# T7 if/then/fi
chk "if then fi"
out="$($BASH -c 'if true; then echo yes; fi')"
assert_eq "$out" "yes"

# T8 if/else
chk "if else"
out="$($BASH -c 'if false; then echo a; else echo b; fi')"
assert_eq "$out" "b"

# T9 for in
chk "for in words"
out="$($BASH -c 'for i in a b c; do echo $i; done')"
echo "$out" | grep -qx a
echo "$out" | grep -qx b
echo "$out" | grep -qx c

# T10 while
chk "while loop"
out="$($BASH -c 'i=0; while [ $i -lt 3 ]; do echo $i; i=$((i+1)); done')"
echo "$out" | grep -qx 0
echo "$out" | grep -qx 1
echo "$out" | grep -qx 2

# T11 &&
chk "&& short-circuit"
out="$($BASH -c 'true && echo ok')"
assert_eq "$out" "ok"

# T12 ||
chk "|| fallback"
out="$($BASH -c 'false || echo fallback')"
assert_eq "$out" "fallback"

# T13 ; list
chk "semicolon list"
out="$($BASH -c 'echo first; echo second')"
echo "$out" | grep -qx first
echo "$out" | grep -qx second

# T14 ${#var}
chk '${#var} length'
out="$($BASH -c 's=hello; echo ${#s}')"
assert_eq "$out" "5"

# T15 ${var:-default}
chk '${x:-default}'
out="$($BASH -c 'unset x; echo ${x:-def}')"
assert_eq "$out" "def"

# T16 redirect write
chk "redirect > file"
$BASH -c 'echo data123 > '"$TD"'/tmp/out.txt'
out="$(cat "$TD/tmp/out.txt")"
assert_eq "$out" "data123"

# T17 redirect read
chk "redirect read < file"
out="$($BASH -c 'read line < '"$TD"'/tmp/a.txt; echo $line')"
assert_eq "$out" "hello world"

# T18 >> append
chk ">> append"
$BASH -c 'echo one > '"$TD"'/tmp/ap.txt; echo two >> '"$TD"'/tmp/ap.txt'
grep -qx one "$TD/tmp/ap.txt"
grep -qx two "$TD/tmp/ap.txt"

# T19 case
chk "case/esac"
out="$($BASH -c 'v=bar; case $v in foo) echo F;; bar) echo B;; *) echo O;; esac')"
assert_eq "$out" "B"

# T20 function
chk "function def+call"
out="$($BASH -c 'f(){ echo hello $1; }; f world')"
assert_eq "$out" "hello world"

# T21 $?
chk '$? exit status'
out="$($BASH -c 'false; echo $?')"
assert_eq "$out" "1"

# T22 pipe awk
chk 'pipe | awk (external same-dir)'
# check for awk in $PATH or same dir
if command -v awk >/dev/null 2>&1; then
  out="$($BASH -c 'echo a b c | awk "{print \$2}"')"
  assert_eq "$out" "b"
else
  echo "SKIP (awk not found in PATH)"
fi

# T23 pipe sed
chk 'pipe | sed s///'
if command -v sed >/dev/null 2>&1; then
  out="$($BASH -c "echo hello | sed 's/llo/i/'")"
  assert_eq "$out" "hei"
else
  echo "SKIP (sed not found)"
fi

# T24 ! negation
chk "! pipeline negation"
out="$($BASH -c 'if ! false; then echo nope; else echo negated; fi' | sed -n '1p')"
# Expected: nope (because !false == true)
# Actually, `if ! false` => take then branch => print "nope"
# Fix:
out="$($BASH -c 'if ! true; then echo nope; else echo negated; fi')"
assert_eq "$out" "negated"

# T25 break
chk "for + break"
out="$($BASH -c 'for i in 1 2 3 4; do if [ $i = 3 ]; then break; fi; echo $i; done')"
echo "$out" | grep -qx 1
echo "$out" | grep -qx 2
{ echo "$out" | grep -qx 3; } && { echo "FAIL break T25"; exit 1; } || true

# T26 continue
chk "for + continue"
out="$($BASH -c 'for i in 1 2 3; do if [ $i = 2 ]; then continue; fi; echo $i; done')"
echo "$out" | grep -qx 1
{ echo "$out" | grep -qx 2; } && { echo "FAIL continue T26"; exit 1; } || true
echo "$out" | grep -qx 3

# T27 shift
chk "shift positional"
cat > "$TD/t27.script" <<'EOF'
echo $1 $2 $3
shift
echo $1 $2
EOF
out="$($BASH "$TD/t27.script" a b c d)"
echo "$out" | grep -qx "a b c"
echo "$out" | grep -qx "b c d"

# T28 printf
chk "printf fmt"
out="$($BASH -c 'printf "%03d %s" 5 hi')"
assert_eq "$out" "005 hi"

# T29 source
chk "source/builtin"
printf 'INC_VAR=sourced_ok\n' > "$TD/inc.sh"
out="$($BASH -c '. '"$TD"'/inc.sh; echo $INC_VAR')"
assert_eq "$out" "sourced_ok"

# T30 which
chk "which awk"
if command -v awk >/dev/null 2>&1; then
  out="$($BASH -c 'which awk')"
  case "$out" in *awk*) ;; *) echo "FAIL which"; exit 1;; esac
else
  echo "SKIP awk"
fi

# T31 ls (external, if exists)
chk "external ls command"
if command -v ls >/dev/null 2>&1; then
  $BASH -c "ls $TD/tmp/a.txt" >/dev/null
else
  echo "SKIP ls"
fi

# T32 test -f
chk "test -f / -d"
out="$($BASH -c 'if [ -f '"$TD"'/tmp/a.txt ]; then echo isfile; fi')"
assert_eq "$out" "isfile"

# T33 test -eq
chk "test -eq compare"
out="$($BASH -c 'if [ 5 -eq 5 ]; then echo eq; fi')"
assert_eq "$out" "eq"

# T34 cp (external, if exists)
chk "cp external"
if command -v cp >/dev/null 2>&1; then
  $BASH -c "cp $TD/tmp/a.txt $TD/tmp/a_copy.txt"
  out="$($BASH -c 'if [ -f '"$TD"'/tmp/a_copy.txt ]; then echo okcp; fi')"
  assert_eq "$out" "okcp"
else
  echo "SKIP cp"
fi

# T35 cd + pwd
chk "cd then pwd"
out="$($BASH -c 'cd '"$TD"'/tmp; pwd')"
case "$out" in *tmp*) ;; *) echo "FAIL cd/pwd"; exit 1;; esac

# T36 3-stage pipe (if awk+sed avail)
chk "3-stage pipe awk+sed"
if command -v awk >/dev/null 2>&1 && command -v sed >/dev/null 2>&1; then
  out="$($BASH -c "echo x y z | awk '{print \$3}' | sed s/z/Z/")"
  assert_eq "$out" "Z"
else
  echo "SKIP pipe awk/sed"
fi

# T37 double-quote preserve spaces
chk 'double-quote no word-split'
out="$($BASH -c 'v="a  b  c"; echo "$v"')"
case "$out" in *"a  b  c"*) ;; *) echo "FAIL quoted='$out'"; exit 1;; esac

# T38 return
chk "return from function"
cat > "$TD/t38.script" <<'EOF'
f() { echo first; return 0; echo second; }
f
EOF
out="$($BASH "$TD/t38.script")"
echo "$out" | grep -qx first
{ echo "$out" | grep -qx second; } && { echo "FAIL return T38"; exit 1; } || true

# T39 exit status
chk "exit status code"
set +e
$BASH -c 'exit 3'
ec=$?
set -e
[ "$ec" = "3" ] || { echo "FAIL exit=$ec"; exit 1; }

# T40 export
chk "export + echo MY_VAR"
out="$($BASH -c 'export MY_VAR=hi_there; echo $MY_VAR')"
assert_eq "$out" "hi_there"

echo
echo "ALL TESTS PASSED."
rm -rf "$TD"
exit 0
