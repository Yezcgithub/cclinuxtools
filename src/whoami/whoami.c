/**
 * @file whoami.c
 * @brief Cross-platform whoami command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils whoami(1).
 *
 * Key behaviors:
 *   - Prints the user name associated with the current effective
 *     user ID. POSIX uses geteuid(2) + getpwuid(3); Windows uses
 *     GetUserNameW converted to UTF-8. Same as "id -un".
 *   - --help / --version: recognized and handled
 *   - Unknown options and extra operands are errors (exit code 1)
 *   - No short options (matches GNU coreutils whoami)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o whoami.exe whoami.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o whoami whoami.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o whoami whoami.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o whoami whoami.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o whoami whoami.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o whoami whoami.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/whoami>
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
    #define WHOAMI_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define WHOAMI_PLATFORM_LINUX   1
    #define WHOAMI_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define WHOAMI_PLATFORM_MACOS   1
    #define WHOAMI_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define WHOAMI_PLATFORM_FREEBSD 1
    #define WHOAMI_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define WHOAMI_PLATFORM_OPENBSD 1
    #define WHOAMI_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define WHOAMI_PLATFORM_NETBSD  1
    #define WHOAMI_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define WHOAMI_PLATFORM_POSIX   1
#else
    #define WHOAMI_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef WHOAMI_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef WHOAMI_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef WHOAMI_PLATFORM_NETBSD
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
#include <stddef.h>

#ifdef WHOAMI_PLATFORM_WINDOWS
    #include <windows.h>
#else /* WHOAMI_PLATFORM_POSIX */
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define WHOAMI_VERSION_STR "v1.0.0"

/** @brief Maximum user name length in bytes (covers LOGIN_NAME_MAX on most systems) */
#define WHOAMI_NAME_MAX 256

/********************************
 *    static prototypes
 ********************************/
static int  _whoami_safe_copy(char * dst, const char * src, size_t dst_size);
static int  _whoami_get_user(char * buf, size_t size);
static void _whoami_print_help(void);
static void _whoami_print_version(void);
static int  _whoami_parse_args(int argc, char ** argv);

#ifdef WHOAMI_PLATFORM_WINDOWS
static int _whoami_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size);
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for whoami_printf.
 *        Defaults to libc @c stdout .
 */
#ifndef whoami_out_stream
    #define whoami_out_stream stdout
#endif

/**
 * @brief Default stderr stream for whoami_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef whoami_err_stream
    #define whoami_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 */
#ifndef whoami_printf
    #define whoami_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr — NULL-safe on the stream.
 */
#ifndef whoami_err_printf
    #define whoami_err_printf(fmt, ...) \
        do { if (whoami_err_stream) { (void)fprintf((whoami_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef whoami_fflush
    #define whoami_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe free-and-null pointer cleanup macro.
 */
#ifndef whoami_safe_free
    #define whoami_safe_free(p) \
        do { if ((p)) { free((p)); (p) = NULL; } } while (0)
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the whoami command
 *
 * Processing flow:
 *   1. Parse command-line options (--help / --version)
 *   2. Resolve the effective user name
 *   3. Print the name
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    if (_whoami_parse_args(argc, argv) != 0) {
        return 1;
    }

    char name[WHOAMI_NAME_MAX + 1];
    memset(name, 0, sizeof(name));

    if (_whoami_get_user(name, sizeof(name)) != 0) {
#ifdef WHOAMI_PLATFORM_POSIX
        whoami_err_printf("whoami: cannot find name for user ID %lu\n",
                           (unsigned long)geteuid());
#else
        whoami_err_printf("%s", "whoami: cannot determine user name\n");
#endif
        return 1;
    }

    /* Defensive: ensure NUL termination before output. */
    name[sizeof(name) - 1] = '\0';
    whoami_printf("%s\n", name);
    whoami_fflush(whoami_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Safer string copy that cannot trigger truncation warnings.
 *
 * Uses memcpy followed by explicit NUL termination so compilers see the
 * bounded copy is safe and don't emit -Wstringop-truncation.
 *
 * @param dst       destination buffer
 * @param src       NUL-terminated source
 * @param dst_size  size of dst in bytes
 * @return 0 on success, -1 if dst_size is too small
 */
static int _whoami_safe_copy(char * dst, const char * src, size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return -1;
    }
    dst[0] = '\0';
    if (!src) {
        return -1;
    }
    size_t slen = strlen(src);
    if (slen >= dst_size) {
        return -1;
    }
    memcpy(dst, src, slen);
    dst[slen] = '\0';
    return 0;
}

#ifdef WHOAMI_PLATFORM_WINDOWS

/**
 * @brief Convert a wide (UTF-16) string to a UTF-8 multi-byte string.
 * @param wide      input UTF-16 NUL-terminated string
 * @param out       output buffer (must be at least out_size bytes)
 * @param out_size  size of the output buffer in bytes
 * @return number of bytes written (excluding NUL) on success, -1 on failure
 */
static int _whoami_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size)
{
    if (!out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!wide) {
        return -1;
    }
    int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) {
        return -1;
    }
    /* needed includes trailing NUL; verify the destination can hold it. */
    if (needed < 1 || needed > (int)out_size) {
        return -1;
    }
    int written = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
    if (written <= 0) {
        out[0] = '\0';
        return -1;
    }
    if ((size_t)written < out_size) {
        out[written] = '\0';
    }
    else {
        out[out_size - 1] = '\0';
    }
    return written - 1;
}

/**
 * @brief Get the effective user name as a UTF-8 string.
 *
 * Windows lacks a POSIX uid/pw database; GetUserNameW provides the SAM
 * account name of the calling thread/user, which is what GNU whoami
 * prints. Converted to UTF-8 for consistent cross-platform output.
 *
 * @param buf   output buffer (NUL-terminated UTF-8)
 * @param size  size of output buffer in bytes
 * @return 0 on success, -1 on failure
 */
static int _whoami_get_user(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';

    wchar_t wtmp[WHOAMI_NAME_MAX + 1];
    DWORD wlen = (DWORD)(sizeof(wtmp) / sizeof(wtmp[0]));
    if (!GetUserNameW(wtmp, &wlen)) {
        return -1;
    }
    wtmp[(sizeof(wtmp) / sizeof(wtmp[0])) - 1] = L'\0';

    /* UTF-8 can use up to 4 bytes per wchar; size accordingly. */
    char utf8[WHOAMI_NAME_MAX * 4 + 1];
    if (_whoami_wide_to_utf8(wtmp, utf8, sizeof(utf8)) < 0) {
        return -1;
    }
    return _whoami_safe_copy(buf, utf8, size);
}

#else /* WHOAMI_PLATFORM_POSIX */

/**
 * @brief Get the effective user name via getpwuid(geteuid()).
 *
 * Mirrors GNU coreutils whoami: resolve the passwd entry for the
 * current effective UID and return pw_name. No environment fallback.
 *
 * @param buf   output buffer (NUL-terminated)
 * @param size  size of output buffer in bytes
 * @return 0 on success, -1 on failure
 */
static int _whoami_get_user(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';

    uid_t uid = geteuid();
    struct passwd * pw = getpwuid(uid);
    if (!pw || !pw->pw_name) {
        return -1;
    }
    return _whoami_safe_copy(buf, pw->pw_name, size);
}

#endif /* WHOAMI_PLATFORM_POSIX */

/**
 * @brief Print usage/help information
 */
static void _whoami_print_help(void)
{
    whoami_printf(
        "Usage: whoami [OPTION]...\n"
        "Print the user name associated with the current effective user ID.\n"
        "Same as id -un.\n"
        "\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "Unknown options and extra operands are errors.\n"
        "GNU coreutils compatibility; no short options are supported.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _whoami_print_version(void)
{
    whoami_printf("whoami %s\n", WHOAMI_VERSION_STR);
    whoami_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    whoami_printf("%s", "License MIT: <https://mit-license.org/>\n");
    whoami_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    whoami_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments.
 *
 * whoami accepts only --help and --version (no short options, no
 * operands). "--" terminates option processing; any subsequent token
 * is an extra operand error. --help/--version exit immediately, even
 * when followed by what would otherwise be an operand error (matching
 * GNU getopt_long left-to-right processing).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, -1 on unknown option / extra operand
 */
static int _whoami_parse_args(int argc, char ** argv)
{
    if (argc < 1 || !argv) {
        return -1;
    }

    bool end_of_opts = false;
    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (!end_of_opts && strcmp(arg, "--") == 0) {
            end_of_opts = true;
            continue;
        }

        if (!end_of_opts && strncmp(arg, "--", 2) == 0) {
            /* Long option: extract name before '=' if present */
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[64];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _whoami_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _whoami_print_version();
                exit(0);
            }
            whoami_err_printf("whoami: unrecognized option '%s'\n", arg);
            whoami_err_printf("%s", "Try 'whoami --help' for more information.\n");
            return -1;
        }
        else if (!end_of_opts && arg[0] == '-' && arg[1] != '\0') {
            /* whoami has no valid short options */
            whoami_err_printf("whoami: invalid option -- '%c'\n", arg[1]);
            whoami_err_printf("%s", "Try 'whoami --help' for more information.\n");
            return -1;
        }
        else {
            whoami_err_printf("whoami: extra operand '%s'\n", arg);
            whoami_err_printf("%s", "Try 'whoami --help' for more information.\n");
            return -1;
        }
    }

    return 0;
}
