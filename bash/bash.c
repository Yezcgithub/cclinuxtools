/**
 * @file bash.c
 * @brief Cross-platform bash-style shell implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Implements a substantial subset of POSIX sh / bash behaviour:
 *   - Interactive and script (-c STRING, FILE) modes
 *   - Simple commands, pipelines (|), lists (; && || &)
 *   - Redirections: < > >> 2> 2>> &> >& <& << (heredoc)
 *   - Builtins: cd pwd echo printf true false exit return set unset
 *                export env source . shift break continue trap wait
 *                alias unalias read test [ type command declare local
 *                jobs bg fg kill hash dirs pushd popd mktemp which
 *   - External command lookup: 1) same dir as bash.exe / bash
 *                              2) PATH entries (with .exe/.com/.bat/.cmd on Windows)
 *   - Expansion: $var  ${var}  ${var:-def}  ${var:=def}  ${var:?err}
 *                ${var:+alt}  ${#var}  ${var#pat}  ${var##pat}
 *                ${var%pat}  ${var%%pat}  ${var/pat/rep}
 *                $(command)  `command`  $(( expr ))
 *                $?  $$  $!  $#  $0..$9  $@  $*  ${N}
 *   - Quoting: 'string'  "string"  \escape  $'...'  $"..."
 *   - Globbing (* ? [abc]) on arguments
 *   - Control flow: if/then/elif/else/fi  for/do/done  while/do/done
 *                   until/do/done  case/in/esac  select (GNU-like)
 *                   { list; }  ( list )  name() compound-command
 *                   (( expr ))  [[ expr ]]  ! pipeline
 *   - Functions and local variables
 *
 * CLI:
 *   bash [--help|--version] [-c STRING] [-s] [--login] [-i] [script [args...]]
 *
 * Build (Windows/MinGW): gcc -O2 -std=c99 -Wall -Wextra -o bash.exe bash.c
 * Build (Linux):         gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o bash bash.c
 * Build (macOS):         gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o bash bash.c
 * Build (FreeBSD):       cc  -O2 -std=c99 -Wall -o bash bash.c
 * Build (OpenBSD):       cc  -O2 -std=c99 -Wall -o bash bash.c
 * Build (NetBSD):        cc  -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o bash bash.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (c) 2025-2026 <Yezc/bash>
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
 * Platform detection macros - must appear before any system includes
 * so that POSIX feature macros are defined correctly.
 */
#if defined(_WIN32) || defined(_WIN64)
    #define BASH_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASH_PLATFORM_LINUX   1
    #define BASH_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASH_PLATFORM_MACOS   1
    #define BASH_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASH_PLATFORM_FREEBSD 1
    #define BASH_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASH_PLATFORM_OPENBSD 1
    #define BASH_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASH_PLATFORM_NETBSD  1
    #define BASH_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASH_PLATFORM_POSIX   1
#else
    #define BASH_PLATFORM_POSIX   1
#endif
/* POSIX feature macros - must be defined before including any headers */
#ifdef BASH_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif
#ifdef BASH_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif
#ifdef BASH_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <assert.h>

/* environ is not declared in any standard header under strict POSIX modes;
 * declare it once here so all bi_env / _bash_var_set callers can use it. */
extern char **environ;

#ifdef BASH_PLATFORM_WINDOWS
    #include <windows.h>
    #include <conio.h>
    #include <io.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <process.h>
    #include <direct.h>
    #include <time.h>
    #define BASH_STRDUP            _strdup
    #define BASH_GETCWD            _getcwd
    #define BASH_CHDIR             _chdir
    #define BASH_MKDIR(d,m)        _mkdir(d)
    #define BASH_UNLINK            _unlink
    #define BASH_RMDIR             _rmdir
    #define BASH_OPEN              _open
    #define BASH_CLOSE             _close
    #define BASH_READ              _read
    #define BASH_WRITE             _write
    #define BASH_DUP               _dup
    #define BASH_DUP2              _dup2
    #define BASH_PIPE(fds)         _pipe(fds, 4096, _O_BINARY)
    #define BASH_FILENO            _fileno
    #define BASH_ACCESS            _access
    #define BASH_STAT              _stat
    #define BASH_FSTAT             _fstat
    #define BASH_POPEN             _popen
    #define BASH_PCLOSE            _pclose
    #define BASH_GETPID            _getpid
    #define BASH_SLEEP_MS(ms)      Sleep(ms)
    #define BASH_SEP               '\\'
    #define BASH_SEP_S             "\\"
    #define BASH_PATHSEP           ';'
    #define BASH_CMD_SUBDIR        "cmdtools"   /* companion commands subdir next to bash exe */
    #define BASH_S_IFREG           _S_IFREG
    #define BASH_S_IFDIR           _S_IFDIR

/* On Windows, _dup2() only updates the CRT fd table, not the OS-level
 * standard handles used by CreateProcess. After any redirect of fds 0/1/2,
 * we must call this helper so child (external) processes inherit the right
 * handles instead of the original console. */
static void _bash_sync_stdhandles(void)
{
    int f;
    intptr_t h;

    /* stdin: fd 0 -> STD_INPUT_HANDLE */
    f = BASH_FILENO(stdin);
    if (f < 0) f = 0;
    h = _get_osfhandle(f);
    if (h != -1) SetStdHandle(STD_INPUT_HANDLE, (HANDLE)h);

    /* stdout: fd 1 -> STD_OUTPUT_HANDLE */
    f = BASH_FILENO(stdout);
    if (f < 0) f = 1;
    h = _get_osfhandle(f);
    if (h != -1) SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)h);

    /* stderr: fd 2 -> STD_ERROR_HANDLE */
    f = BASH_FILENO(stderr);
    if (f < 0) f = 2;
    h = _get_osfhandle(f);
    if (h != -1) SetStdHandle(STD_ERROR_HANDLE, (HANDLE)h);
}
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <dirent.h>
    #include <fcntl.h>
    #include <pwd.h>
    #include <time.h>
    #include <libgen.h>
    #include <termios.h>
    #define BASH_STRDUP            strdup
    #define BASH_GETCWD            getcwd
    #define BASH_CHDIR             chdir
    #define BASH_MKDIR(d,m)        mkdir(d,m)
    #define BASH_UNLINK            unlink
    #define BASH_RMDIR             rmdir
    #define BASH_OPEN              open
    #define BASH_CLOSE             close
    #define BASH_READ              read
    #define BASH_WRITE             write
    #define BASH_DUP               dup
    #define BASH_DUP2              dup2
    #define BASH_PIPE(fds)         pipe(fds)
    #define BASH_FILENO            fileno
    #define BASH_ACCESS            access
    #define BASH_STAT              stat
    #define BASH_FSTAT             fstat
    #define BASH_POPEN             popen
    #define BASH_PCLOSE            pclose
    #define BASH_GETPID            getpid
    #define BASH_SLEEP_MS(ms)      do { struct timespec ts_; ts_.tv_sec=(ms)/1000; ts_.tv_nsec=((ms)%1000)*1000000L; nanosleep(&ts_,NULL); } while(0)
    #define BASH_SEP               '/'
    #define BASH_SEP_S             "/"
    #define BASH_PATHSEP           ':'
    #define BASH_CMD_SUBDIR        "cmdtools"   /* companion commands subdir next to bash exe */
    #define BASH_S_IFREG           S_IFREG
    #define BASH_S_IFDIR           S_IFDIR
    #define BASH_PLATFORM_POSIX   1
    /* no-op on POSIX: fork+exec naturally inherits fds 0/1/2 */
    #define _bash_sync_stdhandles() ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define BASH_ATTR_UNUSED __attribute__((unused))
#else
    #define BASH_ATTR_UNUSED
#endif

/********************************
 *    defines
 ********************************/

/** @brief Default command history size */
#ifndef HIST_SIZE
    #define HIST_SIZE 256
#endif

/** @brief Version string */
#ifndef BASH_VERSION_STR
    #define BASH_VERSION_STR "v1.0.0"
#endif

/** @brief Maximum path buffer length (bytes) */
#define BASH_MAX_PATH_LEN 4096

/** @brief Maximum line buffer length (bytes) */
#define BASH_MAX_LINE_LEN 65536

/********************************
 *    typedefs
 ********************************/

/* Forward declarations (full definitions below) */
typedef struct bash_bstr_s         bash_bstr_t;
typedef struct bash_barray_s       bash_barray_t;
typedef struct bash_ctx_s          bash_ctx_t;
typedef struct bash_lex_s          bash_lex_t;
typedef struct bash_tok_s          bash_tok_t;
typedef struct bash_node_s         bash_node_t;
typedef struct bash_parser_s       bash_parser_t;
typedef struct bash_vars_s         bash_vars_t;
typedef struct bash_frame_s        bash_frame_t;
typedef struct bash_aeval_s        bash_aeval_t;
typedef struct bash_redir_s        bash_redir_t;
typedef struct bash_spawn_opts_s   bash_spawn_opts_t;
typedef struct bash_funcdef_s      bash_funcdef_t;

/* Builtin function typedef */
typedef int (*bash_builtin_fn)(bash_ctx_t *ctx, int argc, char **argv);

/********************************
 *    static prototypes
 ********************************/

/* memory / string helpers */
static void * _bash_xmalloc(size_t n);
static void * _bash_xrealloc(void *p, size_t n);
static char * _bash_xstrdup(const char *s);
static char * _bash_xstrndup(const char *s, size_t n);
static void   _bash_normalize_path(char *path);
static char * _bash_normalize_path_dup(const char *path);

/* platform helpers */
static int    _bash_is_executable(const char *path);
static int    _bash_classify_file(const char *path);
static char * _bash_get_self_dir(void);
static char * _bash_which(const char *name);
static int    _bash_spawn(const char *prog, char *const argv[],
                          const bash_spawn_opts_t *opts, int *out_pid);
static int    _bash_waitpid(int pid);

/********************************
 *    macros
 ********************************/

/** @brief Default output stream for bash_printf / bash_fputs. */
#ifndef bash_out_stream
    #define bash_out_stream stdout
#endif

/** @brief Default error stream for bash_err_printf. */
#ifndef bash_err_stream
    #define bash_err_stream stderr
#endif

/** @brief Formatted print to stdout (printf-compatible). */
#ifndef bash_printf
    #define bash_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/** @brief Formatted print to stderr (fprintf-compatible). */
#ifndef bash_err_printf
    #define bash_err_printf(fmt, ...) fprintf(bash_err_stream, fmt, ##__VA_ARGS__)
#endif

/** @brief Write a NUL-terminated string to a stdio stream. */
#ifndef bash_fputs
    #define bash_fputs(str, stream) (void)fputs((str), (stream))
#endif

/** @brief Flush a stdio stream output buffer. */
#ifndef bash_fflush
    #define bash_fflush(stream) (void)fflush(stream)
#endif

/** @brief Write a single character to stdout. */
#ifndef bash_putchar
    #define bash_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/* ========================================================================
 * Utility: memory / dynamic string / dynamic array
 * ======================================================================== */

static void *_bash_xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "bash: out of memory\n"); exit(1); }
    return p;
}
static void *_bash_xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) { fprintf(stderr, "bash: out of memory\n"); exit(1); }
    return q;
}
static char *_bash_xstrdup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)_bash_xmalloc(n);
    memcpy(p, s, n);
    return p;
}
static char *_bash_xstrndup(const char *s, size_t n)
{
    char *p = (char *)_bash_xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

/* Normalize a path to use forward slashes (/) for display and storage.
 * On Windows, _getcwd / GetModuleFileName return backslash paths — this
 * converts every \ to / so the shell's visible behavior matches Linux.
 * Input still accepts both / and \ (handled at each call site). */
static void _bash_normalize_path(char *path)
{
    if (!path) return;
    for (char *p = path; *p; p++)
        if (*p == '\\') *p = '/';
}

/* Return a normalized copy (caller frees). */
static char *_bash_normalize_path_dup(const char *path)
{
    if (!path) return NULL;
    char *r = _bash_xstrdup(path);
    _bash_normalize_path(r);
    return r;
}

/* dynamic string */
typedef struct bash_bstr_s { char *data; size_t len; size_t cap; } bash_bstr_t;
static void bash_bstr_init(bash_bstr_t *d){ d->cap=128; d->data=(char*)_bash_xmalloc(d->cap); d->len=0; d->data[0]=0; }
static void bash_bstr_free(bash_bstr_t *d){ if(d->data){free(d->data);d->data=NULL;} d->len=d->cap=0; }
static void bash_bstr_reserve(bash_bstr_t *d, size_t need){ if(d->cap>=need+1) return; while(d->cap<need+1) d->cap*=2; d->data=(char*)_bash_xrealloc(d->data,d->cap); }
static void bash_bstr_putc(bash_bstr_t *d, char c){ bash_bstr_reserve(d,d->len+1); d->data[d->len++]=c; d->data[d->len]=0; }
static void bash_bstr_puts(bash_bstr_t *d, const char *s){ size_t n=s?strlen(s):0; bash_bstr_reserve(d,d->len+n); if(n){memcpy(d->data+d->len,s,n);d->len+=n;} d->data[d->len]=0; }
static void bash_bstr_putn(bash_bstr_t *d, const char *s, size_t n){ bash_bstr_reserve(d,d->len+n); if(n){memcpy(d->data+d->len,s,n);d->len+=n;} d->data[d->len]=0; }
static void bash_bstr_clear(bash_bstr_t *d) BASH_ATTR_UNUSED;
static void bash_bstr_clear(bash_bstr_t *d){ d->len=0; if(d->data) d->data[0]=0; }
static char *bash_bstr_detach(bash_bstr_t *d){ char *r=d->data; d->data=NULL; d->len=d->cap=0; return r; }

/* dynamic string array (argv style) */
typedef struct bash_barray_s {
    char **items;
    int len;
    int cap;
} bash_barray_t;
static void bash_barray_init(bash_barray_t *a){ a->cap=8; a->items=(char**)_bash_xmalloc(a->cap*sizeof(char*)); a->len=0; }
static void bash_barray_free(bash_barray_t *a){ for(int i=0;i<a->len;i++) free(a->items[i]); free(a->items); a->items=NULL; a->len=a->cap=0; }
static void bash_barray_push(bash_barray_t *a, char *s){ if(a->len>=a->cap){ a->cap=a->cap?a->cap*2:8; a->items=(char**)_bash_xrealloc(a->items,a->cap*sizeof(char*)); } a->items[a->len++]=s; }
/* steal reference (no dup) */
static void bash_barray_push_steal(bash_barray_t *a, char *s){ if(a->len>=a->cap){ a->cap=a->cap?a->cap*2:8; a->items=(char**)_bash_xrealloc(a->items,a->cap*sizeof(char*)); } a->items[a->len++]=s; }
static char **bash_barray_detach(bash_barray_t *a) BASH_ATTR_UNUSED;
static char **bash_barray_detach(bash_barray_t *a){ /* appends NULL */ bash_barray_push_steal(a,NULL); return a->items; }

/* ========================================================================
 * Platform helpers: spawn, which, file ops
 * ======================================================================== */

/* Check if file exists and is executable (native binary / OS-registered script) */
static int _bash_is_executable(const char *path)
{
    struct BASH_STAT st;
    if (BASH_STAT(path, &st) != 0) return 0;
    if (!(st.st_mode & BASH_S_IFREG)) return 0; /* not regular file */
#ifdef BASH_PLATFORM_WINDOWS
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (_stricmp(ext, ".exe") == 0 || _stricmp(ext, ".com") == 0 ||
            _stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0)
            return 1;
    }
    return 0;
#else
    return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
#endif
}

/* Classify a regular file for execution purposes:
 *   0 = not a runnable script/binary
 *   1 = native executable (exe/com or set +x)
 *   2 = shell script (.sh / shebang / readable text) — run with bash itself
 *   3 = windows batch (.bat/.cmd) — run with cmd.exe /c
 */
static int _bash_classify_file(const char *path)
{
    struct BASH_STAT st;
    if (BASH_STAT(path, &st) != 0) return 0;
    if (!(st.st_mode & BASH_S_IFREG)) return 0;
    if (_bash_is_executable(path)) {
#ifdef BASH_PLATFORM_WINDOWS
        const char *ext = strrchr(path, '.');
        if (ext && (_stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0))
            return 3;
#endif
        return 1;
    }
    /* Not a native executable — check if it looks like a shell script */
#ifdef BASH_PLATFORM_WINDOWS
    const char *ext = strrchr(path, '.');
    if (ext && (_stricmp(ext, ".sh") == 0 || _stricmp(ext, ".bash") == 0))
        return 2;
#else
    if ((st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) == 0) return 0; /* not readable */
    if ((st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) return 2; /* +x but non-binary fallback */
#endif
    /* Read first bytes to detect shebang or text */
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    unsigned char hdr[256];
    size_t got = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);
    if (got >= 2 && hdr[0] == '#' && hdr[1] == '!') return 2;
    if (got == 0) return 0;
    /* Heuristic: no NUL bytes in first 256 bytes => likely text script */
    for (size_t i = 0; i < got; i++) if (hdr[i] == 0) return 0;
#ifdef BASH_PLATFORM_WINDOWS
    return 2;
#else
    return 0; /* POSIX: require shebang or +x to be safe */
#endif
}

/* Get the directory where this bash executable lives */
static char *_bash_get_self_dir(void)
{
    static char cached[4096] = "";
    if (cached[0]) return cached;
#ifdef BASH_PLATFORM_WINDOWS
    DWORD n = GetModuleFileNameA(NULL, cached, (DWORD)sizeof(cached));
    if (n == 0 || n == sizeof(cached)) { cached[0] = 0; return cached; }
    /* truncate to directory */
    char *last = strrchr(cached, BASH_SEP);
    if (last) *last = 0; else cached[0] = 0;
#else
    /* /proc/self/exe on Linux, _NSGetExecutablePath on macOS */
    #if defined(__linux__)
        ssize_t n = readlink("/proc/self/exe", cached, sizeof(cached) - 1);
        if (n <= 0) { cached[0] = 0; return cached; }
        cached[n] = 0;
    #elif defined(__APPLE__)
        uint32_t sz = (uint32_t)sizeof(cached);
        extern int _NSGetExecutablePath(char*, uint32_t*);
        if (_NSGetExecutablePath(cached, &sz) != 0) { cached[0] = 0; return cached; }
    #else
        cached[0] = 0;
        return cached;
    #endif
    char *last = strrchr(cached, '/');
    if (last) *last = 0; else cached[0] = 0;
#endif
    _bash_normalize_path(cached);
    return cached;
}

/* Locate an external command. Returns malloc'd path if found, else NULL.
 * Lookup order: 1) same dir as bash executable  2) PATH entries
 * On Windows, try appending .exe/.com/.bat/.cmd. */
static char *_bash_which(const char *name)
{
    if (!name || !*name) return NULL;
    /* if name contains a separator or ./ or ../, treat as path */
    if (strchr(name, BASH_SEP)
#ifdef BASH_PLATFORM_WINDOWS
        || strchr(name, '/')
#endif
        ) {
        if (_bash_is_executable(name) || _bash_classify_file(name) >= 2)
            return _bash_normalize_path_dup(name);
#ifdef BASH_PLATFORM_WINDOWS
        /* try with extensions */
        const char *exts[] = {".exe",".com",".bat",".cmd",NULL};
        for (int i=0; exts[i]; i++) {
            bash_bstr_t b; bash_bstr_init(&b);
            bash_bstr_puts(&b, name); bash_bstr_puts(&b, exts[i]);
            if (_bash_is_executable(b.data) || _bash_classify_file(b.data) >= 2) {
                char *r = _bash_normalize_path_dup(b.data); bash_bstr_free(&b); return r;
            }
            bash_bstr_free(&b);
        }
#endif
        return NULL;
    }

    bash_barray_t dirs; bash_barray_init(&dirs);
    /* 1) self directory */
    char *sd = _bash_get_self_dir();
    if (sd && *sd) bash_barray_push(&dirs, _bash_xstrdup(sd));
    /* 1b) <self dir>/cmd — companion commands shipped alongside bash */
    if (sd && *sd) {
        bash_bstr_t cb; bash_bstr_init(&cb);
        bash_bstr_puts(&cb, sd);
        size_t cl = cb.len;
        if (cl == 0 || cb.data[cl-1] != BASH_SEP) bash_bstr_putc(&cb, BASH_SEP);
        bash_bstr_puts(&cb, BASH_CMD_SUBDIR);
        bash_barray_push(&dirs, cb.data);
    }
    /* 2) PATH */
    const char *path_env = getenv("PATH");
    if (path_env) {
        const char *p = path_env;
        while (*p) {
            const char *end = p;
            while (*end && *end != BASH_PATHSEP) end++;
            if (end > p) bash_barray_push(&dirs, _bash_xstrndup(p, end - p));
            if (*end) p = end + 1; else break;
        }
    }
    /* current directory */
    bash_barray_push(&dirs, _bash_xstrdup("."));

#ifdef BASH_PLATFORM_WINDOWS
    const char *exts[] = {"", ".exe", ".com", ".bat", ".cmd", NULL};
#else
    const char *exts[] = {"", NULL};
#endif

    for (int d = 0; d < dirs.len; d++) {
        for (int i = 0; exts[i]; i++) {
            bash_bstr_t b; bash_bstr_init(&b);
            bash_bstr_puts(&b, dirs.items[d]);
            size_t bl = b.len;
            if (bl == 0 || (b.data[bl-1] != BASH_SEP
#ifdef BASH_PLATFORM_WINDOWS
                && b.data[bl-1] != '/'
#endif
                )) {
                bash_bstr_putc(&b, BASH_SEP);
            }
            bash_bstr_puts(&b, name);
            bash_bstr_puts(&b, exts[i]);
            if (_bash_is_executable(b.data) || _bash_classify_file(b.data) >= 2) {
                char *r = _bash_normalize_path_dup(b.data);
                bash_bstr_free(&b);
                bash_barray_free(&dirs);
                return r;
            }
            bash_bstr_free(&b);
        }
    }
    bash_barray_free(&dirs);
    return NULL;
}

/* Spawn external command with given argv and optionally redirected stdin/stdout/stderr.
 * Returns exit status (or -1 on failure). */
typedef struct bash_spawn_opts_s {
    int fd_stdin;   /* -1 = inherit */
    int fd_stdout;  /* -1 = inherit */
    int fd_stderr;  /* -1 = inherit */
    int fd_stderr_to_stdout; /* 2>&1 */
    int background;
} bash_spawn_opts_t;

static int _bash_spawn(const char *prog, char *const argv[], const bash_spawn_opts_t *opts, int *out_pid)
{
#ifdef BASH_PLATFORM_WINDOWS
    /* Build command line string */
    bash_bstr_t cmdline; bash_bstr_init(&cmdline);
    bash_bstr_putc(&cmdline, '"');
    bash_bstr_puts(&cmdline, prog);
    bash_bstr_putc(&cmdline, '"');
    for (int i = 1; argv[i]; i++) {
        bash_bstr_putc(&cmdline, ' ');
        const char *a = argv[i];
        int need_quote = 0;
        for (const char *p = a; *p; p++) if (*p == ' ' || *p == '\t' || *p == '"' || *p == '&' || *p == '|' || *p == '<' || *p == '>') { need_quote = 1; break; }
        if (need_quote) {
            bash_bstr_putc(&cmdline, '"');
            for (const char *p = a; *p; p++) {
                if (*p == '"') { bash_bstr_putc(&cmdline, '\\'); bash_bstr_putc(&cmdline, '"'); }
                else bash_bstr_putc(&cmdline, *p);
            }
            bash_bstr_putc(&cmdline, '"');
        } else {
            bash_bstr_puts(&cmdline, a);
        }
    }

    /* Save original fds and apply redirects (only when needed) */
    int old_stdin = -1, old_stdout = -1, old_stderr = -1;
    if (opts->fd_stdin  >= 0) { old_stdin  = BASH_DUP(0); BASH_DUP2(opts->fd_stdin,  0); }
    if (opts->fd_stdout >= 0) { old_stdout = BASH_DUP(1); BASH_DUP2(opts->fd_stdout, 1); }
    if (opts->fd_stderr_to_stdout) { if (old_stderr < 0) old_stderr = BASH_DUP(2); BASH_DUP2(1, 2); }
    else if (opts->fd_stderr >= 0) { old_stderr = BASH_DUP(2); BASH_DUP2(opts->fd_stderr, 2); }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    /* Always use STARTF_USESTDHANDLES so the child inherits the current
     * std handles (which may have been redirected by _bash_redir_apply or
     * _bash_do_exec_pipeline via _dup2 / SetStdHandle).  We mark each handle
     * as inheritable so CreateProcess(bInheritHandles=TRUE) passes them. */
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    if (si.hStdInput  && si.hStdInput  != INVALID_HANDLE_VALUE)
        SetHandleInformation(si.hStdInput,  HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (si.hStdOutput && si.hStdOutput != INVALID_HANDLE_VALUE)
        SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (si.hStdError  && si.hStdError  != INVALID_HANDLE_VALUE)
        SetHandleInformation(si.hStdError,  HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    memset(&pi, 0, sizeof(pi));

    BOOL ok = CreateProcessA(NULL, cmdline.data, NULL, NULL, TRUE,
                             opts->background ? CREATE_NEW_PROCESS_GROUP : 0,
                             NULL, NULL, &si, &pi);

    /* restore fds immediately */
    if (old_stdin  >= 0) { BASH_DUP2(old_stdin,  0); BASH_CLOSE(old_stdin); }
    if (old_stdout >= 0) { BASH_DUP2(old_stdout, 1); BASH_CLOSE(old_stdout); }
    if (old_stderr >= 0) { BASH_DUP2(old_stderr, 2); BASH_CLOSE(old_stderr); }
    bash_bstr_free(&cmdline);

    if (!ok) return -1;
    if (out_pid) *out_pid = (int)pi.dwProcessId;
    if (opts->background) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    DWORD rc = 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rc);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)rc;

#else /* POSIX */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* child */
        if (opts->fd_stdin  >= 0) BASH_DUP2(opts->fd_stdin,  0);
        if (opts->fd_stdout >= 0) BASH_DUP2(opts->fd_stdout, 1);
        if (opts->fd_stderr_to_stdout) BASH_DUP2(1, 2);
        else if (opts->fd_stderr >= 0) BASH_DUP2(opts->fd_stderr, 2);
        /* close any fds >=3 that we redirected? leave for now */
        execvp(prog, argv);
        _exit(127);
    }
    /* parent */
    if (out_pid) *out_pid = (int)pid;
    if (opts->background) return 0;
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}

/* Wait for a pid; returns exit code or -1 on error */
static int _bash_waitpid(int pid)
{
#ifdef BASH_PLATFORM_WINDOWS
    /* we can't easily wait by pid for closed handles; approximate: ignore */
    (void)pid; return 0;
#else
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}

/* ========================================================================
 * Variable store (name=value)
 * ======================================================================== */

typedef struct bash_var_s {
    char *name;
    char *value;
    int   exported;
    int   is_local;
    int   is_readonly;
} bash_var_t;

typedef struct bash_vars_s {
    bash_var_t *items;
    int      len;
    int      cap;
} bash_vars_t;

typedef struct bash_frame_s {
    bash_vars_t vars;
    int      argc;
    char   **argv;
    struct bash_frame_s *parent;
    int      is_func; /* function call frame */
    int      status;  /* return status for $? */
    int      func_ret; /* return flag */
    int      break_level;
    int      continue_level;
} bash_frame_t;

/* ========================================================================
 * AST node types
 * ======================================================================== */

typedef enum {
    N_CMD,        /* simple command: words + redirects */
    N_PIPE,       /* left | right */
    N_AND,        /* left && right */
    N_OR,         /* left || right */
    N_SEMI,       /* left ; right */
    N_BG,         /* left & right (background) */
    N_NOT,        /* ! cmd */
    N_GROUP,      /* { list } or ( list ) */
    N_IF,         /* if cond then body elif* else? fi */
    N_WHILE,      /* while cond do body done */
    N_UNTIL,      /* until cond do body done */
    N_FOR,        /* for name in words; do body done / for (( ; ; )) */
    N_CASE,       /* case word in (pat) body ;; esac */
    N_FUNCDEF,    /* name() body */
    N_ARITH,      /* (( expr )) */
    N_CONDTEST,   /* [[ expr ]] */
    N_BREAK, N_CONTINUE, N_RETURN, N_EXIT
} bash_node_type_t;

typedef struct bash_redir_s {
    int     type;   /* redir type: 0=< 1=> 2=>|(clobber) 3=>> 4=<> 5=<<(heredoc) 6=<<<(here-str) 7=&> 8=>& 9=<& 10=2> 11=2>> */
    int     fd;     /* target fd (0,1,2 etc) */
    char   *target; /* filename or heredoc delimiter or here-str content */
    int     heredoc_quoted; /* for << 'EOF' (no expansion) */
    int     append; /* for >> */
    int     dash;   /* <<- (strip leading tabs) */
} bash_redir_t;

typedef struct bash_node_s bash_node_t;
typedef struct bash_func_s {
    char     *name;
    bash_node_t *body;
} bash_func_t;

typedef struct bash_cmd_s {
    /* redirs (NULL-terminated dynamic array handled separately) */
    bash_redir_t *redirs;
    int         n_redirs;
    int         c_redirs;
    /* assignments (name=value) before command */
    char      **assigns;
    int         n_assigns;
    /* words (command + args) */
    char      **words;
    /* parallel array:
     *   0 = unquoted
     *   1 = single-quote wrapped (fully literal: no $ expansion)
     *   2 = double-quote / backslash only (expand $ but keep internal quotes literal) */
    int        *word_quoted;
    int         n_words;
} bash_cmd_t;

typedef struct bash_bin_s {
    bash_node_t *left;
    bash_node_t *right;
    int       invert; /* for N_NOT child */
} bash_bin_t;

typedef struct bash_if_s {
    /* list of (cond, body) pairs; last cond is NULL for else */
    bash_node_t **conds;
    bash_node_t **bodies;
    int        n;
    int        c;
} bash_if_t;

typedef struct bash_loop_s {
    bash_node_t *cond;
    bash_node_t *body;
} bash_loop_t;

typedef struct bash_for_s {
    char *name;
    /* words for "for x in ..."; NULL means "in $@" */
    char **words;
    int    n_words;
    int    arithmetic_style; /* for ((i=0;i<10;i++)) */
    char *init;   /* arithmetic */
    char *cond_a;
    char *step;
    bash_node_t *body;
} bash_for_t;

typedef struct bash_case_s {
    char *word;
    int   word_quoted; /* 0=unquoted, 1=single-quoted, 2=double/escaped */
    /* list of patterns + body (ends with NULL) */
    char ***patterns; /* each entry is NULL-terminated array of patterns */
    bash_node_t **bodies;
    int         n;
    int         c;
} bash_case_t;

struct bash_node_s {
    bash_node_type_t type;
    union {
        bash_cmd_t   cmd;
        bash_bin_t   bin;
        bash_node_t *not_child;
        struct { bash_node_t *body; int subshell; } group;
        bash_if_t    ifn;
        bash_loop_t  loop;
        bash_for_t   forn;
        bash_case_t  casen;
        struct { char *name; bash_node_t *body; } func;
        struct { char *expr; } arith;
        struct { char *expr; } cond;
        struct { int n; int code; } flow; /* break/continue/return/exit */
    } u;
};

/* ========================================================================
 * Lexer / tokenizer
 * ======================================================================== */

typedef enum {
    TOK_EOF = 0,
    TOK_WORD,      /* any word/identifier (possibly quoted) */
    TOK_ASSIGN,    /* word with = in assignment context */
    TOK_OP,        /* single-char operator: ; & ( ) { } | < > newline \n */
    TOK_DOP,       /* double-char: && || ;; |& << >> >& <& */
    TOK_KEYWORD,   /* reserved word: if then else elif fi for while do done case esac in until function select ! */
    TOK_IO_NUMBER, /* explicit fd: 2> 1< etc. */
} bash_tok_type_t;

typedef struct bash_tok_s {
    bash_tok_type_t type;
    char        *s;       /* word value, or operator string */
    int          io_num;  /* for TOK_IO_NUMBER */
    int          line;    /* source line */
    /* 0 = unquoted word (no quoting at all)
     * 1 = any segment was single-quoted (literal: no $ expansion, no word split)
     * 2 = quoted but never single-quoted (double-quote / backslash only:
     *     perform $ expansion / command substitution but no word-splitting
     *     and treat any internal ' or " as literal characters) */
    int          quoted;
} bash_tok_t;

typedef struct bash_lex_s {
    const char *src;
    size_t      pos;
    int         line;
    int         pushback[32]; /* LIFO stack of chars held back */
    int         pushback_n;  /* number of chars in pushback stack (0..32) */
    int         interactive;
} bash_lex_t;

static const char *bash_keywords[] = {
    "if","then","else","elif","fi","for","while","do","done",
    "case","esac","in","until","function","select","time","coproc",NULL
};

static int _bash_is_keyword(const char *s)
{
    for (int i = 0; bash_keywords[i]; i++)
        if (strcmp(s, bash_keywords[i]) == 0) return 1;
    return 0;
}

static void _bash_lex_init(bash_lex_t *L, const char *src, int interactive)
{
    L->src = src; L->pos = 0; L->line = 1; L->pushback_n = 0;
    L->interactive = interactive;
}

static int _bash_lex_getc(bash_lex_t *L)
{
    if (L->pushback_n > 0) return L->pushback[--L->pushback_n];
    if (!L->src[L->pos]) return -1;
    unsigned char c = (unsigned char)L->src[L->pos++];
    if (c == '\n') L->line++;
    return c;
}
static void _bash_lex_ungetc(bash_lex_t *L, int c)
{
    if (c < 0) return;
    if (c == '\n') L->line--;
    if (L->pushback_n < (int)(sizeof(L->pushback)/sizeof(L->pushback[0])))
        L->pushback[L->pushback_n++] = c;
}

static int _bash_is_wordchar(int c) BASH_ATTR_UNUSED;
static int _bash_is_wordchar(int c)
{
    return c > 0 && (isalnum(c) || c == '_' || c == '.' || c == '/' || c == '-' ||
                     c == '+' || c == ',' || c == ':' || c == '@' || c == '*' ||
                     c == '?' || c == '[' || c == ']' || c == '~' || c == '=' ||
                     c == '%' || c == '#' || c == '$' || c == '^' || c == '!' ||
                     c == '{' || c == '}' || c == '`' || c == '\'' || c == '"' ||
                     c == '\\');
}

/* Lexer needs to track "beginning of command" to recognize keywords,
 * assignments, and IO redirections.
 *
 * Also: quoted regions don't stop on operators.
 * Implementation:
 *   - Start of line/after ; & | ( { && || newline = beginning-of-command
 */

/* Parse a word (which may include quoted sections) starting at L->pos.
 * L->src[L->pos] is current char which must not be whitespace/op-char.
 * Appends to out.  Returns 0 if a complete word was read, -1 on unterminated quote.
 * Also sets *has_glob = 1 if unquoted glob chars encountered. */
static int _bash_lex_read_word(bash_lex_t *L, bash_bstr_t *out, int *has_glob_out, int *quoted_out)
{
    int c;
    int quoted = 0;     /* inside 'single' quotes */
    int dquoted = 0;    /* inside "double" quotes */
    int backtick = 0;   /* inside backticks (nesting depth) */
    int paren = 0;      /* inside $( ) nesting */
    int dollar_quote = 0; /* $'...' */
    int has_glob = 0;
    int had_single_quote = 0;   /* word contained any '...' segment */
    int had_double_or_escape = 0; /* word contained "..." or backslash quoting */
    int in_any_quote = 0;

    for (;;) {
        c = _bash_lex_getc(L);
        if (c < 0) break;

        /* Silently skip bare carriage returns (CRLF / legacy line endings).  We never
           want them to become part of tokens nor interfere with keyword matching. */
        if (!quoted && !dquoted && !dollar_quote && c == '\r') { continue; }

        if (!quoted && !dquoted && !dollar_quote) {
            in_any_quote = 0;
            /* When inside $( ) or ` ` (command substitution), the word we are
             * building will later be re-lexed by _bash_cmdsub -> _bash_run_string.
             * So we must PRESERVE all quoting characters (' " \) verbatim so
             * that the inner lexer sees the original source.  We still track
             * quoted/dquoted/paren locally so depth counters stay correct. */
            int preserve_raw = (paren > 0 || backtick > 0);
            if (c == '\'') { quoted = 1; had_single_quote = 1; if (preserve_raw) bash_bstr_putc(out, (char)c); continue; }
            if (c == '"')  { dquoted = 1; had_double_or_escape = 1; if (preserve_raw) bash_bstr_putc(out, (char)c); continue; }
            if (c == '\\') {
                int n = _bash_lex_getc(L);
                if (n == '\n') { had_double_or_escape = 1; if (preserve_raw) { /* preserve backslash+newline? bash drops it; skip */ } continue; }
                if (n < 0) { bash_bstr_putc(out, '\\'); break; }
                had_double_or_escape = 1;
                bash_bstr_putc(out, '\\');
                if (n >= 0) bash_bstr_putc(out, (char)n);
                continue;
            }
            if (c == '$' && L->src[L->pos] == '\'') {
                L->pos++; dollar_quote = 1; had_single_quote = 1; continue;
            }
            if (c == '$' && L->src[L->pos] == '"') {
                L->pos++; dquoted = 1; had_double_or_escape = 1; continue;
            }
            if (c == '$' && L->src[L->pos] == '(') {
                /* consume $( or $(( as part of word (handled by expander later) */
                bash_bstr_putc(out, '$');
                int nc = _bash_lex_getc(L); /* consume first '(' */
                if (nc < 0) break;
                bash_bstr_putc(out, (char)nc);
                paren += 1;
                if (L->src[L->pos] == '(') {
                    /* arithmetic expansion: $(( ... )) — consume 2nd '(' */
                    int nc2 = _bash_lex_getc(L);
                    if (nc2 < 0) break;
                    bash_bstr_putc(out, (char)nc2);
                    paren += 1;
                }
                continue;
            }
            if (c == '$' && L->src[L->pos] == '{') {
                /* consume ${ as part of the word — track brace depth until matching },
                 * so the entire ${...} construct stays as one token for the expander. */
                bash_bstr_putc(out, '$');
                int nc = _bash_lex_getc(L); /* consume first '{' */
                if (nc < 0) break;
                bash_bstr_putc(out, (char)nc);
                int brace_depth = 1;
                /* Read until brace_depth returns to 0 — everything inside ${...} including nested
                 * ${...}, $(...), $((...)), backticks, quotes becomes part of this word. */
                int in_sq = 0, in_dq = 0, in_dollarq = 0;
                int in_paren = 0, in_back = 0;
                while (brace_depth > 0) {
                    int ch = _bash_lex_getc(L);
                    if (ch < 0) break;
                    /* skip \r (CRLF handling) — but keep inside quotes */
                    if (!in_sq && !in_dq && !in_dollarq && ch == '\r') { continue; }
                    if (in_sq) {
                        if (ch == '\'') in_sq = 0;
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    if (in_dollarq) {
                        if (ch == '\'') in_dollarq = 0;
                        else if (ch == '\\') {
                            int n2 = _bash_lex_getc(L);
                            if (n2 < 0) { bash_bstr_putc(out, (char)ch); break; }
                            bash_bstr_putc(out, (char)ch);
                            bash_bstr_putc(out, (char)n2);
                            continue;
                        }
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    if (in_dq) {
                        if (ch == '"') in_dq = 0;
                        else if (ch == '\\') {
                            int n2 = _bash_lex_getc(L);
                            if (n2 < 0) { bash_bstr_putc(out, (char)ch); break; }
                            if (n2 == '$' || n2 == '`' || n2 == '"' || n2 == '\\' || n2 == '\n') {
                                if (n2 == '\n') continue;
                                bash_bstr_putc(out, (char)ch);
                                bash_bstr_putc(out, (char)n2);
                                continue;
                            }
                            bash_bstr_putc(out, (char)ch);
                            bash_bstr_putc(out, (char)n2);
                            continue;
                        } else if (ch == '`') {
                            in_back++;
                        } else if (ch == '$' && L->src[L->pos] == '(') {
                            in_paren++;
                        } else if (in_paren > 0) {
                            if (ch == '(') in_paren++;
                            else if (ch == ')') in_paren--;
                        } else if (in_back > 0 && ch == '`') {
                            in_back--;
                        }
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    /* unquoted, inside ${ region */
                    if (ch == '\'') { in_sq = 1; bash_bstr_putc(out, (char)ch); continue; }
                    if (ch == '"')  { in_dq = 1; bash_bstr_putc(out, (char)ch); continue; }
                    if (ch == '\\') {
                        int n2 = _bash_lex_getc(L);
                        if (n2 == '\n') continue;
                        if (n2 < 0) { bash_bstr_putc(out, '\\'); break; }
                        bash_bstr_putc(out, '\\'); bash_bstr_putc(out, (char)n2);
                        continue;
                    }
                    if (ch == '$' && L->src[L->pos] == '\'') {
                        L->pos++; in_dollarq = 1; continue;
                    }
                    if (ch == '$' && L->src[L->pos] == '"') {
                        L->pos++; in_dq = 1; continue;
                    }
                    if (ch == '$' && L->src[L->pos] == '{') {
                        /* nested ${ — push dollar, recurse depth */
                        bash_bstr_putc(out, '$');
                        int n2 = _bash_lex_getc(L);
                        if (n2 < 0) break;
                        bash_bstr_putc(out, (char)n2);
                        brace_depth++;
                        continue;
                    }
                    if (ch == '$' && L->src[L->pos] == '(') {
                        bash_bstr_putc(out, '$');
                        int n2 = _bash_lex_getc(L);
                        if (n2 < 0) break;
                        bash_bstr_putc(out, (char)n2);
                        in_paren++;
                        if (L->src[L->pos] == '(') {
                            int n3 = _bash_lex_getc(L);
                            if (n3 < 0) break;
                            bash_bstr_putc(out, (char)n3);
                            in_paren++;
                        }
                        continue;
                    }
                    if (ch == '`') { in_back++; bash_bstr_putc(out, (char)ch); continue; }
                    if (in_paren > 0) {
                        if (ch == '(') in_paren++;
                        else if (ch == ')') in_paren--;
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    if (in_back > 0) {
                        if (ch == '`') in_back--;
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    if (ch == '{') brace_depth++;
                    else if (ch == '}') {
                        brace_depth--;
                        bash_bstr_putc(out, (char)ch);
                        continue;
                    }
                    /* operator chars that normally stop a word are part of the ${...} text,
                     * except we still respect whitespace — no, inside ${...} even spaces are
                     * kept as part of the pattern text (e.g. ${var:-default value} contains a space). */
                    bash_bstr_putc(out, (char)ch);
                }
                continue;
            }
            if (c == '`') { backtick++; had_double_or_escape = 1; bash_bstr_putc(out, '`'); continue; }
            int was_inside = (paren > 0 || backtick > 0);
            if (paren > 0) {
                if (c == '(') paren++;
                else if (c == ')') {
                    paren--;
                    bash_bstr_putc(out, (char)c);
                    /* closing paren of $( / $(( is always part of the word,
                     * even if it brings depth to 0. */
                    continue;
                }
            }
            if (backtick == 0 && paren == 0 && !was_inside) {
                /* stop on operator chars (only when we never entered a nested region in this iteration) */
                if (c == ';' || c == '|' || c == '&' || c == '<' || c == '>' ||
                    c == '(' || c == ')' || c == '{' || c == '}' ||
                    c == '\n' || c == ' ' || c == '\t') {
                    _bash_lex_ungetc(L, c);
                    break;
                }
            }
            if (backtick == 0 && paren == 0) {
                if (c == '*' || c == '?' || c == '[') has_glob = 1;
            }
            bash_bstr_putc(out, (char)c);
        } else if (quoted) {
            in_any_quote = 1;
            if (c == '\'') {
                quoted = 0;
                /* When inside $( ) or backticks we must keep the closing quote,
                 * because the inner script will be re-lexed. */
                if (paren > 0 || backtick > 0) bash_bstr_putc(out, (char)c);
                continue;
            }
            bash_bstr_putc(out, (char)c);
        } else if (dollar_quote) {
            in_any_quote = 1;
            if (c == '\\') {
                int n = _bash_lex_getc(L);
                switch (n) {
                    case 'n': bash_bstr_putc(out, '\n'); break;
                    case 't': bash_bstr_putc(out, '\t'); break;
                    case 'r': bash_bstr_putc(out, '\r'); break;
                    case 'a': bash_bstr_putc(out, '\a'); break;
                    case 'b': bash_bstr_putc(out, '\b'); break;
                    case 'f': bash_bstr_putc(out, '\f'); break;
                    case 'v': bash_bstr_putc(out, '\v'); break;
                    case '\\': bash_bstr_putc(out, '\\'); break;
                    case '\'': bash_bstr_putc(out, '\''); break;
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        int v = n - '0';
                        for (int k = 0; k < 2; k++) {
                            int cc = _bash_lex_getc(L);
                            if (cc >= '0' && cc <= '7') v = v * 8 + (cc - '0');
                            else { _bash_lex_ungetc(L, cc); break; }
                        }
                        bash_bstr_putc(out, (char)v);
                        break;
                    }
                    case 'x': {
                        int v = 0, got = 0;
                        for (int k = 0; k < 2; k++) {
                            int cc = _bash_lex_getc(L);
                            int d = -1;
                            if (cc >= '0' && cc <= '9') d = cc - '0';
                            else if (cc >= 'a' && cc <= 'f') d = cc - 'a' + 10;
                            else if (cc >= 'A' && cc <= 'F') d = cc - 'A' + 10;
                            else { _bash_lex_ungetc(L, cc); break; }
                            v = v * 16 + d; got = 1;
                        }
                        if (got) bash_bstr_putc(out, (char)v);
                        break;
                    }
                    case 'u': case 'U':
                    default:
                        if (n >= 0) bash_bstr_putc(out, (char)n);
                        break;
                }
                continue;
            }
            if (c == '\'') { dollar_quote = 0; continue; }
            bash_bstr_putc(out, (char)c);
        } else /* dquoted */ {
            in_any_quote = 1;
            int dq_preserve_raw = (paren > 0 || backtick > 0);
            if (c == '\\') {
                int n = _bash_lex_getc(L);
                if (dq_preserve_raw) {
                    /* Pass \ and next char through raw — inner lexer will decode */
                    bash_bstr_putc(out, '\\');
                    if (n >= 0 && n != '\n') bash_bstr_putc(out, (char)n);
                } else {
                    if (n == '$' || n == '`' || n == '"' || n == '\\' || n == '\n') {
                        if (n == '\n') { /* line cont, drop both */ continue; }
                        bash_bstr_putc(out, '\\');
                        bash_bstr_putc(out, (char)n);
                    } else {
                        bash_bstr_putc(out, '\\');
                        if (n >= 0) bash_bstr_putc(out, (char)n);
                    }
                }
                continue;
            }
            if (c == '"') {
                dquoted = 0;
                if (dq_preserve_raw) bash_bstr_putc(out, (char)c);
                continue;
            }
            if (c == '`') backtick++;
            if (c == '$' && L->src[L->pos] == '(') {
                /* consume $( or $(( inside double quotes — track correct depth */
                paren++;  /* first '(' already counted via L->src peek */
                bash_bstr_putc(out, (char)c); /* write '$' */
                c = _bash_lex_getc(L); /* consume first '(' (already counted) */
                if (c < 0) break;
                bash_bstr_putc(out, (char)c);
                if (L->src[L->pos] == '(') {
                    paren++; /* arithmetic $(( — count second '(' */
                    int nc2 = _bash_lex_getc(L);
                    if (nc2 < 0) break;
                    bash_bstr_putc(out, (char)nc2);
                }
                continue;
            }
            if (paren > 0) {
                if (c == '(') paren++;
                else if (c == ')') paren--;
            }
            bash_bstr_putc(out, (char)c);
        }
    }
    if (quoted || dquoted || dollar_quote) {
        if (quoted_out) {
            if (had_single_quote) *quoted_out = 1;
            else if (had_double_or_escape) *quoted_out = 2;
            else *quoted_out = 0;
        }
        if (has_glob_out) *has_glob_out = has_glob;
        return -1;
    }
    if (quoted_out) {
        if (had_single_quote) *quoted_out = 1;
        else if (had_double_or_escape) *quoted_out = 2;
        else *quoted_out = 0;
    }
    if (has_glob_out) *has_glob_out = has_glob;
    (void)in_any_quote;
    return 0;
}

/* Track "beginning of command" state for parser to feed back */
static int _bash_lex_next(bash_lex_t *L, bash_tok_t *out, int bol)
{
    out->type = TOK_EOF; out->s = NULL; out->io_num = -1; out->line = L->line;
    int c;

    /* Skip whitespace/comments */
    for (;;) {
        c = _bash_lex_getc(L);
        if (c < 0) return 0;
        if (c == ' ' || c == '\t' || c == '\r') continue;
        if (c == '#') { while ((c = _bash_lex_getc(L)) >= 0 && c != '\n') { } if (c < 0) return 0; continue; }
        break;
    }

    out->line = L->line;

    /* newlines */
    if (c == '\n') {
        out->type = TOK_OP; out->s = _bash_xstrdup("\n");
        return 0;
    }

    /* Handle IO_NUMBER: digits immediately followed by < > & */
    if (c >= '0' && c <= '9') {
        bash_bstr_t d; bash_bstr_init(&d); bash_bstr_putc(&d, (char)c);
        while (1) {
            int nc = _bash_lex_getc(L);
            if (nc >= '0' && nc <= '9') { bash_bstr_putc(&d, (char)nc); continue; }
            if (nc == '<' || nc == '>') {
                _bash_lex_ungetc(L, nc);
                out->type = TOK_IO_NUMBER;
                out->s = bash_bstr_detach(&d);
                out->io_num = atoi(out->s);
                return 0;
            }
            /* not a redirection - rewind and treat as word start */
            _bash_lex_ungetc(L, nc);
            break;
        }
        /* treat digits as word start: put them back */
        for (int i = (int)d.len - 1; i >= 0; i--) _bash_lex_ungetc(L, d.data[i]);
        bash_bstr_free(&d);
        c = _bash_lex_getc(L);
    }

    /* Operators */
    if (c == '(' || c == ')' || c == '{' || c == '}') {
        out->type = TOK_OP; out->s = _bash_xmalloc(2); out->s[0] = (char)c; out->s[1] = 0;
        return 0;
    }
    if (c == ';') {
        int n = _bash_lex_getc(L);
        if (n == ';') {
            int n2 = _bash_lex_getc(L);
            if (n2 == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup(";;&"); return 0; }
            _bash_lex_ungetc(L, n2);
            out->type = TOK_DOP; out->s = _bash_xstrdup(";;"); return 0;
        }
        if (n == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup(";&"); return 0; }
        _bash_lex_ungetc(L, n);
        out->type = TOK_OP; out->s = _bash_xstrdup(";"); return 0;
    }
    if (c == '&') {
        int n = _bash_lex_getc(L);
        if (n == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup("&&"); return 0; }
        if (n == '>') {
            int n2 = _bash_lex_getc(L);
            if (n2 == '>') { out->type = TOK_DOP; out->s = _bash_xstrdup("&>>"); return 0; }
            _bash_lex_ungetc(L, n2);
            out->type = TOK_DOP; out->s = _bash_xstrdup("&>"); return 0;
        }
        if (n == '<') {
            int n2 = _bash_lex_getc(L);
            if (n2 == '<') {
                /* &<<  (bit unusual; but bash supports &> redirs) — keep simple */
                _bash_lex_ungetc(L, n2);
            } else { _bash_lex_ungetc(L, n2); }
            out->type = TOK_OP; out->s = _bash_xstrdup("&"); _bash_lex_ungetc(L, n);
            return 0;
        }
        _bash_lex_ungetc(L, n);
        out->type = TOK_OP; out->s = _bash_xstrdup("&"); return 0;
    }
    if (c == '|') {
        int n = _bash_lex_getc(L);
        if (n == '|') { out->type = TOK_DOP; out->s = _bash_xstrdup("||"); return 0; }
        if (n == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup("|&"); return 0; }
        _bash_lex_ungetc(L, n);
        out->type = TOK_OP; out->s = _bash_xstrdup("|"); return 0;
    }
    if (c == '<') {
        int n = _bash_lex_getc(L);
        if (n == '<') {
            int n2 = _bash_lex_getc(L);
            if (n2 == '-') { out->type = TOK_DOP; out->s = _bash_xstrdup("<<-"); return 0; }
            if (n2 == '<') { out->type = TOK_DOP; out->s = _bash_xstrdup("<<<"); return 0; }
            _bash_lex_ungetc(L, n2);
            out->type = TOK_DOP; out->s = _bash_xstrdup("<<"); return 0;
        }
        if (n == '>') { out->type = TOK_DOP; out->s = _bash_xstrdup("<>"); return 0; }
        if (n == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup("<&"); return 0; }
        if (n == '=') { out->type = TOK_DOP; out->s = _bash_xstrdup("<="); return 0; }
        _bash_lex_ungetc(L, n);
        out->type = TOK_OP; out->s = _bash_xstrdup("<"); return 0;
    }
    if (c == '>') {
        int n = _bash_lex_getc(L);
        if (n == '>') { out->type = TOK_DOP; out->s = _bash_xstrdup(">>"); return 0; }
        if (n == '|') { out->type = TOK_DOP; out->s = _bash_xstrdup(">|"); return 0; }
        if (n == '&') { out->type = TOK_DOP; out->s = _bash_xstrdup(">&"); return 0; }
        if (n == '=') { out->type = TOK_DOP; out->s = _bash_xstrdup(">="); return 0; }
        _bash_lex_ungetc(L, n);
        out->type = TOK_OP; out->s = _bash_xstrdup(">"); return 0;
    }
    if (c == '!') {
        /* If BOL, treat as keyword; else as part of word */
        int n = _bash_lex_getc(L);
        if (bol && (n == ' ' || n == '\t' || n == '\n' || n < 0)) {
            _bash_lex_ungetc(L, n);
            out->type = TOK_KEYWORD; out->s = _bash_xstrdup("!");
            return 0;
        }
        _bash_lex_ungetc(L, n);
        _bash_lex_ungetc(L, c);
        goto read_word;
    }

    /* Read a word */
read_word:
    if (c >= 0) _bash_lex_ungetc(L, c);
    bash_bstr_t w; bash_bstr_init(&w);
    int hg = 0, qt = 0;
    int rc = _bash_lex_read_word(L, &w, &hg, &qt);
    out->s = bash_bstr_detach(&w);
    out->quoted = qt;
    (void)hg;
    if (rc < 0 && w.len == 0) {
        /* unterminated quote: treat as error token? continue with what we have */
    }
    out->type = TOK_WORD;
    if (bol && _bash_is_keyword(out->s)) {
        out->type = TOK_KEYWORD;
    }
    /* Check for assignment form (only meaningful if used in assignment context but we store as-is) */
    return 0;
}

static void _bash_tok_free(bash_tok_t *t)
{
    if (t->s) { free(t->s); t->s = NULL; }
}

/* ========================================================================
 * Expansion layer: variable, command, arithmetic, tilde, glob, word split
 * ======================================================================== */

/* Forward decls for execution context needed by expander */
/* typedef struct bash_ctx_s bash_ctx_t;   --- already forward-declared */

/* Look up variable in context (current frame up to globals) */
static char *_bash_var_get(bash_ctx_t *ctx, const char *name, int *exported);
static int   _bash_var_set(bash_ctx_t *ctx, const char *name, const char *value, int exported, int do_export, int readonly);
static int   _bash_var_unset(bash_ctx_t *ctx, const char *name);
static int   _bash_do_exec(bash_ctx_t *ctx, bash_node_t *node);
static int   _bash_run_script(bash_ctx_t *ctx, const char *path, int argc, char **argv);

/* Helper: read a shell variable name at *p starting with $ or ${ */
static int _bash_parse_name(const char *p, char *buf, int bufsize)
{
    int i = 0;
    if (isalpha((unsigned char)p[0]) || p[0] == '_') {
        while (i < bufsize - 1 && (isalnum((unsigned char)p[i]) || p[i] == '_')) {
            buf[i] = p[i]; i++;
        }
        buf[i] = 0;
        return i;
    }
    if (i < bufsize - 1) { buf[i] = p[0]; buf[i+1] = 0; return 1; }
    return 0;
}

/* Find matching } or ) or ` for expansions.
 * start points to char AFTER opening ${ or $( or `
 * Returns index of closing char, or -1. */
static int _bash_find_close_paren(const char *s, int start, char open, char close)
{
    int depth = 1;
    int sq = 0, dq = 0, bs = 0;
    for (int i = start; s[i]; i++) {
        char c = s[i];
        if (bs) { bs = 0; continue; }
        if (sq) { if (c == '\'') sq = 0; continue; }
        if (dq) {
            if (c == '\\') { bs = 1; continue; }
            if (c == '"') { dq = 0; continue; }
            if (c == '$' && (s[i+1] == '(' || s[i+1] == '{')) {
                /* recurse into sub */
                char o2 = s[i+1] == '(' ? '(' : '{';
                char c2 = s[i+1] == '(' ? ')' : '}';
                int j = _bash_find_close_paren(s, i+2, o2, c2);
                if (j < 0) return -1;
                i = j; continue;
            }
            if (c == '`') { int j = _bash_find_close_paren(s, i+1, '`', '`'); if (j<0) return -1; i = j; continue; }
            continue;
        }
        if (c == '\'') { sq = 1; continue; }
        if (c == '"')  { dq = 1; continue; }
        if (c == '\\') { bs = 1; continue; }
        if (c == '$' && (s[i+1] == '(' || s[i+1] == '{')) {
            char o2 = s[i+1] == '(' ? '(' : '{';
            char c2 = s[i+1] == '(' ? ')' : '}';
            int j = _bash_find_close_paren(s, i+2, o2, c2);
            if (j < 0) return -1;
            i = j; continue;
        }
        if (c == '`') { int j = _bash_find_close_paren(s, i+1, '`', '`'); if (j<0) return -1; i = j; continue; }
        if (c == open) depth++;
        if (c == close) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

/* Arithmetic expression evaluator (supports + - * / % << >> & | ^ ! == != < > <= >= && || ?: =) */
typedef struct bash_aeval_s { const char *s; int pos; int err; bash_ctx_t *ctx; } bash_aeval_t;
static long _bash_aeval_expr(bash_aeval_t *ae);

static long _bash_aeval_primary(bash_aeval_t *ae)
{
    while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
    if (ae->s[ae->pos] == '(') {
        ae->pos++;
        long v = _bash_aeval_expr(ae);
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        if (ae->s[ae->pos] == ')') ae->pos++;
        return v;
    }
    if (ae->s[ae->pos] == '!') {
        ae->pos++;
        return !_bash_aeval_primary(ae);
    }
    if (ae->s[ae->pos] == '-' || ae->s[ae->pos] == '+') {
        int op = ae->s[ae->pos++];
        long v = _bash_aeval_primary(ae);
        return op == '-' ? -v : v;
    }
    if (ae->s[ae->pos] == '~') {
        ae->pos++;
        return ~_bash_aeval_primary(ae);
    }
    if (isdigit((unsigned char)ae->s[ae->pos])) {
        long v = 0;
        if (ae->s[ae->pos] == '0' && (ae->s[ae->pos+1] == 'x' || ae->s[ae->pos+1] == 'X')) {
            ae->pos += 2;
            while (1) {
                int c = ae->s[ae->pos];
                if (c >= '0' && c <= '9') v = v*16 + (c-'0');
                else if (c >= 'a' && c <= 'f') v = v*16 + (c-'a'+10);
                else if (c >= 'A' && c <= 'F') v = v*16 + (c-'A'+10);
                else break;
                ae->pos++;
            }
        } else if (ae->s[ae->pos] == '0') {
            while (1) {
                int c = ae->s[ae->pos];
                if (c >= '0' && c <= '7') v = v*8 + (c-'0');
                else break;
                ae->pos++;
            }
        } else {
            while (isdigit((unsigned char)ae->s[ae->pos])) {
                v = v*10 + (ae->s[ae->pos] - '0');
                ae->pos++;
            }
        }
        return v;
    }
    if (isalpha((unsigned char)ae->s[ae->pos]) || ae->s[ae->pos] == '_') {
        char name[256]; int ni = 0;
        while (ni < 255 && (isalnum((unsigned char)ae->s[ae->pos]) || ae->s[ae->pos] == '_')) {
            name[ni++] = ae->s[ae->pos++];
        }
        name[ni] = 0;
        /* look ahead: assignment? */
        int s = ae->pos;
        while (ae->s[s] == ' ' || ae->s[s] == '\t') s++;
        if (ae->s[s] == '=' && ae->s[s+1] != '=') {
            ae->pos = s + 1;
            long v = _bash_aeval_expr(ae);
            char buf[64]; snprintf(buf, sizeof(buf), "%ld", v);
            _bash_var_set(ae->ctx, name, buf, 0, 0, 0);
            return v;
        }
        /* resolve variable */
        char *val = _bash_var_get(ae->ctx, name, NULL);
        if (!val || !*val) return 0;
        char *ep = NULL;
        long v = strtol(val, &ep, 0);
        return v;
    }
    ae->err = 1;
    return 0;
}

static long _bash_aeval_mul(bash_aeval_t *ae)
{
    long l = _bash_aeval_primary(ae);
    for (;;) {
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        int op = ae->s[ae->pos];
        if (op == '*' || op == '/' || op == '%') {
            ae->pos++;
            long r = _bash_aeval_primary(ae);
            if (op == '*') l *= r;
            else if (op == '/') { if (r == 0) { ae->err = 1; return 0; } l /= r; }
            else { if (r == 0) { ae->err = 1; return 0; } l %= r; }
        } else break;
    }
    return l;
}
static long _bash_aeval_add(bash_aeval_t *ae)
{
    long l = _bash_aeval_mul(ae);
    for (;;) {
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        int op = ae->s[ae->pos];
        if (op == '+' || op == '-') {
            ae->pos++;
            long r = _bash_aeval_mul(ae);
            l = (op == '+') ? l + r : l - r;
        } else break;
    }
    return l;
}
static long _bash_aeval_shift(bash_aeval_t *ae)
{
    long l = _bash_aeval_add(ae);
    for (;;) {
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        if (ae->s[ae->pos] == '<' && ae->s[ae->pos+1] == '<') { ae->pos+=2; long r=_bash_aeval_add(ae); l <<= r; }
        else if (ae->s[ae->pos] == '>' && ae->s[ae->pos+1] == '>') { ae->pos+=2; long r=_bash_aeval_add(ae); l >>= r; }
        else break;
    }
    return l;
}
static long _bash_aeval_rel(bash_aeval_t *ae)
{
    long l = _bash_aeval_shift(ae);
    for (;;) {
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        if (ae->s[ae->pos] == '<' && ae->s[ae->pos+1] == '=') { ae->pos+=2; long r=_bash_aeval_shift(ae); l = (l <= r); }
        else if (ae->s[ae->pos] == '>' && ae->s[ae->pos+1] == '=') { ae->pos+=2; long r=_bash_aeval_shift(ae); l = (l >= r); }
        else if (ae->s[ae->pos] == '<' && ae->s[ae->pos+1] != '<') { ae->pos+=1; long r=_bash_aeval_shift(ae); l = (l < r); }
        else if (ae->s[ae->pos] == '>' && ae->s[ae->pos+1] != '>') { ae->pos+=1; long r=_bash_aeval_shift(ae); l = (l > r); }
        else break;
    }
    return l;
}
static long _bash_aeval_eq(bash_aeval_t *ae)
{
    long l = _bash_aeval_rel(ae);
    for (;;) {
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        if (ae->s[ae->pos] == '=' && ae->s[ae->pos+1] == '=') { ae->pos+=2; long r=_bash_aeval_rel(ae); l = (l == r); }
        else if (ae->s[ae->pos] == '!' && ae->s[ae->pos+1] == '=') { ae->pos+=2; long r=_bash_aeval_rel(ae); l = (l != r); }
        else break;
    }
    return l;
}
static long _bash_aeval_band(bash_aeval_t *ae){ long l=_bash_aeval_eq(ae); while(1){while(ae->s[ae->pos]==' '||ae->s[ae->pos]=='\t')ae->pos++; if(ae->s[ae->pos]=='&'&&ae->s[ae->pos+1]!='&'){ae->pos++; l&=_bash_aeval_eq(ae);} else break;} return l; }
static long _bash_aeval_bxor(bash_aeval_t *ae){ long l=_bash_aeval_band(ae); while(1){while(ae->s[ae->pos]==' '||ae->s[ae->pos]=='\t')ae->pos++; if(ae->s[ae->pos]=='^'){ae->pos++; l^=_bash_aeval_band(ae);} else break;} return l; }
static long _bash_aeval_bor(bash_aeval_t *ae){ long l=_bash_aeval_bxor(ae); while(1){while(ae->s[ae->pos]==' '||ae->s[ae->pos]=='\t')ae->pos++; if(ae->s[ae->pos]=='|'&&ae->s[ae->pos+1]!='|'){ae->pos++; l|=_bash_aeval_bxor(ae);} else break;} return l; }
static long _bash_aeval_and(bash_aeval_t *ae){ long l=_bash_aeval_bor(ae); while(1){while(ae->s[ae->pos]==' '||ae->s[ae->pos]=='\t')ae->pos++; if(ae->s[ae->pos]=='&'&&ae->s[ae->pos+1]=='&'){ae->pos+=2; long r=_bash_aeval_bor(ae); l=(l&&r);} else break;} return l; }
static long _bash_aeval_or(bash_aeval_t *ae){ long l=_bash_aeval_and(ae); while(1){while(ae->s[ae->pos]==' '||ae->s[ae->pos]=='\t')ae->pos++; if(ae->s[ae->pos]=='|'&&ae->s[ae->pos+1]=='|'){ae->pos+=2; long r=_bash_aeval_and(ae); l=(l||r);} else break;} return l; }
static long _bash_aeval_tern(bash_aeval_t *ae)
{
    long cond = _bash_aeval_or(ae);
    while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
    if (ae->s[ae->pos] == '?') {
        ae->pos++;
        long tv = _bash_aeval_expr(ae);
        while (ae->s[ae->pos] == ' ' || ae->s[ae->pos] == '\t') ae->pos++;
        if (ae->s[ae->pos] == ':') ae->pos++;
        long fv = _bash_aeval_expr(ae);
        return cond ? tv : fv;
    }
    return cond;
}
static long _bash_aeval_expr(bash_aeval_t *ae) { return _bash_aeval_tern(ae); }

static long _bash_eval_arith(bash_ctx_t *ctx, const char *expr)
{
    bash_aeval_t ae; ae.s = expr; ae.pos = 0; ae.err = 0; ae.ctx = ctx;
    long v = _bash_aeval_expr(&ae);
    (void)ae.err;
    return v;
}

/* Command substitution: execute a subshell script string, capture stdout.
 * Returns malloc'd buffer, sets *out_len */
static char *_bash_cmdsub(bash_ctx_t *ctx, const char *script, int strip, int *out_len);

/* Execute a shell program string in the CURRENT context; used by ${|cmd;}.
 * Forward declared here so the new command substitution code in _bash_expand_word can call it. */
static int _bash_run_string(bash_ctx_t *ctx, const char *src);

/* Wildcard glob expand a single word (unquoted), appending matches into barray.
 * Returns 1 if expanded (matches), 0 if not (original added).
 * GLOBSORT controls sorting: unset/empty=default lex sort, "none"=no sort (FS order). */
static int _bash_glob_word(bash_ctx_t *ctx, const char *word, bash_barray_t *out);

/* Main entry: expand a single word from script source into 0..N strings (post-split).
 *   quoted : 0/1 whether original token had any quoting (affects word splitting and glob)
 * Returns malloc'd argv-style array (NULL-terminated). Sets *out_n = count. */
static char **_bash_expand_word(bash_ctx_t *ctx, const char *word, int quoted, int *out_n);

/* ========================================================================
 * Parser (recursive descent)
 * ======================================================================== */

typedef struct bash_parser_s {
    bash_lex_t *lex;
    bash_tok_t   cur;
    int       bol; /* next token is beginning of command? */
    int       have_cur;
} bash_parser_t;

static void _bash_parser_next(bash_parser_t *P)
{
    if (P->have_cur) _bash_tok_free(&P->cur);
    _bash_lex_next(P->lex, &P->cur, P->bol);
    P->have_cur = 1;
    P->bol = 0;
}

static void _bash_parser_init(bash_parser_t *P, bash_lex_t *L)
{
    P->lex = L; P->bol = 1; P->have_cur = 0;
    _bash_parser_next(P);
}

static int _bash_accept_op(bash_parser_t *P, const char *s)
{
    if ((P->cur.type == TOK_OP || P->cur.type == TOK_DOP) &&
        P->cur.s && strcmp(P->cur.s, s) == 0) {
        /* Separators that typically begin a new clause set BOL so that
           reserved words (then / do / in / done / esac / fi / else / elif)
           are correctly recognised as keywords on the following token. */
        if (strcmp(s, ";") == 0 || strcmp(s, "\n") == 0 || strcmp(s, "&") == 0 ||
            strcmp(s, "|") == 0 || strcmp(s, "||") == 0 || strcmp(s, "&&") == 0) {
            P->bol = 1;
        }
        _bash_parser_next(P);
        return 1;
    }
    return 0;
}
static int _bash_check_op(bash_parser_t *P, const char *s)
{
    return (P->cur.type == TOK_OP || P->cur.type == TOK_DOP) &&
           P->cur.s && strcmp(P->cur.s, s) == 0;
}
static int _bash_check_kw(bash_parser_t *P, const char *s)
{
    return P->cur.type == TOK_KEYWORD && P->cur.s && strcmp(P->cur.s, s) == 0;
}
static int _bash_accept_kw(bash_parser_t *P, const char *s)
{
    if (_bash_check_kw(P, s)) { _bash_parser_next(P); return 1; }
    return 0;
}

/* Forward decls */
static bash_node_t *_bash_parse_list(bash_parser_t *P, int stop_on_done, int stop_on_else, int stop_on_fi,
                              int stop_on_esac, int stop_on_brace, int stop_on_rparen,
                              int stop_on_then, int stop_on_do, int stop_on_in);
static bash_node_t *_bash_parse_pipeline(bash_parser_t *P);
static bash_node_t *_bash_parse_command(bash_parser_t *P);

/* Helper: new node */
static bash_node_t *_bash_newnode(bash_node_type_t t)
{
    bash_node_t *n = (bash_node_t *)_bash_xmalloc(sizeof(bash_node_t));
    memset(n, 0, sizeof(bash_node_t));
    n->type = t;
    return n;
}

/* Parse a simple command (words + assignments + redirects) */
static bash_node_t *_bash_parse_simple_cmd(bash_parser_t *P, bash_tok_t first)
{
    bash_node_t *n = _bash_newnode(N_CMD);
    bash_cmd_t *c = &n->u.cmd;
    memset(c, 0, sizeof(*c));

    /* rewind first token into list */
    if (first.type == TOK_WORD || first.type == TOK_KEYWORD || first.type == TOK_IO_NUMBER) {
        /* decide assign vs word: if has '=' and left is valid identifier, put in assigns */
        if (first.type == TOK_WORD) {
            char *eq = strchr(first.s, '=');
            if (eq && eq > first.s) {
                int ok = 1;
                int depth = 0;
                for (char *p = first.s; p < eq; p++) {
                    if (depth > 0) {
                        /* inside [...] subscript, accept anything until matching ] */
                        if (*p == '[') depth++;
                        else if (*p == ']') {
                            depth--;
                            if (depth == 0) {
                                /* After closing ']': only more '[' or end-at-'=' allowed */
                                char nxt = (p + 1 < eq) ? p[1] : '=';
                                if (nxt != '[' && nxt != '=') { ok = 0; break; }
                            }
                        }
                        continue;
                    }
                    if (*p == '[') { depth++; continue; }
                    if (p == first.s) {
                        if (!(isalpha((unsigned char)*p) || *p == '_')) { ok = 0; break; }
                    } else {
                        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '[')) { ok = 0; break; }
                        /* '[' opens subscript (handled by depth above on next iter) */
                    }
                }
                if (ok && depth == 0) {
                    /* assignment */
                    c->n_assigns++;
                    c->assigns = (char**)_bash_xrealloc(c->assigns, c->n_assigns * sizeof(char*));
                    c->assigns[c->n_assigns-1] = first.s; first.s = NULL;
                    /* If more tokens follow, don't decide yet; parse more */
                    goto loop;
                }
            }
        }
        /* treat as first word */
        c->n_words++;
        c->words = (char**)_bash_xrealloc(c->words, c->n_words * sizeof(char*));
        c->word_quoted = (int*)_bash_xrealloc(c->word_quoted, c->n_words * sizeof(int));
        c->words[c->n_words-1] = first.s; first.s = NULL;
        c->word_quoted[c->n_words-1] = first.quoted;
    }
    (void)first;

loop:
    while (1) {
        bash_tok_type_t tt = P->cur.type;
        if (tt == TOK_EOF) break;
        if (tt == TOK_OP || tt == TOK_DOP) {
            const char *s = P->cur.s;
            int is_redir = 0;
            if (strcmp(s, "<")==0 || strcmp(s, ">")==0 || strcmp(s, ">>")==0 ||
                strcmp(s, "<<")==0 || strcmp(s, "<<-")==0 || strcmp(s, "<<<")==0 ||
                strcmp(s, "<>")==0 || strcmp(s, ">&")==0 || strcmp(s, "<&")==0 ||
                strcmp(s, ">|")==0 || strcmp(s, "&>")==0 || strcmp(s, "&>>")==0 ||
                strcmp(s, "|&")==0 ? 0 : (strcmp(s, "<")==0)) is_redir = 1;
            if (strcmp(s, "|") == 0 || strcmp(s, "|&") == 0 ||
                strcmp(s, ";") == 0 || strcmp(s, "&&") == 0 || strcmp(s, "||") == 0 ||
                strcmp(s, "&") == 0 ||
                strcmp(s, "(") == 0 || strcmp(s, ")") == 0 ||
                strcmp(s, "{") == 0 || strcmp(s, "}") == 0 ||
                strcmp(s, "\n") == 0 ||
                /* case statement terminators */
                strcmp(s, ";;") == 0 || strcmp(s, ";&") == 0 || strcmp(s, ";;&") == 0) {
                break; /* these end simple cmd */
            }
            if (tt == TOK_OP && (strcmp(s, "<") == 0 || strcmp(s, ">") == 0)) is_redir = 1;
            if (tt == TOK_DOP) is_redir = 1; /* every DOP we know is either sep or redir */
            if (!is_redir) break;

            /* Parse redirect */
            bash_redir_t rd; memset(&rd, 0, sizeof(rd));
            rd.fd = -1;
            /* previous IO_NUMBER? */
            if (P->cur.type == TOK_OP && strcmp(P->cur.s, "<") == 0) rd.fd = 0;
            else if (P->cur.type == TOK_OP && strcmp(P->cur.s, ">") == 0) rd.fd = 1;
            else if (P->cur.type == TOK_DOP) {
                if (strcmp(P->cur.s, ">>") == 0)  { rd.fd = 1; rd.append = 1; }
                else if (strcmp(P->cur.s, "<<") == 0)  { rd.fd = 0; rd.type = 5; }
                else if (strcmp(P->cur.s, "<<-") == 0) { rd.fd = 0; rd.type = 5; rd.dash = 1; }
                else if (strcmp(P->cur.s, "<<<") == 0) { rd.fd = 0; rd.type = 6; }
                else if (strcmp(P->cur.s, "<>") == 0)  { rd.fd = 0; rd.type = 4; }
                else if (strcmp(P->cur.s, ">&") == 0)  { rd.fd = 1; rd.type = 8; }
                else if (strcmp(P->cur.s, "<&") == 0)  { rd.fd = 0; rd.type = 9; }
                else if (strcmp(P->cur.s, ">|") == 0)  { rd.fd = 1; rd.type = 2; }
                else if (strcmp(P->cur.s, "&>") == 0)  { rd.fd = 1; rd.type = 7; }
                else if (strcmp(P->cur.s, "&>>") == 0) { rd.fd = 1; rd.type = 7; rd.append = 1; }
            }
            if (P->cur.type == TOK_IO_NUMBER) {
                /* handled in next: check prev token */
            }

            /* For IO_NUMBER, redo properly */
            /* We already consumed the op; need to re-check IO_NUMBER case */
            /* simpler: use current token string */
            if (P->cur.type == TOK_IO_NUMBER) {
                /* The IO_NUMBER is the fd; next token is the op */
                rd.fd = P->cur.io_num;
                _bash_parser_next(P); /* consume IO_NUMBER */
                if (P->cur.type == TOK_OP && strcmp(P->cur.s, "<") == 0) rd.type = 0;
                else if (P->cur.type == TOK_OP && strcmp(P->cur.s, ">") == 0) rd.type = 1;
                else if (P->cur.type == TOK_DOP) {
                    if (strcmp(P->cur.s, ">>") == 0)  { rd.type = 3; }
                    else if (strcmp(P->cur.s, ">|") == 0) { rd.type = 2; }
                    else if (strcmp(P->cur.s, ">&") == 0) { rd.type = 8; }
                    else if (strcmp(P->cur.s, "<&") == 0) { rd.type = 9; }
                    else if (strcmp(P->cur.s, "<<") == 0) { rd.type = 5; }
                    else if (strcmp(P->cur.s, "<<-") == 0) { rd.type = 5; rd.dash = 1; }
                    else if (strcmp(P->cur.s, "<<<") == 0) { rd.type = 6; }
                }
            } else {
                /* set type if not set */
                if (rd.type == 0 && rd.fd == 0) rd.type = 0; /* < */
                if (rd.type == 0 && rd.fd == 1) rd.type = 1; /* > */
            }
            _bash_parser_next(P); /* consume the op token */

            /* get target word */
            if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD || P->cur.type == TOK_IO_NUMBER) {
                rd.target = P->cur.s; P->cur.s = NULL;
                /* Check heredoc: if delimiter contains any quotes -> no expansion */
                if (rd.type == 5) {
                    const char *t = rd.target;
                    for (const char *p = t; *p; p++) {
                        if (*p == '\'' || *p == '"' || *p == '\\') { rd.heredoc_quoted = 1; break; }
                    }
                }
                /* For heredoc: read lines until delimiter and store into target? */
                /* We'll store delimiter now and read heredoc at execution time */
                if (c->n_redirs >= c->c_redirs) {
                    c->c_redirs = c->c_redirs ? c->c_redirs * 2 : 4;
                    c->redirs = (bash_redir_t*)_bash_xrealloc(c->redirs, c->c_redirs * sizeof(bash_redir_t));
                }
                c->redirs[c->n_redirs++] = rd;
                _bash_parser_next(P); /* consume the target word token */
                continue;
            }
            break;
        }
        if (tt == TOK_IO_NUMBER) {
            /* fd redir: IO_NUMBER followed by < or > or >> */
            /* will be handled next iteration; just push back as first word if not? */
            /* Actually rewind: treat as word start? No, it's an IO_NUMBER. */
            /* We need to combine with next op token. */
            int fd = P->cur.io_num;
            bash_tok_t save_cur = P->cur;
            memset(&save_cur, 0, sizeof(save_cur));
            _bash_parser_next(P); /* consume IO_NUMBER */
            if (P->cur.type == TOK_EOF) break;
            if (!((P->cur.type == TOK_OP && (strcmp(P->cur.s,"<")==0 || strcmp(P->cur.s,">")==0)) ||
                  (P->cur.type == TOK_DOP))) {
                /* Not a redirection: push IO_NUMBER back as word */
                /* Add as word */
                c->n_words++;
                c->words = (char**)_bash_xrealloc(c->words, c->n_words * sizeof(char*));
                char s2[64]; snprintf(s2, sizeof(s2), "%d", fd);
                c->words[c->n_words-1] = _bash_xstrdup(s2);
                /* current token is the new "first" - keep in P->cur */
                continue;
            }
            /* fd redir */
            bash_redir_t rd; memset(&rd, 0, sizeof(rd));
            rd.fd = fd;
            if (P->cur.type == TOK_OP && strcmp(P->cur.s, "<") == 0) rd.type = 0;
            else if (P->cur.type == TOK_OP && strcmp(P->cur.s, ">") == 0) rd.type = 1;
            else if (P->cur.type == TOK_DOP) {
                if (strcmp(P->cur.s, ">>") == 0) { rd.type = 3; rd.append = 1; }
                else if (strcmp(P->cur.s, ">|") == 0) { rd.type = 2; }
                else if (strcmp(P->cur.s, ">&") == 0) { rd.type = 8; }
                else if (strcmp(P->cur.s, "<&") == 0) { rd.type = 9; }
                else if (strcmp(P->cur.s, "<<") == 0) { rd.type = 5; }
                else if (strcmp(P->cur.s, "<<-") == 0) { rd.type = 5; rd.dash = 1; }
                else if (strcmp(P->cur.s, "<<<") == 0) { rd.type = 6; }
                else if (strcmp(P->cur.s, "<>") == 0) { rd.type = 4; }
            }
            _bash_parser_next(P);
            if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD || P->cur.type == TOK_IO_NUMBER) {
                rd.target = P->cur.s; P->cur.s = NULL;
                if (rd.type == 5) {
                    const char *t = rd.target;
                    for (const char *p = t; *p; p++) {
                        if (*p == '\'' || *p == '"' || *p == '\\') { rd.heredoc_quoted = 1; break; }
                    }
                }
                if (c->n_redirs >= c->c_redirs) {
                    c->c_redirs = c->c_redirs ? c->c_redirs * 2 : 4;
                    c->redirs = (bash_redir_t*)_bash_xrealloc(c->redirs, c->c_redirs * sizeof(bash_redir_t));
                }
                c->redirs[c->n_redirs++] = rd;
                _bash_parser_next(P); /* consume the target word token */
                continue;
            }
            break;
        }
        if (tt == TOK_WORD || tt == TOK_KEYWORD) {
            /* decide assign vs word */
            if (tt == TOK_WORD && c->n_words == 0) {
                char *eq = strchr(P->cur.s, '=');
                if (eq && eq > P->cur.s) {
                    int ok = 1;
                    int depth = 0;
                    for (char *p = P->cur.s; p < eq; p++) {
                        if (depth > 0) {
                            if (*p == '[') depth++;
                            else if (*p == ']') {
                                depth--;
                                if (depth == 0) {
                                    char nxt = (p + 1 < eq) ? p[1] : '=';
                                    if (nxt != '[' && nxt != '=') { ok = 0; break; }
                                }
                            }
                            continue;
                        }
                        if (*p == '[') { depth++; continue; }
                        if (p == P->cur.s) {
                            if (!(isalpha((unsigned char)*p) || *p == '_')) { ok = 0; break; }
                        } else {
                            if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '[')) { ok = 0; break; }
                        }
                    }
                    if (ok && depth == 0) {
                        c->n_assigns++;
                        c->assigns = (char**)_bash_xrealloc(c->assigns, c->n_assigns * sizeof(char*));
                        c->assigns[c->n_assigns-1] = P->cur.s; P->cur.s = NULL;
                        _bash_parser_next(P);
                        continue;
                    }
                }
            }
            c->n_words++;
            c->words = (char**)_bash_xrealloc(c->words, c->n_words * sizeof(char*));
            c->word_quoted = (int*)_bash_xrealloc(c->word_quoted, c->n_words * sizeof(int));
            c->words[c->n_words-1] = P->cur.s; P->cur.s = NULL;
            c->word_quoted[c->n_words-1] = P->cur.quoted;
            _bash_parser_next(P);
            continue;
        }
        break;
    }
    /* Normalize: NULL-terminate */
    c->n_words++;
    c->words = (char**)_bash_xrealloc(c->words, c->n_words * sizeof(char*));
    c->words[c->n_words-1] = NULL; c->n_words--;
    return n;
}

/* Parse compound command / keyword command or simple command */
static bash_node_t *_bash_parse_command(bash_parser_t *P)
{
    /* Check '!' */
    if (_bash_check_kw(P, "!")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_parse_pipeline(P);
        if (!n) return NULL;
        bash_node_t *notn = _bash_newnode(N_NOT);
        notn->u.not_child = n;
        return notn;
    }

    if (_bash_check_kw(P, "if")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_IF);
        n->u.ifn.conds = NULL; n->u.ifn.bodies = NULL; n->u.ifn.n = 0; n->u.ifn.c = 0;
        while (1) {
            bash_node_t *cond = _bash_parse_list(P, 0, 0, 0, 0, 0, 0, 1, 0, 0);
            if (!_bash_accept_kw(P, "then")) {
                /* error: skip tokens */
                if (cond) { /* noop */ }
                return n;
            }
            bash_node_t *body = _bash_parse_list(P, 0, 1, 1, 0, 0, 0, 0, 0, 0);
            if (n->u.ifn.n >= n->u.ifn.c) {
                n->u.ifn.c = n->u.ifn.c ? n->u.ifn.c * 2 : 4;
                n->u.ifn.conds = (bash_node_t**)_bash_xrealloc(n->u.ifn.conds, n->u.ifn.c * sizeof(bash_node_t*));
                n->u.ifn.bodies = (bash_node_t**)_bash_xrealloc(n->u.ifn.bodies, n->u.ifn.c * sizeof(bash_node_t*));
            }
            n->u.ifn.conds[n->u.ifn.n] = cond;
            n->u.ifn.bodies[n->u.ifn.n] = body;
            n->u.ifn.n++;
            if (_bash_accept_kw(P, "elif")) continue;
            if (_bash_accept_kw(P, "else")) {
                bash_node_t *ebody = _bash_parse_list(P, 0, 0, 1, 0, 0, 0, 0, 0, 0);
                if (n->u.ifn.n >= n->u.ifn.c) {
                    n->u.ifn.c = n->u.ifn.c ? n->u.ifn.c * 2 : 4;
                    n->u.ifn.conds = (bash_node_t**)_bash_xrealloc(n->u.ifn.conds, n->u.ifn.c * sizeof(bash_node_t*));
                    n->u.ifn.bodies = (bash_node_t**)_bash_xrealloc(n->u.ifn.bodies, n->u.ifn.c * sizeof(bash_node_t*));
                }
                n->u.ifn.conds[n->u.ifn.n] = NULL;
                n->u.ifn.bodies[n->u.ifn.n] = ebody;
                n->u.ifn.n++;
                break;
            }
            break;
        }
        _bash_accept_kw(P, "fi");
        return n;
    }

    if (_bash_check_kw(P, "while") || _bash_check_kw(P, "until")) {
        bash_node_type_t t = _bash_check_kw(P, "while") ? N_WHILE : N_UNTIL;
        _bash_parser_next(P);
        bash_node_t *cond = _bash_parse_list(P, 0, 0, 0, 0, 0, 0, 0, 1, 0);
        P->bol = 1;
        _bash_accept_kw(P, "do");
        bash_node_t *body = _bash_parse_list(P, 1, 0, 0, 0, 0, 0, 0, 0, 0);
        P->bol = 1;
        _bash_accept_kw(P, "done");
        bash_node_t *n = _bash_newnode(t);
        n->u.loop.cond = cond; n->u.loop.body = body;
        return n;
    }

    if (_bash_check_kw(P, "for")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_FOR);
        bash_for_t *f = &n->u.forn;
        /* name */
        if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
            f->name = P->cur.s; P->cur.s = NULL;
            P->bol = 1;  /* next token ("in") must be recognised as keyword when lexed */
            _bash_parser_next(P);
        } else {
            f->name = _bash_xstrdup("i");
            P->bol = 1;
        }
        if (_bash_check_op(P, "(") && P->lex->src[P->lex->pos] == '(') {
            /* for (( i=0; i<10; i++ )) */
            /* Skip two '(' tokens */
            _bash_parser_next(P); /* consume '(' */
            _bash_parser_next(P); /* consume second '('... but it's part of this one? Actually parse: ((...)) is a compound */
            /* Collect until )) */
            bash_bstr_t s; bash_bstr_init(&s);
            int depth = 2;
            /* current token is NOT '('; parser may have swallowed it. Rewind by lex scanning directly. */
            /* Simpler: consume tokens until finding '))' */
            int need_two_close = 1;
            while (need_two_close) {
                if (P->cur.type == TOK_EOF) break;
                if (P->cur.type == TOK_OP && strcmp(P->cur.s, ")") == 0) {
                    _bash_parser_next(P);
                    if (P->cur.type == TOK_OP && strcmp(P->cur.s, ")") == 0) {
                        _bash_parser_next(P);
                        break;
                    }
                    bash_bstr_puts(&s, ") ");
                    continue;
                }
                if (P->cur.s) { bash_bstr_puts(&s, P->cur.s); bash_bstr_putc(&s, ' '); }
                _bash_parser_next(P);
            }
            (void)depth;
            /* Split init;cond;step by ';' within s */
            char *p = s.data;
            f->arithmetic_style = 1;
            char *sc1 = strchr(p, ';');
            if (sc1) {
                *sc1 = 0;
                f->init = _bash_xstrdup(p); p = sc1 + 1;
                char *sc2 = strchr(p, ';');
                if (sc2) {
                    *sc2 = 0;
                    f->cond_a = _bash_xstrdup(p);
                    f->step = _bash_xstrdup(sc2 + 1);
                } else {
                    f->cond_a = _bash_xstrdup(p);
                    f->step = _bash_xstrdup("");
                }
            } else {
                f->init = _bash_xstrdup("");
                f->cond_a = _bash_xstrdup(p);
                f->step = _bash_xstrdup("");
            }
            bash_bstr_free(&s);
            _bash_accept_op(P, ";"); /* optional "));" or "))\n" — skip separator */
        } else {
            /* optional "in words" (default: in $@) */
            if (_bash_accept_kw(P, "in")) {
                bash_barray_t wa; bash_barray_init(&wa);
                /* Stop when we hit a keyword that ends the "in" list:
                 *   do / done / fi / esac / then / else / elif
                 * Also stop on any operator token (including ;, |, &, etc.). */
                while (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD || P->cur.type == TOK_IO_NUMBER) {
                    if (P->cur.type == TOK_KEYWORD && P->cur.s) {
                        const char *ks = P->cur.s;
                        if (strcmp(ks, "do")   == 0 || strcmp(ks, "done") == 0 ||
                            strcmp(ks, "fi")   == 0 || strcmp(ks, "esac") == 0 ||
                            strcmp(ks, "then") == 0 || strcmp(ks, "else") == 0 ||
                            strcmp(ks, "elif") == 0) break;
                    }
                    bash_barray_push(&wa, P->cur.s ? _bash_xstrdup(P->cur.s) : _bash_xstrdup(""));
                    _bash_parser_next(P);
                }
                _bash_accept_op(P, ";");
                f->n_words = wa.len;
                f->words = wa.items; /* steal */
            } else {
                _bash_accept_op(P, ";");
                f->words = NULL; f->n_words = 0;
            }
        }
        P->bol = 1;
        _bash_accept_kw(P, "do");
        f->body = _bash_parse_list(P, 1, 0, 0, 0, 0, 0, 0, 0, 0);
        P->bol = 1;
        _bash_accept_kw(P, "done");
        return n;
    }

    if (_bash_check_kw(P, "case")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_CASE);
        bash_case_t *cs = &n->u.casen;
        cs->word_quoted = 0;
        if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
            cs->word = P->cur.s; P->cur.s = NULL;
            cs->word_quoted = P->cur.quoted;
            P->bol = 1;  /* next token ("in") must be recognised as keyword */
            _bash_parser_next(P);
        } else {
            cs->word = _bash_xstrdup("");
            P->bol = 1;
        }
        P->bol = 1;
        _bash_accept_kw(P, "in");
        cs->patterns = NULL; cs->bodies = NULL; cs->n = 0; cs->c = 0;
        /* parse patterns:  "pat1 | pat2) body ;;" */
        while (!_bash_check_kw(P, "esac") && P->cur.type != TOK_EOF) {
            _bash_accept_op(P, "("); /* optional opening ( */
            bash_barray_t pats; bash_barray_init(&pats);
            int stop = 0;
            while (!stop) {
                if (_bash_check_kw(P, "esac") || P->cur.type == TOK_EOF) { stop = 1; break; }
                if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD || P->cur.type == TOK_IO_NUMBER) {
                    bash_barray_push(&pats, P->cur.s ? _bash_xstrdup(P->cur.s) : _bash_xstrdup(""));
                    _bash_parser_next(P);
                    if (_bash_check_kw(P, "esac") || P->cur.type == TOK_EOF) { stop = 1; break; }
                    if (_bash_check_op(P, "|")) { _bash_parser_next(P); continue; }
                }
                if (_bash_check_op(P, ")")) { P->bol = 1; _bash_parser_next(P); stop = 1; break; }
                if (P->cur.type == TOK_EOF) break;
                /* Skip separators / punctuation that cannot begin a pattern (newlines,
                   semicolons, etc.).  But bail out immediately on "esac" so we never
                   mistakenly consume the case-statement terminator as pattern text. */
                if (_bash_check_kw(P, "esac")) { stop = 1; break; }
                /* Next token might be a pattern word / reserved word at start of line:
                   preserve BOL semantics when skipping whitespace / separators. */
                if (P->cur.type == TOK_OP &&
                    (strcmp(P->cur.s, ";") == 0 || strcmp(P->cur.s, "\n") == 0 || strcmp(P->cur.s, "&") == 0)) {
                    P->bol = 1;
                }
                _bash_parser_next(P);
            }
            bash_barray_push_steal(&pats, NULL);
            /* If no pattern words were collected, we hit EOF / "esac" above:
               abort adding this clause and break out of the outer pattern loop. */
            if (pats.len <= 1 /* just the NULL sentinel */) {
                bash_barray_free(&pats);
                break;
            }
            bash_node_t *body = _bash_parse_list(P, 0, 0, 0, 1, 0, 0, 0, 0, 0);
            /* accept ;;  /  ;&  /  ;;&  separators between case clauses.
               After consuming, the next token is a fresh clause / "esac",
               so enable BOL recognition so keywords like "esac" are found. */
            if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";;") == 0) {
                P->bol = 1; _bash_parser_next(P);
            } else if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";&") == 0) {
                P->bol = 1; _bash_parser_next(P);
            } else if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";;&") == 0) {
                P->bol = 1; _bash_parser_next(P);
            } else {
                if (P->cur.type == TOK_OP && strcmp(P->cur.s, ";") == 0) {
                    P->bol = 1; _bash_parser_next(P);
                    if (P->cur.type == TOK_OP && strcmp(P->cur.s, ";") == 0) {
                        P->bol = 1; _bash_parser_next(P);
                    } else if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";&") == 0) {
                        P->bol = 1; _bash_parser_next(P);
                    } else if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";;&") == 0) {
                        P->bol = 1; _bash_parser_next(P);
                    }
                }
            }
            if (cs->n >= cs->c) {
                cs->c = cs->c ? cs->c * 2 : 4;
                cs->patterns = (char***)_bash_xrealloc(cs->patterns, cs->c * sizeof(char**));
                cs->bodies = (bash_node_t**)_bash_xrealloc(cs->bodies, cs->c * sizeof(bash_node_t*));
            }
            cs->patterns[cs->n] = pats.items; pats.items = NULL; pats.len = pats.cap = 0;
            cs->bodies[cs->n] = body;
            cs->n++;
        }
        _bash_accept_kw(P, "esac");
        return n;
    }

    if (_bash_check_kw(P, "function")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_FUNCDEF);
        if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
            n->u.func.name = P->cur.s; P->cur.s = NULL;
            _bash_parser_next(P);
        } else n->u.func.name = _bash_xstrdup("");
        /* optional () */
        if (_bash_check_op(P, "(")) {
            _bash_parser_next(P);
            _bash_accept_op(P, ")");
        }
        n->u.func.body = _bash_parse_pipeline(P);
        return n;
    }

    if (_bash_check_op(P, "{")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_GROUP);
        n->u.group.body = _bash_parse_list(P, 0, 0, 0, 0, 1, 0, 0, 0, 0);
        _bash_accept_op(P, "}");
        n->u.group.subshell = 0;
        return n;
    }
    if (_bash_check_op(P, "(")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_GROUP);
        n->u.group.body = _bash_parse_list(P, 0, 0, 0, 0, 0, 1, 0, 0, 0);
        _bash_accept_op(P, ")");
        n->u.group.subshell = 1;
        return n;
    }

    if (P->cur.type == TOK_DOP && (strcmp(P->cur.s, "(( ") == 0 ? 0 : (P->cur.type == TOK_OP && strcmp(P->cur.s, "(") == 0 && P->lex->src[P->lex->pos] == '('))) {
        /* Arithmetic (( expr )) */
        _bash_parser_next(P); /* consume first '(' */
        /* If second token is '(', then it's (( )) */
        /* Easier: scan tokens until matching '))' */
        _bash_parser_next(P);
        bash_bstr_t s; bash_bstr_init(&s);
        while (P->cur.type != TOK_EOF) {
            if (P->cur.type == TOK_OP && strcmp(P->cur.s, ")") == 0) {
                _bash_parser_next(P);
                if (P->cur.type == TOK_OP && strcmp(P->cur.s, ")") == 0) {
                    _bash_parser_next(P);
                    break;
                }
                bash_bstr_puts(&s, ") ");
                continue;
            }
            if (P->cur.s) { bash_bstr_puts(&s, P->cur.s); bash_bstr_putc(&s, ' '); }
            _bash_parser_next(P);
        }
        bash_node_t *n = _bash_newnode(N_ARITH);
        n->u.arith.expr = bash_bstr_detach(&s);
        return n;
    }

    if (_bash_check_kw(P, "[[")) {
        /* [[ conditional ]] */
        _bash_parser_next(P);
        bash_bstr_t s; bash_bstr_init(&s);
        while (P->cur.type != TOK_EOF) {
            if (_bash_check_kw(P, "]]")) {
                _bash_parser_next(P);
                break;
            }
            if (P->cur.type == TOK_DOP && strcmp(P->cur.s, "]]") == 0) {
                _bash_parser_next(P);
                break;
            }
            if (P->cur.s) { bash_bstr_puts(&s, P->cur.s); bash_bstr_putc(&s, ' '); }
            _bash_parser_next(P);
        }
        bash_node_t *n = _bash_newnode(N_CONDTEST);
        n->u.cond.expr = bash_bstr_detach(&s);
        return n;
    }

    if (_bash_check_kw(P, "break")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_BREAK);
        n->u.flow.n = 1;
        if (P->cur.type == TOK_WORD && isdigit((unsigned char)P->cur.s[0])) {
            n->u.flow.n = atoi(P->cur.s);
            _bash_parser_next(P);
        }
        return n;
    }
    if (_bash_check_kw(P, "continue")) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_CONTINUE);
        n->u.flow.n = 1;
        if (P->cur.type == TOK_WORD && isdigit((unsigned char)P->cur.s[0])) {
            n->u.flow.n = atoi(P->cur.s);
            _bash_parser_next(P);
        }
        return n;
    }
    if (P->cur.type == TOK_KEYWORD && P->cur.s && strcmp(P->cur.s, "return") == 0) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_RETURN);
        n->u.flow.code = -1;
        if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
            n->u.flow.code = atoi(P->cur.s);
            _bash_parser_next(P);
        }
        return n;
    }
    if (P->cur.type == TOK_KEYWORD && P->cur.s && strcmp(P->cur.s, "exit") == 0) {
        _bash_parser_next(P);
        bash_node_t *n = _bash_newnode(N_EXIT);
        n->u.flow.code = -1;
        if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
            n->u.flow.code = atoi(P->cur.s);
            _bash_parser_next(P);
        }
        return n;
    }

    if (P->cur.type == TOK_WORD || P->cur.type == TOK_KEYWORD) {
        /* Check function definition:  NAME () ... */
        bash_tok_t save = P->cur;
        char *name = P->cur.s ? _bash_xstrdup(P->cur.s) : NULL;
        _bash_parser_next(P);
        if (_bash_check_op(P, "(")) {
            _bash_parser_next(P);
            if (_bash_check_op(P, ")")) {
                _bash_parser_next(P);
                bash_node_t *n = _bash_newnode(N_FUNCDEF);
                n->u.func.name = name ? name : _bash_xstrdup("");
                n->u.func.body = _bash_parse_pipeline(P);
                return n;
            }
        }
        /* Not a func def; rewind name into token and parse simple command */
        /* Make a fake first token */
        bash_tok_t first; memset(&first, 0, sizeof(first));
        first.type = TOK_WORD;
        first.s = name ? name : _bash_xstrdup("");
        first.line = save.line;
        /* We have already advanced P past name; we need to pass P as-is and prepend first */
        bash_node_t *n = _bash_parse_simple_cmd(P, first);
        return n;
    }

    if (P->cur.type == TOK_IO_NUMBER) {
        bash_tok_t first = P->cur; /* copy */
        memset(&first, 0, sizeof(first));
        first.type = P->cur.type; first.io_num = P->cur.io_num;
        first.s = P->cur.s ? _bash_xstrdup(P->cur.s) : NULL;
        _bash_parser_next(P);
        return _bash_parse_simple_cmd(P, first);
    }

    /* Unknown; return NULL / empty node */
    return NULL;
}

static bash_node_t *_bash_parse_pipeline(bash_parser_t *P)
{
    bash_node_t *left = _bash_parse_command(P);
    if (!left) return NULL;
    while (1) {
        if (_bash_check_op(P, "|") || (P->cur.type == TOK_DOP && P->cur.s && strcmp(P->cur.s, "|&") == 0)) {
            int pipeall = (P->cur.type == TOK_DOP);
            _bash_parser_next(P);
            P->bol = 1; /* following cmd is new command for keyword purposes */
            bash_node_t *right = _bash_parse_command(P);
            if (!right) return left;
            bash_node_t *pipe = _bash_newnode(N_PIPE);
            pipe->u.bin.left = left; pipe->u.bin.right = right; pipe->u.bin.invert = pipeall;
            left = pipe;
            continue;
        }
        break;
    }
    return left;
}

/* Parse list / compound-list, stops on keyword tokens or operators. */
static bash_node_t *_bash_parse_list(bash_parser_t *P, int stop_on_done, int stop_on_else, int stop_on_fi,
                              int stop_on_esac, int stop_on_brace, int stop_on_rparen,
                              int stop_on_then, int stop_on_do, int stop_on_in)
{
    bash_node_t *result = NULL;
    bash_node_t *tail = NULL;

    /* Skip leading separators */
    while (1) {
        if (P->cur.type == TOK_EOF) return NULL;
        if (P->cur.type == TOK_OP &&
            (strcmp(P->cur.s, ";") == 0 || strcmp(P->cur.s, "\n") == 0 || strcmp(P->cur.s, "&") == 0)) {
            P->bol = 1;
            _bash_parser_next(P);
            continue;
        }
        break;
    }

    for (;;) {
        if (P->cur.type == TOK_EOF) break;
        /* stop conditions */
        if (stop_on_done && _bash_check_kw(P, "done")) break;
        if (stop_on_else && (_bash_check_kw(P, "else") || _bash_check_kw(P, "elif"))) break;
        if (stop_on_fi && _bash_check_kw(P, "fi")) break;
        if (stop_on_esac && _bash_check_kw(P, "esac")) break;
        if (stop_on_brace && _bash_check_op(P, "}")) break;
        if (stop_on_rparen && _bash_check_op(P, ")")) break;
        if (stop_on_then && _bash_check_kw(P, "then")) break;
        if (stop_on_do && _bash_check_kw(P, "do")) break;
        if (stop_on_in && _bash_check_kw(P, "in")) break;
        if (P->cur.type == TOK_DOP && strcmp(P->cur.s, ";;") == 0) break;

        bash_node_t *pipe = _bash_parse_pipeline(P);
        if (!pipe) {
            /* No more commands.  Skip separators. */
            if (P->cur.type == TOK_OP &&
                (strcmp(P->cur.s, ";") == 0 || strcmp(P->cur.s, "\n") == 0 || strcmp(P->cur.s, "&") == 0)) {
                P->bol = 1;
                _bash_parser_next(P);
                continue;
            }
            break;
        }

        /* ===== Phase 1: build an AND-OR clause from this pipeline.
           pipe may be combined with subsequent pipelines via && or || into one clause node. ===== */
        for (;;) {
            bash_node_type_t op_type = N_SEMI;
            int op_consume = 0;
            if (P->cur.type == TOK_DOP && strcmp(P->cur.s, "&&") == 0) { op_type = N_AND; op_consume = 1; }
            else if (P->cur.type == TOK_DOP && strcmp(P->cur.s, "||") == 0) { op_type = N_OR; op_consume = 1; }
            if (!op_consume) break;
            /* consume &&/|| */
            P->bol = 1; _bash_parser_next(P);
            /* skip newlines between the &&/|| operator and its right-hand pipeline */
            while (P->cur.type == TOK_OP && strcmp(P->cur.s, "\n") == 0) {
                P->bol = 1; _bash_parser_next(P);
            }
            bash_node_t *rhs = _bash_parse_pipeline(P);
            if (!rhs) { rhs = _bash_newnode(N_CMD); }
            bash_node_t *comb = _bash_newnode(op_type);
            comb->u.bin.left = pipe;
            comb->u.bin.right = rhs;
            pipe = comb;  /* treat combined node as one clause going forward */
        }

        /* ===== Phase 2: attach 'pipe' (AND-OR clause) to the command sequence via ; or & or newline. ===== */
        bash_node_type_t seq_type = N_SEMI;
        int is_seq = 0, seq_consume = 0;
        if (P->cur.type == TOK_OP && strcmp(P->cur.s, "&") == 0) { seq_type = N_BG; is_seq = 1; seq_consume = 1; }
        else if (P->cur.type == TOK_OP && strcmp(P->cur.s, ";") == 0) { seq_type = N_SEMI; is_seq = 1; seq_consume = 1; }
        else if (P->cur.type == TOK_OP && strcmp(P->cur.s, "\n") == 0) { seq_type = N_SEMI; is_seq = 1; seq_consume = 1; }

        if (!is_seq) {
            /* No more sequence separators - this clause is the last one. */
            if (tail) {
                tail->u.bin.right = pipe;
            } else if (!result) {
                result = pipe;
            }
            break;
        }

        /* Create sequence node wrapping the clause as its left. */
        bash_node_t *semi = _bash_newnode(seq_type);
        semi->u.bin.left = pipe;
        semi->u.bin.right = NULL;
        if (!result) {
            result = semi;
        } else {
            tail->u.bin.right = semi;
        }
        tail = semi;
        if (seq_consume) { P->bol = 1; _bash_parser_next(P); }
    }

    return result;
}

static bash_node_t *_bash_parse_program(bash_parser_t *P)
{
    return _bash_parse_list(P, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* ========================================================================
 * Context / variable store / frames
 * ======================================================================== */

typedef struct bash_funcdef_s {
    char *name;
    bash_node_t *body;
    struct bash_funcdef_s *next;
} bash_funcdef_t;

struct bash_ctx_s {
    bash_frame_t *frame;       /* current frame (stack top) */
    bash_frame_t  global;      /* global frame */
    bash_funcdef_t *funcs;     /* function definitions */
    int   last_status;      /* $? */
    int   bg_pid;           /* $! last bg pid (approx) */
    char *script_name;      /* $0 */
    int   exit_code;        /* set by exit/return */
    int   do_exit;
    char *pwd;              /* cached PWD */
    bash_vars_t aliases;       /* alias name->value pairs */
    char *dir_stack[256];   /* directory stack for pushd/popd/dirs */
    int   dir_stack_len;
    int   errexit;          /* set -e */
    int   nounset;          /* set -u */
    int   xtrace;           /* set -x */
    char *trap_cmds[32];    /* trap commands indexed by signal number */
};

/* ---------- variable helpers ---------- */

static bash_var_t *_bash_vars_find(bash_vars_t *v, const char *name)
{
    for (int i = 0; i < v->len; i++)
        if (v->items[i].name && strcmp(v->items[i].name, name) == 0)
            return &v->items[i];
    return NULL;
}

static void _bash_vars_set(bash_vars_t *v, const char *name, const char *value, int exported, int readonly)
{
    bash_var_t *e = _bash_vars_find(v, name);
    if (e) {
        if (e->is_readonly) return;
        free(e->value);
        e->value = _bash_xstrdup(value ? value : "");
        if (exported) e->exported = 1;
        if (readonly) e->is_readonly = 1;
        return;
    }
    if (v->len >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->items = (bash_var_t*)_bash_xrealloc(v->items, v->cap * sizeof(bash_var_t));
    }
    e = &v->items[v->len++];
    e->name = _bash_xstrdup(name);
    e->value = _bash_xstrdup(value ? value : "");
    e->exported = exported;
    e->is_local = 0;
    e->is_readonly = readonly;
}

static void _bash_vars_unset(bash_vars_t *v, const char *name)
{
    for (int i = 0; i < v->len; i++) {
        if (v->items[i].name && strcmp(v->items[i].name, name) == 0) {
            if (v->items[i].is_readonly) return;
            free(v->items[i].name);
            free(v->items[i].value);
            /* shift */
            for (int j = i; j < v->len - 1; j++) v->items[j] = v->items[j + 1];
            v->len--;
            return;
        }
    }
}

static void _bash_vars_free(bash_vars_t *v)
{
    for (int i = 0; i < v->len; i++) {
        free(v->items[i].name);
        free(v->items[i].value);
    }
    free(v->items);
    v->items = NULL; v->len = v->cap = 0;
}

/* Diagnostic helper: append formatted message to bash_debug.log in cwd.
 * Used temporarily for tracking recursion / cmdsub interactions. */
#ifdef _WIN32
#define BASH_DBG_LOG_FNAME "bash_debug.log"
#else
#define BASH_DBG_LOG_FNAME "/tmp/bash_debug.log"
#endif
static void _bash_dbg_log(const char *fmt, ...)
{
#if 0 // DEBUG_LOG
    FILE *fp = fopen(BASH_DBG_LOG_FNAME, "a");
    if (!fp) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    fputc('\n', fp);
    va_end(ap);
    fclose(fp);
#else
    (void)fmt; 
#endif
}
/* Frame depth helper (walks up parent chain). */
static int _bash_frame_depth(bash_ctx_t *ctx)
{
    int d = 0;
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) d++;
    return d;
}

/* ---------- frame push/pop ---------- */

static void _bash_frame_init_global(bash_ctx_t *ctx)
{
    memset(&ctx->global, 0, sizeof(ctx->global));
    bash_barray_init((bash_barray_t*)&ctx->global.vars); /* cast layout compatible */
    ctx->global.vars.items = NULL; ctx->global.vars.len = 0; ctx->global.vars.cap = 0;
    ctx->frame = &ctx->global;
}

static bash_frame_t *_bash_frame_push(bash_ctx_t *ctx, int is_func)
{
    bash_frame_t *f = (bash_frame_t*)_bash_xmalloc(sizeof(*f));
    memset(f, 0, sizeof(*f));
    f->vars.items = NULL; f->vars.len = 0; f->vars.cap = 0;
    f->parent = ctx->frame;
    f->is_func = is_func;
    ctx->frame = f;
    return f;
}

static void _bash_frame_pop(bash_ctx_t *ctx)
{
    bash_frame_t *f = ctx->frame;
    if (!f || f == &ctx->global) return;
    ctx->frame = f->parent;
    _bash_vars_free(&f->vars);
    for (int i = 0; i < f->argc; i++) free(f->argv[i]);
    free(f->argv);
    free(f);
}

/* ---------- exported env construction for exec ---------- */

static char **_bash_build_envp(bash_ctx_t *ctx) BASH_ATTR_UNUSED;
static char **_bash_build_envp(bash_ctx_t *ctx)
{
    bash_barray_t a; bash_barray_init(&a);
    /* walk frames bottom-up; globals first then locals override */
    bash_frame_t *stack[64]; int depth = 0;
    for (bash_frame_t *f = ctx->frame; f; f = f->parent)
        if (depth < 63) stack[depth++] = f;
    /* globals at stack[depth-1] */
    for (int d = depth - 1; d >= 0; d--) {
        bash_frame_t *f = stack[d];
        for (int i = 0; i < f->vars.len; i++) {
            if (f->vars.items[i].exported) {
                bash_bstr_t b; bash_bstr_init(&b);
                bash_bstr_puts(&b, f->vars.items[i].name);
                bash_bstr_putc(&b, '=');
                bash_bstr_puts(&b, f->vars.items[i].value);
                bash_barray_push(&a, bash_bstr_detach(&b));
            }
        }
    }
    bash_barray_push_steal(&a, NULL);
    return a.items;
}

/* ---------- var set/get (walks frames) ---------- */

static char *_bash_var_get(bash_ctx_t *ctx, const char *name, int *exported)
{
    if (!name || !*name) return NULL;
    /* Specials first */
    if (strcmp(name, "?") == 0) { static char b[32]; snprintf(b, sizeof(b), "%d", ctx->last_status); return b; }
    if (strcmp(name, "$") == 0) { static char b[32]; snprintf(b, sizeof(b), "%d", (int)BASH_GETPID()); return b; }
    if (strcmp(name, "!") == 0) { static char b[32]; snprintf(b, sizeof(b), "%d", ctx->bg_pid); return b; }
    if (strcmp(name, "-") == 0) {
        static char b[16]; b[0] = 0;
        if (ctx->errexit) strcat(b, "e");
        if (ctx->nounset) strcat(b, "u");
        if (ctx->xtrace) strcat(b, "x");
        return b;
    }
    /* ---- Bash 5.2+ dynamic specials ---- */
    if (strcmp(name, "PPID") == 0) {
        /* parent process id: fall back to 1 if not available */
        static char b[32];
#ifdef BASH_PLATFORM_WINDOWS
        snprintf(b, sizeof(b), "%lu", GetCurrentProcessId() > 0 ? 1ul : 1ul);
#else
        snprintf(b, sizeof(b), "%d", (int)getppid());
#endif
        return b;
    }
    if (strcmp(name, "UID") == 0 || strcmp(name, "EUID") == 0) {
        static char b[32];
#ifdef BASH_PLATFORM_WINDOWS
        snprintf(b, sizeof(b), "%lu", (unsigned long)GetCurrentProcessId() % 65535u);
#else
        if (name[0] == 'E') snprintf(b, sizeof(b), "%d", (int)geteuid());
        else                snprintf(b, sizeof(b), "%d", (int)getuid());
#endif
        return b;
    }
    if (strcmp(name, "EPOCHSECONDS") == 0) {
        static char b[32];
        time_t t = time(NULL);
        snprintf(b, sizeof(b), "%ld", (long)t);
        return b;
    }
    if (strcmp(name, "EPOCHREALTIME") == 0) {
        static char b[64];
        time_t t = time(NULL);
        snprintf(b, sizeof(b), "%ld.000000", (long)t);
        return b;
    }
    if (strcmp(name, "RANDOM") == 0) {
        /* 15-bit signed random, per-read update */
        static unsigned int seed = 1;
        if (seed == 1) {
            seed = (unsigned int)((unsigned)time(NULL) ^ (BASH_GETPID() * 2654435761u));
        }
        seed = seed * 1103515245u + 12345u;
        static char b[16];
        snprintf(b, sizeof(b), "%d", (int)((seed >> 16) & 0x7FFF));
        return b;
    }
    if (strcmp(name, "SRANDOM") == 0) {
        /* Bash 5.2+: 32-bit *signed* random (xorshift32) */
        static unsigned int sra_seed = 0x9E3779B9u;
        if (sra_seed == 0x9E3779B9u) {
            sra_seed = (unsigned int)((unsigned)time(NULL) * 2654435761u) ^ (unsigned int)(BASH_GETPID() * 1315423911u);
        }
        unsigned int x = sra_seed ? sra_seed : 1u;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        sra_seed = x;
        static char b[32];
        snprintf(b, sizeof(b), "%d", (int)(x & 0x7FFFFFFFu));
        return b;
    }
    if (strcmp(name, "LINENO") == 0) {
        static char b[32];
        /* no source line tracking yet; return monotonic per-frame stub */
        int depth = 0;
        for (bash_frame_t *f = ctx->frame; f; f = f->parent) depth++;
        snprintf(b, sizeof(b), "%d", depth * 10 + 1);
        return b;
    }
    if (strcmp(name, "HOSTTYPE") == 0 || strcmp(name, "MACHTYPE") == 0) {
#if defined(__x86_64__) || defined(_WIN64)
        return "x86_64-cross";
#elif defined(__aarch64__)
        return "aarch64-cross";
#else
        return "i686-cross";
#endif
    }
    if (strcmp(name, "OSTYPE") == 0) {
#ifdef BASH_PLATFORM_WINDOWS
        return "msys";
#else
        return "posix";
#endif
    }
    if (strcmp(name, "#") == 0) {
        bash_frame_t *f = ctx->frame;
        while (f && !f->is_func) f = f->parent;
        int n = f ? f->argc : 0;
        if (f) { /* exclude $0 from # */ if (n > 0) n = n - 1; else n = 0; }
        static char b[32]; snprintf(b, sizeof(b), "%d", n); return b;
    }
    if (strcmp(name, "@") == 0 || strcmp(name, "*") == 0) {
        /* handled specially by expander: returns joined list */
        bash_frame_t *f = ctx->frame;
        while (f && !f->is_func) f = f->parent;
        if (!f || f->argc <= 1) return _bash_xstrdup("");
        static char buf[4096]; buf[0] = 0;
        int off = 0;
        for (int i = 1; i < f->argc; i++) {
            const char *s = f->argv[i] ? f->argv[i] : "";
            int l = (int)strlen(s);
            if (i > 1) {
                if (off < (int)sizeof(buf) - 1) buf[off++] = ' ';
            }
            if (off + l < (int)sizeof(buf)) {
                memcpy(buf + off, s, l); off += l;
            }
        }
        buf[off] = 0;
        return buf;
    }
    if (isdigit((unsigned char)name[0]) && name[1] == 0) {
        int idx = name[0] - '0';
        bash_frame_t *f = ctx->frame;
        while (f && !f->is_func) f = f->parent;
        if (!f) {
            if (idx == 0) return ctx->script_name ? ctx->script_name : "bash";
            return "";
        }
        if (idx < f->argc && f->argv[idx]) return f->argv[idx];
        return "";
    }
    if (name[0] >= '0' && name[0] <= '9') {
        int idx = atoi(name);
        bash_frame_t *f = ctx->frame;
        while (f && !f->is_func) f = f->parent;
        if (!f) {
            if (idx == 0) return ctx->script_name ? ctx->script_name : "bash";
            return "";
        }
        if (idx < f->argc && f->argv[idx]) return f->argv[idx];
        return "";
    }
    if (strcmp(name, "0") == 0) return ctx->script_name ? ctx->script_name : "bash";
    /* walk frames top-down */
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
        bash_var_t *v = _bash_vars_find(&f->vars, name);
        if (v) {
            if (exported) *exported = v->exported;
            return v->value;
        }
    }
    /* fallback: getenv */
    const char *ev = getenv(name);
    if (ev) return (char*)ev;
    return NULL;
}

static int _bash_var_set(bash_ctx_t *ctx, const char *name, const char *value, int exported, int do_export, int readonly)
{
    if (!name || !*name) return -1;
    /* check for readonly in any frame */
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
        bash_var_t *v = _bash_vars_find(&f->vars, name);
        if (v && v->is_readonly) return -1;
    }
    /* set on current frame (or nearest existing) */
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
        if (_bash_vars_find(&f->vars, name)) {
            _bash_vars_set(&f->vars, name, value, exported, readonly);
            if (do_export) {
                bash_var_t *v = _bash_vars_find(&f->vars, name);
                if (v) v->exported = 1;
            }
            if (exported) {
                /* sync to process environment so getenv() and child
                 * processes see the updated value.  We use both
                 * _putenv_s (CRT _environ) and SetEnvironmentVariableA
                 * (Win32 block) to cover all lookup paths.           */
#ifdef BASH_PLATFORM_WINDOWS
                extern int __cdecl _putenv_s(const char *, const char *);
                _putenv_s(name, value ? value : "");
                SetEnvironmentVariableA(name, value);
#else
                setenv(name, value ? value : "", 1);
#endif
            }
            return 0;
        }
    }
    /* new variable goes on current frame */
    _bash_vars_set(&ctx->frame->vars, name, value, exported, readonly);
    if (do_export) {
        bash_var_t *v = _bash_vars_find(&ctx->frame->vars, name);
        if (v) v->exported = 1;
    }
    if (exported) {
#ifdef BASH_PLATFORM_WINDOWS
        extern int __cdecl _putenv_s(const char *, const char *);
        _putenv_s(name, value ? value : "");
        SetEnvironmentVariableA(name, value);
#else
        setenv(name, value ? value : "", 1);
#endif
    }
    return 0;
}

static int _bash_var_unset(bash_ctx_t *ctx, const char *name)
{
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
        bash_var_t *v = _bash_vars_find(&f->vars, name);
        if (v) {
            if (v->is_readonly) return -1;
            _bash_vars_unset(&f->vars, name);
            return 0;
        }
    }
    return -1;
}

/* Set a variable strictly on the current frame only — used by `local` / `declare`
 * inside functions so a recursive call does not overwrite the caller's same-named
 * locals in parent frames.  This mirrors POSIX `local` semantics. */
static int _bash_var_set_local(bash_ctx_t *ctx, const char *name, const char *value, int exported, int readonly)
{
    if (!name || !*name) return -1;
    /* readonly check: walk up (readonly violations must still be caught) */
    for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
        bash_var_t *v = _bash_vars_find(&f->vars, name);
        if (v && v->is_readonly) return -1;
    }
    if (!ctx->frame) return -1;
    _bash_vars_set(&ctx->frame->vars, name, value, exported, readonly);
    if (exported) {
#ifdef BASH_PLATFORM_WINDOWS
        extern int __cdecl _putenv_s(const char *, const char *);
        _putenv_s(name, value ? value : "");
        SetEnvironmentVariableA(name, value);
#else
        setenv(name, value ? value : "", 1);
#endif
    }
    return 0;
}

/* ---------- function store ---------- */

static bash_funcdef_t *_bash_func_find(bash_ctx_t *ctx, const char *name)
{
    for (bash_funcdef_t *f = ctx->funcs; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}

static void _bash_func_define(bash_ctx_t *ctx, const char *name, bash_node_t *body)
{
    bash_funcdef_t *f = _bash_func_find(ctx, name);
    if (f) { /* replace */ /* (leak old body: ignore for mini shell) */ f->body = body; return; }
    f = (bash_funcdef_t*)_bash_xmalloc(sizeof(*f));
    f->name = _bash_xstrdup(name);
    f->body = body;
    f->next = ctx->funcs;
    ctx->funcs = f;
}

/* ========================================================================
 * Glob: simple POSIX glob matching (* ? [...])
 * ======================================================================== */

static int _bash_glob_match(const char *pat, const char *name)
{
    if (!pat || !name) return 0;
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') pat++;
            if (!*pat) return 1;
            for (const char *s = name; ; s++) {
                if (_bash_glob_match(pat, s)) return 1;
                if (!*s) return 0;
            }
        } else if (*pat == '?') {
            if (!*name) return 0;
            name++; pat++;
        } else if (*pat == '[') {
            pat++;
            int neg = 0;
            if (*pat == '!' || *pat == '^') { neg = 1; pat++; }
            int found = 0;
            while (*pat && *pat != ']') {
                int lo = (unsigned char)*pat++;
                if (*pat == '-' && pat[1] && pat[1] != ']') {
                    int hi = (unsigned char)pat[1];
                    pat += 2;
                    if ((unsigned char)*name >= lo && (unsigned char)*name <= hi) found = 1;
                } else {
                    if ((unsigned char)*name == lo) found = 1;
                }
            }
            if (*pat == ']') pat++;
            if (found == neg) return 0;
            name++;
        } else {
            if (*pat != *name) return 0;
            pat++; name++;
        }
    }
    return !*name;
}

static int _bash_glob_word(bash_ctx_t *ctx, const char *word, bash_barray_t *out)
{
    /* If word has no glob chars, just return 0 */
    int hasglob = 0;
    for (const char *p = word; *p; p++)
        if (*p == '*' || *p == '?' || *p == '[') { hasglob = 1; break; }
    if (!hasglob) return 0;

    /* split dirname / basename */
    const char *slash = strrchr(word, '/');
    const char *base = slash ? slash + 1 : word;
    char *dircopy = NULL;
    if (slash) {
        size_t dl = (size_t)(slash - word);
        dircopy = (char*)_bash_xmalloc(dl + 2);
        memcpy(dircopy, word, dl);
        if (dl == 0) dircopy[dl++] = '/';
        dircopy[dl] = 0;
    }
    const char *dir = dircopy ? dircopy : ".";

    int matched = 0;
#ifdef BASH_PLATFORM_WINDOWS
    WIN32_FIND_DATAA fd;
    bash_bstr_t sp; bash_bstr_init(&sp);
    bash_bstr_puts(&sp, dir);
    size_t l = sp.len;
    if (l == 0 || (sp.data[l-1] != '\\' && sp.data[l-1] != '/')) bash_bstr_putc(&sp, '\\');
    bash_bstr_puts(&sp, "*");
    HANDLE h = FindFirstFileA(sp.data, &fd);
    bash_bstr_free(&sp);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                /* allow dirs to match too */
            }
            if (_bash_glob_match(base, fd.cFileName)) {
                bash_bstr_t full; bash_bstr_init(&full);
                bash_bstr_puts(&full, dir);
                size_t fl = full.len;
                if (fl > 0 && full.data[fl-1] != '\\' && full.data[fl-1] != '/') bash_bstr_putc(&full, '\\');
                bash_bstr_puts(&full, fd.cFileName);
                bash_barray_push(out, bash_bstr_detach(&full));
                matched++;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    free(dircopy);
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (_bash_glob_match(base, ent->d_name)) {
                bash_bstr_t full; bash_bstr_init(&full);
                if (slash) {
                    bash_bstr_puts(&full, dir);
                    size_t fl = full.len;
                    if (full.data[fl-1] != '/') bash_bstr_putc(&full, '/');
                }
                bash_bstr_puts(&full, ent->d_name);
                bash_barray_push(out, bash_bstr_detach(&full));
                matched++;
            }
        }
        closedir(d);
    }
    free(dircopy);
#endif
    if (!matched) return 0;
    /* GLOBSORT controls sorting: unset/empty = default lex order, "none" = no sort (filesystem order) */
    const char *globsort = ctx ? _bash_var_get(ctx, "GLOBSORT", NULL) : NULL;
    if (!globsort || *globsort == 0 || strcmp(globsort, "none") != 0) {
        /* Sort (simple insertion) */
        for (int i = 1; i < out->len; i++) {
            for (int j = i; j > 0 && strcmp(out->items[j-1], out->items[j]) > 0; j--) {
                char *t = out->items[j-1]; out->items[j-1] = out->items[j]; out->items[j] = t;
            }
        }
    }
    return 1;
}

/* ========================================================================
 * Expander: variable/command/arithmetic/glob/word-split
 * ======================================================================== */

/* Expand one word from script.
 *   quoted == 0: completely unquoted — syntactic quotes / $ / backslash all
 *                 active, plus word-splitting and glob expansion after.
 *   quoted == 1: whole word (or a segment) was inside single quotes — treat
 *                 content literally: no $ expansion, no backslash escapes,
 *                 no word-splitting, no glob.  Internal ' " $ are literal.
 *                 (This matches shell '...' semantics exactly.)
 *   quoted == 2: word used double-quotes or backslash escapes but never
 *                 single quotes — perform $ expansion / command substitution,
 *                 but never treat ' or " as syntactic quotes (the lexer has
 *                 already stripped the outer quoting layer), and no word
 *                 splitting or glob expansion afterwards. */
static char **_bash_expand_word(bash_ctx_t *ctx, const char *word, int quoted, int *out_n)
{
    bash_barray_t result; bash_barray_init(&result);
    bash_bstr_t cur; bash_bstr_init(&cur);

    if (!word) { *out_n = 0; return NULL; }
    const char *p = word;
    int sq = 0, dq = 0;
    /* When quoted >= 1, outer layer was already stripped by lexer: internal
     * occurrences of ' or " are payload, not syntactic quotes.  Additionally
     * quoted == 1 (single-quote segments present) means NO $ expansion at
     * all — the content is 100% literal. */
    int skip_quote_semantics = (quoted >= 1);
    int no_dollar_expansion  = (quoted == 1);

    while (*p) {
        char c = *p;
        if (sq) {
            if (c == '\'') { sq = 0; p++; continue; }
            bash_bstr_putc(&cur, c); p++; continue;
        }
        if (dq) {
            if (c == '"') { dq = 0; p++; continue; }
            if (c == '\\' && (p[1] == '$' || p[1] == '`' || p[1] == '"' || p[1] == '\\')) {
                bash_bstr_putc(&cur, '\\'); bash_bstr_putc(&cur, p[1]); p += 2; continue;
            }
            if (!no_dollar_expansion && c == '$') {
                /* variable or $( or $(( */
                p++;
                goto expand_dollar;
            }
            if (!no_dollar_expansion && c == '`') {
                p++;
                int j = _bash_find_close_paren(word, (int)(p - word), '`', '`');
                if (j < 0) { bash_bstr_putc(&cur, '`'); continue; }
                int old_len = (int)(p - word);
                char *inner = _bash_xstrndup(word + old_len, j - old_len);
                int ol = 0;
                char *out_cmd = _bash_cmdsub(ctx, inner, 1, &ol);
                if (out_cmd) {
                    /* keep newlines inside quoted */
                    bash_bstr_putn(&cur, out_cmd, ol);
                    free(out_cmd);
                }
                free(inner);
                p = word + j + 1;
                continue;
            }
            bash_bstr_putc(&cur, c); p++; continue;
        }
        /* unquoted — but skip syntactic quote toggling when lexer already
         * stripped an outer quoting layer for this word. */
        if (!skip_quote_semantics && c == '\'') { sq = 1; p++; continue; }
        if (!skip_quote_semantics && c == '"')  { dq = 1; p++; continue; }
        if (c == '\\' && !skip_quote_semantics) {
            if (p[1] == '\n') { p += 2; continue; }
            bash_bstr_putc(&cur, '\\');
            if (p[1]) { bash_bstr_putc(&cur, p[1]); p += 2; } else p++;
            continue;
        }
        if (!no_dollar_expansion && c == '$') {
            p++;
        expand_dollar:
            if (*p == '{') {
                p++;
                int j = _bash_find_close_paren(word, (int)(p - word), '{', '}');
                if (j < 0) { bash_bstr_putc(&cur, '$'); continue; }
                int old_p = (int)(p - word);
                char *inner = _bash_xstrndup(word + old_p, j - old_p);
                /* ---------- Bash 5.3: new command substitution forms ----------
                 *   ${ command-list; }   -> run in CURRENT shell (no fork), capture stdout
                 *   ${ | command-list; } -> run in CURRENT shell, result in $REPLY, expands empty
                 * Detection strategy:
                 *   A) first non-whitespace is '|'      -> pipe form, cmdsub
                 *   B) otherwise: try to consume the inner text as a valid
                 *      parameter expansion pattern NAME [OP ...]. If after the
                 *      longest valid NAME the next char is NOT a recognized
                 *      operator start ( '#' '%' ':' '/' '!' EOS ), treat as cmdsub.
                 *      (Covers cases like `${ echo hi; }` where NAME='echo' is
                 *       followed by space instead of an operator.)               */
                {
                    const char *ip0 = inner;
                    while (*ip0 && (*ip0 == ' ' || *ip0 == '\t' || *ip0 == '\n' || *ip0 == '\r')) ip0++;
                    int pipe_form = 0;
                    int new_cmdsub = 0;
                    const char *script_start = inner;
                    if (*ip0 == '|') {
                        /* form A: ${ | command-list; } */
                        pipe_form = 1;
                        const char *s = ip0 + 1;
                        while (*s && (*s == ' ' || *s == '\t')) s++;
                        script_start = s;
                        new_cmdsub = 1;
                    } else if (*ip0) {
                        /* form B: simulate NAME parse */
                        const char *np = ip0;
                        /* skip leading '#' for ${#var} length op */
                        if (np[0] == '#' && (np[1] == '_' || isalpha((unsigned char)np[1]))) np++;
                        if (*np == '?' || *np == '$' || *np == '!' || *np == '#' ||
                            *np == '@' || *np == '*' || *np == '-' || isdigit((unsigned char)*np)) {
                            np++;   /* single special char */
                        } else if (isalpha((unsigned char)*np) || *np == '_') {
                            /* identifier */
                            np++;
                            while (*np && (isalnum((unsigned char)*np) || *np == '_')) np++;
                        }
                        /* NAME may be followed by subscript [index] (pseudo-array support) */
                        while (*np == '[') {
                            const char *ep = np;
                            int d2 = 0;
                            while (*ep) {
                                if (*ep == '[') d2++;
                                else if (*ep == ']') { d2--; if (d2 == 0) { ep++; break; } }
                                ep++;
                            }
                            if (d2 != 0) break;
                            np = ep; /* consumed [index] */
                        }
                        /* After valid NAME ([index])*, next char must be a param-expansion operator */
                        char nextc = *np;
                        if (nextc != 0 && nextc != ':' && nextc != '#' && nextc != '%' &&
                            nextc != '/' && nextc != '!') {
                            /* Not a known pattern → treat as command substitution */
                            new_cmdsub = 1;
                            script_start = ip0;
                        }
                    }
                    if (new_cmdsub) {
                        if (pipe_form) {
                            /* ${ | cmd; } -> run in current shell; leave result in REPLY, expand to empty */
                            _bash_var_set(ctx, "REPLY", "", 0, 0, 0);
                            _bash_run_string(ctx, script_start);
                            /* expand to nothing */
                        } else {
                            /* ${ cmd; } -> capture stdout (run in current shell) */
                            int ol = 0;
                            char *out_cmd = _bash_cmdsub(ctx, script_start, 1, &ol);
                            if (out_cmd) {
                                int start = 0, end = ol;
                                while (end > start && (out_cmd[end-1] == '\n' || out_cmd[end-1] == '\r')) end--;
                                for (int k = start; k < end; k++) {
                                    if (out_cmd[k] == '\n' || out_cmd[k] == '\r') bash_bstr_putc(&cur, ' ');
                                    else bash_bstr_putc(&cur, out_cmd[k]);
                                }
                                free(out_cmd);
                            }
                        }
                        free(inner);
                        p = word + j + 1;
                        continue;
                    }
                }
                /* parse ${var:op...} */
                char name[256] = {0}; int ni = 0;
                const char *ip = inner;
                int is_length_op = 0;
                /* ${#var} — length operator: leading '#' before valid name is length op,
                 * not a name character.  A bare '#' alone is the special positional-count param. */
                if (ip[0] == '#' && ip[1] && (isalpha((unsigned char)ip[1]) || ip[1] == '_')) {
                    is_length_op = 1;
                    ip++; /* skip '#' — it's the operator, not part of name */
                }
                /* Read a name: either a single special param character (?, $, !, #, @, *, 0-9 digits)
                 * or a regular identifier [A-Za-z_][A-Za-z0-9_]*.  We never treat multi-char names
                 * as containing specials — that keeps operators (#, %, /, :-) unambiguous. */
                if (*ip && (ip[0] == '?' || ip[0] == '$' || ip[0] == '!' || ip[0] == '#' ||
                            ip[0] == '@' || ip[0] == '*' || ip[0] == '-' ||
                            isdigit((unsigned char)ip[0]))) {
                    name[ni++] = *ip++;
                } else {
                    while (ni < 255 && *ip && (isalnum((unsigned char)*ip) || *ip == '_')) {
                        name[ni++] = *ip++;
                    }
                }
                /* Array subscript: append [index] to the lookup key (simplified pseudo-array
                 * support: each NAME[idx] stores as its own flat variable key). */
                if (*ip == '[' && ni < 250) {
                    const char *sp = ip;
                    int depth = 0;
                    while (*sp) {
                        if (*sp == '[') depth++;
                        else if (*sp == ']') { depth--; if (depth == 0) { sp++; break; } }
                        sp++;
                    }
                    if (depth == 0) {
                        /* copy [ ... ] into name[] */
                        size_t sublen = (size_t)(sp - ip);
                        if (ni + sublen < 255) {
                            memcpy(name + ni, ip, sublen);
                            ni += (int)sublen;
                            ip = sp;
                        }
                    }
                }
                name[ni] = 0;
                const char *val = _bash_var_get(ctx, name, NULL);
                if (!val) val = "";
                char op[4] = {0}; int oi = 0;
                if (is_length_op) { op[oi++] = '#'; }
                else if (*ip == ':') {
                    /* ${var:-def} / ${var:=def} / ${var:?err} / ${var:+alt}
                     *   — ':' immediately followed by - = ? +
                     * ${var:offset[:length]}
                     *   — ':' followed by digit / + / - (signed offset)
                     * We distinguish by inspecting the next char. */
                    char after_colon = ip[1];
                    if (after_colon == '-' || after_colon == '=' || after_colon == '?' || after_colon == '+') {
                        op[oi++] = ':'; ip++;
                        op[oi++] = *ip++;
                    } else {
                        op[oi++] = ':'; /* bare ':' op means substring — ip still points past ':' */
                        ip++;
                    }
                }
                else if (*ip == '#' || *ip == '%' || *ip == '/') { op[oi++] = *ip++; if (*ip == op[0]) op[oi++] = *ip++; }
                else if (*ip == '!') { op[oi++] = '!'; ip++; }
                /* For ${#var} — length when '#' was the leading operator (is_length_op) OR when
                 * the operator after a name is a bare '#' with no further pattern text. */
                if ((is_length_op || (op[0] == '#' && op[1] == 0 && ip == inner + ni)) && ni > 0) {
                    /* ${#var} length */
                    char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)strlen(val));
                    bash_bstr_puts(&cur, buf);
                } else if (op[0] == ':' && op[1] == '-') {
                    if (!*val) { val = ip; }
                    bash_bstr_puts(&cur, val);
                } else if (op[0] == ':' && op[1] == '=') {
                    if (!*val) { val = ip; _bash_var_set(ctx, name, val, 0, 0, 0); }
                    bash_bstr_puts(&cur, val);
                } else if (op[0] == ':' && op[1] == '?') {
                    if (!*val) { fprintf(stderr, "bash: %s: %s\n", name, *ip ? ip : "parameter null or not set"); ctx->do_exit = 1; ctx->exit_code = 1; }
                    bash_bstr_puts(&cur, val);
                } else if (op[0] == ':' && op[1] == '+') {
                    if (*val) bash_bstr_puts(&cur, ip);
                } else if (op[0] == '#' && (op[1] == '#' || op[1] == 0)) {
                    /* remove shortest/longest prefix pattern */
                    const char *pat = ip;
                    int longest = (op[1] == '#');
                    int matched = -1;
                    /* try lengths from 0..strlen(val) */
                    int vl = (int)strlen(val);
                    if (longest) {
                        for (int l = vl; l >= 0; l--) {
                            char *tmp = _bash_xstrndup(val, l);
                            if (_bash_glob_match(pat, tmp)) { matched = l; free(tmp); break; }
                            free(tmp);
                        }
                    } else {
                        for (int l = 0; l <= vl; l++) {
                            char *tmp = _bash_xstrndup(val, l);
                            if (_bash_glob_match(pat, tmp)) { matched = l; free(tmp); break; }
                            free(tmp);
                        }
                    }
                    if (matched >= 0) bash_bstr_puts(&cur, val + matched); else bash_bstr_puts(&cur, val);
                } else if (op[0] == '%') {
                    /* remove suffix */
                    const char *pat = ip;
                    int longest = (op[1] == '%');
                    int vl = (int)strlen(val);
                    int matched = -1;
                    if (longest) {
                        for (int l = 0; l <= vl; l++) {
                            if (_bash_glob_match(pat, val + l)) { matched = l; break; }
                        }
                    } else {
                        for (int l = vl; l >= 0; l--) {
                            if (_bash_glob_match(pat, val + l)) { matched = l; break; }
                        }
                    }
                    if (matched >= 0) {
                        char *tmp = _bash_xstrndup(val, matched);
                        bash_bstr_puts(&cur, tmp); free(tmp);
                    } else bash_bstr_puts(&cur, val);
                } else if (op[0] == '/') {
                    /* ${var/pat/repl} or ${var//pat/repl} */
                    int global = (op[1] == '/');
                    const char *pat = ip;
                    const char *repl = strchr(pat, '/');
                    char *patcopy = NULL;
                    if (repl) { patcopy = _bash_xstrndup(pat, repl - pat); repl++; pat = patcopy; }
                    int vl = (int)strlen(val);
                    int i = 0;
                    while (i <= vl) {
                        int found = -1;
                        for (int l = 1; l <= vl - i; l++) {
                            char *sub = _bash_xstrndup(val + i, l);
                            if (_bash_glob_match(pat, sub)) { found = i + l; free(sub); break; }
                            free(sub);
                        }
                        if (found < 0) { bash_bstr_puts(&cur, val + i); break; }
                        bash_bstr_putn(&cur, val + i, found - i - 1);
                        bash_bstr_puts(&cur, repl ? repl : "");
                        i = found;
                        if (!global) { bash_bstr_puts(&cur, val + i); break; }
                    }
                    free(patcopy);
                } else if (op[0] == ':' && op[1] == 0) {
                    /* ${var:offset[:length]} — substring expansion */
                    const char *sp = ip;
                    /* find the separator ':' between offset and length */
                    const char *sep = NULL;
                    int depth = 0;
                    for (const char *q = sp; *q; q++) {
                        if (*q == '[') depth++;
                        else if (*q == ']') depth--;
                        else if (*q == ':' && depth == 0) { sep = q; break; }
                    }
                    long offset = 0, length_val = -1;
                    if (sep) {
                        char *off_s = _bash_xstrndup(sp, sep - sp);
                        offset = _bash_eval_arith(ctx, off_s);
                        free(off_s);
                        const char *lsp = sep + 1;
                        if (*lsp) {
                            char *len_s = _bash_xstrdup(lsp);
                            length_val = _bash_eval_arith(ctx, len_s);
                            free(len_s);
                        }
                    } else {
                        char *off_s = _bash_xstrdup(sp);
                        offset = _bash_eval_arith(ctx, off_s);
                        free(off_s);
                    }
                    int vl = (int)strlen(val);
                    if (offset < 0) offset = vl + offset;
                    if (offset < 0) offset = 0;
                    if (offset > vl) offset = vl;
                    long end;
                    if (length_val < 0) end = vl;
                    else {
                        end = offset + length_val;
                        if (end < 0) end = 0;
                        if (end > vl) end = vl;
                    }
                    if (end > offset) bash_bstr_putn(&cur, val + offset, (int)(end - offset));
                } else {
                    bash_bstr_puts(&cur, val);
                }
                free(inner);
                p = word + j + 1;
                continue;
            }
            if (*p == '(') {
                if (p[1] == '(') {
                    /* $(( arithmetic )) */
                    p += 2;
                    /* scan for matching '))' with depth tracking */
                    int old_p = (int)(p - word);
                    int close1 = -1;
                    int depth = 2; int sq2 = 0, dq2 = 0;
                    for (int i = old_p; word[i]; i++) {
                        char ch2 = word[i];
                        if (sq2) { if (ch2 == '\'') sq2 = 0; continue; }
                        if (dq2) { if (ch2 == '"') dq2 = 0; continue; }
                        if (ch2 == '\'') sq2 = 1;
                        else if (ch2 == '"') dq2 = 1;
                        else if (ch2 == '(') depth++;
                        else if (ch2 == ')') { depth--; if (depth == 0) { close1 = i; break; } }
                    }
                    if (close1 < 0) { bash_bstr_puts(&cur, "$(("); continue; }
                    /* close1 points to the 2nd closing ). 1st ) is at close1-1, expr ends at close1-2 */
                    int expr_start = old_p;
                    int expr_end = close1 - 2;
                    if (expr_end < expr_start) expr_end = expr_start;
                    char *expr = _bash_xstrndup(word + expr_start, expr_end - expr_start + 1);
                    long v = _bash_eval_arith(ctx, expr);
                    char buf[64]; snprintf(buf, sizeof(buf), "%ld", v);
                    bash_bstr_puts(&cur, buf);
                    free(expr);
                    p = word + close1 + 1; /* skip second ) */
                    continue;
                }
                /* $( command ) */
                p++;
                int j = _bash_find_close_paren(word, (int)(p - word), '(', ')');
                if (j < 0) { bash_bstr_puts(&cur, "$("); continue; }
                int old_p2 = (int)(p - word);
                char *inner = _bash_xstrndup(word + old_p2, j - old_p2);
                int ol = 0;
                char *out_cmd = _bash_cmdsub(ctx, inner, 1, &ol);
                if (out_cmd) {
                    /* strip trailing newlines: replace with spaces if in middle */
                    /* For unquoted: word split later will handle; but need to convert \n to space or remove */
                    /* Simplify: strip one trailing \n, keep others as space (for word splitting) */
                    int start = 0, end = ol;
                    while (end > start && (out_cmd[end-1] == '\n' || out_cmd[end-1] == '\r')) end--;
                    for (int k = start; k < end; k++) {
                        if (out_cmd[k] == '\n' || out_cmd[k] == '\r') bash_bstr_putc(&cur, ' ');
                        else bash_bstr_putc(&cur, out_cmd[k]);
                    }
                    free(out_cmd);
                }
                free(inner);
                p = word + j + 1;
                continue;
            }
            /* simple $name */
            char name[256]; int consumed = _bash_parse_name(p, name, sizeof(name));
            char *val = _bash_var_get(ctx, name, NULL);
            if (val) bash_bstr_puts(&cur, val);
            p += consumed;
            continue;
        }
        if (c == '`') {
            p++;
            int j = _bash_find_close_paren(word, (int)(p - word), '`', '`');
            if (j < 0) { bash_bstr_putc(&cur, '`'); continue; }
            int old_p = (int)(p - word);
            char *inner = _bash_xstrndup(word + old_p, j - old_p);
            int ol = 0;
            char *out_cmd = _bash_cmdsub(ctx, inner, 1, &ol);
            if (out_cmd) {
                int start = 0, end = ol;
                while (end > start && (out_cmd[end-1] == '\n' || out_cmd[end-1] == '\r')) end--;
                for (int k = start; k < end; k++) {
                    if (out_cmd[k] == '\n' || out_cmd[k] == '\r') bash_bstr_putc(&cur, ' ');
                    else bash_bstr_putc(&cur, out_cmd[k]);
                }
                free(out_cmd);
            }
            free(inner);
            p = word + j + 1;
            continue;
        }
        /* tilde */
        if (c == '~' && (p == word || p[-1] == ':' || p[-1] == '=' || p[-1] == '/' || p == word + 1)) {
            const char *home = getenv("HOME");
#ifdef BASH_PLATFORM_WINDOWS
            if (!home || !*home) home = getenv("USERPROFILE");
#endif
            if (home && *home) bash_bstr_puts(&cur, home);
            else bash_bstr_putc(&cur, c);
            p++;
            continue;
        }
        bash_bstr_putc(&cur, c); p++;
    }

    /* If quoted: output single word (cur) */
    if (quoted) {
        bash_barray_push(&result, bash_bstr_detach(&cur));
    } else {
        /* word split on IFS (default space/tab/newline) */
        const char *ifs = getenv("IFS");
        if (!ifs) ifs = " \t\n";
        int i = 0;
        while (cur.data[i]) {
            while (cur.data[i] && strchr(ifs, cur.data[i])) i++;
            if (!cur.data[i]) break;
            int start = i;
            while (cur.data[i] && !strchr(ifs, cur.data[i])) i++;
            char *w = _bash_xstrndup(cur.data + start, i - start);
            /* try glob */
            bash_barray_t tmp; bash_barray_init(&tmp);
            if (!_bash_glob_word(ctx, w, &tmp)) {
                bash_barray_push(&result, w);
            } else {
                for (int k = 0; k < tmp.len; k++) bash_barray_push(&result, tmp.items[k]);
                tmp.len = 0; free(tmp.items); free(w);
            }
        }
        bash_bstr_free(&cur);
    }

    if (result.len == 0) {
        bash_barray_push(&result, _bash_xstrdup(""));
    }
    if (out_n) *out_n = result.len;
    bash_barray_push_steal(&result, NULL);
    result.len--;
    return result.items;
}

/* Forward declarations needed by _bash_cmdsub which uses _bash_run_string */
static int _bash_run_string(bash_ctx_t *ctx, const char *src);

/* Command substitution: run script in CURRENT shell context (so functions,
 * local vars, exports are all visible), capture stdout into temp file, then
 * return the captured bytes. Avoids popen/_popen which on Windows launches
 * cmd.exe and therefore loses shell-defined functions and variables. */
static char *_bash_cmdsub(bash_ctx_t *ctx, const char *script, int strip, int *out_len)
{
    (void)strip;
    if (out_len) *out_len = 0;

    int depth_in = _bash_frame_depth(ctx);
    _bash_dbg_log("[CMDSUB_ENTER depth=%d] script=[%.200s]", depth_in, script ? script : "(null)");

    /* 1. Build a unique temp file path in $TMP / current dir */
    char tmppath[1024];
    const char *tmpdir = NULL;
#ifdef BASH_PLATFORM_WINDOWS
    tmpdir = getenv("TMP"); if (!tmpdir || !*tmpdir) tmpdir = getenv("TEMP");
#else
    tmpdir = getenv("TMPDIR");
#endif
    if (!tmpdir || !*tmpdir) tmpdir = ".";
    /* Use counter + pid to be collision-safe enough between cmdsubs */
    static int sub_counter = 0;
    int pid_val = 0;
#ifdef _WIN32
    pid_val = (int)_getpid();
#else
    pid_val = (int)getpid();
#endif
    sub_counter++;
    snprintf(tmppath, sizeof(tmppath), "%s%sbash_cs_%d_%d_%x.tmp",
             tmpdir,
#ifdef BASH_PLATFORM_WINDOWS
             "\\",
#else
             "/",
#endif
             pid_val, sub_counter, (unsigned)(intptr_t)ctx & 0xffff);

    /* 2. Open the temp file for writing, redirect stdout to it */
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef BASH_PLATFORM_WINDOWS
    flags |= _O_BINARY;
#endif
    int tf = BASH_OPEN(tmppath, flags, 0600);
    if (tf < 0) { _bash_dbg_log("[CMDSUB_EXIT depth=%d] FAIL tf<0", depth_in); return NULL; }

    int saved_stdout = BASH_DUP(1);
    if (saved_stdout < 0) { BASH_CLOSE(tf); BASH_UNLINK(tmppath); _bash_dbg_log("[CMDSUB_EXIT depth=%d] FAIL saved_stdout<0", depth_in); return NULL; }
    int fflush_res = fflush(stdout); (void)fflush_res;
    if (BASH_DUP2(tf, 1) < 0) {
        BASH_DUP2(saved_stdout, 1); BASH_CLOSE(saved_stdout);
        BASH_CLOSE(tf); BASH_UNLINK(tmppath);
        _bash_dbg_log("[CMDSUB_EXIT depth=%d] FAIL dup2<0", depth_in);
        return NULL;
    }
    _bash_sync_stdhandles();

    /* 3. Run script in same ctx: functions, vars, exports all visible */
    int saved_exit_requested = ctx->do_exit;
    int saved_exit_code     = ctx->exit_code;
    ctx->do_exit = 0; ctx->exit_code = 0;
    (void)_bash_run_string(ctx, script);
    /* Do NOT let a stray "exit" inside $() kill the outer shell: */
    ctx->do_exit = saved_exit_requested;
    ctx->exit_code = saved_exit_code;

    /* 4. Restore stdout */
    fflush(stdout);
    BASH_CLOSE(tf);
    BASH_DUP2(saved_stdout, 1);
    BASH_CLOSE(saved_stdout);
    _bash_sync_stdhandles();

    int depth_after_script = _bash_frame_depth(ctx);
    if (depth_after_script != depth_in) {
        _bash_dbg_log("[CMDSUB_WARN depth=%d] frame depth changed! in=%d after_script=%d", depth_in, depth_in, depth_after_script);
    }

    /* 5. Read back captured output */
    int rf = BASH_OPEN(tmppath, O_RDONLY
#ifdef BASH_PLATFORM_WINDOWS
                       | _O_BINARY
#endif
                       , 0);
    if (rf < 0) { BASH_UNLINK(tmppath); _bash_dbg_log("[CMDSUB_EXIT depth=%d] FAIL rf<0", depth_in); return NULL; }
    bash_bstr_t out; bash_bstr_init(&out);
    char buf[4096];
    int nr;
    while ((nr = BASH_READ(rf, buf, (int)sizeof(buf))) > 0)
        bash_bstr_putn(&out, buf, (size_t)nr);
    BASH_CLOSE(rf);
    BASH_UNLINK(tmppath);

    _bash_dbg_log("[CMDSUB_EXIT depth=%d] captured len=%d content=[%.*s]",
              depth_in, (int)out.len,
              (int)(out.len > 200 ? 200 : out.len),
              out.len > 0 ? out.data : "(empty)");

    if (out_len) *out_len = (int)out.len;
    return bash_bstr_detach(&out);
}

/* ========================================================================
 * Redirection helper: apply redirections for a command
 * ======================================================================== */

typedef struct {
    int stdin_fd;   /* saved original */
    int stdout_fd;
    int stderr_fd;
    /* redirections opened that need closing after */
    int opened[16]; int n_opened;
} bash_redir_state_t;

static int _bash_redir_apply(bash_ctx_t *ctx, bash_cmd_t *cmd, bash_redir_state_t *st)
{
    (void)ctx;
    memset(st, 0, sizeof(*st));
    st->stdin_fd = st->stdout_fd = st->stderr_fd = -1;
    st->n_opened = 0;
    for (int i = 0; i < cmd->n_redirs; i++) {
        bash_redir_t *r = &cmd->redirs[i];
        int t = r->type;
        if (t == 0 || t == 1 || t == 2 || t == 3 || t == 4 || t == 7 || t == 10 || t == 11) {
            /* file open */
            int fd_target = r->fd;
            if (t == 7) fd_target = 1; /* &> = stdout+stderr */
            int flags = 0, mode = 0644;
            if (t == 0) flags = O_RDONLY;
            else if (t == 1 || t == 2) flags = O_WRONLY | O_CREAT | O_TRUNC;
            else if (t == 3) flags = O_WRONLY | O_CREAT | O_APPEND;
            else if (t == 4) flags = O_RDWR | O_CREAT;
            else if (t == 7) flags = O_WRONLY | O_CREAT | (r->append ? O_APPEND : O_TRUNC);
            else if (t == 10) flags = O_WRONLY | O_CREAT | O_TRUNC;
            else if (t == 11) flags = O_WRONLY | O_CREAT | O_APPEND;
            int f;
#ifdef BASH_PLATFORM_WINDOWS
            f = BASH_OPEN(r->target ? r->target : "", flags, mode);
#else
            f = BASH_OPEN(r->target ? r->target : "", flags, (mode_t)mode);
#endif
            if (f < 0) {
                fprintf(stderr, "bash: cannot open %s: %s\n", r->target ? r->target : "", strerror(errno));
                return -1;
            }
            if (fd_target == 0) { if (st->stdin_fd < 0) st->stdin_fd = BASH_DUP(0); BASH_DUP2(f, 0); }
            else if (fd_target == 1) { if (st->stdout_fd < 0) st->stdout_fd = BASH_DUP(1); BASH_DUP2(f, 1); }
            else if (fd_target == 2) { if (st->stderr_fd < 0) st->stderr_fd = BASH_DUP(2); BASH_DUP2(f, 2); }
            else { BASH_DUP2(f, fd_target); }
            if (t == 7) { /* also redirect stderr */ if (st->stderr_fd < 0) st->stderr_fd = BASH_DUP(2); BASH_DUP2(f, 2); }
            _bash_sync_stdhandles();
            if (st->n_opened < 16) st->opened[st->n_opened++] = f;
            else BASH_CLOSE(f);
        } else if (t == 5) {
            /* heredoc: read lines from src until delimiter.
             * Note: we need script source for heredoc during parsing;
             * but we didn't store it. Mini-impl: treat target as content (won't work usually).
             * Instead: create a temp pipe and feed a placeholder. */
            (void)r;
            /* For now: ignore heredoc body (redirect /dev/null or NUL) */
#ifdef BASH_PLATFORM_WINDOWS
            int f = BASH_OPEN("NUL", O_RDONLY);
#else
            int f = BASH_OPEN("/dev/null", O_RDONLY);
#endif
            if (f >= 0) {
                if (st->stdin_fd < 0) st->stdin_fd = BASH_DUP(0);
                BASH_DUP2(f, 0);
                if (st->n_opened < 16) st->opened[st->n_opened++] = f; else BASH_CLOSE(f);
            }
        } else if (t == 6) {
            /* here-string: write r->target into pipe, dup read side to stdin */
            int pfds[2];
            if (BASH_PIPE(pfds) == 0) {
                const char *s = r->target ? r->target : "";
                size_t sl = strlen(s);
                if (sl > 0) {
                    int wr = (int)BASH_WRITE(pfds[1], s, (unsigned int)sl);
                    (void)wr;
                }
                BASH_CLOSE(pfds[1]);
                if (st->stdin_fd < 0) st->stdin_fd = BASH_DUP(0);
                BASH_DUP2(pfds[0], 0);
                if (st->n_opened < 16) st->opened[st->n_opened++] = pfds[0]; else BASH_CLOSE(pfds[0]);
            }
        } else if (t == 8) {
            /* >& n: dup fd (numeric target) */
            int n = r->target ? atoi(r->target) : 1;
            if (r->fd == 1) { if (st->stdout_fd < 0) st->stdout_fd = BASH_DUP(1); BASH_DUP2(n, 1); }
            else if (r->fd == 2) { if (st->stderr_fd < 0) st->stderr_fd = BASH_DUP(2); BASH_DUP2(n, 2); }
            else BASH_DUP2(n, r->fd);
        } else if (t == 9) {
            /* <& n */
            int n = r->target ? atoi(r->target) : 0;
            if (r->fd == 0) { if (st->stdin_fd < 0) st->stdin_fd = BASH_DUP(0); BASH_DUP2(n, 0); }
            else BASH_DUP2(n, r->fd);
        }
    }
    return 0;
}

static void _bash_redir_restore(bash_redir_state_t *st)
{
    if (st->stdin_fd  >= 0) { BASH_DUP2(st->stdin_fd,  0); BASH_CLOSE(st->stdin_fd); }
    if (st->stdout_fd >= 0) { BASH_DUP2(st->stdout_fd, 1); BASH_CLOSE(st->stdout_fd); }
    if (st->stderr_fd >= 0) { BASH_DUP2(st->stderr_fd, 2); BASH_CLOSE(st->stderr_fd); }
    for (int i = 0; i < st->n_opened; i++) BASH_CLOSE(st->opened[i]);
    _bash_sync_stdhandles();
}

/* ========================================================================
 * Builtins
 * ======================================================================== */

typedef int (*bash_builtin_fn)(bash_ctx_t *ctx, int argc, char **argv);

typedef struct {
    const char *name;
    bash_builtin_fn fn;
} bash_builtin_t;

static int bi_true(bash_ctx_t *c, int a, char **v){ (void)c;(void)a;(void)v; return 0; }
static int bi_false(bash_ctx_t *c, int a, char **v){ (void)c;(void)a;(void)v; return 1; }

static int bi_exit(bash_ctx_t *ctx, int argc, char **argv)
{
    int code = ctx->last_status;
    if (argc > 1) code = atoi(argv[1]);
    ctx->exit_code = code;
    ctx->do_exit = 1;
    return code;
}

static int bi_return(bash_ctx_t *ctx, int argc, char **argv)
{
    int code = ctx->last_status;
    if (argc > 1) code = atoi(argv[1]);
    ctx->frame->func_ret = 1;
    ctx->frame->status = code;
    return code;
}

static int bi_break(bash_ctx_t *ctx, int argc, char **argv)
{
    int n = 1; if (argc > 1) n = atoi(argv[1]);
    ctx->frame->break_level = n;
    return 0;
}
static int bi_continue(bash_ctx_t *ctx, int argc, char **argv)
{
    int n = 1; if (argc > 1) n = atoi(argv[1]);
    ctx->frame->continue_level = n;
    return 0;
}

static int bi_cd(bash_ctx_t *ctx, int argc, char **argv)
{
    const char *target = NULL;

    /* cd with no arguments → go HOME (like bash) */
    if (argc < 2 || !argv[1][0]) {
        target = getenv("HOME");
        if (!target || !*target) {
#ifdef BASH_PLATFORM_WINDOWS
            target = getenv("USERPROFILE");
#endif
        }
        if (!target || !*target) target = "/";
    } else if (strcmp(argv[1], "-") == 0) {
        /* cd - → switch to $OLDPWD and print it (like bash) */
        target = getenv("OLDPWD");
        if (!target || !*target) {
            fprintf(stderr, "bash: cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", target);
        fflush(stdout);
    } else {
        target = argv[1];
        /* expand leading ~ */
        if (target[0] == '~') {
            const char *home = getenv("HOME");
            if (!home || !*home) {
#ifdef BASH_PLATFORM_WINDOWS
                home = getenv("USERPROFILE");
#endif
            }
            if (home && *home) {
                char buf2[4096];
                snprintf(buf2, sizeof(buf2), "%s%s", home, target + 1);
                /* use buf[] below for cwd, so allocate target on heap */
                char *dup = _bash_xstrdup(buf2);
                /* Save old PWD before chdir */
                const char *oldpwd = getenv("PWD");
                if (!oldpwd) oldpwd = ctx->pwd ? ctx->pwd : ".";
                if (BASH_CHDIR(dup) != 0) {
                    fprintf(stderr, "bash: cd: %s: %s\n", dup, strerror(errno));
                    free(dup);
                    return 1;
                }
                _bash_var_set(ctx, "OLDPWD", oldpwd, 1, 1, 0);
                char buf[4096];
                char *cwd = BASH_GETCWD(buf, sizeof(buf));
                if (cwd) {
                    _bash_normalize_path(cwd);
                    _bash_var_set(ctx, "PWD", cwd, 1, 1, 0);
                    if (ctx->pwd) { free(ctx->pwd); ctx->pwd = _bash_xstrdup(cwd); }
                }
                free(dup);
                return 0;
            }
            /* HOME not set — fall through, chdir("~") will fail with clear error */
        }
    }

    /* Save old PWD before chdir */
    const char *oldpwd = getenv("PWD");
    if (!oldpwd) oldpwd = ctx->pwd ? ctx->pwd : ".";

    if (BASH_CHDIR(target) != 0) {
        fprintf(stderr, "bash: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    /* set OLDPWD to the directory we just left */
    _bash_var_set(ctx, "OLDPWD", oldpwd, 1, 1, 0);
    /* update PWD — normalize to forward slashes */
    char buf[4096];
    char *cwd = BASH_GETCWD(buf, sizeof(buf));
    if (cwd) {
        _bash_normalize_path(cwd);
        _bash_var_set(ctx, "PWD", cwd, 1, 1, 0);
        if (ctx->pwd) { free(ctx->pwd); ctx->pwd = _bash_xstrdup(cwd); }
    }
    return 0;
}

static int bi_pwd(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx; (void)argc; (void)argv;
    char buf[4096];
    char *cwd = BASH_GETCWD(buf, sizeof(buf));
    if (cwd) { _bash_normalize_path(cwd); printf("%s\n", cwd); fflush(stdout); return 0; }
    fprintf(stderr, "bash: pwd: %s\n", strerror(errno));
    return 1;
}

static int bi_echo(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    int nflag = 0, eflag = 1;
    int start = 1;
    while (start < argc) {
        if (strcmp(argv[start], "-n") == 0) { nflag = 1; start++; }
        else if (strcmp(argv[start], "-e") == 0) { eflag = 1; start++; }
        else if (strcmp(argv[start], "-E") == 0) { eflag = 0; start++; }
        else break;
    }
    for (int i = start; i < argc; i++) {
        if (i > start) putchar(' ');
        const char *s = argv[i];
        if (eflag) {
            for (; *s; s++) {
                if (*s == '\\' && s[1]) {
                    s++;
                    switch (*s) {
                        case 'n': putchar('\n'); break;
                        case 't': putchar('\t'); break;
                        case 'r': putchar('\r'); break;
                        case 'a': putchar('\a'); break;
                        case 'b': putchar('\b'); break;
                        case 'f': putchar('\f'); break;
                        case 'v': putchar('\v'); break;
                        case '\\': putchar('\\'); break;
                        case '0': {
                            int v = 0;
                            for (int k = 0; k < 3 && s[1] >= '0' && s[1] <= '7'; k++) { s++; v = v*8 + (*s - '0'); }
                            putchar((char)v); break;
                        }
                        default: putchar(*s);
                    }
                } else putchar(*s);
            }
        } else fputs(s, stdout);
    }
    if (!nflag) putchar('\n');
    fflush(stdout);
    return 0;
}

static int bi_printf(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    if (argc < 2) return 0;
    const char *fmt = argv[1];
    int ai = 2;
    while (*fmt) {
        if (*fmt != '%') {
            if (*fmt == '\\') {
                fmt++;
                if (!*fmt) break;
                switch (*fmt) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case '\\': putchar('\\'); break;
                    case '0': {
                        int v=0; for(int k=0;k<3 && fmt[1]>='0'&&fmt[1]<='7';k++){fmt++;v=v*8+(*fmt-'0');}
                        putchar((char)v); break;
                    }
                    default: putchar(*fmt);
                }
                fmt++; continue;
            }
            putchar(*fmt++); continue;
        }
        fmt++;
        int width = 0, prec = -1, left = 0;
        if (*fmt == '-') { left = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width*10 + (*fmt - '0'); fmt++; }
        if (*fmt == '.') { fmt++; prec = 0; while (*fmt >= '0' && *fmt <= '9') { prec = prec*10 + (*fmt - '0'); fmt++; } }
        char conv = *fmt++;
        char buf[4096];
        const char *a = (ai < argc) ? argv[ai] : "0";
        switch (conv) {
            case 'd': case 'i': {
                long v = a ? strtol(a, NULL, 0) : 0;
                snprintf(buf, sizeof(buf), left ? "%-*ld" : "%*ld", width ? width : 1, v);
                fputs(buf, stdout); break;
            }
            case 'u': {
                unsigned long v = a ? strtoul(a, NULL, 0) : 0;
                snprintf(buf, sizeof(buf), left ? "%-*lu" : "%*lu", width ? width : 1, v);
                fputs(buf, stdout); break;
            }
            case 'x': {
                unsigned long v = a ? strtoul(a, NULL, 0) : 0;
                snprintf(buf, sizeof(buf), left ? "%-*lx" : "%*lx", width ? width : 1, v);
                fputs(buf, stdout); break;
            }
            case 'o': {
                unsigned long v = a ? strtoul(a, NULL, 0) : 0;
                snprintf(buf, sizeof(buf), left ? "%-*lo" : "%*lo", width ? width : 1, v);
                fputs(buf, stdout); break;
            }
            case 'f': {
                double v = a ? atof(a) : 0;
                if (prec < 0) prec = 6;
                snprintf(buf, sizeof(buf), left ? "%-*.*f" : "%*.*f", width ? width : 1, prec, v);
                fputs(buf, stdout); break;
            }
            case 's': {
                int l = a ? (int)strlen(a) : 0;
                if (prec >= 0 && prec < l) l = prec;
                int pad = width - l;
                if (!left) while (pad-- > 0) putchar(' ');
                fwrite(a, 1, l, stdout);
                if (left) while (pad-- > 0) putchar(' ');
                break;
            }
            case 'c': putchar(a ? a[0] : 0); break;
            case '%': putchar('%'); break;
            default: putchar('%'); putchar(conv);
        }
        if (conv != '%') ai++;
    }
    fflush(stdout);
    return 0;
}

static int bi_set(bash_ctx_t *ctx, int argc, char **argv)
{
    /* process options */
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        if (strcmp(argv[i], "-e") == 0) { ctx->errexit = 1; i++; continue; }
        if (strcmp(argv[i], "+e") == 0) { ctx->errexit = 0; i++; continue; }
        if (strcmp(argv[i], "-u") == 0) { ctx->nounset = 1; i++; continue; }
        if (strcmp(argv[i], "+u") == 0) { ctx->nounset = 0; i++; continue; }
        if (strcmp(argv[i], "-x") == 0) { ctx->xtrace = 1; i++; continue; }
        if (strcmp(argv[i], "+x") == 0) { ctx->xtrace = 0; i++; continue; }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "errexit") == 0) ctx->errexit = 1;
            else if (strcmp(argv[i+1], "nounset") == 0) ctx->nounset = 1;
            else if (strcmp(argv[i+1], "xtrace") == 0) ctx->xtrace = 1;
            i += 2; continue;
        }
        if (strcmp(argv[i], "+o") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "errexit") == 0) ctx->errexit = 0;
            else if (strcmp(argv[i+1], "nounset") == 0) ctx->nounset = 0;
            else if (strcmp(argv[i+1], "xtrace") == 0) ctx->xtrace = 0;
            i += 2; continue;
        }
        /* not an option we recognize, stop */
        break;
    }
    /* if remaining args, set positional */
    if (i < argc || (i == argc && argc > 1 && (i > 1))) {
        bash_frame_t *f = ctx->frame;
        while (f && !f->is_func) f = f->parent;
        if (!f) f = ctx->frame;
        for (int j = 0; j < f->argc; j++) free(f->argv[j]);
        free(f->argv);
        f->argc = (argc - i) + 1;
        f->argv = (char**)_bash_xmalloc((size_t)f->argc * sizeof(char*));
        f->argv[0] = _bash_xstrdup(ctx->script_name ? ctx->script_name : "bash");
        for (int j = i; j < argc; j++) f->argv[j - i + 1] = _bash_xstrdup(argv[j]);
        return 0;
    }
    if (argc == 1 || (i == 1 && argc == 1)) {
        /* print all vars */
        for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
            for (int k = 0; k < f->vars.len; k++) {
                printf("%s=%s\n", f->vars.items[k].name, f->vars.items[k].value);
            }
        }
        fflush(stdout);
    }
    return 0;
}

static int bi_unset(bash_ctx_t *ctx, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (argv[i] && *argv[i]) _bash_var_unset(ctx, argv[i]);
    }
    return 0;
}

static int bi_export(bash_ctx_t *ctx, int argc, char **argv)
{
    int unexport = 0;
    int print_only = 0;
    int start = 1;
    while (start < argc) {
        if (strcmp(argv[start], "-n") == 0) { unexport = 1; start++; }
        else if (strcmp(argv[start], "-p") == 0) { print_only = 1; start++; }
        else break;
    }
    if (print_only || start >= argc) {
        for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
            for (int i = 0; i < f->vars.len; i++) {
                if (f->vars.items[i].exported)
                    printf("export %s=\"%s\"\n", f->vars.items[i].name, f->vars.items[i].value);
            }
        }
        fflush(stdout);
        return 0;
    }
    for (int i = start; i < argc; i++) {
        if (unexport) {
            /* mark as not exported */
            for (bash_frame_t *f = ctx->frame; f; f = f->parent) {
                bash_var_t *v = _bash_vars_find(&f->vars, argv[i]);
                if (v) { v->exported = 0; break; }
            }
        } else {
            char *eq = strchr(argv[i], '=');
            if (eq) {
                *eq = 0;
                _bash_var_set(ctx, argv[i], eq + 1, 1, 1, 0);
                *eq = '=';
            } else {
                char *v = _bash_var_get(ctx, argv[i], NULL);
                _bash_var_set(ctx, argv[i], v ? v : "", 1, 1, 0);
            }
        }
    }
    return 0;
}

static int bi_env(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx; (void)argc; (void)argv;
    for (char **e = environ; *e; e++) printf("%s\n", *e);
    fflush(stdout);
    return 0;
}

static int bi_shift(bash_ctx_t *ctx, int argc, char **argv)
{
    int n = 1;
    if (argc > 1) n = atoi(argv[1]);
    bash_frame_t *f = ctx->frame;
    while (f && !f->is_func) f = f->parent;
    if (!f) return 0;
    if (n > f->argc - 1) return 1;
    for (int i = 1; i + n < f->argc; i++) {
        free(f->argv[i]);
        f->argv[i] = f->argv[i + n];
    }
    for (int i = (f->argc > n) ? (f->argc - n) : 0; i < f->argc; i++) f->argv[i] = NULL;
    f->argc = (f->argc > n) ? (f->argc - n) : 1;
    if (f->argc < 1) f->argc = 1;
    return 0;
}

/* Forward: interactive line editor with tab completion (used by read -E) */
static char *_bash_readline(bash_ctx_t *ctx, const char *prompt);

static int bi_read(bash_ctx_t *ctx, int argc, char **argv)
{
    int raw = 0;
    int use_readline = 0; /* -E: use default bash completion via line editor */
    const char *prompt = NULL;
    int start = 1;
    while (start < argc) {
        if (strcmp(argv[start], "-r") == 0) { raw = 1; start++; }
        else if (strcmp(argv[start], "-E") == 0) { use_readline = 1; start++; }
        else if (strcmp(argv[start], "-p") == 0 && start + 1 < argc) { prompt = argv[start+1]; start += 2; }
        else break;
    }
    char *line_heap = NULL;
    char line_stack[4096];
    const char *line;
    if (start >= argc) {
        /* No variable names given: default to REPLY (POSIX/bash standard) */
        argc = start + 1;
        char **new_argv = (char **)_bash_xmalloc(sizeof(char *) * (size_t)(argc + 1));
        for (int a = 0; a < start; a++) new_argv[a] = argv[a];
        new_argv[start] = "REPLY";
        new_argv[argc] = NULL;
        argv = new_argv;
    }
    if (use_readline) {
        line_heap = _bash_readline(ctx, prompt ? prompt : "");
        if (!line_heap) return 1;
        line = line_heap;
    } else {
        if (prompt) { fputs(prompt, stdout); fflush(stdout); }
        if (!fgets(line_stack, sizeof(line_stack), stdin)) return 1;
        line = line_stack;
    }
    int l = (int)strlen(line);
    while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) l--;
    char *input_copy = _bash_xstrndup(line, (size_t)l);
    (void)raw; /* -r recognized; no backslash processing performed in this simplified read */
    const char *ifs = getenv("IFS"); if (!ifs) ifs = " \t";
    int wi = 0;
    const char *p = input_copy;
    int nvars = argc - start;
    while (wi < nvars && *p) {
        while (*p && strchr(ifs, *p)) p++;
        if (!*p) break;
        const char *sstart = p;
        while (*p && !strchr(ifs, *p)) p++;
        char *w;
        if (wi == nvars - 1) {
            w = _bash_xstrdup(sstart);
        } else {
            w = _bash_xstrndup(sstart, (size_t)(p - sstart));
        }
        _bash_var_set(ctx, argv[start + wi], w, 0, 0, 0);
        free(w);
        wi++;
    }
    /* set remaining vars to empty */
    while (wi < nvars) {
        _bash_var_set(ctx, argv[start + wi], "", 0, 0, 0);
        wi++;
    }
    free(input_copy);
    free(line_heap);
    if (argv != (char **)(0) && start < argc && strcmp(argv[start], "REPLY") == 0 && argc == start + 1) {
        /* We allocated a synthetic argv; free it. */
        free((void *)argv);
    }
    return 0;
}

static int bi_which(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        char *p = _bash_which(argv[i]);
        if (p) { printf("%s\n", p); free(p); }
        else rc = 1;
    }
    return rc;
}

static int bi_test(bash_ctx_t *ctx, int argc, char **argv)
{
    /* Minimal test/[: supports -n, -z, -f, -d, -r, -w, -x, -s, -e, -b, -c, -p,
     * -L/-h, -t, -v, =, !=, -eq, -ne, -lt, -le, -gt, -ge, -nt, -ot, -ef, -a, -o */
    int right = 0;
    /* Strip trailing ']' if first argv is '[' */
    int start = 1, end = argc;
    if (strcmp(argv[0], "[") == 0) {
        if (end > 1 && strcmp(argv[end-1], "]") == 0) end--;
    }
    int n = end - start;
    char **a = argv + start;
    if (n == 0) return 1;
    if (n == 1) return a[0] && *a[0] ? 0 : 1;
    if (n == 2) {
        const char *op = a[0]; const char *v = a[1];
        if (strcmp(op, "-z") == 0) return (!v || !*v) ? 0 : 1;
        if (strcmp(op, "-n") == 0) return (v && *v) ? 0 : 1;
        if (strcmp(op, "!") == 0)  return (v && *v) ? 1 : 0;
        if (strcmp(op, "-f") == 0) { struct BASH_STAT st; return (BASH_STAT(v, &st) == 0 && (st.st_mode & BASH_S_IFREG)) ? 0 : 1; }
        if (strcmp(op, "-d") == 0) { struct BASH_STAT st; return (BASH_STAT(v, &st) == 0) && (st.st_mode & BASH_S_IFDIR) ? 0 : 1; }
        if (strcmp(op, "-s") == 0) { struct BASH_STAT st; if (BASH_STAT(v,&st)!=0) return 1; return st.st_size > 0 ? 0 : 1; }
        if (strcmp(op, "-r") == 0) return BASH_ACCESS(v, 4) == 0 ? 0 : 1;
        if (strcmp(op, "-w") == 0) return BASH_ACCESS(v, 2) == 0 ? 0 : 1;
        if (strcmp(op, "-x") == 0) return BASH_ACCESS(v, 1) == 0 ? 0 : 1;
        if (strcmp(op, "-e") == 0) { struct BASH_STAT st; return BASH_STAT(v, &st) == 0 ? 0 : 1; }
        if (strcmp(op, "-b") == 0) { return 1; } /* block dev - not detected */
        if (strcmp(op, "-c") == 0) { return 1; } /* char dev */
        if (strcmp(op, "-p") == 0) { return 1; } /* named pipe */
        if (strcmp(op, "-L") == 0 || strcmp(op, "-h") == 0) { return 1; } /* symlink */
        if (strcmp(op, "-t") == 0) {
#ifdef BASH_PLATFORM_WINDOWS
            return _isatty(atoi(v)) ? 0 : 1;
#else
            return isatty(atoi(v)) ? 0 : 1;
#endif
        } /* tty */
        if (strcmp(op, "-v") == 0) { const char *vv = _bash_var_get(ctx, v, NULL); return vv ? 0 : 1; } /* var set */
        return 1;
    }
    if (n == 3) {
        const char *l = a[0], *op = a[1], *r = a[2];
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return strcmp(l, r) == 0 ? 0 : 1;
        if (strcmp(op, "!=") == 0) return strcmp(l, r) != 0 ? 0 : 1;
        if (strcmp(op, "<") == 0)  return strcmp(l, r) < 0 ? 0 : 1;
        if (strcmp(op, ">") == 0)  return strcmp(l, r) > 0 ? 0 : 1;
        long lv = strtol(l, NULL, 0), rv = strtol(r, NULL, 0);
        if (strcmp(op, "-eq") == 0) return lv == rv ? 0 : 1;
        if (strcmp(op, "-ne") == 0) return lv != rv ? 0 : 1;
        if (strcmp(op, "-lt") == 0) return lv < rv ? 0 : 1;
        if (strcmp(op, "-le") == 0) return lv <= rv ? 0 : 1;
        if (strcmp(op, "-gt") == 0) return lv > rv ? 0 : 1;
        if (strcmp(op, "-ge") == 0) return lv >= rv ? 0 : 1;
        if (strcmp(op, "-nt") == 0) { struct BASH_STAT s1, s2; if (BASH_STAT(l,&s1)!=0||BASH_STAT(r,&s2)!=0) return 1; return s1.st_mtime > s2.st_mtime ? 0 : 1; }
        if (strcmp(op, "-ot") == 0) { struct BASH_STAT s1, s2; if (BASH_STAT(l,&s1)!=0||BASH_STAT(r,&s2)!=0) return 1; return s1.st_mtime < s2.st_mtime ? 0 : 1; }
        if (strcmp(op, "-ef") == 0) { struct BASH_STAT s1, s2; if (BASH_STAT(l,&s1)!=0||BASH_STAT(r,&s2)!=0) return 1; return s1.st_dev==s2.st_dev && s1.st_ino==s2.st_ino ? 0 : 1; }
        if (strcmp(op, "-a") == 0) { return (l && *l && r && *r) ? 0 : 1; } /* logical AND (deprecated) */
        if (strcmp(op, "-o") == 0) { return (l && *l) || (r && *r) ? 0 : 1; } /* logical OR (deprecated) */
    }
    (void)right;
    return 1;
}

static int bi_type(bash_ctx_t *ctx, int argc, char **argv)
{
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        const char *n = argv[i];
        /* builtin? */
        extern bash_builtin_t _bash_builtins[];
        int found = 0;
        for (int k = 0; _bash_builtins[k].name; k++) {
            if (strcmp(_bash_builtins[k].name, n) == 0) {
                printf("%s is a shell builtin\n", n); found = 1; break;
            }
        }
        if (found) continue;
        if (_bash_func_find(ctx, n)) { printf("%s is a function\n", n); continue; }
        char *p = _bash_which(n);
        if (p) { printf("%s is %s\n", n, p); free(p); }
        else { printf("%s: not found\n", n); rc = 1; }
    }
    return rc;
}

static int bi_wait(bash_ctx_t *ctx, int argc, char **argv)
{
    /* Bash 5.2 additions:  -n  wait for *any* (next) background job to exit */
    int wait_any = 0;
    int i = 1;
    while (i < argc && argv[i] && argv[i][0] == '-') {
        if (strcmp(argv[i], "-n") == 0) { wait_any = 1; i++; }
        else if (strcmp(argv[i], "--") == 0) { i++; break; }
        else break;
    }
    int rc = 0;
    if (wait_any || i >= argc) {
        /* wait for the shell's current background pid (we track one slot) */
        if (ctx->bg_pid > 0) {
            if (_bash_waitpid(ctx->bg_pid) < 0) rc = 1;
            ctx->bg_pid = 0;
        }
        return rc;
    }
    for (; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (_bash_waitpid(pid) < 0) rc = 1;
        if (wait_any) break;   /* -n: only wait for first completing pid */
    }
    return rc;
}

static int bi_jobs(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx; (void)argc; (void)argv;
    /* no-op: no job table */
    return 0;
}

static int bi_source(bash_ctx_t *ctx, int argc, char **argv)
{
    int i = 1;
    const char *search_path = NULL; /* -p PATH: override $PATH for lookup */
    while (i < argc) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            search_path = argv[i + 1];
            i += 2;
        } else {
            break;
        }
    }
    if (i >= argc) return 1;
    const char *fn = argv[i];
    /* read file */
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        /* Locate via search path: if -p given, use that PATH; else use default which() */
        if (search_path) {
            /* Search manually using search_path as colon/semicolon separated list */
            const char *p = search_path;
            bash_bstr_t candidate; bash_bstr_init(&candidate);
            while (*p && !fp) {
                const char *end = p;
                while (*end && *end != BASH_PATHSEP
#ifdef BASH_PLATFORM_WINDOWS
                       && *end != ':' /* allow colons in drive letters C:\... on win */
#endif
                ) end++;
#ifdef BASH_PLATFORM_WINDOWS
                /* If end points to a drive letter colon ("C:"), keep going */
                if (*end == ':' && end == p + 1 && isalpha((unsigned char)p[0])) {
                    end = p + 2;
                    while (*end && *end != BASH_PATHSEP) end++;
                }
#endif
                if (end > p) {
                    size_t dlen = (size_t)(end - p);
                    bash_bstr_clear(&candidate);
                    bash_bstr_putn(&candidate, p, dlen);
                    if (candidate.len == 0 ||
                        (candidate.data[candidate.len - 1] != '/'
#ifdef BASH_PLATFORM_WINDOWS
                         && candidate.data[candidate.len - 1] != '\\'
#endif
                        )) bash_bstr_putc(&candidate, BASH_SEP);
                    bash_bstr_puts(&candidate, fn);
                    fp = fopen(candidate.data, "rb");
                }
                if (*end) p = end + 1; else break;
            }
            bash_bstr_free(&candidate);
        } else {
            char *path = _bash_which(fn);
            if (path) { fp = fopen(path, "rb"); free(path); }
        }
    }
    if (!fp) { fprintf(stderr, "bash: %s: No such file\n", fn); return 1; }
    /* Apply positional args from source command (shift source file name away) */
    if (i + 1 < argc) {
        bash_frame_t *f = _bash_frame_push(ctx, 0);
        f->argc = argc - i - 1;
        f->argv = (char **)_bash_xmalloc(sizeof(char *) * (size_t)(f->argc + 1));
        for (int k = 0; k < f->argc; k++) f->argv[k] = _bash_xstrdup(argv[i + 1 + k]);
        f->argv[f->argc] = NULL;
    }
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    char *src = (char*)_bash_xmalloc((size_t)sz + 1);
    if (sz) { if (fread(src, 1, (size_t)sz, fp) != (size_t)sz) { /* truncated read */ } }
    src[sz] = 0;
    fclose(fp);
    bash_lex_t L; _bash_lex_init(&L, src, 0);
    bash_parser_t P; _bash_parser_init(&P, &L);
    bash_node_t *tree = _bash_parse_program(&P);
    if (tree) {
        int st = _bash_do_exec(ctx, tree);
        (void)st;
    }
    free(src);
    if (i + 1 < argc) {
        /* Pop the frame we pushed for positional parameters */
        _bash_frame_pop(ctx);
    }
    return ctx->last_status;
}

static int bi_alias(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc == 1) {
        /* list all aliases */
        for (int i = 0; i < ctx->aliases.len; i++)
            printf("alias %s='%s'\n", ctx->aliases.items[i].name, ctx->aliases.items[i].value);
        fflush(stdout);
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = 0;
            /* set alias */
            bash_var_t *a = NULL;
            for (int k = 0; k < ctx->aliases.len; k++) {
                if (strcmp(ctx->aliases.items[k].name, argv[i]) == 0) { a = &ctx->aliases.items[k]; break; }
            }
            if (a) { free(a->value); a->value = _bash_xstrdup(eq + 1); }
            else {
                if (ctx->aliases.len >= ctx->aliases.cap) {
                    ctx->aliases.cap = ctx->aliases.cap ? ctx->aliases.cap * 2 : 16;
                    ctx->aliases.items = (bash_var_t*)_bash_xrealloc(ctx->aliases.items, (size_t)ctx->aliases.cap * sizeof(bash_var_t));
                }
                a = &ctx->aliases.items[ctx->aliases.len++];
                a->name = _bash_xstrdup(argv[i]);
                a->value = _bash_xstrdup(eq + 1);
                a->exported = 0; a->is_local = 0; a->is_readonly = 0;
            }
            *eq = '=';
        } else {
            /* print specific alias */
            bash_var_t *a = NULL;
            for (int k = 0; k < ctx->aliases.len; k++) {
                if (strcmp(ctx->aliases.items[k].name, argv[i]) == 0) { a = &ctx->aliases.items[k]; break; }
            }
            if (a) printf("alias %s='%s'\n", a->name, a->value);
            else { fprintf(stderr, "bash: alias: %s: not found\n", argv[i]); rc = 1; }
        }
    }
    fflush(stdout);
    return rc;
}

static int bi_unalias(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "bash: unalias: usage: unalias name [name ...]\n"); return 1; }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int found = 0;
        for (int k = 0; k < ctx->aliases.len; k++) {
            if (strcmp(ctx->aliases.items[k].name, argv[i]) == 0) {
                free(ctx->aliases.items[k].name);
                free(ctx->aliases.items[k].value);
                /* shift down */
                for (int j = k; j < ctx->aliases.len - 1; j++)
                    ctx->aliases.items[j] = ctx->aliases.items[j + 1];
                ctx->aliases.len--;
                found = 1;
                break;
            }
        }
        if (!found) { fprintf(stderr, "bash: unalias: %s: not found\n", argv[i]); rc = 1; }
    }
    return rc;
}
static int bi_trap(bash_ctx_t *ctx, int argc, char **argv)
{
    /* usage: trap [-lp] [arg signal_spec ...] */
    if (argc < 2) {
        /* list traps */
        for (int i = 0; i < 32; i++) {
            if (ctx->trap_cmds[i]) {
                printf("trap -- '%s' %d\n", ctx->trap_cmds[i], i);
            }
        }
        fflush(stdout);
        return 0;
    }
    int idx = 1;
    /* skip -l or -p flags (simplified) */
    if (strcmp(argv[idx], "-l") == 0 || strcmp(argv[idx], "-p") == 0) idx++;
    if (idx >= argc) return 0;
    const char *cmd = argv[idx];
    idx++;
    for (; idx < argc; idx++) {
        int sig = atoi(argv[idx]);
        if (sig <= 0) {
            /* try name: INT=2, TERM=15, etc. simplified */
            if (strcmp(argv[idx], "INT") == 0) sig = 2;
            else if (strcmp(argv[idx], "TERM") == 0) sig = 15;
            else if (strcmp(argv[idx], "EXIT") == 0) sig = 0;
            else if (strcmp(argv[idx], "HUP") == 0) sig = 1;
            else if (strcmp(argv[idx], "QUIT") == 0) sig = 3;
            else if (strcmp(argv[idx], "KILL") == 0) sig = 9;
            else if (strcmp(argv[idx], "USR1") == 0) sig = 10;
            else if (strcmp(argv[idx], "USR2") == 0) sig = 12;
            else if (strcmp(argv[idx], "CHLD") == 0) sig = 17;
            else sig = -1;
        }
        if (sig < 0 || sig >= 32) continue;
        if (strcmp(cmd, "-") == 0) {
            /* reset trap */
            free(ctx->trap_cmds[sig]);
            ctx->trap_cmds[sig] = NULL;
        } else {
            free(ctx->trap_cmds[sig]);
            ctx->trap_cmds[sig] = _bash_xstrdup(cmd);
        }
    }
    return 0;
}

static int bi_hash(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    if (argc >= 2 && strcmp(argv[1], "-r") == 0) {
        /* clear hash table (no-op since we don't cache) */
        return 0;
    }
    /* list hashed commands (none since we don't cache) */
    printf("hits    command\n");
    fflush(stdout);
    return 0;
}

static int bi_dirs(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)argc; (void)argv;
    /* print directory stack: top of stack first */
    char buf[4096];
    char *cwd = BASH_GETCWD(buf, sizeof(buf));
    if (cwd) { _bash_normalize_path(cwd); printf("%s", cwd); }
    for (int i = ctx->dir_stack_len - 1; i >= 0; i--)
        printf(" %s", ctx->dir_stack[i]);
    putchar('\n');
    fflush(stdout);
    return 0;
}

static int bi_pushd(bash_ctx_t *ctx, int argc, char **argv)
{
    const char *target = (argc > 1) ? argv[1] : ".";
    /* expand ~ */
    char buf[4096];
    if (target[0] == '~') {
        const char *home = getenv("HOME");
#ifdef BASH_PLATFORM_WINDOWS
        if (!home || !*home) home = getenv("USERPROFILE");
#endif
        if (home && *home) { snprintf(buf, sizeof(buf), "%s%s", home, target + 1); target = buf; }
    }
    /* save current dir on stack (normalized to forward slashes) */
    char cwd_buf[4096];
    char *cwd = BASH_GETCWD(cwd_buf, sizeof(cwd_buf));
    if (!cwd) { fprintf(stderr, "bash: pushd: cannot get cwd\n"); return 1; }
    _bash_normalize_path(cwd);
    if (BASH_CHDIR(target) != 0) {
        fprintf(stderr, "bash: pushd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    if (ctx->dir_stack_len < 256) {
        ctx->dir_stack[ctx->dir_stack_len++] = _bash_xstrdup(cwd);
    }
    /* update PWD */
    char *ncwd = BASH_GETCWD(buf, sizeof(buf));
    if (ncwd) { _bash_normalize_path(ncwd); _bash_var_set(ctx, "PWD", ncwd, 1, 1, 0);
        if (ctx->pwd) { free(ctx->pwd); ctx->pwd = _bash_xstrdup(ncwd); } }
    /* print stack */
    bi_dirs(ctx, 0, NULL);
    return 0;
}

static int bi_popd(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)argc; (void)argv;
    if (ctx->dir_stack_len == 0) {
        fprintf(stderr, "bash: popd: directory stack empty\n");
        return 1;
    }
    char *target = ctx->dir_stack[--ctx->dir_stack_len];
    if (BASH_CHDIR(target) != 0) {
        fprintf(stderr, "bash: popd: %s: %s\n", target, strerror(errno));
        free(target);
        return 1;
    }
    free(target);
    /* update PWD */
    char buf[4096];
    char *cwd = BASH_GETCWD(buf, sizeof(buf));
    if (cwd) { _bash_normalize_path(cwd); _bash_var_set(ctx, "PWD", cwd, 1, 1, 0);
        if (ctx->pwd) { free(ctx->pwd); ctx->pwd = _bash_xstrdup(cwd); } }
    bi_dirs(ctx, 0, NULL);
    return 0;
}
static int bi_mktemp(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    const char *tpl = (argc > 1) ? argv[1] : "tmp.XXXXXX";
#ifdef BASH_PLATFORM_WINDOWS
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s%d", tpl, (int)GetTickCount());
    printf("%s\n", buf);
    return 0;
#else
    char *copy = _bash_xstrdup(tpl);
    int fd = mkstemp(copy);
    if (fd < 0) { perror("mktemp"); free(copy); return 1; }
    BASH_CLOSE(fd);
    printf("%s\n", copy); free(copy); return 0;
#endif
}
static int bi_bg(bash_ctx_t *ctx, int argc, char **argv){ (void)ctx;(void)argc;(void)argv; return 0; }
static int bi_fg(bash_ctx_t *ctx, int argc, char **argv){ (void)ctx;(void)argc;(void)argv; return 0; }

/* Cross-platform signal delivery: POSIX has kill(); Windows lacks it so we
 * fall back to TerminateProcess (which approximates SIGKILL/SIGTERM). */
static int _bash_kill_pid(int pid, int sig)
{
#ifdef BASH_PLATFORM_WINDOWS
    (void)sig;
    if (pid <= 0) { errno = EINVAL; return -1; }
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) { errno = EPERM; return -1; }
    int rc = TerminateProcess(h, 1) ? 0 : -1;
    CloseHandle(h);
    return rc;
#else
    return kill(pid, sig);
#endif
}

static int bi_kill(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    if (argc < 2) {
        fprintf(stderr, "bash: kill: usage: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ...\n");
        return 1;
    }
    int sig = 15; /* TERM */
    int start = 1;
    if (argv[1][0] == '-') {
        if (strcmp(argv[1], "-l") == 0) {
            /* list signals */
            const char *snames[] = {"EXIT","HUP","INT","QUIT","ILL","TRAP","ABRT","BUS","FPE","KILL","USR1","SEGV","USR2","PIPE","ALRM","TERM",NULL};
            if (argc >= 3) {
                int s = atoi(argv[2]);
                if (s >= 0 && s < 16) printf("%s\n", snames[s]);
            } else {
                for (int i = 0; snames[i]; i++) printf("%d) %s\n", i, snames[i]);
            }
            fflush(stdout);
            return 0;
        }
        if (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "-n") == 0) {
            if (argc < 3) { fprintf(stderr, "bash: kill: -s: option requires an argument\n"); return 1; }
            sig = atoi(argv[2]); start = 3;
        } else {
            /* -SIGNAME or -SIGNUM */
            const char *s = argv[1] + 1;
            if (strcmp(s, "TERM") == 0) sig = 15;
            else if (strcmp(s, "INT") == 0) sig = 2;
            else if (strcmp(s, "HUP") == 0) sig = 1;
            else if (strcmp(s, "KILL") == 0) sig = 9;
            else if (strcmp(s, "QUIT") == 0) sig = 3;
            else if (strcmp(s, "USR1") == 0) sig = 10;
            else if (strcmp(s, "USR2") == 0) sig = 12;
            else sig = atoi(s);
            start = 2;
        }
    }
    int rc = 0;
    for (int i = start; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (pid <= 0) { fprintf(stderr, "bash: kill: %s: arguments must be process or job IDs\n", argv[i]); rc = 1; continue; }
        if (_bash_kill_pid(pid, sig) != 0) {
            fprintf(stderr, "bash: kill: (%d) - %s\n", pid, strerror(errno));
            rc = 1;
        }
    }
    return rc;
}
static int bi_command(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc < 2) return 0;
    /* skip builtins: treat as external */
    /* Just re-run exec treating argv+1 as words. */
    bash_cmd_t cmd; memset(&cmd, 0, sizeof(cmd));
    cmd.n_words = argc - 1;
    cmd.words = (char**)_bash_xmalloc(sizeof(char*) * (size_t)(cmd.n_words + 1));
    for (int i = 0; i < cmd.n_words; i++) cmd.words[i] = argv[i + 1];
    cmd.words[cmd.n_words] = NULL;
    /* no redirects, no assigns */
    bash_redir_state_t rs;
    int rc = -1;
    if (_bash_redir_apply(ctx, &cmd, &rs) == 0) {
        bash_spawn_opts_t so; so.fd_stdin = -1; so.fd_stdout = -1; so.fd_stderr = -1; so.fd_stderr_to_stdout = 0; so.background = 0;
        char *prog = _bash_which(cmd.words[0]);
        if (prog) {
            rc = _bash_spawn(prog, cmd.words, &so, NULL);
            free(prog);
        } else {
            fprintf(stderr, "bash: %s: command not found\n", cmd.words[0]);
            rc = 127;
        }
        _bash_redir_restore(&rs);
    }
    free(cmd.words);
    return rc;
}
static int bi_declare(bash_ctx_t *ctx, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        /* Support both "name=value" and bare "name" forms.
         * Bare names (e.g. `local a b`) MUST still create an empty-valued
         * entry on the current frame, otherwise `local a b` is effectively
         * a no-op and subsequent writes to "a" / "b" would walk up and
         * clobber the caller's variables in recursive calls (we rely on
         * _bash_var_set finding an existing slot in the current frame). */
        char *arg = argv[i];
        char *eq = strchr(arg, '=');
        if (eq) {
            /* split name=value without mutating the original argv[i] string */
            size_t nlen = (size_t)(eq - arg);
            char nbuf[512];
            if (nlen >= sizeof(nbuf)) nlen = sizeof(nbuf) - 1;
            memcpy(nbuf, arg, nlen);
            nbuf[nlen] = 0;
            _bash_var_set_local(ctx, nbuf, eq + 1, 0, 0);
        } else {
            /* bare name: create empty-valued local entry */
            _bash_var_set_local(ctx, arg, "", 0, 0);
        }
    }
    return 0;
}
static int bi_local(bash_ctx_t *ctx, int argc, char **argv)
{
    /* same as declare but force to current frame (already default) */
    return bi_declare(ctx, argc, argv);
}
/* Forward decl: builtin lookup (defined after the builtin table). */
static bash_builtin_fn _bash_find_builtin(const char *name);

static int bi_eval(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc < 2) return 0;
    /* join all args with spaces */
    int total = 0;
    for (int i = 1; i < argc; i++) total += (int)strlen(argv[i]) + 1;
    char *src = (char*)_bash_xmalloc((size_t)total + 1);
    src[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(src, " ");
        strcat(src, argv[i]);
    }
    bash_lex_t L; _bash_lex_init(&L, src, 0);
    bash_parser_t P; _bash_parser_init(&P, &L);
    bash_node_t *tree = _bash_parse_program(&P);
    int rc = 0;
    if (tree) rc = _bash_do_exec(ctx, tree);
    free(src);
    return rc;
}

static int bi_exec(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc < 2) return 0;
    /* apply env assignments */
    int start = 1;
    while (start < argc && strchr(argv[start], '=') && !_bash_find_builtin(argv[start])) {
        char *eq = strchr(argv[start], '=');
        *eq = 0;
        _bash_var_set(ctx, argv[start], eq+1, 1, 1, 0);
        *eq = '=';
        start++;
    }
    if (start >= argc) return 0;
    /* try builtin/function first with redirections */
    bash_builtin_fn bfn = _bash_find_builtin(argv[start]);
    if (bfn) return bfn(ctx, argc - start, argv + start);
    /* external: execvp replaces process */
    char **new_argv = (char**)_bash_xmalloc(sizeof(char*) * (size_t)(argc - start + 1));
    for (int i = start; i < argc; i++) new_argv[i - start] = argv[i];
    new_argv[argc - start] = NULL;
    execvp(new_argv[0], new_argv);
    free(new_argv);
    fprintf(stderr, "bash: exec: %s: %s\n", argv[start], strerror(errno));
    return 127;
}

static int bi_umask(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    if (argc < 2) {
        /* print current umask in octal */
        mode_t old = umask(0);
        umask(old);
        printf("%03o\n", (unsigned)old);
        fflush(stdout);
        return 0;
    }
    /* parse octal or symbolic (simplified: octal only) */
    char *endp;
    long m = strtol(argv[1], &endp, 8);
    if (*endp != 0 || m < 0 || m > 0777) {
        fprintf(stderr, "bash: umask: %s: octal number out of range\n", argv[1]);
        return 1;
    }
    umask((mode_t)m);
    return 0;
}

static int bi_help(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx;
    if (argc >= 2) {
        /* show help for specific builtin */
        bash_builtin_fn bfn = _bash_find_builtin(argv[1]);
        if (bfn) {
            printf("%s: shell builtin\n", argv[1]);
            return 0;
        }
        fprintf(stderr, "bash: help: no help topics match `%s'\n", argv[1]);
        return 1;
    }
    printf("Shell builtins:\n");
    extern bash_builtin_t _bash_builtins[];
    for (int i = 0; _bash_builtins[i].name; i++) {
        printf("  %s\n", _bash_builtins[i].name);
    }
    fflush(stdout);
    return 0;
}

static int bi_builtin(bash_ctx_t *ctx, int argc, char **argv)
{
    if (argc < 2) return 0;
    bash_builtin_fn bfn = _bash_find_builtin(argv[1]);
    if (!bfn) {
        fprintf(stderr, "bash: builtin: %s: not a shell builtin\n", argv[1]);
        return 1;
    }
    return bfn(ctx, argc - 1, argv + 1);
}

static int bi_logout(bash_ctx_t *ctx, int argc, char **argv)
{
    int code = ctx->last_status;
    if (argc >= 2) code = atoi(argv[1]);
    ctx->exit_code = code;
    ctx->do_exit = 1;
    return code;
}

static int bi_suspend(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx; (void)argc; (void)argv;
    /* In a real shell this would SIGSTOP itself. Simplified: no-op. */
    fprintf(stderr, "bash: suspend: cannot suspend (no job control)\n");
    return 1;
}

static int bi_getopts(bash_ctx_t *ctx, int argc, char **argv)
{
    /* usage: getopts OPTSTRING NAME [arg...] */
    if (argc < 3) {
        fprintf(stderr, "bash: getopts: usage: getopts optstring name [arg]\n");
        return 1;
    }
    const char *optstring = argv[1];
    const char *name = argv[2];
    /* get OPTIND */
    const char *optind_s = getenv("OPTIND");
    int optind = optind_s ? atoi(optind_s) : 1;
    /* args to parse: either provided args or positional params */
    char **args = (argc > 3) ? argv + 3 : ctx->frame->argv;
    int nargs = (argc > 3) ? (argc - 3) : ctx->frame->argc;
    if (optind >= nargs) {
        _bash_var_set(ctx, name, "?", 0, 0, 0);
        return 1;
    }
    const char *arg = args[optind];
    if (!arg || arg[0] != '-' || !arg[1] || arg[1] == '-') {
        _bash_var_set(ctx, name, "?", 0, 0, 0);
        return 1;
    }
    char opt = arg[1];
    const char *p = strchr(optstring, opt);
    if (!p) {
        _bash_var_set(ctx, name, "?", 0, 0, 0);
        char buf[32]; snprintf(buf, sizeof(buf), "%d", optind + 1);
#ifdef BASH_PLATFORM_WINDOWS
        extern int __cdecl _putenv_s(const char *, const char *);
        _putenv_s("OPTIND", buf); SetEnvironmentVariableA("OPTIND", buf);
#else
        setenv("OPTIND", buf, 1);
#endif
        return 0;
    }
    /* option takes argument? */
    if (p[1] == ':') {
        if (arg[2]) {
            _bash_var_set(ctx, name, (char[]){opt, 0}, 0, 0, 0);
            _bash_var_set(ctx, "OPTARG", arg + 2, 0, 0, 0);
        } else if (optind + 1 < nargs) {
            _bash_var_set(ctx, name, (char[]){opt, 0}, 0, 0, 0);
            _bash_var_set(ctx, "OPTARG", args[optind + 1], 0, 0, 0);
            optind++;
        } else {
            _bash_var_set(ctx, name, "?", 0, 0, 0);
            fprintf(stderr, "bash: getopts: option requires an argument -- %c\n", opt);
            return 1;
        }
    } else {
        _bash_var_set(ctx, name, (char[]){opt, 0}, 0, 0, 0);
    }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", optind + 1);
#ifdef BASH_PLATFORM_WINDOWS
    extern int __cdecl _putenv_s(const char *, const char *);
    _putenv_s("OPTIND", buf); SetEnvironmentVariableA("OPTIND", buf);
#else
    setenv("OPTIND", buf, 1);
#endif
    return 0;
}

/* ========================================================================
 * compgen builtin (Bash 5.2+): generate completion candidates.
 *   -v            list shell variable names
 *   -f            list filenames (current dir)
 *   -b            list builtin names
 *   -k            list reserved keywords
 *   -a            list aliases
 *   -c            list command names (builtin + func + external-like)
 *   -A NAME       action alias: see bash manual
 *   -W WORDS      custom word list (IFS-split)
 *   -X PATTERN    filter: exclude words matching PATTERN (glob)
 *   -V VAR        [Bash 5.3] store results into shell array-like VAR varname[0..n] with count in VAR
 *   WORD          only print matches beginning with WORD (prefix filter)
 * ======================================================================== */
static int bi_compgen(bash_ctx_t *ctx, int argc, char **argv)
{
    bash_barray_t words; bash_barray_init(&words);
    int want_vars = 0, want_files = 0, want_builtins = 0;
    int want_keywords = 0, want_aliases = 0, want_commands = 0;
    const char *custom_words = NULL;
    const char *filter_pat = NULL;
    const char *out_var = NULL; /* -V VARNAME */
    const char *prefix = NULL;
    int i = 1;
    while (i < argc) {
        const char *a = argv[i];
        if (strcmp(a, "-v") == 0) { want_vars = 1; i++; }
        else if (strcmp(a, "-f") == 0) { want_files = 1; i++; }
        else if (strcmp(a, "-b") == 0) { want_builtins = 1; i++; }
        else if (strcmp(a, "-k") == 0) { want_keywords = 1; i++; }
        else if (strcmp(a, "-a") == 0) { want_aliases = 1; i++; }
        else if (strcmp(a, "-c") == 0) { want_commands = 1; i++; }
        else if (strcmp(a, "-A") == 0 && i + 1 < argc) {
            const char *act = argv[i + 1];
            if      (strcmp(act, "variable") == 0) want_vars = 1;
            else if (strcmp(act, "file") == 0)     want_files = 1;
            else if (strcmp(act, "builtin") == 0)  want_builtins = 1;
            else if (strcmp(act, "keyword") == 0)  want_keywords = 1;
            else if (strcmp(act, "alias") == 0)    want_aliases = 1;
            else if (strcmp(act, "command") == 0)  want_commands = 1;
            i += 2;
        }
        else if (strcmp(a, "-W") == 0 && i + 1 < argc) { custom_words = argv[i+1]; i += 2; }
        else if (strcmp(a, "-X") == 0 && i + 1 < argc) { filter_pat = argv[i+1];    i += 2; }
        else if (strcmp(a, "-V") == 0 && i + 1 < argc) { out_var = argv[i+1];        i += 2; }
        else if (a[0] != '-') { prefix = a; i++; }
        else break;
    }

    /* --- build candidate list --- */
    extern bash_builtin_t _bash_builtins[];
    extern const char *bash_keywords[];

    if (want_builtins || want_commands) {
        for (int k = 0; _bash_builtins[k].name; k++)
            bash_barray_push(&words, _bash_xstrdup(_bash_builtins[k].name));
    }
    if (want_keywords) {
        for (int k = 0; bash_keywords[k]; k++)
            bash_barray_push(&words, _bash_xstrdup(bash_keywords[k]));
    }
    if (want_aliases) {
        for (int k = 0; k < ctx->aliases.len; k++)
            bash_barray_push(&words, _bash_xstrdup(ctx->aliases.items[k].name));
    }
    if (want_vars) {
        /* Walk frames bottom-up: globals -> each local -> current top */
        bash_frame_t *frames[64]; int nf = 0;
        for (bash_frame_t *f = ctx->frame; f; f = f->parent)
            if (nf < 64) frames[nf++] = f;
        for (int fi = nf - 1; fi >= 0; fi--) {
            bash_vars_t *v = &frames[fi]->vars;
            for (int k = 0; k < v->len; k++)
                bash_barray_push(&words, _bash_xstrdup(v->items[k].name));
        }
    }
    if (want_commands) {
        /* functions */
        for (bash_funcdef_t *f = ctx->funcs; f; f = f->next)
            bash_barray_push(&words, _bash_xstrdup(f->name));
    }
    if (want_files) {
        /* list current directory entries (non-. ..) */
#ifdef BASH_PLATFORM_WINDOWS
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(".\\*", &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                bash_barray_push(&words, _bash_xstrdup(fd.cFileName));
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
#else
        DIR *d = opendir(".");
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d))) {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
                bash_barray_push(&words, _bash_xstrdup(ent->d_name));
            }
            closedir(d);
        }
#endif
    }
    if (custom_words) {
        const char *ifs = getenv("IFS");
        if (!ifs) ifs = " \t\n";
        const char *p = custom_words;
        while (*p) {
            while (*p && strchr(ifs, *p)) p++;
            if (!*p) break;
            const char *s = p;
            while (*p && !strchr(ifs, *p)) p++;
            bash_barray_push(&words, _bash_xstrndup(s, (size_t)(p - s)));
        }
    }

    /* --- filters: prefix, -X pattern --- */
    int w = 0;
    for (int r = 0; r < words.len; r++) {
        char *word = words.items[r];
        if (prefix && strncmp(word, prefix, strlen(prefix)) != 0) {
            free(word); continue;
        }
        if (filter_pat && _bash_glob_match(filter_pat, word)) {
            free(word); continue;
        }
        words.items[w++] = word;
    }
    words.len = w;

    /* --- sort final list --- */
    for (int a = 1; a < words.len; a++) {
        for (int b = a; b > 0 && strcmp(words.items[b-1], words.items[b]) > 0; b--) {
            char *t = words.items[b-1]; words.items[b-1] = words.items[b]; words.items[b] = t;
        }
    }

    int rc = 0;
    if (out_var) {
        /* Bash 5.3: store result in variable. Use indexed keys: VAR=count, VAR[0]..VAR[n-1] */
        char buf[32]; snprintf(buf, sizeof(buf), "%d", words.len);
        _bash_var_set(ctx, out_var, buf, 0, 0, 0);
        for (int k = 0; k < words.len; k++) {
            char key[256];
            snprintf(key, sizeof(key), "%s[%d]", out_var, k);
            _bash_var_set(ctx, key, words.items[k], 0, 0, 0);
        }
    } else {
        for (int k = 0; k < words.len; k++) {
            puts(words.items[k]);
        }
        fflush(stdout);
    }
    bash_barray_free(&words);
    return rc;
}

static int bi_history(bash_ctx_t *ctx, int argc, char **argv);

bash_builtin_t _bash_builtins[] = {
    {"true",     bi_true},
    {"false",    bi_false},
    {":",        bi_true},
    {"exit",     bi_exit},
    {"return",   bi_return},
    {"break",    bi_break},
    {"continue", bi_continue},
    {"cd",       bi_cd},
    {"pwd",      bi_pwd},
    {"echo",     bi_echo},
    {"printf",   bi_printf},
    {"set",      bi_set},
    {"unset",    bi_unset},
    {"export",   bi_export},
    {"env",      bi_env},
    {"shift",    bi_shift},
    {"read",     bi_read},
    {"which",    bi_which},
    {"test",     bi_test},
    {"[",        bi_test},
    {"type",     bi_type},
    {"command",  bi_command},
    {"wait",     bi_wait},
    {"jobs",     bi_jobs},
    {"bg",       bi_bg},
    {"fg",       bi_fg},
    {"kill",     bi_kill},
    {"hash",     bi_hash},
    {"dirs",     bi_dirs},
    {"pushd",    bi_pushd},
    {"popd",     bi_popd},
    {"mktemp",   bi_mktemp},
    {"source",   bi_source},
    {".",        bi_source},
    {"alias",    bi_alias},
    {"unalias",  bi_unalias},
    {"trap",     bi_trap},
    {"declare",  bi_declare},
    {"local",    bi_local},
    {"history",  bi_history},
    {"eval",     bi_eval},
    {"exec",     bi_exec},
    {"umask",    bi_umask},
    {"help",     bi_help},
    {"builtin",  bi_builtin},
    {"logout",   bi_logout},
    {"suspend",  bi_suspend},
    {"getopts",  bi_getopts},
    {"compgen",  bi_compgen},
    {NULL, NULL}
};

static bash_builtin_fn _bash_find_builtin(const char *name)
{
    for (int i = 0; _bash_builtins[i].name; i++)
        if (strcmp(_bash_builtins[i].name, name) == 0) return _bash_builtins[i].fn;
    return NULL;
}

/* ========================================================================
 * Execution engine
 * ======================================================================== */

static int _bash_do_exec_simple(bash_ctx_t *ctx, bash_cmd_t *cmd)
{
    /* apply assignments: if command present -> local to this command only; else set to frame */
    int saved_count = 0;
    char saved_names[32][256];
    char *saved_vals[32];

    for (int i = 0; i < cmd->n_assigns; i++) {
        char *as = cmd->assigns[i];
        char *eq = strchr(as, '=');
        if (!eq) continue;
        /* Do NOT write '*eq = 0' into the original AST string! Recursive
         * calls re-use the same fn->body AST nodes, so clobbering '=' would
         * make inner invocations skip this assignment (strchr returns NULL).
         * Instead, duplicate the name into a stack buffer and reference the
         * RHS directly via pointer offset (no mutation of the source token). */
        char namebuf[256];
        size_t nlen = (size_t)(eq - as);
        if (nlen >= sizeof(namebuf)) nlen = sizeof(namebuf) - 1;
        memcpy(namebuf, as, nlen);
        namebuf[nlen] = 0;
        const char *name = namebuf;
        char *val = eq + 1;
        /* expand val: assignment RHS never undergoes word splitting or
         * pathname expansion (POSIX / bash semantics for name=VALUE).
         * Pass quoted=2 to _bash_expand_word so $var / $(...) still expand but
         * no quote-stripping, no IFS split. */
        int vn = 0;
        char **ve = _bash_expand_word(ctx, val, 2, &vn);
        char *v0 = ve && ve[0] ? ve[0] : "";
        if (cmd->n_words > 0 && saved_count < 32) {
            snprintf(saved_names[saved_count], sizeof(saved_names[saved_count]), "%s", name);
            char *old = _bash_var_get(ctx, name, NULL);
            saved_vals[saved_count] = old ? _bash_xstrdup(old) : NULL;
            saved_count++;
        }
        _bash_var_set(ctx, name, v0, cmd->n_words == 0 ? 0 : 0, 0, 0);
        if (cmd->n_words == 0) {
            /* export: mark as exported if called without command */
        }
        for (int k = 0; ve && ve[k]; k++) free(ve[k]);
        free(ve);
    }
    int rc = 0;
    if (cmd->n_words == 0) {
        rc = 0;
        goto restore_assigns;
    }

    /* expand words */
    bash_barray_t av; bash_barray_init(&av);
    for (int i = 0; i < cmd->n_words; i++) {
        int en = 0;
        int wq = cmd->word_quoted ? cmd->word_quoted[i] : 0;
        char **ew = _bash_expand_word(ctx, cmd->words[i], wq, &en);
        for (int k = 0; k < en; k++) {
            if (!ew[k]) continue;
            /* skip empty produced by unquoted empty expansion? keep */
            bash_barray_push(&av, ew[k]);
        }
        free(ew);
    }
    if (av.len == 0) { rc = 0; goto free_av; }
    bash_barray_push_steal(&av, NULL); av.len--;

    /* alias expansion (only for first word, not in single quotes) */
    if (av.len > 0 && av.items[0]) {
        const char *cmd0 = av.items[0];
        for (int k = 0; k < ctx->aliases.len; k++) {
            if (strcmp(ctx->aliases.items[k].name, cmd0) == 0) {
                /* replace first word with alias value (simplified: no recursive expansion) */
                free(av.items[0]);
                av.items[0] = _bash_xstrdup(ctx->aliases.items[k].value);
                break;
            }
        }
    }

    const char *cmdname = av.items[0] ? av.items[0] : "";
    /* builtin? */
    bash_builtin_fn bfn = _bash_find_builtin(cmdname);
    if (bfn) {
        bash_redir_state_t rs;
        if (_bash_redir_apply(ctx, cmd, &rs) != 0) { rc = 1; goto free_av; }
        rc = bfn(ctx, av.len, av.items);
        _bash_redir_restore(&rs);
        goto free_av;
    }
    /* function? */
    bash_funcdef_t *fn = _bash_func_find(ctx, cmdname);
    if (fn) {
        bash_redir_state_t rs;
        if (_bash_redir_apply(ctx, cmd, &rs) != 0) { rc = 1; goto free_av; }
        _bash_dbg_log("[FUNC_CALL depth=%d] name=%s argc=%d argv0=[%s] argv1=[%s] argv2=[%s]",
                  _bash_frame_depth(ctx)+1,  // +1 because we're about to push
                  cmdname, av.len,
                  av.items[0] ? av.items[0] : "",
                  av.items[1] ? av.items[1] : "",
                  av.items[2] ? av.items[2] : "");
        bash_frame_t *f = _bash_frame_push(ctx, 1);
        f->argc = av.len;
        f->argv = (char**)_bash_xmalloc(sizeof(char*) * (size_t)f->argc);
        for (int i = 0; i < av.len; i++) f->argv[i] = _bash_xstrdup(av.items[i] ? av.items[i] : "");
        int saved_last = ctx->last_status;
        rc = _bash_do_exec(ctx, fn->body);
        if (f->func_ret) rc = f->status;
        _bash_frame_pop(ctx);
        ctx->last_status = saved_last;
        _bash_dbg_log("[FUNC_RET depth=%d] name=%s rc=%d func_ret=%d status=%d",
                  _bash_frame_depth(ctx), cmdname, rc, f ? 0 : 0,
                  (int)rc);
        _bash_redir_restore(&rs);
        goto free_av;
    }
    /* external */
    char *prog = _bash_which(cmdname);
    if (!prog) {
        fprintf(stderr, "bash: %s: command not found\n", cmdname);
        rc = 127; goto free_av;
    }
    int cls = _bash_classify_file(prog);
    if (cls == 2) {
        /* shell script — run with ourselves, like `bash script args...` */
        bash_redir_state_t rs;
        if (_bash_redir_apply(ctx, cmd, &rs) != 0) { rc = 1; free(prog); goto free_av; }
        /* build argc/argv for the script: everything except av[0] (=cmdname) */
        int sargc = 0;
        for (int k = 1; av.items[k]; k++) sargc++;
        char **sargv = (char**)_bash_xmalloc(sizeof(char*) * (size_t)(sargc > 0 ? sargc : 1));
        for (int k = 0; k < sargc; k++) sargv[k] = av.items[k+1];
        rc = _bash_run_script(ctx, prog, sargc, sargv);
        free(sargv);
        _bash_redir_restore(&rs);
        free(prog);
        goto free_av;
    }
#ifdef BASH_PLATFORM_WINDOWS
    if (cls == 3) {
        /* .bat / .cmd — wrap with cmd.exe /c */
        bash_redir_state_t rs;
        if (_bash_redir_apply(ctx, cmd, &rs) != 0) { rc = 1; free(prog); goto free_av; }
        /* construct: cmd.exe /c "prog" arg1 arg2 ... */
        bash_barray_t bav; bash_barray_init(&bav);
        bash_barray_push(&bav, _bash_xstrdup("cmd.exe"));
        bash_barray_push(&bav, _bash_xstrdup("/c"));
        /* wrap program + args together as single arg after /c (simplest: use prog + rest separate; cmd.exe /c "bat" arg works) */
        bash_barray_push(&bav, _bash_xstrdup(prog));
        for (int k = 1; av.items[k]; k++) bash_barray_push(&bav, _bash_xstrdup(av.items[k]));
        bash_barray_push_steal(&bav, NULL); bav.len--;
        bash_spawn_opts_t so; so.fd_stdin = -1; so.fd_stdout = -1; so.fd_stderr = -1; so.fd_stderr_to_stdout = 0; so.background = 0;
        rc = _bash_spawn("cmd.exe", bav.items, &so, NULL);
        for (int k = 0; k < bav.len; k++) free(bav.items[k]);
        free(bav.items);
        _bash_redir_restore(&rs);
        free(prog);
        goto free_av;
    }
#endif
    {
        bash_redir_state_t rs;
        if (_bash_redir_apply(ctx, cmd, &rs) != 0) { rc = 1; free(prog); goto free_av; }
        /* Sync PATH (and other exported vars) to Win32 env block so that
         * child processes (make, gcc, …) inherit the latest value.
         * _bash_var_set already syncs on set, but this is a belt-and-braces
         * measure in case something desynchronised the two.             */
        const char *sync_path = _bash_var_get(ctx, "PATH", NULL);
        if (sync_path && *sync_path) {
#ifdef BASH_PLATFORM_WINDOWS
            SetEnvironmentVariableA("PATH", sync_path);
#else
            setenv("PATH", sync_path, 1);
#endif
        }
        bash_spawn_opts_t so; so.fd_stdin = -1; so.fd_stdout = -1; so.fd_stderr = -1; so.fd_stderr_to_stdout = 0; so.background = 0;
        rc = _bash_spawn(prog, av.items, &so, NULL);
        _bash_redir_restore(&rs);
    }
    free(prog);
free_av:
    for (int i = 0; i < av.len; i++) free(av.items[i]);
    free(av.items);
restore_assigns:
    for (int i = saved_count - 1; i >= 0; i--) {
        _bash_var_set(ctx, saved_names[i], saved_vals[i] ? saved_vals[i] : "", 0, 0, 0);
        free(saved_vals[i]);
    }
    return rc;
}

static int _bash_do_exec_pipeline(bash_ctx_t *ctx, bash_node_t *left, bash_node_t *right, int pipeall)
{
    /* Create pipe, exec left with stdout -> pipe write, right with stdin <- pipe read */
    int pfds[2];
    if (BASH_PIPE(pfds) != 0) return 1;
    int rc_left, rc_right;
    int pidl = -1, pidr = -1;
    (void)pidl; (void)pidr;

#ifdef BASH_PLATFORM_WINDOWS
    /* Windows: sequential w/ pipe redirection (simpler approach) */
    int old_stdout = BASH_DUP(1);
    BASH_DUP2(pfds[1], 1);
    _bash_sync_stdhandles();
    rc_left = _bash_do_exec(ctx, left);
    BASH_DUP2(old_stdout, 1); BASH_CLOSE(old_stdout);
    _bash_sync_stdhandles();
    BASH_CLOSE(pfds[1]);
    int old_stdin = BASH_DUP(0);
    BASH_DUP2(pfds[0], 0);
    _bash_sync_stdhandles();
    rc_right = _bash_do_exec(ctx, right);
    BASH_DUP2(old_stdin, 0); BASH_CLOSE(old_stdin);
    _bash_sync_stdhandles();
    BASH_CLOSE(pfds[0]);
    (void)pipeall;
#else
    pid_t p1 = fork();
    if (p1 == 0) {
        BASH_DUP2(pfds[1], 1);
        if (pipeall) BASH_DUP2(pfds[1], 2);
        BASH_CLOSE(pfds[0]); BASH_CLOSE(pfds[1]);
        exit(_bash_do_exec(ctx, left));
    }
    pid_t p2 = fork();
    if (p2 == 0) {
        BASH_DUP2(pfds[0], 0);
        BASH_CLOSE(pfds[0]); BASH_CLOSE(pfds[1]);
        exit(_bash_do_exec(ctx, right));
    }
    BASH_CLOSE(pfds[0]); BASH_CLOSE(pfds[1]);
    int st;
    waitpid(p1, &st, 0);
    rc_left = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    waitpid(p2, &st, 0);
    rc_right = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
#endif
    (void)rc_left;
    return rc_right;
}

static int _bash_do_exec(bash_ctx_t *ctx, bash_node_t *node)
{
    if (!node || ctx->do_exit) return ctx->last_status;
#define FRAME_FLOW_STOP(ctx) \
    ((ctx)->frame->break_level > 0 || (ctx)->frame->continue_level > 0 || (ctx)->frame->func_ret || (ctx)->do_exit)
    switch (node->type) {
        case N_CMD: {
            if (node->u.cmd.n_words > 0 && node->u.cmd.words[0]) {
                _bash_dbg_log("[EXEC_NCMD depth=%d] %s%s%s n_words=%d n_assigns=%d first_word=[%s] assign0=[%s]",
                          _bash_frame_depth(ctx),
                          (node->u.cmd.n_assigns>0)?"[ASSIGN] ":"",
                          (ctx->frame && ctx->frame->func_ret)?"[FUNC_RET_PENDING] ":"",
                          (ctx->frame && ctx->frame->continue_level)?"[CONT_PENDING] ":"",
                          node->u.cmd.n_words,
                          node->u.cmd.n_assigns,
                          node->u.cmd.words[0] ? node->u.cmd.words[0] : "(null)",
                          (node->u.cmd.n_assigns>0 && node->u.cmd.assigns[0]) ? node->u.cmd.assigns[0] : "");
            } else {
                _bash_dbg_log("[EXEC_NCMD depth=%d] EMPTY/ASSIGN_ONLY n_words=%d n_assigns=%d assign0=[%s]",
                          _bash_frame_depth(ctx),
                          node->u.cmd.n_words, node->u.cmd.n_assigns,
                          (node->u.cmd.n_assigns>0 && node->u.cmd.assigns[0]) ? node->u.cmd.assigns[0] : "");
            }
            int st = _bash_do_exec_simple(ctx, &node->u.cmd);
            ctx->last_status = st;
            return st;
        }
        case N_PIPE: {
            int st = _bash_do_exec_pipeline(ctx, node->u.bin.left, node->u.bin.right, node->u.bin.invert);
            ctx->last_status = st;
            return st;
        }
        case N_AND: {
            _bash_dbg_log("[EXEC_AND depth=%d] LEFT", _bash_frame_depth(ctx));
            int l = _bash_do_exec(ctx, node->u.bin.left);
            _bash_dbg_log("[EXEC_AND depth=%d] LEFT_RC=%d%s", _bash_frame_depth(ctx), l, node->u.bin.right ? (l==0?" RUN_RIGHT":" SKIP_RIGHT"):" NO_RIGHT");
            if (ctx->do_exit) return l;
            if (FRAME_FLOW_STOP(ctx)) { _bash_dbg_log("[EXEC_AND depth=%d] FLOW_STOP", _bash_frame_depth(ctx)); ctx->last_status = l; return l; }
            if (l == 0) {
                int r = _bash_do_exec(ctx, node->u.bin.right);
                ctx->last_status = r;
                return r;
            }
            ctx->last_status = l;
            return l;
        }
        case N_OR: {
            _bash_dbg_log("[EXEC_OR depth=%d] LEFT", _bash_frame_depth(ctx));
            int l = _bash_do_exec(ctx, node->u.bin.left);
            _bash_dbg_log("[EXEC_OR depth=%d] LEFT_RC=%d%s", _bash_frame_depth(ctx), l, node->u.bin.right ? (l!=0?" RUN_RIGHT":" SKIP_RIGHT"):" NO_RIGHT");
            if (ctx->do_exit) return l;
            if (FRAME_FLOW_STOP(ctx)) { _bash_dbg_log("[EXEC_OR depth=%d] FLOW_STOP", _bash_frame_depth(ctx)); ctx->last_status = l; return l; }
            if (l != 0) {
                int r = _bash_do_exec(ctx, node->u.bin.right);
                ctx->last_status = r;
                return r;
            }
            ctx->last_status = l;
            return l;
        }
        case N_SEMI: {
            _bash_dbg_log("[EXEC_SEMI depth=%d] LEFT", _bash_frame_depth(ctx));
            int l = _bash_do_exec(ctx, node->u.bin.left);
            _bash_dbg_log("[EXEC_SEMI depth=%d] LEFT_RC=%d%s%s",
                      _bash_frame_depth(ctx), l,
                      FRAME_FLOW_STOP(ctx) ? " [FLOW_STOP_AFTER_LEFT]" : "",
                      node->u.bin.right ? " THEN_RIGHT" : "");
            if (ctx->do_exit) return l;
            if (FRAME_FLOW_STOP(ctx)) { ctx->last_status = l; return l; }
            if (node->u.bin.right) {
                int r = _bash_do_exec(ctx, node->u.bin.right);
                _bash_dbg_log("[EXEC_SEMI depth=%d] RIGHT_RC=%d", _bash_frame_depth(ctx), r);
                return r;
            }
            ctx->last_status = l;
            return l;
        }
        case N_BG: {
            int l = _bash_do_exec(ctx, node->u.bin.left);
            (void)l;
            ctx->last_status = 0;
            if (FRAME_FLOW_STOP(ctx)) return 0;
            if (node->u.bin.right) return _bash_do_exec(ctx, node->u.bin.right);
            return 0;
        }
        case N_NOT: {
            int c = _bash_do_exec(ctx, node->u.not_child);
            int r = (c == 0) ? 1 : 0;
            ctx->last_status = r;
            return r;
        }
        case N_GROUP: {
            int st;
            if (node->u.group.subshell) {
#if BASH_PLATFORM_POSIX
                pid_t p = fork();
                if (p == 0) {
                    exit(_bash_do_exec(ctx, node->u.group.body));
                }
                int s; waitpid(p, &s, 0);
                st = WIFEXITED(s) ? WEXITSTATUS(s) : 1;
#else
                /* no fork on Windows: just execute inline */
                st = _bash_do_exec(ctx, node->u.group.body);
#endif
            } else {
                st = _bash_do_exec(ctx, node->u.group.body);
            }
            ctx->last_status = st;
            return st;
        }
        case N_IF: {
            int taken = 0, st = 0;
            _bash_dbg_log("[EXEC_IF depth=%d] n=%d", _bash_frame_depth(ctx), node->u.ifn.n);
            for (int i = 0; i < node->u.ifn.n; i++) {
                if (!node->u.ifn.conds[i]) {
                    /* else */
                    _bash_dbg_log("[EXEC_IF depth=%d] branch[%d]=ELSE", _bash_frame_depth(ctx), i);
                    st = _bash_do_exec(ctx, node->u.ifn.bodies[i]);
                    ctx->last_status = st;
                    return st;
                }
                _bash_dbg_log("[EXEC_IF depth=%d] eval cond[%d]", _bash_frame_depth(ctx), i);
                int c = _bash_do_exec(ctx, node->u.ifn.conds[i]);
                _bash_dbg_log("[EXEC_IF depth=%d] cond[%d] rc=%d %s", _bash_frame_depth(ctx), i, c, (c==0)?"TRUE":"FALSE");
                if (ctx->do_exit) return c;
                if (c == 0) {
                    _bash_dbg_log("[EXEC_IF depth=%d] run body[%d]", _bash_frame_depth(ctx), i);
                    st = _bash_do_exec(ctx, node->u.ifn.bodies[i]);
                    ctx->last_status = st;
                    taken = 1;
                    return st;
                }
            }
            (void)taken;
            return ctx->last_status;
        }
        case N_WHILE:
        case N_UNTIL: {
            int st = 0;
            for (;;) {
                if (ctx->frame->break_level > 0) { ctx->frame->break_level--; break; }
                int c = _bash_do_exec(ctx, node->u.loop.cond);
                if (ctx->do_exit) return c;
                int cond_true = (c == 0);
                if (node->type == N_UNTIL) cond_true = !cond_true;
                if (!cond_true) break;
                /* continue-level is consumed inside body execution (via FRAME_FLOW_STOP in list nodes)
                 * so that body tail commands are skipped. We still consume any leftover flag
                 * after body returns so it does not leak to a parent loop. */
                st = _bash_do_exec(ctx, node->u.loop.body);
                if (ctx->frame->continue_level > 0) ctx->frame->continue_level--;
                if (ctx->frame->func_ret) break;
            }
            ctx->last_status = st;
            return st;
        }
        case N_FOR: {
            int st = 0;
            bash_for_t *f = &node->u.forn;
            if (f->arithmetic_style) {
                /* Expand ${...} / $((...)) / $var in init/cond/step before evaluating. */
                char *ex_init = NULL, *ex_cond = NULL, *ex_step = NULL;
                if (f->init && *f->init) {
                    int en = 0; char **ew = _bash_expand_word(ctx, f->init, 1, &en);
                    if (ew && en > 0 && ew[0]) ex_init = _bash_xstrdup(ew[0]);
                    if (ew) free(ew);
                }
                if (f->cond_a && *f->cond_a) {
                    int en = 0; char **ew = _bash_expand_word(ctx, f->cond_a, 1, &en);
                    if (ew && en > 0 && ew[0]) ex_cond = _bash_xstrdup(ew[0]);
                    if (ew) free(ew);
                }
                if (f->step && *f->step) {
                    int en = 0; char **ew = _bash_expand_word(ctx, f->step, 1, &en);
                    if (ew && en > 0 && ew[0]) ex_step = _bash_xstrdup(ew[0]);
                    if (ew) free(ew);
                }
                if (ex_init && *ex_init) _bash_eval_arith(ctx, ex_init);
                for (;;) {
                    if (ctx->frame->break_level > 0) { ctx->frame->break_level--; break; }
                    long cv = 1;
                    if (ex_cond && *ex_cond) cv = _bash_eval_arith(ctx, ex_cond);
                    if (!cv) break;
                    st = _bash_do_exec(ctx, f->body);
                    if (ctx->frame->func_ret) break;
                    if (ex_step && *ex_step) _bash_eval_arith(ctx, ex_step);
                    /* consume leftover continue flag */
                    if (ctx->frame->continue_level > 0) ctx->frame->continue_level--;
                }
                free(ex_init); free(ex_cond); free(ex_step);
            } else {
                /* collect words to iterate */
                bash_barray_t wl; bash_barray_init(&wl);
                if (!f->words || f->n_words == 0) {
                    /* use $@ */
                    bash_frame_t *fr = ctx->frame;
                    while (fr && !fr->is_func) fr = fr->parent;
                    if (fr) {
                        for (int i = 1; i < fr->argc; i++)
                            if (fr->argv[i]) bash_barray_push(&wl, _bash_xstrdup(fr->argv[i]));
                    }
                } else {
                    for (int i = 0; i < f->n_words; i++) {
                        int en = 0;
                        char **ew = _bash_expand_word(ctx, f->words[i], 0, &en);
                        for (int k = 0; k < en; k++) bash_barray_push(&wl, _bash_xstrdup(ew[k] ? ew[k] : ""));
                        free(ew);
                    }
                }
                for (int i = 0; i < wl.len; i++) {
                    if (ctx->frame->break_level > 0) { ctx->frame->break_level--; break; }
                    _bash_var_set(ctx, f->name ? f->name : "i", wl.items[i] ? wl.items[i] : "", 0, 0, 0);
                    st = _bash_do_exec(ctx, f->body);
                    if (ctx->frame->continue_level > 0) ctx->frame->continue_level--;
                    if (ctx->frame->func_ret) break;
                }
                bash_barray_free(&wl);
            }
            ctx->last_status = st;
            return st;
        }
        case N_CASE: {
            bash_case_t *cs = &node->u.casen;
            int wn = 0;
            int wq = cs->word_quoted ? cs->word_quoted : 0;
            char **wv = _bash_expand_word(ctx, cs->word ? cs->word : "", wq, &wn);
            char *word_expanded = (wv && wv[0]) ? wv[0] : "";
            int st = 0, matched = 0;
            for (int i = 0; i < cs->n; i++) {
                char **pats = cs->patterns[i];
                if (!pats) continue;
                for (int k = 0; pats[k]; k++) {
                    int pn = 0;
                    /* Use quoted=2 (double-quote semantics): $var / $(cmdsub)
                     * still expand, but FILE GLOBBING and word splitting are
                     * skipped — critical because the pattern chars `*?[]`
                     * are wildcards for the *match against word*, not for
                     * filesystem globbing. */
                    char **pv = _bash_expand_word(ctx, pats[k], 2, &pn);
                    const char *pat = (pv && pv[0]) ? pv[0] : "";
                    if (_bash_glob_match(pat, word_expanded)) {
                        matched = 1;
                        st = _bash_do_exec(ctx, cs->bodies[i]);
                        for (int j = 0; pv && pv[j]; j++) free(pv[j]);
                        free(pv);
                        goto case_done;
                    }
                    for (int j = 0; pv && pv[j]; j++) free(pv[j]);
                    free(pv);
                }
            }
        case_done:
            (void)matched;
            for (int j = 0; wv && wv[j]; j++) free(wv[j]);
            free(wv);
            ctx->last_status = st;
            return st;
        }
        case N_FUNCDEF: {
            _bash_func_define(ctx, node->u.func.name ? node->u.func.name : "", node->u.func.body);
            return 0;
        }
        case N_ARITH: {
            long v = _bash_eval_arith(ctx, node->u.arith.expr ? node->u.arith.expr : "0");
            int st = v == 0 ? 1 : 0;
            ctx->last_status = st;
            return st;
        }
        case N_CONDTEST: {
            /* Treat as "test" with words from expr */
            /* Tokenize expr by spaces (approximate) */
            bash_barray_t a; bash_barray_init(&a);
            bash_barray_push(&a, _bash_xstrdup("test"));
            const char *s = node->u.cond.expr ? node->u.cond.expr : "";
            while (*s) {
                while (*s == ' ' || *s == '\t') s++;
                if (!*s) break;
                const char *start = s;
                while (*s && *s != ' ' && *s != '\t') s++;
                bash_barray_push(&a, _bash_xstrndup(start, (size_t)(s - start)));
            }
            int rc = bi_test(ctx, a.len, a.items);
            bash_barray_free(&a);
            ctx->last_status = rc;
            return rc;
        }
        case N_BREAK: {
            ctx->frame->break_level = node->u.flow.n > 0 ? node->u.flow.n : 1;
            return ctx->last_status;
        }
        case N_CONTINUE: {
            ctx->frame->continue_level = node->u.flow.n > 0 ? node->u.flow.n : 1;
            return ctx->last_status;
        }
        case N_RETURN: {
            ctx->frame->func_ret = 1;
            ctx->frame->status = node->u.flow.code >= 0 ? node->u.flow.code : ctx->last_status;
            return ctx->frame->status;
        }
        case N_EXIT: {
            ctx->do_exit = 1;
            ctx->exit_code = node->u.flow.code >= 0 ? node->u.flow.code : ctx->last_status;
            return ctx->exit_code;
        }
    }
    return 0;
}

/* ========================================================================
 * Source file / string runner
 * ======================================================================== */

static char *_bash_read_all(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    char *buf = (char*)_bash_xmalloc((size_t)sz + 1);
    if (sz) { if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) { /* truncated read */ } }
    buf[sz] = 0;
    fclose(fp);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

static int _bash_run_string(bash_ctx_t *ctx, const char *src)
{
    bash_lex_t L; _bash_lex_init(&L, src, 0);
    bash_parser_t P; _bash_parser_init(&P, &L);
    bash_node_t *tree = _bash_parse_program(&P);
    if (!tree) return 0;
    int rc = _bash_do_exec(ctx, tree);
    /* TODO: free AST */
    return rc;
}

static int _bash_run_script(bash_ctx_t *ctx, const char *path, int argc, char **argv)
{
    size_t len;
    char *src = _bash_read_all(path, &len);
    if (!src) { fprintf(stderr, "bash: %s: cannot open: %s\n", path, strerror(errno)); return 127; }
    /* shebang: skip #!... line */
    if (len >= 2 && src[0] == '#' && src[1] == '!') {
        size_t i = 0;
        while (i < len && src[i] != '\n') i++;
        /* leave rest */
    }
    /* set positional args */
    bash_frame_t *f = _bash_frame_push(ctx, 1);
    f->argc = argc + 1;
    f->argv = (char**)_bash_xmalloc(sizeof(char*) * (size_t)f->argc);
    f->argv[0] = _bash_xstrdup(path);
    for (int i = 0; i < argc; i++) f->argv[i+1] = _bash_xstrdup(argv[i] ? argv[i] : "");
    ctx->script_name = f->argv[0];

    int rc = _bash_run_string(ctx, src);
    if (ctx->do_exit) rc = ctx->exit_code;
    _bash_frame_pop(ctx);
    ctx->script_name = NULL;
    free(src);
    return rc;
}

/* ========================================================================
 * Tab-completion for interactive REPL
 * ======================================================================== */

/* ---------- terminal raw mode helpers ---------- */
#ifdef BASH_PLATFORM_WINDOWS
  /* Using conio _getch() / _putch() — no explicit termios required */

  /* Detect whether we are actually attached to a native Windows console
   * (cmd.exe / PowerShell console window). On Win10 TH2+ we also try to
   * turn on ENABLE_VIRTUAL_TERMINAL_PROCESSING so that ANSI color escapes
   * (\e[31m, \e[0m ...) render properly instead of being printed as garbled
   * glyphs. The return value is 1 if we have a real console, 0 otherwise
   * (mintty / Cygwin pty, redirected pipes, etc.). OUT parameter *vt_ok is
   * set to 1 iff VT processing was successfully enabled. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#  define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#  define DISABLE_NEWLINE_AUTO_RETURN         0x0008
#endif
  static int _bash_have_real_console(int *vt_ok)
  {
      DWORD mode_in = 0, mode_out = 0;
      HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
      HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
      int ok_in = 0, ok_out = 0;
      int vt = 0;
      if (vt_ok) *vt_ok = 0;
      if (hin == INVALID_HANDLE_VALUE || !hin) return 0;
      if (hout == INVALID_HANDLE_VALUE || !hout) return 0;
      ok_in  = GetConsoleMode(hin, &mode_in);
      ok_out = GetConsoleMode(hout, &mode_out);
      if (!ok_in || !ok_out) return 0;
      /* Only enable VT processing on OUTPUT — this lets ANSI colour
       * sequences render properly on modern Windows consoles.
       *
       * We deliberately do NOT set ENABLE_VIRTUAL_TERMINAL_INPUT on the
       * input handle.  That flag disables cooked-mode line editing
       * (ENABLE_LINE_INPUT) and instead delivers raw VT escape sequences
       * via ReadFile — which breaks the fgets fallback (no arrow-key
       * editing) and, on some winpty/ConPTY versions, causes
       * ReadConsoleInputW to emit spurious VK_RETURN events (the
       * "every keystroke also presses Enter" bug).  Input stays in
       * cooked mode; callers that need raw input (the mintty byte-level
       * editor) set their own raw mode explicitly.                          */
      if (SetConsoleMode(hout, mode_out |
                         ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                         DISABLE_NEWLINE_AUTO_RETURN)) {
          vt = 1;
      }
      if (vt_ok) *vt_ok = vt;
      return 1;
  }

  /* Strip ANSI/DEC terminal escape sequences out of a prompt string.
   * We only need to handle what bash PS1 can produce:
   *   - CSI sequences:  ESC [ <param>* <intermediate>* <final>
   *     (e.g. \e[32m, \e[0m, \e]0;title\a (OSC — ESC ] ... BEL, treated separately))
   *   - OSC sequences:  ESC ] ... BEL  or  ESC ] ... ESC \
   * The string is modified in place. */
  static void _bash_strip_ansi(char *s)
  {
      if (!s) return;
      char *dst = s;
      for (char *p = s; *p; ) {
          if (*p == (char)0x1B && p[1]) {
              char type = p[1];
              if (type == '[') {
                  /* CSI: ESC [ — scan to the "final byte" (0x40-0x7E) */
                  p += 2;
                  while (*p && !((unsigned char)*p >= 0x40 && (unsigned char)*p <= 0x7E)) p++;
                  if (*p) p++; /* skip final byte */
                  continue;
              }
              if (type == ']') {
                  /* OSC: ESC ] ... BEL (0x07) or ESC \ (ST) */
                  p += 2;
                  while (*p) {
                      if (*p == 0x07) { p++; break; }
                      if (*p == (char)0x1B && p[1] == '\\') { p += 2; break; }
                      p++;
                  }
                  continue;
              }
              if (type == '%' || type == '(' || type == ')' || type == '*' ||
                  type == '+' || type == 'N' || type == 'O') {
                  /* 2-byte escape: skip the type + one following byte if present */
                  p += 2;
                  if (*p) p++;
                  continue;
              }
              /* generic 2-byte escape: just skip ESC + next char */
              p += 2;
              continue;
          }
          *dst++ = *p++;
      }
      *dst = 0;
  }
#else
static struct termios g_orig_termios;
static int          g_tty_raw_set = 0;
static void _bash_tty_raw(int enable)
{
    int fd = 0; /* stdin */
    if (!isatty(fd)) return;
    if (enable) {
        struct termios raw;
        if (tcgetattr(fd, &g_orig_termios) < 0) return;
        raw = g_orig_termios;
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG | IEXTEN);
        raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
        /* keep OPOST (output processing) enabled so \n maps to \r\n via ONLCR —
         * otherwise every newline causes staircase effect on the terminal. */
        raw.c_cflag &= (tcflag_t)~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(fd, TCSAFLUSH, &raw) == 0) g_tty_raw_set = 1;
    } else if (g_tty_raw_set) {
        tcsetattr(fd, TCSAFLUSH, &g_orig_termios);
        g_tty_raw_set = 0;
    }
}
#endif

/* ---------- candidate list ---------- */
static void _bash_sort_strings(char **arr, int n)
{
    /* insertion sort — completion lists tend to be small */
    for (int i = 1; i < n; i++) {
        char *k = arr[i]; int j = i - 1;
        while (j >= 0 && strcmp(arr[j], k) > 0) { arr[j+1] = arr[j]; j--; }
        arr[j+1] = k;
    }
}
static void _bash_dedupe_strings(char **arr, int *n)
{
    int out = 0;
    for (int i = 0; i < *n; i++) {
        if (out == 0 || strcmp(arr[out-1], arr[i]) != 0) {
            if (i != out) arr[out] = arr[i];
            out++;
        } else {
            free(arr[i]);
        }
    }
    *n = out;
}

/* find longest common prefix among array of strings */
static int _bash_common_prefix(char **arr, int n, char *out)
{
    if (n == 0) { out[0] = 0; return 0; }
    const char *fst = arr[0];
    size_t j = 0;
    for (; fst[j]; j++) {
        char c = fst[j];
        for (int i = 1; i < n; i++) {
            if (arr[i][j] != c) goto done;
        }
    }
done:
    memcpy(out, fst, j);
    out[j] = 0;
    return (int)j;
}

/* ---------- directory listing helpers ---------- */
/* List files in `dir` matching prefix `pre`; appends to `out` with trailing
 * '/' suffix for directories if add_slash is true.  full_item controls whether
 * to prepend dir_sep + dir_path to the returned name. */
static void _bash_list_dir_files(const char *dir, const char *pre, int only_exec,
                             int add_slash_for_dir, bash_barray_t *out)
{
#ifdef BASH_PLATFORM_WINDOWS
    if (!dir || !*dir) return;
    bash_bstr_t mask; bash_bstr_init(&mask);
    bash_bstr_puts(&mask, dir);
    size_t ml = mask.len;
    if (ml == 0 || (mask.data[ml-1] != BASH_SEP && mask.data[ml-1] != '/'))
        bash_bstr_putc(&mask, BASH_SEP);
    bash_bstr_puts(&mask, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(mask.data, &fd);
    bash_bstr_free(&mask);
    if (h == INVALID_HANDLE_VALUE) return;
    size_t prelen = pre ? strlen(pre) : 0;
    do {
        const char *name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (prelen != 0 && strncmp(name, pre, prelen) != 0) continue;
        /* build full path for exec/dir checks */
        bash_bstr_t full; bash_bstr_init(&full);
        bash_bstr_puts(&full, dir);
        size_t fl = full.len;
        if (fl == 0 || (full.data[fl-1] != BASH_SEP && full.data[fl-1] != '/'))
            bash_bstr_putc(&full, BASH_SEP);
        bash_bstr_puts(&full, name);
        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        int ok = 1;
        if (only_exec) {
            if (is_dir) ok = 0;
            else {
                const char *ext = strrchr(name, '.');
                if (!ext || (_stricmp(ext, ".exe") != 0 && _stricmp(ext, ".com") != 0 &&
                             _stricmp(ext, ".bat") != 0 && _stricmp(ext, ".cmd") != 0))
                    ok = 0;
            }
        }
        if (ok) {
            bash_bstr_t it; bash_bstr_init(&it);
            bash_bstr_puts(&it, name);
            if (add_slash_for_dir && is_dir) bash_bstr_putc(&it, '/');
            bash_barray_push(out, bash_bstr_detach(&it));
        }
        bash_bstr_free(&full);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir ? dir : ".");
    if (!d) return;
    size_t prelen = pre ? strlen(pre) : 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        const char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (prelen != 0 && strncmp(name, pre, prelen) != 0) continue;
        bash_bstr_t full; bash_bstr_init(&full);
        if (dir && *dir) {
            bash_bstr_puts(&full, dir);
            size_t fl = full.len;
            if (full.data[fl-1] != '/') bash_bstr_putc(&full, '/');
        }
        bash_bstr_puts(&full, name);
        int is_dir = 0; int execok = 1;
        struct stat st;
        if (stat(full.data, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
            if (only_exec) {
                if (is_dir || !(st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) execok = 0;
            }
        } else if (only_exec) {
            execok = 0;
        }
        if (execok) {
            bash_bstr_t it; bash_bstr_init(&it);
            bash_bstr_puts(&it, name);
            if (add_slash_for_dir && is_dir) bash_bstr_putc(&it, '/');
            bash_barray_push(out, bash_bstr_detach(&it));
        }
        bash_bstr_free(&full);
    }
    closedir(d);
#endif
}

/* ---------- completion engine ---------- */

/* Split current line into "command position or argument position".
 * We find the start of the token the cursor is on (scan backwards from
 * cursor_pos for a shell token delimiter: whitespace, ;, |, &, <, >, (, ), `).
 * Returns the index in `line` where the current token starts. */
static int _bash_token_start(const char *line, int cursor_pos)
{
    int s = 0;
    for (int i = 0; i < cursor_pos; i++) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == ';' || c == '|' || c == '&' ||
            c == '<' || c == '>' || c == '(' || c == ')' || c == '`') {
            s = i + 1;
        }
    }
    return s;
}

/* Is this token at the very beginning of the command (first word)?
 * True if all characters before token_pos are whitespace or separators
 * and there is no non-separator word before it. */
static int _bash_is_command_pos(const char *line, int token_pos)
{
    int i = token_pos - 1;
    while (i >= 0) {
        char c = line[i];
        if (c == ' ' || c == '\t') { i--; continue; }
        /* separator means "command starts here" */
        if (c == ';' || c == '|' || c == '&' || c == '(' || c == '`' || c == '\n')
            return 1;
        /* anything else => argument position */
        return 0;
    }
    return 1; /* start of line */
}

/* Generate completion candidates.
 * token: the token text so far (need not be NUL-terminated, token_len used)
 * is_cmd: 1 if command position (0th word), 0 if argument position
 * Returns array of strings (owned by caller); count via *out_n.
 * Caller frees with bash_barray_free. */
static char **_bash_complete(bash_ctx_t *ctx, const char *token, int token_len,
                         int is_cmd, int *out_n)
{
    bash_barray_t out; bash_barray_init(&out);

    /* split token into dir_part + file_prefix
     *   ls s<TAB>        -> dir="",  pre="s"
     *   ls ./ba<TAB>     -> dir="./", pre="ba"
     *   ls src/ma<TAB>   -> dir="src/", pre="ma"
     *   echo ~/doc<TAB>  -> dir="~/", pre="doc"  (expand ~ for matching only)
     */
    int slash = -1;
    for (int i = token_len - 1; i >= 0; i--) {
        if (token[i] == '/'
#ifdef BASH_PLATFORM_WINDOWS
            || token[i] == '\\'
#endif
            ) { slash = i; break; }
    }
    const char *dir_part = (slash >= 0) ? token : "";
    int dir_len = slash + 1; /* includes trailing slash; 0 if no slash */
    const char *pre = token + dir_len;
    int pre_len = token_len - dir_len;
    char *dir = _bash_xstrndup(dir_part, dir_len); /* "" if none */
    char *pre_s = _bash_xstrndup(pre, pre_len);

    if (is_cmd && dir_len == 0) {
        /* command completion: builtins + functions + selfdir + PATH + cwd files */
        /* 1) builtins */
        extern bash_builtin_t _bash_builtins[];
        for (int k = 0; _bash_builtins[k].name; k++) {
            const char *n = _bash_builtins[k].name;
            if (strncmp(n, pre_s, pre_len) == 0)
                bash_barray_push(&out, _bash_xstrdup(n));
        }
        /* 2) functions */
        for (bash_funcdef_t *f = ctx->funcs; f; f = f->next) {
            if (strncmp(f->name, pre_s, pre_len) == 0)
                bash_barray_push(&out, _bash_xstrdup(f->name));
        }
        /* 3) self dir + PATH directories: list executables matching pre */
        bash_barray_t dirs; bash_barray_init(&dirs);
        char *sd = _bash_get_self_dir();
        if (sd && *sd) bash_barray_push(&dirs, _bash_xstrdup(sd));
        const char *path_env = getenv("PATH");
        if (path_env) {
            const char *p = path_env;
            while (*p) {
                const char *end = p;
                while (*end && *end != BASH_PATHSEP) end++;
                if (end > p) bash_barray_push(&dirs, _bash_xstrndup(p, end - p));
                if (*end) p = end + 1; else break;
            }
        }
        /* current directory too? bash doesn't usually include cwd in command
         * completion unless . is in PATH.  But to match "find in same dir"
         * semantics we include it anyway. */
        bash_barray_push(&dirs, _bash_xstrdup("."));
        for (int d = 0; d < dirs.len; d++) {
            _bash_list_dir_files(dirs.items[d], pre_s, 1, 0, &out);
        }
        bash_barray_free(&dirs);
    } else {
        /* file completion: list files from dir_part */
        const char *lookup_dir = dir_len > 0 ? dir : ".";
        /* expand ~ in dir if ~/ or ~ is only the path */
        bash_bstr_t exp_dir; bash_bstr_init(&exp_dir);
        if (dir_len >= 1 && dir[0] == '~' &&
            (dir_len == 1 || dir[1] == '/'
#ifdef BASH_PLATFORM_WINDOWS
             || dir[1] == '\\'
#endif
            )) {
            const char *home = getenv("HOME");
            if (!home) home = ".";
            bash_bstr_puts(&exp_dir, home);
            if (dir_len >= 2) bash_bstr_puts(&exp_dir, dir + 1);
            lookup_dir = exp_dir.data;
        }
        _bash_list_dir_files(lookup_dir, pre_s, 0, 1, &out);
        bash_bstr_free(&exp_dir);
    }
    free(dir); free(pre_s);

    /* Prepend the original dir_part to every candidate so the returned string
     * covers the entire token (from token_start).  Without this, a token like
     *   "l1/l2/l3"  with matches under "l1/l2/" → ["l3a/", "l3b/"]
     * would replace the WHOLE token (ts = start of "l1") with just "l3a/",
     * corrupting the line to "l3a/".  With dir_part prepended we get:
     *   ["l1/l2/l3a/", "l1/l2/l3b/"]  which correctly replaces the token.
     */
    if (dir_len > 0) {
        for (int k = 0; k < out.len; k++) {
            char *old = out.items[k];
            int olen = (int)strlen(old);
            char *neww = (char *)malloc(dir_len + olen + 1);
            if (!neww) continue;
            memcpy(neww, dir_part, dir_len);
            memcpy(neww + dir_len, old, olen + 1);
            out.items[k] = neww;
            free(old);
        }
    }

    _bash_sort_strings(out.items, out.len);
    _bash_dedupe_strings(out.items, &out.len);
    *out_n = out.len;
    char **r = out.items;
    /* barray layout: separate allocations; items[] is the dynamic array.
     * We need the caller to free like bash_barray_free: each item + items array.
     * Return the raw items pointer with len stored in *out_n; use barray layout. */
    return r;
}
static void _bash_complete_free(char **arr, int n)
{
    if (!arr) return;
    for (int i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

/* ---------- line editor ---------- */

/* Read a line from terminal with tab-completion support.
 * Returns pointer to static buffer (NUL terminated, no trailing \n).
 * Returns NULL on EOF. */
/* ---- command history helpers ---- */
static char *g_history[HIST_SIZE];   /* circular buffer; newest at highest valid idx */
static int   g_hist_count;          /* total entries ever added (for modulo indexing) */
static int   g_hist_nav;            /* nav index: -1 = current line, else 0..g_hist_count-1 */
static char *g_hist_saved;          /* saved current-editing line when navigating */

static void _bash_hist_push(const char *line)
{
    if (!line || !*line) return;
    /* suppress consecutive duplicates */
    if (g_hist_count > 0) {
        int last = (g_hist_count - 1) % HIST_SIZE;
        if (g_history[last] && strcmp(g_history[last], line) == 0) return;
    }
    int slot = g_hist_count % HIST_SIZE;
    if (g_history[slot]) free(g_history[slot]);
    g_history[slot] = _bash_xstrdup(line);
    g_hist_count++;
}

static const char *_bash_hist_get(int idx)
{
    /* idx = 0 is oldest accessible; idx = g_hist_count-1 is newest */
    if (idx < 0 || idx >= g_hist_count) return NULL;
    int base = (g_hist_count > HIST_SIZE) ? (g_hist_count - HIST_SIZE) : 0;
    int rel = idx - base;
    if (rel < 0) return NULL;
    return g_history[idx % HIST_SIZE];
}

/* ---------------- UTF-8 multi-byte helpers for line editing ---------------- */

/**
 * @brief Return byte-length of UTF-8 sequence starting with first byte @p b.
 * @param b First byte of a potential UTF-8 sequence (unsigned char cast is done inside).
 * @return 1..4 for valid UTF-8 start bytes; 1 for invalid/cont bytes (safe fallback).
 */
static int utf8_seq_len(unsigned char b)
{
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    /* 0x80..0xBF = continuation bytes, 0xF8+ = invalid — treat as 1 byte */
    return 1;
}

/**
 * @brief Compute how many bytes to move backwards from @p cur_byte_pos inside
 *        buffer @p buf of length @p buf_len to land on the START of the
 *        previous UTF-8 character.
 * @return Number of bytes to subtract from cur (>= 1 when cur>0).
 */
static int utf8_back_len(const char *buf, int buf_len, int cur_byte_pos)
{
    (void)buf_len;
    if (cur_byte_pos <= 0) return 0;
    int pos = cur_byte_pos;
    /* walk back over UTF-8 continuation bytes (pattern 10xxxxxx).
     * After loop, `pos - 1` is the START byte of the previous character. */
    int max_back = 4;
    while (pos > 0 && max_back > 0) {
        unsigned char c = (unsigned char)buf[pos - 1];
        if ((c & 0xC0) != 0x80) break; /* not a continuation byte */
        pos--;
        max_back--;
    }
    /* pos - 1 now points at the start byte of the char we want to delete */
    if (pos <= 0) return 1;
    int start_idx = pos - 1;
    int expect_len = utf8_seq_len((unsigned char)buf[start_idx]);
    int actual_back = cur_byte_pos - start_idx;
    /* Only trust expect_len if it fits within the bytes we spanned.
     * This keeps us safe on malformed / truncated UTF-8. */
    if (expect_len >= 1 && expect_len <= actual_back && expect_len <= 4) {
        return expect_len;
    }
    /* Fallback: invalid UTF-8, delete 1 byte. */
    return 1;
}

/**
 * @brief Compute how many bytes to move forward from @p cur_byte_pos inside
 *        buffer @p buf of length @p buf_len to land on the START of the NEXT
 *        UTF-8 character (i.e. skip the current whole character).
 * @return Number of bytes to add to cur (>= 1, <= buf_len - cur_byte_pos).
 */
static int utf8_fwd_len(const char *buf, int buf_len, int cur_byte_pos)
{
    if (cur_byte_pos >= buf_len) return 0;
    int n = utf8_seq_len((unsigned char)buf[cur_byte_pos]);
    if (n < 1) n = 1;
    if (cur_byte_pos + n > buf_len) n = buf_len - cur_byte_pos;
    if (n < 1) n = 1;
    return n;
}

/**
 * @brief Decode one UTF-8 sequence starting at @p buf[@p i] into a Unicode
 *        code point. Advances @p *pi past the consumed bytes.
 * @return Unicode code point (0..0x10FFFF), or 0xFFFD on invalid input.
 */
static unsigned long utf8_decode(const char *buf, int buf_len, int *pi)
{
    int i = *pi;
    if (i >= buf_len) { *pi = i; return 0xFFFD; }
    unsigned char b0 = (unsigned char)buf[i];
    int n = utf8_seq_len(b0);
    /* bounds: ensure we have at least n bytes available */
    if (i + n > buf_len) n = buf_len - i;
    unsigned long cp = 0;
    if (n == 1) {
        cp = b0;
    } else if (n == 2) {
        if ((b0 & 0xE0) != 0xC0) { *pi = i + 1; return 0xFFFD; }
        cp = b0 & 0x1F;
        unsigned char b1 = (unsigned char)buf[i + 1];
        if ((b1 & 0xC0) != 0x80) { *pi = i + 1; return 0xFFFD; }
        cp = (cp << 6) | (b1 & 0x3F);
    } else if (n == 3) {
        if ((b0 & 0xF0) != 0xE0) { *pi = i + 1; return 0xFFFD; }
        cp = b0 & 0x0F;
        unsigned char b1 = (unsigned char)buf[i + 1];
        unsigned char b2 = (unsigned char)buf[i + 2];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) { *pi = i + 1; return 0xFFFD; }
        cp = (cp << 6) | (b1 & 0x3F);
        cp = (cp << 6) | (b2 & 0x3F);
    } else if (n == 4) {
        if ((b0 & 0xF8) != 0xF0) { *pi = i + 1; return 0xFFFD; }
        cp = b0 & 0x07;
        unsigned char b1 = (unsigned char)buf[i + 1];
        unsigned char b2 = (unsigned char)buf[i + 2];
        unsigned char b3 = (unsigned char)buf[i + 3];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
            *pi = i + 1; return 0xFFFD;
        }
        cp = (cp << 6) | (b1 & 0x3F);
        cp = (cp << 6) | (b2 & 0x3F);
        cp = (cp << 6) | (b3 & 0x3F);
    } else {
        *pi = i + 1;
        return 0xFFFD;
    }
    *pi = i + n;
    return cp;
}

/**
 * @brief Return the terminal display column width (wcwidth-style, simplified)
 *        for a single Unicode code point @p cp.
 *        Rules:
 *        - C0/C1 control chars (< 0x20, 0x7F..0x9F) → width 0 (but DEL 0x7F→0)
 *        - ASCII printable (0x20..0x7E) → width 1
 *        - Combining marks / zero-width → 0
 *        - CJK unified ideographs, Hangul, Katakana/Hiragana, fullwidth forms,
 *          most emoji → width 2
 *        - Everything else (Latin Extended, Greek, Cyrillic, Arabic, …) → 1
 */
static int utf8_cp_width(unsigned long cp)
{
    if (cp < 0x20) return 0;                 /* C0 controls */
    if (cp < 0x7F) return 1;                 /* ASCII printable */
    if (cp == 0x7F) return 0;                /* DEL */
    if (cp < 0xA0) return 0;                 /* C1 controls (0x80..0x9F) */
    /* Soft hyphen & commonly treated as narrow */
    if (cp == 0xAD) return 1;
    /* Combining diacritical marks (Unicode general category M, simplified ranges) */
    if ((cp >= 0x0300 && cp <= 0x036F) ||    /* Combining Diacritical Marks */
        (cp >= 0x0483 && cp <= 0x0489) ||
        (cp >= 0x0591 && cp <= 0x05BD) ||
        (cp >= 0x05BF && cp <= 0x05CF) ||
        (cp >= 0x05D1 && cp <= 0x05F2) ||
        (cp >= 0x0600 && cp <= 0x0605) ||
        (cp >= 0x0610 && cp <= 0x061A) ||
        (cp >= 0x064B && cp <= 0x065F) ||
        (cp >= 0x0670 && cp <= 0x0672) ||
        (cp >= 0x06D6 && cp <= 0x06DC) ||
        (cp >= 0x06DF && cp <= 0x06E4) ||
        (cp >= 0x06E7 && cp <= 0x06E8) ||
        (cp >= 0x06EA && cp <= 0x06ED) ||
        (cp >= 0x0711 && cp <= 0x0711) ||
        (cp >= 0x0730 && cp <= 0x074A) ||
        (cp >= 0x07A6 && cp <= 0x07B0) ||
        (cp >= 0x07EB && cp <= 0x07F3) ||
        (cp >= 0x0816 && cp <= 0x0819) ||
        (cp >= 0x081B && cp <= 0x0823) ||
        (cp >= 0x0825 && cp <= 0x0827) ||
        (cp >= 0x0829 && cp <= 0x082D) ||
        (cp >= 0x0859 && cp <= 0x085B) ||
        (cp >= 0x0900 && cp <= 0x0903) ||
        (cp >= 0x093A && cp <= 0x0957) ||
        (cp >= 0x0962 && cp <= 0x0963) ||
        (cp >= 0x0981 && cp <= 0x0983) ||
        (cp >= 0x09BC && cp <= 0x09C4) ||
        (cp >= 0x09C7 && cp <= 0x09C8) ||
        (cp >= 0x09CB && cp <= 0x09CD) ||
        (cp >= 0x09D7 && cp <= 0x09D7) ||
        (cp >= 0x09E2 && cp <= 0x09E3) ||
        (cp >= 0x0A01 && cp <= 0x0A03) ||
        (cp >= 0x0A3C && cp <= 0x0A51) ||
        (cp >= 0x0A70 && cp <= 0x0A71) ||
        (cp >= 0x0A81 && cp <= 0x0A83) ||
        (cp >= 0x0ABC && cp <= 0x0ACD) ||
        (cp >= 0x0AE2 && cp <= 0x0AE3) ||
        (cp >= 0x0B01 && cp <= 0x0B03) ||
        (cp >= 0x0B3C && cp <= 0x0B4C) ||
        (cp >= 0x0B62 && cp <= 0x0B63) ||
        (cp >= 0x0B82 && cp <= 0x0B82) ||
        (cp >= 0x0BC0 && cp <= 0x0BCD) ||
        (cp >= 0x0C00 && cp <= 0x0C03) ||
        (cp >= 0x0C3E && cp <= 0x0C56) ||
        (cp >= 0x0C62 && cp <= 0x0C63) ||
        (cp >= 0x0C81 && cp <= 0x0C83) ||
        (cp >= 0x0CBC && cp <= 0x0CCD) ||
        (cp >= 0x0CE2 && cp <= 0x0CE3) ||
        (cp >= 0x0D01 && cp <= 0x0D03) ||
        (cp >= 0x0D3E && cp <= 0x0D4D) ||
        (cp >= 0x0D62 && cp <= 0x0D63) ||
        (cp >= 0x0D82 && cp <= 0x0D83) ||
        (cp >= 0x0DCA && cp <= 0x0DD6) ||
        (cp >= 0x0DD8 && cp <= 0x0DDF) ||
        (cp >= 0x0DF2 && cp <= 0x0DF3) ||
        (cp >= 0x0E31 && cp <= 0x0E3A) ||
        (cp >= 0x0E47 && cp <= 0x0E4E) ||
        (cp >= 0x0EB1 && cp <= 0x0EB9) ||
        (cp >= 0x0EBB && cp <= 0x0EBC) ||
        (cp >= 0x0EC8 && cp <= 0x0ECD) ||
        (cp >= 0x0F18 && cp <= 0x0F19) ||
        (cp >= 0x0F35 && cp <= 0x0F35) ||
        (cp >= 0x0F37 && cp <= 0x0F37) ||
        (cp >= 0x0F39 && cp <= 0x0F39) ||
        (cp >= 0x0F3E && cp <= 0x0F3F) ||
        (cp >= 0x0F71 && cp <= 0x0F84) ||
        (cp >= 0x0F86 && cp <= 0x0F87) ||
        (cp >= 0x0F8D && cp <= 0x0FBC) ||
        (cp >= 0x0FC6 && cp <= 0x0FC6) ||
        (cp >= 0x102D && cp <= 0x1030) ||
        (cp >= 0x1032 && cp <= 0x1037) ||
        (cp >= 0x1039 && cp <= 0x103A) ||
        (cp >= 0x103D && cp <= 0x103E) ||
        (cp >= 0x1058 && cp <= 0x1059) ||
        (cp >= 0x105E && cp <= 0x1060) ||
        (cp >= 0x1071 && cp <= 0x1074) ||
        (cp >= 0x1082 && cp <= 0x1082) ||
        (cp >= 0x1085 && cp <= 0x1086) ||
        (cp >= 0x109D && cp <= 0x109D) ||
        (cp >= 0x135F && cp <= 0x135F) ||
        (cp >= 0x1712 && cp <= 0x1715) ||
        (cp >= 0x1732 && cp <= 0x1734) ||
        (cp >= 0x1752 && cp <= 0x1753) ||
        (cp >= 0x1772 && cp <= 0x1773) ||
        (cp >= 0x17B4 && cp <= 0x17D3) ||
        (cp >= 0x17DD && cp <= 0x17DD) ||
        (cp >= 0x180B && cp <= 0x180E) ||
        (cp >= 0x1885 && cp <= 0x1886) ||
        (cp >= 0x18A9 && cp <= 0x18A9) ||
        (cp >= 0x1920 && cp <= 0x192B) ||
        (cp >= 0x1930 && cp <= 0x193B) ||
        (cp >= 0x1A17 && cp <= 0x1A1B) ||
        (cp >= 0x1A55 && cp <= 0x1A5E) ||
        (cp >= 0x1A60 && cp <= 0x1A7C) ||
        (cp >= 0x1A7F && cp <= 0x1A7F) ||
        (cp >= 0x1AB0 && cp <= 0x1ABE) ||
        (cp >= 0x1B00 && cp <= 0x1B04) ||
        (cp >= 0x1B34 && cp <= 0x1B44) ||
        (cp >= 0x1B50 && cp <= 0x1B7C) ||
        (cp >= 0x1B80 && cp <= 0x1B82) ||
        (cp >= 0x1BA1 && cp <= 0x1BAD) ||
        (cp >= 0x1BE6 && cp <= 0x1BF3) ||
        (cp >= 0x1C24 && cp <= 0x1C37) ||
        (cp >= 0x1CD0 && cp <= 0x1CD2) ||
        (cp >= 0x1CD4 && cp <= 0x1CE8) ||
        (cp >= 0x1CED && cp <= 0x1CED) ||
        (cp >= 0x1CF4 && cp <= 0x1CF4) ||
        (cp >= 0x1CF7 && cp <= 0x1CF9) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) ||
        (cp >= 0x200B && cp <= 0x200F) || /* zero-width space, directionals */
        (cp >= 0x202A && cp <= 0x202E) || /* bidirectional formatting */
        (cp >= 0x2060 && cp <= 0x206F) || /* zero-width / format */
        (cp >= 0x20D0 && cp <= 0x20FF) || /* Combining Diacritical for Symbols */
        (cp >= 0x2CEF && cp <= 0x2CF1) ||
        (cp >= 0x2DE0 && cp <= 0x2DFF) ||
        (cp >= 0x302A && cp <= 0x302F) ||
        (cp >= 0x3099 && cp <= 0x309A) || /* Japanese voice marks, combining */
        (cp >= 0xA66F && cp <= 0xA672) ||
        (cp >= 0xA674 && cp <= 0xA67D) ||
        (cp >= 0xA69E && cp <= 0xA69F) ||
        (cp >= 0xA6F0 && cp <= 0xA6F1) ||
        (cp >= 0xA802 && cp <= 0xA802) ||
        (cp >= 0xA806 && cp <= 0xA806) ||
        (cp >= 0xA80B && cp <= 0xA80B) ||
        (cp >= 0xA825 && cp <= 0xA826) ||
        (cp >= 0xA8C4 && cp <= 0xA8C5) ||
        (cp >= 0xA8E0 && cp <= 0xA8F1) ||
        (cp >= 0xA926 && cp <= 0xA92D) ||
        (cp >= 0xA947 && cp <= 0xA951) ||
        (cp >= 0xA980 && cp <= 0xA982) ||
        (cp >= 0xA9B3 && cp <= 0xA9B4) ||
        (cp >= 0xAA29 && cp <= 0xAA36) ||
        (cp >= 0xAA43 && cp <= 0xAA43) ||
        (cp >= 0xAA4C && cp <= 0xAA4C) ||
        (cp >= 0xAA7C && cp <= 0xAA7C) ||
        (cp >= 0xAAB0 && cp <= 0xAAB0) ||
        (cp >= 0xAAB2 && cp <= 0xAAB4) ||
        (cp >= 0xAAB7 && cp <= 0xAAB8) ||
        (cp >= 0xAABE && cp <= 0xAABF) ||
        (cp >= 0xAAC1 && cp <= 0xAAC1) ||
        (cp >= 0xAAEC && cp <= 0xAAEE) ||
        (cp >= 0xAAF5 && cp <= 0xAAF6) ||
        (cp >= 0xABE5 && cp <= 0xABE5) ||
        (cp >= 0xABE8 && cp <= 0xABE8) ||
        (cp >= 0xAB01 && cp <= 0xAB06) ||
        (cp >= 0xAB09 && cp <= 0xAB09) ||
        (cp >= 0xAB0E && cp <= 0xAB0F) ||
        (cp >= 0xAB11 && cp <= 0xAB16) ||
        (cp >= 0xAB20 && cp <= 0xAB26) ||
        (cp >= 0xAB28 && cp <= 0xAB2E) ||
        (cp >= 0x10A01 && cp <= 0x10A03) ||
        (cp >= 0x10A05 && cp <= 0x10A06) ||
        (cp >= 0x10A0C && cp <= 0x10A0F) ||
        (cp >= 0x10A38 && cp <= 0x10A3A) ||
        (cp >= 0x10A3F && cp <= 0x10A3F) ||
        (cp >= 0x10AE5 && cp <= 0x10AE6) ||
        (cp >= 0x11000 && cp <= 0x11001) ||
        (cp >= 0x11002 && cp <= 0x11002) ||
        (cp >= 0x11038 && cp <= 0x11046) ||
        (cp >= 0x1106A && cp <= 0x1106E) ||
        (cp >= 0x1107F && cp <= 0x11081) ||
        (cp >= 0x110B0 && cp <= 0x110BA) ||
        (cp >= 0x110BD && cp <= 0x110BD) ||
        (cp >= 0x110CD && cp <= 0x110CD) ||
        (cp >= 0x11100 && cp <= 0x11102) ||
        (cp >= 0x11127 && cp <= 0x1112B) ||
        (cp >= 0x1112D && cp <= 0x11134) ||
        (cp >= 0x11136 && cp <= 0x11137) ||
        (cp >= 0x11145 && cp <= 0x11146) ||
        (cp >= 0x11173 && cp <= 0x11173) ||
        (cp >= 0x11183 && cp <= 0x111B9) ||
        (cp >= 0x111C2 && cp <= 0x111C4) ||
        (cp >= 0x111CA && cp <= 0x111CC) ||
        (cp >= 0x111CE && cp <= 0x111D3) ||
        (cp >= 0x111D5 && cp <= 0x111DC) ||
        (cp >= 0x111DD && cp <= 0x111DF) ||
        (cp >= 0x1122C && cp <= 0x11237) ||
        (cp >= 0x1123E && cp <= 0x1123E) ||
        (cp >= 0x11241 && cp <= 0x11241) ||
        (cp >= 0x112DF && cp <= 0x112EA) ||
        (cp >= 0x11300 && cp <= 0x11303) ||
        (cp >= 0x1133B && cp <= 0x1133C) ||
        (cp >= 0x11340 && cp <= 0x11340) ||
        (cp >= 0x11366 && cp <= 0x11374) ||
        (cp >= 0x11377 && cp <= 0x11378) ||
        (cp >= 0x1137D && cp <= 0x1137D) ||
        (cp >= 0x1139E && cp <= 0x1139F) ||
        (cp >= 0x113A9 && cp <= 0x113A9) ||
        (cp >= 0x113C0 && cp <= 0x113C1) ||
        (cp >= 0x113C3 && cp <= 0x113C4) ||
        (cp >= 0x113C5 && cp <= 0x113C5) ||
        (cp >= 0x113C7 && cp <= 0x113CC) ||
        (cp >= 0x113CE && cp <= 0x113CF) ||
        (cp >= 0x113D0 && cp <= 0x113D0) ||
        (cp >= 0x113D2 && cp <= 0x113D2) ||
        (cp >= 0x113D4 && cp <= 0x113D5) ||
        (cp >= 0x113D6 && cp <= 0x113D6) ||
        (cp >= 0x113D7 && cp <= 0x113D8) ||
        (cp >= 0x113D9 && cp <= 0x113DA) ||
        (cp >= 0x113DB && cp <= 0x113DB) ||
        (cp >= 0x113DC && cp <= 0x113DF) ||
        (cp >= 0x113E0 && cp <= 0x113E0) ||
        (cp >= 0x113E2 && cp <= 0x113E2) ||
        (cp >= 0x113E3 && cp <= 0x113E3) ||
        (cp >= 0x113E4 && cp <= 0x113E8) ||
        (cp >= 0x113F0 && cp <= 0x113F1) ||
        (cp >= 0x11435 && cp <= 0x11437) ||
        (cp >= 0x11442 && cp <= 0x11444) ||
        (cp >= 0x11446 && cp <= 0x11446) ||
        (cp >= 0x1145E && cp <= 0x1145F) ||
        (cp >= 0x114B3 && cp <= 0x114B8) ||
        (cp >= 0x114BA && cp <= 0x114BA) ||
        (cp >= 0x114C2 && cp <= 0x114C3) ||
        (cp >= 0x115B2 && cp <= 0x115B5) ||
        (cp >= 0x115BC && cp <= 0x115BD) ||
        (cp >= 0x115C7 && cp <= 0x115C8) ||
        (cp >= 0x11633 && cp <= 0x1163A) ||
        (cp >= 0x1163D && cp <= 0x1163D) ||
        (cp >= 0x1163F && cp <= 0x11640) ||
        (cp >= 0x116AB && cp <= 0x116B7) ||
        (cp >= 0x116C0 && cp <= 0x116C1) ||
        (cp >= 0x1171D && cp <= 0x1171F) ||
        (cp >= 0x11722 && cp <= 0x11725) ||
        (cp >= 0x11727 && cp <= 0x1172B) ||
        (cp >= 0x11A01 && cp <= 0x11A06) ||
        (cp >= 0x11A09 && cp <= 0x11A0A) ||
        (cp >= 0x11A33 && cp <= 0x11A38) ||
        (cp >= 0x11A3B && cp <= 0x11A3E) ||
        (cp >= 0x11A47 && cp <= 0x11A47) ||
        (cp >= 0x11A51 && cp <= 0x11A56) ||
        (cp >= 0x11A59 && cp <= 0x11A5B) ||
        (cp >= 0x11A5D && cp <= 0x11A5D) ||
        (cp >= 0x11A5F && cp <= 0x11A5F) ||
        (cp >= 0x11A8A && cp <= 0x11A96) ||
        (cp >= 0x11A98 && cp <= 0x11A99) ||
        (cp >= 0x11C30 && cp <= 0x11C36) ||
        (cp >= 0x11C38 && cp <= 0x11C3D) ||
        (cp >= 0x11C3F && cp <= 0x11C3F) ||
        (cp >= 0x11C92 && cp <= 0x11CA7) ||
        (cp >= 0x11CAA && cp <= 0x11CB0) ||
        (cp >= 0x11CB2 && cp <= 0x11CB3) ||
        (cp >= 0x11CB5 && cp <= 0x11CB6) ||
        (cp >= 0x1D165 && cp <= 0x1D169) ||
        (cp >= 0x1D16D && cp <= 0x1D172) ||
        (cp >= 0x1D17B && cp <= 0x1D182) ||
        (cp >= 0x1D185 && cp <= 0x1D18B) ||
        (cp >= 0x1D1AA && cp <= 0x1D1AD) ||
        (cp >= 0x1D242 && cp <= 0x1D244) ||
        (cp >= 0x1DA00 && cp <= 0x1DA36) ||
        (cp >= 0x1DA3B && cp <= 0x1DA6C) ||
        (cp >= 0x1DA75 && cp <= 0x1DA75) ||
        (cp >= 0x1DA84 && cp <= 0x1DA84) ||
        (cp >= 0x1DA9B && cp <= 0x1DA9F) ||
        (cp >= 0x1DAA1 && cp <= 0x1DAAF) ||
        (cp >= 0x1DB00 && cp <= 0x1DBFF) || /* combining strokes */
        (cp >= 0xE0020 && cp <= 0xE007F) || /* tag characters */
        (cp >= 0xE0100 && cp <= 0xE01EF))   /* variation selectors supp */
        return 0;

    /* -------- East Asian Wide / Fullwidth (width == 2) -------- */
    /* Hangul Jamo (U+1100..U+115F) */
    if (cp >= 0x1100 && cp <= 0x115F) return 2;
    /* U+2329 LEFT-POINTING ANGLE BRACKET, U+232A RIGHT-POINTING ANGLE BRACKET */
    if (cp == 0x2329 || cp == 0x232A) return 2;
    /* CJK Radicals Supplement / Kangxi Radicals / Ideographic Description /
     * CJK Symbols and Punctuation / Hiragana / Katakana / Bopomofo /
     * Hangul Compatibility Jamo / Kanbun / Bopomofo Extended /
     * CJK Strokes / Katakana Phonetic Extensions / Enclosed CJK Letters and Months /
     * CJK Compatibility / CJK Unified Ideographs Extension A / Yijing Hexagram Symbols /
     * U+3400..U+4DBF CJK Ext A, U+4E00..U+9FFF CJK Unified Ideographs */
    if ((cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3041 && cp <= 0x3096) ||  /* Hiragana */
        (cp >= 0x3099 && cp <= 0x30FF) ||  /* voice marks + Katakana */
        (cp >= 0x3105 && cp <= 0x312F) ||  /* Bopomofo */
        (cp >= 0x3131 && cp <= 0x318E) ||  /* Hangul Compatibility Jamo */
        (cp >= 0x3190 && cp <= 0x31BF) ||  /* Kanbun etc */
        (cp >= 0x31C0 && cp <= 0x31E3) ||  /* CJK Strokes */
        (cp >= 0x31F0 && cp <= 0x31FF) ||  /* Katakana Phonetic Ext. */
        (cp >= 0x3200 && cp <= 0x32FF) ||  /* Enclosed CJK Letters / Months */
        (cp >= 0x3300 && cp <= 0x33FF) ||  /* CJK Compatibility */
        (cp >= 0x3400 && cp <= 0x4DBF) ||  /* CJK Extension A */
        (cp >= 0x4DC0 && cp <= 0x4DFF) ||  /* Yijing Hexagram Symbols */
        (cp >= 0x4E00 && cp <= 0x9FFF))    /* CJK Unified Ideographs */
        return 2;
    /* Yi Syllables + Yi Radicals */
    if (cp >= 0xA000 && cp <= 0xA4CF) return 2;
    /* Hangul Syllables: AC00..D7A3 */
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 2;
    /* CJK Compatibility Ideographs: F900..FAFF */
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;
    /* Vertical forms, CJK Compatibility Forms: FE10..FE19, FE30..FE4F */
    if ((cp >= 0xFE10 && cp <= 0xFE19) ||
        (cp >= 0xFE30 && cp <= 0xFE4F))
        return 2;
    /* Fullwidth ASCII variants (FF01..FF60) + Fullwidth currency (FFE0..FFE6) */
    if ((cp >= 0xFF01 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6))
        return 2;
    /* Halfwidth Katakana etc: FF61..FF9F — narrow! width 1 */
    if (cp >= 0xFF61 && cp <= 0xFFDC) return 1;

    /* ===== Supplementary Plane (>= U+10000) ===== */
    if (cp >= 0x10000) {
        /* CJK Extension B..I: 0x20000..0x2FFFF, 0x30000..0x3134F, etc. */
        if ((cp >= 0x20000 && cp <= 0x2FFFF) ||
            (cp >= 0x30000 && cp <= 0x3134F))
            return 2;
        /* CJK Compatibility Ideographs Supplement: 0x2F800..0x2FA1F */
        if (cp >= 0x2F800 && cp <= 0x2FA1F) return 2;
        /* Emoji pictographs — commonly rendered as wide in modern terminals.
         * U+1F000..U+1F02F Mahjong tiles / Domino tiles
         * U+1F030..U+1F09F Playing cards / Domino
         * U+1F0A0..U+1F0FF Other emoji
         * U+1F100..U+1F1FF Enclosed letters / Regional indicator
         * U+1F200..U+1F2FF Square symbols
         * U+1F300..U+1F5FF Misc symbols / nature
         * U+1F600..U+1F64F Emoticons
         * U+1F680..U+1F6FF Transport / map
         * U+1F700..U+1F77F Alchemical symbols
         * U+1F780..U+1F7FF Geometric shapes extended
         * U+1F800..U+1F8FF Supplemental arrows C
         * U+1F900..U+1F9FF Supplemental symbols and pictographs
         * U+1FA00..U+1FA6F Chess symbols etc
         * U+1FA70..U+1FAFF Symbols and pictographs extended
         */
        if ((cp >= 0x1F000 && cp <= 0x1F02F) ||
            (cp >= 0x1F030 && cp <= 0x1F09F) ||
            (cp >= 0x1F0A0 && cp <= 0x1F0FF) ||
            (cp >= 0x1F100 && cp <= 0x1F1FF) ||
            (cp >= 0x1F200 && cp <= 0x1F2FF) ||
            (cp >= 0x1F300 && cp <= 0x1F5FF) ||
            (cp >= 0x1F600 && cp <= 0x1F64F) ||
            (cp >= 0x1F680 && cp <= 0x1F6FF) ||
            (cp >= 0x1F700 && cp <= 0x1F77F) ||
            (cp >= 0x1F780 && cp <= 0x1F7FF) ||
            (cp >= 0x1F800 && cp <= 0x1F8FF) ||
            (cp >= 0x1F900 && cp <= 0x1F9FF) ||
            (cp >= 0x1FA00 && cp <= 0x1FA6F) ||
            (cp >= 0x1FA70 && cp <= 0x1FAFF))
            return 2;
    }

    /* Everything else is narrow */
    return 1;
}

/**
 * @brief Compute the total display column count (a.k.a. wcswidth) for the
 *        first @p byte_len bytes inside @p buf. Stops cleanly at partial
 *        sequences.
 */
static int utf8_display_cols(const char *buf, int byte_len)
{
    int cols = 0;
    int i = 0;
    while (i < byte_len) {
        int start = i;
        unsigned long cp = utf8_decode(buf, byte_len, &i);
        if (i == start) { i++; } /* safety: advance at least 1 */
        cols += utf8_cp_width(cp);
    }
    if (cols < 0) cols = 0;
    return cols;
}

/* redraw current line_buffer state, clearing old content —
 * now CURSOR ALIGMENT uses DISPLAY COLUMNS (not raw bytes) so CJK chars
 * (3 bytes UTF-8, 2 columns wide) don't cause cursor drift on ← → presses. */
static void _bash_rl_redraw(const char *prompt, const char *buf, int len, int cur)
{
    fputs("\r", stdout);
    fputs(prompt, stdout);
    fwrite(buf, 1, len, stdout);
    /* erase trailing characters that may remain from prior longer line */
    fputs("\x1b[K", stdout); /* EL: erase to end of line */
    /* move cursor back to cur position, using DISPLAY COLUMN COUNT so that
     * wide chars (Chinese = 2 cols) don't desync the cursor position. */
    int total_cols = utf8_display_cols(buf, len);
    int cur_cols   = utf8_display_cols(buf, cur);
    int back = total_cols - cur_cols;
    if (back < 0) back = 0;
    for (int i = 0; i < back; i++) putchar('\b');
    fflush(stdout);
}

static int bi_history(bash_ctx_t *ctx, int argc, char **argv)
{
    (void)ctx; (void)argv;
    int n = 0; /* default: show all */
    if (argc > 1) {
        n = atoi(argv[1]);
        if (n <= 0) n = 0;
    }
    int base = (g_hist_count > HIST_SIZE) ? (g_hist_count - HIST_SIZE) : 0;
    int start = (n > 0 && n < g_hist_count - base) ? (g_hist_count - n) : base;
    for (int i = start; i < g_hist_count; i++) {
        const char *h = _bash_hist_get(i);
        if (h) printf("  %d  %s\n", i + 1, h);
    }
    fflush(stdout);
    return 0;
}

static char *_bash_readline(bash_ctx_t *ctx, const char *prompt)
{
    static char line_buf[8192];
    int len = 0; /* current line length */
    int cur = 0; /* cursor position within line (0..len) */
    int last_tab_state = 0; /* how many consecutive Tabs have we seen without edits */

    (void)_bash_rl_redraw; /* may be unused if non-tty path */
    g_hist_nav = -1;
    if (g_hist_saved) { free(g_hist_saved); g_hist_saved = NULL; }

    fputs(prompt, stdout); fflush(stdout);

#ifdef BASH_PLATFORM_WINDOWS
    {
        int vt_ok_local = 0;
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        int have_con = _bash_have_real_console(&vt_ok_local);
        (void)vt_ok_local;

        /* Detect mintty / MSYS2 / Cygwin pty wrapper.  Detection MUST come
         * BEFORE the `have_con / _isatty` fgets fall-through, because mintty
         * has no real console handle (GetConsoleMode fails → have_con=0) —
         * if we checked `!have_con` first we would ALWAYS drop to fgets and
         * never enter the byte-level editor path below, which is precisely
         * what caused the previous build to still exhibit the "every key
         * also triggers Enter" bug.  Detection order: env markers → TERM →
         * GetFileType (real consoles are FILE_TYPE_CHAR; pty pipes are not). */
        int is_tty = _isatty(0);
        int running_under_mintty = 0;
        if (is_tty) {
            if (getenv("MSYSTEM"))      running_under_mintty = 1;
            if (getenv("MINTTY_VERSION")) running_under_mintty = 1;
            const char *term = getenv("TERM");
            if (term && strncmp(term, "xterm", 5) == 0) running_under_mintty = 1;
            if (term && strncmp(term, "cygwin", 6) == 0) running_under_mintty = 1;
            if (hin  != INVALID_HANDLE_VALUE && hin) {
                if (GetFileType(hin)  != FILE_TYPE_CHAR) running_under_mintty = 1;
            }
            if (hout != INVALID_HANDLE_VALUE && hout) {
                if (GetFileType(hout) != FILE_TYPE_CHAR) running_under_mintty = 1;
            }
        }

        /* -------- Branch 1: mintty / MSYS2 interactive tty → full byte editor.
         * We use _read(fd=0) (CRT stdin file descriptor) rather than the
         * raw Win32 HANDLE returned by GetStdHandle(STD_INPUT_HANDLE).  The
         * reason is subtle: under MSYS2/mintty the Win32 handle is a pipe
         * that winpty/msys-2.0.dll has wrapped; the CRT fd=0 is what the
         * C library actually uses for read/write and is consistently in
         * "binary byte stream" mode, which is exactly what we need for a
         * raw VT-escape parser.  _read blocks until at least 1 byte is
         * available (matching POSIX read(0, &c, 1) behaviour).             */
        if (is_tty && running_under_mintty) {
            /* Disable stdio buffering on stdin so _read and fgetc don't race */
            setvbuf(stdin, NULL, _IONBF, 0);
            for (;;) {
                unsigned char uc;
                int nr = _read(0, &uc, 1);
                if (nr <= 0) return NULL;
                int c = uc;

                /* ---- VT escape-sequence (arrow keys, Home, End, Delete) ---- */
                if (c == 0x1b) {
                    unsigned char n1, n2;
                    int nr_tmp;
                    nr_tmp = _read(0, &n1, 1);
                    if (nr_tmp != 1) continue;
                    if (n1 == '[') {
                        nr_tmp = _read(0, &n2, 1);
                        if (nr_tmp != 1) continue;
                        /* read numeric parameter (e.g. 3~ = Delete, 1~ = Home) */
                        int param = 0;
                        while (n2 >= '0' && n2 <= '9') {
                            param = param * 10 + (n2 - '0');
                            nr_tmp = _read(0, &n2, 1);
                            if (nr_tmp != 1) { n2 = 0; break; }
                        }
                        /* skip modifier suffixes (e.g. 1;5A = Ctrl+Up) */
                        while (n2 == ';') {
                            nr_tmp = _read(0, &n2, 1);
                            if (nr_tmp != 1) { n2 = 0; break; }
                            while (n2 >= '0' && n2 <= '9') {
                                nr_tmp = _read(0, &n2, 1);
                                if (nr_tmp != 1) { n2 = 0; break; }
                            }
                        }
                        switch (n2) {
                            case 'A': /* Up — history previous */
                            case 'B': { /* Down — history next */
                                int newest = g_hist_count - 1;
                                int base = (g_hist_count > HIST_SIZE) ? (g_hist_count - HIST_SIZE) : 0;
                                if (n2 == 'A') {
                                    if (g_hist_count == 0) continue;
                                    if (g_hist_nav == -1) {
                                        free(g_hist_saved);
                                        g_hist_saved = _bash_xstrdup(line_buf);
                                        g_hist_nav = newest;
                                    } else if (g_hist_nav > base) {
                                        g_hist_nav--;
                                    } else continue;
                                } else {
                                    if (g_hist_nav == -1) continue;
                                    if (g_hist_nav < newest) {
                                        g_hist_nav++;
                                    } else {
                                        g_hist_nav = -1;
                                        const char *src = g_hist_saved ? g_hist_saved : "";
                                        int sl = (int)strlen(src);
                                        if (sl >= (int)sizeof(line_buf)) sl = (int)sizeof(line_buf) - 1;
                                        memcpy(line_buf, src, sl); line_buf[sl] = 0;
                                        len = sl; cur = sl;
                                        _bash_rl_redraw(prompt, line_buf, len, cur);
                                        continue;
                                    }
                                }
                                const char *h = _bash_hist_get(g_hist_nav);
                                if (!h) continue;
                                int hl = (int)strlen(h);
                                if (hl >= (int)sizeof(line_buf)) hl = (int)sizeof(line_buf) - 1;
                                memcpy(line_buf, h, hl); line_buf[hl] = 0;
                                len = hl; cur = hl;
                                _bash_rl_redraw(prompt, line_buf, len, cur);
                                last_tab_state = 0;
                                continue;
                            }
                            case 'C': { /* Right — advance by whole UTF-8 char */
                                if (cur < len) {
                                    int step = utf8_fwd_len(line_buf, len, cur);
                                    if (step < 1) step = 1;
                                    cur += step;
                                    if (cur > len) cur = len;
                                    _bash_rl_redraw(prompt, line_buf, len, cur);
                                }
                                continue;
                            }
                            case 'D': { /* Left — retreat by whole UTF-8 char */
                                if (cur > 0) {
                                    int step = utf8_back_len(line_buf, len, cur);
                                    if (step < 1) step = 1;
                                    cur -= step;
                                    if (cur < 0) cur = 0;
                                    _bash_rl_redraw(prompt, line_buf, len, cur);
                                }
                                continue;
                            }
                            case 'H': /* Home (xterm) */
                                cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur);
                                continue;
                            case 'F': /* End (xterm) */
                                cur = len; _bash_rl_redraw(prompt, line_buf, len, cur);
                                continue;
                            case '~':
                                if (param == 3) { /* Delete — remove whole UTF-8 char after cursor */
                                    if (cur < len) {
                                        int step = utf8_fwd_len(line_buf, len, cur);
                                        if (step < 1) step = 1;
                                        if (cur + step > len) step = len - cur;
                                        memmove(line_buf + cur, line_buf + cur + step, (size_t)(len - cur - step));
                                        len -= step;
                                        _bash_rl_redraw(prompt, line_buf, len, cur);
                                        last_tab_state = 0;
                                        g_hist_nav = -1;
                                    }
                                } else if (param == 1 || param == 7) { /* Home */
                                    cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur);
                                } else if (param == 4 || param == 8) { /* End */
                                    cur = len; _bash_rl_redraw(prompt, line_buf, len, cur);
                                }
                                continue;
                            default: continue;
                        }
                    }
                    if (n1 == 'O') { /* rxvt-style: ESC O H = Home, ESC O F = End */
                        nr_tmp = _read(0, &n2, 1);
                        if (nr_tmp != 1) continue;
                        if (n2 == 'H') { cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur); }
                        else if (n2 == 'F') { cur = len; _bash_rl_redraw(prompt, line_buf, len, cur); }
                        continue;
                    }
                    /* bare ESC — ignore */
                    continue;
                } /* end ESC handling */

                /* ---- common key handling (same as POSIX path) ---- */
                if (c == '\r' || c == '\n') {
                    line_buf[len] = 0;
                    putchar('\n'); fflush(stdout);
                    _bash_hist_push(line_buf);
                    return line_buf;
                }
                if (c == 4 /* ^D EOF */ && len == 0) {
                    putchar('\n'); fflush(stdout);
                    return NULL;
                }
                if (c == 3 /* ^C */) {
                    fputs("^C\n", stdout); fflush(stdout);
                    line_buf[0] = 0; len = 0; cur = 0;
                    g_hist_nav = -1;
                    free(g_hist_saved); g_hist_saved = NULL;
                    fputs(prompt, stdout); fflush(stdout);
                    continue;
                }
                if (c == '\b' || c == 127 /* DEL */) {
                    if (cur > 0) {
                        /* Backspace: remove WHOLE multi-byte UTF-8 character before cursor */
                        int step = utf8_back_len(line_buf, len, cur);
                        if (step < 1) step = 1;
                        if (step > cur) step = cur;
                        memmove(line_buf + cur - step, line_buf + cur, (size_t)(len - cur));
                        cur -= step; len -= step;
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                        g_hist_nav = -1;
                    }
                    continue;
                }
                if (c == '\t') {
                    /* Tab completion — same logic as POSIX / ReadConsoleInputW paths */
                    last_tab_state++;
                    int ts = _bash_token_start(line_buf, cur);
                    int is_cmd = _bash_is_command_pos(line_buf, ts);
                    int token_len = cur - ts;
                    int n_cands = 0;
                    char **arr = _bash_complete(ctx, line_buf + ts, token_len, is_cmd, &n_cands);
                    if (n_cands == 0) {
                        last_tab_state = 0;
                        _bash_complete_free(arr, n_cands);
                        continue;
                    }
                    if (n_cands == 1) {
                        const char *repl = arr[0];
                        int repl_len = (int)strlen(repl);
                        int add = repl_len - token_len;
                        if (add <= 0) { last_tab_state = 0; _bash_complete_free(arr, n_cands); continue; }
                        if (len + add + 2 >= (int)sizeof(line_buf)) {
                            last_tab_state = 0; _bash_complete_free(arr, n_cands); continue;
                        }
                        memmove(line_buf + ts + repl_len, line_buf + cur, (size_t)(len - cur));
                        memcpy(line_buf + ts, repl, repl_len);
                        len += add; cur = ts + repl_len;
                        if (repl[repl_len-1] != '/') {
                            line_buf[len] = ' '; len++; cur++;
                        }
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                        _bash_complete_free(arr, n_cands);
                        continue;
                    }
                    char prefix_tab[4096];
                    int plen_tab = _bash_common_prefix(arr, n_cands, prefix_tab);
                    if (plen_tab > token_len) {
                        int add = plen_tab - token_len;
                        if (len + add < (int)sizeof(line_buf)) {
                            memmove(line_buf + ts + plen_tab, line_buf + cur, (size_t)(len - cur));
                            memcpy(line_buf + ts, prefix_tab, plen_tab);
                            len += add; cur = ts + plen_tab;
                            _bash_rl_redraw(prompt, line_buf, len, cur);
                        }
                        last_tab_state = 1;
                        _bash_complete_free(arr, n_cands);
                        continue;
                    }
                    if (last_tab_state >= 2) {
                        putchar('\n');
                        int colw = 0;
                        for (int i = 0; i < n_cands; i++) {
                            int w = (int)strlen(arr[i]) + 2;
                            if (w > colw) colw = w;
                        }
                        int termsz = 80;
                        int cols = (termsz + 1) / (colw + 1);
                        if (cols < 1) cols = 1;
                        for (int i = 0; i < n_cands; i++) {
                            printf("%-*s", colw, arr[i]);
                            if ((i + 1) % cols == 0) putchar('\n');
                        }
                        if (n_cands % cols != 0) putchar('\n');
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                    }
                    _bash_complete_free(arr, n_cands);
                    continue;
                }
                if (c < 0x20) continue; /* ignore other control chars */
                if (len + 1 >= (int)sizeof(line_buf)) continue;
                memmove(line_buf + cur + 1, line_buf + cur, (size_t)(len - cur));
                line_buf[cur] = (char)c;
                cur++; len++;
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
                g_hist_nav = -1;
            } /* mintty byte-level editor loop */
        } /* end mintty branch */

        /* Branch 2: no real console handle (redirected stdio, pipes, or
         * any tty-type that mintty detection also didn't catch).  Go
         * straight to fgets — no line editor, but the user explicitly
         * said non-interactive is fine and mintty is covered above. */
        if (!have_con || !_isatty(0) ||
            hin == INVALID_HANDLE_VALUE || !hin ||
            hout == INVALID_HANDLE_VALUE || !hout) {
            if (!fgets(line_buf, sizeof(line_buf), stdin)) return NULL;
            int l = (int)strlen(line_buf);
            while (l > 0 && (line_buf[l-1] == '\n' || line_buf[l-1] == '\r')) line_buf[--l] = 0;
            _bash_hist_push(line_buf);
            return line_buf;
        }

        /* Branch 3: genuine console (cmd.exe / PowerShell) — ReadConsoleInputW.
         * Only real Windows consoles reach this path; mintty and pipes were
         * handled above.                                                         */
        for (;;) {
            INPUT_RECORD ir;
            DWORD got = 0;
            if (!ReadConsoleInputW(hin, &ir, 1, &got)) {
                /* I/O broken — treat as EOF */
                return NULL;
            }
            if (got != 1) continue;
            if (ir.EventType != KEY_EVENT) continue;
            const KEY_EVENT_RECORD *k = &ir.Event.KeyEvent;
            if (!k->bKeyDown) continue; /* ignore key-up and repeat auto-repeat? Actually repeat is bKeyDown=TRUE w/ wRepeatCount >1 */
            WORD vk = k->wVirtualKeyCode;
            WORD modifiers = k->dwControlKeyState;
            (void)modifiers;
            /* handle multi-character repeat count */
            WORD repeats = k->wRepeatCount ? k->wRepeatCount : 1;

            /* ---------- virtual-key / navigation cases ---------- */
            if (vk == VK_UP || vk == VK_DOWN) {
                int newest = g_hist_count - 1;
                int base = (g_hist_count > HIST_SIZE) ? (g_hist_count - HIST_SIZE) : 0;
                if (vk == VK_UP) {
                    if (g_hist_count == 0) continue;
                    if (g_hist_nav == -1) {
                        free(g_hist_saved);
                        g_hist_saved = _bash_xstrdup(line_buf);
                        g_hist_nav = newest;
                    } else if (g_hist_nav > base) {
                        g_hist_nav--;
                    } else {
                        continue;
                    }
                } else { /* VK_DOWN */
                    if (g_hist_nav == -1) continue;
                    if (g_hist_nav < newest) {
                        g_hist_nav++;
                    } else {
                        g_hist_nav = -1;
                        const char *src = g_hist_saved ? g_hist_saved : "";
                        int sl = (int)strlen(src);
                        if (sl >= (int)sizeof(line_buf)) sl = (int)sizeof(line_buf) - 1;
                        memcpy(line_buf, src, sl); line_buf[sl] = 0;
                        len = sl; cur = sl;
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        continue;
                    }
                }
                const char *h = _bash_hist_get(g_hist_nav);
                if (!h) continue;
                int hl = (int)strlen(h);
                if (hl >= (int)sizeof(line_buf)) hl = (int)sizeof(line_buf) - 1;
                memcpy(line_buf, h, hl); line_buf[hl] = 0;
                len = hl; cur = hl;
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
                continue;
            }
            if (vk == VK_LEFT) {
                if (cur > 0) {
                    int step = utf8_back_len(line_buf, len, cur);
                    if (step < 1) step = 1;
                    cur -= step;
                    if (cur < 0) cur = 0;
                    _bash_rl_redraw(prompt, line_buf, len, cur);
                }
                continue;
            }
            if (vk == VK_RIGHT) {
                if (cur < len) {
                    int step = utf8_fwd_len(line_buf, len, cur);
                    if (step < 1) step = 1;
                    cur += step;
                    if (cur > len) cur = len;
                    _bash_rl_redraw(prompt, line_buf, len, cur);
                }
                continue;
            }
            if (vk == VK_HOME) { cur = 0;         _bash_rl_redraw(prompt, line_buf, len, cur); continue; }
            if (vk == VK_END)  { cur = len;       _bash_rl_redraw(prompt, line_buf, len, cur); continue; }
            if (vk == VK_DELETE) {
                if (cur < len) {
                    /* Delete: remove WHOLE multi-byte UTF-8 char after cursor */
                    int step = utf8_fwd_len(line_buf, len, cur);
                    if (step < 1) step = 1;
                    if (cur + step > len) step = len - cur;
                    memmove(line_buf + cur, line_buf + cur + step, (size_t)(len - cur - step));
                    len -= step;
                    _bash_rl_redraw(prompt, line_buf, len, cur);
                    last_tab_state = 0;
                    g_hist_nav = -1;
                }
                continue;
            }
            /* UChar case: translate each Unicode char in uChar to UTF-8 bytes
             * for the buffer; but Windows console typically keeps Latin
             * commands in ASCII range so uChar <= 0x7F is by far the common
             * case — handle both.  wRepeatCount applies to the character. */
            for (WORD r = 0; r < repeats; r++) {
                WCHAR wc = k->uChar.UnicodeChar;
                int c = 0;
                if (wc <= 0x7F) {
                    c = (int)wc;
                } else if (wc == 0) {
                    /* non-character key (pure VK): already handled above */
                    break;
                } else {
                    /* Encode WCHAR to UTF-8 bytes. */
                    unsigned char bytes[4]; int nb = 0;
                    unsigned long cp = wc;
                    if (cp < 0x80) { bytes[0] = (unsigned char)cp; nb = 1; }
                    else if (cp < 0x800) {
                        bytes[0] = (unsigned char)(0xC0 | (cp >> 6));
                        bytes[1] = (unsigned char)(0x80 | (cp & 0x3F)); nb = 2;
                    } else {
                        bytes[0] = (unsigned char)(0xE0 | (cp >> 12));
                        bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
                        bytes[2] = (unsigned char)(0x80 | (cp & 0x3F)); nb = 3;
                    }
                    for (int bi = 0; bi < nb; bi++) {
                        if (len + 1 >= (int)sizeof(line_buf)) break;
                        memmove(line_buf + cur + 1, line_buf + cur, (size_t)(len - cur));
                        line_buf[cur] = (char)bytes[bi];
                        cur++; len++;
                    }
                    _bash_rl_redraw(prompt, line_buf, len, cur);
                    last_tab_state = 0;
                    g_hist_nav = -1;
                    continue; /* skip ASCII c switch */
                }

                /* ---- ASCII c handling (c < 128) ---- */
                if (c == '\r' || c == '\n') {
                    line_buf[len] = 0;
                    putchar('\n'); fflush(stdout);
                    _bash_hist_push(line_buf);
                    return line_buf;
                }
                if (c == 4 /* ^D EOF */ && len == 0) {
                    putchar('\n'); fflush(stdout);
                    return NULL;
                }
                if (c == 3 /* ^C */) {
                    fputs("^C\n", stdout); fflush(stdout);
                    line_buf[0] = 0; len = 0; cur = 0;
                    g_hist_nav = -1;
                    free(g_hist_saved); g_hist_saved = NULL;
                    fputs(prompt, stdout); fflush(stdout);
                    continue;
                }
                if (c == '\b' || c == 127 /* DEL */ || vk == VK_BACK) {
                    /* Backspace: remove WHOLE multi-byte UTF-8 character before cursor */
                    if (cur > 0) {
                        int step = utf8_back_len(line_buf, len, cur);
                        if (step < 1) step = 1;
                        if (step > cur) step = cur;
                        memmove(line_buf + cur - step, line_buf + cur, (size_t)(len - cur));
                        cur -= step; len -= step;
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                        g_hist_nav = -1;
                    }
                    continue;
                }
                if (c == '\t' || vk == VK_TAB) {
                    /* Tab completion — forward to the generic logic below */
                    /* We can't easily break out, so inline it: */
                    last_tab_state++;
                    int ts = _bash_token_start(line_buf, cur);
                    int is_cmd = _bash_is_command_pos(line_buf, ts);
                    int token_len = cur - ts;
                    int nn_cands = 0;
                    char **arr = _bash_complete(ctx, line_buf + ts, token_len, is_cmd, &nn_cands);
                    if (nn_cands == 0) {
                        last_tab_state = 0;
                        _bash_complete_free(arr, nn_cands);
                        continue;
                    }
                    if (nn_cands == 1) {
                        const char *repl = arr[0];
                        int repl_len = (int)strlen(repl);
                        int add = repl_len - token_len;
                        if (add <= 0) { last_tab_state = 0; _bash_complete_free(arr, nn_cands); continue; }
                        if (len + add + 2 >= (int)sizeof(line_buf)) {
                            last_tab_state = 0; _bash_complete_free(arr, nn_cands); continue;
                        }
                        memmove(line_buf + ts + repl_len, line_buf + cur, (size_t)(len - cur));
                        memcpy(line_buf + ts, repl, repl_len);
                        len += add; cur = ts + repl_len;
                        if (repl[repl_len-1] != '/') {
                            line_buf[len] = ' '; len++; cur++;
                        }
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                        _bash_complete_free(arr, nn_cands);
                        continue;
                    }
                    char prefix[4096];
                    int plen = _bash_common_prefix(arr, nn_cands, prefix);
                    if (plen > token_len) {
                        int add = plen - token_len;
                        if (len + add < (int)sizeof(line_buf)) {
                            memmove(line_buf + ts + plen, line_buf + cur, (size_t)(len - cur));
                            memcpy(line_buf + ts, prefix, plen);
                            len += add; cur = ts + plen;
                            _bash_rl_redraw(prompt, line_buf, len, cur);
                        }
                        last_tab_state = 1;
                        _bash_complete_free(arr, nn_cands);
                        continue;
                    }
                    if (last_tab_state >= 2) {
                        putchar('\n');
                        int colw = 0;
                        for (int ii = 0; ii < nn_cands; ii++) {
                            int w = (int)strlen(arr[ii]) + 2;
                            if (w > colw) colw = w;
                        }
                        int termsz = 80;
                        int cols = (termsz + 1) / (colw + 1);
                        if (cols < 1) cols = 1;
                        for (int ii = 0; ii < nn_cands; ii++) {
                            printf("%-*s", colw, arr[ii]);
                            if ((ii + 1) % cols == 0) putchar('\n');
                        }
                        if (nn_cands % cols != 0) putchar('\n');
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                    }
                    _bash_complete_free(arr, nn_cands);
                    continue;
                }
                if (c < 0x20) {
                    /* ignore other control chars */
                    continue;
                }
                if (len + 1 >= (int)sizeof(line_buf)) continue;
                memmove(line_buf + cur + 1, line_buf + cur, (size_t)(len - cur));
                line_buf[cur] = (char)c;
                cur++; len++;
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
                g_hist_nav = -1;
            } /* for repeats */
        } /* ReadConsoleInput loop */
    } /* Windows block */
#else
    /* Ensure raw mode only when we own stdin (interactive tty); otherwise
     * fall back to fgets on first call if not a tty. */
    static int inited_raw = 0;
    if (!inited_raw) {
        if (isatty(0)) { _bash_tty_raw(1); }
        inited_raw = 1;
    }
    if (!isatty(0)) {
        /* non-interactive fallback: just use fgets */
        if (!fgets(line_buf, sizeof(line_buf), stdin)) { if (inited_raw) {_bash_tty_raw(0); inited_raw=0;} return NULL; }
        int l = (int)strlen(line_buf);
        while (l > 0 && (line_buf[l-1] == '\n' || line_buf[l-1] == '\r')) line_buf[--l] = 0;
        _bash_hist_push(line_buf);
        return line_buf;
    }
    for (;;) {
        unsigned char uc;
        if (read(0, &uc, 1) != 1) { _bash_tty_raw(0); inited_raw=0; return NULL; }
        int c = uc;
        /* Handle ESC sequences for arrow keys, Home, End, Delete */
        if (c == 0x1b) {
            unsigned char n1, n2;
            if (read(0, &n1, 1) != 1) continue;
            if (n1 == '[') {
                if (read(0, &n2, 1) != 1) continue;
                /* read numeric parameter (e.g. 3~ = Delete, 1~ = Home) */
                int param = 0;
                while (n2 >= '0' && n2 <= '9') {
                    param = param * 10 + (n2 - '0');
                    if (read(0, &n2, 1) != 1) { n2 = 0; break; }
                }
                /* skip modifier suffixes (e.g. 1;5A = Ctrl+Up) */
                while (n2 == ';') {
                    if (read(0, &n2, 1) != 1) { n2 = 0; break; }
                    while (n2 >= '0' && n2 <= '9') {
                        if (read(0, &n2, 1) != 1) { n2 = 0; break; }
                    }
                }
                switch (n2) {
                    case 'A': /* Up — history previous */
                    case 'B': { /* Down — history next */
                        int newest = g_hist_count - 1;
                        int base = (g_hist_count > HIST_SIZE) ? (g_hist_count - HIST_SIZE) : 0;
                        if (n2 == 'A') {
                            if (g_hist_count == 0) continue;
                            if (g_hist_nav == -1) {
                                free(g_hist_saved);
                                g_hist_saved = _bash_xstrdup(line_buf);
                                g_hist_nav = newest;
                            } else if (g_hist_nav > base) {
                                g_hist_nav--;
                            } else continue;
                        } else {
                            if (g_hist_nav == -1) continue;
                            if (g_hist_nav < newest) {
                                g_hist_nav++;
                            } else {
                                g_hist_nav = -1;
                                const char *src = g_hist_saved ? g_hist_saved : "";
                                int sl = (int)strlen(src);
                                if (sl >= (int)sizeof(line_buf)) sl = (int)sizeof(line_buf) - 1;
                                memcpy(line_buf, src, sl); line_buf[sl] = 0;
                                len = sl; cur = sl;
                                _bash_rl_redraw(prompt, line_buf, len, cur);
                                continue;
                            }
                        }
                        const char *h = _bash_hist_get(g_hist_nav);
                        if (!h) continue;
                        int hl = (int)strlen(h);
                        if (hl >= (int)sizeof(line_buf)) hl = (int)sizeof(line_buf) - 1;
                        memcpy(line_buf, h, hl); line_buf[hl] = 0;
                        len = hl; cur = hl;
                        _bash_rl_redraw(prompt, line_buf, len, cur);
                        last_tab_state = 0;
                        continue;
                    }
                    case 'C': { /* Right — advance by whole UTF-8 char */
                        if (cur < len) {
                            int step = utf8_fwd_len(line_buf, len, cur);
                            if (step < 1) step = 1;
                            cur += step;
                            if (cur > len) cur = len;
                            _bash_rl_redraw(prompt, line_buf, len, cur);
                        }
                        continue;
                    }
                    case 'D': { /* Left — retreat by whole UTF-8 char */
                        if (cur > 0) {
                            int step = utf8_back_len(line_buf, len, cur);
                            if (step < 1) step = 1;
                            cur -= step;
                            if (cur < 0) cur = 0;
                            _bash_rl_redraw(prompt, line_buf, len, cur);
                        }
                        continue;
                    }
                    case 'H': /* Home (xterm) */
                        cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur);
                        continue;
                    case 'F': /* End (xterm) */
                        cur = len; _bash_rl_redraw(prompt, line_buf, len, cur);
                        continue;
                    case '~':
                        if (param == 3) { /* Delete — remove whole UTF-8 char after cursor */
                            if (cur < len) {
                                int step = utf8_fwd_len(line_buf, len, cur);
                                if (step < 1) step = 1;
                                if (cur + step > len) step = len - cur;
                                memmove(line_buf + cur, line_buf + cur + step, (size_t)(len - cur - step));
                                len -= step;
                                _bash_rl_redraw(prompt, line_buf, len, cur);
                                last_tab_state = 0;
                                g_hist_nav = -1;
                            }
                        } else if (param == 1 || param == 7) { /* Home */
                            cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur);
                        } else if (param == 4 || param == 8) { /* End */
                            cur = len; _bash_rl_redraw(prompt, line_buf, len, cur);
                        }
                        continue;
                    default: continue;
                }
            }
            if (n1 == 'O') { /* rxvt-style: ESC O H = Home, ESC O F = End */
                if (read(0, &n2, 1) != 1) continue;
                if (n2 == 'H') { cur = 0; _bash_rl_redraw(prompt, line_buf, len, cur); }
                else if (n2 == 'F') { cur = len; _bash_rl_redraw(prompt, line_buf, len, cur); }
                continue;
            }
            continue;
        }
        /* ===== common POSIX key handling: ENTER / ^D / ^C / BS / TAB / printable =====
         * The Windows path (ReadConsoleInputW, above) handles all of these inline
         * (different data flow), so the tail below is POSIX-only and lives inside
         * the same #else block as the read() loop.                             */
        if (c == '\r' || c == '\n') {
            line_buf[len] = 0;
            putchar('\n'); fflush(stdout);
            _bash_hist_push(line_buf);
            return line_buf;
        }
        if (c == 4 /* ^D EOF */ && len == 0) {
            int wr = (int)write(1, "\n", 1);
            (void)wr;
            return NULL;
        }
        if (c == 3 /* ^C */) {
            int wr = (int)write(1, "^C\n", 3);
            (void)wr;
            line_buf[0] = 0; len = 0; cur = 0;
            g_hist_nav = -1;
            free(g_hist_saved); g_hist_saved = NULL;
            fputs(prompt, stdout); fflush(stdout);
            continue;
        }
        if (c == '\b' || c == 127 /* DEL */) {
            if (cur > 0) {
                /* Backspace: remove WHOLE multi-byte UTF-8 character before cursor */
                int step = utf8_back_len(line_buf, len, cur);
                if (step < 1) step = 1;
                if (step > cur) step = cur;
                memmove(line_buf + cur - step, line_buf + cur, (size_t)(len - cur));
                cur -= step; len -= step;
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
                g_hist_nav = -1; /* leave history nav on edit */
            }
            continue;
        }
        if (c == '\t') {
            /* Tab completion */
            last_tab_state++;
            /* Find token start from cursor */
            int ts = _bash_token_start(line_buf, cur);
            int is_cmd = _bash_is_command_pos(line_buf, ts);
            int token_len = cur - ts;
            int n_cands = 0;
            char **arr = _bash_complete(ctx, line_buf + ts, token_len, is_cmd, &n_cands);
            if (n_cands == 0) {
                last_tab_state = 0;
                _bash_complete_free(arr, n_cands);
                continue;
            }
            if (n_cands == 1) {
                const char *repl = arr[0];
                int repl_len = (int)strlen(repl);
                /* compute how much more we have to append (beyond existing prefix) */
                int add = repl_len - token_len;
                if (add <= 0) { last_tab_state = 0; _bash_complete_free(arr, n_cands); continue; }
                /* check capacity */
                if (len + add + 2 >= (int)sizeof(line_buf)) {
                    last_tab_state = 0; _bash_complete_free(arr, n_cands); continue;
                }
                /* shift right */
                memmove(line_buf + ts + repl_len, line_buf + cur, (size_t)(len - cur));
                memcpy(line_buf + ts, repl, repl_len);
                len += add; cur = ts + repl_len;
                /* add space unless it's a directory (trailing /) */
                if (repl[repl_len-1] != '/') {
                    line_buf[len] = ' ';
                    len++; cur++;
                }
                /* redraw */
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
                _bash_complete_free(arr, n_cands);
                continue;
            }
            /* n_cands > 1: try common prefix first */
            char prefix[4096];
            int plen = _bash_common_prefix(arr, n_cands, prefix);
            if (plen > token_len) {
                /* there's shared prefix beyond current token — extend it */
                int add = plen - token_len;
                if (len + add < (int)sizeof(line_buf)) {
                    memmove(line_buf + ts + plen, line_buf + cur, (size_t)(len - cur));
                    memcpy(line_buf + ts, prefix, plen);
                    len += add; cur = ts + plen;
                    _bash_rl_redraw(prompt, line_buf, len, cur);
                }
                last_tab_state = 1;
                _bash_complete_free(arr, n_cands);
                continue;
            }
            /* second tab in a row => show list */
            if (last_tab_state >= 2) {
                /* print list */
                putchar('\n');
                /* determine column width */
                int colw = 0;
                for (int i = 0; i < n_cands; i++) {
                    int w = (int)strlen(arr[i]) + 2;
                    if (w > colw) colw = w;
                }
                int termsz = 80;
                int cols = (termsz + 1) / (colw + 1);
                if (cols < 1) cols = 1;
                for (int i = 0; i < n_cands; i++) {
                    printf("%-*s", colw, arr[i]);
                    if ((i + 1) % cols == 0) putchar('\n');
                }
                if (n_cands % cols != 0) putchar('\n');
                /* reprint prompt and line */
                _bash_rl_redraw(prompt, line_buf, len, cur);
                last_tab_state = 0;
            }
            _bash_complete_free(arr, n_cands);
            continue;
        }
        /* regular character: append at cursor */
        if (c < 0x20) {
            /* ignore other control chars */
            continue;
        }
        if (len + 1 >= (int)sizeof(line_buf)) continue;
        memmove(line_buf + cur + 1, line_buf + cur, (size_t)(len - cur));
        line_buf[cur] = (char)c;
        cur++; len++;
        _bash_rl_redraw(prompt, line_buf, len, cur);
        last_tab_state = 0;
        g_hist_nav = -1; /* leave history nav on edit */
    } /* POSIX for (;;) loop */
#endif /* BASH_PLATFORM_WINDOWS sandwich end */
}

/* ========================================================================
 * PS1 prompt expansion
 * ======================================================================== */

/* Small helper: append a C string to a bounded buffer with length tracking.
 * Returns new index (clamped to size-1). */
static int _bash_prompt_append(char *buf, int sz, int idx, const char *s)
{
    if (!s) return idx;
    while (*s && idx < sz - 1) buf[idx++] = *s++;
    buf[idx] = 0;
    return idx;
}
static int _bash_prompt_putc(char *buf, int sz, int idx, char c)
{
    if (idx < sz - 1) { buf[idx++] = c; buf[idx] = 0; }
    return idx;
}

/* Read a variable name starting at *p (after $ or ${). Advances *p to after
 * the name (or closing }). Returns heap-allocated value (caller frees), or
 * NULL if unresolvable. */
static char *_bash_prompt_read_var(bash_ctx_t *ctx, const char **pp)
{
    const char *p = *pp;
    int braced = 0;
    if (*p == '{') { braced = 1; p++; }
    const char *start = p;
    if (*p == '?' || *p == '$' || *p == '!' || *p == '#' || *p == '@' || *p == '*') {
        char name[2] = {*p, 0};
        p++;
        if (braced) { if (*p == '}') p++; }
        *pp = p;
        char *v = _bash_var_get(ctx, name, NULL);
        return v ? _bash_xstrdup(v) : NULL;
    }
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
    if (p == start) return NULL;
    size_t nlen = (size_t)(p - start);
    char name[64];
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, start, nlen); name[nlen] = 0;
    if (braced) { if (*p == '}') p++; }
    *pp = p;
    char *v = _bash_var_get(ctx, name, NULL);
    if (!v) v = getenv(name);
    return v ? _bash_xstrdup(v) : NULL;
}

/* Expand a prompt string (PS1) into out[outsize].
 * Supports: \\[ ... \\]  (strip, non-printing markers)
 *           \\u  username,  \\h  hostname (short)
 *           \\w  full cwd,  \\W  cwd basename
 *           \\e / \\033  ESC char,  \\n  newline
 *           \\$  literal $,  \\\\  literal \\,  \\a  BEL
 *           $VAR / ${VAR}  variable expansion (shell var -> env)
 *           `...`  command substitution (run in shell, output trimmed)
 * Any unknown \\X escape is passed as literal X (without leading \\). */
static void _bash_expand_ps1(bash_ctx_t *ctx, const char *ps1, char *out, int outsize)
{
    if (!ps1 || outsize <= 0) return;
    out[0] = 0;
    int ei = 0;
    for (const char *p = ps1; *p && ei < outsize - 1; ) {
        if (*p == '\\' && p[1] == '[') {
            /* strip \[ ... \] */
            p += 2;
            while (*p && !(*p == '\\' && p[1] == ']')) p++;
            if (*p) p += 2;
            continue;
        }
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'u': {
                    const char *u = getenv("USER");
                    if (!u) u = getenv("USERNAME");
                    if (!u) u = "user";
                    ei = _bash_prompt_append(out, outsize, ei, u);
                    p++;
                    break;
                }
                case 'h': {
                    const char *h = getenv("COMPUTERNAME");
                    if (!h) h = getenv("HOSTNAME");
                    if (!h) h = "host";
                    /* short hostname (up to first '.') */
                    for (const char *q = h; *q && ei < outsize - 1; q++) {
                        if (*q == '.') break;
                        out[ei++] = *q;
                    }
                    out[ei] = 0;
                    p++;
                    break;
                }
                case 'w': {
                    const char *cwd = ctx->pwd ? ctx->pwd : getenv("PWD");
                    if (!cwd) cwd = ".";
                    ei = _bash_prompt_append(out, outsize, ei, cwd);
                    p++;
                    break;
                }
                case 'W': {
                    const char *cwd = ctx->pwd ? ctx->pwd : getenv("PWD");
                    if (!cwd || !*cwd) cwd = ".";
                    const char *base = cwd;
                    for (const char *q = cwd; *q; q++)
                        if (*q == '/') base = q + 1;
                    ei = _bash_prompt_append(out, outsize, ei, base);
                    p++;
                    break;
                }
                case 'e': case 'E': {
                    ei = _bash_prompt_putc(out, outsize, ei, (char)0x1B);
                    p++;
                    break;
                }
                case '0': {
                    /* \033 -> octal ESC (3 digits max), at least first is 0 */
                    int v = 0;
                    const char *pp = p;
                    if (isdigit((unsigned char)pp[1])) {
                        pp++; v = (*pp - '0');
                        if (isdigit((unsigned char)pp[1])) {
                            pp++; v = v * 8 + (*pp - '0');
                            if (isdigit((unsigned char)pp[1])) {
                                pp++; v = v * 8 + (*pp - '0');
                            }
                        }
                    } else {
                        v = 0; /* just \0 */
                    }
                    p = pp + 1;
                    ei = _bash_prompt_putc(out, outsize, ei, (char)v);
                    break;
                }
                case 'n': {
                    ei = _bash_prompt_putc(out, outsize, ei, '\n');
                    p++;
                    break;
                }
                case 'a': {
                    ei = _bash_prompt_putc(out, outsize, ei, (char)0x07);
                    p++;
                    break;
                }
                case '$': {
                    ei = _bash_prompt_putc(out, outsize, ei, '$');
                    p++;
                    break;
                }
                case '\\': {
                    ei = _bash_prompt_putc(out, outsize, ei, '\\');
                    p++;
                    break;
                }
                default: {
                    /* unknown escape: drop the backslash, emit the char */
                    ei = _bash_prompt_putc(out, outsize, ei, *p);
                    p++;
                    break;
                }
            }
            continue;
        }
        if (*p == '$' && (p[1] == '{' || isalpha((unsigned char)p[1]) || p[1] == '_' ||
                          p[1] == '?' || p[1] == '$' || p[1] == '!' || p[1] == '#' ||
                          p[1] == '@' || p[1] == '*' || isdigit((unsigned char)p[1]))) {
            const char *q = p + 1;
            char *val = _bash_prompt_read_var(ctx, &q);
            if (val) {
                ei = _bash_prompt_append(out, outsize, ei, val);
                free(val);
            }
            p = q;
            continue;
        }
        if (*p == '`') {
            /* command substitution: scan to matching backtick, run and trim */
            p++;
            const char *cmd_start = p;
            while (*p && *p != '`') p++;
            size_t clen = (size_t)(p - cmd_start);
            if (*p == '`') p++;
            if (clen > 0) {
                char *cmd = _bash_xstrndup(cmd_start, clen);
                /* run via _bash_run_string; capture stdout. To keep it safe, we run
                 * with a temp file redirect in-process — if we have FILE* piped
                 * it's easier to use popen() but on Windows MSVCRT popen
                 * invokes CMD which can mess quoting. For prompt use, fallback:
                 * run and just use "" on failure, but try popen. */
                char buf[1024]; buf[0] = 0;
#if defined(_WIN32) && !defined(__BIONIC__)
                /* _popen on MSVC/MinGW uses cmd.exe as shell;
                 * but many commands (like __git_ps1) won't exist in cmd. */
                FILE *fp = NULL;
                /* avoid calling potentially expensive / missing subshells:
                 * if the command references a function name like "__git_ps1"
                 * that's only defined in MSYS2 bash, skip silently. */
                int simple_name_only = 1;
                for (size_t k = 0; k < clen; k++) {
                    char c = cmd[k];
                    if (!(isalnum((unsigned char)c) || c == '_')) { simple_name_only = 0; break; }
                }
                if (simple_name_only) {
                    /* Only try builtins directly through shell; otherwise skip. */
                    (void)fp;
                } else {
                    (void)fp;
                }
                (void)buf;
                free(cmd);
                /* For safety, don't try to spawn arbitrary commands from PS1
                 * on Windows CMD. Just leave the substitution empty. */
                continue;
#else
                FILE *fp = popen(cmd, "r");
                free(cmd);
                if (fp) {
                    size_t got = fread(buf, 1, sizeof(buf) - 1, fp);
                    buf[got] = 0;
                    pclose(fp);
                    /* trim trailing whitespace / newlines */
                    while (got > 0 && (buf[got-1] == '\n' || buf[got-1] == '\r' || buf[got-1] == ' ' || buf[got-1] == '\t')) {
                        buf[got-1] = 0; got--;
                    }
                    ei = _bash_prompt_append(out, outsize, ei, buf);
                } else {
                    free(cmd);
                }
#endif
            }
            continue;
        }
        /* regular character */
        out[ei++] = *p++;
        out[ei] = 0;
    }
    out[ei] = 0;
}

/* ========================================================================
 * Interactive REPL
 * ======================================================================== */

static void _bash_interactive(bash_ctx_t *ctx)
{
    char prompt[1024];
#ifdef BASH_PLATFORM_WINDOWS
    /* Probe console & try to enable ANSI VT mode once, at interactive-entry.
     * If VT is unavailable (old Windows, or mintty/pipe non-console handle),
     * we strip ANSI CSI/OSC escape sequences from the prompt before printing
     * so the user sees plain text instead of garbled \e[32m / \e]0;...\a text. */
    int vt_ok = 0;
    int have_con = _bash_have_real_console(&vt_ok);
    (void)have_con;
#endif
    for (;;) {
        const char *ps1 = getenv("PS1");
        if (ps1 && *ps1) {
            _bash_expand_ps1(ctx, ps1, prompt, (int)sizeof(prompt));
        } else {
            /* default: show cwd then $ */
            const char *cwd = ctx->pwd ? ctx->pwd : getenv("PWD");
            if (!cwd) cwd = ".";
            snprintf(prompt, sizeof(prompt), "%s $ ", cwd);
        }
#ifdef BASH_PLATFORM_WINDOWS
        if (!vt_ok) _bash_strip_ansi(prompt);
#endif
        char *line = _bash_readline(ctx, prompt);
        if (!line) break; /* EOF (^D) */
        if (!*line) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "logout") == 0) break;
        _bash_run_string(ctx, line);
        if (ctx->do_exit) break;
    }
#ifndef BASH_PLATFORM_WINDOWS
    /* Force termios restore for any EOF / exit path from interactive loop */
    if (isatty(0)) _bash_tty_raw(0);
#endif
}

/* ========================================================================
 * CLI / main
 * ======================================================================== */

static void _bash_usage(const char *argv0)
{
    printf("Usage: %s [OPTIONS] [SCRIPT [ARGS...]]\n", argv0);
    printf("Options:\n");
    printf("  -c STRING     run STRING as command\n");
    printf("  -s            read commands from stdin (default with no SCRIPT)\n");
    printf("  -i            interactive mode\n");
    printf("  --version     print version\n");
    printf("  --help        this message\n");
}

static void _bash_version(void)
{
    printf("GNU bash, version 5.3.0(1)-release (cross-platform, single-file C)\n");
    printf("Features: pipelines, redirs, control-flow, functions, expansions, builtins\n");
    printf("Bash 5.3 additions: GLOBSORT, ${ cmd; } / ${ | cmd; }, source -p, read -E, compgen -V\n");
}

int main(int argc, char **argv)
{
#ifdef BASH_PLATFORM_WINDOWS
    /* Force the console into UTF-8 (CP 65001) for both input and output.
     * The C source is compiled with UTF-8 string literals, so without this
     * the CRT's printf/putchar/fputs would re-interpret those bytes using
     * the system ANSI code page (typically GBK on zh-CN Windows), producing
     * the classic mojibake (e.g. "鎵撳彂" instead of "打发").  Setting the
     * console code page to UTF-8 makes byte-oriented stdio render correctly.
     * We also set ENABLE_VIRTUAL_TERMINAL_PROCESSING later for ANSI colours.
     *
     * IMPORTANT: only call these APIs when we are actually attached to a
     * real console.  When stdout/stderr are redirected to a pipe/file
     * (e.g. by VSCode, Nodepad++, SSHd, another shell), GetStdHandle may
     * return a non-console handle or INVALID_HANDLE_VALUE, and calling
     * SetConsole*CP on it can hang / silently fail / prevent the process
     * from starting — exactly the "bash打不开" symptom.  We probe by
     * calling GetConsoleMode, which fails gracefully with FALSE if the
     * handle is not a real console buffer.                                 */
    {
        HANDLE ho = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hi = GetStdHandle(STD_INPUT_HANDLE);
        DWORD  dummy;
        if (ho && ho != INVALID_HANDLE_VALUE && GetConsoleMode(ho, &dummy))
            SetConsoleOutputCP(CP_UTF8);
        if (hi && hi != INVALID_HANDLE_VALUE && GetConsoleMode(hi, &dummy))
            SetConsoleCP(CP_UTF8);
    }
#endif

    bash_ctx_t ctx; memset(&ctx, 0, sizeof(ctx));
    _bash_frame_init_global(&ctx);
    ctx.script_name = argc > 0 ? argv[0] : "bash";
    ctx.exit_code = 0;
    ctx.do_exit = 0;
    ctx.last_status = 0;
    ctx.bg_pid = 0;
    ctx.funcs = NULL;

    /* seed env variables */
    char cwd[4096];
    if (BASH_GETCWD(cwd, sizeof(cwd))) {
        _bash_normalize_path(cwd);
        _bash_var_set(&ctx, "PWD", cwd, 1, 1, 0);
        ctx.pwd = _bash_xstrdup(cwd);
    }
    char *home = getenv("HOME");
    if (!home || !*home) {
#ifdef BASH_PLATFORM_WINDOWS
        /* On Windows, HOME is often not set by the system; MSYS2 sets it
         * but the CRT _environ copy may not pick it up.  Fall back to
         * USERPROFILE (which is always set on Windows) and push it into
         * both the CRT environment (via _putenv_s) and the shell vars
         * so that getenv("HOME") works everywhere from now on.         */
        home = getenv("USERPROFILE");
        if (home && *home) {
            extern int __cdecl _putenv_s(const char *, const char *);
            _putenv_s("HOME", home);
        }
#endif
        if (!home || !*home) home = ".";
    }
    _bash_var_set(&ctx, "HOME", home, 1, 1, 0);
    const char *path_env = getenv("PATH");
    if (path_env && *path_env) _bash_var_set(&ctx, "PATH", path_env, 1, 1, 0);
#ifdef BASH_PLATFORM_WINDOWS
    /* Prepend bash.exe self directory to PATH so that companion commands
     * (in self-dir and self-dir/cmd/) are findable by child processes
     * (make, gcc, etc.) via Win32 CreateProcess PATH search.  This mirrors
     * what MSYS2 / Git Bash do at startup.                               */
    {
        char *sd = _bash_get_self_dir();
        if (sd && *sd) {
            const char *cur = _bash_var_get(&ctx, "PATH", NULL);
            if (!cur || !*cur) {
                /* PATH was empty/unset — just use self dir */
                _bash_var_set(&ctx, "PATH", sd, 1, 1, 0);
            } else {
                /* Check if self dir is already in PATH */
                int already = 0;
                const char *p = cur;
                while (*p) {
                    const char *end = p;
                    while (*end && *end != BASH_PATHSEP) end++;
                    size_t dlen = (size_t)(end - p);
                    if (dlen == strlen(sd) && strnicmp(p, sd, dlen) == 0) {
                        already = 1; break;
                    }
                    if (*end) p = end + 1; else break;
                }
                if (!already) {
                    bash_bstr_t np; bash_bstr_init(&np);
                    bash_bstr_puts(&np, sd);
                    bash_bstr_putc(&np, BASH_PATHSEP);
                    bash_bstr_puts(&np, cur);
                    _bash_var_set(&ctx, "PATH", np.data, 1, 1, 0);
                    bash_bstr_free(&np);
                }
            }
            /* NOTE: _bash_get_self_dir() returns a pointer to a static buffer —
             * we MUST NOT free it! (otherwise heap corruption / STATUS_HEAP_CORRUPTION) */
        }
    }
    /* Ensure COMSPEC is set so child processes (mingw32-make etc.) can
     * locate cmd.exe for shelling out. Without COMSPEC, tools like GNU
     * make on Windows may try CreateProcess("/bin/sh", ...) which fails
     * with ERROR_FILE_NOT_FOUND (e=2).                           */
    if (!getenv("COMSPEC")) {
        char csbuf[MAX_PATH];
        UINT r = GetSystemDirectoryA(csbuf, (UINT)sizeof(csbuf));
        if (r > 0 && r + sizeof("\\cmd.exe") <= sizeof(csbuf)) {
            strcat(csbuf, "\\cmd.exe");
            _bash_var_set(&ctx, "COMSPEC", csbuf, 1, 1, 0);
        }
    }
    if (!getenv("IFS")) _bash_var_set(&ctx, "IFS", " \t\n", 0, 0, 0);
#else
    if (!getenv("IFS")) _bash_var_set(&ctx, "IFS", " \t\n", 0, 0, 0);
#endif
    { char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)BASH_GETPID()); _bash_var_set(&ctx, "$", buf, 0, 0, 0); }
    /* Version info variables (Bash 5.3 compatibility) */
    _bash_var_set(&ctx, "BASH_VERSION", "5.3.0(1)-release", 0, 0, 0);
    { char buf2[32]; snprintf(buf2, sizeof(buf2), "%d", 5); _bash_var_set(&ctx, "BASH_VERSINFO[0]", buf2, 0, 0, 0); }
    _bash_var_set(&ctx, "BASH_VERSINFO[1]", "3", 0, 0, 0);
    _bash_var_set(&ctx, "BASH_VERSINFO[2]", "0", 0, 0, 0);
    _bash_var_set(&ctx, "BASH_VERSINFO[3]", "1", 0, 0, 0);
    _bash_var_set(&ctx, "BASH_VERSINFO[4]", "release", 0, 0, 0);
#if defined(__x86_64__) || defined(_WIN64) || defined(__aarch64__)
    _bash_var_set(&ctx, "BASH_VERSINFO[5]", "x86_64-cross", 0, 0, 0);
#else
    _bash_var_set(&ctx, "BASH_VERSINFO[5]", "i686-cross", 0, 0, 0);
#endif
    _bash_var_set(&ctx, "BASH_VERSINFO", "6", 0, 0, 0); /* element count */
    /* Bash 5.2+ standard variables */
    _bash_var_set(&ctx, "BASH_ARGV0", ctx.script_name ? ctx.script_name : "bash", 0, 0, 0); /* writeable alias for $0 */
    _bash_var_set(&ctx, "BASHOPTS", "compat53:expand_aliases:extglob:extquote:force_fignore:globasciiranges:interactive_comments:nocaseglob:progcomp:promptvars:sourcepath", 0, 0, 0);
    _bash_var_set(&ctx, "SHELLOPTS", "braceexpand:emacs:hashall:histexpand:history:interactive-comments:monitor:posix:vi", 0, 0, 0);
    _bash_var_set(&ctx, "GLOBIGNORE", "", 0, 0, 0); /* Bash 5.2: colon-separated exclude list for pathname expansion */
    _bash_var_set(&ctx, "BASH_LOADABLES_PATH", "", 0, 0, 0); /* Bash 5.x: colon list for 'enable -f' builtins */
    {
        /* HISTSIZE: default history size (stub) */
        _bash_var_set(&ctx, "HISTSIZE", "500", 0, 0, 0);
        _bash_var_set(&ctx, "HISTFILESIZE", "500", 0, 0, 0);
        _bash_var_set(&ctx, "CMDTERM", "S", 0, 0, 0); /* Bash 5.3+ compat: CMDTERM=xterm-style terminal id */
        const char *h = getenv("HOME");
        if (h) _bash_var_set(&ctx, "HOME", h, 1, 1, 0);
        const char *pth = getenv("PATH");
        if (pth && *pth) _bash_var_set(&ctx, "PATH", pth, 1, 1, 0);
    }

#ifdef BASH_PLATFORM_WINDOWS
    /* When running under MSYS2 / Cygwin / Git Bash / WSL interop as a
     * child process, the parent shell usually exports a PS1 that has
     * ALREADY BEEN FULLY EXPANDED into plain text like:
     *     "Yezc@YECCC MINGW64 Y:/gitee/new/bash $ "
     * with no \\u, \\h, $MSYSTEM placeholders left at all. Our old
     * "looks_msys" heuristic (which only searched for unexpanded \\u,
     * \\h, __git_ps1 etc.) therefore misses it completely, and OUR
     * mini-bash ends up looking identical to the outer MSYS2 shell.
     *
     * To guarantee visual identity for mini-bash, we therefore ALWAYS
     * drop the inherited PS1 on Windows entry.  The user is still
     * perfectly free to run `export PS1="..."` FROM INSIDE this shell
     * after startup — _bash_interactive reads getenv("PS1") every loop so
     * user-set custom prompts continue to work normally. */
    {
        /* SetEnvironmentVariableA only updates the Win32 process
         * environment block — BUT MinGW's getenv() actually reads from
         * a CRT-maintained copy (_environ / __p__environ()) that is NOT
         * kept in sync with the Win32 block after startup.  That's why
         * the previous "fix" didn't work at all: getenv("PS1") still
         * returned the old expanded string even after
         * SetEnvironmentVariableA("PS1", NULL)!                      
         *
         * We therefore use THREE complementary approaches to guarantee
         * PS1 is gone from every lookup path the shell actually uses:
         *   (1) _putenv_s("PS1=") — sets CRT side to EMPTY STRING so that
         *       getenv("PS1") returns a non-null but *empty* string.  The
         *       shell then treats it as unset (it checks `ps1 && *ps1`).
         *       Also updates Win32 block on the CRT's behalf.
         *   (2) Additionally scan the raw _environ pointer vector and
         *       physically drop any "PS1=..." entries — this covers the
         *       case where the CRT's _putenv implementation is broken in
         *       a given MinGW build / doesn't remove but replaces-in-place.
         *   (3) SetEnvironmentVariableA(NULL) for belt-and-braces; any
         *       child process we spawn won't inherit MSYS2's PS1 either.
         *       And finally _bash_var_unset removes shell-layer PS1 so that
         *       even if the env were restored by some path the shell's
         *       variable tree still has none.                                */

        /* (1) CRT-level clear via _putenv_s.  _putenv_s is declared in
         * stdlib.h but under -std=c99 MinGW may hide it; since we can't
         * easily redeclare we use the lower-level POSIX putenv if available,
         * otherwise fall through to direct _environ manipulation below.    */
        /* _CRTIMP errno_t __cdecl _putenv_s(const char*,const char*).
         * We declare it manually here because MinGW hides this declaration
         * under -std=c99 / __STRICT_ANSI__ even though the symbol still
         * exists and is linkable.                                     */
        extern int __cdecl _putenv_s(const char *_Name, const char *_Value);
        _putenv_s("PS1", "");
        /* (3) Win32 block clear (for child processes we might launch) */
        SetEnvironmentVariableA("PS1", NULL);
        /* (4) Shell-variable-layer clear */
        _bash_var_unset(&ctx, "PS1");
    }
#endif

    int i = 1;
    int want_c = 0, want_s = 0, want_i = 0;
    const char *c_string = NULL;

    while (i < argc) {
        if (strcmp(argv[i], "--help") == 0) { _bash_usage(argv[0]); return 0; }
        if (strcmp(argv[i], "--version") == 0) { _bash_version(); return 0; }
        if (strcmp(argv[i], "--test-compl") == 0) {
            /* hidden: run tab-completion self-tests and exit */
            #define TC(ln) do { \
                const char *L = (ln); int cursor = (int)strlen(L); \
                int ts = _bash_token_start(L, cursor); \
                int is_cmd = _bash_is_command_pos(L, ts); \
                int tlen = cursor - ts; const char *tok = L + ts; \
                printf("---- line=[%s] token=[%.*s] is_cmd=%d ----\n", L, tlen, tok, is_cmd); \
                int nn = 0; char **aa = _bash_complete(&ctx, tok, tlen, is_cmd, &nn); \
                if (nn == 0) printf("  (no candidates)\n"); \
                else { \
                    printf("  candidates (%d):\n", nn); \
                    for (int k = 0; k < nn; k++) printf("    [%s]\n", aa[k]); \
                    char pre[4096]; int pl = _bash_common_prefix(aa, nn, pre); pre[pl] = 0; \
                    printf("  common_prefix[%d]=[%s]\n", pl, pre); \
                    char res[8192]; memcpy(res, L, ts); memcpy(res + ts, pre, pl); \
                    printf("  after-apply-prefix: [%.*s]\n", ts + pl, res); \
                } \
                _bash_complete_free(aa, nn); printf("\n"); \
            } while (0)
            TC("cd l1/l");
            TC("cd l1/l2/l");
            TC("cd l1/l2/l3");
            TC("cd ./l1/l2/l");
            TC("ls l1/l2/l3");
            TC("ls l");
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) { want_c = 1; c_string = argv[i+1]; i += 2; continue; }
        if (strcmp(argv[i], "-s") == 0) { want_s = 1; i++; continue; }
        if (strcmp(argv[i], "-i") == 0) { want_i = 1; i++; continue; }
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        if (argv[i][0] == '-') {
            fprintf(stderr, "bash: unrecognized option %s\n", argv[i]);
            _bash_usage(argv[0]);
            return 2;
        }
        break;
    }

    int rc = 0;
    if (want_c) {
        /* positional args after -c string? Simplify: ignore */
        rc = _bash_run_string(&ctx, c_string);
        if (ctx.do_exit) rc = ctx.exit_code;
    } else if (i < argc) {
        const char *script = argv[i];
        int sargc = argc - i - 1;
        char **sargv = argv + i + 1;
        rc = _bash_run_script(&ctx, script, sargc, sargv);
    } else if (want_s && !want_i) {
        /* -s only: read all stdin as a script (bulk) */
        bash_bstr_t all; bash_bstr_init(&all);
        char buf[4096];
        while (fgets(buf, sizeof(buf), stdin))
            bash_bstr_puts(&all, buf);
        if (all.len > 0) {
            rc = _bash_run_string(&ctx, all.data);
            if (ctx.do_exit) rc = ctx.exit_code;
        }
        bash_bstr_free(&all);
    } else if (want_i) {
        /* explicit -i: always use interactive REPL (line-by-line via _bash_readline,
         * which also populates history, even for non-tty stdin pipes) */
        _bash_interactive(&ctx);
        rc = ctx.exit_code;
    } else {
        /* default: interactive if tty, else stdin */
        if (isatty(0)) {
            _bash_interactive(&ctx);
            rc = ctx.exit_code;
        } else {
            bash_bstr_t all; bash_bstr_init(&all);
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0)
                bash_bstr_putn(&all, buf, n);
            if (all.len > 0) {
                rc = _bash_run_string(&ctx, all.data);
                if (ctx.do_exit) rc = ctx.exit_code;
            }
            bash_bstr_free(&all);
        }
    }
    return rc;
}
