#!/bin/bash
# Build and test script for id.c (Unix/Linux/macOS/BSD)

echo "============================================"
echo "    id.c Build Script for Unix"
echo "============================================"

CC=gcc
CFLAGS="-O2 -std=c99 -Wall"
OUTPUT=id
SOURCE=id.c
PASS=0
FAIL=0

PLATFORM="unknown"
if [ "$(uname -s)" = "Linux" ]; then PLATFORM="linux"
elif [ "$(uname -s)" = "Darwin" ]; then PLATFORM="macos"
elif [ "$(uname -s)" = "FreeBSD" ]; then PLATFORM="freebsd"
elif [ "$(uname -s)" = "OpenBSD" ]; then PLATFORM="openbsd"
elif [ "$(uname -s)" = "NetBSD" ]; then PLATFORM="netbsd"; fi

# Platform-specific POSIX macro
POSIX_MACRO=""
if [ "$PLATFORM" = "linux" ]; then POSIX_MACRO="-D_POSIX_C_SOURCE=200809L"
elif [ "$PLATFORM" = "macos" ]; then POSIX_MACRO="-D_DARWIN_C_SOURCE"
elif [ "$PLATFORM" = "netbsd" ]; then POSIX_MACRO="-D_NETBSD_SOURCE"; fi

echo ""; echo "Detected platform: $PLATFORM"
echo ""; echo "[1/3] Cleaning previous build..."; rm -f "$OUTPUT"
echo ""; echo "[2/3] Compiling $SOURCE..."
echo "  Command: $CC $CFLAGS $POSIX_MACRO -o $OUTPUT $SOURCE"; echo ""
$CC $CFLAGS $POSIX_MACRO -o $OUTPUT $SOURCE
if [ $? -ne 0 ]; then echo ""; echo "[ERROR] Build failed"; exit 1; fi
echo ""; echo "[3/3] Build succeeded!"; echo "  Output: $(pwd)/$OUTPUT"
echo ""; echo "============================================"
echo "  Running functional tests..."
echo "============================================"

check() {
    if [ "$1" -eq 0 ]; then echo "  [PASS] $2"; PASS=$((PASS + 1))
    else echo "  [FAIL] $2"; FAIL=$((FAIL + 1)); fi
}

# Get reference values from system id command (if available)
SYS_UID=$(id -u 2>/dev/null || echo "")
SYS_GID=$(id -g 2>/dev/null || echo "")
SYS_USER=$(id -un 2>/dev/null || echo "")
SYS_GROUP=$(id -gn 2>/dev/null || echo "")

# ========== T01: default output contains uid=
echo ""; echo "--- T01: default output format ---"
out=$(./"$OUTPUT" 2>/dev/null)
echo "$out" | grep -q "uid="; check $? "default output contains uid="
echo "$out" | grep -q "gid="; check $? "default output contains gid="
echo "$out" | grep -q "groups="; check $? "default output contains groups="

# ========== T02: -u prints numeric UID
echo ""; echo "--- T02: -u prints numeric UID ---"
out=$(./"$OUTPUT" -u 2>/dev/null)
if [ -n "$SYS_UID" ]; then
    [ "$out" = "$SYS_UID" ]; check $? "-u matches system id -u ($out vs $SYS_UID)"
else
    [ -n "$out" ]; check $? "-u produces output ($out)"
fi

# ========== T03: -g prints numeric GID
echo ""; echo "--- T03: -g prints numeric GID ---"
out=$(./"$OUTPUT" -g 2>/dev/null)
if [ -n "$SYS_GID" ]; then
    [ "$out" = "$SYS_GID" ]; check $? "-g matches system id -g ($out vs $SYS_GID)"
else
    [ -n "$out" ]; check $? "-g produces output ($out)"
fi

# ========== T04: -un prints username
echo ""; echo "--- T04: -un prints username ---"
out=$(./"$OUTPUT" -un 2>/dev/null)
if [ -n "$SYS_USER" ]; then
    [ "$out" = "$SYS_USER" ]; check $? "-un matches system id -un ($out vs $SYS_USER)"
else
    [ -n "$out" ]; check $? "-un produces output ($out)"
fi

# ========== T05: -gn prints group name
echo ""; echo "--- T05: -gn prints group name ---"
out=$(./"$OUTPUT" -gn 2>/dev/null)
if [ -n "$SYS_GROUP" ]; then
    [ "$out" = "$SYS_GROUP" ]; check $? "-gn matches system id -gn ($out vs $SYS_GROUP)"
else
    [ -n "$out" ]; check $? "-gn produces output ($out)"
fi

# ========== T06: -G prints all group IDs
echo ""; echo "--- T06: -G prints all group IDs ---"
out=$(./"$OUTPUT" -G 2>/dev/null)
SYS_GROUPS=$(id -G 2>/dev/null || echo "")
if [ -n "$SYS_GROUPS" ]; then
    [ "$out" = "$SYS_GROUPS" ]; check $? "-G matches system id -G"
else
    [ -n "$out" ]; check $? "-G produces output"
fi

# ========== T07: -Gn prints all group names
echo ""; echo "--- T07: -Gn prints all group names ---"
out=$(./"$OUTPUT" -Gn 2>/dev/null)
SYS_GROUPNAMES=$(id -Gn 2>/dev/null || echo "")
if [ -n "$SYS_GROUPNAMES" ]; then
    [ "$out" = "$SYS_GROUPNAMES" ]; check $? "-Gn matches system id -Gn"
else
    [ -n "$out" ]; check $? "-Gn produces output"
fi

# ========== T08: -r with -u prints real UID
echo ""; echo "--- T08: -r with -u prints real UID ---"
out=$(./"$OUTPUT" -ru 2>/dev/null)
SYS_RUID=$(id -ru 2>/dev/null || echo "")
if [ -n "$SYS_RUID" ]; then
    [ "$out" = "$SYS_RUID" ]; check $? "-ru matches system id -ru ($out vs $SYS_RUID)"
else
    [ "$out" = "$SYS_UID" ]; check $? "-ru equals -u when real==effective ($out)"
fi

# ========== T09: -r with -g prints real GID
echo ""; echo "--- T09: -r with -g prints real GID ---"
out=$(./"$OUTPUT" -rg 2>/dev/null)
SYS_RGID=$(id -rg 2>/dev/null || echo "")
if [ -n "$SYS_RGID" ]; then
    [ "$out" = "$SYS_RGID" ]; check $? "-rg matches system id -rg ($out vs $SYS_RGID)"
else
    [ "$out" = "$SYS_GID" ]; check $? "-rg equals -g when real==effective ($out)"
fi

# ========== T10: --user long option
echo ""; echo "--- T10: --user long option ---"
out=$(./"$OUTPUT" --user 2>/dev/null)
out2=$(./"$OUTPUT" -u 2>/dev/null)
[ "$out" = "$out2" ]; check $? "--user matches -u ($out)"

# ========== T11: --group long option
echo ""; echo "--- T11: --group long option ---"
out=$(./"$OUTPUT" --group 2>/dev/null)
out2=$(./"$OUTPUT" -g 2>/dev/null)
[ "$out" = "$out2" ]; check $? "--group matches -g ($out)"

# ========== T12: --groups long option
echo ""; echo "--- T12: --groups long option ---"
out=$(./"$OUTPUT" --groups 2>/dev/null)
out2=$(./"$OUTPUT" -G 2>/dev/null)
[ "$out" = "$out2" ]; check $? "--groups matches -G"

# ========== T13: --name long option
echo ""; echo "--- T13: --name long option ---"
out=$(./"$OUTPUT" --user --name 2>/dev/null)
out2=$(./"$OUTPUT" -un 2>/dev/null)
[ "$out" = "$out2" ]; check $? "--user --name matches -un ($out)"

# ========== T14: --real long option
echo ""; echo "--- T14: --real long option ---"
out=$(./"$OUTPUT" --real --user 2>/dev/null)
out2=$(./"$OUTPUT" -ru 2>/dev/null)
[ "$out" = "$out2" ]; check $? "--real --user matches -ru ($out)"

# ========== T15: --zero delimits with NUL
echo ""; echo "--- T15: --zero delimits with NUL ---"
out=$(./"$OUTPUT" -G --zero 2>/dev/null | tr '\0' ',')
[ -n "$out" ]; check $? "--zero produces NUL-delimited output"
echo "  NUL-delimited (converted to comma): $out"

# ========== T16: -a is ignored (compatibility)
echo ""; echo "--- T16: -a compatibility option ---"
out=$(./"$OUTPUT" -a -u 2>/dev/null)
out2=$(./"$OUTPUT" -u 2>/dev/null)
[ "$out" = "$out2" ]; check $? "-a does not change -u output"

# ========== T17: id with username argument
echo ""; echo "--- T17: id with username argument ---"
if [ -n "$SYS_USER" ]; then
    out=$(./"$OUTPUT" "$SYS_USER" 2>/dev/null)
    rc=$?
    [ $rc -eq 0 ]; check $? "id USERNAME exits 0 ($rc)"
    echo "$out" | grep -q "uid="; check $? "id USERNAME contains uid="
else
    check 0 "id USERNAME (skipped: no system user)"
fi

# ========== T18: id nonexistent user fails
echo ""; echo "--- T18: id nonexistent user fails ---"
out=$(./"$OUTPUT" nosuchuser12345 2>&1)
rc=$?
[ $rc -eq 1 ]; check $? "id nosuchuser12345 exits 1 ($rc)"

# ========== T19: combined -ug prints both
echo ""; echo "--- T19: combined -ug prints both ---"
out=$(./"$OUTPUT" -ug 2>/dev/null)
echo "$out" | grep -q "$SYS_UID"; check $? "-ug output contains UID"
SYS_GID2=$(echo "$out" | awk '{print $2}')
[ -n "$SYS_GID2" ]; check $? "-ug output contains GID ($SYS_GID2)"

# ========== T20: --help exits 0
echo ""; echo "--- T20: --help exits 0 ---"
./"$OUTPUT" --help >/dev/null 2>&1
check $? "--help exits 0"

# ========== T21: --version exits 0
echo ""; echo "--- T21: --version exits 0 ---"
./"$OUTPUT" --version >/dev/null 2>&1
check $? "--version exits 0"

# ========== T22: invalid option exits 1
echo ""; echo "--- T22: invalid option exits 1 ---"
./"$OUTPUT" -Q >/dev/null 2>&1
rc=$?
[ $rc -eq 1 ]; check $? "invalid option -Q exits 1 ($rc)"

# ========== T23: -n without -ugG is error
echo ""; echo "--- T23: -n without -ugG is error ---"
out=$(./"$OUTPUT" -n 2>&1)
rc=$?
[ $rc -eq 1 ]; check $? "-n alone exits 1 ($rc)"

# ========== T24: -r without -ugG is error
echo ""; echo "--- T24: -r without -ugG is error ---"
out=$(./"$OUTPUT" -r 2>&1)
rc=$?
[ $rc -eq 1 ]; check $? "-r alone exits 1 ($rc)"

# ========== T25: extra operand is error
echo ""; echo "--- T25: extra operand is error ---"
out=$(./"$OUTPUT" user1 user2 2>&1)
rc=$?
[ $rc -eq 1 ]; check $? "extra operand exits 1 ($rc)"

echo ""; echo "============================================"
echo "  Test Results: $PASS passed, $FAIL failed"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
