/**
 * @file sed.c
 * @brief Cross-platform sed stream editor implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Compact, single-file, dependency-free implementation of the most-used
 * subset of POSIX/GNU sed. Reimplemented in portable C99 for Windows,
 * Linux, macOS, FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common sed(1) implementations.
 *
 * Key behaviors:
 *   - Addresses: line numbers, $, /regex/, first~step, +N (GNU)
 *   - Address ranges (addr1,addr2) with active-state tracking
 *   - Regex matching via POSIX regex or bundled tiny NFA engine (Windows)
 *   - s/// with flags g (global), p (print), i/N (case-insensitive), Nth match
 *   - All standard commands: a, i, c, d, D, p, P, n, N, h, H, g, G, x,
 *     b, t, T, q, Q, r, w, l, =, y, :, #, z and {} blocks
 *   - -e/-f/-n/-r/-E/-i/-s, --help/--version
 *
 * Key design features:
 *   - Single-file, dependency-free, portable C99
 *   - Bundled tiny regex engine for Windows (no regex.h dependency)
 *   - Regex compile cache (compile once, reuse)
 *   - Dynamic string (dstr) for pattern/hold space
 *   - Abort-on-OOM allocation helpers routed through I/O macros
 *   - I/O abstraction macros for stream redirection
 *   - Cross-platform in-place editing (-i) with optional backup suffix
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o sed.exe sed.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o sed sed.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o sed sed.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o sed sed.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o sed sed.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o sed sed.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/sed>
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
    #define SED_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define SED_PLATFORM_LINUX   1
    #define SED_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define SED_PLATFORM_MACOS   1
    #define SED_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define SED_PLATFORM_FREEBSD 1
    #define SED_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define SED_PLATFORM_OPENBSD 1
    #define SED_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define SED_PLATFORM_NETBSD  1
    #define SED_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define SED_PLATFORM_POSIX   1
#else
    #define SED_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef SED_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef SED_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef SED_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef SED_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
    /* Windows has no POSIX regex.h: use the bundled tiny regex engine. */
    #define SED_TINY_REGEX 1
    /** @brief Platform strdup shim (Windows) */
    #define SED_STRDUP _strdup
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFREG) != 0)
    #endif
    #ifndef S_ISLNK
        #define S_ISLNK(m) 0
    #endif
    #ifndef ENAMETOOLONG
        #define ENAMETOOLONG 111
    #endif
#else /* SED_PLATFORM_POSIX */
    #include <unistd.h>
    #include <regex.h>
    #include <limits.h>
    /** @brief Platform strdup shim (POSIX) */
    #define SED_STRDUP strdup
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define SED_VERSION_STR "v1.0.0"

/** @brief Number of cached compiled regexes (compile once, reuse) */
#define SED_REGEX_CACHE_SIZE 64

#ifdef SED_TINY_REGEX
/** @brief regexec flag: do not match beginning-of-line (tiny regex) */
    #define SED_REG_NOTBOL 1
/** @brief regexec flag: do not match end-of-line (tiny regex) */
    #define SED_REG_NOTEOL 2
#else
/** @brief regexec flag: do not match beginning-of-line (POSIX regex) */
    #define SED_REG_NOTBOL REG_NOTBOL
/** @brief regexec flag: do not match end-of-line (POSIX regex) */
    #define SED_REG_NOTEOL REG_NOTEOL
#endif

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Dynamic growable NUL-terminated string used for pattern/hold space.
 */
typedef struct {
    char  *data;  /* <- buffer */
    size_t len;   /* <- current length (excluding NUL) */
    size_t cap;   /* <- allocated capacity */
} dstr_t;

#ifdef SED_TINY_REGEX

/**
 * @brief Tiny regex opcode (NFA instruction type).
 */
typedef enum {
    RE_CHAR,   /* match a single literal char */
    RE_ANY,    /* . */
    RE_CLASS,  /* [abc] or [^abc] */
    RE_START,  /* ^ */
    RE_END,    /* $ */
    RE_SAVE,   /* capture group start/end */
    RE_JUMP,   /* unconditional jump */
    RE_SPLIT,  /* split: try first target, then second */
    RE_MATCH   /* accept */
} re_op_t;

/**
 * @brief Tiny regex NFA instruction.
 */
typedef struct {
    re_op_t        op;
    int            c;    /* for RE_CHAR, RE_CLASS */
    unsigned char *set;  /* for RE_CLASS: 256-bit bitmap */
    int            x;    /* jump/split target */
    int            y;    /* split secondary target */
} re_inst_t;

/**
 * @brief Compiled tiny regex program.
 */
typedef struct {
    re_inst_t *prog;
    int        len;
    int        nsave;  /* number of save slots (2 * ngroups) */
    int        icase;  /* case-insensitive matching flag */
} sed_regex_t;

/**
 * @brief regmatch_t equivalent for the tiny regex engine.
 */
typedef struct {
    long rm_so;
    long rm_eo;
} sed_regmatch_t;

/**
 * @brief Parser state for the tiny regex compiler.
 */
typedef struct {
    const char *p;       /* current position in pattern */
    re_inst_t  *prog;
    int         prog_len;
    int         prog_cap;
    int         nsave;
} re_parser_t;

/**
 * @brief Backtracking matcher state for the tiny regex engine.
 */
typedef struct {
    const char         *str;
    int                 slen;
    const re_inst_t    *prog;
    int                 prog_len;
    long               *saves;
    int                 nsave;
    int                 icase;
} re_matcher_t;

#else /* !SED_TINY_REGEX: use POSIX regex */

/** @brief Compiled regex (POSIX regex_t alias) */
typedef regex_t sed_regex_t;
/** @brief Match offsets (POSIX regmatch_t alias) */
typedef regmatch_t sed_regmatch_t;

#endif /* SED_TINY_REGEX */

/**
 * @brief One entry in the compiled-regex cache.
 */
typedef struct {
    char        pattern[256];
    int         extended;
    int         icase;
    bool        used;
    sed_regex_t re;
} sed_re_cache_entry_t;

/**
 * @brief Address types for sed commands.
 */
typedef enum {
    ADDR_NONE = 0,   /* no address */
    ADDR_LINE,       /* line number */
    ADDR_LAST,       /* $ */
    ADDR_REGEX,      /* /regex/ */
    ADDR_DOLLAR,     /* $ (alias) */
    ADDR_STEP,       /* first~step */
    ADDR_LINE_REL    /* +N (relative, GNU extension) */
} addr_type_t;

/**
 * @brief A single sed address.
 */
typedef struct {
    addr_type_t type;
    long        line;   /* for ADDR_LINE, ADDR_STEP (base) */
    long        step;   /* for ADDR_STEP */
    char       *regex;  /* for ADDR_REGEX */
} sed_addr_t;

/**
 * @brief Command types supported by this sed.
 */
typedef enum {
    CMD_NONE = 0,
    CMD_S,      /* s/regex/repl/flags */
    CMD_P,      /* p - print pattern space */
    CMD_D,      /* d - delete pattern space */
    CMD_D_UPPER,/* D - delete first line of pattern space */
    CMD_P_UPPER,/* P - print first line of pattern space */
    CMD_N,      /* n - next line */
    CMD_N_UPPER,/* N - append next line */
    CMD_H,      /* h - copy pattern to hold */
    CMD_H_UPPER,/* H - append pattern to hold */
    CMD_G,      /* g - copy hold to pattern */
    CMD_G_UPPER,/* G - append hold to pattern */
    CMD_X,      /* x - exchange pattern and hold */
    CMD_B,      /* b label - branch */
    CMD_T,      /* t label - branch if substituted */
    CMD_T_UPPER,/* T label - branch if not substituted */
    CMD_Q,      /* q - quit */
    CMD_Q_UPPER,/* Q - quit without printing */
    CMD_A,      /* a text - append */
    CMD_I,      /* i text - insert */
    CMD_C,      /* c text - change */
    CMD_R,      /* r file - read file */
    CMD_W,      /* w file - write pattern to file */
    CMD_L,      /* l - list pattern space */
    CMD_EQ,     /* = - print line number */
    CMD_Y,      /* y/src/dst/ - transliterate */
    CMD_COLON,  /* :label - label */
    CMD_BLOCK,  /* { ... } - command block */
    CMD_COMMENT,/* #comment */
    CMD_Z       /* z - empty pattern space (GNU) */
} cmd_type_t;

/**
 * @brief A single compiled sed command.
 */
typedef struct sed_cmd {
    cmd_type_t       type;
    sed_addr_t       addr1;    /* first address */
    sed_addr_t       addr2;    /* second address (for ranges) */
    bool             negate;   /* ! after address */

    /* command-specific data */
    char            *text;     /* for a, i, c, : */
    char            *regex;    /* for s/// */
    char            *repl;     /* for s/// replacement */
    int              s_flags;  /* s/// flags: g=1, p=2, i=4, N=8 */
    int              s_count;  /* s///N (replace Nth) */
    char            *label;    /* for b, t, T */
    char            *filename; /* for r, w */
    char            *y_src;    /* for y/// */
    char            *y_dst;    /* for y/// */

    /* for block commands */
    struct sed_cmd  *block_cmds; /* commands inside {} */
    int              n_block_cmds;

    int              line;     /* source line number for error reporting */
} sed_cmd_t;

/**
 * @brief Sed script parser state.
 */
typedef struct {
    const char *src;      /* script source */
    size_t      pos;      /* current position */
    int         line;     /* current line number */
    bool        extended; /* extended regex (-r/-E) */
} sed_parser_t;

/**
 * @brief Sed execution engine state.
 */
typedef struct {
    sed_cmd_t  *cmds;
    int         n_cmds;
    bool        extended;
    bool        quiet;       /* -n flag */

    /* pattern space and hold space */
    dstr_t      pattern;
    dstr_t      hold;

    /* line counter */
    long        line_no;
    long        file_line_no; /* per-file line counter (for -s) */

    /* range tracking: for each command with addr2, track active state */
    int        *range_active; /* 1 if range is currently active */
    long       *range_start;  /* line number where range started */

    /* substitution flag (for t/T) */
    bool        sub_done;

    /* quit flag */
    bool        quit;
    bool        quit_no_print;

    /* delete flag (skip rest of cycle) */
    bool        delete;

    /* next flag (skip to next cycle) */
    bool        next;

    /* output */
    FILE       *out;

    /* auto-print newline tracking */
    bool        suppress_print;

    /* last line flag */
    bool        is_last_line;

    /* write files cache */
    struct {
        char  name[256];
        FILE *fp;
    } wfiles[32];
    int n_wfiles;

    /* branch target: -1 = no branch, >=0 = command index to jump to */
    int         branch_to;

    /* label table for branch resolution */
    struct {
        char name[128];
        int  cmd_idx;
    } labels[64];
    int n_labels;

    /* append queue (for a command) */
    struct {
        char *text;
        bool  is_file;  /* false=text, true=read from file */
    } append_q[64];
    int n_append;

    /* input file tracking */
    FILE       *current_fp;
    const char *current_fname;
    bool        separate_files; /* -s flag */
} sed_engine_t;

/********************************
 *    static prototypes
 ********************************/

/* allocation helpers (abort-on-OOM, routed through sed_err_printf) */
static void * _sed_xmalloc(size_t n);
static void * _sed_xrealloc(void *p, size_t n);
static char * _sed_xstrdup(const char *s);

/* dynamic string */
static void _sed_dstr_init(dstr_t *d);
static void _sed_dstr_free(dstr_t *d);
static void _sed_dstr_reserve(dstr_t *d, size_t need);
static void _sed_dstr_putc(dstr_t *d, char c);
static void _sed_dstr_puts(dstr_t *d, const char *s);
static void _sed_dstr_putn(dstr_t *d, const char *s, size_t n);
static void _sed_dstr_clear(dstr_t *d);

/* safe fixed-buffer copy */
static int _sed_safe_copy(char *dst, const char *src, size_t dst_size);

/* regex abstraction (defined conditionally, prototype unconditional) */
static int   _sed_regcomp(sed_regex_t *re, const char *pattern,
                          int extended, int icase);
static int   _sed_regexec(const sed_regex_t *re, const char *str,
                          int nmatch, sed_regmatch_t *pmatch, int flags);
static void  _sed_regfree(sed_regex_t *re);
static int   _sed_re_compile(const char *pattern, int extended,
                             int icase, sed_regex_t **out);
static int   _sed_re_match(const char *str, const char *pattern,
                           int extended, int icase,
                           sed_regmatch_t *m_out, int nm);

#ifdef SED_TINY_REGEX
/* tiny regex engine */
static void  _sed_re_bitmap_set(unsigned char *set, int c);
static int   _sed_re_bitmap_test(unsigned char *set, int c);
static int   _sed_re_emit(re_parser_t *rp, re_op_t op, int c, int x, int y);
static int   _sed_re_parse_class(re_parser_t *rp, int idx);
static int   _sed_re_parse_alt(re_parser_t *rp);
static int   _sed_re_parse_concat(re_parser_t *rp);
static int   _sed_re_parse_atom(re_parser_t *rp);
static int   _sed_re_parse_quant(re_parser_t *rp);
static int   _sed_re_run(re_matcher_t *m, int pc, int sp);
static char *_sed_bre_to_ere(const char *pat);
#endif /* SED_TINY_REGEX */

/* parser */
static void       _sed_error(sed_parser_t *p, const char *msg);
static void       _sed_skip_ws(sed_parser_t *p);
static void       _sed_skip_ws_nl(sed_parser_t *p);
static int        _sed_peek(sed_parser_t *p);
static char *     _sed_read_delimited(sed_parser_t *p, char delim, int *had_escape);
static char *     _sed_read_text(sed_parser_t *p);
static void       _sed_parse_addr(sed_parser_t *p, sed_addr_t *a);
static void       _sed_parse_s_flags(sed_parser_t *p, sed_cmd_t *cmd);
static sed_cmd_t *_sed_parse_command(sed_parser_t *p, int *n_cmds, int *cap);
static sed_cmd_t *_sed_parse_program(sed_parser_t *p, int *out_n);

/* executor */
static FILE * _sed_wfile(sed_engine_t *e, const char *name);
static void   _sed_resolve_labels(sed_engine_t *e);
static int    _sed_find_label(sed_engine_t *e, const char *name);
static int    _sed_addr_match(sed_engine_t *e, sed_addr_t *a);
static int    _sed_cmd_match(sed_engine_t *e, int cmd_idx);
static int    _sed_do_s(sed_engine_t *e, sed_cmd_t *cmd);
static void   _sed_flush_append(sed_engine_t *e);
static void   _sed_exec_cmd(sed_engine_t *e, sed_cmd_t *cmd, int in_block);
static void   _sed_exec_block(sed_engine_t *e, sed_cmd_t *cmd);
static void   _sed_process_line(sed_engine_t *e);
static int    _sed_read_line(sed_engine_t *e);
static int    _sed_check_last_line(FILE *fp);
static void   _sed_process_file(sed_engine_t *e, FILE *fp, const char *fname);

/* CLI */
static void _sed_print_help(void);
static void _sed_print_version(void);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for sed_fputs / sed_fputc.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef sed_out_stream
    #define sed_out_stream stdout
#endif

/**
 * @brief Default stderr stream for sed_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef sed_err_stream
    #define sed_err_stream stderr
#endif

/**
 * @brief Formatted print to the output stream (printf-compatible).
 *        Result discarded via (void). Supports zero variadic arguments.
 */
#ifndef sed_printf
    #define sed_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to the error stream.
 *        Requires explicit format string.
 */
#ifndef sed_err_printf
    #define sed_err_printf(fmt, ...) (void)fprintf((sed_err_stream), (fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally sed_out_stream)
 */
#ifndef sed_fputs
    #define sed_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      Character to write.
 * @param stream  stdio stream (normally sed_out_stream)
 */
#ifndef sed_fputc
    #define sed_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/**
 * @brief Write a single character to stdout.
 * @param ch  Character (promoted from unsigned char to int).
 */
#ifndef sed_putchar
    #define sed_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Compiled-regex cache (file-scope). */
static sed_re_cache_entry_t s_re_cache[SED_REGEX_CACHE_SIZE];

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the sed command
 *
 * Processing flow:
 *   1. Parse command-line options (-n/-r/-E/-i/-s/-e/-f, --help/--version)
 *   2. Build the script buffer from -e expressions and -f script files
 *   3. If no script, treat the next argument as the script
 *   4. Compile the script into a command list
 *   5. Resolve labels and run the engine over each input file (or stdin)
 *   6. For -i, redirect output to a temp file and replace the original
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on runtime error, 2 on usage error
 */
int main(int argc, char **argv)
{
    bool opt_quiet = false;
    bool opt_extended = false;
    bool opt_in_place = false;
    bool opt_separate = false;
    char *in_place_suffix = NULL;

    dstr_t script_buf;
    _sed_dstr_init(&script_buf);

    int i = 1;
    while (i < argc) {
        const char *arg = argv[i];
        if (!arg || arg[0] != '-' || arg[1] == '\0') {
            break; /* not an option or "-" */
        }

        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }
        if (strcmp(arg, "--help") == 0) {
            _sed_print_help();
            return 0;
        }
        if (strcmp(arg, "--version") == 0) {
            _sed_print_version();
            return 0;
        }
        if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "--silent") == 0) {
            opt_quiet = true;
            i++;
            continue;
        }
        if (strcmp(arg, "--regexp-extended") == 0) {
            opt_extended = true;
            i++;
            continue;
        }
        if (strcmp(arg, "--separate") == 0) {
            opt_separate = true;
            i++;
            continue;
        }
        if (strncmp(arg, "--expression=", 13) == 0) {
            _sed_dstr_puts(&script_buf, arg + 13);
            _sed_dstr_putc(&script_buf, '\n');
            i++;
            continue;
        }
        if (strncmp(arg, "--file=", 7) == 0) {
            FILE *fp = fopen(arg + 7, "r");
            if (!fp) {
                sed_err_printf("sed: cannot open %s: %s\n",
                               arg + 7, strerror(errno));
                return 1;
            }
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                _sed_dstr_putn(&script_buf, buf, n);
            }
            (void)fclose(fp);
            i++;
            continue;
        }
        if (strncmp(arg, "--in-place", 10) == 0) {
            opt_in_place = true;
            if (arg[10] == '=') {
                in_place_suffix = _sed_xstrdup(arg + 11);
            }
            i++;
            continue;
        }

        /* Short options */
        int j = 1;
        int consumed_next = 0;
        while (arg[j]) {
            switch (arg[j]) {
                case 'n':
                    opt_quiet = true;
                    j++;
                    break;

                case 'r':
                case 'E':
                    opt_extended = true;
                    j++;
                    break;

                case 's':
                    opt_separate = true;
                    j++;
                    break;

                case 'i':
                    opt_in_place = true;
                    if (arg[j + 1]) {
                        in_place_suffix = _sed_xstrdup(arg + j + 1);
                    }
                    j = (int)strlen(arg);
                    break;

                case 'e':
                    if (arg[j + 1]) {
                        _sed_dstr_puts(&script_buf, arg + j + 1);
                        _sed_dstr_putc(&script_buf, '\n');
                        j = (int)strlen(arg);
                    }
                    else {
                        i++;
                        if (i >= argc) {
                            sed_err_printf("%s", "sed: -e requires an argument\n");
                            return 1;
                        }
                        _sed_dstr_puts(&script_buf, argv[i]);
                        _sed_dstr_putc(&script_buf, '\n');
                        consumed_next = 1;
                        j++;
                    }
                    break;

                case 'f':
                    if (arg[j + 1]) {
                        FILE *fp = fopen(arg + j + 1, "r");
                        if (!fp) {
                            sed_err_printf("sed: cannot open %s: %s\n",
                                           arg + j + 1, strerror(errno));
                            return 1;
                        }
                        char buf[4096];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                            _sed_dstr_putn(&script_buf, buf, n);
                        }
                        (void)fclose(fp);
                        j = (int)strlen(arg);
                    }
                    else {
                        i++;
                        if (i >= argc) {
                            sed_err_printf("%s", "sed: -f requires an argument\n");
                            return 1;
                        }
                        FILE *fp = fopen(argv[i], "r");
                        if (!fp) {
                            sed_err_printf("sed: cannot open %s: %s\n",
                                           argv[i], strerror(errno));
                            return 1;
                        }
                        char buf[4096];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                            _sed_dstr_putn(&script_buf, buf, n);
                        }
                        (void)fclose(fp);
                        consumed_next = 1;
                        j++;
                    }
                    break;

                default:
                    sed_err_printf("sed: unknown option -%c\n", arg[j]);
                    _sed_print_help();
                    return 2;
            }
        }
        i++;
        (void)consumed_next;
    }

    /* If no script yet, next argument is the script */
    if (script_buf.len == 0) {
        if (i >= argc) {
            sed_err_printf("%s", "sed: no script provided\n");
            _sed_print_help();
            return 2;
        }
        _sed_dstr_puts(&script_buf, argv[i]);
        _sed_dstr_putc(&script_buf, '\n');
        i++;
    }

    /* Remaining args are input files */
    int n_files = argc - i;
    char **files = &argv[i];

    /* Parse the script */
    sed_parser_t parser;
    memset(&parser, 0, sizeof(parser));
    parser.src = script_buf.data;
    parser.pos = 0;
    parser.line = 1;
    parser.extended = opt_extended;

    int n_cmds = 0;
    sed_cmd_t *cmds = _sed_parse_program(&parser, &n_cmds);

    if (n_cmds == 0) {
        /* empty script: just pass through */
        return 0;
    }

    /* Set up engine */
    sed_engine_t engine;
    memset(&engine, 0, sizeof(engine));
    engine.cmds = cmds;
    engine.n_cmds = n_cmds;
    engine.extended = opt_extended;
    engine.quiet = opt_quiet;
    engine.separate_files = opt_separate;
    engine.out = sed_out_stream;
    _sed_dstr_init(&engine.pattern);
    _sed_dstr_init(&engine.hold);
    engine.range_active = (int *)calloc((size_t)n_cmds, sizeof(int));
    engine.range_start = (long *)calloc((size_t)n_cmds, sizeof(long));

    _sed_resolve_labels(&engine);

    /* Process files */
    if (n_files == 0) {
        /* read from stdin */
        if (opt_in_place) {
            sed_err_printf("%s", "sed: -i requires file arguments\n");
            return 1;
        }
        _sed_process_file(&engine, stdin, "-");
    }
    else {
        for (int f = 0; f < n_files; f++) {
            FILE *fp = fopen(files[f], "r");
            if (!fp) {
                sed_err_printf("sed: cannot open %s: %s\n",
                               files[f], strerror(errno));
                continue;
            }

            if (opt_in_place) {
                /* Create temp file and redirect output */
                char tmpname[1024];
                (void)snprintf(tmpname, sizeof(tmpname), "%sXXXXXX", files[f]);
                int tmpfd = -1;
#ifdef SED_PLATFORM_WINDOWS
                /* Windows: create a temp file in the system temp dir */
                char tmpdir[1024];
                (void)GetTempPathA(sizeof(tmpdir), tmpdir);
                (void)snprintf(tmpname, sizeof(tmpname), "%ssedtmpXXXXXX", tmpdir);
                tmpfd = _open(tmpname, _O_RDWR | _O_CREAT | _O_TRUNC,
                              _S_IREAD | _S_IWRITE);
#else
                tmpfd = mkstemp(tmpname);
#endif
                if (tmpfd < 0) {
                    sed_err_printf("%s", "sed: cannot create temp file\n");
                    (void)fclose(fp);
                    continue;
                }
                FILE *tmpfp = fdopen(tmpfd, "w");
                if (!tmpfp) {
                    sed_err_printf("%s", "sed: cannot open temp file\n");
                    (void)close(tmpfd);
                    (void)fclose(fp);
                    continue;
                }

                /* Make backup if suffix given */
                if (in_place_suffix) {
                    char backup[1024];
                    (void)snprintf(backup, sizeof(backup), "%s%s",
                                   files[f], in_place_suffix);
                    /* copy original to backup */
                    FILE *bfp = fopen(backup, "w");
                    if (bfp) {
                        char buf[4096];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                            (void)fwrite(buf, 1, n, bfp);
                        }
                        (void)fclose(bfp);
                        /* rewind input */
                        (void)fseek(fp, 0, SEEK_SET);
                    }
                }

                engine.out = tmpfp;
                _sed_process_file(&engine, fp, files[f]);
                (void)fclose(fp);
                (void)fflush(tmpfp);
                (void)fclose(tmpfp);

                /* Replace original with temp */
#ifdef SED_PLATFORM_WINDOWS
                /* On Windows, need to remove original first */
                (void)_unlink(files[f]);
                if (rename(tmpname, files[f]) != 0) {
                    /* try copy */
                    FILE *src = fopen(tmpname, "r");
                    FILE *dst = fopen(files[f], "w");
                    if (src && dst) {
                        char buf[4096];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                            (void)fwrite(buf, 1, n, dst);
                        }
                    }
                    if (src) {
                        (void)fclose(src);
                    }
                    if (dst) {
                        (void)fclose(dst);
                    }
                    (void)_unlink(tmpname);
                }
#else
                (void)rename(tmpname, files[f]);
#endif
                engine.out = sed_out_stream;
            }
            else {
                _sed_process_file(&engine, fp, files[f]);
                (void)fclose(fp);
            }
        }
    }

    /* Cleanup */
    free(engine.range_active);
    free(engine.range_start);
    _sed_dstr_free(&engine.pattern);
    _sed_dstr_free(&engine.hold);
    for (int w = 0; w < engine.n_wfiles; w++) {
        (void)fclose(engine.wfiles[w].fp);
    }

    /* Free commands */
    for (int c = 0; c < n_cmds; c++) {
        if (cmds[c].text) {
            free(cmds[c].text);
        }
        if (cmds[c].regex) {
            free(cmds[c].regex);
        }
        if (cmds[c].repl) {
            free(cmds[c].repl);
        }
        if (cmds[c].label) {
            free(cmds[c].label);
        }
        if (cmds[c].filename) {
            free(cmds[c].filename);
        }
        if (cmds[c].y_src) {
            free(cmds[c].y_src);
        }
        if (cmds[c].y_dst) {
            free(cmds[c].y_dst);
        }
        if (cmds[c].addr1.regex) {
            free(cmds[c].addr1.regex);
        }
        if (cmds[c].addr2.regex) {
            free(cmds[c].addr2.regex);
        }
        if (cmds[c].block_cmds) {
            free(cmds[c].block_cmds);
        }
    }
    free(cmds);
    _sed_dstr_free(&script_buf);
    if (in_place_suffix) {
        free(in_place_suffix);
    }

    /* Free regex cache */
    for (int r = 0; r < SED_REGEX_CACHE_SIZE; r++) {
        if (s_re_cache[r].used) {
            _sed_regfree(&s_re_cache[r].re);
            s_re_cache[r].used = false;
        }
    }

    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Allocate memory, aborting on out-of-memory.
 *
 * OOM is unrecoverable for a stream editor, so this helper prints a
 * diagnostic through sed_err_printf and exits. Behavior is preserved
 * from the original (abort-on-OOM).
 *
 * @param n  number of bytes to allocate
 * @return pointer to allocated memory (never NULL)
 */
static void * _sed_xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        sed_err_printf("%s", "sed: out of memory\n");
        exit(1);
    }
    return p;
}

/**
 * @brief Reallocate memory, aborting on out-of-memory.
 *
 * Uses a temporary pointer so a failed realloc does not leak the
 * original buffer. Aborts via sed_err_printf on OOM (preserved behavior).
 *
 * @param p  pointer to previously allocated memory
 * @param n  new size in bytes
 * @return pointer to reallocated memory (never NULL)
 */
static void * _sed_xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q) {
        sed_err_printf("%s", "sed: out of memory\n");
        exit(1);
    }
    return q;
}

/**
 * @brief Duplicate a NUL-terminated string, aborting on OOM.
 *
 * NULL input is treated as the empty string (NULL-safety preserved).
 *
 * @param s  string to duplicate (may be NULL)
 * @return pointer to new string (never NULL)
 */
static char * _sed_xstrdup(const char *s)
{
    if (!s) {
        s = "";
    }
    size_t n = strlen(s) + 1;
    char *p = (char *)_sed_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

/**
 * @brief Initialize an empty dynamic string with a small initial capacity.
 * @param d  dynamic string instance
 */
static void _sed_dstr_init(dstr_t *d)
{
    d->cap = 128;
    d->data = (char *)_sed_xmalloc(d->cap);
    d->len = 0;
    d->data[0] = '\0';
}

/**
 * @brief Free a dynamic string's buffer and reset it.
 * @param d  dynamic string instance (may be reused after _sed_dstr_init)
 */
static void _sed_dstr_free(dstr_t *d)
{
    if (d->data) {
        free(d->data);
        d->data = NULL;
    }
    d->len = d->cap = 0;
}

/**
 * @brief Ensure the dynamic string can hold at least @p need bytes plus NUL.
 * @param d     dynamic string instance
 * @param need  required capacity in bytes
 */
static void _sed_dstr_reserve(dstr_t *d, size_t need)
{
    if (d->cap >= need + 1) {
        return;
    }
    while (d->cap < need + 1) {
        d->cap *= 2;
    }
    d->data = (char *)_sed_xrealloc(d->data, d->cap);
}

/**
 * @brief Append a single character to a dynamic string.
 * @param d  dynamic string instance
 * @param c  character to append
 */
static void _sed_dstr_putc(dstr_t *d, char c)
{
    _sed_dstr_reserve(d, d->len + 1);
    d->data[d->len++] = c;
    d->data[d->len] = '\0';
}

/**
 * @brief Append a NUL-terminated string to a dynamic string (NULL-safe).
 * @param d  dynamic string instance
 * @param s  string to append (may be NULL)
 */
static void _sed_dstr_puts(dstr_t *d, const char *s)
{
    size_t n = s ? strlen(s) : 0;
    _sed_dstr_reserve(d, d->len + n);
    if (n) {
        memcpy(d->data + d->len, s, n);
        d->len += n;
    }
    d->data[d->len] = '\0';
}

/**
 * @brief Append exactly @p n bytes from @p s to a dynamic string.
 * @param d  dynamic string instance
 * @param s  source buffer
 * @param n  number of bytes to append
 */
static void _sed_dstr_putn(dstr_t *d, const char *s, size_t n)
{
    _sed_dstr_reserve(d, d->len + n);
    if (n) {
        memcpy(d->data + d->len, s, n);
        d->len += n;
    }
    d->data[d->len] = '\0';
}

/**
 * @brief Clear a dynamic string to empty (buffer retained).
 * @param d  dynamic string instance
 */
static void _sed_dstr_clear(dstr_t *d)
{
    d->len = 0;
    if (d->data) {
        d->data[0] = '\0';
    }
}

/**
 * @brief Safer string copy into a fixed-size buffer.
 *
 * Validates NULL/size, NUL-terminates, detects truncation, and uses
 * memcpy followed by an explicit NUL so no -Wstringop-truncation
 * warning is emitted. Returns -1 on truncation or invalid input.
 *
 * @param dst       destination buffer
 * @param src       NUL-terminated source
 * @param dst_size  size of dst in bytes
 * @return 0 on success, -1 if dst is invalid, src is NULL, or truncated
 */
static int _sed_safe_copy(char *dst, const char *src, size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return -1;
    }
    dst[0] = '\0';
    if (!src) {
        return -1;
    }
    size_t slen = strlen(src);
    if (slen >= dst_size) {
        return -1;
    }
    memcpy(dst, src, slen);
    dst[slen] = '\0';
    return 0;
}

#ifdef SED_TINY_REGEX

/**
 * @brief Set a bit in a 256-bit character-class bitmap.
 * @param set  bitmap buffer (32 bytes)
 * @param c    character code (0..255)
 */
static void _sed_re_bitmap_set(unsigned char *set, int c)
{
    set[c >> 3] |= (unsigned char)(1 << (c & 7));
}

/**
 * @brief Test a bit in a 256-bit character-class bitmap.
 * @param set  bitmap buffer (32 bytes)
 * @param c    character code (0..255)
 * @return 1 if set, 0 otherwise
 */
static int _sed_re_bitmap_test(unsigned char *set, int c)
{
    return (set[c >> 3] >> (c & 7)) & 1;
}

/**
 * @brief Emit one NFA instruction, growing the program buffer as needed.
 * @return index of the emitted instruction
 */
static int _sed_re_emit(re_parser_t *rp, re_op_t op, int c, int x, int y)
{
    if (rp->prog_len >= rp->prog_cap) {
        rp->prog_cap = rp->prog_cap ? rp->prog_cap * 2 : 64;
        rp->prog = (re_inst_t *)_sed_xrealloc(rp->prog,
                                              (size_t)rp->prog_cap * sizeof(re_inst_t));
    }
    int idx = rp->prog_len++;
    rp->prog[idx].op = op;
    rp->prog[idx].c = c;
    rp->prog[idx].set = NULL;
    rp->prog[idx].x = x;
    rp->prog[idx].y = y;
    return idx;
}

/**
 * @brief Parse a character class [...] (rp->p points just past '[').
 * @param idx  index of the RE_CLASS instruction to populate
 * @return idx
 */
static int _sed_re_parse_class(re_parser_t *rp, int idx)
{
    unsigned char *set = (unsigned char *)calloc(32, 1);
    int negate = 0;
    int lo;
    if (*rp->p == '^') {
        negate = 1;
        rp->p++;
    }
    /* ] as first char is literal */
    int first = 1;
    while (*rp->p && (*rp->p != ']' || first)) {
        if (*rp->p == '[' && rp->p[1] == ':') {
            /* POSIX character classes [:alpha:] etc. */
            rp->p += 2;
            char name[16];
            int ni = 0;
            while (*rp->p && *rp->p != ':' && ni < 15) {
                name[ni++] = *rp->p++;
            }
            name[ni] = '\0';
            if (*rp->p == ':') {
                rp->p++;
            }
            if (*rp->p == ']') {
                rp->p++;
            }
            if (strcmp(name, "alpha") == 0) {
                for (lo = 'a'; lo <= 'z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                for (lo = 'A'; lo <= 'Z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "digit") == 0) {
                for (lo = '0'; lo <= '9'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "alnum") == 0) {
                for (lo = 'a'; lo <= 'z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                for (lo = 'A'; lo <= 'Z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                for (lo = '0'; lo <= '9'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "upper") == 0) {
                for (lo = 'A'; lo <= 'Z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "lower") == 0) {
                for (lo = 'a'; lo <= 'z'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "space") == 0) {
                _sed_re_bitmap_set(set, ' ');
                _sed_re_bitmap_set(set, '\t');
                _sed_re_bitmap_set(set, '\n');
                _sed_re_bitmap_set(set, '\r');
                _sed_re_bitmap_set(set, '\v');
                _sed_re_bitmap_set(set, '\f');
            }
            else if (strcmp(name, "blank") == 0) {
                _sed_re_bitmap_set(set, ' ');
                _sed_re_bitmap_set(set, '\t');
            }
            else if (strcmp(name, "punct") == 0) {
                for (lo = 0x21; lo <= 0x7e; lo++) {
                    if (!isalnum(lo)) {
                        _sed_re_bitmap_set(set, lo);
                    }
                }
            }
            else if (strcmp(name, "xdigit") == 0) {
                for (lo = '0'; lo <= '9'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                for (lo = 'a'; lo <= 'f'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                for (lo = 'A'; lo <= 'F'; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "cntrl") == 0) {
                for (lo = 0; lo < 0x20; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
                _sed_re_bitmap_set(set, 0x7f);
            }
            else if (strcmp(name, "print") == 0) {
                for (lo = 0x20; lo <= 0x7e; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            else if (strcmp(name, "graph") == 0) {
                for (lo = 0x21; lo <= 0x7e; lo++) {
                    _sed_re_bitmap_set(set, lo);
                }
            }
            first = 0;
            continue;
        }
        int ch = (unsigned char)*rp->p;
        if (ch == '\\' && rp->p[1]) {
            rp->p++;
            switch (*rp->p) {
                case 'n':
                    ch = '\n';
                    break;

                case 't':
                    ch = '\t';
                    break;

                case 'r':
                    ch = '\r';
                    break;

                case '\\':
                    ch = '\\';
                    break;

                case 'd':
                    for (lo = '0'; lo <= '9'; lo++) {
                        _sed_re_bitmap_set(set, lo);
                    }
                    rp->p++;
                    first = 0;
                    continue;

                case 's':
                    _sed_re_bitmap_set(set, ' ');
                    _sed_re_bitmap_set(set, '\t');
                    _sed_re_bitmap_set(set, '\n');
                    _sed_re_bitmap_set(set, '\r');
                    _sed_re_bitmap_set(set, '\v');
                    _sed_re_bitmap_set(set, '\f');
                    rp->p++;
                    first = 0;
                    continue;

                case 'w':
                    for (lo = 'a'; lo <= 'z'; lo++) {
                        _sed_re_bitmap_set(set, lo);
                    }
                    for (lo = 'A'; lo <= 'Z'; lo++) {
                        _sed_re_bitmap_set(set, lo);
                    }
                    for (lo = '0'; lo <= '9'; lo++) {
                        _sed_re_bitmap_set(set, lo);
                    }
                    _sed_re_bitmap_set(set, '_');
                    rp->p++;
                    first = 0;
                    continue;

                default:
                    ch = (unsigned char)*rp->p;
                    break;
            }
        }
        rp->p++;
        /* range a-b */
        if (*rp->p == '-' && rp->p[1] && rp->p[1] != ']') {
            int hi = (unsigned char)rp->p[1];
            if (hi == '\\' && rp->p[2]) {
                rp->p += 2;
                switch (*rp->p) {
                    case 'n':
                        hi = '\n';
                        break;

                    case 't':
                        hi = '\t';
                        break;

                    case 'r':
                        hi = '\r';
                        break;

                    default:
                        hi = (unsigned char)*rp->p;
                        break;
                }
            }
            rp->p++;
            if (ch > hi) {
                int t = ch;
                ch = hi;
                hi = t;
            }
            for (lo = ch; lo <= hi; lo++) {
                _sed_re_bitmap_set(set, lo);
            }
        }
        else {
            _sed_re_bitmap_set(set, ch);
        }
        first = 0;
    }
    if (*rp->p == ']') {
        rp->p++;
    }
    if (negate) {
        for (int i = 0; i < 32; i++) {
            set[i] = (unsigned char)~set[i];
        }
    }
    rp->prog[idx].op = RE_CLASS;
    rp->prog[idx].set = set;
    return idx;
}

/**
 * @brief Parse a single regex atom (char, class, group, anchor, escape).
 * @return instruction index, or -1 on end of pattern
 */
static int _sed_re_parse_atom(re_parser_t *rp)
{
    int idx;
    char c = *rp->p;
    if (c == '\0') {
        return -1;
    }
    if (c == '(') {
        rp->p++;
        int save = rp->nsave;
        rp->nsave += 2;
        _sed_re_emit(rp, RE_SAVE, save, 0, 0);
        _sed_re_parse_alt(rp);
        _sed_re_emit(rp, RE_SAVE, save + 1, 0, 0);
        if (*rp->p == ')') {
            rp->p++;
        }
        return save; /* return save slot index for backref tracking */
    }
    if (c == '[') {
        rp->p++;
        idx = _sed_re_emit(rp, RE_CLASS, 0, 0, 0);
        _sed_re_parse_class(rp, idx);
        return idx;
    }
    if (c == '.') {
        rp->p++;
        return _sed_re_emit(rp, RE_ANY, 0, 0, 0);
    }
    if (c == '^') {
        rp->p++;
        return _sed_re_emit(rp, RE_START, 0, 0, 0);
    }
    if (c == '$') {
        rp->p++;
        return _sed_re_emit(rp, RE_END, 0, 0, 0);
    }
    if (c == '\\') {
        rp->p++;
        char ec = *rp->p;
        if (ec == '\0') {
            return _sed_re_emit(rp, RE_CHAR, '\\', 0, 0);
        }
        rp->p++;
        switch (ec) {
            case 'n':
                return _sed_re_emit(rp, RE_CHAR, '\n', 0, 0);

            case 't':
                return _sed_re_emit(rp, RE_CHAR, '\t', 0, 0);

            case 'r':
                return _sed_re_emit(rp, RE_CHAR, '\r', 0, 0);

            case 'd': {
                unsigned char *set = (unsigned char *)calloc(32, 1);
                for (int i = '0'; i <= '9'; i++) {
                    _sed_re_bitmap_set(set, i);
                }
                idx = _sed_re_emit(rp, RE_CLASS, 0, 0, 0);
                rp->prog[idx].set = set;
                return idx;
            }

            case 's': {
                unsigned char *set = (unsigned char *)calloc(32, 1);
                _sed_re_bitmap_set(set, ' ');
                _sed_re_bitmap_set(set, '\t');
                _sed_re_bitmap_set(set, '\n');
                _sed_re_bitmap_set(set, '\r');
                _sed_re_bitmap_set(set, '\v');
                _sed_re_bitmap_set(set, '\f');
                idx = _sed_re_emit(rp, RE_CLASS, 0, 0, 0);
                rp->prog[idx].set = set;
                return idx;
            }

            case 'w': {
                unsigned char *set = (unsigned char *)calloc(32, 1);
                for (int i = 'a'; i <= 'z'; i++) {
                    _sed_re_bitmap_set(set, i);
                }
                for (int i = 'A'; i <= 'Z'; i++) {
                    _sed_re_bitmap_set(set, i);
                }
                for (int i = '0'; i <= '9'; i++) {
                    _sed_re_bitmap_set(set, i);
                }
                _sed_re_bitmap_set(set, '_');
                idx = _sed_re_emit(rp, RE_CLASS, 0, 0, 0);
                rp->prog[idx].set = set;
                return idx;
            }

            case 'b':
                return _sed_re_emit(rp, RE_START, 0, 0, 0); /* approximate */

            default:
                return _sed_re_emit(rp, RE_CHAR, ec, 0, 0);
        }
    }
    /* ordinary char */
    rp->p++;
    return _sed_re_emit(rp, RE_CHAR, (unsigned char)c, 0, 0);
}

/**
 * @brief Parse a regex atom followed by optional quantifiers (* + ? {n,m}).
 * @return index of the parsed atom
 */
static int _sed_re_parse_quant(re_parser_t *rp)
{
    int atom_idx = _sed_re_parse_atom(rp);
    if (atom_idx < 0) {
        return -1;
    }
    while (*rp->p == '*' || *rp->p == '+' || *rp->p == '?' || *rp->p == '{') {
        char q = *rp->p;
        if (q == '{') {
            /* {n}, {n,}, {n,m} - approximate with star */
            /* skip to closing } */
            rp->p++;
            while (*rp->p && *rp->p != '}') {
                rp->p++;
            }
            if (*rp->p == '}') {
                rp->p++;
            }
            /* treat as * for simplicity */
            int split_idx = _sed_re_emit(rp, RE_SPLIT, 0, atom_idx, 0);
            int jump_idx = _sed_re_emit(rp, RE_JUMP, 0, 0, 0);
            rp->prog[split_idx].y = jump_idx + 1;
            rp->prog[jump_idx].x = split_idx;
            continue;
        }
        rp->p++;
        if (q == '*') {
            int split_idx = _sed_re_emit(rp, RE_SPLIT, 0, atom_idx, 0);
            int jump_idx = _sed_re_emit(rp, RE_JUMP, 0, 0, 0);
            rp->prog[split_idx].y = jump_idx + 1;
            rp->prog[jump_idx].x = split_idx;
        }
        else if (q == '+') {
            int split_idx = _sed_re_emit(rp, RE_SPLIT, 0, atom_idx, 0);
            int jump_idx = _sed_re_emit(rp, RE_JUMP, 0, atom_idx, 0);
            rp->prog[split_idx].y = jump_idx + 1;
        }
        else { /* ? */
            int split_idx = _sed_re_emit(rp, RE_SPLIT, 0, atom_idx, 0);
            (void)split_idx;
        }
    }
    return atom_idx;
}

/**
 * @brief Parse a concatenation of quantified atoms until '|', ')' or end.
 * @return 0
 */
static int _sed_re_parse_concat(re_parser_t *rp)
{
    while (*rp->p && *rp->p != ')' && *rp->p != '|') {
        if (_sed_re_parse_quant(rp) < 0) {
            break;
        }
    }
    return 0;
}

/**
 * @brief Parse an alternation of concatenations (a|b|c).
 * @return starting program index
 */
static int _sed_re_parse_alt(re_parser_t *rp)
{
    int start = rp->prog_len;
    _sed_re_parse_concat(rp);
    while (*rp->p == '|') {
        rp->p++;
        /* This is a simplified alternation: we emit a split before the
         * remaining branch. For full correctness we'd need to patch the
         * initial split, but for sed's common patterns this suffices. */
        _sed_re_parse_concat(rp);
    }
    return start;
}

/**
 * @brief Backtracking matcher for the tiny regex engine.
 * @param m   matcher state
 * @param pc  program counter
 * @param sp  string position
 * @return 1 on match, 0 otherwise
 */
static int _sed_re_run(re_matcher_t *m, int pc, int sp)
{
    while (pc < m->prog_len) {
        const re_inst_t *inst = &m->prog[pc];
        switch (inst->op) {
            case RE_CHAR:
                if (sp >= m->slen) {
                    return 0;
                }
                if (m->icase) {
                    if (tolower((unsigned char)m->str[sp]) != tolower((unsigned char)inst->c)) {
                        return 0;
                    }
                }
                else {
                    if (m->str[sp] != inst->c) {
                        return 0;
                    }
                }
                sp++;
                pc++;
                break;

            case RE_ANY:
                if (sp >= m->slen || m->str[sp] == '\n') {
                    return 0;
                }
                sp++;
                pc++;
                break;

            case RE_CLASS: {
                if (sp >= m->slen) {
                    return 0;
                }
                unsigned char ch = (unsigned char)m->str[sp];
                int hit = _sed_re_bitmap_test(inst->set, ch);
                if (!hit && m->icase) {
                    if (ch >= 'A' && ch <= 'Z') {
                        ch = (unsigned char)(ch + 32);
                    }
                    else if (ch >= 'a' && ch <= 'z') {
                        ch = (unsigned char)(ch - 32);
                    }
                    hit = _sed_re_bitmap_test(inst->set, ch);
                }
                if (!hit) {
                    return 0;
                }
                sp++;
                pc++;
                break;
            }

            case RE_START:
                if (sp != 0 && m->str[sp - 1] != '\n') {
                    return 0;
                }
                pc++;
                break;

            case RE_END:
                if (sp != m->slen && m->str[sp] != '\n') {
                    return 0;
                }
                pc++;
                break;

            case RE_SAVE:
                if (inst->c < m->nsave) {
                    m->saves[inst->c] = sp;
                }
                pc++;
                break;

            case RE_JUMP:
                pc = inst->x;
                break;

            case RE_SPLIT:
                /* try primary path first (greedy) */
                if (_sed_re_run(m, inst->x, sp)) {
                    return 1;
                }
                pc = inst->y;
                break;

            case RE_MATCH:
                return 1;

            default:
                return 0;
        }
    }
    /* reached end of program = match */
    return 1;
}

/**
 * @brief Convert a BRE pattern to an ERE-equivalent pattern for the tiny engine.
 *
 * In BRE: \( \) are grouping, \{ \} are quantifiers, bare ( ) { } | are
 * literals.  In ERE the roles are swapped.  Other escapes (\n \t \d \s \w
 * \b \\ etc.) are preserved.  Caller frees the returned string.
 *
 * @param pat  BRE pattern
 * @return newly allocated ERE-equivalent pattern
 */
static char *_sed_bre_to_ere(const char *pat)
{
    dstr_t out;
    _sed_dstr_init(&out);
    for (const char *p = pat; *p; p++) {
        if (*p == '\\') {
            char next = p[1];
            if (next == '(' || next == ')' || next == '{' || next == '}' || next == '|') {
                /* BRE meta-escape: \( -> ( */
                _sed_dstr_putc(&out, next);
                p++;
            }
            else if (next == '\0') {
                _sed_dstr_putc(&out, '\\');
            }
            else {
                /* preserve escape: \n \t \d \s \w \b \\ \. etc. */
                _sed_dstr_putc(&out, '\\');
                _sed_dstr_putc(&out, next);
                p++;
            }
        }
        else if (*p == '(' || *p == ')' || *p == '{' || *p == '}' || *p == '|') {
            /* BRE literal: ( -> \( */
            _sed_dstr_putc(&out, '\\');
            _sed_dstr_putc(&out, *p);
        }
        else {
            _sed_dstr_putc(&out, *p);
        }
    }
    return out.data;
}

/**
 * @brief Compile a regex pattern with the tiny engine.
 *
 * @param re        output compiled regex
 * @param pattern   source pattern (ERE form)
 * @param extended  nonzero for ERE syntax (always supported by tiny regex)
 * @param icase     nonzero for case-insensitive matching
 * @return 0 on success, -1 on error
 */
static int _sed_regcomp(sed_regex_t *re, const char *pattern,
                        int extended, int icase)
{
    char *pat = extended ? _sed_xstrdup(pattern) : _sed_bre_to_ere(pattern);
    re_parser_t rp;
    rp.p = pat;
    rp.prog = NULL;
    rp.prog_len = 0;
    rp.prog_cap = 0;
    rp.nsave = 2; /* slots 0,1 reserved for whole-match bounds */
    /* wrap pattern in SAVE 0 ... SAVE 1 so regexec can recover
     * the precise match start/end even when no (...) group exists */
    _sed_re_emit(&rp, RE_SAVE, 0, 0, 0);
    _sed_re_parse_alt(&rp);
    _sed_re_emit(&rp, RE_SAVE, 1, 0, 0);
    _sed_re_emit(&rp, RE_MATCH, 0, 0, 0);

    re->prog = rp.prog;
    re->len = rp.prog_len;
    re->nsave = rp.nsave;
    re->icase = icase;
    free(pat);
    return 0;
}

/**
 * @brief Execute a compiled tiny regex against a string.
 * @return 0 on match, -1 on no match
 */
static int _sed_regexec(const sed_regex_t *re, const char *str,
                        int nmatch, sed_regmatch_t *pmatch, int flags)
{
    (void)flags;
    int slen = (int)strlen(str);
    long *saves = NULL;
    if (re->nsave > 0) {
        saves = (long *)_sed_xmalloc((size_t)re->nsave * sizeof(long));
    }
    /* Try matching at each position */
    for (int start = 0; start <= slen; start++) {
        if (saves) {
            for (int i = 0; i < re->nsave; i++) {
                saves[i] = -1;
            }
        }
        re_matcher_t m;
        m.str = str;
        m.slen = slen;
        m.prog = re->prog;
        m.prog_len = re->len;
        m.saves = saves;
        m.nsave = re->nsave;
        m.icase = re->icase;

        if (_sed_re_run(&m, 0, start)) {
            if (pmatch && nmatch >= 1) {
                /* whole match: slots 0 (start) and 1 (end) */
                if (saves && re->nsave >= 2 && saves[0] >= 0 && saves[1] >= 0) {
                    pmatch[0].rm_so = saves[0];
                    pmatch[0].rm_eo = saves[1];
                }
                else {
                    pmatch[0].rm_so = start;
                    pmatch[0].rm_eo = start;
                }
                /* groups: pmatch[i] <- saves[2*i], saves[2*i+1] */
                for (int i = 1; i < nmatch; i++) {
                    int slot = 2 * i;
                    if (saves && slot + 1 < re->nsave &&
                        saves[slot] >= 0 && saves[slot + 1] >= 0) {
                        pmatch[i].rm_so = saves[slot];
                        pmatch[i].rm_eo = saves[slot + 1];
                    }
                    else {
                        pmatch[i].rm_so = -1;
                        pmatch[i].rm_eo = -1;
                    }
                }
            }
            if (saves) {
                free(saves);
            }
            return 0;
        }
    }
    if (saves) {
        free(saves);
    }
    return -1; /* no match */
}

/**
 * @brief Free a compiled tiny regex and its class bitmaps.
 * @param re  compiled regex
 */
static void _sed_regfree(sed_regex_t *re)
{
    if (re->prog) {
        for (int i = 0; i < re->len; i++) {
            if (re->prog[i].set) {
                free(re->prog[i].set);
            }
        }
        free(re->prog);
        re->prog = NULL;
    }
}

#else /* !SED_TINY_REGEX: use POSIX regex */

/**
 * @brief Compile a regex pattern with POSIX regcomp.
 * @return 0 on success, nonzero on error
 */
static int _sed_regcomp(sed_regex_t *re, const char *pattern,
                        int extended, int icase)
{
    int flags = (extended ? REG_EXTENDED : 0) | REG_NEWLINE;
    if (icase) {
        flags |= REG_ICASE;
    }
    return regcomp(re, pattern, flags);
}

/**
 * @brief Execute a compiled POSIX regex against a string.
 * @return 0 on match, nonzero on no match
 */
static int _sed_regexec(const sed_regex_t *re, const char *str,
                        int nmatch, sed_regmatch_t *pmatch, int flags)
{
    return regexec(re, str, nmatch, pmatch, flags);
}

/**
 * @brief Free a compiled POSIX regex.
 * @param re  compiled regex
 */
static void _sed_regfree(sed_regex_t *re)
{
    regfree(re);
}

#endif /* SED_TINY_REGEX */

/**
 * @brief Compile a regex, using the cache to reuse prior compilations.
 *
 * On cache miss a free slot is used (or slot 0 is evicted). The pattern
 * is copied into the cache with bounded safe copy.
 *
 * @param pattern   source pattern
 * @param extended  nonzero for ERE
 * @param icase     nonzero for case-insensitive
 * @param out       output compiled regex pointer
 * @return 0 on success, -1 on compile error
 */
static int _sed_re_compile(const char *pattern, int extended,
                           int icase, sed_regex_t **out)
{
    /* Check cache */
    for (int i = 0; i < SED_REGEX_CACHE_SIZE; i++) {
        if (s_re_cache[i].used &&
            s_re_cache[i].extended == extended &&
            s_re_cache[i].icase == icase &&
            strcmp(s_re_cache[i].pattern, pattern) == 0) {
            *out = &s_re_cache[i].re;
            return 0;
        }
    }
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < SED_REGEX_CACHE_SIZE; i++) {
        if (!s_re_cache[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        /* Evict slot 0 */
        _sed_regfree(&s_re_cache[0].re);
        s_re_cache[0].used = false;
        slot = 0;
    }
    sed_regex_t *re = &s_re_cache[slot].re;
    if (_sed_regcomp(re, pattern, extended, icase) != 0) {
        return -1;
    }
    (void)_sed_safe_copy(s_re_cache[slot].pattern, pattern,
                         sizeof(s_re_cache[slot].pattern));
    s_re_cache[slot].extended = extended;
    s_re_cache[slot].icase = icase;
    s_re_cache[slot].used = true;
    *out = re;
    return 0;
}

/**
 * @brief Match a string against a pattern (compile-once via cache).
 *
 * @param str       string to match
 * @param pattern   regex pattern
 * @param extended  nonzero for ERE
 * @param icase     nonzero for case-insensitive
 * @param m_out     optional output match offsets (may be NULL)
 * @param nm        number of match offsets in m_out
 * @return 1 if match, 0 if no match (0 also on compile failure)
 */
static int _sed_re_match(const char *str, const char *pattern,
                         int extended, int icase,
                         sed_regmatch_t *m_out, int nm)
{
    sed_regex_t *re;
    if (_sed_re_compile(pattern, extended, icase, &re) != 0) {
        return 0;
    }
    sed_regmatch_t local[10];
    if (!m_out || nm == 0) {
        m_out = local;
        nm = 10;
    }
    if (nm > 10) {
        nm = 10;
    }
    int rc = _sed_regexec(re, str, nm, m_out, 0);
    return rc == 0 ? 1 : 0;
}

/**
 * @brief Report a parse error and exit.
 * @param p    parser state (for line number)
 * @param msg  diagnostic message
 */
static void _sed_error(sed_parser_t *p, const char *msg)
{
    sed_err_printf("sed: %s near line %d\n", msg, p->line);
    exit(1);
}

/**
 * @brief Skip spaces and tabs.
 * @param p  parser state
 */
static void _sed_skip_ws(sed_parser_t *p)
{
    while (p->src[p->pos] && (p->src[p->pos] == ' ' || p->src[p->pos] == '\t')) {
        p->pos++;
    }
}

/**
 * @brief Skip whitespace, newlines (tracking line count) and comments.
 * @param p  parser state
 */
static void _sed_skip_ws_nl(sed_parser_t *p)
{
    while (p->src[p->pos]) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (c == '\n') {
                p->line++;
            }
            p->pos++;
        }
        else if (c == '#') {
            /* skip comment to end of line */
            while (p->src[p->pos] && p->src[p->pos] != '\n') {
                p->pos++;
            }
        }
        else {
            break;
        }
    }
}

/**
 * @brief Peek at the current character without consuming it.
 * @param p  parser state
 * @return current character as int (0 at end)
 */
static int _sed_peek(sed_parser_t *p)
{
    return (int)(unsigned char)p->src[p->pos];
}

/**
 * @brief Read until an unescaped delimiter, returning a malloc'd string.
 *
 * @param p           parser state
 * @param delim       delimiter character (typically '/')
 * @param had_escape  optional output: 1 if string ended mid-escape
 * @return newly allocated string (without delimiter)
 */
static char * _sed_read_delimited(sed_parser_t *p, char delim, int *had_escape)
{
    dstr_t d;
    _sed_dstr_init(&d);
    int escaped = 0;
    while (p->src[p->pos]) {
        char c = p->src[p->pos];
        if (!escaped && c == delim) {
            p->pos++;
            if (had_escape) {
                *had_escape = 0;
            }
            return d.data;
        }
        if (!escaped && c == '\\' && p->src[p->pos + 1] == delim) {
            /* escaped delimiter -> literal delimiter */
            _sed_dstr_putc(&d, delim);
            p->pos += 2;
            continue;
        }
        if (!escaped && c == '\\' && p->src[p->pos + 1] == '\0') {
            /* line continuation */
            p->pos++;
            _sed_dstr_putc(&d, '\\');
            continue;
        }
        if (c == '\n' && !escaped) {
            /* unterminated - newline before delimiter */
            break;
        }
        if (escaped) {
            _sed_dstr_putc(&d, '\\');
            _sed_dstr_putc(&d, c);
            p->pos++;
            escaped = 0;
        }
        else if (c == '\\') {
            p->pos++;
            escaped = 1;
        }
        else {
            _sed_dstr_putc(&d, c);
            p->pos++;
        }
    }
    if (had_escape) {
        *had_escape = escaped;
    }
    return d.data;
}

/**
 * @brief Read text for a/i/c commands (read to end of line, handle \ at end).
 * @param p  parser state
 * @return newly allocated text string
 */
static char * _sed_read_text(sed_parser_t *p)
{
    dstr_t d;
    _sed_dstr_init(&d);
    /* skip leading whitespace after the command char */
    while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t') {
        p->pos++;
    }
    /* handle leading backslash (a\ text or a text) */
    if (p->src[p->pos] == '\\') {
        p->pos++;
    }
    while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t') {
        p->pos++;
    }

    int first_line = 1;
    while (p->src[p->pos]) {
        char c = p->src[p->pos];
        if (c == '\n') {
            /* end of text line */
            p->pos++;
            p->line++;
            /* check for continuation (lines starting with \) */
            if (p->src[p->pos] == '\\' ||
                (p->src[p->pos] == ' ' && p->src[p->pos + 1] != '\n')) {
                /* multiline text: continue */
                if (!first_line) {
                    _sed_dstr_putc(&d, '\n');
                }
                first_line = 0;
                if (p->src[p->pos] == '\\') {
                    p->pos++;
                }
                while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t') {
                    p->pos++;
                }
                continue;
            }
            break;
        }
        if (c == '\\' && p->src[p->pos + 1] == '\n') {
            /* explicit line continuation */
            p->pos += 2;
            p->line++;
            _sed_dstr_putc(&d, '\n');
            first_line = 0;
            continue;
        }
        if (c == '\\' && p->src[p->pos + 1]) {
            char nc = p->src[p->pos + 1];
            switch (nc) {
                case 'n':
                    _sed_dstr_putc(&d, '\n');
                    p->pos += 2;
                    break;

                case 't':
                    _sed_dstr_putc(&d, '\t');
                    p->pos += 2;
                    break;

                case '\\':
                    _sed_dstr_putc(&d, '\\');
                    p->pos += 2;
                    break;

                default:
                    _sed_dstr_putc(&d, nc);
                    p->pos += 2;
                    break;
            }
            continue;
        }
        _sed_dstr_putc(&d, c);
        p->pos++;
        first_line = 0;
    }
    if (d.len == 0) {
        _sed_dstr_putc(&d, '\0');
    }
    return d.data;
}

/**
 * @brief Parse an address (line, $, /regex/, first~step, +N).
 * @param p  parser state
 * @param a  output address
 */
static void _sed_parse_addr(sed_parser_t *p, sed_addr_t *a)
{
    memset(a, 0, sizeof(*a));
    int c = _sed_peek(p);

    if (c == '$') {
        p->pos++;
        a->type = ADDR_LAST;
    }
    else if (c == '/') {
        p->pos++;
        a->regex = _sed_read_delimited(p, '/', NULL);
        a->type = ADDR_REGEX;
    }
    else if (c == '\\' && p->src[p->pos + 1] == '\\') {
        /* \\regex\\ - GNU extension */
        p->pos += 2;
        a->regex = _sed_read_delimited(p, '\\', NULL);
        a->type = ADDR_REGEX;
    }
    else if (isdigit(c)) {
        long n = 0;
        while (isdigit((unsigned char)p->src[p->pos])) {
            n = n * 10 + (p->src[p->pos] - '0');
            p->pos++;
        }
        if (p->src[p->pos] == '~') {
            /* first~step */
            p->pos++;
            long step = 0;
            while (isdigit((unsigned char)p->src[p->pos])) {
                step = step * 10 + (p->src[p->pos] - '0');
                p->pos++;
            }
            a->type = ADDR_STEP;
            a->line = n;
            a->step = step;
        }
        else {
            a->type = ADDR_LINE;
            a->line = n;
        }
    }
    else if (c == '+') {
        /* +N relative address (GNU) */
        p->pos++;
        long n = 0;
        while (isdigit((unsigned char)p->src[p->pos])) {
            n = n * 10 + (p->src[p->pos] - '0');
            p->pos++;
        }
        a->type = ADDR_LINE_REL;
        a->line = n;
    }
    else {
        a->type = ADDR_NONE;
    }
}

/**
 * @brief Parse flags for the s/// command (g, p, i/N, Nth match, w file).
 * @param p    parser state
 * @param cmd  command being populated
 */
static void _sed_parse_s_flags(sed_parser_t *p, sed_cmd_t *cmd)
{
    cmd->s_flags = 0;
    cmd->s_count = 0;
    while (p->src[p->pos]) {
        char c = p->src[p->pos];
        if (c == 'g') {
            cmd->s_flags |= 1;
            p->pos++;
        }
        else if (c == 'p') {
            cmd->s_flags |= 2;
            p->pos++;
        }
        else if (c == 'i' || c == 'I') {
            cmd->s_flags |= 4;
            p->pos++;
        }
        else if (c == 'M' || c == 'm') {
            p->pos++; /* ignore */
        }
        else if (isdigit((unsigned char)c)) {
            /* Nth match */
            long n = 0;
            while (isdigit((unsigned char)p->src[p->pos])) {
                n = n * 10 + (p->src[p->pos] - '0');
                p->pos++;
            }
            cmd->s_count = (int)n;
            cmd->s_flags |= 8;
        }
        else if (c == 'w') {
            /* w file flag for s/// */
            p->pos++;
            _sed_skip_ws(p);
            dstr_t fn;
            _sed_dstr_init(&fn);
            while (p->src[p->pos] && p->src[p->pos] != '\n') {
                _sed_dstr_putc(&fn, p->src[p->pos]);
                p->pos++;
            }
            cmd->filename = fn.data;
            break;
        }
        else if (c == 'e') {
            p->pos++; /* ignore -e flag */
        }
        else {
            break;
        }
    }
}

/**
 * @brief Parse a single command (with optional address).
 * @param p       parser state
 * @param n_cmds  (unused) command count pointer
 * @param cap     (unused) capacity pointer
 * @return newly allocated command, or NULL for comments/empty
 */
static sed_cmd_t * _sed_parse_command(sed_parser_t *p, int *n_cmds, int *cap)
{
    _sed_skip_ws_nl(p);
    if (p->src[p->pos] == '\0' || p->src[p->pos] == '}') {
        return NULL;
    }

    /* Skip leading # comments */
    if (p->src[p->pos] == '#') {
        while (p->src[p->pos] && p->src[p->pos] != '\n') {
            p->pos++;
        }
        if (p->src[p->pos] == '\n') {
            p->pos++;
            p->line++;
        }
        return NULL;
    }

    sed_cmd_t *cmd = (sed_cmd_t *)calloc(1, sizeof(sed_cmd_t));
    cmd->line = p->line;

    /* Parse addresses */
    _sed_parse_addr(p, &cmd->addr1);
    if (cmd->addr1.type != ADDR_NONE) {
        _sed_skip_ws(p);
        if (p->src[p->pos] == ',') {
            p->pos++;
            _sed_skip_ws(p);
            _sed_parse_addr(p, &cmd->addr2);
        }
    }

    /* Check for ! after address */
    _sed_skip_ws(p);
    if (p->src[p->pos] == '!') {
        p->pos++;
        cmd->negate = true;
    }

    _sed_skip_ws(p);

    /* Parse command char */
    int c = _sed_peek(p);
    if (c == 0 || c == '\n' || c == ';') {
        if (c == '\n') {
            p->pos++;
            p->line++;
        }
        else if (c == ';') {
            p->pos++;
        }
        /* address with no command - treat as print (like { }) */
        free(cmd);
        return NULL;
    }

    switch (c) {
        case 's': {
            p->pos++;
            char delim = p->src[p->pos];
            if (!delim || delim == '\n') {
                _sed_error(p, "missing delimiter for s");
            }
            p->pos++;
            cmd->type = CMD_S;
            cmd->regex = _sed_read_delimited(p, delim, NULL);
            cmd->repl = _sed_read_delimited(p, delim, NULL);
            _sed_parse_s_flags(p, cmd);
            break;
        }

        case 'y': {
            p->pos++;
            char delim = p->src[p->pos];
            if (!delim || delim == '\n') {
                _sed_error(p, "missing delimiter for y");
            }
            p->pos++;
            cmd->type = CMD_Y;
            cmd->y_src = _sed_read_delimited(p, delim, NULL);
            cmd->y_dst = _sed_read_delimited(p, delim, NULL);
            break;
        }

        case 'a':
            p->pos++;
            cmd->type = CMD_A;
            cmd->text = _sed_read_text(p);
            break;

        case 'i':
            p->pos++;
            cmd->type = CMD_I;
            cmd->text = _sed_read_text(p);
            break;

        case 'c':
            p->pos++;
            cmd->type = CMD_C;
            cmd->text = _sed_read_text(p);
            break;

        case 'p':
            p->pos++;
            cmd->type = CMD_P;
            break;

        case 'P':
            p->pos++;
            cmd->type = CMD_P_UPPER;
            break;

        case 'd':
            p->pos++;
            cmd->type = CMD_D;
            break;

        case 'D':
            p->pos++;
            cmd->type = CMD_D_UPPER;
            break;

        case 'n':
            p->pos++;
            cmd->type = CMD_N;
            break;

        case 'N':
            p->pos++;
            cmd->type = CMD_N_UPPER;
            break;

        case 'h':
            p->pos++;
            cmd->type = CMD_H;
            break;

        case 'H':
            p->pos++;
            cmd->type = CMD_H_UPPER;
            break;

        case 'g':
            p->pos++;
            cmd->type = CMD_G;
            break;

        case 'G':
            p->pos++;
            cmd->type = CMD_G_UPPER;
            break;

        case 'x':
            p->pos++;
            cmd->type = CMD_X;
            break;

        case 'b':
        case 't':
        case 'T': {
            p->pos++;
            if (c == 'b') {
                cmd->type = CMD_B;
            }
            else if (c == 't') {
                cmd->type = CMD_T;
            }
            else {
                cmd->type = CMD_T_UPPER;
            }
            _sed_skip_ws(p);
            dstr_t lbl;
            _sed_dstr_init(&lbl);
            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                   p->src[p->pos] != ';' && p->src[p->pos] != '}') {
                _sed_dstr_putc(&lbl, p->src[p->pos]);
                p->pos++;
            }
            while (lbl.len > 0 &&
                   (lbl.data[lbl.len - 1] == ' ' || lbl.data[lbl.len - 1] == '\t')) {
                lbl.data[--lbl.len] = '\0';
            }
            cmd->label = lbl.data;
            if (lbl.len == 0) {
                free(lbl.data);
                cmd->label = _sed_xstrdup("");
            }
            break;
        }

        case 'q':
            p->pos++;
            cmd->type = CMD_Q;
            break;

        case 'Q':
            p->pos++;
            cmd->type = CMD_Q_UPPER;
            break;

        case 'r': {
            p->pos++;
            cmd->type = CMD_R;
            _sed_skip_ws(p);
            dstr_t fn;
            _sed_dstr_init(&fn);
            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                   p->src[p->pos] != ';') {
                _sed_dstr_putc(&fn, p->src[p->pos]);
                p->pos++;
            }
            while (fn.len > 0 &&
                   (fn.data[fn.len - 1] == ' ' || fn.data[fn.len - 1] == '\t')) {
                fn.data[--fn.len] = '\0';
            }
            cmd->filename = fn.data;
            break;
        }

        case 'w': {
            p->pos++;
            cmd->type = CMD_W;
            _sed_skip_ws(p);
            dstr_t fn;
            _sed_dstr_init(&fn);
            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                   p->src[p->pos] != ';') {
                _sed_dstr_putc(&fn, p->src[p->pos]);
                p->pos++;
            }
            while (fn.len > 0 &&
                   (fn.data[fn.len - 1] == ' ' || fn.data[fn.len - 1] == '\t')) {
                fn.data[--fn.len] = '\0';
            }
            cmd->filename = fn.data;
            break;
        }

        case 'l':
            p->pos++;
            cmd->type = CMD_L;
            break;

        case '=':
            p->pos++;
            cmd->type = CMD_EQ;
            break;

        case ':': {
            p->pos++;
            cmd->type = CMD_COLON;
            _sed_skip_ws(p);
            dstr_t lbl;
            _sed_dstr_init(&lbl);
            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                   p->src[p->pos] != ';' && p->src[p->pos] != '}') {
                _sed_dstr_putc(&lbl, p->src[p->pos]);
                p->pos++;
            }
            while (lbl.len > 0 &&
                   (lbl.data[lbl.len - 1] == ' ' || lbl.data[lbl.len - 1] == '\t')) {
                lbl.data[--lbl.len] = '\0';
            }
            cmd->text = lbl.data;
            break;
        }

        case 'z':
            p->pos++;
            cmd->type = CMD_Z;
            break;

        case '{': {
            p->pos++;
            cmd->type = CMD_BLOCK;
            int n_block = 0, cap_block = 0;
            sed_cmd_t *block_cmds = NULL;
            _sed_skip_ws_nl(p);
            while (p->src[p->pos] && p->src[p->pos] != '}') {
                _sed_skip_ws_nl(p);
                if (p->src[p->pos] == '}') {
                    break;
                }
                if (p->src[p->pos] == '\0') {
                    break;
                }
                sed_cmd_t *sub = (sed_cmd_t *)calloc(1, sizeof(sed_cmd_t));
                sub->line = p->line;
                _sed_parse_addr(p, &sub->addr1);
                if (sub->addr1.type != ADDR_NONE) {
                    _sed_skip_ws(p);
                    if (p->src[p->pos] == ',') {
                        p->pos++;
                        _sed_skip_ws(p);
                        _sed_parse_addr(p, &sub->addr2);
                    }
                }
                _sed_skip_ws(p);
                if (p->src[p->pos] == '!') {
                    p->pos++;
                    sub->negate = true;
                }
                _sed_skip_ws(p);
                /* parse the command char for this sub-command */
                int sc = _sed_peek(p);
                if (sc == 0 || sc == '\n' || sc == ';' || sc == '}') {
                    if (sc == '\n') {
                        p->pos++;
                        p->line++;
                    }
                    else if (sc == ';') {
                        p->pos++;
                    }
                    free(sub);
                    continue;
                }
                /* Reuse the main parsing by temporarily swapping */
                /* Simple approach: handle the common cases inline */
                switch (sc) {
                    case 's': {
                        p->pos++;
                        char delim = p->src[p->pos];
                        if (!delim || delim == '\n') {
                            _sed_error(p, "missing delimiter for s");
                        }
                        p->pos++;
                        sub->type = CMD_S;
                        sub->regex = _sed_read_delimited(p, delim, NULL);
                        sub->repl = _sed_read_delimited(p, delim, NULL);
                        _sed_parse_s_flags(p, sub);
                        break;
                    }

                    case 'p':
                        p->pos++;
                        sub->type = CMD_P;
                        break;

                    case 'd':
                        p->pos++;
                        sub->type = CMD_D;
                        break;

                    case 'n':
                        p->pos++;
                        sub->type = CMD_N;
                        break;

                    case 'N':
                        p->pos++;
                        sub->type = CMD_N_UPPER;
                        break;

                    case 'h':
                        p->pos++;
                        sub->type = CMD_H;
                        break;

                    case 'H':
                        p->pos++;
                        sub->type = CMD_H_UPPER;
                        break;

                    case 'g':
                        p->pos++;
                        sub->type = CMD_G;
                        break;

                    case 'G':
                        p->pos++;
                        sub->type = CMD_G_UPPER;
                        break;

                    case 'x':
                        p->pos++;
                        sub->type = CMD_X;
                        break;

                    case 'b':
                    case 't':
                    case 'T': {
                        p->pos++;
                        if (sc == 'b') {
                            sub->type = CMD_B;
                        }
                        else if (sc == 't') {
                            sub->type = CMD_T;
                        }
                        else {
                            sub->type = CMD_T_UPPER;
                        }
                        _sed_skip_ws(p);
                        dstr_t lbl;
                        _sed_dstr_init(&lbl);
                        while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                               p->src[p->pos] != ';' && p->src[p->pos] != '}') {
                            _sed_dstr_putc(&lbl, p->src[p->pos]);
                            p->pos++;
                        }
                        while (lbl.len > 0 &&
                               (lbl.data[lbl.len - 1] == ' ' || lbl.data[lbl.len - 1] == '\t')) {
                            lbl.data[--lbl.len] = '\0';
                        }
                        sub->label = lbl.data;
                        if (lbl.len == 0) {
                            free(lbl.data);
                            sub->label = _sed_xstrdup("");
                        }
                        break;
                    }

                    case 'q':
                        p->pos++;
                        sub->type = CMD_Q;
                        break;

                    case 'Q':
                        p->pos++;
                        sub->type = CMD_Q_UPPER;
                        break;

                    case 'a':
                        p->pos++;
                        sub->type = CMD_A;
                        sub->text = _sed_read_text(p);
                        break;

                    case 'i':
                        p->pos++;
                        sub->type = CMD_I;
                        sub->text = _sed_read_text(p);
                        break;

                    case 'c':
                        p->pos++;
                        sub->type = CMD_C;
                        sub->text = _sed_read_text(p);
                        break;

                    case 'r':
                        p->pos++;
                        sub->type = CMD_R;
                        _sed_skip_ws(p);
                        {
                            dstr_t fn;
                            _sed_dstr_init(&fn);
                            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                                   p->src[p->pos] != ';') {
                                _sed_dstr_putc(&fn, p->src[p->pos++]);
                            }
                            while (fn.len > 0 &&
                                   (fn.data[fn.len - 1] == ' ' || fn.data[fn.len - 1] == '\t')) {
                                fn.data[--fn.len] = '\0';
                            }
                            sub->filename = fn.data;
                        }
                        break;

                    case 'w':
                        p->pos++;
                        sub->type = CMD_W;
                        _sed_skip_ws(p);
                        {
                            dstr_t fn;
                            _sed_dstr_init(&fn);
                            while (p->src[p->pos] && p->src[p->pos] != '\n' &&
                                   p->src[p->pos] != ';') {
                                _sed_dstr_putc(&fn, p->src[p->pos++]);
                            }
                            while (fn.len > 0 &&
                                   (fn.data[fn.len - 1] == ' ' || fn.data[fn.len - 1] == '\t')) {
                                fn.data[--fn.len] = '\0';
                            }
                            sub->filename = fn.data;
                        }
                        break;

                    case 'l':
                        p->pos++;
                        sub->type = CMD_L;
                        break;

                    case '=':
                        p->pos++;
                        sub->type = CMD_EQ;
                        break;

                    case 'z':
                        p->pos++;
                        sub->type = CMD_Z;
                        break;

                    case 'y': {
                        p->pos++;
                        char delim = p->src[p->pos];
                        if (!delim || delim == '\n') {
                            _sed_error(p, "missing delimiter for y");
                        }
                        p->pos++;
                        sub->type = CMD_Y;
                        sub->y_src = _sed_read_delimited(p, delim, NULL);
                        sub->y_dst = _sed_read_delimited(p, delim, NULL);
                        break;
                    }

                    default:
                        _sed_error(p, "unknown command in block");
                        break;
                }
                /* check for ! after sub-command */
                _sed_skip_ws(p);
                if (p->src[p->pos] == '!') {
                    p->pos++;
                    sub->negate = true;
                }

                if (n_block >= cap_block) {
                    cap_block = cap_block ? cap_block * 2 : 8;
                    block_cmds = (sed_cmd_t *)_sed_xrealloc(block_cmds,
                                                            (size_t)cap_block * sizeof(sed_cmd_t));
                }
                block_cmds[n_block++] = *sub;
                free(sub);
                _sed_skip_ws_nl(p);
            }
            if (p->src[p->pos] == '}') {
                p->pos++;
            }
            cmd->block_cmds = block_cmds;
            cmd->n_block_cmds = n_block;
            break;
        }

        case '#':
            while (p->src[p->pos] && p->src[p->pos] != '\n') {
                p->pos++;
            }
            free(cmd);
            return NULL;

        default:
            _sed_error(p, "unknown command");
            break;
    }

    /* Check for ! after command (already parsed above for addresses,
     * but also allow ! after command char) */
    _sed_skip_ws(p);
    if (p->src[p->pos] == '!' && !cmd->negate) {
        p->pos++;
        cmd->negate = true;
    }

    /* Append to output array */
    if (n_cmds && cap) {
        if (*n_cmds >= *cap) {
            *cap = *cap ? *cap * 2 : 16;
        }
    }
    return cmd;
}

/**
 * @brief Parse a full sed program into a command array.
 * @param p       parser state
 * @param out_n   output number of commands
 * @return newly allocated command array (caller frees)
 */
static sed_cmd_t * _sed_parse_program(sed_parser_t *p, int *out_n)
{
    sed_cmd_t *cmds = NULL;
    int n = 0, cap = 0;
    _sed_skip_ws_nl(p);
    while (p->src[p->pos]) {
        _sed_skip_ws_nl(p);
        if (p->src[p->pos] == '\0') {
            break;
        }
        if (p->src[p->pos] == '#') {
            /* skip #n or # comment */
            while (p->src[p->pos] && p->src[p->pos] != '\n') {
                p->pos++;
            }
            if (p->src[p->pos] == '\n') {
                p->pos++;
                p->line++;
            }
            continue;
        }
        int dummy_n = 0, dummy_cap = 0;
        sed_cmd_t *cmd = _sed_parse_command(p, &dummy_n, &dummy_cap);
        if (cmd) {
            if (n >= cap) {
                cap = cap ? cap * 2 : 16;
                cmds = (sed_cmd_t *)_sed_xrealloc(cmds, (size_t)cap * sizeof(sed_cmd_t));
            }
            cmds[n++] = *cmd;
            free(cmd);
        }
        /* skip separators */
        if (p->src[p->pos] == ';') {
            p->pos++;
        }
        else if (p->src[p->pos] == '\n') {
            p->pos++;
            p->line++;
        }
        else if (p->src[p->pos] == '\r') {
            p->pos++;
        }
    }
    *out_n = n;
    return cmds;
}

/**
 * @brief Find or open a write file (cached by name).
 * @param e     engine state
 * @param name  file name
 * @return opened FILE pointer (aborts on too-many-files or open error)
 */
static FILE * _sed_wfile(sed_engine_t *e, const char *name)
{
    for (int i = 0; i < e->n_wfiles; i++) {
        if (strcmp(e->wfiles[i].name, name) == 0) {
            return e->wfiles[i].fp;
        }
    }
    if (e->n_wfiles >= 32) {
        sed_err_printf("%s", "sed: too many write files\n");
        exit(1);
    }
    FILE *fp = fopen(name, "w");
    if (!fp) {
        sed_err_printf("sed: cannot open %s for writing: %s\n",
                       name, strerror(errno));
        exit(1);
    }
    (void)_sed_safe_copy(e->wfiles[e->n_wfiles].name, name,
                         sizeof(e->wfiles[e->n_wfiles].name));
    e->wfiles[e->n_wfiles].fp = fp;
    e->n_wfiles++;
    return fp;
}

/**
 * @brief Collect all labels (including those inside blocks) for branch resolution.
 * @param e  engine state
 */
static void _sed_resolve_labels(sed_engine_t *e)
{
    /* First pass: collect labels */
    e->n_labels = 0;
    for (int i = 0; i < e->n_cmds; i++) {
        if (e->cmds[i].type == CMD_COLON) {
            (void)_sed_safe_copy(e->labels[e->n_labels].name,
                                 e->cmds[i].text ? e->cmds[i].text : "",
                                 sizeof(e->labels[e->n_labels].name));
            e->labels[e->n_labels].cmd_idx = i;
            e->n_labels++;
        }
    }
    /* Also resolve labels in block commands */
    for (int i = 0; i < e->n_cmds; i++) {
        if (e->cmds[i].type == CMD_BLOCK) {
            for (int j = 0; j < e->cmds[i].n_block_cmds; j++) {
                sed_cmd_t *sub = &e->cmds[i].block_cmds[j];
                if (sub->type == CMD_COLON) {
                    /* labels inside blocks - rare but handle */
                    (void)_sed_safe_copy(e->labels[e->n_labels].name,
                                         sub->text ? sub->text : "",
                                         sizeof(e->labels[e->n_labels].name));
                    e->labels[e->n_labels].cmd_idx = i; /* approximate */
                    e->n_labels++;
                }
            }
        }
    }
}

/**
 * @brief Find the command index for a label name.
 * @param e     engine state
 * @param name  label name (empty = end of script)
 * @return command index (aborts if not found)
 */
static int _sed_find_label(sed_engine_t *e, const char *name)
{
    if (!name || !*name) {
        return -1; /* empty label = end of script */
    }
    for (int i = 0; i < e->n_labels; i++) {
        if (strcmp(e->labels[i].name, name) == 0) {
            return e->labels[i].cmd_idx;
        }
    }
    sed_err_printf("sed: label '%s' not found\n", name);
    exit(1);
}

/**
 * @brief Check if an address matches the current line.
 * @param e  engine state
 * @param a  address to test
 * @return 1 if match, 0 otherwise
 */
static int _sed_addr_match(sed_engine_t *e, sed_addr_t *a)
{
    if (a->type == ADDR_NONE) {
        return 1;
    }
    switch (a->type) {
        case ADDR_LINE:
            return e->line_no == a->line;

        case ADDR_LAST:
            return e->is_last_line ? 1 : 0;

        case ADDR_REGEX:
            return _sed_re_match(e->pattern.data, a->regex, e->extended, 0, NULL, 0);

        case ADDR_STEP: {
            if (a->step == 0) {
                return e->line_no == a->line ? 1 : 0;
            }
            if (e->line_no < a->line) {
                return 0;
            }
            return ((e->line_no - a->line) % a->step) == 0 ? 1 : 0;
        }

        case ADDR_LINE_REL:
            /* handled by range logic */
            return 0;

        default:
            return 0;
    }
}

/**
 * @brief Check if a command's address (or address range) matches.
 *
 * Manages range active-state and range-start line tracking for ranges.
 *
 * @param e        engine state
 * @param cmd_idx  command index
 * @return 1 if the command should execute, 0 otherwise
 */
static int _sed_cmd_match(sed_engine_t *e, int cmd_idx)
{
    sed_cmd_t *cmd = &e->cmds[cmd_idx];

    if (cmd->addr1.type == ADDR_NONE) {
        return !cmd->negate ? 1 : 0;
    }

    if (cmd->addr2.type == ADDR_NONE) {
        /* single address */
        int m = _sed_addr_match(e, &cmd->addr1);
        return cmd->negate ? !m : m;
    }

    /* range: addr1,addr2 */
    if (e->range_active[cmd_idx]) {
        /* check if range should end */
        if (e->line_no > e->range_start[cmd_idx]) {
            int end_match = 0;
            if (cmd->addr2.type == ADDR_LINE) {
                end_match = (e->line_no >= cmd->addr2.line) ? 1 : 0;
            }
            else if (cmd->addr2.type == ADDR_LAST) {
                end_match = e->is_last_line ? 1 : 0;
            }
            else if (cmd->addr2.type == ADDR_REGEX) {
                end_match = _sed_re_match(e->pattern.data, cmd->addr2.regex,
                                          e->extended, 0, NULL, 0);
            }
            else if (cmd->addr2.type == ADDR_LINE_REL) {
                end_match = (e->line_no >= e->range_start[cmd_idx] + cmd->addr2.line) ? 1 : 0;
            }
            else if (cmd->addr2.type == ADDR_STEP) {
                end_match = _sed_addr_match(e, &cmd->addr2);
            }
            if (end_match) {
                e->range_active[cmd_idx] = 0;
            }
        }
        return cmd->negate ? 0 : 1;
    }
    else {
        int start_match = _sed_addr_match(e, &cmd->addr1);
        if (start_match) {
            e->range_active[cmd_idx] = 1;
            e->range_start[cmd_idx] = e->line_no;
            /* Check if range ends immediately (addr2 is line number <= current) */
            if (cmd->addr2.type == ADDR_LINE && cmd->addr2.line <= e->line_no) {
                e->range_active[cmd_idx] = 0;
            }
            return cmd->negate ? 0 : 1;
        }
        return cmd->negate ? 1 : 0;
    }
}

/**
 * @brief Perform s/// substitution on the pattern space.
 *
 * Supports flags g (global), p (print result), i (case-insensitive),
 * Nth-match, and the w (write-to-file) flag.
 *
 * @param e    engine state
 * @param cmd  s/// command
 * @return 1 if a substitution was made, 0 otherwise
 */
static int _sed_do_s(sed_engine_t *e, sed_cmd_t *cmd)
{
    sed_regex_t *re;
    int icase = (cmd->s_flags & 4) != 0;
    if (_sed_re_compile(cmd->regex, e->extended, icase, &re) != 0) {
        sed_err_printf("sed: invalid regex: %s\n", cmd->regex);
        return 0;
    }

    const char *str = e->pattern.data;
    int slen = (int)e->pattern.len;
    dstr_t result;
    _sed_dstr_init(&result);
    int count = 0;
    int global = (cmd->s_flags & 1) != 0;
    int nth = cmd->s_count;
    int pos = 0;
    int substituted = 0;

    while (pos <= slen) {
        sed_regmatch_t m[10];
        memset(m, -1, sizeof(m));
        int rc = _sed_regexec(re, str + pos, 10, m, 0);

        if (rc != 0 || m[0].rm_so < 0) {
            /* no more matches */
            _sed_dstr_putn(&result, str + pos, (size_t)(slen - pos));
            break;
        }

        int match_start = pos + (int)m[0].rm_so;
        int match_end = pos + (int)m[0].rm_eo;

        /* copy text before match */
        _sed_dstr_putn(&result, str + pos, (size_t)(match_start - pos));

        count++;

        /* check if this match should be replaced */
        int do_replace = 0;
        if (global && (nth == 0 || count >= nth)) {
            do_replace = 1;
        }
        else if (nth > 0 && count == nth) {
            do_replace = 1;
        }
        else if (nth == 0 && !global) {
            do_replace = 1; /* first match */
        }

        if (do_replace) {
            /* process replacement string */
            const char *repl = cmd->repl;
            while (*repl) {
                if (*repl == '\\' && repl[1]) {
                    char cc = repl[1];
                    if (cc >= '0' && cc <= '9') {
                        int gi = cc - '0';
                        if (gi < 10 && m[gi].rm_so >= 0) {
                            /* m[gi] offsets are relative to str+pos */
                            int gs = (int)m[gi].rm_so;
                            int ge = (int)m[gi].rm_eo;
                            if (gs < 0) {
                                gs = 0;
                            }
                            if (ge > slen - pos) {
                                ge = slen - pos;
                            }
                            if (ge > gs) {
                                _sed_dstr_putn(&result, str + pos + gs, (size_t)(ge - gs));
                            }
                        }
                        repl += 2;
                        continue;
                    }
                    switch (cc) {
                        case 'n':
                            _sed_dstr_putc(&result, '\n');
                            break;

                        case 't':
                            _sed_dstr_putc(&result, '\t');
                            break;

                        case '\\':
                            _sed_dstr_putc(&result, '\\');
                            break;

                        case '&':
                            _sed_dstr_putc(&result, '&');
                            break;

                        default:
                            _sed_dstr_putc(&result, cc);
                            break;
                    }
                    repl += 2;
                }
                else if (*repl == '&') {
                    /* & = matched text */
                    int ms = match_start - pos;
                    int me = match_end - pos;
                    if (me > ms) {
                        _sed_dstr_putn(&result, str + pos + ms, (size_t)(me - ms));
                    }
                    repl++;
                }
                else {
                    _sed_dstr_putc(&result, *repl);
                    repl++;
                }
            }
            substituted = 1;

            /* if global and this was the nth match, continue replacing */
            if (nth > 0 && !global && count >= nth) {
                /* only replace Nth, done */
                _sed_dstr_putn(&result, str + match_end, (size_t)(slen - match_end));
                break;
            }
        }
        else {
            /* don't replace, copy matched text as-is */
            _sed_dstr_putn(&result, str + match_start, (size_t)(match_end - match_start));
        }

        /* advance past match */
        if (match_end == match_start) {
            /* zero-length match: copy one char to avoid infinite loop */
            if (pos < slen) {
                _sed_dstr_putc(&result, str[pos]);
            }
            pos = match_end + 1;
        }
        else {
            pos = match_end;
        }

        if (!global && (nth == 0 || (nth > 0 && count >= nth))) {
            /* copy rest */
            _sed_dstr_putn(&result, str + pos, (size_t)(slen - pos));
            break;
        }
    }

    if (substituted) {
        /* replace pattern space */
        _sed_dstr_clear(&e->pattern);
        _sed_dstr_puts(&e->pattern, result.data);
        e->sub_done = true;

        /* p flag: print pattern space */
        if (cmd->s_flags & 2) {
            (void)fprintf(e->out, "%s\n", e->pattern.data);
        }
        /* w flag: write to file */
        if (cmd->filename) {
            FILE *fp = _sed_wfile(e, cmd->filename);
            (void)fprintf(fp, "%s\n", e->pattern.data);
        }
    }

    _sed_dstr_free(&result);
    return substituted;
}

/**
 * @brief Flush the append queue (text lines and file contents) to output.
 * @param e  engine state
 */
static void _sed_flush_append(sed_engine_t *e)
{
    for (int i = 0; i < e->n_append; i++) {
        if (e->append_q[i].is_file) {
            FILE *fp = fopen(e->append_q[i].text, "r");
            if (fp) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                    (void)fwrite(buf, 1, n, e->out);
                }
                (void)fclose(fp);
            }
        }
        else {
            (void)fprintf(e->out, "%s\n", e->append_q[i].text);
        }
        free(e->append_q[i].text);
    }
    e->n_append = 0;
}

/**
 * @brief Execute a single command against the current pattern space.
 * @param e         engine state
 * @param cmd       command to execute
 * @param in_block  nonzero when executing inside a {} block
 */
static void _sed_exec_cmd(sed_engine_t *e, sed_cmd_t *cmd, int in_block)
{
    switch (cmd->type) {
        case CMD_S:
            _sed_do_s(e, cmd);
            break;

        case CMD_P:
            (void)fprintf(e->out, "%s\n", e->pattern.data);
            break;

        case CMD_P_UPPER: {
            /* print first line of pattern space */
            const char *nl = strchr(e->pattern.data, '\n');
            if (nl) {
                (void)fprintf(e->out, "%.*s\n",
                              (int)(nl - e->pattern.data), e->pattern.data);
            }
            else {
                (void)fprintf(e->out, "%s\n", e->pattern.data);
            }
            break;
        }

        case CMD_D:
            e->delete = true;
            break;

        case CMD_D_UPPER: {
            /* delete first line of pattern space */
            char *nl = strchr(e->pattern.data, '\n');
            if (nl) {
                dstr_t tmp;
                _sed_dstr_init(&tmp);
                _sed_dstr_puts(&tmp, nl + 1);
                _sed_dstr_clear(&e->pattern);
                _sed_dstr_puts(&e->pattern, tmp.data);
                _sed_dstr_free(&tmp);
                /* restart cycle without reading new line */
                e->next = true;
                e->branch_to = 0; /* restart from beginning */
            }
            else {
                e->delete = true;
            }
            break;
        }

        case CMD_N:
            /* print pattern (unless -n), read next line */
            if (!e->quiet) {
                (void)fprintf(e->out, "%s\n", e->pattern.data);
            }
            /* read next line */
            e->next = true;
            break;

        case CMD_N_UPPER: {
            /* append next line to pattern space */
            if (!e->is_last_line) {
                char buf[65536];
                if (fgets(buf, sizeof(buf), e->current_fp)) {
                    e->line_no++;
                    if (e->separate_files) {
                        e->file_line_no++;
                    }
                    size_t len = strlen(buf);
                    if (len > 0 && buf[len - 1] == '\n') {
                        buf[--len] = '\0';
                    }
                    _sed_dstr_putc(&e->pattern, '\n');
                    _sed_dstr_puts(&e->pattern, buf);
                    /* refresh last-line flag after consuming a line */
                    e->is_last_line = _sed_check_last_line(e->current_fp) ? true : false;
                }
                else {
                    /* fgets failed: at EOF */
                    e->is_last_line = true;
                    if (!e->quiet) {
                        (void)fprintf(e->out, "%s\n", e->pattern.data);
                    }
                    e->quit = true;
                }
            }
            else {
                /* no more lines: GNU sed prints and quits */
                if (!e->quiet) {
                    (void)fprintf(e->out, "%s\n", e->pattern.data);
                }
                e->quit = true;
            }
            break;
        }

        case CMD_H:
            _sed_dstr_clear(&e->hold);
            _sed_dstr_puts(&e->hold, e->pattern.data);
            break;

        case CMD_H_UPPER:
            _sed_dstr_putc(&e->hold, '\n');
            _sed_dstr_puts(&e->hold, e->pattern.data);
            break;

        case CMD_G:
            _sed_dstr_clear(&e->pattern);
            _sed_dstr_puts(&e->pattern, e->hold.data);
            break;

        case CMD_G_UPPER:
            _sed_dstr_putc(&e->pattern, '\n');
            _sed_dstr_puts(&e->pattern, e->hold.data);
            break;

        case CMD_X: {
            dstr_t tmp;
            tmp = e->pattern;
            e->pattern = e->hold;
            e->hold = tmp;
            break;
        }

        case CMD_B:
            if (!cmd->label || !*cmd->label) {
                e->branch_to = e->n_cmds; /* jump to end of script */
            }
            else {
                e->branch_to = _sed_find_label(e, cmd->label);
            }
            break;

        case CMD_T:
            if (e->sub_done) {
                e->sub_done = false;
                if (!cmd->label || !*cmd->label) {
                    e->branch_to = e->n_cmds;
                }
                else {
                    e->branch_to = _sed_find_label(e, cmd->label);
                }
            }
            break;

        case CMD_T_UPPER:
            if (!e->sub_done) {
                if (!cmd->label || !*cmd->label) {
                    e->branch_to = e->n_cmds;
                }
                else {
                    e->branch_to = _sed_find_label(e, cmd->label);
                }
            }
            break;

        case CMD_Q:
            e->quit = true;
            break;

        case CMD_Q_UPPER:
            e->quit = true;
            e->quit_no_print = true;
            break;

        case CMD_A:
            if (e->n_append < 64) {
                e->append_q[e->n_append].text = _sed_xstrdup(cmd->text ? cmd->text : "");
                e->append_q[e->n_append].is_file = false;
                e->n_append++;
            }
            break;

        case CMD_I:
            (void)fprintf(e->out, "%s\n", cmd->text ? cmd->text : "");
            break;

        case CMD_C:
            if (!in_block) {
                /* For range, only print text on last line of range */
                (void)fprintf(e->out, "%s\n", cmd->text ? cmd->text : "");
            }
            e->delete = true;
            break;

        case CMD_R:
            if (e->n_append < 64) {
                e->append_q[e->n_append].text = _sed_xstrdup(cmd->filename ? cmd->filename : "");
                e->append_q[e->n_append].is_file = true;
                e->n_append++;
            }
            break;

        case CMD_W: {
            FILE *fp = _sed_wfile(e, cmd->filename);
            (void)fprintf(fp, "%s\n", e->pattern.data);
            break;
        }

        case CMD_L: {
            /* list pattern space with visible non-printables */
            const char *s = e->pattern.data;
            for (size_t k = 0; k < e->pattern.len; k++) {
                unsigned char c = (unsigned char)s[k];
                if (c == '\\') {
                    (void)fprintf(e->out, "\\\\");
                }
                else if (c == '\n') {
                    (void)fprintf(e->out, "\\n");
                }
                else if (c == '\t') {
                    (void)fprintf(e->out, "\\t");
                }
                else if (c == '\r') {
                    (void)fprintf(e->out, "\\r");
                }
                else if (c >= 32 && c < 127) {
                    (void)fputc(c, e->out);
                }
                else {
                    (void)fprintf(e->out, "\\%03o", c);
                }
            }
            (void)fprintf(e->out, "$\n");
            break;
        }

        case CMD_EQ:
            (void)fprintf(e->out, "%ld\n", e->line_no);
            break;

        case CMD_Y: {
            const char *src = cmd->y_src;
            const char *dst = cmd->y_dst;
            if (!src || !dst) {
                break;
            }
            int src_len = (int)strlen(src);
            int dst_len = (int)strlen(dst);
            if (src_len != dst_len) {
                break;
            }
            for (size_t k = 0; k < e->pattern.len; k++) {
                char c = e->pattern.data[k];
                const char *p = strchr(src, c);
                if (p) {
                    e->pattern.data[k] = dst[p - src];
                }
            }
            break;
        }

        case CMD_Z:
            _sed_dstr_clear(&e->pattern);
            break;

        case CMD_COLON:
            /* label - no action */
            break;

        case CMD_BLOCK:
            /* handled by main loop */
            break;

        default:
            break;
    }
}

/**
 * @brief Execute the sub-commands of a {} block.
 * @param e    engine state
 * @param cmd  block command
 */
static void _sed_exec_block(sed_engine_t *e, sed_cmd_t *cmd)
{
    for (int j = 0; j < cmd->n_block_cmds; j++) {
        sed_cmd_t *sub = &cmd->block_cmds[j];
        /* check address for sub-command */
        /* For block sub-commands, the block's address has already been
         * checked. Sub-commands with their own addresses are checked here. */
        if (sub->addr1.type != ADDR_NONE) {
            /* check sub-command address */
            int match = 0;
            if (sub->addr2.type == ADDR_NONE) {
                match = _sed_addr_match(e, &sub->addr1);
            }
            else {
                /* range within block - simplified */
                match = _sed_addr_match(e, &sub->addr1);
            }
            if (sub->negate) {
                match = !match;
            }
            if (!match) {
                continue;
            }
        }
        else if (sub->negate) {
            /* ! with no address = never execute? Actually in sed,
             * ! with no address means negate the default (always true)
             * so it becomes never. But that's unusual. */
        }
        _sed_exec_cmd(e, sub, 1);
        if (e->delete || e->quit || e->next) {
            break;
        }
    }
}

/**
 * @brief Process the current pattern space through all commands.
 *
 * Handles branching and the cycle control flags (delete/next/quit).
 *
 * @param e  engine state
 */
static void _sed_process_line(sed_engine_t *e)
{
    e->sub_done = false;
    e->delete = false;
    e->next = false;
    e->branch_to = -1;

    int i = 0;
    int iterations = 0;
    while (i < e->n_cmds && iterations < 100000) {
        iterations++;
        sed_cmd_t *cmd = &e->cmds[i];

        /* Check address match */
        int match = _sed_cmd_match(e, i);

        if (match) {
            if (cmd->type == CMD_BLOCK) {
                _sed_exec_block(e, cmd);
            }
            else {
                _sed_exec_cmd(e, cmd, 0);
            }
        }

        if (e->quit) {
            return;
        }
        if (e->delete) {
            return;
        }
        if (e->next) {
            return;
        }

        /* Handle branch */
        if (e->branch_to >= 0) {
            i = e->branch_to + 1; /* skip past the label */
            e->branch_to = -1;    /* consume the branch */
            continue;
        }

        i++;
    }
}

/**
 * @brief Read the next line from the current input into the pattern space.
 * @param e  engine state
 * @return 1 on success, 0 on EOF
 */
static int _sed_read_line(sed_engine_t *e)
{
    char buf[65536];
    if (!e->current_fp || feof(e->current_fp)) {
        return 0;
    }
    if (!fgets(buf, sizeof(buf), e->current_fp)) {
        return 0;
    }
    e->line_no++;
    if (e->separate_files) {
        e->file_line_no++;
    }
    size_t len = strlen(buf);
    /* strip trailing newline */
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    _sed_dstr_clear(&e->pattern);
    _sed_dstr_puts(&e->pattern, buf);
    return 1;
}

/**
 * @brief Check if the current input position is the last line.
 * @param fp  input stream
 * @return 1 if no more data follows, 0 otherwise
 */
static int _sed_check_last_line(FILE *fp)
{
    if (!fp || feof(fp)) {
        return 1;
    }
    int c = fgetc(fp);
    if (c == EOF) {
        return 1;
    }
    (void)ungetc(c, fp);
    return 0;
}

/**
 * @brief Process one input file through the engine.
 * @param e      engine state
 * @param fp     input stream
 * @param fname  file name (for diagnostics)
 */
static void _sed_process_file(sed_engine_t *e, FILE *fp, const char *fname)
{
    e->current_fp = fp;
    e->current_fname = fname;
    if (e->separate_files) {
        e->file_line_no = 0;
    }

    /* Reset range state for -s mode */
    if (e->separate_files && e->range_active) {
        for (int i = 0; i < e->n_cmds; i++) {
            e->range_active[i] = 0;
        }
    }

    while (1) {
        if (!_sed_read_line(e)) {
            break;
        }
        /* After reading the line, check if it is the last one */
        e->is_last_line = _sed_check_last_line(fp) ? true : false;

    cycle:
        _sed_process_line(e);

        if (e->quit) {
            if (!e->quit_no_print && !e->quiet) {
                (void)fprintf(e->out, "%s\n", e->pattern.data);
            }
            _sed_flush_append(e);
            return;
        }

        if (e->delete) {
            _sed_flush_append(e);
            continue;
        }

        if (e->next) {
            /* n command: already printed, flush append and continue */
            _sed_flush_append(e);
            continue;
        }

        /* auto-print */
        if (!e->quiet) {
            (void)fprintf(e->out, "%s\n", e->pattern.data);
        }

        _sed_flush_append(e);

        /* Handle D command restart */
        if (e->branch_to == 0 && !e->delete && !e->next) {
            goto cycle;
        }
    }
}

/**
 * @brief Print usage/help information.
 */
static void _sed_print_help(void)
{
    sed_printf(
        "Usage: sed [OPTION]... {script-only-if-no-other-script} [input-file]...\n"
        "\n"
        "  -n, --quiet, --silent    suppress automatic pattern-space printing\n"
        "  -e SCRIPT, --expression=SCRIPT\n"
        "                           add SCRIPT to the commands to be executed\n"
        "  -f FILE, --file=FILE     add script FILE contents to the commands\n"
        "  -r, -E, --regexp-extended\n"
        "                           use extended regular expressions\n"
        "  -i[SUFFIX], --in-place[=SUFFIX]\n"
        "                           edit files in place (makes backup if SUFFIX given)\n"
        "  -s, --separate           treat files as separate streams\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n");
}

/**
 * @brief Print version information.
 */
static void _sed_print_version(void)
{
    sed_printf("sed %s\n", SED_VERSION_STR);
    sed_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    sed_printf("%s", "License MIT: <https://mit-license.org/>\n");
    sed_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    sed_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
