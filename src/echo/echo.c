/**
 * @file echo.c
 * @brief Cross-platform implementation of the Linux echo command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common echo(1) implementations.
 *
 * Key behaviors:
 *   - -n: do not output trailing newline
 *   - -e: enable backslash escape interpretation
 *   - -E: disable backslash escape interpretation (default)
 *   - --help / --version: only recognized as sole argument
 *   - Invalid option chars: treat the whole argument as a string (NOT error)
 *   - POSIXLY_CORRECT: enables escape interpretation by default,
 *     disables option parsing (except -n as first arg)
 *   - Direct output via putchar (no buffer truncation)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o echo.exe echo.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o echo echo.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o echo echo.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o echo echo.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o echo echo.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o echo echo.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/echo>
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
    #define ECHO_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define ECHO_PLATFORM_LINUX   1
    #define ECHO_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define ECHO_PLATFORM_MACOS   1
    #define ECHO_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define ECHO_PLATFORM_FREEBSD 1
    #define ECHO_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define ECHO_PLATFORM_OPENBSD 1
    #define ECHO_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define ECHO_PLATFORM_NETBSD  1
    #define ECHO_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define ECHO_PLATFORM_POSIX   1
#else
    #define ECHO_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef ECHO_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef ECHO_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef ECHO_PLATFORM_NETBSD
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
#include <stdint.h>
#include <limits.h>

#ifdef ECHO_PLATFORM_WINDOWS
    #include <io.h>
#else
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define ECHO_VERSION_STR "v1.0.0"

/** @brief Default escape mode: false = -E (no escape), true = -e (escape) */
#define ECHO_DEFAULT_XPG_MODE false

/** @brief Numeric base for hexadecimal conversion */
#define ECHO_HEX_BASE 16U

/** @brief Numeric base for octal conversion */
#define ECHO_OCT_BASE 8U

/** @brief Maximum allowed octal escape value (8-bit byte clamp) */
#define ECHO_OCT_MAX_BYTE 255U

/** @brief Maximum allowed hex escape value (8-bit byte clamp) */
#define ECHO_HEX_MAX_BYTE 255U

/********************************
 *    typedefs
 ********************************/

/********************************
 *    static prototypes
 ********************************/
static unsigned int _echo_hextobin(unsigned char c);
static bool         _echo_is_odigit(unsigned char c);
static bool         _echo_is_xdigit(unsigned char c);
static void         _echo_print_help(void);
static void         _echo_print_version(void);
static bool         _echo_streq(const char * a, const char * b);
static bool         _echo_env_has(const char * name);
static void         _echo_emit(unsigned char c);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for echo_fputs / echo_fflush.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef echo_out_stream
    #define echo_out_stream stdout
#endif

/**
 * @brief Formatted print (printf-compatible).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__"
 * (works on GCC, Clang, MSVC, MinGW; also accepted with a pedantic
 * warning in strict -std=c99 builds).
 *
 * Usage:
 * @code
 *     echo_printf("count=%d\n", count);
 *     echo_printf("%s", "done\n");
 * @endcode
 */
#ifndef echo_printf
    #define echo_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 *
 * Note: we cast to unsigned char first so values with the MSB set do
 *       not trigger undefined behavior in putchar's @c int argument
 *       when char is signed on the host platform.
 */
#ifndef echo_putchar
    #define echo_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally echo_out_stream)
 */
#ifndef echo_fputs
    #define echo_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream (normally echo_out_stream)
 */
#ifndef echo_fflush
    #define echo_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the echo command
 *
 * Processing flow:
 *   1. Check POSIXLY_CORRECT environment variable (safe non-NULL check)
 *   2. Determine allow_options (option parsing allowed?)
 *   3. Check for --help / --version (only when sole argument)
 *   4. Parse -n / -e / -E options; invalid chars => treat as string
 *   5. Output all remaining arguments; always bounds-check pointer reads
 *   6. Print trailing newline unless -n or \c
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success
 */
int main(int argc, char ** argv)
{
    bool output_newline = true;                   /* -n: suppress trailing newline */
    bool escape_enabled = ECHO_DEFAULT_XPG_MODE;  /* -e/-E: escape interpretation   */
    bool posixly_correct = _echo_env_has("POSIXLY_CORRECT");

    /* argv/argc bounds check: avoid indexing when argc is obviously wrong */
    if (argc < 1 || !argv) {
        return 0;
    }

    /* Determine whether option parsing is allowed.
     * When POSIXLY_CORRECT is set, options are NOT allowed,
     * EXCEPT: -n as the first argument is still honored
     * (only when ECHO_DEFAULT_XPG_MODE is false, which is the default). */
    bool allow_options = false;
    if (!posixly_correct) {
        allow_options = true;
    }
    else if (!ECHO_DEFAULT_XPG_MODE && (argc > 1) &&
             _echo_streq(argv[1], "-n")) {
        allow_options = true;
    }

    /* --help and --version are only recognized as the sole argument
     * and only when options are allowed. */
    if (allow_options && (argc == 2)) {
        if (_echo_streq(argv[1], "--help")) {
            _echo_print_help();
            return 0;
        }
        if (_echo_streq(argv[1], "--version")) {
            _echo_print_version();
            return 0;
        }
    }

    /* Skip argv[0] (program name) but guard against going backwards */
    if (argc > 0) {
        --argc;
        ++argv;
    }

    /* Parse options: -n, -e, -E (possibly combined like -ne, -en, etc.)
     *
     * Key behavior: if ANY character after '-' is not n/e/E,
     * stop parsing options and treat the current argument (and all
     * remaining args) as strings to echo. Invalid options do NOT
     * produce an error. */
    if (allow_options) {
        while ((argc > 0) && argv && argv[0] && (argv[0][0] == '-')) {
            const char * temp = argv[0] + 1; /* skip the '-' */

            /* If just "-" with nothing after, treat as string */
            if (!temp || *temp == '\0') {
                break;
            }

            /* Check if ALL chars are valid options (n, e, E);
             * also refuse to parse excessively long option tokens. */
            bool valid = true;
            size_t tlen = strlen(temp);
            if (tlen == 0 || tlen > 1024) {
                valid = false;
            }
            else {
                for (size_t i = 0; temp[i] != '\0'; i++) {
                    if (temp[i] != 'e' && temp[i] != 'E' && temp[i] != 'n') {
                        valid = false;
                        break;
                    }
                }
            }
            if (!valid) {
                break;
            }

            /* All chars are valid options — process them */
            while (*temp != '\0') {
                switch (*temp++) {
                    case 'e':
                        escape_enabled = true;
                        break;

                    case 'E':
                        escape_enabled = false;
                        break;

                    case 'n':
                        output_newline = false;
                        break;

                    default:
                        break;
                }
            }
            --argc;
            ++argv;
        }
    }

    /* Output phase */
    if (escape_enabled || posixly_correct) {
        /* Escape interpretation enabled (-e or POSIXLY_CORRECT) */
        while (argc > 0 && argv && argv[0]) {
            const char * s = argv[0];
            unsigned char c = 0;

            while (s && ((c = (unsigned char)*s++) != 0)) {
                if (c == '\\' && s && *s != '\0') {
                    c = (unsigned char)*s++;
                    switch (c) {
                        case 'a':
                            c = '\a';
                            break;

                        case 'b':
                            c = '\b';
                            break;

                        case 'c':
                            echo_fflush(echo_out_stream);
                            return 0; /* \c: stop all output */

                        case 'e':
                            c = '\x1b';
                            break;

                        case 'f':
                            c = '\f';
                            break;

                        case 'n':
                            c = '\n';
                            break;

                        case 'r':
                            c = '\r';
                            break;

                        case 't':
                            c = '\t';
                            break;

                        case 'v':
                            c = '\v';
                            break;

                        case 'x':
                        {
                            /* Parse \xHH: 1 or 2 hex digits, clamp to 8-bit */
                            unsigned char ch = (s) ? (unsigned char)*s : 0;
                            if (!_echo_is_xdigit(ch)) {
                                /* Not a hex digit: print '\' then 'x' */
                                _echo_emit('\\');
                                c = 'x';
                            }
                            else {
                                unsigned int val = _echo_hextobin(ch);
                                if (s) {
                                    s++;
                                }
                                ch = (s) ? (unsigned char)*s : 0;
                                if (_echo_is_xdigit(ch)) {
                                    val = val * ECHO_HEX_BASE + _echo_hextobin(ch);
                                    if (s) {
                                        s++;
                                    }
                                }
                                if (val > ECHO_HEX_MAX_BYTE) {
                                    val = ECHO_HEX_MAX_BYTE;
                                }
                                c = (unsigned char)val;
                            }
                            break;
                        }

                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        {
                            /* Parse up to 3 octal digits.
                             * Leading '0' means 1..3 more oct digits;
                             * '1'..'7' without \0 prefix means literal.
                             * We always clamp to 8-bit to prevent overflow. */
                            unsigned int val = (unsigned int)(c - '0');
                            if (s && _echo_is_odigit((unsigned char)*s)) {
                                val = val * ECHO_OCT_BASE +
                                      (unsigned int)((unsigned char)*s++ - '0');
                            }
                            if (s && _echo_is_odigit((unsigned char)*s)) {
                                val = val * ECHO_OCT_BASE +
                                      (unsigned int)((unsigned char)*s++ - '0');
                            }
                            if (val > ECHO_OCT_MAX_BYTE) {
                                val = ECHO_OCT_MAX_BYTE;
                            }
                            c = (unsigned char)val;
                            break;
                        }

                        case '\\':
                            /* c is already '\\' */
                            break;

                        default:
                            /* Unknown escape: print '\' then the char */
                            _echo_emit('\\');
                            break;
                    }
                }
                _echo_emit(c);
            }
            --argc;
            ++argv;
            if (argc > 0) {
                _echo_emit(' ');
            }
        }
    }
    else {
        /* No escape interpretation (default -E mode) */
        while (argc > 0 && argv && argv[0]) {
            echo_fputs(argv[0], echo_out_stream);
            --argc;
            ++argv;
            if (argc > 0) {
                _echo_emit(' ');
            }
        }
    }

    if (output_newline) {
        _echo_emit('\n');
    }

    echo_fflush(echo_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Convert a hexadecimal character to its integer value.
 *        Caller must ensure c is a valid hex digit (see _echo_is_xdigit).
 * @param c  Hexadecimal character (0-9, a-f, A-F)
 * @return Integer value 0-15
 */
static unsigned int _echo_hextobin(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return (unsigned int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (unsigned int)(c - 'a' + 10);
    }
    return (unsigned int)(c - 'A' + 10);
}

/**
 * @brief Check if a character is an octal digit (0-7).
 *        Accepts unsigned char (explicit, avoids ctype.h UB for signed char < 0).
 * @param c  Character to test
 * @return true if octal digit, false otherwise
 */
static bool _echo_is_odigit(unsigned char c)
{
    return (c >= '0') && (c <= '7');
}

/**
 * @brief Check if a character is a hexadecimal digit.
 *        Accepts unsigned char (explicit, avoids ctype.h UB for signed char < 0).
 * @param c  Character to test
 * @return true if hexadecimal digit, false otherwise
 */
static bool _echo_is_xdigit(unsigned char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/**
 * @brief Safe wrapper for getenv: only tests presence, not the value contents.
 *        Avoids calling strlen / string operations on environment values
 *        that may be extremely long.
 * @param name  Environment variable name
 * @return true if variable exists and is non-empty
 */
static bool _echo_env_has(const char * name)
{
    if (!name) {
        return false;
    }
    const char * v = getenv(name);
    if (!v) {
        return false;
    }
    return (v[0] != '\0');
}

/**
 * @brief Emit one byte using echo_putchar; helper so all call sites
 *        consistently pass through the (unsigned char) cast and never
 *        rely on the implicit int promotion of signed char.
 * @param c  byte to write
 */
static void _echo_emit(unsigned char c)
{
    echo_putchar(c);
}

/**
 * @brief Print usage/help information
 */
static void _echo_print_help(void)
{
    echo_printf(
        "Usage: echo [SHORT-OPTION]... [STRING]...\n"
        "  or:  echo LONG-OPTION\n"
        "Echo the STRING(s) to standard output.\n"
        "\n"
        "  -n     do not output the trailing newline\n"
        "  -e     enable interpretation of backslash escapes\n"
        "  -E     disable interpretation of backslash escapes (default)\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "If -e is in effect, the following sequences are recognized:\n"
        "\n"
        "  \\\\      backslash\n"
        "  \\a      alert (bell)\n"
        "  \\b      backspace\n"
        "  \\c      produce no further output\n"
        "  \\e      escape\n"
        "  \\f      form feed\n"
        "  \\n      new line\n"
        "  \\r      carriage return\n"
        "  \\t      horizontal tab\n"
        "  \\v      vertical tab\n"
        "  \\0NNN   byte with octal value NNN (1 to 3 digits)\n"
        "  \\xHH    byte with hexadecimal value HH (1 to 2 digits)\n"
        "\n"
        "NOTE: your shell may have its own version of echo, which usually supersedes\n"
        "the version described here.  Please refer to your shell's documentation\n"
        "for details about the options it supports.\n"
    );
}

/**
 * @brief Print version information
 */
static void _echo_print_version(void)
{
    echo_printf("echo %s\n", ECHO_VERSION_STR);
    echo_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    echo_printf("%s", "License MIT: <https://mit-license.org/>\n");
    echo_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    echo_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _echo_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}
