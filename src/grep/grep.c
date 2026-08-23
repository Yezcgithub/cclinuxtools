/**
 * @file grep.c
 * @brief Cross-platform implementation of the Linux grep command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU grep(1).
 *
 * Key behaviors:
 *   - Pattern syntax: BRE (-G, default), ERE (-E), fixed strings (-F)
 *   - -P accepted (mapped onto the ERE engine; PCRE-only extensions unsupported)
 *   - Bundled byte-oriented backtracking regex engine used on ALL platforms:
 *     . [] [^...] [:class:] ranges, *, +, ?, {m,n}, ^ $ anchors,
 *     groups, alternation, backreferences \1..\9, \< \> \b \B \w \W \s \S
 *   - Matching: -e/-f multiple patterns, -i/-y ignore case, -v invert,
 *     -w whole word, -x whole line
 *   - Output: -n line numbers, -b byte offsets, -c count, -o only-matching,
 *     -l/-L file lists, -q quiet, -H/-h filename control, --color[=WHEN]
 *   - Context: -A/-B/-C NUM with "--" group separators,
 *     --group-separator / --no-group-separator
 *   - Files: -r/-R recursion, -d ACTION, -a text mode, -I skip binary,
 *     --binary-files=TYPE, "Binary file X matches" summary
 *   - Filters: --include/--exclude/--exclude-from/--exclude-dir globs
 *   - Misc: -m max-count, -s no messages, --line-buffered,
 *     --help/--version, "-" reads stdin, exit status 0/1/2
 *
 * Known deviations from GNU grep:
 *   - Regex matching is leftmost-first (backtracking), not POSIX
 *     leftmost-longest; visible only with overlapping alternatives.
 *   - Case folding is ASCII-only; patterns operate on bytes, not
 *     multibyte characters.
 *   - At most 9 capture groups per pattern (\1 .. \9).
 *   - A single trailing CR is stripped from each input line so files
 *     written on Windows behave like their Unix counterparts.
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o grep.exe grep.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o grep grep.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o grep grep.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o grep grep.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o grep grep.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o grep grep.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/grep>
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
    #define GREP_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define GREP_PLATFORM_LINUX   1
    #define GREP_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define GREP_PLATFORM_MACOS   1
    #define GREP_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define GREP_PLATFORM_FREEBSD 1
    #define GREP_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define GREP_PLATFORM_OPENBSD 1
    #define GREP_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define GREP_PLATFORM_NETBSD  1
    #define GREP_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define GREP_PLATFORM_POSIX   1
#else
    #define GREP_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef GREP_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef GREP_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef GREP_PLATFORM_NETBSD
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
#include <errno.h>

#ifdef GREP_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define GREP_VERSION_STR       "v1.0.0"

/** @brief Program name used in diagnostics */
#define GREP_PROGRAM_NAME      "grep"

/** @brief Label used when searching standard input */
#define GREP_STDIN_LABEL       "(standard input)"

/** @brief Default context group separator string */
#define GREP_GROUP_SEP_DEFAULT "--"

/** @brief Maximum expansion of an interval expression {m,n} */
#define GREP_RE_DUP_MAX_LOCAL  1024L

/** @brief Maximum number of capture groups (backrefs \1..\9) */
#define GREP_RE_MAX_GROUPS     9

/** @brief Parser recursion guard against deeply nested expressions */
#define GREP_RE_MAX_DEPTH      512

/** @brief Total save slots: whole match + 2 slots per group */
#define GREP_SLOTS_MAX         (2 * (GREP_RE_MAX_GROUPS) + 2)

/** @brief Initial backtrack frame capacity */
#define GREP_FRAMES_INIT       256

/** @brief Initial visited-state hash buckets (power of two) */
#define GREP_VISIT_INIT        8192U

/** @brief Initial program instruction capacity */
#define GREP_CODE_INIT         64

/** @brief Initial character-class pool capacity */
#define GREP_CLASS_INIT        8

/** @brief Read buffer size for the streaming line reader */
#define GREP_RBUF_SIZE         (1u << 16)

/** @brief Initial growable line-buffer capacity */
#define GREP_LINEBUF_INIT      4096

/** @brief Initial context ring capacity */
#define GREP_RING_INIT         16

/** @brief Maximum directory recursion depth */
#define GREP_WALK_DEPTH_MAX    128

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Regex VM opcodes
 */
typedef enum {
    GREP_RE_OP_CHAR = 0,     /* <- literal char (z: folded char)          */
    GREP_RE_OP_ANY,          /* <- any char except '\n'                   */
    GREP_RE_OP_CLASS,        /* <- class match (x: class index)           */
    GREP_RE_OP_BOL,          /* <- start of line                          */
    GREP_RE_OP_EOL,          /* <- end of line                            */
    GREP_RE_OP_BOW,          /* <- beginning of word                      */
    GREP_RE_OP_EOW,          /* <- end of word                            */
    GREP_RE_OP_WBOUND,       /* <- word boundary (\b)                     */
    GREP_RE_OP_NWBOUND,      /* <- not a word boundary (\B)               */
    GREP_RE_OP_SAVE,         /* <- record position (x: slot)              */
    GREP_RE_OP_BACKREF,      /* <- backreference (x: group 1..9)          */
    GREP_RE_OP_SPLIT,        /* <- try x first, y on backtrack            */
    GREP_RE_OP_JMP,          /* <- jump (x: target)                       */
    GREP_RE_OP_MATCH,        /* <- success                                */
    GREP_RE_OP_STARCHAR,     /* <- greedy loop over one char (x:min y:max z:char)   */
    GREP_RE_OP_STARANY,      /* <- greedy loop over '.' (x:min y:max)     */
    GREP_RE_OP_STARCLASS     /* <- greedy loop over class (x:min y:max z:cls idx)   */
} grep_re_op_t;

/**
 * @brief Single regex VM instruction
 */
typedef struct {
    int32_t op;              /* <- opcode                                 */
    int32_t x;               /* <- operand 1                              */
    int32_t y;               /* <- operand 2                              */
    int32_t z;               /* <- operand 3                              */
} grep_re_inst_t;

/**
 * @brief Character class: 256-bit bitmap plus negation flag
 */
typedef struct {
    unsigned char bits[32];  /* <- membership bitmap                      */
    bool negate;             /* <- true if [^...]                         */
} grep_re_class_t;

/**
 * @brief Compiled regular expression program
 */
typedef struct {
    grep_re_inst_t * code;   /* <- instruction array                      */
    int32_t len;             /* <- used instructions                      */
    int32_t cap;             /* <- allocated instructions                 */
    grep_re_class_t * classes;
    int32_t ncls;            /* <- used classes                           */
    int32_t clscap;          /* <- allocated classes                      */
    bool icase;              /* <- case-insensitive matching              */
    int32_t ngroups;         /* <- number of capture groups               */
} grep_re_prog_t;

/**
 * @brief Pattern parse-tree node types
 */
typedef enum {
    GREP_N_EMPTY = 0,
    GREP_N_CHAR,
    GREP_N_ANY,
    GREP_N_CLASS,
    GREP_N_BOL,
    GREP_N_EOL,
    GREP_N_BOW,
    GREP_N_EOW,
    GREP_N_WBOUND,
    GREP_N_NWBOUND,
    GREP_N_CAT,
    GREP_N_ALT,
    GREP_N_STAR,
    GREP_N_PLUS,
    GREP_N_QUEST,
    GREP_N_REPEAT,
    GREP_N_GROUP,
    GREP_N_BACKREF
} grep_nodetype_t;

/**
 * @brief Pattern parse-tree node
 */
typedef struct grep_node {
    grep_nodetype_t type;    /* <- node kind                              */
    int32_t ch;              /* <- CHAR: folded char / BACKREF: group no  */
    int32_t cls;             /* <- CLASS: index into class pool           */
    int32_t gnum;            /* <- GROUP: group number                    */
    int32_t lo;              /* <- REPEAT minimum                         */
    int32_t hi;              /* <- REPEAT maximum (-1 = unbounded)        */
    struct grep_node * l;    /* <- first child                            */
    struct grep_node * r;    /* <- second child (CAT/ALT)                 */
} grep_node_t;

/**
 * @brief Recursive-descent pattern parser state
 */
typedef struct {
    const unsigned char * pat;
    size_t pos;              /* <- current scan offset                    */
    size_t len;              /* <- pattern length                         */
    bool bre;                /* <- true when BRE syntax                   */
    bool icase;              /* <- fold letters while building classes    */
    bool anchor_ok;          /* <- BRE '^' is an anchor at this position  */
    int32_t depth;           /* <- nesting guard                          */
    int32_t ngroups;         /* <- capture groups opened so far           */
    const char * err;        /* <- static error message on failure        */
    grep_re_prog_t * p;      /* <- emit target                            */
} grep_parser_t;

/**
 * @brief Backtrack frame: saved state of one alternative choice
 */
typedef struct {
    int32_t pc;              /* <- resume point                           */
    int32_t sp;              /* <- resume position                        */
    int32_t slot;            /* <- SAVE slot to restore (-1 = none)       */
    int32_t old;             /* <- previous value of that slot            */
} grep_frame_t;

/**
 * @brief Open-addressing set of visited (pc,sp) states, prevents
 *        infinite loops for patterns whose sub-expressions match empty
 */
typedef struct {
    uint64_t * keys;         /* <- bucket array (0 = empty)               */
    size_t cap;              /* <- bucket count (power of two)            */
    size_t mask;             /* <- cap - 1                                */
    size_t cnt;              /* <- live entries                           */
} grep_visit_t;

/**
 * @brief Streaming line reader over a FILE*
 */
typedef struct {
    FILE * fp;
    unsigned char * buf;     /* <- read-ahead buffer                      */
    size_t pos;              /* <- next unread byte                       */
    size_t len;              /* <- valid bytes in buf                     */
    bool eof;                /* <- no more input                          */
    bool bin_checked;        /* <- first chunk scanned for NUL            */
    bool bin_detected;       /* <- NUL found in first chunk               */
    uint64_t consumed;       /* <- absolute bytes handed out (incl '\n')  */
} grep_reader_t;

/**
 * @brief Growable list of strings
 */
typedef struct {
    char ** v;
    size_t n;
    size_t cap;
} grep_strlist_t;

/**
 * @brief One compiled search pattern
 */
typedef struct {
    char * raw;              /* <- original pattern text                  */
    bool fixed;              /* <- plain substring search (-F)            */
    bool compiled;           /* <- re program is valid                    */
    grep_re_prog_t re;       /* <- compiled program                       */
} grep_pattern_t;

/**
 * @brief Colorization mode for --color[=WHEN]
 */
typedef enum {
    GREP_COLOR_NEVER = 0,
    GREP_COLOR_AUTO,
    GREP_COLOR_ALWAYS
} grep_colormode_t;

/**
 * @brief Binary-file handling mode (--binary-files=TYPE / -a / -I)
 */
typedef enum {
    GREP_BIN_BINARY = 0,
    GREP_BIN_TEXT,
    GREP_BIN_SKIP
} grep_binmode_t;

/**
 * @brief Directory handling mode (-d ACTION)
 */
typedef enum {
    GREP_DIR_READ = 0,
    GREP_DIR_SKIP,
    GREP_DIR_RECURSE
} grep_dirmode_t;

/**
 * @brief Device/FIFO handling mode (--devices=ACTION)
 */
typedef enum {
    GREP_DEV_READ = 0,
    GREP_DEV_SKIP
} grep_devmode_t;

/**
 * @brief Pattern syntax selector (last of -E/-F/-G/-P wins)
 */
typedef enum {
    GREP_SYN_BRE = 0,
    GREP_SYN_ERE,
    GREP_SYN_FIXED,
    GREP_SYN_PERL
} grep_syntax_t;

/**
 * @brief All command-line options after parsing
 */
typedef struct {
    bool icase;              /* <- -i                                     */
    bool word;               /* <- -w                                     */
    bool line_regexp;        /* <- -x                                     */
    bool invert;             /* <- -v                                     */
    bool no_msgs;            /* <- -s                                     */
    bool count_mode;         /* <- -c                                     */
    bool list_with;          /* <- -l                                     */
    bool list_without;       /* <- -L                                     */
    bool only_matching;      /* <- -o                                     */
    bool quiet;              /* <- -q                                     */
    bool line_buffered;      /* <- --line-buffered                        */
    bool suppress_name;      /* <- -h                                     */
    bool force_name;         /* <- -H                                     */
    bool byte_offset;        /* <- -b                                     */
    bool line_number;        /* <- -n                                     */
    bool no_group_sep;       /* <- --no-group-separator                   */
    long long max_count;     /* <- -m (-1 = unlimited)                    */
    long long after_ctx;     /* <- -A                                     */
    long long before_ctx;    /* <- -B                                     */
    grep_colormode_t color;  /* <- --color=WHEN                           */
    grep_binmode_t binary;   /* <- --binary-files=TYPE                    */
    grep_dirmode_t dirs;     /* <- --directories=ACTION                   */
    grep_devmode_t devices;  /* <- --devices=ACTION                       */
    char * group_sep;        /* <- --group-separator value                */
    bool color_on;           /* <- resolved at startup (tty check)        */
    grep_strlist_t include;  /* <- --include globs                        */
    grep_strlist_t exclude;  /* <- --exclude globs                        */
    grep_strlist_t excl_dir; /* <- --exclude-dir globs                    */
} grep_options_t;

/**
 * @brief Global run status accumulated across all operands
 */
typedef struct {
    bool matched_any;        /* <- at least one line selected anywhere    */
    bool had_error;          /* <- at least one error occurred            */
    bool quit;               /* <- -q short-circuit requested             */
} grep_status_t;

/**
 * @brief One buffered history line (for before-context)
 */
typedef struct {
    unsigned char * text;    /* <- line copy (without newline)            */
    size_t len;              /* <- line length                            */
    uint64_t off;            /* <- absolute byte offset of line start     */
    long long no;            /* <- 1-based line number                    */
} grep_ctxline_t;

/**
 * @brief Circular ring of recent lines (before-context window)
 */
typedef struct {
    grep_ctxline_t * v;      /* <- slot array                             */
    size_t cap;              /* <- capacity                               */
    size_t head;             /* <- index of oldest entry                  */
    size_t count;            /* <- live entries                           */
} grep_ring_t;

/**
 * @brief Per-file processing state
 */
typedef struct {
    grep_ring_t ring;        /* <- before-context window                  */
    long long pending_after; /* <- remaining after-context lines          */
    long long last_out;      /* <- last printed line number (-1 none)     */
    bool printed_any;        /* <- anything printed for this file yet     */
    long long count;         /* <- selected-line counter                  */
    bool matched_file;       /* <- this file had >= 1 selected line       */
} grep_fstate_t;

/**
 * @brief Shared environment threaded through processing functions
 */
typedef struct {
    const grep_options_t * o;
    const grep_pattern_t * pats;
    size_t npat;
    grep_status_t * st;
} grep_env_t;

/********************************
 *    static prototypes
 ********************************/
/* --- small utilities --- */
static void *        _grep_xmalloc(size_t n);
static void *        _grep_xrealloc(void * p, size_t n);
static char *        _grep_xstrdup(const char * s);
static void          _grep_list_add(grep_strlist_t * l, const char * s);
static bool          _grep_streq(const char * a, const char * b);
static int           _grep_lower(int c);
static bool          _grep_is_word(unsigned c);
static bool          _grep_char_eq(unsigned a, unsigned b, bool icase);

/* --- option parsing helpers --- */
static void          _grep_opts_defaults(grep_options_t * o);
static void          _grep_opts_free(grep_options_t * o);
static void          _grep_usage_hint(void);
static void          _grep_print_help(void);
static void          _grep_print_version(void);
static long long     _grep_parse_num(const char * s, const char * what);
static void          _grep_set_color_mode(grep_options_t * o, const char * w);
static void          _grep_set_binary_mode(grep_options_t * o, const char * w);
static void          _grep_set_dir_mode(grep_options_t * o, const char * w);
static void          _grep_set_dev_mode(grep_options_t * o, const char * w);
static void          _grep_parse_args(int argc, char ** argv, grep_options_t * o,
                                      grep_strlist_t * pats, grep_strlist_t * files,
                                      grep_syntax_t * syn);
static void          _grep_patterns_from_file(grep_strlist_t * pats, const char * path);

/* --- glob matching (fnmatch subset) --- */
static bool          _grep_glob_match(const char * pat, const char * str);
static bool          _grep_glob_list_hit(const grep_strlist_t * l, const char * s);

/* --- regex engine: compilation --- */
static grep_node_t * _grep_mknode(grep_nodetype_t t);
static void          _grep_free_ast(grep_node_t * n);
static int32_t       _grep_re_emit(grep_re_prog_t * p, int32_t op,
                                   int32_t x, int32_t y, int32_t z);
static int32_t       _grep_re_class_new(grep_re_prog_t * p);
static void          _grep_class_add_range(grep_re_prog_t * p, int32_t idx,
                                           unsigned lo, unsigned hi, bool icase);
static void          _grep_class_add_named(grep_re_prog_t * p, int32_t idx,
                                           const char * name, bool icase);
static void          _grep_re_parse_class(grep_parser_t * ps);
static grep_node_t * _grep_re_parse_atom(grep_parser_t * ps);
static grep_node_t * _grep_re_parse_quant(grep_parser_t * ps);
static grep_node_t * _grep_re_parse_concat(grep_parser_t * ps);
static grep_node_t * _grep_re_parse_alt(grep_parser_t * ps);
static bool          _grep_re_try_interval(grep_parser_t * ps, bool ere_bare,
                                            long * plo, long * phi);
static bool          _grep_dollar_is_anchor(grep_parser_t * ps);
static grep_node_t * _grep_wrap(grep_nodetype_t kind, grep_node_t * child);
static void          _grep_class_bit(grep_re_prog_t * p, int32_t idx, unsigned c);
static bool          _grep_at_stop(grep_parser_t * ps);
static bool          _grep_alt_next(grep_parser_t * ps);
static void          _grep_alt_skip(grep_parser_t * ps);
static void          _grep_re_gen(const grep_node_t * n, grep_re_prog_t * p);
static int           _grep_re_compile(const char * pattern, bool ere, bool icase,
                                      grep_re_prog_t * out, const char ** err);
static void          _grep_re_prog_free(grep_re_prog_t * p);

/* --- regex engine: execution --- */
static bool          _grep_visit_add(grep_visit_t * v, int32_t pc, int32_t sp);
static bool          _grep_region_eq(const grep_re_prog_t * p,
                                     const unsigned char * a,
                                     const unsigned char * b, int32_t n);
static bool          _grep_class_test(const grep_re_prog_t * p, int32_t idx,
                                      unsigned c);
static bool          _grep_re_exec(const grep_re_prog_t * p,
                                   const unsigned char * line, int32_t len,
                                   int32_t start, int32_t * caps);

/* --- matching layer --- */
static bool          _grep_span_ok(const grep_options_t * o,
                                   const unsigned char * line, size_t len,
                                   size_t s, size_t e);
static bool          _grep_fixed_span(const char * needle, bool icase,
                                      const unsigned char * line, size_t len,
                                      size_t from, size_t * ms, size_t * me);
static bool          _grep_pat_span(const grep_pattern_t * p,
                                    const grep_options_t * o,
                                    const unsigned char * line, size_t len,
                                    size_t from, size_t * ms, size_t * me);
static bool          _grep_first_span(const grep_env_t * env,
                                      const unsigned char * line, size_t len,
                                      size_t from, size_t * ms, size_t * me);
static size_t        _grep_collect_spans(const grep_env_t * env,
                                         const unsigned char * line, size_t len,
                                         size_t from, uint32_t * spans,
                                         size_t max_pairs);
static bool          _grep_line_selected(const grep_env_t * env,
                                         const unsigned char * line, size_t len);

/* --- output helpers --- */
static void          _grep_maybe_sep(const grep_env_t * env, grep_fstate_t * fs,
                                     long long no);
static void          _grep_print_head(const char * disp, bool show_label,
                                      uint64_t off, bool show_off,
                                      long long no, bool show_no,
                                      char sepch, const grep_options_t * o);
static void          _grep_print_line(const grep_env_t * env,
                                      const unsigned char * line, size_t len,
                                      bool highlight);
static bool          _grep_ring_init(grep_ring_t * rg, long long want);
static void          _grep_ring_free(grep_ring_t * rg);
static void          _grep_ring_push(grep_ring_t * rg,
                                     const unsigned char * text, size_t len,
                                     uint64_t off, long long no);
static void          _grep_flush_ring(const grep_env_t * env,
                                      const char * disp, grep_fstate_t * fs,
                                      bool show_label);
static void          _grep_emit_context_line(const grep_env_t * env,
                                             const char * disp, grep_fstate_t * fs,
                                             const unsigned char * line, size_t len,
                                             uint64_t off, long long no,
                                             bool show_label);

/* --- input traversal --- */
static bool          _grep_reader_init(grep_reader_t * r, FILE * fp);
static void          _grep_reader_close(grep_reader_t * r);
static bool          _grep_readline(grep_reader_t * r, unsigned char ** buf,
                                    size_t * cap, size_t * plen, uint64_t * raw_off);
static void          _grep_report_error(const grep_env_t * env, const char * path);
static void          _grep_process_stream(const grep_env_t * env, FILE * fp,
                                          const char * disp, bool show_label);
static void          _grep_process_regular(const grep_env_t * env,
                                           const char * path, const char * disp,
                                           bool show_label);
static void          _grep_walk_dir(const grep_env_t * env, const char * path,
                                    int depth);
static void          _grep_process_operand(const grep_env_t * env,
                                           const char * spec, size_t total,
                                           bool from_recursive);

/********************************
 *    macros
 ********************************/

#ifndef grep_out_stream
    #define grep_out_stream stdout
#endif

#ifndef grep_err_stream
    #define grep_err_stream stderr
#endif

/** @brief Formatted print to stdout (printf-compatible). */
#ifndef grep_printf
    #define grep_printf(fmt, ...) fprintf(grep_out_stream, fmt, ##__VA_ARGS__)
#endif

/** @brief Write a NUL-terminated string to stdout. */
#ifndef grep_puts_raw
    #define grep_puts_raw(s) (void)fputs((s), grep_out_stream)
#endif

/** @brief Flush the stdout stream. */
#ifndef grep_fflush
    #define grep_fflush() (void)fflush(grep_out_stream)
#endif

/** @brief Diagnostics to stderr. */
#ifndef grep_eprintf
    #define grep_eprintf(fmt, ...) fprintf(grep_err_stream, fmt, ##__VA_ARGS__)
#endif

/** @brief TTY detection on standard output. */
#ifdef GREP_PLATFORM_WINDOWS
    #define grep_isatty_out() (_isatty(_fileno(grep_out_stream)) != 0)
#else
    #define grep_isatty_out() (isatty(fileno(grep_out_stream)) == 1)
#endif

/** @brief ANSI color fragments (GNU grep palette). */
#define GREP_C_RESET   "\033[0m"
#define GREP_C_MATCH   "\033[01;31m"
#define GREP_C_FNAME   "\033[35m"
#define GREP_C_LINENO  "\033[32m"
#define GREP_C_OFFSET  "\033[32m"
#define GREP_C_SEP     "\033[36m"

/** @brief Unbounded repeat sentinel stored in STARC max field. */
#define GREP_INF_REPEAT INT32_MAX

/** @brief printf conversion for long long (msvcrt lacks %lld). */
#ifdef GREP_PLATFORM_WINDOWS
    #define GREP_LL_FMT  "%I64d"
    #define GREP_ULL_FMT "%I64u"
#else
    #define GREP_LL_FMT  "%lld"
    #define GREP_ULL_FMT "%llu"
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the grep command
 *
 * Processing flow:
 *   1. Initialize default options
 *   2. Parse arguments (help/version handled inside, exits directly)
 *   3. Compile every supplied pattern (exits on bad regex)
 *   4. Choose operands: given files, stdin ("-"), or "." when recursive
 *   5. Process each operand; accumulate global match/error status
 *   6. Return GNU-compatible exit status: 0 match, 1 none, 2 error
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0/1/2 per POSIX grep conventions
 */
int main(int argc, char ** argv)
{
    grep_options_t opts;
    grep_status_t st = {false, false, false};
    grep_strlist_t raw_pats;
    grep_strlist_t files;
    grep_syntax_t syn = GREP_SYN_BRE;
    grep_pattern_t * pats = NULL;
    grep_env_t env;
    size_t i;
    size_t total_operands;
    int exit_code;

    memset(&raw_pats, 0, sizeof(raw_pats));
    memset(&files, 0, sizeof(files));
    _grep_opts_defaults(&opts);

    if (argc < 1 || !argv) {
        return 2;
    }

    _grep_parse_args(argc, argv, &opts, &raw_pats, &files, &syn);

#ifdef GREP_PLATFORM_WINDOWS
    {
        /* Enable VT processing so ANSI colors render on modern consoles */
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
            (void)SetConsoleMode(h, mode | 0x0004u);
        }
    }
#endif

    opts.color_on = (opts.color == GREP_COLOR_ALWAYS) ||
                    (opts.color == GREP_COLOR_AUTO && grep_isatty_out());

    /* Compile all patterns up front; any failure is fatal (exit 2). */
    if (raw_pats.n > 0) {
        bool fixed = (syn == GREP_SYN_FIXED);
        bool ere = (syn == GREP_SYN_ERE) || (syn == GREP_SYN_PERL);

        pats = (grep_pattern_t *)calloc(raw_pats.n, sizeof(grep_pattern_t));
        if (!pats) {
            grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
            return 2;
        }
        for (i = 0; i < raw_pats.n; i++) {
            pats[i].raw = _grep_xstrdup(raw_pats.v[i]);
            pats[i].fixed = fixed;
            if (!fixed) {
                const char * err = NULL;
                if (_grep_re_compile(raw_pats.v[i], ere, opts.icase,
                                     &pats[i].re, &err) != 0) {
                    grep_eprintf("%s: %s: %s\n", GREP_PROGRAM_NAME,
                                 raw_pats.v[i], err);
                    return 2;
                }
                pats[i].compiled = true;
            }
        }
    }

    env.o = &opts;
    env.pats = pats;
    env.npat = raw_pats.n;
    env.st = &st;

    /* No operands: recurse into "." or read standard input. */
    if (files.n == 0) {
        if (opts.dirs == GREP_DIR_RECURSE) {
            _grep_process_operand(&env, ".", 1U, false);
        }
        else {
            _grep_process_operand(&env, "-", 1U, false);
        }
    }
    else {
        total_operands = files.n;
        for (i = 0; i < files.n; i++) {
            _grep_process_operand(&env, files.v[i], total_operands, false);
            if (st.quit) {
                break;
            }
        }
    }

    if (st.quit) {
        exit_code = 0;
    }
    else if (st.had_error) {
        exit_code = 2;
    }
    else {
        exit_code = st.matched_any ? 0 : 1;
    }

    /* cleanup */
    for (i = 0; i < raw_pats.n; i++) {
        free(raw_pats.v[i]);
    }
    free(raw_pats.v);
    for (i = 0; i < files.n; i++) {
        free(files.v[i]);
    }
    free(files.v);
    if (pats) {
        for (i = 0; i < raw_pats.n; i++) {
            free(pats[i].raw);
            if (pats[i].compiled) {
                _grep_re_prog_free(&pats[i].re);
            }
        }
        free(pats);
    }
    _grep_opts_free(&opts);

    grep_fflush();
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Allocate memory or abort with a diagnostic
 */
static void * _grep_xmalloc(size_t n)
{
    void * p = malloc(n ? n : 1U);
    if (!p) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }
    return p;
}

/**
 * @brief Reallocate memory or abort with a diagnostic
 */
static void * _grep_xrealloc(void * p, size_t n)
{
    void * np = realloc(p, n ? n : 1U);
    if (!np) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }
    return np;
}

/**
 * @brief Duplicate a string or abort
 */
static char * _grep_xstrdup(const char * s)
{
    size_t n = strlen(s) + 1U;
    char * d = (char *)_grep_xmalloc(n);
    memcpy(d, s, n);
    return d;
}

/**
 * @brief Append a copy of s to a growable string list
 */
static void _grep_list_add(grep_strlist_t * l, const char * s)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2U : 8U;
        l->v = (char **)_grep_xrealloc(l->v, l->cap * sizeof(char *));
    }
    l->v[l->n++] = _grep_xstrdup(s);
}

/**
 * @brief NULL-safe string equality
 */
static bool _grep_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return strcmp(a, b) == 0;
}

/**
 * @brief ASCII lowercase folding (bytes >127 are returned unchanged)
 */
static int _grep_lower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/**
 * @brief Word-constituent test: alphanumerics plus underscore
 */
static bool _grep_is_word(unsigned c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c == '_');
}

/**
 * @brief Compare two bytes honoring optional ASCII case folding
 */
static bool _grep_char_eq(unsigned a, unsigned b, bool icase)
{
    if (icase) {
        return _grep_lower((int)a) == _grep_lower((int)b);
    }
    return a == b;
}

/**
 * @brief Install default option values
 */
static void _grep_opts_defaults(grep_options_t * o)
{
    memset(o, 0, sizeof(*o));
    o->max_count = -1;
    o->after_ctx = 0;
    o->before_ctx = 0;
    o->color = GREP_COLOR_AUTO;
    o->binary = GREP_BIN_BINARY;
    o->dirs = GREP_DIR_READ;
    o->devices = GREP_DEV_READ;
    o->group_sep = _grep_xstrdup(GREP_GROUP_SEP_DEFAULT);
}

/**
 * @brief Release heap members of the options structure
 */
static void _grep_opts_free(grep_options_t * o)
{
    size_t i;
    free(o->group_sep);
    for (i = 0; i < o->include.n; i++) {
        free(o->include.v[i]);
    }
    free(o->include.v);
    for (i = 0; i < o->exclude.n; i++) {
        free(o->exclude.v[i]);
    }
    free(o->exclude.v);
    for (i = 0; i < o->excl_dir.n; i++) {
        free(o->excl_dir.v[i]);
    }
    free(o->excl_dir.v);
}

/**
 * @brief Print the short usage hint used after option errors
 */
static void _grep_usage_hint(void)
{
    grep_eprintf("Usage: grep [OPTION]... PATTERNS [FILE]...\n");
    grep_eprintf("Try '%s --help' for more information.\n", GREP_PROGRAM_NAME);
}

/**
 * @brief Print full help text (mirrors GNU grep layout)
 */
static void _grep_print_help(void)
{
    grep_printf(
        "Usage: grep [OPTION]... PATTERNS [FILE]...\n"
        "Search for PATTERNS in each FILE.\n"
        "Example: grep -i 'hello world' menu.h main.c\n"
        "\n"
        "Pattern selection and interpretation:\n"
        "  -E, --extended-regexp       PATTERNS are extended regular expressions\n"
        "  -F, --fixed-strings         PATTERNS are strings\n"
        "  -G, --basic-regexp          PATTERNS are basic regular expressions\n"
        "  -P, --perl-regexp           PATTERNS are Perl-style regular expressions\n"
        "  -e, --regexp=PATTERNS       use PATTERNS for matching\n"
        "  -f, --file=FILE             take PATTERNS from FILE\n"
        "  -i, --ignore-case           ignore case distinctions\n"
        "  -w, --word-regexp           force PATTERNS to match only whole words\n"
        "  -x, --line-regexp           force PATTERNS to match only whole lines\n"
        "  -v, --invert-match          select non-matching lines\n"
        "\n"
        "Miscellaneous:\n"
        "  -s, --no-messages           suppress error messages\n"
        "      --help                  display this help and exit\n"
        "  -V, --version               display version information and exit\n"
        "\n"
        "Output control:\n"
        "  -m, --max-count=NUM         stop after NUM selected lines\n"
        "  -b, --byte-offset           print the byte offset with output lines\n"
        "  -n, --line-number           print line number with output lines\n"
        "      --line-buffered         flush output on every line\n"
        "  -H, --with-filename         print file name with output lines\n"
        "  -h, --no-filename           suppress the file name prefix on output\n"
        "  -o, --only-matching         show only nonempty parts of lines that match\n"
        "  -q, --quiet, --silent       suppress all normal output\n"
        "      --label=LABEL           ignored (compatibility)\n"
        "  -L, --files-without-match   print only names of FILEs with no selected lines\n"
        "  -l, --files-with-matches    print only names of FILEs with selected lines\n"
        "  -c, --count                 print only a count of selected lines per FILE\n"
        "      --color[=WHEN]          colorize the output;\n"
        "      --colour[=WHEN]         WHEN is 'always', 'never', or 'auto' (default)\n"
        "      --no-color              suppress color markers\n"
        "      --group-separator=SEP   use SEP around matching lines in context output\n"
        "      --no-group-separator    do not print separator lines around context\n"
        "\n"
        "Context control:\n"
        "  -B, --before-context=NUM    print NUM lines of leading context\n"
        "  -A, --after-context=NUM     print NUM lines of trailing context\n"
        "  -C, --context=NUM           print NUM lines of output context\n"
        "  -NUM                        same as -C NUM\n"
        "\n"
        "File and directory selection:\n"
        "  -a, --text                  process a binary file as if it were text\n"
        "  -I                          equivalent to --binary-files=without-match\n"
        "      --binary-files=TYPE     TYPE is 'binary', 'text', or 'without-match'\n"
        "  -d, --directories=ACTION    how to handle directories;\n"
        "                              ACTION is 'read', 'skip', or 'recurse'\n"
        "      --devices=ACTION        how to handle devices and FIFOs;\n"
        "                              ACTION is 'read' or 'skip'\n"
        "  -r, --recursive             like --directories=recurse\n"
        "  -R, --dereference-recursive likewise\n"
        "      --include=GLOB          search only files whose base name matches GLOB\n"
        "      --exclude=GLOB          skip files whose base name matches GLOB\n"
        "      --exclude-from=FILE     read exclude globs from FILE\n"
        "      --exclude-dir=GLOB      skip directories whose base name matches GLOB\n"
        "\n"
        "Exit status:\n"
        "  0 if a line is selected, 1 if none were selected, 2 if an error occurred.\n"
    );
}

/**
 * @brief Print version banner (project style)
 */
static void _grep_print_version(void)
{
    grep_printf("grep (cclinuxtools) %s\n", GREP_VERSION_STR);
    grep_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    grep_printf("%s", "License MIT: <https://mit-license.org/>\n");
    grep_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    grep_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse a non-negative integer argument
 * @param s     digit string
 * @param what  human-readable option description for diagnostics
 * @return parsed value; exits with status 2 on malformed input
 */
static long long _grep_parse_num(const char * s, const char * what)
{
    char * end = NULL;
    long long v;

    if (!s || !*s) {
        goto bad;
    }
    v = strtoll(s, &end, 10);
    if (*end != '\0' || v < 0) {
        goto bad;
    }
    return v;

bad:
    grep_eprintf("%s: invalid %s\n", GREP_PROGRAM_NAME, what);
    exit(2);
}

/**
 * @brief Apply a --color WHEN keyword
 */
static void _grep_set_color_mode(grep_options_t * o, const char * w)
{
    if (_grep_streq(w, "never") || _grep_streq(w, "no") ||
        _grep_streq(w, "none")) {
        o->color = GREP_COLOR_NEVER;
    }
    else if (_grep_streq(w, "always") || _grep_streq(w, "yes")) {
        o->color = GREP_COLOR_ALWAYS;
    }
    else if (_grep_streq(w, "auto") || _grep_streq(w, "tty")) {
        o->color = GREP_COLOR_AUTO;
    }
    else {
        grep_eprintf("%s: invalid argument '%s' for '--color'\n",
                     GREP_PROGRAM_NAME, w);
        exit(2);
    }
}

/**
 * @brief Apply a --binary-files TYPE keyword
 */
static void _grep_set_binary_mode(grep_options_t * o, const char * w)
{
    if (_grep_streq(w, "binary")) {
        o->binary = GREP_BIN_BINARY;
    }
    else if (_grep_streq(w, "text") || _grep_streq(w, "ascii")) {
        o->binary = GREP_BIN_TEXT;
    }
    else if (_grep_streq(w, "without-match")) {
        o->binary = GREP_BIN_SKIP;
    }
    else {
        grep_eprintf("%s: invalid argument '%s' for '--binary-files'\n",
                     GREP_PROGRAM_NAME, w);
        exit(2);
    }
}

/**
 * @brief Apply a --directories ACTION keyword
 */
static void _grep_set_dir_mode(grep_options_t * o, const char * w)
{
    if (_grep_streq(w, "read")) {
        o->dirs = GREP_DIR_READ;
    }
    else if (_grep_streq(w, "skip")) {
        o->dirs = GREP_DIR_SKIP;
    }
    else if (_grep_streq(w, "recurse")) {
        o->dirs = GREP_DIR_RECURSE;
    }
    else {
        grep_eprintf("%s: invalid argument '%s' for '--directories'\n",
                     GREP_PROGRAM_NAME, w);
        exit(2);
    }
}

/**
 * @brief Apply a --devices ACTION keyword
 */
static void _grep_set_dev_mode(grep_options_t * o, const char * w)
{
    if (_grep_streq(w, "read")) {
        o->devices = GREP_DEV_READ;
    }
    else if (_grep_streq(w, "skip")) {
        o->devices = GREP_DEV_SKIP;
    }
    else {
        grep_eprintf("%s: invalid argument '%s' for '--devices'\n",
                     GREP_PROGRAM_NAME, w);
        exit(2);
    }
}

/**
 * @brief Read patterns from a file, one per line ("-" means stdin).
 *        Trailing LF and CR characters are stripped; blank lines become
 *        empty patterns which match every line.
 */
static void _grep_patterns_from_file(grep_strlist_t * pats, const char * path)
{
    grep_reader_t rd;
    unsigned char * buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    uint64_t off = 0;
    FILE * fp;
    bool from_stdin = _grep_streq(path, "-");

    fp = from_stdin ? stdin : fopen(path, "rb");
    if (!fp) {
        grep_eprintf("%s: %s: %s\n", GREP_PROGRAM_NAME, path, strerror(errno));
        exit(2);
    }
    if (!_grep_reader_init(&rd, fp)) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }
    while (_grep_readline(&rd, &buf, &cap, &len, &off)) {
        char tmp[4096];
        size_t take = len < sizeof(tmp) - 1U ? len : sizeof(tmp) - 1U;
        memcpy(tmp, buf, take);
        tmp[take] = '\0';
        _grep_list_add(pats, tmp);
    }
    free(buf);
    _grep_reader_close(&rd);
    if (!from_stdin) {
        (void)fclose(fp);
    }
}

/**
 * @brief Full argument parser (GNU permutation style).
 *
 * The first bare operand becomes the pattern unless -e/-f was seen
 * before it; later bare operands are always files. Exits directly on
 * --help/--version and on any malformed option.
 */
static void _grep_parse_args(int argc, char ** argv, grep_options_t * o,
                             grep_strlist_t * pats, grep_strlist_t * files,
                             grep_syntax_t * syn)
{
    bool opt_ended = false;
    bool pat_src_seen = false;
    bool positional_taken = false;
    int i;

    for (i = 1; i < argc; i++) {
        const char * a = argv[i];

        if (opt_ended || a[0] != '-' || a[1] == '\0') {
            /* operand */
            if (!pat_src_seen && !positional_taken) {
                _grep_list_add(pats, a);
                positional_taken = true;
            }
            else {
                _grep_list_add(files, a);
            }
            continue;
        }

        if (a[1] == '-') {
            /* long option */
            const char * name = a + 2;
            const char * eq = strchr(name, '=');
            const char * val = NULL;
            char bufname[64];
            size_t nl;

            if (eq) {
                val = eq + 1;
                nl = (size_t)(eq - name);
            }
            else {
                nl = strlen(name);
            }
            if (nl >= sizeof(bufname)) {
                nl = sizeof(bufname) - 1U;
            }
            memcpy(bufname, name, nl);
            bufname[nl] = '\0';

#define GREP_LONG_IS(nm) (_grep_streq(bufname, (nm)))
#define GREP_NEED_VAL()                                                  \
            do {                                                         \
                if (!val) {                                              \
                    grep_eprintf("%s: option '--%s' requires an argument\n", \
                                 GREP_PROGRAM_NAME, bufname);            \
                    _grep_usage_hint();                                  \
                    exit(2);                                             \
                }                                                        \
            } while (0)

            if (GREP_LONG_IS("help")) {
                _grep_print_help();
                exit(0);
            }
            else if (GREP_LONG_IS("version")) {
                _grep_print_version();
                exit(0);
            }
            else if (GREP_LONG_IS("extended-regexp")) {
                *syn = GREP_SYN_ERE;
            }
            else if (GREP_LONG_IS("fixed-strings")) {
                *syn = GREP_SYN_FIXED;
            }
            else if (GREP_LONG_IS("basic-regexp")) {
                *syn = GREP_SYN_BRE;
            }
            else if (GREP_LONG_IS("perl-regexp")) {
                *syn = GREP_SYN_PERL;
            }
            else if (GREP_LONG_IS("regexp")) {
                if (!val) {
                    grep_eprintf("%s: option '--regexp' requires an argument\n",
                                 GREP_PROGRAM_NAME);
                    _grep_usage_hint();
                    exit(2);
                }
                _grep_list_add(pats, val);
                pat_src_seen = true;
            }
            else if (GREP_LONG_IS("file")) {
                if (!val) {
                    grep_eprintf("%s: option '--file' requires an argument\n",
                                 GREP_PROGRAM_NAME);
                    _grep_usage_hint();
                    exit(2);
                }
                _grep_patterns_from_file(pats, val);
                pat_src_seen = true;
            }
            else if (GREP_LONG_IS("ignore-case")) {
                o->icase = true;
            }
            else if (GREP_LONG_IS("word-regexp")) {
                o->word = true;
            }
            else if (GREP_LONG_IS("line-regexp")) {
                o->line_regexp = true;
            }
            else if (GREP_LONG_IS("invert-match")) {
                o->invert = true;
            }
            else if (GREP_LONG_IS("no-messages")) {
                o->no_msgs = true;
            }
            else if (GREP_LONG_IS("count")) {
                o->count_mode = true;
            }
            else if (GREP_LONG_IS("files-with-matches")) {
                o->list_with = true;
            }
            else if (GREP_LONG_IS("files-without-match")) {
                o->list_without = true;
            }
            else if (GREP_LONG_IS("only-matching")) {
                o->only_matching = true;
            }
            else if (GREP_LONG_IS("quiet") || GREP_LONG_IS("silent")) {
                o->quiet = true;
            }
            else if (GREP_LONG_IS("line-number")) {
                o->line_number = true;
            }
            else if (GREP_LONG_IS("byte-offset")) {
                o->byte_offset = true;
            }
            else if (GREP_LONG_IS("with-filename")) {
                o->force_name = true;
                o->suppress_name = false;
            }
            else if (GREP_LONG_IS("no-filename")) {
                o->suppress_name = true;
                o->force_name = false;
            }
            else if (GREP_LONG_IS("line-buffered")) {
                o->line_buffered = true;
            }
            else if (GREP_LONG_IS("recursive")) {
                o->dirs = GREP_DIR_RECURSE;
            }
            else if (GREP_LONG_IS("dereference-recursive")) {
                o->dirs = GREP_DIR_RECURSE;
            }
            else if (GREP_LONG_IS("text")) {
                o->binary = GREP_BIN_TEXT;
            }
            else if (GREP_LONG_IS("max-count")) {
                GREP_NEED_VAL();
                o->max_count = _grep_parse_num(val, "max count");
            }
            else if (GREP_LONG_IS("after-context")) {
                GREP_NEED_VAL();
                o->after_ctx = _grep_parse_num(val, "context length argument");
            }
            else if (GREP_LONG_IS("before-context")) {
                GREP_NEED_VAL();
                o->before_ctx = _grep_parse_num(val, "context length argument");
            }
            else if (GREP_LONG_IS("context")) {
                GREP_NEED_VAL();
                o->after_ctx = _grep_parse_num(val, "context length argument");
                o->before_ctx = o->after_ctx;
            }
            else if (GREP_LONG_IS("color") || GREP_LONG_IS("colour")) {
                if (val) {
                    _grep_set_color_mode(o, val);
                }
                else {
                    o->color = GREP_COLOR_ALWAYS;
                }
            }
            else if (GREP_LONG_IS("no-color") || GREP_LONG_IS("no-colour")) {
                o->color = GREP_COLOR_NEVER;
            }
            else if (GREP_LONG_IS("group-separator")) {
                GREP_NEED_VAL();
                free(o->group_sep);
                o->group_sep = _grep_xstrdup(val);
            }
            else if (GREP_LONG_IS("no-group-separator")) {
                o->no_group_sep = true;
            }
            else if (GREP_LONG_IS("label")) {
                /* accepted for compatibility, ignored */
            }
            else if (GREP_LONG_IS("binary-files")) {
                GREP_NEED_VAL();
                _grep_set_binary_mode(o, val);
            }
            else if (GREP_LONG_IS("directories")) {
                GREP_NEED_VAL();
                _grep_set_dir_mode(o, val);
            }
            else if (GREP_LONG_IS("devices")) {
                GREP_NEED_VAL();
                _grep_set_dev_mode(o, val);
            }
            else if (GREP_LONG_IS("include")) {
                GREP_NEED_VAL();
                _grep_list_add(&o->include, val);
            }
            else if (GREP_LONG_IS("exclude")) {
                GREP_NEED_VAL();
                _grep_list_add(&o->exclude, val);
            }
            else if (GREP_LONG_IS("exclude-dir")) {
                GREP_NEED_VAL();
                _grep_list_add(&o->excl_dir, val);
            }
            else if (GREP_LONG_IS("exclude-from")) {
                GREP_NEED_VAL();
                _grep_patterns_from_file(&o->exclude, val);
            }
            else {
                grep_eprintf("%s: unrecognized option '--%s'\n",
                             GREP_PROGRAM_NAME, bufname);
                _grep_usage_hint();
                exit(2);
            }
#undef GREP_NEED_VAL
#undef GREP_LONG_IS
            continue;
        }

        /* short option cluster (or -NUM context form) */
        {
            const char * q = a + 1;

            if (q[0] >= '0' && q[0] <= '9') {
                long long n = 0;
                while (*q >= '0' && *q <= '9') {
                    n = n * 10 + (*q - '0');
                    q++;
                }
                if (*q == '\0') {
                    o->after_ctx = n;
                    o->before_ctx = n;
                    continue;
                }
                grep_eprintf("%s: invalid option combination\n",
                             GREP_PROGRAM_NAME);
                _grep_usage_hint();
                exit(2);
            }

            while (*q != '\0') {
                char c = *q++;

                switch (c) {
                    case 'E':
                        *syn = GREP_SYN_ERE;
                        break;

                    case 'F':
                        *syn = GREP_SYN_FIXED;
                        break;

                    case 'G':
                        *syn = GREP_SYN_BRE;
                        break;

                    case 'P':
                        *syn = GREP_SYN_PERL;
                        break;

                    case 'i':
                    case 'y':
                        o->icase = true;
                        break;

                    case 'w':
                        o->word = true;
                        break;

                    case 'x':
                        o->line_regexp = true;
                        break;

                    case 'v':
                        o->invert = true;
                        break;

                    case 's':
                        o->no_msgs = true;
                        break;

                    case 'c':
                        o->count_mode = true;
                        break;

                    case 'l':
                        o->list_with = true;
                        break;

                    case 'L':
                        o->list_without = true;
                        break;

                    case 'o':
                        o->only_matching = true;
                        break;

                    case 'q':
                        o->quiet = true;
                        break;

                    case 'n':
                        o->line_number = true;
                        break;

                    case 'b':
                        o->byte_offset = true;
                        break;

                    case 'H':
                        o->force_name = true;
                        o->suppress_name = false;
                        break;

                    case 'h':
                        o->suppress_name = true;
                        o->force_name = false;
                        break;

                    case 'a':
                        o->binary = GREP_BIN_TEXT;
                        break;

                    case 'I':
                        o->binary = GREP_BIN_SKIP;
                        break;

                    case 'r':
                    case 'R':
                        o->dirs = GREP_DIR_RECURSE;
                        break;

                    case 'V':
                        _grep_print_version();
                        exit(0);

                    case 'e': {
                        const char * rest = *q ? q : NULL;
                        const char * pat;
                        if (rest) {
                            pat = rest;
                            q += strlen(rest);
                        }
                        else if (i + 1 < argc) {
                            pat = argv[++i];
                        }
                        else {
                            grep_eprintf(
                                "%s: option requires an argument -- 'e'\n",
                                GREP_PROGRAM_NAME);
                            _grep_usage_hint();
                            exit(2);
                        }
                        _grep_list_add(pats, pat);
                        pat_src_seen = true;
                        break;
                    }

                    case 'f': {
                        const char * rest = *q ? q : NULL;
                        const char * path;
                        if (rest) {
                            path = rest;
                            q += strlen(rest);
                        }
                        else if (i + 1 < argc) {
                            path = argv[++i];
                        }
                        else {
                            grep_eprintf(
                                "%s: option requires an argument -- 'f'\n",
                                GREP_PROGRAM_NAME);
                            _grep_usage_hint();
                            exit(2);
                        }
                        _grep_patterns_from_file(pats, path);
                        pat_src_seen = true;
                        break;
                    }

                    case 'm':
                    case 'A':
                    case 'B':
                    case 'C': {
                        const char * rest = *q ? q : NULL;
                        const char * vs;
                        long long n;
                        if (rest) {
                            vs = rest;
                            q += strlen(rest);
                        }
                        else if (i + 1 < argc) {
                            vs = argv[++i];
                        }
                        else {
                            grep_eprintf(
                                "%s: option requires an argument -- '%c'\n",
                                GREP_PROGRAM_NAME, c);
                            _grep_usage_hint();
                            exit(2);
                        }
                        n = _grep_parse_num(vs, c == 'm'
                                                ? "max count"
                                                : "context length argument");
                        if (c == 'm') {
                            o->max_count = n;
                        }
                        else if (c == 'A') {
                            o->after_ctx = n;
                        }
                        else if (c == 'B') {
                            o->before_ctx = n;
                        }
                        else {
                            o->after_ctx = n;
                            o->before_ctx = n;
                        }
                        break;
                    }

                    case 'd': {
                        const char * rest = *q ? q : NULL;
                        const char * vs;
                        if (rest) {
                            vs = rest;
                            q += strlen(rest);
                        }
                        else if (i + 1 < argc) {
                            vs = argv[++i];
                        }
                        else {
                            grep_eprintf(
                                "%s: option requires an argument -- 'd'\n",
                                GREP_PROGRAM_NAME);
                            _grep_usage_hint();
                            exit(2);
                        }
                        _grep_set_dir_mode(o, vs);
                        break;
                    }

                    default:
                        grep_eprintf("%s: invalid option -- '%c'\n",
                                     GREP_PROGRAM_NAME, c);
                        _grep_usage_hint();
                        exit(2);
                }
            }
        }
    }

    if (pats->n == 0) {
        grep_eprintf("%s: no PATTERNS given\n", GREP_PROGRAM_NAME);
        _grep_usage_hint();
        exit(2);
    }
}

/**
 * @brief fnmatch-subset wildcard compare: supports '*', '?',
 *        bracket classes with ranges and leading '!'/^ negation.
 * @return true when str matches pat
 */
static bool _grep_glob_match(const char * pat, const char * str)
{
    while (*pat != '\0') {
        if (*pat == '*') {
            while (pat[1] == '*') {
                pat++;
            }
            if (pat[1] == '\0') {
                return true;
            }
            while (*str != '\0') {
                if (_grep_glob_match(pat + 1, str)) {
                    return true;
                }
                str++;
            }
            return false;
        }
        else if (*pat == '?') {
            if (*str == '\0') {
                return false;
            }
        }
        else if (*pat == '[') {
            const char * p = pat + 1;
            bool negated = false;
            bool hit = false;

            if (*p == '!' || *p == '^') {
                negated = true;
                p++;
            }
            while (*p != '\0' && (*p != ']' || p == pat + 1 ||
                   (negated && p == pat + 2))) {
                unsigned char lo = (unsigned char)*p;
                unsigned char hi = lo;

                if (*p == '\\' && p[1] != '\0') {
                    p++;
                    lo = (unsigned char)*p;
                }
                if (p[1] == '-' && p[2] != '\0' && p[2] != ']') {
                    hi = (unsigned char)p[2];
                    p += 2;
                }
                if ((unsigned char)*str >= lo && (unsigned char)*str <= hi) {
                    hit = true;
                }
                p++;
            }
            if (*p == '\0') {
                return false;
            }
            if (hit == negated || *str == '\0') {
                return false;
            }
            pat = p;
        }
        else {
            if (*pat == '\\' && pat[1] != '\0') {
                pat++;
            }
            if (*pat != *str) {
                return false;
            }
        }
        pat++;
        if (*str != '\0') {
            str++;
        }
        else if (*pat != '\0' && *pat != '*') {
            return false;
        }
    }
    return *str == '\0';
}

/**
 * @brief True when any glob in the list matches s.
 *        An empty include-list counts as "everything allowed" only for
 *        the caller's explicit handling; here we simply report hits.
 */
static bool _grep_glob_list_hit(const grep_strlist_t * l, const char * s)
{
    size_t i;

    for (i = 0; i < l->n; i++) {
        if (_grep_glob_match(l->v[i], s)) {
            return true;
        }
    }
    return false;
}

/* ===================== regex engine: compilation ===================== */

/**
 * @brief Allocate a zeroed parse node
 */
static grep_node_t * _grep_mknode(grep_nodetype_t t)
{
    grep_node_t * n = (grep_node_t *)calloc(1U, sizeof(grep_node_t));
    if (!n) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }
    n->type = t;
    return n;
}

/**
 * @brief Recursively release a parse tree
 */
static void _grep_free_ast(grep_node_t * n)
{
    if (!n) {
        return;
    }
    _grep_free_ast(n->l);
    _grep_free_ast(n->r);
    free(n);
}

/**
 * @brief Emit one instruction, growing the code array as needed
 * @return index of the emitted instruction
 */
static int32_t _grep_re_emit(grep_re_prog_t * p, int32_t op,
                             int32_t x, int32_t y, int32_t z)
{
    int32_t idx;

    if (p->len == p->cap) {
        p->cap = p->cap ? p->cap * 2 : GREP_CODE_INIT;
        p->code = (grep_re_inst_t *)_grep_xrealloc(p->code,
                                                   (size_t)p->cap * sizeof(grep_re_inst_t));
    }
    idx = p->len++;
    p->code[idx].op = op;
    p->code[idx].x = x;
    p->code[idx].y = y;
    p->code[idx].z = z;
    return idx;
}

/**
 * @brief Allocate and clear a new character class; returns its index
 */
static int32_t _grep_re_class_new(grep_re_prog_t * p)
{
    int32_t idx;

    if (p->ncls == p->clscap) {
        p->clscap = p->clscap ? p->clscap * 2 : GREP_CLASS_INIT;
        p->classes = (grep_re_class_t *)_grep_xrealloc(p->classes,
                                                       (size_t)p->clscap * sizeof(grep_re_class_t));
    }
    idx = p->ncls++;
    memset(&p->classes[idx], 0, sizeof(grep_re_class_t));
    return idx;
}

/**
 * @brief Set one membership bit in a class bitmap
 */
static void _grep_class_bit(grep_re_prog_t * p, int32_t idx, unsigned c)
{
    if (c < 256U) {
        p->classes[idx].bits[c >> 3] |= (unsigned char)(1U << (c & 7U));
    }
}

/**
 * @brief Add range [lo,hi] to a class, mirroring letter cases under -i
 */
static void _grep_class_add_range(grep_re_prog_t * p, int32_t idx,
                                  unsigned lo, unsigned hi, bool icase)
{
    unsigned c;

    if (hi > 255U) {
        hi = 255U;
    }
    for (c = lo; c <= hi; c++) {
        _grep_class_bit(p, idx, c);
        if (icase) {
            _grep_class_bit(p, idx, (unsigned)_grep_lower((int)c));
            _grep_class_bit(p, idx, (unsigned)((int)c >= 'a' && (int)c <= 'z'
                                               ? (int)c - ('a' - 'A')
                                               : (int)c));
        }
    }
}

/**
 * @brief Fill a class with the members of a POSIX [:name:] class
 */
static void _grep_class_add_named(grep_re_prog_t * p, int32_t idx,
                                  const char * name, bool icase)
{
    if (_grep_streq(name, "alnum")) {
        _grep_class_add_range(p, idx, '0', '9', false);
        _grep_class_add_range(p, idx, 'A', 'Z', false);
        _grep_class_add_range(p, idx, 'a', 'z', false);
    }
    else if (_grep_streq(name, "alpha")) {
        _grep_class_add_range(p, idx, 'A', 'Z', false);
        _grep_class_add_range(p, idx, 'a', 'z', false);
    }
    else if (_grep_streq(name, "blank")) {
        _grep_class_add_range(p, idx, ' ', ' ', false);
        _grep_class_add_range(p, idx, '\t', '\t', false);
    }
    else if (_grep_streq(name, "cntrl")) {
        _grep_class_add_range(p, idx, 0U, 31U, false);
        _grep_class_add_range(p, idx, 127U, 127U, false);
    }
    else if (_grep_streq(name, "digit")) {
        _grep_class_add_range(p, idx, '0', '9', false);
    }
    else if (_grep_streq(name, "graph")) {
        _grep_class_add_range(p, idx, '!', '~', false);
    }
    else if (_grep_streq(name, "lower")) {
        _grep_class_add_range(p, idx, 'a', 'z', icase);
    }
    else if (_grep_streq(name, "print")) {
        _grep_class_add_range(p, idx, ' ', '~', false);
    }
    else if (_grep_streq(name, "punct")) {
        _grep_class_add_range(p, idx, '!', '/', false);
        _grep_class_add_range(p, idx, ':', '@', false);
        _grep_class_add_range(p, idx, '[', '`', false);
        _grep_class_add_range(p, idx, '{', '~', false);
    }
    else if (_grep_streq(name, "space")) {
        _grep_class_add_range(p, idx, '\t', '\r', false);
        _grep_class_add_range(p, idx, ' ', ' ', false);
    }
    else if (_grep_streq(name, "upper")) {
        _grep_class_add_range(p, idx, 'A', 'Z', icase);
    }
    else if (_grep_streq(name, "xdigit")) {
        _grep_class_add_range(p, idx, '0', '9', false);
        _grep_class_add_range(p, idx, 'A', 'F', false);
        _grep_class_add_range(p, idx, 'a', 'f', false);
    }
    /* unknown names are silently treated as an empty class */
}

/**
 * @brief Parse a bracket expression starting at '['.
 *        Allocates the class and fills its bitmap; advances past ']'.
 */
static void _grep_re_parse_class(grep_parser_t * ps)
{
    grep_re_prog_t * p = ps->p;
    int32_t idx = _grep_re_class_new(p);
    bool neg = false;
    bool first = true;

    ps->pos++; /* consume '[' */

    if (ps->pos < ps->len && ps->pat[ps->pos] == '^') {
        neg = true;
        ps->pos++;
    }

    for (;;) {
        unsigned char c;
        unsigned char nx;

        if (ps->pos >= ps->len) {
            ps->err = "Unmatched [ or [^";
            return;
        }
        c = ps->pat[ps->pos];

        if (c == ']' && !first) {
            ps->pos++;
            break;
        }
        first = false;

        /* named class [:name:] */
        if (c == '[' && ps->pos + 1U < ps->len && ps->pat[ps->pos + 1U] == ':') {
            size_t close = ps->pos + 2U;
            char namebuf[32];

            while (close + 1U < ps->len &&
                   !(ps->pat[close] == ':' && ps->pat[close + 1U] == ']')) {
                close++;
            }
            if (close + 1U < ps->len) {
                size_t nlen = close - (ps->pos + 2U);
                if (nlen == 0 || nlen >= sizeof(namebuf)) {
                    ps->err = "Invalid character class name";
                    return;
                }
                memcpy(namebuf, &ps->pat[ps->pos + 2U], nlen);
                namebuf[nlen] = '\0';
                _grep_class_add_named(p, idx, namebuf, ps->icase);
                ps->pos = close + 2U;
                continue;
            }
            /* no closing ":]" — treat '[' literally */
        }

        nx = (ps->pos + 1U < ps->len) ? ps->pat[ps->pos + 1U] : 0U;

        /* possible range lo-hi ( '-' not last before ']' ) */
        if (nx == '-' && ps->pos + 2U < ps->len && ps->pat[ps->pos + 2U] != ']') {
            unsigned char hi = ps->pat[ps->pos + 2U];

            if ((unsigned)c > (unsigned)hi) {
                ps->err = "Invalid range end";
                return;
            }
            _grep_class_add_range(p, idx, (unsigned)c, (unsigned)hi, ps->icase);
            ps->pos += 3U;
            continue;
        }

        _grep_class_add_range(p, idx, (unsigned)c, (unsigned)c, ps->icase);
        ps->pos++;
    }

    p->classes[idx].negate = neg;
}

/**
 * @brief True when the upcoming token terminates a concat sequence
 */
static bool _grep_at_stop(grep_parser_t * ps)
{
    if (ps->pos >= ps->len) {
        return true;
    }
    if (!ps->bre) {
        return ps->pat[ps->pos] == '|' || ps->pat[ps->pos] == ')';
    }
    return ps->pat[ps->pos] == '\\' && ps->pos + 1U < ps->len &&
           (ps->pat[ps->pos + 1U] == '|' || ps->pat[ps->pos + 1U] == ')');
}

/**
 * @brief True when an alternation separator follows
 */
static bool _grep_alt_next(grep_parser_t * ps)
{
    if (ps->pos >= ps->len) {
        return false;
    }
    if (!ps->bre) {
        return ps->pat[ps->pos] == '|';
    }
    return ps->pat[ps->pos] == '\\' && ps->pos + 1U < ps->len &&
           ps->pat[ps->pos + 1U] == '|';
}

/**
 * @brief Consume an alternation separator ('|' or '\|')
 */
static void _grep_alt_skip(grep_parser_t * ps)
{
    ps->pos += ps->bre ? 2U : 1U;
}

/**
 * @brief Decide whether '$' at the current position acts as an anchor:
 *        end of pattern, or immediately followed only by group/alt closers.
 */
static bool _grep_dollar_is_anchor(grep_parser_t * ps)
{
    size_t q = ps->pos + 1U;

    while (q < ps->len) {
        unsigned char c = ps->pat[q];

        if (c == '\\') {
            if (q + 1U >= ps->len) {
                return false;
            }
            if (ps->bre && (ps->pat[q + 1U] == ')' || ps->pat[q + 1U] == '|')) {
                return true;
            }
            q += 2U;
            continue;
        }
        if (!ps->bre && (c == ')' || c == '|')) {
            return true;
        }
        return false;
    }
    return true;
}

/**
 * @brief Try to parse an interval quantifier body.
 *        Entry: positioned just after the opener ('{' consumed by the
 *        caller for ERE, "\\{" for BRE). On success positions past the
 *        closer and stores bounds (hi = -1 meaning unbounded).
 * @param ere_bare  true when '{' was bare (ERE): failures restore the
 *                  position and return false so '{' can be literal.
 *                  In BRE failures raise a hard error instead.
 */
static bool _grep_re_try_interval(grep_parser_t * ps, bool ere_bare,
                                  long * plo, long * phi)
{
    size_t start = ps->pos;
    size_t save = start;
    long lo = 0;
    long hi = -1;
    bool have_lo = false;
    bool comma = false;
    bool bad_content = false;

#define GREP_INTERVAL_FAIL(msg)                                        \
    do {                                                               \
        if (ere_bare) {                                                \
            ps->pos = save;                                            \
            return false;                                              \
        }                                                              \
        ps->err = (msg);                                               \
        ps->pos = save;                                                \
        return false;                                                  \
    } while (0)

    while (ps->pos < ps->len && ps->pat[ps->pos] >= '0' &&
           ps->pat[ps->pos] <= '9') {
        lo = lo * 10L + (long)(ps->pat[ps->pos] - '0');
        if (lo > GREP_RE_DUP_MAX_LOCAL) {
            bad_content = true;
        }
        have_lo = true;
        ps->pos++;
    }

    if (ps->pos < ps->len && ps->pat[ps->pos] == ',') {
        comma = true;
        ps->pos++;
        hi = -1;
        while (ps->pos < ps->len && ps->pat[ps->pos] >= '0' &&
               ps->pat[ps->pos] <= '9') {
            hi = (hi < 0 ? 0 : hi) * 10L + (long)(ps->pat[ps->pos] - '0');
            if (hi > GREP_RE_DUP_MAX_LOCAL) {
                bad_content = true;
            }
            ps->pos++;
        }
    }

    if (!have_lo && !comma) {
        GREP_INTERVAL_FAIL("Unmatched \\{");
    }
    if (comma && !have_lo) {
        lo = 0;
    }

    /* closer */
    if (ps->bre) {
        if (!(ps->pos + 1U < ps->len && ps->pat[ps->pos] == '\\' &&
              ps->pat[ps->pos + 1U] == '}')) {
            GREP_INTERVAL_FAIL("Unmatched \\{");
        }
        ps->pos += 2U;
    }
    else {
        if (!(ps->pos < ps->len && ps->pat[ps->pos] == '}')) {
            GREP_INTERVAL_FAIL("Unmatched \\{");
        }
        ps->pos++;
    }

    if (bad_content || (!comma && !have_lo) ||
        (hi >= 0 && lo > hi)) {
        GREP_INTERVAL_FAIL("Invalid content of \\{\\}");
    }

    *plo = lo;
    *phi = hi;
#undef GREP_INTERVAL_FAIL
    return true;
}

/**
 * @brief Create a STAR/PLUS/QUEST wrapper node around child
 */
static grep_node_t * _grep_wrap(grep_nodetype_t kind, grep_node_t * child)
{
    grep_node_t * n = _grep_mknode(kind);
    n->l = child;
    return n;
}

/**
 * @brief Parse atom plus any trailing quantifiers ({m,n}, *, +, ?)
 */
static grep_node_t * _grep_re_parse_quant(grep_parser_t * ps)
{
    grep_node_t * node = _grep_re_parse_atom(ps);

    if (ps->err || !node) {
        return node;
    }

    for (;;) {
        unsigned char c = (ps->pos < ps->len) ? ps->pat[ps->pos] : 0U;
        unsigned char nx = (ps->pos + 1U < ps->len) ? ps->pat[ps->pos + 1U] : 0U;
        grep_node_t * wrapped;

        if (c == '*') {
            ps->pos++;
            wrapped = _grep_wrap(GREP_N_STAR, node);
        }
        else if (!ps->bre && (c == '+' || c == '?')) {
            ps->pos++;
            wrapped = _grep_wrap(c == '+' ? GREP_N_PLUS : GREP_N_QUEST, node);
        }
        else if (ps->bre && c == '\\' && (nx == '+' || nx == '?')) {
            ps->pos += 2U;
            wrapped = _grep_wrap(nx == '+' ? GREP_N_PLUS : GREP_N_QUEST, node);
        }
        else if (!ps->bre && c == '{') {
            long lo = 0;
            long hi = -1;

            ps->pos++; /* consume '{' */
            if (_grep_re_try_interval(ps, true, &lo, &hi)) {
                wrapped = _grep_mknode(GREP_N_REPEAT);
                wrapped->l = node;
                wrapped->lo = (int32_t)lo;
                wrapped->hi = (int32_t)hi;
            }
            else {
                if (ps->err) {
                    _grep_free_ast(node);
                    return NULL;
                }
                /* bare '{' is a literal; rewind and stop quantifying */
                ps->pos--;
                break;
            }
        }
        else if (ps->bre && c == '\\' && nx == '{') {
            long lo = 0;
            long hi = -1;

            ps->pos += 2U; /* consume "\{" */
            if (!_grep_re_try_interval(ps, false, &lo, &hi)) {
                _grep_free_ast(node);
                return NULL;
            }
            wrapped = _grep_mknode(GREP_N_REPEAT);
            wrapped->l = node;
            wrapped->lo = (int32_t)lo;
            wrapped->hi = (int32_t)hi;
        }
        else {
            break;
        }
        node = wrapped;
    }

    return node;
}

/**
 * @brief Parse one atomic element of the pattern
 */
static grep_node_t * _grep_re_parse_atom(grep_parser_t * ps)
{
    unsigned char c;

    if (ps->pos >= ps->len) {
        return _grep_mknode(GREP_N_EMPTY);
    }

    c = ps->pat[ps->pos];

    /* ---- group open ---- */
    if ((!ps->bre && c == '(') ||
        (ps->bre && c == '\\' && ps->pos + 1U < ps->len &&
         ps->pat[ps->pos + 1U] == '(')) {
        grep_node_t * inner;
        grep_node_t * grp;
        int32_t g;

        if (++ps->depth > GREP_RE_MAX_DEPTH) {
            ps->err = "Expression nested too deeply";
            return NULL;
        }
        ps->pos += ps->bre ? 2U : 1U;
        if (++ps->ngroups > GREP_RE_MAX_GROUPS) {
            ps->err = "Too many () groups";
            return NULL;
        }
        g = ps->ngroups;
        ps->anchor_ok = true;

        inner = _grep_re_parse_alt(ps);
        ps->depth--;

        if (ps->err) {
            return NULL;
        }
        if (ps->bre) {
            if (!(ps->pos + 1U < ps->len && ps->pat[ps->pos] == '\\' &&
                  ps->pat[ps->pos + 1U] == ')')) {
                ps->err = "Unmatched ( or \\(";
                _grep_free_ast(inner);
                return NULL;
            }
            ps->pos += 2U;
        }
        else {
            if (!(ps->pos < ps->len && ps->pat[ps->pos] == ')')) {
                ps->err = "Unmatched ( or \\(";
                _grep_free_ast(inner);
                return NULL;
            }
            ps->pos++;
        }
        grp = _grep_mknode(GREP_N_GROUP);
        grp->gnum = g;
        grp->l = inner;
        ps->anchor_ok = false;
        return grp;
    }

    /* ---- alternation/group closers reaching atom level: error ---- */
    if (ps->bre && c == '\\' && ps->pos + 1U < ps->len &&
        (ps->pat[ps->pos + 1U] == ')' || ps->pat[ps->pos + 1U] == '(')) {
        ps->err = "Unmatched ) or \\)";
        return NULL;
    }

    switch (c) {
        case '[':
            {
                grep_node_t * n = _grep_mknode(GREP_N_CLASS);
                _grep_re_parse_class(ps);
                if (ps->err) {
                    free(n);
                    return NULL;
                }
                n->cls = ps->p->ncls - 1;
                ps->anchor_ok = false;
                return n;
            }

        case '.':
            ps->pos++;
            ps->anchor_ok = false;
            return _grep_mknode(GREP_N_ANY);

        case '^':
            if (!ps->bre || ps->anchor_ok) {
                ps->pos++;
                ps->anchor_ok = false;
                return _grep_mknode(GREP_N_BOL);
            }
            ps->pos++;
            ps->anchor_ok = false;
            {
                grep_node_t * n = _grep_mknode(GREP_N_CHAR);
                n->ch = '^';
                return n;
            }

        case '$':
            if (!ps->bre || _grep_dollar_is_anchor(ps)) {
                ps->pos++;
                ps->anchor_ok = false;
                return _grep_mknode(GREP_N_EOL);
            }
            ps->pos++;
            ps->anchor_ok = false;
            {
                grep_node_t * n = _grep_mknode(GREP_N_CHAR);
                n->ch = '$';
                return n;
            }

        case '\\':
            {
                unsigned char e;
                grep_node_t * n;

                if (ps->pos + 1U >= ps->len) {
                    ps->err = "Trailing backslash";
                    return NULL;
                }
                e = ps->pat[ps->pos + 1U];

                /* BRE escaped interval openers belong to the quantifier
                 * scanner; reaching the atom level means no preceding
                 * atom existed. */
                if (ps->bre && e == '{') {
                    ps->err = "Unmatched \\{";
                    return NULL;
                }
                if (e >= '1' && e <= '9') {
                    int g = e - '0';
                    if (g > ps->ngroups) {
                        ps->err = "Invalid back reference";
                        return NULL;
                    }
                    ps->pos += 2U;
                    ps->anchor_ok = false;
                    n = _grep_mknode(GREP_N_BACKREF);
                    n->ch = g;
                    return n;
                }
                ps->pos += 2U;
                ps->anchor_ok = false;
                switch (e) {
                    case '<':
                        return _grep_mknode(GREP_N_BOW);

                    case '>':
                        return _grep_mknode(GREP_N_EOW);

                    case 'b':
                        return _grep_mknode(GREP_N_WBOUND);

                    case 'B':
                        return _grep_mknode(GREP_N_NWBOUND);

                    case 'w':
                    case 'W':
                    case 's':
                    case 'S':
                        {
                            int32_t idx = _grep_re_class_new(ps->p);
                            if (e == 'w' || e == 'W') {
                                _grep_class_add_range(ps->p, idx, '0', '9', false);
                                _grep_class_add_range(ps->p, idx, 'A', 'Z', false);
                                _grep_class_add_range(ps->p, idx, 'a', 'z', false);
                                _grep_class_add_range(ps->p, idx, '_', '_', false);
                            }
                            else {
                                _grep_class_add_range(ps->p, idx, '\t', '\r', false);
                                _grep_class_add_range(ps->p, idx, ' ', ' ', false);
                            }
                            ps->p->classes[idx].negate =
                                (e == 'W' || e == 'S');
                            n = _grep_mknode(GREP_N_CLASS);
                            n->cls = idx;
                            return n;
                        }

                    default:
                        n = _grep_mknode(GREP_N_CHAR);
                        n->ch = (int32_t)(ps->icase ? _grep_lower((int)e) : (int)e);
                        return n;
                }
            }

        default:
            {
                grep_node_t * n = _grep_mknode(GREP_N_CHAR);
                ps->pos++;
                ps->anchor_ok = false;
                n->ch = (int32_t)(ps->icase ? _grep_lower((int)c) : (int)c);
                return n;
            }
    }
}

/**
 * @brief Parse a concatenation sequence of quantified atoms
 */
static grep_node_t * _grep_re_parse_concat(grep_parser_t * ps)
{
    grep_node_t * left = NULL;

    while (!ps->err) {
        grep_node_t * item;

        if (_grep_at_stop(ps)) {
            break;
        }
        item = _grep_re_parse_quant(ps);
        if (ps->err || !item) {
            _grep_free_ast(left);
            return NULL;
        }
        if (!left) {
            left = item;
        }
        else {
            grep_node_t * cat = _grep_mknode(GREP_N_CAT);
            cat->l = left;
            cat->r = item;
            left = cat;
        }
    }

    if (ps->err) {
        _grep_free_ast(left);
        return NULL;
    }
    return left ? left : _grep_mknode(GREP_N_EMPTY);
}

/**
 * @brief Parse alternation branches joined by '|' (ERE) or '\|' (BRE ext)
 */
static grep_node_t * _grep_re_parse_alt(grep_parser_t * ps)
{
    grep_node_t * left = _grep_re_parse_concat(ps);

    while (!ps->err && _grep_alt_next(ps)) {
        grep_node_t * right;
        grep_node_t * alt;

        _grep_alt_skip(ps);
        ps->anchor_ok = true;
        right = _grep_re_parse_concat(ps);
        if (ps->err) {
            _grep_free_ast(left);
            return NULL;
        }
        alt = _grep_mknode(GREP_N_ALT);
        alt->l = left;
        alt->r = right;
        left = alt;
    }

    if (ps->err) {
        _grep_free_ast(left);
        return NULL;
    }
    return left;
}

/**
 * @brief Generate VM instructions from a parse subtree.
 *        Quantifiers over simple atoms collapse into fast greedy loops;
 *        general cases expand into SPLIT/JMP combinations.
 */
static void _grep_re_gen(const grep_node_t * n, grep_re_prog_t * p)
{
    switch (n->type) {
        case GREP_N_EMPTY:
        case GREP_N_CAT:
            if (n->type == GREP_N_CAT) {
                _grep_re_gen(n->l, p);
                _grep_re_gen(n->r, p);
            }
            break;

        case GREP_N_CHAR:
            (void)_grep_re_emit(p, GREP_RE_OP_CHAR, 0, 0, n->ch);
            break;

        case GREP_N_ANY:
            (void)_grep_re_emit(p, GREP_RE_OP_ANY, 0, 0, 0);
            break;

        case GREP_N_CLASS:
            (void)_grep_re_emit(p, GREP_RE_OP_CLASS, n->cls, 0, 0);
            break;

        case GREP_N_BOL:
            (void)_grep_re_emit(p, GREP_RE_OP_BOL, 0, 0, 0);
            break;

        case GREP_N_EOL:
            (void)_grep_re_emit(p, GREP_RE_OP_EOL, 0, 0, 0);
            break;

        case GREP_N_BOW:
            (void)_grep_re_emit(p, GREP_RE_OP_BOW, 0, 0, 0);
            break;

        case GREP_N_EOW:
            (void)_grep_re_emit(p, GREP_RE_OP_EOW, 0, 0, 0);
            break;

        case GREP_N_WBOUND:
            (void)_grep_re_emit(p, GREP_RE_OP_WBOUND, 0, 0, 0);
            break;

        case GREP_N_NWBOUND:
            (void)_grep_re_emit(p, GREP_RE_OP_NWBOUND, 0, 0, 0);
            break;

        case GREP_N_BACKREF:
            (void)_grep_re_emit(p, GREP_RE_OP_BACKREF, n->ch, 0, 0);
            break;

        case GREP_N_GROUP:
            (void)_grep_re_emit(p, GREP_RE_OP_SAVE, 2 * n->gnum, 0, 0);
            _grep_re_gen(n->l, p);
            (void)_grep_re_emit(p, GREP_RE_OP_SAVE, 2 * n->gnum + 1, 0, 0);
            break;

        case GREP_N_ALT:
            {
                int32_t split = _grep_re_emit(p, GREP_RE_OP_SPLIT, 0, 0, 0);
                int32_t b1 = p->len;
                int32_t jmp;
                int32_t b2;

                _grep_re_gen(n->l, p);
                jmp = _grep_re_emit(p, GREP_RE_OP_JMP, 0, 0, 0);
                b2 = p->len;
                _grep_re_gen(n->r, p);
                p->code[split].x = b1;
                p->code[split].y = b2;
                p->code[jmp].x = p->len;
            }
            break;

        case GREP_N_STAR:
        case GREP_N_PLUS:
        case GREP_N_QUEST:
        case GREP_N_REPEAT:
            {
                grep_node_t * child = n->l;
                bool simple = child && (child->type == GREP_N_CHAR ||
                                        child->type == GREP_N_ANY ||
                                        child->type == GREP_N_CLASS);
                int32_t min = 0;
                int32_t max = GREP_INF_REPEAT;

                if (n->type == GREP_N_PLUS) {
                    min = 1;
                }
                else if (n->type == GREP_N_QUEST) {
                    max = 1;
                }
                else if (n->type == GREP_N_REPEAT) {
                    min = n->lo;
                    max = (n->hi < 0) ? GREP_INF_REPEAT : n->hi;
                }

                if (simple) {
                    if (child->type == GREP_N_CHAR) {
                        (void)_grep_re_emit(p, GREP_RE_OP_STARCHAR,
                                            min, max, child->ch);
                    }
                    else if (child->type == GREP_N_ANY) {
                        (void)_grep_re_emit(p, GREP_RE_OP_STARANY,
                                            min, max, 0);
                    }
                    else {
                        (void)_grep_re_emit(p, GREP_RE_OP_STARCLASS,
                                            min, max, child->cls);
                    }
                    break;
                }

                /* generic expansions */
                if (n->type == GREP_N_STAR) {
                    int32_t split = _grep_re_emit(p, GREP_RE_OP_SPLIT, 0, 0, 0);
                    int32_t body = p->len;
                    int32_t jmp;

                    _grep_re_gen(child, p);
                    jmp = _grep_re_emit(p, GREP_RE_OP_JMP, split, 0, 0);
                    (void)jmp;
                    p->code[split].x = body;
                    p->code[split].y = p->len;
                }
                else if (n->type == GREP_N_PLUS) {
                    int32_t first = p->len;
                    int32_t split;

                    _grep_re_gen(child, p);
                    split = _grep_re_emit(p, GREP_RE_OP_SPLIT,
                                          first, p->len, 0);
                    (void)split;
                }
                else if (n->type == GREP_N_QUEST) {
                    int32_t split = _grep_re_emit(p, GREP_RE_OP_SPLIT, 0, 0, 0);
                    int32_t body = p->len;

                    _grep_re_gen(child, p);
                    p->code[split].x = body;
                    p->code[split].y = p->len;
                }
                else {
                    /* REPEAT: mandatory copies then optional/unbounded tail */
                    int32_t k;

                    for (k = 0; k < n->lo && n->hi != 0; k++) {
                        _grep_re_gen(child, p);
                    }
                    if (n->hi < 0) {
                        int32_t split = _grep_re_emit(p, GREP_RE_OP_SPLIT, 0, 0, 0);
                        int32_t body = p->len;

                        _grep_re_gen(child, p);
                        (void)_grep_re_emit(p, GREP_RE_OP_JMP, split, 0, 0);
                        p->code[split].x = body;
                        p->code[split].y = p->len;
                    }
                    else {
                        for (k = n->lo; k < n->hi; k++) {
                            int32_t split = _grep_re_emit(p, GREP_RE_OP_SPLIT, 0, 0, 0);
                            int32_t body = p->len;

                            _grep_re_gen(child, p);
                            p->code[split].x = body;
                            p->code[split].y = p->len;
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Compile a pattern into a VM program
 * @return 0 on success; -1 with *err pointing at a static message
 */
static int _grep_re_compile(const char * pattern, bool ere, bool icase,
                            grep_re_prog_t * out, const char ** err)
{
    grep_parser_t ps;
    grep_node_t * ast;

    memset(out, 0, sizeof(*out));

    memset(&ps, 0, sizeof(ps));
    ps.pat = (const unsigned char *)pattern;
    ps.len = strlen(pattern);
    ps.pos = 0;
    ps.bre = !ere;
    ps.icase = icase;
    ps.anchor_ok = true;
    ps.p = out;

    ast = _grep_re_parse_alt(&ps);
    if (!ps.err && ast && ps.pos < ps.len) {
        ps.err = "Unmatched ) or \\)";
    }
    if (!ps.err && ast) {
        _grep_re_gen(ast, out);
        (void)_grep_re_emit(out, GREP_RE_OP_MATCH, 0, 0, 0);
        out->ngroups = ps.ngroups;
        out->icase = icase;
    }
    _grep_free_ast(ast);

    if (ps.err) {
        free(out->code);
        free(out->classes);
        memset(out, 0, sizeof(*out));
        *err = ps.err;
        return -1;
    }
    return 0;
}

/**
 * @brief Release all storage owned by a compiled program
 */
static void _grep_re_prog_free(grep_re_prog_t * p)
{
    free(p->code);
    free(p->classes);
    memset(p, 0, sizeof(*p));
}

/* ===================== regex engine: execution ===================== */

/**
 * @brief Insert (pc,sp) into the visited set.
 * @return true if newly inserted; false when the state was seen already
 */
static bool _grep_visit_add(grep_visit_t * v, int32_t pc, int32_t sp)
{
    uint64_t key = (((uint64_t)(uint32_t)pc) << 32) | (uint32_t)sp;
    size_t idx;

    key |= 1ULL; /* reserve 0 as "empty bucket" */
    idx = (size_t)((key * 0x9E3779B97F4A7C15ULL >> 29)) & v->mask;

    while (v->keys[idx] != 0ULL) {
        if (v->keys[idx] == key) {
            return false;
        }
        idx = (idx + 1U) & v->mask;
    }
    v->keys[idx] = key;
    v->cnt++;

    if (v->cnt * 10U > v->cap * 7U) {
        /* grow and rehash, preserving every stored key */
        size_t ncap = v->cap * 2U;
        uint64_t * nk = (uint64_t *)calloc(ncap, sizeof(uint64_t));

        if (nk) {
            size_t i;

            for (i = 0; i < v->cap; i++) {
                uint64_t k = v->keys[i];
                size_t j;

                if (k == 0ULL) {
                    continue;
                }
                j = (size_t)((k * 0x9E3779B97F4A7C15ULL >> 29)) & (ncap - 1U);
                while (nk[j] != 0ULL) {
                    j = (j + 1U) & (ncap - 1U);
                }
                nk[j] = k;
            }
            free(v->keys);
            v->keys = nk;
            v->cap = ncap;
            v->mask = ncap - 1U;
        }
    }
    return true;
}

/**
 * @brief Fold-aware comparison of two byte regions
 */
static bool _grep_region_eq(const grep_re_prog_t * p, const unsigned char * a,
                            const unsigned char * b, int32_t n)
{
    int32_t i;

    if (n <= 0) {
        return true;
    }
    if (!p->icase) {
        return memcmp(a, b, (size_t)n) == 0;
    }
    for (i = 0; i < n; i++) {
        if (_grep_lower((int)a[i]) != _grep_lower((int)b[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Class membership test honoring negation and case folding
 */
static bool _grep_class_test(const grep_re_prog_t * p, int32_t idx, unsigned c)
{
    const grep_re_class_t * cl = &p->classes[idx];
    unsigned k = p->icase ? (unsigned)_grep_lower((int)c) : c;
    bool bit = (k < 256U) && ((cl->bits[k >> 3] >> (k & 7U)) & 1U);

    return cl->negate ? !bit : bit;
}

/**
 * @brief Execute the compiled program against a line.
 *        Iterative backtracking VM: SPLIT pushes the second branch as a
 *        backtrack frame; SAVE frames remember prior slot values so
 *        captures unwind correctly. A visited-set prunes repeated
 *        (pc,position) states, guaranteeing termination even when
 *        subexpressions match the empty string.
 *
 * @param p      compiled program
 * @param line   input bytes (may contain NULs)
 * @param len    line length
 * @param start  forced starting offset (leftmost scan)
 * @param caps   output save slots (whole match in slots 0/1)
 * @return true when the pattern matches starting exactly at start
 */
static bool _grep_re_exec(const grep_re_prog_t * p,
                          const unsigned char * line, int32_t len,
                          int32_t start, int32_t * caps)
{
    grep_frame_t * frames;
    size_t fcap = GREP_FRAMES_INIT;
    size_t fcnt = 0;
    grep_visit_t vis;
    int32_t pc = 0;
    int32_t sp = start;
    bool ok = false;
    bool need_bt = false;
    size_t si;

    for (si = 0; si < GREP_SLOTS_MAX; si++) {
        caps[si] = -1;
    }
    caps[0] = start;

    vis.cap = GREP_VISIT_INIT;
    vis.mask = vis.cap - 1U;
    vis.cnt = 0;
    vis.keys = (uint64_t *)calloc(vis.cap, sizeof(uint64_t));
    frames = (grep_frame_t *)malloc(fcap * sizeof(grep_frame_t));
    if (!vis.keys || !frames) {
        free(vis.keys);
        free(frames);
        /* Treat OOM as a non-match rather than losing the whole run */
        return false;
    }

#define GREP_VM_PUSH(P_, S_, SL_, OL_)                                   \
    do {                                                                 \
        if (fcnt == fcap) {                                              \
            fcap *= 2U;                                                  \
            frames = (grep_frame_t *)realloc(frames, fcap * sizeof(grep_frame_t)); \
            if (!frames) {                                               \
                free(vis.keys);                                          \
                return false;                                            \
            }                                                            \
        }                                                                \
        frames[fcnt].pc = (int32_t)(P_);                                 \
        frames[fcnt].sp = (int32_t)(S_);                                 \
        frames[fcnt].slot = (int32_t)(SL_);                              \
        frames[fcnt].old = (int32_t)(OL_);                               \
        fcnt++;                                                          \
    } while (0)

    for (;;) {
        const grep_re_inst_t * in = &p->code[pc];

        need_bt = false;

        switch (in->op) {
            case GREP_RE_OP_CHAR:
                if (sp < len &&
                    _grep_char_eq(line[sp], (unsigned)in->z, p->icase)) {
                    sp++;
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_ANY:
                if (sp < len && line[sp] != '\n') {
                    sp++;
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_CLASS:
                if (sp < len && _grep_class_test(p, in->x, line[sp])) {
                    sp++;
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_BOL:
                if (sp == 0) {
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_EOL:
                if (sp == len) {
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_BOW:
                if ((sp < len && _grep_is_word(line[sp])) &&
                    !(sp > 0 && _grep_is_word(line[sp - 1]))) {
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_EOW:
                if ((sp > 0 && _grep_is_word(line[sp - 1])) &&
                    !(sp < len && _grep_is_word(line[sp]))) {
                    pc++;
                }
                else {
                    need_bt = true;
                }
                break;

            case GREP_RE_OP_WBOUND:
                {
                    bool lw = sp > 0 && _grep_is_word(line[sp - 1]);
                    bool rw = sp < len && _grep_is_word(line[sp]);
                    if (lw != rw) {
                        pc++;
                    }
                    else {
                        need_bt = true;
                    }
                }
                break;

            case GREP_RE_OP_NWBOUND:
                {
                    bool lw = sp > 0 && _grep_is_word(line[sp - 1]);
                    bool rw = sp < len && _grep_is_word(line[sp]);
                    if (lw == rw) {
                        pc++;
                    }
                    else {
                        need_bt = true;
                    }
                }
                break;

            case GREP_RE_OP_SAVE:
                GREP_VM_PUSH(pc + 1, sp, in->x, caps[in->x]);
                caps[in->x] = sp;
                pc++;
                break;

            case GREP_RE_OP_BACKREF:
                {
                    int32_t g = in->x;
                    int32_t bs = caps[2 * g];
                    int32_t be = caps[2 * g + 1];
                    int32_t n;

                    if (bs < 0 || be < 0 || be < bs) {
                        /* unset reference matches the empty string */
                        pc++;
                        break;
                    }
                    n = be - bs;
                    if (sp + n > len ||
                        !_grep_region_eq(p, line + sp, line + bs, n)) {
                        need_bt = true;
                    }
                    else {
                        sp += n;
                        pc++;
                    }
                }
                break;

            case GREP_RE_OP_SPLIT:
                if (_grep_visit_add(&vis, pc, sp)) {
                    GREP_VM_PUSH(in->y, sp, -1, 0);
                    pc = in->x;
                }
                else {
                    need_bt = true; /* prune repeated state */
                }
                break;

            case GREP_RE_OP_JMP:
                pc = in->x;
                break;

            case GREP_RE_OP_STARCHAR:
                {
                    int32_t n = 0;
                    int32_t k;

                    while (n < in->y && sp + n < len &&
                           _grep_char_eq(line[sp + n], (unsigned)in->z,
                                         p->icase)) {
                        n++;
                    }
                    if (n < in->x) {
                        need_bt = true;
                    }
                    else {
                        for (k = in->x; k < n; k++) {
                            GREP_VM_PUSH(pc + 1, sp + k, -1, 0);
                        }
                        pc++;
                        sp += n;
                    }
                }
                break;

            case GREP_RE_OP_STARANY:
                {
                    int32_t n = 0;
                    int32_t k;

                    while (n < in->y && sp + n < len && line[sp + n] != '\n') {
                        n++;
                    }
                    if (n < in->x) {
                        need_bt = true;
                    }
                    else {
                        for (k = in->x; k < n; k++) {
                            GREP_VM_PUSH(pc + 1, sp + k, -1, 0);
                        }
                        pc++;
                        sp += n;
                    }
                }
                break;

            case GREP_RE_OP_STARCLASS:
                {
                    int32_t n = 0;
                    int32_t k;

                    while (n < in->y && sp + n < len &&
                           _grep_class_test(p, in->z, line[sp + n])) {
                        n++;
                    }
                    if (n < in->x) {
                        need_bt = true;
                    }
                    else {
                        for (k = in->x; k < n; k++) {
                            GREP_VM_PUSH(pc + 1, sp + k, -1, 0);
                        }
                        pc++;
                        sp += n;
                    }
                }
                break;

            case GREP_RE_OP_MATCH:
                caps[1] = sp;
                ok = true;
                break;

            default:
                need_bt = true;
                break;
        }

        if (ok) {
            break;
        }

        if (need_bt) {
            if (fcnt == 0) {
                break;
            }
            fcnt--;
            pc = frames[fcnt].pc;
            sp = frames[fcnt].sp;
            if (frames[fcnt].slot >= 0) {
                caps[frames[fcnt].slot] = frames[fcnt].old;
            }
        }
    }

#undef GREP_VM_PUSH

    free(frames);
    free(vis.keys);
    return ok;
}

/* ===================== matching layer ===================== */

/**
 * @brief Apply -x / -w constraints to a candidate span
 */
static bool _grep_span_ok(const grep_options_t * o,
                          const unsigned char * line, size_t len,
                          size_t s, size_t e)
{
    if (s > len || e > len || e < s) {
        return false;
    }
    if (o->line_regexp && !(s == 0 && e == len)) {
        return false;
    }
    if (o->word) {
        if (!(s == 0 || !_grep_is_word(line[s - 1]))) {
            return false;
        }
        if (!(e == len || !_grep_is_word(line[e]))) {
            return false;
        }
    }
    return true;
}

/**
 * @plain fixed-string search with optional ASCII case folding
 */
static bool _grep_fixed_span(const char * needle, bool icase,
                             const unsigned char * line, size_t len,
                             size_t from, size_t * ms, size_t * me)
{
    size_t nlen = strlen(needle);
    size_t i;

    if (from > len) {
        return false;
    }
    if (nlen == 0) {
        *ms = *me = from;
        return true;
    }
    if (len < nlen) {
        return false;
    }
    for (i = from; i + nlen <= len; i++) {
        size_t j;

        for (j = 0; j < nlen; j++) {
            if (!_grep_char_eq(line[i + j], (unsigned char)needle[j], icase)) {
                break;
            }
        }
        if (j == nlen) {
            *ms = i;
            *me = i + nlen;
            return true;
        }
    }
    return false;
}

/**
 * @brief Find the first acceptable span of one pattern at/after `from`
 */
static bool _grep_pat_span(const grep_pattern_t * p,
                           const grep_options_t * o,
                           const unsigned char * line, size_t len,
                           size_t from, size_t * ms, size_t * me)
{
    size_t s;
    size_t e;

    if (from > len) {
        return false;
    }

    if (p->fixed) {
        size_t cur = from;

        while (cur <= len &&
               _grep_fixed_span(p->raw, o->icase, line, len, cur, &s, &e)) {
            if (_grep_span_ok(o, line, len, s, e)) {
                *ms = s;
                *me = e;
                return true;
            }
            cur = (e > cur) ? e : cur + 1U;
        }
        return false;
    }

    if (o->line_regexp) {
        /* anchored to the whole line: only offset 0 can succeed */
        if (from != 0) {
            return false;
        }
    }

    for (s = from; s <= len; s++) {
        int32_t caps[GREP_SLOTS_MAX];

        if (!_grep_re_exec(&p->re, line, (int32_t)len, (int32_t)s, caps)) {
            continue;
        }
        e = (caps[1] >= 0) ? (size_t)caps[1] : s;
        if (e < s) {
            e = s;
        }
        if (_grep_span_ok(o, line, len, s, e)) {
            *ms = s;
            *me = e;
            return true;
        }
    }
    return false;
}

/**
 * @brief First span across all patterns (OR semantics)
 */
static bool _grep_first_span(const grep_env_t * env,
                             const unsigned char * line, size_t len,
                             size_t from, size_t * ms, size_t * me)
{
    size_t i;

    for (i = 0; i < env->npat; i++) {
        if (_grep_pat_span(&env->pats[i], env->o, line, len, from, ms, me)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Collect successive non-overlapping spans (used by -o and colors)
 * @param spans  packed pairs (start,end) of at most max_pairs
 * @return number of pairs stored
 */
static size_t _grep_collect_spans(const grep_env_t * env,
                                  const unsigned char * line, size_t len,
                                  size_t from, uint32_t * spans,
                                  size_t max_pairs)
{
    size_t cursor = from;
    size_t cnt = 0;

    while (cursor <= len && cnt < max_pairs) {
        size_t s;
        size_t e;

        if (!_grep_first_span(env, line, len, cursor, &s, &e)) {
            break;
        }
        if (e > s) {
            spans[cnt * 2U] = (uint32_t)s;
            spans[cnt * 2U + 1U] = (uint32_t)e;
            cnt++;
        }
        cursor = (e > cursor) ? e : cursor + 1U;
    }
    return cnt;
}

/**
 * @brief Selection test for one line (honors -v and -m 0)
 */
static bool _grep_line_selected(const grep_env_t * env,
                                const unsigned char * line, size_t len)
{
    size_t s;
    size_t e;
    bool any;

    if (env->o->max_count == 0) {
        return false;
    }
    any = _grep_first_span(env, line, len, 0, &s, &e);
    return env->o->invert ? !any : any;
}

/* ===================== output helpers ===================== */

/**
 * @brief Print the "--" group separator when the new output block is
 *        not contiguous with the previously printed line
 */
static void _grep_maybe_sep(const grep_env_t * env, grep_fstate_t * fs,
                            long long no)
{
    const grep_options_t * o = env->o;

    if (o->after_ctx == 0 && o->before_ctx == 0) {
        return; /* separators exist only in context mode */
    }
    if (!fs->printed_any || no == fs->last_out + 1 || o->no_group_sep) {
        return;
    }
    if (o->color_on) {
        grep_puts_raw(GREP_C_SEP);
    }
    fwrite(o->group_sep, 1, strlen(o->group_sep), grep_out_stream);
    if (o->color_on) {
        grep_puts_raw(GREP_C_RESET);
    }
    fputc('\n', grep_out_stream);
}

/**
 * @brief Print the colon/dash prefix chain preceding a line's content
 */
static void _grep_print_head(const char * disp, bool show_label,
                             uint64_t off, bool show_off,
                             long long no, bool show_no,
                             char sepch, const grep_options_t * o)
{
    if (show_label) {
        if (o->color_on) {
            grep_puts_raw(GREP_C_FNAME);
        }
        fputs(disp, grep_out_stream);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
        if (o->color_on) {
            grep_puts_raw(GREP_C_SEP);
        }
        fputc(sepch, grep_out_stream);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
    }
    if (show_off) {
        if (o->color_on) {
            grep_puts_raw(GREP_C_OFFSET);
        }
        grep_printf(GREP_ULL_FMT, (unsigned long long)off);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
        if (o->color_on) {
            grep_puts_raw(GREP_C_SEP);
        }
        fputc(sepch, grep_out_stream);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
    }
    if (show_no) {
        if (o->color_on) {
            grep_puts_raw(GREP_C_LINENO);
        }
        grep_printf(GREP_LL_FMT, no);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
        if (o->color_on) {
            grep_puts_raw(GREP_C_SEP);
        }
        fputc(sepch, grep_out_stream);
        if (o->color_on) {
            grep_puts_raw(GREP_C_RESET);
        }
    }
}

/**
 * @brief Write one content line, highlighting match spans when coloring
 */
static void _grep_print_line(const grep_env_t * env,
                             const unsigned char * line, size_t len,
                             bool highlight)
{
    if (!highlight || !env->o->color_on) {
        fwrite(line, 1, len, grep_out_stream);
        fputc('\n', grep_out_stream);
        return;
    }
    else {
        uint32_t * spans = (uint32_t *)malloc(sizeof(uint32_t) * 2U *
                                              (len + 1U));
        if (!spans) {
            fwrite(line, 1, len, grep_out_stream);
            fputc('\n', grep_out_stream);
            return;
        }
        else {
            size_t n = _grep_collect_spans(env, line, len, 0, spans, len + 1U);
            size_t pos = 0;
            size_t i;

            for (i = 0; i < n; i++) {
                size_t s = spans[i * 2U];
                size_t e = spans[i * 2U + 1U];

                fwrite(line + pos, 1, s - pos, grep_out_stream);
                grep_puts_raw(GREP_C_MATCH);
                fwrite(line + s, 1, e - s, grep_out_stream);
                grep_puts_raw(GREP_C_RESET);
                pos = e;
            }
            fwrite(line + pos, 1, len - pos, grep_out_stream);
            fputc('\n', grep_out_stream);
            free(spans);
        }
    }
}

/**
 * @brief Allocate a context ring large enough to hold `want` lines.
 *        A non-positive request leaves the ring empty (all pushes no-op).
 */
static bool _grep_ring_init(grep_ring_t * rg, long long want)
{
    memset(rg, 0, sizeof(*rg));
    if (want <= 0) {
        return true;
    }
    rg->cap = (size_t)want;
    rg->v = (grep_ctxline_t *)calloc(rg->cap, sizeof(grep_ctxline_t));
    return rg->v != NULL;
}

/**
 * @brief Release ring storage together with every buffered line copy
 */
static void _grep_ring_free(grep_ring_t * rg)
{
    size_t i;

    for (i = 0; i < rg->cap; i++) {
        free(rg->v[i].text);
    }
    free(rg->v);
    memset(rg, 0, sizeof(*rg));
}

/**
 * @brief Append a line to the before-context window, evicting the
 *        oldest entry once the ring is full
 */
static void _grep_ring_push(grep_ring_t * rg, const unsigned char * text,
                            size_t len, uint64_t off, long long no)
{
    grep_ctxline_t * slot;
    unsigned char * copy;

    if (rg->cap == 0U) {
        return;
    }
    if (rg->count == rg->cap) {
        /* full: overwrite the oldest slot, then advance head */
        slot = &rg->v[rg->head];
        free(slot->text);
        rg->head = (rg->head + 1U) % rg->cap;
    }
    else {
        slot = &rg->v[(rg->head + rg->count) % rg->cap];
        rg->count++;
    }
    copy = (unsigned char *)_grep_xmalloc(len ? len : 1U);
    memcpy(copy, text, len);
    slot->text = copy;
    slot->len = len;
    slot->off = off;
    slot->no = no;
}

/**
 * @brief Emit one context line (before- or after-context), updating
 *        separator and progress bookkeeping
 */
static void _grep_emit_context_line(const grep_env_t * env, const char * disp,
                                    grep_fstate_t * fs,
                                    const unsigned char * line, size_t len,
                                    uint64_t off, long long no,
                                    bool show_label)
{
    _grep_maybe_sep(env, fs, no);
    _grep_print_head(disp, show_label, off, env->o->byte_offset,
                     no, env->o->line_number, '-', env->o);
    _grep_print_line(env, line, len, false);
    fs->last_out = no;
    fs->printed_any = true;
}

/**
 * @brief Drain the before-context ring ahead of a match line
 */
static void _grep_flush_ring(const grep_env_t * env, const char * disp,
                             grep_fstate_t * fs, bool show_label)
{
    grep_ring_t * rg = &fs->ring;
    size_t k;

    for (k = 0; k < rg->count; k++) {
        const grep_ctxline_t * e = &rg->v[(rg->head + k) % rg->cap];

        if (e->no <= fs->last_out) {
            continue; /* already shown as after-context */
        }
        _grep_emit_context_line(env, disp, fs, e->text, e->len,
                                e->off, e->no, show_label);
    }
    rg->count = 0;
    rg->head = 0;
}

/* ===================== input traversal ===================== */

/**
 * @brief Bind a streaming reader to an open FILE*
 */
static bool _grep_reader_init(grep_reader_t * r, FILE * fp)
{
    memset(r, 0, sizeof(*r));
    r->fp = fp;
    r->buf = (unsigned char *)malloc(GREP_RBUF_SIZE);
    return r->buf != NULL;
}

/**
 * @brief Release the reader's internal buffer (not the FILE*)
 */
static void _grep_reader_close(grep_reader_t * r)
{
    free(r->buf);
    r->buf = NULL;
}

/**
 * @brief Read one line from the stream.
 *
 * The returned slice excludes the trailing LF and at most one CR that
 * immediately precedes it. Byte accounting (`consumed`, `*raw_off`)
 * always refers to the raw stream so -b offsets stay exact.
 *
 * @param r       reader state
 * @param buf     caller-owned growable line buffer
 * @param cap     capacity of that buffer
 * @param plen    receives the returned line length
 * @param raw_off receives the absolute start offset of the line
 * @return true when a line was produced; false at end of input
 */
static bool _grep_readline(grep_reader_t * r, unsigned char ** buf,
                           size_t * cap, size_t * plen, uint64_t * raw_off)
{
    uint64_t start = r->consumed;
    size_t n = 0;
    bool saw_nl = false;

    if (r->eof && r->pos >= r->len) {
        return false;
    }

    for (;;) {
        int c;

        if (r->pos >= r->len) {
            if (r->eof) {
                break;
            }
            r->len = fread(r->buf, 1U, GREP_RBUF_SIZE, r->fp);
            r->pos = 0;
            if (r->len == 0U) {
                r->eof = true;
                break;
            }
            if (!r->bin_checked) {
                size_t i;

                for (i = 0; i < r->len; i++) {
                    if (r->buf[i] == 0U) {
                        r->bin_detected = true;
                        break;
                    }
                }
                r->bin_checked = true;
            }
        }

        c = r->buf[r->pos++];
        r->consumed++;

        if (c == '\n') {
            saw_nl = true;
            break;
        }
        if (n + 1U > *cap) {
            *cap = *cap ? *cap * 2U : (size_t)GREP_LINEBUF_INIT;
            *buf = (unsigned char *)_grep_xrealloc(*buf, *cap);
        }
        (*buf)[n++] = (unsigned char)c;
    }

    if (n > 0U && (*buf)[n - 1U] == '\r') {
        n--; /* documented deviation: strip one trailing CR */
    }

    *plen = n;
    if (raw_off) {
        *raw_off = start;
    }
    return saw_nl || n > 0U;
}

/**
 * @brief Report a failed open/stat for `path` (honors -s)
 */
static void _grep_report_error(const grep_env_t * env, const char * path)
{
    env->st->had_error = true;
    if (env->o->no_msgs) {
        return;
    }
    grep_eprintf("%s: %s: %s\n", GREP_PROGRAM_NAME, path, strerror(errno));
}

/**
 * @brief Run the match loop over one open input stream.
 *
 * Implements selection counting, quiet/max-count short circuits,
 * default/-o line output, context windows, count/list summaries and
 * the "Binary file ... matches" report.
 */
static void _grep_process_stream(const grep_env_t * env, FILE * fp,
                                 const char * disp, bool show_label)
{
    const grep_options_t * o = env->o;
    grep_status_t * st = env->st;
    grep_fstate_t fs;
    grep_reader_t rd;
    unsigned char * buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    uint64_t off = 0;
    long long no = 0;
    bool stop = false;
    bool header_only;

    header_only = o->count_mode || o->list_with || o->list_without;

    memset(&fs, 0, sizeof(fs));
    fs.last_out = -1;

    if (!_grep_ring_init(&fs.ring, o->before_ctx)) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }
    if (!_grep_reader_init(&rd, fp)) {
        grep_eprintf("%s: out of memory\n", GREP_PROGRAM_NAME);
        exit(2);
    }

    while (!stop && _grep_readline(&rd, &buf, &cap, &len, &off)) {
        bool selected;
        bool binary_quiet;
        bool line_output;

        no++;

        if (rd.bin_detected && o->binary == GREP_BIN_SKIP) {
            break; /* --binary-files=without-match / -I */
        }

        selected = _grep_line_selected(env, buf, len);
        binary_quiet = rd.bin_detected && o->binary == GREP_BIN_BINARY;
        line_output = !header_only && !binary_quiet &&
                      !(o->invert && o->only_matching);

        if (!selected) {
            if (line_output) {
                if (fs.pending_after > 0) {
                    _grep_emit_context_line(env, disp, &fs, buf, len,
                                            off, no, show_label);
                    fs.pending_after--;
                }
                else if (o->before_ctx > 0) {
                    _grep_ring_push(&fs.ring, buf, len, off, no);
                }
            }
            continue;
        }

        st->matched_any = true;
        fs.matched_file = true;

        if (o->quiet) {
            st->quit = true;
            break;
        }
        if (o->max_count >= 0 && fs.count >= o->max_count) {
            stop = true;
            break;
        }
        fs.count++;
        fs.pending_after = o->after_ctx;

        if (!line_output) {
            continue;
        }

        if (o->only_matching) {
            uint32_t * spans = (uint32_t *)malloc(sizeof(uint32_t) * 2U *
                                                  (len + 1U));

            if (spans) {
                size_t np = _grep_collect_spans(env, buf, len, 0,
                                                spans, len + 1U);
                size_t q;

                for (q = 0; q < np; q++) {
                    size_t s = spans[q * 2U];
                    size_t e = spans[q * 2U + 1U];

                    _grep_print_head(disp, show_label,
                                     off + (uint64_t)s, o->byte_offset,
                                     no, o->line_number, ':', o);
                    if (o->color_on) {
                        grep_puts_raw(GREP_C_MATCH);
                    }
                    fwrite(buf + s, 1, e - s, grep_out_stream);
                    if (o->color_on) {
                        grep_puts_raw(GREP_C_RESET);
                    }
                    fputc('\n', grep_out_stream);
                    fs.printed_any = true;
                    fs.last_out = no;
                }
                free(spans);
            }
        }
        else {
            _grep_flush_ring(env, disp, &fs, show_label);
            _grep_maybe_sep(env, &fs, no);
            _grep_print_head(disp, show_label, off, o->byte_offset,
                             no, o->line_number, ':', o);
            _grep_print_line(env, buf, len, true);
            fs.last_out = no;
            fs.printed_any = true;
        }

        if (o->line_buffered) {
            grep_fflush();
        }
    }

    if (!o->quiet) {
        if (o->count_mode) {
            if (show_label) {
                fputs(disp, grep_out_stream);
                fputc(':', grep_out_stream);
            }
            grep_printf(GREP_LL_FMT"\n", fs.count);
        }
        else if (o->list_with) {
            if (fs.matched_file) {
                grep_printf("%s\n", disp);
            }
        }
        else if (o->list_without) {
            if (!fs.matched_file) {
                grep_printf("%s\n", disp);
            }
        }
        else if (rd.bin_detected && o->binary == GREP_BIN_BINARY &&
                 fs.matched_file) {
            grep_printf("Binary file %s matches\n", disp);
        }
    }

    _grep_ring_free(&fs.ring);
    _grep_reader_close(&rd);
    free(buf);
}

/**
 * @brief Search one regular file by path
 */
static void _grep_process_regular(const grep_env_t * env, const char * path,
                                  const char * disp, bool show_label)
{
    FILE * fp = fopen(path, "rb");

    if (!fp) {
        _grep_report_error(env, path);
        return;
    }
    _grep_process_stream(env, fp, disp, show_label);
    fclose(fp);
}

/**
 * @brief Depth-first recursive directory walk honoring --include,
 *        --exclude, --exclude-dir and --devices.
 */
static void _grep_walk_dir(const grep_env_t * env, const char * path,
                           int depth)
{
    const grep_options_t * o = env->o;
    bool label_children = o->force_name || !o->suppress_name;
    DIR * dp;
    struct dirent * de;

    if (depth > GREP_WALK_DEPTH_MAX) {
        return;
    }
    dp = opendir(path);
    if (!dp) {
        _grep_report_error(env, path);
        return;
    }

    while ((de = readdir(dp)) != NULL && !env->st->quit) {
        const char * name = de->d_name;
        size_t plen = strlen(path);
        size_t nlen = strlen(name);
        bool need_sep;
        char * full;
        struct stat sb;

        if (name[0] == '.' &&
            (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }
        if (_grep_glob_list_hit(&o->excl_dir, name)) {
            continue;
        }

        need_sep = plen > 0U && path[plen - 1U] != '/' &&
                   path[plen - 1U] != '\\';
        full = (char *)_grep_xmalloc(plen + nlen + 2U);
        memcpy(full, path, plen);
        if (need_sep) {
            full[plen++] = '/';
        }
        memcpy(full + plen, name, nlen + 1U);

        if (stat(full, &sb) != 0) {
            _grep_report_error(env, full);
            free(full);
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            _grep_walk_dir(env, full, depth + 1);
        }
        else if (S_ISREG(sb.st_mode)) {
            if (!_grep_glob_list_hit(&o->exclude, name) &&
                (o->include.n == 0U ||
                 _grep_glob_list_hit(&o->include, name))) {
                _grep_process_regular(env, full, full, label_children);
            }
        }
        else if (o->devices == GREP_DEV_READ) {
            _grep_process_regular(env, full, full, label_children);
        }
        free(full);
    }
    closedir(dp);
}

/**
 * @brief Dispatch one command-line operand: stdin marker, directory,
 *        regular file or special device.
 */
static void _grep_process_operand(const grep_env_t * env, const char * spec,
                                  size_t total, bool from_recursive)
{
    const grep_options_t * o = env->o;
    bool show_label = o->force_name ||
                      (!o->suppress_name &&
                       (total > 1U || from_recursive));
    struct stat sb;

    if (_grep_streq(spec, "-")) {
        _grep_process_stream(env, stdin, GREP_STDIN_LABEL, show_label);
        return; /* never close stdin */
    }

    if (stat(spec, &sb) != 0) {
        _grep_report_error(env, spec);
        return;
    }

    if (S_ISDIR(sb.st_mode)) {
        if (o->dirs == GREP_DIR_RECURSE) {
            _grep_walk_dir(env, spec, 0);
        }
        else if (o->dirs == GREP_DIR_SKIP) {
            return;
        }
        else {
            env->st->had_error = true;
            if (!o->no_msgs) {
                grep_eprintf("%s: %s: Is a directory\n",
                             GREP_PROGRAM_NAME, spec);
            }
        }
        return;
    }

    if (S_ISREG(sb.st_mode)) {
        _grep_process_regular(env, spec, spec, show_label);
        return;
    }

    /* FIFOs, character/block devices and sockets */
    if (o->devices == GREP_DEV_READ) {
        _grep_process_regular(env, spec, spec, show_label);
    }
}
