/**
 * @file awk.c
 * @brief Cross-platform AWK interpreter implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with a large subset of GNU Awk.
 *
 * Key design features:
 *   - Pattern-action rules with BEGIN/END blocks
 *   - Fields $0, $1..$NF and field splitting (-F / FS)
 *   - User and builtin variables, associative arrays
 *   - Arithmetic, string, and regex operations
 *   - Built-in functions: print/printf/sprintf, length, sub/gsub,
 *     match, split, index, substr, tolower/toupper, system, getline
 *   - Control flow: if/else, while, do-while, for, for-in, switch
 *   - User-defined functions with local scope
 *   - Range patterns, ENVIRON/PROCINFO, ARGC/ARGV
 *
 * Key behaviors:
 *   - -F fs:          set field separator
 *   - -f progfile:    read program from file
 *   - -v var=val:     assign variable before execution
 *   - --help:         show usage and exit
 *   - --version:      show version and exit
 *   - getline, system(), and file/stream I/O supported
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o awk.exe awk.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o awk awk.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o awk awk.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o awk awk.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o awk awk.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o awk awk.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/awk>
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
    #define AWK_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define AWK_PLATFORM_LINUX   1
    #define AWK_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define AWK_PLATFORM_MACOS   1
    #define AWK_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define AWK_PLATFORM_FREEBSD 1
    #define AWK_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define AWK_PLATFORM_OPENBSD 1
    #define AWK_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define AWK_PLATFORM_NETBSD  1
    #define AWK_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define AWK_PLATFORM_POSIX   1
#else
    #define AWK_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef AWK_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef AWK_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef AWK_PLATFORM_NETBSD
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
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef AWK_PLATFORM_WINDOWS
  #define AWK_TINY_REGEX 1
  #include <windows.h>
  #include <io.h>
  #include <fcntl.h>
  #include <process.h>
  #ifndef S_ISREG
  #define S_ISREG(m) (((m) & _S_IFREG) != 0)
  #endif
  #ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
  #endif
  #ifndef S_ISFIFO
  #define S_ISFIFO(m) 0
  #endif
  #ifndef getpid
  #define getpid _getpid
  #endif
  #ifndef strcasecmp
  #define strcasecmp _stricmp
  #endif
  #ifndef strncasecmp
  #define strncasecmp _strnicmp
  #endif
#else
  #include <unistd.h>
  #include <regex.h>
  #include <sys/wait.h>
  extern char **environ;
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define AWK_VERSION_STR "v1.0.0"

/** @brief Maximum input line length (bytes) */
#ifndef AWK_MAX_LINE
    #define AWK_MAX_LINE (1 << 20)
#endif

/** @brief Maximum number of fields per record */
#ifndef AWK_MAX_FIELDS
    #define AWK_MAX_FIELDS  4096
#endif

/** @brief Maximum number of global variables */
#ifndef AWK_MAX_VARS
    #define AWK_MAX_VARS    4096
#endif

/** @brief Maximum number of pattern-action rules */
#ifndef AWK_MAX_RULES
    #define AWK_MAX_RULES   4096
#endif

/** @brief Maximum number of associative arrays */
#ifndef AWK_MAX_ARRAYS
    #define AWK_MAX_ARRAYS  1024
#endif

/** @brief Maximum number of user-defined functions */
#ifndef AWK_MAX_FUNCS
    #define AWK_MAX_FUNCS   1024
#endif

/** @brief Maximum number of simultaneously open files */
#ifndef AWK_MAX_OPEN_FILES
    #define AWK_MAX_OPEN_FILES  128
#endif

/** @brief Maximum local-scope stack depth */
#ifndef AWK_SCOPE_MAX
    #define AWK_SCOPE_MAX   256
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for awk_printf / awk_fputs.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef awk_out_stream
    #define awk_out_stream stdout
#endif

/**
 * @brief Default error stream for awk_err_printf / awk_fputs.
 *        Defaults to libc @c stderr .
 */
#ifndef awk_err_stream
    #define awk_err_stream stderr
#endif

/**
 * @brief Formatted print to the output stream (printf-compatible).
 *        Result is cast to (void) so unused return values never warn.
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef awk_printf
    #define awk_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to the error stream (error messages).
 *        NULL-safe on the stream and requires an explicit format string.
 */
#ifndef awk_err_printf
    #define awk_err_printf(fmt, ...) \
        do { if (awk_err_stream) { (void)fprintf((awk_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally awk_out_stream)
 */
#ifndef awk_fputs
    #define awk_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      character to write
 * @param stream  stdio stream (normally awk_out_stream)
 */
#ifndef awk_fputc
    #define awk_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/**
 * @brief Write a single character to the output stream.
 *        Cast to unsigned char first so MSB-set values avoid UB in putchar.
 * @param ch  byte to write
 */
#ifndef awk_putchar
    #define awk_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/********************************
 *    typedefs
 ********************************/
/* Forward declarations of the core AWK value/container types. */
typedef struct awk_val_s awk_val_t;
typedef struct awk_array_s awk_array_t;
typedef struct awk_array_entry_s awk_array_entry_t;
typedef struct awk_func_s awk_func_t;
typedef struct awk_var_s awk_var_t;
typedef struct awk_scope_s awk_scope_t;

/********************************
 *    static prototypes
 ********************************/
static awk_var_t *_awk_vm_get(const char *name, int create);
static void       _awk_vm_set_s(const char *name, const char *val);
static void       _awk_vm_set_n(const char *name, double n);
static const char *_awk_vm_get_s(const char *name);
static double     _awk_vm_get_n(const char *name);
static awk_array_t *_awk_arr_find(const char *name, int create);
static awk_var_t *_awk_scope_find_var(awk_scope_t *sc, const char *name, int create);
static awk_array_t *_awk_scope_find_array_by_name(awk_scope_t *sc, const char *name, int create);

/********************************
 *    static variables
 ********************************/

/********************************
 *    static functions
 ********************************/

/* -------------------------- tiny regex (bundled) ------------------------ */
#ifdef AWK_TINY_REGEX

static void _awk_xdie(const char *fmt, ...);
static char *_awk_xstrdup(const char *s);
static void *_awk_xrealloc(void *p, size_t n);
static char *_awk_xstrndup(const char *s, size_t n);

#define AWK_REGSUB 16
typedef struct { long rm_so; long rm_eo; } awk_regmatch_t;

typedef enum {
    OP_LITERAL = 1,
    OP_CLASS,
    OP_DOT,
    OP_BOL, OP_EOL,
    OP_SPLIT_A,
    OP_JUMP,
    OP_MATCH,
    OP_SAVE,
    OP_QUANT_MINMAX
} re_op_t;

typedef struct {
    uint8_t  op;
    int32_t  operand;
    int32_t  operand2;
    uint8_t  lit;
    uint8_t  bitmap[32];
    int32_t  child_span;
} re_ins_t;

typedef struct awk_regex_s {
    re_ins_t *ins;
    int       nin;
    int       ncap;
    char     *pattern;
} awk_regex_t;

static int _awk_cc_test(const uint8_t bm[32], unsigned char c)
{
    return (bm[c >> 3] >> (c & 7)) & 1;
}
static void _awk_cc_set(uint8_t bm[32], unsigned char c)
{
    bm[c >> 3] |= (uint8_t)(1u << (c & 7));
}
static void _awk_cc_set_range(uint8_t bm[32], unsigned char a, unsigned char b)
{
    if (a > b) { unsigned char t = a; a = b; b = t; }
    for (unsigned c = a; c <= b; c++) _awk_cc_set(bm, (unsigned char)c);
}

typedef struct {
    const char *s;
    int pos, len;
    int ncap;
    re_ins_t *ins;
    int nin, cap;
} recomp_t;

static void _awk_add_ins(recomp_t *R, re_ins_t in)
{
    if (R->nin == R->cap) {
        R->cap = R->cap ? R->cap * 2 : 16;
        R->ins = (re_ins_t *)realloc(R->ins, sizeof(re_ins_t) * (size_t)R->cap);
    }
    R->ins[R->nin++] = in;
}
static int _awk_peek(recomp_t *R, int off)
{
    int p = R->pos + off;
    return (p >= 0 && p < R->len) ? (unsigned char)R->s[p] : -1;
}
static int _awk_eat(recomp_t *R)
{
    return R->pos < R->len ? (unsigned char)R->s[R->pos++] : -1;
}

static int _awk_re_parse_branch(recomp_t *R, int *out_start);
static int _awk_re_parse_atom(recomp_t *R, int *out_start);
static int _awk_re_parse_quant(recomp_t *R, int *out_start);

static int _awk_re_parse_class(recomp_t *R, re_ins_t *out)
{
    memset(out->bitmap, 0, sizeof out->bitmap);
    out->op = OP_CLASS; out->operand = 0; out->operand2 = 0;
    int neg = 0;
    int c;
    if ((c = _awk_peek(R, 0)) == '^') { neg = 1; _awk_eat(R); }
    int first = 1;
    while ((c = _awk_peek(R, 0)) != -1 && c != ']') {
        if (c == '\\') {
            _awk_eat(R);
            int e = _awk_eat(R);
            switch (e) {
                case 'd': _awk_cc_set_range(out->bitmap, '0', '9'); break;
                case 'D': { uint8_t bm[32]; memset(bm,0,32); _awk_cc_set_range(bm,'0','9');
                    for (int i=0;i<32;i++) out->bitmap[i] |= (uint8_t)~bm[i]; } break;
                case 'w': _awk_cc_set_range(out->bitmap,'a','z'); _awk_cc_set_range(out->bitmap,'A','Z');
                          _awk_cc_set_range(out->bitmap,'0','9'); _awk_cc_set(out->bitmap,'_'); break;
                case 'W': { uint8_t bm[32]; memset(bm,0,32);
                    _awk_cc_set_range(bm,'a','z'); _awk_cc_set_range(bm,'A','Z');
                    _awk_cc_set_range(bm,'0','9'); _awk_cc_set(bm,'_');
                    for (int i=0;i<32;i++) out->bitmap[i] |= (uint8_t)~bm[i]; } break;
                case 's': _awk_cc_set(out->bitmap,' '); _awk_cc_set(out->bitmap,'\t'); _awk_cc_set(out->bitmap,'\n');
                          _awk_cc_set(out->bitmap,'\r'); _awk_cc_set(out->bitmap,'\f'); _awk_cc_set(out->bitmap,'\v'); break;
                case 'S': { uint8_t bm[32]; memset(bm,0,32);
                    _awk_cc_set(bm,' '); _awk_cc_set(bm,'\t'); _awk_cc_set(bm,'\n');
                    _awk_cc_set(bm,'\r'); _awk_cc_set(bm,'\f'); _awk_cc_set(bm,'\v');
                    for (int i=0;i<32;i++) out->bitmap[i] |= (uint8_t)~bm[i]; } break;
                case 'r': _awk_cc_set(out->bitmap,'\r'); break;
                case 'n': _awk_cc_set(out->bitmap,'\n'); break;
                case 't': _awk_cc_set(out->bitmap,'\t'); break;
                case '0': _awk_cc_set(out->bitmap, 0);  break;
                case -1: return -1;
                default: _awk_cc_set(out->bitmap, (unsigned char)e); break;
            }
            first = 0;
            continue;
        }
        _awk_eat(R);
        int n = _awk_peek(R, 0);
        /* '-' is literal only when it appears as the very first character (or last, handled below) */
        int dash_is_literal = (first && c == '-');
        if (!dash_is_literal && n == '-' && _awk_peek(R, 1) != ']' && _awk_peek(R, 1) != -1) {
            _awk_eat(R);
            int c2 = _awk_eat(R);
            if (c2 == -1) return -1;
            _awk_cc_set_range(out->bitmap, (unsigned char)c, (unsigned char)c2);
        } else {
            _awk_cc_set(out->bitmap, (unsigned char)c);
        }
        first = 0;
    }
    if (c != ']') return -1;
    _awk_eat(R);
    if (neg) for (int i = 0; i < 32; i++) out->bitmap[i] = (uint8_t)~out->bitmap[i];
    return 0;
}

static int _awk_re_parse_quant(recomp_t *R, int *out_start)
{
    int atom_start = -1;
    if (_awk_re_parse_atom(R, &atom_start) != 0) return -1;
    int c = _awk_peek(R, 0);
    if (c == '*' || c == '+' || c == '?') {
        _awk_eat(R);
        int span = R->nin - atom_start;
        if (span <= 0) { *out_start = atom_start; return 0; }
        int grow = (c == '?') ? 1 : 2;
        re_ins_t *body = (re_ins_t *)malloc(sizeof(re_ins_t) * (size_t)span);
        memcpy(body, &R->ins[atom_start], sizeof(re_ins_t) * (size_t)span);
        while (grow-- > 0) {
            re_ins_t dummy; memset(&dummy, 0, sizeof dummy);
            dummy.op = OP_JUMP;
            dummy.operand = 1;
            _awk_add_ins(R, dummy);
        }
        int ngrow = (c == '?') ? 1 : 2;
        int tail = (R->nin - ngrow) - (atom_start + span);
        if (tail > 0) {
            memmove(&R->ins[atom_start + span + ngrow],
                    &R->ins[atom_start + span],
                    sizeof(re_ins_t) * (size_t)tail);
        }
        if (c == '*') {
            re_ins_t split, jump;
            memset(&split, 0, sizeof split); memset(&jump, 0, sizeof jump);
            split.op = OP_SPLIT_A;
            split.operand = span + 2;
            jump.op = OP_JUMP;
            jump.operand = -(span + 1);
            R->ins[atom_start] = split;
            memcpy(&R->ins[atom_start + 1], body, sizeof(re_ins_t) * (size_t)span);
            R->ins[atom_start + 1 + span] = jump;
        } else if (c == '+') {
            memcpy(&R->ins[atom_start], body, sizeof(re_ins_t) * (size_t)span);
            re_ins_t split, jump;
            memset(&split, 0, sizeof split); memset(&jump, 0, sizeof jump);
            split.op = OP_SPLIT_A;
            split.operand = 2;
            jump.op = OP_JUMP;
            jump.operand = -(span + 1);
            R->ins[atom_start + span] = split;
            R->ins[atom_start + span + 1] = jump;
        } else {
            re_ins_t split; memset(&split, 0, sizeof split);
            split.op = OP_SPLIT_A;
            split.operand = span + 1;
            R->ins[atom_start] = split;
            memcpy(&R->ins[atom_start + 1], body, sizeof(re_ins_t) * (size_t)span);
        }
        free(body);
        *out_start = atom_start;
        return 0;
    }
    if (c == '{') {
        _awk_eat(R);
        int mn = 0, mx = -1, have_comma = 0;
        while (isdigit(c = _awk_peek(R, 0))) { mn = mn * 10 + (c - '0'); _awk_eat(R); }
        if (_awk_peek(R, 0) == ',') { _awk_eat(R); have_comma = 1;
            while (isdigit(c = _awk_peek(R, 0))) { if (mx < 0) mx = 0; mx = mx * 10 + (c - '0'); _awk_eat(R); }
        }
        if (_awk_peek(R, 0) != '}') return -1;
        _awk_eat(R);
        if (!have_comma) mx = mn;
        int span = R->nin - atom_start;
        if (span <= 0) { *out_start = atom_start; return 0; }
        re_ins_t q; memset(&q, 0, sizeof q);
        q.op = OP_QUANT_MINMAX; q.operand = mn; q.operand2 = mx; q.child_span = span;
        re_ins_t dummy; memset(&dummy, 0, sizeof dummy); dummy.op = OP_JUMP; dummy.operand = 1;
        _awk_add_ins(R, dummy);
        memmove(&R->ins[atom_start + 1], &R->ins[atom_start], sizeof(re_ins_t) * (size_t)span);
        R->ins[atom_start] = q;
        *out_start = atom_start;
        return 0;
    }
    *out_start = atom_start;
    return 0;
}

static int _awk_re_parse_atom(recomp_t *R, int *out_start)
{
    int start = R->nin;
    int c = _awk_peek(R, 0);
    if (c == '(') {
        _awk_eat(R);
        R->ncap++;
        int cap_idx = R->ncap;
        re_ins_t save_begin; memset(&save_begin, 0, sizeof save_begin);
        save_begin.op = OP_SAVE; save_begin.operand = 2 * cap_idx - 1;
        _awk_add_ins(R, save_begin);
        int br_start = -1;
        if (_awk_re_parse_branch(R, &br_start) != 0) return -1;
        if (_awk_peek(R, 0) != ')') return -1;
        _awk_eat(R);
        re_ins_t save_end; memset(&save_end, 0, sizeof save_end);
        save_end.op = OP_SAVE; save_end.operand = 2 * cap_idx;
        _awk_add_ins(R, save_end);
        *out_start = start;
        return 0;
    }
    if (c == '^') { _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in); in.op = OP_BOL; _awk_add_ins(R, in);
        *out_start = start; return 0;
    }
    if (c == '$') { _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in); in.op = OP_EOL; _awk_add_ins(R, in);
        *out_start = start; return 0;
    }
    if (c == '.') { _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in); in.op = OP_DOT; _awk_add_ins(R, in);
        *out_start = start; return 0;
    }
    if (c == '[') {
        _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in);
        if (_awk_re_parse_class(R, &in) != 0) return -1;
        _awk_add_ins(R, in);
        *out_start = start; return 0;
    }
    if (c == '\\') {
        _awk_eat(R);
        int e = _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in);
        switch (e) {
            case 'd': in.op = OP_CLASS; _awk_cc_set_range(in.bitmap, '0', '9'); _awk_add_ins(R, in); *out_start = start; return 0;
            case 'D': in.op = OP_CLASS;
                for (int i = 0; i < 256; i++) if (i < '0' || i > '9') _awk_cc_set(in.bitmap, (unsigned char)i);
                _awk_add_ins(R, in); *out_start = start; return 0;
            case 'w': in.op = OP_CLASS; _awk_cc_set_range(in.bitmap,'a','z'); _awk_cc_set_range(in.bitmap,'A','Z');
                      _awk_cc_set_range(in.bitmap,'0','9'); _awk_cc_set(in.bitmap,'_'); _awk_add_ins(R, in); *out_start = start; return 0;
            case 'W': in.op = OP_CLASS; for (int i = 0; i < 256; i++) {
                          int ok = !((i>='a'&&i<='z') || (i>='A'&&i<='Z') || (i>='0'&&i<='9') || i=='_');
                          if (ok) _awk_cc_set(in.bitmap, (unsigned char)i);
                      } _awk_add_ins(R, in); *out_start = start; return 0;
            case 's': in.op = OP_CLASS;
                      _awk_cc_set(in.bitmap,' '); _awk_cc_set(in.bitmap,'\t'); _awk_cc_set(in.bitmap,'\n');
                      _awk_cc_set(in.bitmap,'\r'); _awk_cc_set(in.bitmap,'\f'); _awk_cc_set(in.bitmap,'\v');
                      _awk_add_ins(R, in); *out_start = start; return 0;
            case 'S': in.op = OP_CLASS; for (int i=0;i<256;i++) {
                          int sp = (i==' '||i=='\t'||i=='\n'||i=='\r'||i=='\f'||i=='\v');
                          if (!sp) _awk_cc_set(in.bitmap, (unsigned char)i);
                      } _awk_add_ins(R, in); *out_start = start; return 0;
            case 'b': case 'B':
                      in.op = OP_DOT; _awk_add_ins(R, in); *out_start = start; return 0;
            case 'r': in.op = OP_LITERAL; in.lit = '\r'; _awk_add_ins(R, in); *out_start = start; return 0;
            case 'n': in.op = OP_LITERAL; in.lit = '\n'; _awk_add_ins(R, in); *out_start = start; return 0;
            case 't': in.op = OP_LITERAL; in.lit = '\t'; _awk_add_ins(R, in); *out_start = start; return 0;
            case '0': in.op = OP_LITERAL; in.lit = 0;    _awk_add_ins(R, in); *out_start = start; return 0;
            case -1: return -1;
            default: in.op = OP_LITERAL; in.lit = (uint8_t)e; _awk_add_ins(R, in); *out_start = start; return 0;
        }
    }
    if (c != '|' && c != ')' && c != '*' && c != '+' && c != '?' && c != -1) {
        _awk_eat(R);
        re_ins_t in; memset(&in, 0, sizeof in); in.op = OP_LITERAL; in.lit = (uint8_t)c;
        _awk_add_ins(R, in);
        *out_start = start;
        return 0;
    }
    return -1;
}

static int _awk_re_parse_piece(recomp_t *R, int *out_start)
{
    int first_start = -1;
    if (_awk_re_parse_quant(R, &first_start) != 0) return -1;
    *out_start = first_start;
    while (1) {
        int c = _awk_peek(R, 0);
        if (c == '|' || c == ')' || c == -1) return 0;
        int ns = -1;
        if (_awk_re_parse_quant(R, &ns) != 0) return -1;
        (void)ns;
    }
}

static int _awk_re_parse_branch(recomp_t *R, int *out_start)
{
    int first = -1;
    if (_awk_re_parse_piece(R, &first) != 0) return -1;
    *out_start = first;
    while (_awk_peek(R, 0) == '|') {
        _awk_eat(R);
        int sp = R->nin;
        int alt_start = -1;
        if (_awk_re_parse_piece(R, &alt_start) != 0) return -1;
        int b_start = alt_start, b_end = R->nin - 1;
        int b_len = b_end - b_start + 1;
        re_ins_t *tmp = (re_ins_t *)malloc(sizeof(re_ins_t) * (size_t)b_len);
        memcpy(tmp, &R->ins[b_start], sizeof(re_ins_t) * (size_t)b_len);
        int new_nin = sp + 2 + b_len;
        R->ins = (re_ins_t *)realloc(R->ins, sizeof(re_ins_t) * (size_t)new_nin + 16);
        memcpy(&R->ins[sp + 2], tmp, sizeof(re_ins_t) * (size_t)b_len);
        free(tmp);
        re_ins_t split, jump; memset(&split, 0, sizeof split); memset(&jump, 0, sizeof jump);
        split.op = OP_SPLIT_A; split.operand = 2;
        jump.op  = OP_JUMP;    jump.operand  = b_len + 1;
        R->ins[sp] = split;
        R->ins[sp + 1] = jump;
        if (first < sp) {
            re_ins_t saved_split = R->ins[sp];
            memmove(&R->ins[first + 1], &R->ins[first], sizeof(re_ins_t) * (size_t)(sp - first));
            R->ins[first] = saved_split;
            R->ins[first].operand = (int32_t)((sp + 2) - first);
            R->ins[sp + 1].operand = (int32_t)(b_len + 1);
        }
        R->nin = new_nin;
        *out_start = first;
    }
    return 0;
}

static int awk_regcomp(awk_regex_t **out_re, const char *pat)
{
    recomp_t R; memset(&R, 0, sizeof R);
    R.s = pat; R.pos = 0; R.len = (int)strlen(pat); R.ncap = 0;
    int start = -1;
    {
        re_ins_t sv0; memset(&sv0, 0, sizeof sv0);
        sv0.op = OP_SAVE; sv0.operand = 0; _awk_add_ins(&R, sv0);
    }
    if (R.len != 0) {
        if (_awk_re_parse_branch(&R, &start) != 0) {
            free(R.ins); *out_re = NULL; return 1;
        }
    }
    {
        re_ins_t sv1; memset(&sv1, 0, sizeof sv1);
        sv1.op = OP_SAVE; sv1.operand = 1; _awk_add_ins(&R, sv1);
    }
    re_ins_t m; memset(&m, 0, sizeof m); m.op = OP_MATCH;
    _awk_add_ins(&R, m);
    awk_regex_t *r = (awk_regex_t *)calloc(1, sizeof(*r));
    r->ins = R.ins; r->nin = R.nin; r->ncap = R.ncap;
    r->pattern = _awk_xstrdup(pat);
    *out_re = r;
    return 0;
}

typedef struct {
    const awk_regex_t *re;
    const char *input;
    size_t      input_len;
    awk_regmatch_t *pm;
    size_t      nmatch;
    int         found;
} reexe_t;

static int _awk_run(reexe_t *E, int pc, long sp, long saves[20])
{
    const awk_regex_t *re = E->re;
    const char *s = E->input;
    size_t slen = E->input_len;
    if (pc < 0 || pc > re->nin) return 0;
    while (pc < re->nin) {
        re_ins_t *in = &re->ins[pc];
        switch (in->op) {
            case OP_MATCH: {
                if (E->nmatch > 0) {
                    size_t cap = (size_t)(re->ncap + 1);
                    if (cap > E->nmatch) cap = E->nmatch;
                    for (size_t i = 0; i < cap; i++) {
                        if (i == 0) {
                            E->pm[0].rm_so = saves[0]; E->pm[0].rm_eo = saves[1];
                        } else {
                            if (2 * (long)i < 20 && 2 * (long)i + 1 < 20) {
                                E->pm[i].rm_so = saves[2 * (long)i];
                                E->pm[i].rm_eo = saves[2 * (long)i + 1];
                            }
                        }
                    }
                }
                return 1;
            }
            case OP_SAVE: {
                int slot = in->operand;
                if (slot >= 0 && slot < 20) saves[slot] = sp;
                pc++;
                continue;
            }
            case OP_LITERAL: {
                if (sp < 0 || (size_t)sp >= slen) return 0;
                if ((unsigned char)s[sp] != in->lit) return 0;
                sp++; pc++;
                continue;
            }
            case OP_CLASS: {
                if (sp < 0 || (size_t)sp >= slen) return 0;
                if (!_awk_cc_test(in->bitmap, (unsigned char)s[sp])) return 0;
                sp++; pc++;
                continue;
            }
            case OP_DOT: {
                if (sp < 0 || (size_t)sp >= slen) return 0;
                if (s[sp] == '\n') return 0;
                sp++; pc++;
                continue;
            }
            case OP_BOL: {
                if (sp != 0) return 0;
                pc++; continue;
            }
            case OP_EOL: {
                if (!((size_t)sp == slen || (s[sp] == '\n' && (size_t)sp + 1 == slen))) return 0;
                pc++; continue;
            }
            case OP_JUMP: {
                pc += in->operand;
                continue;
            }
            case OP_SPLIT_A: {
                long saves_copy[20]; memcpy(saves_copy, saves, sizeof saves_copy);
                if (_awk_run(E, pc + 1, sp, saves_copy)) return 1;
                long saves_copy2[20]; memcpy(saves_copy2, saves, sizeof saves_copy2);
                return _awk_run(E, pc + in->operand, sp, saves_copy2);
            }
            case OP_QUANT_MINMAX: {
                int mn = in->operand;
                int mx = in->operand2;
                int span = in->child_span;
                int child_pc = pc + 1;
                long start_sp = sp;
                int max_attempts;
                if (mx < 0) max_attempts = (int)(slen - (size_t)sp) + mn + 2;
                else        max_attempts = mx;
                if (max_attempts < mn) { return 0; }
                int total_slots = (mx < 0) ? (max_attempts + 2) : (mx + 2);
                long *positions = (long *)calloc((size_t)total_slots, sizeof(long));
                if (!positions) return 0;
                positions[0] = sp;
                int attained = 0;
                long cur_sp = sp;
                struct frame_q { int pc; long ssp; long saves[20]; };
#define QCAP 32
                struct frame_q *qstack = (struct frame_q *)calloc(QCAP, sizeof(struct frame_q));
                if (!qstack) { free(positions); return 0; }
                for (int k = 1; k <= max_attempts; k++) {
                    long sv[20]; memcpy(sv, saves, sizeof sv);
                    int tos = 0;
                    int found_child = 0;
                    long end_sp = cur_sp;
                    qstack[tos].pc = child_pc;
                    qstack[tos].ssp = cur_sp;
                    memcpy(qstack[tos].saves, saves, 20 * sizeof(long));
                    tos++;
                    while (tos > 0 && !found_child) {
                        tos--;
                        int pcc = qstack[tos].pc;
                        long ssp = qstack[tos].ssp;
                        long svv[20]; memcpy(svv, qstack[tos].saves, 20 * sizeof(long));
                        int steps = 0;
                        int child_ok = 1;
                        while (pcc >= child_pc && pcc < child_pc + span) {
                            if (steps++ > 100000) { child_ok = 0; break; }
                            re_ins_t *ic = &re->ins[pcc];
                            switch (ic->op) {
                                case OP_LITERAL: if ((size_t)ssp >= slen || (unsigned char)s[ssp] != ic->lit) { child_ok = 0; goto sim_done_q; }
                                                 ssp++; pcc++; break;
                                case OP_CLASS: if ((size_t)ssp >= slen || !_awk_cc_test(ic->bitmap, (unsigned char)s[ssp])) { child_ok = 0; goto sim_done_q; }
                                                 ssp++; pcc++; break;
                                case OP_DOT: if ((size_t)ssp >= slen || s[ssp] == '\n') { child_ok = 0; goto sim_done_q; }
                                                 ssp++; pcc++; break;
                                case OP_BOL: if (ssp != 0) { child_ok = 0; goto sim_done_q; } pcc++; break;
                                case OP_EOL: if ((size_t)ssp != slen) { child_ok = 0; goto sim_done_q; } pcc++; break;
                                case OP_SAVE: if (ic->operand >= 0 && ic->operand < 20) svv[ic->operand] = ssp;
                                              pcc++; break;
                                case OP_QUANT_MINMAX: { child_ok = 0; goto sim_done_q; }
                                case OP_SPLIT_A: {
                                    if (tos < QCAP - 1) {
                                        qstack[tos].pc = pcc + ic->operand;
                                        qstack[tos].ssp = ssp;
                                        memcpy(qstack[tos].saves, svv, 20 * sizeof(long));
                                        tos++;
                                    }
                                    pcc++;
                                    break;
                                }
                                case OP_JUMP: pcc += ic->operand; break;
                                case OP_MATCH: child_ok = 1; goto sim_done_q;
                                default: pcc++; break;
                            }
                        }
                    sim_done_q:
                        if (child_ok) {
                            found_child = 1;
                            end_sp = ssp;
                            memcpy(saves, svv, 20 * sizeof(long));
                        }
                    }
                    (void)total_slots;
                    if (!found_child) break;
                    if (end_sp == cur_sp && k > mn + 2) {
                        attained = k - 1;
                        break;
                    }
                    cur_sp = end_sp;
                    positions[k] = cur_sp;
                    attained = k;
                    if (mx >= 0 && attained >= max_attempts) break;
                }
                free(qstack);
#undef QCAP
                int rest_pc = pc + 1 + span;
                int ok = 0;
                for (int k = attained; k >= mn; k--) {
                    long ss[20]; memcpy(ss, saves, sizeof ss);
                    if (k == 0) sp = start_sp;
                    else        sp = positions[k];
                    if (_awk_run(E, rest_pc, sp, ss)) { ok = 1; break; }
                }
                free(positions);
                return ok;
            }
            default:
                pc++;
        }
    }
    return 0;
}

static int awk_regexec(const awk_regex_t *re, const char *s, size_t nmatch, awk_regmatch_t *pm, int flags)
{
    (void)flags;
    reexe_t E; memset(&E, 0, sizeof E);
    E.re = re; E.input = s ? s : ""; E.input_len = strlen(E.input);
    E.pm = pm; E.nmatch = nmatch;
    for (size_t i = 0; i < nmatch; i++) { pm[i].rm_so = -1; pm[i].rm_eo = -1; }
    long saves[20];
    for (int i = 0; i < 20; i++) saves[i] = -1;
    if (nmatch > 0) { pm[0].rm_so = -1; pm[0].rm_eo = -1; }
    for (size_t i = 0; i <= E.input_len; i++) {
        long sv[20]; memcpy(sv, saves, sizeof sv);
        if (nmatch > 0) { pm[0].rm_so = (long)i; pm[0].rm_eo = (long)i; }
        sv[0] = (long)i; sv[1] = (long)i;
        if (_awk_run(&E, 0, (long)i, sv)) { return 0; }
    }
    return 1;
}

static void awk_regfree(awk_regex_t *re)
{
    if (!re) return;
    free(re->ins);
    free(re->pattern);
    free(re);
}

static size_t awk_regerror(int errcode, const awk_regex_t *re, char *buf, size_t n)
{
    (void)errcode; (void)re;
    const char *m = "invalid regular expression";
    size_t ml = strlen(m);
    if (buf && n > 0) {
        size_t k = (ml + 1 < n) ? (ml + 1) : n;
        memcpy(buf, m, k - 1);
        buf[k - 1] = 0;
    }
    return ml;
}

#else
typedef regex_t         awk_regex_t;
typedef regmatch_t      awk_regmatch_t;
static int awk_regcomp_wrap(awk_regex_t **out, const char *p)
{
    *out = (awk_regex_t *)calloc(1, sizeof(awk_regex_t));
    if (!*out) return 1;
    int rc = regcomp(*out, p, REG_EXTENDED);
    if (rc != 0) { free(*out); *out = NULL; return rc; }
    return 0;
}
#define awk_regcomp(r,p) awk_regcomp_wrap((r),(p))
#define awk_regexec(r,s,n,p,f) regexec((r),(s),(n),(p),(f))
#define awk_regfree(r)        regfree(r)
#define awk_regerror(e,r,b,n) regerror((e),(r),(b),(n))
#endif

/* -------------------------- core types --------------------------------- */

typedef enum {
    V_UNASSIGNED = 0,
    V_STR   = 1,
    V_NUM   = 2,
    V_REGEX = 3,
    V_ARRAY_REF = 4
} awk_vtype_t;

struct awk_val_s
{
    awk_vtype_t type;
    double      num;
    char       *s;
    size_t      s_len;
    awk_array_t *arr_ref;
};

/* -------------------------- utilities ---------------------------------- */
static void _awk_xdie(const char *fmt, ...);
static char *_awk_xstrdup(const char *s);
static void *_awk_xrealloc(void *p, size_t n);
static char *_awk_xstrndup(const char *s, size_t n);
static int  _awk_safe_copy(char *dst, const char *src, size_t dst_size);
static int  _awk_xstreq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }

static void _awk_xdie(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    awk_err_printf("%s", "awk: ");
    if (awk_err_stream) { (void)vfprintf(awk_err_stream, fmt, ap); }
    awk_err_printf("%s", "\n");
    va_end(ap);
    exit(2);
}

static char *_awk_xstrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) _awk_xdie("out of memory");
    memcpy(p, s, n);
    return p;
}

static void *_awk_xrealloc(void *p, size_t n)
{
    void *r = realloc(p, n);
    if (!r && n) _awk_xdie("out of memory");
    return r;
}

static char *_awk_xstrndup(const char *s, size_t n)
{
    char *r = (char *)malloc(n + 1);
    if (!r) _awk_xdie("out of memory");
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

/**
 * @brief Safely copy a NUL-terminated string into a fixed-size buffer.
 * @param dst       destination buffer
 * @param src       source string (may be NULL)
 * @param dst_size  size of the destination buffer in bytes
 * @return 0 on success, -1 if dst is NULL/empty, src is NULL, or truncation occurs
 * @note On failure dst[0] is set to '\0' (when dst is valid). Truncation is
 *       reported as an error; the caller decides whether to ignore it.
 */
static int _awk_safe_copy(char *dst, const char *src, size_t dst_size)
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

/* -------------------------- dynamic strings ---------------------------- */
typedef struct {
    char *data;
    size_t len, cap;
} dstr_t;
static void _awk_dstr_init(dstr_t *d) { d->len = 0; d->cap = 64; d->data = (char *)malloc(d->cap); if (d->data) d->data[0] = 0; }
static void _awk_dstr_feed(dstr_t *d, const void *p, size_t n)
{
    if (!d->data) { _awk_dstr_init(d); }
    if (d->len + n + 1 > d->cap) {
        while (d->len + n + 1 > d->cap) d->cap *= 2;
        d->data = (char *)_awk_xrealloc(d->data, d->cap);
    }
    memcpy(d->data + d->len, p, n);
    d->len += n;
    d->data[d->len] = 0;
}
static void _awk_dstr_puts(dstr_t *d, const char *s) { if (s) _awk_dstr_feed(d, s, strlen(s)); }
static void _awk_dstr_putc(dstr_t *d, char c) { _awk_dstr_feed(d, &c, 1); }
static void _awk_dstr_free(dstr_t *d) { if (d && d->data) { free(d->data); d->data = NULL; } d->len = d->cap = 0; }

/* -------------------------- associative arrays ------------------------- */
#define AWK_ARR_HASH_BUCKETS 512

struct awk_array_entry_s
{
    char   *key;
    awk_val_t value;
    struct awk_array_entry_s *next;
};

struct awk_array_s
{
    char   *name;
    awk_array_entry_t *buckets[AWK_ARR_HASH_BUCKETS];
    char  **keys;
    size_t  keys_count, keys_cap;
};

typedef struct {
    awk_array_t *a;
    size_t count, cap;
} awk_arrays_t;

/* -------------------------- user-defined functions ---------------------- */
struct awk_func_s
{
    char   *name;
    char  **params;
    int    *is_array_param;
    int     nparams;
    int     nlocals;
    char   *body;
    size_t  body_len;
    size_t  body_start;
};

typedef struct {
    awk_func_t *f;
    size_t count, cap;
} awk_funcs_t;

/* -------------------------- open file/pipe table ----------------------- */
typedef enum {
    AWK_FH_NONE = 0,
    AWK_FH_FILE_READ,
    AWK_FH_FILE_WRITE,
    AWK_FH_FILE_APPEND,
    AWK_FH_PIPE_READ,
    AWK_FH_PIPE_WRITE,
} awk_fh_kind_t;

typedef struct {
    char         *name;
    awk_fh_kind_t kind;
    FILE         *fp;
#ifdef AWK_PLATFORM_WINDOWS
    HANDLE        hproc;
    HANDLE        hRead;
    HANDLE        hWrite;
#else
    pid_t         pid;
#endif
} awk_fh_t;

/* -------------------------- variables ---------------------------------- */
struct awk_var_s
{
    char *name;
    char *s;
    double num;
    int    num_ok;
    size_t s_len;
    int    is_array;
    awk_array_t *arr;
};
typedef struct awk_var_s awk_var_t;

typedef struct {
    awk_var_t *v;
    size_t count, cap;
} awk_vm_t;

/* -------------------------- scope stack (for functions) ---------------- */
struct awk_scope_s
{
    awk_vm_t vars;
    awk_arrays_t arrays;
};
typedef struct awk_scope_s awk_scope_t;

/* -------------------------- value ops ---------------------------------- */

static awk_val_t _awk_v_init(void)
{
    awk_val_t v;
    v.type = V_UNASSIGNED;
    v.num  = 0.0;
    v.s    = NULL;
    v.s_len = 0;
    v.arr_ref = NULL;
    return v;
}

static void _awk_v_set_s(awk_val_t *v, const char *s);
static void _awk_v_set_n(awk_val_t *v, double n);
static void _awk_v_set_re(awk_val_t *v, const char *re);
static void _awk_v_set_arr(awk_val_t *v, awk_array_t *a);
static double _awk_v_get_n(awk_val_t *v);
static const char *_awk_v_get_s(awk_val_t *v);
static void _awk_v_clear(awk_val_t *v);
static void _awk_v_copy(awk_val_t *dst, const awk_val_t *src);

static void _awk_v_set_s(awk_val_t *v, const char *s)
{
    if (v->s) free(v->s);
    v->s = _awk_xstrdup(s ? s : "");
    v->s_len = v->s ? strlen(v->s) : 0;
    v->type = V_STR;
    char *endp = NULL;
    v->num = v->s ? strtod(v->s, &endp) : 0.0;
    if (endp && endp != v->s && *endp == '\0') { v->type = V_NUM; }
    v->arr_ref = NULL;
}

static void _awk_v_set_n(awk_val_t *v, double n)
{
    if (v->s) free(v->s);
    char buf[64];
    snprintf(buf, sizeof buf, "%.6g", n);
    v->s = _awk_xstrdup(buf);
    v->s_len = strlen(v->s);
    v->num = n;
    v->type = V_NUM;
    v->arr_ref = NULL;
}

static void _awk_v_set_re(awk_val_t *v, const char *re)
{
    if (v->s) free(v->s);
    v->s = _awk_xstrdup(re ? re : "");
    v->s_len = v->s ? strlen(v->s) : 0;
    v->num = 0.0;
    v->type = V_REGEX;
    v->arr_ref = NULL;
}

static void _awk_v_set_arr(awk_val_t *v, awk_array_t *a)
{
    if (v->s) free(v->s);
    v->s = NULL;
    v->s_len = 0;
    v->num = 0.0;
    v->type = V_ARRAY_REF;
    v->arr_ref = a;
}

static double _awk_v_get_n(awk_val_t *v)
{
    if (!v) return 0.0;
    if (v->type == V_NUM) return v->num;
    if (v->type == V_ARRAY_REF) return 0.0;
    if (!v->s) return 0.0;
    char *e = NULL;
    double r = strtod(v->s, &e);
    return (e && e != v->s) ? r : 0.0;
}

static const char *_awk_v_get_s(awk_val_t *v)
{
    if (!v) return "";
    if (v->type == V_ARRAY_REF) return "";
    if (v->s) return v->s;
    static __thread char buf[64];
    snprintf(buf, sizeof buf, "%.6g", v->num);
    return buf;
}

static void _awk_v_clear(awk_val_t *v)
{
    if (!v) return;
    if (v->s) { free(v->s); v->s = NULL; }
    v->num = 0;
    v->type = V_UNASSIGNED;
    v->s_len = 0;
    v->arr_ref = NULL;
}

static void _awk_v_copy(awk_val_t *dst, const awk_val_t *src)
{
    if (!dst || !src) return;
    _awk_v_clear(dst);
    if (src->type == V_ARRAY_REF) {
        dst->type = V_ARRAY_REF;
        dst->arr_ref = src->arr_ref;
    } else if (src->type == V_REGEX) {
        _awk_v_set_re(dst, src->s ? src->s : "");
    } else if (src->type == V_NUM) {
        _awk_v_set_n(dst, src->num);
    } else if (src->type == V_STR) {
        _awk_v_set_s(dst, src->s ? src->s : "");
    }
}

/* -------------------------- globals ------------------------------------ */

static awk_vm_t     G_vm;
static awk_arrays_t G_arrays;
static awk_funcs_t  G_funcs;
static awk_fh_t     G_fh_table[AWK_MAX_OPEN_FILES];
static int          G_seeded_rand;

static awk_scope_t  G_scope_stack[AWK_SCOPE_MAX];
static int          G_scope_sp = 0;

static char  *G_line      = NULL;
static size_t G_line_cap  = 0;
static size_t G_line_len  = 0;

static char **G_fields    = NULL;
static size_t G_fields_cap = 0, G_fields_count = 0;

static char   *G_OFS = NULL;
static char   *G_ORS = NULL;
static char   *G_RS  = NULL;
static char   *G_FS  = NULL;
static size_t G_NR = 0, G_FNR = 0;
static char   *G_FILENAME = NULL;
static int    G_exit = 0, G_exit_code = 0;
static int    G_next = 0, G_break = 0, G_cont = 0;
static int    G_nextfile = 0;
static char   *G_ERRNO = NULL;
static int    G_return = 0;
static awk_val_t G_return_value;

static int      G_argc = 0;
static char   **G_argv = NULL;
static int      G_argind = 0;

static int     *G_range_active = NULL;
static size_t   G_range_count = 0;
static size_t   G_range_cap = 0;

/* -------------------------- regex cache -------------------------------- */
#define AWK_REG_CACHE 256
typedef struct {
    char *pat;
    awk_regex_t *re;
    int    compiled;
    int    icase;
} awk_regex_entry_t;
static awk_regex_entry_t G_regcache[AWK_REG_CACHE];
static size_t            G_regcount = 0;

static int _awk_regex_match(const char *s, const char *pat, awk_regmatch_t *m_out, size_t nmatch);

static int _awk_re_cache_find_or_compile(const char *pat, awk_regex_t **out, int icase)
{
    for (size_t i = 0; i < G_regcount; i++) {
        if (G_regcache[i].pat && strcmp(G_regcache[i].pat, pat) == 0 && G_regcache[i].icase == icase) {
            *out = G_regcache[i].re;
            return 0;
        }
    }
    if (G_regcount >= AWK_REG_CACHE) return -1;
    G_regcache[G_regcount].pat = _awk_xstrdup(pat);
    G_regcache[G_regcount].icase = icase;
    if (icase) {
        dstr_t d; _awk_dstr_init(&d);
        for (const char *p = pat; *p; p++) {
            if (*p == '\\' && (p[1] == 'd' || p[1] == 'D' || p[1] == 'w' || p[1] == 'W' ||
                               p[1] == 's' || p[1] == 'S' || p[1] == 'b' || p[1] == 'B' ||
                               p[1] == 'r' || p[1] == 'n' || p[1] == 't' || p[1] == '0')) {
                _awk_dstr_putc(&d, *p); _awk_dstr_putc(&d, p[1]); p++;
            } else if (isalpha((unsigned char)*p)) {
                char lo = (char)tolower((unsigned char)*p);
                char hi = (char)toupper((unsigned char)*p);
                _awk_dstr_putc(&d, '['); _awk_dstr_putc(&d, lo); _awk_dstr_putc(&d, hi); _awk_dstr_putc(&d, ']');
            } else {
                _awk_dstr_putc(&d, *p);
            }
        }
        if (awk_regcomp(&G_regcache[G_regcount].re, d.data ? d.data : "") != 0) {
            char eb[128]; awk_regerror(1, NULL, eb, sizeof eb);
            _awk_dstr_free(&d);
            _awk_xdie("bad regex /%s/: %s", pat, eb);
        }
        _awk_dstr_free(&d);
    } else {
        if (awk_regcomp(&G_regcache[G_regcount].re, pat) != 0) {
            char eb[128]; awk_regerror(1, NULL, eb, sizeof eb);
            _awk_xdie("bad regex /%s/: %s", pat, eb);
        }
    }
    G_regcache[G_regcount].compiled = 1;
    *out = G_regcache[G_regcount].re;
    G_regcount++;
    return 0;
}

static int _awk_regex_match(const char *s, const char *pat, awk_regmatch_t *m_out, size_t nmatch)
{
    if (!pat) return 0;
    awk_regex_t *re = NULL;
    int icase = (int)_awk_vm_get_n("IGNORECASE") != 0;
    if (_awk_re_cache_find_or_compile(pat, &re, icase) != 0) return 0;
    awk_regmatch_t local[16];
    if (!m_out) { m_out = local; nmatch = 16; }
    if (nmatch == 0) { m_out = local; nmatch = 1; }
    int rc = awk_regexec(re, s ? s : "", nmatch, m_out, 0);
    if (rc == 0 && nmatch > 0) {
        _awk_vm_set_n("RSTART", (double)((int)m_out[0].rm_so + 1));
        _awk_vm_set_n("RLENGTH", (double)((int)(m_out[0].rm_eo - m_out[0].rm_so)));
    }
    return rc == 0;
}

/* -------------------------- parser / lexer ----------------------------- */

enum {
    TOK_EOF = 0,
    TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_SEMI,
    TOK_LBRACK, TOK_RBRACK,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN, TOK_ADDEQ, TOK_SUBEQ, TOK_MULEQ, TOK_DIVEQ, TOK_MODEQ,
    TOK_POW, TOK_POWEQ,
    TOK_INC, TOK_DEC,
    TOK_LT, TOK_LE, TOK_GT, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_MATCH, TOK_NMATCH,
    TOK_QUESTION, TOK_COLON, TOK_DOLLAR,
    TOK_BEGIN, TOK_END,
    TOK_PRINT, TOK_PRINTF,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_DO,
    TOK_BREAK, TOK_CONTINUE, TOK_NEXT, TOK_NEXTFILE,
    TOK_FUNCTION, TOK_RETURN, TOK_EXIT,
    TOK_DELETE,
    TOK_SWITCH, TOK_CASE, TOK_DEFAULT,
    TOK_IN, TOK_GETLINE,
    TOK_GTGT,
    TOK_PIPE, TOK_LESS,
    TOK_INT, TOK_FLOAT,
    TOK_STRING,
    TOK_REGEX,
    TOK_IDENT,
};

typedef struct {
    int    type;
    double num;
    char  *s;
    size_t s_len;
    int    line;
    size_t start;
} awk_tok_t;

typedef struct {
    const char *src;
    size_t      pos, end;
    int         line;
    int         peek_ok;
    awk_tok_t   peek;
    int         saw_newline; /* set when a newline was skipped before current token */
} awk_lex_t;

static void _awk_lex_error(awk_lex_t *l, const char *msg)
{
    _awk_xdie("%s at line %d", msg, l->line);
}

static int is_id_start(int c) { return isalpha(c) || c == '_'; }
static int is_id_cont(int c)  { return isalnum(c) || c == '_'; }

static void awk_lex_init(awk_lex_t *l, const char *src)
{
    l->src = src; l->pos = 0; l->end = strlen(src);
    l->line = 1; l->peek_ok = 0; l->saw_newline = 0;
    memset(&l->peek, 0, sizeof(l->peek));
}

static void _awk_lex_invalidate_peek(awk_lex_t *l)
{
    if (!l) return;
    if (l->peek_ok && l->peek.s) { free(l->peek.s); }
    memset(&l->peek, 0, sizeof(l->peek));
    l->peek_ok = 0;
}

static int _awk_ch(awk_lex_t *l, size_t off) { return (l->pos + off < l->end) ? (unsigned char)l->src[l->pos + off] : -1; }
static void _awk_adv(awk_lex_t *l, size_t n) { for (size_t i = 0; i < n; i++) if (_awk_ch(l, i) == '\n') l->line++; l->pos += n; }

static void _awk_tok_free(awk_tok_t *t) { if (t) { free(t->s); t->s = NULL; t->s_len = 0; } }

static int _awk_lex_next_raw(awk_lex_t *l, awk_tok_t *t)
{
    memset(t, 0, sizeof(*t));
    t->line = l->line;
    if (l->pos == 0 && (unsigned char)_awk_ch(l,0) == 0xEF &&
        (unsigned char)_awk_ch(l,1) == 0xBB &&
        (unsigned char)_awk_ch(l,2) == 0xBF) { _awk_adv(l, 3); }
    int nl = 0;
    while (l->pos < l->end) {
        int c = _awk_ch(l, 0);
        if (c == ' ' || c == '\t' || c == '\r') _awk_adv(l, 1);
        else if (c == '\n') { nl = 1; _awk_adv(l, 1); }
        else if (c == '#') {
            while (l->pos < l->end && _awk_ch(l, 0) != '\n') _awk_adv(l, 1);
        } else break;
    }
    l->saw_newline = nl;
    if (l->pos >= l->end) { t->type = TOK_EOF; t->start = l->pos; return 0; }
    int c = _awk_ch(l, 0);
    t->start = l->pos;

    if (c == '"') {
        _awk_adv(l, 1);
        dstr_t d; _awk_dstr_init(&d);
        while (l->pos < l->end && _awk_ch(l, 0) != '"') {
            int k = _awk_ch(l, 0);
            if (k == '\\') {
                _awk_adv(l, 1);
                int n = _awk_ch(l, 0);
                switch (n) {
                    case 'n':  _awk_dstr_putc(&d, '\n'); _awk_adv(l, 1); break;
                    case 't':  _awk_dstr_putc(&d, '\t'); _awk_adv(l, 1); break;
                    case 'r':  _awk_dstr_putc(&d, '\r'); _awk_adv(l, 1); break;
                    case 'a':  _awk_dstr_putc(&d, '\a'); _awk_adv(l, 1); break;
                    case 'b':  _awk_dstr_putc(&d, '\b'); _awk_adv(l, 1); break;
                    case 'f':  _awk_dstr_putc(&d, '\f'); _awk_adv(l, 1); break;
                    case 'v':  _awk_dstr_putc(&d, '\v'); _awk_adv(l, 1); break;
                    case '\\': _awk_dstr_putc(&d, '\\'); _awk_adv(l, 1); break;
                    case '"':  _awk_dstr_putc(&d, '"');  _awk_adv(l, 1); break;
                    case '/':  _awk_dstr_putc(&d, '/');  _awk_adv(l, 1); break;
                    case '?':  _awk_dstr_putc(&d, '?');  _awk_adv(l, 1); break;
                    case '\'': _awk_dstr_putc(&d, '\''); _awk_adv(l, 1); break;
                    case '0': case '1': case '2': case '3': {
                        int v = 0, cnt = 0;
                        while (cnt < 3 && _awk_ch(l, 0) >= '0' && _awk_ch(l, 0) <= '7') {
                            v = v * 8 + (_awk_ch(l, 0) - '0'); _awk_adv(l, 1); cnt++;
                        }
                        _awk_dstr_putc(&d, (char)(unsigned char)v);
                        break;
                    }
                    default:
                        if (n == 'x') {
                            _awk_adv(l, 1); int v = 0, ok = 0;
                            while (1) {
                                int h = _awk_ch(l, 0);
                                if (h >= '0' && h <= '9') { v = v * 16 + (h - '0'); _awk_adv(l, 1); ok = 1; }
                                else if (h >= 'a' && h <= 'f') { v = v * 16 + (h - 'a' + 10); _awk_adv(l, 1); ok = 1; }
                                else if (h >= 'A' && h <= 'F') { v = v * 16 + (h - 'A' + 10); _awk_adv(l, 1); ok = 1; }
                                else break;
                            }
                            if (ok) _awk_dstr_putc(&d, (char)(unsigned char)v);
                        } else {
                            _awk_dstr_putc(&d, (char)n); _awk_adv(l, 1);
                        }
                }
            } else {
                _awk_dstr_putc(&d, (char)k); _awk_adv(l, 1);
            }
        }
        if (l->pos >= l->end) { _awk_dstr_free(&d); _awk_lex_error(l, "unterminated string"); }
        _awk_adv(l, 1);
        t->type = TOK_STRING; t->s = d.data; t->s_len = d.len;
        return 0;
    }

    if (c == '/') {
        _awk_adv(l, 1);
        dstr_t d; _awk_dstr_init(&d);
        int bracketed = 0;
        while (l->pos < l->end) {
            int k = _awk_ch(l, 0);
            if (!bracketed && k == '/') break;
            if (k == '\n') break;
            if (k == '\\') {
                _awk_dstr_putc(&d, (char)k); _awk_adv(l, 1);
                if (l->pos < l->end) { _awk_dstr_putc(&d, (char)_awk_ch(l,0)); _awk_adv(l, 1); }
                continue;
            }
            if (k == '[') bracketed++;
            else if (k == ']' && bracketed > 0) bracketed--;
            _awk_dstr_putc(&d, (char)k); _awk_adv(l, 1);
        }
        if (l->pos < l->end && _awk_ch(l, 0) == '/') _awk_adv(l, 1);
        else { _awk_dstr_free(&d); _awk_lex_error(l, "unterminated regex literal"); }
        t->type = TOK_REGEX; t->s = d.data; t->s_len = d.len;
        return 0;
    }

    if (isdigit(c) || (c == '.' && isdigit(_awk_ch(l, 1)))) {
        dstr_t d; _awk_dstr_init(&d);
        int got_dot = 0, got_exp = 0;
        while (l->pos < l->end) {
            int k = _awk_ch(l, 0);
            if (isdigit(k)) { _awk_dstr_putc(&d, (char)k); _awk_adv(l, 1); }
            else if (k == '.' && !got_dot && !got_exp) { got_dot = 1; _awk_dstr_putc(&d, '.'); _awk_adv(l, 1); }
            else if ((k == 'e' || k == 'E') && !got_exp) {
                got_exp = 1; _awk_dstr_putc(&d, (char)k); _awk_adv(l, 1);
                if (_awk_ch(l,0) == '+' || _awk_ch(l,0) == '-') { _awk_dstr_putc(&d, (char)_awk_ch(l,0)); _awk_adv(l, 1); }
            } else break;
        }
        t->type = got_dot || got_exp ? TOK_FLOAT : TOK_INT;
        t->num = strtod(d.data, NULL);
        _awk_dstr_free(&d);
        return 0;
    }

    if (is_id_start(c)) {
        size_t start = l->pos;
        while (l->pos < l->end && is_id_cont(_awk_ch(l, 0))) _awk_adv(l, 1);
        size_t len = l->pos - start;
        char *id = _awk_xstrndup(l->src + start, len);
        if      (_awk_xstreq(id, "BEGIN"))    { t->type = TOK_BEGIN;    free(id); return 0; }
        else if (_awk_xstreq(id, "END"))      { t->type = TOK_END;      free(id); return 0; }
        else if (_awk_xstreq(id, "print"))    { t->type = TOK_PRINT;    free(id); return 0; }
        else if (_awk_xstreq(id, "printf"))   { t->type = TOK_PRINTF;   free(id); return 0; }
        else if (_awk_xstreq(id, "if"))       { t->type = TOK_IF;       free(id); return 0; }
        else if (_awk_xstreq(id, "else"))     { t->type = TOK_ELSE;     free(id); return 0; }
        else if (_awk_xstreq(id, "while"))    { t->type = TOK_WHILE;    free(id); return 0; }
        else if (_awk_xstreq(id, "for"))      { t->type = TOK_FOR;      free(id); return 0; }
        else if (_awk_xstreq(id, "do"))       { t->type = TOK_DO;       free(id); return 0; }
        else if (_awk_xstreq(id, "break"))    { t->type = TOK_BREAK;    free(id); return 0; }
        else if (_awk_xstreq(id, "continue")) { t->type = TOK_CONTINUE; free(id); return 0; }
        else if (_awk_xstreq(id, "next"))     { t->type = TOK_NEXT;     free(id); return 0; }
        else if (_awk_xstreq(id, "nextfile")) { t->type = TOK_NEXTFILE; free(id); return 0; }
        else if (_awk_xstreq(id, "function")) { t->type = TOK_FUNCTION; free(id); return 0; }
        else if (_awk_xstreq(id, "return"))   { t->type = TOK_RETURN;   free(id); return 0; }
        else if (_awk_xstreq(id, "exit"))     { t->type = TOK_EXIT;     free(id); return 0; }
        else if (_awk_xstreq(id, "delete"))   { t->type = TOK_DELETE;   free(id); return 0; }
        else if (_awk_xstreq(id, "switch"))   { t->type = TOK_SWITCH;   free(id); return 0; }
        else if (_awk_xstreq(id, "case"))     { t->type = TOK_CASE;     free(id); return 0; }
        else if (_awk_xstreq(id, "default"))  { t->type = TOK_DEFAULT;  free(id); return 0; }
        else if (_awk_xstreq(id, "in"))       { t->type = TOK_IN;       free(id); return 0; }
        else if (_awk_xstreq(id, "getline"))  { t->type = TOK_GETLINE;  free(id); return 0; }
        t->type = TOK_IDENT; t->s = id; t->s_len = len;
        return 0;
    }

#define TWO(a,b,ty)  if (_awk_ch(l,0)==(a) && _awk_ch(l,1)==(b)) { t->type=(ty); _awk_adv(l,2); return 0; }
    TWO('+','=',TOK_ADDEQ); TWO('-','=',TOK_SUBEQ); TWO('*','=',TOK_MULEQ);
    TWO('/','=',TOK_DIVEQ); TWO('%','=',TOK_MODEQ); TWO('^','=',TOK_POWEQ);
    TWO('+','+',TOK_INC);  TWO('-','-',TOK_DEC);
    TWO('=','=',TOK_EQ);   TWO('!','=',TOK_NEQ);
    TWO('<','=',TOK_LE);   TWO('>','=',TOK_GE);
    TWO('&','&',TOK_AND);  TWO('|','|',TOK_OR);
    TWO('!','~',TOK_NMATCH); TWO('>','>',TOK_GTGT);
#undef TWO
    if (_awk_ch(l, 0) == '~') { t->type = TOK_MATCH; _awk_adv(l, 1); return 0; }
    if (_awk_ch(l, 0) == '|') { t->type = TOK_PIPE;  _awk_adv(l, 1); return 0; }
    if (_awk_ch(l, 0) == '^') { t->type = TOK_POW;   _awk_adv(l, 1); return 0; }
    if (_awk_ch(l, 0) == '<') { t->type = TOK_LT;    _awk_adv(l, 1); return 0; }
    if (_awk_ch(l, 0) == '>') { t->type = TOK_GT;    _awk_adv(l, 1); return 0; }

    switch (c) {
        case '{': _awk_adv(l, 1); t->type = TOK_LBRACE; return 0;
        case '}': _awk_adv(l, 1); t->type = TOK_RBRACE; return 0;
        case '(': _awk_adv(l, 1); t->type = TOK_LPAREN; return 0;
        case ')': _awk_adv(l, 1); t->type = TOK_RPAREN; return 0;
        case '[': _awk_adv(l, 1); t->type = TOK_LBRACK; return 0;
        case ']': _awk_adv(l, 1); t->type = TOK_RBRACK; return 0;
        case ',': _awk_adv(l, 1); t->type = TOK_COMMA;  return 0;
        case ';': _awk_adv(l, 1); t->type = TOK_SEMI;   return 0;
        case '?': _awk_adv(l, 1); t->type = TOK_QUESTION; return 0;
        case ':': _awk_adv(l, 1); t->type = TOK_COLON;  return 0;
        case '$': _awk_adv(l, 1); t->type = TOK_DOLLAR; return 0;
        case '+': _awk_adv(l, 1); t->type = TOK_PLUS;   return 0;
        case '-': _awk_adv(l, 1); t->type = TOK_MINUS;  return 0;
        case '*': _awk_adv(l, 1); t->type = TOK_STAR;   return 0;
        case '%': _awk_adv(l, 1); t->type = TOK_PERCENT;return 0;
        case '!': _awk_adv(l, 1); t->type = TOK_NOT;    return 0;
        case '=': _awk_adv(l, 1); t->type = TOK_ASSIGN; return 0;
        case '/': _awk_adv(l, 1); t->type = TOK_SLASH;  return 0;
        default: break;
    }
    _awk_xdie("unexpected character '0x%02X' at line %d", (unsigned char)c, l->line);
    return -1;
}

static void _awk_lex_next(awk_lex_t *l, awk_tok_t *t)
{
    if (l->peek_ok) {
        if (l->peek.s) {
            t->s = l->peek.s; t->s_len = l->peek.s_len;
            l->peek.s = NULL; l->peek.s_len = 0;
        } else {
            t->s = NULL; t->s_len = 0;
        }
        t->type = l->peek.type; t->line = l->peek.line; t->start = l->peek.start;
        t->num = l->peek.num;
        l->peek_ok = 0;
        return;
    }
    _awk_lex_next_raw(l, t);
}

static void _awk_lex_peek(awk_lex_t *l, awk_tok_t *t)
{
    if (!l->peek_ok) { _awk_lex_next_raw(l, &l->peek); l->peek_ok = 1; }
    if (l->peek.s) {
        t->s = _awk_xstrdup(l->peek.s); t->s_len = l->peek.s_len;
    } else {
        t->s = NULL; t->s_len = 0;
    }
    t->type = l->peek.type; t->line = l->peek.line; t->start = l->peek.start;
    t->num = l->peek.num;
}

/* -------------------------- rule storage ------------------------------- */

typedef enum {
    RULE_PAT_ACTION = 0,
    RULE_BEGIN,
    RULE_END,
    RULE_RANGE
} awk_rule_kind_t;

typedef struct {
    awk_rule_kind_t kind;
    char *pattern1;
    char *pattern2;
    char *action;
    size_t action_len;
    size_t action_start;
} awk_rule_t;

static awk_rule_t *G_rules = NULL;
static size_t      G_rules_count = 0, G_rules_cap = 0;

static void _awk_rule_add(awk_rule_kind_t kind, const char *pat1, const char *pat2,
                     const char *act, size_t act_start, size_t act_len)
{
    if (G_rules_count == G_rules_cap) {
        G_rules_cap = G_rules_cap ? G_rules_cap * 2 : 32;
        G_rules = (awk_rule_t *)_awk_xrealloc(G_rules, G_rules_cap * sizeof(awk_rule_t));
    }
    awk_rule_t *r = &G_rules[G_rules_count++];
    r->kind = kind;
    r->pattern1 = pat1 ? _awk_xstrdup(pat1) : NULL;
    r->pattern2 = pat2 ? _awk_xstrdup(pat2) : NULL;
    r->action  = act ? _awk_xstrdup(act) : NULL;
    r->action_start = act_start;
    r->action_len = act_len;
}

/* -------------------------- expression / stmt evaluation --------------- */
typedef struct {
    awk_lex_t *lex;
} awk_eval_ctx_t;

static void _awk_eval_error(awk_eval_ctx_t *c, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    awk_err_printf("%s", "awk: ");
    if (awk_err_stream) { (void)vfprintf(awk_err_stream, fmt, ap); }
    awk_err_printf(" near line %d\n", (c && c->lex) ? c->lex->line : 0);
    va_end(ap);
    exit(2);
}

static void   _awk_eval_stmt_list(awk_eval_ctx_t *c);
static void   _awk_eval_stmt(awk_eval_ctx_t *c);
static void   _awk_eval_print(awk_eval_ctx_t *c, int is_printf);
static awk_val_t _awk_eval_expr(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_tern(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_or(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_and(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_not(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_cmp(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_add(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_concat(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_mul(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_pow(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_unary(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_postfix(awk_eval_ctx_t *c);
static awk_val_t _awk_eval_primary(awk_eval_ctx_t *c);

/* ------------------- scope / variable / array helpers ------------------ */

static awk_var_t *_awk_scope_find_var(awk_scope_t *sc, const char *name, int create)
{
    if (sc) {
        for (size_t i = 0; i < sc->vars.count; i++)
            if (strcmp(sc->vars.v[i].name, name) == 0) return &sc->vars.v[i];
    }
    for (size_t i = 0; i < G_vm.count; i++)
        if (strcmp(G_vm.v[i].name, name) == 0) return &G_vm.v[i];
    if (!create) return NULL;
    awk_vm_t *target = sc ? &sc->vars : &G_vm;
    if (target->count == target->cap) {
        target->cap = target->cap ? target->cap * 2 : 64;
        target->v = (awk_var_t *)_awk_xrealloc(target->v, target->cap * sizeof(awk_var_t));
    }
    awk_var_t *v = &target->v[target->count++];
    memset(v, 0, sizeof(*v));
    v->name = _awk_xstrdup(name);
    v->s = _awk_xstrdup(""); v->s_len = 0; v->num = 0.0; v->num_ok = 1;
    v->is_array = 0;
    v->arr = NULL;
    return v;
}

static awk_array_t *_awk_scope_find_array_by_name(awk_scope_t *sc, const char *name, int create)
{
    awk_var_t *vv = _awk_scope_find_var(sc, name, create);
    if (!vv) return NULL;
    if (vv->arr) return vv->arr;
    if (!create) return NULL;
    vv->is_array = 1;
    awk_array_t *a = (awk_array_t *)calloc(1, sizeof(*a));
    if (!a) _awk_xdie("out of memory");
    a->name = _awk_xstrdup(name);
    vv->arr = a;
    return a;
}

static awk_array_t *_awk_arr_find(const char *name, int create)
{
    awk_scope_t *sc = G_scope_sp > 0 ? &G_scope_stack[G_scope_sp - 1] : NULL;
    return _awk_scope_find_array_by_name(sc, name, create);
}

static size_t awk_hash_str(const char *s)
{
    size_t h = 1469598103934665603ULL;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= (size_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

static awk_array_entry_t *_awk_arr_entry_find(awk_array_t *a, const char *key)
{
    if (!a || !key) return NULL;
    size_t b = awk_hash_str(key) & (AWK_ARR_HASH_BUCKETS - 1);
    for (awk_array_entry_t *e = a->buckets[b]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

static void _awk_arr_keys_add(awk_array_t *a, const char *key)
{
    for (size_t i = 0; i < a->keys_count; i++)
        if (strcmp(a->keys[i], key) == 0) return;
    if (a->keys_count == a->keys_cap) {
        a->keys_cap = a->keys_cap ? a->keys_cap * 2 : 16;
        a->keys = (char **)_awk_xrealloc(a->keys, a->keys_cap * sizeof(char *));
    }
    a->keys[a->keys_count++] = _awk_xstrdup(key);
}

static void _awk_arr_keys_remove(awk_array_t *a, const char *key)
{
    size_t j = 0;
    for (size_t i = 0; i < a->keys_count; i++) {
        if (strcmp(a->keys[i], key) == 0) { free(a->keys[i]); continue; }
        a->keys[j++] = a->keys[i];
    }
    a->keys_count = j;
}

static awk_val_t *_awk_arr_get_val(awk_array_t *a, const char *key, int create)
{
    if (!a) return NULL;
    awk_array_entry_t *e = _awk_arr_entry_find(a, key);
    if (e) return &e->value;
    if (!create) return NULL;
    size_t b = awk_hash_str(key) & (AWK_ARR_HASH_BUCKETS - 1);
    e = (awk_array_entry_t *)calloc(1, sizeof(*e));
    if (!e) _awk_xdie("out of memory");
    e->key = _awk_xstrdup(key);
    e->value = _awk_v_init();
    e->next = a->buckets[b];
    a->buckets[b] = e;
    _awk_arr_keys_add(a, key);
    return &e->value;
}

static void _awk_arr_del_entry(awk_array_t *a, const char *key)
{
    if (!a) return;
    size_t b = awk_hash_str(key) & (AWK_ARR_HASH_BUCKETS - 1);
    awk_array_entry_t *prev = NULL, *cur = a->buckets[b];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            if (prev) prev->next = cur->next;
            else       a->buckets[b] = cur->next;
            _awk_v_clear(&cur->value);
            free(cur->key);
            free(cur);
            _awk_arr_keys_remove(a, key);
            return;
        }
        prev = cur; cur = cur->next;
    }
}

static void _awk_arr_clear(awk_array_t *a)
{
    if (!a) return;
    for (size_t bi = 0; bi < AWK_ARR_HASH_BUCKETS; bi++) {
        awk_array_entry_t *e = a->buckets[bi];
        while (e) { awk_array_entry_t *nx = e->next; _awk_v_clear(&e->value); free(e->key); free(e); e = nx; }
        a->buckets[bi] = NULL;
    }
    for (size_t i = 0; i < a->keys_count; i++) free(a->keys[i]);
    a->keys_count = 0;
}

static char *_awk_build_multi_key(awk_val_t *idxs, int nidxs)
{
    const char *subsep = _awk_vm_get_s("SUBSEP");
    if (!subsep || !*subsep) subsep = "\034";
    dstr_t d; _awk_dstr_init(&d);
    for (int i = 0; i < nidxs; i++) {
        if (i > 0) _awk_dstr_puts(&d, subsep);
        _awk_dstr_puts(&d, _awk_v_get_s(&idxs[i]));
    }
    char *r = _awk_xstrdup(d.data ? d.data : "");
    _awk_dstr_free(&d);
    return r;
}

static awk_func_t *_awk_func_find(const char *name)
{
    for (size_t i = 0; i < G_funcs.count; i++)
        if (strcmp(G_funcs.f[i].name, name) == 0) return &G_funcs.f[i];
    return NULL;
}

static awk_var_t *_awk_vm_get(const char *name, int create)
{
    awk_scope_t *sc = G_scope_sp > 0 ? &G_scope_stack[G_scope_sp - 1] : NULL;
    awk_var_t *v = _awk_scope_find_var(sc, name, create);
    if (getenv("AWK_DEBUG_VM")) {
        awk_err_printf("[vm_get] name=[%s] create=%d scope_sp=%d found=%d", name, create, G_scope_sp, v?1:0);
        if (v) awk_err_printf(" s=[%s] num=%g is_array=%d", v->s?v->s:"", v->num, v->is_array);
        awk_err_printf("%s", "\n");
    }
    return v;
}

static void _awk_vm_set_s(const char *name, const char *val)
{
    awk_var_t *v = _awk_vm_get(name, 1);
    free(v->s);
    v->s = _awk_xstrdup(val ? val : "");
    v->s_len = v->s ? strlen(v->s) : 0;
    char *e = NULL;
    v->num = v->s ? strtod(v->s, &e) : 0.0;
    v->num_ok = 1;
    v->is_array = 0;
}

static void _awk_vm_set_n(const char *name, double n)
{
    awk_var_t *v = _awk_vm_get(name, 1);
    free(v->s);
    char buf[64];
    snprintf(buf, sizeof buf, "%.6g", n);
    v->s = _awk_xstrdup(buf);
    v->s_len = strlen(v->s);
    v->num = n; v->num_ok = 1;
    v->is_array = 0;
}

static const char *_awk_vm_get_s(const char *name)
{
    awk_var_t *v = _awk_vm_get(name, 0);
    if (!v) return "";
    return v->s ? v->s : "";
}

static double _awk_vm_get_n(const char *name)
{
    awk_var_t *v = _awk_vm_get(name, 0);
    if (!v) return 0.0;
    return v->num;
}

static int _awk_v_true(awk_val_t *v)
{
    if (!v) return 0;
    if (v->type == V_NUM) return v->num != 0.0;
    if (v->type == V_ARRAY_REF) return !!v->arr_ref;
    if (v->type == V_REGEX) {
        const char *s0 = (G_fields && G_fields_count > 0) ? G_fields[0] : "";
        return _awk_regex_match(s0, v->s ? v->s : "", NULL, 0) ? 1 : 0;
    }
    const char *s = _awk_v_get_s(v);
    if (!s || *s == 0) return 0;
    double n = _awk_v_get_n(v);
    return n != 0.0 || strlen(s) > 0;
}

static int _awk_looks_numeric(const char *s)
{
    if (!s || !*s) return 0;
    char *e = NULL;
    strtod(s, &e);
    return (e && e != s && *e == 0);
}

/* -------------------------- file/pipe helpers -------------------------- */

static awk_fh_t *fh_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < AWK_MAX_OPEN_FILES; i++) {
        if (G_fh_table[i].name && strcmp(G_fh_table[i].name, name) == 0)
            return &G_fh_table[i];
    }
    return NULL;
}

static awk_fh_t *fh_alloc(const char *name)
{
    for (int i = 0; i < AWK_MAX_OPEN_FILES; i++) {
        if (G_fh_table[i].kind == AWK_FH_NONE) {
            G_fh_table[i].name = _awk_xstrdup(name);
            return &G_fh_table[i];
        }
    }
    return NULL;
}

static void fh_close(awk_fh_t *fh)
{
    if (!fh) return;
    if (fh->fp) {
#ifdef AWK_PLATFORM_WINDOWS
        if (fh->kind == AWK_FH_PIPE_READ || fh->kind == AWK_FH_PIPE_WRITE) {
            (void)_pclose(fh->fp);
        } else
#endif
        {
            (void)fclose(fh->fp);
        }
    }
#ifdef AWK_PLATFORM_WINDOWS
    if (fh->hproc && fh->hproc != INVALID_HANDLE_VALUE) {
        CloseHandle(fh->hproc);
    }
    if (fh->hRead && fh->hRead != INVALID_HANDLE_VALUE) CloseHandle(fh->hRead);
    if (fh->hWrite && fh->hWrite != INVALID_HANDLE_VALUE) CloseHandle(fh->hWrite);
#else
    if (fh->pid > 0) {
        int st = 0;
        (void)waitpid(fh->pid, &st, 0);
    }
#endif
    free(fh->name);
    memset(fh, 0, sizeof(*fh));
}

static int _awk_read_record_from(FILE *f)
{
    size_t len = 0;
    if (!G_line) { G_line_cap = 4096; G_line = (char *)malloc(G_line_cap); if (G_line) G_line[0] = 0; }
    if (!G_line) return 0;
    G_line[0] = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\r') continue;
        if (c == '\n') break;
        if (len + 2 > G_line_cap) {
            G_line_cap *= 2;
            G_line = (char *)_awk_xrealloc(G_line, G_line_cap);
        }
        G_line[len++] = (char)c;
    }
    G_line[len] = 0; G_line_len = len;
    return (len > 0 || c != EOF) ? 1 : 0;
}

/* -------------------------- field splitting ---------------------------- */

static void _awk_split_line(const char *line)
{
    for (size_t i = 0; i < G_fields_count; i++) free(G_fields[i]);
    G_fields_count = 0;
    if (G_fields_cap < 1) {
        G_fields_cap = 32;
        G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *));
    }
    G_fields[G_fields_count++] = _awk_xstrdup(line ? line : "");

    const char *FW = _awk_vm_get_s("FIELDWIDTHS");
    const char *FPAT = _awk_vm_get_s("FPAT");
    const char *FS = G_FS ? G_FS : _awk_vm_get_s("FS");

    if (FW && *FW) {
        int widths[256]; int nw = 0;
        const char *p = FW;
        while (*p && nw < 256) {
            while (*p == ' ') p++;
            if (!*p) break;
            int w = 0;
            while (isdigit((unsigned char)*p)) { w = w * 10 + (*p - '0'); p++; }
            if (w > 0) widths[nw++] = w;
            while (*p == ' ') p++;
        }
        const char *lp = line ? line : "";
        int pos = 0;
        for (int i = 0; i < nw; i++) {
            int w = widths[i];
            int llen = (int)strlen(lp);
            if (pos > llen) pos = llen;
            int end = pos + w;
            if (end > llen) end = llen;
            if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
            G_fields[G_fields_count++] = _awk_xstrndup(lp + pos, (size_t)(end - pos));
            pos = end;
        }
        return;
    }

    if (FPAT && *FPAT) {
        awk_regex_t *rx = NULL;
        if (awk_regcomp(&rx, FPAT) != 0) {
            if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
            G_fields[G_fields_count++] = _awk_xstrdup(line ? line : "");
            return;
        }
        const char *p = line ? line : "";
        awk_regmatch_t m[1];
        while (1) {
            if (awk_regexec(rx, p, 1, m, 0) != 0) break;
            int s = (int)m[0].rm_so, e = (int)m[0].rm_eo;
            if (e <= s) break;
            if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
            G_fields[G_fields_count++] = _awk_xstrndup(p + s, (size_t)(e - s));
            p += e;
        }
        awk_regfree(rx);
        return;
    }

    if (!FS || FS[0] == 0) FS = " ";

    if (_awk_xstreq(FS, " ")) {
        size_t i = 0, len = strlen(line ? line : "");
        const char *lp = line ? line : "";
        while (i < len && (lp[i] == ' ' || lp[i] == '\t')) i++;
        while (i < len) {
            size_t j = i;
            while (j < len && lp[j] != ' ' && lp[j] != '\t') j++;
            if (G_fields_count == G_fields_cap) {
                G_fields_cap *= 2;
                G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *));
            }
            G_fields[G_fields_count++] = _awk_xstrndup(lp + i, j - i);
            i = j;
            while (i < len && (lp[i] == ' ' || lp[i] == '\t')) i++;
        }
        return;
    }

    const char *lp = line ? line : "";
    size_t llen = strlen(lp);
    if (FS[0] && FS[1] == 0 && !strchr("[]()^$.*+?|\\", FS[0])) {
        char sep = FS[0];
        size_t i = 0, start = 0;
        while (1) {
            if (lp[i] == sep || lp[i] == 0) {
                if (G_fields_count == G_fields_cap) {
                    G_fields_cap *= 2;
                    G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *));
                }
                G_fields[G_fields_count++] = _awk_xstrndup(lp + start, i - start);
                if (lp[i] == 0) break;
                i++; start = i; continue;
            }
            i++;
            if (i > llen) break;
        }
        return;
    }

    awk_regex_t *rx;
    if (awk_regcomp(&rx, FS) != 0) {
        if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
        G_fields[G_fields_count++] = _awk_xstrdup(lp);
        return;
    }
    awk_regmatch_t m[1];
    const char *p = lp;
    while (1) {
        if (awk_regexec(rx, p, 1, m, 0) != 0) {
            if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
            G_fields[G_fields_count++] = _awk_xstrdup(p);
            break;
        }
        int s = (int)m[0].rm_so, e = (int)m[0].rm_eo;
        if (s == 0 && e == 0 && *p == 0) break;
        if (s == 0 && e == 0) {
            char ch2[2] = { *p, 0 };
            if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
            G_fields[G_fields_count++] = _awk_xstrdup(ch2);
            p++;
            continue;
        }
        if (G_fields_count == G_fields_cap) { G_fields_cap *= 2; G_fields = (char **)_awk_xrealloc(G_fields, G_fields_cap * sizeof(char *)); }
        G_fields[G_fields_count++] = _awk_xstrndup(p, (size_t)s);
        p += e;
    }
    awk_regfree(rx);
}

static void _awk_get_field(int idx, awk_val_t *out)
{
    if (idx < 0) { _awk_v_set_s(out, ""); return; }
    if ((size_t)idx >= G_fields_count) { _awk_v_set_s(out, ""); return; }
    _awk_v_set_s(out, G_fields[idx] ? G_fields[idx] : "");
}

/* -------------------------- skip helpers (for control flow) ------------ */
static void _awk_skip_stmt(awk_eval_ctx_t *c);

static void _awk_skip_block(awk_eval_ctx_t *c)
{
    awk_tok_t sk; _awk_lex_next(c->lex, &sk);
    if (sk.type != TOK_LBRACE) { _awk_tok_free(&sk); _awk_skip_stmt(c); return; }
    _awk_tok_free(&sk);
    int depth = 1;
    while (depth > 0) {
        awk_tok_t s2; _awk_lex_next(c->lex, &s2);
        if (s2.type == TOK_LBRACE) depth++;
        else if (s2.type == TOK_RBRACE) depth--;
        else if (s2.type == TOK_EOF) _awk_eval_error(c, "unterminated block in skip");
        _awk_tok_free(&s2);
    }
}

static void _awk_skip_stmt(awk_eval_ctx_t *c)
{
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_LBRACE) { _awk_skip_block(c); return; }
    if (t.type == TOK_IF || t.type == TOK_WHILE || t.type == TOK_DO ||
        t.type == TOK_FOR || t.type == TOK_SWITCH) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        if (t.type == TOK_DO) {
            _awk_skip_stmt(c);
            awk_tok_t w; _awk_lex_peek(c->lex, &w);
            if (w.type == TOK_WHILE) {
                _awk_lex_next(c->lex, &w); _awk_tok_free(&w);
                awk_tok_t p; _awk_lex_next(c->lex, &p);
                if (p.type == TOK_LPAREN) { _awk_tok_free(&p); int par = 1;
                    while (par > 0) { awk_tok_t k; _awk_lex_next(c->lex, &k);
                        if (k.type == TOK_LPAREN) par++;
                        else if (k.type == TOK_RPAREN) par--;
                        _awk_tok_free(&k);
                    }
                } else _awk_tok_free(&p);
            }
            return;
        }
        awk_tok_t p; _awk_lex_next(c->lex, &p);
        if (p.type == TOK_LPAREN) { _awk_tok_free(&p); int par = 1;
            while (par > 0) { awk_tok_t k; _awk_lex_next(c->lex, &k);
                if (k.type == TOK_LPAREN) par++;
                else if (k.type == TOK_RPAREN) par--;
                else if (k.type == TOK_EOF) _awk_eval_error(c, "unterminated cond");
                _awk_tok_free(&k);
            }
        } else _awk_tok_free(&p);
        _awk_skip_stmt(c);
        if (t.type == TOK_IF) {
            awk_tok_t e; _awk_lex_peek(c->lex, &e);
            if (e.type == TOK_ELSE) { _awk_lex_next(c->lex, &e); _awk_tok_free(&e); _awk_skip_stmt(c); }
        }
        return;
    }
    int par = 0;
    for (;;) {
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_SEMI || t.type == TOK_EOF) { _awk_tok_free(&t); return; }
        if (t.type == TOK_RBRACE && par == 0) { _awk_tok_free(&t); return; }
        if (t.type == TOK_LPAREN) par++;
        else if (t.type == TOK_RPAREN && par > 0) par--;
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
    }
}

static void _awk_run_block_or_stmt(awk_eval_ctx_t *c, int run)
{
    awk_tok_t lbt; _awk_lex_peek(c->lex, &lbt);
    if (lbt.type == TOK_LBRACE) {
        _awk_lex_next(c->lex, &lbt); _awk_tok_free(&lbt);
        if (run) _awk_eval_stmt_list(c);
        else {
            int depth = 1;
            while (depth > 0) {
                awk_tok_t sk; _awk_lex_next(c->lex, &sk);
                if (sk.type == TOK_LBRACE) depth++;
                else if (sk.type == TOK_RBRACE) depth--;
                else if (sk.type == TOK_EOF) _awk_eval_error(c, "unterminated block");
                _awk_tok_free(&sk);
            }
        }
    } else {
        if (run) _awk_eval_stmt(c);
        else _awk_skip_stmt(c);
    }
}

/* -------------------------- forward decl for builtins ------------------ */
static awk_val_t _awk_call_builtin(awk_eval_ctx_t *c, const char *name);
static awk_val_t _awk_call_user_function(awk_eval_ctx_t *c, awk_func_t *fn);
static double awk_rand(void);

/* -------------------------- print redirection helpers ------------------ */

static void _awk_do_format_one(dstr_t *out, const char *spec, awk_val_t *arg);

static void _awk_eval_print(awk_eval_ctx_t *c, int is_printf)
{
    awk_tok_t t;
    _awk_lex_peek(c->lex, &t);
    int has_paren = 0;
    if (t.type == TOK_LPAREN) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); has_paren = 1; }

    awk_val_t fmt = _awk_v_init();
    awk_val_t args[256]; int ac = 0;
    FILE *outf = stdout;
    awk_fh_t *fh = NULL;

    if (is_printf) {
        fmt = _awk_eval_expr(c);
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_COMMA) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); }
    }

    int first = 1;
    const char *ofs = _awk_vm_get_s("OFS");
    const char *ors = _awk_vm_get_s("ORS");
    dstr_t linebuf; _awk_dstr_init(&linebuf);

    if (!is_printf) {
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_RBRACE || t.type == TOK_SEMI || t.type == TOK_EOF || (has_paren && t.type == TOK_RPAREN)) {
            /* no args: $0 will be output in the formatting section below */
        } else {
            while (1) {
                if (ac >= 256) _awk_eval_error(c, "too many print args");
                args[ac++] = _awk_eval_expr(c);
                _awk_lex_peek(c->lex, &t);
                if (t.type == TOK_COMMA) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); continue; }
                break;
            }
        }
    } else {
        while (1) {
            _awk_lex_peek(c->lex, &t);
            if (t.type == TOK_COMMA) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); continue; }
            if (t.type == TOK_RBRACE || t.type == TOK_SEMI || t.type == TOK_EOF) break;
            if (has_paren && t.type == TOK_RPAREN) break;
            if (ac >= 256) _awk_eval_error(c, "too many printf args");
            args[ac++] = _awk_eval_expr(c);
        }
    }

    _awk_lex_peek(c->lex, &t);
    int redirect = 0;
    if (t.type == TOK_GT || t.type == TOK_GTGT || t.type == TOK_PIPE) {
        redirect = 1;
        int rtype = t.type;
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t rv = _awk_eval_expr(c);
        const char *target = _awk_v_get_s(&rv);
        fh = fh_find(target);
        if (rtype == TOK_PIPE) {
            if (fh && fh->kind == AWK_FH_PIPE_WRITE) outf = fh->fp;
            else {
#ifdef AWK_PLATFORM_WINDOWS
                outf = _popen(target, "w");
#else
                int pipefd[2];
                if (pipe(pipefd) == 0) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        close(pipefd[1]);
                        dup2(pipefd[0], 0);
                        close(pipefd[0]);
                        execl("/bin/sh", "sh", "-c", target, (char *)NULL);
                        _exit(127);
                    } else if (pid > 0) {
                        close(pipefd[0]);
                        outf = fdopen(pipefd[1], "w");
                        fh = fh_alloc(target);
                        if (fh) {
                            fh->kind = AWK_FH_PIPE_WRITE;
                            fh->fp = outf;
                            fh->pid = pid;
                        }
                    }
                }
#endif
                if (outf && !fh) {
                    fh = fh_alloc(target);
                    if (fh) { fh->kind = AWK_FH_PIPE_WRITE; fh->fp = outf; }
                }
            }
        } else {
            int append = (rtype == TOK_GTGT);
            if (fh) {
                if ((append && fh->kind == AWK_FH_FILE_APPEND) ||
                    (!append && fh->kind == AWK_FH_FILE_WRITE))
                    outf = fh->fp;
            }
            if (!outf || outf == stdout) {
                outf = fopen(target, append ? "a" : "w");
                if (outf) {
                    if (!fh) {
                        fh = fh_alloc(target);
                        if (fh) {
                            fh->kind = append ? AWK_FH_FILE_APPEND : AWK_FH_FILE_WRITE;
                            fh->fp = outf;
                        }
                    }
                }
            }
        }
        _awk_v_clear(&rv);
    }

    if (is_printf) {
        const char *fs = _awk_v_get_s(&fmt);
        int ai = 0;
        for (const char *p = fs; *p; ) {
            if (*p == '%') {
                p++;
                if (*p == '%') { _awk_dstr_putc(&linebuf, '%'); p++; continue; }
                char spec[64]; int si = 0;
                spec[si++] = '%';
                while (*p && (strchr("-+ #0", *p) || isdigit((unsigned char)*p) || *p == '.' || *p == 'l' || *p == 'h' || *p == 'z' || *p == 'j' || *p == 't')) {
                    if (si < 62) spec[si++] = *p;
                    p++;
                }
                if (*p) { if (si < 62) spec[si++] = *p; p++; }
                spec[si] = 0;
                awk_val_t dummy; memset(&dummy, 0, sizeof dummy);
                awk_val_t *a = (ai < ac) ? &args[ai++] : &dummy;
                _awk_do_format_one(&linebuf, spec, a);
            } else {
                _awk_dstr_putc(&linebuf, *p);
                p++;
            }
        }
    } else {
        if (!ac) {
            if (G_fields_count > 0) {
                _awk_dstr_puts(&linebuf, G_fields[0] ? G_fields[0] : "");
            }
        } else {
            for (int i = 0; i < ac; i++) {
                if (!first) _awk_dstr_puts(&linebuf, ofs);
                first = 0;
                _awk_dstr_puts(&linebuf, _awk_v_get_s(&args[i]));
            }
        }
        _awk_dstr_puts(&linebuf, ors);
    }

    if (outf) {
        awk_fputs(linebuf.data ? linebuf.data : "", outf);
        if (!redirect) (void)fflush(outf);
    }

    for (int i = 0; i < ac; i++) _awk_v_clear(&args[i]);
    _awk_v_clear(&fmt);
    _awk_dstr_free(&linebuf);
    (void)first;
}

static void _awk_do_format_one(dstr_t *out, const char *spec, awk_val_t *arg)
{
    char type = 's';
    for (const char *p = spec; *p; p++) {
        if (strchr("diouxXeEfFgGaAcCsSpn%", *p)) { type = *p; break; }
    }
    char tmp[512];
    switch (type) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
            long long v = (long long)_awk_v_get_n(arg);
            (void)snprintf(tmp, sizeof tmp, spec, v);
            _awk_dstr_puts(out, tmp);
            break;
        }
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
        case 'a': case 'A': {
            double v = _awk_v_get_n(arg);
            (void)snprintf(tmp, sizeof tmp, spec, v);
            _awk_dstr_puts(out, tmp);
            break;
        }
        case 'c': {
            int ch = (int)_awk_v_get_n(arg);
            char cc[2] = { (char)ch, 0 };
            _awk_dstr_puts(out, cc);
            break;
        }
        case 's': default: {
            const char *s = _awk_v_get_s(arg);
            char nspec[64];
            const char *star = spec;
            while (*star) star++;
            if (star > spec && *(star-1) == 's') {
                (void)_awk_safe_copy(nspec, spec, sizeof nspec);
                if (strchr(spec, '.') || strchr(spec, '-')) {
                    (void)snprintf(tmp, sizeof tmp, nspec, s ? s : "");
                    _awk_dstr_puts(out, tmp);
                } else {
                    _awk_dstr_puts(out, s ? s : "");
                }
            } else {
                _awk_dstr_puts(out, s ? s : "");
            }
            break;
        }
    }
}

/* -------------------------- eval: getline helper ----------------------- */
static awk_val_t _awk_eval_getline(awk_eval_ctx_t *c, const char *var_name)
{
    awk_val_t r = _awk_v_init();
    awk_tok_t t;
    _awk_lex_peek(c->lex, &t);
    int got_input = 0;
    FILE *src = NULL;
    awk_fh_t *fh = NULL;

    if (t.type == TOK_LT) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t fv = _awk_eval_expr(c);
        const char *fn = _awk_v_get_s(&fv);
        fh = fh_find(fn);
        if (fh) {
            if (fh->kind == AWK_FH_FILE_READ) src = fh->fp;
        }
        if (!src) {
            src = fopen(fn, "r");
            if (src) {
                fh = fh_alloc(fn);
                if (fh) { fh->kind = AWK_FH_FILE_READ; fh->fp = src; }
            }
        }
        if (!src) {
            if (G_ERRNO) free(G_ERRNO);
            G_ERRNO = _awk_xstrdup(fn ? fn : "");
            _awk_v_set_n(&r, -1.0);
            _awk_v_clear(&fv);
            return r;
        }
        _awk_v_clear(&fv);
    } else if (t.type == TOK_PIPE) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t cv = _awk_eval_expr(c);
        const char *cmd = _awk_v_get_s(&cv);
        fh = fh_find(cmd);
        if (fh && fh->kind == AWK_FH_PIPE_READ) src = fh->fp;
        if (!src) {
#ifdef AWK_PLATFORM_WINDOWS
            src = _popen(cmd, "r");
#else
            int pipefd[2];
            if (pipe(pipefd) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    close(pipefd[0]);
                    dup2(pipefd[1], 1);
                    close(pipefd[1]);
                    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                    _exit(127);
                } else if (pid > 0) {
                    close(pipefd[1]);
                    src = fdopen(pipefd[0], "r");
                    fh = fh_alloc(cmd);
                    if (fh) {
                        fh->kind = AWK_FH_PIPE_READ;
                        fh->fp = src;
                        fh->pid = pid;
                    }
                }
            }
#endif
            if (src) {
                if (!fh) {
                    fh = fh_alloc(cmd);
                    if (fh) { fh->kind = AWK_FH_PIPE_READ; fh->fp = src; }
                }
            }
        }
        if (!src) {
            _awk_v_set_n(&r, -1.0);
            _awk_v_clear(&cv);
            return r;
        }
        _awk_v_clear(&cv);
    }

    if (src) {
        got_input = _awk_read_record_from(src);
        if (!got_input && feof(src)) { _awk_v_set_n(&r, 0.0); return r; }
        if (!got_input) { _awk_v_set_n(&r, -1.0); return r; }
    } else {
        got_input = _awk_read_record_from(stdin);
        if (got_input) {
            G_NR++;
            G_FNR++;
        } else if (feof(stdin)) {
            _awk_v_set_n(&r, 0.0);
            return r;
        } else {
            _awk_v_set_n(&r, -1.0);
            return r;
        }
    }

    if (var_name) {
        _awk_vm_set_s(var_name, G_line);
    } else {
        _awk_split_line(G_line);
    }
    _awk_v_set_n(&r, 1.0);
    return r;
}

/* -------------------------- eval: delete / switch ---------------------- */
static void _awk_eval_delete(awk_eval_ctx_t *c)
{
    awk_tok_t t;
    _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
    _awk_lex_next(c->lex, &t);
    if (t.type != TOK_IDENT) _awk_eval_error(c, "delete: expected array name");
    char *aname = _awk_xstrdup(t.s ? t.s : "");
    _awk_tok_free(&t);
    awk_tok_t lb; _awk_lex_peek(c->lex, &lb);
    if (lb.type == TOK_LBRACK) {
        _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb);
        awk_val_t idxs[64]; int nidx = 0;
        while (1) {
            _awk_lex_peek(c->lex, &lb);
            if (lb.type == TOK_RBRACK) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); break; }
            if (nidx >= 64) _awk_eval_error(c, "too many dimensions");
            idxs[nidx++] = _awk_eval_expr(c);
            _awk_lex_peek(c->lex, &lb);
            if (lb.type == TOK_COMMA) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); continue; }
            if (lb.type == TOK_RBRACK) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); }
            break;
        }
        awk_array_t *a = _awk_arr_find(aname, 0);
        if (nidx == 1 && a) {
            _awk_arr_del_entry(a, _awk_v_get_s(&idxs[0]));
        } else if (a) {
            char *mk = _awk_build_multi_key(idxs, nidx);
            _awk_arr_del_entry(a, mk);
            free(mk);
        }
        for (int i = 0; i < nidx; i++) _awk_v_clear(&idxs[i]);
    } else {
        awk_array_t *a = _awk_arr_find(aname, 0);
        if (a) _awk_arr_clear(a);
    }
    free(aname);
}

static void _awk_eval_switch(awk_eval_ctx_t *c)
{
    awk_tok_t dummy; _awk_lex_next(c->lex, &dummy); _awk_tok_free(&dummy);
    awk_tok_t lp; _awk_lex_next(c->lex, &lp);
    if (lp.type != TOK_LPAREN) _awk_eval_error(c, "expected '(' after switch");
    _awk_tok_free(&lp);
    awk_val_t expr = _awk_eval_expr(c);
    awk_tok_t rp; _awk_lex_next(c->lex, &rp);
    if (rp.type != TOK_RPAREN) _awk_eval_error(c, "expected ')' after switch expr");
    _awk_tok_free(&rp);

    awk_tok_t lb; _awk_lex_next(c->lex, &lb);
    if (lb.type != TOK_LBRACE) _awk_eval_error(c, "expected '{' after switch");
    _awk_tok_free(&lb);

    size_t saved_pos = c->lex->pos;
    int matched = 0;
    int break_flag = 0;
    int done = 0;

    while (!done) {
        c->lex->pos = saved_pos; _awk_lex_invalidate_peek(c->lex);
        awk_tok_t t2;
        int found_case = 0;
        while (1) {
            _awk_lex_peek(c->lex, &t2);
            if (t2.type == TOK_RBRACE || t2.type == TOK_EOF) { done = 1; _awk_tok_free(&t2); break; }
            if (t2.type == TOK_CASE) {
                found_case = 1;
                _awk_lex_next(c->lex, &t2); _awk_tok_free(&t2);
                awk_val_t casev = _awk_eval_expr(c);
                awk_tok_t col; _awk_lex_next(c->lex, &col);
                if (col.type != TOK_COLON) _awk_eval_error(c, "expected ':' after case");
                _awk_tok_free(&col);
                int eq = 0;
                if (casev.type == V_REGEX) {
                    eq = _awk_regex_match(_awk_v_get_s(&expr), _awk_v_get_s(&casev), NULL, 0);
                } else {
                    const char *se = _awk_v_get_s(&expr), *sc = _awk_v_get_s(&casev);
                    int ne = _awk_looks_numeric(se), nc = _awk_looks_numeric(sc);
                    if (ne && nc) eq = (_awk_v_get_n(&expr) == _awk_v_get_n(&casev));
                    else eq = (strcmp(se ? se : "", sc ? sc : "") == 0);
                }
                _awk_v_clear(&casev);
                if (eq || matched) {
                    matched = 1;
                    while (!break_flag) {
                        _awk_lex_peek(c->lex, &t2);
                        if (t2.type == TOK_RBRACE || t2.type == TOK_EOF ||
                            t2.type == TOK_CASE || t2.type == TOK_DEFAULT) {
                            _awk_tok_free(&t2); break;
                        }
                        if (G_break) { G_break = 0; break_flag = 1; break; }
                        if (G_exit || G_next || G_nextfile || G_return) { break_flag = 1; break; }
                        _awk_eval_stmt(c);
                    }
                    saved_pos = c->lex->pos;
                    if (break_flag) { done = 1; break; }
                } else {
                    _awk_skip_stmt(c);
                    saved_pos = c->lex->pos;
                }
                break;
            } else if (t2.type == TOK_DEFAULT) {
                found_case = 1;
                _awk_lex_next(c->lex, &t2); _awk_tok_free(&t2);
                awk_tok_t col; _awk_lex_next(c->lex, &col);
                if (col.type != TOK_COLON) _awk_eval_error(c, "expected ':' after default");
                _awk_tok_free(&col);
                if (!matched) {
                    matched = 1;
                    while (!break_flag) {
                        _awk_lex_peek(c->lex, &t2);
                        if (t2.type == TOK_RBRACE || t2.type == TOK_EOF ||
                            t2.type == TOK_CASE || t2.type == TOK_DEFAULT) {
                            _awk_tok_free(&t2); break;
                        }
                        if (G_break) { G_break = 0; break_flag = 1; break; }
                        if (G_exit || G_next || G_nextfile || G_return) { break_flag = 1; break; }
                        _awk_eval_stmt(c);
                    }
                } else {
                    _awk_skip_stmt(c);
                }
                saved_pos = c->lex->pos;
                if (break_flag) done = 1;
                break;
            } else {
                _awk_lex_next(c->lex, &t2); _awk_tok_free(&t2);
            }
        }
        if (!found_case) { done = 1; break; }
    }

    _awk_v_clear(&expr);
    int depth = 1;
    while (depth > 0) {
        awk_tok_t sk; _awk_lex_next(c->lex, &sk);
        if (sk.type == TOK_LBRACE) depth++;
        else if (sk.type == TOK_RBRACE) depth--;
        else if (sk.type == TOK_EOF) break;
        _awk_tok_free(&sk);
    }
}

/* -------------------------- eval: statements --------------------------- */

static void _awk_eval_stmt(awk_eval_ctx_t *c)
{
    awk_tok_t t;
    _awk_lex_peek(c->lex, &t);

    if (t.type == TOK_RETURN) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t pk; _awk_lex_peek(c->lex, &pk);
        if (pk.type != TOK_SEMI && pk.type != TOK_RBRACE && pk.type != TOK_EOF) {
            awk_val_t rv = _awk_eval_expr(c);
            _awk_v_copy(&G_return_value, &rv);
            _awk_v_clear(&rv);
        } else {
            _awk_v_clear(&G_return_value);
            _awk_v_set_n(&G_return_value, 0.0);
        }
        _awk_tok_free(&pk);
        G_return = 1;
        return;
    }
    if (t.type == TOK_NEXTFILE) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t); G_nextfile = 1; return;
    }
    if (t.type == TOK_EXIT) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t pk; _awk_lex_peek(c->lex, &pk);
        if (pk.type != TOK_SEMI && pk.type != TOK_RBRACE && pk.type != TOK_EOF) {
            awk_val_t ev = _awk_eval_expr(c);
            G_exit_code = (int)_awk_v_get_n(&ev);
            _awk_v_clear(&ev);
        }
        _awk_tok_free(&pk);
        G_exit = 1;
        return;
    }
    if (t.type == TOK_DELETE) { _awk_eval_delete(c); return; }
    if (t.type == TOK_SWITCH) { _awk_eval_switch(c); return; }
    if (t.type == TOK_GETLINE) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t pk; _awk_lex_peek(c->lex, &pk);
        char *vn = NULL;
        if (pk.type == TOK_IDENT) {
            awk_tok_t pk2; _awk_lex_peek(c->lex, &pk2);
            if (pk2.type != TOK_LT && pk2.type != TOK_PIPE &&
                pk2.type != TOK_SEMI && pk2.type != TOK_RBRACE &&
                pk2.type != TOK_COMMA && pk2.type != TOK_EOF) {
                _awk_lex_next(c->lex, &pk2);
                vn = _awk_xstrdup(pk2.s ? pk2.s : "");
                _awk_tok_free(&pk2);
            }
            _awk_tok_free(&pk);
        } else {
            _awk_tok_free(&pk);
        }
        awk_val_t gr = _awk_eval_getline(c, vn);
        _awk_v_clear(&gr);
        free(vn);
        return;
    }
    if (t.type == TOK_PRINT || t.type == TOK_PRINTF) {
        int is_pf = (t.type == TOK_PRINTF);
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        _awk_eval_print(c, is_pf);
        return;
    }
    if (t.type == TOK_BREAK)    { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); G_break = 1;    return; }
    if (t.type == TOK_CONTINUE) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); G_cont = 1;     return; }
    if (t.type == TOK_NEXT)     { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); G_next = 1;     return; }

    if (t.type == TOK_IF) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t p; _awk_lex_next(c->lex, &p);
        if (p.type != TOK_LPAREN) _awk_eval_error(c, "expected '(' after 'if'");
        _awk_tok_free(&p);
        awk_val_t cond = _awk_eval_expr(c);
        awk_tok_t q; _awk_lex_next(c->lex, &q);
        if (q.type != TOK_RPAREN) _awk_eval_error(c, "expected ')' after if condition");
        _awk_tok_free(&q);
        int true_br = _awk_v_true(&cond);
        _awk_v_clear(&cond);
        _awk_run_block_or_stmt(c, true_br);
        if (G_break || G_cont || G_next || G_exit || G_nextfile || G_return) return;
        awk_tok_t semi_peek; _awk_lex_peek(c->lex, &semi_peek);
        if (semi_peek.type == TOK_SEMI) { _awk_lex_next(c->lex, &semi_peek); _awk_tok_free(&semi_peek); }
        awk_tok_t el; _awk_lex_peek(c->lex, &el);
        if (el.type == TOK_ELSE) {
            _awk_lex_next(c->lex, &el); _awk_tok_free(&el);
            _awk_run_block_or_stmt(c, !true_br);
        }
        return;
    }
    if (t.type == TOK_WHILE) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t p; _awk_lex_next(c->lex, &p);
        if (p.type != TOK_LPAREN) _awk_eval_error(c, "expected '(' after 'while'");
        _awk_tok_free(&p);
        size_t cond_start = c->lex->pos;
        {
            int par = 1;
            awk_tok_t k;
            for (;;) {
                _awk_lex_next(c->lex, &k);
                if (k.type == TOK_LPAREN) par++;
                else if (k.type == TOK_RPAREN) { par--; if (par == 0) { _awk_tok_free(&k); break; } }
                else if (k.type == TOK_EOF) _awk_eval_error(c, "unterminated while header");
                _awk_tok_free(&k);
            }
        }
        size_t after_header = c->lex->pos;
        for (;;) {
            c->lex->pos = cond_start; _awk_lex_invalidate_peek(c->lex);
            awk_val_t cond = _awk_eval_expr(c);
            int ok = _awk_v_true(&cond);
            _awk_v_clear(&cond);
            awk_tok_t rp; _awk_lex_next(c->lex, &rp);
            if (rp.type != TOK_RPAREN) _awk_eval_error(c, "expected ')' after while cond");
            _awk_tok_free(&rp);
            if (!ok) break;
            c->lex->pos = after_header; _awk_lex_invalidate_peek(c->lex);
            _awk_run_block_or_stmt(c, 1);
            if (G_exit || G_next || G_nextfile || G_return) return;
            if (G_break) { G_break = 0; break; }
            G_cont = 0;
        }
        c->lex->pos = after_header; _awk_lex_invalidate_peek(c->lex);
        {
            awk_tok_t peek_tok; _awk_lex_peek(c->lex, &peek_tok);
            if (peek_tok.type == TOK_LBRACE) {
                int depth = 1;
                _awk_lex_next(c->lex, &peek_tok); _awk_tok_free(&peek_tok);
                while (depth > 0) {
                    awk_tok_t sk; _awk_lex_next(c->lex, &sk);
                    if (sk.type == TOK_LBRACE) depth++;
                    else if (sk.type == TOK_RBRACE) depth--;
                    else if (sk.type == TOK_EOF) _awk_eval_error(c, "unterminated while body");
                    _awk_tok_free(&sk);
                }
            } else {
                _awk_skip_stmt(c);
            }
        }
        _awk_lex_invalidate_peek(c->lex);
        return;
    }
    if (t.type == TOK_DO) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t blk; _awk_lex_peek(c->lex, &blk);
        int is_block = (blk.type == TOK_LBRACE);
        size_t saved_pos = c->lex->pos;
        for (;;) {
            if (is_block) {
                _awk_lex_next(c->lex, &blk); _awk_tok_free(&blk);
                _awk_eval_stmt_list(c);
            } else {
                _awk_eval_stmt(c);
            }
            if (G_exit || G_next || G_nextfile || G_return) return;
            if (G_break) { G_break = 0; break; }
            G_cont = 0;
            awk_tok_t w; _awk_lex_next(c->lex, &w);
            if (w.type != TOK_WHILE) _awk_eval_error(c, "expected 'while' after do block");
            _awk_tok_free(&w);
            awk_tok_t lp2; _awk_lex_next(c->lex, &lp2);
            if (lp2.type != TOK_LPAREN) _awk_eval_error(c, "expected '(' after do/while");
            _awk_tok_free(&lp2);
            awk_val_t cond = _awk_eval_expr(c);
            int ok = _awk_v_true(&cond);
            _awk_v_clear(&cond);
            awk_tok_t rp2; _awk_lex_next(c->lex, &rp2);
            if (rp2.type != TOK_RPAREN) _awk_eval_error(c, "expected ')' after do/while");
            _awk_tok_free(&rp2);
            awk_tok_t sm; _awk_lex_peek(c->lex, &sm);
            if (sm.type == TOK_SEMI) { _awk_lex_next(c->lex, &sm); _awk_tok_free(&sm); }
            if (!ok) break;
            c->lex->pos = saved_pos; _awk_lex_invalidate_peek(c->lex);
        }
        return;
    }
    if (t.type == TOK_FOR) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_tok_t lp; _awk_lex_next(c->lex, &lp);
        if (lp.type != TOK_LPAREN) _awk_eval_error(c, "expected '(' after 'for'");
        _awk_tok_free(&lp);
        awk_tok_t t1; _awk_lex_peek(c->lex, &t1);
        if (t1.type == TOK_IDENT) {
            size_t save_pos = t1.start;
            _awk_tok_free(&t1);
            awk_tok_t id; _awk_lex_next(c->lex, &id);
            awk_tok_t after; _awk_lex_peek(c->lex, &after);
            int is_for_in = (after.type == TOK_IN);
            _awk_tok_free(&after);

            if (is_for_in) {
                char *kname = _awk_xstrdup(id.s ? id.s : "");
                _awk_tok_free(&id);
                awk_tok_t intok; _awk_lex_next(c->lex, &intok); _awk_tok_free(&intok);
                awk_tok_t aname; _awk_lex_next(c->lex, &aname);
                char *arrname = _awk_xstrdup(aname.s ? aname.s : "");
                _awk_tok_free(&aname);
                awk_tok_t rp3; _awk_lex_next(c->lex, &rp3);
                if (rp3.type != TOK_RPAREN) _awk_eval_error(c, "expected ')' after for in");
                _awk_tok_free(&rp3);
                awk_array_t *aa = _awk_arr_find(arrname, 0);
                size_t body_start = c->lex->pos;
                if (aa) {
                    for (size_t ki = 0; ki < aa->keys_count; ki++) {
                        _awk_vm_set_s(kname, aa->keys[ki]);
                        c->lex->pos = body_start; _awk_lex_invalidate_peek(c->lex);
                        _awk_run_block_or_stmt(c, 1);
                        if (G_exit || G_next || G_nextfile || G_return) { free(kname); free(arrname); return; }
                        if (G_break) { G_break = 0; break; }
                        G_cont = 0;
                    }
                } else {
                    _awk_run_block_or_stmt(c, 0);
                }
                c->lex->pos = body_start; _awk_lex_invalidate_peek(c->lex);
                {
                    awk_tok_t bk; _awk_lex_peek(c->lex, &bk);
                    if (bk.type == TOK_LBRACE) {
                        int depth = 1;
                        _awk_lex_next(c->lex, &bk); _awk_tok_free(&bk);
                        while (depth > 0) {
                            awk_tok_t sk; _awk_lex_next(c->lex, &sk);
                            if (sk.type == TOK_LBRACE) depth++;
                            else if (sk.type == TOK_RBRACE) depth--;
                            else if (sk.type == TOK_EOF) _awk_eval_error(c, "unterminated for-in body");
                            _awk_tok_free(&sk);
                        }
                    } else {
                        _awk_skip_stmt(c);
                    }
                }
                free(kname); free(arrname);
                return;
            }
            _awk_tok_free(&id);
            c->lex->pos = save_pos;
            _awk_lex_invalidate_peek(c->lex);
        } else {
            _awk_tok_free(&t1);
        }

        size_t init_start = c->lex->pos;
        (void)init_start;
        awk_tok_t sk2;
        int par = 1;
        size_t sc1_off = 0, sc2_off = 0;
        int sc_count = 0;
        for (;;) {
            size_t here = c->lex->pos;
            _awk_lex_next(c->lex, &sk2);
            if (sk2.type == TOK_LPAREN) par++;
            else if (sk2.type == TOK_RPAREN) {
                par--;
                if (par == 0) { _awk_tok_free(&sk2); break; }
            } else if (sk2.type == TOK_SEMI && par == 1) {
                sc_count++;
                if (sc_count == 1) sc1_off = here;
                else if (sc_count == 2) sc2_off = here;
            } else if (sk2.type == TOK_EOF) {
                _awk_tok_free(&sk2);
                _awk_eval_error(c, "unterminated for header");
            }
            _awk_tok_free(&sk2);
        }
        if (sc_count < 2) _awk_eval_error(c, "for (...) needs two ';' separators");
        size_t after_header = c->lex->pos;
        size_t cond_start = sc1_off + 1;
        size_t post_start = sc2_off + 1;
        awk_val_t d = _awk_v_init();
        {
            const char *src = c->lex->src;
            size_t p = sc1_off;
            int d2 = 0;
            while (p > 0) {
                p--;
                if (src[p] == ')') d2++;
                else if (src[p] == '(') { if (d2 == 0) break; d2--; }
            }
            c->lex->pos = p + 1; _awk_lex_invalidate_peek(c->lex);
        }
        {
            awk_tok_t pi; _awk_lex_peek(c->lex, &pi);
            if (pi.type != TOK_SEMI) { d = _awk_eval_expr(c); _awk_v_clear(&d); }
            _awk_tok_free(&pi);
        }
        for (;;) {
            int ok = 1;
            if (sc2_off > sc1_off + 1) {
                c->lex->pos = cond_start; _awk_lex_invalidate_peek(c->lex);
                awk_val_t cond = _awk_eval_expr(c);
                ok = _awk_v_true(&cond);
                _awk_v_clear(&cond);
            }
            if (!ok) break;
            c->lex->pos = after_header; _awk_lex_invalidate_peek(c->lex);
            _awk_run_block_or_stmt(c, 1);
            if (G_exit || G_next || G_nextfile || G_return) return;
            if (G_break) { G_break = 0; break; }
            G_cont = 0;
            c->lex->pos = post_start; _awk_lex_invalidate_peek(c->lex);
            for (;;) {
                awk_tok_t pp; _awk_lex_peek(c->lex, &pp);
                if (pp.type == TOK_RPAREN) { _awk_tok_free(&pp); break; }
                d = _awk_eval_expr(c);
                _awk_v_clear(&d);
                _awk_lex_peek(c->lex, &pp);
                if (pp.type == TOK_COMMA) { _awk_lex_next(c->lex, &pp); _awk_tok_free(&pp); continue; }
                _awk_tok_free(&pp);
                break;
            }
        }
        {
            c->lex->pos = after_header; _awk_lex_invalidate_peek(c->lex);
            awk_tok_t peek_tok; _awk_lex_peek(c->lex, &peek_tok);
            if (peek_tok.type == TOK_LBRACE) {
                int depth = 1;
                _awk_lex_next(c->lex, &peek_tok); _awk_tok_free(&peek_tok);
                while (depth > 0) {
                    awk_tok_t sk3; _awk_lex_next(c->lex, &sk3);
                    if (sk3.type == TOK_LBRACE) depth++;
                    else if (sk3.type == TOK_RBRACE) depth--;
                    else if (sk3.type == TOK_EOF) _awk_eval_error(c, "unterminated for body");
                    _awk_tok_free(&sk3);
                }
            } else {
                _awk_skip_stmt(c);
            }
        }
        _awk_lex_invalidate_peek(c->lex);
        return;
    }

    awk_val_t v = _awk_eval_expr(c);
    _awk_v_clear(&v);
}

static void _awk_eval_stmt_list(awk_eval_ctx_t *c)
{
    awk_tok_t t;
    for (;;) {
        if (G_break || G_cont || G_next || G_exit || G_nextfile || G_return) return;
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_RBRACE || t.type == TOK_EOF) { _awk_tok_free(&t); return; }
        _awk_eval_stmt(c);
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_SEMI) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); }
        else _awk_tok_free(&t);
    }
}

/* -------------------------- expression evaluator ----------------------- */

static awk_val_t _awk_eval_primary(awk_eval_ctx_t *c)
{
    awk_val_t r = _awk_v_init();
    awk_tok_t t;
    _awk_lex_next(c->lex, &t);

    switch (t.type) {
    case TOK_INT:
    case TOK_FLOAT: {
        _awk_v_set_n(&r, t.num);
        _awk_tok_free(&t);
        return r;
    }
    case TOK_STRING: {
        _awk_v_set_s(&r, t.s ? t.s : "");
        _awk_tok_free(&t);
        return r;
    }
    case TOK_REGEX: {
        _awk_v_set_re(&r, t.s ? t.s : "");
        _awk_tok_free(&t);
        return r;
    }
    case TOK_LPAREN: {
        _awk_tok_free(&t);
        r = _awk_eval_expr(c);
        awk_tok_t rp; _awk_lex_next(c->lex, &rp);
        if (rp.type != TOK_RPAREN) _awk_eval_error(c, "expected ')'");
        _awk_tok_free(&rp);
        return r;
    }
    case TOK_DOLLAR: {
        _awk_tok_free(&t);
        awk_val_t idx = _awk_eval_unary(c);
        int i = (int)_awk_v_get_n(&idx);
        _awk_v_clear(&idx);
        _awk_get_field(i, &r);
        return r;
    }
    case TOK_IDENT: {
        char *name = _awk_xstrdup(t.s ? t.s : "");
        _awk_tok_free(&t);
        awk_tok_t pk; _awk_lex_peek(c->lex, &pk);
        if (pk.type == TOK_LPAREN) {
            _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk);
            awk_func_t *fn = _awk_func_find(name);
            if (fn) {
                free(name);
                return _awk_call_user_function(c, fn);
            }
            awk_val_t rv = _awk_call_builtin(c, name);
            free(name);
            return rv;
        }
        if (pk.type == TOK_LBRACK) {
            _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk);
            awk_val_t idxs[64]; int nidx = 0;
            while (1) {
                _awk_lex_peek(c->lex, &pk);
                if (pk.type == TOK_RBRACK) { _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk); break; }
                if (nidx >= 64) _awk_eval_error(c, "too many dimensions");
                idxs[nidx++] = _awk_eval_expr(c);
                _awk_lex_peek(c->lex, &pk);
                if (pk.type == TOK_COMMA) { _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk); continue; }
                if (pk.type == TOK_RBRACK) { _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk); }
                break;
            }
            awk_array_t *a = _awk_arr_find(name, 1);
            if (nidx == 1) {
                awk_val_t *vp = _awk_arr_get_val(a, _awk_v_get_s(&idxs[0]), 0);
                if (vp) _awk_v_copy(&r, vp);
            } else {
                char *mk = _awk_build_multi_key(idxs, nidx);
                awk_val_t *vp = _awk_arr_get_val(a, mk, 0);
                if (vp) _awk_v_copy(&r, vp);
                free(mk);
            }
            for (int i = 0; i < nidx; i++) _awk_v_clear(&idxs[i]);
            free(name);
            return r;
        }
        if (pk.type == TOK_IN) {
            _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk);
            awk_tok_t an; _awk_lex_next(c->lex, &an);
            if (an.type != TOK_IDENT) _awk_eval_error(c, "expected array name after 'in'");
            awk_array_t *a = _awk_arr_find(an.s ? an.s : "", 0);
            int found = 0;
            if (a) {
                awk_array_entry_t *e = _awk_arr_entry_find(a, name);
                found = !!e;
            }
            _awk_tok_free(&an);
            free(name);
            _awk_v_set_n(&r, found ? 1.0 : 0.0);
            return r;
        }
        _awk_tok_free(&pk);
        awk_var_t *v = _awk_vm_get(name, 0);
        if (v && v->is_array) {
            _awk_v_set_arr(&r, v->arr);
        } else if (v) {
            if (_awk_looks_numeric(v->s)) _awk_v_set_n(&r, v->num);
            else _awk_v_set_s(&r, v->s ? v->s : "");
        }
        free(name);
        return r;
    }
    default:
        _awk_eval_error(c, "unexpected token in expression");
    }
    _awk_tok_free(&t);
    return r;
}

static awk_val_t _awk_eval_postfix(awk_eval_ctx_t *c)
{
    awk_tok_t pk; _awk_lex_peek(c->lex, &pk);
    if (pk.type == TOK_INC || pk.type == TOK_DEC) {
        int op = pk.type;
        _awk_lex_next(c->lex, &pk); _awk_tok_free(&pk);
        awk_tok_t id; _awk_lex_next(c->lex, &id);
        if (id.type != TOK_IDENT) _awk_eval_error(c, "expected identifier after ++/--");
        char *name = _awk_xstrdup(id.s ? id.s : "");
        _awk_tok_free(&id);
        awk_var_t *v = _awk_vm_get(name, 1);
        double n = v->num;
        if (op == TOK_INC) v->num = n + 1;
        else v->num = n - 1;
        char buf[64]; snprintf(buf, sizeof buf, "%.6g", v->num);
        free(v->s); v->s = _awk_xstrdup(buf); v->s_len = strlen(buf);
        awk_val_t r = _awk_v_init();
        if (op == TOK_INC) _awk_v_set_n(&r, n + 1);
        else _awk_v_set_n(&r, n - 1);
        free(name);
        return r;
    }
    size_t before = pk.start;
    int is_ident = (pk.type == TOK_IDENT);
    _awk_tok_free(&pk);

    awk_val_t base = _awk_eval_primary(c);

    if (is_ident) {
        awk_tok_t t2; _awk_lex_peek(c->lex, &t2);
        if (t2.type == TOK_INC || t2.type == TOK_DEC) {
            int op = t2.type;
            _awk_tok_free(&t2);
            c->lex->pos = before; _awk_lex_invalidate_peek(c->lex);
            awk_tok_t id2; _awk_lex_next(c->lex, &id2);
            char *name = _awk_xstrdup(id2.s ? id2.s : "");
            _awk_tok_free(&id2);
            awk_tok_t pm; _awk_lex_next(c->lex, &pm); _awk_tok_free(&pm);
            awk_var_t *v = _awk_vm_get(name, 1);
            double old = v->num;
            if (op == TOK_INC) v->num = old + 1;
            else v->num = old - 1;
            char buf[64]; snprintf(buf, sizeof buf, "%.6g", v->num);
            free(v->s); v->s = _awk_xstrdup(buf); v->s_len = strlen(buf);
            awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, old);
            _awk_v_clear(&base);
            free(name);
            return r;
        }
        if (t2.type == TOK_ASSIGN || t2.type == TOK_ADDEQ || t2.type == TOK_SUBEQ ||
            t2.type == TOK_MULEQ  || t2.type == TOK_DIVEQ || t2.type == TOK_MODEQ ||
            t2.type == TOK_POWEQ) {
            int op = t2.type;
            _awk_tok_free(&t2);
            c->lex->pos = before; _awk_lex_invalidate_peek(c->lex);
            awk_tok_t id3; _awk_lex_next(c->lex, &id3);
            char *name = _awk_xstrdup(id3.s ? id3.s : "");
            _awk_tok_free(&id3);
            awk_array_t *a = NULL;
            awk_val_t *ap = NULL;
            char *mk = NULL;
            awk_tok_t lb; _awk_lex_peek(c->lex, &lb);
            if (lb.type == TOK_LBRACK) {
                _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb);
                awk_val_t idxs[64]; int nidx = 0;
                while (1) {
                    _awk_lex_peek(c->lex, &lb);
                    if (lb.type == TOK_RBRACK) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); break; }
                    if (nidx >= 64) _awk_eval_error(c, "too many dimensions");
                    idxs[nidx++] = _awk_eval_expr(c);
                    _awk_lex_peek(c->lex, &lb);
                    if (lb.type == TOK_COMMA) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); continue; }
                    if (lb.type == TOK_RBRACK) { _awk_lex_next(c->lex, &lb); _awk_tok_free(&lb); }
                    break;
                }
                a = _awk_arr_find(name, 1);
                if (nidx == 1) ap = _awk_arr_get_val(a, _awk_v_get_s(&idxs[0]), 1);
                else { mk = _awk_build_multi_key(idxs, nidx); ap = _awk_arr_get_val(a, mk, 1); }
                for (int i = 0; i < nidx; i++) _awk_v_clear(&idxs[i]);
            }
            awk_tok_t asgn_tok; _awk_lex_next(c->lex, &asgn_tok); _awk_tok_free(&asgn_tok);
            awk_val_t rhs = _awk_eval_expr(c);
            double rn = _awk_v_get_n(&rhs);
            awk_val_t result = _awk_v_init();
            if (a && ap) {
                double cur = _awk_v_get_n(ap);
                switch (op) {
                case TOK_ASSIGN: _awk_v_copy(ap, &rhs); break;
                case TOK_ADDEQ:  _awk_v_set_n(ap, cur + rn); break;
                case TOK_SUBEQ:  _awk_v_set_n(ap, cur - rn); break;
                case TOK_MULEQ:  _awk_v_set_n(ap, cur * rn); break;
                case TOK_DIVEQ:  _awk_v_set_n(ap, rn != 0 ? cur / rn : 0); break;
                case TOK_MODEQ:  _awk_v_set_n(ap, rn != 0 ? fmod(cur, rn) : 0); break;
                case TOK_POWEQ:  _awk_v_set_n(ap, pow(cur, rn)); break;
                }
                _awk_v_copy(&result, ap);
            } else {
                awk_var_t *vv = _awk_vm_get(name, 1);
                double cur = vv->num;
                switch (op) {
                case TOK_ASSIGN:
                    if (rhs.type == V_ARRAY_REF) {
                        free(vv->s); vv->s = _awk_xstrdup(""); vv->s_len = 0;
                        vv->num = 0.0; vv->num_ok = 1;
                        vv->is_array = 1;
                        vv->arr = rhs.arr_ref;
                    } else if (rhs.type == V_STR) {
                        free(vv->s);
                        vv->s = _awk_xstrdup(_awk_v_get_s(&rhs));
                        vv->s_len = vv->s ? strlen(vv->s) : 0;
                        vv->num = 0.0;
                        vv->num_ok = 0;
                        vv->is_array = 0;
                    } else {
                        free(vv->s);
                        vv->s = _awk_xstrdup(_awk_v_get_s(&rhs));
                        vv->s_len = vv->s ? strlen(vv->s) : 0;
                        vv->num = _awk_v_get_n(&rhs);
                        vv->num_ok = 1;
                        vv->is_array = 0;
                    }
                    break;
                case TOK_ADDEQ:  cur += rn; break;
                case TOK_SUBEQ:  cur -= rn; break;
                case TOK_MULEQ:  cur *= rn; break;
                case TOK_DIVEQ:  cur = rn != 0 ? cur / rn : 0; break;
                case TOK_MODEQ:  cur = rn != 0 ? fmod(cur, rn) : 0; break;
                case TOK_POWEQ:  cur = pow(cur, rn); break;
                }
                if (op != TOK_ASSIGN) {
                    free(vv->s);
                    char buf[64]; snprintf(buf, sizeof buf, "%.6g", cur);
                    vv->s = _awk_xstrdup(buf); vv->s_len = strlen(buf);
                    vv->num = cur; vv->num_ok = 1;
                    vv->is_array = 0;
                }
                if (vv->is_array && vv->arr) _awk_v_set_arr(&result, vv->arr);
                else if (_awk_looks_numeric(vv->s)) _awk_v_set_n(&result, vv->num);
                else _awk_v_set_s(&result, vv->s ? vv->s : "");
            }
            free(mk);
            _awk_v_clear(&rhs);
            _awk_v_clear(&base);
            free(name);
            return result;
        }
        _awk_tok_free(&t2);
    }
    return base;
}

static awk_val_t _awk_eval_unary(awk_eval_ctx_t *c)
{
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_PLUS) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t v = _awk_eval_unary(c);
        double n = _awk_v_get_n(&v); _awk_v_clear(&v);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, n);
        return r;
    }
    if (t.type == TOK_MINUS) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t v = _awk_eval_unary(c);
        double n = _awk_v_get_n(&v); _awk_v_clear(&v);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, -n);
        return r;
    }
    if (t.type == TOK_NOT) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t v = _awk_eval_unary(c);
        int tv = _awk_v_true(&v); _awk_v_clear(&v);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, tv ? 0.0 : 1.0);
        return r;
    }
    _awk_tok_free(&t);
    return _awk_eval_postfix(c);
}

static awk_val_t _awk_eval_pow(awk_eval_ctx_t *c)
{
    awk_val_t base = _awk_eval_unary(c);
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_POW) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t exp = _awk_eval_pow(c);
        double bn = _awk_v_get_n(&base), en = _awk_v_get_n(&exp);
        _awk_v_clear(&base); _awk_v_clear(&exp);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, pow(bn, en));
        return r;
    }
    _awk_tok_free(&t);
    return base;
}

static awk_val_t _awk_eval_mul(awk_eval_ctx_t *c)
{
    awk_val_t acc = _awk_eval_pow(c);
    for (;;) {
        awk_tok_t t; _awk_lex_peek(c->lex, &t);
        int op = t.type;
        if (op != TOK_STAR && op != TOK_SLASH && op != TOK_PERCENT) { _awk_tok_free(&t); break; }
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t rhs = _awk_eval_pow(c);
        double an = _awk_v_get_n(&acc), bn = _awk_v_get_n(&rhs);
        _awk_v_clear(&acc); _awk_v_clear(&rhs);
        if (op == TOK_STAR) an = an * bn;
        else if (op == TOK_SLASH) an = bn != 0 ? an / bn : 0;
        else an = bn != 0 ? fmod(an, bn) : 0;
        _awk_v_set_n(&acc, an);
    }
    return acc;
}

static awk_val_t _awk_eval_concat(awk_eval_ctx_t *c)
{
    awk_val_t acc = _awk_eval_mul(c);
    for (;;) {
        awk_tok_t t; _awk_lex_peek(c->lex, &t);
        int can_concat = 0;
        switch (t.type) {
        case TOK_IDENT: case TOK_INT: case TOK_FLOAT: case TOK_STRING:
        case TOK_REGEX: case TOK_LPAREN: case TOK_DOLLAR:
            can_concat = 1; break;
        }
        /* Don't concatenate across a newline — newline acts as statement separator */
        if (can_concat && c->lex->saw_newline) can_concat = 0;
        _awk_tok_free(&t);
        if (!can_concat) break;
        awk_val_t rhs = _awk_eval_mul(c);
        const char *as = _awk_v_get_s(&acc), *bs = _awk_v_get_s(&rhs);
        dstr_t d; _awk_dstr_init(&d);
        _awk_dstr_puts(&d, as ? as : "");
        _awk_dstr_puts(&d, bs ? bs : "");
        awk_val_t na = _awk_v_init(); _awk_v_set_s(&na, d.data ? d.data : "");
        _awk_dstr_free(&d);
        _awk_v_clear(&acc); _awk_v_clear(&rhs);
        acc = na;
    }
    return acc;
}

static awk_val_t _awk_eval_add(awk_eval_ctx_t *c)
{
    awk_val_t acc = _awk_eval_concat(c);
    for (;;) {
        awk_tok_t t; _awk_lex_peek(c->lex, &t);
        int op = t.type;
        if (op != TOK_PLUS && op != TOK_MINUS) { _awk_tok_free(&t); break; }
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t rhs = _awk_eval_concat(c);
        double an = _awk_v_get_n(&acc), bn = _awk_v_get_n(&rhs);
        _awk_v_clear(&acc); _awk_v_clear(&rhs);
        if (op == TOK_PLUS) an += bn; else an -= bn;
        _awk_v_set_n(&acc, an);
    }
    return acc;
}

static awk_val_t _awk_eval_cmp(awk_eval_ctx_t *c)
{
    awk_val_t a = _awk_eval_add(c);
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    int op = t.type;
    if (op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE ||
        op == TOK_EQ || op == TOK_NEQ || op == TOK_MATCH || op == TOK_NMATCH) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        awk_val_t b = _awk_eval_add(c);
        int result = 0;
        if (op == TOK_MATCH || op == TOK_NMATCH) {
            const char *sa = _awk_v_get_s(&a), *sb = _awk_v_get_s(&b);
            int m = _awk_regex_match(sa ? sa : "", sb ? sb : "", NULL, 0);
            result = (op == TOK_MATCH) ? m : !m;
        } else {
            const char *sa = _awk_v_get_s(&a), *sb = _awk_v_get_s(&b);
            int na = _awk_looks_numeric(sa), nb = _awk_looks_numeric(sb);
            if (na && nb) {
                double an = _awk_v_get_n(&a), bn = _awk_v_get_n(&b);
                switch (op) {
                case TOK_LT: result = an < bn; break;
                case TOK_LE: result = an <= bn; break;
                case TOK_GT: result = an > bn; break;
                case TOK_GE: result = an >= bn; break;
                case TOK_EQ: result = an == bn; break;
                case TOK_NEQ: result = an != bn; break;
                }
            } else {
                int sc = strcmp(sa ? sa : "", sb ? sb : "");
                switch (op) {
                case TOK_LT: result = sc < 0; break;
                case TOK_LE: result = sc <= 0; break;
                case TOK_GT: result = sc > 0; break;
                case TOK_GE: result = sc >= 0; break;
                case TOK_EQ: result = sc == 0; break;
                case TOK_NEQ: result = sc != 0; break;
                }
            }
        }
        _awk_v_clear(&a); _awk_v_clear(&b);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, result ? 1.0 : 0.0);
        return r;
    }
    _awk_tok_free(&t);
    return a;
}

static awk_val_t _awk_eval_not(awk_eval_ctx_t *c)
{
    return _awk_eval_cmp(c);
}

static awk_val_t _awk_eval_and(awk_eval_ctx_t *c)
{
    awk_val_t a = _awk_eval_not(c);
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_AND) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        int ta = _awk_v_true(&a);
        _awk_v_clear(&a);
        if (!ta) { awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, 0.0); return r; }
        awk_val_t b = _awk_eval_not(c);
        int tb = _awk_v_true(&b);
        _awk_v_clear(&b);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, tb ? 1.0 : 0.0);
        return r;
    }
    _awk_tok_free(&t);
    return a;
}

static awk_val_t _awk_eval_or(awk_eval_ctx_t *c)
{
    awk_val_t a = _awk_eval_and(c);
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_OR) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        int ta = _awk_v_true(&a);
        _awk_v_clear(&a);
        if (ta) { awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, 1.0); return r; }
        awk_val_t b = _awk_eval_and(c);
        int tb = _awk_v_true(&b);
        _awk_v_clear(&b);
        awk_val_t r = _awk_v_init(); _awk_v_set_n(&r, tb ? 1.0 : 0.0);
        return r;
    }
    _awk_tok_free(&t);
    return a;
}

static awk_val_t _awk_eval_tern(awk_eval_ctx_t *c)
{
    awk_val_t a = _awk_eval_or(c);
    awk_tok_t t; _awk_lex_peek(c->lex, &t);
    if (t.type == TOK_QUESTION) {
        _awk_lex_next(c->lex, &t); _awk_tok_free(&t);
        int ta = _awk_v_true(&a);
        _awk_v_clear(&a);
        awk_val_t tv = _awk_eval_expr(c);
        awk_tok_t col; _awk_lex_next(c->lex, &col);
        if (col.type != TOK_COLON) _awk_eval_error(c, "expected ':' in ternary");
        _awk_tok_free(&col);
        awk_val_t fv = _awk_eval_tern(c);
        awk_val_t r = _awk_v_init();
        if (ta) { _awk_v_copy(&r, &tv); } else { _awk_v_copy(&r, &fv); }
        _awk_v_clear(&tv); _awk_v_clear(&fv);
        return r;
    }
    _awk_tok_free(&t);
    return a;
}

static awk_val_t _awk_eval_expr(awk_eval_ctx_t *c)
{
    return _awk_eval_tern(c);
}

/* -------------------------- random helpers ----------------------------- */

static double awk_rand(void)
{
    if (!G_seeded_rand) {
        srand((unsigned int)time(NULL));
        G_seeded_rand = 1;
    }
    return (double)rand() / (double)RAND_MAX;
}

/* -------------------------- builtin functions -------------------------- */

static double _awk_builtin_abs(double x) { return x < 0 ? -x : x; }

static awk_val_t _awk_call_builtin(awk_eval_ctx_t *c, const char *name)
{
    awk_val_t args[32]; int ac = 0;
    char *ident_args[32];
    int i;
    for (i = 0; i < 32; i++) ident_args[i] = NULL;
    awk_tok_t t;
    for (;;) {
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_RPAREN) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); break; }
        if (t.type == TOK_COMMA)  { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); continue; }
        if (ac >= 32) _awk_eval_error(c, "too many arguments");
        /* capture bare identifier name for builtins that need array-by-reference */
        if (t.type == TOK_IDENT) {
            ident_args[ac] = _awk_xstrdup(t.s ? t.s : "");
        }
        _awk_tok_free(&t);
        args[ac] = _awk_eval_expr(c);
        ac++;
    }
    awk_val_t r = _awk_v_init();
    char lname[64];
    (void)_awk_safe_copy(lname, name ? name : "", sizeof lname);

    if (strcmp(lname, "sin") == 0)     { _awk_v_set_n(&r, sin(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "cos") == 0)     { _awk_v_set_n(&r, cos(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "tan") == 0)     { _awk_v_set_n(&r, tan(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "asin") == 0)    { _awk_v_set_n(&r, asin(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "acos") == 0)    { _awk_v_set_n(&r, acos(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "atan2") == 0)   { _awk_v_set_n(&r, atan2(_awk_v_get_n(&args[0]), _awk_v_get_n(&args[1]))); }
    else if (strcmp(lname, "abs") == 0)     { _awk_v_set_n(&r, _awk_builtin_abs(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "log10") == 0)   { _awk_v_set_n(&r, log10(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "log") == 0)     { _awk_v_set_n(&r, log(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "exp") == 0)     { _awk_v_set_n(&r, exp(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "sqrt") == 0)    { _awk_v_set_n(&r, sqrt(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "floor") == 0)   { _awk_v_set_n(&r, floor(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "ceil") == 0)    { _awk_v_set_n(&r, ceil(_awk_v_get_n(&args[0]))); }
    else if (strcmp(lname, "round") == 0)   { double x = _awk_v_get_n(&args[0]); _awk_v_set_n(&r, x >= 0 ? floor(x + 0.5) : ceil(x - 0.5)); }
    else if (strcmp(lname, "int") == 0)     { _awk_v_set_n(&r, (double)(long long)_awk_v_get_n(&args[0])); }
    else if (strcmp(lname, "rand") == 0)    { _awk_v_set_n(&r, awk_rand()); }
    else if (strcmp(lname, "srand") == 0)   {
        unsigned int seed = ac > 0 ? (unsigned int)_awk_v_get_n(&args[0]) : (unsigned int)time(NULL);
        srand(seed); G_seeded_rand = 1; _awk_v_set_n(&r, 0);
    }
    else if (strcmp(lname, "length") == 0) {
        const char *s = ac > 0 ? _awk_v_get_s(&args[0]) : (G_fields_count > 0 ? G_fields[0] : "");
        _awk_v_set_n(&r, (double)(s ? strlen(s) : 0));
    }
    else if (strcmp(lname, "substr") == 0) {
        const char *s = ac > 0 ? _awk_v_get_s(&args[0]) : "";
        int start = ac > 1 ? (int)_awk_v_get_n(&args[1]) : 1;
        int len = ac > 2 ? (int)_awk_v_get_n(&args[2]) : -1;
        if (!s) s = "";
        int slen = (int)strlen(s);
        if (start < 1) start = 1;
        if (start > slen) { _awk_v_set_s(&r, ""); }
        else {
            int s0 = start - 1;
            int e0 = (len < 0) ? slen : (s0 + len);
            if (e0 > slen) e0 = slen;
            char *ss = _awk_xstrndup(s + s0, (size_t)(e0 - s0));
            _awk_v_set_s(&r, ss ? ss : ""); free(ss);
        }
    }
    else if (strcmp(lname, "index") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        const char *needle = _awk_v_get_s(&args[1]);
        if (!s) s = "";
        if (!needle) needle = "";
        char *p = strstr(s, needle);
        _awk_v_set_n(&r, p ? (double)(p - s + 1) : 0);
    }
    else if (strcmp(lname, "tolower") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        dstr_t d; _awk_dstr_init(&d);
        for (const char *p = s ? s : ""; *p; p++) _awk_dstr_putc(&d, (char)tolower((unsigned char)*p));
        _awk_v_set_s(&r, d.data ? d.data : ""); _awk_dstr_free(&d);
    }
    else if (strcmp(lname, "toupper") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        dstr_t d; _awk_dstr_init(&d);
        for (const char *p = s ? s : ""; *p; p++) _awk_dstr_putc(&d, (char)toupper((unsigned char)*p));
        _awk_v_set_s(&r, d.data ? d.data : ""); _awk_dstr_free(&d);
    }
    else if (strcmp(lname, "split") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        /* Prefer captured identifier name over value-derived string.
           When arg is a bare identifier (e.g. `curr`), it must be taken literally
           as the array name, not evaluated as a scalar variable whose value might
           be 0/"". */
        const char *aname = (ac >= 2 && ident_args[1]) ? ident_args[1] :
                            (ac >= 2 ? _awk_v_get_s(&args[1]) : "");
        const char *fs = ac >= 3 ? _awk_v_get_s(&args[2]) : (G_FS ? G_FS : _awk_vm_get_s("FS"));
        awk_array_t *a = _awk_arr_find(aname, 1);
        if (a) _awk_arr_clear(a);
        int nf = 0;
        if (s && a) {
            if (!fs || !*fs) fs = " ";
            if (strcmp(fs, " ") == 0) {
                size_t i = 0, len = strlen(s);
                while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
                while (i < len) {
                    size_t j = i;
                    while (j < len && s[j] != ' ' && s[j] != '\t') j++;
                    nf++;
                    char k[32]; snprintf(k, sizeof k, "%d", nf);
                    char *f = _awk_xstrndup(s + i, j - i);
                    awk_val_t *vp = _awk_arr_get_val(a, k, 1);
                    _awk_v_set_s(vp, f ? f : "");
                    free(f);
                    i = j;
                    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
                }
            } else {
                size_t llen = strlen(s);
                if (fs[0] && fs[1] == 0) {
                    /* Single-char separator: always literal, not regex, matching gawk behavior */
                    char sep = fs[0];
                    size_t i = 0, start = 0;
                    while (1) {
                        if (s[i] == sep || s[i] == 0) {
                            nf++;
                            char k[32]; snprintf(k, sizeof k, "%d", nf);
                            char *f = _awk_xstrndup(s + start, i - start);
                            awk_val_t *vp = _awk_arr_get_val(a, k, 1);
                            _awk_v_set_s(vp, f ? f : "");
                            free(f);
                            if (s[i] == 0) break;
                            i++; start = i; continue;
                        }
                        i++; if (i > llen) break;
                    }
                } else {
                    awk_regex_t *rx;
                    if (awk_regcomp(&rx, fs) == 0) {
                        awk_regmatch_t m[1];
                        const char *p = s;
                        while (1) {
                            if (awk_regexec(rx, p, 1, m, 0) != 0) {
                                nf++; char k[32]; snprintf(k, sizeof k, "%d", nf);
                                awk_val_t *vp = _awk_arr_get_val(a, k, 1);
                                _awk_v_set_s(vp, p ? p : "");
                                break;
                            }
                            int so = (int)m[0].rm_so, eo = (int)m[0].rm_eo;
                            nf++; char k[32]; snprintf(k, sizeof k, "%d", nf);
                            char *f = _awk_xstrndup(p, (size_t)so);
                            awk_val_t *vp = _awk_arr_get_val(a, k, 1);
                            _awk_v_set_s(vp, f ? f : "");
                            free(f);
                            p += eo;
                        }
                        awk_regfree(rx);
                    }
                }
            }
        }
        _awk_v_set_n(&r, (double)nf);
    }
    else if (strcmp(lname, "sprintf") == 0) {
        const char *fs = _awk_v_get_s(&args[0]);
        dstr_t out; _awk_dstr_init(&out);
        int ai = 1;
        for (const char *p = fs ? fs : ""; *p; ) {
            if (*p == '%') {
                p++;
                if (*p == '%') { _awk_dstr_putc(&out, '%'); p++; continue; }
                char spec[64]; int si = 0;
                spec[si++] = '%';
                while (*p && (strchr("-+ #0", *p) || isdigit((unsigned char)*p) || *p == '.' ||
                              *p == 'l' || *p == 'h' || *p == 'z' || *p == 'j' || *p == 't')) {
                    if (si < 62) spec[si++] = *p;
                    p++;
                }
                if (*p) { if (si < 62) spec[si++] = *p; p++; }
                spec[si] = 0;
                awk_val_t dummy; memset(&dummy, 0, sizeof dummy);
                awk_val_t *a = (ai < ac) ? &args[ai++] : &dummy;
                _awk_do_format_one(&out, spec, a);
            } else {
                _awk_dstr_putc(&out, *p);
                p++;
            }
        }
        _awk_v_set_s(&r, out.data ? out.data : "");
        _awk_dstr_free(&out);
    }
    else if (strcmp(lname, "printf") == 0) {
        const char *fs = _awk_v_get_s(&args[0]);
        dstr_t out; _awk_dstr_init(&out);
        int ai = 1;
        for (const char *p = fs ? fs : ""; *p; ) {
            if (*p == '%') {
                p++;
                if (*p == '%') { _awk_dstr_putc(&out, '%'); p++; continue; }
                char spec[64]; int si = 0;
                spec[si++] = '%';
                while (*p && (strchr("-+ #0", *p) || isdigit((unsigned char)*p) || *p == '.' ||
                              *p == 'l' || *p == 'h' || *p == 'z' || *p == 'j' || *p == 't')) {
                    if (si < 62) spec[si++] = *p;
                    p++;
                }
                if (*p) { if (si < 62) spec[si++] = *p; p++; }
                spec[si] = 0;
                awk_val_t dummy; memset(&dummy, 0, sizeof dummy);
                awk_val_t *a = (ai < ac) ? &args[ai++] : &dummy;
                _awk_do_format_one(&out, spec, a);
            } else {
                _awk_dstr_putc(&out, *p);
                p++;
            }
        }
        awk_fputs(out.data ? out.data : "", awk_out_stream);
        (void)fflush(awk_out_stream);
        _awk_dstr_free(&out);
    }
    else if (strcmp(lname, "sub") == 0 || strcmp(lname, "gsub") == 0) {
        const char *pat = _awk_v_get_s(&args[0]);
        const char *repl = _awk_v_get_s(&args[1]);
        const char *tgt = ac >= 3 ? _awk_v_get_s(&args[2]) : (G_fields_count > 0 ? G_fields[0] : "");
        if (getenv("AWK_DEBUG_SUB")) {
            awk_err_printf("[sub] pat=[%s] repl=[%s] tgt=[%s] ac=%d ident2=[%s]\n",
                    pat?pat:"", repl?repl:"", tgt?tgt:"", ac, ident_args[2]?ident_args[2]:"(null)");
        }
        awk_regex_t *re = NULL;
        int count = 0;
        dstr_t out; _awk_dstr_init(&out);
        if (awk_regcomp(&re, pat ? pat : "") == 0) {
            const char *p = tgt ? tgt : "";
            awk_regmatch_t m[1];
            int global = (strcmp(lname, "gsub") == 0);
            do {
                int rc = awk_regexec(re, p, 1, m, 0);
                if (getenv("AWK_DEBUG_SUB")) {
                    awk_err_printf("[sub] regexec rc=%d p=[%s]\n", rc, p?p:"");
                }
                if (rc != 0) break;
                int so = (int)m[0].rm_so, eo = (int)m[0].rm_eo;
                if (eo <= so && *p == 0) break;
                _awk_dstr_feed(&out, p, (size_t)so);
                for (const char *rp = repl ? repl : ""; *rp; rp++) {
                    if (*rp == '&') {
                        _awk_dstr_feed(&out, p + so, (size_t)(eo - so));
                    } else if (*rp == '\\' && rp[1]) {
                        rp++; _awk_dstr_putc(&out, *rp);
                    } else {
                        _awk_dstr_putc(&out, *rp);
                    }
                }
                p += eo;
                count++;
                if (eo <= so && *p) { _awk_dstr_putc(&out, *p); p++; }
            } while (global);
            _awk_dstr_puts(&out, p);
            awk_regfree(re);
        }
        if (ac >= 3) {
            /* Write result back to the target variable (3rd arg).
               Prefer the captured bare identifier name if available. */
            const char *result = out.data ? out.data : "";
            if (ident_args[2]) {
                _awk_vm_set_s(ident_args[2], result);
            }
            /* Also handle the case where the 3rd arg is a field reference ($N).
               For now we only handle bare scalar variables via ident_args[2]. */
            _awk_v_set_n(&r, (double)count);
        } else if (G_fields_count > 0) {
            free(G_fields[0]);
            G_fields[0] = _awk_xstrdup(out.data ? out.data : "");
            _awk_v_set_n(&r, (double)count);
        } else {
            _awk_v_set_n(&r, (double)count);
        }
        _awk_dstr_free(&out);
    }
    else if (strcmp(lname, "match") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        const char *pat = _awk_v_get_s(&args[1]);
        awk_regmatch_t mm[16]; size_t nmatch = 16;
        int ok = _awk_regex_match(s ? s : "", pat ? pat : "", mm, nmatch);
        if (ac >= 3 && args[2].type == V_ARRAY_REF && args[2].arr_ref) {
            awk_array_t *a = args[2].arr_ref;
            _awk_arr_clear(a);
            if (ok) {
                char buf[32];
                for (size_t i = 0; i < 10 && mm[i].rm_so >= 0; i++) {
                    snprintf(buf, sizeof buf, "%d", (int)i);
                    awk_val_t *vp = _awk_arr_get_val(a, buf, 1);
                    char *ss = _awk_xstrndup(s + mm[i].rm_so, (size_t)(mm[i].rm_eo - mm[i].rm_so));
                    _awk_v_set_s(vp, ss ? ss : "");
                    free(ss);
                }
                if (mm[0].rm_so >= 0) {
                    awk_val_t *vp = _awk_arr_get_val(a, "start", 1);
                    _awk_v_set_n(vp, (double)(mm[0].rm_so + 1));
                    vp = _awk_arr_get_val(a, "length", 1);
                    _awk_v_set_n(vp, (double)(mm[0].rm_eo - mm[0].rm_so));
                }
            }
        }
        if (ok) _awk_v_set_n(&r, (double)(mm[0].rm_so + 1));
        else _awk_v_set_n(&r, 0.0);
    }
    else if (strcmp(lname, "gensub") == 0) {
        const char *pat = _awk_v_get_s(&args[0]);
        const char *repl = _awk_v_get_s(&args[1]);
        const char *how_s = _awk_v_get_s(&args[2]);
        const char *tgt = ac >= 4 ? _awk_v_get_s(&args[3]) : (G_fields_count > 0 ? G_fields[0] : "");
        int how_n = (int)_awk_v_get_n(&args[2]);
        int global = (how_s && (*how_s == 'g' || *how_s == 'G'));
        int which = global ? -1 : (how_n > 0 ? how_n : 1);
        awk_regex_t *re = NULL;
        awk_regmatch_t caps[10];
        dstr_t out; _awk_dstr_init(&out);
        if (awk_regcomp(&re, pat ? pat : "") == 0) {
            const char *p = tgt ? tgt : "";
            int count = 0;
            for (;;) {
                if (awk_regexec(re, p, 10, caps, 0) != 0) break;
                int so = (int)caps[0].rm_so, eo = (int)caps[0].rm_eo;
                if (eo <= so) break;
                count++;
                int do_repl = global || (count == which);
                _awk_dstr_feed(&out, p, (size_t)so);
                if (do_repl) {
                    for (const char *rp = repl ? repl : ""; *rp; rp++) {
                        if (*rp == '\\' && rp[1] >= '1' && rp[1] <= '9') {
                            int gi = rp[1] - '0'; rp++;
                            if (gi < 10 && caps[gi].rm_so >= 0) {
                                _awk_dstr_feed(&out, p + caps[gi].rm_so,
                                         (size_t)(caps[gi].rm_eo - caps[gi].rm_so));
                            }
                        } else if (*rp == '&') {
                            _awk_dstr_feed(&out, p + so, (size_t)(eo - so));
                        } else {
                            _awk_dstr_putc(&out, *rp);
                        }
                    }
                } else {
                    _awk_dstr_feed(&out, p + so, (size_t)(eo - so));
                }
                p += eo;
                if (!global) break;
            }
            _awk_dstr_puts(&out, p);
            awk_regfree(re);
        }
        _awk_v_set_s(&r, out.data ? out.data : "");
        _awk_dstr_free(&out);
    }
    else if (strcmp(lname, "patsplit") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        const char *aname = (ident_args[1]) ? ident_args[1] : _awk_v_get_s(&args[1]);
        const char *fpat = ac >= 3 ? _awk_v_get_s(&args[2]) : _awk_vm_get_s("FPAT");
        awk_array_t *a = _awk_arr_find(aname, 1);
        if (a) _awk_arr_clear(a);
        int n = 0;
        if (a && fpat && *fpat && s) {
            awk_regex_t *rx;
            if (awk_regcomp(&rx, fpat) == 0) {
                awk_regmatch_t m[1];
                const char *p = s;
                while (1) {
                    if (awk_regexec(rx, p, 1, m, 0) != 0) break;
                    int so = (int)m[0].rm_so, eo = (int)m[0].rm_eo;
                    if (eo <= so) break;
                    n++;
                    char k[32]; snprintf(k, sizeof k, "%d", n);
                    char *f = _awk_xstrndup(p + so, (size_t)(eo - so));
                    awk_val_t *vp = _awk_arr_get_val(a, k, 1);
                    _awk_v_set_s(vp, f ? f : "");
                    free(f);
                    p += eo;
                }
                awk_regfree(rx);
            }
        }
        _awk_v_set_n(&r, (double)n);
    }
    else if (strcmp(lname, "strtonum") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        double v = 0;
        if (s && *s) {
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                unsigned long long ull = 0;
                for (const char *p = s + 2; *p; p++) {
                    int d;
                    if (*p >= '0' && *p <= '9') d = *p - '0';
                    else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
                    else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
                    else break;
                    ull = ull * 16 + d;
                }
                v = (double)(long long)ull;
            } else if (s[0] == '0') {
                unsigned long long ull = 0;
                for (const char *p = s + 1; *p; p++) {
                    if (*p < '0' || *p > '7') break;
                    ull = ull * 8 + (*p - '0');
                }
                v = (double)(long long)ull;
            } else {
                v = strtod(s, NULL);
            }
        }
        _awk_v_set_n(&r, v);
    }
    else if (strcmp(lname, "asort") == 0 || strcmp(lname, "asorti") == 0) {
        int sort_keys = (strcmp(lname, "asorti") == 0);
        const char *sname = (ident_args[0]) ? ident_args[0] : _awk_v_get_s(&args[0]);
        const char *dname = (ac >= 2 && ident_args[1]) ? ident_args[1] :
                           (ac >= 2 ? _awk_v_get_s(&args[1]) : sname);
        awk_array_t *src = _awk_arr_find(sname, 0);
        awk_array_t *dst = _awk_arr_find(dname, 1);
        if (!src || !dst) { _awk_v_set_n(&r, 0.0); goto args_done; }
        if (dst != src) _awk_arr_clear(dst);
        size_t n = src->keys_count;
        char **vals = (char **)malloc(n * sizeof(char *));
        double *nums = (double *)malloc(n * sizeof(double));
        for (size_t i = 0; i < n; i++) {
            awk_val_t *vp = _awk_arr_get_val(src, src->keys[i], 0);
            if (sort_keys) { vals[i] = _awk_xstrdup(src->keys[i]); }
            else { vals[i] = vp ? _awk_xstrdup(_awk_v_get_s(vp)) : _awk_xstrdup(""); }
            nums[i] = vp ? _awk_v_get_n(vp) : 0;
        }
        for (size_t i = 1; i < n; i++) {
            for (size_t j = i; j > 0; j--) {
                int swap = 0;
                int a_num = _awk_looks_numeric(vals[j-1]);
                int b_num = _awk_looks_numeric(vals[j]);
                if (a_num && b_num) {
                    if (nums[j] < nums[j-1]) swap = 1;
                } else {
                    if (strcmp(vals[j], vals[j-1]) < 0) swap = 1;
                }
                if (!swap) break;
                char *ts = vals[j-1]; vals[j-1] = vals[j]; vals[j] = ts;
                double tn = nums[j-1]; nums[j-1] = nums[j]; nums[j] = tn;
            }
        }
        if (dst == src) _awk_arr_clear(dst);
        for (size_t i = 0; i < n; i++) {
            char k[32]; snprintf(k, sizeof k, "%d", (int)(i + 1));
            awk_val_t *vp = _awk_arr_get_val(dst, k, 1);
            if (sort_keys) {
                _awk_v_set_s(vp, vals[i] ? vals[i] : "");
            } else {
                if (_awk_looks_numeric(vals[i])) _awk_v_set_n(vp, nums[i]);
                else _awk_v_set_s(vp, vals[i] ? vals[i] : "");
            }
            free(vals[i]);
        }
        free(vals); free(nums);
        _awk_v_set_n(&r, (double)n);
    }
    else if (strcmp(lname, "mktime") == 0) {
        const char *s = _awk_v_get_s(&args[0]);
        int Y=0,Mo=0,D=0,H=0,Mi=0,S=0;
        if (s) (void)sscanf(s, "%d %d %d %d %d %d", &Y, &Mo, &D, &H, &Mi, &S);
        struct tm tm; memset(&tm, 0, sizeof tm);
        tm.tm_year = Y - 1900; tm.tm_mon = Mo - 1; tm.tm_mday = D;
        tm.tm_hour = H; tm.tm_min = Mi; tm.tm_sec = S;
time_t tt = (time_t)-1;
#ifdef AWK_PLATFORM_WINDOWS
        /* MinGW-32's _mkgmtime() may fail to link (_mkgmtime32 missing in the
         * 32-bit runtime) and _putenv_s (Vista+) is absent on Windows XP.
         * Replicate timegm() with the TZ=UTC mktime trick, using the legacy
         * _putenv()/_tzset() which XP's msvcrt exports. */
        {
            extern int __cdecl _putenv(const char *);
            char *old_tz = getenv("TZ");
            char *tz_save = old_tz ? _awk_xstrdup(old_tz) : NULL;
            _putenv("TZ=UTC"); _tzset();
            tt = mktime(&tm);
            if (tz_save) {
                size_t zl = strlen(tz_save);
                char *zb = (char *)malloc(zl + 4);
                if (zb) { memcpy(zb, "TZ=", 3); memcpy(zb + 3, tz_save, zl + 1); _putenv(zb); }
            } else {
                _putenv("TZ=");
            }
            free(tz_save);
            _tzset();
        }
#else
        /* timegm() is a GNU/BSD extension not declared under strict
         * _POSIX_C_SOURCE.  Use the portable TZ-UTC mktime fallback. */
        {
            char *old_tz = getenv("TZ");
            char *tz_save = old_tz ? _awk_xstrdup(old_tz) : NULL;
            setenv("TZ", "UTC", 1);
            tzset();
            tt = mktime(&tm);
            if (tz_save) { setenv("TZ", tz_save, 1); free(tz_save); }
            else          unsetenv("TZ");
            tzset();
        }
#endif
        _awk_v_set_n(&r, (double)tt);
    }
    else if (strcmp(lname, "systime") == 0) {
        _awk_v_set_n(&r, (double)time(NULL));
    }
    else if (strcmp(lname, "strftime") == 0) {
        const char *fmt = ac > 0 ? _awk_v_get_s(&args[0]) : "%a %b %d %H:%M:%S %Y";
        time_t ts = ac >= 2 ? (time_t)_awk_v_get_n(&args[1]) : time(NULL);
        struct tm *tm = localtime(&ts);
        char buf[4096];
        size_t l = strftime(buf, sizeof buf, fmt ? fmt : "", tm ? tm : NULL);
        (void)l;
        _awk_v_set_s(&r, buf);
    }
    else if (strcmp(lname, "typeof") == 0) {
        if (ac == 0) _awk_v_set_s(&r, "unassigned");
        else if (args[0].type == V_ARRAY_REF) _awk_v_set_s(&r, "array");
        else if (args[0].type == V_NUM) _awk_v_set_s(&r, "number");
        else if (args[0].type == V_STR) _awk_v_set_s(&r, "string");
        else if (args[0].type == V_REGEX) _awk_v_set_s(&r, "string");
        else _awk_v_set_s(&r, "unassigned");
    }
    else if (strcmp(lname, "isarray") == 0) {
        int y = 0;
        if (ac > 0) {
            if (args[0].type == V_ARRAY_REF) y = 1;
        }
        _awk_v_set_n(&r, y ? 1.0 : 0.0);
    }
    else if (strcmp(lname, "close") == 0) {
        const char *fn = _awk_v_get_s(&args[0]);
        awk_fh_t *fh = fh_find(fn);
        if (fh) fh_close(fh);
        _awk_v_set_n(&r, 0.0);
    }
    else if (strcmp(lname, "fflush") == 0) {
        if (ac == 0) {
            fflush(stdout);
        } else {
            const char *fn = _awk_v_get_s(&args[0]);
            awk_fh_t *fh = fh_find(fn);
            if (fh && fh->fp) fflush(fh->fp);
            else if (strcmp(fn ? fn : "", "stdout") == 0 || strcmp(fn ? fn : "", "/dev/stdout") == 0) fflush(stdout);
        }
        _awk_v_set_n(&r, 0.0);
    }
    else if (strcmp(lname, "system") == 0) {
        const char *cmd = _awk_v_get_s(&args[0]);
        int rc = 0;
        if (cmd && *cmd) {
            rc = system(cmd);
        }
        _awk_v_set_n(&r, (double)rc);
    }
    else if (strcmp(lname, "getline") == 0) {
        awk_val_t gr = _awk_eval_getline(c, NULL);
        _awk_v_copy(&r, &gr);
        _awk_v_clear(&gr);
    }
    else if (strcmp(lname, "exit") == 0) {
        G_exit = 1;
        if (ac > 0) G_exit_code = (int)_awk_v_get_n(&args[0]);
    }
    else {
        _awk_eval_error(c, "unknown function '%s'", name ? name : "");
    }
args_done:
    for (int i = 0; i < ac; i++) _awk_v_clear(&args[i]);
    for (int i = 0; i < 32; i++) free(ident_args[i]);
    return r;
}

/* -------------------------- user-defined function calls ---------------- */

static awk_val_t _awk_call_user_function(awk_eval_ctx_t *c, awk_func_t *fn)
{
    awk_val_t args[64]; int ac = 0;
    awk_tok_t t;
    for (;;) {
        _awk_lex_peek(c->lex, &t);
        if (t.type == TOK_RPAREN) { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); break; }
        if (t.type == TOK_COMMA)  { _awk_lex_next(c->lex, &t); _awk_tok_free(&t); continue; }
        if (ac >= 64) _awk_eval_error(c, "too many arguments");
        args[ac++] = _awk_eval_expr(c);
    }
    if (G_scope_sp >= AWK_SCOPE_MAX) _awk_eval_error(c, "function call stack overflow");

    awk_scope_t *sc = &G_scope_stack[G_scope_sp++];
    memset(sc, 0, sizeof(*sc));

    for (int i = 0; i < fn->nparams; i++) {
        if (fn->is_array_param[i]) {
            if (i < ac && args[i].type == V_ARRAY_REF) {
                awk_var_t *v = _awk_scope_find_var(sc, fn->params[i], 1);
                v->is_array = 1;
                v->arr = args[i].arr_ref;
            }
        } else {
            if (i < ac) {
                awk_var_t *v = _awk_scope_find_var(sc, fn->params[i], 1);
                free(v->s);
                v->s = _awk_xstrdup(_awk_v_get_s(&args[i]));
                v->s_len = v->s ? strlen(v->s) : 0;
                v->num = _awk_v_get_n(&args[i]);
                v->num_ok = 1;
            } else {
                _awk_scope_find_var(sc, fn->params[i], 1);
            }
        }
    }
    for (int i = fn->nparams; i < fn->nparams + fn->nlocals; i++) {
        _awk_scope_find_var(sc, fn->params[i], 1);
    }

    const char *src_save = c->lex->src;
    size_t pos_save = c->lex->pos;
    size_t end_save = c->lex->end;
    int line_save = c->lex->line;
    int peek_save = c->lex->peek_ok;
    awk_tok_t peek_tok_save = c->lex->peek;
    c->lex->peek.s = NULL;

    awk_lex_t local_lex;
    awk_lex_init(&local_lex, fn->body ? fn->body : "");
    awk_eval_ctx_t local_ctx;
    local_ctx.lex = &local_lex;

    G_return = 0;
    _awk_v_clear(&G_return_value);
    _awk_eval_stmt_list(&local_ctx);

    awk_val_t r = _awk_v_init();
    _awk_v_copy(&r, &G_return_value);
    _awk_v_clear(&G_return_value);
    G_return = 0;

    for (size_t i = 0; i < sc->vars.count; i++) {
        free(sc->vars.v[i].name);
        free(sc->vars.v[i].s);
    }
    free(sc->vars.v);
    for (size_t i = 0; i < sc->arrays.count; i++) {
        free(sc->arrays.a[i].name);
        for (size_t j = 0; j < sc->arrays.a[i].keys_count; j++) free(sc->arrays.a[i].keys[j]);
        free(sc->arrays.a[i].keys);
    }
    free(sc->arrays.a);
    memset(sc, 0, sizeof(*sc));
    G_scope_sp--;

    c->lex->src = src_save;
    c->lex->pos = pos_save;
    c->lex->end = end_save;
    c->lex->line = line_save;
    c->lex->peek_ok = peek_save;
    c->lex->peek = peek_tok_save;

    for (int i = 0; i < ac; i++) _awk_v_clear(&args[i]);
    return r;
}

/* -------------------------- program parser ----------------------------- */

static awk_lex_t *G_program_lex = NULL;

static void _awk_parse_function_def(awk_lex_t *l)
{
    awk_tok_t nm; _awk_lex_next(l, &nm);
    if (nm.type != TOK_IDENT) _awk_eval_error((awk_eval_ctx_t*)0, "expected function name");
    char *fname = _awk_xstrdup(nm.s ? nm.s : "");
    _awk_tok_free(&nm);

    awk_tok_t lp; _awk_lex_next(l, &lp);
    if (lp.type != TOK_LPAREN) _awk_eval_error((awk_eval_ctx_t*)0, "expected '(' after function name");
    _awk_tok_free(&lp);

    char *params[256];
    int   is_arr[256];
    int   is_local[256];
    int np = 0;
    int entered_locals = 0;
    for (;;) {
        awk_tok_t pk; _awk_lex_peek(l, &pk);
        if (pk.type == TOK_RPAREN) { _awk_lex_next(l, &pk); _awk_tok_free(&pk); break; }
        if (pk.type == TOK_COMMA)  { _awk_lex_next(l, &pk); _awk_tok_free(&pk); continue; }
        if (pk.type != TOK_IDENT) _awk_eval_error((awk_eval_ctx_t*)0, "expected parameter name");
        _awk_lex_next(l, &pk);
        char *pname = _awk_xstrdup(pk.s ? pk.s : "");
        _awk_tok_free(&pk);
        awk_tok_t pk2; _awk_lex_peek(l, &pk2);
        int arr = 0;
        if (pk2.type == TOK_LBRACK) {
            _awk_lex_next(l, &pk2); _awk_tok_free(&pk2);
            awk_tok_t rb; _awk_lex_next(l, &rb);
            if (rb.type != TOK_RBRACK) _awk_eval_error((awk_eval_ctx_t*)0, "expected ']'");
            _awk_tok_free(&rb);
            arr = 1;
        } else {
            _awk_tok_free(&pk2);
        }
        params[np] = pname;
        is_arr[np] = arr;
        is_local[np] = entered_locals;
        np++;
        awk_tok_t cm; _awk_lex_peek(l, &cm);
        if (cm.type == TOK_COMMA) {
            size_t comma_pos = l->pos;
            _awk_lex_next(l, &cm); _awk_tok_free(&cm);
            size_t p = l->pos;
            int space_count = 0;
            while (p < l->end) {
                char ch2 = l->src[p];
                if (ch2 == ' ' || ch2 == '\t') { space_count++; p++; }
                else if (ch2 == '\n' || ch2 == '\r') { p++; }
                else break;
            }
            if (space_count >= 2) entered_locals = 1;
            (void)comma_pos;
        } else {
            _awk_tok_free(&cm);
        }
        if (np >= 256) _awk_eval_error((awk_eval_ctx_t*)0, "too many parameters");
    }

    int nparams = 0, nlocals = 0;
    for (int i = 0; i < np; i++) {
        if (is_local[i]) nlocals++;
        else nparams++;
    }
    int total = np;

    awk_tok_t lb; _awk_lex_next(l, &lb);
    if (lb.type != TOK_LBRACE) _awk_eval_error((awk_eval_ctx_t*)0, "expected '{' for function body");
    _awk_tok_free(&lb);
    size_t body_start = l->pos;
    int depth = 1;
    while (depth > 0) {
        awk_tok_t t2; _awk_lex_next(l, &t2);
        if (t2.type == TOK_LBRACE) depth++;
        else if (t2.type == TOK_RBRACE) { depth--; if (depth == 0) { _awk_tok_free(&t2); break; } }
        else if (t2.type == TOK_EOF) _awk_eval_error((awk_eval_ctx_t*)0, "unterminated function body");
        _awk_tok_free(&t2);
    }
    size_t body_end = l->pos;

    if (G_funcs.count == G_funcs.cap) {
        G_funcs.cap = G_funcs.cap ? G_funcs.cap * 2 : 16;
        G_funcs.f = (awk_func_t *)_awk_xrealloc(G_funcs.f, G_funcs.cap * sizeof(awk_func_t));
    }
    awk_func_t *fn = &G_funcs.f[G_funcs.count++];
    memset(fn, 0, sizeof(*fn));
    fn->name = fname;
    fn->params = (char **)malloc(sizeof(char *) * (size_t)total);
    fn->is_array_param = (int *)malloc(sizeof(int) * (size_t)total);
    int wi = 0;
    for (int i = 0; i < np; i++) {
        if (!is_local[i]) {
            fn->params[wi] = params[i];
            fn->is_array_param[wi] = is_arr[i];
            wi++;
        }
    }
    for (int i = 0; i < np; i++) {
        if (is_local[i]) {
            fn->params[wi] = params[i];
            fn->is_array_param[wi] = is_arr[i];
            wi++;
        }
    }
    fn->nparams = nparams;
    fn->nlocals = nlocals;
    size_t blen = body_end - body_start;
    fn->body = _awk_xstrndup(l->src + body_start, blen > 0 ? blen - 1 : 0);
    fn->body_len = blen;
    fn->body_start = body_start;
}

static void _awk_parse_program(const char *source)
{
    awk_lex_t l;
    awk_lex_init(&l, source);
    G_program_lex = &l;

    for (;;) {
        awk_lex_t save_lex = l;
        awk_tok_t t; _awk_lex_peek(&l, &t);
        if (t.type == TOK_EOF) { _awk_tok_free(&t); break; }

        if (t.type == TOK_FUNCTION) {
            _awk_lex_next(&l, &t); _awk_tok_free(&t);
            _awk_parse_function_def(&l);
            continue;
        }

        if (t.type == TOK_BEGIN) {
            _awk_lex_next(&l, &t); _awk_tok_free(&t);
            awk_tok_t lb; _awk_lex_next(&l, &lb);
            if (lb.type != TOK_LBRACE) _awk_eval_error((awk_eval_ctx_t*)0, "expected '{' after BEGIN");
            _awk_tok_free(&lb);
            size_t bs = l.pos; int depth = 1;
            while (depth > 0) {
                awk_tok_t t2; _awk_lex_next(&l, &t2);
                if (t2.type == TOK_LBRACE) depth++;
                else if (t2.type == TOK_RBRACE) { depth--; if (depth == 0) { _awk_tok_free(&t2); break; } }
                else if (t2.type == TOK_EOF) _awk_eval_error((awk_eval_ctx_t*)0, "unterminated BEGIN");
                _awk_tok_free(&t2);
            }
            size_t blen = l.pos - bs;
            char *act = _awk_xstrndup(source + bs, blen > 0 ? blen - 1 : 0);
            _awk_rule_add(RULE_BEGIN, NULL, NULL, act, bs, blen);
            free(act);
            continue;
        }

        if (t.type == TOK_END) {
            _awk_lex_next(&l, &t); _awk_tok_free(&t);
            awk_tok_t lb; _awk_lex_next(&l, &lb);
            if (lb.type != TOK_LBRACE) _awk_eval_error((awk_eval_ctx_t*)0, "expected '{' after END");
            _awk_tok_free(&lb);
            size_t bs = l.pos; int depth = 1;
            while (depth > 0) {
                awk_tok_t t2; _awk_lex_next(&l, &t2);
                if (t2.type == TOK_LBRACE) depth++;
                else if (t2.type == TOK_RBRACE) { depth--; if (depth == 0) { _awk_tok_free(&t2); break; } }
                else if (t2.type == TOK_EOF) _awk_eval_error((awk_eval_ctx_t*)0, "unterminated END");
                _awk_tok_free(&t2);
            }
            size_t blen = l.pos - bs;
            char *act = _awk_xstrndup(source + bs, blen > 0 ? blen - 1 : 0);
            _awk_rule_add(RULE_END, NULL, NULL, act, bs, blen);
            free(act);
            continue;
        }

        size_t pat1_start = l.pos;
        (void)pat1_start;
        char *pat1 = NULL;
        int has_pat = 0;

        awk_tok_t first_tok; _awk_lex_peek(&l, &first_tok);
        if (first_tok.type == TOK_LBRACE) {
            _awk_tok_free(&first_tok);
            has_pat = 0;
        } else {
            has_pat = 1;
            l = save_lex;
            _awk_lex_invalidate_peek(&l);
            int found_comma = 0;
            int pdep = 0;
            size_t p1s = l.pos;
            while (1) {
                awk_tok_t cx; _awk_lex_next(&l, &cx);
                if (cx.type == TOK_LPAREN || cx.type == TOK_LBRACK) pdep++;
                else if (cx.type == TOK_RPAREN || cx.type == TOK_RBRACK) { if (pdep > 0) pdep--; }
                else if (cx.type == TOK_COMMA && pdep == 0) { found_comma = 1; _awk_tok_free(&cx); break; }
                else if (cx.type == TOK_LBRACE || cx.type == TOK_EOF) {
                    _awk_lex_invalidate_peek(&l);
                    size_t back = cx.start;
                    _awk_tok_free(&cx);
                    l = save_lex;
                    _awk_lex_invalidate_peek(&l);
                    while (l.pos < back) {
                        int c2 = (l.pos < l.end) ? (unsigned char)l.src[l.pos] : -1;
                        if (c2 == '\n') l.line++;
                        l.pos++;
                    }
                    _awk_lex_invalidate_peek(&l);
                    break;
                }
                _awk_tok_free(&cx);
            }
            size_t p1e = l.pos;
            if (found_comma) {
                size_t csave = l.pos;
                awk_lex_t l2 = save_lex;
                _awk_lex_invalidate_peek(&l2);
                while (l2.pos < p1e - 1) {
                    int c2 = (l2.pos < l2.end) ? (unsigned char)l2.src[l2.pos] : -1;
                    if (c2 == '\n') l2.line++;
                    l2.pos++;
                }
                (void)l2;
                pat1 = _awk_xstrndup(source + p1s, (p1e - 1) - p1s);
                awk_tok_t pt2;
                size_t p2s = l.pos;
                while (1) {
                    _awk_lex_next(&l, &pt2);
                    if (pt2.type == TOK_LBRACE || pt2.type == TOK_EOF) {
                        _awk_lex_invalidate_peek(&l);
                        size_t back2 = pt2.start;
                        _awk_tok_free(&pt2);
                        l.pos = csave;
                        _awk_lex_invalidate_peek(&l);
                        while (l.pos < back2) {
                            int c3 = (l.pos < l.end) ? (unsigned char)l.src[l.pos] : -1;
                            if (c3 == '\n') l.line++;
                            l.pos++;
                        }
                        _awk_lex_invalidate_peek(&l);
                        break;
                    }
                    _awk_tok_free(&pt2);
                }
                (void)p2s;
                char *pat2 = _awk_xstrndup(source + csave, (l.pos) - csave);
                (void)pat2;
                awk_tok_t lb2; _awk_lex_next(&l, &lb2);
                if (lb2.type != TOK_LBRACE) _awk_eval_error((awk_eval_ctx_t*)0, "expected '{'");
                _awk_tok_free(&lb2);
                size_t bs = l.pos; int dp2 = 1;
                while (dp2 > 0) {
                    awk_tok_t t3; _awk_lex_next(&l, &t3);
                    if (t3.type == TOK_LBRACE) dp2++;
                    else if (t3.type == TOK_RBRACE) { dp2--; if (dp2 == 0) { _awk_tok_free(&t3); break; } }
                    else if (t3.type == TOK_EOF) _awk_eval_error((awk_eval_ctx_t*)0, "unterminated range action");
                    _awk_tok_free(&t3);
                }
                size_t blen = l.pos - bs;
                char *act = _awk_xstrndup(source + bs, blen > 0 ? blen - 1 : 0);
                _awk_rule_add(RULE_RANGE, pat1, pat2, act, bs, blen);
                free(act); free(pat1); free(pat2);
                continue;
            } else {
                pat1 = _awk_xstrndup(source + p1s, p1e - p1s);
            }
            _awk_tok_free(&first_tok);
        }

        if (has_pat) _awk_lex_invalidate_peek(&l);
        awk_tok_t lb3; _awk_lex_next(&l, &lb3);
        if (lb3.type != TOK_LBRACE) {
            if (has_pat) {
                /* Pattern without action: default action is { print } */
                _awk_tok_free(&lb3);
                _awk_rule_add(RULE_PAT_ACTION, pat1, NULL, "print", 0, 5);
                free(pat1);
                continue;
            }
            _awk_tok_free(&lb3);
            continue;
        }
        _awk_tok_free(&lb3);
        size_t bs = l.pos; int dp3 = 1;
        while (dp3 > 0) {
            awk_tok_t t4; _awk_lex_next(&l, &t4);
            if (t4.type == TOK_LBRACE) dp3++;
            else if (t4.type == TOK_RBRACE) { dp3--; if (dp3 == 0) { _awk_tok_free(&t4); break; } }
            else if (t4.type == TOK_EOF) _awk_eval_error((awk_eval_ctx_t*)0, "unterminated action");
            _awk_tok_free(&t4);
        }
        size_t blen = l.pos - bs;
        char *act = _awk_xstrndup(source + bs, blen > 0 ? blen - 1 : 0);
        _awk_rule_add(has_pat ? RULE_PAT_ACTION : RULE_PAT_ACTION,
                 has_pat ? pat1 : NULL, NULL, act, bs, blen);
        free(act);
        free(pat1);
    }
}

/* -------------------------- execute rules ------------------------------ */

static void _awk_execute_action(const char *action_src)
{
    if (!action_src) return;
    if (getenv("AWK_DEBUG_ACTION")) {
        awk_err_printf("[action] src=[%s]\n", action_src);
    }
    awk_lex_t l;
    awk_lex_init(&l, action_src);
    awk_eval_ctx_t ctx; ctx.lex = &l;
    _awk_eval_stmt_list(&ctx);
}

static void _awk_execute_begin_rules(void)
{
    for (size_t i = 0; i < G_rules_count; i++) {
        if (G_rules[i].kind == RULE_BEGIN) {
            _awk_execute_action(G_rules[i].action);
            if (G_exit) return;
        }
    }
}

static void _awk_execute_end_rules(void)
{
    for (size_t i = 0; i < G_rules_count; i++) {
        if (G_rules[i].kind == RULE_END) {
            _awk_execute_action(G_rules[i].action);
            if (G_exit) return;
        }
    }
}

static void _awk_execute_pattern_rules(void)
{
    for (size_t i = 0; i < G_rules_count; i++) {
        if (G_rules[i].kind == RULE_PAT_ACTION) {
            if (!G_rules[i].pattern1) {
                _awk_execute_action(G_rules[i].action);
                if (G_exit || G_next || G_nextfile) return;
                continue;
            }
            awk_lex_t l;
            awk_lex_init(&l, G_rules[i].pattern1);
            awk_eval_ctx_t ctx; ctx.lex = &l;
            awk_val_t cond = _awk_eval_expr(&ctx);
            int ok = _awk_v_true(&cond);
            _awk_v_clear(&cond);
            if (ok) {
                _awk_execute_action(G_rules[i].action);
                if (G_exit || G_next || G_nextfile) return;
            }
        } else if (G_rules[i].kind == RULE_RANGE) {
            if (i >= G_range_cap) {
                G_range_cap = G_range_cap ? G_range_cap * 2 : G_rules_count + 16;
                G_range_active = (int *)_awk_xrealloc(G_range_active, G_range_cap * sizeof(int));
                for (size_t j = G_range_count; j < G_range_cap; j++) G_range_active[j] = 0;
            }
            G_range_count = G_rules_count;
            awk_lex_t l1; awk_val_t c1; int ok1 = 0;
            if (G_rules[i].pattern1) {
                awk_lex_init(&l1, G_rules[i].pattern1);
                awk_eval_ctx_t ctx1; ctx1.lex = &l1;
                c1 = _awk_eval_expr(&ctx1);
                ok1 = _awk_v_true(&c1);
                _awk_v_clear(&c1);
            }
            awk_lex_t l2; awk_val_t c2; int ok2 = 0;
            if (G_rules[i].pattern2) {
                awk_lex_init(&l2, G_rules[i].pattern2);
                awk_eval_ctx_t ctx2; ctx2.lex = &l2;
                c2 = _awk_eval_expr(&ctx2);
                ok2 = _awk_v_true(&c2);
                _awk_v_clear(&c2);
            }
            int active = G_range_active[i];
            if (!active && ok1) active = 1;
            if (active) {
                _awk_execute_action(G_rules[i].action);
                if (ok2) active = 0;
                G_range_active[i] = active;
                if (G_exit || G_next || G_nextfile) return;
            } else {
                G_range_active[i] = 0;
            }
        }
    }
}

/* -------------------------- main() entry point ------------------------- */

static void _awk_init_builtin_vars(int argc, char **argv)
{
    _awk_vm_set_s("SUBSEP", "\034");
    _awk_vm_set_s("FS", " ");
    _awk_vm_set_s("OFS", " ");
    _awk_vm_set_s("ORS", "\n");
    _awk_vm_set_s("OFMT", "%.6g");
    _awk_vm_set_s("CONVFMT", "%.6g");
    _awk_vm_set_n("IGNORECASE", 0.0);
    _awk_vm_set_s("FIELDWIDTHS", "");
    _awk_vm_set_s("FPAT", "");
    _awk_vm_set_n("NF", 0.0);
    _awk_vm_set_n("NR", 0.0);
    _awk_vm_set_n("FNR", 0.0);
    _awk_vm_set_n("RLENGTH", -1.0);
    _awk_vm_set_n("RSTART", 0.0);
    if (G_ERRNO) { free(G_ERRNO); G_ERRNO = NULL; }
    _awk_vm_set_s("ERRNO", "");

    awk_array_t *argc_arr = _awk_arr_find("ARGV", 1);
    if (argc_arr) _awk_arr_clear(argc_arr);
    awk_array_t *env = _awk_arr_find("ENVIRON", 1);
    if (env) _awk_arr_clear(env);
    awk_array_t *proc = _awk_arr_find("PROCINFO", 1);
    if (proc) _awk_arr_clear(proc);

    G_argc = argc;
    G_argv = argv;
    _awk_vm_set_n("ARGC", (double)argc);
    if (argc_arr) {
        for (int i = 0; i < argc; i++) {
            char k[32]; snprintf(k, sizeof k, "%d", i);
            awk_val_t *vp = _awk_arr_get_val(argc_arr, k, 1);
            _awk_v_set_s(vp, argv[i] ? argv[i] : "");
        }
    }
    _awk_vm_set_n("ARGIND", 0.0);

#ifdef AWK_PLATFORM_WINDOWS
    {
        extern char **_environ;
        char **envp = _environ;
        if (envp) {
            for (int i = 0; envp[i]; i++) {
                char *e = envp[i];
                char *eq = strchr(e, '=');
                if (eq) {
                    size_t nlen = (size_t)(eq - e);
                    char *name = _awk_xstrndup(e, nlen);
                    const char *val = eq + 1;
                    awk_val_t *vp = _awk_arr_get_val(env, name, 1);
                    _awk_v_set_s(vp, val);
                    free(name);
                }
            }
        }
    }
#else
    if (environ) {
        for (int i = 0; environ[i]; i++) {
            char *e = environ[i];
            char *eq = strchr(e, '=');
            if (eq) {
                size_t nlen = (size_t)(eq - e);
                char *name = _awk_xstrndup(e, nlen);
                const char *val = eq + 1;
                awk_val_t *vp = _awk_arr_get_val(env, name, 1);
                _awk_v_set_s(vp, val);
                free(name);
            }
        }
    }
#endif

    if (proc) {
        char buf[64];
        snprintf(buf, sizeof buf, "%ld", (long)getpid());
        awk_val_t *vp = _awk_arr_get_val(proc, "pid", 1);
        _awk_v_set_s(vp, buf);
#ifdef AWK_PLATFORM_POSIX
        snprintf(buf, sizeof buf, "%ld", (long)getppid());
        vp = _awk_arr_get_val(proc, "ppid", 1);
        _awk_v_set_s(vp, buf);
        snprintf(buf, sizeof buf, "%ld", (long)getuid());
        vp = _awk_arr_get_val(proc, "uid", 1);
        _awk_v_set_s(vp, buf);
        snprintf(buf, sizeof buf, "%ld", (long)geteuid());
        vp = _awk_arr_get_val(proc, "euid", 1);
        _awk_v_set_s(vp, buf);
        snprintf(buf, sizeof buf, "%ld", (long)getgid());
        vp = _awk_arr_get_val(proc, "gid", 1);
        _awk_v_set_s(vp, buf);
#endif
        vp = _awk_arr_get_val(proc, "version", 1);
        _awk_v_set_s(vp, AWK_VERSION_STR);
    }
}

static void _awk_process_file(const char *fn)
{
    FILE *f = stdin;
    if (fn && *fn && strcmp(fn, "-") != 0) {
        f = fopen(fn, "r");
        if (!f) {
            if (G_ERRNO) free(G_ERRNO);
            G_ERRNO = _awk_xstrdup(fn ? fn : "");
            return;
        }
    }
    if (G_FILENAME) { free(G_FILENAME); G_FILENAME = NULL; }
    G_FILENAME = _awk_xstrdup(fn ? fn : "-");
    G_FNR = 0;
    _awk_vm_set_s("FILENAME", G_FILENAME ? G_FILENAME : "");
    G_nextfile = 0;
    while (!G_exit && !G_nextfile) {
        if (!_awk_read_record_from(f)) break;
        G_NR++; G_FNR++;
        _awk_vm_set_n("NR", (double)G_NR);
        _awk_vm_set_n("FNR", (double)G_FNR);
        _awk_split_line(G_line);
        size_t cnt = 0;
        if (G_fields_count > 1) cnt = G_fields_count - 1;
        _awk_vm_set_n("NF", (double)cnt);
        G_next = 0;
        _awk_execute_pattern_rules();
    }
    if (f != stdin) fclose(f);
}

/**
 * @brief Print version information
 */
static void _awk_print_version(void)
{
    awk_printf("awk %s\n", AWK_VERSION_STR);
    awk_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    awk_printf("%s", "License MIT: <https://mit-license.org/>\n");
    awk_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    awk_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Print usage/help information
 */
static void _awk_usage(void)
{
    awk_printf("Usage: awk [options] 'program' [file ...]\n");
    awk_printf("       awk [options] -f progfile [--] [file ...]\n");
    awk_printf("%s", "Options:\n");
    awk_printf("  -f progfile   Read program from file\n");
    awk_printf("  -F fs         Field separator\n");
    awk_printf("  -v var=val    Assign variable\n");
    awk_printf("  --version     Show version\n");
    awk_printf("  --help        Show this help\n");
}

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the awk command
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, non-zero on error
 */
int main(int argc, char **argv)
{
    const char *progfile = NULL;
    const char *inline_prog = NULL;
    const char *fs_sep = NULL;
    const char **vars_names = NULL;
    const char **vars_vals = NULL;
    int vars_count = 0, vars_cap = 0;
    int show_version = 0, show_help = 0;
    int arg_i = 1;
    for (; arg_i < argc; arg_i++) {
        const char *a = argv[arg_i];
        if (!a) break;
        if (a[0] != '-' || strcmp(a, "-") == 0) break;
        if (strcmp(a, "--") == 0) { arg_i++; break; }
        if (strcmp(a, "--version") == 0) { show_version = 1; continue; }
        if (strcmp(a, "--help") == 0) { show_help = 1; continue; }
        if (strcmp(a, "-f") == 0) {
            if (arg_i + 1 >= argc) _awk_xdie("-f requires argument");
            progfile = argv[++arg_i]; continue;
        }
        if (strncmp(a, "-f", 2) == 0 && a[2]) { progfile = a + 2; continue; }
        if (strcmp(a, "-F") == 0) {
            if (arg_i + 1 >= argc) _awk_xdie("-F requires argument");
            fs_sep = argv[++arg_i]; continue;
        }
        if (strncmp(a, "-F", 2) == 0 && a[2]) { fs_sep = a + 2; continue; }
        if (strcmp(a, "-v") == 0) {
            if (arg_i + 1 >= argc) _awk_xdie("-v requires argument");
            const char *assign = argv[++arg_i];
            const char *eq = strchr(assign, '=');
            if (!eq) _awk_xdie("-v expects var=value");
            if (vars_count == vars_cap) {
                vars_cap = vars_cap ? vars_cap * 2 : 8;
                vars_names = (const char **)_awk_xrealloc((void*)vars_names, vars_cap * sizeof(char *));
                vars_vals  = (const char **)_awk_xrealloc((void*)vars_vals, vars_cap * sizeof(char *));
            }
            char *nm = _awk_xstrndup(assign, (size_t)(eq - assign));
            vars_names[vars_count] = nm;
            vars_vals[vars_count] = eq + 1;
            vars_count++;
            continue;
        }
        if (a[0] == '-' && a[1] == '-') {
            awk_err_printf("awk: unknown option '%s'\n", a);
            return 2;
        }
        if (a[0] == '-' && a[1] && !strchr("fvWF:", a[1])) {
            awk_err_printf("awk: unknown option '%s'\n", a);
            return 2;
        }
        break;
    }
    if (show_version) {
        _awk_print_version();
        return 0;
    }
    if (show_help) { _awk_usage();
        return 0;
    }

    char *program_src = NULL;
    if (progfile) {
        FILE *fp = fopen(progfile, "rb");
        if (!fp) _awk_xdie("cannot open program file '%s'", progfile);
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
        if (sz < 0) sz = 0;
        program_src = (char *)malloc((size_t)sz + 1);
        if (program_src) {
            size_t rd = fread(program_src, 1, (size_t)sz, fp);
            program_src[rd] = 0;
        }
        fclose(fp);
    } else if (arg_i < argc) {
        inline_prog = argv[arg_i++];
        program_src = _awk_xstrdup(inline_prog ? inline_prog : "");
    } else {
        _awk_xdie("no program source specified");
    }

    memset(&G_vm, 0, sizeof G_vm);
    memset(&G_arrays, 0, sizeof G_arrays);
    memset(&G_funcs, 0, sizeof G_funcs);
    memset(G_fh_table, 0, sizeof G_fh_table);
    G_scope_sp = 0;
    memset(G_scope_stack, 0, sizeof G_scope_stack);
    G_rules = NULL; G_rules_count = 0; G_rules_cap = 0;
    G_NR = 0; G_FNR = 0;
    G_exit = 0; G_exit_code = 0;
    G_seeded_rand = 0;
    G_fields = NULL; G_fields_count = 0; G_fields_cap = 0;
    G_line = NULL; G_line_cap = 0; G_line_len = 0;
    G_next = G_break = G_cont = G_nextfile = G_return = 0;
    G_return_value = _awk_v_init();
    G_range_active = NULL; G_range_count = 0; G_range_cap = 0;
    G_regcount = 0; memset(G_regcache, 0, sizeof G_regcache);
    G_argind = 0;
    G_FS = NULL; G_OFS = NULL; G_ORS = NULL; G_RS = NULL; G_FILENAME = NULL; G_ERRNO = NULL;

    int file_argc = 1 + (argc - arg_i);
    char **file_argv = (char **)malloc(sizeof(char *) * (size_t)(file_argc + 1));
    file_argv[0] = _awk_xstrdup("awk");
    for (int j = 1; j < file_argc; j++) {
        file_argv[j] = _awk_xstrdup(argv[arg_i + j - 1] ? argv[arg_i + j - 1] : "");
    }
    file_argv[file_argc] = NULL;

    _awk_init_builtin_vars(file_argc, file_argv);

    if (fs_sep) {
        _awk_vm_set_s("FS", fs_sep);
        G_FS = _awk_xstrdup(fs_sep);
    } else {
        G_FS = _awk_xstrdup(_awk_vm_get_s("FS"));
    }
    G_OFS = _awk_xstrdup(_awk_vm_get_s("OFS"));
    G_ORS = _awk_xstrdup(_awk_vm_get_s("ORS"));

    for (int vi = 0; vi < vars_count; vi++) {
        _awk_vm_set_s(vars_names[vi], vars_vals[vi]);
    }

    _awk_parse_program(program_src);

    _awk_execute_begin_rules();

    if (!G_exit) {
        if (file_argc <= 1) {
            G_argind = 0;
            _awk_vm_set_n("ARGIND", 0.0);
            _awk_process_file(NULL);
        } else {
            for (int fi = 1; fi < file_argc && !G_exit; fi++) {
                G_argind = fi;
                _awk_vm_set_n("ARGIND", (double)fi);
                _awk_process_file(file_argv[fi]);
            }
        }
    }

    _awk_execute_end_rules();

    if (G_FS) free(G_FS);
    if (G_OFS) free(G_OFS);
    if (G_ORS) free(G_ORS);
    if (G_FILENAME) free(G_FILENAME);
    if (G_ERRNO) free(G_ERRNO);
    for (int i = 0; i < AWK_MAX_OPEN_FILES; i++) {
        if (G_fh_table[i].kind != AWK_FH_NONE) fh_close(&G_fh_table[i]);
    }
    free(G_line);
    for (size_t i = 0; i < G_fields_count; i++) free(G_fields[i]);
    free(G_fields);
    for (size_t i = 0; i < G_rules_count; i++) {
        free(G_rules[i].pattern1);
        free(G_rules[i].pattern2);
        free(G_rules[i].action);
    }
    free(G_rules);
    free(G_range_active);
    for (size_t i = 0; i < G_regcount; i++) {
        free(G_regcache[i].pat);
        if (G_regcache[i].compiled && G_regcache[i].re) awk_regfree(G_regcache[i].re);
    }
    free(program_src);
    for (size_t i = 0; i < G_vm.count; i++) {
        free(G_vm.v[i].name);
        free(G_vm.v[i].s);
        if (G_vm.v[i].arr) {
            _awk_arr_clear(G_vm.v[i].arr);
            free(G_vm.v[i].arr->name);
            free(G_vm.v[i].arr->keys);
            free(G_vm.v[i].arr);
        }
    }
    free(G_vm.v);
    for (size_t i = 0; i < G_funcs.count; i++) {
        free(G_funcs.f[i].name);
        for (int j = 0; j < G_funcs.f[i].nparams + G_funcs.f[i].nlocals; j++) {
            if (j < G_funcs.f[i].nparams || (G_funcs.f[i].params && G_funcs.f[i].params[j]))
                free(G_funcs.f[i].params[j]);
        }
        free(G_funcs.f[i].params);
        free(G_funcs.f[i].is_array_param);
        free(G_funcs.f[i].body);
    }
    free(G_funcs.f);
    for (int i = 0; i < file_argc; i++) free(file_argv[i]);
    free(file_argv);
    for (int vi = 0; vi < vars_count; vi++) free((void*)vars_names[vi]);
    free((void*)vars_names);
    free((void*)vars_vals);
    return G_exit_code;
}
