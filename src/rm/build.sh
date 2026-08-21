#!/bin/bash
set -e

echo "============================================"
echo "     rm.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="rm"
SOURCE="rm.c"
PASS=0
FAIL=0

# Detect platform and set flags
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

echo ""
echo "[1/3] Cleaning previous build..."
rm -f "$OUTPUT"
echo "  Removed $OUTPUT"

echo ""
echo "[2/3] Detecting platform and compiling..."
echo "  Platform: $PLATFORM"
echo "  Compiler: $CC $CFLAGS $EXTRA_FLAGS"
echo "  Command:  $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE"

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"

echo ""
echo "============================================"
echo "  Running full functional tests..."
echo "============================================"

# Helper
check() { if [ "$1" -eq 0 ]; then PASS=$((PASS+1)); echo "  [PASS] $2"; else FAIL=$((FAIL+1)); echo "  [FAIL] $2"; fi; }

# Setup
TDIR="/tmp/rm_fulltest_$$"
rm -rf "$TDIR"
mkdir -p "$TDIR"

echo ""
echo "--- Test 01: Basic file removal ---"
echo "Hello" > "$TDIR/f1.txt"
./"$OUTPUT" "$TDIR/f1.txt" > /dev/null 2>&1
[ ! -f "$TDIR/f1.txt" ]
check $? "basic file removal"

echo ""
echo "--- Test 02: Multiple file removal ---"
echo "A" > "$TDIR/a.txt"
echo "B" > "$TDIR/b.txt"
echo "C" > "$TDIR/c.txt"
./"$OUTPUT" "$TDIR/a.txt" "$TDIR/b.txt" "$TDIR/c.txt" > /dev/null 2>&1
[ ! -f "$TDIR/a.txt" ] && [ ! -f "$TDIR/b.txt" ] && [ ! -f "$TDIR/c.txt" ]
check $? "multiple files"

echo ""
echo "--- Test 03: -v verbose ---"
echo "V" > "$TDIR/v.txt"
./"$OUTPUT" -v "$TDIR/v.txt" > "$TDIR/vout.txt" 2>&1
grep -q "removed" "$TDIR/vout.txt" && [ ! -f "$TDIR/v.txt" ]
check $? "-v verbose"

echo ""
echo "--- Test 04: --verbose (long) ---"
echo "VL" > "$TDIR/vl.txt"
./"$OUTPUT" --verbose "$TDIR/vl.txt" > "$TDIR/vlout.txt" 2>&1
grep -q "removed" "$TDIR/vlout.txt" && [ ! -f "$TDIR/vl.txt" ]
check $? "--verbose long option"

echo ""
echo "--- Test 05: -f force nonexistent ---"
./"$OUTPUT" -f "$TDIR/nonexistent.txt" > /dev/null 2>&1
check $? "-f force nonexistent"

echo ""
echo "--- Test 06: --force (long) nonexistent ---"
./"$OUTPUT" --force "$TDIR/nonexistent2.txt" > /dev/null 2>&1
check $? "--force long option"

echo ""
echo "--- Test 07: Nonexistent without -f errors ---"
./"$OUTPUT" "$TDIR/nonexist3.txt" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "nonexistent without -f errors"

echo ""
echo "--- Test 08: -f cancels -i ---"
echo "FCI" > "$TDIR/fci.txt"
echo "n" | ./"$OUTPUT" -if "$TDIR/fci.txt" > /dev/null 2>&1
[ ! -f "$TDIR/fci.txt" ]
check $? "-f cancels -i"

echo ""
echo "--- Test 09: -d empty directory ---"
mkdir -p "$TDIR/emptydir"
./"$OUTPUT" -d "$TDIR/emptydir" > /dev/null 2>&1
[ ! -d "$TDIR/emptydir" ]
check $? "-d empty dir"

echo ""
echo "--- Test 10: --dir (long) empty directory ---"
mkdir -p "$TDIR/emptydir2"
./"$OUTPUT" --dir "$TDIR/emptydir2" > /dev/null 2>&1
[ ! -d "$TDIR/emptydir2" ]
check $? "--dir long option"

echo ""
echo "--- Test 11: -d non-empty directory errors ---"
mkdir -p "$TDIR/nonempty"
echo "hi" > "$TDIR/nonempty/inner.txt"
./"$OUTPUT" -d "$TDIR/nonempty" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "-d non-empty errors"
./"$OUTPUT" -rf "$TDIR/nonempty" > /dev/null 2>&1

echo ""
echo "--- Test 12: Directory without -r/-d errors ---"
mkdir -p "$TDIR/dironly"
./"$OUTPUT" "$TDIR/dironly" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "dir without -r/-d errors"
./"$OUTPUT" -rf "$TDIR/dironly" > /dev/null 2>&1

echo ""
echo "--- Test 13: -r recursive removal ---"
mkdir -p "$TDIR/rdir/sub1/sub2"
echo "F1" > "$TDIR/rdir/file1.txt"
echo "F2" > "$TDIR/rdir/sub1/file2.txt"
echo "F3" > "$TDIR/rdir/sub1/sub2/file3.txt"
./"$OUTPUT" -r "$TDIR/rdir" > /dev/null 2>&1
[ ! -d "$TDIR/rdir" ]
check $? "-r recursive"

echo ""
echo "--- Test 14: -R uppercase recursive ---"
mkdir -p "$TDIR/Rdir/sub"
echo "F" > "$TDIR/Rdir/f.txt"
./"$OUTPUT" -R "$TDIR/Rdir" > /dev/null 2>&1
[ ! -d "$TDIR/Rdir" ]
check $? "-R uppercase recursive"

echo ""
echo "--- Test 15: --recursive (long) ---"
mkdir -p "$TDIR/recdir"
echo "F" > "$TDIR/recdir/f.txt"
./"$OUTPUT" --recursive "$TDIR/recdir" > /dev/null 2>&1
[ ! -d "$TDIR/recdir" ]
check $? "--recursive long option"

echo ""
echo "--- Test 16: -rf combined ---"
mkdir -p "$TDIR/rfdir"
echo "F" > "$TDIR/rfdir/f.txt"
./"$OUTPUT" -rf "$TDIR/rfdir" > /dev/null 2>&1
[ ! -d "$TDIR/rfdir" ]
check $? "-rf combined"

echo ""
echo "--- Test 17: -fr combined (order independent) ---"
mkdir -p "$TDIR/frdir"
echo "F" > "$TDIR/frdir/f.txt"
./"$OUTPUT" -fr "$TDIR/frdir" > /dev/null 2>&1
[ ! -d "$TDIR/frdir" ]
check $? "-fr combined"

echo ""
echo "--- Test 18: -rv verbose recursive ---"
mkdir -p "$TDIR/rvdir"
echo "F" > "$TDIR/rvdir/f.txt"
./"$OUTPUT" -rv "$TDIR/rvdir" > "$TDIR/rvout.txt" 2>&1
grep -q "removed" "$TDIR/rvout.txt" && [ ! -d "$TDIR/rvdir" ]
check $? "-rv verbose recursive"

echo ""
echo "--- Test 19: Symlink removal (removes link, not target) ---"
echo "target content" > "$TDIR/target.txt"
ln -s "$TDIR/target.txt" "$TDIR/link.txt"
./"$OUTPUT" "$TDIR/link.txt" > /dev/null 2>&1
[ ! -L "$TDIR/link.txt" ] && [ -f "$TDIR/target.txt" ]
check $? "symlink removal preserves target"
./"$OUTPUT" -f "$TDIR/target.txt" > /dev/null 2>&1

echo ""
echo "--- Test 20: -i interactive yes ---"
echo "IY" > "$TDIR/iy.txt"
echo "y" | ./"$OUTPUT" -i "$TDIR/iy.txt" > /dev/null 2>&1
[ ! -f "$TDIR/iy.txt" ]
check $? "-i yes removes"

echo ""
echo "--- Test 21: -i interactive no ---"
echo "IN" > "$TDIR/in.txt"
echo "n" | ./"$OUTPUT" -i "$TDIR/in.txt" > /dev/null 2>&1
[ -f "$TDIR/in.txt" ]
check $? "-i no keeps file"
./"$OUTPUT" -f "$TDIR/in.txt" > /dev/null 2>&1

echo ""
echo "--- Test 22: --interactive=always ---"
echo "IA" > "$TDIR/ialways.txt"
echo "y" | ./"$OUTPUT" --interactive=always "$TDIR/ialways.txt" > /dev/null 2>&1
[ ! -f "$TDIR/ialways.txt" ]
check $? "--interactive=always"

echo ""
echo "--- Test 23: --interactive=never ---"
echo "INE" > "$TDIR/inever.txt"
./"$OUTPUT" --interactive=never "$TDIR/inever.txt" > /dev/null 2>&1
[ ! -f "$TDIR/inever.txt" ]
check $? "--interactive=never"

echo ""
echo "--- Test 24: --interactive=invalid errors ---"
./"$OUTPUT" --interactive=invalid "$TDIR/x.txt" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "--interactive=invalid errors"

echo ""
echo "--- Test 25: --interactive (no value = always) ---"
echo "IAL" > "$TDIR/ival.txt"
echo "y" | ./"$OUTPUT" --interactive "$TDIR/ival.txt" > /dev/null 2>&1
[ ! -f "$TDIR/ival.txt" ]
check $? "--interactive (no value = always)"

echo ""
echo "--- Test 26: -I prompt yes (>3 files) ---"
echo "I1" > "$TDIR/i1.txt"
echo "I2" > "$TDIR/i2.txt"
echo "I3" > "$TDIR/i3.txt"
echo "I4" > "$TDIR/i4.txt"
echo "y" | ./"$OUTPUT" -I "$TDIR/i1.txt" "$TDIR/i2.txt" "$TDIR/i3.txt" "$TDIR/i4.txt" > /dev/null 2>&1
[ ! -f "$TDIR/i1.txt" ] && [ ! -f "$TDIR/i2.txt" ] && [ ! -f "$TDIR/i3.txt" ] && [ ! -f "$TDIR/i4.txt" ]
check $? "-I prompt yes"

echo ""
echo "--- Test 27: -I prompt no (>3 files, keeps) ---"
echo "J1" > "$TDIR/j1.txt"
echo "J2" > "$TDIR/j2.txt"
echo "J3" > "$TDIR/j3.txt"
echo "J4" > "$TDIR/j4.txt"
echo "n" | ./"$OUTPUT" -I "$TDIR/j1.txt" "$TDIR/j2.txt" "$TDIR/j3.txt" "$TDIR/j4.txt" > /dev/null 2>&1
[ -f "$TDIR/j1.txt" ] && [ -f "$TDIR/j2.txt" ] && [ -f "$TDIR/j3.txt" ] && [ -f "$TDIR/j4.txt" ]
check $? "-I prompt no keeps files"
./"$OUTPUT" -f "$TDIR/j1.txt" "$TDIR/j2.txt" "$TDIR/j3.txt" "$TDIR/j4.txt" > /dev/null 2>&1

echo ""
echo "--- Test 28: -I no prompt for <=3 files ---"
echo "K1" > "$TDIR/k1.txt"
echo "K2" > "$TDIR/k2.txt"
echo "K3" > "$TDIR/k3.txt"
./"$OUTPUT" -I "$TDIR/k1.txt" "$TDIR/k2.txt" "$TDIR/k3.txt" < /dev/null > /dev/null 2>&1
[ ! -f "$TDIR/k1.txt" ] && [ ! -f "$TDIR/k2.txt" ] && [ ! -f "$TDIR/k3.txt" ]
check $? "-I no prompt <=3 files"

echo ""
echo "--- Test 29: -I with -r prompts ---"
mkdir -p "$TDIR/iRdir"
echo "F" > "$TDIR/iRdir/f.txt"
echo "y" | ./"$OUTPUT" -Ir "$TDIR/iRdir" > /dev/null 2>&1
[ ! -d "$TDIR/iRdir" ]
check $? "-I with -r prompts"

echo ""
echo "--- Test 30: --interactive=once ---"
mkdir -p "$TDIR/ionce"
echo "F1" > "$TDIR/ionce/f1.txt"
echo "F2" > "$TDIR/ionce/f2.txt"
echo "F3" > "$TDIR/ionce/f3.txt"
echo "F4" > "$TDIR/ionce/f4.txt"
echo "y" | ./"$OUTPUT" --interactive=once -r "$TDIR/ionce" > /dev/null 2>&1
[ ! -d "$TDIR/ionce" ]
check $? "--interactive=once"

echo ""
echo "--- Test 31: --version ---"
./"$OUTPUT" --version 2>&1 | grep -q "1.0.0"
check $? "--version"

echo ""
echo "--- Test 32: --help ---"
./"$OUTPUT" --help 2>&1 | grep -q "Usage:"
check $? "--help"

echo ""
echo "--- Test 33: -h help ---"
./"$OUTPUT" -h 2>&1 | grep -q "Usage:"
check $? "-h help"

echo ""
echo "--- Test 34: Root protection (refuse /) ---"
./"$OUTPUT" / > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "root protection /"

echo ""
echo "--- Test 35: --preserve-root (default) ---"
./"$OUTPUT" --preserve-root / > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "--preserve-root refuse /"

echo ""
echo "--- Test 36: --no-preserve-root bypasses root check ---"
./"$OUTPUT" --no-preserve-root / 2>&1 | grep -q "dangerous" && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "--no-preserve-root"

echo ""
echo "--- Test 37: --preserve-root=all accepted ---"
echo "PA" > "$TDIR/pa.txt"
./"$OUTPUT" --preserve-root=all "$TDIR/pa.txt" > /dev/null 2>&1
[ ! -f "$TDIR/pa.txt" ]
check $? "--preserve-root=all accepted"

echo ""
echo "--- Test 38: --preserve-root=invalid errors ---"
./"$OUTPUT" --preserve-root=invalid "$TDIR/x.txt" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "--preserve-root=invalid errors"

echo ""
echo "--- Test 39: Refuse to remove '.' ---"
./"$OUTPUT" . 2> /dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "refuse to remove '.'"

echo ""
echo "--- Test 40: Refuse to remove '..' ---"
./"$OUTPUT" .. 2> /dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "refuse to remove '..'"

echo ""
echo "--- Test 41: Refusal message for './.' ---"
./"$OUTPUT" ./. 2> "$TDIR/dotmsg.txt"
grep -q "refusing" "$TDIR/dotmsg.txt"
check $? "refusal message for '.'"

echo ""
echo "--- Test 42: Missing operand errors ---"
./"$OUTPUT" 2> /dev/null && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "missing operand errors"

echo ""
echo "--- Test 43: -f missing operand OK ---"
./"$OUTPUT" -f 2> /dev/null
check $? "-f missing operand OK"

echo ""
echo "--- Test 44: Invalid option errors ---"
./"$OUTPUT" -Z "$TDIR/x.txt" > /dev/null 2>&1 && rc=0 || rc=1
[ "$rc" -ne 0 ]
check $? "invalid option errors"

echo ""
echo "--- Test 45: -- separator ---"
echo "W" > "$TDIR/--weird.txt"
./"$OUTPUT" -- "$TDIR/--weird.txt" > /dev/null 2>&1
[ ! -f "$TDIR/--weird.txt" ]
check $? "-- separator"

echo ""
echo "--- Test 46: Deep nested removal ---"
mkdir -p "$TDIR/deep/d1/d2/d3/d4"
echo "F" > "$TDIR/deep/d1/d2/d3/d4/f.txt"
./"$OUTPUT" -rf "$TDIR/deep" > /dev/null 2>&1
[ ! -d "$TDIR/deep" ]
check $? "deep nested removal"

echo ""
echo "--- Test 47: -ri interactive recursive ---"
mkdir -p "$TDIR/idir"
echo "F1" > "$TDIR/idir/f1.txt"
echo "F2" > "$TDIR/idir/f2.txt"
printf "y\ny\ny\n" | ./"$OUTPUT" -ri "$TDIR/idir" > /dev/null 2>&1
[ ! -d "$TDIR/idir" ]
check $? "-ri interactive recursive"

echo ""
echo "--- Test 48: --one-file-system accepted ---"
echo "OFS" > "$TDIR/ofs.txt"
./"$OUTPUT" --one-file-system "$TDIR/ofs.txt" > /dev/null 2>&1
[ ! -f "$TDIR/ofs.txt" ]
check $? "--one-file-system accepted"

echo ""
echo "--- Test 49: --one-file-system recursive (same fs) ---"
mkdir -p "$TDIR/ofsdir/sub"
echo "F" > "$TDIR/ofsdir/sub/f.txt"
./"$OUTPUT" -r --one-file-system "$TDIR/ofsdir" > /dev/null 2>&1
[ ! -d "$TDIR/ofsdir" ]
check $? "--one-file-system same fs recursive"

echo ""
echo "--- Test 50: Mixed long options ---"
mkdir -p "$TDIR/mixdir"
echo "F" > "$TDIR/mixdir/f.txt"
./"$OUTPUT" --recursive --force --verbose "$TDIR/mixdir" > "$TDIR/mixout.txt" 2>&1
grep -q "removed" "$TDIR/mixout.txt" && [ ! -d "$TDIR/mixdir" ]
check $? "mixed long options"

echo ""
echo "--- Test 51: Nested verbose recursive (directory message) ---"
mkdir -p "$TDIR/vdr/sub"
echo "F" > "$TDIR/vdr/sub/file.txt"
./"$OUTPUT" -rv "$TDIR/vdr" > "$TDIR/vdrout.txt" 2>&1
grep -q "removed directory" "$TDIR/vdrout.txt" && [ ! -d "$TDIR/vdr" ]
check $? "nested -rv verbose dir"

echo ""
echo "--- Test 52: Recursive with symlink inside (removes link) ---"
mkdir -p "$TDIR/rsymdir"
echo "tgt" > "$TDIR/rsymdir/target.txt"
ln -s "target.txt" "$TDIR/rsymdir/link.txt"
./"$OUTPUT" -rf "$TDIR/rsymdir" > /dev/null 2>&1
[ ! -d "$TDIR/rsymdir" ]
check $? "recursive with symlink inside"

# Cleanup
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
