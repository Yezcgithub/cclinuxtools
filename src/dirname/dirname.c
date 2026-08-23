/**
 * @file dirname.c
 * @brief Cross-platform implementation of the GNU coreutils dirname command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils dirname(1).
 *
 * Key behaviors:
 *   - NAME...                     each NAME stripped of its last component
 *   - -z, --zero                  end each output line with NUL, not newline
 *   - --help                      display help and exit
 *   - --version                   output version information and exit
 *   - if NAME has no slash, output "." (current directory)
 *   - if NAME is all slashes, output "/"
 *   - if NAME is empty, output "."
 *   - trailing slashes on NAME are removed before component stripping
 *   - on Windows both '/' and '\' are treated as path separators
 *
 * Platform <resource> sources:
 *   Linux:     stdio, stdlib, string, errno
 *   Windows:   stdio, windows.h, shellapi.h, io.h, fcntl.h
 *   macOS:     stdio, stdlib, string, errno
 *   FreeBSD:   stdio, stdlib, string, errno
 *   OpenBSD:   stdio, stdlib, string, errno
 *   NetBSD:    stdio, stdlib, string, errno
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN -o dirname.exe dirname.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -o dirname dirname.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -o dirname dirname.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o dirname dirname.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o dirname dirname.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -o dirname dirname.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/dirname>
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
    #define DIRNAME_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define DIRNAME_PLATFORM_LINUX   1
    #define DIRNAME_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define DIRNAME_PLATFORM_MACOS   1
    #define DIRNAME_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define DIRNAME_PLATFORM_FREEBSD 1
    #define DIRNAME_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define DIRNAME_PLATFORM_OPENBSD 1
    #define DIRNAME_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define DIRNAME_PLATFORM_NETBSD  1
    #define DIRNAME_PLATFORM_POSIX   1
#else
    #define DIRNAME_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef DIRNAME_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef DIRNAME_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef DIRNAME_PLATFORM_NETBSD
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
#include <stddef.h>

#ifdef DIRNAME_PLATFORM_WINDOWS
    #include <windows.h>
    #include <shellapi.h>
    #include <io.h>
    #include <fcntl.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Program version string (mirrors build banner). */
#define DIRNAME_VERSION_STR "v1.0.0"

/** @brief Scratch buffer for dirname_printf on Windows. */
#define DIRNAME_PRINTF_BUFSZ 2048U

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Parsed command-line options for one dirname invocation.
 */
typedef struct {
    bool zero;     /**< -z / --zero: NUL-terminate output instead of newline  */
} dirname_opts_t;

/********************************
 *    static prototypes
 ********************************/

/* Diagnostics + option parsing */
static void _dirname_print_help(void);
static void _dirname_print_version(void);
static int  _dirname_parse_opts(int argc, char ** argv,
                                dirname_opts_t * opts,
                                char *** names, int * nnames);

/* Core algorithm */
static char * _dirname_compute(const char * name);

/* Driver */
static int _dirname_run(const dirname_opts_t * opts, char ** names, int nnames);

/* Platform helpers */
#ifdef DIRNAME_PLATFORM_WINDOWS
static HANDLE  _dirname_std_handle_for_fd(int fd);
static bool    _dirname_is_console_stream(FILE * fp);
static size_t  _dirname_write_win32(const void * buf, size_t len, FILE * fp);
static char ** _dirname_argv_utf8_alloc(int * argc, char ** argv);
static void    _dirname_argv_utf8_free(int argc, char ** argv_utf8);
#endif

/********************************
 *    static variables
 ********************************/

/* dirname is stateless per invocation; no module-scoped state required. */

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for dirname_fwrite / dirname_fputs / dirname_fflush.
 *        Defaults to libc @c stdout .  Redefine externally to redirect.
 */
#ifndef dirname_out_stream
    #define dirname_out_stream stdout
#endif

#ifdef DIRNAME_PLATFORM_WINDOWS
/**
 * @brief Portable byte-write macro used by every text-output path.
 *        On Windows stdout/stderr are routed through @c _dirname_write_win32
 *        which converts UTF-8 bytes to UTF-16LE via WriteConsoleW for real
 *        console streams, and writes raw UTF-8 for disk redirections and pipes.
 *        Non-stdio streams use plain fwrite so test output is byte-identical.
 */
    #ifndef dirname_fwrite
        #define dirname_fwrite(buf, sz, cnt, fp) \
            _dirname_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef dirname_fwrite
        #define dirname_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print wrapper (printf-compatible).
 */
#ifndef dirname_printf
    #ifdef DIRNAME_PLATFORM_WINDOWS
        #define dirname_printf(fmt, ...) \
            do { \
                char _dnpf[DIRNAME_PRINTF_BUFSZ]; \
                int _dnpf_n = snprintf(_dnpf, sizeof(_dnpf), (fmt), ##__VA_ARGS__); \
                if (_dnpf_n > 0) { (void)_dirname_write_win32(_dnpf, (size_t)_dnpf_n, dirname_out_stream); } \
            } while (0)
    #else
        #define dirname_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Formatted print to @c stderr (diagnostics).
 */
#ifndef dirname_eprintf
    #ifdef DIRNAME_PLATFORM_WINDOWS
        #define dirname_eprintf(fmt, ...) \
            do { \
                char _dnepf[DIRNAME_PRINTF_BUFSZ]; \
                int _dnepf_n = snprintf(_dnepf, sizeof(_dnepf), (fmt), ##__VA_ARGS__); \
                if (_dnepf_n > 0) { (void)_dirname_write_win32(_dnepf, (size_t)_dnepf_n, stderr); } \
            } while (0)
    #else
        #define dirname_eprintf(fmt, ...) fprintf(stderr, (fmt), ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef dirname_fputs
    #define dirname_fputs(str, stream) \
        do { const char * _dnsp = (str); if (_dnsp) { (void)dirname_fwrite(_dnsp, 1, strlen(_dnsp), (stream)); } } while (0)
#endif

/**
 * @brief Flush the given stdio stream.
 */
#ifndef dirname_fflush
    #define dirname_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free: free(*p) and set the pointer to NULL.
 *        Callable when @p p itself is NULL (no-op).
 */
#ifndef dirname_safe_free
    #define dirname_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Program entry point.
 *
 * Parses the command line, applies Windows console setup, then hands off
 * to @c _dirname_run which computes and prints the directory prefix of
 * each operand.
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on any usage error
 */
int main(int argc, char ** argv)
{
    dirname_opts_t opts;
    char ** names     = NULL;
#ifdef DIRNAME_PLATFORM_WINDOWS
    char ** argv_utf8 = NULL;
#endif
    int     nnames    = 0;
    int     rc;

    memset(&opts, 0, sizeof(opts));
    opts.zero = false;

#ifdef DIRNAME_PLATFORM_WINDOWS
    /* Transcode argv from the C runtime codepage (typically ACP/CP936) to
     * UTF-8 so that error messages and path operands are byte-correct
     * regardless of the host's console configuration. */
    {
        int     wc  = 0;
        char ** u8a = _dirname_argv_utf8_alloc(&wc, argv);
        if (u8a) {
            argc      = wc;
            argv_utf8 = u8a;
            argv      = u8a;
        }
    }

    /* Attach to parent console (if spawned detached) and request UTF-8 I/O.
     * Real per-glyph correctness on legacy console hosts is handled by
     * _dirname_write_win32 / WriteConsoleW below. */
    (void)AttachConsole(ATTACH_PARENT_PROCESS);
    (void)SetConsoleOutputCP(65001U);
    (void)SetConsoleCP(65001U);
    /* Binary mode prevents CRLF translation so byte-exact tests stay stable. */
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    rc = _dirname_parse_opts(argc, argv, &opts, &names, &nnames);
    if (rc != 0) {
        dirname_safe_free(names);
#ifdef DIRNAME_PLATFORM_WINDOWS
        _dirname_argv_utf8_free(argc, argv_utf8);
#endif
        return (rc < 0) ? 1 : rc;
    }

    rc = _dirname_run(&opts, names, nnames);
    dirname_safe_free(names);
#ifdef DIRNAME_PLATFORM_WINDOWS
    _dirname_argv_utf8_free(argc, argv_utf8);
#endif
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information and exit with status 0 (GNU --help).
 */
static void _dirname_print_help(void)
{
    dirname_printf(
        "Usage: dirname [OPTION] NAME...\n"
        "Output each NAME with its last non-slash component and trailing slashes removed;\n"
        "if NAME contains no /'s, output '.' (meaning the current directory).\n"
        "\n"
        "  -z, --zero     end each output line with NUL, not newline\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "Examples:\n"
        "  dirname /usr/bin/sort      -> \"/usr/bin\"\n"
        "  dirname dir1/str dir2/str  -> \"dir1\" followed by \"dir2\"\n"
        "  dirname stdio.h            -> \".\"\n"
        "\n"
    );
    exit(0);
}

/**
 * @brief Print version information and exit with status 0 (GNU --version).
 */
static void _dirname_print_version(void)
{
    dirname_printf("dirname %s\n", DIRNAME_VERSION_STR);
    dirname_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    dirname_printf("%s", "License MIT: <https://mit-license.org/>\n");
    dirname_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    dirname_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
    exit(0);
}

/**
 * @brief Parse command-line options into @p opts and collect name operands.
 *
 * Recognised options:
 *   -z, --zero       NUL-terminate each output line
 *       --help       show help and exit
 *       --version    show version and exit
 *       --           end of options
 *
 * Non-option arguments are collected into a freshly allocated array whose
 * pointer is stored in @p *names and count in @p *nnames.  The caller must
 * free the array (but not the individual strings, which alias @c argv ).
 *
 * @param argc    argument count
 * @param argv    argument vector
 * @param opts    output options struct
 * @param names   output: array of name pointers (NULL if none)
 * @param nnames  output: number of names
 * @return 0 on success, -1 on usage error (message printed to stderr),
 *         1 if --help/--version was printed (caller should exit 0)
 */
static int _dirname_parse_opts(int argc, char ** argv,
                               dirname_opts_t * opts,
                               char *** names, int * nnames)
{
    int i;
    int nalloc = 0;
    char ** arr = NULL;

    if (!opts || !names || !nnames) { return -1; }
    *names  = NULL;
    *nnames = 0;

    if (argc < 1 || !argv) { return -1; }

    for (i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) { continue; }

        if (strcmp(arg, "--") == 0) {
            /* Everything after -- is a name operand. */
            i++;
            for (; i < argc; i++) {
                if (!argv[i]) { continue; }
                if (*nnames >= nalloc) {
                    int    newcap = (nalloc == 0) ? 4 : nalloc * 2;
                    char ** tmp = (char **)realloc(arr,
                                                   (size_t)newcap * sizeof(char *));
                    if (!tmp) {
                        dirname_eprintf("dirname: out of memory\n");
                        dirname_safe_free(arr);
                        return -1;
                    }
                    arr    = tmp;
                    nalloc = newcap;
                }
                arr[*nnames] = argv[i];
                (*nnames)++;
            }
            break;
        }

        if (strncmp(arg, "--", 2) == 0) {
            /* Long option. */
            if (strcmp(arg, "--zero") == 0) {
                opts->zero = true;
            }
            else if (strcmp(arg, "--help") == 0) {
                _dirname_print_help();
                dirname_safe_free(arr);
                return 1;
            }
            else if (strcmp(arg, "--version") == 0) {
                _dirname_print_version();
                dirname_safe_free(arr);
                return 1;
            }
            else {
                dirname_eprintf("dirname: unrecognized option '%s'\n", arg);
                dirname_eprintf("%s", "Try 'dirname --help' for more information.\n");
                dirname_safe_free(arr);
                return -1;
            }
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short option cluster: -z */
            int j;
            for (j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'z':
                        opts->zero = true;
                        break;
                    default:
                        dirname_eprintf("dirname: invalid option -- '%c'\n",
                                        arg[j]);
                        dirname_eprintf("%s",
                            "Try 'dirname --help' for more information.\n");
                        dirname_safe_free(arr);
                        return -1;
                }
            }
        }
        else {
            /* Name operand (includes "-" which is a valid name). */
            if (*nnames >= nalloc) {
                int    newcap = (nalloc == 0) ? 4 : nalloc * 2;
                char ** tmp = (char **)realloc(arr,
                                               (size_t)newcap * sizeof(char *));
                if (!tmp) {
                    dirname_eprintf("dirname: out of memory\n");
                    dirname_safe_free(arr);
                    return -1;
                }
                arr    = tmp;
                nalloc = newcap;
            }
            arr[*nnames] = arg;
            (*nnames)++;
        }
    }

    if (*nnames == 0) {
        dirname_eprintf("%s", "dirname: missing operand\n");
        dirname_eprintf("%s", "Try 'dirname --help' for more information.\n");
        dirname_safe_free(arr);
        return -1;
    }

    *names = arr;
    return 0;
}

/**
 * @brief Test whether @p ch is a path separator on the current platform.
 *
 * On POSIX only @c '/' is a separator.  On Windows both @c '/' and @c '\\'
 * are separators so paths like <tt>C:\\foo\\bar</tt> are handled correctly.
 */
static bool _dirname_is_sep(int ch)
{
    if (ch == '/') { return true; }
#ifdef DIRNAME_PLATFORM_WINDOWS
    if (ch == '\\') { return true; }
#endif
    return false;
}

/**
 * @brief Compute the directory prefix of @p name (GNU coreutils semantics).
 *
 * Algorithm (matches POSIX XBD 4.13 "Pathname Resolution" and GNU dirname):
 *   1. If @p name is NULL or empty, return ".".
 *   2. Strip trailing separators from a working copy, but keep at least
 *      one character if the string is entirely separators.
 *   3. If the stripped copy is empty (was all separators), return "/".
 *   4. Find the last separator in the stripped copy.  If none, return ".".
 *   5. Take everything up to (but excluding) that last separator.
 *   6. Strip trailing separators from this prefix.
 *   7. If the prefix is now empty, return "/" (the separator was at pos 0).
 *   8. Otherwise return the prefix.
 *
 * The returned string is freshly @c malloc 'd and must be freed by the caller.
 *
 * @param name  input path string (may be NULL or empty)
 * @return newly allocated directory prefix string
 */
static char * _dirname_compute(const char * name)
{
    char * buf;
    size_t len;
    size_t end;

    /* Step 1: NULL or empty -> "." */
    if (!name || name[0] == '\0') {
        char * dot = (char *)malloc(2U);
        if (dot) { dot[0] = '.'; dot[1] = '\0'; }
        return dot;
    }

    len = strlen(name);

    /* Make a working copy. */
    buf = (char *)malloc(len + 1U);
    if (!buf) { return NULL; }
    memcpy(buf, name, len + 1U);

    /* Step 2: strip trailing separators (keep at least one char). */
    end = len;
    while (end > 1U && _dirname_is_sep((unsigned char)buf[end - 1U])) {
        buf[--end] = '\0';
    }

    /* Step 3: if we stripped everything down to nothing (all separators),
     * the remaining single character is a separator -> return "/". */
    if (end == 1U && _dirname_is_sep((unsigned char)buf[0])) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }
    if (end == 0U) {
        /* Should not happen, but be safe. */
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }

    /* Step 4: find the last separator in the stripped string. */
    {
        size_t last_sep = (size_t)-1;  /* (size_t)-1 means "none" */
        size_t k;
        for (k = 0; k < end; k++) {
            if (_dirname_is_sep((unsigned char)buf[k])) {
                last_sep = k;
            }
        }

        /* No separator found -> "." */
        if (last_sep == (size_t)-1) {
            buf[0] = '.';
            buf[1] = '\0';
            return buf;
        }

        /* Step 5: prefix is buf[0 .. last_sep). */
        buf[last_sep] = '\0';

        /* Step 6: strip trailing separators from the prefix. */
        {
            size_t plen = last_sep;
            while (plen > 1U &&
                   _dirname_is_sep((unsigned char)buf[plen - 1U])) {
                buf[--plen] = '\0';
            }
        }

        /* Step 7: if prefix is empty, return "/". */
        if (buf[0] == '\0') {
            buf[0] = '/';
            buf[1] = '\0';
            return buf;
        }

        /* Step 8: return the prefix. */
        return buf;
    }
}

/**
 * @brief Compute and print the directory prefix for each name operand.
 *
 * @param opts   parsed options
 * @param names  array of name strings
 * @param nnames number of names
 * @return 0 on success, 1 on allocation failure
 */
static int _dirname_run(const dirname_opts_t * opts, char ** names, int nnames)
{
    int    i;
    int    exit_code = 0;
    int    delim;

    if (!opts || !names || nnames <= 0) { return 1; }
    delim = opts->zero ? '\0' : '\n';

    for (i = 0; i < nnames; i++) {
        char * result = _dirname_compute(names[i]);
        if (!result) {
            dirname_eprintf("dirname: out of memory\n");
            exit_code = 1;
            continue;
        }
        dirname_fputs(result, dirname_out_stream);
        {
            unsigned char d = (unsigned char)delim;
            (void)dirname_fwrite(&d, 1, 1, dirname_out_stream);
        }
        dirname_safe_free(result);
    }

    dirname_fflush(dirname_out_stream);
    return exit_code;
}

#ifdef DIRNAME_PLATFORM_WINDOWS

/**
 * @brief Map a libc fd (1/2) to its Windows standard handle.
 */
static HANDLE _dirname_std_handle_for_fd(int fd)
{
    if (fd == 1)      { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief True if @p fp is a real console stream (not a pipe/disk).
 */
static bool _dirname_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _dirname_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

/**
 * @brief Windows-aware byte emitter for stdout/stderr.
 *
 * Splits output by handle type:
 *   - console  : UTF-8 -> UTF-16LE -> WriteConsoleW (correct glyphs on CP936)
 *   - disk/pipe: raw UTF-8 (byte-exact for redirections and PowerShell captures)
 * Non-stdio streams use plain fwrite.
 */
static size_t _dirname_write_win32(const void * buf, size_t len, FILE * fp)
{
    int  fd;
    bool is_std;

    if (!fp) { return 0; }
    fd     = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /* Explicit temp streams etc. -> raw bytes. */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_dirname_is_console_stream(fp)) {
        int       wlen;
        wchar_t * wbuf;
        DWORD     written = 0;
        BOOL      ok;
        HANDLE    h = _dirname_std_handle_for_fd(fd);

        if (len == 0) { return 0; }
        wlen = MultiByteToWideChar(CP_UTF8, 0, (const char *)buf,
                                   (int)len, NULL, 0);
        if (wlen <= 0) { return fwrite(buf, 1, len, fp); }
        wbuf = (wchar_t *)malloc((size_t)(wlen + 1) * sizeof(wchar_t));
        if (!wbuf) { return fwrite(buf, 1, len, fp); }
        MultiByteToWideChar(CP_UTF8, 0, (const char *)buf,
                            (int)len, wbuf, wlen);
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
 * @brief Transcode argv from the Windows wide-char command line to UTF-8.
 *
 * MSVCRT's narrow argv is decoded via the ANSI code page, which corrupts
 * non-ACP bytes (e.g. UTF-8).  This helper re-acquires the original
 * @c CommandLineToArgvW wide vector and converts each element to UTF-8.
 *
 * @param pargc     output: number of arguments (may be NULL)
 * @param argv_orig original argv (unused, kept for API symmetry)
 * @return newly allocated UTF-8 argv array (NULL-terminated), or NULL on failure
 */
static char ** _dirname_argv_utf8_alloc(int * pargc, char ** argv_orig)
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
        int    wlen = (int)wcslen(wargv[i]);
        int    blen;
        char * buf;

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
        int j;
        for (j = 0; j < i; j++) { dirname_safe_free(u8a[j]); }
        dirname_safe_free(u8a);
        LocalFree(wargv);
        return NULL;
    }

    LocalFree(wargv);
    u8a[wargc] = NULL;
    *pargc = wargc;
    return u8a;
}

/**
 * @brief Release an argv vector produced by _dirname_argv_utf8_alloc.
 */
static void _dirname_argv_utf8_free(int argc, char ** argv_utf8)
{
    int i;
    if (!argv_utf8) { return; }
    for (i = 0; i < argc; i++) { dirname_safe_free(argv_utf8[i]); }
    dirname_safe_free(argv_utf8);
}

#endif  /* DIRNAME_PLATFORM_WINDOWS */
