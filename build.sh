#!/bin/bash
#============================
# -Project Information
#============================
# @file build.sh
# @General build script file
# @Coding format UTF-8
# @Description : Cross-platform build script for cclinuxtools
# Auto-discovers every .c file under src/ (and its subdirectories) and
# compiles each one into an executable under build/, following the
# current build layout. You can freely add or remove .c files and src
# subdirectories; they are picked up automatically on the next run.

#============================
# -License
#============================
# https://mit-license.org/
# The MIT License (MIT)
# Copyright © 2025-2026 <Yezc/cclinuxtools>
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
# and associated documentation files (the “Software”), to deal in the Software without restriction, 
# including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or 
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

set -u

#----------------------------
# - Build Configuration
#----------------------------
#  Maps any .c file under src/ to an executable under build/:
#    src/bash/bash.c   -> build/bash, build/sh
#    src/test/test.c   -> build/cmdtools/test, build/cmdtools/[
#    src/<tool>/<t>.c  -> build/cmdtools/<t>
#  Programs are built as release versions (no debug information).
PROJECT_NAME="cclinuxtools"
PROJECT_VERSION="V1.0.0"
CC=""
CFLAGS="-O2 -std=c99 -Wall -Wextra"
M32_FLAG=""
STATIC_FLAG="-static"
SPECIFY=""

# Extra flags and libraries per platform, matching the per-tool build scripts.
EXTRA_FLAGS=""
EXTRA_LIBS=""
case "$(uname -s)" in
    Linux)     EXTRA_FLAGS="-D_POSIX_C_SOURCE=200809L"; EXTRA_LIBS="-lm -lrt -lpthread -ldl -lutil" ;;
    Darwin)    EXTRA_FLAGS="-D_DARWIN_C_SOURCE"; EXTRA_LIBS="-lm -lpthread" ;;
    FreeBSD)   EXTRA_FLAGS=""; EXTRA_LIBS="-lm -lpthread" ;;
    OpenBSD)   EXTRA_FLAGS=""; EXTRA_LIBS="-lm -lpthread" ;;
    NetBSD)    EXTRA_FLAGS="-D_NETBSD_SOURCE"; EXTRA_LIBS="-lm -lpthread -lutil" ;;
    MINGW*|MSYS*|CYGWIN*) EXTRA_FLAGS=""; M32_FLAG="-m32"; EXTRA_LIBS="-lpsapi -ladvapi32 -lws2_32 -lnetapi32 -lm" ;;
    *)         EXTRA_FLAGS=""; EXTRA_LIBS="-lm" ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"
CMDTOOLS_DIR="$BUILD_DIR/cmdtools"

PASS_COUNT=0
FAIL_COUNT=0

#----------------------------
# - Target: all (compile every tool)
#----------------------------
all() {
    if [ -z "$CC" ]; then
        for c in gcc cc clang; do
            if command -v "$c" >/dev/null 2>&1; then
                CC="$c"
                break
            fi
        done
        [ -n "$CC" ] || {
            echo "[ERROR] No C compiler found (gcc/clang)." >&2
            echo "[ERROR] Install a compiler or pass \"-cc <toolchain>\"." >&2
            exit 1
        }
    fi

    mkdir -p "$BUILD_DIR" "$CMDTOOLS_DIR"
    PASS_COUNT=0
    FAIL_COUNT=0

    echo ""
    echo "  Compiler: $CC"
    echo "  Flags   : $CFLAGS $EXTRA_FLAGS $M32_FLAG $STATIC_FLAG"
    echo "  Libs    : $EXTRA_LIBS"
    echo ""

    local -a spec_names=()
    if [ -n "$SPECIFY" ]; then
        IFS=',' read -r -a spec_names <<< "$SPECIFY"
        local name
        for name in "${spec_names[@]}"; do
            [ -d "$SRC_DIR/$name" ] || echo "[WARN] Unknown tool (not found in src/): $name" >&2
        done
    fi

    while IFS= read -r -d '' cfile; do
        # Get relative path from src/ (e.g. "bash/bash.c")
        rel_path="${cfile#$SRC_DIR/}"
        # Get directory name (tool name) and base name without extension
        dir_name="$(dirname "$rel_path")"
        base_name="$(basename "$rel_path" .c)"

        # Skip tools not requested via -s/--specify
        if [ ${#spec_names[@]} -gt 0 ]; then
            local wanted=0
            local n
            for n in "${spec_names[@]}"; do
                [ "$n" = "$dir_name" ] && wanted=1
            done
            [ "$wanted" -eq 1 ] || continue
        fi

        case "$dir_name" in
            bash)
                if compile_one "$cfile" "$BUILD_DIR/bash"; then
                    link_binary "$BUILD_DIR" bash sh
                fi
                ;;
            test)
                if compile_one "$cfile" "$CMDTOOLS_DIR/test"; then
                    link_binary "$CMDTOOLS_DIR" test "["
                fi
                ;;
            sh|vi|vim)
                # No .c source, skip
                ;;
            *)
                compile_one "$cfile" "$CMDTOOLS_DIR/$base_name"
                ;;
        esac
    done < <(find "$SRC_DIR" -name '*.c' -type f -print0 | sort -z)

    echo ""
    echo "  Success: $PASS_COUNT   Failed: $FAIL_COUNT"
}

#----------------------------
# - Target: clean (remove build outputs)
#----------------------------
clean() {
    echo ""
    echo "  Clean: $BUILD_DIR"
    rm -f "$BUILD_DIR/bash" "$BUILD_DIR/bash.exe" "$BUILD_DIR/sh" "$BUILD_DIR/sh.exe"
    rm -rf "$CMDTOOLS_DIR"
    echo "  done"
}

#----------------------------
# - Target: start_main (run the built shell)
#----------------------------
start_main() {
    local bin="$BUILD_DIR/bash"
    [ -f "$bin" ] || bin="$BUILD_DIR/bash.exe"
    if [ ! -f "$bin" ]; then
        echo "[ERROR] build/bash not found, run \"build.sh all\" first." >&2
        exit 1
    fi
    exec "$bin"
}

#----------------------------
# - Target: version
#----------------------------
version() {
    echo "$PROJECT_NAME $PROJECT_VERSION (MIT License)"
}

#----------------------------
# - Target: infoprint (print configuration info)
#----------------------------
infoprint() {
    echo ""
    echo "============================================="
    echo "  $PROJECT_NAME build script"
    echo "  A cross-platform Linux tool collection"
    echo "============================================="
    echo "  Version : $PROJECT_VERSION"
    echo "  Platform: $(uname -s)"
    echo "  Script  : build.sh"
    echo "  Source  : $SRC_DIR"
    echo "  Output  : $BUILD_DIR"
    echo ""
}

#----------------------------
# - Target: help
#----------------------------
help() {
    echo "Usage: build.sh [options]"
    echo "Options:"
    echo "  (none)           compile all tools into build/"
    echo "  -cc <cc>         use a specific compiler name or cross toolchain path"
    echo "  -m32             build 32-bit programs (default on Windows)"
    echo "  -static          build statically linked programs"
    echo "  -s <tools>       compile only the given tools, comma-separated (e.g. bash,cat,ls)"
    echo "  --specify <tools> same as -s"
    echo "  -v, --version    print version"
    echo "  -h, --help       print this help"
}

#----------------------------
# - Intermediate stages (make-based workflow only)
#----------------------------
stage_notice() {
    echo "[INFO] '$1' is an intermediate stage used by the make-based workflow."
    echo "[INFO] This script builds finished executables directly; use \"build.sh all\"."
}

#----------------------------
# - Compile one source file (with library retry)
#----------------------------
compile_one() {
    local src_file="$1"
    local out_file="$2"
    local last_err=""
    echo "  CC $(basename "$out_file")"
    if last_err=$($CC $CFLAGS $EXTRA_FLAGS $M32_FLAG $STATIC_FLAG -o "$out_file" "$src_file" $EXTRA_LIBS 2>&1); then
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    fi
    # Link failed - retry with -lm (awk needs libm on Linux)
    if last_err=$($CC $CFLAGS $EXTRA_FLAGS $M32_FLAG $STATIC_FLAG -o "$out_file" "$src_file" -lm 2>&1); then
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    fi
    # Retry with common Windows system libraries (free/htop/top/id/hostname)
    if last_err=$($CC $CFLAGS $EXTRA_FLAGS $M32_FLAG $STATIC_FLAG -o "$out_file" "$src_file" -lpsapi -ladvapi32 -lws2_32 -lnetapi32 2>&1); then
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    fi
    echo "$last_err"
    echo "  [FAILED] $(basename "$out_file")"
    FAIL_COUNT=$((FAIL_COUNT + 1))
    return 1
}

# Create an alias binary (e.g. "[" from test, "sh" from bash).
# Works whether gcc named the output <src> or <src>.exe (MinGW).
link_binary() {
    local dir="$1"
    local src="$2"
    local dst="$3"
    local src_path=""
    if [ -f "$dir/$src" ]; then
        src_path="$dir/$src"
    elif [ -f "$dir/$src.exe" ]; then
        src_path="$dir/$src.exe"
    fi
    [ -n "$src_path" ] || return 0
    local ext=""
    case "$src_path" in
        *.exe) ext=".exe" ;;
    esac
    rm -f "$dir/$dst$ext"
    ln "$src_path" "$dir/$dst$ext" 2>/dev/null || cp -f "$src_path" "$dir/$dst$ext"
    echo "  LINK $dst   <-  $src"
}

#----------------------------
# -command line argument processing
#----------------------------
parse_args() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            -cc)
                shift
                if [ "$#" -eq 0 ]; then
                    echo "[ERROR] -cc requires a compiler name or path." >&2
                    exit 1
                fi
                CC="$1"
                command -v "$CC" >/dev/null 2>&1 || {
                    echo "[ERROR] Compiler not found: $CC" >&2
                    exit 1
                }
                shift
                ;;
            -m32)
                M32_FLAG="-m32"
                shift
                ;;
            -static)
                STATIC_FLAG="-static"
                shift
                ;;
            -s|--specify)
                shift
                if [ "$#" -eq 0 ]; then
                    echo "[ERROR] -s/--specify requires tool name(s), e.g. bash,cat,ls." >&2
                    exit 1
                fi
                if [ -n "$SPECIFY" ]; then
                    SPECIFY="$SPECIFY,$1"
                else
                    SPECIFY="$1"
                fi
                SPECIFY="${SPECIFY//[[:space:]]/}"
                shift
                ;;
            -v|--version)
                version
                exit 0
                ;;
            -h|--help)
                help
                exit 0
                ;;
            *)
                echo "[ERROR] Unknown option: $1" >&2
                help
                exit 1
                ;;
        esac
    done
}

parse_args "$@"
all

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0