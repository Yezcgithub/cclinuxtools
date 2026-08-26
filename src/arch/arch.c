/**
 * @file arch.c
 * @brief Cross-platform implementation of the GNU arch command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils arch(1).
 *
 * Key behaviors:
 *   - No options: print machine hardware name (same as uname -m)
 *   - --help:     display help and exit
 *   - --version:  output version information and exit
 *   - Any other option or operand: error (exit 1)
 *
 * Platform data sources:
 *   POSIX  : uname(2) — machine field (e.g. "x86_64", "aarch64")
 *   Windows: PROCESSOR_ARCHITECTURE environment variable
 *            (e.g. "AMD64", "ARM64")
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o arch.exe arch.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o arch arch.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o arch arch.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o arch arch.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o arch arch.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o arch arch.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/arch>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Platform detection macros — must appear before any system includes
 * so that POSIX feature macros are defined correctly.
 */
#if defined(_WIN32) || defined(_WIN64)
    #define ARCH_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define ARCH_PLATFORM_LINUX   1
    #define ARCH_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define ARCH_PLATFORM_MACOS   1
    #define ARCH_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define ARCH_PLATFORM_FREEBSD 1
    #define ARCH_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define ARCH_PLATFORM_OPENBSD 1
    #define ARCH_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define ARCH_PLATFORM_NETBSD  1
    #define ARCH_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define ARCH_PLATFORM_POSIX   1
#else
    #define ARCH_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef ARCH_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef ARCH_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef ARCH_PLATFORM_NETBSD
    #ifndef _NETBSD_SOURCE
        #define _NETBSD_SOURCE
    #endif
#endif

/********************************
 *    includes
 ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef ARCH_PLATFORM_WINDOWS
    #include <windows.h>
#else /* ARCH_PLATFORM_POSIX */
    #include <sys/utsname.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define ARCH_VERSION_STR "v1.0.0"

/** @brief Size of the machine name buffer */
#define ARCH_FIELD_SIZE 256

/** @brief String used when machine name is unavailable (GNU-compatible) */
#define ARCH_UNKNOWN_STR "unknown"

/** @brief Maximum long option name length accepted by the parser */
#define ARCH_OPT_NAME_MAX 64

/********************************
 *    typedefs
 ********************************/

/********************************
 *    static prototypes
 ********************************/
static void _arch_print_help(void);
static void _arch_print_version(void);
static int  _arch_parse_args(int argc, char ** argv);
static int  _arch_get_machine(char * buf, size_t bufsize);

#ifdef ARCH_PLATFORM_WINDOWS
static void _arch_win_get_env(const char * name, char * buf, size_t bufsize);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for arch_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef arch_out_stream
    #define arch_out_stream stdout
#endif

/**
 * @brief Default stderr stream for arch_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef arch_err_stream
    #define arch_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 */
#ifndef arch_printf
    #define arch_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef arch_err_printf
    #define arch_err_printf(fmt, ...) \
        do { if (arch_err_stream) { (void)fprintf((arch_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 */
#ifndef arch_fputs
    #define arch_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef arch_fflush
    #define arch_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Copy a string into a fixed-size buffer, always NUL-terminating.
 *        If src is NULL, the buffer is set to an empty string.
 *        Implemented as a static inline function (rather than a macro)
 *        so that callers passing stack array addresses do not trigger
 *        -Waddress warnings from constant-address NULL checks.
 * @param dst    destination buffer
 * @param src    source string (may be NULL)
 * @param size   size of destination buffer
 */
static inline void arch_strcpy_safe(char * dst, const char * src, size_t size)
{
    if (!dst || size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the arch command
 *
 * Processing flow:
 *   1. Parse command-line options (only --help and --version accepted)
 *   2. Collect machine hardware name from the host
 *   3. Print the machine name followed by a newline
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on usage error
 */
int main(int argc, char ** argv)
{
    int parse_rc = _arch_parse_args(argc, argv);
    if (parse_rc != 0) {
        return parse_rc;
    }

    char machine[ARCH_FIELD_SIZE];
    if (_arch_get_machine(machine, sizeof(machine)) != 0) {
        return 1;
    }

    arch_fputs(machine, arch_out_stream);
    arch_fputs("\n", arch_out_stream);
    arch_fflush(arch_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _arch_print_help(void)
{
    arch_printf(
        "Usage: arch [OPTION]...\n"
        "Print machine architecture.\n"
        "\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "NOTE: arch prints the machine hardware name, equivalent to 'uname -m'.\n"
        "      It does not accept any short options or operands.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _arch_print_version(void)
{
    arch_printf("arch %s\n", ARCH_VERSION_STR);
    arch_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    arch_printf("%s", "License MIT: <https://mit-license.org/>\n");
    arch_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    arch_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments
 *
 * GNU arch only accepts --help and --version as long options.
 * No short options are supported. Any other option or operand
 * is an error.
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on unknown option or extra operand
 */
static int _arch_parse_args(int argc, char ** argv)
{
    if (argc < 1 || !argv) {
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* Long options (arg starts with "--" but is not exactly "--") */
        if (strncmp(arg, "--", 2) == 0 && arg[2] != '\0') {
            /* Extract name before '=' if present */
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[ARCH_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _arch_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _arch_print_version();
                exit(0);
            }

            arch_err_printf("arch: unrecognized option '%s'\n", arg);
            arch_err_printf("%s", "Try 'arch --help' for more information.\n");
            return 1;
        }

        /* "-" alone or any non-option token is an error */
        arch_err_printf("arch: invalid option or operand '%s'\n", arg);
        arch_err_printf("%s", "Try 'arch --help' for more information.\n");
        return 1;
    }

    return 0;
}

#ifdef ARCH_PLATFORM_WINDOWS
/**
 * @brief Read an environment variable into a buffer (Windows helper).
 *        On failure, leaves the buffer as an empty string.
 * @param name     environment variable name
 * @param buf      output buffer
 * @param bufsize  size of output buffer
 */
static void _arch_win_get_env(const char * name, char * buf, size_t bufsize)
{
    if (!buf || bufsize == 0) {
        return;
    }
    buf[0] = '\0';
    if (!name) {
        return;
    }
    DWORD n = GetEnvironmentVariableA(name, buf, (DWORD)bufsize);
    if (n == 0 || n >= bufsize) {
        buf[0] = '\0';
    }
}
#endif

/**
 * @brief Collect the machine hardware name into buf.
 *
 * On POSIX, uses uname(2) and extracts the machine field.
 * On Windows, reads the PROCESSOR_ARCHITECTURE environment variable.
 *
 * If the field is empty or unavailable, buf is set to "unknown"
 * (GNU-compatible).
 *
 * @param buf      output buffer for machine name
 * @param bufsize  size of output buffer
 * @return 0 on success, -1 on failure
 */
static int _arch_get_machine(char * buf, size_t bufsize)
{
    if (!buf || bufsize == 0) {
        return -1;
    }

    arch_strcpy_safe(buf, ARCH_UNKNOWN_STR, bufsize);

#ifdef ARCH_PLATFORM_WINDOWS
    /* ---- Windows: gather from environment ---- */
    {
        char arch[ARCH_FIELD_SIZE] = {0};
        _arch_win_get_env("PROCESSOR_ARCHITECTURE", arch, sizeof(arch));
        if (arch[0] != '\0') {
            arch_strcpy_safe(buf, arch, bufsize);
        }
    }
#else /* ARCH_PLATFORM_POSIX */
    /* ---- POSIX: use uname(2) ---- */
    {
        struct utsname u;
        memset(&u, 0, sizeof(u));
        if (uname(&u) != 0) {
            arch_err_printf("arch: cannot get system information\n");
            return -1;
        }
        if (u.machine[0] != '\0') {
            arch_strcpy_safe(buf, u.machine, bufsize);
        }
    }
#endif

    return 0;
}
