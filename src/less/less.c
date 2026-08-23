/**
 * @file less.c
 * @brief Cross-platform implementation of the Linux less command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU less(1).
 *
 * Key behaviors:
 *   - -E, --QUIT-AT-EOF          quit at end of file (force streaming)
 *   - -e, --quit-at-eof          quit at end of file
 *   - -F, --quit-if-one-screen   quit if file fits on one screen
 *   - -N, --LINE-NUMBERS         display line numbers
 *   - -n                         line numbers (interactive only)
 *   - -s, --squeeze-blank-lines  squeeze multiple blank lines
 *   - -J, --status-column        display status column
 *   - -S, --chop-long-lines      chop long lines (no effect when streaming)
 *   - -x N, --tabs=N             set tab stops every N characters
 *   - -R, --RAW-CONTROL-CHARS    pass ANSI escape sequences through
 *   - -r, --raw-control-chars    pass raw control characters through
 *   - -i, --ignore-case          case-insensitive search (soft)
 *   - -I, --IGNORE-CASE          case-insensitive search (hard)
 *   - -p PATTERN                 jump to first match of PATTERN
 *   - -~, --no-tilde             don't display ~ marks after end of file
 *   - -f, --force                force open non-regular files
 *   - -o FILE, --log-file=FILE   copy input to FILE (refuse overwrite)
 *   - -O FILE, --LOG-FILE=FILE   copy input to FILE (overwrite)
 *   - -P STR,  --prompt=STR      set prompt string (interactive only)
 *   - -K, --no-keypad            stub (no keypad init/deinit)
 *   - -L, --no-less-open         stub (ignore LESSOPEN pipe)
 *   - -g, --hilite-search        stub (highlight first match only)
 *   - -G, --HILITE-SEARCH        stub (no search highlighting)
 *   - --buffers=N                stub (set buffer count)
 *   - --max-back-scroll=N        stub (set max back scroll)
 *   - +cmd                       apply initial command (+N, +F, +/pat)
 *   - --help                     display help and exit
 *   - --version                  output version and exit
 *   - [FILE...]                  input files (default: stdin)
 *
 * Platform <resource> sources:
 *   Linux:     stdio, termios, sys/ioctl.h, unistd.h
 *   Windows:   windows.h, io.h, fcntl.h
 *   macOS:     stdio, termios, sys/ioctl.h, unistd.h
 *   FreeBSD:   stdio, termios, sys/ioctl.h, unistd.h
 *   OpenBSD:   stdio, termios, sys/ioctl.h, unistd.h
 *   NetBSD:    stdio, termios, sys/ioctl.h, unistd.h
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN -o less.exe less.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o less less.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o less less.c
 * Build (FreeBSD):  cc  -O2 -std=c99 -Wall -Wextra -o less less.c
 * Build (OpenBSD):  cc  -O2 -std=c99 -Wall -Wextra -o less less.c
 * Build (NetBSD):   cc  -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o less less.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/less>
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
    #define LESS_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define LESS_PLATFORM_LINUX   1
    #define LESS_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define LESS_PLATFORM_MACOS   1
    #define LESS_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define LESS_PLATFORM_FREEBSD 1
    #define LESS_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define LESS_PLATFORM_OPENBSD 1
    #define LESS_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define LESS_PLATFORM_NETBSD  1
    #define LESS_PLATFORM_POSIX   1
#else
    #define LESS_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef LESS_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef LESS_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef LESS_PLATFORM_NETBSD
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
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

#ifdef LESS_PLATFORM_WINDOWS
    #include <windows.h>
    #include <shellapi.h>
    #include <io.h>
    #include <fcntl.h>
    #include <conio.h>          /* _getch() for raw keystroke reading       */
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>          /* open() / O_RDONLY for /dev/tty fallback   */
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/ioctl.h>      /* TIOCGWINSZ on every POSIX platform        */
#endif

/********************************
 *    defines
 ********************************/

/**
 * @brief Program version string (mirrors build banner).
 */
#define LESS_VERSION_STR     "v1.0.0"

/**
 * @brief Default tab-stop width (columns) when -x/--tabs is not given.
 */
#define LESS_DEFAULT_TABS    8U

/**
 * @brief Line-number field width in characters (right-justified).
 */
#define LESS_LNO_WIDTH      7U

/**
 * @brief Initial capacity of the line buffer used by the streaming reader.
 */
#define LESS_LINE_INIT_CAP  256U

/**
 * @brief Scratch buffer size for the formatted-print helper on Windows.
 */
#define LESS_PRINTF_BUFSZ   2048U

/**
 * @brief Fallback terminal size used when the real size cannot be queried.
 */
#define LESS_FALLBACK_ROWS  24U
#define LESS_FALLBACK_COLS  80U

/**
 * @brief Scratch buffer size for expanded line text and prompt strings.
 */
#define LESS_EXPAND_BUFSZ   8192U
#define LESS_PROMPT_BUFSZ   256U
#define LESS_SEARCH_BUFSZ   256U

/**
 * @brief Convenience flag: stdout is the canonical output stream.
 */
#ifndef LESS_OUT_STREAM
    #define LESS_OUT_STREAM  stdout
#endif

/**
 * @brief Convenience flag: stderr is the canonical error stream.
 */
#ifndef LESS_ERR_STREAM
    #define LESS_ERR_STREAM  stderr
#endif

/********************************
 *    typedefs
 ********************************/

/**
 * @brief All user-tunable options collected in one place.
 *
 * The structure is filled in by @c _less_parse_args and consumed by
 * the streaming / interactive dispatchers.  Stubs (K, L, g, G,
 * --buffers, --max-back-scroll, -P) are recorded so they are accepted
 * without effect on the streaming output, mirroring GNU less semantics
 * where those switches only matter when an interactive pager is launched.
 */
typedef struct
{
    bool      quit_at_eof;          /**< -e, --quit-at-eof             */
    bool      quit_at_eof_two;     /**< -E, --QUIT-AT-EOF (force)     */
    bool      quit_if_one_screen;   /**< -F, --quit-if-one-screen     */
    bool      force;                /**< -f, --force                  */
    bool      no_tilde;             /**< -~, --no-tilde                */
    bool      line_numbers;         /**< -N, --LINE-NUMBERS            */
    bool      line_numbers_soft;    /**< -n (interactive only)        */
    bool      squeeze_blank;        /**< -s, --squeeze-blank-lines    */
    bool      status_col;           /**< -J, --status-column           */
    bool      chop_long;            /**< -S, --chop-long-lines         */
    bool      raw_ctrl;             /**< -r, --raw-control-chars      */
    bool      raw_ctrl_ansi;        /**< -R, --RAW-CONTROL-CHARS      */
    bool      ignore_case_soft;     /**< -i, --ignore-case            */
    bool      ignore_case_hard;     /**< -I, --IGNORE-CASE            */
    bool      no_keypad;             /**< -K, --no-keypad (stub)       */
    bool      no_less_open;          /**< -L, --no-less-open (stub)    */
    bool      hilite_first;          /**< -g, --hilite-search (stub)   */
    bool      hilite_none;           /**< -G, --HILITE-SEARCH (stub)   */
    bool      follow_mode;           /**< +F (follow, no-op streaming)  */
    unsigned  tab_width;            /**< -x N, --tabs=N               */
    unsigned  buffers;              /**< --buffers=N (stub)           */
    unsigned  max_back_scroll;      /**< --max-back-scroll=N (stub)   */
    const char * pattern;          /**< -p PATTERN                   */
    const char * log_file;          /**< -o/-O FILE                    */
    const char * prompt;            /**< -P STR                        */
    bool      log_overwrite;        /**< -O vs -o                      */
    char *    plus_cmd;             /**< +cmd initial command          */
} less_opts_t;

/**
 * @brief Growable byte buffer used by @c _less_read_line to assemble a
 *        single line (including its trailing newline when present).
 */
typedef struct
{
    char * buf;       /**< heap storage                         */
    size_t len;       /**< number of valid bytes               */
    size_t cap;       /**< allocated capacity                  */
} less_line_t;

/**
 * @brief A single stored input line for the interactive pager.
 *
 * The raw bytes are kept without the trailing newline so the pager can
 * manage line breaks itself; a NUL terminator is appended to make the
 * buffer usable by @c strstr / @c _less_line_matches alike.
 */
typedef struct
{
    char  * raw;       /**< heap storage, NUL-terminated, newline stripped   */
    size_t  raw_len;   /**< number of valid bytes (excluding NUL)          */
    bool    is_blank;  /**< true when the line contains only whitespace     */
} less_lnode_t;

/**
 * @brief Growable collection of all input lines for the interactive pager.
 *
 * Lines from every input file are appended in order; @c file_marks records
 * the starting line index of each file so @c :n / @c :p can switch between
 * them.  The pager owns this storage and frees it on exit.
 */
typedef struct
{
    less_lnode_t * arr;     /**< line records                          */
    size_t         count;   /**< number of valid records               */
    size_t         cap;    /**< allocated capacity                    */
    size_t *       file_marks; /**< start index of each file           */
    int            n_marks; /**< number of entries in @c file_marks    */
    const char *   cur_name;/**< current file name (for prompt)         */
} less_lines_t;

/**
 * @brief A single visual row on the pager's virtual screen.
 *
 * One source line may produce several rows when line wrapping is active
 * (no @c -S); each row carries the index of its source line so search
 * hits can be mapped back to a screen position.
 */
typedef struct
{
    char  * text;       /**< NUL-terminated row text (prefix included)  */
    size_t  len;        /**< number of bytes in @c text                 */
    size_t  line_idx;   /**< index into less_lines_t.arr (search map)  */
} less_row_t;

/**
 * @brief Growable list of visual rows built from @c less_lines_t.
 *
 * Paging is pure arithmetic on row indices, which keeps wrap/chop/squeeze
 * handling localised to the row-builder.
 */
typedef struct
{
    less_row_t * arr;   /**< row records                                */
    size_t       count; /**< number of valid records                    */
    size_t       cap;   /**< allocated capacity                         */
    size_t *     line_first_row; /**< source line index -> first row   */
} less_rows_t;

/**
 * @brief Search state carried across keystroke loop iterations.
 */
typedef struct
{
    char     pattern[LESS_SEARCH_BUFSZ]; /**< current search text          */
    bool     has_pattern;                /**< true once a search was done  */
    bool     ignore_case;                 /**< case-insensitive flag        */
    bool     forward;                     /**< last search direction        */
} less_search_t;

/********************************
 *    static prototypes
 ********************************/

static void  _less_print_help(void);
static void  _less_print_version(void);

static bool  _less_str2uint(const char * s, unsigned * out);

static bool  _less_line_matches(const char * line, size_t len,
                                const char * pat, bool ignore_case);

static int   _less_read_line(less_line_t * lb, FILE * fp);
static void  _less_line_free(less_line_t * lb);

static void  _less_emit_line(const char * line, size_t len,
                             unsigned long lno, const less_opts_t * opts,
                             FILE * log_fp);

static int   _less_stream_file(const char * path, FILE * fp,
                               const less_opts_t * opts, unsigned long * lno_io,
                               bool * prev_blank_io, bool * pattern_found_io,
                               FILE * log_fp);

static int   _less_dispatch(const less_opts_t * opts, char ** files, int nfiles);

static int   _less_parse_short(char c, const char * next_arg, bool * consumed_next,
                               less_opts_t * opts);
static int   _less_parse_long(const char * name, const char * val,
                              bool * consumed_val, less_opts_t * opts);
static int   _less_parse_plus(const char * s, less_opts_t * opts);
static int   _less_parse_args(int argc, char ** argv, less_opts_t * opts,
                              char *** out_files, int * out_nfiles);

/* ---- interactive pager helpers ------------------------------------- */
static bool   _less_tty_size(unsigned * rows, unsigned * cols);
static int    _less_getkey(void);
static void   _less_term_raw_enter(void);
static void   _less_term_raw_leave(void);
static size_t _less_expand_line(const less_lnode_t * ln, unsigned long lno,
                               const less_opts_t * opts,
                               char * dst, size_t dst_cap);
static int    _less_append_row(less_rows_t * rs, const char * text,
                               size_t len, size_t line_idx);
static int    _less_build_rows(const less_lines_t * ll,
                              const less_opts_t * opts, unsigned cols,
                              less_rows_t * rs);
static void   _less_rows_free(less_rows_t * rs);
static size_t _less_find_match(const less_lines_t * ll, size_t start,
                               const char * pat, bool ignore_case,
                               bool forward);
static int    _less_load_lines(char ** files, int nfiles,
                               less_lines_t * ll);
static void   _less_lines_free(less_lines_t * ll);
static void   _less_render_screen(const less_rows_t * rs, size_t top,
                                  unsigned rows, bool no_tilde);
static int    _less_interactive(const less_opts_t * opts, char ** files, int nfiles);

#ifdef LESS_PLATFORM_WINDOWS
static HANDLE _less_std_handle_for_fd(int fd);
static bool   _less_is_console_stream(FILE * fp);
static size_t _less_write_win32(const void * buf, size_t len, FILE * fp);
static char ** _less_argv_utf8_alloc(int * argc, char ** argv);
static void    _less_argv_utf8_free(int argc, char ** argv_utf8);
#endif

static FILE * _less_fopen_utf8(const char * path, const char * mode);

/********************************
 *    macros
 ********************************/

#ifdef LESS_PLATFORM_WINDOWS
    /**
     * @brief Portable byte-write macro used by every text-output path.
     *        On Windows stdout/stderr are routed through @c _less_write_win32
     *        which converts UTF-8 bytes to UTF-16LE via WriteConsoleW for
     *        real console streams, transcodes to the console output
     *        codepage for PowerShell 5.x pipes, and writes raw UTF-8 for
     *        disk redirections.  Any non-stdio stream (explicit -o FILE,
     *        temporary buffers, ...) uses plain fwrite so test output is
     *        byte-identical to GNU less.
     * @sa _less_write_win32
     */
    #ifndef less_fwrite
        #define less_fwrite(buf, sz, cnt, fp) \
            _less_write_win32((buf), (size_t)(sz) * (size_t)(cnt), (fp))
    #endif
#else
    #ifndef less_fwrite
        #define less_fwrite(buf, sz, cnt, fp) fwrite((buf), (sz), (cnt), (fp))
    #endif
#endif

/**
 * @brief Formatted print wrapper (printf-compatible).
 *
 * On Windows the formatted buffer is emitted through @c _less_write_win32
 * so CJK glyphs render correctly even on legacy CP936 hosts.
 */
#ifndef less_printf
    #ifdef LESS_PLATFORM_WINDOWS
        #define less_printf(fmt, ...) \
            do { \
                char _lesspf[LESS_PRINTF_BUFSZ]; \
                int _lesspf_n = snprintf(_lesspf, sizeof(_lesspf), (fmt), ##__VA_ARGS__); \
                if (_lesspf_n > 0) { (void)_less_write_win32(_lesspf, (size_t)_lesspf_n, LESS_OUT_STREAM); } \
            } while (0)
    #else
        #define less_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#endif

/**
 * @brief Formatted print to stderr (always raw bytes).
 */
#ifndef less_eprintf
    #define less_eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single byte to the default output stream.
 */
#ifndef less_putchar
    #define less_putchar(ch) \
        do { unsigned char _lesspc = (unsigned char)(ch); (void)less_fwrite(&_lesspc, 1, 1, LESS_OUT_STREAM); } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef less_fputs
    #define less_fputs(str, stream) \
        do { const char * _lesssp = (str); if (_lesssp) { (void)less_fwrite(_lesssp, 1, strlen(_lesssp), (stream)); } } while (0)
#endif

/**
 * @brief Flush the given stdio stream.
 */
#ifndef less_fflush
    #define less_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free: free(*p) and set the pointer to NULL.
 */
#ifndef less_safe_free
    #define less_safe_free(p) do { if ((p) != NULL) { free(p); (p) = NULL; } } while (0)
#endif

/**
 * @brief Clamp @p v into the inclusive range [lo, hi].
 */
#ifndef less_clamp
    #define less_clamp(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#endif

/********************************
 *    static variables
 ********************************/

/* less is stateless per invocation; no module-scoped state required. */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point: parse arguments and dispatch to streaming mode.
 *
 * When stdout is not a TTY (or when -E / -F / -e force streaming),
 * less behaves like a transforming cat: every input line is emitted
 * with the requested transformations (line numbers, squeeze, tabs,
 * pattern filter, ...).  An interactive pager is only launched when
 * stdout is a real TTY and no force-quit option is present.
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, non-zero on failure
 */
int main(int argc, char ** argv)
{
    less_opts_t opts;
    char **      files      = NULL;
    int          nfiles     = 0;
    int          rc;
    char **      argv_utf8  = NULL;

    memset(&opts, 0, sizeof(opts));
    opts.tab_width = LESS_DEFAULT_TABS;

#ifdef LESS_PLATFORM_WINDOWS
    /* MSVCRT populates argv with bytes decoded via the active ACP
     * (typically CP936 on Chinese systems).  Reconstruct a UTF-8 argv
     * from the real UTF-16 command line so that filenames, search
     * patterns, and prompt strings are byte-correct everywhere. */
    {
        int     wc  = 0;
        char ** u8a = _less_argv_utf8_alloc(&wc, argv);
        if (u8a) {
            argc      = wc;
            argv_utf8 = u8a;
            argv      = u8a;
        }
    }
#endif

    rc = _less_parse_args(argc, argv, &opts, &files, &nfiles);
    if (rc != 0) {
        less_safe_free(opts.plus_cmd);
#ifdef LESS_PLATFORM_WINDOWS
        _less_argv_utf8_free(argc, argv_utf8);
#endif
        return rc;
    }

#ifdef LESS_PLATFORM_WINDOWS
    /* Force stdio into binary mode so CRLF translation does not corrupt
     * byte-exact tests (T27 CJK, T43 8192 lines, T44 no trailing newline). */
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    rc = _less_dispatch(&opts, files, nfiles);

    less_safe_free(opts.plus_cmd);
#ifdef LESS_PLATFORM_WINDOWS
    _less_argv_utf8_free(argc, argv_utf8);
#endif
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Open a file with a UTF-8 encoded path.
 *
 * On POSIX systems, @c fopen paths are opaque byte sequences so plain
 * fopen works correctly for UTF-8.  On Windows, however, the MSVCRT
 * narrow-character @c fopen decodes the path using the active ANSI
 * code page (typically CP936 on Chinese hosts), which corrupts any
 * non-ACP byte sequence such as UTF-8.  We therefore transcode to
 * UTF-16 and call @c _wfopen instead.
 */
static FILE * _less_fopen_utf8(const char * path, const char * mode)
{
    if (!path || !mode) { return NULL; }

#ifdef LESS_PLATFORM_WINDOWS
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

/* --------------------------------------------------------------------
 *  help / version
 * -------------------------------------------------------------------- */

/**
 * @brief Print the full --help text to stdout and exit successfully.
 */
static void _less_print_help(void)
{
    less_fputs(
        "Usage: less [OPTION]... [FILE]...\n"
        "  less - a pager similar to more but with forward/backward navigation.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -?, --help                 display this help and exit\n"
        "      --version              output version information and exit\n"
        "  -e, --quit-at-eof          quit at end of file\n"
        "  -E, --QUIT-AT-EOF          quit at end of file (force streaming)\n"
        "  -F, --quit-if-one-screen   quit if file fits on one screen\n"
        "  -f, --force                force open non-regular files\n"
        "  -g, --hilite-search        highlight only last match (stub)\n"
        "  -G, --HILITE-SEARCH        no search highlighting (stub)\n"
        "  -i, --ignore-case          case-insensitive search (soft)\n"
        "  -I, --IGNORE-CASE          case-insensitive search (hard)\n"
        "  -J, --status-column        display status column\n"
        "  -K, --no-keypad            no keypad init/deinit (stub)\n"
        "  -L, --no-less-open         ignore LESSOPEN (stub)\n"
        "  -N, --LINE-NUMBERS         display line numbers\n"
        "  -n                         line numbers (interactive only)\n"
        "  -o FILE, --log-file=FILE  copy input to FILE (refuse overwrite)\n"
        "  -O FILE, --LOG-FILE=FILE   copy input to FILE (overwrite)\n"
        "  -p PATTERN                 jump to first match of PATTERN\n"
        "  -P STR,  --prompt=STR     set prompt string (interactive only)\n"
        "  -r, --raw-control-chars    pass raw control chars through\n"
        "  -R, --RAW-CONTROL-CHARS    pass ANSI escape sequences through\n"
        "  -s, --squeeze-blank-lines  squeeze multiple blank lines\n"
        "  -S, --chop-long-lines      chop long lines (no effect when streaming)\n"
        "  -x N, --tabs=N             set tab stops every N characters\n"
        "  -~, --no-tilde             don't display ~ after end of file\n"
        "      --buffers=N            set buffer count (stub)\n"
        "      --max-back-scroll=N    set max back scroll (stub)\n"
        "  +cmd                       apply initial command (+N, +F, +/pat)\n"
        "\n"
        "Report bugs to <Yezc via cclinuxtools>.\n",
        LESS_OUT_STREAM);
    less_fflush(LESS_OUT_STREAM);
    exit(0);
}

/**
 * @brief Print the --version banner to stdout and exit successfully.
 */
static void _less_print_version(void)
{
    less_printf("less (Yezc cclinuxtools) %s\n", LESS_VERSION_STR);
    less_printf("%s", "MIT License\n");
    less_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    less_printf("%s", "License MIT: <https://mit-license.org/>\n");
    less_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    less_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
    less_fflush(LESS_OUT_STREAM);
    exit(0);
}

/* --------------------------------------------------------------------
 *  small helpers
 * -------------------------------------------------------------------- */

/**
 * @brief Parse a non-negative decimal integer from @p s.
 *
 * Leading '+' is allowed (GNU convention).  Values that overflow
 * @c ULONG_MAX are treated as errors.
 *
 * @param s     NUL-terminated text.
 * @param out   Receives the parsed value on success.
 * @return true on success, false on any parse error.
 */
static bool _less_str2uint(const char * s, unsigned * out)
{
    unsigned long v;
    char * endp = NULL;

    if (!s || !*s)                       { return false; }
    errno = 0;
    v = strtoul(s, &endp, 10);
    if (errno == ERANGE)                 { return false; }
    if (!endp || endp == s)              { return false; }
    if (*endp != '\0')                   { return false; }
    if (v > (unsigned long)~0U)          { v = (unsigned long)~0U; }
    *out = (unsigned)v;
    return true;
}

/**
 * @brief Test whether @p line contains the substring @p pat.
 *
 * @param line         pointer to the line bytes (need not be NUL-terminated)
 * @param len          number of valid bytes in @p line
 * @param pat          NUL-terminated pattern (no regex, just substring)
 * @param ignore_case  if true, compare case-insensitively
 * @return true if @p pat occurs in @p line, false otherwise
 */
static bool _less_line_matches(const char * line, size_t len,
                               const char * pat, bool ignore_case)
{
    size_t plen;
    size_t i;

    if (!line || !pat)                   { return false; }
    plen = strlen(pat);
    if (plen == 0)                       { return true; }
    if (plen > len)                      { return false; }

    for (i = 0; i + plen <= len; i++) {
        size_t j;
        for (j = 0; j < plen; j++) {
            unsigned char a = (unsigned char)line[i + j];
            unsigned char b = (unsigned char)pat[j];
            if (ignore_case) {
                a = (unsigned char)tolower(a);
                b = (unsigned char)tolower(b);
            }
            if (a != b) { break; }
        }
        if (j == plen) { return true; }
    }
    return false;
}

/* --------------------------------------------------------------------
 *  line reader
 * -------------------------------------------------------------------- */

/**
 * @brief Read one line (including its trailing newline) from @p fp.
 *
 * The line buffer grows on demand.  At EOF with no pending data,
 * @c LESS_READ_EOF is returned.
 *
 * @param lb  line buffer (in/out; @c buf is heap-allocated)
 * @param fp  input stream
 * @return 0 on success (line stored), 1 on EOF with no data, -1 on I/O error
 */
static int _less_read_line(less_line_t * lb, FILE * fp)
{
    int c;

    if (!lb || !fp) { return -1; }
    lb->len = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (lb->len + 1 > lb->cap) {
            size_t new_cap = lb->cap ? lb->cap * 2U : LESS_LINE_INIT_CAP;
            char * p = (char *)realloc(lb->buf, new_cap);
            if (!p) { return -1; }
            lb->buf = p;
            lb->cap = new_cap;
        }
        lb->buf[lb->len++] = (char)c;
        if (c == '\n') { break; }
    }

    return (lb->len > 0) ? 0 : 1;
}

/**
 * @brief Release the heap storage owned by a line buffer.
 */
static void _less_line_free(less_line_t * lb)
{
    if (!lb) { return; }
    less_safe_free(lb->buf);
    lb->len = 0;
    lb->cap = 0;
}

/* --------------------------------------------------------------------
 *  output emission
 * -------------------------------------------------------------------- */

/**
 * @brief Write one line to stdout (and optionally to a log file).
 *
 * Applies the following transformations in order:
 *   1. squeeze (handled by caller; not here)
 *   2. status column prefix (one space when -J)
 *   3. line number prefix (right-justified 7-wide + space when -N)
 *   4. tab expansion to @c opts->tab_width stops (counting from col 0)
 *   5. chop-long-lines is a no-op when streaming (no terminal width)
 *
 * @param line    raw line bytes (may include trailing '\n')
 * @param len     number of valid bytes in @p line
 * @param lno     1-based line number for display
 * @param opts    parsed options
 * @param log_fp  optional log file (NULL to skip)
 */
static void _less_emit_line(const char * line, size_t len,
                            unsigned long lno, const less_opts_t * opts,
                            FILE * log_fp)
{
    char         prefix[32];
    size_t       prefix_len = 0;
    size_t       col = 0;
    size_t       i;
    unsigned     tw;

    if (!line || !opts) { return; }

    /* Build prefix: optional status column + optional line number. */
    if (opts->status_col) {
        prefix[prefix_len++] = ' ';
    }
    if (opts->line_numbers) {
        int n = snprintf(prefix + prefix_len,
                         sizeof(prefix) - prefix_len,
                         "%*lu ",
                         (int)LESS_LNO_WIDTH, lno);
        if (n > 0) {
            size_t take = (size_t)n;
            if (prefix_len + take > sizeof(prefix)) { take = sizeof(prefix) - prefix_len; }
            prefix_len += take;
        }
    }

    if (prefix_len > 0) {
        (void)less_fwrite(prefix, 1, prefix_len, LESS_OUT_STREAM);
        if (log_fp) { (void)fwrite(prefix, 1, prefix_len, log_fp); }
        col = prefix_len;
    }

    /* Tab expansion (or raw write if -r / -R / no -x given). */
    tw = opts->tab_width;
    if (tw == 0U) { tw = LESS_DEFAULT_TABS; }

    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)line[i];

        if (ch == '\t' && tw > 0U) {
            size_t target = ((col / tw) + 1U) * tw;
            size_t spaces = target - col;
            char spbuf[64];
            size_t sleft = spaces;

            while (sleft > 0) {
                size_t chunk = (sleft < sizeof(spbuf)) ? sleft : sizeof(spbuf);
                memset(spbuf, ' ', chunk);
                (void)less_fwrite(spbuf, 1, chunk, LESS_OUT_STREAM);
                if (log_fp) { (void)fwrite(spbuf, 1, chunk, log_fp); }
                sleft -= chunk;
            }
            col = target;
        }
        else {
            (void)less_fwrite(&ch, 1, 1, LESS_OUT_STREAM);
            if (log_fp) { (void)fwrite(&ch, 1, 1, log_fp); }
            col++;
            if (ch == '\n') { col = 0; }
        }
    }
}

/* --------------------------------------------------------------------
 *  file streaming
 * -------------------------------------------------------------------- */

/**
 * @brief Stream a single file (or stdin) through the transformation pipeline.
 *
 * @param path              file path (NULL/- for stdin)
 * @param fp                already-open stream (NULL to open @p path)
 * @param opts              parsed options
 * @param lno_io            in/out line counter (continues across files)
 * @param prev_blank_io     in/out "previous line was blank" state (squeeze)
 * @param pattern_found_io  in/out "pattern has already been matched"
 * @param log_fp           optional log file (NULL to skip)
 * @return 0 on success, non-zero on error
 */
static int _less_stream_file(const char * path, FILE * fp,
                             const less_opts_t * opts, unsigned long * lno_io,
                             bool * prev_blank_io, bool * pattern_found_io,
                             FILE * log_fp)
{
    less_line_t  lb = {0};
    bool         prev_blank = (prev_blank_io ? *prev_blank_io : false);
    bool         pat_found  = (pattern_found_io ? *pattern_found_io : true);
    bool         ignore_case;
    bool         own_fp = false;
    int          rc = 0;
    int          r;

    if (!opts) { return -1; }

    if (!fp) {
        if (!path || strcmp(path, "-") == 0) {
            fp = stdin;
        }
        else {
            fp = _less_fopen_utf8(path, "rb");
            if (!fp) {
                less_eprintf("less: %s: %s\n",
                             path ? path : "stdin",
                             strerror(errno));
                return 1;
            }
            own_fp = true;
        }
    }

    ignore_case = opts->ignore_case_soft || opts->ignore_case_hard;

    for (;;) {
        r = _less_read_line(&lb, fp);
        if (r == 1)  { break; }    /* clean EOF */
        if (r != 0)  { rc = 1; break; }

        if (lno_io) { (*lno_io)++; }
        unsigned long lno = (lno_io ? *lno_io : 0UL);

        bool is_blank = (lb.len == 1 && lb.buf[0] == '\n');

        /* Squeeze: collapse consecutive blank lines into one. */
        if (opts->squeeze_blank && is_blank && prev_blank) {
            prev_blank = true;
            continue;
        }
        prev_blank = is_blank;

        /* Pattern filter: skip lines until the first match. */
        if (!pat_found && opts->pattern) {
            if (_less_line_matches(lb.buf, lb.len,
                                   opts->pattern, ignore_case)) {
                pat_found = true;
            }
            else {
                continue;
            }
        }

        _less_emit_line(lb.buf, lb.len, lno, opts, log_fp);
    }

    _less_line_free(&lb);
    if (own_fp && fp) { (void)fclose(fp); }

    if (prev_blank_io)    { *prev_blank_io    = prev_blank; }
    if (pattern_found_io) { *pattern_found_io = pat_found; }
    return rc;
}

/* --------------------------------------------------------------------
 *  interactive pager (TTY mode)
 * -------------------------------------------------------------------- */

/*
 * Special key codes returned by _less_getkey.  Negative values avoid
 * clashing with printable bytes (1..255) and NUL (0).
 */
#define LESS_KEY_UP        (-1)
#define LESS_KEY_DOWN      (-2)
#define LESS_KEY_PGUP      (-3)
#define LESS_KEY_PGDN      (-4)
#define LESS_KEY_HOME      (-5)
#define LESS_KEY_END       (-6)
#define LESS_KEY_RIGHT     (-7)
#define LESS_KEY_LEFT      (-8)
#define LESS_KEY_ESC       (-9)
#define LESS_KEY_UNKNOWN   (-99)

#ifndef LESS_PLATFORM_WINDOWS
    /** Saved terminal attributes, restored when the pager exits. */
    static struct termios _less_saved_termios;
    /** True once raw mode has been successfully enabled on @c g_key_fd. */
    static bool           _less_term_is_raw = false;
#endif

/**
 * @brief File descriptor used to read keystrokes.
 *
 * Equals STDIN_FILENO when stdin is a TTY; otherwise points at the
 * controlling terminal (/dev/tty) so `cat file | less` still works.
 */
static int _less_key_fd = -1;

/**
 * @brief Query the terminal size of the controlling stdout stream.
 *
 * Falls back to @c LESS_FALLBACK_ROWS / @c LESS_FALLBACK_COLS when the
 * real size cannot be queried (non-TTY or ioctl failure).
 *
 * @param rows  receives the number of screen rows (may be NULL)
 * @param cols  receives the number of screen columns (may be NULL)
 * @return true on success
 */
static bool _less_tty_size(unsigned * rows, unsigned * cols)
{
    unsigned r = LESS_FALLBACK_ROWS;
    unsigned c = LESS_FALLBACK_COLS;

#ifdef LESS_PLATFORM_WINDOWS
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO bi;
    if (h && h != INVALID_HANDLE_VALUE &&
        GetConsoleScreenBufferInfo(h, &bi)) {
        long rr = (long)(bi.srWindow.Bottom - bi.srWindow.Top + 1);
        long cc = (long)(bi.srWindow.Right - bi.srWindow.Left + 1);
        if (rr > 0) { r = (unsigned)rr; }
        if (cc > 0) { c = (unsigned)cc; }
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_row > 0) { r = (unsigned)ws.ws_row; }
        if (ws.ws_col > 0) { c = (unsigned)ws.ws_col; }
    }
#endif
    if (rows) { *rows = r; }
    if (cols) { *cols = c; }
    return true;
}

#ifndef LESS_PLATFORM_WINDOWS
/**
 * @brief Enable raw mode on the keystroke fd (POSIX).
 *
 * Disables canonical mode and echo so individual keystrokes arrive
 * immediately without line buffering or screen echo.
 */
static void _less_term_raw_enter(void)
{
    struct termios t;

    if (_less_key_fd < 0)                    { return; }
    if (!isatty(_less_key_fd))               { return; }
    if (tcgetattr(_less_key_fd, &_less_saved_termios) != 0) { return; }
    t = _less_saved_termios;
    t.c_lflag &= ~(unsigned)(ICANON | ECHO | ECHOE | ISIG);
    t.c_iflag &= ~(unsigned)(IXON | ICRNL);
    t.c_oflag &= ~(unsigned)OPOST;     /* we emit \r\n ourselves          */
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(_less_key_fd, TCSANOW, &t) == 0) {
        _less_term_is_raw = true;
    }
}

/**
 * @brief Restore the terminal attributes saved by @c _less_term_raw_enter.
 */
static void _less_term_raw_leave(void)
{
    if (_less_term_is_raw) {
        (void)tcsetattr(_less_key_fd, TCSANOW, &_less_saved_termios);
        _less_term_is_raw = false;
    }
}
#else
    /* Windows reads keystrokes via _getch(); no raw-mode setup needed. */
static void _less_term_raw_enter(void) { /* no-op */ }
static void _less_term_raw_leave(void) { /* no-op */ }
#endif

/**
 * @brief Read one keystroke and translate it to a key code.
 *
 * Printable keys return their byte value; special keys (arrows, page up,
 * etc.) return one of the @c LESS_KEY_* constants.
 *
 * @return key code, or @c LESS_KEY_UNKNOWN on end-of-input / parse failure
 */
static int _less_getkey(void)
{
#ifdef LESS_PLATFORM_WINDOWS
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) {
        int ext = _getch();
        switch (ext) {
            case 72:  return LESS_KEY_UP;
            case 80:  return LESS_KEY_DOWN;
            case 73:  return LESS_KEY_PGUP;
            case 81:  return LESS_KEY_PGDN;
            case 71:  return LESS_KEY_HOME;
            case 79:  return LESS_KEY_END;
            case 75:  return LESS_KEY_LEFT;
            case 77:  return LESS_KEY_RIGHT;
            default:  return LESS_KEY_UNKNOWN;
        }
    }
    return ch;
#else
    unsigned char ch;
    ssize_t n;

    if (_less_key_fd < 0) { return LESS_KEY_UNKNOWN; }
    n = read(_less_key_fd, &ch, 1);
    if (n <= 0) { return LESS_KEY_UNKNOWN; }
    if (ch != 0x1B) { return (int)ch; }   /* not ESC                       */

    /* ESC: maybe the start of a cursor-key sequence.  Briefly switch to a
     * VMIN=0 / VTIME=1 read so a lone Escape still returns promptly.      */
    {
        struct termios t;
        struct termios saved;
        unsigned char  seq[8];
        ssize_t        m = 0;

        if (tcgetattr(_less_key_fd, &t) != 0) { return LESS_KEY_ESC; }
        saved = t;
        t.c_cc[VMIN]  = 0;
        t.c_cc[VTIME] = 1;
        (void)tcsetattr(_less_key_fd, TCSANOW, &t);
        m = read(_less_key_fd, seq, sizeof(seq));
        (void)tcsetattr(_less_key_fd, TCSANOW, &saved);
        if (m <= 0) { return LESS_KEY_ESC; }   /* lone Escape              */

        if (seq[0] == '[' || seq[0] == 'O') {
            if (m >= 2 && seq[1] >= 'A' && seq[1] <= 'D') {
                /* A=up B=down C=right D=left */
                static const int arrows[4] =
                    { LESS_KEY_UP, LESS_KEY_DOWN, LESS_KEY_RIGHT, LESS_KEY_LEFT };
                return arrows[seq[1] - 'A'];
            }
            if (m >= 3 && seq[1] == '5' && seq[2] == '~') { return LESS_KEY_PGUP; }
            if (m >= 3 && seq[1] == '6' && seq[2] == '~') { return LESS_KEY_PGDN; }
            if (m >= 3 && (seq[1] == '1' || seq[1] == '7') && seq[2] == '~')
            { return LESS_KEY_HOME; }
            if (m >= 3 && (seq[1] == '4' || seq[1] == '8') && seq[2] == '~')
            { return LESS_KEY_END; }
            if (m >= 2 && seq[1] == 'H') { return LESS_KEY_HOME; }
            if (m >= 2 && seq[1] == 'F') { return LESS_KEY_END; }
        }
        return LESS_KEY_UNKNOWN;
    }
#endif
}

/**
 * @brief Expand one source line into a NUL-terminated display string.
 *
 * The output includes the optional status column and line-number prefix,
 * and expands tabs to spaces according to @c opts->tab_width.  Control
 * characters are passed through unchanged (the @c -r / @c -R switches
 * only affect ANSI sequences, which a byte-exact copy already preserves).
 *
 * @param ln       source line (raw bytes, newline already stripped)
 * @param lno      1-based line number for the @c -N prefix
 * @param opts     parsed options
 * @param dst      destination buffer
 * @param dst_cap  capacity of @p dst
 * @return number of bytes written (excluding the NUL terminator)
 */
static size_t _less_expand_line(const less_lnode_t * ln, unsigned long lno,
                                const less_opts_t * opts,
                                char * dst, size_t dst_cap)
{
    size_t   di = 0;
    size_t   col = 0;
    unsigned tw = opts->tab_width ? opts->tab_width : LESS_DEFAULT_TABS;
    size_t   i;

    if (!ln || !opts || !dst || dst_cap == 0) { return 0; }

    if (opts->status_col && di + 1 < dst_cap) {
        dst[di++] = ' ';
        col++;
    }
    if (opts->line_numbers) {
        char numbuf[32];
        int  n = snprintf(numbuf, sizeof(numbuf), "%*lu ",
                          (int)LESS_LNO_WIDTH, lno);
        if (n > 0) {
            size_t take = (size_t)n;
            if (take > dst_cap - di - 1) { take = dst_cap - di - 1; }
            memcpy(dst + di, numbuf, take);
            di  += take;
            col += (size_t)n;
        }
    }

    for (i = 0; i < ln->raw_len; i++) {
        unsigned char ch = (unsigned char)ln->raw[i];

        if (ch == '\t' && tw > 0) {
            size_t target = ((col / tw) + 1U) * tw;
            while (col < target && di + 1 < dst_cap) {
                dst[di++] = ' ';
                col++;
            }
        }
        else if (ch == '\n' || ch == '\r') {
            /* newlines were already stripped; ignore stray CRs          */
        }
        else if (di + 1 < dst_cap) {
            dst[di++] = (char)ch;
            col++;
        }
    }
    if (di < dst_cap) { dst[di] = '\0'; }
    return di;
}

/**
 * @brief Append one visual row (copying @p text) to the row list.
 *
 * @return 0 on success, -1 on allocation failure
 */
static int _less_append_row(less_rows_t * rs, const char * text,
                            size_t len, size_t line_idx)
{
    less_row_t * nr;
    char       *  nt;

    if (!rs || (!text && len > 0)) { return -1; }
    if (rs->count == rs->cap) {
        size_t nc = rs->cap ? rs->cap * 2U : 512U;
        nr = (less_row_t *)realloc(rs->arr, nc * sizeof(less_row_t));
        if (!nr) { return -1; }
        rs->arr = nr;
        rs->cap = nc;
    }
    nt = (char *)malloc(len + 1U);
    if (!nt) { return -1; }
    if (len > 0) { memcpy(nt, text, len); }
    nt[len] = '\0';
    rs->arr[rs->count].text     = nt;
    rs->arr[rs->count].len      = len;
    rs->arr[rs->count].line_idx = line_idx;
    rs->count++;
    return 0;
}

/**
 * @brief Build the virtual screen rows from the loaded source lines.
 *
 * Applies, in order: squeeze (@c -s), prefix (@c -J / @c -N), tab
 * expansion, and either chopping (@c -S) or wrapping at @p cols.  Each
 * source line contributes one or more rows; @c line_first_row maps a
 * source line index to its first row for search positioning.
 *
 * @param ll    loaded source lines
 * @param opts  parsed options
 * @param cols  available screen width
 * @param rs    [out] built row list
 * @return 0 on success, -1 on allocation failure
 */
static int _less_build_rows(const less_lines_t * ll,
                            const less_opts_t * opts, unsigned cols,
                            less_rows_t * rs)
{
    char   exp[LESS_EXPAND_BUFSZ];
    bool   prev_blank = false;
    size_t i;

    if (!ll || !opts || !rs) { return -1; }
    rs->arr = NULL; rs->count = 0; rs->cap = 0;
    rs->line_first_row = (size_t *)calloc(ll->count + 1U, sizeof(size_t));
    if (ll->count > 0 && !rs->line_first_row) { return -1; }

    if (cols < 1U) { cols = LESS_FALLBACK_COLS; }

    for (i = 0; i < ll->count; i++) {
        bool blank = (ll->arr[i].raw_len == 0U);
        rs->line_first_row[i] = rs->count;

        if (opts->squeeze_blank && blank && prev_blank) {
            prev_blank = true;
            continue;   /* collapse consecutive blank lines               */
        }
        prev_blank = blank;

        {
            size_t len = _less_expand_line(&ll->arr[i], (unsigned long)(i + 1U),
                                           opts, exp, sizeof(exp));

            if (opts->chop_long) {
                if (len > cols) { len = cols; }
                if (_less_append_row(rs, exp, len, i) != 0) { return -1; }
            }
            else if (len == 0U) {
                if (_less_append_row(rs, "", 0, i) != 0) { return -1; }
            }
            else {
                size_t off = 0U;
                while (off < len) {
                    size_t take = (len - off < cols) ? (len - off) : cols;
                    if (_less_append_row(rs, exp + off, take, i) != 0) {
                        return -1;
                    }
                    off += take;
                }
            }
        }
    }
    rs->line_first_row[ll->count] = rs->count;   /* sentinel              */
    return 0;
}

/**
 * @brief Release all storage owned by a row list.
 */
static void _less_rows_free(less_rows_t * rs)
{
    size_t i;

    if (!rs) { return; }
    if (rs->arr) {
        for (i = 0; i < rs->count; i++) { less_safe_free(rs->arr[i].text); }
        less_safe_free(rs->arr);
    }
    less_safe_free(rs->line_first_row);
    rs->count = 0;
    rs->cap = 0;
}

/**
 * @brief Find the next source line matching @p pat.
 *
 * @param ll           loaded source lines
 * @param start        first index to consider (inclusive, forward only)
 * @param pat          substring pattern
 * @param ignore_case  case-insensitive comparison
 * @param forward      true to search toward EOF, false toward BOF
 * @return matching line index, or @c (size_t)-1 if none
 */
static size_t _less_find_match(const less_lines_t * ll, size_t start,
                               const char * pat, bool ignore_case,
                               bool forward)
{
    size_t i;

    if (!ll || !pat || ll->count == 0U) { return (size_t)-1; }

    if (forward) {
        for (i = start; i < ll->count; i++) {
            if (_less_line_matches(ll->arr[i].raw, ll->arr[i].raw_len,
                                    pat, ignore_case)) {
                return i;
            }
        }
    }
    else {
        if (start >= ll->count) { start = ll->count - 1U; }
        i = start;
        for (;;) {
            if (_less_line_matches(ll->arr[i].raw, ll->arr[i].raw_len,
                                    pat, ignore_case)) {
                return i;
            }
            if (i == 0U) { break; }
            i--;
        }
    }
    return (size_t)-1;
}

/**
 * @brief Append one source line to the line store (copying its bytes).
 *
 * @return 0 on success, -1 on allocation failure
 */
static int _less_append_line(less_lines_t * ll, const less_line_t * lb)
{
    less_lnode_t * ln;
    size_t         n;

    if (!ll || !lb) { return -1; }
    if (ll->count == ll->cap) {
        size_t         nc = ll->cap ? ll->cap * 2U : 256U;
        less_lnode_t * na = (less_lnode_t *)realloc(ll->arr,
                                                    nc * sizeof(less_lnode_t));
        if (!na) { return -1; }
        ll->arr = na;
        ll->cap = nc;
    }
    ln = &ll->arr[ll->count];
    n  = lb->len;
    if (n > 0U && lb->buf[n - 1U] == '\n') { n--; }   /* strip newline   */
    ln->raw = (char *)malloc(n + 1U);
    if (!ln->raw) { return -1; }
    if (n > 0U) { memcpy(ln->raw, lb->buf, n); }
    ln->raw[n]  = '\0';
    ln->raw_len = n;
    ln->is_blank = (n == 0U);
    ll->count++;
    return 0;
}

/**
 * @brief Read every input file into @p ll as a flat list of source lines.
 *
 * @c file_marks records the start index of each file for @c :n / @c :p.
 *
 * @param files   file path arguments (NULL/- for stdin)
 * @param nfiles  number of entries in @p files
 * @param ll      [out] populated line store
 * @return 0 on success, non-zero on error
 */
static int _less_load_lines(char ** files, int nfiles, less_lines_t * ll)
{
    less_line_t lb = {0};
    int         i;
    int         rc = 0;

    if (!ll) { return -1; }
    ll->arr = NULL; ll->count = 0; ll->cap = 0;
    ll->file_marks = NULL; ll->n_marks = 0; ll->cur_name = NULL;

    if (nfiles > 0) {
        ll->file_marks = (size_t *)calloc((size_t)nfiles + 1U, sizeof(size_t));
        if (!ll->file_marks) { return -1; }
    }

    if (nfiles <= 0 || !files) {
        ll->cur_name = "stdin";
        while (_less_read_line(&lb, stdin) == 0) {
            if (_less_append_line(ll, &lb) != 0) {
                _less_line_free(&lb);
                rc = -1;
                goto done;
            }
        }
    }
    else {
        for (i = 0; i < nfiles; i++) {
            FILE *       fp;
            const char * path = files[i];

            if (!path || strcmp(path, "-") == 0) {
                fp = stdin;
                path = "stdin";
            }
            else {
                fp = _less_fopen_utf8(path, "rb");
                if (!fp) {
                    less_eprintf("less: %s: %s\n", path, strerror(errno));
                    continue;
                }
            }
            ll->cur_name  = path;
            ll->file_marks[ll->n_marks++] = ll->count;

            while (_less_read_line(&lb, fp) == 0) {
                if (_less_append_line(ll, &lb) != 0) {
                    if (fp != stdin) { (void)fclose(fp); }
                    _less_line_free(&lb);
                    rc = -1;
                    goto done;
                }
            }
            if (fp != stdin) { (void)fclose(fp); }
        }
        ll->file_marks[ll->n_marks] = ll->count;   /* end sentinel        */
    }

done:
    _less_line_free(&lb);
    return rc;
}

/**
 * @brief Release all storage owned by a line store.
 */
static void _less_lines_free(less_lines_t * ll)
{
    size_t i;

    if (!ll) { return; }
    if (ll->arr) {
        for (i = 0; i < ll->count; i++) { less_safe_free(ll->arr[i].raw); }
        less_safe_free(ll->arr);
    }
    less_safe_free(ll->file_marks);
    ll->count = 0;
    ll->cap = 0;
    ll->n_marks = 0;
    ll->cur_name = NULL;
}

/**
 * @brief Paint one screen: clear, print rows from @p top, pad with ~.
 *
 * @p rows includes the bottom prompt line, so only @c rows-1 rows of
 * content are printed.
 *
 * @param rs       built virtual rows
 * @param top      index of the first row to display
 * @param rows     total screen rows (content uses rows-1)
 * @param no_tilde when true, do not print ~ padding past end-of-file
 */
static void _less_render_screen(const less_rows_t * rs, size_t top,
                                unsigned rows, bool no_tilde)
{
    unsigned scr_rows = (rows > 1U) ? (rows - 1U) : 1U;
    unsigned used = 0U;
    size_t   i;

    /* home cursor and clear to end of screen */
    less_fputs("\x1b[H\x1b[J", LESS_OUT_STREAM);

    if (!rs) { goto pad; }
    for (i = top; i < rs->count && used < scr_rows; i++) {
        (void)less_fwrite(rs->arr[i].text, 1, rs->arr[i].len, LESS_OUT_STREAM);
        less_fputs("\r\n", LESS_OUT_STREAM);
        used++;
    }

pad:
    if (!no_tilde) {
        while (used < scr_rows) {
            less_fputs("~\r\n", LESS_OUT_STREAM);
            used++;
        }
    }
    else {
        while (used < scr_rows) {
            less_fputs("\r\n", LESS_OUT_STREAM);
            used++;
        }
    }
}

/**
 * @brief Read a search pattern typed after @c / or @c ? .
 *
 * Echoes characters until Enter; Backspace edits the in-progress pattern.
 *
 * @param prompt  the leading character ('/' or '?')
 * @param buf     destination buffer
 * @param buf_sz  capacity of @p buf
 * @return 0 on success (pattern stored), -1 if cancelled by Escape
 */
static int _less_read_search_pattern(int prompt, char * buf, size_t buf_sz)
{
    size_t n = 0U;

    if (!buf || buf_sz == 0U) { return -1; }
    buf[0] = '\0';
    /* clear the prompt line, then show the search prompt char            */
    less_fputs("\r\x1b[K", LESS_OUT_STREAM);
    less_putchar(prompt);
    less_fflush(LESS_OUT_STREAM);

    for (;;) {
        int k = _less_getkey();
        if (k == LESS_KEY_UNKNOWN) { return -1; }
        if (k == '\r' || k == '\n' || k == LESS_KEY_ESC) {
            if (k == LESS_KEY_ESC) { buf[0] = '\0'; return -1; }
            buf[n] = '\0';
            return 0;
        }
        if ((k == '\b' || k == 0x7F) && n > 0U) {
            n--;
            buf[n] = '\0';
            less_fputs("\b \b", LESS_OUT_STREAM);
            less_fflush(LESS_OUT_STREAM);
            continue;
        }
        if (k >= 0x20 && k < 0x7F && n + 1U < buf_sz) {
            buf[n++] = (char)k;
            less_putchar(k);
            less_fflush(LESS_OUT_STREAM);
        }
    }
}

/**
 * @brief Run the interactive pager loop when stdout is a TTY.
 *
 * Loads all input into memory, builds virtual screen rows, then loops
 * on keystrokes (Space/b/q/g/G/=/h/... plus @c / @c ? @c n @c N search and
 * @c :n @c :p file switching) until the user quits.
 *
 * @param opts    parsed options
 * @param files   file path arguments
 * @param nfiles  number of entries in @p files
 * @return 0 on success
 */
static int _less_interactive(const less_opts_t * opts, char ** files, int nfiles)
{
    less_lines_t   ll;
    less_rows_t    rs;
    less_search_t  sch;

    memset(&ll, 0, sizeof(ll));
    memset(&rs, 0, sizeof(rs));
    memset(&sch, 0, sizeof(sch));
    unsigned       rows = LESS_FALLBACK_ROWS;
    unsigned       cols = LESS_FALLBACK_COLS;
    size_t         top = 0;
    size_t         max_top = 0;
    size_t         cur_file = 0;
    int            rc = 0;
    bool           running = true;
    bool           eof_seen = false;
    size_t         page;
    size_t         half;

    if (!opts) { return -1; }
    sch.ignore_case = opts->ignore_case_soft || opts->ignore_case_hard;

    if (_less_load_lines(files, nfiles, &ll) != 0) {
        _less_lines_free(&ll);
        return 1;
    }
    if (ll.count == 0U) {
        _less_lines_free(&ll);
        return 0;
    }

    _less_tty_size(&rows, &cols);
    if (rows < 2U)  { rows = LESS_FALLBACK_ROWS; }
    if (cols < 2U)  { cols = LESS_FALLBACK_COLS; }

    if (_less_build_rows(&ll, opts, cols, &rs) != 0) {
        _less_rows_free(&rs);
        _less_lines_free(&ll);
        return 1;
    }

    /* -F: if the whole file fits on one screen, dump it and exit.        */
    if (opts->quit_if_one_screen && rs.count <= (size_t)(rows - 1U)) {
        size_t i;
        for (i = 0; i < rs.count; i++) {
            (void)less_fwrite(rs.arr[i].text, 1, rs.arr[i].len,
                              LESS_OUT_STREAM);
            less_fputs("\n", LESS_OUT_STREAM);
        }
        _less_rows_free(&rs);
        _less_lines_free(&ll);
        return 0;
    }

    /* initial position: -p PATTERN jumps to first match                  */
    if (opts->pattern) {
        size_t m = _less_find_match(&ll, 0U, opts->pattern,
                                    sch.ignore_case, true);
        if (m != (size_t)-1 && m < ll.count) {
            top = rs.line_first_row[m];
        }
    }

    /* +cmd: +/pat, +N, +F                                                */
    if (opts->plus_cmd) {
        if (opts->follow_mode) {
            top = (rs.count > 0U) ? rs.count - 1U : 0U;
        }
        else if (opts->plus_cmd[0] == '/') {
            size_t m = _less_find_match(&ll, 0U, opts->plus_cmd + 1,
                                        sch.ignore_case, true);
            if (m != (size_t)-1 && m < ll.count) {
                top = rs.line_first_row[m];
                (void)snprintf(sch.pattern, sizeof(sch.pattern), "%s",
                               opts->plus_cmd + 1);
                sch.has_pattern = true;
                sch.forward = true;
            }
        }
        else if (opts->plus_cmd[0] >= '0' && opts->plus_cmd[0] <= '9') {
            unsigned long ln = strtoul(opts->plus_cmd, NULL, 10);
            if (ln > 0UL && ln - 1UL < ll.count) {
                top = rs.line_first_row[ln - 1UL];
            }
        }
    }

    /* open a keystroke fd: stdin when it's a TTY, else the controlling tty */
#ifdef LESS_PLATFORM_WINDOWS
    _less_key_fd = -1;   /* _getch reads the console directly             */
#else
    if (isatty(STDIN_FILENO)) {
        _less_key_fd = STDIN_FILENO;
    }
    else {
        int fd = open("/dev/tty", O_RDONLY);
        _less_key_fd = (fd >= 0) ? fd : -1;
    }
#endif
    if (_less_key_fd < 0
#ifdef LESS_PLATFORM_WINDOWS
        && 0   /* Windows uses _getch, fd is irrelevant                    */
#endif
        ) {
        /* no way to read keystrokes -> fall back to streaming            */
        _less_rows_free(&rs);
        _less_lines_free(&ll);
        return 0;
    }

    _less_term_raw_enter();

    page = (size_t)(rows - 1U);
    half = (size_t)(rows / 2U);
    if (half == 0U) { half = 1U; }

    while (running) {
        if (top >= rs.count) { top = (rs.count > 0U) ? rs.count - 1U : 0U; }
        max_top = (rs.count > page) ? (rs.count - page) : 0U;
        if (top > max_top) { top = max_top; }

        _less_render_screen(&rs, top, rows, opts->no_tilde);

        /* bottom prompt line                                            */
        {
            char p[LESS_PROMPT_BUFSZ];
            const char * nm = ll.cur_name ? ll.cur_name : "stdin";
            size_t       last_row = (rs.count > 0U) ? (rs.count - 1U) : 0U;
            size_t       bot = top + page;
            if (bot > last_row) { bot = last_row; }

            if (opts->prompt) {
                (void)snprintf(p, sizeof(p), "%s", opts->prompt);
            }
            else if (top >= last_row) {
                (void)snprintf(p, sizeof(p), "%s (END)", nm);
            }
            else {
                unsigned pct;
                if (rs.count > 0U) {
                    pct = (unsigned)((bot + 1U) * 100U / rs.count);
                }
                else { pct = 0U; }
                (void)snprintf(p, sizeof(p),
                               "%s lines %zu-%zu/%zu %u%%",
                               nm, top + 1U, bot + 1U, rs.count, pct);
            }
            /* clear the prompt line then print the prompt and return CR  */
            less_fputs("\x1b[K", LESS_OUT_STREAM);
            less_fputs(p, LESS_OUT_STREAM);
            less_fflush(LESS_OUT_STREAM);
        }

        {
            int key = _less_getkey();
            size_t last_row = (rs.count > 0U) ? (rs.count - 1U) : 0U;

            switch (key) {
                case 'q':
                case 'Q':
                    running = false;
                    break;

                case ' ':
                case 'f':
                case 'z':
                case LESS_KEY_PGDN:
                case LESS_KEY_RIGHT:
                    top = (top + page <= last_row) ? top + page : last_row;
                    /* -E quits first time at EOF, -e the second time     */
                    if (top >= last_row) {
                        if (opts->quit_at_eof_two) {
                            running = false;
                        }
                        else if (opts->quit_at_eof) {
                            if (eof_seen) { running = false; }
                            else          { eof_seen = true; }
                        }
                    }
                    break;

                case 'b':
                case LESS_KEY_PGUP:
                case LESS_KEY_LEFT:
                    top = (top > page) ? (top - page) : 0U;
                    break;

                case '\r':
                case '\n':
                case 'e':
                case 'j':
                case LESS_KEY_DOWN:
                    top = (top < last_row) ? top + 1U : last_row;
                    break;

                case 'y':
                case 'k':
                case LESS_KEY_UP:
                    top = (top > 0U) ? top - 1U : 0U;
                    break;

                case 'd':
                    top = (top + half <= last_row) ? top + half : last_row;
                    break;

                case 'u':
                    top = (top > half) ? (top - half) : 0U;
                    break;

                case 'g':
                case '<':
                case LESS_KEY_HOME:
                    top = 0U;
                    break;

                case 'G':
                case '>':
                case LESS_KEY_END:
                    top = max_top;
                    break;

                case '=':
                    {
                        char info[LESS_PROMPT_BUFSZ];
                        (void)snprintf(info, sizeof(info),
                                       "%s lines %zu-%zu/%zu",
                                       ll.cur_name ? ll.cur_name : "stdin",
                                       top + 1U,
                                       (top + page <= last_row)
                                           ? top + page : last_row,
                                       ll.count);
                        less_fputs("\r\x1b[K", LESS_OUT_STREAM);
                        less_fputs(info, LESS_OUT_STREAM);
                        less_fflush(LESS_OUT_STREAM);
                        (void)_less_getkey();   /* any key to continue   */
                    }
                    break;

                case 'h':
                case 'H':
                    {
                        const char * help_text =
                            "less keys:  SPACE/f fwd-page  b back-page  "
                            "ENTER/e/j fwd-line  y/k back-line\n"
                            "  d fwd-half  u back-half  g top  G bottom  "
                            "q quit  = info\n"
                            "  /pat fwd-search  ?pat back-search  "
                            "n repeat  N reverse\n"
                            "  :n next-file  :p prev-file  "
                            "(any key to continue)";
                        less_fputs("\x1b[H\x1b[J", LESS_OUT_STREAM);
                        less_fputs(help_text, LESS_OUT_STREAM);
                        less_fflush(LESS_OUT_STREAM);
                        (void)_less_getkey();
                    }
                    break;

                case 'F':
                    /* follow mode: park at end-of-file                   */
                    top = max_top;
                    break;

                case '/':
                case '?':
                    {
                        char pat[LESS_SEARCH_BUFSZ];
                        bool fwd = (key == '/');
                        if (_less_read_search_pattern(key, pat,
                                                      sizeof(pat)) == 0) {
                            size_t start;
                            size_t m;
                            if (fwd) {
                                start = top + 1U;
                                if (start >= ll.count) { start = 0U; }
                                m = _less_find_match(&ll, start, pat,
                                                     sch.ignore_case, true);
                            }
                            else {
                                if (top >= ll.count) { start = ll.count - 1U; }
                                else {
                                    /* map top row -> source line          */
                                    size_t li = (rs.count > 0U)
                                        ? rs.arr[top].line_idx : 0U;
                                    start = (li > 0U) ? li - 1U : 0U;
                                }
                                m = _less_find_match(&ll, start, pat,
                                                     sch.ignore_case, false);
                            }
                            if (m == (size_t)-1) {
                                less_fputs("\r\x1b[KPattern not found",
                                           LESS_OUT_STREAM);
                                less_fflush(LESS_OUT_STREAM);
                                (void)_less_getkey();
                            }
                            else {
                                top = rs.line_first_row[m];
                                (void)snprintf(sch.pattern, sizeof(sch.pattern),
                                               "%s", pat);
                                sch.has_pattern = true;
                                sch.forward = fwd;
                            }
                        }
                    }
                    break;

                case 'n':
                case 'N':
                    {
                        bool fwd = (key == 'n') ? sch.forward : !sch.forward;
                        size_t start;
                        size_t m;
                        if (!sch.has_pattern) { break; }
                        if (fwd) {
                            start = top + 1U;
                            if (start >= ll.count) { start = 0U; }
                            m = _less_find_match(&ll, start, sch.pattern,
                                                 sch.ignore_case, true);
                        }
                        else {
                            size_t li = (rs.count > 0U)
                                ? rs.arr[top].line_idx : 0U;
                            start = (li > 0U) ? li - 1U : 0U;
                            m = _less_find_match(&ll, start, sch.pattern,
                                                 sch.ignore_case, false);
                        }
                        if (m == (size_t)-1) {
                            less_fputs("\r\x1b[KPattern not found",
                                       LESS_OUT_STREAM);
                            less_fflush(LESS_OUT_STREAM);
                            (void)_less_getkey();
                        }
                        else {
                            top = rs.line_first_row[m];
                        }
                    }
                    break;

                case ':':
                    {
                        int f = _less_getkey();
                        if (f == 'n' && ll.n_marks > 0) {
                            if (cur_file + 1 < (size_t)ll.n_marks) {
                                cur_file++;
                                top = rs.line_first_row[
                                    ll.file_marks[cur_file]];
                            }
                        }
                        else if (f == 'p' && ll.n_marks > 0) {
                            if (cur_file > 0U) {
                                cur_file--;
                                top = rs.line_first_row[
                                    ll.file_marks[cur_file]];
                            }
                        }
                    }
                    break;

                default:
                    /* ignore unmapped keys                                  */
                    break;
            }
        }
    }

    _less_term_raw_leave();

#ifndef LESS_PLATFORM_WINDOWS
    if (_less_key_fd >= 0 && _less_key_fd != STDIN_FILENO) {
        (void)close(_less_key_fd);
    }
#endif
    _less_key_fd = -1;

    /* clear screen on exit so the terminal is left tidy                */
    less_fputs("\x1b[H\x1b[J", LESS_OUT_STREAM);
    less_fflush(LESS_OUT_STREAM);

    _less_rows_free(&rs);
    _less_lines_free(&ll);
    return rc;
}

/**
 * @brief Top-level dispatch: choose interactive pager or streaming mode.
 *
 * When stdout is a TTY (interactive use, e.g. `less file.txt`) the input
 * is loaded into memory and the @c _less_interactive pager loop takes
 * over, providing forward/backward paging, search, and GNU less style
 * keystroke handling.
 *
 * When stdout is NOT a TTY (pipes, redirects, the test harness), or when
 * the user gave a log file (@c -o / @c -O) which requires byte-exact
 * mirroring, every file is streamed through @c _less_stream_file so the
 * binary behaves like `cat` while still applying the same transformation
 * options (-N, -s, -x, -p, ...).
 *
 * @param opts    parsed options
 * @param files   array of file path arguments (may be NULL)
 * @param nfiles  number of entries in @p files
 * @return 0 on success, non-zero on error
 */
static int _less_dispatch(const less_opts_t * opts, char ** files, int nfiles)
{
    FILE *       log_fp = NULL;
    unsigned long lno = 0UL;
    bool         prev_blank = false;
    bool         pat_found  = false;
    int          rc = 0;
    int          i;

    if (!opts) { return -1; }

    /* Set up log file if requested. */
    if (opts->log_file) {
        if (opts->log_overwrite) {
            log_fp = _less_fopen_utf8(opts->log_file, "wb");
        }
        else {
            /* Refuse to overwrite an existing file (GNU less semantics). */
            FILE * probe = _less_fopen_utf8(opts->log_file, "rb");
            if (probe) {
                (void)fclose(probe);
                less_eprintf("less: %s: file exists (use -O to overwrite)\n",
                             opts->log_file);
                return 1;
            }
            log_fp = _less_fopen_utf8(opts->log_file, "wb");
        }
        if (!log_fp) {
            less_eprintf("less: %s: %s\n",
                         opts->log_file, strerror(errno));
            return 1;
        }
    }

    /* Interactive pager only when stdout is a real terminal and no log
     * file is mirroring the raw bytes (a log file implies byte-exact
     * streaming semantics, which the screen renderer would break).    */
    if (!opts->log_file && isatty(fileno(LESS_OUT_STREAM))) {
        int irc = _less_interactive(opts, files, nfiles);
        if (log_fp) { (void)fclose(log_fp); }
        return irc;
    }

    if (nfiles <= 0 || !files) {
        rc = _less_stream_file("-", NULL, opts, &lno, &prev_blank,
                               &pat_found, log_fp);
    }
    else {
        for (i = 0; i < nfiles; i++) {
            int r = _less_stream_file(files[i], NULL, opts, &lno,
                                      &prev_blank, &pat_found, log_fp);
            if (r != 0) { rc = r; }
        }
    }

    if (log_fp) { (void)fclose(log_fp); }
    less_fflush(LESS_OUT_STREAM);
    return rc;
}

/* --------------------------------------------------------------------
 *  option parsing
 * -------------------------------------------------------------------- */

/**
 * @brief Parse one short option character.
 *
 * @param c             the option char
 * @param next_arg      the next argv element (for options that take an arg)
 * @param consumed_next set to true if @p next_arg was consumed
 * @param opts          options to fill
 * @return 0 on success, non-zero on unknown / bad option
 */
static int _less_parse_short(char c, const char * next_arg, bool * consumed_next,
                             less_opts_t * opts)
{
    *consumed_next = false;

    switch (c) {
        case '?': _less_print_help(); break;               /* never returns */
        case 'e': opts->quit_at_eof = true;               break;
        case 'E': opts->quit_at_eof_two = true;           break;
        case 'F': opts->quit_if_one_screen = true;        break;
        case 'f': opts->force = true;                     break;
        case 'N': opts->line_numbers = true;              break;
        case 'n': opts->line_numbers_soft = true;         break;
        case 's': opts->squeeze_blank = true;             break;
        case 'J': opts->status_col = true;               break;
        case 'S': opts->chop_long = true;                 break;
        case 'r': opts->raw_ctrl = true;                 break;
        case 'R': opts->raw_ctrl_ansi = true;             break;
        case 'i': opts->ignore_case_soft = true;         break;
        case 'I': opts->ignore_case_hard = true;         break;
        case 'K': opts->no_keypad = true;                 break;
        case 'L': opts->no_less_open = true;              break;
        case 'g': opts->hilite_first = true;             break;
        case 'G': opts->hilite_none = true;              break;
        case '~': opts->no_tilde = true;                  break;
        case 'x':
            if (!next_arg) { return 2; }
            if (!_less_str2uint(next_arg, &opts->tab_width)) { return 2; }
            *consumed_next = true;
            break;
        case 'p':
            if (!next_arg) { return 2; }
            opts->pattern = next_arg;
            *consumed_next = true;
            break;
        case 'o':
            if (!next_arg) { return 2; }
            opts->log_file = next_arg;
            opts->log_overwrite = false;
            *consumed_next = true;
            break;
        case 'O':
            if (!next_arg) { return 2; }
            opts->log_file = next_arg;
            opts->log_overwrite = true;
            *consumed_next = true;
            break;
        case 'P':
            if (!next_arg) { return 2; }
            opts->prompt = next_arg;
            *consumed_next = true;
            break;
        default:
            return 1;
    }
    return 0;
}

/**
 * @brief Parse one long option (name only; value via @c --opt=val or next arg).
 *
 * @param name        option name with leading "--" stripped, value stripped
 * @param val         value if given as "--opt=val" (NULL otherwise)
 * @param consumed_val set to true if @p val was consumed
 * @param opts        options to fill
 * @return 0 on success, non-zero on error
 */
static int _less_parse_long(const char * name, const char * val,
                            bool * consumed_val, less_opts_t * opts)
{
    *consumed_val = false;

    if (strcmp(name, "help") == 0)              { _less_print_help();     }
    if (strcmp(name, "version") == 0)           { _less_print_version();  }

    if (strcmp(name, "quit-at-eof") == 0)        { opts->quit_at_eof = true;        return 0; }
    if (strcmp(name, "QUIT-AT-EOF") == 0)        { opts->quit_at_eof_two = true;    return 0; }
    if (strcmp(name, "quit-if-one-screen") == 0) { opts->quit_if_one_screen = true; return 0; }
    if (strcmp(name, "force") == 0)              { opts->force = true;              return 0; }
    if (strcmp(name, "squeeze-blank-lines") == 0){ opts->squeeze_blank = true;     return 0; }
    if (strcmp(name, "chop-long-lines") == 0)    { opts->chop_long = true;          return 0; }
    if (strcmp(name, "status-column") == 0)      { opts->status_col = true;        return 0; }
    if (strcmp(name, "LINE-NUMBERS") == 0)       { opts->line_numbers = true;       return 0; }
    if (strcmp(name, "RAW-CONTROL-CHARS") == 0)  { opts->raw_ctrl_ansi = true;     return 0; }
    if (strcmp(name, "raw-control-chars") == 0)  { opts->raw_ctrl = true;          return 0; }
    if (strcmp(name, "ignore-case") == 0)        { opts->ignore_case_soft = true;  return 0; }
    if (strcmp(name, "IGNORE-CASE") == 0)        { opts->ignore_case_hard = true;  return 0; }
    if (strcmp(name, "no-tilde") == 0)           { opts->no_tilde = true;          return 0; }
    if (strcmp(name, "no-keypad") == 0)          { opts->no_keypad = true;         return 0; }
    if (strcmp(name, "no-less-open") == 0)       { opts->no_less_open = true;       return 0; }
    if (strcmp(name, "hilite-search") == 0)      { opts->hilite_first = true;      return 0; }
    if (strcmp(name, "HILITE-SEARCH") == 0)     { opts->hilite_none = true;       return 0; }

    if (strcmp(name, "tabs") == 0) {
        if (!val) { return 2; }
        if (!_less_str2uint(val, &opts->tab_width)) { return 2; }
        *consumed_val = true;
        return 0;
    }
    if (strcmp(name, "buffers") == 0) {
        if (!val) { return 2; }
        unsigned b = 0;
        if (!_less_str2uint(val, &b)) { return 2; }
        opts->buffers = b;
        *consumed_val = true;
        return 0;
    }
    if (strcmp(name, "max-back-scroll") == 0) {
        if (!val) { return 2; }
        unsigned b = 0;
        if (!_less_str2uint(val, &b)) { return 2; }
        opts->max_back_scroll = b;
        *consumed_val = true;
        return 0;
    }
    if (strcmp(name, "log-file") == 0) {
        if (!val) { return 2; }
        opts->log_file = val;
        opts->log_overwrite = false;
        *consumed_val = true;
        return 0;
    }
    if (strcmp(name, "LOG-FILE") == 0) {
        if (!val) { return 2; }
        opts->log_file = val;
        opts->log_overwrite = true;
        *consumed_val = true;
        return 0;
    }
    if (strcmp(name, "prompt") == 0) {
        if (!val) { return 2; }
        opts->prompt = val;
        *consumed_val = true;
        return 0;
    }

    return 1;
}

/**
 * @brief Apply a @c +cmd initial command.
 *
 * Recognised forms:
 *   - @c +N  : toggle line numbers on (same as @c -N)
 *   - @c +n  : line numbers off (no-op for streaming since default is off)
 *   - @c +F  : follow mode (no-op for streaming)
 *   - @c +/pat : jump to first match of @p pat (same as @c -p pat)
 *   - @c +&lt;number&gt; : jump to line N (no-op for streaming)
 *
 * @param s    the command string WITHOUT the leading '+'
 * @param opts options to update
 * @return 0 on success, non-zero on unknown command
 */
static int _less_parse_plus(const char * s, less_opts_t * opts)
{
    if (!s || !*s) { return 1; }

    if (s[0] == 'N')                { opts->line_numbers = true;  opts->follow_mode = false; return 0; }
    if (s[0] == 'n')                { opts->line_numbers = false;                            return 0; }
    if (s[0] == 'F')                { opts->follow_mode = true;                               return 0; }
    if (s[0] == 'f')                { opts->follow_mode = true;                               return 0; }
    if (s[0] == '/' && s[1] != '\0'){ opts->pattern = s + 1;                                  return 0; }

    /* Numeric: +<lineno> — accepted but no-op for streaming. */
    if (isdigit((unsigned char)s[0])) {
        char * endp = NULL;
        errno = 0;
        (void)strtoul(s, &endp, 10);
        if (errno == 0 && endp && *endp == '\0') { return 0; }
    }

    return 1;
}

/**
 * @brief Parse the full argument vector into @p opts and a file list.
 *
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        options to fill
 * @param out_files   on success, points to a heap array of file names
 * @param out_nfiles  on success, number of entries in @p *out_files
 * @return 0 on success, non-zero on error
 */
static int _less_parse_args(int argc, char ** argv, less_opts_t * opts,
                            char *** out_files, int * out_nfiles)
{
    int    i;
    int    nfiles = 0;
    int    fcap = 8;
    char **files = (char **)calloc((size_t)fcap, sizeof(char *));

    if (!files) { return -1; }

    for (i = 1; i < argc; i++) {
        const char * a = argv[i];

        /* '+' initial command (e.g. +N, +F, +/pat). */
        if (a[0] == '+' && a[1] != '\0') {
            if (_less_parse_plus(a + 1, opts) != 0) {
                less_eprintf("less: invalid initial command %s\n", a);
                free(files);
                return 2;
            }
            continue;
        }

        /* '-' short option cluster, or '--' long option. */
        if (a[0] == '-' && a[1] != '\0') {
            if (a[1] == '-') {
                /* Long option. */
                const char * name = a + 2;
                const char * val  = NULL;
                char *       eq   = strchr(name, '=');
                if (eq) { *eq = '\0'; val = eq + 1; }

                int r = _less_parse_long(name, val, &(bool){false}, opts);
                if (r == 2 && val == NULL && i + 1 < argc) {
                    /* Value supplied as the next argv. */
                    bool consumed = false;
                    r = _less_parse_long(name, argv[i + 1], &consumed, opts);
                    if (consumed) { i++; }
                }
                if (r != 0) {
                    if (r == 1) {
                        less_eprintf("less: unknown option %s\n", a);
                    }
                    free(files);
                    return 2;
                }
                continue;
            }
            else {
                /* Short cluster: -abc → process each char. */
                size_t k;
                size_t alen = strlen(a);
                for (k = 1; k < alen; k++) {
                    const char * next_arg = NULL;
                    bool         consumed = false;
                    int          r;

                    /* Options that take an argument consume the rest of the
                     * cluster (if any) or the next argv element. */
                    if (a[k] == 'x' || a[k] == 'p' ||
                        a[k] == 'o' || a[k] == 'O' || a[k] == 'P') {
                        if (k + 1 < alen) { next_arg = a + k + 1; }
                        else if (i + 1 < argc) { next_arg = argv[i + 1]; }
                    }
                    r = _less_parse_short(a[k], next_arg, &consumed, opts);
                    if (r != 0) {
                        if (r == 1) {
                            less_eprintf("less: unknown option -%c\n", a[k]);
                        }
                        else if (r == 2) {
                            less_eprintf("less: option -%c requires an argument\n", a[k]);
                        }
                        free(files);
                        return 2;
                    }
                    if (consumed) {
                        if (next_arg == a + k + 1) { k = alen; }      /* consumed rest of cluster */
                        else if (next_arg == argv[i + 1]) { i++; }    /* consumed next argv */
                        break;
                    }
                }
                continue;
            }
        }

        /* Positional: input file. */
        if (nfiles >= fcap) {
            int    ncap = fcap * 2;
            char **np = (char **)realloc(files, (size_t)ncap * sizeof(char *));
            if (!np) { free(files); return -1; }
            fcap = ncap;
            files = np;
        }
        files[nfiles++] = argv[i];
    }

    *out_files  = files;
    *out_nfiles = nfiles;
    return 0;
}

/* --------------------------------------------------------------------
 *  Windows console encoding (ported from uniq.c)
 * -------------------------------------------------------------------- */

#ifdef LESS_PLATFORM_WINDOWS

/**
 * @brief Reconstruct a UTF-8 encoded argv from the real process command line.
 *
 * The MSVCRT main() argv array is populated from the process command line
 * using the active ANSI code page, which is typically CP936 on Chinese
 * hosts.  Any non-ASCII argument (CJK filenames, patterns) is therefore
 * byte-mangled before main even runs.  We re-read the authoritative
 * UTF-16 command line via GetCommandLineW(), parse it with
 * CommandLineToArgvW(), and transcode every element to canonical UTF-8.
 *
 * Returns @c NULL on OOM / API failure and leaves @p *pargc unchanged so
 * callers can fall back to the original argv.
 */
static char ** _less_argv_utf8_alloc(int * pargc, char ** argv_orig)
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
        for (j = 0; j < i; j++) { less_safe_free(u8a[j]); }
        less_safe_free(u8a);
        LocalFree(wargv);
        return NULL;
    }

    LocalFree(wargv);
    u8a[wargc] = NULL;
    *pargc = wargc;
    return u8a;
}

/**
 * @brief Release an argv vector produced by _less_argv_utf8_alloc.
 */
static void _less_argv_utf8_free(int argc, char ** argv_utf8)
{
    int i;
    if (!argv_utf8) { return; }
    for (i = 0; i < argc; i++) { less_safe_free(argv_utf8[i]); }
    less_safe_free(argv_utf8);
}

/**
 * @brief Resolve a stdio fd (0/1/2) to its Win32 HANDLE.
 */
static HANDLE _less_std_handle_for_fd(int fd)
{
    if (fd == 0)      { return GetStdHandle(STD_INPUT_HANDLE);  }
    else if (fd == 1) { return GetStdHandle(STD_OUTPUT_HANDLE); }
    else if (fd == 2) { return GetStdHandle(STD_ERROR_HANDLE);  }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief Test whether @p fp is bound to a real console (screen buffer).
 */
static bool _less_is_console_stream(FILE * fp)
{
    HANDLE h;
    DWORD  mode = 0;
    int    fd;

    if (!fp) { return false; }
    fd = _fileno(fp);
    h  = _less_std_handle_for_fd(fd);
    if (h == INVALID_HANDLE_VALUE || !h) { return false; }
    return (GetConsoleMode(h, &mode) != FALSE);
}

/**
 * @brief Windows output router for stdout/stderr.
 *
 * - Console: UTF-8 → UTF-16LE → WriteConsoleW (correct CJK display on
 *   legacy CP936 consoles).
 * - Disk / pipe / other: raw UTF-8 bytes.  Modern PowerShell 5.x+ hosts
 *   decode piped bytes with Console.OutputEncoding (UTF-8 on modern
 *   systems), and byte-exact tests require parity with GNU less output
 *   (disk and pipe must use identical bytes).
 * - Non-stdio streams (explicit -o FILE, temporary buffers, ...): plain
 *   fwrite.
 */
static size_t _less_write_win32(const void * buf, size_t len, FILE * fp)
{
    int  fd;
    bool is_std;

    if (!fp) { return 0; }
    fd     = _fileno(fp);
    is_std = (fd == 1) || (fd == 2);

    /* Explicit OUTPUT files, temp streams, etc. -> raw UTF-8 bytes. */
    if (!is_std) { return fwrite(buf, 1, len, fp); }

    if (_less_is_console_stream(fp)) {
        int       wlen;
        wchar_t * wbuf;
        DWORD     written = 0;
        BOOL      ok;
        HANDLE    h = _less_std_handle_for_fd(fd);

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

    /* disk, pipe, and unknown streams all emit raw UTF-8 bytes. */
    return fwrite(buf, 1, len, fp);
}

#endif  /* LESS_PLATFORM_WINDOWS */
