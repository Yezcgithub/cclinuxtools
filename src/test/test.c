/**
 * @file test.c
 * @brief Cross-platform implementation of the GNU test / [ command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils test(1) / [(1).
 *
 * Key behaviors:
 *   - File tests:   -b -c -d -e -f -g -G -h -k -L -O -p -r -s -S -t -u -w -x
 *   - String tests: -z -n = != < >
 *   - Integer ops:  -eq -ne -lt -le -gt -ge
 *   - File compare: -nt -ot -ef
 *   - Logical:      ! -a -o ( )
 *   - When invoked as '[', last arg must be ']'
 *   - --help / --version recognized (test --help, [ --help ])
 *
 * Exit codes:
 *   0 = expression is true
 *   1 = expression is false
 *   2 = error
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o test.exe test.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o test test.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o test test.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o test test.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o test test.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o test test.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/test>
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
    #define TEST_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TEST_PLATFORM_LINUX   1
    #define TEST_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TEST_PLATFORM_MACOS   1
    #define TEST_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TEST_PLATFORM_FREEBSD 1
    #define TEST_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TEST_PLATFORM_OPENBSD 1
    #define TEST_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TEST_PLATFORM_NETBSD  1
    #define TEST_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define TEST_PLATFORM_POSIX   1
#else
    #define TEST_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef TEST_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef TEST_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef TEST_PLATFORM_NETBSD
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
#include <errno.h>

#ifdef TEST_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#else /* TEST_PLATFORM_POSIX */
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <time.h>
#endif

/* Fallback S_IS* macros for platforms that don't define them */
#ifndef S_ISBLK
    #define S_ISBLK(m) 0
#endif
#ifndef S_ISCHR
    #define S_ISCHR(m) 0
#endif
#ifndef S_ISFIFO
    #define S_ISFIFO(m) 0
#endif
#ifndef S_ISSOCK
    #define S_ISSOCK(m) 0
#endif
#ifndef S_ISLNK
    #define S_ISLNK(m) 0
#endif

/* Permission bit fallbacks — not defined under strict _POSIX_C_SOURCE */
#ifndef S_ISUID
    #define S_ISUID 0004000
#endif
#ifndef S_ISGID
    #define S_ISGID 0002000
#endif
#ifndef S_ISVTX
    #define S_ISVTX 0001000
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define TEST_VERSION_STR "v1.0.0"

/** @brief Maximum long option name length accepted by the parser */
#define TEST_OPT_NAME_MAX 64

/********************************
 *    typedefs
 ********************************/

/********************************
 *    static prototypes
 ********************************/
static void _test_print_help(void);
static void _test_print_version(void);
static bool _test_is_bracket(const char * argv0);
static int  _test_run(int argc, char ** argv);
static bool _test_posix(int nargs, char ** args);
static bool _test_or_expr(void);
static bool _test_and_expr(void);
static bool _test_not_expr(void);
static bool _test_term(void);
static bool _test_unary(const char * op, const char * arg);
static bool _test_binary(const char * op, const char * left, const char * right);
static bool _test_is_unary_op(const char * s);
static bool _test_is_binary_op(const char * s);
static bool _test_file_test(const char * op, const char * path);
static bool _test_file_newer(const char * f1, const char * f2);
static bool _test_file_older(const char * f1, const char * f2);
static bool _test_file_same(const char * f1, const char * f2);
static bool _test_parse_int(const char * s, long * out);
static bool _test_str_ieq(const char * a, const char * b);

#ifdef TEST_PLATFORM_WINDOWS
static bool _test_win_executable(const char * path);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for test_printf.
 *        Defaults to libc @c stdout .
 */
#ifndef test_out_stream
    #define test_out_stream stdout
#endif

/**
 * @brief Default stderr stream for test_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef test_err_stream
    #define test_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 */
#ifndef test_printf
    #define test_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream.
 */
#ifndef test_err_printf
    #define test_err_printf(fmt, ...) \
        do { if (test_err_stream) { (void)fprintf((test_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef test_fputs
    #define test_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef test_fflush
    #define test_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 * @return true if strings match, false otherwise
 */
#ifndef test_streq
    #define test_streq(a, b) \
        (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Program name for error messages (basename of argv[0]) */
static const char * test_prog_name = "test";

/** @brief Current position in argv (after program name) */
static char ** test_args = NULL;

/** @brief Remaining argument count */
static int test_argc = 0;

/** @brief Error counter; non-zero means a syntax error occurred */
static int test_errors = 0;

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the test / [ command
 *
 * Processing flow:
 *   1. Determine if invoked as '[' (basename ends with '[')
 *   2. If '[', verify and strip trailing ']'
 *   3. Handle --help / --version
 *   4. Evaluate the conditional expression
 *   5. Return 0 (true), 1 (false), or 2 (error)
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 if true, 1 if false, 2 on error
 */
int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 2;
    }

    /* Determine program name (basename) */
    test_prog_name = argv[0];
    for (const char * p = argv[0]; *p; p++) {
        if (*p == '/' || *p == '\\') {
            test_prog_name = p + 1;
        }
    }

    /* If invoked as '[', strip '.exe' from name check and require ']' */
    bool is_bracket = _test_is_bracket(argv[0]);
    if (is_bracket) {
        test_prog_name = "[";
        if (argc < 2 || !test_streq(argv[argc - 1], "]")) {
            test_err_printf("%s: missing ']'\n", test_prog_name);
            test_err_printf("%s", "Try '[ --help' for more information.\n");
            return 2;
        }
        argc--;
    }

    /* Handle --help / --version */
    if (argc == 2 && argv[1]) {
        if (test_streq(argv[1], "--help")) {
            _test_print_help();
            return 0;
        }
        if (test_streq(argv[1], "--version")) {
            _test_print_version();
            return 0;
        }
    }

    /* Skip argv[0] */
    int expr_argc = argc - 1;
    char ** expr_argv = argv + 1;

    int result = _test_run(expr_argc, expr_argv);
    test_fflush(test_out_stream);
    return result;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _test_print_help(void)
{
    test_printf(
        "Usage: test EXPRESSION\n"
        "  or:  test\n"
        "  or:  [ EXPRESSION ]\n"
        "  or:  [ ]\n"
        "  or:  [ OPTION\n"
        "Evaluate conditional expression.\n"
        "\n"
        "Exits with status 0 if true, 1 if false, 2 if error.\n"
        "\n"
        "File operators:\n"
        "  -b FILE        FILE exists and is block special\n"
        "  -c FILE        FILE exists and is character special\n"
        "  -d FILE        FILE exists and is a directory\n"
        "  -e FILE        FILE exists\n"
        "  -f FILE        FILE exists and is a regular file\n"
        "  -g FILE        FILE exists and is set-group-ID\n"
        "  -G FILE        FILE exists and is owned by effective group\n"
        "  -h FILE        FILE exists and is a symbolic link (same as -L)\n"
        "  -k FILE        FILE exists and has sticky bit set\n"
        "  -L FILE        FILE exists and is a symbolic link\n"
        "  -O FILE        FILE exists and is owned by effective user\n"
        "  -p FILE        FILE exists and is a named pipe (FIFO)\n"
        "  -r FILE        FILE exists and is readable\n"
        "  -s FILE        FILE exists and has size > 0\n"
        "  -S FILE        FILE exists and is a socket\n"
        "  -t FD          FD is opened on a terminal\n"
        "  -u FILE        FILE exists and is set-user-ID\n"
        "  -w FILE        FILE exists and is writable\n"
        "  -x FILE        FILE exists and is executable\n"
        "\n"
        "String operators:\n"
        "  -z STRING      STRING is empty\n"
        "  -n STRING      STRING is not empty\n"
        "  STRING1 = STRING2   strings are equal\n"
        "  STRING1 != STRING2  strings are not equal\n"
        "  STRING1 < STRING2   STRING1 sorts before STRING2\n"
        "  STRING1 > STRING2   STRING1 sorts after STRING2\n"
        "\n"
        "Integer operators:\n"
        "  INTEGER1 -eq INTEGER2  equal\n"
        "  INTEGER1 -ne INTEGER2  not equal\n"
        "  INTEGER1 -lt INTEGER2  less than\n"
        "  INTEGER1 -le INTEGER2  less than or equal\n"
        "  INTEGER1 -gt INTEGER2  greater than\n"
        "  INTEGER1 -ge INTEGER2  greater than or equal\n"
        "\n"
        "File comparison:\n"
        "  FILE1 -nt FILE2  FILE1 is newer than FILE2 (mtime)\n"
        "  FILE1 -ot FILE2  FILE1 is older than FILE2 (mtime)\n"
        "  FILE1 -ef FILE2  FILE1 and FILE2 have same device and inode\n"
        "\n"
        "Logical operators:\n"
        "  ! EXPR        logical NOT\n"
        "  EXPR1 -a EXPR2  logical AND\n"
        "  EXPR1 -o EXPR2  logical OR\n"
        "  ( EXPR )      grouping\n"
        "\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "NOTE: [ requires a matching ] as the last argument.\n"
        "NOTE: Binary operators (-eq, -ne, etc.) must be separate arguments.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _test_print_version(void)
{
    test_printf("test %s\n", TEST_VERSION_STR);
    test_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    test_printf("%s", "License MIT: <https://mit-license.org/>\n");
    test_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    test_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Determine whether the program was invoked as '['
 *        by checking if the basename (minus path and .exe) is '['.
 * @param argv0  value of argv[0]
 * @return true if invoked as '['
 */
static bool _test_is_bracket(const char * argv0)
{
    if (!argv0) {
        return false;
    }

    /* Get basename */
    const char * base = argv0;
    for (const char * p = argv0; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }

    /* Exactly "[" */
    if (base[0] == '[' && base[1] == '\0') {
        return true;
    }

    /* "[.exe" or "[.EXE" */
    if (base[0] == '[' && base[1] == '.' &&
        _test_str_ieq(base + 2, "exe") && base[5] == '\0') {
        return true;
    }

    return false;
}

/**
 * @brief Top-level expression evaluator
 *
 * Dispatches to the POSIX algorithm (0-3 args) or the
 * recursive descent parser (4+ args, which handles parentheses
 * grouping that the POSIX 4-arg case does not support).
 *
 * @param argc  number of expression arguments
 * @param argv  expression arguments
 * @return 0 if true, 1 if false, 2 on error
 */
static int _test_run(int argc, char ** argv)
{
    if (argc == 0) {
        /* No arguments: false */
        return 1;
    }

    bool result;

    if (argc <= 3) {
        /* POSIX algorithm for 1-3 args */
        result = _test_posix(argc, argv);
    }
    else {
        /* Recursive descent parser for 4+ args */
        test_args = argv;
        test_argc = argc;
        result = _test_or_expr();

        /* All arguments must be consumed */
        if (test_argc > 0 && test_errors == 0) {
            test_err_printf("%s: too many arguments\n", test_prog_name);
            test_errors++;
        }
    }

    if (test_errors > 0) {
        return 2;
    }

    return result ? 0 : 1;
}

/**
 * @brief POSIX algorithm for 1-4 argument expressions
 *
 * This implements the standard POSIX test parsing rules:
 *   1 arg:  true if string is non-empty
 *   2 args: ! STRING, or UNARY_OP STRING
 *   3 args: ! EXPR(2), STRING1 OP STRING2, or ( STRING )
 *   4 args: ! EXPR(3)
 *
 * @param nargs  number of arguments (1-4)
 * @param args   argument array
 * @return true if expression is true, false otherwise
 */
static bool _test_posix(int nargs, char ** args)
{
    if (!args || nargs < 1) {
        test_errors++;
        return false;
    }

    switch (nargs) {
        case 1:
            /* Non-empty string is true */
            return args[0][0] != '\0';

        case 2:
            /* ! STRING */
            if (args[0][0] == '!' && args[0][1] == '\0') {
                return args[1][0] == '\0';
            }
            /* UNARY_OP STRING */
            if (_test_is_unary_op(args[0])) {
                return _test_unary(args[0], args[1]);
            }
            test_err_printf("%s: '%s': unary operator expected\n",
                            test_prog_name, args[0]);
            test_errors++;
            return false;

        case 3:
            /* ! EXPR(2) */
            if (args[0][0] == '!' && args[0][1] == '\0') {
                return !_test_posix(2, args + 1);
            }
            /* STRING1 OP STRING2 */
            if (_test_is_binary_op(args[1])) {
                return _test_binary(args[1], args[0], args[2]);
            }
            /* ( STRING ) */
            if (args[0][0] == '(' && args[0][1] == '\0' &&
                args[2][0] == ')' && args[2][1] == '\0') {
                return args[1][0] != '\0';
            }
            test_err_printf("%s: '%s': binary operator expected\n",
                            test_prog_name,
                            args[1] ? args[1] : "(null)");
            test_errors++;
            return false;

        case 4:
            /* ! EXPR(3) */
            if (args[0][0] == '!' && args[0][1] == '\0') {
                return !_test_posix(3, args + 1);
            }
            test_err_printf("%s: too many arguments\n", test_prog_name);
            test_errors++;
            return false;

        default:
            test_err_printf("%s: too many arguments\n", test_prog_name);
            test_errors++;
            return false;
    }
}

/**
 * @brief Parse and evaluate an OR expression
 *
 * Grammar: or_expr := and_expr ( -o and_expr )*
 *
 * @return true if any sub-expression is true
 */
static bool _test_or_expr(void)
{
    bool result = _test_and_expr();

    while (test_argc > 0 && test_args[0] &&
           test_args[0][0] == '-' && test_args[0][1] == 'o' &&
           test_args[0][2] == '\0') {
        test_args++;
        test_argc--;
        bool right = _test_and_expr();
        result = result || right;
    }

    return result;
}

/**
 * @brief Parse and evaluate an AND expression
 *
 * Grammar: and_expr := not_expr ( -a not_expr )*
 *
 * @return true if all sub-expressions are true
 */
static bool _test_and_expr(void)
{
    bool result = _test_not_expr();

    while (test_argc > 0 && test_args[0] &&
           test_args[0][0] == '-' && test_args[0][1] == 'a' &&
           test_args[0][2] == '\0') {
        test_args++;
        test_argc--;
        bool right = _test_not_expr();
        result = result && right;
    }

    return result;
}

/**
 * @brief Parse and evaluate a NOT expression
 *
 * Grammar: not_expr := ! not_expr | term
 *
 * @return negated or primary result
 */
static bool _test_not_expr(void)
{
    if (test_argc > 0 && test_args[0] &&
        test_args[0][0] == '!' && test_args[0][1] == '\0') {
        test_args++;
        test_argc--;
        return !_test_not_expr();
    }

    return _test_term();
}

/**
 * @brief Parse and evaluate a primary expression (term)
 *
 * Grammar:
 *   term := ( or_expr )
 *        |  UNARY_OP operand
 *        |  operand BINARY_OP operand
 *        |  operand   (true if non-empty)
 *
 * The parser uses lookahead to disambiguate operators from operands.
 *
 * @return evaluated result
 */
static bool _test_term(void)
{
    if (test_argc <= 0 || !test_args || !test_args[0]) {
        test_err_printf("%s: missing argument\n", test_prog_name);
        test_errors++;
        return false;
    }

    /* ( or_expr ) */
    if (test_args[0][0] == '(' && test_args[0][1] == '\0') {
        test_args++;
        test_argc--;
        bool result = _test_or_expr();
        if (test_argc > 0 && test_args[0] &&
            test_args[0][0] == ')' && test_args[0][1] == '\0') {
            test_args++;
            test_argc--;
        }
        else {
            test_err_printf("%s: missing ')'\n", test_prog_name);
            test_errors++;
        }
        return result;
    }

    /* UNARY_OP operand (need at least 2 remaining args) */
    if (test_argc >= 2 && _test_is_unary_op(test_args[0])) {
        const char * op = test_args[0];
        const char * arg = test_args[1];
        test_args += 2;
        test_argc -= 2;
        return _test_unary(op, arg);
    }

    /* operand BINARY_OP operand (need at least 3 remaining args) */
    if (test_argc >= 3 && _test_is_binary_op(test_args[1])) {
        const char * left = test_args[0];
        const char * op = test_args[1];
        const char * right = test_args[2];
        test_args += 3;
        test_argc -= 3;
        return _test_binary(op, left, right);
    }

    /* Single operand: true if non-empty */
    const char * s = test_args[0];
    test_args++;
    test_argc--;
    return s[0] != '\0';
}

/**
 * @brief Check if a string is a known unary operator
 * @param s  string to check
 * @return true if s is a unary operator
 */
static bool _test_is_unary_op(const char * s)
{
    if (!s || s[0] != '-' || s[1] == '\0' || s[2] != '\0') {
        return false;
    }

    switch (s[1]) {
        case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'G': case 'h': case 'k': case 'L':
        case 'O': case 'p': case 'r': case 's': case 'S':
        case 't': case 'u': case 'w': case 'x': case 'z':
        case 'n':
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if a string is a known binary operator
 *
 * Note: -a and -o are NOT binary operators here; they are
 * logical operators handled by the recursive descent parser.
 *
 * @param s  string to check
 * @return true if s is a binary operator
 */
static bool _test_is_binary_op(const char * s)
{
    if (!s) {
        return false;
    }

    /* Single-char operators: =, <, > */
    if (s[0] == '=' && s[1] == '\0') {
        return true;
    }
    if (s[0] == '<' && s[1] == '\0') {
        return true;
    }
    if (s[0] == '>' && s[1] == '\0') {
        return true;
    }

    /* != operator */
    if (s[0] == '!' && s[1] == '=' && s[2] == '\0') {
        return true;
    }

    /* Dash-prefixed operators: -eq -ne -lt -le -gt -ge -nt -ot -ef */
    if (s[0] == '-' && s[1] != '\0' && s[2] != '\0' && s[3] == '\0') {
        if (s[1] == 'e') {
            return s[2] == 'q' || s[2] == 'f';
        }
        if (s[1] == 'n') {
            return s[2] == 'e' || s[2] == 't';
        }
        if (s[1] == 'l') {
            return s[2] == 't' || s[2] == 'e';
        }
        if (s[1] == 'g') {
            return s[2] == 't' || s[2] == 'e';
        }
        if (s[1] == 'o') {
            return s[2] == 't';
        }
    }

    return false;
}

/**
 * @brief Evaluate a unary operator on an argument
 *
 * Handles string operators (-z, -n), terminal test (-t),
 * and all file tests.
 *
 * @param op   operator string
 * @param arg  operand string
 * @return true if condition is met
 */
static bool _test_unary(const char * op, const char * arg)
{
    if (!op || !arg) {
        test_errors++;
        return false;
    }

    /* String operators */
    if (test_streq(op, "-z")) {
        return arg[0] == '\0';
    }
    if (test_streq(op, "-n")) {
        return arg[0] != '\0';
    }

    /* Terminal test: -t FD */
    if (test_streq(op, "-t")) {
        long fd;
        if (!_test_parse_int(arg, &fd)) {
            test_err_printf("%s: '%s': integer expression expected\n",
                            test_prog_name, arg);
            test_errors++;
            return false;
        }
#ifdef TEST_PLATFORM_WINDOWS
        return _isatty((int)fd) != 0;
#else
        return isatty((int)fd) != 0;
#endif
    }

    /* File tests */
    return _test_file_test(op, arg);
}

/**
 * @brief Evaluate a binary operator on two operands
 *
 * Handles string comparison (=, !=, <, >),
 * integer comparison (-eq, -ne, -lt, -le, -gt, -ge),
 * and file comparison (-nt, -ot, -ef).
 *
 * @param op     operator string
 * @param left   left operand
 * @param right  right operand
 * @return true if condition is met
 */
static bool _test_binary(const char * op, const char * left,
                         const char * right)
{
    if (!op || !left || !right) {
        test_errors++;
        return false;
    }

    /* String comparison */
    if (test_streq(op, "=")) {
        return strcmp(left, right) == 0;
    }
    if (test_streq(op, "!=")) {
        return strcmp(left, right) != 0;
    }
    if (test_streq(op, "<")) {
        return strcmp(left, right) < 0;
    }
    if (test_streq(op, ">")) {
        return strcmp(left, right) > 0;
    }

    /* File comparison */
    if (test_streq(op, "-nt")) {
        return _test_file_newer(left, right);
    }
    if (test_streq(op, "-ot")) {
        return _test_file_older(left, right);
    }
    if (test_streq(op, "-ef")) {
        return _test_file_same(left, right);
    }

    /* Integer comparison */
    long li, ri;
    if (!_test_parse_int(left, &li)) {
        test_err_printf("%s: '%s': integer expression expected\n",
                        test_prog_name, left);
        test_errors++;
        return false;
    }
    if (!_test_parse_int(right, &ri)) {
        test_err_printf("%s: '%s': integer expression expected\n",
                        test_prog_name, right);
        test_errors++;
        return false;
    }

    if (test_streq(op, "-eq")) {
        return li == ri;
    }
    if (test_streq(op, "-ne")) {
        return li != ri;
    }
    if (test_streq(op, "-lt")) {
        return li < ri;
    }
    if (test_streq(op, "-le")) {
        return li <= ri;
    }
    if (test_streq(op, "-gt")) {
        return li > ri;
    }
    if (test_streq(op, "-ge")) {
        return li >= ri;
    }

    test_err_printf("%s: '%s': unknown operator\n", test_prog_name, op);
    test_errors++;
    return false;
}

/**
 * @brief Evaluate a file test operator
 *
 * Performs stat() / lstat() on the path and checks the
 * corresponding property.
 *
 * @param op    file operator (-e, -f, -d, etc.)
 * @param path  file path
 * @return true if the file test passes
 */
static bool _test_file_test(const char * op, const char * path)
{
    if (!op || !path) {
        test_errors++;
        return false;
    }

    /* Determine whether to use lstat (for -h / -L) */
    bool use_lstat = test_streq(op, "-h") || test_streq(op, "-L");

    struct stat st;
    int rc;

#ifdef TEST_PLATFORM_WINDOWS
    /* Windows: _stat doesn't have lstat; use GetFileAttributes for symlinks */
    rc = stat(path, &st);
    (void)use_lstat; /* not used on Windows */
#else
    if (use_lstat) {
        rc = lstat(path, &st);
    }
    else {
        rc = stat(path, &st);
    }
#endif

    /* -e: file exists */
    if (test_streq(op, "-e")) {
        return rc == 0;
    }

    /* All other file tests require existence */
    if (rc != 0) {
        return false;
    }

    /* File type tests */
    if (test_streq(op, "-d")) {
        return S_ISDIR(st.st_mode) != 0;
    }
    if (test_streq(op, "-f")) {
        return S_ISREG(st.st_mode) != 0;
    }
    if (test_streq(op, "-b")) {
        return S_ISBLK(st.st_mode) != 0;
    }
    if (test_streq(op, "-c")) {
        return S_ISCHR(st.st_mode) != 0;
    }
    if (test_streq(op, "-p")) {
        return S_ISFIFO(st.st_mode) != 0;
    }
    if (test_streq(op, "-S")) {
        return S_ISSOCK(st.st_mode) != 0;
    }

#ifdef TEST_PLATFORM_WINDOWS
    /* Windows: symlinks are reparse points; lstat not available */
    if (test_streq(op, "-h") || test_streq(op, "-L")) {
        DWORD attr = GetFileAttributesA(path);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }
#else
    if (test_streq(op, "-h") || test_streq(op, "-L")) {
        return S_ISLNK(st.st_mode) != 0;
    }
#endif

    /* File size: -s */
    if (test_streq(op, "-s")) {
        return st.st_size > 0;
    }

    /* Permission bits: -g, -u, -k */
#ifndef TEST_PLATFORM_WINDOWS
    if (test_streq(op, "-g")) {
        return (st.st_mode & S_ISGID) != 0;
    }
    if (test_streq(op, "-u")) {
        return (st.st_mode & S_ISUID) != 0;
    }
    if (test_streq(op, "-k")) {
        return (st.st_mode & S_ISVTX) != 0;
    }
    /* Ownership: -O, -G */
    if (test_streq(op, "-O")) {
        return st.st_uid == geteuid();
    }
    if (test_streq(op, "-G")) {
        return st.st_gid == getegid();
    }
#else
    /* Windows: no permission bits or ownership concepts */
    if (test_streq(op, "-g") || test_streq(op, "-u") ||
        test_streq(op, "-k") || test_streq(op, "-O") ||
        test_streq(op, "-G")) {
        return false;
    }
#endif

    /* Readable: -r */
    if (test_streq(op, "-r")) {
#ifdef TEST_PLATFORM_WINDOWS
        return _access(path, 4) == 0;
#else
        return access(path, R_OK) == 0;
#endif
    }

    /* Writable: -w */
    if (test_streq(op, "-w")) {
#ifdef TEST_PLATFORM_WINDOWS
        return _access(path, 2) == 0;
#else
        return access(path, W_OK) == 0;
#endif
    }

    /* Executable: -x */
    if (test_streq(op, "-x")) {
#ifdef TEST_PLATFORM_WINDOWS
        return _test_win_executable(path);
#else
        return access(path, X_OK) == 0;
#endif
    }

    test_err_printf("%s: '%s': unknown file operator\n",
                    test_prog_name, op);
    test_errors++;
    return false;
}

/**
 * @brief Compare file modification times (newer than)
 *
 * -nt: true if file1 exists and (file2 doesn't exist
 *      or file1's mtime > file2's mtime).
 *
 * @param f1  first file path
 * @param f2  second file path
 * @return true if f1 is newer than f2
 */
static bool _test_file_newer(const char * f1, const char * f2)
{
    struct stat st1, st2;

    if (stat(f1, &st1) != 0) {
        return false;
    }
    if (stat(f2, &st2) != 0) {
        return true;
    }

#ifdef TEST_PLATFORM_WINDOWS
    return st1.st_mtime > st2.st_mtime;
#else
    if (st1.st_mtime != st2.st_mtime) {
        return st1.st_mtime > st2.st_mtime;
    }
    /* If seconds are equal, compare nanoseconds */
    #ifdef st_mtim
    return st1.st_mtim.tv_nsec > st2.st_mtim.tv_nsec;
    #else
    return false;
    #endif
#endif
}

/**
 * @brief Compare file modification times (older than)
 *
 * -ot: true if file2 exists and (file1 doesn't exist
 *      or file1's mtime < file2's mtime).
 *
 * @param f1  first file path
 * @param f2  second file path
 * @return true if f1 is older than f2
 */
static bool _test_file_older(const char * f1, const char * f2)
{
    struct stat st1, st2;

    if (stat(f2, &st2) != 0) {
        return false;
    }
    if (stat(f1, &st1) != 0) {
        return true;
    }

#ifdef TEST_PLATFORM_WINDOWS
    return st1.st_mtime < st2.st_mtime;
#else
    if (st1.st_mtime != st2.st_mtime) {
        return st1.st_mtime < st2.st_mtime;
    }
    #ifdef st_mtim
    return st1.st_mtim.tv_nsec < st2.st_mtim.tv_nsec;
    #else
    return false;
    #endif
#endif
}

/**
 * @brief Check if two files are the same (same device and inode)
 *
 * -ef: true if both files exist and have the same st_dev and st_ino.
 *
 * @param f1  first file path
 * @param f2  second file path
 * @return true if files are the same
 */
static bool _test_file_same(const char * f1, const char * f2)
{
    struct stat st1, st2;

    if (stat(f1, &st1) != 0) {
        return false;
    }
    if (stat(f2, &st2) != 0) {
        return false;
    }

    return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
}

/**
 * @brief Parse a string as an integer (base 10)
 *
 * The entire string must be a valid integer; no trailing characters.
 *
 * @param s    string to parse
 * @param out  output integer value
 * @return true on success, false if not a valid integer
 */
static bool _test_parse_int(const char * s, long * out)
{
    if (!s || !out || s[0] == '\0') {
        return false;
    }

    char * end = NULL;
    errno = 0;
    long val = strtol(s, &end, 10);

    if (errno != 0 || !end || *end != '\0' || end == s) {
        return false;
    }

    *out = val;
    return true;
}

/**
 * @brief Case-insensitive string comparison
 * @param a  first string
 * @param b  second string
 * @return true if strings are equal (case-insensitive)
 */
static bool _test_str_ieq(const char * a, const char * b)
{
    if (!a || !b) {
        return a == b;
    }

    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca + 32);
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb + 32);
        }
        if (ca != cb) {
            return false;
        }
        a++;
        b++;
    }

    return *a == *b;
}

#ifdef TEST_PLATFORM_WINDOWS
/**
 * @brief Check if a file is executable on Windows
 *
 * On Windows, there is no execute permission bit. A file is
 * considered executable if it has a recognized executable
 * extension (.exe, .bat, .cmd, .com, .ps1, .sh).
 *
 * @param path  file path
 * @return true if file appears to be executable
 */
static bool _test_win_executable(const char * path)
{
    if (!path) {
        return false;
    }

    size_t len = strlen(path);

    /* Check for executable extensions (case-insensitive) */
    const char * exts[] = {".exe", ".bat", ".cmd", ".com", ".ps1", ".sh"};
    size_t ext_count = sizeof(exts) / sizeof(exts[0]);

    for (size_t i = 0; i < ext_count; i++) {
        size_t ext_len = strlen(exts[i]);
        if (len >= ext_len) {
            const char * suffix = path + len - ext_len;
            if (_test_str_ieq(suffix, exts[i])) {
                /* Verify file exists */
                struct stat st;
                if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                    return true;
                }
            }
        }
    }

    /* Directories are "executable" (traversable) on Unix;
     * on Windows, we'll also return true for directories */
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    return false;
}
#endif
