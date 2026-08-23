/**
 * @file uniq.c
 * @brief Cross-platform implementation of the Linux uniq command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils uniq(1).
 *
 * Key behaviors:
 *   - -c, --count                 prefix lines with occurrence count
 *   - -d, --repeated              print only first copy of repeated lines
 *   - -D, --all-repeated[=METHOD] print all copies of repeated lines
 *   -       --group[=METHOD]      print all lines, delimiting each group
 *   - -f N, --skip-fields=N       skip N leading fields before compare
 *   -       -N                   (traditional) same as -f N
 *   - -s N, --skip-chars=N        skip N characters before compare
 *   - -i, --ignore-case           ignore case in comparisons
 *   - -u, --unique                print only unique (non-repeated) lines
 *   - -w N, --check-chars=N       compare at most N chars per line
 *   - -z, --zero-terminated       NUL-delimited items (not newlines)
 *   -       --help                display help and exit
 *   -       --version             output version and exit
 *   - [INPUT [OUTPUT]]            optional input / output file names
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o uniq.exe uniq.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o uniq uniq.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o uniq uniq.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o uniq uniq.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o uniq uniq.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o uniq uniq.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/uniq>
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
    #define UNIQ_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define UNIQ_PLATFORM_LINUX   1
    #define UNIQ_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define UNIQ_PLATFORM_MACOS   1
    #define UNIQ_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define UNIQ_PLATFORM_FREEBSD 1
    #define UNIQ_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define UNIQ_PLATFORM_OPENBSD 1
    #define UNIQ_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define UNIQ_PLATFORM_NETBSD  1
    #define UNIQ_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define UNIQ_PLATFORM_POSIX   1
#else
    #define UNIQ_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef UNIQ_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef UNIQ_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef UNIQ_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#ifdef UNIQ_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define UNIQ_VERSION_STR "v1.0.0"

/** @brief Initial allocation size (bytes) of the dynamic per-item buffer */
#define UNIQ_LINE_BUF_INIT  512U

/** @brief Maximum width used by the count prefix when formatting with -c */
#define UNIQ_COUNT_BUF_LEN  64U

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Delimiter style for --all-repeated and --group.
 */
typedef enum {
    UNIQ_DELIM_NONE = 0,     /**< No delimiter around groups */
    UNIQ_DELIM_PREPEND,      /**< Output a delimiter before each group */
    UNIQ_DELIM_SEPARATE,     /**< Separate groups with a single delimiter (not before first) */
    UNIQ_DELIM_APPEND,       /**< Output a delimiter after each group */
    UNIQ_DELIM_BOTH          /**< Delimiter before AND after each group */
} uniq_delim_t;

/**
 * @brief Primary output mode (mutually exclusive: default, -c, -d, -D, -u, --group).
 */
typedef enum {
    UNIQ_MODE_DEFAULT = 0,   /**< One output line per unique group (first copy) */
    UNIQ_MODE_COUNT,         /**< -c : count + first copy */
    UNIQ_MODE_REPEATED,      /**< -d : first copy, only for groups with count>=2 */
    UNIQ_MODE_ALL_REPEATED,  /**< -D : every line, only for groups with count>=2 */
    UNIQ_MODE_UNIQUE,        /**< -u : only lines from groups with count==1 */
    UNIQ_MODE_GROUP          /**< --group : every line from every group, with delimiters */
} uniq_mode_t;

/**
 * @brief Dynamic buffer that holds a single input record (line or NUL-item).
 */
typedef struct {
    char * text;             /**< Payload bytes (no trailing terminator kept) */
    size_t len;              /**< Number of valid bytes in @c text */
    size_t cap;              /**< Allocated capacity (bytes) */
} uniq_linebuf_t;

/**
 * @brief All runtime options / state for one uniq invocation.
 */
typedef struct {
    long skip_fields;        /**< -f N (traditional -N).  Values < 0 clamped to 0 */
    long skip_chars;         /**< -s N.  Values < 0 clamped to 0 */
    long check_chars;        /**< -w N.  Values <= 0 mean "unlimited" */
    bool ignore_case;        /**< -i */
    bool zero_terminated;    /**< -z */
    uniq_mode_t mode;        /**< mutually exclusive output mode */
    uniq_delim_t delim;      /**< delimiter method used by -D / --group */
    const char * in_path;    /**< NULL => stdin */
    const char * out_path;   /**< NULL => stdout */
} uniq_opts_t;

/********************************
 *    static prototypes
 ********************************/
/* Option parsing + diagnostics */
static void         _uniq_print_help(void);
static void         _uniq_print_version(void);
static bool         _uniq_streq(const char * a, const char * b);
static bool         _uniq_str2long(const char * s, long * out);
static bool         _uniq_parse_long(const char * arg, char ** argv, int i, int argc,
                                     uniq_opts_t * opts, int * consumed);
static bool         _uniq_parse_short(const char * arg, char ** argv, int i, int argc,
                                      uniq_opts_t * opts, int * consumed);
static bool         _uniq_parse_opts(int argc, char ** argv, uniq_opts_t * opts);

/* Line buffer helpers */
static void         _uniq_linebuf_init(uniq_linebuf_t * lb);
static void         _uniq_linebuf_reset(uniq_linebuf_t * lb);
static bool         _uniq_linebuf_reserve(uniq_linebuf_t * lb, size_t extra);
static bool         _uniq_linebuf_append(uniq_linebuf_t * lb, const char * data, size_t n);
static void         _uniq_linebuf_free(uniq_linebuf_t * lb);

/* Input reader */
static int          _uniq_read_record(FILE * fp, uniq_linebuf_t * lb, int term);

/* Compare logic (fields / chars / case / length) */
static size_t       _uniq_skip_n_fields(const char * text, size_t len, long n);
static size_t       _uniq_key_start(const char * text, size_t len, const uniq_opts_t * opts);
static size_t       _uniq_key_len(const char * text, size_t len, const uniq_opts_t * opts);
static bool         _uniq_keys_equal(const char * a, size_t alen,
                                     const char * b, size_t blen,
                                     const uniq_opts_t * opts);

/* Output helpers */
static void         _uniq_emit_count_prefix(unsigned long count, FILE * out);
static void         _uniq_emit_record(const char * text, size_t len, int term, FILE * out);
static void         _uniq_emit_delimiter(int term, FILE * out);

/* Main processing pipeline */
static void         _uniq_flush_group(const uniq_linebuf_t * rep,
                                      unsigned long count,
                                      bool * any_output_yet,
                                      const uniq_opts_t * opts,
                                      FILE * out,
                                      int term,
                                      const uniq_linebuf_t * acc);
static int          _uniq_run(const uniq_opts_t * opts);

#ifdef UNIQ_PLATFORM_WINDOWS
static HANDLE       _uniq_std_handle_for_fd(int fd);
static bool         _uniq_is_console_stream(FILE * fp);
static bool         _uniq_is_disk_stream(FILE * fp);
static bool         _uniq_is_pipe_stream(FILE * fp);
static UINT         _uniq_output_codepage(void);
static size_t       _uniq_write_pipe_cp(const void * buf, size_t len, FILE * fp);
static size_t       _uniq_write_win32(const void * buf, size_t len, FILE * fp);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for uniq_fputs / uniq_fflush wrappers.
 *        Defaults to libc @c stdout .
 */
#ifndef uniq_out_stream
    #define uniq_out_stream stdout
#endif

#ifdef UNIQ_PLATFORM_WINDOWS
/**
 * @brief Portable byte-write macro used by every text-output path.
 *        On Windows stdout/stderr are routed through @c _uniq_write_win32
 *        which converts UTF-8 bytes to UTF-16LE via WriteConsoleW for
 *        real console streams, transcodes to the console output
 *        codepage for PowerShell 5.x pipes, and writes raw UTF-8 for
 *        disk redirections.  Any non-stdio stream (explicit -o FILE,
 *        temporary buffers, ...) uses plain fwrite so test output is
 *        byte-identical to GNU uniq.
 * @sa _uniq_write_win32
 */
    #ifndef uniq_fwrite
        #define uniq_fwrite(buf, sz, cnt, fp) \
            _uniq_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef uniq_fwrite
        #define uniq_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print wrapper (printf-compatible).
 *
 * On Windows the formatted buffer is emitted through @c _uniq_write_win32
 * so CJK glyphs render correctly even on legacy CP936 hosts.
 */
#ifndef uniq_printf
    #ifdef UNIQ_PLATFORM_WINDOWS
        #define uniq_printf(fmt, ...) \
            do { \
                char _uniqpf[2048]; \
                int _uniqpf_n = snprintf(_uniqpf, sizeof(_uniqpf), (fmt), ##__VA_ARGS__); \
                if (_uniqpf_n > 0) { (void)_uniq_write_win32(_uniqpf, (size_t)_uniqpf_n, uniq_out_stream); } \
            } while (0)
    #else
        #define uniq_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Write a single byte (cast via unsigned char to avoid UB with
 *        signed @c char on some compilers) to the default output stream.
 */
#ifndef uniq_putchar
    #define uniq_putchar(ch) \
        do { unsigned char _uniqpc = (unsigned char)(ch); (void)uniq_fwrite(&_uniqpc, 1, 1, uniq_out_stream); } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef uniq_fputs
    #define uniq_fputs(str, stream) \
        do { const char * _uniqsp = (str); if (_uniqsp) { (void)uniq_fwrite(_uniqsp, 1, strlen(_uniqsp), (stream)); } } while (0)
#endif

/**
 * @brief Flush the given stdio stream.
 */
#ifndef uniq_fflush
    #define uniq_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free: free(*p) and set the pointer to NULL.
 *        Callable when @p p itself is NULL (no-op).
 */
#ifndef uniq_safe_free
    #define uniq_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/**
 * @brief Clamp @p v into the inclusive range [lo, hi].
 */
#ifndef uniq_clamp
    #define uniq_clamp(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#endif

/********************************
 *    static variables
 ********************************/

/* uniq is fully stateless per invocation, so no module-scoped state is
 * required beyond the build helpers above.  */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Program entry point.
 *
 * Parses the command line, applies Windows console setup, then hands off
 * to @c _uniq_run for the actual record-processing pipeline.
 */
int main(int argc, char ** argv)
{
    uniq_opts_t opts;

#ifdef UNIQ_PLATFORM_WINDOWS
    /*
     * Attach to the parent console (in case we were spawned detached)
     * and request UTF-8 decoding of our own stdin/stdout byte streams;
     * the real per-character glyph correctness on legacy console hosts
     * is achieved via _uniq_write_win32 / WriteConsoleW below.
     */
    (void)AttachConsole(ATTACH_PARENT_PROCESS);
    (void)SetConsoleOutputCP(65001U);
    (void)SetConsoleCP(65001U);
    /* Prevent CRLF translation so our byte-exact compare stays stable */
    _setmode(_fileno(stdin),  O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
    _setmode(_fileno(stderr), O_BINARY);
#endif

    (void)memset(&opts, 0, sizeof(opts));
    opts.mode  = UNIQ_MODE_DEFAULT;
    opts.delim = UNIQ_DELIM_NONE;

    if (!_uniq_parse_opts(argc, argv, &opts)) { return 1; }

    return _uniq_run(&opts);
}

/********************************
 *    static functions
 ********************************/

/* --------------------------------------------------------------------
 *  diagnostics / tiny helpers
 * -------------------------------------------------------------------- */

/**
 * @brief NUL-terminated string equality (avoids custom macro -Waddress).
 */
static bool _uniq_streq(const char * a, const char * b)
{
    if (!a || !b) { return false; }
    return (strcmp(a, b) == 0);
}

/**
 * @brief Print the full --help text to stdout and exit successfully.
 */
static void _uniq_print_help(void)
{
    uniq_fputs(
        "Usage: uniq [OPTION]... [INPUT [OUTPUT]]\n"
        "Filter adjacent matching lines from INPUT (or standard input),\n"
        "writing to OUTPUT (or standard output).\n"
        "\n"
        "With no options, matching lines are merged to the first occurrence.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -c, --count           prefix lines by the number of occurrences\n"
        "  -d, --repeated        only print duplicate lines, one for each group\n"
        "  -D, --all-repeated[=METHOD]  print all duplicate lines\n"
        "                           groups can be delimited with an empty line\n"
        "                           METHOD={none(default),prepend,separate}\n"
        "  -f, --skip-fields=N   avoid comparing the first N fields\n"
        "      --group[=METHOD]  show all items, separating groups with an empty line\n"
        "                           METHOD={separate(default),prepend,append,both}\n"
        "  -i, --ignore-case     ignore differences in case when comparing\n"
        "  -s, --skip-chars=N    avoid comparing the first N characters\n"
        "  -u, --unique          only print unique lines\n"
        "  -w, --check-chars=N   compare no more than N characters in lines\n"
        "  -z, --zero-terminated  line delimiter is NUL, not newline\n"
        "      -N                (traditional) same as --skip-fields=N\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "A field is a run of blanks (usually spaces and/or TABs), then non-blank\n"
        "characters.  Fields are skipped before chars.\n"
        "\n"
        "Note: 'uniq' does not detect repeated lines unless they are adjacent.\n"
        "You may want to sort the input first, or use 'sort -u' without 'uniq'.\n"
        "Also, comparisons honor the rules specified by 'LC_COLLATE'.\n"
        "\n"
        "Report bugs to <Yezc via cclinuxtools>.\n",
        uniq_out_stream);
    uniq_fflush(uniq_out_stream);
    exit(0);
}

/**
 * @brief Print the --version banner to stdout and exit successfully.
 */
static void _uniq_print_version(void)
{
    uniq_printf("uniq (Yezc cclinuxtools) %s\n", UNIQ_VERSION_STR);
    uniq_printf("%s", "MIT License\n");
    uniq_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    uniq_printf("%s", "License MIT: <https://mit-license.org/>\n");
    uniq_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    uniq_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
    uniq_fflush(uniq_out_stream);
    exit(0);
}

/* --------------------------------------------------------------------
 *  option parsing
 * -------------------------------------------------------------------- */

/**
 * @brief Parse a non-negative long integer from @p s (decimal digits only).
 *
 * GNU uniq accepts the usual forms; leading '+' is allowed, anything
 * else is rejected.  Values that overflow @c LONG_MAX are treated as
 * errors so the user gets a clear diagnostic.
 *
 * @param s     NUL-terminated text.
 * @param out   Receives the parsed value when returning true.
 * @return true on successful parse, false on any error.
 */
static bool _uniq_str2long(const char * s, long * out)
{
    long v;
    char * endp = NULL;

    if (!s || !*s)                       { return false; }
    errno = 0;
    v = strtol(s, &endp, 10);
    if (errno == ERANGE)                 { return false; }
    if (!endp || endp == s)              { return false; }
    if (*endp != '\0')                   { return false; }
    if (v < 0)                           { return false; }
    *out = v;
    return true;
}

/**
 * @brief Parse the optional METHOD argument for --all-repeated and --group.
 *
 * @param arg     The token after the '=' (may be NULL/empty if no '=').
 * @param default_method  Method to use when none is supplied (differs per option).
 * @param[out] out  Receives the resolved uniq_delim_t.
 * @return true on success, false on unknown method.
 */
static bool _uniq_parse_delim_method(const char * arg,
                                     uniq_delim_t default_method,
                                     uniq_delim_t * out)
{
    if (!arg || !*arg) {
        *out = default_method;
        return true;
    }
    if (strcmp(arg, "none") == 0)        { *out = UNIQ_DELIM_NONE;     return true; }
    if (strcmp(arg, "prepend") == 0)     { *out = UNIQ_DELIM_PREPEND;  return true; }
    if (strcmp(arg, "separate") == 0)    { *out = UNIQ_DELIM_SEPARATE; return true; }
    if (strcmp(arg, "append") == 0)      { *out = UNIQ_DELIM_APPEND;   return true; }
    if (strcmp(arg, "both") == 0)        { *out = UNIQ_DELIM_BOTH;     return true; }
    return false;
}

/**
 * @brief Setter for mutually-exclusive output modes.
 *
 * Emits a GNU-style "invalid argument" error when the mode has already
 * been chosen, matching how GNU coreutils reports -c vs -d conflicts.
 *
 * @param mode     Mode the caller wants to switch to.
 * @param opts     Options structure to mutate.
 * @param optname  Human-readable option name used in error messages.
 * @return true on success, false on conflict.
 */
static bool _uniq_set_mode(uniq_mode_t mode, uniq_opts_t * opts, const char * optname)
{
    if (opts->mode != UNIQ_MODE_DEFAULT && opts->mode != mode) {
        fprintf(stderr, "uniq: incompatible modes: '%s' conflicts with previous mode\n", optname);
        return false;
    }
    opts->mode = mode;
    return true;
}

/**
 * @brief Parse a `--long-option[=VALUE]` argument.
 *
 * @param[in]  arg       Current argv entry (starts with "--").
 * @param[in]  argv      Full argument vector (for consuming next entries).
 * @param[in]  i         Current index in @p argv.
 * @param[in]  argc      Argument count.
 * @param[out] opts      Options struct to mutate.
 * @param[out] consumed  Number of additional argv entries consumed (0 or 1).
 * @return true on success, false on parse failure (caller exits 1).
 */
static bool _uniq_parse_long(const char * arg, char ** argv, int i, int argc,
                             uniq_opts_t * opts, int * consumed)
{
    const char * optname = arg + 2;
    const char * eq      = strchr(optname, '=');
    size_t       name_len;
    char         name_buf[64];
    const char * val;

    (void)argv; (void)i; (void)argc;
    *consumed = 0;

    if (eq) {
        name_len = (size_t)(eq - optname);
        val      = eq + 1;
    } else {
        name_len = strlen(optname);
        val      = NULL;
    }
    if (name_len >= sizeof(name_buf)) {
        fprintf(stderr, "uniq: unrecognized option '%s'\n", arg);
        return false;
    }
    (void)memcpy(name_buf, optname, name_len);
    name_buf[name_len] = '\0';

    if (_uniq_streq(name_buf, "help"))                    { _uniq_print_help(); }
    if (_uniq_streq(name_buf, "version"))                 { _uniq_print_version(); }
    if (_uniq_streq(name_buf, "count"))                   { return _uniq_set_mode(UNIQ_MODE_COUNT, opts, name_buf); }
    if (_uniq_streq(name_buf, "repeated"))                { return _uniq_set_mode(UNIQ_MODE_REPEATED, opts, name_buf); }
    if (_uniq_streq(name_buf, "unique"))                  { return _uniq_set_mode(UNIQ_MODE_UNIQUE, opts, name_buf); }
    if (_uniq_streq(name_buf, "ignore-case"))             { opts->ignore_case = true; return true; }
    if (_uniq_streq(name_buf, "zero-terminated"))         { opts->zero_terminated = true; return true; }

    if (_uniq_streq(name_buf, "all-repeated")) {
        if (!_uniq_parse_delim_method(val, UNIQ_DELIM_NONE, &opts->delim)) {
            fprintf(stderr, "uniq: invalid argument '%s' for --all-repeated\n", val ? val : "");
            return false;
        }
        return _uniq_set_mode(UNIQ_MODE_ALL_REPEATED, opts, name_buf);
    }
    if (_uniq_streq(name_buf, "group")) {
        if (!_uniq_parse_delim_method(val, UNIQ_DELIM_SEPARATE, &opts->delim)) {
            fprintf(stderr, "uniq: invalid argument '%s' for --group\n", val ? val : "");
            return false;
        }
        return _uniq_set_mode(UNIQ_MODE_GROUP, opts, name_buf);
    }

    if (_uniq_streq(name_buf, "skip-fields")) {
        long n;
        if (!val)                               { fprintf(stderr, "uniq: option '--skip-fields' requires an argument\n"); return false; }
        if (!_uniq_str2long(val, &n))         { fprintf(stderr, "uniq: invalid number of fields to skip: '%s'\n", val); return false; }
        opts->skip_fields = n;
        return true;
    }
    if (_uniq_streq(name_buf, "skip-chars")) {
        long n;
        if (!val)                               { fprintf(stderr, "uniq: option '--skip-chars' requires an argument\n"); return false; }
        if (!_uniq_str2long(val, &n))         { fprintf(stderr, "uniq: invalid number of bytes to skip: '%s'\n", val); return false; }
        opts->skip_chars = n;
        return true;
    }
    if (_uniq_streq(name_buf, "check-chars")) {
        long n;
        if (!val)                               { fprintf(stderr, "uniq: option '--check-chars' requires an argument\n"); return false; }
        if (!_uniq_str2long(val, &n))         { fprintf(stderr, "uniq: invalid number of bytes to compare: '%s'\n", val); return false; }
        opts->check_chars = (n <= 0) ? 0 : n;   /* 0 = unlimited */
        return true;
    }

    fprintf(stderr, "uniq: unrecognized option '%s'\n", arg);
    return false;
}

/**
 * @brief Parse a short option cluster starting with '-' (but not '--').
 *
 * Handles:
 *   - Bundled boolean shorts: -cdiuDz
 *   - Shorts with values, either glued (-f3) or in next argv (-f 3)
 *   - Traditional skip-fields: -123 (digits only, no letter)
 *
 * @return true on success; @p *consumed tells the caller how many extra
 *         argv entries were slurped (0 or 1).
 */
static bool _uniq_parse_short(const char * arg, char ** argv, int i, int argc,
                              uniq_opts_t * opts, int * consumed)
{
    const char * p = arg + 1;  /* skip leading '-' */
    int          j;
    long         n;

    *consumed = 0;

    /* Traditional "uniq -3" form: all digits after the dash => skip_fields */
    if (*p >= '0' && *p <= '9') {
        if (!_uniq_str2long(p, &n)) {
            fprintf(stderr, "uniq: invalid number of fields to skip: '%s'\n", arg);
            return false;
        }
        opts->skip_fields = n;
        return true;
    }

    for (j = 0; p[j] != '\0'; j++) {
        char ch = p[j];
        const char * glued = p + j + 1;
        bool  has_glued = (*glued != '\0');
        long  tmp;

        switch (ch) {
        case 'c':
            if (!_uniq_set_mode(UNIQ_MODE_COUNT, opts, "c"))        { return false; }
            continue;
        case 'd':
            if (!_uniq_set_mode(UNIQ_MODE_REPEATED, opts, "d"))    { return false; }
            continue;
        case 'D':
            if (!_uniq_parse_delim_method(NULL, UNIQ_DELIM_NONE, &opts->delim)) { return false; }
            if (!_uniq_set_mode(UNIQ_MODE_ALL_REPEATED, opts, "D"))             { return false; }
            continue;
        case 'i':
            opts->ignore_case = true;
            continue;
        case 'u':
            if (!_uniq_set_mode(UNIQ_MODE_UNIQUE, opts, "u"))      { return false; }
            continue;
        case 'z':
            opts->zero_terminated = true;
            continue;
        case 'f':
        case 's':
        case 'w':
            if (has_glued) {
                if (!_uniq_str2long(glued, &tmp)) {
                    fprintf(stderr, "uniq: invalid number for '-%c': '%s'\n", ch, glued);
                    return false;
                }
            } else {
                if (i + 1 >= argc) {
                    fprintf(stderr, "uniq: option requires an argument -- '%c'\n", ch);
                    return false;
                }
                if (!_uniq_str2long(argv[i + 1], &tmp)) {
                    fprintf(stderr, "uniq: invalid number for '-%c': '%s'\n", ch, argv[i + 1]);
                    return false;
                }
                *consumed = 1;
            }
            switch (ch) {
            case 'f': opts->skip_fields = tmp; break;
            case 's': opts->skip_chars  = tmp; break;
            case 'w': opts->check_chars = (tmp <= 0) ? 0 : tmp; break;
            default: break;
            }
            goto _break_short_loop;
        default:
            fprintf(stderr, "uniq: invalid option -- '%c'\n", ch);
            return false;
        }
    }
_break_short_loop:
    return true;
}

/**
 * @brief Top-level option parser.  Exits directly (0) on --help/--version.
 */
static bool _uniq_parse_opts(int argc, char ** argv, uniq_opts_t * opts)
{
    int i;
    int positional = 0;

    for (i = 1; i < argc; i++) {
        const char * a = argv[i];
        int consumed = 0;

        if (!a) { continue; }

        if (a[0] == '-' && a[1] != '\0') {
            bool ok;
            if (a[1] == '-') {
                /* Explicit '--' terminates options (POSIX convention) */
                if (a[2] == '\0') {
                    i++;
                    break;
                }
                ok = _uniq_parse_long(a, argv, i, argc, opts, &consumed);
            } else {
                ok = _uniq_parse_short(a, argv, i, argc, opts, &consumed);
            }
            if (!ok) { return false; }
            i += consumed;
            continue;
        }

        /* plain '-' means stdin / stdout positional, still counted */
        switch (positional) {
        case 0: opts->in_path  = a; break;
        case 1: opts->out_path = a; break;
        default:
            fprintf(stderr, "uniq: extra operand '%s'\n", a);
            return false;
        }
        positional++;
    }

    /* Any argv entries remaining after explicit '--' are positional */
    for (; i < argc; i++) {
        const char * a = argv[i];
        switch (positional) {
        case 0: opts->in_path  = a; break;
        case 1: opts->out_path = a; break;
        default:
            fprintf(stderr, "uniq: extra operand '%s'\n", a);
            return false;
        }
        positional++;
    }

    return true;
}

/* --------------------------------------------------------------------
 *  dynamic line buffer + record reader
 * -------------------------------------------------------------------- */

static void _uniq_linebuf_init(uniq_linebuf_t * lb)
{
    lb->text = NULL;
    lb->len  = 0;
    lb->cap  = 0;
}

static void _uniq_linebuf_reset(uniq_linebuf_t * lb)
{
    lb->len = 0;
}

static bool _uniq_linebuf_reserve(uniq_linebuf_t * lb, size_t extra)
{
    size_t new_cap;
    char * nbuf;

    if (lb->cap - lb->len >= extra) { return true; }
    new_cap = lb->cap ? lb->cap : UNIQ_LINE_BUF_INIT;
    while (new_cap - lb->len < extra) {
        if (new_cap > (SIZE_MAX / 2U)) { new_cap = SIZE_MAX; break; }
        new_cap *= 2U;
    }
    nbuf = (char *)realloc(lb->text, new_cap);
    if (!nbuf) {
        fprintf(stderr, "uniq: out of memory\n");
        return false;
    }
    lb->text = nbuf;
    lb->cap  = new_cap;
    return true;
}

static bool _uniq_linebuf_append(uniq_linebuf_t * lb, const char * data, size_t n)
{
    if (n == 0) { return true; }
    if (!_uniq_linebuf_reserve(lb, n)) { return false; }
    (void)memcpy(lb->text + lb->len, data, n);
    lb->len += n;
    return true;
}

static void _uniq_linebuf_free(uniq_linebuf_t * lb)
{
    if (!lb) { return; }
    uniq_safe_free(lb->text);
    lb->len = 0;
    lb->cap = 0;
}

/**
 * @brief Read one record from @p fp into @p lb.
 *
 * A "record" is either a newline-terminated line or (with -z) a
 * NUL-terminated item.  The trailing terminator is consumed from the
 * stream but NOT stored in @c lb->len.  Callers can tell whether the
 * file ended without a terminator by comparing the return value:
 *
 *   1 => OK, terminator seen
 *   0 => EOF with a partial record (caller should still emit it)
 *  -1 => I/O error
 *
 * @return 1 on success with terminator, 0 on EOF-with-data, -1 on error.
 */
static int _uniq_read_record(FILE * fp, uniq_linebuf_t * lb, int term)
{
    /*
     * Call-persistent scratch buffer.  A previous call may have left
     * (scratch_end - scratch_start) unconsumed bytes at the *start* of
     * the buffer; subsequent freads therefore append at scratch_end
     * so those tail bytes are not clobbered.  After a terminator is
     * found the leftovers are memmove()d down to scratch[0] and the
     * cursors are rewound accordingly.
     */
    static char scratch[4096];
    static size_t scratch_start = 0;
    static size_t scratch_end   = 0;

    size_t used;

    _uniq_linebuf_reset(lb);

    for (;;) {
        size_t n;
        size_t k;

        /* Top up the buffer from the file when we have nothing pending. */
        while (scratch_end == scratch_start) {
            n = fread(scratch, 1, sizeof(scratch), fp);
            if (n == 0) {
                if (lb->len > 0)              { return 0; }   /* EOF, partial */
                if (ferror(fp))               { return -1; }
                return 0;                                      /* true EOF */
            }
            scratch_start = 0;
            scratch_end   = n;
        }

        used = scratch_end - scratch_start;

        /* find the terminator byte in scratch[start .. end-1] */
        for (k = 0; k < used; k++) {
            size_t off = scratch_start + k;
            if ((unsigned char)scratch[off] == (unsigned char)term) {
                if (k > 0) {
                    if (!_uniq_linebuf_append(lb, scratch + scratch_start, k)) {
                        return -1;
                    }
                }
                /* Drop the k consumed bytes plus the terminator. */
                scratch_start += (k + 1);
                if (scratch_start == scratch_end) { scratch_start = scratch_end = 0; }
                else if (scratch_start > sizeof(scratch) / 2U) {
                    /* Shift down so future fread() has room. */
                    used = scratch_end - scratch_start;
                    (void)memmove(scratch, scratch + scratch_start, used);
                    scratch_start = 0;
                    scratch_end   = used;
                }
                return 1;
            }
        }

        /* no terminator; consume everything currently in scratch. */
        if (!_uniq_linebuf_append(lb, scratch + scratch_start, used)) {
            return -1;
        }
        scratch_start = scratch_end = 0;
    }
}

/* --------------------------------------------------------------------
 *  key computation + compare
 * -------------------------------------------------------------------- */

/**
 * @brief Return the byte offset at which the key portion of a line begins
 *        after skipping @p n fields.
 *
 * Per GNU uniq documentation, a "field" is zero-or-more blanks followed
 * by one-or-more non-blanks.  Fields are 1-based; passing @p n <= 0
 * returns 0 (skip nothing).  If the line ends before @p n full fields
 * could be consumed we return @c len, meaning the key is an empty
 * string (matches GNU behavior).
 */
static size_t _uniq_skip_n_fields(const char * text, size_t len, long n)
{
    size_t i = 0;
    long   skipped;

    if (n <= 0) { return 0; }

    for (skipped = 0; skipped < n; skipped++) {
        /* skip leading blanks (optional per GNU definition) */
        while (i < len &&
               (text[i] == ' ' || text[i] == '\t' ||
                text[i] == '\v' || text[i] == '\f' || text[i] == '\r')) {
            i++;
        }
        if (i == len) { return len; }  /* too few fields => empty key */
        /* skip non-blanks (the field body) */
        while (i < len &&
               !(text[i] == ' ' || text[i] == '\t' ||
                 text[i] == '\v' || text[i] == '\f' || text[i] == '\r')) {
            i++;
        }
    }
    return i;
}

/**
 * @brief Compute the byte offset into @p text/@p len where compare-key
 *        begins after applying the -f and -s skips.
 */
static size_t _uniq_key_start(const char * text, size_t len, const uniq_opts_t * opts)
{
    size_t off;

    off = _uniq_skip_n_fields(text, len, opts->skip_fields);
    if ((long)off > LONG_MAX)                             { off = len; }
    if (opts->skip_chars > 0) {
        size_t add = (size_t)opts->skip_chars;
        if (off > len || add > len - off) {
            return len;
        }
        off += add;
    }
    return off;
}

/**
 * @brief Compute the effective length of the compare key for a line,
 *        applying the -w cap.  A cap of 0 means "use all bytes".
 */
static size_t _uniq_key_len(const char * text, size_t len, const uniq_opts_t * opts)
{
    size_t start = _uniq_key_start(text, len, opts);
    size_t avail = (start > len) ? 0 : (len - start);

    if (opts->check_chars <= 0) { return avail; }
    if ((size_t)opts->check_chars < avail) { return (size_t)opts->check_chars; }
    return avail;
}

/**
 * @brief Compare two records' keys for equality under the current opts.
 *
 * Uses either a bytewise compare or a tolower()-based case-insensitive
 * walk.  Returns true only when the two keys are identical.
 */
static bool _uniq_keys_equal(const char * a, size_t alen,
                             const char * b, size_t blen,
                             const uniq_opts_t * opts)
{
    size_t sa = _uniq_key_start(a, alen, opts);
    size_t sb = _uniq_key_start(b, blen, opts);
    size_t ka = _uniq_key_len(a, alen, opts);  /* length *after* start */
    size_t kb = _uniq_key_len(b, blen, opts);
    size_t kmin;
    size_t i;

    if (ka != kb) { return false; }
    kmin = ka;  /* ka == kb */

    if (!opts->ignore_case) {
        return (memcmp(a + sa, b + sb, kmin) == 0);
    }
    for (i = 0; i < kmin; i++) {
        unsigned char ca = (unsigned char)a[sa + i];
        unsigned char cb = (unsigned char)b[sb + i];
        if (tolower(ca) != tolower(cb)) { return false; }
    }
    return true;
}

/* --------------------------------------------------------------------
 *  output helpers
 * -------------------------------------------------------------------- */

/**
 * @brief Emit the "   NNNN " count prefix used by the -c mode.
 *
 * GNU uniq uses a 7-character-wide right-aligned field followed by a
 * single space; we use a conservative local buffer and the same 7-char
 * width so output matches GNU byte-for-byte on a typical corpus.
 */
static void _uniq_emit_count_prefix(unsigned long count, FILE * out)
{
    char buf[UNIQ_COUNT_BUF_LEN];
    int  n = snprintf(buf, sizeof(buf), "%7lu ", count);
    if (n > 0) { (void)uniq_fwrite(buf, 1, (size_t)n, out); }
}

/**
 * @brief Write a single record payload plus its terminator to @p out.
 */
static void _uniq_emit_record(const char * text, size_t len, int term, FILE * out)
{
    char tc = (char)term;
    if (len > 0) { (void)uniq_fwrite(text, 1, len, out); }
    (void)uniq_fwrite(&tc, 1, 1, out);
}

/**
 * @brief Write one group-delimiter record (an empty record).  The
 *        terminator byte is the same as that used for records (LF or NUL).
 */
static void _uniq_emit_delimiter(int term, FILE * out)
{
    char tc = (char)term;
    (void)uniq_fwrite(&tc, 1, 1, out);
}

/* --------------------------------------------------------------------
 *  group-flush state machine
 * -------------------------------------------------------------------- */

/**
 * @brief Emit one finished group according to the current @p opts.
 *
 * When a mode requires every line of the group (UNIQ_MODE_ALL_REPEATED
 * or UNIQ_MODE_GROUP), we consume @p acc as the pre-serialised byte
 * stream of "<record><term>..." pairs; otherwise @p rep (the first line
 * of the group) plus @p count is enough.
 *
 * @param any_output_yet  Tracks whether ANY group has been printed so
 *                        far, so delim styles like SEPARATE which should
 *                        not fire before the first group can decide.
 */
static void _uniq_flush_group(const uniq_linebuf_t * rep,
                              unsigned long count,
                              bool * any_output_yet,
                              const uniq_opts_t * opts,
                              FILE * out,
                              int term,
                              const uniq_linebuf_t * acc)
{
    const bool repeated = (count >= 2UL);
    const bool unique   = (count == 1UL);
    (void)rep;

    /* ---------- decide whether this group emits anything at all ------ */
    switch (opts->mode) {
    case UNIQ_MODE_REPEATED:
    case UNIQ_MODE_ALL_REPEATED:
        if (!repeated) { return; }
        break;
    case UNIQ_MODE_UNIQUE:
        if (!unique)   { return; }
        break;
    default:
        /* default, count, group: always emit */
        break;
    }

    /* ---------- delimiters before the group content ----------------- */
    switch (opts->mode) {
    case UNIQ_MODE_ALL_REPEATED:
        if (opts->delim == UNIQ_DELIM_PREPEND) {
            _uniq_emit_delimiter(term, out);
        } else if (opts->delim == UNIQ_DELIM_SEPARATE) {
            if (*any_output_yet) { _uniq_emit_delimiter(term, out); }
        } else {
            /* UNIQ_DELIM_NONE / UNIQ_DELIM_APPEND / UNIQ_DELIM_BOTH: none up front */
        }
        break;
    case UNIQ_MODE_GROUP:
        if (opts->delim == UNIQ_DELIM_PREPEND) {
            _uniq_emit_delimiter(term, out);
        } else if (opts->delim == UNIQ_DELIM_SEPARATE) {
            if (*any_output_yet) { _uniq_emit_delimiter(term, out); }
        } else if (opts->delim == UNIQ_DELIM_BOTH) {
            /*
             * BOTH mode: leading shared separator before each group.
             * The leading (first group) + the inter-group separators
             * are emitted here; the trailing delimiter after the very
             * last group is emitted once by _uniq_run after the final
             * flush, so group-to-group boundaries share one delimiter
             * rather than stacking two.
             */
            _uniq_emit_delimiter(term, out);
        }
        /* UNIQ_DELIM_APPEND: nothing up front */
        break;
    default:
        /* Non-grouped modes never insert leading delimiters. */
        break;
    }

    /* ---------- emit the group content itself ----------------------- */
    switch (opts->mode) {
    case UNIQ_MODE_DEFAULT:
        /* first copy only */
        _uniq_emit_record(rep->text, rep->len, term, out);
        break;
    case UNIQ_MODE_COUNT:
        _uniq_emit_count_prefix(count, out);
        _uniq_emit_record(rep->text, rep->len, term, out);
        break;
    case UNIQ_MODE_REPEATED:
        _uniq_emit_record(rep->text, rep->len, term, out);
        break;
    case UNIQ_MODE_UNIQUE:
        _uniq_emit_record(rep->text, rep->len, term, out);
        break;
    case UNIQ_MODE_ALL_REPEATED:
    case UNIQ_MODE_GROUP:
        if (acc && acc->len > 0) {
            (void)uniq_fwrite(acc->text, 1, acc->len, out);
        }
        break;
    }

    /* ---------- delimiters after the group content ------------------ */
    switch (opts->mode) {
    case UNIQ_MODE_GROUP:
        /*
         * APPEND: each group ends with a delimiter.
         * BOTH  : inter-group delimiters are already shared via the
         *         BEFORE block; only the very last group still needs
         *         a trailing delimiter which is emitted by _uniq_run
         *         once after the final flush (so 4 groups => 5 D bytes).
         */
        if (opts->delim == UNIQ_DELIM_APPEND) {
            _uniq_emit_delimiter(term, out);
        }
        break;
    default:
        break;
    }

    *any_output_yet = true;
}

/* --------------------------------------------------------------------
 *  main processing loop
 * -------------------------------------------------------------------- */

/**
 * @brief Append the @p n bytes at @p data followed by the terminator
 *        @p term onto the accumulator @p acc (used for -D / --group).
 */
static bool _uniq_accumulate_record(uniq_linebuf_t * acc,
                                    const char * data, size_t n, int term)
{
    char tc = (char)term;
    if (n > 0) { if (!_uniq_linebuf_append(acc, data, n)) { return false; } }
    return _uniq_linebuf_append(acc, &tc, 1);
}

/**
 * @brief Open / assign the input and output streams for one uniq run.
 */
static int _uniq_open_streams(const uniq_opts_t * opts, FILE ** inp, FILE ** outp)
{
    FILE * in  = stdin;
    FILE * out = stdout;

    if (opts->in_path && !_uniq_streq(opts->in_path, "-")) {
        in = fopen(opts->in_path, "rb");
        if (!in) {
            fprintf(stderr, "uniq: cannot open '%s' for reading: %s\n",
                    opts->in_path, strerror(errno));
            return -1;
        }
    }
    if (opts->out_path && !_uniq_streq(opts->out_path, "-")) {
        out = fopen(opts->out_path, "wb");
        if (!out) {
            fprintf(stderr, "uniq: cannot open '%s' for writing: %s\n",
                    opts->out_path, strerror(errno));
            if (in != stdin) { (void)fclose(in); }
            return -1;
        }
    }
    *inp  = in;
    *outp = out;
    return 0;
}

/**
 * @brief Top-level processing loop: read records, group by equal keys,
 *        emit each group using @c _uniq_flush_group.
 */
static int _uniq_run(const uniq_opts_t * opts)
{
    int       term    = opts->zero_terminated ? '\0' : '\n';
    bool      buf_all = (opts->mode == UNIQ_MODE_ALL_REPEATED) ||
                        (opts->mode == UNIQ_MODE_GROUP);
    FILE * in  = NULL;
    FILE * out = NULL;

    uniq_linebuf_t rep;  /* representative text of the current group */
    uniq_linebuf_t acc;  /* accumulated full records for the group */
    uniq_linebuf_t cur;  /* most recently read record (scratch) */

    bool          have_any_group = false;
    unsigned long cur_count      = 0UL;
    bool          any_output     = false;
    int           rc             = 0;
    int           read_rc;

    if (_uniq_open_streams(opts, &in, &out) != 0) { return 1; }

    /*
     * have_any_group == false means we have not yet read the very first
     * record; the variable is used below inside the read loop so some
     * compilers would otherwise warn about it being unused on a subset
     * of control-flow paths.
     */
    (void)any_output;

    _uniq_linebuf_init(&rep);
    _uniq_linebuf_init(&acc);
    _uniq_linebuf_init(&cur);

    for (;;) {
        bool keys_eq;

        read_rc = _uniq_read_record(in, &cur, term);
        if (read_rc < 0) {
            fprintf(stderr, "uniq: read error: %s\n", strerror(errno));
            rc = 1;
            break;
        }
        if (read_rc == 0 && cur.len == 0) {
            /* Genuine end of stream with no partial record. */
            break;
        }

        if (!have_any_group) {
            /* Seed the first group. */
            if (!_uniq_linebuf_reserve(&rep, cur.len)) { rc = 1; break; }
            (void)memcpy(rep.text, cur.text, cur.len);
            rep.len = cur.len;
            cur_count = 1UL;
            have_any_group = true;
            if (buf_all) {
                _uniq_linebuf_reset(&acc);
                if (!_uniq_accumulate_record(&acc, cur.text, cur.len, term)) { rc = 1; break; }
            }
            continue;
        }

        keys_eq = _uniq_keys_equal(cur.text, cur.len, rep.text, rep.len, opts);
        if (keys_eq) {
            cur_count++;
            if (buf_all) {
                if (!_uniq_accumulate_record(&acc, cur.text, cur.len, term)) { rc = 1; break; }
            }
        } else {
            /* Flush the previous group, then begin the new one. */
            _uniq_flush_group(&rep, cur_count, &any_output, opts, out, term, &acc);
            if (!_uniq_linebuf_reserve(&rep, cur.len)) { rc = 1; break; }
            (void)memcpy(rep.text, cur.text, cur.len);
            rep.len = cur.len;
            cur_count = 1UL;
            if (buf_all) {
                _uniq_linebuf_reset(&acc);
                if (!_uniq_accumulate_record(&acc, cur.text, cur.len, term)) { rc = 1; break; }
            }
        }

        if (read_rc == 0) {
            /* Partial record (no trailing terminator) was the last one. */
            break;
        }
    }

    /* Flush the final in-flight group if any. */
    if (rc == 0 && have_any_group) {
        _uniq_flush_group(&rep, cur_count, &any_output, opts, out, term, &acc);
    }
    /*
     * --group=both: emit the closing delimiter after the last group.
     * Combined with the BEFORE-block shared separators this yields
     * N+1 delimiters for N groups (around each = both ends + between).
     */
    if (rc == 0 && have_any_group &&
        opts->mode == UNIQ_MODE_GROUP && opts->delim == UNIQ_DELIM_BOTH) {
        _uniq_emit_delimiter(term, out);
    }

    _uniq_linebuf_free(&rep);
    _uniq_linebuf_free(&acc);
    _uniq_linebuf_free(&cur);

    uniq_fflush(out);
    if (in  != stdin)  { (void)fclose(in);  }
    if (out != stdout) {
        if (fclose(out) != 0) {
            fprintf(stderr, "uniq: write error: %s\n", strerror(errno));
            rc = 1;
        }
    }
    return rc;
}

/* --------------------------------------------------------------------
 *  Windows console / pipe output helpers
 *
 *  These exactly mirror the implementation used by sort.c so the
 *  Chinese-CP936 mojibake fix is consistent across cclinuxtools.
 * -------------------------------------------------------------------- */
#ifdef UNIQ_PLATFORM_WINDOWS

static HANDLE _uniq_std_handle_for_fd(int fd)
{
    if (fd == 1)      { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

static bool _uniq_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _uniq_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

static bool _uniq_is_disk_stream(FILE * fp)
{
    HANDLE h;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _uniq_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetFileType(h) == FILE_TYPE_DISK);
}

static bool _uniq_is_pipe_stream(FILE * fp)
{
    HANDLE h;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _uniq_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetFileType(h) == FILE_TYPE_PIPE);
}

static UINT _uniq_output_codepage(void)
{
    UINT cp = GetConsoleOutputCP();
    if (cp == 0U) { cp = GetACP(); }
    if (cp == 0U) { cp = 65001U; }
    return cp;
}

static size_t _uniq_write_pipe_cp(const void * buf, size_t len, FILE * fp)
{
    UINT      cp    = _uniq_output_codepage();
    int       fd    = _fileno(fp);
    HANDLE    h     = _uniq_std_handle_for_fd(fd);
    int       wlen;
    wchar_t * wbuf  = NULL;
    int       blen;
    char    * obuf  = NULL;
    DWORD     written = 0;
    BOOL      ok;

    if (len == 0)                                          { return 0; }
    if (cp == 65001U)                                      { return fwrite(buf, 1, len, fp); }
    if (h == INVALID_HANDLE_VALUE || !h)                   { return fwrite(buf, 1, len, fp); }

    wlen = MultiByteToWideChar(CP_UTF8, 0, (const char *)buf, (int)len, NULL, 0);
    if (wlen <= 0) { return fwrite(buf, 1, len, fp); }
    wbuf = (wchar_t *)malloc((size_t)(wlen + 1) * sizeof(wchar_t));
    if (!wbuf) { return fwrite(buf, 1, len, fp); }
    MultiByteToWideChar(CP_UTF8, 0, (const char *)buf, (int)len, wbuf, wlen);
    wbuf[wlen] = L'\0';

    blen = WideCharToMultiByte(cp, 0, wbuf, wlen, NULL, 0, NULL, NULL);
    if (blen <= 0) { free(wbuf); return fwrite(buf, 1, len, fp); }
    obuf = (char *)malloc((size_t)(blen + 1));
    if (!obuf) { free(wbuf); return fwrite(buf, 1, len, fp); }
    WideCharToMultiByte(cp, 0, wbuf, wlen, obuf, blen, NULL, NULL);
    obuf[blen] = '\0';

    ok = WriteFile(h, obuf, (DWORD)blen, &written, NULL);
    free(wbuf);
    free(obuf);
    if (!ok) { return fwrite(buf, 1, len, fp); }
    (void)written;
    return len;
}

static size_t _uniq_write_win32(const void * buf, size_t len, FILE * fp)
{
    int  fd;
    bool is_std;

    if (!fp) { return 0; }
    fd     = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /* Explicit OUTPUT files, temp streams, etc. -> raw UTF-8 bytes. */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_uniq_is_console_stream(fp)) {
        int       wlen;
        wchar_t * wbuf;
        DWORD     written = 0;
        BOOL      ok;
        HANDLE    h = _uniq_std_handle_for_fd(fd);

        if (len == 0) { return 0; }
        wlen = MultiByteToWideChar(CP_UTF8, 0, (const char *)buf, (int)len, NULL, 0);
        if (wlen <= 0) { return fwrite(buf, 1, len, fp); }
        wbuf = (wchar_t *)malloc((size_t)(wlen + 1) * sizeof(wchar_t));
        if (!wbuf) { return fwrite(buf, 1, len, fp); }
        MultiByteToWideChar(CP_UTF8, 0, (const char *)buf, (int)len, wbuf, wlen);
        wbuf[wlen] = L'\0';
        ok = WriteConsoleW(h, wbuf, (DWORD)wlen, &written, NULL);
        free(wbuf);
        if (!ok) { return fwrite(buf, 1, len, fp); }
        (void)written;
        return len;
    }

    if (_uniq_is_disk_stream(fp)) { return fwrite(buf, 1, len, fp); }
    if (_uniq_is_pipe_stream(fp)) { return _uniq_write_pipe_cp(buf, len, fp); }
    return fwrite(buf, 1, len, fp);
}

#endif  /* UNIQ_PLATFORM_WINDOWS */