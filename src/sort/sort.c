/**
 * @file sort.c
 * @brief Cross-platform implementation of the Linux sort command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils sort(1).
 *
 * Key behaviors:
 *   - -b, --ignore-leading-blanks   ignore leading blanks
 *   - -d, --dictionary-order        only blanks and alphanumeric
 *   - -f, --ignore-case             fold lower to upper case
 *   - -g, --general-numeric-sort    compare by general numeric value
 *   - -h, --human-numeric-sort      compare by human readable size
 *   - -i, --ignore-nonprinting      only printable and blanks
 *   - -M, --month-sort              compare JAN < ... < DEC
 *   - -n, --numeric-sort            compare by string numeric value
 *   - -R, --random-sort             sort by random hash of keys
 *   - -r, --reverse                 reverse the result
 *   - -V, --version-sort            natural version number sort
 *   -       --sort=WORD             set sort type via word
 *   - -c, --check                   check for sorted input
 *   - -C, --check=silent            like -c but don't report
 *   - -k, --key=KEYDEF              sort by key definition
 *   - -m, --merge                   merge already sorted files
 *   - -o, --output=FILE             output to FILE
 *   - -s, --stable                  stabilize sort (disable last-resort compare)
 *   - -t, --field-separator=SEP     use SEP as field separator
 *   - -u, --unique                  only output unique lines
 *   - -z, --zero-terminated         line delimiter is NUL
 *   - -T, --temporary-directory=DIR temp directory (accepted, in-memory)
 *   - -S, --buffer-size=SIZE        buffer size (accepted, in-memory)
 *   -       --batch-size=SIZE       merge batch (accepted, in-memory)
 *   -       --files0-from=F         read NUL-terminated file list from F
 *   -       --compress-program=PROG compression program (accepted, ignored)
 *   -       --parallel=N            parallel threads (accepted, ignored)
 *   -       --random-source=FILE    random data source (accepted, ignored)
 *   -       --debug                 annotate sort keys and warn to stderr
 *   -       --help                  display help and exit
 *   -       --version               output version and exit
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o sort.exe sort.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o sort sort.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o sort sort.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o sort sort.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o sort sort.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o sort sort.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/sort>
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
    #define SORT_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define SORT_PLATFORM_LINUX   1
    #define SORT_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define SORT_PLATFORM_MACOS   1
    #define SORT_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define SORT_PLATFORM_FREEBSD 1
    #define SORT_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define SORT_PLATFORM_OPENBSD 1
    #define SORT_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define SORT_PLATFORM_NETBSD  1
    #define SORT_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define SORT_PLATFORM_POSIX   1
#else
    #define SORT_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef SORT_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef SORT_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef SORT_PLATFORM_NETBSD
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
#include <locale.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#ifdef SORT_PLATFORM_WINDOWS
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
#define SORT_VERSION_STR "v1.0.0"

/** @brief Initial size (bytes) of the dynamic per-line read buffer */
#define SORT_LINE_BUF_INIT  256U

/** @brief Upper bound on the number of files read via --files0-from=F */
#define SORT_MAX_FILES0     4096U


/********************************
 *    typedefs
 ********************************/

/**
 * @brief Key specification (one `--key=KEYDEF` entry).
 *
 * Each of the boolean option fields has a matching @c set_* bitfield
 * that records whether the option was explicitly attached to this key
 * by KEYDEF modifiers. When @c set_* is zero, the corresponding global
 * option from sort_opts_t is inherited.
 */
typedef struct {
    int start_field;              /**< 1-based starting field (0 = invalid) */
    int start_char;               /**< 1-based char offset within start_field */
    int end_field;                /**< 1-based ending field (-1 = rest of line) */
    int end_char;                 /**< 1-based char offset within end_field (0 = end of field) */
    int ignore_blanks;            /**< -b modifier */
    int dictionary;               /**< -d modifier */
    int ignore_case;              /**< -f modifier */
    int general_numeric;          /**< -g modifier */
    int human_numeric;            /**< -h modifier */
    int month;                    /**< -M modifier */
    int ignore_nonprinting;       /**< -i modifier */
    int numeric;                  /**< -n modifier */
    int random;                   /**< -R modifier */
    int version;                  /**< -V modifier */
    int reverse;                  /**< -r modifier */
    unsigned set_ignore_blanks      : 1;  /**< true if this key overrides global -b */
    unsigned set_dictionary         : 1;  /**< true if this key overrides global -d */
    unsigned set_ignore_case        : 1;  /**< true if this key overrides global -f */
    unsigned set_general_numeric    : 1;  /**< true if this key overrides global -g */
    unsigned set_human_numeric      : 1;  /**< true if this key overrides global -h */
    unsigned set_month              : 1;  /**< true if this key overrides global -M */
    unsigned set_ignore_nonprinting : 1;  /**< true if this key overrides global -i */
    unsigned set_numeric            : 1;  /**< true if this key overrides global -n */
    unsigned set_random             : 1;  /**< true if this key overrides global -R */
    unsigned set_version            : 1;  /**< true if this key overrides global -V */
    unsigned set_reverse            : 1;  /**< true if this key overrides global -r */
} key_spec_t;

/**
 * @brief Global sort options / option state.
 */
typedef struct {
    int ignore_blanks;          /**< -b */
    int dictionary;             /**< -d */
    int ignore_case;            /**< -f */
    int general_numeric;        /**< -g */
    int human_numeric;          /**< -h */
    int month;                  /**< -M */
    int ignore_nonprinting;     /**< -i */
    int numeric;                /**< -n */
    int random;                 /**< -R */
    int version;                /**< -V */
    int reverse;                /**< -r */
    int stable;                 /**< -s */
    int unique;                 /**< -u */
    int check;                  /**< 0 = off, 1 = -c, 2 = -C silent */
    int merge;                  /**< -m */
    int zero_terminated;        /**< -z */
    int debug;                  /**< --debug */
    char field_sep;             /**< -t SEP ('\0' means default: whitespace) */
    const char * output_file;   /**< -o FILE */
    const char * files0_from;   /**< --files0-from=F */
    key_spec_t * keys;          /**< dynamic array of key specifications */
    int nkeys;                  /**< number of entries in keys[] */
} sort_opts_t;

/**
 * @brief One input line stored in memory.
 */
typedef struct {
    char * text;                /**< NUL-terminated line content (no terminator char) */
    size_t len;                 /**< length of text[] in bytes (excludes trailing NUL) */
    size_t index;               /**< original input index, used for stable sort tie-break */
} sort_line_t;

/**
 * @brief Dynamic array of sort_line_t entries.
 */
typedef struct {
    sort_line_t * lines;        /**< heap-allocated array of lines */
    size_t count;               /**< number of used entries */
    size_t capacity;            /**< allocated capacity of lines[] */
} line_array_t;

/********************************
 *    static prototypes
 ********************************/
static void         _sort_usage(void);
static void         _sort_version(void);
static void         _sort_permute_argv(int argc, char ** argv, int * file_start);
static int          _sort_parse_opts(int argc, char ** argv, sort_opts_t * opts, int * file_start);
static int          _sort_parse_keydef(const char * spec, key_spec_t * key);
static int          _sort_add_key(sort_opts_t * opts, const key_spec_t * key);
static int          _sort_add_line(line_array_t * arr, const char * text, size_t len);
static int          _sort_read_file(const char * filename, sort_opts_t * opts, line_array_t * arr);
static int          _sort_read_files0_from(const char * src, sort_opts_t * opts, line_array_t * arr);
static const char * _sort_find_field(const char * line, size_t len, int field, char sep, size_t * field_len);
static void         _sort_extract_key(const char * line, size_t len, const key_spec_t * key,
                                      const sort_opts_t * global, char sep,
                                      const char ** start, size_t * key_len,
                                      int * eff_ignore_blanks);
static unsigned long _sort_hash(const char * str, size_t len);
static int          _sort_str_cmp(const char * a, size_t alen, const char * b, size_t blen,
                                  int dictionary, int ignore_nonprinting, int fold_case);
static int          _sort_numeric_cmp(const char * a, size_t alen, const char * b, size_t blen);
static int          _sort_human_cmp(const char * a, size_t alen, const char * b, size_t blen);
static int          _sort_month_cmp(const char * a, size_t alen, const char * b, size_t blen);
static int          _sort_version_cmp(const char * a, size_t alen, const char * b, size_t blen);
static void         _sort_resolve_key_opts(const key_spec_t * key, const sort_opts_t * global,
                                           int * ib, int * dic, int * ic,
                                           int * gn, int * hn, int * mo,
                                           int * inp, int * num, int * rnd,
                                           int * ver, int * rev);
static int          _sort_cmp_values(const char * a, size_t alen, const char * b, size_t blen,
                                     int dictionary, int ignore_nonprinting, int fold_case,
                                     int general_numeric, int human_numeric, int month,
                                     int numeric, int random, int version);
static int          _sort_cmp_by_key(const sort_line_t * la, const sort_line_t * lb,
                                     const key_spec_t * key, const sort_opts_t * opts);
static int          _sort_compare(const sort_line_t * la, const sort_line_t * lb,
                                  const sort_opts_t * opts, int * last_resort_used);
static int          _sort_qsort_cmp(const void * a, const void * b);
static void         _sort_merge_k(line_array_t * arr, const sort_opts_t * opts,
                                  size_t * file_offsets, int nfiles);
static int          _sort_check(const line_array_t * arr, const sort_opts_t * opts);
static int          _sort_output(const line_array_t * arr, const sort_opts_t * opts);
static void         _sort_free_lines(line_array_t * arr);
static void         _sort_free_opts(sort_opts_t * opts);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for sort_fputs / sort_fflush.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef sort_out_stream
    #define sort_out_stream stdout
#endif

#ifdef SORT_PLATFORM_WINDOWS
/**
 * @brief On Windows, decide whether a FILE * is directly attached to a
 *        console screen buffer (as opposed to a pipe / file redirection).
 *        Used to select between WriteConsoleW (UTF-16) and raw byte
 *        output, because the legacy console host does not honour
 *        UTF-8 bytes reliably even after SetConsoleOutputCP(65001).
 * @param fp  stdio stream (typically stdout or stderr).
 * @return true when the underlying handle is a console character device.
 */
/**
 * @brief On Windows, return the raw kernel handle for the standard
 *        streams stdout (fd == 1) and stderr (fd == 2).
 * @param fd  _fileno() result (must be 1 or 2).
 * @return HANDLE value; caller must test for INVALID_HANDLE_VALUE / NULL.
 */
static HANDLE _sort_std_handle_for_fd(int fd)
{
    if (fd == 1)      { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief On Windows, decide whether a FILE * is directly attached to a
 *        console screen buffer (as opposed to a pipe / file redirection).
 *        Used to select between WriteConsoleW (UTF-16) and the
 *        code-page-aware pipe emitter — see @b _sort_write_win32.
 */
static bool _sort_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;

    if (fp == NULL) { return false; }
    fd = _fileno(fp);
    h  = _sort_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

/**
 * @brief On Windows, decide whether a stdout / stderr FILE * is currently
 *        redirected to a disk file (e.g. @c sort a.txt > out.txt).
 *        Disk redirections must receive byte-for-byte identical UTF-8
 *        output so cross-platform tests stay reproducible.
 */
static bool _sort_is_disk_stream(FILE * fp)
{
    HANDLE h;
    int    fd;

    if (fp == NULL) { return false; }
    fd = _fileno(fp);
    h  = _sort_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) { return false; }
    return (GetFileType(h) == FILE_TYPE_DISK);
}

/**
 * @brief On Windows, decide whether a stdout / stderr FILE * is an
 *        anonymous or named pipe — which happens every time PowerShell
 *        5.x spawns a native command: it creates a pipe to capture the
 *        bytes and decodes them through [Console]::OutputEncoding.
 *        For this case we transcode our internal UTF-8 to the current
 *        console output code page (e.g. 936 on SC Windows) so PS5's
 *        decoder matches our emitter (fixes mojibake like 鐨勮娉曟槸
 *        when the user's default CP is not 65001).
 */
static bool _sort_is_pipe_stream(FILE * fp)
{
    HANDLE h;
    int    fd;

    if (fp == NULL) { return false; }
    fd = _fileno(fp);
    h  = _sort_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) { return false; }
    return (GetFileType(h) == FILE_TYPE_PIPE);
}

/**
 * @brief On Windows, return the console output code page to use for
 *        pipe-transcode.  If the attached console cannot supply one we
 *        fall back to the system ANSI code page so CJK glyphs still
 *        decode correctly on non-65001 default shells.
 */
static UINT _sort_output_codepage(void)
{
    UINT cp = GetConsoleOutputCP();
    if (cp == 0U) { cp = GetACP(); }
    if (cp == 0U) { cp = 65001U; }
    return cp;
}

/**
 * @brief On Windows, write @p len UTF-8 bytes at @p buf to the pipe
 *        handle underlying @p fp, first transcoding to the console's
 *        current output code page.  This keeps PowerShell 5.x's pipe
 *        decoder (which uses [Console]::OutputEncoding === CodePage)
 *        synchronised with us, fixing Chinese mojibake on default
 *        Simplified-Chinese Windows installs.
 *
 * @return @p len on success, smaller value on partial failure (matches
 *         the contract expected by sort_fwrite callers).
 */
static size_t _sort_write_pipe_cp(const void * buf, size_t len, FILE * fp)
{
    UINT     cp   = _sort_output_codepage();
    int      fd   = _fileno(fp);
    HANDLE   h    = _sort_std_handle_for_fd(fd);
    int      wlen;
    wchar_t * wbuf   = NULL;
    int      blen;
    char    * obuf   = NULL;
    DWORD    written = 0;
    BOOL     ok;
    size_t   ret;

    if (len == 0)                                          { return 0; }
    if (cp == 65001U)                                      { return fwrite(buf, 1, len, fp); }
    if (h == INVALID_HANDLE_VALUE || h == NULL)            { return fwrite(buf, 1, len, fp); }

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
    ret = len; /* caller expects (sz*cnt) not actual bytes after xcode */
    (void)written;
    return ret;
}

/**
 * @brief Write @p len bytes starting at @p buf to @p fp on Windows.
 *
 * If @p fp is a real console stream the bytes are interpreted as UTF-8
 * (the internal representation used everywhere in this program) and
 * emitted via WriteConsoleW as UTF-16LE, which bypasses the console's
 * code-page decoder and always renders CJK glyphs correctly regardless
 * of whether SetConsoleOutputCP(65001) was honoured by the host.
 *
 * If @p fp is NOT a console (pipe to shell, file redirection, NUL,
 * --output=FILE, the test harness, etc.) then the original UTF-8
 * bytes are written verbatim through fwrite, which matches GNU sort
 * behaviour and keeps byte-for-byte reproducibility for build tests.
 *
 * @param buf     Bytes to write (must be valid UTF-8 for console use).
 * @param len     Number of bytes in @p buf.
 * @param fp      Destination stdio stream.
 * @return number of bytes actually written (same as fwrite return).
 */
static size_t _sort_write_win32(const void * buf, size_t len, FILE * fp)
{
    int    fd;
    bool   is_std;

    if (fp == NULL) { return 0; }
    fd    = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /*
     * Only stdout and stderr need special Windows treatment.  Any other
     * stream (--output=FILE, --debug help text temp files, ...) must
     * keep raw UTF-8 bytes so sort's output is byte-identical to GNU
     * sort regardless of the host code page.
     */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_sort_is_console_stream(fp)) {
        /* Real console (cmd.exe / modern Terminal): bypass CP entirely. */
        int      wlen;
        wchar_t * wbuf;
        DWORD    written = 0;
        BOOL     ok;
        HANDLE   h = _sort_std_handle_for_fd(fd);

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

    if (_sort_is_disk_stream(fp)) {
        /* > file / NUL: byte-identical UTF-8 */
        return fwrite(buf, 1, len, fp);
    }

    if (_sort_is_pipe_stream(fp)) {
        /*
         * PowerShell 5.x pipe / |other-cmd: transcode to the console's
         * active output code page so the decoder at the other side
         * (PS5 uses [Console]::OutputEncoding) matches our bytes.
         */
        return _sort_write_pipe_cp(buf, len, fp);
    }

    return fwrite(buf, 1, len, fp);
}
#endif  /* SORT_PLATFORM_WINDOWS */

/**
 * @brief Portable byte-write macro used by all text output paths
 *        (sort_printf, sort_putchar, sort_fputs, sort_fflush, the
 *        output emitter, --debug annotations, error messages, ...).
 *
 * On Windows the call is routed through _sort_write_win32 so console
 * output is rendered with correct glyphs (see @b _sort_write_win32).
 * On POSIX systems the macro simply expands to @c fwrite.
 */
#ifdef SORT_PLATFORM_WINDOWS
    #ifndef sort_fwrite
        #define sort_fwrite(buf, sz, cnt, fp) \
            _sort_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef sort_fwrite
        #define sort_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print (printf-compatible).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__"
 * (works on GCC, Clang, MSVC, MinGW; also accepted with a pedantic
 * warning in strict -std=c99 builds).
 *
 * On Windows the formatted buffer is emitted through
 * @c _sort_write_win32 so console glyphs remain correct.
 */
#ifndef sort_printf
    #ifdef SORT_PLATFORM_WINDOWS
        #define sort_printf(fmt, ...) \
            do { \
                char _sortpf[2048]; \
                int _sortpf_n = _snprintf(_sortpf, sizeof(_sortpf), (fmt), ##__VA_ARGS__); \
                if (_sortpf_n > 0) { (void)_sort_write_win32(_sortpf, (size_t)_sortpf_n, sort_out_stream); } \
            } while (0)
    #else
        #define sort_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 *
 * Note: we cast to unsigned char first so values with the MSB set do
 *       not trigger undefined behaviour in putchar's @c int argument
 *       when char is signed on the host platform.
 */
#ifndef sort_putchar
    #define sort_putchar(ch) \
        do { unsigned char _sortpc = (unsigned char)(ch); (void)sort_fwrite(&_sortpc, 1, 1, sort_out_stream); } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally sort_out_stream)
 */
#ifndef sort_fputs
    #define sort_fputs(str, stream) \
        do { const char * _sortsp = (str); if (_sortsp) { (void)sort_fwrite(_sortsp, 1, strlen(_sortsp), (stream)); } } while (0)
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream (normally sort_out_stream)
 */
#ifndef sort_fflush
    #define sort_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free helper: free(*p) and set the pointer to NULL.
 *        Safe to call when @p p itself is NULL (does nothing).
 * @param p  Address of the pointer to clear.
 */
#ifndef sort_safe_free
    #define sort_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif


/********************************
 *    static variables
 ********************************/

/** @brief Global options pointer used by qsort's stateless comparator. */
static sort_opts_t * g_sort_opts = NULL;

/** @brief English month abbreviations used by -M / --month-sort. */
static const char * const sort_month_names[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the sort command.
 *
 * Processing flow:
 *   1. Apply platform-specific I/O setup (setlocale, Windows binary mode)
 *   2. Parse options / files, permuting argv so options come first
 *   3. If --merge, allocate per-file offset array for k-way merge
 *   4. Read input files (or stdin, or --files0-from) into line_array_t
 *   5. If --check (-c / -C), verify order and return
 *   6. Otherwise sort (or merge) lines
 *   7. Output to stdout or --output=FILE, honoring -u / -z / --debug
 *   8. Free resources and return
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, non-zero on error / disorder
 */
int main(int argc, char ** argv)
{
    setlocale(LC_ALL, "");
#ifdef SORT_PLATFORM_WINDOWS
    _setmode(_fileno(stdin),  O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
    _setmode(_fileno(stderr), O_BINARY);
    {
        /*
         * Attach to the parent console if the launcher did not give us
         * one (for example a .NET GUI host that captures stdout as a
         * pipe but the user still has an interactive console window
         * somewhere upstream).  We do this before the first write so
         * that _sort_is_console_stream / _sort_is_pipe_stream below
         * see the real handles.
         *
         * We INTENTIONALLY do NOT call SetConsoleOutputCP(65001) here
         * any more.  For legacy console output the code instead uses
         * WriteConsoleW with UTF-16LE which bypasses the code-page
         * decoder entirely; for PowerShell 5.x pipes the bytes are
         * transcoded by _sort_write_pipe_cp() to whatever the user's
         * current console output CP actually is (typically 936 on
         * Simplified-Chinese Windows), which is what PS5's
         * [Console]::OutputEncoding uses to decode the pipe bytes —
         * forcing CP 65001 here would break that matching and bring
         * back the 鐨勮娉曟槸 mojibake.
         */
        DWORD outMode = 0;
        DWORD errMode = 0;
        BOOL  outIsConsole;
        BOOL  errIsConsole;
        BOOL  anyConsole;

        outIsConsole = (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outMode) != FALSE);
        errIsConsole = (GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE),  &errMode) != FALSE);
        anyConsole   = outIsConsole || errIsConsole;

        if (!anyConsole) {
            (void)AttachConsole(ATTACH_PARENT_PROCESS);
            (void)GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outMode);
            (void)GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE),  &errMode);
        }
    }
#endif

    sort_opts_t opts;
    memset(&opts, 0, sizeof(opts));

    int file_start = 0;
    if (!_sort_parse_opts(argc, argv, &opts, &file_start)) {
        _sort_free_opts(&opts);
        return 2;
    }

    line_array_t arr;
    memset(&arr, 0, sizeof(arr));

    int nfiles = argc - file_start;
    int ok = 1;
    size_t * file_offsets = NULL;
    int merge_nfiles = 0;

    if (opts.merge) {
        int total_files = (nfiles == 0) ? 1 : nfiles;
        if (opts.files0_from) {
            total_files = (int)SORT_MAX_FILES0;
        }
        file_offsets = (size_t *)malloc((size_t)(total_files + 1) * sizeof(size_t));
        if (!file_offsets) {
            fprintf(stderr, "sort: out of memory\n");
            _sort_free_opts(&opts);
            return 2;
        }
        file_offsets[0] = 0;
    }

    if (opts.files0_from) {
        if (!_sort_read_files0_from(opts.files0_from, &opts, &arr)) {
            ok = 0;
        }
        if (ok && opts.merge) {
            opts.merge = 0;
        }
    }
    else if (nfiles == 0) {
        if (!_sort_read_file("-", &opts, &arr)) {
            ok = 0;
        }
        if (ok && opts.merge) {
            file_offsets[1] = arr.count;
            merge_nfiles = 1;
        }
    }
    else {
        for (int i = 0; i < nfiles; i++) {
            if (!_sort_read_file(argv[file_start + i], &opts, &arr)) {
                ok = 0;
                break;
            }
            if (opts.merge) {
                file_offsets[i + 1] = arr.count;
            }
        }
        if (ok && opts.merge) {
            merge_nfiles = nfiles;
        }
    }

    if (!ok) {
        sort_safe_free(file_offsets);
        _sort_free_lines(&arr);
        _sort_free_opts(&opts);
        return 2;
    }

    if (opts.check) {
        int ret = _sort_check(&arr, &opts);
        sort_safe_free(file_offsets);
        _sort_free_lines(&arr);
        _sort_free_opts(&opts);
        return ret;
    }

    g_sort_opts = &opts;

    if (opts.merge && merge_nfiles >= 2 && arr.count > 1) {
        _sort_merge_k(&arr, &opts, file_offsets, merge_nfiles);
    }
    else if (arr.count > 1) {
        qsort(arr.lines, arr.count, sizeof(sort_line_t), _sort_qsort_cmp);
    }

    _sort_output(&arr, &opts);

    sort_safe_free(file_offsets);
    _sort_free_lines(&arr);
    _sort_free_opts(&opts);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information to stdout and exit(0).
 */
static void _sort_usage(void)
{
    sort_fputs(
        "Usage: sort [OPTION]... [FILE]...\n"
        "  or:  sort [OPTION]... --files0-from=F\n"
        "Write sorted concatenation of all FILE(s) to standard output.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "Ordering options:\n"
        "\n"
        "  -b, --ignore-leading-blanks  ignore leading blanks\n"
        "  -d, --dictionary-order      only consider blanks and alphanumeric\n"
        "  -f, --ignore-case           fold lower case to upper case characters\n"
        "  -g, --general-numeric-sort  compare according to general numerical value\n"
        "  -h, --human-numeric-sort    compare human-readable numbers (e.g., 2K 1G)\n"
        "  -i, --ignore-nonprinting    consider only printable characters\n"
        "  -M, --month-sort            compare (unknown) < 'JAN' < ... < 'DEC'\n"
        "  -n, --numeric-sort          compare according to string numerical value\n"
        "  -R, --random-sort           shuffle, but group identical keys\n"
        "  -r, --reverse               reverse the result of comparisons\n"
        "      --sort=WORD             sort according to WORD:\n"
        "                                general-numeric -g, human-numeric -h,\n"
        "                                month -M, numeric -n, random -R, version -V\n"
        "  -V, --version-sort          natural sort of (version) numbers within text\n"
        "\n"
        "Other options:\n"
        "\n"
        "      --batch-size=NMERGE     merge at most NMERGE inputs at once (ignored)\n"
        "  -c, --check, --check=diagnose-first\n"
        "                              check for sorted input; do not sort\n"
        "  -C, --check=quiet, --check=silent\n"
        "                              like -c, but do not report first bad line\n"
        "      --compress-program=PROG compress temporaries with PROG (ignored)\n"
        "      --debug                 annotate the part of the line used to sort,\n"
        "                              and warn about questionable usage to stderr\n"
        "      --files0-from=F         read input from the files specified by\n"
        "                                NUL-terminated names in file F;\n"
        "                                if F is - then read names from standard input\n"
        "  -k, --key=KEYDEF            sort via a key; KEYDEF gives location and type\n"
        "  -m, --merge                 merge already sorted files; do not sort\n"
        "  -o, --output=FILE           write result to FILE instead of standard output\n"
        "      --parallel=N            change the number of sorts run concurrently\n"
        "                                to N (ignored, single-threaded)\n"
        "  -s, --stable                stabilize sort by disabling last-resort comparison\n"
        "  -S, --buffer-size=SIZE      use SIZE for main memory buffer (ignored)\n"
        "  -t, --field-separator=SEP   use SEP instead of non-blank to blank transition\n"
        "  -T, --temporary-directory=DIR  use DIR for temporaries (ignored, in-memory)\n"
        "  -u, --unique                with -c, check for strict ordering; without -c,\n"
        "                                output only the first of an equal run\n"
        "  -z, --zero-terminated       line delimiter is NUL, not newline\n"
        "      --random-source=FILE    get random bytes from FILE (ignored)\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "KEYDEF is F[.C][OPTS][,F[.C][OPTS]] for start and stop position, where F is a\n"
        "field number and C a character position in the field; both are 1-based. OPTS is\n"
        "one or more of b, d, f, g, h, i, M, n, R, r, V.\n"
        "\n"
        "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n",
        sort_out_stream
    );
    exit(0);
}

/**
 * @brief Print version information to stdout and exit(0).
 */
static void _sort_version(void)
{
    sort_printf("sort (Yezc cclinuxtools) %s\n", SORT_VERSION_STR);
    sort_printf("%s", "MIT License\n");
    sort_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    sort_printf("%s", "License MIT: <https://mit-license.org/>\n");
    sort_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    sort_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
    exit(0);
}

/**
 * @brief Apply a --sort=WORD option to @p opts (used by both long-option
 *        paths: `--sort=WORD` in parse_opts and by _sort_parse_long as a
 *        fallback).
 * @param opts  target options struct (non-NULL)
 * @param word  sort type keyword (e.g. "numeric", "version")
 * @return 1 on success, 0 if the keyword is unknown (and prints an error)
 */
static int _sort_set_sort_word(sort_opts_t * opts, const char * word)
{
    if (strcmp(word, "general-numeric") == 0)      { opts->general_numeric = 1; }
    else if (strcmp(word, "human-numeric") == 0)   { opts->human_numeric = 1; }
    else if (strcmp(word, "month") == 0)           { opts->month = 1; }
    else if (strcmp(word, "numeric") == 0)         { opts->numeric = 1; }
    else if (strcmp(word, "random") == 0)          { opts->random = 1; }
    else if (strcmp(word, "version") == 0)         { opts->version = 1; }
    else {
        fprintf(stderr, "sort: invalid argument '%s' for '--sort'\n", word);
        fprintf(stderr, "Valid arguments are:\n"
                        "  - 'general-numeric'\n"
                        "  - 'human-numeric'\n"
                        "  - 'month'\n"
                        "  - 'numeric'\n"
                        "  - 'random'\n"
                        "  - 'version'\n");
        return 0;
    }
    return 1;
}

/**
 * @brief Parse a GNU-style long option (no leading `--`).
 *
 * Options that take an argument without an `=` are handled by consuming
 * the next argv[] entry via @p i / @p argc / @p argv , exactly like the
 * short-option path.
 *
 * @param opt   the option name (without leading dashes, possibly "NAME=VALUE")
 * @param opts  target options struct
 * @param i     current argv[] index (advanced when an argument is consumed)
 * @param argc  total argument count
 * @param argv  argument vector
 * @return 1 on success, 0 on failure (prints an error to stderr)
 */
static int _sort_parse_long(const char * opt, sort_opts_t * opts, int * i, int argc, char ** argv)
{
    if (strcmp(opt, "help") == 0)                                    { _sort_usage(); }
    else if (strcmp(opt, "version") == 0)                            { _sort_version(); }
    else if (strcmp(opt, "ignore-leading-blanks") == 0)              { opts->ignore_blanks = 1; }
    else if (strcmp(opt, "dictionary-order") == 0)                   { opts->dictionary = 1; }
    else if (strcmp(opt, "ignore-case") == 0)                        { opts->ignore_case = 1; }
    else if (strcmp(opt, "general-numeric-sort") == 0)               { opts->general_numeric = 1; }
    else if (strcmp(opt, "human-numeric-sort") == 0)                 { opts->human_numeric = 1; }
    else if (strcmp(opt, "ignore-nonprinting") == 0)                 { opts->ignore_nonprinting = 1; }
    else if (strcmp(opt, "month-sort") == 0)                         { opts->month = 1; }
    else if (strcmp(opt, "numeric-sort") == 0)                       { opts->numeric = 1; }
    else if (strcmp(opt, "random-sort") == 0)                        { opts->random = 1; }
    else if (strcmp(opt, "reverse") == 0)                            { opts->reverse = 1; }
    else if (strcmp(opt, "version-sort") == 0)                       { opts->version = 1; }
    else if (strcmp(opt, "check") == 0)                              { opts->check = 1; }
    else if (strncmp(opt, "check=", 6) == 0) {
        const char * mode = opt + 6;
        if (strcmp(mode, "silent") == 0 || strcmp(mode, "quiet") == 0)          { opts->check = 2; }
        else if (strcmp(mode, "diagnose-first") == 0)                           { opts->check = 1; }
        else if (strcmp(mode, "none") == 0)                                      { opts->check = 0; }
        else {
            fprintf(stderr, "sort: invalid --check mode: '%s'\n", mode);
            return 0;
        }
    }
    else if (strncmp(opt, "sort=", 5) == 0) {
        if (!_sort_set_sort_word(opts, opt + 5)) {
            return 0;
        }
    }
    else if (strcmp(opt, "merge") == 0)                               { opts->merge = 1; }
    else if (strcmp(opt, "stable") == 0)                              { opts->stable = 1; }
    else if (strcmp(opt, "unique") == 0)                              { opts->unique = 1; }
    else if (strcmp(opt, "zero-terminated") == 0)                     { opts->zero_terminated = 1; }
    else if (strcmp(opt, "debug") == 0)                               { opts->debug = 1; }
    else if (strcmp(opt, "batch-size") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--batch-size' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "buffer-size") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--buffer-size' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "temporary-directory") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--temporary-directory' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "random-source") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--random-source' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "compress-program") == 0 || strcmp(opt, "compress") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--compress-program' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "parallel") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--parallel' requires an argument\n");
            return 0;
        }
        (*i)++;
    }
    else if (strcmp(opt, "files0-from") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "sort: option '--files0-from' requires an argument\n");
            return 0;
        }
        opts->files0_from = argv[*i + 1];
        (*i)++;
    }
    else {
        fprintf(stderr, "sort: unrecognized option '--%s'\n", opt);
        fprintf(stderr, "Try 'sort --help' for more information.\n");
        return 0;
    }
    return 1;
}

/**
 * @brief Return true if the short option @p c requires an argument.
 */
static int _sort_short_takes_arg(char c)
{
    return (c == 'k') || (c == 'o') || (c == 't') || (c == 'T') || (c == 'S');
}

/**
 * @brief Return true if the long option @p name (no leading dashes) takes
 *        a value argument (i.e. the caller should look for `=` in the
 *        same entry or consume the next argv[] entry).
 */
static int _sort_long_takes_arg(const char * name)
{
    return strcmp(name, "key") == 0 ||
           strcmp(name, "output") == 0 ||
           strcmp(name, "field-separator") == 0 ||
           strcmp(name, "temporary-directory") == 0 ||
           strcmp(name, "buffer-size") == 0 ||
           strcmp(name, "batch-size") == 0 ||
           strcmp(name, "random-source") == 0 ||
           strcmp(name, "compress-program") == 0 ||
           strcmp(name, "compress") == 0 ||
           strcmp(name, "parallel") == 0 ||
           strcmp(name, "files0-from") == 0;
}

/**
 * @brief Permute argv[] so that all options (and their arguments) appear
 *        before all non-option file arguments. A standalone `--` stops
 *        option processing, keeping the following args as files.
 *
 * @param argc        argument count
 * @param argv        argument vector (modified in place)
 * @param file_start  [out] index of the first file argument after permute
 */
static void _sort_permute_argv(int argc, char ** argv, int * file_start)
{
    if (argc <= 2) {
        *file_start = argc;
        return;
    }
    char ** opts_buf  = (char **)malloc((size_t)argc * sizeof(char *));
    char ** files_buf = (char **)malloc((size_t)argc * sizeof(char *));
    int nopt = 0;
    int nfile = 0;
    int i = 1;
    while (i < argc) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            files_buf[nfile++] = argv[i];
            i++;
        }
        else if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            while (i < argc) {
                files_buf[nfile++] = argv[i++];
            }
        }
        else {
            int takes_next = 0;
            if (argv[i][1] == '-') {
                const char * opt = argv[i] + 2;
                const char * eq = strchr(opt, '=');
                if (!eq && _sort_long_takes_arg(opt)) {
                    takes_next = 1;
                }
            }
            else {
                for (int j = 1; argv[i][j] != '\0'; j++) {
                    if (_sort_short_takes_arg(argv[i][j])) {
                        if (argv[i][j + 1] == '\0') {
                            takes_next = 1;
                        }
                        break;
                    }
                }
            }
            opts_buf[nopt++] = argv[i++];
            if (takes_next && i < argc) {
                opts_buf[nopt++] = argv[i++];
            }
        }
    }
    int pos = 1;
    for (int k = 0; k < nopt; k++) {
        argv[pos++] = opts_buf[k];
    }
    for (int k = 0; k < nfile; k++) {
        argv[pos++] = files_buf[k];
    }
    *file_start = 1 + nopt;
    free(opts_buf);
    free(files_buf);
}

/**
 * @brief Parse command-line options into @p opts.
 *
 * Delegates to _sort_permute_argv so options may appear in any order
 * relative to file names (matches GNU getopt behavior).
 *
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        [out] parsed options
 * @param file_start  [out] index of the first file argument
 * @return 1 on success, 0 on failure
 */
static int _sort_parse_opts(int argc, char * argv[], sort_opts_t * opts, int * file_start)
{
    _sort_permute_argv(argc, argv, file_start);

    int i;
    for (i = 1; i < *file_start; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        if (argv[i][1] == '-') {
            const char * opt = argv[i] + 2;
            const char * val = strchr(opt, '=');
            char optname[80];
            if (val) {
                size_t len = (size_t)(val - opt);
                if (len >= sizeof(optname)) {
                    len = sizeof(optname) - 1;
                }
                memcpy(optname, opt, len);
                optname[len] = '\0';
                if (strcmp(optname, "key") == 0) {
                    key_spec_t key;
                    memset(&key, 0, sizeof(key));
                    if (!_sort_parse_keydef(val + 1, &key)) {
                        fprintf(stderr, "sort: invalid key definition: '%s'\n", val + 1);
                        return 0;
                    }
                    if (!_sort_add_key(opts, &key)) {
                        return 0;
                    }
                    continue;
                }
                else if (strcmp(optname, "output") == 0) {
                    opts->output_file = val + 1;
                    continue;
                }
                else if (strcmp(optname, "field-separator") == 0) {
                    if (val[1] == '\0') {
                        fprintf(stderr, "sort: option '--field-separator' requires an argument\n");
                        return 0;
                    }
                    opts->field_sep = val[1];
                    continue;
                }
                else if (strcmp(optname, "temporary-directory") == 0) { continue; }
                else if (strcmp(optname, "buffer-size") == 0)         { continue; }
                else if (strcmp(optname, "batch-size") == 0)          { continue; }
                else if (strcmp(optname, "random-source") == 0)       { continue; }
                else if (strcmp(optname, "compress-program") == 0)    { continue; }
                else if (strcmp(optname, "compress") == 0)                { continue; }
                else if (strcmp(optname, "parallel") == 0)            { continue; }
                else if (strcmp(optname, "files0-from") == 0) {
                    opts->files0_from = val + 1;
                    continue;
                }
                else if (strcmp(optname, "sort") == 0) {
                    if (!_sort_set_sort_word(opts, val + 1)) {
                        return 0;
                    }
                    continue;
                }
                opt = optname;
            }
            if (!_sort_parse_long(opt, opts, &i, argc, argv)) {
                return 0;
            }
            continue;
        }

        int j;
        for (j = 1; argv[i][j] != '\0'; j++) {
            char c = argv[i][j];
            switch (c) {
                case 'b': opts->ignore_blanks = 1; break;
                case 'd': opts->dictionary = 1;    break;
                case 'f': opts->ignore_case = 1;   break;
                case 'g': opts->general_numeric = 1; break;
                case 'h': opts->human_numeric = 1;   break;
                case 'i': opts->ignore_nonprinting = 1; break;
                case 'M': opts->month = 1;          break;
                case 'n': opts->numeric = 1;        break;
                case 'R': opts->random = 1;         break;
                case 'r': opts->reverse = 1;        break;
                case 'V': opts->version = 1;        break;
                case 'c': opts->check = 1;          break;
                case 'C': opts->check = 2;          break;
                case 'm': opts->merge = 1;          break;
                case 's': opts->stable = 1;         break;
                case 'u': opts->unique = 1;         break;
                case 'z': opts->zero_terminated = 1; break;
                case 'k':
                case 'o':
                case 't':
                case 'T':
                case 'S': {
                    const char * arg;
                    int consumed_next = 0;
                    if (argv[i][j + 1] != '\0') {
                        arg = &argv[i][j + 1];
                        j = (int)strlen(argv[i]) - 1;
                    }
                    else {
                        if (i + 1 >= argc) {
                            fprintf(stderr, "sort: option '-%c' requires an argument\n", c);
                            return 0;
                        }
                        i++;
                        arg = argv[i];
                        consumed_next = 1;
                    }
                    if (c == 'k') {
                        key_spec_t key;
                        memset(&key, 0, sizeof(key));
                        if (!_sort_parse_keydef(arg, &key)) {
                            fprintf(stderr, "sort: invalid key definition: '%s'\n", arg);
                            return 0;
                        }
                        if (!_sort_add_key(opts, &key)) {
                            return 0;
                        }
                    }
                    else if (c == 'o') {
                        opts->output_file = arg;
                    }
                    else if (c == 't') {
                        opts->field_sep = arg[0];
                    }
                    if (consumed_next) {
                        goto _break_short_loop;
                    }
                    break;
                }
                default:
                    fprintf(stderr, "sort: invalid option -- '%c'\n", c);
                    fprintf(stderr, "Try 'sort --help' for more information.\n");
                    return 0;
            }
        }
        _break_short_loop:;
    }
    *file_start = i;
    return 1;
}

/**
 * @brief Parse a `--key=KEYDEF` style key definition.
 *
 * KEYDEF syntax: F[.C][OPTS][,F[.C][OPTS]] where
 *   - F = 1-based field number
 *   - C = 1-based character position in the field
 *   - OPTS = zero or more of {b,d,f,g,h,i,M,n,R,r,V}
 *
 * @param spec  the key spec string (e.g. "2,3nr", "2.4,3.1r")
 * @param key   [out] parsed key
 * @return 1 on success, 0 on syntax error
 */
static int _sort_parse_keydef(const char * spec, key_spec_t * key)
{
    key->start_field = 0;
    key->start_char = 1;
    key->end_field = -1;
    key->end_char = 0;

    char * endp;
    key->start_field = (int)strtol(spec, &endp, 10);
    if (key->start_field < 1 || endp == spec) {
        return 0;
    }
    if (*endp == '.') {
        endp++;
        key->start_char = (int)strtol(endp, &endp, 10);
        if (key->start_char < 1) {
            key->start_char = 1;
        }
    }
    while (*endp != '\0' && *endp != ',') {
        switch (*endp) {
            case 'b': key->ignore_blanks = 1;        key->set_ignore_blanks = 1;      break;
            case 'd': key->dictionary = 1;           key->set_dictionary = 1;         break;
            case 'f': key->ignore_case = 1;          key->set_ignore_case = 1;        break;
            case 'g': key->general_numeric = 1;      key->set_general_numeric = 1;    break;
            case 'h': key->human_numeric = 1;        key->set_human_numeric = 1;      break;
            case 'i': key->ignore_nonprinting = 1;   key->set_ignore_nonprinting = 1; break;
            case 'M': key->month = 1;                key->set_month = 1;              break;
            case 'n': key->numeric = 1;              key->set_numeric = 1;            break;
            case 'R': key->random = 1;               key->set_random = 1;             break;
            case 'r': key->reverse = 1;              key->set_reverse = 1;            break;
            case 'V': key->version = 1;              key->set_version = 1;            break;
            default:  return 0;
        }
        endp++;
    }
    if (*endp == ',') {
        endp++;
        key->end_field = (int)strtol(endp, &endp, 10);
        if (key->end_field < 1) {
            return 0;
        }
        key->end_char = 0;
        if (*endp == '.') {
            endp++;
            key->end_char = (int)strtol(endp, &endp, 10);
        }
        while (*endp != '\0') {
            switch (*endp) {
                case 'b': key->ignore_blanks = 1;        key->set_ignore_blanks = 1;      break;
                case 'd': key->dictionary = 1;           key->set_dictionary = 1;         break;
                case 'f': key->ignore_case = 1;          key->set_ignore_case = 1;        break;
                case 'g': key->general_numeric = 1;      key->set_general_numeric = 1;    break;
                case 'h': key->human_numeric = 1;        key->set_human_numeric = 1;      break;
                case 'i': key->ignore_nonprinting = 1;   key->set_ignore_nonprinting = 1; break;
                case 'M': key->month = 1;                key->set_month = 1;              break;
                case 'n': key->numeric = 1;              key->set_numeric = 1;            break;
                case 'R': key->random = 1;               key->set_random = 1;             break;
                case 'r': key->reverse = 1;              key->set_reverse = 1;            break;
                case 'V': key->version = 1;              key->set_version = 1;            break;
                default:  return 0;
            }
            endp++;
        }
    }
    return 1;
}

/**
 * @brief Append a key definition to the dynamic array in opts.
 * @param opts  target options struct
 * @param key   key to append (copied by value)
 * @return 1 on success, 0 on out of memory
 */
static int _sort_add_key(sort_opts_t * opts, const key_spec_t * key)
{
    key_spec_t * tmp = (key_spec_t *)realloc(opts->keys,
                        (size_t)(opts->nkeys + 1) * sizeof(key_spec_t));
    if (!tmp) {
        fprintf(stderr, "sort: out of memory\n");
        return 0;
    }
    opts->keys = tmp;
    opts->keys[opts->nkeys++] = *key;
    return 1;
}

/**
 * @brief Append one line (without terminator byte) to the dynamic array.
 * @param arr   target array
 * @param text  pointer to line bytes (not necessarily NUL-terminated)
 * @param len   length of line in bytes
 * @return 1 on success, 0 on out of memory
 */
static int _sort_add_line(line_array_t * arr, const char * text, size_t len)
{
    if (arr->count >= arr->capacity) {
        size_t new_cap = arr->capacity ? arr->capacity * 2 : 64;
        sort_line_t * tmp = (sort_line_t *)realloc(arr->lines, new_cap * sizeof(sort_line_t));
        if (!tmp) {
            return 0;
        }
        arr->lines = tmp;
        arr->capacity = new_cap;
    }
    char * text_copy = (char *)malloc(len + 1);
    if (!text_copy) {
        return 0;
    }
    memcpy(text_copy, text, len);
    text_copy[len] = '\0';
    arr->lines[arr->count].text = text_copy;
    arr->lines[arr->count].len = len;
    arr->lines[arr->count].index = arr->count;
    arr->count++;
    return 1;
}

/**
 * @brief Read lines from @p filename (or stdin, when @p filename is "-")
 *        into @p arr.
 * @param filename  path to file, or "-" for stdin
 * @param opts      options struct (used for -z / separator implied handling)
 * @param arr       target array (lines are appended)
 * @return 1 on success, 0 on error (message already written to stderr)
 */
static int _sort_read_file(const char * filename, sort_opts_t * opts, line_array_t * arr)
{
    FILE * fp;
    if (!filename || strcmp(filename, "-") == 0) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            fprintf(stderr, "sort: %s: %s\n", filename, strerror(errno));
            return 0;
        }
    }

    int term = opts->zero_terminated ? '\0' : '\n';
    char * buf = NULL;
    size_t buf_cap = 0;
    size_t buf_len = 0;
    int c;
    int ok = 1;

    while ((c = fgetc(fp)) != EOF) {
        if (buf_len + 1 >= buf_cap) {
            buf_cap = buf_cap ? buf_cap * 2 : (size_t)SORT_LINE_BUF_INIT;
            char * tmp = (char *)realloc(buf, buf_cap);
            if (!tmp) {
                ok = 0;
                break;
            }
            buf = tmp;
        }
        buf[buf_len++] = (char)c;
        if (c == term) {
            size_t line_len = buf_len - 1;
            if (term == '\n' && line_len > 0 && buf[line_len - 1] == '\r') {
                line_len--;
            }
            if (!_sort_add_line(arr, buf, line_len)) {
                ok = 0;
                break;
            }
            buf_len = 0;
        }
    }

    if (ok && buf_len > 0) {
        size_t line_len = buf_len;
        if (!opts->zero_terminated && line_len > 0 && buf[line_len - 1] == '\r') {
            line_len--;
        }
        if (!_sort_add_line(arr, buf, line_len)) {
            ok = 0;
        }
    }

    sort_safe_free(buf);
    if (fp != stdin) {
        fclose(fp);
    }
    return ok;
}

/**
 * @brief Read NUL-separated file names from @p src (or stdin if "-"),
 *        then concatenate their contents into @p arr.
 * @param src   source file (or "-") of NUL-terminated name list
 * @param opts  options
 * @param arr   target line array
 * @return 1 on success, 0 on error
 */
static int _sort_read_files0_from(const char * src, sort_opts_t * opts, line_array_t * arr)
{
    FILE * fp;
    if (strcmp(src, "-") == 0) {
        fp = stdin;
    }
    else {
        fp = fopen(src, "rb");
        if (!fp) {
            fprintf(stderr, "sort: %s: %s\n", src, strerror(errno));
            return 0;
        }
    }

    char * name_buf = NULL;
    size_t name_cap = 0;
    size_t name_len = 0;
    int c;
    int ok = 1;

    while ((c = fgetc(fp)) != EOF) {
        if (name_len + 1 >= name_cap) {
            name_cap = name_cap ? name_cap * 2 : 128;
            char * tmp = (char *)realloc(name_buf, name_cap);
            if (!tmp) {
                ok = 0;
                break;
            }
            name_buf = tmp;
        }
        if (c == '\0') {
            name_buf[name_len] = '\0';
            if (name_len > 0) {
                if (!_sort_read_file(name_buf, opts, arr)) {
                    ok = 0;
                    break;
                }
            }
            name_len = 0;
        }
        else {
            name_buf[name_len++] = (char)c;
        }
    }

    if (ok && name_len > 0) {
        name_buf[name_len] = '\0';
        if (!_sort_read_file(name_buf, opts, arr)) {
            ok = 0;
        }
    }

    sort_safe_free(name_buf);
    if (fp != stdin) {
        fclose(fp);
    }
    return ok;
}

/**
 * @brief Locate the start of field number @p field within a line.
 *
 * Field separator rules:
 *   - When @p sep is '\0' (default), "non-blank to blank transition"
 *     rules apply: consecutive blanks count as one separator and are
 *     skipped before each field.
 *   - When @p sep is non-zero, that exact byte is the separator and
 *     consecutive separators produce empty fields (matches GNU sort).
 *
 * @param line       input line bytes
 * @param len        length of the line
 * @param field      1-based field index
 * @param sep        field separator ('\0' = whitespace mode)
 * @param field_len  [out,optional] receives the length of the found field
 * @return pointer into line[], or NULL if the line has no such field
 */
static const char * _sort_find_field(const char * line, size_t len, int field, char sep, size_t * field_len)
{
    if (field < 1) {
        return NULL;
    }
    const char * p = line;
    const char * end = line + len;
    int f = 1;

    if (sep == 0) {
        while (p < end && isspace((unsigned char)*p)) { p++; }
        while (f < field) {
            while (p < end && !isspace((unsigned char)*p)) { p++; }
            while (p < end && isspace((unsigned char)*p))  { p++; }
            f++;
            if (p >= end) { return NULL; }
        }
    }
    else {
        while (f < field) {
            while (p < end && *p != sep) { p++; }
            if (p >= end) { return NULL; }
            p++;
            f++;
        }
    }

    if (p >= end) {
        return NULL;
    }
    const char * field_end;
    if (sep == 0) {
        field_end = p;
        while (field_end < end && !isspace((unsigned char)*field_end)) {
            field_end++;
        }
    }
    else {
        field_end = p;
        while (field_end < end && *field_end != sep) {
            field_end++;
        }
    }
    if (field_len) {
        *field_len = (size_t)(field_end - p);
    }
    return p;
}

/**
 * @brief Extract the byte range of @p key within a line.
 *
 * @param line              line bytes
 * @param len               length of line
 * @param key               key spec
 * @param global            global options (currently unused, reserved)
 * @param sep               field separator
 * @param start             [out] pointer inside line[] at key start
 * @param key_len           [out] length of key in bytes
 * @param eff_ignore_blanks [in,out] on input: whether to skip leading blanks;
 *                                  on output: true if the effect of -b applied
 */
static void _sort_extract_key(const char * line, size_t len, const key_spec_t * key,
                              const sort_opts_t * global, char sep,
                              const char ** start, size_t * key_len,
                              int * eff_ignore_blanks)
{
    (void)global;
    size_t flen;
    const char * s = _sort_find_field(line, len, key->start_field, sep, &flen);
    if (!s) {
        *start = line + len;
        *key_len = 0;
        return;
    }
    if (key->start_char > 1) {
        size_t advance = (size_t)(key->start_char - 1);
        if (advance > flen) {
            advance = flen;
        }
        s += advance;
        if (flen >= advance) {
            flen -= advance;
        }
        else {
            flen = 0;
        }
    }
    if (eff_ignore_blanks && *eff_ignore_blanks) {
        const char * end = line + len;
        while (s < end && isspace((unsigned char)*s)) {
            s++;
        }
    }
    const char * end_pos;
    if (key->end_field < 0) {
        end_pos = line + len;
    }
    else {
        size_t end_flen;
        const char * efs = _sort_find_field(line, len, key->end_field, sep, &end_flen);
        if (!efs) {
            end_pos = line + len;
        }
        else if (key->end_char > 0) {
            size_t adv = (size_t)key->end_char;
            if (adv > end_flen) {
                adv = end_flen;
            }
            end_pos = efs + adv;
        }
        else {
            end_pos = efs + end_flen;
        }
    }
    if (end_pos < s) {
        end_pos = s;
    }
    *start = s;
    *key_len = (size_t)(end_pos - s);
}

/**
 * @brief Compute a simple non-cryptographic djb2 hash of a byte sequence.
 *        Used by -R / --random-sort as a deterministic "random" ordering.
 *
 * @param str  bytes
 * @param len  length
 * @return 64-bit-ish unsigned long hash value (width = host's ULONG_MAX)
 */
static unsigned long _sort_hash(const char * str, size_t len)
{
    unsigned long hash = 5381UL;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)str[i];
    }
    return hash;
}

/**
 * @brief Locale-aware byte-by-byte string comparison with transforms.
 *
 * @param a                   left bytes
 * @param alen                left length
 * @param b                   right bytes
 * @param blen                right length
 * @param dictionary          skip characters that are not alnum/space
 * @param ignore_nonprinting  skip non-printable characters
 * @param fold_case           lower-case before comparing
 * @return negative if a<b, positive if a>b, zero if equal
 */
static int _sort_str_cmp(const char * a, size_t alen,
                         const char * b, size_t blen,
                         int dictionary, int ignore_nonprinting, int fold_case)
{
    size_t i = 0;
    size_t j = 0;
    while (1) {
        unsigned char ca = 0;
        unsigned char cb = 0;
        int have_a = 0;
        int have_b = 0;

        while (i < alen) {
            ca = (unsigned char)a[i];
            if (dictionary && !(isalnum(ca) || isspace(ca))) {
                i++;
                continue;
            }
            if (ignore_nonprinting && !isprint(ca)) {
                i++;
                continue;
            }
            have_a = 1;
            break;
        }
        while (j < blen) {
            cb = (unsigned char)b[j];
            if (dictionary && !(isalnum(cb) || isspace(cb))) {
                j++;
                continue;
            }
            if (ignore_nonprinting && !isprint(cb)) {
                j++;
                continue;
            }
            have_b = 1;
            break;
        }
        if (!have_a && !have_b) { return 0; }
        if (!have_a) { return -1; }
        if (!have_b) { return  1; }
        if (fold_case) {
            ca = (unsigned char)tolower(ca);
            cb = (unsigned char)tolower(cb);
        }
        if (ca != cb) { return (ca < cb) ? -1 : 1; }
        i++;
        j++;
    }
}

/**
 * @brief Numeric comparison (both -n and -g use this).
 *
 * Leading whitespace is skipped (matches GNU sort -n default of implied -b).
 * If two numbers parse to the same value, the original strings are used as
 * the tie breaker (so "1" < "01" consistently).
 */
static int _sort_numeric_cmp(const char * a, size_t alen, const char * b, size_t blen)
{
    char bufa[512];
    char bufb[512];
    const char * pa = a;
    const char * pb = b;
    size_t la = alen;
    size_t lb = blen;
    while (la > 0 && isspace((unsigned char)*pa)) { pa++; la--; }
    while (lb > 0 && isspace((unsigned char)*pb)) { pb++; lb--; }
    if (la >= sizeof(bufa)) { la = sizeof(bufa) - 1; }
    if (lb >= sizeof(bufb)) { lb = sizeof(bufb) - 1; }
    memcpy(bufa, pa, la); bufa[la] = '\0';
    memcpy(bufb, pb, lb); bufb[lb] = '\0';

    char * endpa;
    char * endpb;
    double va = strtod(bufa, &endpa);
    double vb = strtod(bufb, &endpb);
    if (va < vb) { return -1; }
    if (va > vb) { return  1; }
    return _sort_str_cmp(a, alen, b, blen, 0, 0, 0);
}

/**
 * @brief Human-readable numeric comparison (-h).
 *        Understands suffixes K/M/G/T/P/E (binary, 1024-based, case-insensitive
 *        for K).  If no suffix is present, the parsed double is used as-is.
 */
static int _sort_human_cmp(const char * a, size_t alen, const char * b, size_t blen)
{
    char bufa[512];
    char bufb[512];
    const char * pa = a;
    const char * pb = b;
    size_t la = alen;
    size_t lb = blen;
    while (la > 0 && isspace((unsigned char)*pa)) { pa++; la--; }
    while (lb > 0 && isspace((unsigned char)*pb)) { pb++; lb--; }
    if (la >= sizeof(bufa)) { la = sizeof(bufa) - 1; }
    if (lb >= sizeof(bufb)) { lb = sizeof(bufb) - 1; }
    memcpy(bufa, pa, la); bufa[la] = '\0';
    memcpy(bufb, pb, lb); bufb[lb] = '\0';

    char * endpa;
    char * endpb;
    double va = strtod(bufa, &endpa);
    double vb = strtod(bufb, &endpb);

    if (endpa < bufa + la) {
        switch (*endpa) {
            case 'k': case 'K': va *= 1024.0;                             break;
            case 'M':           va *= 1024.0 * 1024.0;                   break;
            case 'G':           va *= 1024.0 * 1024.0 * 1024.0;          break;
            case 'T':           va *= 1099511627776.0;                   break;
            case 'P':           va *= 1125899906842624.0;                break;
            case 'E':           va *= 1152921504606846976.0;             break;
            default:                                                      break;
        }
    }
    if (endpb < bufb + lb) {
        switch (*endpb) {
            case 'k': case 'K': vb *= 1024.0;                             break;
            case 'M':           vb *= 1024.0 * 1024.0;                   break;
            case 'G':           vb *= 1024.0 * 1024.0 * 1024.0;          break;
            case 'T':           vb *= 1099511627776.0;                   break;
            case 'P':           vb *= 1125899906842624.0;                break;
            case 'E':           vb *= 1152921504606846976.0;             break;
            default:                                                      break;
        }
    }
    if (va < vb) { return -1; }
    if (va > vb) { return  1; }
    return _sort_str_cmp(a, alen, b, blen, 0, 0, 0);
}

/**
 * @brief Month comparison (-M / --month-sort).
 *
 * Looks for a 3-byte English month name at the start of each string
 * (JAN…DEC, case-insensitive).  If no month name is found, the string
 * compares as "unknown" (less than all months).  Equal months fall
 * back to lexicographic compare of the whole strings.
 */
static int _sort_month_cmp(const char * a, size_t alen, const char * b, size_t blen)
{
    const char * pa = a;
    const char * pb = b;
    size_t la = alen;
    size_t lb = blen;
    while (la > 0 && isspace((unsigned char)*pa)) { pa++; la--; }
    while (lb > 0 && isspace((unsigned char)*pb)) { pb++; lb--; }

    int ma = -1;
    int mb = -1;

    if (la >= 3) {
        char up[4];
        up[0] = (char)toupper((unsigned char)pa[0]);
        up[1] = (char)toupper((unsigned char)pa[1]);
        up[2] = (char)toupper((unsigned char)pa[2]);
        up[3] = '\0';
        for (int k = 0; k < 12; k++) {
            if (strcmp(up, sort_month_names[k]) == 0) {
                ma = k;
                break;
            }
        }
    }
    if (lb >= 3) {
        char up[4];
        up[0] = (char)toupper((unsigned char)pb[0]);
        up[1] = (char)toupper((unsigned char)pb[1]);
        up[2] = (char)toupper((unsigned char)pb[2]);
        up[3] = '\0';
        for (int k = 0; k < 12; k++) {
            if (strcmp(up, sort_month_names[k]) == 0) {
                mb = k;
                break;
            }
        }
    }

    if (ma < mb) { return -1; }
    if (ma > mb) { return  1; }
    return _sort_str_cmp(a, alen, b, blen, 0, 0, 0);
}

/**
 * @brief Natural / version sort (-V).
 *
 * Compares run-length-decoded strings using the GNU rules:
 *   - non-alphanumeric separators are skipped between runs
 *   - digit runs are compared by numeric value after stripping leading zeros
 *   - equal-length digit runs fall back to memcmp so string equality holds
 *   - alpha runs compare case-insensitive alphabetically
 *   - digit runs sort after alpha runs when the two types differ (GNU rule)
 */
static int _sort_version_cmp(const char * a, size_t alen, const char * b, size_t blen)
{
    size_t ia = 0;
    size_t ib = 0;
    while (ia < alen || ib < blen) {
        while (ia < alen && !isalnum((unsigned char)a[ia])) { ia++; }
        while (ib < blen && !isalnum((unsigned char)b[ib])) { ib++; }

        int a_digit = 0;
        int b_digit = 0;
        size_t a_start = ia;
        size_t b_start = ib;
        if (ia < alen && isdigit((unsigned char)a[ia])) {
            a_digit = 1;
            while (ia < alen && isdigit((unsigned char)a[ia])) { ia++; }
        }
        else if (ia < alen) {
            while (ia < alen && isalpha((unsigned char)a[ia])) { ia++; }
        }
        if (ib < blen && isdigit((unsigned char)b[ib])) {
            b_digit = 1;
            while (ib < blen && isdigit((unsigned char)b[ib])) { ib++; }
        }
        else if (ib < blen) {
            while (ib < blen && isalpha((unsigned char)b[ib])) { ib++; }
        }

        size_t arun = ia - a_start;
        size_t brun = ib - b_start;
        if (arun == 0 && brun == 0) { break; }
        if (arun == 0) { return -1; }
        if (brun == 0) { return  1; }

        if (a_digit && b_digit) {
            size_t az = a_start;
            size_t bz = b_start;
            while (az < ia - 1 && a[az] == '0') { az++; }
            while (bz < ib - 1 && b[bz] == '0') { bz++; }
            size_t anz = ia - az;
            size_t bnz = ib - bz;
            if (anz < bnz) { return -1; }
            if (anz > bnz) { return  1; }
            int r = memcmp(a + az, b + bz, anz);
            if (r != 0) { return (r < 0) ? -1 : 1; }
        }
        else if (a_digit != b_digit) {
            return a_digit ? 1 : -1;
        }
        else {
            size_t m = (arun < brun) ? arun : brun;
            for (size_t k = 0; k < m; k++) {
                unsigned char ca = (unsigned char)tolower((unsigned char)a[a_start + k]);
                unsigned char cb = (unsigned char)tolower((unsigned char)b[b_start + k]);
                if (ca != cb) { return (ca < cb) ? -1 : 1; }
            }
            if (arun != brun) { return (arun < brun) ? -1 : 1; }
        }
    }
    return 0;
}

/**
 * @brief Decide the effective set of per-key comparison options.
 *
 * For each boolean option, if the key has an explicit modifier (set_*
 * bitfield is 1) the key's value wins; otherwise the global option is
 * inherited.  The final value of @p ib is additionally forced to 1 if
 * any numeric-ish sort type is in effect (matches GNU sort semantics
 * where -n/-g/-h/-M each imply -b).
 */
static void _sort_resolve_key_opts(const key_spec_t * key, const sort_opts_t * global,
                                   int * ib, int * dic, int * ic,
                                   int * gn, int * hn, int * mo,
                                   int * inp, int * num, int * rnd,
                                   int * ver, int * rev)
{
    *ib  = key->set_ignore_blanks      ? key->ignore_blanks      : global->ignore_blanks;
    *dic = key->set_dictionary         ? key->dictionary         : global->dictionary;
    *ic  = key->set_ignore_case        ? key->ignore_case        : global->ignore_case;
    *gn  = key->set_general_numeric    ? key->general_numeric    : global->general_numeric;
    *hn  = key->set_human_numeric      ? key->human_numeric      : global->human_numeric;
    *mo  = key->set_month              ? key->month              : global->month;
    *inp = key->set_ignore_nonprinting ? key->ignore_nonprinting : global->ignore_nonprinting;
    *num = key->set_numeric            ? key->numeric            : global->numeric;
    *rnd = key->set_random             ? key->random             : global->random;
    *ver = key->set_version            ? key->version            : global->version;
    *rev = key->set_reverse            ? key->reverse            : global->reverse;
    if (*gn || *hn || *mo || *num) {
        *ib = 1;
    }
}

/**
 * @brief Dispatch to the correct specialized comparison function based on
 *        the resolved option set.  The chosen ordering is: random > version
 *        > month > human > general/numeric > lexicographic.
 */
static int _sort_cmp_values(const char * a, size_t alen, const char * b, size_t blen,
                            int dictionary, int ignore_nonprinting, int fold_case,
                            int general_numeric, int human_numeric, int month,
                            int numeric, int random, int version)
{
    if (random) {
        unsigned long ha = _sort_hash(a, alen);
        unsigned long hb = _sort_hash(b, blen);
        if (ha < hb) { return -1; }
        if (ha > hb) { return  1; }
        return 0;
    }
    else if (version) {
        return _sort_version_cmp(a, alen, b, blen);
    }
    else if (month) {
        return _sort_month_cmp(a, alen, b, blen);
    }
    else if (human_numeric) {
        return _sort_human_cmp(a, alen, b, blen);
    }
    else if (general_numeric || numeric) {
        return _sort_numeric_cmp(a, alen, b, blen);
    }
    else {
        return _sort_str_cmp(a, alen, b, blen, dictionary, ignore_nonprinting, fold_case);
    }
}

/**
 * @brief Compare two sort lines via a single key definition.
 *
 * Extracts the key from both lines and calls the value comparator.
 * If the key has a per-key reverse modifier, the result is negated.
 */
static int _sort_cmp_by_key(const sort_line_t * la, const sort_line_t * lb,
                            const key_spec_t * key, const sort_opts_t * opts)
{
    int ib, dic, ic, gn, hn, mo, inp, num, rnd, ver, rev;
    _sort_resolve_key_opts(key, opts, &ib, &dic, &ic, &gn, &hn, &mo, &inp, &num, &rnd, &ver, &rev);

    const char * ka;
    const char * kb;
    size_t ka_len;
    size_t kb_len;
    _sort_extract_key(la->text, la->len, key, opts, opts->field_sep, &ka, &ka_len, &ib);
    _sort_extract_key(lb->text, lb->len, key, opts, opts->field_sep, &kb, &kb_len, &ib);

    int result = _sort_cmp_values(ka, ka_len, kb, kb_len, dic, inp, ic, gn, hn, mo, num, rnd, ver);
    if (rev) {
        result = -result;
    }
    return result;
}

/**
 * @brief Top-level comparator used by sorting, -m merge, and -c check.
 *
 * When the caller has defined key(s) each key is consulted in order
 * until a non-zero result is found; when no keys are defined the whole
 * line is used as a single key using the global options.
 *
 * If no key produces a difference and @p opts->stable is not set, a
 * last-resort byte comparison across the original line bytes and
 * lengths is used (GNU behavior).
 *
 * @param la                left line
 * @param lb                right line
 * @param opts              sort options
 * @param last_resort_used  [out,optional] set to 1 when the final
 *                          byte-level tiebreaker was needed, 0 otherwise
 * @return -1 / 0 / 1 comparison result
 */
static int _sort_compare(const sort_line_t * la, const sort_line_t * lb,
                         const sort_opts_t * opts, int * last_resort_used)
{
    int result = 0;
    if (opts->nkeys > 0) {
        for (int i = 0; i < opts->nkeys; i++) {
            result = _sort_cmp_by_key(la, lb, &opts->keys[i], opts);
            if (result != 0) {
                if (last_resort_used) { *last_resort_used = 0; }
                return result;
            }
        }
    }
    else {
        int ib = opts->ignore_blanks;
        int gn = opts->general_numeric;
        int hn = opts->human_numeric;
        int mo = opts->month;
        int num = opts->numeric;
        if (gn || hn || mo || num) {
            ib = 1;
        }
        const char * ka = la->text;
        size_t kl = la->len;
        if (ib) {
            while (kl > 0 && isspace((unsigned char)*ka)) { ka++; kl--; }
        }
        const char * kb = lb->text;
        size_t ll = lb->len;
        if (ib) {
            while (ll > 0 && isspace((unsigned char)*kb)) { kb++; ll--; }
        }
        result = _sort_cmp_values(ka, kl, kb, ll,
                                  opts->dictionary, opts->ignore_nonprinting, opts->ignore_case,
                                  gn, hn, mo, num, opts->random, opts->version);
        if (opts->reverse) {
            result = -result;
        }
        if (result != 0) {
            if (last_resort_used) { *last_resort_used = 0; }
            return result;
        }
    }

    if (!opts->stable && !opts->unique) {
        size_t m = (la->len < lb->len) ? la->len : lb->len;
        int r = 0;
        if (m > 0) {
            r = memcmp(la->text, lb->text, m);
        }
        if (r == 0) {
            if (la->len < lb->len)      { r = -1; }
            else if (la->len > lb->len) { r =  1; }
        }
        if (r != 0) {
            if (last_resort_used) { *last_resort_used = 1; }
            return r;
        }
    }
    if (last_resort_used) { *last_resort_used = 0; }
    return 0;
}

/**
 * @brief Thunk used by qsort() — casts the @c void* arguments to
 *        sort_line_t and delegates to _sort_compare().  Uses the
 *        global g_sort_opts set by main().
 */
static int _sort_qsort_cmp(const void * a, const void * b)
{
    const sort_line_t * la = (const sort_line_t *)a;
    const sort_line_t * lb = (const sort_line_t *)b;
    int lr_used = 0;
    int result = _sort_compare(la, lb, g_sort_opts, &lr_used);
    if (result == 0 && g_sort_opts->stable) {
        if (la->index < lb->index) { return -1; }
        if (la->index > lb->index) { return  1; }
    }
    return result;
}

/**
 * @brief K-way merge of already-sorted file chunks for the `-m` flag.
 *
 * @p file_offsets is an array of length @p nfiles + 1 that gives the
 * start (inclusive) index of each file chunk in @p arr->lines[] .  The
 * algorithm does a simple linear scan over the head of each file to
 * pick the minimum, which is fine because we keep everything in memory
 * and never expected a truly huge number of files (the implementation
 * is correctness-first, performance is bounded by k-way merge cost).
 */
static void _sort_merge_k(line_array_t * arr, const sort_opts_t * opts,
                          size_t * file_offsets, int nfiles)
{
    if (nfiles <= 1 || arr->count <= 1) {
        return;
    }

    sort_line_t * tmp = (sort_line_t *)malloc(arr->count * sizeof(sort_line_t));
    if (!tmp) {
        qsort(arr->lines, arr->count, sizeof(sort_line_t), _sort_qsort_cmp);
        return;
    }

    size_t * ptrs = (size_t *)malloc((size_t)nfiles * sizeof(size_t));
    if (!ptrs) {
        free(tmp);
        qsort(arr->lines, arr->count, sizeof(sort_line_t), _sort_qsort_cmp);
        return;
    }

    for (int f = 0; f < nfiles; f++) {
        ptrs[f] = file_offsets[f];
    }
    size_t out = 0;
    g_sort_opts = (sort_opts_t *)opts;

    while (1) {
        int best = -1;
        for (int f = 0; f < nfiles; f++) {
            if (ptrs[f] >= file_offsets[f + 1]) {
                continue;
            }
            if (best < 0) {
                best = f;
                continue;
            }
            int lr = 0;
            int c = _sort_compare(&arr->lines[ptrs[f]], &arr->lines[ptrs[best]], opts, &lr);
            if (c < 0) {
                best = f;
            }
        }
        if (best < 0) { break; }
        tmp[out++] = arr->lines[ptrs[best]];
        ptrs[best]++;
    }

    memcpy(arr->lines, tmp, arr->count * sizeof(sort_line_t));
    free(tmp);
    free(ptrs);
}

/**
 * @brief -c / -C check: verify that @p arr is already sorted according
 *        to the options.  With -u additionally fail on equal
 *        consecutive elements.
 *
 * @return 0 if sorted, 1 if disorder found (and writes the offending
 *         line to stderr unless opts->check == 2 silent mode).
 */
static int _sort_check(const line_array_t * arr, const sort_opts_t * opts)
{
    int silent = (opts->check == 2);
    for (size_t i = 1; i < arr->count; i++) {
        int lr = 0;
        int cmp = _sort_compare(&arr->lines[i - 1], &arr->lines[i], opts, &lr);
        if (cmp > 0) {
            if (!silent) {
                fprintf(stderr, "sort: disorder: ");
                fwrite(arr->lines[i].text, 1, arr->lines[i].len, stderr);
                fputc('\n', stderr);
            }
            return 1;
        }
        if (opts->unique && cmp == 0) {
            if (!silent) {
                fprintf(stderr, "sort: disorder: ");
                fwrite(arr->lines[i].text, 1, arr->lines[i].len, stderr);
                fputc('\n', stderr);
            }
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Write the (already sorted) lines to the output destination.
 *
 * Honors:
 *   - -o FILE                 : fopen the target instead of stdout
 *   - -u / --unique           : skip lines equal to their predecessor
 *   - -z / --zero-terminated  : write '\0' instead of '\n'
 *   - --debug                 : emit key boundary markers (spaces + ^)
 *                               and the key content to stderr after each line
 */
static int _sort_output(const line_array_t * arr, const sort_opts_t * opts)
{
    FILE * out = sort_out_stream;
    if (opts->output_file) {
        out = fopen(opts->output_file, opts->zero_terminated ? "wb" : "w");
        if (!out) {
            fprintf(stderr, "sort: %s: %s\n", opts->output_file, strerror(errno));
            return 0;
        }
    }

    int term = opts->zero_terminated ? '\0' : '\n';
    for (size_t i = 0; i < arr->count; i++) {
        if (opts->unique && i > 0) {
            int lr = 0;
            int cmp = _sort_compare(&arr->lines[i - 1], &arr->lines[i], opts, &lr);
            if (cmp == 0) {
                continue;
            }
        }
        /*
         * When writing to the default stream (stdout / sort_out_stream)
         * route bytes through sort_fwrite, which on a real Windows
         * console converts the internal UTF-8 representation to
         * UTF-16LE and emits it via WriteConsoleW — this guarantees
         * correct Chinese glyphs.  For --output=FILE or redirected
         * streams keep byte-exact fwrite to match GNU sort.
         */
        if (out == sort_out_stream) {
            if (arr->lines[i].len > 0) {
                (void)sort_fwrite(arr->lines[i].text, 1, arr->lines[i].len, out);
            }
            { char _tc = (char)term; (void)sort_fwrite(&_tc, 1, 1, out); }
        } else {
            if (arr->lines[i].len > 0) {
                (void)fwrite(arr->lines[i].text, 1, arr->lines[i].len, out);
            }
            (void)fputc(term, out);
        }

        if (opts->debug) {
            if (opts->nkeys > 0) {
                for (int k = 0; k < opts->nkeys; k++) {
                    const key_spec_t * key = &opts->keys[k];
                    int ib, dic, ic, gn, hn, mo, inp, num, rnd, ver, rev;
                    _sort_resolve_key_opts(key, opts, &ib, &dic, &ic, &gn, &hn, &mo, &inp, &num, &rnd, &ver, &rev);
                    const char * ks;
                    size_t kl;
                    _sort_extract_key(arr->lines[i].text, arr->lines[i].len, key, opts, opts->field_sep,
                                      &ks, &kl, &ib);
                    if (ks && ks >= arr->lines[i].text && ks <= arr->lines[i].text + arr->lines[i].len) {
                        size_t offset = (size_t)(ks - arr->lines[i].text);
                        for (size_t j = 0; j < offset; j++) {
                            fputc(' ', stderr);
                        }
                        for (size_t j = 0; j < kl; j++) {
                            fputc('^', stderr);
                        }
                        if (kl == 0) {
                            fputc('^', stderr);
                        }
                        fputc('\n', stderr);
                    }
                }
            }
            else {
                for (size_t j = 0; j < arr->lines[i].len; j++) {
                    fputc('^', stderr);
                }
                fputc('\n', stderr);
            }
        }
    }

    if (out != sort_out_stream) {
        fclose(out);
    }
    return 1;
}

/**
 * @brief Free all line payloads and the line array itself.
 *        Safe to call on a zero-initialized line_array_t.
 */
static void _sort_free_lines(line_array_t * arr)
{
    for (size_t i = 0; i < arr->count; i++) {
        sort_safe_free(arr->lines[i].text);
    }
    sort_safe_free(arr->lines);
    arr->count = 0;
    arr->capacity = 0;
}

/**
 * @brief Free the dynamic key array inside the options struct.
 *        Does not free char* option values (they point into argv[]).
 */
static void _sort_free_opts(sort_opts_t * opts)
{
    sort_safe_free(opts->keys);
    opts->nkeys = 0;
}
