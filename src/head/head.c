/**
 * @file head.c
 * @brief Cross-platform implementation of the Linux head command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils head(1).
 *
 * Key behaviors:
 *   - -n, --lines=[+]NUM        print first NUM lines (default 10)
 *   - -c, --bytes=[+]NUM        print first NUM bytes
 *   - NUM forms: +NUM (first NUM), -NUM (all but last NUM)
 *   - byte suffixes: b(512) c(1) w(2) kB(1000) K/KB/KiB(1024)
 *                     MB(1e6) M/MiB(2^20) GB(1e9) G/GiB(2^30) ...
 *   - obsolete form: -NUM[bcklmqv] (e.g. head -5, head -5c, head -5q)
 *   - -q, --quiet, --silent     never print file name headers
 *   - -v, --verbose             always print file name headers
 *   - -z, --zero-terminated     line delimiter is NUL, not newline
 *   - multiple files           print "==> name <==" header per file
 *   - --help                    display help and exit
 *   - --version                 output version and exit
 *   - [FILE]...                 input files (default: stdin; - means stdin)
 *
 * Platform <resource> sources:
 *   Linux:     stdio, stdlib, string, errno, unistd
 *   Windows:   stdio, windows.h, io.h, fcntl.h
 *   macOS:     stdio, stdlib, string, errno, unistd
 *   FreeBSD:   stdio, stdlib, string, errno, unistd
 *   OpenBSD:   stdio, stdlib, string, errno, unistd
 *   NetBSD:    stdio, stdlib, string, errno, unistd
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN -o head.exe head.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -o head head.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -o head head.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o head head.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o head head.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -o head head.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/head>
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
    #define HEAD_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define HEAD_PLATFORM_LINUX   1
    #define HEAD_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define HEAD_PLATFORM_MACOS   1
    #define HEAD_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define HEAD_PLATFORM_FREEBSD 1
    #define HEAD_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define HEAD_PLATFORM_OPENBSD 1
    #define HEAD_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define HEAD_PLATFORM_NETBSD  1
    #define HEAD_PLATFORM_POSIX   1
#else
    #define HEAD_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef HEAD_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef HEAD_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef HEAD_PLATFORM_NETBSD
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

#ifdef HEAD_PLATFORM_WINDOWS
    #include <windows.h>
    #include <shellapi.h>
    #include <io.h>
    #include <fcntl.h>
#else
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Program version string (mirrors build banner). */
#define HEAD_VERSION_STR "v1.0.0"

/** @brief Default number of lines printed when -n is not given. */
#define HEAD_DEFAULT_LINES 10ULL

/** @brief Chunk size (bytes) used by the streaming byte/line readers. */
#define HEAD_READ_CHUNK   65536U

/** @brief Initial capacity of a dynamic line buffer. */
#define HEAD_LINE_INIT    256U

/** @brief Scratch buffer for head_printf on Windows. */
#define HEAD_PRINTF_BUFSZ 2048U

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Output unit selector: count lines or count bytes.
 */
typedef enum {
    HEAD_UNIT_LINES = 0,   /**< -n : count is a number of lines (default)  */
    HEAD_UNIT_BYTES        /**< -c : count is a number of bytes            */
} head_unit_t;

/**
 * @brief Parsed command-line options for one head invocation.
 */
typedef struct {
    head_unit_t       unit;        /**< lines vs bytes                        */
    unsigned long long count;      /**< magnitude of the limit                */
    bool              from_end;    /**< true => print all but the last count  */
    bool              quiet;       /**< -q : suppress file name headers        */
    bool              verbose;     /**< -v : force file name headers           */
    bool              zero_term;   /**< -z : NUL line delimiter                */
    bool              have_count;  /**< a -n / -c was given on the command line */
} head_opts_t;

/**
 * @brief Growable byte buffer used to hold one input record (line).
 */
typedef struct {
    char * buf;       /**< payload bytes (no trailing terminator kept)      */
    size_t len;       /**< number of valid bytes in @c buf                  */
    size_t cap;       /**< allocated capacity (bytes)                       */
} head_linebuf_t;

/**
 * @brief Persistent read scratch carrying leftover bytes across records.
 *
 * Replaces the previous @c ungetc -based pushback, which is only guaranteed
 * for a single byte and silently dropped multi-byte remainders (causing
 * head to lose lines after the first chunk). The scratch keeps unprocessed
 * bytes from the last @c fread so record boundaries can be split safely.
 */
typedef struct {
    char * buf;       /**< raw read buffer (allocated once per file)        */
    size_t cap;       /**< capacity of @c buf (bytes)                        */
    size_t start;     /**< index of first unprocessed byte (inclusive)      */
    size_t end;       /**< index one past last valid byte (exclusive)       */
} head_scratch_t;

/********************************
 *    static prototypes
 ********************************/

/* Diagnostics + option parsing */
static void         _head_print_help(void);
static void         _head_print_version(void);
static bool         _head_parse_suffix(const char * s, unsigned long long * out);
static bool         _head_parse_count(const char * s, bool allow_suffix,
                                      unsigned long long * count, bool * from_end);
static int          _head_parse_short(char opt, const char * arg,
                                      head_opts_t * opts, bool * need_arg);
static int          _head_parse_obsolete(const char * arg, head_opts_t * opts);
static int          _head_parse_opts(int argc, char ** argv,
                                    head_opts_t * opts, char *** files, int * nfiles);

/* Line buffer helpers */
static void         _head_linebuf_init(head_linebuf_t * lb);
static void         _head_linebuf_free(head_linebuf_t * lb);
static bool         _head_linebuf_reserve(head_linebuf_t * lb, size_t extra);
static bool         _head_linebuf_append(head_linebuf_t * lb, const char * data, size_t n);

/* Read scratch (carries leftover bytes across record boundaries) */
static void         _head_scratch_init(head_scratch_t * sc);
static void         _head_scratch_free(head_scratch_t * sc);

/* Record reader: returns 1 = record, 0 = EOF (no data), -1 = error.
 * Uses @p sc to keep unprocessed bytes between calls (no ungetc).
 * @p had_delim is set true when the record ended on a real delimiter,
 * false when it ended at EOF without one (last line, no newline). */
static int          _head_read_record(FILE * fp, head_linebuf_t * lb, int delim,
                                       head_scratch_t * sc, bool * had_delim);

/* Emitters */
static int          _head_first_lines(FILE * fp, unsigned long long n, int delim);
static int          _head_first_bytes(FILE * fp, unsigned long long n);
static int          _head_rest_lines(FILE * fp, unsigned long long n, int delim);
static int          _head_rest_bytes(FILE * fp, unsigned long long n);
static int          _head_process_file(const head_opts_t * opts, const char * name,
                                       bool print_header);
static int          _head_run(const head_opts_t * opts, char ** files, int nfiles);

/* Platform helpers */
static FILE *       _head_fopen_utf8(const char * path, const char * mode);

#ifdef HEAD_PLATFORM_WINDOWS
static HANDLE       _head_std_handle_for_fd(int fd);
static bool         _head_is_console_stream(FILE * fp);
static size_t       _head_write_win32(const void * buf, size_t len, FILE * fp);
static char **      _head_argv_utf8_alloc(int * argc, char ** argv);
static void         _head_argv_utf8_free(int argc, char ** argv_utf8);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for head_fwrite / head_fputs / head_fflush.
 *        Defaults to libc @c stdout .  Redefine externally to redirect.
 */
#ifndef head_out_stream
    #define head_out_stream stdout
#endif

#ifdef HEAD_PLATFORM_WINDOWS
/**
 * @brief Portable byte-write macro used by every text-output path.
 *        On Windows stdout/stderr are routed through @c _head_write_win32
 *        which converts UTF-8 bytes to UTF-16LE via WriteConsoleW for real
 *        console streams, transcodes to the console output codepage for
 *        PowerShell 5.x pipes, and writes raw UTF-8 for disk redirections.
 *        Any non-stdio stream (explicit temp buffers, ...) uses plain fwrite
 *        so test output is byte-identical to GNU head.
 * @sa _head_write_win32
 */
    #ifndef head_fwrite
        #define head_fwrite(buf, sz, cnt, fp) \
            _head_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef head_fwrite
        #define head_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print wrapper (printf-compatible).
 *
 * On Windows the formatted buffer is emitted through @c _head_write_win32 so
 * CJK glyphs render correctly even on legacy CP936 hosts.
 */
#ifndef head_printf
    #ifdef HEAD_PLATFORM_WINDOWS
        #define head_printf(fmt, ...) \
            do { \
                char _headpf[HEAD_PRINTF_BUFSZ]; \
                int _headpf_n = snprintf(_headpf, sizeof(_headpf), (fmt), ##__VA_ARGS__); \
                if (_headpf_n > 0) { (void)_head_write_win32(_headpf, (size_t)_headpf_n, head_out_stream); } \
            } while (0)
    #else
        #define head_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Formatted print to @c stderr (diagnostics).
 *
 * On Windows the formatted buffer is emitted through @c _head_write_win32
 * targeting @c stderr so console/pipe/disk handling is identical to stdout.
 */
#ifndef head_eprintf
    #ifdef HEAD_PLATFORM_WINDOWS
        #define head_eprintf(fmt, ...) \
            do { \
                char _headepf[HEAD_PRINTF_BUFSZ]; \
                int _headepf_n = snprintf(_headepf, sizeof(_headepf), (fmt), ##__VA_ARGS__); \
                if (_headepf_n > 0) { (void)_head_write_win32(_headepf, (size_t)_headepf_n, stderr); } \
            } while (0)
    #else
        #define head_eprintf(fmt, ...) fprintf(stderr, (fmt), ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Write a NUL-terminated string to @c stderr (diagnostics).
 */
#ifndef head_efputs
    #define head_efputs(str) \
        do { const char * _headesp = (str); if (_headesp) { (void)head_fwrite(_headesp, 1, strlen(_headesp), stderr); } } while (0)
#endif

/**
 * @brief Write a single byte (cast via unsigned char to avoid UB with
 *        signed @c char on some compilers) to the default output stream.
 */
#ifndef head_putchar
    #define head_putchar(ch) \
        do { unsigned char _headpc = (unsigned char)(ch); (void)head_fwrite(&_headpc, 1, 1, head_out_stream); } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef head_fputs
    #define head_fputs(str, stream) \
        do { const char * _headsp = (str); if (_headsp) { (void)head_fwrite(_headsp, 1, strlen(_headsp), (stream)); } } while (0)
#endif

/**
 * @brief Flush the given stdio stream.
 */
#ifndef head_fflush
    #define head_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free: free(*p) and set the pointer to NULL.
 *        Callable when @p p itself is NULL (no-op).
 */
#ifndef head_safe_free
    #define head_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/**
 * @brief Clamp @p v into the inclusive range [lo, hi].
 */
#ifndef head_clamp
    #define head_clamp(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#endif

/********************************
 *    static variables
 ********************************/

/* head is stateless per invocation; no module-scoped state required. */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Program entry point.
 *
 * Parses the command line, applies Windows console setup, then hands off
 * to @c _head_run which processes every input file (or stdin).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on any I/O or usage error
 */
int main(int argc, char ** argv)
{
    head_opts_t opts;
    char **     files       = NULL;
    char **     argv_utf8   = NULL;
    int         nfiles      = 0;
    int         rc;

    memset(&opts, 0, sizeof(opts));
    opts.unit      = HEAD_UNIT_LINES;
    opts.count     = HEAD_DEFAULT_LINES;
    opts.from_end  = false;
    opts.zero_term = false;
    opts.quiet     = false;
    opts.verbose   = false;
    opts.have_count= false;

#ifdef HEAD_PLATFORM_WINDOWS
    /* Transcode argv from the C runtime codepage (typically ACP/CP936) to
     * UTF-8 so that error messages, file headers, and fopen paths are
     * byte-correct regardless of the host's console configuration. */
    {
        int     wc  = 0;
        char ** u8a = _head_argv_utf8_alloc(&wc, argv);
        if (u8a) {
            argc      = wc;
            argv_utf8 = u8a;
            argv      = u8a;
        }
    }

    /* Attach to parent console (if spawned detached) and request UTF-8 I/O.
     * Real per-glyph correctness on legacy console hosts is handled by
     * _head_write_win32 / WriteConsoleW below. */
    (void)AttachConsole(ATTACH_PARENT_PROCESS);
    (void)SetConsoleOutputCP(65001U);
    (void)SetConsoleCP(65001U);
    /* Binary mode prevents CRLF translation so byte-exact tests stay stable. */
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    rc = _head_parse_opts(argc, argv, &opts, &files, &nfiles);
    if (rc != 0) {
        head_safe_free(files);
#ifdef HEAD_PLATFORM_WINDOWS
        _head_argv_utf8_free(argc, argv_utf8);
#endif
        return (rc < 0) ? 1 : rc;
    }

    rc = _head_run(&opts, files, nfiles);
    head_safe_free(files);
#ifdef HEAD_PLATFORM_WINDOWS
    _head_argv_utf8_free(argc, argv_utf8);
#endif
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Open a file with a UTF-8-encoded path.
 *
 * On POSIX systems the C runtime already treats @c fopen paths as opaque
 * byte strings which naturally carry UTF-8.  On Windows, however, MSVCRT
 * interprets narrow paths via the ANSI code page, which corrupts any
 * non-ACP byte sequence (e.g. UTF-8).  We therefore transcode to UTF-16
 * and call @c _wfopen.
 */
static FILE * _head_fopen_utf8(const char * path, const char * mode)
{
    if (!path || !mode) { return NULL; }

#ifdef HEAD_PLATFORM_WINDOWS
    {
        int      wpath_len;
        int      wmode_len;
        wchar_t *wpath = NULL;
        wchar_t *wmode = NULL;
        FILE *   fp;

        wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
        if (wpath_len <= 0) { return NULL; }
        wpath = (wchar_t *)malloc((size_t)wpath_len * sizeof(wchar_t));
        if (!wpath) { return NULL; }
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_len);

        wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
        if (wmode_len <= 0) { free(wpath); return NULL; }
        wmode = (wchar_t *)malloc((size_t)wmode_len * sizeof(wchar_t));
        if (!wmode) { free(wpath); return NULL; }
        MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmode_len);

        fp = _wfopen(wpath, wmode);
        free(wpath);
        free(wmode);
        return fp;
    }
#else
    return fopen(path, mode);
#endif
}

/**
 * @brief Print usage to stdout and exit with status 0 (GNU --help).
 */
static void _head_print_help(void)
{
    head_fputs(
        "Usage: head [OPTION]... [FILE]...\n"
        "Print the first 10 lines of each FILE to standard output.\n"
        "With more than one FILE, precede each with a header giving the file name.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -c, --bytes=[-]NUM       print the first NUM bytes of each file;\n"
        "                            with the leading '-', print all but the last\n"
        "                            NUM bytes of each file\n"
        "  -n, --lines=[-]NUM       print the first NUM lines instead of the first 10;\n"
        "                            with the leading '-', print all but the last\n"
        "                            NUM lines of each file\n"
        "  -q, --quiet, --silent    never print headers giving file names\n"
        "  -v, --verbose            always print headers giving file names\n"
        "  -z, --zero-terminated    line delimiter is NUL, not newline\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "NUM may have a multiplier suffix:\n"
        "b 512, c 1, w 2, kB 1000, K 1024, MB 1000000, M 1048576,\n"
        "GB 1000000000, G 1073741824, and so on for T, P, E, Z, Y.\n"
        "\n"
        "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n",
        head_out_stream);
    exit(0);
}

/**
 * @brief Print version to stdout and exit with status 0 (GNU --version).
 */
static void _head_print_version(void)
{
    head_printf("head %s\n", HEAD_VERSION_STR);
    head_fputs("Copyright (C) 2025-2026 Yezc\n"
               "License MIT <https://mit-license.org/>.\n"
               "This is free software: you are free to change and redistribute it.\n"
               "There is NO WARRANTY, to the extent permitted by law.\n",
               head_out_stream);
    exit(0);
}

/**
 * @brief Apply a GNU head byte-multiplier suffix to a magnitude.
 *
 * Recognised suffixes (per GNU coreutils head(1) manual):
 *   - @c b   => x512
 *   - @c c   => x1   (no scaling)
 *   - @c w   => x2
 *   - @c kB  => x1000
 *   - @c K / @c KB / @c KiB => x1024
 *   - @c MB  => x1000000
 *   - @c M / @c MiB => x1048576
 *   - @c GB / @c G / @c GiB ... following the same SI / binary pattern up
 *     to @c YB / @c Y / @c YiB.
 *
 * Overflow is clamped to @c ULLONG_MAX.
 *
 * @param s    suffix text (must be non-empty); compared case-sensitively
 * @param out  receives the multiplied magnitude on success
 * @return true if the suffix is recognised, false otherwise
 */
static bool _head_parse_suffix(const char * s, unsigned long long * out)
{
    unsigned long long mul = 0ULL;
    unsigned long long v   = *out;

    if (!s || !*s) { return false; }

    if (strcmp(s, "b") == 0)        { mul = 512ULL; }
    else if (strcmp(s, "c") == 0)  { mul = 1ULL; }
    else if (strcmp(s, "w") == 0)  { mul = 2ULL; }
    else if (strcmp(s, "kB") == 0) { mul = 1000ULL; }
    else if (strcmp(s, "K") == 0
          || strcmp(s, "KB") == 0
          || strcmp(s, "KiB") == 0) { mul = 1024ULL; }
    else if (strcmp(s, "MB") == 0) { mul = 1000000ULL; }
    else if (strcmp(s, "M") == 0
          || strcmp(s, "MiB") == 0) { mul = 1048576ULL; }
    else if (strcmp(s, "GB") == 0) { mul = 1000000000ULL; }
    else if (strcmp(s, "G") == 0
          || strcmp(s, "GiB") == 0) { mul = 1073741824ULL; }
    else if (strcmp(s, "TB") == 0) { mul = 1000000000000ULL; }
    else if (strcmp(s, "T") == 0
          || strcmp(s, "TiB") == 0) { mul = 1099511627776ULL; }
    else if (strcmp(s, "PB") == 0) { mul = 1000000000000000ULL; }
    else if (strcmp(s, "P") == 0
          || strcmp(s, "PiB") == 0) { mul = 1125899906842624ULL; }
    else if (strcmp(s, "EB") == 0) { mul = 1000000000000000000ULL; }
    else if (strcmp(s, "E") == 0
          || strcmp(s, "EiB") == 0) { mul = ULLONG_MAX; }
    else { return false; }

    if (v != 0ULL) {
        if (mul > ULLONG_MAX / v) {
            *out = ULLONG_MAX;
        }
        else {
            *out = v * mul;
        }
    }
    return true;
}

/**
 * @brief Parse a count argument of the form [+/-]NUM[suffix].
 *
 * Used by @c -n and @c -c.  A leading @c + is the explicit "first NUM"
 * form (from_end = false); a leading @c - is the "all but last NUM" form
 * (from_end = true).
 *
 * @param s             argument text (e.g. "5", "+5", "-5", "1k", "-2M")
 * @param allow_suffix  if true, a trailing byte suffix is scaled
 *                      (used by @c -c ); @c -n never scales.
 * @param count         receives the magnitude
 * @param from_end      receives true if the value is the "all but last" form
 * @return true on success, false on malformed input
 */
static bool _head_parse_count(const char * s, bool allow_suffix,
                              unsigned long long * count, bool * from_end)
{
    char *                   endp   = NULL;
    unsigned long long       v;
    unsigned long long       parsed;
    const char *             p;
    bool                     neg = false;

    if (!s || !*s) { return false; }

    p = s;
    if (*p == '+') {
        p++;
        neg = false;
    }
    else if (*p == '-') {
        p++;
        neg = true;
    }

    if (*p == '\0') { return false; }

    errno = 0;
    parsed = strtoull(p, &endp, 10);
    if (errno != 0) { return false; }
    if (endp == p) { return false; }

    v = parsed;

    if (allow_suffix && *endp != '\0') {
        if (!_head_parse_suffix(endp, &v)) { return false; }
    }
    else if (*endp != '\0') {
        /* -n does not accept any suffix. */
        return false;
    }

    *count    = v;
    *from_end = neg;
    return true;
}

/**
 * @brief Parse a single short option letter (cluster form, e.g. -qv).
 *
 * @param opt       the option character ('n','c','q','v','z')
 * @param arg       remainder of the argv slot after the letter, or NULL.
 *                  For @c -n / @c -c this is either the attached count
 *                  (e.g. "-n5" => arg="5") or empty, in which case the
 *                  count is taken from the following argv slot.
 * @param opts      options struct to update
 * @param need_arg  set to true when @c -n / @c -c still need the next slot
 * @return 0 on success, 1 on --help/--version exit, -1 on error
 */
static int _head_parse_short(char opt, const char * arg,
                             head_opts_t * opts, bool * need_arg)
{
    *need_arg = false;

    switch (opt) {
        case 'n':
        case 'c': {
            const char * cnt;
            bool         allow_suffix;
            if (opt == 'n') {
                opts->unit  = HEAD_UNIT_LINES;
                allow_suffix = false;
            }
            else {
                opts->unit  = HEAD_UNIT_BYTES;
                allow_suffix = true;
            }
            if (arg && *arg) {
                cnt = arg;
            }
            else {
                *need_arg = true;
                return 0;
            }
            if (!_head_parse_count(cnt, allow_suffix,
                                   &opts->count, &opts->from_end)) {
                head_efputs("head: invalid number '");
                head_fputs(cnt, stderr);
                head_efputs("'\n");
                return -1;
            }
            opts->have_count = true;
            return 0;
        }
        case 'q':
            opts->quiet   = true;
            opts->verbose = false;
            return 0;
        case 'v':
            opts->verbose = true;
            opts->quiet   = false;
            return 0;
        case 'z':
            opts->zero_term = true;
            return 0;
        case '?':
            _head_print_help();
            break; /* never returns */
        default:
            head_eprintf("head: invalid option -- '%c'\n", opt);
            head_efputs("Try 'head --help' for more information.\n");
            return -1;
    }
    return -1; /* unreachable */
}

/**
 * @brief Parse the obsolete @c -NUM[suffix] form (e.g. @c head -5, @c head -5c).
 *
 * The obsolete form starts with @c '-' followed by digits, optionally
 * followed by unit/flag letters: @c b (512-byte blocks->bytes),
 * @c c (bytes), @c k (1024 bytes), @c l (lines), @c m (1048576 bytes),
 * @c q (quiet), @c v (verbose).  At most one unit letter selects the
 * output unit; @c l or none means lines.
 *
 * @param arg   argument text beginning with @c - and a digit
 * @param opts  options struct to update
 * @return 0 on success, -1 on error
 */
static int _head_parse_obsolete(const char * arg, head_opts_t * opts)
{
    char *                   endp = NULL;
    unsigned long long       n;
    bool                     have_unit = false;
    const char *             p = arg + 1;   /* skip '-' */

    errno = 0;
    n = strtoull(p, &endp, 10);
    if (errno != 0 || endp == p) { return -1; }

    opts->count      = n;
    opts->from_end   = false;   /* obsolete form is always the "first N" form */
    opts->have_count = true;

    while (*endp != '\0') {
        switch (*endp) {
            case 'b':
                if (have_unit) { return -1; }
                have_unit = true;
                opts->unit = HEAD_UNIT_BYTES;
                opts->count = (n > ULLONG_MAX / 512ULL) ? ULLONG_MAX : n * 512ULL;
                break;
            case 'c':
                if (have_unit) { return -1; }
                have_unit = true;
                opts->unit = HEAD_UNIT_BYTES;
                break;
            case 'k':
                if (have_unit) { return -1; }
                have_unit = true;
                opts->unit = HEAD_UNIT_BYTES;
                opts->count = (n > ULLONG_MAX / 1024ULL) ? ULLONG_MAX : n * 1024ULL;
                break;
            case 'm':
                if (have_unit) { return -1; }
                have_unit = true;
                opts->unit = HEAD_UNIT_BYTES;
                opts->count = (n > ULLONG_MAX / (1024ULL * 1024ULL))
                              ? ULLONG_MAX : n * 1024ULL * 1024ULL;
                break;
            case 'l':
                if (have_unit) { return -1; }
                have_unit = true;
                opts->unit = HEAD_UNIT_LINES;
                break;
            case 'q':
                opts->quiet   = true;
                opts->verbose = false;
                break;
            case 'v':
                opts->verbose = true;
                opts->quiet   = false;
                break;
            default:
                return -1;
        }
        endp++;
    }
    return 0;
}

/**
 * @brief Parse all command-line arguments.
 *
 * Supports GNU-style long options (@c --lines= ), short option clusters
 * (@c -qv ), the obsolete @c -NUM form, @c -- to terminate options, and
 * interleaved file operands.
 *
 * @param argc   argument count
 * @param argv   argument vector
 * @param opts   options struct to fill
 * @param files  receives a malloc'd array of file operand strings
 * @param nfiles receives the number of file operands
 * @return 0 on success, 1 on --help/--version exit (treated as success),
 *         -1 on usage error
 */
static int _head_parse_opts(int argc, char ** argv,
                            head_opts_t * opts, char *** files, int * nfiles)
{
    int    i;
    int    fcap = 0;
    int    fn   = 0;
    char **flist = NULL;
    bool   end_opts = false;

    for (i = 1; i < argc; i++) {
        const char * a = argv[i];

        if (end_opts) {
            /* everything after -- is a file operand */
            goto file_operand;
        }

        if (a[0] != '-' || a[1] == '\0') {
            /* '-' alone, or a non-option: treat as a file operand */
            goto file_operand;
        }

        if (strcmp(a, "--") == 0) {
            end_opts = true;
            continue;
        }

        if (a[0] == '-' && a[1] == '-') {
            /* long option */
            const char * name = a + 2;
            const char * eq   = strchr(name, '=');
            char         nm[32];
            size_t       nlen;

            if (eq) {
                nlen = (size_t)(eq - name);
            }
            else {
                nlen = strlen(name);
            }
            if (nlen >= sizeof(nm)) { nlen = sizeof(nm) - 1; }
            memcpy(nm, name, nlen);
            nm[nlen] = '\0';

            if (strcmp(nm, "help") == 0) {
                _head_print_help();
                break; /* never returns */
            }
            else if (strcmp(nm, "version") == 0) {
                _head_print_version();
                break; /* never returns */
            }
            else if (strcmp(nm, "lines") == 0) {
                const char * val = eq ? eq + 1 : NULL;
                opts->unit = HEAD_UNIT_LINES;
                if (!val) {
                    if (i + 1 >= argc) {
                        head_efputs("head: option '--lines' requires an argument\n");
                        goto fail;
                    }
                    val = argv[++i];
                }
                if (!_head_parse_count(val, false,
                                       &opts->count, &opts->from_end)) {
                    head_efputs("head: invalid number '");
                    head_fputs(val, stderr);
                    head_efputs("'\n");
                    goto fail;
                }
                opts->have_count = true;
                continue;
            }
            else if (strcmp(nm, "bytes") == 0) {
                const char * val = eq ? eq + 1 : NULL;
                opts->unit = HEAD_UNIT_BYTES;
                if (!val) {
                    if (i + 1 >= argc) {
                        head_efputs("head: option '--bytes' requires an argument\n");
                        goto fail;
                    }
                    val = argv[++i];
                }
                if (!_head_parse_count(val, true,
                                       &opts->count, &opts->from_end)) {
                    head_efputs("head: invalid number '");
                    head_fputs(val, stderr);
                    head_efputs("'\n");
                    goto fail;
                }
                opts->have_count = true;
                continue;
            }
            else if (strcmp(nm, "quiet") == 0 || strcmp(nm, "silent") == 0) {
                opts->quiet = true;
                continue;
            }
            else if (strcmp(nm, "verbose") == 0) {
                opts->verbose = true;
                continue;
            }
            else if (strcmp(nm, "zero-terminated") == 0) {
                opts->zero_term = true;
                continue;
            }
            else {
                head_efputs("head: unrecognized option '");
                head_fputs(a, stderr);
                head_efputs("'\nTry 'head --help' for more information.\n");
                goto fail;
            }
        }

        /* a[0]=='-' && a[1] is a digit -> obsolete -NUM form */
        if (a[0] == '-' && a[1] >= '0' && a[1] <= '9') {
            if (_head_parse_obsolete(a, opts) != 0) {
                head_efputs("head: invalid number '");
                head_fputs(a, stderr);
                head_efputs("'\n");
                goto fail;
            }
            continue;
        }

        /* short option cluster: -n5, -qv, -c 1k, ... */
        {
            size_t k;
            for (k = 1; a[k] != '\0'; k++) {
                const char * rest = (a[k + 1] != '\0') ? (a + k + 1) : NULL;
                int          r;
                bool         need_next = false;

                r = _head_parse_short(a[k], rest, opts, &need_next);
                if (r < 0) { goto fail; }
                if (r == 1) { goto done; }   /* --help / --version handled exit */
                if (need_next) {
                    /* -n / -c without an attached count: take next argv slot */
                    if (i + 1 >= argc) {
                        head_eprintf("head: option requires an argument -- '%c'\n",
                                     a[k]);
                        goto fail;
                    }
                    {
                        char         unit;
                        const char * val = argv[++i];
                        bool         allow_suffix;
                        unit = a[k];
                        if (unit == 'n') {
                            opts->unit = HEAD_UNIT_LINES;
                            allow_suffix = false;
                        }
                        else {
                            opts->unit = HEAD_UNIT_BYTES;
                            allow_suffix = true;
                        }
                        if (!_head_parse_count(val, allow_suffix,
                                               &opts->count, &opts->from_end)) {
                            head_efputs("head: invalid number '");
                            head_fputs(val, stderr);
                            head_efputs("'\n");
                            goto fail;
                        }
                        opts->have_count = true;
                    }
                    break;   /* rest of cluster consumed by the count */
                }
                if (rest) {
                    /* attached count (e.g. -n5) consumed the rest of the slot */
                    break;
                }
            }
            continue;
        }

file_operand:
        if (fn >= fcap) {
            int    ncap = (fcap == 0) ? 8 : fcap * 2;
            char **tmp  = (char **)realloc(flist, (size_t)ncap * sizeof(char *));
            if (!tmp) {
                head_efputs("head: out of memory\n");
                goto fail;
            }
            flist = tmp;
            fcap  = ncap;
        }
        flist[fn++] = (char *)a;
        continue;
    }

done:
    *files  = flist;
    *nfiles = fn;
    return 0;

fail:
    head_safe_free(flist);
    return -1;
}

/**
 * @brief Initialise a line buffer to empty (no allocation yet).
 */
static void _head_linebuf_init(head_linebuf_t * lb)
{
    if (!lb) { return; }
    lb->buf = NULL;
    lb->len = 0U;
    lb->cap = 0U;
}

/**
 * @brief Free a line buffer and reset it to empty.
 */
static void _head_linebuf_free(head_linebuf_t * lb)
{
    if (!lb) { return; }
    head_safe_free(lb->buf);
    lb->len = 0U;
    lb->cap = 0U;
}

/**
 * @brief Ensure @p extra more bytes are available in the line buffer.
 * @return true on success, false on allocation failure
 */
static bool _head_linebuf_reserve(head_linebuf_t * lb, size_t extra)
{
    size_t need;
    size_t ncap;
    char * tmp;

    if (!lb) { return false; }
    need = lb->len + extra + 1U;     /* +1 for a trailing NUL guard */
    if (need <= lb->cap) { return true; }

    ncap = (lb->cap == 0U) ? HEAD_LINE_INIT : lb->cap;
    while (ncap < need) {
        if (ncap > (SIZE_MAX / 2U)) { ncap = need; break; }
        ncap *= 2U;
    }
    tmp = (char *)realloc(lb->buf, ncap);
    if (!tmp) { return false; }
    lb->buf = tmp;
    lb->cap = ncap;
    return true;
}

/**
 * @brief Append @p n bytes to the line buffer.
 */
static bool _head_linebuf_append(head_linebuf_t * lb, const char * data, size_t n)
{
    if (!lb || (!data && n)) { return false; }
    if (n == 0U) { return true; }
    if (!_head_linebuf_reserve(lb, n)) { return false; }
    memcpy(lb->buf + lb->len, data, n);
    lb->len += n;
    lb->buf[lb->len] = '\0';
    return true;
}

/**
 * @brief Initialise an empty read scratch (no allocation yet).
 */
static void _head_scratch_init(head_scratch_t * sc)
{
    if (!sc) { return; }
    sc->buf   = NULL;
    sc->cap   = 0U;
    sc->start = 0U;
    sc->end   = 0U;
}

/**
 * @brief Release the scratch buffer (idempotent).
 */
static void _head_scratch_free(head_scratch_t * sc)
{
    if (!sc) { return; }
    head_safe_free(sc->buf);
    sc->cap   = 0U;
    sc->start = 0U;
    sc->end   = 0U;
}

/**
 * @brief Read one record (a line terminated by @p delim, or up to EOF).
 *
 * The terminator (if any) is NOT stored in the buffer. The caller can
 * re-append it when emitting.
 *
 * Uses @p sc to carry leftover bytes from the previous @c fread across
 * record boundaries. This avoids @c ungetc, which is only guaranteed to
 * push back a single byte and would silently drop multi-byte remainders
 * (causing head to lose lines after the first 64 KiB chunk).
 *
 * @param fp    input stream
 * @param lb    line buffer (initialised by caller); reset here
 * @param delim record delimiter ('\n' or '\0')
 * @param sc    persistent read scratch (allocated bytes carry over)
 * @param had_delim  set true if the record ended on a real delimiter,
 *                   false if it ended at EOF without one
 * @return 1 = a record was read (possibly without terminator at EOF),
 *         0 = clean EOF with no data, -1 = read error
 */
static int _head_read_record(FILE * fp, head_linebuf_t * lb, int delim,
                                head_scratch_t * sc, bool * had_delim)
{
    bool got = false;

    if (!fp || !lb || !sc || !had_delim) { return -1; }
    *had_delim = false;
    lb->len = 0U;
    if (lb->buf) { lb->buf[0] = '\0'; }

    for (;;) {
        /* refill scratch when fully consumed */
        if (sc->start >= sc->end) {
            if (sc->cap == 0U) {
                sc->cap   = HEAD_READ_CHUNK;
                sc->buf   = (char *)malloc(sc->cap);
                if (!sc->buf) { sc->cap = 0U; return -1; }
            }
            sc->start = 0U;
            sc->end   = fread(sc->buf, 1, sc->cap, fp);
            if (sc->end == 0U) {
                if (ferror(fp)) { return -1; }
                break;   /* clean EOF */
            }
        }

        /* scan the unprocessed region [start, end) for the delimiter */
        {
            size_t   i  = sc->start;
            size_t   lim = sc->end;
            unsigned char d = (unsigned char)delim;
            for (; i < lim; i++) {
                if ((unsigned char)sc->buf[i] == d) {
                    /* bytes [start, i) belong to this record */
                    if (i > sc->start) {
                        if (!_head_linebuf_append(lb, sc->buf + sc->start,
                                                     i - sc->start)) {
                            return -1;
                        }
                    }
                    /* advance past the delimiter; keep the remainder */
                    sc->start = i + 1U;
                    *had_delim = true;
                    return 1;
                }
            }
            /* no delimiter: consume the whole region and keep reading */
            if (!_head_linebuf_append(lb, sc->buf + sc->start,
                                         lim - sc->start)) {
                return -1;
            }
            sc->start = lim;
            got = true;
        }
    }

    return got ? 1 : 0;
}

/**
 * @brief Emit the first @p n lines (records) of @p fp.
 *
 * Streams directly without buffering the whole input.
 *
 * @return 0 on success, -1 on read error
 */
static int _head_first_lines(FILE * fp, unsigned long long n, int delim)
{
    head_linebuf_t   lb;
    head_scratch_t   sc;
    unsigned long long seen = 0ULL;

    if (!fp) { return -1; }
    _head_linebuf_init(&lb);
    _head_scratch_init(&sc);

    if (n == 0ULL) {
        _head_scratch_free(&sc);
        _head_linebuf_free(&lb);
        return 0;
    }

    while (seen < n) {
        bool had_delim = false;
        int  r = _head_read_record(fp, &lb, delim, &sc, &had_delim);
        if (r < 0) {
            _head_scratch_free(&sc);
            _head_linebuf_free(&lb);
            return -1;
        }
        if (r == 0) {
            break;   /* EOF */
        }
        if (lb.len > 0U) {
            (void)head_fwrite(lb.buf, 1, lb.len, head_out_stream);
        }
        if (had_delim) {
            head_putchar(delim);
        }
        seen++;
    }

    _head_scratch_free(&sc);
    _head_linebuf_free(&lb);
    return 0;
}

/**
 * @brief Emit the first @p n bytes of @p fp.
 *
 * @return 0 on success, -1 on read error
 */
static int _head_first_bytes(FILE * fp, unsigned long long n)
{
    char   buf[HEAD_READ_CHUNK];

    if (!fp) { return -1; }
    if (n == 0ULL) { return 0; }

    while (n > 0ULL) {
        size_t to_read = (n > (unsigned long long)sizeof(buf))
                         ? sizeof(buf) : (size_t)n;
        size_t got = fread(buf, 1, to_read, fp);
        if (got == 0U) {
            if (ferror(fp)) { return -1; }
            break;   /* EOF */
        }
        (void)head_fwrite(buf, 1, got, head_out_stream);
        n -= (unsigned long long)got;
    }
    return 0;
}

/**
 * @brief Emit all but the last @p n lines of @p fp (ring buffer of lines).
 *
 * A line is emitted only when @c n newer lines have already been read, so
 * at EOF the most recent @c n lines are discarded.  For @c n == 0 every
 * line is emitted.
 *
 * @return 0 on success, -1 on read error
 */
static int _head_rest_lines(FILE * fp, unsigned long long n, int delim)
{
    head_linebuf_t * ring = NULL;
    bool           * delim_seen = NULL;
    head_linebuf_t   lb;
    head_scratch_t   sc;
    unsigned long long i;
    unsigned long long count = 0ULL;
    unsigned long long idx   = 0ULL;

    if (!fp) { return -1; }
    if (n == 0ULL) {
        /* all but last 0 = everything */
        return _head_first_lines(fp, ULLONG_MAX, delim);
    }

    ring = (head_linebuf_t *)calloc((size_t)n, sizeof(head_linebuf_t));
    if (!ring) { return -1; }
    delim_seen = (bool *)calloc((size_t)n, sizeof(bool));
    if (!delim_seen) {
        free(ring);
        return -1;
    }
    _head_linebuf_init(&lb);
    _head_scratch_init(&sc);

    for (i = 0; i < n; i++) {
        _head_linebuf_init(&ring[i]);
    }

    for (;;) {
        bool had_delim = false;
        int  r = _head_read_record(fp, &lb, delim, &sc, &had_delim);
        if (r < 0) {
            int k;
            for (k = 0; (unsigned long long)k < n; k++) {
                _head_linebuf_free(&ring[k]);
            }
            free(delim_seen);
            free(ring);
            _head_scratch_free(&sc);
            _head_linebuf_free(&lb);
            return -1;
        }
        if (r == 0) {
            break;
        }

        if (count == n) {
            /* ring full: evict the oldest slot and emit it */
            head_linebuf_t * slot = &ring[idx];
            if (slot->len > 0U) {
                (void)head_fwrite(slot->buf, 1, slot->len, head_out_stream);
            }
            if (delim_seen[idx]) {
                head_putchar(delim);
            }
            _head_linebuf_free(slot);
        }
        else {
            count++;
        }

        /* move the freshly read line into the ring slot */
        {
            head_linebuf_t * slot = &ring[idx];
            *slot = lb;                 /* transfer ownership of lb.buf */
            _head_linebuf_init(&lb);    /* lb is now empty, ready for reuse */
            delim_seen[idx] = had_delim;
        }
        idx = (idx + 1ULL) % n;
    }

    /* discard the last `count` lines still buffered in the ring */
    {
        int k;
        for (k = 0; (unsigned long long)k < n; k++) {
            _head_linebuf_free(&ring[k]);
        }
    }
    free(delim_seen);
    free(ring);
    _head_scratch_free(&sc);
    _head_linebuf_free(&lb);
    return 0;
}

/**
 * @brief Emit all but the last @p n bytes of @p fp (byte ring buffer).
 *
 * A byte is emitted only when @c n newer bytes have already been read.
 * For @c n == 0 every byte is emitted.
 *
 * @return 0 on success, -1 on read error
 */
static int _head_rest_bytes(FILE * fp, unsigned long long n)
{
    char *  ring = NULL;
    char    buf[HEAD_READ_CHUNK];
    size_t  filled = 0U;     /* bytes currently in the ring (0..n) */
    unsigned long long wpos = 0ULL;   /* next ring slot to write */

    if (!fp) { return -1; }
    if (n == 0ULL) {
        return _head_first_bytes(fp, ULLONG_MAX);
    }
    if (n > (unsigned long long)SIZE_MAX) { n = (unsigned long long)SIZE_MAX; }

    ring = (char *)malloc((size_t)n);
    if (!ring) { return -1; }

    for (;;) {
        size_t to_read = sizeof(buf);
        size_t got = fread(buf, 1, to_read, fp);
        size_t i;
        if (got == 0U) {
            if (ferror(fp)) { free(ring); return -1; }
            break;
        }
        for (i = 0; i < got; i++) {
            if (filled == (size_t)n) {
                /* ring full: emit the byte being evicted */
                head_putchar((unsigned char)ring[wpos]);
            }
            else {
                filled++;
            }
            ring[wpos] = buf[i];
            wpos = (wpos + 1ULL) % n;
        }
    }

    free(ring);   /* last `filled` bytes are discarded */
    return 0;
}

/**
 * @brief Process a single input file (or stdin) according to the options.
 *
 * Opens the file (or stdin), prints the header if requested, and dispatches
 * to the appropriate emitter (first/rest, lines/bytes).
 *
 * @param opts         parsed options
 * @param name         file path; @c "-" or NULL means stdin
 * @param print_header if true, print the @c ==> name <== header first
 * @return 0 on success, 1 on open/read error
 */
static int _head_process_file(const head_opts_t * opts, const char * name,
                             bool print_header)
{
    FILE * fp     = NULL;
    bool   is_stdin;
    int    delim  = opts->zero_term ? '\0' : '\n';
    int    rc     = 0;

    if (!opts) { return 1; }

    is_stdin = (!name || name[0] == '-' || strcmp(name, "-") == 0);
    if (is_stdin) {
        fp = stdin;
    }
    else {
        fp = _head_fopen_utf8(name, "rb");
        if (!fp) {
            char        buf[1024];
            const char * emsg;
            snprintf(buf, sizeof(buf), "%s",
                     strerror(errno) ? strerror(errno) : "cannot open");
            emsg = buf;
            head_fputs("head: cannot open '", stderr);
            head_fputs(name, stderr);
            head_fputs("' for reading: ", stderr);
            head_fputs(emsg, stderr);
            head_fputs("\n", stderr);
            head_fflush(stderr);
            return 1;
        }
    }

    if (print_header) {
        head_fputs("==> ", head_out_stream);
        head_fputs(is_stdin ? "standard input" : name, head_out_stream);
        head_fputs(" <==\n", head_out_stream);
    }

    if (opts->from_end) {
        if (opts->unit == HEAD_UNIT_LINES) {
            rc = (_head_rest_lines(fp, opts->count, delim) == 0) ? 0 : 1;
        }
        else {
            rc = (_head_rest_bytes(fp, opts->count) == 0) ? 0 : 1;
        }
    }
    else {
        if (opts->unit == HEAD_UNIT_LINES) {
            rc = (_head_first_lines(fp, opts->count, delim) == 0) ? 0 : 1;
        }
        else {
            rc = (_head_first_bytes(fp, opts->count) == 0) ? 0 : 1;
        }
    }

    head_fflush(head_out_stream);

    if (!is_stdin && fp) {
        (void)fclose(fp);
    }
    return rc;
}

/**
 * @brief Process every file operand (or stdin when none given).
 *
 * Determines per-file header visibility (quiet / verbose / multi-file) and
 * aggregates a non-zero exit status when any file fails.
 *
 * @param opts   parsed options
 * @param files  file operands (may be NULL)
 * @param nfiles number of file operands
 * @return 0 if all files processed, 1 if any failed
 */
static int _head_run(const head_opts_t * opts, char ** files, int nfiles)
{
    int  rc      = 0;
    int  idx;
    bool multi   = (nfiles > 1);

    if (!opts) { return 1; }

    if (nfiles <= 0) {
        /* default to stdin, no header (unless -v) */
        bool ph = opts->verbose;
        if (_head_process_file(opts, "-", ph) != 0) { rc = 1; }
        return rc;
    }

    for (idx = 0; idx < nfiles; idx++) {
        const char * name = files[idx];
        bool is_stdin = (!name || strcmp(name, "-") == 0);
        bool print_header;

        (void)is_stdin;

        if (opts->quiet) {
            print_header = false;
        }
        else if (opts->verbose) {
            print_header = true;
        }
        else {
            print_header = multi;
        }

        if (_head_process_file(opts, name, print_header) != 0) {
            rc = 1;
        }
    }

    return rc;
}

#ifdef HEAD_PLATFORM_WINDOWS

/**
 * @brief Convert the host process command line to UTF-8 argv.
 *
 * The C runtime populates main's argv using the current ANSI code page,
 * which is typically CP936 on Chinese hosts, so any non-ASCII argument
 * is byte-mangled before main even sees it.  We reconstruct argv by
 * reading the real UTF-16 command line via GetCommandLineW() and
 * transcode every element to canonical UTF-8.
 *
 * On failure (OOM etc.) the function returns @c NULL and leaves
 * @p *pargc unchanged, so callers can fall back to the original argv.
 */
static char ** _head_argv_utf8_alloc(int * pargc, char ** argv_orig)
{
    LPWSTR * wargv;
    int      wargc;
    int      i;
    char **  u8a;

    (void)argv_orig;
    if (!pargc) { return NULL; }
    wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) { return NULL; }

    u8a = (char **)calloc((size_t)wargc + 1U, sizeof(char *));
    if (!u8a) {
        LocalFree(wargv);
        return NULL;
    }

    for (i = 0; i < wargc; i++) {
        int      wlen = (int)wcslen(wargv[i]);
        int      blen;
        char *   buf;

        if (wlen == 0) {
            u8a[i] = (char *)calloc(1U, sizeof(char));
            if (!u8a[i]) { break; }
            u8a[i][0] = '\0';
            continue;
        }
        blen = WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen,
                                   NULL, 0, NULL, NULL);
        if (blen <= 0) {
            u8a[i] = NULL;
            break;
        }
        buf = (char *)malloc((size_t)blen + 1U);
        if (!buf) {
            u8a[i] = NULL;
            break;
        }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen,
                            buf, blen, NULL, NULL);
        buf[blen] = '\0';
        u8a[i] = buf;
    }

    if (i < wargc) {
        /* rollback already-allocated slots */
        int j;
        for (j = 0; j < i; j++) { head_safe_free(u8a[j]); }
        head_safe_free(u8a);
        LocalFree(wargv);
        return NULL;
    }

    LocalFree(wargv);
    u8a[wargc] = NULL;
    *pargc = wargc;
    return u8a;
}

/**
 * @brief Release an argv vector produced by _head_argv_utf8_alloc.
 */
static void _head_argv_utf8_free(int argc, char ** argv_utf8)
{
    int i;
    if (!argv_utf8) { return; }
    for (i = 0; i < argc; i++) { head_safe_free(argv_utf8[i]); }
    head_safe_free(argv_utf8);
}

/**
 * @brief Map a libc fd (1/2) to its Windows standard handle.
 */
static HANDLE _head_std_handle_for_fd(int fd)
{
    if (fd == 1)      { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief True if @p fp is a real console stream (not a pipe/disk).
 */
static bool _head_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _head_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

/**
 * @brief Windows-aware byte emitter for stdout/stderr.
 *
 * Splits output by handle type:
 *   - console  : UTF-8 -> UTF-16LE -> WriteConsoleW (correct glyphs on CP936)
 *   - disk     : raw UTF-8 (byte-exact for @c head > file redirections)
 *   - pipe     : raw UTF-8 (byte-exact for PowerShell captures and native
 *                pipelines; modern hosts use Console.OutputEncoding=UTF-8
 *                and older CP936 scripts that capture output expect byte
 *                parity with disk redirections).
 * Non-stdio streams (temp buffers) use plain fwrite.
 */
static size_t _head_write_win32(const void * buf, size_t len, FILE * fp)
{
    int  fd;
    bool is_std;

    if (!fp) { return 0; }
    fd     = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /* Explicit temp streams etc. -> raw bytes. */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_head_is_console_stream(fp)) {
        int       wlen;
        wchar_t * wbuf;
        DWORD     written = 0;
        BOOL      ok;
        HANDLE    h = _head_std_handle_for_fd(fd);

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

    /* disk, pipe, and unknown stream types all emit raw UTF-8 bytes. */
    return fwrite(buf, 1, len, fp);
}

#endif  /* HEAD_PLATFORM_WINDOWS */
