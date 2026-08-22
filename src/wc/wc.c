/**
 * @file wc.c
 * @brief Cross-platform wc command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils wc(1).
 *
 * Key behaviors:
 *   - -c, --bytes    print byte counts
 *   - -m, --chars    print character counts
 *   - -l, --lines    print newline counts
 *   - -w, --words    print word counts
 *   - -L, --max-line-length   print length of longest line
 *   -    --files0-from=F     read input from files separated by NUL
 *   -    --help              display help and exit
 *   -    --version           output version and exit
 *   - Without options:  -l -w -c
 *   - Multiple files print a totals line
 *   - Input from files or stdin
 *
 * Platform notes:
 *   - Character count (-m): uses mbrtowc() for proper multibyte support
 *   - On Windows, uses _setmode for binary stdin
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o wc.exe wc.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o wc wc.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o wc wc.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o wc wc.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o wc wc.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o wc wc.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/wc>
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
 * Platform detection macros -- must appear before any system includes
 * so that POSIX feature macros are defined correctly.
 */
#if defined(_WIN32) || defined(_WIN64)
    #define WC_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define WC_PLATFORM_LINUX   1
    #define WC_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define WC_PLATFORM_MACOS   1
    #define WC_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define WC_PLATFORM_FREEBSD 1
    #define WC_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define WC_PLATFORM_OPENBSD 1
    #define WC_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define WC_PLATFORM_NETBSD  1
    #define WC_PLATFORM_POSIX   1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>

#ifdef WC_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <sys/stat.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

/* ============================================================
 * - prototypes
 * ============================================================ */

typedef struct {
    unsigned long long lines;
    unsigned long long words;
    unsigned long long chars;
    unsigned long long bytes;
    unsigned long long max_line_len;
} wc_counts_t;

typedef struct {
    int show_lines;
    int show_words;
    int show_chars;
    int show_bytes;
    int show_max_line;
    int any_option_set;
    const char * files0_from;
} wc_opts_t;

static void _wc_usage(void);
static void _wc_version(void);
static int  _wc_parse_opts(int argc, char ** argv, wc_opts_t * opts, int * file_start);
static int  _wc_count_file(const char * filename, const wc_opts_t * opts, wc_counts_t * counts);
static int  _wc_count_stream(FILE * fp, const wc_opts_t * opts, wc_counts_t * counts);
static int  _wc_count_stdin(const wc_opts_t * opts, wc_counts_t * counts);
static void _wc_print_counts(const wc_counts_t * counts, const wc_opts_t * opts, const char * name);
static void _wc_print_line(const wc_counts_t * counts, const wc_opts_t * opts, unsigned max_width, const char * name);
static int  _wc_process_files(char ** files, int nfiles, const wc_opts_t * opts);
static int  _wc_process_files0(const char * from_file, const wc_opts_t * opts);
static unsigned long long _wc_max_ull(unsigned long long a, unsigned long long b);
static unsigned _wc_count_width(unsigned long long val);
static void _wc_print_field(unsigned long long val, unsigned width);

/* ============================================================
 * - macros
 * ============================================================ */

#define wc_version_string "1.0.0"
#define wc_buf_size       65536
#define wc_max_line_buf   1048576

/* Wrapper macros for library calls */
#define wc_putchar(c)       putchar(c)
#define wc_safe_free(p)    do { if (p) { free(p); (p) = NULL; } } while (0)

/* ============================================================
 * - static variables
 * ============================================================ */

/* none */

/* ============================================================
 * - global functions
 * ============================================================ */

/**
 * @brief Entry point for wc command.
 */
int main(int argc, char * argv[])
{
    setlocale(LC_ALL, "");

#ifdef WC_PLATFORM_WINDOWS
    /* Ensure stdin is in binary mode for byte counting */
    _setmode(_fileno(stdin), O_BINARY);
#endif

    wc_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    int file_start = 0;

    if (!_wc_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    /* If no specific option set, default to -l -w -c */
    if (!opts.any_option_set) {
        opts.show_lines = 1;
        opts.show_words = 1;
        opts.show_bytes = 1;
    }

    /* --files0-from mode */
    if (opts.files0_from) {
        return _wc_process_files0(opts.files0_from, &opts);
    }

    /* Determine file list */
    int nfiles = argc - file_start;
    char ** files = NULL;

    if (nfiles > 0) {
        files = &argv[file_start];
    }

    /* If no files, read from stdin */
    if (nfiles == 0) {
        wc_counts_t counts;
        memset(&counts, 0, sizeof(counts));
        if (!_wc_count_stdin(&opts, &counts)) {
            return 1;
        }
        _wc_print_counts(&counts, &opts, "");
        return 0;
    }

    return _wc_process_files(files, nfiles, &opts);
}

/* ============================================================
 * - static functions
 * ============================================================ */

/**
 * @brief Print usage information and exit.
 */
static void _wc_usage(void)
{
    printf(
        "Usage: wc [OPTION]... [FILE]...\n"
        "  or:  wc [OPTION]... --files0-from=F\n"
        "Print newline, word, and byte counts for each FILE, and a total line if\n"
        "more than one FILE is specified. A word is a nonempty sequence of\n"
        "non-whitespace characters.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "The options below may be used to select which counts are printed, always in\n"
        "the following order: newline, word, character, byte, maximum line length.\n"
        "\n"
        "  -c, --bytes            print byte counts\n"
        "  -m, --chars            print character counts\n"
        "  -l, --lines            print newline counts\n"
        "    --files0-from=F      read input from the files specified by\n"
        "                          NUL-terminated names in file F;\n"
        "                          If F is - then read names from standard input\n"
        "  -L, --max-line-length  print maximum display width\n"
        "  -w, --words            print word counts\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n"
    );
    exit(0);
}

/**
 * @brief Print version information and exit.
 */
static void _wc_version(void)
{
    printf("wc (Yezc cclinuxtools) %s\n", wc_version_string);
    printf("MIT License\n");
    exit(0);
}

/**
 * @brief Parse command-line options.
 * @return 1 on success, 0 on error.
 */
static int _wc_parse_opts(int argc, char * argv[], wc_opts_t * opts, int * file_start)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            /* Not an option (or "-" for stdin) */
            break;
        }

        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            /* "--" separator */
            i++;
            break;
        }

        if (argv[i][1] == '-') {
            /* Long option */
            const char * opt = argv[i] + 2;

            if (strcmp(opt, "help") == 0) {
                _wc_usage();
            }
            else if (strcmp(opt, "version") == 0) {
                _wc_version();
            }
            else if (strcmp(opt, "bytes") == 0) {
                opts->show_bytes = 1;
                opts->any_option_set = 1;
            }
            else if (strcmp(opt, "chars") == 0) {
                opts->show_chars = 1;
                opts->any_option_set = 1;
            }
            else if (strcmp(opt, "lines") == 0) {
                opts->show_lines = 1;
                opts->any_option_set = 1;
            }
            else if (strcmp(opt, "max-line-length") == 0) {
                opts->show_max_line = 1;
                opts->any_option_set = 1;
            }
            else if (strcmp(opt, "words") == 0) {
                opts->show_words = 1;
                opts->any_option_set = 1;
            }
            else if (strncmp(opt, "files0-from=", 12) == 0) {
                opts->files0_from = opt + 12;
                if (opts->files0_from[0] == '\0') {
                    fprintf(stderr, "wc: option '--files0-from' requires an argument\n");
                    return 0;
                }
            }
            else if (strcmp(opt, "files0-from") == 0) {
                /* --files0-from F (separate argument) */
                if (i + 1 >= argc) {
                    fprintf(stderr, "wc: option '--files0-from' requires an argument\n");
                    return 0;
                }
                i++;
                opts->files0_from = argv[i];
            }
            else {
                fprintf(stderr, "wc: unrecognized option '--%s'\n", opt);
                fprintf(stderr, "Try 'wc --help' for more information.\n");
                return 0;
            }
        }
        else {
            /* Short options - may be combined */
            int j;
            for (j = 1; argv[i][j] != '\0'; j++) {
                switch (argv[i][j]) {
                    case 'c':
                        opts->show_bytes = 1;
                        opts->any_option_set = 1;
                        break;
                    case 'm':
                        opts->show_chars = 1;
                        opts->any_option_set = 1;
                        break;
                    case 'l':
                        opts->show_lines = 1;
                        opts->any_option_set = 1;
                        break;
                    case 'w':
                        opts->show_words = 1;
                        opts->any_option_set = 1;
                        break;
                    case 'L':
                        opts->show_max_line = 1;
                        opts->any_option_set = 1;
                        break;
                    default:
                        fprintf(stderr, "wc: invalid option -- '%c'\n", argv[i][j]);
                        fprintf(stderr, "Try 'wc --help' for more information.\n");
                        return 0;
                }
            }
        }
    }

    *file_start = i;
    return 1;
}

/**
 * @brief Count statistics from a file stream.
 */
static int _wc_count_stream(FILE * fp, const wc_opts_t * opts, wc_counts_t * counts)
{
    unsigned char buf[wc_buf_size];
    size_t n;
    int in_word = 0;
    unsigned long long cur_line_len = 0;
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));

    int need_chars = opts->show_chars;
    int need_lines = opts->show_lines;
    int need_words = opts->show_words;
    int need_bytes = opts->show_bytes;
    int need_max   = opts->show_max_line;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        size_t idx = 0;

        if (need_bytes) {
            counts->bytes += n;
        }

        while (idx < n) {
            unsigned char c = buf[idx];

            if (need_lines) {
                if (c == '\n') {
                    counts->lines++;
                }
            }

            if (need_words) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
                    if (in_word) {
                        in_word = 0;
                        counts->words++;
                    }
                }
                else {
                    in_word = 1;
                }
            }

            if (need_max) {
                if (c == '\n') {
                    counts->max_line_len = _wc_max_ull(counts->max_line_len, cur_line_len);
                    cur_line_len = 0;
                }
                else {
                    cur_line_len++;
                }
            }

            if (need_chars) {
                /* Use mbrtowc for multibyte character counting */
                wchar_t wc;
                size_t len = mbrtowc(&wc, (const char *)(buf + idx), n - idx, &mbs);

                if (len == (size_t)-1 || len == (size_t)-2) {
                    /* Invalid sequence; count each byte as a char */
                    counts->chars++;
                    idx++;
                    memset(&mbs, 0, sizeof(mbs));
                    continue;
                }
                else if (len == 0) {
                    /* NUL byte */
                    counts->chars++;
                    idx++;
                    continue;
                }
                else {
                    counts->chars++;
                    idx += len;
                    continue;
                }
            }

            idx++;
        }
    }

    if (need_max) {
        counts->max_line_len = _wc_max_ull(counts->max_line_len, cur_line_len);
    }

    /* Handle last word if file doesn't end with whitespace */
    if (need_words && in_word) {
        counts->words++;
    }

    return 1;
}

/**
 * @brief Count statistics from a file by name.
 */
static int _wc_count_file(const char * filename, const wc_opts_t * opts, wc_counts_t * counts)
{
    if (strcmp(filename, "-") == 0) {
        return _wc_count_stdin(opts, counts);
    }

    FILE * fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "wc: %s: %s\n", filename, strerror(errno));
        return 0;
    }

    int ret = _wc_count_stream(fp, opts, counts);
    fclose(fp);
    return ret;
}

/**
 * @brief Count statistics from stdin.
 */
static int _wc_count_stdin(const wc_opts_t * opts, wc_counts_t * counts)
{
    return _wc_count_stream(stdin, opts, counts);
}

/**
 * @brief Print counts in GNU-compatible format.
 */
static void _wc_print_counts(const wc_counts_t * counts, const wc_opts_t * opts, const char * name)
{
    unsigned max_width = 1;

    if (opts->show_lines) {
        max_width = _wc_max_ull(max_width, _wc_count_width(counts->lines));
    }
    if (opts->show_words) {
        max_width = _wc_max_ull(max_width, _wc_count_width(counts->words));
    }
    if (opts->show_chars) {
        max_width = _wc_max_ull(max_width, _wc_count_width(counts->chars));
    }
    if (opts->show_bytes) {
        max_width = _wc_max_ull(max_width, _wc_count_width(counts->bytes));
    }
    if (opts->show_max_line) {
        max_width = _wc_max_ull(max_width, _wc_count_width(counts->max_line_len));
    }

    int first = 1;

    if (opts->show_lines) {
        _wc_print_field(counts->lines, max_width);
        first = 0;
    }
    if (opts->show_words) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->words, max_width);
        first = 0;
    }
    if (opts->show_chars) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->chars, max_width);
        first = 0;
    }
    if (opts->show_bytes) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->bytes, max_width);
        first = 0;
    }
    if (opts->show_max_line) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->max_line_len, max_width);
        first = 0;
    }

    if (name && name[0] != '\0') {
        printf(" %s", name);
    }

    wc_putchar('\n');
}

/**
 * @brief Print a single line of counts (used in multi-file mode).
 */
static void _wc_print_line(const wc_counts_t * counts, const wc_opts_t * opts,
                           unsigned max_width, const char * name)
{
    int first = 1;

    if (opts->show_lines) {
        _wc_print_field(counts->lines, max_width);
        first = 0;
    }
    if (opts->show_words) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->words, max_width);
        first = 0;
    }
    if (opts->show_chars) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->chars, max_width);
        first = 0;
    }
    if (opts->show_bytes) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->bytes, max_width);
        first = 0;
    }
    if (opts->show_max_line) {
        if (!first) {
            wc_putchar(' ');
        }
        _wc_print_field(counts->max_line_len, max_width);
        first = 0;
    }

    if (name && name[0] != '\0') {
        printf(" %s", name);
    }
    wc_putchar('\n');
}

/**
 * @brief Process multiple files and print totals.
 */
static int _wc_process_files(char ** files, int nfiles, const wc_opts_t * opts)
{
    wc_counts_t totals;
    memset(&totals, 0, sizeof(totals));
    int all_ok = 1;

    if (nfiles <= 0) {
        return 1;
    }

    wc_counts_t * all_counts = (wc_counts_t *)calloc((size_t)nfiles, sizeof(wc_counts_t));
    if (!all_counts) {
        fprintf(stderr, "wc: out of memory\n");
        return 1;
    }

    unsigned max_width = 1;
    int i;

    /* Single pass: read each file once, store counts, compute max_width */
    for (i = 0; i < nfiles; i++) {
        if (!_wc_count_file(files[i], opts, &all_counts[i])) {
            all_ok = 0;
            continue;
        }

        if (opts->show_lines) {
            max_width = _wc_max_ull(max_width, _wc_count_width(all_counts[i].lines));
        }
        if (opts->show_words) {
            max_width = _wc_max_ull(max_width, _wc_count_width(all_counts[i].words));
        }
        if (opts->show_chars) {
            max_width = _wc_max_ull(max_width, _wc_count_width(all_counts[i].chars));
        }
        if (opts->show_bytes) {
            max_width = _wc_max_ull(max_width, _wc_count_width(all_counts[i].bytes));
        }
        if (opts->show_max_line) {
            max_width = _wc_max_ull(max_width, _wc_count_width(all_counts[i].max_line_len));
        }
    }

    /* Print pass: use stored counts */
    for (i = 0; i < nfiles; i++) {
        _wc_print_line(&all_counts[i], opts, max_width, files[i]);

        totals.lines       += all_counts[i].lines;
        totals.words       += all_counts[i].words;
        totals.chars       += all_counts[i].chars;
        totals.bytes       += all_counts[i].bytes;
        totals.max_line_len = _wc_max_ull(totals.max_line_len, all_counts[i].max_line_len);
    }

    /* Print totals if more than one file */
    if (nfiles > 1) {
        _wc_print_line(&totals, opts, max_width, "total");
    }

    wc_safe_free(all_counts);
    return all_ok ? 0 : 1;
}

/**
 * @brief Process files from a NUL-separated list.
 */
static int _wc_process_files0(const char * from_file, const wc_opts_t * opts)
{
    FILE * fp;

    if (strcmp(from_file, "-") == 0) {
        fp = stdin;
    }
    else {
        fp = fopen(from_file, "rb");
        if (!fp) {
            fprintf(stderr, "wc: %s: %s\n", from_file, strerror(errno));
            return 1;
        }
    }

    wc_counts_t totals;
    memset(&totals, 0, sizeof(totals));
    int nfiles = 0;
    int all_ok = 1;
    char * name_buf = NULL;
    size_t name_cap = 0;
    size_t name_len = 0;

    /* Read NUL-separated names */
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (name_len + 1 > name_cap) {
            name_cap = name_cap ? name_cap * 2 : 256;
            char * tmp = (char *)realloc(name_buf, name_cap);
            if (!tmp) {
                wc_safe_free(name_buf);
                if (fp != stdin) {
                    fclose(fp);
                }
                fprintf(stderr, "wc: out of memory\n");
                return 1;
            }
            name_buf = tmp;
        }

        if (c == '\0') {
            /* End of a name */
            if (name_len == 0) {
                /* Empty name, skip */
                continue;
            }

            name_buf[name_len] = '\0';

            wc_counts_t counts;
            memset(&counts, 0, sizeof(counts));

            if (!_wc_count_file(name_buf, opts, &counts)) {
                all_ok = 0;
            }
            else {
                _wc_print_counts(&counts, opts, name_buf);
                totals.lines       += counts.lines;
                totals.words       += counts.words;
                totals.chars       += counts.chars;
                totals.bytes       += counts.bytes;
                totals.max_line_len = _wc_max_ull(totals.max_line_len, counts.max_line_len);
                nfiles++;
            }

            name_len = 0;
        }
        else {
            name_buf[name_len++] = (char)c;
        }
    }

    if (fp != stdin) {
        fclose(fp);
    }

    wc_safe_free(name_buf);

    /* Print totals if more than one file */
    if (nfiles > 1) {
        _wc_print_counts(&totals, opts, "total");
    }

    return all_ok ? 0 : 1;
}

/**
 * @brief Return the maximum of two unsigned long long values.
 */
static unsigned long long _wc_max_ull(unsigned long long a, unsigned long long b)
{
    return a > b ? a : b;
}

/**
 * @brief Count the number of digits in an unsigned long long value.
 */
static unsigned _wc_count_width(unsigned long long val)
{
    unsigned width = 1;
    while (val >= 10) {
        val /= 10;
        width++;
    }
    return width;
}

/**
 * @brief Print an unsigned long long value right-aligned to width.
 *        Avoids %*llu which is not supported by Windows msvcrt.
 */
static void _wc_print_field(unsigned long long val, unsigned width)
{
    char numbuf[32];
    snprintf(numbuf, sizeof(numbuf), "%llu", val);
    int numlen = (int)strlen(numbuf);
    int i;
    for (i = numlen; i < (int)width; i++) {
        wc_putchar(' ');
    }
    fputs(numbuf, stdout);
}
