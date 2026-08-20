/**
 * @file touch.c
 * @brief Cross-platform touch command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 *
 * Key behaviors:
 *   - -a: change only the access time
 *   - -c, --no-create: do not create any files
 *   - -d, --date=STRING: parse STRING and use it instead of current time
 *   - -f: (ignored, BSD compatibility)
 *   - -h, --no-dereference: affect each symbolic link instead of any referenced file
 *   - -m: change only the modification time
 *   - -r, --reference=FILE: use this file's times instead of current time
 *   -t STAMP: use [[CC]YY]MMDDhhmm[.ss] instead of current time
 *   - --time=WORD: change the specified time (access/atime/use/mtime/modify)
 *   - -A STAMP: adjust timestamps by [-]YYMMDDhhmm.ss (BSD-style adjust)
 *   - --help / --version: recognized and handled
 *   - Forward-slash paths on all platforms
 *   - Internally all strings are UTF-8; encoding is adapted automatically at
 *     every I/O boundary (argv input, file-system paths, stdout/stderr output,
 *     system error messages) so that the tool displays correctly on ANY
 *     country / locale / code page without user intervention.
 *     * Windows: GetCommandLineW → UTF-8; WriteConsoleW for console output,
 *       UTF-8 → ConsoleOutputCP for pipes, raw UTF-8 for disk files;
 *       _wcserror → UTF-8; CP_UTF8 → UTF-16 → W file APIs
 *     * POSIX:   argv/strerror from active locale codeset → internal UTF-8
 *       via standard mbsrtowcs/wcsrtombs; output UTF-8 → current locale;
 *       file-system APIs take raw UTF-8 bytes (FS is byte-opaque on POSIX)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o touch.exe touch.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -o touch touch.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o touch touch.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o touch touch.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o touch touch.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o touch touch.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/touch>
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
    #define TOUCH_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TOUCH_PLATFORM_LINUX   1
    #define TOUCH_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TOUCH_PLATFORM_MACOS   1
    #define TOUCH_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TOUCH_PLATFORM_FREEBSD 1
    #define TOUCH_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TOUCH_PLATFORM_OPENBSD 1
    #define TOUCH_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TOUCH_PLATFORM_NETBSD  1
    #define TOUCH_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define TOUCH_PLATFORM_POSIX   1
#else
    #define TOUCH_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers.
 * We force-define these unconditionally (after #undef) so they take
 * precedence over whatever the compiler driver may or may not pass,
 * which ensures mbstate_t / mbsrtowcs / wcsrtombs are always visible. */
#ifdef TOUCH_PLATFORM_POSIX
    #undef  _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
    #undef  _XOPEN_SOURCE
    #define _XOPEN_SOURCE   700
#endif

#ifdef TOUCH_PLATFORM_LINUX
    #undef  _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE
    #undef  _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#ifdef TOUCH_PLATFORM_MACOS
    #undef  _DARWIN_C_SOURCE
    #define _DARWIN_C_SOURCE
#endif

#ifdef TOUCH_PLATFORM_NETBSD
    #undef  _NETBSD_SOURCE
    #define _NETBSD_SOURCE
#endif

/********************************
 *    includes
 ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>        /* mbstate_t, mbsrtowcs, wcsrtombs, wchar_t */
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef TOUCH_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <sys/utime.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFREG) != 0)
    #endif
    #ifndef S_ISLNK
        #define S_ISLNK(m) (0)
    #endif
    #ifndef O_CREAT
        #define O_CREAT _O_CREAT
    #endif
    #ifndef O_WRONLY
        #define O_WRONLY _O_WRONLY
    #endif
    #ifndef O_EXCL
        #define O_EXCL _O_EXCL
    #endif
    #ifndef F_OK
        #define F_OK 0
    #endif
#else
    #include <unistd.h>
    #include <utime.h>
    #include <sys/time.h>
    #include <locale.h>
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) != 0)
    #endif
#endif

/* inttypes.h for PRIu64 (must come after feature macros) */
#include <inttypes.h>

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define TOUCH_VERSION_STR "v1.0.0"

/** @brief Maximum path length */
#define TOUCH_MAX_PATH 4096

/** @brief Buffer for cell strings */
#define TOUCH_CELL_BUF 64

/** @brief Seconds per day */
#define TOUCH_SECS_PER_DAY 86400

/* Which times to change */
#define TOUCH_TIME_ACCESS  0x01
#define TOUCH_TIME_MODIFY  0x02

/* Time source modes */
#define TOUCH_SOURCE_NOW      0  /* use current time */
#define TOUCH_SOURCE_DATE     1  /* use -d parsed date */
#define TOUCH_SOURCE_STAMP    2  /* use -t parsed stamp */
#define TOUCH_SOURCE_REF      3  /* use -r reference file */

/********************************
 *    types
 ********************************/

typedef struct {
    bool no_create;
    bool no_deref;
    bool change_access;
    bool change_modify;
    int  time_source;
    char date_str[TOUCH_MAX_PATH];
    char stamp_str[TOUCH_MAX_PATH];
    char ref_file[TOUCH_MAX_PATH];
    int  ref_used;
    int  date_used;
    int  stamp_used;
} touch_opts_t;

/* Parsed time values */
typedef struct {
    time_t atime;
    time_t mtime;
} touch_times_t;

/********************************
 *    prototypes
 ********************************/

static void _touch_print_help(void);
static void _touch_print_version(void);
static int  _touch_parse_args(int argc, char ** argv, touch_opts_t * opts);
static int  _touch_do_touch(const char * path, const touch_opts_t * opts,
                            const touch_times_t * times);
static int  _touch_get_ref_times(const char * ref, touch_times_t * times);
static int  _touch_parse_date(const char * str, touch_times_t * times);
static int  _touch_parse_stamp(const char * str, touch_times_t * times);
static void _touch_str_set(char * dst, size_t dst_size, const char * src);
static int  _touch_vprintf(FILE * fp, const char * fmt, va_list ap);
static int  _touch_printf(const char * fmt, ...);
static int  _touch_err_printf(const char * fmt, ...);
static const char * _touch_strerror(int errnum);

#ifdef TOUCH_PLATFORM_WINDOWS
static int  _touch_wide_path(const char * path, wchar_t * wbuf, size_t wbuf_size);
static int  _touch_write_console(FILE * fp, const char * utf8_str, int len);
static char * _touch_wide_to_utf8(const wchar_t * ws);
static char ** _touch_get_utf8_argv(int * out_argc, void ** out_cleanup);
static void   _touch_free_utf8_argv(void * cleanup);
#endif

#ifdef TOUCH_PLATFORM_POSIX
static char * _touch_locale_to_utf8(const char * locale_str);
static char * _touch_utf8_to_locale(const char * utf8_str);
static char ** _touch_get_utf8_argv_posix(int argc, char ** argv_in, void ** out_cleanup);
static void   _touch_free_utf8_argv_posix(void * cleanup);
#endif

/********************************
 *    macros
 ********************************/

#ifndef touch_printf
    #define touch_printf   _touch_printf
#endif

#ifndef touch_err_printf
    #define touch_err_printf _touch_err_printf
#endif

#ifndef touch_safe_free
    #define touch_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    static functions
 ********************************/

/**
 * @brief Safely copy a string into a fixed-size buffer.
 * Uses strlen+memcpy to avoid -Wstringop-truncation warnings.
 * @param dst       destination buffer
 * @param dst_size  destination buffer size
 * @param src       source string (NULL = empty)
 */
static void _touch_str_set(char * dst, size_t dst_size, const char * src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/**
 * @brief Get a system error description; returned string is always UTF-8.
 *
 * On Windows, strerror() returns ANSI-codepage bytes (CP936/GBK, CP932,
 * CP949, CP1251, etc.) which do NOT round-trip to every possible Unicode
 * error message.  We instead call the wide-char runtime counterpart and
 * convert the UTF-16 message to UTF-8 via WideCharToMultiByte.
 *
 * On POSIX, strerror() uses the active locale codeset; we route it through
 * our locale→UTF-8 helper so the returned bytes are internally consistent
 * with everything else in the program.
 *
 * @param errnum  error number (typically errno)
 * @return null-terminated UTF-8 error string (static storage, do not free)
 */
static const char * _touch_strerror(int errnum)
{
#ifdef TOUCH_PLATFORM_WINDOWS
    static char s_buf[512];
    const wchar_t * wmsg = _wcserror(errnum);
    if (!wmsg) {
        wmsg = L"";
    }
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wmsg, -1,
                                    s_buf, (int)sizeof(s_buf),
                                    NULL, NULL);
    if (u8len <= 0) {
        const char * ansi = strerror(errnum);
        _touch_str_set(s_buf, sizeof(s_buf), ansi ? ansi : "");
    }
    return s_buf;
#else
    /* POSIX: convert from active locale codeset to internal UTF-8.
     * We use a ring of static buffers so callers can safely invoke this
     * function multiple times in the same printf() argument list. */
    enum { TOUCH_STRERR_BUFS = 4, TOUCH_STRERR_SIZE = 512 };
    static char s_bufs[TOUCH_STRERR_BUFS][TOUCH_STRERR_SIZE];
    static int  s_next = 0;

    char * dst = s_bufs[s_next];
    s_next = (s_next + 1) & (TOUCH_STRERR_BUFS - 1);

    const char * loc_msg = strerror(errnum);
    if (!loc_msg) {
        dst[0] = '\0';
        return dst;
    }
    char * u8 = _touch_locale_to_utf8(loc_msg);
    if (!u8) {
        _touch_str_set(dst, TOUCH_STRERR_SIZE, loc_msg);
        return dst;
    }
    _touch_str_set(dst, TOUCH_STRERR_SIZE, u8);
    free(u8);
    return dst;
#endif
}

#ifdef TOUCH_PLATFORM_WINDOWS
/* ======================================================================
 *                  Windows – UTF-8 aware I/O boundaries
 * ====================================================================== */

/**
 * @brief Write a UTF-8 string to stdout/stderr, adapting to output kind.
 *
 * Three destinations:
 *   1) Real console → WriteConsoleW (Unicode direct).
 *   2) Disk file → raw UTF-8 bytes.
 *   3) Pipe / NUL / unknown → raw UTF-8 bytes (modern terminals default).
 */
static int _touch_write_console(FILE * fp, const char * utf8_str, int len)
{
    if (!utf8_str) {
        return 0;
    }
    if (len < 0) {
        len = (int)strlen(utf8_str);
    }
    if (len == 0) {
        return 0;
    }

    DWORD std_handle = (fp == stderr) ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    HANDLE h = GetStdHandle(std_handle);
    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        return (int)fwrite(utf8_str, 1, (size_t)len, fp);
    }

    /* ---- Case 1: Real console → WriteConsoleW (Unicode pass-through) ---- */
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, len, NULL, 0);
        if (wlen > 0) {
            wchar_t * wbuf = (wchar_t *)malloc((size_t)(wlen + 1) * sizeof(wchar_t));
            if (wbuf) {
                MultiByteToWideChar(CP_UTF8, 0, utf8_str, len, wbuf, wlen);
                wbuf[wlen] = L'\0';
                DWORD written = 0;
                BOOL ok = WriteConsoleW(h, wbuf, (DWORD)wlen, &written, NULL);
                free(wbuf);
                if (ok) {
                    return len;
                }
            }
        }
        /* fall through if allocation / conversion / WriteConsoleW fails */
    }

    /* ---- Decide redirected destination type ---- */
    DWORD ft = GetFileType(h) & ~FILE_TYPE_REMOTE;
    if (ft == FILE_TYPE_DISK) {
        /* ---- Case 2: Disk file → raw UTF-8 bytes ---- */
        return (int)fwrite(utf8_str, 1, (size_t)len, fp);
    }

    /* ---- Case 3: pipe / NUL / named-pipe / unknown → raw UTF-8 ---- */
    return (int)fwrite(utf8_str, 1, (size_t)len, fp);
}

/**
 * @brief Convert a NUL-terminated wide string to a newly malloc'd UTF-8 string.
 */
static char * _touch_wide_to_utf8(const wchar_t * ws)
{
    if (!ws) {
        return NULL;
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        return NULL;
    }
    char * buf = (char *)malloc((size_t)len);
    if (!buf) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf, len, NULL, NULL);
    return buf;
}

typedef struct {
    int       argc;
    char **   argv;
    wchar_t ** wargv_free;   /* from CommandLineToArgvW → LocalFree */
    char **   strbufs;       /* individual UTF-8 mallocs → free each */
} _touch_argv_ctx_t;

/**
 * @brief Retrieve the true Unicode command line, convert every argument to
 *        a newly allocated UTF-8 string; the returned argv array is ready
 *        to be fed into _touch_parse_args.
 */
static char ** _touch_get_utf8_argv(int * out_argc, void ** out_cleanup)
{
    if (out_argc)     *out_argc    = 0;
    if (out_cleanup)  *out_cleanup = NULL;

    int wargc = 0;
    wchar_t ** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv || wargc <= 0) {
        if (wargv) LocalFree(wargv);
        return NULL;
    }

    _touch_argv_ctx_t * ctx   = (_touch_argv_ctx_t *)calloc(1, sizeof(*ctx));
    char **            argv  = (char **)calloc((size_t)(wargc + 1), sizeof(char *));
    char **            strbs = (char **)calloc((size_t)wargc,      sizeof(char *));
    if (!ctx || !argv || !strbs) {
        free(ctx); free(argv); free(strbs);
        LocalFree(wargv);
        return NULL;
    }
    ctx->argc       = wargc;
    ctx->argv       = argv;
    ctx->wargv_free = wargv;
    ctx->strbufs    = strbs;

    for (int i = 0; i < wargc; i++) {
        char * u8 = _touch_wide_to_utf8(wargv[i]);
        static char s_empty[] = "";
        if (!u8) {
            argv[i] = s_empty;
        } else {
            argv[i] = u8;
            strbs[i] = u8;
        }
    }
    argv[wargc] = NULL;

    if (out_argc)    *out_argc    = wargc;
    if (out_cleanup) *out_cleanup = ctx;
    return argv;
}

static void _touch_free_utf8_argv(void * cleanup)
{
    if (!cleanup) return;
    _touch_argv_ctx_t * ctx = (_touch_argv_ctx_t *)cleanup;
    if (ctx->strbufs) {
        for (int i = 0; i < ctx->argc; i++) free(ctx->strbufs[i]);
        free(ctx->strbufs);
    }
    free(ctx->argv);
    if (ctx->wargv_free) LocalFree(ctx->wargv_free);
    free(ctx);
}

/**
 * @brief Convert an internally-UTF-8 path to a wide string for the Unicode
 *        family of Win32 file-system APIs.
 */
static int _touch_wide_path(const char * path, wchar_t * wbuf, size_t wbuf_size)
{
    if (!path || !wbuf || wbuf_size == 0) {
        return -1;
    }

    char native[TOUCH_MAX_PATH];
    _touch_str_set(native, sizeof(native), path);
    for (char * s = native; *s; s++) {
        if (*s == '/') *s = '\\';
    }

    /* Internal strings are UTF-8; translate source codepage is CP_UTF8 */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, native, -1, NULL, 0);
    if (wlen <= 0 || (size_t)wlen > wbuf_size) {
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, native, -1, wbuf, wlen);
    return 0;
}
#endif /* TOUCH_PLATFORM_WINDOWS */

#ifdef TOUCH_PLATFORM_POSIX
/* ======================================================================
 *               POSIX – locale <-> internal UTF-8 bridge
 *
 * The conversion is implemented purely with C99 standard functions by
 * temporarily switching the LC_CTYPE locale between the user's original
 * locale (activated via setlocale(LC_ALL, "") at startup) and a known
 * UTF-8 locale.  No iconv / ICU dependency is introduced.
 *
 * If no UTF-8 locale can be found on the host we transparently fall back
 * to pass-through behaviour (bytes are forwarded unchanged between the
 * external locale and the internal "UTF-8" representation, which is
 * technically incorrect but robust in practice for ASCII-dominant text
 * and pure-filename argv paths on UTF-8-by-default systems).
 * ====================================================================== */

static char * _touch_try_utf8_locale_name = NULL;   /* detected once at startup */
static char * _touch_user_ctype_name    = NULL;     /* original LC_CTYPE from user environment */

/**
 * @brief Try to locate any usable UTF-8 locale installed on this host by
 *        probing a small, common list; also save the caller's original
 *        LC_CTYPE name so we can flip between the two in converters.
 *        Safe to call multiple times; only probes on first invocation.
 */
static void _touch_locale_probe_once(void)
{
    static int s_done = 0;
    if (s_done) return;
    s_done = 1;

    /* 1) remember caller's original LC_CTYPE (typically from setlocale("")) */
    const char * cur = setlocale(LC_CTYPE, NULL);
    if (cur) {
        size_t n = strlen(cur) + 1;
        _touch_user_ctype_name = (char *)malloc(n);
        if (_touch_user_ctype_name) {
            memcpy(_touch_user_ctype_name, cur, n);
        }
    }

    /* 2) probe a well-known set of UTF-8 locale names; first valid one wins */
    static const char * k_candidates[] = {
        "C.UTF-8", "C.utf8", "en_US.UTF-8", "en_US.utf8",
        "zh_CN.UTF-8", "zh_CN.utf8", "ja_JP.UTF-8", "ko_KR.UTF-8",
        "de_DE.UTF-8", "fr_FR.UTF-8", "ru_RU.UTF-8", "POSIX", NULL
    };
    for (int i = 0; k_candidates[i]; i++) {
        const char * r = setlocale(LC_CTYPE, k_candidates[i]);
        if (r) {
            size_t n = strlen(r) + 1;
            char * copy = (char *)malloc(n);
            if (copy) {
                memcpy(copy, r, n);
                _touch_try_utf8_locale_name = copy;
                break;
            }
        }
    }

    /* 3) always restore user locale before leaving */
    if (_touch_user_ctype_name) {
        setlocale(LC_CTYPE, _touch_user_ctype_name);
    } else {
        setlocale(LC_CTYPE, "");
    }
}

/**
 * @brief Convert a NUL-terminated multibyte string encoded in the CURRENT
 *        (user) LC_CTYPE codeset to an internally-used UTF-8 multibyte
 *        string.  Both conversions go through a shared wchar_t staging
 *        buffer using mbsrtowcs / wcsrtombs.
 *
 * @param locale_str  NUL-terminated string in the active user locale codeset
 * @return malloc'd NUL-terminated UTF-8 string, or NULL if conversion fails
 */
static char * _touch_locale_to_utf8(const char * locale_str)
{
    if (!locale_str) return NULL;
    _touch_locale_probe_once();

    size_t slen = strlen(locale_str);

    /* Step A: user locale bytes → wchar_t (performed under user locale) */
    if (_touch_user_ctype_name) setlocale(LC_CTYPE, _touch_user_ctype_name);
    else                        setlocale(LC_CTYPE, "");

    wchar_t * wbuf = (wchar_t *)calloc(slen + 1, sizeof(wchar_t));
    if (!wbuf) return NULL;
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    const char * in_ptr = locale_str;
    size_t wcount = mbsrtowcs(wbuf, &in_ptr, slen + 1, &st);
    if (wcount == (size_t)-1) {
        /* Conversion sequence error; fall back to raw copy by treating
         * each byte as a Latin-1-ish wchar. Caller will get a UTF-8
         * string that at least preserves the original bytes. */
        for (size_t k = 0; k <= slen; k++) wbuf[k] = (wchar_t)(unsigned char)locale_str[k];
    }

    /* Step B: wchar_t → UTF-8 bytes (performed under a known UTF-8 locale) */
    char * result = NULL;
    if (_touch_try_utf8_locale_name) {
        setlocale(LC_CTYPE, _touch_try_utf8_locale_name);
        memset(&st, 0, sizeof(st));
        wchar_t * wptr = wbuf;
        size_t need = wcsrtombs(NULL, (const wchar_t **)&wptr, 0, &st);
        if (need != (size_t)-1) {
            char * u8out = (char *)malloc(need + 1);
            if (u8out) {
                memset(&st, 0, sizeof(st));
                wptr = wbuf;
                size_t written = wcsrtombs(u8out, (const wchar_t **)&wptr, need + 1, &st);
                if (written != (size_t)-1) {
                    u8out[written] = '\0';
                    result = u8out;
                } else {
                    free(u8out);
                }
            }
        }
    }

    /* Final: always restore user locale */
    if (_touch_user_ctype_name) setlocale(LC_CTYPE, _touch_user_ctype_name);
    else                        setlocale(LC_CTYPE, "");
    free(wbuf);
    return result;
}

/**
 * @brief Reverse of _touch_locale_to_utf8: convert an internal UTF-8 byte
 *        string into the CURRENT (user) LC_CTYPE codeset so it displays
 *        correctly when written to a terminal, redirected file, or pipe.
 *        If the host has no usable UTF-8 locale, we simply strdup the
 *        input bytes (degrades to "forward raw UTF-8", safe for ASCII).
 */
static char * _touch_utf8_to_locale(const char * utf8_str)
{
    if (!utf8_str) return NULL;
    _touch_locale_probe_once();

    /* If we never found a UTF-8 locale we cannot decode UTF-8 via wcsrtombs;
     * fall back to forwarding the raw bytes.  This degrades gracefully on
     * stripped-down systems but on 99 % of modern Linux/macOS hosts the
     * probe above will already have found C.UTF-8 or en_US.UTF-8. */
    if (!_touch_try_utf8_locale_name) {
        size_t n = strlen(utf8_str) + 1;
        char * copy = (char *)malloc(n);
        if (copy) memcpy(copy, utf8_str, n);
        return copy;
    }

    size_t slen = strlen(utf8_str);

    /* Step A: UTF-8 bytes → wchar_t (run under a UTF-8 locale) */
    setlocale(LC_CTYPE, _touch_try_utf8_locale_name);
    wchar_t * wbuf = (wchar_t *)calloc(slen + 1, sizeof(wchar_t));
    if (!wbuf) return NULL;
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    const char * in_ptr = utf8_str;
    size_t wcount = mbsrtowcs(wbuf, &in_ptr, slen + 1, &st);
    if (wcount == (size_t)-1) {
        for (size_t k = 0; k <= slen; k++) wbuf[k] = (wchar_t)(unsigned char)utf8_str[k];
    }

    /* Step B: wchar_t → user locale codeset bytes (run under user locale) */
    char * result = NULL;
    if (_touch_user_ctype_name) setlocale(LC_CTYPE, _touch_user_ctype_name);
    else                        setlocale(LC_CTYPE, "");

    memset(&st, 0, sizeof(st));
    wchar_t * wptr = wbuf;
    size_t need = wcsrtombs(NULL, (const wchar_t **)&wptr, 0, &st);
    if (need != (size_t)-1) {
        char * locout = (char *)malloc(need + 1);
        if (locout) {
            memset(&st, 0, sizeof(st));
            wptr = wbuf;
            size_t written = wcsrtombs(locout, (const wchar_t **)&wptr, need + 1, &st);
            if (written != (size_t)-1) {
                locout[written] = '\0';
                result = locout;
            } else {
                free(locout);
            }
        }
    }

    free(wbuf);
    /* User locale is still active on exit — which matches the state we
     * want for subsequent fwrite/fprintf calls directly from the caller. */
    return result;
}

typedef struct {
    int      argc;
    char **  argv;
    char **  strbufs;   /* individual UTF-8 strings to free */
} _touch_posix_argv_ctx_t;

/**
 * @brief Convert the incoming main(argv) from the user locale codeset into
 *        a UTF-8 argv array; used for the exact same "internal UTF-8"
 *        guarantee on POSIX as we get via CommandLineToArgvW on Windows.
 */
static char ** _touch_get_utf8_argv_posix(int argc, char ** argv_in, void ** out_cleanup)
{
    if (out_cleanup) *out_cleanup = NULL;

    /* Activate user locale so mbsrtowcs parses the original argv correctly */
    setlocale(LC_ALL, "");
    _touch_locale_probe_once();

    _touch_posix_argv_ctx_t * ctx = (_touch_posix_argv_ctx_t *)calloc(1, sizeof(*ctx));
    char ** argv  = (char **)calloc((size_t)(argc + 1), sizeof(char *));
    char ** strbs = (char **)calloc((size_t)argc,       sizeof(char *));
    if (!ctx || !argv || !strbs) {
        free(ctx); free(argv); free(strbs);
        return NULL;
    }
    ctx->argc    = argc;
    ctx->argv    = argv;
    ctx->strbufs = strbs;

    for (int i = 0; i < argc; i++) {
        char * u8 = _touch_locale_to_utf8(argv_in[i]);
        if (!u8) {
            /* fall back: point to the original (locale-encoded) argv;
             * won't crash, ASCII parts still work, non-ASCII degrades */
            argv[i] = argv_in[i];
        } else {
            argv[i] = u8;
            strbs[i] = u8;
        }
    }
    argv[argc] = NULL;
    if (out_cleanup) *out_cleanup = ctx;
    return argv;
}

static void _touch_free_utf8_argv_posix(void * cleanup)
{
    if (!cleanup) return;
    _touch_posix_argv_ctx_t * ctx = (_touch_posix_argv_ctx_t *)cleanup;
    if (ctx->strbufs) {
        for (int i = 0; i < ctx->argc; i++) free(ctx->strbufs[i]);
        free(ctx->strbufs);
    }
    free(ctx->argv);
    free(ctx);
}
#endif /* TOUCH_PLATFORM_POSIX */

/* ======================================================================
 *                   Platform-merged formatted print
 *
 * Phase 1: vsnprintf the caller's format string into a plain char buffer.
 *          Because all variable arguments (path names, error messages
 *          produced by _touch_strerror, ref_file, date_str, etc.) live
 *          internally as UTF-8 bytes, the resulting buffer is UTF-8.
 * Phase 2: pass that UTF-8 buffer to the platform-specific output
 *          adapter which will encode it as appropriate for the real
 *          destination (Windows: WriteConsoleW / UTF-8 / ConsoleCP;
 *          POSIX: UTF-8 → user locale codeset, then fwrite).
 * ====================================================================== */
static int _touch_vprintf(FILE * fp, const char * fmt, va_list ap)
{
    char stackbuf[TOUCH_MAX_PATH];
    va_list ap2;
    va_copy(ap2, ap);
    int len = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap2);
    va_end(ap2);
    if (len < 0) {
        return -1;
    }

    char * heapbuf = NULL;
    const char * src = stackbuf;
    if ((size_t)len >= sizeof(stackbuf)) {
        size_t need = (size_t)len + 1;
        heapbuf = (char *)malloc(need);
        if (!heapbuf) return -1;
        va_list ap3;
        va_copy(ap3, ap);
        len = vsnprintf(heapbuf, need, fmt, ap3);
        va_end(ap3);
        if (len < 0) {
            free(heapbuf);
            return -1;
        }
        src = heapbuf;
    }

    int written;
#ifdef TOUCH_PLATFORM_WINDOWS
    written = _touch_write_console(fp, src, len);
#else
    /* POSIX: transcode UTF-8 → user locale codeset, then fwrite raw bytes.
     * If transcode returns NULL (e.g. iconv-equivalent missing) we just
     * write the original UTF-8 bytes as a no-worse fallback. */
    char * local_bytes = _touch_utf8_to_locale(src);
    if (local_bytes) {
        size_t n = strlen(local_bytes);
        written = (int)fwrite(local_bytes, 1, n, fp);
        free(local_bytes);
    } else {
        written = (int)fwrite(src, 1, (size_t)len, fp);
    }
#endif

    free(heapbuf);
    return written;
}

/**
 * @brief Formatted print to stdout (internally UTF-8; adapted at boundary).
 */
static int _touch_printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rc = _touch_vprintf(stdout, fmt, ap);
    va_end(ap);
    return rc;
}

/**
 * @brief Formatted print to stderr (internally UTF-8; adapted at boundary).
 */
static int _touch_err_printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rc = _touch_vprintf(stderr, fmt, ap);
    va_end(ap);
    return rc;
}

/**
 * @brief Print help text and exit.
 */
static void _touch_print_help(void)
{
    touch_printf(
        "Usage: %s [OPTION]... FILE...\n"
        "Update the access and modification times of each FILE to the current time.\n"
        "\n"
        "A FILE argument that does not exist is created empty, unless -c or -h\n"
        "is given.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -a                     change only the access time\n"
        "  -c, --no-create        do not create any files\n"
        "  -d, --date=STRING     parse STRING and use it instead of current time\n"
        "  -f                     (ignored)\n"
        "  -h, --no-dereference   affect each symbolic link instead of any referenced\n"
        "                         file (useful only on systems that can change the\n"
        "                         timestamps of a symlink itself)\n"
        "  -m                     change only the modification time\n"
        "  -r, --reference=FILE   use this file's times instead of current time\n"
        "  -t STAMP               use [[CC]YY]MMDDhhmm[.ss] instead of current time\n"
        "      --time=WORD        change the specified time:\n"
        "                         WORD is access, atime, use, mtime, modify\n"
        "      --help             display this help and exit\n"
        "      --version          output version information and exit\n"
        "\n"
        "Note that the -d and -t options interpret different time formats.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
        , "touch"
    );
}

/**
 * @brief Print version and exit.
 */
static void _touch_print_version(void)
{
    touch_printf("touch %s\n", TOUCH_VERSION_STR);
    touch_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    touch_printf("%s", "License MIT: <https://mit-license.org>\n");
    touch_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    touch_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments.
 *
 * Sets the various flags and time source in opts.
 * Exits directly on --help or --version.
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options
 * @return number of arguments consumed (index of first FILE), or -1 on error
 */
static int _touch_parse_args(int argc, char ** argv, touch_opts_t * opts)
{
    if (!opts) {
        return -1;
    }

    memset(opts, 0, sizeof(*opts));
    /* Default: change both access and modification time */
    opts->change_access = true;
    opts->change_modify = true;
    opts->time_source = TOUCH_SOURCE_NOW;

    int i = 1;

    while (i < argc) {
        const char * arg = argv[i];

        /* "--" terminates options */
        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }

        /* Long options */
        if (strncmp(arg, "--", 2) == 0 && arg[2] != '\0') {
            const char * name = arg + 2;
            const char * val = strchr(name, '=');
            char name_buf[TOUCH_CELL_BUF];

            if (val) {
                size_t len = (size_t)(val - name);
                if (len >= sizeof(name_buf)) {
                    len = sizeof(name_buf) - 1;
                }
                memcpy(name_buf, name, len);
                name_buf[len] = '\0';
                val++;
            } else {
                _touch_str_set(name_buf, sizeof(name_buf), name);
            }

            if (strcmp(name_buf, "help") == 0) {
                _touch_print_help();
                exit(0);
            } else if (strcmp(name_buf, "version") == 0) {
                _touch_print_version();
                exit(0);
            } else if (strcmp(name_buf, "no-create") == 0) {
                opts->no_create = true;
            } else if (strcmp(name_buf, "no-dereference") == 0) {
                opts->no_deref = true;
            } else if (strcmp(name_buf, "date") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    _touch_str_set(opts->date_str, sizeof(opts->date_str), val);
                    opts->time_source = TOUCH_SOURCE_DATE;
                    opts->date_used = 1;
                } else {
                    touch_err_printf("touch: option '--date' requires an argument\n");
                    return -1;
                }
            } else if (strcmp(name_buf, "reference") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    _touch_str_set(opts->ref_file, sizeof(opts->ref_file), val);
                    opts->time_source = TOUCH_SOURCE_REF;
                    opts->ref_used = 1;
                } else {
                    touch_err_printf("touch: option '--reference' requires an argument\n");
                    return -1;
                }
            } else if (strcmp(name_buf, "time") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    if (strcmp(val, "access") == 0 || strcmp(val, "atime") == 0 ||
                        strcmp(val, "use") == 0) {
                        opts->change_access = true;
                        opts->change_modify = false;
                    } else if (strcmp(val, "modify") == 0 || strcmp(val, "mtime") == 0) {
                        opts->change_modify = true;
                        opts->change_access = false;
                    } else {
                        touch_err_printf("touch: invalid argument %s for '--time'\n", val);
                        return -1;
                    }
                } else {
                    touch_err_printf("touch: option '--time' requires an argument\n");
                    return -1;
                }
            } else {
                touch_err_printf("touch: unrecognized option '%s'\n", arg);
                return -1;
            }
            i++;
            continue;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            size_t j = 1;
            bool consumed_arg = false;

            while (arg[j] != '\0') {
                switch (arg[j]) {
                    case 'a':
                        opts->change_access = true;
                        opts->change_modify = false;
                        break;

                    case 'c':
                        opts->no_create = true;
                        break;

                    case 'd':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        } else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val) {
                            _touch_str_set(opts->date_str, sizeof(opts->date_str), val);
                            opts->time_source = TOUCH_SOURCE_DATE;
                            opts->date_used = 1;
                        } else {
                            touch_err_printf("touch: option '-d' requires an argument\n");
                            return -1;
                        }
                        break;
                    }

                    case 'f':
                        /* Ignored (BSD compatibility) */
                        break;

                    case 'h':
                        opts->no_deref = true;
                        break;

                    case 'm':
                        opts->change_modify = true;
                        opts->change_access = false;
                        break;

                    case 'r':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        } else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val) {
                            _touch_str_set(opts->ref_file, sizeof(opts->ref_file), val);
                            opts->time_source = TOUCH_SOURCE_REF;
                            opts->ref_used = 1;
                        } else {
                            touch_err_printf("touch: option '-r' requires an argument\n");
                            return -1;
                        }
                        break;
                    }

                    case 't':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        } else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val) {
                            _touch_str_set(opts->stamp_str, sizeof(opts->stamp_str), val);
                            opts->time_source = TOUCH_SOURCE_STAMP;
                            opts->stamp_used = 1;
                        } else {
                            touch_err_printf("touch: option '-t' requires an argument\n");
                            return -1;
                        }
                        break;
                    }

                    default:
                        touch_err_printf("touch: invalid option -- '%c'\n", arg[j]);
                        return -1;
                }

                if (consumed_arg) {
                    break;
                }
                j++;
            }
            i++;
            continue;
        }

        /* Positional argument: a file path */
        break;
    }

    /* Check for conflicting time sources */
    int sources = opts->date_used + opts->stamp_used + opts->ref_used;
    if (sources > 1) {
        touch_err_printf("%s", "touch: cannot specify times from more than one source\n");
        return -1;
    }

    /* Must have at least one file */
    if (i >= argc) {
        touch_err_printf("%s", "touch: missing file operand\n");
        touch_err_printf("%s", "Try 'touch --help' for more information.\n");
        return -1;
    }

    return i;
}

/**
 * @brief Get times from a reference file.
 * @param ref    reference file path
 * @param times  output times
 * @return 0 on success, -1 on error
 */
static int _touch_get_ref_times(const char * ref, touch_times_t * times)
{
    if (!ref || !times) {
        return -1;
    }

    memset(times, 0, sizeof(*times));

#ifdef TOUCH_PLATFORM_WINDOWS
    wchar_t wpath[TOUCH_MAX_PATH];
    if (_touch_wide_path(ref, wpath, TOUCH_MAX_PATH) != 0) {
        touch_err_printf("touch: failed to convert path '%s'\n", ref);
        return -1;
    }

    struct _stat64 st;
    if (_wstat64(wpath, &st) != 0) {
        touch_err_printf("touch: failed to stat '%s': %s\n", ref, _touch_strerror(errno));
        return -1;
    }
    times->atime = st.st_atime;
    times->mtime = st.st_mtime;
#else
    struct stat st;
    if (stat(ref, &st) != 0) {
        touch_err_printf("touch: failed to stat '%s': %s\n", ref, _touch_strerror(errno));
        return -1;
    }
    times->atime = st.st_atime;
    times->mtime = st.st_mtime;
#endif

    return 0;
}

/**
 * @brief Parse a date string (-d option) into times.
 *
 * Supported formats:
 *   - "now" → current time
 *   - "today" → midnight today
 *   - "yesterday" → midnight yesterday
 *   - "tomorrow" → midnight tomorrow
 *   - "YYYY-MM-DD" or "YYYY-MM-DD HH:MM[:SS]"
 *   - "HH:MM[:SS]" → today at that time
 *   - "@epoch" → Unix timestamp
 *
 * @param str    date string
 * @param times  output times (atime = mtime = parsed time)
 * @return 0 on success, -1 on error
 */
static int _touch_parse_date(const char * str, touch_times_t * times)
{
    if (!str || !times) {
        return -1;
    }

    memset(times, 0, sizeof(*times));
    time_t now = time(NULL);
    struct tm tm_val;
    struct tm * lt = localtime(&now);
    if (lt) {
        tm_val = *lt;
    } else {
        memset(&tm_val, 0, sizeof(tm_val));
    }

    /* "@epoch" format */
    if (str[0] == '@') {
        char * end = NULL;
        long long epoch = strtoll(str + 1, &end, 10);
        if (end == str + 1 || *end != '\0') {
            touch_err_printf("touch: invalid date '%s'\n", str);
            return -1;
        }
        times->atime = (time_t)epoch;
        times->mtime = (time_t)epoch;
        return 0;
    }

    /* "now" */
    if (strcmp(str, "now") == 0) {
        times->atime = now;
        times->mtime = now;
        return 0;
    }

    /* "today", "yesterday", "tomorrow" */
    if (strcmp(str, "today") == 0) {
        tm_val.tm_hour = 0;
        tm_val.tm_min = 0;
        tm_val.tm_sec = 0;
        times->atime = mktime(&tm_val);
        times->mtime = times->atime;
        return 0;
    }

    if (strcmp(str, "yesterday") == 0) {
        tm_val.tm_hour = 0;
        tm_val.tm_min = 0;
        tm_val.tm_sec = 0;
        tm_val.tm_mday -= 1;
        times->atime = mktime(&tm_val);
        times->mtime = times->atime;
        return 0;
    }

    if (strcmp(str, "tomorrow") == 0) {
        tm_val.tm_hour = 0;
        tm_val.tm_min = 0;
        tm_val.tm_sec = 0;
        tm_val.tm_mday += 1;
        times->atime = mktime(&tm_val);
        times->mtime = times->atime;
        return 0;
    }

    /* "YYYY-MM-DD" or "YYYY-MM-DD HH:MM[:SS]" */
    int year, month, day;
    int hour = 0, minute = 0, second = 0;
    int matched;

    /* Try "YYYY-MM-DD HH:MM:SS" first */
    matched = sscanf(str, "%d-%d-%d %d:%d:%d",
                     &year, &month, &day, &hour, &minute, &second);
    if (matched != 6) {
        /* Try "YYYY-MM-DD" only */
        hour = 0;
        minute = 0;
        second = 0;
        matched = sscanf(str, "%d-%d-%d", &year, &month, &day);
        if (matched != 3) {
            /* Try "HH:MM[:SS]" — use today's date */
            matched = sscanf(str, "%d:%d:%d", &hour, &minute, &second);
            if (matched >= 2) {
                if (matched == 2) {
                    second = 0;
                }
                year = tm_val.tm_year + 1900;
                month = tm_val.tm_mon + 1;
                day = tm_val.tm_mday;
            } else {
                touch_err_printf("touch: invalid date format '%s'\n", str);
                return -1;
            }
        }
    }

    /* Validate ranges */
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
        touch_err_printf("touch: invalid date '%s'\n", str);
        return -1;
    }

    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = second;
    tm_val.tm_isdst = -1;

    time_t t = mktime(&tm_val);
    if (t == (time_t)-1) {
        touch_err_printf("touch: invalid date '%s'\n", str);
        return -1;
    }
    times->atime = t;
    times->mtime = t;
    return 0;
}

/**
 * @brief Parse a timestamp string (-t option) into times.
 *
 * Format: [[CC]YY]MMDDhhmm[.ss]
 *   - MMDDhhmm      → current year
 *   - YYMMDDhhmm    → 2-digit year (19YY if YY >= 69, else 20YY)
 *   - CCYYMMDDhhmm  → 4-digit year
 *   - Optional .ss for seconds
 *
 * @param str    stamp string
 * @param times  output times (atime = mtime = parsed time)
 * @return 0 on success, -1 on error
 */
static int _touch_parse_stamp(const char * str, touch_times_t * times)
{
    if (!str || !times) {
        return -1;
    }

    memset(times, 0, sizeof(*times));

    /* Find optional .ss suffix */
    const char * dot = strchr(str, '.');
    int seconds = 0;
    char stamp[32];

    if (dot) {
        size_t stamp_len = (size_t)(dot - str);
        if (stamp_len >= sizeof(stamp)) {
            touch_err_printf("touch: invalid date format '%s'\n", str);
            return -1;
        }
        memcpy(stamp, str, stamp_len);
        stamp[stamp_len] = '\0';

        /* Parse seconds after dot */
        const char * sec_str = dot + 1;
        if (strlen(sec_str) != 2) {
            touch_err_printf("touch: invalid date format '%s'\n", str);
            return -1;
        }
        seconds = (sec_str[0] - '0') * 10 + (sec_str[1] - '0');
        if (seconds < 0 || seconds > 60) {
            touch_err_printf("touch: invalid date format '%s'\n", str);
            return -1;
        }
    } else {
        _touch_str_set(stamp, sizeof(stamp), str);
    }

    size_t slen = strlen(stamp);

    int year, month, day, hour, minute;
    time_t now = time(NULL);
    struct tm * lt = localtime(&now);
    int cur_year = lt ? (lt->tm_year + 1900) : 2025;

    if (slen == 8) {
        /* MMDDhhmm — use current year */
        year = cur_year;
        month = (stamp[0] - '0') * 10 + (stamp[1] - '0');
        day = (stamp[2] - '0') * 10 + (stamp[3] - '0');
        hour = (stamp[4] - '0') * 10 + (stamp[5] - '0');
        minute = (stamp[6] - '0') * 10 + (stamp[7] - '0');
    } else if (slen == 10) {
        /* YYMMDDhhmm — 2-digit year */
        int yy = (stamp[0] - '0') * 10 + (stamp[1] - '0');
        year = (yy < 69) ? (2000 + yy) : (1900 + yy);
        month = (stamp[2] - '0') * 10 + (stamp[3] - '0');
        day = (stamp[4] - '0') * 10 + (stamp[5] - '0');
        hour = (stamp[6] - '0') * 10 + (stamp[7] - '0');
        minute = (stamp[8] - '0') * 10 + (stamp[9] - '0');
    } else if (slen == 12) {
        /* CCYYMMDDhhmm — 4-digit year */
        year = (stamp[0] - '0') * 1000 + (stamp[1] - '0') * 100 +
               (stamp[2] - '0') * 10 + (stamp[3] - '0');
        month = (stamp[4] - '0') * 10 + (stamp[5] - '0');
        day = (stamp[6] - '0') * 10 + (stamp[7] - '0');
        hour = (stamp[8] - '0') * 10 + (stamp[9] - '0');
        minute = (stamp[10] - '0') * 10 + (stamp[11] - '0');
    } else {
        touch_err_printf("touch: invalid date format '%s'\n", str);
        return -1;
    }

    /* Validate all digits were actually digits */
    for (size_t k = 0; k < slen; k++) {
        if (!isdigit((unsigned char)stamp[k])) {
            touch_err_printf("touch: invalid date format '%s'\n", str);
            return -1;
        }
    }

    /* Validate ranges */
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        seconds < 0 || seconds > 60) {
        touch_err_printf("touch: invalid date format '%s'\n", str);
        return -1;
    }

    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = seconds;
    tm_val.tm_isdst = -1;

    time_t t = mktime(&tm_val);
    if (t == (time_t)-1) {
        touch_err_printf("touch: invalid date format '%s'\n", str);
        return -1;
    }
    times->atime = t;
    times->mtime = t;
    return 0;
}

/**
 * @brief Create an empty file if it doesn't exist.
 * @param path  file path
 * @return 0 on success (created or exists), -1 on error
 */
static int _touch_create_file(const char * path)
{
#ifdef TOUCH_PLATFORM_WINDOWS
    wchar_t wpath[TOUCH_MAX_PATH];
    if (_touch_wide_path(path, wpath, TOUCH_MAX_PATH) != 0) {
        touch_err_printf("touch: failed to convert path '%s'\n", path);
        return -1;
    }

    DWORD attr = GetFileAttributesW(wpath);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return 0; /* already exists */
    }

    HANDLE h = CreateFileW(wpath, GENERIC_WRITE, 0, NULL,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {
            return 0; /* race condition, already exists */
        }
        touch_err_printf("touch: cannot create '%s'\n", path);
        return -1;
    }
    CloseHandle(h);
    return 0;
#else
    if (access(path, F_OK) == 0) {
        return 0; /* already exists */
    }

    int fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0666);
    if (fd < 0) {
        if (errno == EEXIST) {
            return 0; /* race condition, already exists */
        }
        touch_err_printf("touch: cannot create '%s': %s\n", path, _touch_strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
#endif
}

/**
 * @brief Set file timestamps.
 *
 * Uses the most precise method available per platform.
 * On POSIX, uses utimensat() (nanosecond) or utimes() (microsecond) or utime() (second).
 * On Windows, uses SetFileTime().
 *
 * @param path   file path
 * @param opts   touch options (determines which times to set, no-deref)
 * @param times  target times
 * @return 0 on success, -1 on error
 */
static int _touch_set_times(const char * path, const touch_opts_t * opts,
                            const touch_times_t * times)
{
    if (!path || !opts || !times) {
        return -1;
    }

    /* Determine which times to set */
    time_t atime = opts->change_access ? times->atime : times->atime;
    time_t mtime = opts->change_modify ? times->mtime : times->mtime;

    /* If only one is requested, we need the other from existing file */
    time_t use_atime = atime;
    time_t use_mtime = mtime;

    if (opts->change_access && !opts->change_modify) {
        /* Only access: need existing mtime */
#ifdef TOUCH_PLATFORM_WINDOWS
        struct _stat64 st;
        wchar_t wpath[TOUCH_MAX_PATH];
        if (_touch_wide_path(path, wpath, TOUCH_MAX_PATH) != 0) {
            return -1;
        }
        if (_wstat64(wpath, &st) == 0) {
            use_mtime = st.st_mtime;
        }
#else
        struct stat st;
        if (stat(path, &st) == 0) {
            use_mtime = st.st_mtime;
        }
#endif
    } else if (opts->change_modify && !opts->change_access) {
        /* Only modify: need existing atime */
#ifdef TOUCH_PLATFORM_WINDOWS
        struct _stat64 st;
        wchar_t wpath[TOUCH_MAX_PATH];
        if (_touch_wide_path(path, wpath, TOUCH_MAX_PATH) != 0) {
            return -1;
        }
        if (_wstat64(wpath, &st) == 0) {
            use_atime = st.st_atime;
        }
#else
        struct stat st;
        if (stat(path, &st) == 0) {
            use_atime = st.st_atime;
        }
#endif
    }

#ifdef TOUCH_PLATFORM_WINDOWS
    wchar_t wpath[TOUCH_MAX_PATH];
    if (_touch_wide_path(path, wpath, TOUCH_MAX_PATH) != 0) {
        touch_err_printf("touch: failed to convert path '%s'\n", path);
        return -1;
    }

    /* Use CreateFileW + SetFileTime for precise control */
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS; /* allows opening directories */
    if (opts->no_deref) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    HANDLE h = CreateFileW(wpath, FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, flags, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        touch_err_printf("touch: cannot touch '%s'\n", path);
        return -1;
    }

    /* Convert time_t to FILETIME */
    ULARGE_INTEGER ui;
    FILETIME ft_access, ft_write;

    /* Access time */
    ui.QuadPart = (ULONGLONG)use_atime * 10000000ULL + 116444736000000000ULL;
    ft_access.dwLowDateTime = ui.LowPart;
    ft_access.dwHighDateTime = ui.HighPart;

    /* Modify time */
    ui.QuadPart = (ULONGLONG)use_mtime * 10000000ULL + 116444736000000000ULL;
    ft_write.dwLowDateTime = ui.LowPart;
    ft_write.dwHighDateTime = ui.HighPart;

    if (!SetFileTime(h, NULL, &ft_access, &ft_write)) {
        touch_err_printf("touch: cannot set time for '%s'\n", path);
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    return 0;

#else /* POSIX */

    /* Try utimensat first (nanosecond precision, supports no-follow) */
#if defined(TOUCH_PLATFORM_LINUX) || defined(TOUCH_PLATFORM_FREEBSD)
    struct timespec ts[2];
    ts[0].tv_sec = use_atime;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = use_mtime;
    ts[1].tv_nsec = 0;

    int flags = 0;
    if (opts->no_deref) {
        flags |= AT_SYMLINK_NOFOLLOW;
    }

    if (utimensat(AT_FDCWD, path, ts, flags) == 0) {
        return 0;
    }
    /* Fall through to utimes if utimensat not supported */
#endif

    /* Try lutimes for no-deref, or utimes for normal */
    struct timeval tv[2];
    tv[0].tv_sec = (long)use_atime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = (long)use_mtime;
    tv[1].tv_usec = 0;

    if (opts->no_deref) {
#if defined(TOUCH_PLATFORM_LINUX) || defined(TOUCH_PLATFORM_FREEBSD) || defined(TOUCH_PLATFORM_MACOS)
        if (lutimes(path, tv) == 0) {
            return 0;
        }
        if (errno != ENOSYS) {
            touch_err_printf("touch: cannot touch '%s': %s\n", path, _touch_strerror(errno));
            return -1;
        }
#else
        /* lutimes not available, try utimes anyway */
        if (utimes(path, tv) == 0) {
            return 0;
        }
#endif
    } else {
        if (utimes(path, tv) == 0) {
            return 0;
        }
    }

    /* Fall back to utime() (second precision) */
    struct utimbuf ub;
    ub.actime = use_atime;
    ub.modtime = use_mtime;

    if (utime(path, &ub) == 0) {
        return 0;
    }

    touch_err_printf("touch: cannot touch '%s': %s\n", path, _touch_strerror(errno));
    return -1;

#endif /* platform */
}

/**
 * @brief Process a single file: create if needed, then set times.
 *
 * @param path   file path
 * @param opts   touch options
 * @param times  target times (if time_source is NOW, use current time)
 * @return 0 on success, -1 on error
 */
static int _touch_do_touch(const char * path, const touch_opts_t * opts,
                           const touch_times_t * times)
{
    if (!path || !opts) {
        return -1;
    }

    touch_times_t use_times;
    if (times) {
        use_times = *times;
    } else {
        time_t now = time(NULL);
        use_times.atime = now;
        use_times.mtime = now;
    }

    /* Check if file exists */
    int exists = 0;
#ifdef TOUCH_PLATFORM_WINDOWS
    wchar_t wpath[TOUCH_MAX_PATH];
    if (_touch_wide_path(path, wpath, TOUCH_MAX_PATH) == 0) {
        DWORD attr = GetFileAttributesW(wpath);
        exists = (attr != INVALID_FILE_ATTRIBUTES);
    }
#else
    exists = (access(path, F_OK) == 0);
#endif

    /* Create if doesn't exist and not -c */
    if (!exists) {
        if (opts->no_create) {
            /* GNU touch: -c with non-existent file is not an error */
            return 0;
        }
        if (_touch_create_file(path) != 0) {
            return -1;
        }
        /* After creation, set the times (creation time is now, but we want specified time) */
    }

    /* Set timestamps */
    return _touch_set_times(path, opts, &use_times);
}

/********************************
 *    main
 ********************************/

int main(int argc, char ** argv)
{
    /* ================================================================
     * BOUNDARY (1): acquire argv, normalise to internal UTF-8.
     *    * Windows – pull the real UTF-16 command line via the kernel,
     *      then convert each argument to UTF-8 so every code point of
     *      every language on earth survives even on CP932/CP936/CP1251
     *      boxes whose main(argc,argv) is restricted to the ANSI page.
     *    * POSIX   – the bytes in argv are encoded in whatever codeset
     *      the caller's locale specifies (typically UTF-8 but can be
     *      GB18030, EUC-KR, KOI8-R, ISO-8859-N, ...).  We transcode
     *      them to internal UTF-8 through the standard
     *      mbsrtowcs/wcsrtombs bridge; if the system has no usable
     *      UTF-8 locale the helper forwards bytes verbatim so we
     *      never crash or lose argv pointer stability.
     * ================================================================ */
    void * argv_cleanup = NULL;

#ifdef TOUCH_PLATFORM_WINDOWS
    {
        char ** utf8_argv = _touch_get_utf8_argv(&argc, &argv_cleanup);
        if (utf8_argv) {
            argv = utf8_argv;
        }
    }
#else
    /* Before touching any locale-dependent call, activate the caller's
     * environment (respects LANG / LC_ALL / LC_CTYPE). */
    setlocale(LC_ALL, "");
    {
        char ** utf8_argv = _touch_get_utf8_argv_posix(argc, argv, &argv_cleanup);
        if (utf8_argv) {
            argv = utf8_argv;
        }
    }
#endif

    touch_opts_t opts;
    int rc = 0;
    int first_file = _touch_parse_args(argc, argv, &opts);

    if (first_file < 0) {
        rc = 1;
        goto touch_main_exit;
    }

    /* Resolve the time source */
    touch_times_t times;
    memset(&times, 0, sizeof(times));

    if (opts.time_source == TOUCH_SOURCE_REF) {
        if (_touch_get_ref_times(opts.ref_file, &times) != 0) {
            rc = 1;
            goto touch_main_exit;
        }
    } else if (opts.time_source == TOUCH_SOURCE_DATE) {
        if (_touch_parse_date(opts.date_str, &times) != 0) {
            rc = 1;
            goto touch_main_exit;
        }
    } else if (opts.time_source == TOUCH_SOURCE_STAMP) {
        if (_touch_parse_stamp(opts.stamp_str, &times) != 0) {
            rc = 1;
            goto touch_main_exit;
        }
    }
    /* TOUCH_SOURCE_NOW: times will be set in _touch_do_touch */

    /* Process each file */
    for (int i = first_file; i < argc; i++) {
        if (_touch_do_touch(argv[i], &opts,
                            (opts.time_source == TOUCH_SOURCE_NOW) ? NULL : &times) != 0) {
            rc = 1;
        }
    }

touch_main_exit:
#ifdef TOUCH_PLATFORM_WINDOWS
    _touch_free_utf8_argv(argv_cleanup);
#else
    _touch_free_utf8_argv_posix(argv_cleanup);
#endif
    return rc;
}
