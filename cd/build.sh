#!/bin/bash
set -e

echo "============================================"
echo "     cd.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="cd"
SOURCE="cd.c"
PASS=0
FAIL=0

PLATFORM="unknown"
EXTRA_FLAGS=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    PLATFORM="$ID"
elif [ "$(uname)" = "Darwin" ]; then
    PLATFORM="macos"
elif [ "$(uname)" = "FreeBSD" ]; then
    PLATFORM="freebsd"
elif [ "$(uname)" = "OpenBSD" ]; then
    PLATFORM="openbsd"
elif [ "$(uname)" = "NetBSD" ]; then
    PLATFORM="netbsd"
elif [ "$(uname)" = "Linux" ]; then
    PLATFORM="linux"
fi
case "$PLATFORM" in
    linux)        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L" ;;
    macos|darwin) EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; CC="gcc" ;;
    freebsd)      EXTRA_FLAGS="" ;;
    openbsd)      EXTRA_FLAGS="" ;;
    netbsd)       EXTRA_FLAGS="-D_NETBSD_SOURCE" ;;
    *)            EXTRA_FLAGS="" ;;
esac

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
EXE="$SCRIPT_DIR/$OUTPUT"
TDIR="$SCRIPT_DIR/_build_test"
rm -rf "$TDIR" "$OUTPUT"
mkdir -p "$TDIR" "$TDIR/sub1" "$TDIR/sub2"

echo ""
echo "[1/3] Cleaning previous build..."
echo "  Removed $OUTPUT"

echo ""
echo "[2/3] Detecting platform and compiling..."
echo "  Platform: $PLATFORM"
echo "  Compiler: $CC $CFLAGS $EXTRA_FLAGS"
echo "  Command:  $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
( cd "$SCRIPT_DIR" && $CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE" )

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $EXE"

echo ""
echo "============================================"
echo "  Running basic tests..."
echo "============================================"

check() { if [ "$1" -eq 0 ]; then PASS=$((PASS+1)); echo "  [PASS] $2"; else FAIL=$((FAIL+1)); echo "  [FAIL] $2"; fi; }

echo ""
echo "--- Test 1: help ---"
"$EXE" --help > "$TDIR/t1.txt" 2>&1
grep -q "Usage" "$TDIR/t1.txt"
check $? "help mentions Usage"

echo ""
echo "--- Test 2: version ---"
"$EXE" --version > "$TDIR/t2.txt" 2>&1
grep -q "1.0.0" "$TDIR/t2.txt"
check $? "version contains 1.0.0"

echo ""
echo "--- Test 3: relative dir sub1 ---"
(
    cd "$TDIR"
    out=$("$EXE" sub1)
    case "$out" in */sub1) exit 0;; *) exit 1;; esac
)
check $? "relative to sub1"

echo ""
echo "--- Test 4: absolute target ---"
(
    out=$("$EXE" "$TDIR/sub2")
    case "$out" in */sub2) exit 0;; *) exit 1;; esac
)
check $? "absolute to sub2"

echo ""
echo "--- Test 5: tilde to HOME ---"
(
    out=$("$EXE" "~")
    home_norm=$(printf '%s' "$HOME" | sed 's:/*$::')
    out_norm=$(printf '%s' "$out" | sed 's:/*$::')
    [ "$out_norm" = "$home_norm" ]
)
check $? "tilde resolves HOME"

echo ""
echo "--- Test 6: -P physical mode ---"
(
    cd "$TDIR"
    out=$("$EXE" -P sub1)
    case "$out" in */sub1) exit 0;; *) exit 1;; esac
)
check $? "-P physical sub1"

echo ""
echo "--- Test 7: -e option accepted ---"
(
    cd "$TDIR"
    "$EXE" -e -P sub1 >/dev/null 2>&1
)
check $? "-e -P does not error"

echo ""
echo "--- Test 8: not a directory ---"
touch "$TDIR/file.txt"
"$EXE" "$TDIR/file.txt" >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "file target returns non-zero"

echo ""
echo "--- Test 9: nonexistent dir ---"
"$EXE" "$TDIR/no_such_dir_xyz" >/dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "nonexistent dir returns non-zero"

echo ""
echo "--- Test 10: unknown short option returns 2 ---"
"$EXE" -Z >/dev/null 2>&1 ; [ "$?" -eq 2 ]
check $? "unknown short option -> exit 2"

echo ""
echo "--- Test 11: unknown long option returns 2 ---"
"$EXE" --nosuchoption >/dev/null 2>&1 ; [ "$?" -eq 2 ]
check $? "unknown long option -> exit 2"

echo ""
echo "--- Test 12: logical collapse .. ---"
(
    cd "$TDIR"
    out=$("$EXE" "sub1/../sub2")
    case "$out" in
        *..*) exit 1;;
        */sub2) exit 0;;
        *) exit 1;;
    esac
)
check $? "-L collapses '..' without symlinks"

echo ""
echo "--- Test 13: \".\" stays ---"
(
    cd "$TDIR"
    out=$("$EXE" ".")
    norm=$(printf '%s' "$TDIR" | sed 's:/*$::')
    onorm=$(printf '%s' "$out" | sed 's:/*$::')
    [ "$onorm" = "$norm" ]
)
check $? "dot stays same dir"

echo ""
echo "--- Test 14: -P vs -L symlink ---"
(
    cd "$TDIR"
    ln -sf sub1 linkdir 2>/dev/null || exit 77
    out_L=$("$EXE" -L linkdir)
    out_P=$("$EXE" -P linkdir)
    case "$out_L" in
        *linkdir) has_link=0;;
        *) has_link=1;;
    esac
    case "$out_P" in
        *sub1) has_sub1=0;;
        *) has_sub1=1;;
    esac
    case "$out_P" in
        *linkdir) has_link_in_P=0;;
        *) has_link_in_P=1;;
    esac
    [ "$has_link" -eq 0 ] && [ "$has_sub1" -eq 0 ] && [ "$has_link_in_P" -eq 1 ]
)
rc=$?
if [ $rc -eq 77 ]; then
    check 0 "symlink (skipped, no symlink support)"
else
    check $rc "-L keeps symlink; -P resolves to real dir"
fi

echo ""
echo "--- Test 15: last flag wins LP then PL ---"
(
    cd "$TDIR"
    a=$("$EXE" -L -P "sub1/../sub2")
    b=$("$EXE" -P "sub2")
    [ "$a" = "$b" ]
)
check_a=$?
(
    cd "$TDIR"
    out=$("$EXE" -P -L "sub1/../sub2")
    case "$out" in
        *..*) exit 1;;
        */sub2) exit 0;;
        *) exit 1;;
    esac
)
check_b=$?
[ $check_a -eq 0 ] && [ $check_b -eq 0 ]
check $? "last of -L/-P wins"

rm -rf "$TDIR"

echo ""
echo "============================================"
echo "  Test Results: PASS=$PASS  FAIL=$FAIL"
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo "  All tests passed!"
    exit 0
else
    echo "  Some tests failed!"
    exit 1
fi
