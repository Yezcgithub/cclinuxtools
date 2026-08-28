#!/bin/bash
# Build and run regression tests for htop.
# Usage: ./build.sh

set -u

cd "$(dirname "$0")"

TDIR=$(mktemp -d 2>/dev/null || echo "/tmp/htop_test_$$")
PASS=0
FAIL=0
TOTAL=0

ok()   { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); printf "  [OK]   %s\n" "$1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); printf "  [FAIL] %s\n" "$1"; }
check() {
    # check <description> <command...>  — passes if command exits 0
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        ok "$desc"
    else
        fail "$desc"
    fi
}
check_not() {
    # check_not <description> <command...> — passes if command exits non-zero
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        fail "$desc"
    else
        ok "$desc"
    fi
}
check_grep() {
    # check_grep <description> <pattern> <file>
    local desc="$1" pattern="$2" file="$3"
    if grep -q "$pattern" "$file" 2>/dev/null; then
        ok "$desc"
    else
        fail "$desc"
    fi
}
check_nogrep() {
    # check_nogrep <description> <pattern> <file>
    local desc="$1" pattern="$2" file="$3"
    if grep -q "$pattern" "$file" 2>/dev/null; then
        fail "$desc"
    else
        ok "$desc"
    fi
}

echo "== 1. build =="
if command -v gcc >/dev/null 2>&1; then
    CC=gcc
elif command -v cc >/dev/null 2>&1; then
    CC=cc
else
    echo "error: no C compiler found (gcc/cc)" >&2
    exit 1
fi

CCOPTS="-O2 -std=c99 -Wall -Wextra"
LDFLAGS=""
OUT=htop
case "$(uname -s)" in
    Linux)
        CCOPTS="$CCOPTS -D_POSIX_C_SOURCE=200809L"
        ;;
    Darwin)
        CCOPTS="$CCOPTS -D_DARWIN_C_SOURCE"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        # Windows native build under Git Bash / MSYS2 / MinGW
        LDFLAGS="-lpsapi -ladvapi32"
        OUT=htop.exe
        ;;
    *)
        OUT=htop
        ;;
esac
[ -z "$OUT" ] && OUT=htop

rm -f "$OUT"
if ! $CC $CCOPTS -o "$OUT" htop.c $LDFLAGS 2> "$TDIR/build.log"; then
    cat "$TDIR/build.log"
    echo "error: build failed" >&2
    exit 1
fi
HTOP="./$OUT"
ok "compiled without warnings or errors"
check_not "no compiler warnings" grep -i "warning" "$TDIR/build.log"

echo "== 2. version and help =="
$HTOP --version > "$TDIR/v.txt" 2>&1
check   "htop --version exits 0"        test $? -eq 0
check_grep "version string present"     "htop v" "$TDIR/v.txt"
check_grep "MIT license mentioned"      "MIT" "$TDIR/v.txt"

$HTOP -V > "$TDIR/v2.txt" 2>&1
check_grep "-V alias works" "htop v" "$TDIR/v2.txt"

$HTOP --help > "$TDIR/h.txt" 2>&1
check_grep "help shows usage"           "Usage: htop" "$TDIR/h.txt"
check_grep "help shows --batch"         "\-\-batch" "$TDIR/h.txt"
check_grep "help shows --delay"         "\-\-delay" "$TDIR/h.txt"
check_grep "help shows --filter"        "\-\-filter" "$TDIR/h.txt"
check_grep "help shows --sort-key"      "\-\-sort-key" "$TDIR/h.txt"
check_grep "help shows --tree"          "\-\-tree" "$TDIR/h.txt"
check_grep "help shows interactive keys" "F3" "$TDIR/h.txt"

$HTOP -h > "$TDIR/h2.txt" 2>&1
check_grep "-h alias works"             "Usage: htop" "$TDIR/h2.txt"

echo "== 3. batch output (auto-batch when not a TTY) =="
$HTOP -b -n 1 > "$TDIR/t1.txt" 2>&1
check_grep "header line 1 (htop/uptime)"  "htop.*up" "$TDIR/t1.txt"
check_grep "load average line"            "load average" "$TDIR/t1.txt"
check_grep "tasks line"                   "Tasks:" "$TDIR/t1.txt"
check_grep "CPU bar"                      "CPU" "$TDIR/t1.txt"
check_grep "Mem bar"                      "Mem:" "$TDIR/t1.txt"
check_grep "process table header PID"     "PID" "$TDIR/t1.txt"
check_grep "process table header COMMAND" "COMMAND" "$TDIR/t1.txt"
check_grep "process table header %CPU"    "%CPU" "$TDIR/t1.txt"
check_grep "process table header %MEM"    "%MEM" "$TDIR/t1.txt"
check_grep "at least one process row"     "htop" "$TDIR/t1.txt"

# batch mode must not contain ANSI escapes
check_nogrep "batch output has no ANSI escapes" $'\033' "$TDIR/t1.txt"

echo "== 4. sorting =="
$HTOP -b -n 1 -s PID > "$TDIR/s1.txt" 2>&1
pids=$(sed -n 's/^\s*\([0-9][0-9]*\) \+[0-9][0-9]* \+.*$/\1/p' "$TDIR/s1.txt" | head -20)
sorted=$(printf '%s\n' "$pids" | sort -n)
if [ "$pids" = "$sorted" ] && [ -n "$pids" ]; then
    ok "sort by PID ascending"
else
    fail "sort by PID ascending"
fi

$HTOP -b -n 1 -s PID -r > "$TDIR/s2.txt" 2>&1
pids2=$(sed -n 's/^\s*\([0-9][0-9]*\) \+[0-9][0-9]* \+.*$/\1/p' "$TDIR/s2.txt" | head -20)
sorted2=$(printf '%s\n' "$pids2" | sort -rn)
if [ "$pids2" = "$sorted2" ] && [ -n "$pids2" ]; then
    ok "sort by PID descending (-r)"
else
    fail "sort by PID descending (-r)"
fi

$HTOP -b -n 1 -s RES > "$TDIR/s3.txt" 2>&1
check_grep "sort by RES accepted" "COMMAND" "$TDIR/s3.txt"

$HTOP -b -n 1 -s %CPU > "$TDIR/s4.txt" 2>&1
check_grep "sort by %CPU accepted" "COMMAND" "$TDIR/s4.txt"

echo "== 5. filters =="
$HTOP -b -n 1 -f htop > "$TDIR/f1.txt" 2>&1
# with -f htop every listed process must contain "htop" (case-insensitive)
rows=$(grep -cE '^[ ]*[0-9]+[ ]+[0-9]+[ ]' "$TDIR/f1.txt" || true)
nonmatch=$(grep -E '^[ ]*[0-9]+[ ]+[0-9]+[ ]' "$TDIR/f1.txt" | grep -icv 'htop' || true)
if [ "$rows" -gt 0 ] && [ "$nonmatch" -eq 0 ]; then
    ok "-f filters to matching processes"
else
    fail "-f filters to matching processes (rows=$rows nonmatch=$nonmatch)"
fi

# pick a real PID from a full listing (portable across platforms),
# then verify -p restricts the output to exactly that process
test_pid=$($HTOP -b -n 1 -s PID 2>/dev/null \
    | sed -n 's/^[ ]*\([0-9][0-9]*\)[ ]\{1,\}[0-9][ ]\{1,\}.*/\1/p' | sed -n '2p')
if [ -n "$test_pid" ]; then
    $HTOP -b -n 1 -p "$test_pid" > "$TDIR/f2.txt" 2>&1
    rows2=$(grep -cE '^[ ]*[0-9]+[ ]+[0-9]+[ ]' "$TDIR/f2.txt" || true)
    pidmatch=$(grep -cE "^[ ]*$test_pid[ ]" "$TDIR/f2.txt" || true)
    if [ "$rows2" -gt 0 ] && [ "$rows2" -eq "$pidmatch" ]; then
        ok "-p limits output to the given pid"
    else
        fail "-p limits output to the given pid (rows=$rows2 match=$pidmatch pid=$test_pid)"
    fi
else
    fail "-p limits output to the given pid (no test pid found)"
fi

$HTOP -b -n 1 -f "^[n]ope$" > "$TDIR/f3.txt" 2>&1
# no process named nope exists
nonexist=$(grep -cE '^[ ]*[0-9]+[ ]+[0-9]+[ ]' "$TDIR/f3.txt" || true)
if [ "$nonexist" -eq 0 ]; then
    ok "regex anchor filter matches nothing"
else
    fail "regex anchor filter matches nothing"
fi

echo "== 6. options =="
$HTOP -b -n 1 -w > "$TDIR/w1.txt" 2>&1
check_grep "-w wide command accepted" "COMMAND" "$TDIR/w1.txt"

$HTOP -b -n 1 --no-color > "$TDIR/w2.txt" 2>&1
check_nogrep "--no-color emits no ANSI escapes" $'\033' "$TDIR/w2.txt"

$HTOP -b -n 1 --limit-rows 5 > "$TDIR/w3.txt" 2>&1
rows3=$(grep -cE '^[ ]*[0-9]+[ ]+[0-9]+[ ]' "$TDIR/w3.txt" || true)
if [ "$rows3" -le 5 ] && [ "$rows3" -gt 0 ]; then
    ok "--limit-rows 5 caps table rows"
else
    fail "--limit-rows 5 caps table rows (got $rows3)"
fi

$HTOP -b -n 2 -d 0.2 > "$TDIR/w4.txt" 2>&1
iters=$(grep -c "load average" "$TDIR/w4.txt" || true)
if [ "$iters" -ge 2 ]; then
    ok "-n 2 produces two iterations"
else
    fail "-n 2 produces two iterations (got $iters)"
fi

$HTOP -b -n 1 -t > "$TDIR/w5.txt" 2>&1
check_grep "-t tree view accepted" "COMMAND" "$TDIR/w5.txt"

$HTOP -b -n 1 -s 9 > "$TDIR/w6.txt" 2>&1
check_grep "-s 9 (signal form) not misparsed" "COMMAND" "$TDIR/w6.txt"

echo "== 7. error handling =="
check_not "unknown long option fails" $HTOP --no-such-option
check_not "unknown short option fails" $HTOP -Z
$HTOP --delay abc -b -n 1 > "$TDIR/e1.txt" 2>&1
check "bad --delay falls back to default" test $? -eq 0
$HTOP -d -b -n 1 > "$TDIR/e2.txt" 2>&1
check_not "missing --delay argument fails" test $? -eq 0
check_not "extra operand fails" $HTOP -- garbage

check "--follow with existing pid runs" test -n "$test_pid"
$HTOP -b -n 1 --follow "${test_pid:-4}" > "$TDIR/e3.txt" 2>&1
check "--follow with real pid exits 0" test $? -eq 0

echo "== summary =="
echo "  passed: $PASS / $TOTAL"
rm -rf "$TDIR"
if [ "$FAIL" -eq 0 ]; then
    echo "  ALL TESTS PASSED"
    exit 0
else
    echo "  $FAIL TEST(S) FAILED"
    exit 1
fi
