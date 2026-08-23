/**
 * @file tail.c
 * @brief Cross-platform implementation of the Linux tail command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils tail(1).
 *
 * Key behaviors:
 *   - -n, --lines=[+]NUM       output the last NUM lines (default 10)
 *   - -c, --bytes=[+]NUM       output the last NUM bytes
 *   - NUM forms: +NUM (start at NUM), -NUM (last NUM)
 *   - byte suffixes: b(512) c(1) w(2) kB(1000) K/KB/KiB(1024)
 *                     MB(1e6) M/MiB(2^20) GB(1e9) G/GiB(2^30) ...
 *   - obsolete form: [-+]N[lbcfFkmqrz] (e.g. tail -5, tail -5c, tail +5l)
 *   - -f, --follow[={name|descriptor}]   output appended data as file grows
 *   - -F                       same as --follow=name --retry
 *   -      --pid=PID           with -f, terminate after process ID dies
 *   - -q, --quiet, --silent    never print file name headers
 *   -      --retry             keep trying to open an inaccessible file
 *   - -s, --sleep-interval=N   with -f, sleep ~N seconds between iterations
 *   - -v, --verbose            always print file name headers
 *   - -z, --zero-terminated    line delimiter is NUL, not newline
 *   - multiple files           print "==> name <==" header per file
 *   - --help                   display help and exit
 *   - --version                output version and exit
 *   - [FILE]...                input files (default: stdin; - means stdin)
 *
 * Platform <resource> sources:
 *   Linux:     stdio, stdlib, string, errno, unistd, sys/stat, sys/select
 *   Windows:   stdio, windows.h, shellapi.h, io.h, fcntl.h, sys/stat
 *   macOS:     stdio, stdlib, string, errno, unistd, sys/stat, sys/select
 *   FreeBSD:   stdio, stdlib, string, errno, unistd, sys/stat, sys/select
 *   OpenBSD:   stdio, stdlib, string, errno, unistd, sys/stat, sys/select
 *   NetBSD:    stdio, stdlib, string, errno, unistd, sys/stat, sys/select
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN -o tail.exe tail.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o tail tail.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -o tail tail.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o tail tail.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o tail tail.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -o tail tail.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/tail>
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
    #define TAIL_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TAIL_PLATFORM_LINUX   1
    #define TAIL_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TAIL_PLATFORM_MACOS   1
    #define TAIL_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TAIL_PLATFORM_FREEBSD 1
    #define TAIL_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TAIL_PLATFORM_OPENBSD 1
    #define TAIL_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TAIL_PLATFORM_NETBSD  1
    #define TAIL_PLATFORM_POSIX   1
#else
    #define TAIL_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef TAIL_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef TAIL_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef TAIL_PLATFORM_NETBSD
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
#include <stddef.h>

#ifdef TAIL_PLATFORM_WINDOWS
    #include <windows.h>
    #include <shellapi.h>
    #include <io.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/select.h>
    #include <signal.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Program version string (mirrors build banner). */
#define TAIL_VERSION_STR   "v1.0.0"

/** @brief Default number of lines printed when -n is not given. */
#define TAIL_DEFAULT_LINES 10ULL

/** @brief Chunk size (bytes) used by the streaming byte/line readers. */
#define TAIL_READ_CHUNK    65536U

/** @brief Block size (bytes) used by the seek-based reverse scanners. */
#define TAIL_SEEK_BLOCK    65536U

/** @brief Scratch buffer for tail_printf on Windows. */
#define TAIL_PRINTF_BUFSZ  2048U

/** @brief Max long-option name length handled by the manual parser. */
#define TAIL_OPT_NAME_MAX   64

/** @brief Default sleep (seconds) between follow iterations. */
#define TAIL_DEFAULT_SLEEP  1.0

/** @brief Initial capacity of a dynamic line buffer. */
#define TAIL_LINE_INIT      256U

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Output unit selector: count lines or count bytes.
 */
typedef enum {
    TAIL_UNIT_LINES = 0,   /**< -n : count is a number of lines (default)  */
    TAIL_UNIT_BYTES        /**< -c : count is a number of bytes            */
} tail_unit_t;

/**
 * @brief Follow-mode selector.
 */
typedef enum {
    TAIL_FOLLOW_NONE = 0,  /**< no following                                  */
    TAIL_FOLLOW_FD,        /**< -f / --follow=descriptor: track open fd        */
    TAIL_FOLLOW_NAME       /**< --follow=name / -F: track by file name         */
} tail_follow_t;

/**
 * @brief Parsed command-line options for one tail invocation.
 */
typedef struct {
    tail_unit_t         unit;           /**< lines vs bytes                       */
    unsigned long long  count;          /**< magnitude of the limit               */
    bool                from_start;     /**< true => +NUM (start at NUM)          */
    bool                quiet;          /**< -q : suppress file name headers       */
    bool                verbose;        /**< -v : force file name headers          */
    bool                zero_term;      /**< -z : NUL line delimiter               */
    bool                have_count;     /**< a -n / -c was given on the command line */
    tail_follow_t       follow;        /**< -f / -F / --follow                    */
    bool                retry;          /**< --retry : keep retrying open          */
    double              sleep_interval; /**< -s : seconds between follow iterations */
    unsigned long long  pid;           /**< --pid : die when this pid exits       */
    bool                have_pid;       /**< --pid was given                        */
} tail_opts_t;

/**
 * @brief Growable byte buffer used to hold one input record (line).
 */
typedef struct {
    char * buf;       /**< payload bytes (no trailing terminator kept)      */
    size_t len;       /**< number of valid bytes in @c buf                  */
    size_t cap;       /**< allocated capacity (bytes)                       */
} tail_linebuf_t;

/**
 * @brief Persistent read scratch carrying leftover bytes across records.
 *
 * Replaces a naive chunk scan that silently lost data when a single line
 * spanned more than one read chunk. The scratch keeps unprocessed bytes
 * from the last @c fread so record boundaries can be split safely.
 */
typedef struct {
    char * buf;       /**< raw read buffer (allocated once per file)        */
    size_t cap;       /**< capacity of @c buf (bytes)                        */
    size_t start;     /**< index of first unprocessed byte (inclusive)      */
    size_t end;       /**< index one past last valid byte (exclusive)       */
} tail_scratch_t;

/**
 * @brief Ring buffer of records for "last N lines" streaming.
 *
 * Each slot owns a heap-allocated byte buffer; push overwrites the oldest
 * slot so only the last @c cap records are retained.
 */
typedef struct {
    char ** entries;  /**< slot payload pointers                             */
    size_t *lens;     /**< slot payload lengths                              */
    size_t  cap;      /**< number of slots                                   */
    size_t  size;     /**< number of valid records (<= cap)                  */
    size_t  head;     /**< next push index (oldest when full)                */
} tail_ring_t;

/**
 * @brief Per-file follow context used by the follow loop in @c _tail_run.
 *
 * One context is allocated per followed file (or stdin).  In descriptor
 * follow mode (@c -f) the open FILE* is kept and drained each iteration.
 * In name follow mode (@c -F / @c --follow=name) the file is reopened when
 * it shrinks (truncation) or is absent (@c --retry keeps retrying).
 */
typedef struct {
    FILE *             fp;                /**< open handle (NULL if missing)     */
    const char *       name;              /**< file name (borrows from files[])   */
    tail_follow_t      mode;              /**< descriptor or name follow           */
    unsigned long long last_size;         /**< last known size (truncation check)   */
    bool               reported_missing;  /**< already warned about inaccessibility */
    bool               is_stdin;          /**< true if this context tracks stdin    */
} tail_follow_ctx_t;

/********************************
 *    static prototypes
 ********************************/

/* Diagnostics + option parsing */
static void   _tail_print_help(void);
static void   _tail_print_version(void);
static bool   _tail_parse_suffix(const char * s, unsigned long long * out);
static bool   _tail_parse_count(const char * s, bool allow_suffix,
                                unsigned long long * count, bool * from_start);
static bool   _tail_parse_ull(const char * s, unsigned long long * out);
static bool   _tail_parse_double(const char * s, double * out);
static int    _tail_set_follow_arg(const char * arg, tail_opts_t * opts);
static int    _tail_parse_obsolete_num(const char * arg, tail_opts_t * opts);
static int    _tail_parse_opts(int argc, char ** argv,
                               tail_opts_t * opts, char *** files, int * nfiles);

/* Line buffer helpers */
static void   _tail_linebuf_init(tail_linebuf_t * lb);
static void   _tail_linebuf_free(tail_linebuf_t * lb);
static bool   _tail_linebuf_reserve(tail_linebuf_t * lb, size_t extra);
static bool   _tail_linebuf_append(tail_linebuf_t * lb, const char * data, size_t n);

/* Read scratch (carries leftover bytes across record boundaries) */
static void   _tail_scratch_init(tail_scratch_t * sc);
static void   _tail_scratch_free(tail_scratch_t * sc);

/**
 * @brief Read one record (a line terminated by @p delim, or up to EOF).
 *
 * Uses @p sc to carry leftover bytes across calls (no ungetc, which is only
 * guaranteed for one byte). @p had_delim is set true when the record ended on
 * a real delimiter, false when it ended at EOF without one (last line).
 * @return 1 = record read, 0 = EOF (no data), -1 = error.
 */
static int    _tail_read_record(FILE * fp, tail_linebuf_t * lb, int delim,
                                tail_scratch_t * sc, bool * had_delim);

/* Ring buffer (last-N-records streaming) */
static int    _tail_ring_init(tail_ring_t * rb, size_t cap);
static void   _tail_ring_free(tail_ring_t * rb);
static int    _tail_ring_push(tail_ring_t * rb, const char * data, size_t len);
static void   _tail_ring_dump(const tail_ring_t * rb);

/* Streaming implementations (non-seekable / pipes / stdin) */
static int    _tail_last_bytes_stream(FILE * fp, unsigned long long n);
static int    _tail_last_lines_stream(FILE * fp, unsigned long long n, int delim);
static int    _tail_from_start_bytes(FILE * fp, unsigned long long byte_num_1);
static int    _tail_from_start_lines(FILE * fp, unsigned long long line_num_1, int delim);

/* Seek-based implementations (regular files) */
static int    _tail_last_bytes_seek(FILE * fp, unsigned long long n,
                                    unsigned long long filesz);
static int    _tail_last_lines_seek(FILE * fp, unsigned long long n, int delim,
                                    unsigned long long filesz);

/* Dispatch / run / follow */
static int    _tail_process_file(const tail_opts_t * opts, const char * name,
                                 bool print_header, bool is_first,
                                 tail_follow_t * out_follow_mode,
                                 FILE ** out_fp_for_follow,
                                 unsigned long long * out_last_size);
static int    _tail_run(const tail_opts_t * opts, char ** files, int nfiles);
static void   _tail_sleep_seconds(double s);
static bool   _tail_pid_alive(unsigned long long pid);
static int    _tail_drain_to_eof(FILE * fp);

/* Platform helpers */
static FILE * _tail_fopen_utf8(const char * path, const char * mode);
static bool   _tail_is_regular(FILE * fp, unsigned long long * out_size);
static void   _tail_print_file_header(const char * fname, bool * first_file);

#ifdef TAIL_PLATFORM_WINDOWS
static HANDLE _tail_std_handle_for_fd(int fd);
static bool   _tail_is_console_stream(FILE * fp);
static size_t _tail_write_win32(const void * buf, size_t len, FILE * fp);
static char **_tail_argv_utf8_alloc(int * argc, char ** argv);
static void   _tail_argv_utf8_free(int argc, char ** argv_utf8);
static void   _tail_console_set_utf8(void);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for tail_fwrite / tail_fputs / tail_fflush.
 *        Defaults to libc @c stdout .  Redefine externally to redirect.
 */
#ifndef tail_out_stream
    #define tail_out_stream stdout
#endif

#ifdef TAIL_PLATFORM_WINDOWS
/**
 * @brief Portable byte-write macro used by every text-output path.
 *        On Windows stdout/stderr are routed through @c _tail_write_win32
 *        which converts UTF-8 bytes to UTF-16LE via WriteConsoleW for real
 *        console streams, and writes raw UTF-8 for pipes/disk redirections.
 *        Any non-stdio stream uses plain fwrite so test output is
 *        byte-identical to GNU tail.
 * @sa _tail_write_win32
 */
    #ifndef tail_fwrite
        #define tail_fwrite(buf, sz, cnt, fp) \
            _tail_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef tail_fwrite
        #define tail_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print wrapper (printf-compatible).
 *
 * On Windows the formatted buffer is emitted through @c _tail_write_win32 so
 * CJK glyphs render correctly even on legacy console hosts.
 */
#ifndef tail_printf
    #ifdef TAIL_PLATFORM_WINDOWS
        #define tail_printf(fmt, ...) \
            do { \
                char _tailpf[TAIL_PRINTF_BUFSZ]; \
                int _tailpf_n = snprintf(_tailpf, sizeof(_tailpf), (fmt), ##__VA_ARGS__); \
                if (_tailpf_n > 0) { (void)_tail_write_win32(_tailpf, (size_t)_tailpf_n, tail_out_stream); } \
            } while (0)
    #else
        #define tail_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Formatted print to @c stderr (diagnostics).
 */
#ifndef tail_eprintf
    #ifdef TAIL_PLATFORM_WINDOWS
        #define tail_eprintf(fmt, ...) \
            do { \
                char _tailepf[TAIL_PRINTF_BUFSZ]; \
                int _tailepf_n = snprintf(_tailepf, sizeof(_tailepf), (fmt), ##__VA_ARGS__); \
                if (_tailepf_n > 0) { (void)_tail_write_win32(_tailepf, (size_t)_tailepf_n, stderr); } \
            } while (0)
    #else
        #define tail_eprintf(fmt, ...) fprintf(stderr, (fmt), ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef tail_fputs
    #define tail_fputs(str, stream) \
        do { const char * _tailsp = (str); if (_tailsp) { (void)tail_fwrite(_tailsp, 1, strlen(_tailsp), (stream)); } } while (0)
#endif

/**
 * @brief Write a single byte (cast via unsigned char to avoid UB with
 *        signed @c char on some compilers) to the default output stream.
 */
#ifndef tail_putchar
    #define tail_putchar(ch) \
        do { unsigned char _tailpc = (unsigned char)(ch); (void)tail_fwrite(&_tailpc, 1, 1, tail_out_stream); } while (0)
#endif

/**
 * @brief Flush the given stdio stream.
 */
#ifndef tail_fflush
    #define tail_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free: free(*p) and set the pointer to NULL.
 *        Callable when @p p itself is NULL (no-op).
 */
#ifndef tail_safe_free
    #define tail_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/* tail is stateless per invocation; no module-scoped state required. */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Program entry point.
 *
 * Parses the command line, applies Windows console setup, then hands off
 * to @c _tail_run which processes every input file (or stdin).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on any I/O or usage error
 */
int main(int argc, char ** argv)
{
    tail_opts_t opts;
    char **     files       = NULL;
    int         nfiles      = 0;
    int         rc;
#ifdef TAIL_PLATFORM_WINDOWS
    char **     argv_utf8   = NULL;
#endif

    memset(&opts, 0, sizeof(opts));
    opts.unit           = TAIL_UNIT_LINES;
    opts.count          = TAIL_DEFAULT_LINES;
    opts.from_start     = false;
    opts.zero_term      = false;
    opts.quiet          = false;
    opts.verbose        = false;
    opts.have_count     = false;
    opts.follow         = TAIL_FOLLOW_NONE;
    opts.retry          = false;
    opts.sleep_interval = TAIL_DEFAULT_SLEEP;
    opts.pid            = 0ULL;
    opts.have_pid       = false;

#ifdef TAIL_PLATFORM_WINDOWS
    /* Transcode argv from the C runtime codepage to UTF-8 so that error
     * messages, file headers, and fopen paths are byte-correct regardless
     * of the host's console configuration. */
    {
        int     wc  = 0;
        char ** u8a = _tail_argv_utf8_alloc(&wc, argv);
        if (u8a) {
            argc      = wc;
            argv_utf8 = u8a;
            argv      = u8a;
        }
    }
    /* Attach to parent console (if spawned detached) and request UTF-8 I/O.
     * Real per-glyph correctness on legacy console hosts is handled by
     * _tail_write_win32 / WriteConsoleW below. */
    (void)AttachConsole(ATTACH_PARENT_PROCESS);
    _tail_console_set_utf8();
    /* Binary mode prevents CRLF translation so byte-exact tests stay stable. */
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    rc = _tail_parse_opts(argc, argv, &opts, &files, &nfiles);
    if (rc != 0) {
        tail_safe_free(files);
#ifdef TAIL_PLATFORM_WINDOWS
        _tail_argv_utf8_free(argc, argv_utf8);
#endif
        return (rc < 0) ? 1 : rc;
    }

    rc = _tail_run(&opts, files, nfiles);
    tail_safe_free(files);
#ifdef TAIL_PLATFORM_WINDOWS
    _tail_argv_utf8_free(argc, argv_utf8);
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
static FILE * _tail_fopen_utf8(const char * path, const char * mode)
{
    if (!path || !mode) { return NULL; }

#ifdef TAIL_PLATFORM_WINDOWS
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

#ifdef TAIL_PLATFORM_WINDOWS
/**
 * @brief Map a CRT fd (0/1/2) to the corresponding Win32 standard handle.
 */
static HANDLE _tail_std_handle_for_fd(int fd)
{
    if (fd == 0)      { return GetStdHandle(STD_INPUT_HANDLE);  }
    else if (fd == 1) { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief Test whether @p fp is a real console (not a pipe/disk redirection).
 */
static bool _tail_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;
    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _tail_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

/**
 * @brief Write @p len bytes to @p fp with console/pipe/disk awareness.
 *
 * Real console streams: UTF-8 bytes are transcoded to UTF-16LE and written
 * via @c WriteConsoleW so CJK glyphs render correctly on every locale.
 * Pipes and disk redirections emit raw UTF-8 bytes (byte-exact vs GNU tail).
 * Non-stdio streams fall back to plain @c fwrite.
 */
static size_t _tail_write_win32(const void * buf, size_t len, FILE * fp)
{
    int  fd;
    bool is_std;

    if (!fp) { return 0; }
    if (len == 0) { return 0; }
    fd     = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /* Explicit temp streams etc. -> raw bytes. */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_tail_is_console_stream(fp)) {
        int       wlen;
        wchar_t * wbuf;
        DWORD     written = 0;
        BOOL      ok;
        HANDLE    h = _tail_std_handle_for_fd(fd);

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

/**
 * @brief Switch the console code page to UTF-8 (CP 65001) when a console is
 *        attached, so legacy hosts render UTF-8 correctly.
 */
static void _tail_console_set_utf8(void)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD  m   = 0;
    BOOL   anyConsole = FALSE;
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL &&
        GetConsoleMode(hOut, &m)) { anyConsole = TRUE; }
    if (!anyConsole && hErr != INVALID_HANDLE_VALUE && hErr != NULL &&
        GetConsoleMode(hErr, &m))  { anyConsole = TRUE; }
    if (anyConsole) {
        (void)SetConsoleOutputCP(CP_UTF8);
        (void)SetConsoleCP(CP_UTF8);
    }
}

/**
 * @brief Rebuild argv as UTF-8 from the raw Win32 command line.
 *
 * MSVCRT's narrow @c argv is decoded via the ANSI code page, which corrupts
 * non-ACP bytes. We use @c CommandLineToArgvW + @c WideCharToMultiByte so the
 * rest of the program sees proper UTF-8 strings.
 */
static char ** _tail_argv_utf8_alloc(int * pargc, char ** argv_orig)
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
    if (!u8a) { LocalFree(wargv); return NULL; }
    for (i = 0; i < wargc; i++) {
        int    wlen = (int)wcslen(wargv[i]);
        int    blen;
        char * buf;
        if (wlen == 0) {
            u8a[i] = (char *)calloc(1U, sizeof(char));
            if (!u8a[i]) { break; }
            u8a[i][0] = '\0';
            continue;
        }
        blen = WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen, NULL, 0, NULL, NULL);
        if (blen <= 0) { u8a[i] = NULL; break; }
        buf = (char *)malloc((size_t)blen + 1U);
        if (!buf) { u8a[i] = NULL; break; }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen, buf, blen, NULL, NULL);
        buf[blen] = '\0';
        u8a[i] = buf;
    }
    if (i < wargc) {
        for (int j = 0; j < i; j++) { free(u8a[j]); }
        free(u8a);
        LocalFree(wargv);
        return NULL;
    }
    LocalFree(wargv);
    u8a[wargc] = NULL;
    *pargc = wargc;
    return u8a;
}

/**
 * @brief Free an argv array previously produced by @c _tail_argv_utf8_alloc.
 */
static void _tail_argv_utf8_free(int argc, char ** argv_utf8)
{
    int i;
    if (!argv_utf8) { return; }
    for (i = 0; i < argc; i++) {
        free(argv_utf8[i]);
        argv_utf8[i] = NULL;
    }
    free(argv_utf8);
}
#endif  /* TAIL_PLATFORM_WINDOWS */

/**
 * @brief Print usage to stdout and exit with status 0 (GNU --help).
 */
static void _tail_print_help(void)
{
    tail_fputs(
        "Usage: tail [OPTION]... [FILE]...\n"
        "Print the last 10 lines of each FILE to standard output.\n"
        "With more than one FILE, precede each with a header giving the file name.\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -c, --bytes=[+]NUM       output the last NUM bytes; or use -c +NUM to\n"
        "                             output starting with byte NUM of each file\n"
        "  -f, --follow[={name|descriptor}]\n"
        "                           output appended data as the file grows;\n"
        "                             an absent option argument means 'descriptor'\n"
        "  -F                       same as --follow=name --retry\n"
        "  -n, --lines=[+]NUM       output the last NUM lines, instead of the last 10;\n"
        "                             or use -n +NUM to output starting with line NUM\n"
        "      --pid=PID            with -f, terminate after process ID, PID dies\n"
        "  -q, --quiet, --silent    never output headers giving file names\n"
        "      --retry              keep trying to open a file if it is inaccessible\n"
        "  -s, --sleep-interval=N   with -f, sleep for approximately N seconds\n"
        "                             (default 1.0) between iterations\n"
        "  -v, --verbose            always output headers giving file names\n"
        "  -z, --zero-terminated    line delimiter is NUL, not newline\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "NUM may have a multiplier suffix:\n"
        "b 512, c 1, w 2, kB 1000, K 1024, MB 1000000, M 1048576,\n"
        "GB 1000000000, G 1073741824, and so on for T, P, E, Z, Y.\n"
        "\n"
        "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n",
        tail_out_stream);
    exit(0);
}

/**
 * @brief Print version to stdout and exit with status 0 (GNU --version).
 */
static void _tail_print_version(void)
{
    tail_printf("tail %s\n", TAIL_VERSION_STR);
    tail_fputs("Copyright (C) 2025-2026 Yezc\n"
               "License MIT <https://mit-license.org/>.\n"
               "This is free software: you are free to change and redistribute it.\n"
               "There is NO WARRANTY, to the extent permitted by law.\n",
               tail_out_stream);
    exit(0);
}

/**
 * @brief Apply a GNU tail byte-multiplier suffix to a magnitude.
 *
 * Recognised suffixes (per GNU coreutils tail(1) manual):
 *   - @c b   => x512
 *   - @c c   => x1   (no scaling)
 *   - @c w   => x2
 *   - @c kB  => x1000
 *   - @c K / @c KB / @c KiB => x1024
 *   - @c MB  => x1e6, @c M / @c MiB => x2^20
 *   - @c GB  => x1e9, @c G / @c GiB => x2^30
 *   - @c TB  => x1e12, @c T / @c TiB => x2^40
 *   - @c PB  => x1e15, @c P / @c PiB => x2^50
 *   - @c EB  => x1e18, @c E / @c EiB => x2^60
 *   - @c ZB  => x1e21, @c Z / @c ZiB => x2^70 (clamped to ULLONG_MAX)
 *   - @c YB  => x1e24, @c Y / @c YiB => x2^80 (clamped to ULLONG_MAX)
 *
 * Binary multiples that overflow @c unsigned long long are clamped to
 * @c ULLONG_MAX so subsequent arithmetic stays well-defined.
 */
static bool _tail_parse_suffix(const char * s, unsigned long long * out)
{
    unsigned long long mul = 0ULL;
    unsigned long long v   = *out;
    if (!s || !*s) { return false; }
    if      (strcmp(s, "b") == 0)                          { mul = 512ULL; }
    else if (strcmp(s, "c") == 0)                         { mul = 1ULL; }
    else if (strcmp(s, "w") == 0)                         { mul = 2ULL; }
    else if (strcmp(s, "kB") == 0)                        { mul = 1000ULL; }
    else if (strcmp(s, "K") == 0  || strcmp(s, "KB") == 0
          || strcmp(s, "KiB") == 0)                       { mul = 1024ULL; }
    else if (strcmp(s, "MB") == 0)                        { mul = 1000000ULL; }
    else if (strcmp(s, "M") == 0  || strcmp(s, "MiB") == 0){ mul = 1048576ULL; }
    else if (strcmp(s, "GB") == 0)                        { mul = 1000000000ULL; }
    else if (strcmp(s, "G") == 0  || strcmp(s, "GiB") == 0){ mul = 1073741824ULL; }
    else if (strcmp(s, "TB") == 0)                        { mul = 1000000000000ULL; }
    else if (strcmp(s, "T") == 0  || strcmp(s, "TiB") == 0){ mul = 1099511627776ULL; }
    else if (strcmp(s, "PB") == 0)                        { mul = 1000000000000000ULL; }
    else if (strcmp(s, "P") == 0  || strcmp(s, "PiB") == 0){ mul = 1125899906842624ULL; }
    else if (strcmp(s, "EB") == 0)                        { mul = 1000000000000000000ULL; }
    else if (strcmp(s, "E") == 0  || strcmp(s, "EiB") == 0){ mul = ULLONG_MAX; }
    else if (strcmp(s, "ZB") == 0)                        { mul = ULLONG_MAX; }
    else if (strcmp(s, "Z") == 0  || strcmp(s, "ZiB") == 0){ mul = ULLONG_MAX; }
    else if (strcmp(s, "YB") == 0)                        { mul = ULLONG_MAX; }
    else if (strcmp(s, "Y") == 0  || strcmp(s, "YiB") == 0){ mul = ULLONG_MAX; }
    else { return false; }
    if (v != 0ULL) {
        if (mul > ULLONG_MAX / v) { *out = ULLONG_MAX; }
        else                      { *out = v * mul;    }
    }
    return true;
}

/**
 * @brief Parse a NUM ([+/-]NNN[suffix]) argument for -n / -c.
 *
 * @param s            input string
 * @param allow_suffix accept a byte-multiplier suffix (true for -c)
 * @param count        [out] parsed magnitude
 * @param from_start   [out] true when the value had a leading '+'
 * @return true on success
 */
static bool _tail_parse_count(const char * s, bool allow_suffix,
                              unsigned long long * count, bool * from_start)
{
    char *                   endp   = NULL;
    unsigned long long       parsed;
    unsigned long long       v;
    const char *             p;
    bool                     plus = false;
    if (!s || !*s) { return false; }
    p = s;
    if (*p == '+')      { p++; plus = true;  }
    else if (*p == '-') { p++; plus = false; }
    if (*p == '\0') { return false; }
    errno = 0;
    parsed = strtoull(p, &endp, 10);
    if (errno != 0 || endp == p) { return false; }
    v = parsed;
    if (allow_suffix && *endp != '\0') {
        if (!_tail_parse_suffix(endp, &v)) { return false; }
    }
    else if (*endp != '\0') { return false; }
    *count      = v;
    *from_start = plus;
    return true;
}

/**
 * @brief Parse a bare non-negative integer (no sign, no suffix).
 */
static bool _tail_parse_ull(const char * s, unsigned long long * out)
{
    char * endp = NULL;
    unsigned long long v;
    if (!s || !*s) { return false; }
    errno = 0;
    v = strtoull(s, &endp, 10);
    if (errno != 0 || endp == s || *endp != '\0') { return false; }
    *out = v;
    return true;
}

/**
 * @brief Parse a non-negative real (used by --sleep-interval).
 */
static bool _tail_parse_double(const char * s, double * out)
{
    char * endp = NULL;
    double v;
    if (!s || !*s) { return false; }
    errno = 0;
    v = strtod(s, &endp);
    if (errno != 0 || endp == s || *endp != '\0' || v < 0.0) { return false; }
    *out = v;
    return true;
}

/**
 * @brief Parse the GNU tail obsolete numeric form: [-+]N[lbcfkmqrz].
 *
 * Accepted suffix chars (a single line/unit selector, then flags):
 *   - @c l  => lines (default)
 *   - @c b  => bytes, x512
 *   - @c c  => bytes, x1
 *   - @c k  => bytes, x1024
 *   - @c m  => bytes, x1048576
 *   - @c q/@c Q, @c v/@c V, @c r/@c R, @c z/@c Z, @c f/@c F => flag toggles
 *
 * @return 0 on success, -1 if @p arg is not a valid obsolete form.
 */
static int _tail_parse_obsolete_num(const char * arg, tail_opts_t * opts)
{
    const char *           p = arg;
    bool                   plus = false;
    char *                 endp = NULL;
    unsigned long long     v;
    bool                   have_unit = false;
    tail_unit_t            unit = TAIL_UNIT_LINES;
    unsigned long long     mult = 1ULL;
    if (!arg || !*arg) { return -1; }
    if (*p == '+')      { plus = true;  p++; }
    else if (*p == '-') { p++; }
    if (!isdigit((unsigned char)*p)) { return -1; }
    errno = 0;
    v = strtoull(p, &endp, 10);
    if (errno != 0 || endp == p) { return -1; }
    while (*endp != '\0') {
        char c = *endp++;
        switch (c) {
            case 'l':
                if (have_unit) { return -1; }
                unit = TAIL_UNIT_LINES; have_unit = true; break;
            case 'c':
                if (have_unit) { return -1; }
                unit = TAIL_UNIT_BYTES; mult = 1ULL; have_unit = true; break;
            case 'b':
                if (have_unit) { return -1; }
                unit = TAIL_UNIT_BYTES; mult = 512ULL; have_unit = true; break;
            case 'k':
                if (have_unit) { return -1; }
                unit = TAIL_UNIT_BYTES; mult = 1024ULL; have_unit = true; break;
            case 'm':
                if (have_unit) { return -1; }
                unit = TAIL_UNIT_BYTES; mult = 1048576ULL; have_unit = true; break;
            case 'q': case 'Q': opts->quiet = true; opts->verbose = false; break;
            case 'v': case 'V': opts->verbose = true; opts->quiet = false; break;
            case 'r': case 'R': opts->retry = true; break;
            case 'z': case 'Z': opts->zero_term = true; break;
            case 'f': case 'F': opts->follow = TAIL_FOLLOW_FD; break;
            default: return -1;
        }
    }
    if (v != 0ULL && mult != 1ULL) {
        if (mult > ULLONG_MAX / v) { v = ULLONG_MAX; }
        else                       { v = v * mult; }
    }
    opts->unit       = unit;
    opts->count      = v;
    opts->from_start = plus;
    opts->have_count = true;
    return 0;
}

/**
 * @brief Apply the argument of @c --follow (descriptor|name|fd).
 */
static int _tail_set_follow_arg(const char * arg, tail_opts_t * opts)
{
    if (!arg || *arg == '\0') {
        opts->follow = TAIL_FOLLOW_FD;
        return 0;
    }
    if (strcmp(arg, "descriptor") == 0 || strcmp(arg, "fd") == 0) {
        opts->follow = TAIL_FOLLOW_FD;
        return 0;
    }
    if (strcmp(arg, "name") == 0) {
        opts->follow = TAIL_FOLLOW_NAME;
        return 0;
    }
    tail_eprintf("tail: invalid argument for --follow: '%s'\n", arg);
    tail_fputs("Try 'tail --help' for more information.\n", stderr);
    return -1;
}

/**
 * @brief Manual GNU-style option parser (no getopt dependency).
 *
 * Handles long options (@c --name / @c --name=value), short-option clusters
 * (@c -nfv), the GNU tail obsolete numeric form (@c -5 / @c +5l), and the
 * @c -- end-of-options separator. File operands are collected into @p *files.
 *
 * @return 0 on success, 1 if --help/--version was requested (and printed),
 *         -1 on any usage error.
 */
static int _tail_parse_opts(int argc, char ** argv,
                            tail_opts_t * opts, char *** files, int * nfiles)
{
    int    i        = 1;
    bool   end_opts = false;
    size_t nf       = 0;
    size_t nf_cap   = 0;
    char **out      = NULL;
    if (!opts || !files || !nfiles) { return -1; }
    *files  = NULL;
    *nfiles = 0;

#define TAIL_PUSH_FILE(fn) \
    do { \
        if (nf == nf_cap) { \
            size_t nc = nf_cap ? (nf_cap * 2U) : 4U; \
            char **nt = (char **)realloc(out, nc * sizeof(char *)); \
            if (!nt) { return -1; } \
            nf_cap = nc; \
            out = nt; \
        } \
        out[nf++] = (fn); \
    } while (0)

    for (; i < argc; i++) {
        const char * arg = argv[i];
        if (!arg) { continue; }

        if (!end_opts && strcmp(arg, "--") == 0) { end_opts = true; continue; }

        if (!end_opts && arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
            const char * eq   = strchr(arg, '=');
            size_t       nlen = eq ? (size_t)(eq - (arg + 2)) : strlen(arg + 2);
            char         name[TAIL_OPT_NAME_MAX];
            const char * val  = eq ? (eq + 1) : NULL;
            if (nlen >= sizeof(name)) { nlen = sizeof(name) - 1; }
            memcpy(name, arg + 2, nlen);
            name[nlen] = '\0';

            if (strcmp(name, "help") == 0)    { _tail_print_help();    return 1; }
            if (strcmp(name, "version") == 0) { _tail_print_version(); return 1; }
            if (strcmp(name, "lines") == 0) {
                const char * cnt;
                if (val) { cnt = val; }
                else if (i + 1 < argc) { cnt = argv[++i]; }
                else {
                    tail_eprintf("tail: option '--lines' requires an argument\n");
                    tail_fputs("Try 'tail --help' for more information.\n", stderr);
                    return -1;
                }
                if (!_tail_parse_count(cnt, false, &opts->count, &opts->from_start)) {
                    tail_eprintf("tail: invalid number of lines: '%s'\n", cnt);
                    return -1;
                }
                opts->unit = TAIL_UNIT_LINES;
                opts->have_count = true;
                continue;
            }
            if (strcmp(name, "bytes") == 0) {
                const char * cnt;
                if (val) { cnt = val; }
                else if (i + 1 < argc) { cnt = argv[++i]; }
                else {
                    tail_eprintf("tail: option '--bytes' requires an argument\n");
                    tail_fputs("Try 'tail --help' for more information.\n", stderr);
                    return -1;
                }
                if (!_tail_parse_count(cnt, true, &opts->count, &opts->from_start)) {
                    tail_eprintf("tail: invalid number of bytes: '%s'\n", cnt);
                    return -1;
                }
                opts->unit = TAIL_UNIT_BYTES;
                opts->have_count = true;
                continue;
            }
            if (strcmp(name, "follow") == 0) {
                if (_tail_set_follow_arg(val, opts) != 0) { return -1; }
                continue;
            }
            if (strcmp(name, "retry") == 0) { opts->retry = true; continue; }
            if (strcmp(name, "sleep-interval") == 0) {
                const char * sv;
                if (val) { sv = val; }
                else if (i + 1 < argc) { sv = argv[++i]; }
                else {
                    tail_eprintf("tail: option '--sleep-interval' requires an argument\n");
                    return -1;
                }
                if (!_tail_parse_double(sv, &opts->sleep_interval)) {
                    tail_eprintf("tail: invalid number of seconds: '%s'\n", sv);
                    return -1;
                }
                continue;
            }
            if (strcmp(name, "pid") == 0) {
                const char * pv;
                if (val) { pv = val; }
                else if (i + 1 < argc) { pv = argv[++i]; }
                else {
                    tail_eprintf("tail: option '--pid' requires an argument\n");
                    return -1;
                }
                if (!_tail_parse_ull(pv, &opts->pid)) {
                    tail_eprintf("tail: invalid process id: '%s'\n", pv);
                    return -1;
                }
                opts->have_pid = true;
                continue;
            }
            if (strcmp(name, "quiet") == 0 || strcmp(name, "silent") == 0) {
                opts->quiet = true; opts->verbose = false; continue;
            }
            if (strcmp(name, "verbose") == 0) {
                opts->verbose = true; opts->quiet = false; continue;
            }
            if (strcmp(name, "zero-terminated") == 0) {
                opts->zero_term = true; continue;
            }
            if (strcmp(name, "max-unchanged-stats") == 0
                || strcmp(name, "disable-inotify") == 0
                || strcmp(name, "presume-input-pipe") == 0) {
                if (!val && strcmp(name, "max-unchanged-stats") == 0) {
                    if (i + 1 < argc) { (void)argv[++i]; }
                }
                continue;
            }
            tail_eprintf("tail: unrecognized option '%s'\n", arg);
            tail_fputs("Try 'tail --help' for more information.\n", stderr);
            return -1;
        }

        /* Try obsolete GNU forms: [-+]N[clb...][qvrzfF] before short-option cluster */
        if (!end_opts
            && nf == 0
            && ((arg[0] == '+' && isdigit((unsigned char)arg[1]))
                || (arg[0] == '-' && isdigit((unsigned char)arg[1]))
                || isdigit((unsigned char)arg[0]))) {
            if (_tail_parse_obsolete_num(arg, opts) == 0) { continue; }
        }

        if (!end_opts && arg[0] == '-' && arg[1] != '\0') {
            int j = 1;
            while (arg[j] != '\0') {
                char c = arg[j];
                switch (c) {
                    case 'n':
                    case 'c': {
                        const char * rest = arg + j + 1;
                        const char * cnt;
                        bool         allow_suffix;
                        if (*rest != '\0') { cnt = rest; j = (int)strlen(arg); }
                        else if (i + 1 < argc) { cnt = argv[++i]; j = (int)strlen(arg); }
                        else {
                            tail_eprintf("tail: option requires an argument -- '%c'\n", c);
                            tail_fputs("Try 'tail --help' for more information.\n", stderr);
                            return -1;
                        }
                        allow_suffix = (c == 'c');
                        if (!_tail_parse_count(cnt, allow_suffix,
                                              &opts->count, &opts->from_start)) {
                            if (c == 'n') tail_eprintf("tail: invalid number of lines: '%s'\n", cnt);
                            else          tail_eprintf("tail: invalid number of bytes: '%s'\n", cnt);
                            return -1;
                        }
                        opts->unit = (c == 'n') ? TAIL_UNIT_LINES : TAIL_UNIT_BYTES;
                        opts->have_count = true;
                        break;
                    }
                    case 'q': opts->quiet = true; opts->verbose = false; j++; break;
                    case 'v': opts->verbose = true; opts->quiet = false; j++; break;
                    case 'z': opts->zero_term = true; j++; break;
                    case 'f': opts->follow = TAIL_FOLLOW_FD; j++; break;
                    case 'F': opts->follow = TAIL_FOLLOW_NAME; opts->retry = true; j++; break;
                    case 's': {
                        const char * rest = arg + j + 1;
                        const char * sv;
                        if (*rest != '\0') { sv = rest; j = (int)strlen(arg); }
                        else if (i + 1 < argc) { sv = argv[++i]; j = (int)strlen(arg); }
                        else {
                            tail_eprintf("tail: option requires an argument -- 's'\n");
                            return -1;
                        }
                        if (!_tail_parse_double(sv, &opts->sleep_interval)) {
                            tail_eprintf("tail: invalid number of seconds: '%s'\n", sv);
                            return -1;
                        }
                        break;
                    }
                    default:
                        tail_eprintf("tail: invalid option -- '%c'\n", c);
                        tail_fputs("Try 'tail --help' for more information.\n", stderr);
                        return -1;
                }
            }
            continue;
        }

        TAIL_PUSH_FILE(argv[i]);
    }
#undef TAIL_PUSH_FILE
    *files  = out;
    *nfiles = (int)nf;
    return 0;
}

/* --------- line buffer + scratch + record reader --------- */

/**
 * @brief Initialise an empty line buffer.
 */
static void _tail_linebuf_init(tail_linebuf_t * lb)
{
    if (!lb) { return; }
    lb->buf = NULL;
    lb->len = 0U;
    lb->cap = 0U;
}

/**
 * @brief Free a line buffer and reset it to empty.
 */
static void _tail_linebuf_free(tail_linebuf_t * lb)
{
    if (!lb) { return; }
    free(lb->buf);
    lb->buf = NULL;
    lb->len = 0U;
    lb->cap = 0U;
}

/**
 * @brief Ensure @p extra more bytes can be appended without reallocation.
 */
static bool _tail_linebuf_reserve(tail_linebuf_t * lb, size_t extra)
{
    size_t need;
    size_t nc;
    char * nb;
    if (!lb) { return false; }
    need = lb->len + extra;
    if (need <= lb->cap) { return true; }
    nc = lb->cap ? lb->cap : TAIL_LINE_INIT;
    while (nc < need) {
        if (nc > (SIZE_MAX / 2U)) { nc = need; break; }
        nc *= 2U;
    }
    nb = (char *)realloc(lb->buf, nc);
    if (!nb) {
        /* try exact-fit fallback before failing */
        nb = (char *)realloc(lb->buf, need);
        if (!nb) { return false; }
        nc = need;
    }
    lb->buf = nb;
    lb->cap = nc;
    return true;
}

/**
 * @brief Append @p n bytes from @p data to @p lb.
 */
static bool _tail_linebuf_append(tail_linebuf_t * lb, const char * data, size_t n)
{
    if (!lb || (!data && n > 0U)) { return false; }
    if (n == 0U) { return true; }
    if (!_tail_linebuf_reserve(lb, n)) { return false; }
    memcpy(lb->buf + lb->len, data, n);
    lb->len += n;
    return true;
}

/**
 * @brief Initialise an empty scratch buffer.
 */
static void _tail_scratch_init(tail_scratch_t * sc)
{
    if (!sc) { return; }
    sc->buf = NULL;
    sc->cap = 0U;
    sc->start = 0U;
    sc->end = 0U;
}

/**
 * @brief Free a scratch buffer and reset it.
 */
static void _tail_scratch_free(tail_scratch_t * sc)
{
    if (!sc) { return; }
    free(sc->buf);
    sc->buf = NULL;
    sc->cap = 0U;
    sc->start = 0U;
    sc->end = 0U;
}

/**
 * @brief Read one record (a line terminated by @p delim, or up to EOF).
 *
 * Uses @p sc to carry leftover bytes across calls (no @c ungetc, which is only
 * guaranteed for a single byte). @p had_delim is set true when the record
 * ended on a real delimiter, false when it ended at EOF without one. This
 * makes lines longer than @c TAIL_READ_CHUNK safe, fixing a data-loss bug
 * present in chunk-only scanners.
 *
 * @return 1 = record read, 0 = EOF (no data), -1 = error.
 */
static int _tail_read_record(FILE * fp, tail_linebuf_t * lb, int delim,
                              tail_scratch_t * sc, bool * had_delim)
{
    bool got = false;
    if (!fp || !lb || !sc || !had_delim) { return -1; }
    *had_delim = false;
    lb->len = 0U;
    if (lb->buf) { lb->buf[0] = '\0'; }
    for (;;) {
        if (sc->start >= sc->end) {
            if (!sc->buf) {
                sc->cap = TAIL_READ_CHUNK;
                sc->buf = (char *)malloc(sc->cap);
                if (!sc->buf) { sc->cap = 0U; return -1; }
            }
            sc->start = 0U;
            sc->end = fread(sc->buf, 1, sc->cap, fp);
            if (sc->end == 0U) { if (ferror(fp)) { return -1; } break; }
        }
        {
            /* Scan for delimiter in [start, end). */
            size_t           i   = sc->start;
            size_t           lim = sc->end;
            unsigned char    d   = (unsigned char)delim;
            bool             found = false;
            for (; i < lim; i++) {
                if ((unsigned char)sc->buf[i] == d) {
                    found = true;
                    break;
                }
            }
            if (found) {
                if (i > sc->start) {
                    if (!_tail_linebuf_append(lb, sc->buf + sc->start, i - sc->start)) {
                        return -1;
                    }
                }
                sc->start = i + 1U;
                *had_delim = true;
                return 1;
            }
            /* No delimiter: append the whole remainder and continue. */
            if (!_tail_linebuf_append(lb, sc->buf + sc->start, lim - sc->start)) {
                return -1;
            }
            sc->start = lim;
            got = true;
        }
    }
    return got ? 1 : 0;
}

/* --------- ring buffer for non-seekable streams --------- */

/**
 * @brief Allocate a ring buffer of @p cap record slots.
 */
static int _tail_ring_init(tail_ring_t * rb, size_t cap)
{
    if (!rb) { return -1; }
    if (cap == 0) { cap = 1; }
    rb->entries = (char **)calloc(cap, sizeof(char *));
    rb->lens    = (size_t *)calloc(cap, sizeof(size_t));
    if (!rb->entries || !rb->lens) {
        free(rb->entries);
        free(rb->lens);
        memset(rb, 0, sizeof(*rb));
        return -1;
    }
    rb->cap  = cap;
    rb->size = 0;
    rb->head = 0;
    return 0;
}

/**
 * @brief Free all slots and reset a ring buffer.
 */
static void _tail_ring_free(tail_ring_t * rb)
{
    size_t i;
    if (!rb) { return; }
    if (rb->entries) {
        for (i = 0; i < rb->cap; i++) { free(rb->entries[i]); rb->entries[i] = NULL; }
        free(rb->entries);
    }
    free(rb->lens);
    memset(rb, 0, sizeof(*rb));
}

/**
 * @brief Push a record into the ring, evicting the oldest when full.
 */
static int _tail_ring_push(tail_ring_t * rb, const char * data, size_t len)
{
    char * dup;
    size_t slot;
    if (!rb || !rb->entries) { return -1; }
    dup = (char *)malloc(len + 1U);
    if (!dup) { return -1; }
    if (len > 0U) { memcpy(dup, data, len); }
    dup[len] = '\0';
    slot = rb->head;
    free(rb->entries[slot]);
    rb->entries[slot] = dup;
    rb->lens[slot]    = len;
    rb->head = (rb->head + 1U) % rb->cap;
    if (rb->size < rb->cap) { rb->size++; }
    return 0;
}

/**
 * @brief Emit the retained records in order, oldest first.
 */
static void _tail_ring_dump(const tail_ring_t * rb)
{
    size_t start;
    size_t i;
    if (!rb || rb->size == 0U) { return; }
    if (rb->size < rb->cap) { start = 0U; }
    else                    { start = rb->head % rb->cap; }
    for (i = 0U; i < rb->size; i++) {
        size_t idx = (start + i) % rb->cap;
        if (rb->entries[idx] && rb->lens[idx] > 0U) {
            (void)tail_fwrite(rb->entries[idx], 1, rb->lens[idx], tail_out_stream);
        }
    }
}

/* ----------- streaming implementations ------------- */

/**
 * @brief Output the last @p n bytes of a non-seekable stream.
 *
 * Uses a rolling window of size @p n so memory is bounded by the request,
 * not by the stream length.
 */
static int _tail_last_bytes_stream(FILE * fp, unsigned long long n)
{
    size_t         cap;
    size_t         start;
    size_t         count;
    char *         buf;
    if (n == 0ULL) { return 0; }
    if (n > (unsigned long long)(SIZE_MAX - 64U)) { cap = SIZE_MAX - 64U; }
    else                                          { cap = (size_t)n; }
    buf = (char *)malloc(cap);
    if (!buf) { tail_eprintf("tail: out of memory\n"); return -1; }
    start = 0U;
    count = 0U;
    for (;;) {
        if (count < cap) {
            size_t rd = fread(buf + count, 1, cap - count, fp);
            if (rd == 0U) { if (ferror(fp)) { free(buf); return -1; } break; }
            count += rd;
        }
        else {
            unsigned char tmp[4096];
            size_t to_rd = sizeof(tmp);
            size_t rd = fread(tmp, 1, to_rd, fp);
            if (rd == 0U) { if (ferror(fp)) { free(buf); return -1; } break; }
            for (size_t k = 0; k < rd; k++) {
                buf[start] = (char)tmp[k];
                start = (start + 1U) % cap;
            }
        }
    }
    if (count < cap) {
        (void)tail_fwrite(buf, 1, count, tail_out_stream);
    }
    else if (start == 0U) {
        (void)tail_fwrite(buf, 1, cap, tail_out_stream);
    }
    else {
        (void)tail_fwrite(buf + start, 1, cap - start, tail_out_stream);
        (void)tail_fwrite(buf,       1, start,       tail_out_stream);
    }
    free(buf);
    return 0;
}

/**
 * @brief Output the last @p n lines (records) of a non-seekable stream.
 *
 * Uses the record reader + ring buffer so lines of arbitrary length are
 * handled correctly (no truncation at chunk boundaries).
 */
static int _tail_last_lines_stream(FILE * fp, unsigned long long n, int delim)
{
    tail_linebuf_t lb;
    tail_scratch_t  sc;
    tail_ring_t     rb;
    bool            had_delim = false;
    int             rc = 0;
    size_t          cap;
    if (n == 0ULL) { return 0; }
    cap = (n > (unsigned long long)(SIZE_MAX / 2U)) ? (SIZE_MAX / 2U) : (size_t)n;
    _tail_linebuf_init(&lb);
    _tail_scratch_init(&sc);
    if (_tail_ring_init(&rb, cap) != 0) {
        tail_eprintf("tail: out of memory\n");
        _tail_linebuf_free(&lb);
        _tail_scratch_free(&sc);
        return -1;
    }
    for (;;) {
        int r = _tail_read_record(fp, &lb, delim, &sc, &had_delim);
        if (r < 0) { rc = -1; break; }
        if (r == 0) { break; }
        /* Append the delimiter so the ring stores complete records and
         * _tail_ring_dump reproduces the original byte stream exactly. */
        if (had_delim) {
            char d = (char)delim;
            if (!_tail_linebuf_append(&lb, &d, 1)) { rc = -1; break; }
        }
        if (_tail_ring_push(&rb, lb.buf, lb.len) != 0) { rc = -1; break; }
    }
    if (rc == 0) { _tail_ring_dump(&rb); }
    _tail_ring_free(&rb);
    _tail_linebuf_free(&lb);
    _tail_scratch_free(&sc);
    return rc;
}

/**
 * @brief Output a regular file starting at byte @p byte_num_1 (1-based).
 *
 * @p byte_num_1 == 0 means "from the very first byte".
 */
static int _tail_from_start_bytes(FILE * fp, unsigned long long byte_num_1)
{
    unsigned long long skip = (byte_num_1 == 0ULL) ? 0ULL : byte_num_1 - 1ULL;
    char               buf[TAIL_READ_CHUNK];
    while (skip > 0ULL) {
        size_t want = (skip > (unsigned long long)sizeof(buf))
                        ? sizeof(buf) : (size_t)skip;
        size_t rd = fread(buf, 1, want, fp);
        if (rd == 0U) { return ferror(fp) ? -1 : 0; }
        skip -= (unsigned long long)rd;
    }
    for (;;) {
        size_t rd = fread(buf, 1, sizeof(buf), fp);
        if (rd == 0U) { return ferror(fp) ? -1 : 0; }
        (void)tail_fwrite(buf, 1, rd, tail_out_stream);
    }
}

/**
 * @brief Output a stream starting at line @p line_num_1 (1-based).
 *
 * Uses the record reader so lines longer than a single read chunk are kept
 * intact. @p seen counts complete lines seen *before* the current record;
 * once @c seen >= skip the current record and everything after is emitted.
 */
static int _tail_from_start_lines(FILE * fp, unsigned long long line_num_1, int delim)
{
    unsigned long long skip = (line_num_1 == 0ULL) ? 0ULL : line_num_1 - 1ULL;
    tail_linebuf_t     lb;
    tail_scratch_t     sc;
    bool               had_delim = false;
    unsigned long long seen     = 0ULL;
    bool               in_pass  = false;
    int                rc        = 0;
    _tail_linebuf_init(&lb);
    _tail_scratch_init(&sc);
    for (;;) {
        int r = _tail_read_record(fp, &lb, delim, &sc, &had_delim);
        if (r < 0) { rc = -1; break; }
        if (r == 0) { break; }
        if (in_pass) {
            if (lb.len > 0U) { (void)tail_fwrite(lb.buf, 1, lb.len, tail_out_stream); }
            if (had_delim)  { tail_putchar(delim); }
        }
        else if (seen >= skip) {
            if (lb.len > 0U) { (void)tail_fwrite(lb.buf, 1, lb.len, tail_out_stream); }
            if (had_delim)  { tail_putchar(delim); }
            in_pass = true;
        }
        if (had_delim) { seen++; }
    }
    _tail_linebuf_free(&lb);
    _tail_scratch_free(&sc);
    return rc;
}

/* ------------- seek-based implementations ------------ */

/**
 * @brief Output the last @p n bytes of a regular file of size @p filesz.
 *
 * Seeks directly to @c filesz-n, then streams forward. Falls back to the
 * streaming implementation when seeking fails.
 */
static int _tail_last_bytes_seek(FILE * fp, unsigned long long n,
                                 unsigned long long filesz)
{
    unsigned long long off;
    char               buf[TAIL_SEEK_BLOCK];
    unsigned long long want;
    size_t             rd;
    if (n == 0ULL) { return 0; }
    if (filesz < n) { n = filesz; }
    off = filesz - n;
    if (fseek(fp, (long)off, SEEK_SET) != 0) {
        if (fseek(fp, 0L, SEEK_SET) != 0) { return -1; }
        return _tail_last_bytes_stream(fp, n);
    }
    want = n;
    while (want > 0ULL) {
        size_t bsz = (want > (unsigned long long)sizeof(buf))
                        ? sizeof(buf) : (size_t)want;
        rd = fread(buf, 1, bsz, fp);
        if (rd == 0U) { return ferror(fp) ? -1 : 0; }
        (void)tail_fwrite(buf, 1, rd, tail_out_stream);
        want -= (unsigned long long)rd;
    }
    return 0;
}

/**
 * @brief Output the last @p n lines of a regular file of size @p filesz.
 *
 * Scans backward in @c TAIL_SEEK_BLOCK-sized blocks counting delimiters,
 * then seeks to the start of the Nth-from-last line and streams forward.
 * Handles the GNU tail rule: if the file does not end in a delimiter, the
 * final partial line counts as one line.
 */
static int _tail_last_lines_seek(FILE * fp, unsigned long long n, int delim,
                                 unsigned long long filesz)
{
    unsigned long long want_more;
    unsigned long long cursor;
    unsigned long long emit_from;
    char               block[TAIL_SEEK_BLOCK];
    bool               found_enough = false;
    bool               last_not_delim = false;
    if (n == 0ULL)     { return 0; }
    if (filesz == 0ULL){ return 0; }
    {
        unsigned char last_b;
        if (fseek(fp, (long)(filesz - 1ULL), SEEK_SET) != 0) { return -1; }
        if (fread(&last_b, 1, 1, fp) != 1) { return -1; }
        last_not_delim = (last_b != (unsigned char)delim);
    }
    want_more = n;
    if (last_not_delim) {
        if (want_more == 0ULL) { found_enough = true; }
        else                  { want_more--; }
    }
    cursor = filesz;
    emit_from = 0ULL;
    while (cursor > 0ULL && !found_enough) {
        size_t             blen;
        unsigned long long block_start;
        size_t             i;
        if (cursor >= (unsigned long long)TAIL_SEEK_BLOCK) {
            block_start = cursor - (unsigned long long)TAIL_SEEK_BLOCK;
            blen = TAIL_SEEK_BLOCK;
        }
        else {
            block_start = 0ULL;
            blen = (size_t)cursor;
        }
        if (fseek(fp, (long)block_start, SEEK_SET) != 0) { break; }
        if (fread(block, 1, blen, fp) != blen) { break; }
        for (i = blen; i > 0U; i--) {
            size_t idx = i - 1U;
            if ((unsigned char)block[idx] == (unsigned char)delim) {
                if (want_more == 0ULL) {
                    emit_from = block_start + (unsigned long long)idx + 1ULL;
                    found_enough = true;
                    break;
                }
                want_more--;
            }
        }
        cursor = block_start;
    }
    if (!found_enough) { emit_from = 0ULL; }
    if (fseek(fp, (long)emit_from, SEEK_SET) != 0) {
        if (fseek(fp, 0L, SEEK_SET) != 0) { return -1; }
        return _tail_last_lines_stream(fp, n, delim);
    }
    for (;;) {
        char   buf[TAIL_READ_CHUNK];
        size_t rd = fread(buf, 1, sizeof(buf), fp);
        if (rd == 0U) { return ferror(fp) ? -1 : 0; }
        (void)tail_fwrite(buf, 1, rd, tail_out_stream);
    }
}

/* ---------- dispatch / run ------------ */

/**
 * @brief Test whether @p fp is a regular file and report its size.
 *
 * On Windows uses @c GetFileType / @c GetFileSizeEx; on POSIX @c fstat.
 */
static bool _tail_is_regular(FILE * fp, unsigned long long * out_size)
{
#ifdef TAIL_PLATFORM_WINDOWS
    HANDLE h;
    DWORD  type;
    int    fd;
    LARGE_INTEGER sz;
#else
    int            fd;
    struct stat    st;
#endif
    if (!fp) { return false; }
    fd = fileno(fp);
    if (fd < 0) { return false; }
#ifdef TAIL_PLATFORM_WINDOWS
    h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { return false; }
    type = GetFileType(h);
    if (type != FILE_TYPE_DISK) { return false; }
    if (!GetFileSizeEx(h, &sz)) { return false; }
    if (sz.QuadPart < 0) { return false; }
    if (out_size) { *out_size = (unsigned long long)(sz.QuadPart); }
    return true;
#else
    if (fstat(fd, &st) != 0) { return false; }
    if (!S_ISREG(st.st_mode)) { return false; }
    if (st.st_size < 0) { return false; }
    if (out_size) { *out_size = (unsigned long long)(st.st_size); }
    return true;
#endif
}

/**
 * @brief Print the "==> name <==" header (used in multi-file / -v mode).
 */
static void _tail_print_file_header(const char * fname, bool * first_file)
{
    if (!fname) { return; }
    if (first_file && !*first_file) { tail_putchar('\n'); }
    tail_printf("==> %s <==\n",
                strcmp(fname, "-") == 0 ? "standard input" : fname);
    if (first_file) { *first_file = false; }
}

/**
 * @brief Drain all remaining bytes from @p fp to stdout (used by -f).
 */
static int _tail_drain_to_eof(FILE * fp)
{
    char   buf[TAIL_READ_CHUNK];
    size_t rd;
    if (!fp) { return -1; }
    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd == 0U) {
            if (feof(fp))   { clearerr(fp); return 0; }
            if (ferror(fp)) { return -1; }
            return 0;
        }
        (void)tail_fwrite(buf, 1, rd, tail_out_stream);
    }
}

/**
 * @brief Process one file: print its tail (and optional header).
 *
 * When following is requested and @p is_last is true, the still-open FILE*
 * is handed back through @p out_fp_for_follow for the follow loop to drain.
 */
static int _tail_process_file(const tail_opts_t * opts, const char * name,
                              bool print_header, bool is_first,
                              tail_follow_t * out_follow_mode,
                              FILE ** out_fp_for_follow,
                              unsigned long long * out_last_size)
{
    FILE *   fp;
    int      delim;
    int      rc = 0;
    unsigned long long filesz = 0ULL;
    bool     reg;
    bool     is_stdin;
    if (out_follow_mode)    { *out_follow_mode    = TAIL_FOLLOW_NONE; }
    if (out_fp_for_follow) { *out_fp_for_follow  = NULL; }
    if (out_last_size)      { *out_last_size      = 0ULL; }
    if (!opts || !name) { return -1; }
    is_stdin = (strcmp(name, "-") == 0);
    if (is_stdin) { fp = stdin; }
    else {
        fp = _tail_fopen_utf8(name, "rb");
        if (!fp) {
            tail_eprintf("tail: cannot open '%s' for reading: %s\n",
                         name, strerror(errno));
            return -1;
        }
    }
    if (print_header) { _tail_print_file_header(name, &is_first); }
    delim = opts->zero_term ? '\0' : '\n';
    reg   = _tail_is_regular(fp, &filesz);
    if (opts->from_start) {
        if (opts->unit == TAIL_UNIT_BYTES) { rc = _tail_from_start_bytes(fp, opts->count); }
        else                                { rc = _tail_from_start_lines(fp, opts->count, delim); }
    }
    else {
        if (opts->unit == TAIL_UNIT_BYTES) {
            if (reg && opts->count < ULLONG_MAX) {
                rc = _tail_last_bytes_seek(fp, opts->count, filesz);
            }
            else {
                rc = _tail_last_bytes_stream(fp, opts->count);
            }
        }
        else {
            if (reg) { rc = _tail_last_lines_seek(fp, opts->count, delim, filesz); }
            else     { rc = _tail_last_lines_stream(fp, opts->count, delim); }
        }
    }
    tail_fflush(tail_out_stream);
    if (opts->follow != TAIL_FOLLOW_NONE && rc == 0 && fp) {
        if (is_stdin) {
            /* stdin has no name to track; follow only by descriptor. */
            if (out_follow_mode)   { *out_follow_mode   = TAIL_FOLLOW_FD; }
            if (out_fp_for_follow) { *out_fp_for_follow = fp; }
            if (out_last_size)     { *out_last_size     = 0ULL; }
        }
        else {
            (void)fseek(fp, 0L, SEEK_END);
            if (out_follow_mode)   { *out_follow_mode   = opts->follow; }
            if (out_fp_for_follow) { *out_fp_for_follow = fp; }
            if (out_last_size) {
                unsigned long long sz = 0ULL;
                (void)_tail_is_regular(fp, &sz);
                *out_last_size = sz;
            }
        }
    }
    else {
        if (fp != stdin) { fclose(fp); }
    }
    return rc;
}

/**
 * @brief Return whether process @p pid is still alive.
 *
 * Used by @c --pid to terminate follow mode when the watched process exits.
 * @c pid == 0 means "no pid watch" and always reports alive.
 */
static bool _tail_pid_alive(unsigned long long pid)
{
    if (pid == 0ULL) { return true; }
#ifdef TAIL_PLATFORM_WINDOWS
    {
        HANDLE h;
        DWORD  dwPid = (pid > (unsigned long long)0xFFFFFFFFUL)
                        ? 0xFFFFFFFFUL : (DWORD)pid;
        if (dwPid == 0UL) { return true; }
        h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
        if (!h) { return false; }
        {
            DWORD ec = 0;
            BOOL ok = GetExitCodeProcess(h, &ec);
            CloseHandle(h);
            if (!ok) { return true; }
            return (ec == STILL_ACTIVE);
        }
    }
#else
    {
        pid_t p;
        if (pid > (unsigned long long)99999999ULL) { return true; }
        p = (pid_t)pid;
        if (kill(p, 0) == 0) { return true; }
        if (errno == EPERM)   { return true; }
        return false;
    }
#endif
}

/**
 * @brief Sleep for @p s seconds (sub-second resolution) in a portable way.
 */
static void _tail_sleep_seconds(double s)
{
    if (s <= 0.0) { return; }
#ifdef TAIL_PLATFORM_WINDOWS
    {
        DWORD ms;
        if (s >= (double)0xFFFFFFFFUL / 1000.0) { ms = 0xFFFFFFFFUL; }
        else                                    { ms = (DWORD)(s * 1000.0); }
        if (ms == 0U) { ms = 1U; }
        Sleep(ms);
    }
#else
    {
        struct timeval tv;
        tv.tv_sec  = (time_t)s;
        tv.tv_usec = (suseconds_t)((s - (double)tv.tv_sec) * 1.0e6);
        if (tv.tv_usec < 0) { tv.tv_usec = 0; }
        (void)select(0, NULL, NULL, NULL, &tv);
    }
#endif
}

/**
 * @brief Top-level driver: process every file, then optionally follow.
 *
 * Header policy mirrors GNU tail: -q suppresses headers, -v forces them,
 * otherwise headers appear only when more than one file is given.
 *
 * Follow: when -f/-F is set, ALL operands are followed (not just the last
 * one).  With @c --follow=name (or -F) each file is reopened when it shrinks
 * or rotates (truncation detection), and @c --retry keeps retrying a missing
 * file.  In descriptor mode (@c -f) the open FILE* is kept and drained each
 * iteration; the loop ends when every followed descriptor has been closed.
 */
static int _tail_run(const tail_opts_t * opts, char ** files, int nfiles)
{
    int                exit_code = 0;
    bool               need_header;
    bool               first_file_header = true;
    int                i;
    tail_follow_ctx_t *fctx      = NULL;
    int                nfollow   = 0;
    int                nactive   = 0;
    if (!opts) { return 1; }
    if (nfiles == 0)      { need_header = opts->verbose; }
    else if (opts->quiet) { need_header = false; }
    else if (opts->verbose) { need_header = true; }
    else                  { need_header = (nfiles > 1); }

    /* Allocate follow contexts (one per file, or one for stdin). */
    if (opts->follow != TAIL_FOLLOW_NONE) {
        nfollow = (nfiles == 0) ? 1 : nfiles;
        fctx = (tail_follow_ctx_t *)calloc((size_t)nfollow,
                                           sizeof(tail_follow_ctx_t));
        if (!fctx) {
            tail_eprintf("tail: out of memory\n");
            return 1;
        }
    }

    /* --- Process all files (or stdin), capturing follow info --- */
    if (nfiles == 0) {
        tail_follow_t     fm  = TAIL_FOLLOW_NONE;
        FILE *             ffp = NULL;
        unsigned long long sz  = 0ULL;
        int rc = _tail_process_file(opts, "-", need_header, true,
                                    fctx ? &fm : NULL,
                                    fctx ? &ffp : NULL,
                                    fctx ? &sz : NULL);
        if (rc != 0) { exit_code = 1; }
        if (fctx && fm != TAIL_FOLLOW_NONE && ffp) {
            fctx[0].fp       = ffp;
            fctx[0].name     = "-";
            fctx[0].mode     = TAIL_FOLLOW_FD;  /* stdin: descriptor only */
            fctx[0].last_size = 0ULL;
            fctx[0].is_stdin = true;
            nactive = 1;
        }
        if (nactive == 0) { free(fctx); return exit_code; }
    }
    else {
        for (i = 0; i < nfiles; i++) {
            const char *       name = files[i];
            tail_follow_t      fm   = TAIL_FOLLOW_NONE;
            FILE *             ffp  = NULL;
            unsigned long long sz   = 0ULL;
            int rc = _tail_process_file(opts, name, need_header,
                                        first_file_header,
                                        fctx ? &fm : NULL,
                                        fctx ? &ffp : NULL,
                                        fctx ? &sz : NULL);
            if (rc != 0) { exit_code = 1; }
            if (need_header) { first_file_header = false; }

            if (fctx && fm != TAIL_FOLLOW_NONE && ffp) {
                fctx[nactive].fp        = ffp;
                fctx[nactive].name      = name;
                fctx[nactive].mode      = fm;
                fctx[nactive].last_size = sz;
                fctx[nactive].is_stdin  = (strcmp(name, "-") == 0);
                nactive++;
            }
            else if (fctx && opts->follow == TAIL_FOLLOW_NAME
                     && opts->retry && strcmp(name, "-") != 0) {
                /* File not opened but --retry was requested: track it. */
                fctx[nactive].fp               = NULL;
                fctx[nactive].name             = name;
                fctx[nactive].mode             = TAIL_FOLLOW_NAME;
                fctx[nactive].last_size        = 0ULL;
                fctx[nactive].reported_missing = true;
                fctx[nactive].is_stdin         = false;
                nactive++;
            }
        }
        if (nactive == 0) { free(fctx); return exit_code; }
    }

    /* --- Follow loop: drain all tracked files each iteration --- */
    for (;;) {
        bool any_open = false;
        if (opts->have_pid && !_tail_pid_alive(opts->pid)) { break; }

        for (i = 0; i < nactive; i++) {
            tail_follow_ctx_t * fc = &fctx[i];

            if (fc->is_stdin) {
                /* stdin: descriptor-only, just drain. */
                if (fc->fp) {
                    int rc2 = _tail_drain_to_eof(fc->fp);
                    tail_fflush(tail_out_stream);
                    if (rc2 != 0) { exit_code = 1; }
                    any_open = true;
                }
                continue;
            }

            /* Name follow: detect truncation/rotation and reopen. */
            if (fc->mode == TAIL_FOLLOW_NAME) {
                bool should_reopen = false;
                if (!fc->fp) {
                    should_reopen = true;
                }
                else {
                    unsigned long long sz = 0ULL;
                    if (_tail_is_regular(fc->fp, &sz) && sz < fc->last_size) {
                        should_reopen = true;
                    }
                }
                if (should_reopen) {
                    FILE * np;
                    if (fc->fp) { fclose(fc->fp); fc->fp = NULL; }
                    np = _tail_fopen_utf8(fc->name, "rb");
                    if (np) {
                        fc->fp = np;
                        if (fc->reported_missing) {
                            tail_eprintf("tail: '%s' has appeared;  "
                                         "following new file\n", fc->name);
                            fc->reported_missing = false;
                        }
                        /* Start from beginning: drain existing content. */
                        (void)fseek(fc->fp, 0L, SEEK_SET);
                        fc->last_size = 0ULL;
                        (void)_tail_is_regular(fc->fp, &fc->last_size);
                    }
                    else if (opts->retry) {
                        if (!fc->reported_missing) {
                            tail_eprintf("tail: cannot open '%s' for "
                                         "reading: %s\n",
                                         fc->name, strerror(errno));
                            fc->reported_missing = true;
                        }
                        any_open = true;
                    }
                    /* Without --retry, stop tracking this file (fp stays NULL). */
                }
            }

            /* Drain any newly-appended bytes. */
            if (fc->fp) {
                int rc2 = _tail_drain_to_eof(fc->fp);
                tail_fflush(tail_out_stream);
                {
                    unsigned long long sz = 0ULL;
                    if (_tail_is_regular(fc->fp, &sz)) {
                        fc->last_size = sz;
                    }
                }
                if (rc2 != 0) { exit_code = 1; }
                any_open = true;
            }
        }

        /* In descriptor mode, stop when every file has been closed. */
        if (!any_open) { break; }
        _tail_sleep_seconds(opts->sleep_interval);
    }

    /* Close all still-open followed files (skip stdin). */
    for (i = 0; i < nactive; i++) {
        if (fctx[i].fp && !fctx[i].is_stdin) {
            fclose(fctx[i].fp);
            fctx[i].fp = NULL;
        }
    }
    free(fctx);
    return exit_code;
}
