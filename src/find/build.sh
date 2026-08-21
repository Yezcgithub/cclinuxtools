#!/bin/bash
set -e

echo "============================================"
echo "     find.c Build Script for Unix/Linux/macOS"
echo "============================================"

CC="cc"
CFLAGS="-O2 -std=c99 -Wall -Wextra"
OUTPUT="find"
SOURCE="find.c"

echo ""
echo "[1/3] Cleaning previous build..."
if [ -f "$OUTPUT" ]; then
    rm -f "$OUTPUT"
    echo "  Removed $OUTPUT"
fi

echo ""
echo "[2/3] Detecting platform and compiling..."

# Detect platform and set appropriate flags
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

echo "  Platform: $PLATFORM"

case "$PLATFORM" in
    linux)
        EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L"
        ;;
    macos|darwin)
        EXTRA_FLAGS="-D_DARWIN_C_SOURCE"
        CC="gcc"
        ;;
    freebsd)
        EXTRA_FLAGS=""
        ;;
    openbsd)
        EXTRA_FLAGS=""
        ;;
    netbsd)
        EXTRA_FLAGS="-D_NETBSD_SOURCE"
        ;;
    *)
        EXTRA_FLAGS=""
        ;;
esac

echo "  Compiler: $CC"
echo "  Flags: $CFLAGS $EXTRA_FLAGS"
echo ""
echo "  Command: $CC $CFLAGS $EXTRA_FLAGS -o $OUTPUT $SOURCE"
echo ""
$CC $CFLAGS $EXTRA_FLAGS -o "$OUTPUT" "$SOURCE"

echo ""
echo "[3/3] Build succeeded!"
echo "  Output: $(pwd)/$OUTPUT"
echo ""
echo "============================================"
echo "  Testing basic functionality..."
echo "============================================"
echo ""
./"$OUTPUT" --version
echo ""
./"$OUTPUT" . -maxdepth 1 -name "*.c"