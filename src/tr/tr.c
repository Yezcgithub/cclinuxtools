/**
 * @file tr.c
 * @brief Cross-platform implementation of the coreutils tr command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils tr(1) (coreutils 9.11+).
 *
 * Key behaviors:
 *   - -c, -C, --complement:       use the complement of STRING1
 *   - -d, --delete:               delete characters in STRING1
 *   - -s, --squeeze-repeats:      replace repeats in the last specified SET
 *   - -t, --truncate-set1:        truncate STRING1 to length of STRING2
 *   - --help / --version:         display help or version information
 *   - Reads from standard input, writes to standard output
 *
 * SET specification:
 *   \NNN            character with octal value NNN (1 to 3 octal digits)
 *   \\              backslash
 *   \a              audible BEL
 *   \b              backspace
 *   \f              form feed
 *   \n              new line
 *   \r              return
 *   \t              horizontal tab
 *   \v              vertical tab
 *   CHAR1-CHAR2     all characters from CHAR1 to CHAR2 in ascending order
 *   [CHAR*]         in SET2, copies of CHAR until length of SET1
 *   [CHAR*REPEAT]   REPEAT copies of CHAR, REPEAT octal if starting with 0
 *   [:alnum:]       all letters and digits
 *   [:alpha:]       all letters
 *   [:blank:]       all horizontal whitespace
 *   [:cntrl:]       all control characters
 *   [:digit:]       all digits
 *   [:graph:]       all printable characters, not including space
 *   [:lower:]       all lower case letters
 *   [:print:]       all printable characters, including space
 *   [:punct:]       all punctuation characters
 *   [:space:]       all horizontal or vertical whitespace
 *   [:upper:]       all upper case letters
 *   [:xdigit:]      all hexadecimal digits
 *   [=CHAR=]        all characters which are equivalent to CHAR
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o tr.exe tr.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o tr tr.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o tr tr.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o tr tr.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o tr tr.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o tr tr.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/tr>
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
    #define TR_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TR_PLATFORM_LINUX   1
    #define TR_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TR_PLATFORM_MACOS   1
    #define TR_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TR_PLATFORM_FREEBSD 1
    #define TR_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TR_PLATFORM_OPENBSD 1
    #define TR_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TR_PLATFORM_NETBSD  1
    #define TR_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define TR_PLATFORM_POSIX   1
#else
    #define TR_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef TR_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef TR_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef TR_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>

#ifdef TR_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define TR_VERSION_STR "v1.0.0"

/** @brief Number of possible character values (0..255) */
#define TR_N_CHARS 256

/** @brief Read buffer size */
#define TR_BUF_SIZE 8192

/********************************
 *    typedefs
 ********************************/

/**
 * @brief A character set — a boolean array over all 256 byte values,
 *        plus an ordered list for translation (preserves order and repeats).
 */
typedef struct {
    bool chars[TR_N_CHARS];   /**< chars[c] is true if c is in the set */
    int count;                /**< Number of distinct characters in the set */
    int order[TR_N_CHARS];    /**< Ordered list of characters (for SET2) */
    int order_len;            /**< Length of ordered list (may exceed count) */
} tr_set;

/**
 * @brief Options structure for tr
 */
typedef struct {
    bool complement;          /**< -c: complement STRING1 */
    bool delete;              /**< -d: delete chars in STRING1 */
    bool squeeze;             /**< -s: squeeze repeats */
    bool truncate;            /**< -t: truncate STRING1 to STRING2 length */
} tr_opts;

/********************************
 *    static prototypes
 ********************************/
static void _tr_print_help(void);
static void _tr_print_version(void);
static bool _tr_streq(const char * a, const char * b);
static void _tr_set_add(tr_set * s, int c);
static int  _tr_parse_set(const char * str, bool is_set2, int set1_len,
                          bool complement, tr_set * out);
static int  _tr_parse_octal(const char ** p);
static void _tr_expand_class(const char * name, tr_set * s);
static void _tr_complement(tr_set * s);
static int  _tr_run(const tr_opts * opts, const tr_set * set1,
                    const tr_set * set2, const tr_set * squeeze_set);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream.
 *        Defaults to libc @c stdout .
 */
#ifndef tr_out_stream
    #define tr_out_stream stdout
#endif

/**
 * @brief Default error stream.
 *        Defaults to libc @c stderr .
 */
#ifndef tr_err_stream
    #define tr_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 */
#ifndef tr_printf
    #define tr_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef tr_err_printf
    #define tr_err_printf(fmt, ...) fprintf(tr_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 */
#ifndef tr_putchar
    #define tr_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 */
#ifndef tr_fflush
    #define tr_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the tr command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Parse STRING1 and STRING2
 *   3. Validate option/operand combinations
 *   4. Process stdin → stdout
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
#ifdef TR_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    tr_opts opts;
    memset(&opts, 0, sizeof(opts));

    const char * set1_str = NULL;
    const char * set2_str = NULL;
    int operand_start = argc;

    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_tr_streq(arg, "--")) {
            operand_start = i + 1;
            break;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_tr_streq(arg, "--help")) {
                _tr_print_help();
                return 0;
            }
            if (_tr_streq(arg, "--version")) {
                _tr_print_version();
                return 0;
            }
            if (_tr_streq(arg, "--complement")) {
                opts.complement = true;
                continue;
            }
            if (_tr_streq(arg, "--delete")) {
                opts.delete = true;
                continue;
            }
            if (_tr_streq(arg, "--squeeze-repeats")) {
                opts.squeeze = true;
                continue;
            }
            if (_tr_streq(arg, "--truncate-set1")) {
                opts.truncate = true;
                continue;
            }

            tr_err_printf("tr: unrecognized option '%s'\n", arg);
            tr_err_printf("Try 'tr --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case 'c':
                    case 'C':
                        opts.complement = true;
                        p++;
                        break;

                    case 'd':
                        opts.delete = true;
                        p++;
                        break;

                    case 's':
                        opts.squeeze = true;
                        p++;
                        break;

                    case 't':
                        opts.truncate = true;
                        p++;
                        break;

                    default:
                        tr_err_printf("tr: invalid option -- '%c'\n", *p);
                        tr_err_printf("Try 'tr --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — first operand */
        operand_start = i;
        break;
    }

    /* Collect operands */
    int num_operands = argc - operand_start;
    if (num_operands < 1) {
        tr_err_printf("tr: missing operand\n");
        tr_err_printf("Try 'tr --help' for more information.\n");
        return 1;
    }

    set1_str = argv[operand_start];

    if (num_operands >= 2) {
        set2_str = argv[operand_start + 1];
    }

    if (num_operands > 2) {
        if (opts.delete && !opts.squeeze) {
            tr_err_printf("tr: extra operand '%s'\n", argv[operand_start + 2]);
            tr_err_printf("Only one string may be given when deleting without squeezing repeats.\n");
            return 1;
        }
        tr_err_printf("tr: extra operand '%s'\n", argv[operand_start + 2]);
        return 1;
    }

    /* Validate operand counts for different modes */
    if (opts.delete && opts.squeeze) {
        if (num_operands < 2) {
            tr_err_printf("tr: missing operand after '%s'\n", set1_str);
            tr_err_printf("Two strings must be given when both deleting and squeezing repeats.\n");
            return 1;
        }
    }
    else if (opts.delete) {
        /* -d only: one operand */
        /* set2_str is not used */
    }
    else if (opts.squeeze) {
        /* -s only: one or two operands */
        if (!set2_str && !opts.complement) {
            /* tr -s SET: squeeze using SET1 */
        }
    }
    else {
        /* Translation: two operands required */
        if (!set2_str) {
            tr_err_printf("tr: missing operand after '%s'\n", set1_str);
            tr_err_printf("Two strings must be given when translating.\n");
            return 1;
        }
    }

    /* Parse SET1 */
    tr_set set1;
    memset(&set1, 0, sizeof(set1));
    int set1_len = _tr_parse_set(set1_str, false, 0, false, &set1);
    if (set1_len < 0) {
        return 1;
    }

    /* Apply complement to SET1 if requested */
    if (opts.complement) {
        _tr_complement(&set1);
    }

    /* Parse SET2 */
    tr_set set2;
    memset(&set2, 0, sizeof(set2));
    int set2_len = 0;

    if (set2_str) {
        set2_len = _tr_parse_set(set2_str, true, set1.order_len, false, &set2);
        if (set2_len < 0) {
            return 1;
        }
    }

    /* Determine the squeeze set */
    tr_set squeeze_set;
    memset(&squeeze_set, 0, sizeof(squeeze_set));

    if (opts.squeeze) {
        if (opts.delete && set2_str) {
            /* -ds: squeeze on SET2 (last specified) */
            squeeze_set = set2;
        }
        else if (set2_str) {
            /* -s with two strings: squeeze on SET2 */
            squeeze_set = set2;
        }
        else {
            /* -s with one string: squeeze on SET1 */
            squeeze_set = set1;
        }
    }

    /* For translation, handle SET1/SET2 length adjustments */
    if (!opts.delete && set2_str) {
        /* Build translation table */
        /* If -t, truncate SET1 to SET2 length */
        /* If not -t, extend SET2 to SET1 length by repeating last char */
        /* The actual translation table is built in _tr_run */
    }

    /* Run the main processing loop */
    int rc = _tr_run(&opts, &set1, &set2, &squeeze_set);
    if (rc != 0) {
        return 1;
    }

    tr_fflush(tr_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal
 */
static bool _tr_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/**
 * @brief Add a character to a set (both boolean array and ordered list).
 * @param s  Set to add to
 * @param c  Character to add (0..255)
 */
static void _tr_set_add(tr_set * s, int c)
{
    if (!s->chars[c]) {
        s->chars[c] = true;
        s->count++;
    }
    if (s->order_len < TR_N_CHARS) {
        s->order[s->order_len++] = c;
    }
}

/**
 * @brief Parse an octal escape sequence.
 * @param p  Pointer to the string (pointing at first octal digit).
 *           On return, advanced past the octal digits.
 * @return The parsed character value (0..255), or -1 on error.
 */
static int _tr_parse_octal(const char ** p)
{
    int val = 0;
    int count = 0;

    while (count < 3 && **p >= '0' && **p <= '7') {
        val = val * 8 + (**p - '0');
        (*p)++;
        count++;
    }

    if (count == 0) {
        return -1;
    }

    if (val > 255) {
        return -1;
    }

    return val;
}

/**
 * @brief Expand a POSIX character class into the set.
 * @param name  Class name (e.g. "alnum", "alpha", "digit")
 * @param s     Set to expand into
 */
static void _tr_expand_class(const char * name, tr_set * s)
{
    for (int c = 0; c < TR_N_CHARS; c++) {
        bool match = false;

        if (strcmp(name, "alnum") == 0) {
            match = isalnum(c) != 0;
        }
        else if (strcmp(name, "alpha") == 0) {
            match = isalpha(c) != 0;
        }
        else if (strcmp(name, "blank") == 0) {
            match = (c == ' ' || c == '\t');
        }
        else if (strcmp(name, "cntrl") == 0) {
            match = iscntrl(c) != 0;
        }
        else if (strcmp(name, "digit") == 0) {
            match = (c >= '0' && c <= '9');
        }
        else if (strcmp(name, "graph") == 0) {
            match = isgraph(c) != 0;
        }
        else if (strcmp(name, "lower") == 0) {
            match = islower(c) != 0;
        }
        else if (strcmp(name, "print") == 0) {
            match = isprint(c) != 0;
        }
        else if (strcmp(name, "punct") == 0) {
            match = ispunct(c) != 0;
        }
        else if (strcmp(name, "space") == 0) {
            match = isspace(c) != 0;
        }
        else if (strcmp(name, "upper") == 0) {
            match = isupper(c) != 0;
        }
        else if (strcmp(name, "xdigit") == 0) {
            match = isxdigit(c) != 0;
        }

        if (match) {
            _tr_set_add(s, c);
        }
    }
}

/**
 * @brief Complement a set (invert membership).
 *        Rebuilds the ordered list with complemented chars in ascending order.
 * @param s  Set to complement in place
 */
static void _tr_complement(tr_set * s)
{
    for (int c = 0; c < TR_N_CHARS; c++) {
        s->chars[c] = !s->chars[c];
    }
    s->count = TR_N_CHARS - s->count;

    /* Rebuild ordered list with complemented characters in ascending order */
    s->order_len = 0;
    for (int c = 0; c < TR_N_CHARS; c++) {
        if (s->chars[c]) {
            s->order[s->order_len++] = c;
        }
    }
}

/**
 * @brief Parse a SET string into a tr_set.
 *
 * Handles: escapes (\n, \t, \NNN, etc.), ranges (a-z),
 *          character classes [:class:], [c*n] repetition,
 *          and [=c=] equivalence (treated as single char).
 *
 * @param str        SET string
 * @param is_set2    true if this is SET2 (allows [c*] construct)
 * @param set1_len   Length of SET1 (for [c*] expansion)
 * @param complement (unused, complement applied separately)
 * @param out        Output set
 * @return Number of characters in expanded set, or -1 on error
 */
static int _tr_parse_set(const char * str, bool is_set2, int set1_len,
                         bool complement, tr_set * out)
{
    (void)complement;

    const char * p = str;

    while (*p) {
        /* Escape sequences */
        if (*p == '\\') {
            p++;
            int c;

            switch (*p) {
                case 'a':  c = '\a';  p++; break;
                case 'b':  c = '\b';  p++; break;
                case 'f':  c = '\f';  p++; break;
                case 'n':  c = '\n';  p++; break;
                case 'r':  c = '\r';  p++; break;
                case 't':  c = '\t';  p++; break;
                case 'v':  c = '\v';  p++; break;
                case '\\': c = '\\';  p++; break;
                case '\0':
                    tr_err_printf("tr: backslash at end of string\n");
                    return -1;
                default:
                    /* Octal escape */
                    if (*p >= '0' && *p <= '7') {
                        c = _tr_parse_octal(&p);
                        if (c < 0) {
                            tr_err_printf("tr: invalid octal escape\n");
                            return -1;
                        }
                    }
                    else {
                        /* Unknown escape — treat as literal */
                        c = (unsigned char)*p;
                        p++;
                    }
                    break;
            }

            /* Check for range */
            if (*p == '-' && p[1] != '\0' && p[1] != ']') {
                p++;  /* skip '-' */
                int end_c;

                /* Parse end character (may also be escape) */
                if (*p == '\\') {
                    p++;
                    switch (*p) {
                        case 'a':  end_c = '\a';  p++; break;
                        case 'b':  end_c = '\b';  p++; break;
                        case 'f':  end_c = '\f';  p++; break;
                        case 'n':  end_c = '\n';  p++; break;
                        case 'r':  end_c = '\r';  p++; break;
                        case 't':  end_c = '\t';  p++; break;
                        case 'v':  end_c = '\v';  p++; break;
                        case '\\': end_c = '\\';  p++; break;
                        case '\0':
                            tr_err_printf("tr: backslash at end of string\n");
                            return -1;
                        default:
                            if (*p >= '0' && *p <= '7') {
                                end_c = _tr_parse_octal(&p);
                                if (end_c < 0) {
                                    tr_err_printf("tr: invalid octal escape\n");
                                    return -1;
                                }
                            }
                            else {
                                end_c = (unsigned char)*p;
                                p++;
                            }
                            break;
                    }
                }
                else {
                    end_c = (unsigned char)*p;
                    p++;
                }

                /* Expand range */
                if (c > end_c) {
                    tr_err_printf("tr: range-endpoints of '%c' and '%c' are in reverse order\n",
                                  c, end_c);
                    return -1;
                }
                for (int i = c; i <= end_c; i++) {
                    _tr_set_add(out, i);
                }
            }
            else {
                _tr_set_add(out, c);
            }
        }
        /* Character class [:name:] */
        else if (*p == '[' && p[1] == ':') {
            p += 2;  /* skip '[:' */
            char name[32];
            int ni = 0;

            while (*p && *p != ':' && ni < 31) {
                name[ni++] = *p++;
            }
            name[ni] = '\0';

            if (*p != ':' || p[1] != ']') {
                tr_err_printf("tr: invalid character class '%s'\n", name);
                return -1;
            }
            p += 2;  /* skip ':]' */

            _tr_expand_class(name, out);
        }
        /* Equivalence class [=c=] — treat as single character */
        else if (*p == '[' && p[1] == '=') {
            p += 2;  /* skip '[=' */
            int c = (unsigned char)*p;
            p++;

            if (*p != '=' || p[1] != ']') {
                tr_err_printf("tr: invalid equivalence class\n");
                return -1;
            }
            p += 2;  /* skip '=]' */

            _tr_set_add(out, c);
        }
        /* Repetition [c*n] or [c*] — only in SET2 */
        else if (*p == '[' && p[1] != ':' && p[1] != '=') {
            if (!is_set2) {
                tr_err_printf("tr: the [c*] repeat construct may not appear in string1\n");
                return -1;
            }

            p++;  /* skip '[' */
            int rep_char = (unsigned char)*p;
            p++;

            if (*p != '*') {
                tr_err_printf("tr: invalid repeat construct\n");
                return -1;
            }
            p++;  /* skip '*' */

            int repeat = -1;  /* -1 means "fill to set1 length" */
            if (*p != ']') {
                /* Parse repeat count */
                bool is_octal = (*p == '0');
                repeat = 0;
                while (*p >= '0' && *p <= '9') {
                    if (is_octal) {
                        repeat = repeat * 8 + (*p - '0');
                    }
                    else {
                        repeat = repeat * 10 + (*p - '0');
                    }
                    p++;
                }
            }

            if (*p != ']') {
                tr_err_printf("tr: invalid repeat construct\n");
                return -1;
            }
            p++;  /* skip ']' */

            if (repeat < 0) {
                /* [c*] — fill to set1_len */
                if (set1_len > 0) {
                    repeat = set1_len - out->count;
                    if (repeat < 0) {
                        repeat = 0;
                    }
                }
                else {
                    repeat = 0;
                }
            }

            for (int i = 0; i < repeat; i++) {
                _tr_set_add(out, rep_char);
            }
        }
        /* Range a-z */
        else if (p[1] == '-' && p[2] != '\0') {
            int start_c = (unsigned char)*p;
            p++;  /* skip start char */
            p++;  /* skip '-' */

            int end_c;
            if (*p == '\\') {
                p++;
                switch (*p) {
                    case 'a':  end_c = '\a';  p++; break;
                    case 'b':  end_c = '\b';  p++; break;
                    case 'f':  end_c = '\f';  p++; break;
                    case 'n':  end_c = '\n';  p++; break;
                    case 'r':  end_c = '\r';  p++; break;
                    case 't':  end_c = '\t';  p++; break;
                    case 'v':  end_c = '\v';  p++; break;
                    case '\\': end_c = '\\';  p++; break;
                    case '\0':
                        tr_err_printf("tr: backslash at end of string\n");
                        return -1;
                    default:
                        if (*p >= '0' && *p <= '7') {
                            end_c = _tr_parse_octal(&p);
                            if (end_c < 0) {
                                tr_err_printf("tr: invalid octal escape\n");
                                return -1;
                            }
                        }
                        else {
                            end_c = (unsigned char)*p;
                            p++;
                        }
                        break;
                }
            }
            else {
                end_c = (unsigned char)*p;
                p++;
            }

            if (start_c > end_c) {
                tr_err_printf("tr: range-endpoints of '%c' and '%c' are in reverse order\n",
                              start_c, end_c);
                return -1;
            }
            for (int i = start_c; i <= end_c; i++) {
                _tr_set_add(out, i);
            }
        }
        /* Single literal character */
        else {
            int c = (unsigned char)*p;
            p++;
            _tr_set_add(out, c);
        }
    }

    return out->count;
}

/**
 * @brief Main processing loop: read stdin, apply transform, write stdout.
 *
 * Modes:
 *   - delete only: skip chars in set1
 *   - translate only: map chars via set1→set2
 *   - squeeze only: collapse repeats in squeeze_set
 *   - delete + squeeze: delete set1 chars, then squeeze set2 chars
 *   - translate + squeeze: translate, then squeeze
 *
 * @param opts         Options
 * @param set1         Parsed SET1
 * @param set2         Parsed SET2 (may be empty)
 * @param squeeze_set  Squeeze set (may be empty)
 * @return 0 on success, 1 on error
 */
static int _tr_run(const tr_opts * opts, const tr_set * set1,
                   const tr_set * set2, const tr_set * squeeze_set)
{
    /* Build translation table if translating */
    int xlate[TR_N_CHARS];
    bool translating = (!opts->delete && set2->count > 0);

    if (translating) {
        for (int i = 0; i < TR_N_CHARS; i++) {
            xlate[i] = i;  /* identity */
        }

        /* Use the ordered lists from parsing */
        int s1_count = set1->order_len;
        int s2_count = set2->order_len;

        /* Handle -t: truncate SET1 to SET2 length */
        int effective_s1_count;
        if (opts->truncate) {
            effective_s1_count = (s1_count < s2_count) ? s1_count : s2_count;
        }
        else {
            effective_s1_count = s1_count;
        }

        /* Build translation map: SET1[i] → SET2[i] */
        for (int i = 0; i < effective_s1_count; i++) {
            int s2_char;
            if (i < s2_count) {
                s2_char = set2->order[i];
            }
            else {
                /* Extend SET2 by repeating last char */
                s2_char = set2->order[s2_count - 1];
            }
            xlate[set1->order[i]] = s2_char;
        }
    }

    /* Process input */
    unsigned char buf[TR_BUF_SIZE];
    size_t nread;
    int prev_squeeze = -1;  /* Last char squeezed (or -1) */

    while ((nread = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        for (size_t i = 0; i < nread; i++) {
            int c = buf[i];

            /* Delete mode: skip chars in set1 */
            if (opts->delete) {
                if (set1->chars[c]) {
                    continue;
                }
            }

            /* Translation */
            if (translating) {
                c = xlate[c];
            }

            /* Squeeze mode */
            if (opts->squeeze) {
                if (squeeze_set->chars[c] && c == prev_squeeze) {
                    /* Skip repeated char */
                    continue;
                }
                if (squeeze_set->chars[c]) {
                    prev_squeeze = c;
                }
                else {
                    prev_squeeze = -1;
                }
            }

            tr_putchar(c);
        }
    }

    if (ferror(stdin)) {
        tr_err_printf("tr: read error\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Print usage/help information
 */
static void _tr_print_help(void)
{
    tr_printf(
        "Usage: tr [OPTION]... STRING1 [STRING2]\n"
        "Translate, squeeze, and/or delete characters from standard input,\n"
        "writing to standard output.  STRING1 and STRING2 specify arrays of\n"
        "characters ARRAY1 and ARRAY2 that control the action.\n"
        "\n"
        "  -c, -C, --complement     use the complement of ARRAY1\n"
        "  -d, --delete             delete characters in ARRAY1, do not translate\n"
        "  -s, --squeeze-repeats    replace each sequence of a repeated character\n"
        "                             that is listed in the last specified ARRAY,\n"
        "                             with a single occurrence of that character\n"
        "  -t, --truncate-set1      first truncate ARRAY1 to length of ARRAY2\n"
        "\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "ARRAYs are specified as strings of characters.  Most represent themselves.\n"
        "Interpreted sequences are:\n"
        "\n"
        "  \\NNN            character with octal value NNN (1 to 3 octal digits)\n"
        "  \\\\              backslash\n"
        "  \\a              audible BEL\n"
        "  \\b              backspace\n"
        "  \\f              form feed\n"
        "  \\n              new line\n"
        "  \\r              return\n"
        "  \\t              horizontal tab\n"
        "  \\v              vertical tab\n"
        "  CHAR1-CHAR2     all characters from CHAR1 to CHAR2 in ascending order\n"
        "  [CHAR*]         in ARRAY2, copies of CHAR until length of ARRAY1\n"
        "  [CHAR*REPEAT]   REPEAT copies of CHAR, REPEAT octal if starting with 0\n"
        "  [:alnum:]       all letters and digits\n"
        "  [:alpha:]       all letters\n"
        "  [:blank:]       all horizontal whitespace\n"
        "  [:cntrl:]       all control characters\n"
        "  [:digit:]       all digits\n"
        "  [:graph:]       all printable characters, not including space\n"
        "  [:lower:]       all lower case letters\n"
        "  [:print:]       all printable characters, including space\n"
        "  [:punct:]       all punctuation characters\n"
        "  [:space:]       all horizontal or vertical whitespace\n"
        "  [:upper:]       all upper case letters\n"
        "  [:xdigit:]      all hexadecimal digits\n"
        "  [=CHAR=]        all characters which are equivalent to CHAR\n"
        "\n"
        "Translation occurs if -d is not given and both STRING1 and STRING2 appear.\n"
        "-t is only significant when translating.  ARRAY2 is extended to length of\n"
        "ARRAY1 by repeating its last character as necessary.  Excess characters\n"
        "of ARRAY2 are ignored.  Character classes expand in unspecified order;\n"
        "while translating, '[:lower:]' and '[:upper:]' may be used in pairs to\n"
        "specify case conversion.  Squeezing occurs after translation or deletion.\n"
        "Arguments like '[...]' should be quoted, to avoid potential shell globbing.\n"
    );
}

/**
 * @brief Print version information
 */
static void _tr_print_version(void)
{
    tr_printf("tr %s\n", TR_VERSION_STR);
    tr_printf("%s", "Copyright (C) 2025-2026 Yezc/tr\n");
    tr_printf("%s", "License MIT: <https://mit-license.org/>\n");
    tr_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    tr_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
