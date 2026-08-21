/**
 * @file cat.c
 * @brief Cross-platform cat command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 *
 * Key behaviors:
 *   - -A, --show-all: equivalent to -vET
 *   - -b, --number-nonblank: number non-empty output lines
 *   - -e: equivalent to -vE
 *   - -E, --show-ends: display $ at end of each line
 *   - -n, --number: number all output lines
 *   - -s, --squeeze-blank: suppress repeated empty output lines
 *   - -t: equivalent to -vT
 *   - -T, --show-tabs: display TAB characters as ^I
 *   - -v, --show-nonprinting: use ^ and M- notation for non-printing
 *   - --help / --version: recognized and handled
 *   - stdin support (no file args, or - as filename)
 *   - UTF-8 pass-through on all platforms
 *   - Binary-safe I/O (uses full block read/write)
 *   - Windows wide-character path support for non-ANSI filenames
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o cat.exe cat.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o cat cat.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o cat cat.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o cat cat.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o cat cat.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o cat cat.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/cat>
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
    #define CAT_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define CAT_PLATFORM_LINUX   1
    #define CAT_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define CAT_PLATFORM_MACOS   1
    #define CAT_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define CAT_PLATFORM_FREEBSD 1
    #define CAT_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define CAT_PLATFORM_OPENBSD 1
    #define CAT_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define CAT_PLATFORM_NETBSD  1
    #define CAT_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define CAT_PLATFORM_POSIX   1
#else
    #define CAT_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef CAT_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef CAT_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef CAT_PLATFORM_NETBSD
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
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>

#ifdef CAT_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
    #define CAT_SET_BINARY_MODE(fd) _setmode((fd), _O_BINARY)
    #define CAT_IS_TTY(fd) _isatty((fd))
#else /* CAT_PLATFORM_POSIX */
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #define CAT_SET_BINARY_MODE(fd) ((void)0)
    #define CAT_IS_TTY(fd) isatty((fd))
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define CAT_VERSION_STR "v1.0.0"

/** @brief I/O buffer size — 64 KiB for efficient bulk transfer */
#define CAT_IO_BUF_SIZE (64 * 1024)

/** @brief Line number buffer size */
#define CAT_NUM_BUF_SIZE 32

/** @brief Maximum long option name length accepted by the parser */
#define CAT_OPT_NAME_MAX 64

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options for cat
 *   number           - -n: number all lines
 *   number_nonblank  - -b: number non-blank lines (overrides -n)
 *   squeeze_blank    - -s: squeeze consecutive blank lines into one
 *   show_ends        - -E: show $ at end of each line
 *   show_tabs        - -T: show TAB as ^I
 *   show_nonprinting - -v: show non-printing chars in ^ / M- notation
 *   show_all         - -A: equivalent to -vET
 */
typedef struct {
    bool number;
    bool number_nonblank;
    bool squeeze_blank;
    bool show_ends;
    bool show_tabs;
    bool show_nonprinting;
} cat_opts_t;

/**
 * @brief Line-numbering and squeeze state (persists across files)
 *   line_num       - current line number (1-based)
 *   prev_blank     - was the previous output line blank?
 *   at_line_start  - are we at the beginning of a new line?
 *   saw_any_output - have we output anything at all?
 */
typedef struct {
    uint64_t line_num;
    bool prev_blank;
    bool at_line_start;
    bool saw_any_output;
} cat_state_t;

/********************************
 *    static prototypes
 ********************************/
static void _cat_print_help(void);
static void _cat_print_version(void);
static int  _cat_parse_args(int argc, char ** argv, cat_opts_t * opts);
static int  _cat_cat_file(const char * filename, const cat_opts_t * opts,
                          cat_state_t * st);
static int  _cat_process_buf(const unsigned char * buf, size_t len,
                             const cat_opts_t * opts, cat_state_t * st);
static void _cat_put_nonprint(unsigned char c);

#ifdef CAT_PLATFORM_WINDOWS
static int _cat_open_utf8(const char * path, const char * mode, FILE ** out_fp);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for cat_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef cat_out_stream
    #define cat_out_stream stdout
#endif

/**
 * @brief Default stderr stream for cat_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef cat_err_stream
    #define cat_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef cat_printf
    #define cat_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef cat_err_printf
    #define cat_err_printf(fmt, ...) \
        do { if (cat_err_stream) { (void)fprintf((cat_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a single character to stdout.
 *        Casts to unsigned char first so values with the MSB set do
 *        not trigger undefined behavior in putchar's int argument.
 */
#ifndef cat_putchar
    #define cat_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 */
#ifndef cat_fputs
    #define cat_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef cat_fflush
    #define cat_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 *        Two NULLs considered equal (both missing).
 * @return true if strings match, false otherwise
 */
#ifndef cat_streq
    #define cat_streq(a, b) \
        (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/**
 * @brief Safe strncmp wrapper with NULL guards and explicit size.
 */
#ifndef cat_strneq
    #define cat_strneq(a, b, n) \
        (((a) && (b)) ? (strncmp((a), (b), (n)) == 0) : false)
#endif

/**
 * @brief Safe strchr wrapper with NULL guard.
 *        Returns NULL if input string is NULL.
 */
#ifndef cat_strchr
    #define cat_strchr(s, c) (((s)) ? strchr((s), (c)) : NULL)
#endif

/**
 * @brief Safe free-and-null pointer cleanup macro.
 */
#ifndef cat_safe_free
    #define cat_safe_free(p) \
        do { if ((p)) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the cat command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If no file args, read from stdin
 *   3. For each file: open, process, close
 *   4. Return 0 on success, non-zero on error
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    cat_opts_t opts;
    memset(&opts, 0, sizeof(opts));

    int first_file = _cat_parse_args(argc, argv, &opts);
    if (first_file < 0) {
        return 1;
    }

    /* On Windows, set stdout to binary mode so binary files pass through */
#ifdef CAT_PLATFORM_WINDOWS
    CAT_SET_BINARY_MODE(_fileno(stdout));
#endif

    cat_state_t st;
    memset(&st, 0, sizeof(st));
    st.at_line_start = true;

    int exit_code = 0;

    int nfiles = argc - first_file;
    if (nfiles == 0) {
        /* No file arguments: read from stdin */
        if (_cat_cat_file("-", &opts, &st) != 0) {
            exit_code = 1;
        }
    }
    else {
        for (int i = first_file; i < argc; i++) {
            if (_cat_cat_file(argv[i], &opts, &st) != 0) {
                exit_code = 1;
            }
        }
    }

    cat_fflush(cat_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _cat_print_help(void)
{
    cat_printf(
        "Usage: cat [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n"
        "\n"
        "  -A, --show-all           equivalent to -vET\n"
        "  -b, --number-nonblank    number nonempty output lines, overrides -n\n"
        "  -e                        equivalent to -vE\n"
        "  -E, --show-ends          display $ at end of each line\n"
        "  -n, --number             number all output lines\n"
        "  -s, --squeeze-blank      suppress repeated empty output lines\n"
        "  -t                        equivalent to -vT\n"
        "  -T, --show-tabs          display TAB characters as ^I\n"
        "  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "Examples:\n"
        "  cat f - g                Output f's contents, then stdin, then g's contents.\n"
        "  cat                      Copy stdin to stdout.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _cat_print_version(void)
{
    cat_printf("cat %s\n", CAT_VERSION_STR);
    cat_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    cat_printf("%s", "License MIT: <https://mit-license.org/>\n");
    cat_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    cat_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments into cat_opts_t
 *
 * GNU cat accepts both short option clustering (-nse) and long options
 * (--number, --squeeze-blank, etc.).
 * -A = -vET, -e = -vE, -t = -vT.
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options structure
 * @return index of first file argument (or argc if none), -1 on unknown option
 */
static int _cat_parse_args(int argc, char ** argv, cat_opts_t * opts)
{
    if (!opts) {
        return -1;
    }

    if (argc < 1 || !argv) {
        return -1;
    }

    int i = 1;
    bool no_more_opts = false;

    for (; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* "--" terminates options (must check before strncmp catches it) */
        if (!no_more_opts && cat_streq(arg, "--")) {
            no_more_opts = true;
            i++;
            break;
        }

        /* Long options (arg starts with "--" but is not exactly "--") */
        if (!no_more_opts && cat_strneq(arg, "--", 2)) {
            /* Extract name before '=' if present */
            char * eq = cat_strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[CAT_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _cat_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _cat_print_version();
                exit(0);
            }
            if (strcmp(name, "show-all") == 0) {
                opts->show_nonprinting = true;
                opts->show_ends = true;
                opts->show_tabs = true;
            }
            else if (strcmp(name, "number-nonblank") == 0) {
                opts->number_nonblank = true;
                opts->number = false;
            }
            else if (strcmp(name, "show-ends") == 0) {
                opts->show_ends = true;
            }
            else if (strcmp(name, "number") == 0) {
                opts->number = true;
            }
            else if (strcmp(name, "squeeze-blank") == 0) {
                opts->squeeze_blank = true;
            }
            else if (strcmp(name, "show-tabs") == 0) {
                opts->show_tabs = true;
            }
            else if (strcmp(name, "show-nonprinting") == 0) {
                opts->show_nonprinting = true;
            }
            else {
                cat_err_printf("cat: unrecognized option '%s'\n", arg);
                cat_err_printf("%s", "Try 'cat --help' for more information.\n");
                return -1;
            }
            continue;
        }

        /* Short options: must start with '-' and not be just "-" */
        if (!no_more_opts && arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'A':
                        opts->show_nonprinting = true;
                        opts->show_ends = true;
                        opts->show_tabs = true;
                        break;

                    case 'b':
                        opts->number_nonblank = true;
                        opts->number = false;
                        break;

                    case 'e':
                        opts->show_nonprinting = true;
                        opts->show_ends = true;
                        break;

                    case 'E':
                        opts->show_ends = true;
                        break;

                    case 'n':
                        /* -b overrides -n, so only set if -b not active */
                        if (!opts->number_nonblank) {
                            opts->number = true;
                        }
                        break;

                    case 's':
                        opts->squeeze_blank = true;
                        break;

                    case 't':
                        opts->show_nonprinting = true;
                        opts->show_tabs = true;
                        break;

                    case 'T':
                        opts->show_tabs = true;
                        break;

                    case 'v':
                        opts->show_nonprinting = true;
                        break;

                    default:
                        cat_err_printf("cat: invalid option -- '%c'\n", arg[j]);
                        cat_err_printf("%s", "Try 'cat --help' for more information.\n");
                        return -1;
                }
            }
            continue;
        }

        /* Positional argument: this is a file (including "-" for stdin) */
        break;
    }

    return i;
}

#ifdef CAT_PLATFORM_WINDOWS
/**
 * @brief Open a file using UTF-8 path (converted to wide on Windows).
 * @param path   UTF-8 file path (or "-" for stdin)
 * @param mode   fopen mode string
 * @param out_fp output FILE pointer (must fclose by caller)
 * @return 0 on success, -1 on failure
 */
static int _cat_open_utf8(const char * path, const char * mode, FILE ** out_fp)
{
    if (!path || !mode || !out_fp) {
        return -1;
    }

    *out_fp = NULL;

    /* "-" means stdin */
    if (cat_streq(path, "-")) {
        *out_fp = stdin;
        return 0;
    }

    /* Convert UTF-8 to wide for Windows Unicode paths */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0) {
        /* Fall back to ANSI fopen */
        *out_fp = fopen(path, mode);
        return *out_fp ? 0 : -1;
    }

    wchar_t * wpath = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wpath) {
        *out_fp = fopen(path, mode);
        return *out_fp ? 0 : -1;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen) <= 0) {
        cat_safe_free(wpath);
        *out_fp = fopen(path, mode);
        return *out_fp ? 0 : -1;
    }

    /* Convert mode to wide */
    int mlen = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
    wchar_t * wmode = NULL;
    if (mlen > 0) {
        wmode = (wchar_t *)malloc((size_t)mlen * sizeof(wchar_t));
        if (wmode) {
            MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, mlen);
        }
    }

    *out_fp = _wfopen(wpath, wmode ? wmode : L"rb");
    cat_safe_free(wpath);
    if (wmode) {
        cat_safe_free(wmode);
    }

    if (!*out_fp) {
        /* Final fallback to ANSI fopen */
        *out_fp = fopen(path, mode);
    }
    return *out_fp ? 0 : -1;
}
#endif /* CAT_PLATFORM_WINDOWS */

/**
 * @brief Output a single non-printing character in ^ or M- notation.
 *
 * GNU cat -v rules:
 *   - Tab (\t) and Newline (\n) are NOT shown by -v alone
 *     (they are handled by -T and -E respectively)
 *   - Del (127) -> ^?
 *   - Other control chars (0-31) -> ^@, ^A, ..., ^Z, ^[, ^\, ^], ^^, ^_
 *   - High bytes (128-255) -> M- followed by the 128-stripped representation
 *     e.g. \x80 -> M-^@, \xA0 -> M-  (space), \xFF -> M-^?
 *
 * @param c  the byte to display
 */
static void _cat_put_nonprint(unsigned char c)
{
    if (c >= 128) {
        /* High bit set: M- prefix, then show the 128-stripped byte */
        cat_putchar('M');
        cat_putchar('-');
        c = (unsigned char)(c & 0x7F);
        /* Now c is 0-127; fall through to control-char handling */
    }

    if (c == 127) {
        /* Del */
        cat_putchar('^');
        cat_putchar('?');
    }
    else if (c < 32) {
        /* Control char: ^@ (0) through ^_ (31) */
        /* When reached via M- notation, ALL control chars are shown
         * (including \n=^J and \t=^I). When reached directly via -v,
         * \n and \t are NOT passed to this function (handled by caller). */
        cat_putchar('^');
        cat_putchar((int)(c + '@'));
    }
    else {
        /* Printable ASCII (32-126) */
        cat_putchar((int)c);
    }
}

/**
 * @brief Process a buffer of bytes and output with cat options applied.
 *
 * This function handles line numbering (-n, -b), squeezing (-s),
 * showing ends (-E), tabs (-T), and non-printing (-v).
 *
 * It is called for each chunk of data read from a file or stdin.
 * State (line number, blank tracking) persists across calls via `st`.
 *
 * @param buf    input buffer (unsigned bytes)
 * @param len    number of bytes in buffer
 * @param opts   cat options
 * @param st     persistent state (line number, blank tracking, etc.)
 * @return 0 on success, -1 on write error
 */
static int _cat_process_buf(const unsigned char * buf, size_t len,
                            const cat_opts_t * opts, cat_state_t * st)
{
    bool do_number = opts->number || opts->number_nonblank;
    char numbuf[CAT_NUM_BUF_SIZE];

    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];

        /* If at line start, handle line numbering and squeeze logic */
        if (st->at_line_start) {
            bool is_blank = (c == '\n');

            if (opts->squeeze_blank && is_blank) {
                if (st->prev_blank) {
                    /* Skip this blank line entirely */
                    st->at_line_start = true;
                    continue;
                }
                st->prev_blank = true;
            }
            else {
                st->prev_blank = false;
            }

            /* Line numbering */
            if (do_number) {
                if (opts->number_nonblank) {
                    if (!is_blank) {
                        st->line_num++;
                        snprintf(numbuf, sizeof(numbuf), "%6" PRIu64 "  ", st->line_num);
                        cat_fputs(numbuf, cat_out_stream);
                    }
                }
                else {
                    /* -n: number all lines including blanks */
                    st->line_num++;
                    snprintf(numbuf, sizeof(numbuf), "%6" PRIu64 "  ", st->line_num);
                    cat_fputs(numbuf, cat_out_stream);
                }
            }

            st->at_line_start = false;
            st->saw_any_output = true;
        }

        /* Output the character (or its representation) */
        if (c == '\n') {
            if (opts->show_ends) {
                cat_putchar('$');
            }
            cat_putchar('\n');
            st->at_line_start = true;
        }
        else if (c == '\t' && opts->show_tabs) {
            cat_putchar('^');
            cat_putchar('I');
        }
        else if (opts->show_nonprinting) {
            /* -v: show non-printing (but not \n, and not \t unless -T) */
            if (c == '\t') {
                /* \t is shown literally unless -T is also active */
                if (opts->show_tabs) {
                    /* Already handled above; this branch is unreachable
                     * because the `c == '\t' && opts->show_tabs` check
                     * above catches it first. */
                    cat_putchar('^');
                    cat_putchar('I');
                }
                else {
                    cat_putchar('\t');
                }
            }
            else if (c < 32 || c == 127 || c >= 128) {
                _cat_put_nonprint(c);
            }
            else {
                cat_putchar((int)c);
            }
        }
        else {
            cat_putchar((int)c);
        }
    }

    return 0;
}

/**
 * @brief Read a file and output its contents with options applied.
 *
 * Opens the file (or stdin if filename is "-"), reads in large blocks,
 * and calls _cat_process_buf for each block.
 *
 * @param filename  file to open (or "-" for stdin)
 * @param opts      cat options
 * @param st        persistent state across files
 * @return 0 on success, -1 on file error
 */
static int _cat_cat_file(const char * filename, const cat_opts_t * opts,
                         cat_state_t * st)
{
    FILE * fp = NULL;

    if (!filename || cat_streq(filename, "-")) {
        fp = stdin;
#ifdef CAT_PLATFORM_WINDOWS
        CAT_SET_BINARY_MODE(_fileno(stdin));
#endif
    }
    else {
#ifdef CAT_PLATFORM_WINDOWS
        if (_cat_open_utf8(filename, "rb", &fp) != 0 || !fp) {
            cat_err_printf("cat: %s: %s\n", filename, strerror(errno));
            return -1;
        }
        CAT_SET_BINARY_MODE(_fileno(fp));
#else
        fp = fopen(filename, "rb");
        if (!fp) {
            cat_err_printf("cat: %s: %s\n", filename, strerror(errno));
            return -1;
        }
#endif
    }

    unsigned char * buf = (unsigned char *)malloc(CAT_IO_BUF_SIZE);
    if (!buf) {
        if (fp != stdin) {
            fclose(fp);
        }
        cat_err_printf("cat: %s: %s\n", filename ? filename : "stdin", strerror(ENOMEM));
        return -1;
    }

    int rc = 0;
    size_t nread;
    while ((nread = fread(buf, 1, CAT_IO_BUF_SIZE, fp)) > 0) {
        if (_cat_process_buf(buf, nread, opts, st) != 0) {
            rc = -1;
            break;
        }
    }

    if (ferror(fp)) {
        cat_err_printf("cat: %s: %s\n", filename ? filename : "stdin", strerror(errno));
        rc = -1;
    }

    cat_safe_free(buf);
    if (fp != stdin) {
        fclose(fp);
    }
    return rc;
}
