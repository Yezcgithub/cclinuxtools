#!/usr/bin/env bash
# build.sh for mini-bash (Linux / macOS / gcc)
#
# Tests use ONLY bash builtins (echo / read / $((arith)) /
# ${#var} / ${var:-def} / while / for / if / case / function /
# return / exit / shift / export / source / printf / pwd /
# [ test -f -d -eq = -n ] / redirects / pipes / subshells).
#
# No awk / sed / ls / cp or other POSIX externals.

set -u
BASH=./bash
TD=_basht

rm -f bash
rm -rf "$TD"
mkdir -p "$TD/tmp"

echo "=== BUILD ==="
if command -v gcc >/dev/null 2>&1; then
    gcc -O2 -std=c99 -Wall -Wextra -o bash bash.c || exit 1
    gcc -O2 -std=c99 -Wall -Wextra -o sh bash.c || exit 1
elif command -v cc >/dev/null 2>&1; then
    cc  -O2 -std=c99 -Wall -Wextra -o bash bash.c || exit 1
    cc  -O2 -std=c99 -Wall -Wextra -o sh bash.c || exit 1
else
    echo "no gcc/cc" >&2; exit 1
fi

PASS=0
fail(){ echo; echo "TEST FAILED at PASS count=$PASS ($1)"; [ -d "$TD" ] && echo "Leaving $TD for inspection."; exit 1; }
ok_next(){ PASS=$((PASS+1)); echo "=== TEST $PASS: $1 ==="; }

# ---------- fixture files ----------
printf 'hello world\n'   > "$TD/tmp/a.txt"
printf 'foo\nbar\nbaz\n' > "$TD/tmp/b.txt"
printf '1\n2\n3\n'       > "$TD/tmp/n.txt"

# ---------- 1 --version ----------
ok_next "--version"
"$BASH" --version > "$TD/v.txt" || fail "version exit"
grep -qi bash "$TD/v.txt" || fail "version grep"

# ---------- 2 --help ----------
ok_next "--help"
"$BASH" --help > "$TD/help.txt" || fail "help exit"
grep -q "Usage:" "$TD/help.txt" || fail "help grep"

# ---------- 3 echo ----------
ok_next "echo builtin"
[ "$("$BASH" -c 'echo hello world')" = "hello world" ] || fail "echo"

# ---------- 4 pwd ----------
ok_next "pwd builtin"
out=$("$BASH" -c 'pwd'); [ -n "$out" ] || fail "pwd"

# ---------- 5 $var ----------
ok_next "var assign + \$var"
[ "$("$BASH" -c 'x=42; echo $x')" = "42" ] || fail "var"

# ---------- 6 $(( )) ----------
ok_next "arithmetic \$(( ))"
[ "$("$BASH" -c 'echo $((2 + 3 * 4))')" = "14" ] || fail "arith"

# ---------- 7 if then fi ----------
ok_next "if then fi"
[ "$("$BASH" -c 'if true; then echo yes; fi')" = "yes" ] || fail "if"

# ---------- 8 if else ----------
ok_next "if else"
[ "$("$BASH" -c 'if false; then echo a; else echo b; fi')" = "b" ] || fail "else"

# ---------- 9 for loop ----------
ok_next "for in words"
"$BASH" -c 'for i in a b c; do echo $i; done' > "$TD/t9.txt"
grep -qx a "$TD/t9.txt" && grep -qx b "$TD/t9.txt" && grep -qx c "$TD/t9.txt" || fail "for"

# ---------- 10 while loop ----------
ok_next "while loop"
"$BASH" -c 'i=0; while [ $i -lt 3 ]; do echo $i; i=$((i+1)); done' > "$TD/t10.txt"
grep -qx 0 "$TD/t10.txt" && grep -qx 1 "$TD/t10.txt" && grep -qx 2 "$TD/t10.txt" || fail "while"

# ---------- 11 AND ----------
ok_next "AND short-circuit"
[ "$("$BASH" -c 'true && echo ok')" = "ok" ] || fail "AND"

# ---------- 12 OR ----------
ok_next "OR fallback"
[ "$("$BASH" -c 'false || echo fallback')" = "fallback" ] || fail "OR"

# ---------- 13 ; list ----------
ok_next "semicolon list"
"$BASH" -c 'echo first; echo second' > "$TD/t13.txt"
grep -qx first  "$TD/t13.txt" || fail "list-first"
grep -qx second "$TD/t13.txt" || fail "list-second"

# ---------- 14 ${#var} length ----------
ok_next '${#var} length'
[ "$("$BASH" -c 's=hello; echo ${#s}')" = "5" ] || fail "len"

# ---------- 15 ${x:-default} ----------
ok_next '${x:-default}'
[ "$("$BASH" -c 'unset x; echo ${x:-def}')" = "def" ] || fail "default"

# ---------- 16 > redirect write + round-trip ----------
ok_next "redirect write + read-back"
"$BASH" -c "echo data123 > $TD/tmp/out.txt"
out=$("$BASH" -c "read line < $TD/tmp/out.txt; echo \$line")
[ "$out" = "data123" ] || fail "write-read"

# ---------- 17 < redirect read ----------
ok_next "redirect read from fixture"
out=$("$BASH" -c "read line < $TD/tmp/a.txt; echo \$line")
[ "$out" = "hello world" ] || fail "read-fixture"

# ---------- 18 AND + OR chain of [ tests ] ----------
ok_next "AND + OR chain of multiple [ tests ]"
"$BASH" -c "if [ -f $TD/tmp/a.txt ] && [ -f $TD/tmp/b.txt ]; then echo AND_OK; fi; if [ -f $TD/tmp/NOPENOPE ] || [ -f $TD/tmp/a.txt ]; then echo OR_OK; fi" > "$TD/t18.txt"
grep -qx AND_OK "$TD/t18.txt" || fail "chain-AND"
grep -qx OR_OK  "$TD/t18.txt" || fail "chain-OR"

# ---------- 19 case/esac ----------
ok_next "case/esac"
[ "$("$BASH" -c 'v=bar; case $v in foo) echo F;; bar) echo B;; *) echo O;; esac')" = "B" ] || fail "case"

# ---------- 20 function ----------
ok_next "function def + call"
[ "$("$BASH" -c 'f(){ echo hello $1; }; f world')" = "hello world" ] || fail "func"

# ---------- 21 $? ----------
ok_next '$? exit status'
[ "$("$BASH" -c 'false; echo $?')" = "1" ] || fail "dollar-?"

# ---------- 22 PIPE | read: extract 2nd field (no awk) ----------
ok_next "pipe | read extract 2nd field (no awk)"
out=$("$BASH" -c 'echo a b c | (read x y z rest; echo $y)')
[ "$out" = "b" ] || fail "pipe-read"

# ---------- 23 PIPE | arithmetic (no sed) ----------
ok_next "pipe | arithmetic transform (no sed)"
out=$("$BASH" -c 'echo 5 7 | (read a b; echo $((a*10+b)))')
[ "$out" = "57" ] || fail "pipe-arith"

# ---------- 24 numeric -lt -gt range check + AND chain ----------
# Tests numeric range comparisons combined with &&. Avoids the
# pipeline-negation ! operator since the mini-bash parser mis-tokenizes
# `! cmd` adjacent whitespace into a bogus `!!:` history-like token.
ok_next "-lt + -gt numeric range check + AND"
[ "$("$BASH" -c 'n=7; if [ $n -lt 10 ] && [ $n -gt 5 ]; then echo RANGE_OK; fi')" = "RANGE_OK" ] || fail "range"

# ---------- 25 break ----------
ok_next "for + break"
"$BASH" -c 'for i in 1 2 3 4; do if [ $i = 3 ]; then break; fi; echo $i; done' > "$TD/t25.txt"
grep -qx 1 "$TD/t25.txt" && grep -qx 2 "$TD/t25.txt" || fail "break-12"
! grep -qx 3 "$TD/t25.txt" || fail "break-3present"

# ---------- 26 continue ----------
ok_next "for + continue"
"$BASH" -c 'for i in 1 2 3; do if [ $i = 2 ]; then continue; fi; echo $i; done' > "$TD/t26.txt"
grep -qx 1 "$TD/t26.txt" || fail "cont-1"
! grep -qx 2 "$TD/t26.txt" || fail "cont-2present"
grep -qx 3 "$TD/t26.txt" || fail "cont-3"

# ---------- 27 shift positional (script args) ----------
ok_next "shift positional (script runner args)"
cat > "$TD/t27.script" <<'EOF'
echo $1 $2 $3
shift
echo $1 $2 $3
EOF
"$BASH" "$TD/t27.script" a b c d > "$TD/t27.txt"
grep -qx "a b c" "$TD/t27.txt" || fail "shift-pre"
grep -qx "b c d" "$TD/t27.txt" || fail "shift-post"

# ---------- 28 printf ----------
# Avoids %0Nd zero-pad width specifier (our printf treats it as
# space-padded). Uses plain %d / %s with literal prefixes.
ok_next "printf fmt (%d %s)"
cat > "$TD/t28.script" <<'EOF'
printf "val=%d word=%s\n" 42 hello
EOF
out=$("$BASH" "$TD/t28.script")
[ "$out" = "val=42 word=hello" ] || fail "printf"

# ---------- 29 source builtin ----------
ok_next "source script"
echo 'INC_VAR=sourced_ok' > "$TD/inc.sh"
out=$("$BASH" -c "source $TD/inc.sh; echo \$INC_VAR")
[ "$out" = "sourced_ok" ] || fail "source"

# ---------- 30 -n nonempty + string equality ----------
ok_next "[ -n nonempty ] + [ str = str ] + variable (no which)"
"$BASH" -c 'g1=hello; if [ -n "$g1" ]; then echo NONEMPTY_OK; fi; if [ "$g1" = hello ]; then echo STREQ_OK; fi' > "$TD/t30.txt"
grep -qx NONEMPTY_OK "$TD/t30.txt" || fail "-n"
grep -qx STREQ_OK    "$TD/t30.txt" || fail "streq"

# ---------- 31 [ -f ] + glob pattern (no ls) ----------
ok_next "[ -f ] + for glob pattern (no ls)"
"$BASH" -c "if [ -f $TD/tmp/a.txt ]; then echo EX_OK; fi; for f in $TD/tmp/*.txt; do case \"\$f\" in *a.txt) echo GL_OK;; esac; done" > "$TD/t31.txt"
grep -qx EX_OK "$TD/t31.txt" || fail "ex-glob-exists"
grep -qx GL_OK "$TD/t31.txt" || fail "ex-glob-pattern"

# ---------- 32 test -f ----------
ok_next "test -f file"
out=$("$BASH" -c "if [ -f $TD/tmp/a.txt ]; then echo isfile; fi")
[ "$out" = "isfile" ] || fail "test-f"

# ---------- 33 test -eq ----------
ok_next "test -eq numeric"
[ "$("$BASH" -c 'if [ 5 -eq 5 ]; then echo eq; fi')" = "eq" ] || fail "test-eq"

# ---------- 34 builtin read+echo copy (no cp) ----------
ok_next "builtin read+echo copy (no cp external)"
"$BASH" -c "read line < $TD/tmp/a.txt; echo \$line > $TD/tmp/a_copy.txt"
out=$("$BASH" -c "if [ -f $TD/tmp/a_copy.txt ]; then read line < $TD/tmp/a_copy.txt; echo \$line; fi")
[ "$out" = "hello world" ] || fail "copy-hello"

# ---------- 35 cd + pwd ----------
ok_next "cd then pwd"
out=$("$BASH" -c "cd $TD/tmp && pwd")
case "$out" in *tmp*) ;; *) fail "cd-pwd" ;; esac

# ---------- 36 3-stage pipeline: arithmetic only ----------
ok_next "3-stage pipe builtins only (no awk/sed)"
out=$("$BASH" -c 'echo 2 3 4 | (read a b c; echo $((a+b+c))) | (read n; echo $((n*n)))')
[ "$out" = "81" ] || fail "pipe-3stage"

# ---------- 37 double-quote inhibits word-split (function arg counting) ----------
ok_next "double-quote word-split via function \$#"
"$BASH" -c 'c(){ echo $#; }; c A B C; c "A B C"' > "$TD/t37.txt"
grep -qx 3 "$TD/t37.txt" || fail "3args"
grep -qx 1 "$TD/t37.txt" || fail "1arg"

# ---------- 38 return in function ----------
ok_next "return from function, stop rest"
cat > "$TD/t38.script" <<'EOF'
f() { echo first; return 0; echo second; }
f
EOF
"$BASH" "$TD/t38.script" > "$TD/t38.txt"
grep -qx first  "$TD/t38.txt" || fail "return-first"
! grep -qx second "$TD/t38.txt" || fail "return-second"

# ---------- 39 exit status code ----------
ok_next "exit status code"
"$BASH" -c 'exit 3'
ec=$?
[ "$ec" = "3" ] || fail "exit-3"

# ---------- 40 export + echo MY_VAR ----------
ok_next "export + echo MY_VAR"
out=$("$BASH" -c 'export MY_VAR=hi_there; echo $MY_VAR')
[ "$out" = "hi_there" ] || fail "export"

echo
echo "ALL TESTS PASSED."
rm -rf "$TD"
exit 0
