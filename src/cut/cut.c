/**
 * @file cut.c
 * @brief Cross-platform implementation of the coreutils cut command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils cut(1) (coreutils 9.11+).
 *
 * Key behaviors:
 *   - -b, --bytes=LIST:         select only these byte positions
 *   - -c, --characters=LIST:    select only these character positions
 *   - -d, --delimiter=DELIM:    use DELIM instead of TAB for field delimiter
 *   - -f, --fields=LIST:        select only these fields
 *   - -F LIST:                  like -f, but also implies -w and -O ' '
 *   - -n, --no-partial:         with -b, don't output partial multi-byte chars
 *   - -O, --output-delimiter=STRING: use STRING as output delimiter
 *   - -s, --only-delimited:     do not print lines not containing delimiters
 *   - -w, --whitespace-delimited[=trimmed]: use whitespace runs as delimiter
 *   - --complement:             complement the set of selected bytes/chars/fields
 *   - -z, --zero-terminated:    line delimiter is NUL, not newline
 *   - --help / --version:       display help or version information
 *   - With no FILE, or when FILE is -, read standard input
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o cut.exe cut.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o cut cut.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o cut cut.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o cut cut.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o cut cut.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o cut cut.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/cut>
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
    #define CUT_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define CUT_PLATFORM_LINUX   1
    #define CUT_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define CUT_PLATFORM_MACOS   1
    #define CUT_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define CUT_PLATFORM_FREEBSD 1
    #define CUT_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define CUT_PLATFORM_OPENBSD 1
    #define CUT_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define CUT_PLATFORM_NETBSD  1
    #define CUT_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define CUT_PLATFORM_POSIX   1
#else
    #define CUT_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef CUT_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef CUT_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef CUT_PLATFORM_NETBSD
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

#ifdef CUT_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define CUT_VERSION_STR "v1.0.0"

/** @brief Initial line buffer size */
#define CUT_INIT_BUF_SIZE 4096

/** @brief Maximum number of ranges in a LIST */
#define CUT_MAX_RANGES 256

/** @brief Default field delimiter */
#define CUT_DEFAULT_DELIM '\t'

/********************************
 *    typedefs
 ********************************/

/**
 * @brief A single range from the LIST specification.
 *        Positions are 1-based. start=0 means "from beginning", end=0 means "to end".
 */
typedef struct {
    unsigned int start;  /**< 1-based start, 0 means from first */
    unsigned int end;    /**< 1-based end, 0 means to last */
} cut_range;

/**
 * @brief Operating mode for cut
 */
typedef enum {
    CUT_MODE_NONE,    /**< No mode selected yet */
    CUT_MODE_BYTES,   /**< -b: byte mode */
    CUT_MODE_CHARS,   /**< -c: character mode */
    CUT_MODE_FIELDS   /**< -f / -F: field mode */
} cut_mode;

/**
 * @brief Options structure for cut
 */
typedef struct {
    cut_mode mode;            /**< Operating mode */
    bool complement;          /**< --complement */
    char delimiter;           /**< -d delimiter (default TAB) */
    bool only_delimited;      /**< -s: suppress lines without delimiter */
    bool whitespace_delim;    /**< -w: whitespace-delimited */
    bool ws_trimmed;          /**< --whitespace-delimited=trimmed */
    const char * output_delim; /**< -O: output delimiter string */
    int output_delim_len;     /**< Length of output delimiter */
    bool zero_terminated;     /**< -z: NUL line terminator */
    bool no_partial;          /**< -n: no partial multibyte (no-op) */
    cut_range ranges[CUT_MAX_RANGES]; /**< Parsed ranges */
    int num_ranges;           /**< Number of ranges */
} cut_opts;

/********************************
 *    static prototypes
 ********************************/
static void   _cut_print_help(void);
static void   _cut_print_version(void);
static bool   _cut_streq(const char * a, const char * b);
static int    _cut_parse_list(const char * list, cut_opts * opts);
static bool   _cut_position_selected(unsigned int pos, const cut_opts * opts);
static int    _cut_process_file(const char * filename, const cut_opts * opts);
static void   _cut_process_line_bytes(const char * line, size_t len,
                                      const cut_opts * opts);
static void   _cut_process_line_fields(const char * line, size_t len,
                                       const cut_opts * opts);
static bool   _cut_is_delim(char c, const cut_opts * opts);
static char * _cut_read_line(FILE * fp, bool zero_terminated,
                             size_t * out_len);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for cut_fputs / cut_fflush.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef cut_out_stream
    #define cut_out_stream stdout
#endif

/**
 * @brief Default error stream for cut_err_printf.
 *        Defaults to libc @c stderr .
 *        Define externally to redirect all error output.
 */
#ifndef cut_err_stream
    #define cut_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__"
 * (works on GCC, Clang, MSVC, MinGW; also accepted with a pedantic
 * warning in strict -std=c99 builds).
 */
#ifndef cut_printf
    #define cut_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef cut_err_printf
    #define cut_err_printf(fmt, ...) fprintf(cut_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 *
 * Note: we cast to unsigned char first so values with the MSB set do
 *       not trigger undefined behavior in putchar's @c int argument
 *       when char is signed on the host platform.
 */
#ifndef cut_putchar
    #define cut_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally cut_out_stream)
 */
#ifndef cut_fputs
    #define cut_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream (normally cut_out_stream)
 */
#ifndef cut_fflush
    #define cut_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the cut command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Validate options (exactly one of -b/-c/-f/-F must be given)
 *   3. Process each file (or stdin)
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
#ifdef CUT_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    cut_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.mode = CUT_MODE_NONE;
    opts.delimiter = CUT_DEFAULT_DELIM;
    opts.output_delim = NULL;
    opts.output_delim_len = -1;  /* -1 means not set */

    const char * list_str = NULL;
    int file_start = argc;

    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_cut_streq(arg, "--")) {
            file_start = i + 1;
            break;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_cut_streq(arg, "--help")) {
                _cut_print_help();
                return 0;
            }
            if (_cut_streq(arg, "--version")) {
                _cut_print_version();
                return 0;
            }
            if (_cut_streq(arg, "--complement")) {
                opts.complement = true;
                continue;
            }
            if (_cut_streq(arg, "--only-delimited")) {
                opts.only_delimited = true;
                continue;
            }
            if (_cut_streq(arg, "--zero-terminated")) {
                opts.zero_terminated = true;
                continue;
            }
            if (_cut_streq(arg, "--no-partial")) {
                opts.no_partial = true;
                continue;
            }
            if (_cut_streq(arg, "--bytes")) {
                if (i + 1 >= argc) {
                    cut_err_printf("cut: option '--bytes' requires an argument\n");
                    return 1;
                }
                opts.mode = CUT_MODE_BYTES;
                list_str = argv[++i];
                continue;
            }
            if (strncmp(arg, "--bytes=", 8) == 0) {
                opts.mode = CUT_MODE_BYTES;
                list_str = arg + 8;
                continue;
            }
            if (_cut_streq(arg, "--characters")) {
                if (i + 1 >= argc) {
                    cut_err_printf("cut: option '--characters' requires an argument\n");
                    return 1;
                }
                opts.mode = CUT_MODE_CHARS;
                list_str = argv[++i];
                continue;
            }
            if (strncmp(arg, "--characters=", 13) == 0) {
                opts.mode = CUT_MODE_CHARS;
                list_str = arg + 13;
                continue;
            }
            if (_cut_streq(arg, "--fields")) {
                if (i + 1 >= argc) {
                    cut_err_printf("cut: option '--fields' requires an argument\n");
                    return 1;
                }
                opts.mode = CUT_MODE_FIELDS;
                list_str = argv[++i];
                continue;
            }
            if (strncmp(arg, "--fields=", 9) == 0) {
                opts.mode = CUT_MODE_FIELDS;
                list_str = arg + 9;
                continue;
            }
            if (_cut_streq(arg, "--delimiter")) {
                if (i + 1 >= argc) {
                    cut_err_printf("cut: option '--delimiter' requires an argument\n");
                    return 1;
                }
                opts.delimiter = argv[++i][0];
                continue;
            }
            if (strncmp(arg, "--delimiter=", 12) == 0) {
                const char * d = arg + 12;
                if (d[0] == '\0') {
                    cut_err_printf("cut: the delimiter must be a single character\n");
                    return 1;
                }
                opts.delimiter = d[0];
                continue;
            }
            if (_cut_streq(arg, "--output-delimiter")) {
                if (i + 1 >= argc) {
                    cut_err_printf("cut: option '--output-delimiter' requires an argument\n");
                    return 1;
                }
                opts.output_delim = argv[++i];
                opts.output_delim_len = (int)strlen(opts.output_delim);
                continue;
            }
            if (strncmp(arg, "--output-delimiter=", 19) == 0) {
                opts.output_delim = arg + 19;
                opts.output_delim_len = (int)strlen(opts.output_delim);
                continue;
            }
            if (_cut_streq(arg, "--whitespace-delimited")) {
                opts.whitespace_delim = true;
                continue;
            }
            if (strncmp(arg, "--whitespace-delimited=trimmed", 30) == 0) {
                opts.whitespace_delim = true;
                opts.ws_trimmed = true;
                continue;
            }

            cut_err_printf("cut: unrecognized option '%s'\n", arg);
            cut_err_printf("Try 'cut --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            /* Handle combined short options and option-argument */
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case 'b':
                        opts.mode = CUT_MODE_BYTES;
                        if (p[1] != '\0') {
                            list_str = p + 1;
                            p = "";
                        }
                        else if (i + 1 < argc) {
                            list_str = argv[++i];
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'b'\n");
                            return 1;
                        }
                        break;

                    case 'c':
                        opts.mode = CUT_MODE_CHARS;
                        if (p[1] != '\0') {
                            list_str = p + 1;
                            p = "";
                        }
                        else if (i + 1 < argc) {
                            list_str = argv[++i];
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'c'\n");
                            return 1;
                        }
                        break;

                    case 'f':
                        opts.mode = CUT_MODE_FIELDS;
                        if (p[1] != '\0') {
                            list_str = p + 1;
                            p = "";
                        }
                        else if (i + 1 < argc) {
                            list_str = argv[++i];
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'f'\n");
                            return 1;
                        }
                        break;

                    case 'F':
                        opts.mode = CUT_MODE_FIELDS;
                        opts.whitespace_delim = true;
                        if (opts.output_delim_len < 0) {
                            opts.output_delim = " ";
                            opts.output_delim_len = 1;
                        }
                        if (p[1] != '\0') {
                            list_str = p + 1;
                            p = "";
                        }
                        else if (i + 1 < argc) {
                            list_str = argv[++i];
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'F'\n");
                            return 1;
                        }
                        break;

                    case 'd':
                        if (p[1] != '\0') {
                            opts.delimiter = p[1];
                            p += 2;
                        }
                        else if (i + 1 < argc) {
                            opts.delimiter = argv[++i][0];
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'd'\n");
                            return 1;
                        }
                        break;

                    case 'O':
                        if (p[1] != '\0') {
                            opts.output_delim = p + 1;
                            opts.output_delim_len = (int)strlen(opts.output_delim);
                            p = "";
                        }
                        else if (i + 1 < argc) {
                            opts.output_delim = argv[++i];
                            opts.output_delim_len = (int)strlen(opts.output_delim);
                            p = "";
                        }
                        else {
                            cut_err_printf("cut: option requires an argument -- 'O'\n");
                            return 1;
                        }
                        break;

                    case 's':
                        opts.only_delimited = true;
                        p++;
                        break;

                    case 'n':
                        opts.no_partial = true;
                        p++;
                        break;

                    case 'z':
                        opts.zero_terminated = true;
                        p++;
                        break;

                    case 'w':
                        opts.whitespace_delim = true;
                        p++;
                        break;

                    default:
                        cut_err_printf("cut: invalid option -- '%c'\n", *p);
                        cut_err_printf("Try 'cut --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — first file */
        file_start = i;
        break;
    }

    /* Validate: exactly one of -b/-c/-f/-F must be specified */
    if (opts.mode == CUT_MODE_NONE) {
        cut_err_printf("cut: you must specify a list of bytes, characters, or fields\n");
        cut_err_printf("Try 'cut --help' for more information.\n");
        return 1;
    }

    /* Parse the LIST */
    if (!list_str || list_str[0] == '\0') {
        cut_err_printf("cut: the list is empty\n");
        return 1;
    }
    if (_cut_parse_list(list_str, &opts) != 0) {
        return 1;
    }

    /* Set default output delimiter for field mode */
    if (opts.mode == CUT_MODE_FIELDS) {
        if (opts.output_delim_len < 0) {
            if (opts.whitespace_delim) {
                opts.output_delim = " ";
                opts.output_delim_len = 1;
            }
            else {
                /* Default: use input delimiter as a single-char string */
                static char default_out[2];
                default_out[0] = opts.delimiter;
                default_out[1] = '\0';
                opts.output_delim = default_out;
                opts.output_delim_len = 1;
            }
        }
    }

    /* -d with -b or -c is an error */
    if (opts.delimiter != CUT_DEFAULT_DELIM && opts.mode != CUT_MODE_FIELDS) {
        cut_err_printf("cut: only one type of list may be specified\n");
        return 1;
    }

    /* -s only valid with -f */
    if (opts.only_delimited && opts.mode != CUT_MODE_FIELDS) {
        cut_err_printf("cut: -s must be used with -f\n");
        return 1;
    }

    int exit_code = 0;
    int num_files = argc - file_start;

    if (num_files <= 0) {
        if (_cut_process_file("-", &opts) != 0) {
            exit_code = 1;
        }
    }
    else {
        for (int i = file_start; i < argc; i++) {
            if (_cut_process_file(argv[i], &opts) != 0) {
                exit_code = 1;
            }
        }
    }

    cut_fflush(cut_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _cut_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/**
 * @brief Check if a character is a delimiter
 * @param c     Character to check
 * @param opts  Options structure
 * @return true if c is a delimiter
 */
static bool _cut_is_delim(char c, const cut_opts * opts)
{
    if (opts->whitespace_delim) {
        return c == ' ' || c == '\t';
    }
    return c == opts->delimiter;
}

/**
 * @brief Parse a LIST specification into ranges
 *
 * LIST format: N, N-, N-M, -M, separated by commas
 *
 * @param list   LIST string
 * @param opts   Options (ranges stored here)
 * @return 0 on success, -1 on error
 */
static int _cut_parse_list(const char * list, cut_opts * opts)
{
    opts->num_ranges = 0;
    const char * p = list;

    while (*p) {
        if (opts->num_ranges >= CUT_MAX_RANGES) {
            cut_err_printf("cut: too many ranges in list\n");
            return -1;
        }

        cut_range * r = &opts->ranges[opts->num_ranges];
        r->start = 0;
        r->end = 0;

        /* Parse start number */
        if (*p == '-') {
            r->start = 1;  /* -M means 1-M */
        }
        else if (isdigit((unsigned char)*p)) {
            char * end;
            unsigned long val = strtoul(p, &end, 10);
            if (val == 0 || val > UINT_MAX) {
                cut_err_printf("cut: invalid byte/character/field position '%s'\n", list);
                return -1;
            }
            r->start = (unsigned int)val;
            p = end;
        }
        else {
            cut_err_printf("cut: invalid byte/character/field value '%s'\n", list);
            return -1;
        }

        if (*p == '-') {
            p++;
            if (*p == '\0' || *p == ',') {
                r->end = 0;  /* to end */
            }
            else if (isdigit((unsigned char)*p)) {
                char * end;
                unsigned long val = strtoul(p, &end, 10);
                if (val == 0 || val > UINT_MAX) {
                    cut_err_printf("cut: invalid byte/character/field position '%s'\n", list);
                    return -1;
                }
                r->end = (unsigned int)val;
                p = end;
            }
            else {
                cut_err_printf("cut: invalid range '%s'\n", list);
                return -1;
            }
        }
        else {
            r->end = r->start;  /* single position */
        }

        /* Validate range */
        if (r->end != 0 && r->end < r->start) {
            cut_err_printf("cut: invalid decreasing range\n");
            return -1;
        }

        opts->num_ranges++;

        if (*p == ',') {
            p++;
        }
        else if (*p == '\0') {
            break;
        }
        else {
            cut_err_printf("cut: invalid byte/character/field list '%s'\n", list);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Check if a 1-based position is in the selected set
 * @param pos   1-based position
 * @param opts  Options (with ranges and complement)
 * @return true if position is selected
 */
static bool _cut_position_selected(unsigned int pos, const cut_opts * opts)
{
    bool in_range = false;
    for (int i = 0; i < opts->num_ranges; i++) {
        const cut_range * r = &opts->ranges[i];
        unsigned int start = (r->start == 0) ? 1 : r->start;
        unsigned int end = r->end;
        if (end == 0) {
            /* open-ended: start to end of line */
            if (pos >= start) {
                in_range = true;
                break;
            }
        }
        else {
            if (pos >= start && pos <= end) {
                in_range = true;
                break;
            }
        }
    }
    return opts->complement ? !in_range : in_range;
}

/**
 * @brief Read a line from a file (dynamically allocated)
 * @param fp              File pointer
 * @param zero_terminated  If true, use NUL as line terminator
 * @param out_len         Receives line length (excluding terminator)
 * @return Allocated line buffer (caller must free), or NULL at EOF
 */
static char * _cut_read_line(FILE * fp, bool zero_terminated,
                             size_t * out_len)
{
    size_t cap = CUT_INIT_BUF_SIZE;
    size_t len = 0;
    char * buf = (char *)malloc(cap);
    if (!buf) {
        return NULL;
    }

    int term = zero_terminated ? '\0' : '\n';

    while (1) {
        int c = fgetc(fp);
        if (c == EOF) {
            if (len == 0) {
                free(buf);
                return NULL;
            }
            break;
        }
        if (c == term) {
            break;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char * newbuf = (char *)realloc(buf, cap);
            if (!newbuf) {
                free(buf);
                return NULL;
            }
            buf = newbuf;
        }
        buf[len++] = (char)c;
    }

    buf[len] = '\0';
    *out_len = len;
    return buf;
}

/**
 * @brief Process a line in byte or character mode
 * @param line   Line text (NUL-terminated)
 * @param len    Line length
 * @param opts   Options
 */
static void _cut_process_line_bytes(const char * line, size_t len,
                                    const cut_opts * opts)
{
    bool prev_selected = false;
    bool first_output = true;

    for (size_t i = 0; i < len; i++) {
        unsigned int pos = (unsigned int)(i + 1);
        bool selected = _cut_position_selected(pos, opts);

        if (selected) {
            if (!prev_selected && !first_output && opts->output_delim_len > 0) {
                /* Transition from unselected to selected: insert delimiter */
                for (int j = 0; j < opts->output_delim_len; j++) {
                    cut_putchar(opts->output_delim[j]);
                }
            }
            cut_putchar(line[i]);
            prev_selected = true;
            first_output = false;
        }
        else {
            prev_selected = false;
        }
    }

    if (opts->zero_terminated) {
        cut_putchar('\0');
    }
    else {
        cut_putchar('\n');
    }
}

/**
 * @brief Process a line in field mode
 * @param line   Line text (NUL-terminated)
 * @param len    Line length
 * @param opts   Options
 */
static void _cut_process_line_fields(const char * line, size_t len,
                                     const cut_opts * opts)
{
    /* Check if line contains delimiter */
    bool has_delim = false;
    for (size_t i = 0; i < len; i++) {
        if (_cut_is_delim(line[i], opts)) {
            has_delim = true;
            break;
        }
    }

    /* If no delimiter and -s is set, skip this line */
    if (!has_delim && opts->only_delimited) {
        return;
    }

    /* If no delimiter and -s is not set, print the entire line */
    if (!has_delim) {
        for (size_t i = 0; i < len; i++) {
            cut_putchar(line[i]);
        }
        if (opts->zero_terminated) {
            cut_putchar('\0');
        }
        else {
            cut_putchar('\n');
        }
        return;
    }

    /* Split into fields and select */
    bool first_output = true;
    size_t i = 0;

    /* Skip leading whitespace if trimmed mode */
    if (opts->ws_trimmed) {
        while (i < len && _cut_is_delim(line[i], opts)) {
            i++;
        }
    }

    unsigned int field_num = 1;

    while (i <= len) {
        /* Find end of current field */
        size_t field_start = i;
        while (i < len && !_cut_is_delim(line[i], opts)) {
            i++;
        }
        size_t field_end = i;

        if (_cut_position_selected(field_num, opts)) {
            if (!first_output && opts->output_delim_len > 0) {
                for (int j = 0; j < opts->output_delim_len; j++) {
                    cut_putchar(opts->output_delim[j]);
                }
            }
            for (size_t j = field_start; j < field_end; j++) {
                cut_putchar(line[j]);
            }
            first_output = false;
        }

        field_num++;

        if (i >= len) {
            break;
        }

        /* Skip delimiter(s) */
        if (opts->whitespace_delim) {
            while (i < len && _cut_is_delim(line[i], opts)) {
                i++;
            }
            if (opts->ws_trimmed) {
                /* In trimmed mode, trailing whitespace before end is not a new field */
                if (i >= len) {
                    break;
                }
            }
        }
        else {
            i++;  /* skip single delimiter */
        }
    }

    if (opts->zero_terminated) {
        cut_putchar('\0');
    }
    else {
        cut_putchar('\n');
    }
}

/**
 * @brief Process a single file
 * @param filename  File path or "-" for stdin
 * @param opts      Options
 * @return 0 on success, 1 on error
 */
static int _cut_process_file(const char * filename, const cut_opts * opts)
{
    FILE * fp;
    bool is_stdin = _cut_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            cut_err_printf("cut: %s: %s\n", filename, strerror(errno));
            return 1;
        }
    }

    char * line;
    size_t len;

    while ((line = _cut_read_line(fp, opts->zero_terminated, &len)) != NULL) {
        if (opts->mode == CUT_MODE_BYTES || opts->mode == CUT_MODE_CHARS) {
            _cut_process_line_bytes(line, len, opts);
        }
        else {
            _cut_process_line_fields(line, len, opts);
        }
        free(line);
    }

    if (!is_stdin) {
        fclose(fp);
    }

    return 0;
}

/**
 * @brief Print usage/help information
 */
static void _cut_print_help(void)
{
    cut_printf(
        "Usage: cut OPTION... [FILE]...\n"
        "Print selected parts of lines from each FILE to standard output.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -b, --bytes=LIST            select only these bytes\n"
        "  -c, --characters=LIST       select only these characters\n"
        "  -d, --delimiter=DELIM       use DELIM instead of TAB for field delimiter\n"
        "  -f, --fields=LIST           select only these fields; also print any line\n"
        "                                that contains no delimiter character, unless\n"
        "                                the -s option is specified\n"
        "  -F LIST                     like -f, but also implies -w and -O ' '\n"
        "  -n, --no-partial            with -b, don't output partial multi-byte chars\n"
        "  -O, --output-delimiter=STRING  use STRING as the output delimiter;\n"
        "                                the default is to use the input delimiter\n"
        "  -s, --only-delimited        do not print lines not containing delimiters\n"
        "  -w, --whitespace-delimited[=trimmed]  use a run of blank characters as\n"
        "                                the field delimiter; with 'trimmed', ignore\n"
        "                                leading and trailing blanks\n"
        "      --complement            complement the set of selected bytes,\n"
        "                                characters or fields\n"
        "  -z, --zero-terminated       line delimiter is NUL, not newline\n"
        "      --help                  display this help and exit\n"
        "      --version               output version information and exit\n"
        "\n"
        "Use one, and only one of -b, -c, -f or -F. Each LIST is made up of one\n"
        "range, or many ranges separated by commas. Selected input is written in the\n"
        "same order that it is read, and is written exactly once. Each range is one of:\n"
        "\n"
        "  N     N'th byte, character or field, counted from 1\n"
        "  N-    from N'th byte, character or field, to end of line\n"
        "  N-M   from N'th to M'th (included) byte, character or field\n"
        "  -M    from first to M'th (included) byte, character or field\n"
    );
}

/**
 * @brief Print version information
 */
static void _cut_print_version(void)
{
    cut_printf("cut %s\n", CUT_VERSION_STR);
    cut_printf("%s", "Copyright (C) 2025-2026 Yezc/cut\n");
    cut_printf("%s", "License MIT: <https://mit-license.org/>\n");
    cut_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    cut_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
