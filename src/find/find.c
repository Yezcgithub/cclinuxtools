/**
 * @file find.c
 * @brief Cross-platform find command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common find(1) implementations.
 *
 * Key design features:
 *   - GNU find-compatible file search across a directory hierarchy
 *   - Direct system calls (FindFirstFile on Windows, opendir/lstat on POSIX)
 *     for maximum performance
 *   - Recursive-descent expression parser for AND/OR/NOT with parentheses
 *   - Glob matching with * ? [...] character classes (case-sensitive +
 *     case-insensitive option)
 *   - POSIX-style output paths (forward slashes, relative paths preserved)
 *   - Tests: -name/-iname/-path/-ipath/-regex/-iregex/-type/-size/-empty/
 *     -mtime/-atime/-ctime/-perm/-user/-group/-newer/-true/-false
 *   - Actions: -print/-print0/-ls/-delete/-exec/-ok/-prune/-quit
 *   - Options: -maxdepth/-mindepth, --help/--version
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o find.exe find.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o find find.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o find find.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o find find.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o find find.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o find find.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (c) 2025-2026 <Yezc/find>
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
    #define FIND_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define FIND_PLATFORM_LINUX   1
    #define FIND_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define FIND_PLATFORM_MACOS   1
    #define FIND_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define FIND_PLATFORM_FREEBSD 1
    #define FIND_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define FIND_PLATFORM_OPENBSD 1
    #define FIND_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define FIND_PLATFORM_NETBSD  1
    #define FIND_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define FIND_PLATFORM_POSIX   1
#else
    #define FIND_PLATFORM_POSIX   1
#endif

/* POSIX feature macros - must be defined before including any headers */
#ifdef FIND_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef FIND_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef FIND_PLATFORM_NETBSD
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
#include <inttypes.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef FIND_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFREG) != 0)
    #endif
    #ifndef S_ISLNK
        #define S_ISLNK(m) 0
    #endif
    #ifndef S_ISSOCK
        #define S_ISSOCK(m) 0
    #endif
#else /* FIND_PLATFORM_POSIX */
    #include <dirent.h>
    #include <unistd.h>
    #include <pwd.h>
    #include <grp.h>
    #include <regex.h>
    #include <limits.h>
    #ifndef S_ISSOCK
        #define S_ISSOCK(m) 0
    #endif
    #ifndef S_ISFIFO
        #define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
    #endif
#endif

/********************************
 *    defines
 ********************************/

#ifdef FIND_PLATFORM_WINDOWS
    /** @brief Native path separator character on Windows */
    #define PATH_SEP '\\'
    /** @brief Native path separator string on Windows */
    #define PATH_SEP_STR "\\"
    /** @brief Path list separator character on Windows */
    #define PATH_LIST_SEP ';'
#else
    /** @brief Native path separator character on POSIX */
    #define PATH_SEP '/'
    /** @brief Native path separator string on POSIX */
    #define PATH_SEP_STR "/"
    /** @brief Path list separator character on POSIX */
    #define PATH_LIST_SEP ':'
#endif

/** @brief Program version string */
#define FIND_VERSION_STR       "v1.0.0"

/** @brief Maximum directory recursion depth */
#define FIND_MAX_DEPTH         256

/** @brief Maximum path buffer length (bytes) */
#define FIND_MAX_PATH_LEN      4096

/** @brief Maximum number of starting paths */
#define FIND_MAX_PATHS         64

/** @brief Maximum number of expression tokens */
#define FIND_MAX_TOKENS        256

/** @brief Maximum length of owner/group name buffer */
#define FIND_OWNER_BUF_LEN     256

/** @brief Maximum length of permission string buffer */
#define FIND_PERMS_BUF_LEN     16

/** @brief Size of command execution buffer (bytes) */
#define FIND_CMD_BUF_SIZE      8192

/** @brief Default starting path when none is specified */
#define FIND_DEFAULT_PATH      "."

/** @brief Default file permissions on Windows (octal string) */
#define FIND_DEFAULT_PERMS_WIN "666"

/** @brief Default owner/group name placeholder when unavailable */
#define FIND_DEFAULT_OWNER     "root"

/** @brief Size of a kilobyte (1024 bytes) */
#define FIND_KB                1024LL

/** @brief Size of a megabyte (1024^2 bytes) */
#define FIND_MB                (1024LL * 1024LL)

/** @brief Size of a gigabyte (1024^3 bytes) */
#define FIND_GB                (1024LL * 1024LL * 1024LL)

/** @brief Size of a terabyte (1024^4 bytes) */
#define FIND_TB                (1024LL * 1024LL * 1024LL * 1024LL)

/** @brief Size of a petabyte (1024^5 bytes) */
#define FIND_PB                (1024LL * 1024LL * 1024LL * 1024LL * 1024LL)

/** @brief Seconds per minute */
#define FIND_SECS_PER_MINUTE   60LL

/** @brief Seconds per hour */
#define FIND_SECS_PER_HOUR     3600LL

/** @brief Seconds per day */
#define FIND_SECS_PER_DAY      86400LL

/** @brief Seconds per week (7 days) */
#define FIND_SECS_PER_WEEK     (7LL * 86400LL)

/** @brief Difference between FILETIME epoch (1601) and Unix epoch (1970) in 100ns intervals */
#define FIND_FILETIME_EPOCH_DIFF 116444736000000000ULL

/** @brief Number of 100-nanosecond intervals per second (FILETIME resolution) */
#define FIND_HNS_PER_SECOND      10000000ULL

/** @brief File type code: regular file */
#define FIND_MODE_FILE    'f'

/** @brief File type code: directory */
#define FIND_MODE_DIR     'd'

/** @brief File type code: symbolic link */
#define FIND_MODE_LINK    'l'

/** @brief File type code: block device */
#define FIND_MODE_BLOCK   'b'

/** @brief File type code: character device */
#define FIND_MODE_CHAR    'c'

/** @brief File type code: named pipe (FIFO) */
#define FIND_MODE_PIPE    'p'

/** @brief File type code: socket */
#define FIND_MODE_SOCKET  's'

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Cross-platform file metadata
 *
 * A unified file information structure that abstracts away the differences
 * between Windows (WIN32_FILE_ATTRIBUTE_DATA) and POSIX (struct stat).
 * All code that needs file info should use this struct instead of calling
 * platform APIs directly.
 */
typedef struct {
    int     exists;  /* 1 if file exists, 0 otherwise */
    char    mode;    /* 'f'=file, 'd'=dir, 'l'=link, 'b'=block, 'c'=char, 'p'=pipe, 's'=socket */
    int64_t size;    /* file size in bytes (0 for directories) */
    time_t  mtime;   /* last modification time (Unix epoch seconds) */
    time_t  atime;   /* last access time */
    time_t  ctime;   /* last status change / creation time */
    char    perms[FIND_PERMS_BUF_LEN];  /* permission string as 3-digit octal, e.g. "755" */
    char    owner[FIND_OWNER_BUF_LEN];  /* owner name (or uid as string if name unavailable) */
    char    group[FIND_OWNER_BUF_LEN];  /* group name (or gid as string if name unavailable) */
} file_stat_t;

/**
 * @brief Dynamically growing list of directory entry names
 *
 * Used to collect the contents of a directory before processing them.
 * Entries are stored as individually allocated C strings (strdup'd).
 */
typedef struct {
    char **entries;  /* array of entry name pointers */
    int    count;    /* number of entries currently stored */
    int    capacity; /* allocated capacity of entries array */
} dir_list_t;

/**
 * @brief Type of an expression tree node
 *
 * The find expression is parsed into an AST (abstract syntax tree).
 * Each node has a type that determines how it is evaluated.
 */
typedef enum {
    NODE_TEST,   /* leaf node: a test predicate (-name, -size, -type, etc.) */
    NODE_ACTION, /* leaf node: an action (-print, -delete, -exec, etc.) */
    NODE_NOT,    /* unary: logical NOT (negates child result) */
    NODE_AND,    /* binary: logical AND (short-circuit, left then right) */
    NODE_OR,     /* binary: logical OR (short-circuit, left then right) */
    NODE_TRUE,   /* leaf: always returns true (used as empty expression fallback) */
    NODE_FALSE   /* leaf: always returns false (used for unknown predicates) */
} node_type_t;

/**
 * @brief Types of test predicates
 *
 * Each TEST_* corresponds to a find test option. Used to dispatch in eval_test().
 */
typedef enum {
    TEST_NAME,   /* -name PATTERN       : glob match on filename */
    TEST_INAME,  /* -iname PATTERN      : case-insensitive glob on filename */
    TEST_PATH,   /* -path PATTERN       : glob match on full path */
    TEST_IPATH,  /* -ipath PATTERN      : case-insensitive glob on path */
    TEST_REGEX,  /* -regex PATTERN      : regex match on full path */
    TEST_IREGEX, /* -iregex PATTERN     : case-insensitive regex on path */
    TEST_TYPE,   /* -type [fdlbcps]     : file type check */
    TEST_SIZE,   /* -size [+-]N[ckMGTP] : file size check */
    TEST_MTIME,  /* -mtime [+-]N[smhdw] : modification time check */
    TEST_ATIME,  /* -atime [+-]N[smhdw] : access time check */
    TEST_CTIME,  /* -ctime [+-]N[smhdw] : change time check */
    TEST_EMPTY,  /* -empty              : empty file or directory */
    TEST_PERM,   /* -perm MODE          : permission mode check */
    TEST_USER,   /* -user NAME          : owner name check */
    TEST_GROUP,  /* -group NAME         : group name check */
    TEST_NEWER,  /* -newer FILE         : modified more recently than FILE */
    TEST_TRUE,   /* -true               : always true */
    TEST_FALSE   /* -false              : always false */
} test_type_t;

/**
 * @brief Types of actions
 *
 * Each ACTION_* corresponds to a find action. Actions have side effects
 * and their presence suppresses the default -print behavior.
 */
typedef enum {
    ACTION_PRINT,  /* -print              : print path with newline */
    ACTION_PRINT0, /* -print0             : print path with null separator */
    ACTION_LS,     /* -ls                 : list in ls -dils format */
    ACTION_DELETE, /* -delete             : delete matched files/dirs */
    ACTION_EXEC,   /* -exec CMD ;         : execute command */
    ACTION_OK,     /* -ok CMD ;           : execute with confirmation */
    ACTION_PRUNE,  /* -prune              : do not descend into directory */
    ACTION_QUIT    /* -quit               : exit immediately */
} action_type_t;

/**
 * @brief Expression tree node
 *
 * A tagged union: each node has a type, and depending on the type,
 * either test_data or action_data is valid.
 */
typedef struct node {
    node_type_t type;
    union {
        struct {
            test_type_t test;
            char *value;  /* pattern string, type char, perm string, etc. */
            int   sign;   /* for size/time: 0=exact, 1=+, -1=- */
            int64_t num;  /* parsed numeric value or newer_mtime */
        } test_data;
        struct {
            action_type_t action;
            char *cmd;  /* command template for -exec/-ok */
        } action_data;
    };
    struct node *left;    /* left child (AND, OR) */
    struct node *right;   /* right child (AND, OR) */
    struct node *operand; /* operand child (NOT) */
} node_t;

/**
 * @brief Result of parsing command-line arguments
 *
 * Contains the starting paths, the expression tree (if any), and
 * global options like maxdepth/mindepth.
 */
typedef struct {
    char **paths;   /* array of starting paths */
    int    npaths;  /* number of starting paths */
    node_t *tree;   /* root of the expression tree (may be NULL) */
    int    maxdepth;/* max descent depth (-1 = unlimited) */
    int    mindepth;/* min depth to apply tests (-1 = 0) */
} parsed_args_t;

/**
 * @brief Recursive descent parser state
 *
 * The find expression is parsed from a flat token array (argv-derived)
 * using a standard recursive descent parser with three levels:
 *   parse_or_expr    -> handles OR (lowest precedence)
 *   parse_and_expr   -> handles AND (implicit between adjacent tests/actions)
 *   parse_primary    -> handles individual tests, actions, NOT, and ( groups )
 */
typedef struct {
    char **tokens;  /* array of expression tokens */
    int    ntokens; /* number of tokens */
    int    pos;     /* current token position (consumption cursor) */
} parser_t;

/**
 * @brief Evaluation context for expression evaluation
 *
 * Carries side-effect flags back to the walker:
 *   prune: set to 1 if -prune was executed (don't descend)
 *   quit:  set to 1 if -quit was executed (stop everything)
 */
typedef struct {
    int prune;  /* 1 if -prune was triggered for this file */
    int quit;   /* 1 if -quit was triggered */
} eval_ctx_t;

/**
 * @brief Context for the directory tree walker
 *
 * Passed through recursive walk() calls. Contains the expression tree,
 * depth limits, and a flag indicating whether the default -print is
 * suppressed (i.e. any explicit action is present).
 */
typedef struct {
    int     maxdepth;   /* max recursion depth (-1 = unlimited) */
    int     mindepth;   /* min depth to apply tests/actions (-1 = 0) */
    node_t *tree;       /* root of expression tree (NULL = no expression) */
    int     has_print;  /* 1 if tree contains actions suppressing default print */
} walk_ctx_t;

/********************************
 *    static prototypes
 ********************************/
static void         _find_to_posix(char *path);
static const char * _find_get_filename(const char *path);
static int          _find_join_path(char *dst, size_t dstsize, const char *base,
                                    const char *name);
static int          _find_glob_match(const char *pattern, const char *str);
static int          _find_glob_match_ci(const char *pattern, const char *str);
static int          _find_regex_match(const char *pattern, const char *str, int icase);
static void         _find_stat_path(const char *posix_path, file_stat_t *st);
static void         _find_dir_list_init(dir_list_t *dl);
static void         _find_dir_list_add(dir_list_t *dl, const char *name);
static void         _find_dir_list_free(dir_list_t *dl);
static void         _find_read_directory(const char *posix_path, dir_list_t *dl);
static node_t *     _find_new_node(node_type_t type);
static void         _find_free_node(node_t *n);
static int64_t      _find_parse_size(const char *s, int *sign);
static int64_t      _find_parse_time(const char *s, int *sign);
static node_t *     _find_parse_or_expr(parser_t *p);
static node_t *     _find_parse_primary(parser_t *p);
static node_t *     _find_parse_and_expr(parser_t *p);
static int          _find_has_print_action(node_t *n);
static int          _find_eval_node(const char *path, const file_stat_t *st,
                                    node_t *node, eval_ctx_t *ctx);
static int          _find_eval_test(const char *path, const file_stat_t *st,
                                    node_t *node);
static void         _find_exec_cmd(const char *cmd, const char *path);
static int          _find_eval_action(const char *path, const file_stat_t *st,
                                      node_t *node, eval_ctx_t *ctx);
static void         _find_walk(const char *path, int depth, walk_ctx_t *wctx);
static void         _find_print_help(void);
static void         _find_print_version(void);
static int          _find_safe_copy(char *dst, const char *src, size_t dst_size);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for find_fputs / find_fputc.
 *        Defaults to libc @c stdout.
 *        Define externally to redirect all stream output.
 */
#ifndef find_out_stream
    #define find_out_stream stdout
#endif

/**
 * @brief Default error stream for find_err_printf.
 *        Defaults to libc @c stderr.
 */
#ifndef find_err_stream
    #define find_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef find_printf
    #define find_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to the error stream.
 */
#ifndef find_err_printf
    #define find_err_printf(fmt, ...) fprintf(find_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally find_out_stream)
 */
#ifndef find_fputs
    #define find_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      Character to write.
 * @param stream  stdio stream (normally find_out_stream)
 */
#ifndef find_fputc
    #define find_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/********************************
 *    static variables
 ********************************/

/* Global quit flag: set by -quit to stop all directory traversal */
static int g_quit = 0;

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the find command
 *
 * Workflow:
 *   1. Parse command-line arguments, separating paths from expression tokens
 *   2. Handle --help and --version
 *   3. Default to current directory "." if no paths given
 *   4. Parse the expression tokens into an AST via the recursive descent parser
 *   5. Determine whether default -print should be suppressed
 *   6. Walk each starting path, evaluating the expression tree for each file
 *   7. Free the expression tree and return
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success.
 */
int main(int argc, char **argv)
{
    /* Arrays to collect starting paths and expression tokens */
    char *paths[FIND_MAX_PATHS];
    int npaths = 0;
    char *expr_tokens[FIND_MAX_TOKENS];
    int nexpr = 0;
    int maxdepth = -1, mindepth = -1;
    int show_help = 0, show_version = 0;

    int i = 1;
    while (i < argc) {
        char *arg = argv[i];

        if (strcmp(arg, "-maxdepth") == 0 && i + 1 < argc) {
            maxdepth = atoi(argv[i + 1]);
            i += 2;
        }
        else if (strcmp(arg, "-mindepth") == 0 && i + 1 < argc) {
            mindepth = atoi(argv[i + 1]);
            i += 2;
        }
        else if (strcmp(arg, "-help") == 0 || strcmp(arg, "--help") == 0) {
            show_help = 1;
            i++;
        }
        else if (strcmp(arg, "-version") == 0 || strcmp(arg, "--version") == 0) {
            show_version = 1;
            i++;
        }
        else if (arg[0] == '-' || strcmp(arg, "(") == 0 || strcmp(arg, ")") == 0 ||
                 strcmp(arg, "!") == 0) {
            /* Expression token: add to token list and grab its argument if needed */
            expr_tokens[nexpr++] = arg;
            /* Tests/actions that take one argument: pre-fetch the next token */
            if (strcmp(arg, "-name") == 0 || strcmp(arg, "-iname") == 0 ||
                strcmp(arg, "-path") == 0 || strcmp(arg, "-ipath") == 0 ||
                strcmp(arg, "-regex") == 0 || strcmp(arg, "-iregex") == 0 ||
                strcmp(arg, "-type") == 0 || strcmp(arg, "-size") == 0 ||
                strcmp(arg, "-mtime") == 0 || strcmp(arg, "-atime") == 0 ||
                strcmp(arg, "-ctime") == 0 || strcmp(arg, "-perm") == 0 ||
                strcmp(arg, "-user") == 0 || strcmp(arg, "-group") == 0 ||
                strcmp(arg, "-newer") == 0) {
                if (i + 1 < argc) {
                    i++;
                    expr_tokens[nexpr++] = argv[i];
                }
            }
            else if (strcmp(arg, "-exec") == 0 || strcmp(arg, "-ok") == 0) {
                /* -exec/-ok: collect all tokens until the terminating ';' */
                i++;
                while (i < argc && strcmp(argv[i], ";") != 0) {
                    expr_tokens[nexpr++] = argv[i];
                    i++;
                }
                if (i < argc) {
                    expr_tokens[nexpr++] = argv[i]; /* the ; */
                }
            }
            i++;
        }
        else {
            /* Non-option argument: treat as a starting path */
            paths[npaths++] = arg;
            i++;
        }
    }

    if (show_help) {
        _find_print_help();
        return 0;
    }
    if (show_version) {
        _find_print_version();
        return 0;
    }

    if (npaths == 0) {
        paths[npaths++] = FIND_DEFAULT_PATH;
    }

    /* Parse expression tree */
    parser_t parser = { expr_tokens, nexpr, 0 };
    node_t *tree = _find_parse_or_expr(&parser);

    walk_ctx_t wctx = {
        .maxdepth = maxdepth,
        .mindepth = mindepth,
        .tree = tree,
        .has_print = _find_has_print_action(tree)
    };

    for (int p = 0; p < npaths && !g_quit; p++) {
        /* Normalize path to posix */
        char norm[FIND_MAX_PATH_LEN];
        if (_find_safe_copy(norm, paths[p], sizeof(norm)) != 0) {
            continue;
        }
        _find_to_posix(norm);
        _find_walk(norm, 0, &wctx);
    }

    _find_free_node(tree);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Safer string copy that cannot trigger truncation warnings.
 *
 * Uses memcpy followed by explicit NUL termination so compilers see the
 * bounded copy is safe and don't emit -Wstringop-truncation.
 *
 * @param dst       destination buffer
 * @param src       NUL-terminated source
 * @param dst_size  size of dst in bytes
 * @return 0 on success, -1 if dst_size is too small or input invalid
 */
static int _find_safe_copy(char *dst, const char *src, size_t dst_size)
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

/**
 * @brief Convert a path to POSIX style (forward slashes, no trailing slash)
 * @param path  path string modified in-place
 *
 * Converts all backslashes to forward slashes and removes trailing slashes
 * (except for the root "/"). Ensures consistent output format across
 * platforms, matching Linux/MSYS find behavior.
 */
static void _find_to_posix(char *path)
{
    if (!path) {
        return;
    }
    for (char *p = path; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    /* Remove trailing slash (but keep root "/") */
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
}

/**
 * @brief Extract the filename component from a path
 * @param path  input path string
 * @return pointer to the last component of the path (after the final '/'
 *         or '\\'). If no separator is found, returns the path itself.
 */
static const char *_find_get_filename(const char *path)
{
    if (!path) {
        return "";
    }
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

/**
 * @brief Concatenate base path and filename with POSIX separator
 * @param dst      output buffer
 * @param dstsize  size of output buffer
 * @param base     base directory path
 * @param name     filename to append
 * @return the number of characters written (excluding null terminator),
 *         or -1 on truncation (per snprintf semantics).
 *         Strips any trailing '/' from base before joining.
 */
static int _find_join_path(char *dst, size_t dstsize, const char *base, const char *name)
{
    if (!dst || dstsize == 0 || !base || !name) {
        return -1;
    }
    size_t blen = strlen(base);
    if (blen > 0 && base[blen - 1] == '/') {
        blen--;
    }
    return snprintf(dst, dstsize, "%.*s/%s", (int)blen, base, name);
}

/**
 * @brief Case-sensitive glob pattern matching
 * @param pattern  glob pattern (supports *, ?, [...] character classes)
 * @param str      string to match against
 * @return 1 if match, 0 otherwise
 *
 * Supported metacharacters:
 *   *     - matches any sequence of characters (including empty)
 *   ?     - matches any single character
 *   [...] - matches one character in the set; [a-z] for range, [!...] for negation
 *
 * Algorithm: recursive backtracking for '*' (greedy, tries longest match first
 * by advancing str one char at a time and recursively trying the rest).
 */
static int _find_glob_match(const char *pattern, const char *str)
{
    while (*pattern) {
        if (*pattern == '*') {
            /* Wildcard: zero or more characters. */
            pattern++;
            if (!*pattern) {
                return 1;
            }
            while (*str) {
                if (_find_glob_match(pattern, str)) {
                    return 1;
                }
                str++;
            }
            return _find_glob_match(pattern, str);
        }
        else if (*pattern == '?') {
            /* Single-character wildcard: requires exactly one char */
            if (!*str) {
                return 0;
            }
            pattern++;
            str++;
        }
        else if (*pattern == '[') {
            /* Character class: [abc], [a-z], [!abc] */
            if (!*str) {
                return 0;
            }
            pattern++;
            int negate = 0;
            if (*pattern == '!' || *pattern == '^') {
                negate = 1;
                pattern++;
            }
            int matched = 0;
            /* Iterate over characters/ranges inside the brackets */
            while (*pattern && *pattern != ']') {
                if (pattern[1] == '-' && pattern[2] && pattern[2] != ']') {
                    /* Range expression like "a-z" */
                    unsigned char lo = (unsigned char)pattern[0];
                    unsigned char hi = (unsigned char)pattern[2];
                    unsigned char c = (unsigned char)*str;
                    if (c >= lo && c <= hi) {
                        matched = 1;
                    }
                    pattern += 3;
                }
                else {
                    /* Single character */
                    if (*pattern == *str) {
                        matched = 1;
                    }
                    pattern++;
                }
            }
            if (*pattern == ']') {
                pattern++;  /* skip closing bracket */
            }
            if (negate) {
                matched = !matched;  /* apply negation */
            }
            if (!matched) {
                return 0;
            }
            str++;
        }
        else {
            /* Literal character: must match exactly */
            if (*pattern != *str) {
                return 0;
            }
            pattern++;
            str++;
        }
    }
    return *str == '\0';
}

/**
 * @brief Case-insensitive version of _find_glob_match
 * @param pattern  glob pattern
 * @param str  string to match against
 *
 * Same behavior as _find_glob_match but character comparisons are case-insensitive
 * (both pattern and string are lowered via tolower()). Character class ranges
 * are also compared case-insensitively.
 */
static int _find_glob_match_ci(const char *pattern, const char *str)
{
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) {
                return 1;
            }
            while (*str) {
                if (_find_glob_match_ci(pattern, str)) {
                    return 1;
                }
                str++;
            }
            return _find_glob_match_ci(pattern, str);
        }
        else if (*pattern == '?') {
            if (!*str) {
                return 0;
            }
            pattern++;
            str++;
        }
        else if (*pattern == '[') {
            if (!*str) {
                return 0;
            }
            pattern++;
            int negate = 0;
            if (*pattern == '!' || *pattern == '^') {
                negate = 1;
                pattern++;
            }
            int matched = 0;
            while (*pattern && *pattern != ']') {
                if (pattern[1] == '-' && pattern[2] && pattern[2] != ']') {
                    unsigned char lo = tolower((unsigned char)pattern[0]);
                    unsigned char hi = tolower((unsigned char)pattern[2]);
                    unsigned char c = tolower((unsigned char)*str);
                    if (c >= lo && c <= hi) {
                        matched = 1;
                    }
                    pattern += 3;
                }
                else {
                    if (tolower((unsigned char)*pattern) == tolower((unsigned char)*str)) {
                        matched = 1;
                    }
                    pattern++;
                }
            }
            if (*pattern == ']') {
                pattern++;
            }
            if (negate) {
                matched = !matched;
            }
            if (!matched) {
                return 0;
            }
            str++;
        }
        else {
            if (tolower((unsigned char)*pattern) != tolower((unsigned char)*str)) {
                return 0;
            }
            pattern++;
            str++;
        }
    }
    return *str == '\0';
}

/**
 * @brief Regular expression matching
 * @param pattern  regex pattern
 * @param str  string to match against
 * @param icase  if non-zero, perform case-insensitive matching
 *
 * On POSIX: uses POSIX regex (regcomp/regexec) for full regex support.
 * On Windows: falls back to glob matching (no native regex available).
 *
 * @return 1 if match, 0 otherwise.
 */
#ifndef FIND_PLATFORM_WINDOWS
static int _find_regex_match(const char *pattern, const char *str, int icase)
{
    if (!pattern || !str) {
        return 0;
    }
    regex_t re;
    int flags = REG_NOSUB;       /* no need for capture groups */
    if (icase) {
        flags |= REG_ICASE;
    }
    if (regcomp(&re, pattern, flags) != 0) {
        return 0;
    }
    int ret = regexec(&re, str, 0, NULL, 0);
    regfree(&re);
    return ret == 0;
}
#else /* FIND_PLATFORM_WINDOWS */
/* On Windows, fall back to glob matching for regex */
static int _find_regex_match(const char *pattern, const char *str, int icase)
{
    if (!pattern || !str) {
        return 0;
    }
    return _find_glob_match(pattern, str) || (icase && _find_glob_match_ci(pattern, str));
}
#endif /* FIND_PLATFORM_WINDOWS */

/**
 * @brief Get file information for a path (cross-platform)
 * @param posix_path  path in POSIX format (forward slashes)
 * @param st  output struct filled with file metadata
 *
 * On Windows: converts to backslash path, calls GetFileAttributesExA.
 * On Linux:   calls lstat (does not follow symlinks, like GNU find).
 *
 * The file mode ('f', 'd', etc.) and perms are set based on the platform.
 * On Windows, perms default to "666" and owner/group are empty strings.
 */
static void _find_stat_path(const char *posix_path, file_stat_t *st)
{
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    if (!posix_path) {
        return;
    }

#ifdef FIND_PLATFORM_WINDOWS
    /* Convert POSIX path to Windows backslash format for API calls */
    char native[MAX_PATH];
    (void)snprintf(native, sizeof(native), "%s", posix_path);
    for (char *p = native; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) {
        st->exists = 1;
        if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            st->mode = FIND_MODE_DIR;
            st->size = 0;  /* directory size is not meaningful on Windows */
        }
        else {
            st->mode = FIND_MODE_FILE;
            /* Combine 32-bit high/low parts into 64-bit size */
            st->size = ((int64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        }
        /*
         * Convert FILETIME (100-nanosecond intervals since Jan 1, 1601)
         * to Unix time_t (seconds since Jan 1, 1970).
         */
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        st->mtime = (time_t)((uli.QuadPart - FIND_FILETIME_EPOCH_DIFF) / FIND_HNS_PER_SECOND);
        uli.LowPart = fad.ftLastAccessTime.dwLowDateTime;
        uli.HighPart = fad.ftLastAccessTime.dwHighDateTime;
        st->atime = (time_t)((uli.QuadPart - FIND_FILETIME_EPOCH_DIFF) / FIND_HNS_PER_SECOND);
        uli.LowPart = fad.ftCreationTime.dwLowDateTime;
        uli.HighPart = fad.ftCreationTime.dwHighDateTime;
        st->ctime = (time_t)((uli.QuadPart - FIND_FILETIME_EPOCH_DIFF) / FIND_HNS_PER_SECOND);
        /* Windows has no Unix-style permissions; default to rw-rw-rw- */
        (void)_find_safe_copy(st->perms, FIND_DEFAULT_PERMS_WIN, sizeof(st->perms));
        /* owner/group are left empty (already zeroed by memset) */
    }
    else {
        st->exists = 0;
    }
#else /* FIND_PLATFORM_POSIX */
    /* Use lstat so we don't follow symlinks (matches GNU find behavior) */
    struct stat sb;
    if (lstat(posix_path, &sb) == 0) {
        st->exists = 1;
        st->size = (int64_t)sb.st_size;
        st->mtime = sb.st_mtime;
        st->atime = sb.st_atime;
        st->ctime = sb.st_ctime;

        /* Map POSIX file type to our single-char mode code */
        if (S_ISREG(sb.st_mode)) {
            st->mode = FIND_MODE_FILE;
        }
        else if (S_ISDIR(sb.st_mode)) {
            st->mode = FIND_MODE_DIR;
        }
        else if (S_ISLNK(sb.st_mode)) {
            st->mode = FIND_MODE_LINK;
        }
        else if (S_ISBLK(sb.st_mode)) {
            st->mode = FIND_MODE_BLOCK;
        }
        else if (S_ISCHR(sb.st_mode)) {
            st->mode = FIND_MODE_CHAR;
        }
        else if (S_ISFIFO(sb.st_mode)) {
            st->mode = FIND_MODE_PIPE;
        }
        else if (S_ISSOCK(sb.st_mode)) {
            st->mode = FIND_MODE_SOCKET;
        }
        else {
            st->mode = '?';
        }

        /* Format permission bits as 3-digit octal string */
        (void)snprintf(st->perms, sizeof(st->perms), "%03o", sb.st_mode & 0777);
        /* Resolve owner uid to name; fall back to numeric uid */
        struct passwd *pw = getpwuid(sb.st_uid);
        if (pw) {
            (void)snprintf(st->owner, sizeof(st->owner), "%s", pw->pw_name);
        }
        else {
            (void)snprintf(st->owner, sizeof(st->owner), "%d", sb.st_uid);
        }
        /* Resolve group gid to name; fall back to numeric gid */
        struct group *gr = getgrgid(sb.st_gid);
        if (gr) {
            (void)snprintf(st->group, sizeof(st->group), "%s", gr->gr_name);
        }
        else {
            (void)snprintf(st->group, sizeof(st->group), "%d", sb.st_gid);
        }
    }
    else {
        st->exists = 0;
    }
#endif /* FIND_PLATFORM_WINDOWS */
}

/**
 * @brief Initialize an empty directory list (always succeeds)
 * @param dl  directory list instance
 */
static void _find_dir_list_init(dir_list_t *dl)
{
    if (!dl) {
        return;
    }
    dl->entries = NULL;
    dl->count = 0;
    dl->capacity = 0;
}

/**
 * @brief Append an entry to the list, growing capacity as needed.
 *
 * Safe against allocation failure: on realloc/strdup OOM the existing
 * entries are preserved and the new entry is skipped (no leak, no deref).
 *
 * @param dl    directory list instance
 * @param name  entry name (will be strdup'd)
 */
static void _find_dir_list_add(dir_list_t *dl, const char *name)
{
    if (!dl || !name) {
        return;
    }
    if (dl->count >= dl->capacity) {
        int new_cap = dl->capacity ? dl->capacity * 2 : 32;
        char **tmp = (char **)realloc(dl->entries, (size_t)new_cap * sizeof(char *));
        if (!tmp) {
            return;  /* keep existing entries; skip this one */
        }
        dl->entries = tmp;
        dl->capacity = new_cap;
    }
    char *dup = strdup(name);
    if (!dup) {
        return;  /* skip this entry on OOM */
    }
    dl->entries[dl->count++] = dup;
}

/**
 * @brief Free all memory associated with a directory list
 * @param dl  directory list instance (may be NULL, may be called twice safely)
 */
static void _find_dir_list_free(dir_list_t *dl)
{
    if (!dl) {
        return;
    }
    for (int i = 0; i < dl->count; i++) {
        free(dl->entries[i]);
    }
    free(dl->entries);
    dl->entries = NULL;
    dl->count = 0;
    dl->capacity = 0;
}

/**
 * @brief Read all entries in a directory (cross-platform)
 * @param posix_path  directory path in POSIX format
 * @param dl  output list populated with entry names (excluding "." and "..")
 *
 * On Windows: uses FindFirstFileA/FindNextFileA with "\\*" wildcard.
 * On POSIX:   uses opendir/readdir/closedir.
 *
 * The caller is responsible for freeing the list with _find_dir_list_free().
 */
static void _find_read_directory(const char *posix_path, dir_list_t *dl)
{
    if (!dl) {
        return;
    }
    _find_dir_list_init(dl);
    if (!posix_path) {
        return;
    }

#ifdef FIND_PLATFORM_WINDOWS
    char pattern[MAX_PATH];
    (void)snprintf(pattern, sizeof(pattern), "%s\\*", posix_path);
    for (char *p = pattern; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
    WIN32_FIND_DATAA fd;
    HANDLE hfind = FindFirstFileA(pattern, &fd);
    if (hfind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        _find_dir_list_add(dl, fd.cFileName);
    } while (FindNextFileA(hfind, &fd));
    (void)FindClose(hfind);
#else /* FIND_PLATFORM_POSIX */
    DIR *dir = opendir(posix_path);
    if (!dir) {
        return;
    }
    struct dirent *de;
    while ((de = readdir(dir))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        _find_dir_list_add(dl, de->d_name);
    }
    (void)closedir(dir);
#endif /* FIND_PLATFORM_WINDOWS */
}

/**
 * @brief Allocate a new zero-initialized node of the given type
 * @param type  node type to allocate
 * @return new node, or NULL on allocation failure
 */
static node_t *_find_new_node(node_type_t type)
{
    node_t *n = (node_t *)calloc(1, sizeof(node_t));
    if (!n) {
        return NULL;
    }
    n->type = type;
    return n;
}

/**
 * @brief Recursively free an expression tree and all owned strings
 * @param n  root of expression tree (may be NULL)
 */
static void _find_free_node(node_t *n)
{
    if (!n) {
        return;
    }
    if (n->type == NODE_TEST) {
        free(n->test_data.value);
    }
    if (n->type == NODE_ACTION && n->action_data.cmd) {
        free(n->action_data.cmd);
    }
    _find_free_node(n->left);
    _find_free_node(n->right);
    _find_free_node(n->operand);
    free(n);
}

/**
 * @brief Parse a find-style size specification
 * @param s  size string, e.g. "+10k", "5M", "-100c"
 * @param sign  output: 0 for exact, 1 for "+", -1 for "-"
 *
 * Size suffixes: c=bytes, k=kilobytes(1024), M=megabytes,
 *                G=gigabytes, T=terabytes, P=petabytes
 * Default (no suffix) is bytes.
 *
 * @return size in bytes, or -1 on parse error.
 */
static int64_t _find_parse_size(const char *s, int *sign)
{
    if (!s || !sign) {
        return -1;
    }
    *sign = 0;
    const char *p = s;
    if (*p == '+') {
        *sign = 1;
        p++;
    }
    else if (*p == '-') {
        *sign = -1;
        p++;
    }

    char *endp;
    int64_t num = strtoll(p, &endp, 10);
    if (endp == p) {
        return -1;
    }

    int64_t mult = 1;
    switch (*endp) {
        case 'c':
            mult = 1;
            break;
        case 'k':
            mult = FIND_KB;
            break;
        case 'M':
            mult = FIND_MB;
            break;
        case 'G':
            mult = FIND_GB;
            break;
        case 'T':
            mult = FIND_TB;
            break;
        case 'P':
            mult = FIND_PB;
            break;
        case '\0':
            mult = 1;  /* default: bytes */
            break;
        default:
            return -1;
    }
    return num * mult;
}

/**
 * @brief Parse a find-style time specification
 * @param s  time string, e.g. "-7d", "+24h", "30m"
 * @param sign  output: 0 for exact, 1 for "+", -1 for "-"
 *
 * Time suffixes: s=seconds, m=minutes, h=hours, d=days, w=weeks
 * Default (no suffix) is days.
 *
 * @return time in seconds, or -1 on parse error.
 */
static int64_t _find_parse_time(const char *s, int *sign)
{
    if (!s || !sign) {
        return -1;
    }
    *sign = 0;
    const char *p = s;
    if (*p == '+') {
        *sign = 1;
        p++;
    }
    else if (*p == '-') {
        *sign = -1;
        p++;
    }

    char *endp;
    int64_t num = strtoll(p, &endp, 10);
    if (endp == p) {
        return -1;
    }

    switch (*endp) {
        case 's':
            break;
        case 'm':
            num *= FIND_SECS_PER_MINUTE;
            break;
        case 'h':
            num *= FIND_SECS_PER_HOUR;
            break;
        case 'd':
            num *= FIND_SECS_PER_DAY;
            break;
        case 'w':
            num *= FIND_SECS_PER_WEEK;
            break;
        case '\0':
            num *= FIND_SECS_PER_DAY;  /* default: days */
            break;
        default:
            return -1;
    }
    return num;
}

/**
 * @brief Parse a primary expression (leaf node or grouped expression)
 * @param p  parser state
 *
 * Handles:
 *   "(" <expr> ")"   - grouped sub-expression
 *   "!" / "-not"     - logical NOT of a primary
 *   -name, -type, ... - test predicates (take one argument)
 *   -print, -delete...  - actions (no argument)
 *   -exec / -ok      - actions with command arguments ending in ";"
 *   -true / -false    - constant tests
 *   -newer            - test with a file argument
 *   unknown options      - error, returns NODE_FALSE
 *   end of tokens - returns NODE_TRUE (empty expression = match all)
 */
static node_t *_find_parse_primary(parser_t *p)
{
    if (!p) {
        return _find_new_node(NODE_TRUE);
    }
    if (p->pos >= p->ntokens) {
        return _find_new_node(NODE_TRUE);
    }

    char *tok = p->tokens[p->pos];

    /* Parenthesized group: recurse into full expression parser */
    if (strcmp(tok, "(") == 0) {
        p->pos++;
        node_t *n = _find_parse_or_expr(p);
        if (p->pos < p->ntokens && strcmp(p->tokens[p->pos], ")") == 0) {
            p->pos++;
        }
        return n ? n : _find_new_node(NODE_TRUE);
    }

    /* Logical NOT: negates the following primary */
    if (strcmp(tok, "!") == 0 || strcmp(tok, "-not") == 0) {
        p->pos++;
        node_t *child = _find_parse_primary(p);
        node_t *n = _find_new_node(NODE_NOT);
        if (!n) {
            return child ? child : _find_new_node(NODE_TRUE);
        }
        n->operand = child;
        return n;
    }

    /* Tests that take a single argument (e.g. -name, -size, -type) */
    if (strcmp(tok, "-name") == 0 || strcmp(tok, "-iname") == 0 ||
        strcmp(tok, "-path") == 0 || strcmp(tok, "-ipath") == 0 ||
        strcmp(tok, "-regex") == 0 || strcmp(tok, "-iregex") == 0 ||
        strcmp(tok, "-type") == 0 || strcmp(tok, "-size") == 0 ||
        strcmp(tok, "-mtime") == 0 || strcmp(tok, "-atime") == 0 ||
        strcmp(tok, "-ctime") == 0 || strcmp(tok, "-perm") == 0 ||
        strcmp(tok, "-user") == 0 || strcmp(tok, "-group") == 0) {

        /* Validate that an argument is present */
        if (p->pos + 1 >= p->ntokens) {
            find_err_printf("find: %s: missing argument\n", tok);
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }

        node_t *n = _find_new_node(NODE_TEST);
        if (!n) {
            p->pos += 2;
            return _find_new_node(NODE_TRUE);
        }
        n->test_data.value = strdup(p->tokens[p->pos + 1]);
        if (!n->test_data.value) {
            _find_free_node(n);
            p->pos += 2;
            return _find_new_node(NODE_TRUE);
        }

        /* Dispatch by option name to set test type and parse numeric args */
        if (strcmp(tok, "-name") == 0) {
            n->test_data.test = TEST_NAME;
        }
        else if (strcmp(tok, "-iname") == 0) {
            n->test_data.test = TEST_INAME;
        }
        else if (strcmp(tok, "-path") == 0) {
            n->test_data.test = TEST_PATH;
        }
        else if (strcmp(tok, "-ipath") == 0) {
            n->test_data.test = TEST_IPATH;
        }
        else if (strcmp(tok, "-regex") == 0) {
            n->test_data.test = TEST_REGEX;
        }
        else if (strcmp(tok, "-iregex") == 0) {
            n->test_data.test = TEST_IREGEX;
        }
        else if (strcmp(tok, "-type") == 0) {
            n->test_data.test = TEST_TYPE;
        }
        else if (strcmp(tok, "-size") == 0) {
            n->test_data.test = TEST_SIZE;
            n->test_data.num = _find_parse_size(p->tokens[p->pos + 1], &n->test_data.sign);
        }
        else if (strcmp(tok, "-mtime") == 0) {
            n->test_data.test = TEST_MTIME;
            n->test_data.num = _find_parse_time(p->tokens[p->pos + 1], &n->test_data.sign);
        }
        else if (strcmp(tok, "-atime") == 0) {
            n->test_data.test = TEST_ATIME;
            n->test_data.num = _find_parse_time(p->tokens[p->pos + 1], &n->test_data.sign);
        }
        else if (strcmp(tok, "-ctime") == 0) {
            n->test_data.test = TEST_CTIME;
            n->test_data.num = _find_parse_time(p->tokens[p->pos + 1], &n->test_data.sign);
        }
        else if (strcmp(tok, "-perm") == 0) {
            n->test_data.test = TEST_PERM;
        }
        else if (strcmp(tok, "-user") == 0) {
            n->test_data.test = TEST_USER;
        }
        else if (strcmp(tok, "-group") == 0) {
            n->test_data.test = TEST_GROUP;
        }

        p->pos += 2;  /* consume option + argument */
        return n;
    }

    /* -empty: no-argument test for empty files/directories */
    if (strcmp(tok, "-empty") == 0) {
        node_t *n = _find_new_node(NODE_TEST);
        if (!n) {
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }
        n->test_data.test = TEST_EMPTY;
        p->pos++;
        return n;
    }

    /* Simple actions that take no arguments */
    if (strcmp(tok, "-print") == 0 || strcmp(tok, "-print0") == 0 ||
        strcmp(tok, "-ls") == 0 || strcmp(tok, "-delete") == 0 ||
        strcmp(tok, "-prune") == 0 || strcmp(tok, "-quit") == 0) {

        node_t *n = _find_new_node(NODE_ACTION);
        if (!n) {
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }
        if (strcmp(tok, "-print") == 0) {
            n->action_data.action = ACTION_PRINT;
        }
        else if (strcmp(tok, "-print0") == 0) {
            n->action_data.action = ACTION_PRINT0;
        }
        else if (strcmp(tok, "-ls") == 0) {
            n->action_data.action = ACTION_LS;
        }
        else if (strcmp(tok, "-delete") == 0) {
            n->action_data.action = ACTION_DELETE;
        }
        else if (strcmp(tok, "-prune") == 0) {
            n->action_data.action = ACTION_PRUNE;
        }
        else if (strcmp(tok, "-quit") == 0) {
            n->action_data.action = ACTION_QUIT;
        }
        p->pos++;
        return n;
    }

    /* -exec / -ok: collect tokens until terminating ';' */
    if (strcmp(tok, "-exec") == 0 || strcmp(tok, "-ok") == 0) {
        p->pos++;
        /* Collect command tokens separated by spaces until ';' with bounds checking */
        char cmd_buf[FIND_CMD_BUF_SIZE] = {0};
        int first = 1;
        while (p->pos < p->ntokens && strcmp(p->tokens[p->pos], ";") != 0) {
            const char *arg = p->tokens[p->pos];
            int needed = (first ? 0 : 1) + (int)strlen(arg);
            if ((int)strlen(cmd_buf) + needed >= (int)sizeof(cmd_buf) - 1) {
                find_err_printf("find: -exec command too long\n");
                break;
            }
            if (!first) {
                strncat(cmd_buf, " ", sizeof(cmd_buf) - strlen(cmd_buf) - 1);
            }
            strncat(cmd_buf, arg, sizeof(cmd_buf) - strlen(cmd_buf) - 1);
            first = 0;
            p->pos++;
        }
        if (p->pos < p->ntokens) {
            p->pos++; /* skip ';' */
        }

        node_t *n = _find_new_node(NODE_ACTION);
        if (!n) {
            return _find_new_node(NODE_TRUE);
        }
        n->action_data.action = (strcmp(tok, "-exec") == 0) ? ACTION_EXEC : ACTION_OK;
        n->action_data.cmd = strdup(cmd_buf);
        if (!n->action_data.cmd) {
            _find_free_node(n);
            return _find_new_node(NODE_TRUE);
        }
        return n;
    }

    /* -true: always-true test */
    if (strcmp(tok, "-true") == 0) {
        node_t *n = _find_new_node(NODE_TEST);
        if (!n) {
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }
        n->test_data.test = TEST_TRUE;
        p->pos++;
        return n;
    }

    /* -false: always-false test */
    if (strcmp(tok, "-false") == 0) {
        node_t *n = _find_new_node(NODE_TEST);
        if (!n) {
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }
        n->test_data.test = TEST_FALSE;
        p->pos++;
        return n;
    }

    /* -newer FILE: test if file was modified more recently than reference */
    if (strcmp(tok, "-newer") == 0) {
        if (p->pos + 1 >= p->ntokens) {
            find_err_printf("find: -newer: missing argument\n");
            p->pos++;
            return _find_new_node(NODE_TRUE);
        }
        node_t *n = _find_new_node(NODE_TEST);
        if (!n) {
            p->pos += 2;
            return _find_new_node(NODE_TRUE);
        }
        n->test_data.test = TEST_NEWER;
        n->test_data.value = strdup(p->tokens[p->pos + 1]);
        if (!n->test_data.value) {
            _find_free_node(n);
            p->pos += 2;
            return _find_new_node(NODE_TRUE);
        }
        /* Stat the reference file and store its mtime */
        file_stat_t ref_st;
        _find_stat_path(n->test_data.value, &ref_st);
        n->test_data.num = (int64_t)ref_st.mtime;
        p->pos += 2;
        return n;
    }

    /* Unknown option: report error, return false node (file won't match) */
    find_err_printf("find: unknown predicate `%s'\n", tok);
    p->pos++;
    return _find_new_node(NODE_FALSE);
}

/**
 * @brief Parse a sequence of AND-ed primary expressions
 * @param p  parser state
 *
 * AND is implicit between adjacent primaries. Explicit "-a"/"-and" is
 * also accepted but is just syntactic sugar. Stops when it sees:
 *   - "-o" / "-or" (OR belongs to the next higher precedence level)
 *   - ")" (end of parenthesized group)
 *   - end of tokens
 *
 * Builds a left-associative chain of NODE_AND nodes.
 */
static node_t *_find_parse_and_expr(parser_t *p)
{
    if (!p) {
        return _find_new_node(NODE_TRUE);
    }
    node_t *left = _find_parse_primary(p);

    while (p->pos < p->ntokens) {
        char *tok = p->tokens[p->pos];
        /* Stop at OR operators or closing paren (higher/lower precedence) */
        if (strcmp(tok, "-o") == 0 || strcmp(tok, "-or") == 0 ||
            strcmp(tok, ")") == 0) {
            break;
        }
        /* Explicit AND operator: consume and continue (same as implicit) */
        if (strcmp(tok, "-a") == 0 || strcmp(tok, "-and") == 0) {
            p->pos++;
            continue;
        }
        /* Implicit AND: combine with next primary */
        node_t *right = _find_parse_primary(p);
        node_t *n = _find_new_node(NODE_AND);
        if (!n) {
            _find_free_node(left);
            return right ? right : _find_new_node(NODE_TRUE);
        }
        n->left = left;
        n->right = right;
        left = n;
    }
    return left;
}

/**
 * @brief Parse a sequence of OR-ed AND expressions (top-level parser)
 * @param p  parser state
 *
 * Lowest precedence level. Handles "-o"/"-or" operators.
 * Builds a left-associative chain of NODE_OR nodes.
 * This is the entry point for the expression parser.
 */
static node_t *_find_parse_or_expr(parser_t *p)
{
    if (!p) {
        return _find_new_node(NODE_TRUE);
    }
    node_t *left = _find_parse_and_expr(p);

    while (p->pos < p->ntokens) {
        char *tok = p->tokens[p->pos];
        if (strcmp(tok, "-o") == 0 || strcmp(tok, "-or") == 0) {
            p->pos++;
            node_t *right = _find_parse_and_expr(p);
            node_t *n = _find_new_node(NODE_OR);
            if (!n) {
                _find_free_node(left);
                return right ? right : _find_new_node(NODE_TRUE);
            }
            n->left = left;
            n->right = right;
            left = n;
        }
        else {
            break;
        }
    }
    return left;
}

/**
 * @brief Check if the expression tree contains any action
 *        that should suppress the default -print behavior
 * @param n  root of expression tree (may be NULL)
 *
 * @return 1 if a side-effecting or printing action is present.
 * In GNU find, -prune and -quit do NOT suppress default print:
 *   - `find . -quit` prints the first entry then quits
 *   - `find . -delete` deletes silently (no default print)
 *
 * Recursively walks the full tree to find any action node.
 */
static int _find_has_print_action(node_t *n)
{
    if (!n) {
        return 0;
    }
    if (n->type == NODE_ACTION) {
        action_type_t a = n->action_data.action;
        return a != ACTION_PRUNE && a != ACTION_QUIT;
    }
    if (_find_has_print_action(n->left)) {
        return 1;
    }
    if (_find_has_print_action(n->right)) {
        return 1;
    }
    if (_find_has_print_action(n->operand)) {
        return 1;
    }
    return 0;
}

/**
 * @brief Evaluate a test predicate against a file
 * @param path  full path of the file (POSIX format)
 * @param st  file stat info
 * @param node  test node to evaluate
 *
 * Dispatches on the test type and returns 1 if the test passes, 0 otherwise.
 */
static int _find_eval_test(const char *path, const file_stat_t *st, node_t *node)
{
    if (!path || !st || !node) {
        return 0;
    }
    const char *name = _find_get_filename(path);

    switch (node->test_data.test) {
        case TEST_NAME:
            return _find_glob_match(node->test_data.value, name);
        case TEST_INAME:
            return _find_glob_match_ci(node->test_data.value, name);
        case TEST_PATH:
            return _find_glob_match(node->test_data.value, path);
        case TEST_IPATH:
            return _find_glob_match_ci(node->test_data.value, path);
        case TEST_REGEX:
            return _find_regex_match(node->test_data.value, path, 0);
        case TEST_IREGEX:
            return _find_regex_match(node->test_data.value, path, 1);
        case TEST_TYPE: {
            /* Compare file type character (f/d/l/b/c/p/s) */
            char t = node->test_data.value[0];
            return st->mode == t;
        }
        case TEST_SIZE: {
            /* Size comparison: sign=1 means >, sign=-1 means <, 0 means == */
            if (node->test_data.num < 0) {
                return 0;  /* parse error */
            }
            if (node->test_data.sign == 1) {
                return st->size > node->test_data.num;
            }
            if (node->test_data.sign == -1) {
                return st->size < node->test_data.num;
            }
            return st->size == node->test_data.num;
        }
        case TEST_MTIME: {
            /* Time since modification: compared in seconds from now.
               For exact match (sign==0), matches files modified within
               one day of the specified time (GNU find semantics). */
            time_t diff = time(NULL) - st->mtime;
            if (node->test_data.sign == 1) {
                return diff > node->test_data.num;
            }
            if (node->test_data.sign == -1) {
                return diff < node->test_data.num;
            }
            return diff >= node->test_data.num && diff < node->test_data.num + 86400;
        }
        case TEST_ATIME: {
            /* Same logic as mtime but for access time */
            time_t diff = time(NULL) - st->atime;
            if (node->test_data.sign == 1) {
                return diff > node->test_data.num;
            }
            if (node->test_data.sign == -1) {
                return diff < node->test_data.num;
            }
            return diff >= node->test_data.num && diff < node->test_data.num + 86400;
        }
        case TEST_CTIME: {
            /* Same logic as mtime but for change/creation time */
            time_t diff = time(NULL) - st->ctime;
            if (node->test_data.sign == 1) {
                return diff > node->test_data.num;
            }
            if (node->test_data.sign == -1) {
                return diff < node->test_data.num;
            }
            return diff >= node->test_data.num && diff < node->test_data.num + 86400;
        }
        case TEST_EMPTY: {
            /* For directories: empty means no entries (excluding . and ..)
               For files: empty means zero size */
            if (st->mode == 'd') {
                dir_list_t dl;
                _find_read_directory(path, &dl);
                int empty = (dl.count == 0);
                _find_dir_list_free(&dl);
                return empty;
            }
            return st->size == 0;
        }
        case TEST_PERM:
            return strcmp(st->perms, node->test_data.value) == 0;
        case TEST_USER:
            return strcmp(st->owner, node->test_data.value) == 0;
        case TEST_GROUP:
            return strcmp(st->group, node->test_data.value) == 0;
        case TEST_NEWER:
            /* True if file's mtime is strictly greater than reference file's mtime */
            return (int64_t)st->mtime > node->test_data.num;
        case TEST_TRUE:
            return 1;
        case TEST_FALSE:
            return 0;
    }
    return 1;
}

/**
 * @brief Execute a command with {} replaced by the file path
 * @param cmd  command template string (may contain "{}" placeholders)
 * @param path  path to substitute for "{}"
 *
 * Scans the command template, copying characters to a buffer, and
 * replaces each occurrence of "{}" with the full path.
 * On Windows, path separators in the final command are converted to
 * backslashes for native command compatibility.
 * The command is executed via system().
 */
static void _find_exec_cmd(const char *cmd, const char *path)
{
    if (!cmd || !path) {
        return;
    }
    char buf[FIND_CMD_BUF_SIZE];
    const char *p = cmd;
    int bi = 0;

    /* Build the final command string, substituting {} with the path */
    while (*p && bi < (int)sizeof(buf) - 1) {
        if (p[0] == '{' && p[1] == '}') {
            int len = snprintf(buf + bi, sizeof(buf) - bi, "%s", path);
            if (len < 0 || bi + len >= (int)sizeof(buf) - 1) {
                break;
            }
            bi += len;
            p += 2;
        }
        else {
            buf[bi++] = *p++;
        }
    }
    buf[bi] = '\0';

#ifdef FIND_PLATFORM_WINDOWS
    /* Use native path separators for execution */
    for (char *c = buf; *c; c++) {
        if (*c == '/') {
            *c = '\\';
        }
    }
#endif /* FIND_PLATFORM_WINDOWS */

    (void)system(buf);
}

/**
 * @brief Execute an action for a matched file
 * @param path  file path (POSIX format)
 * @param st  file stat info
 * @param node  action node to execute
 * @param ctx  evaluation context (for prune/quit side effects)
 *
 * Dispatches on action type. Each action returns 1 (success) to the
 * expression evaluator, allowing the expression to continue.
 * Side-effect actions (-prune, -quit) set flags in ctx so the walker
 * can respond appropriately.
 */
static int _find_eval_action(const char *path, const file_stat_t *st,
                             node_t *node, eval_ctx_t *ctx)
{
    if (!path || !st || !node || !ctx) {
        return 1;
    }
    switch (node->action_data.action) {
        case ACTION_PRINT:
            find_printf("%s\n", path);
            return 1;
        case ACTION_PRINT0:
            find_printf("%s", path);
            find_fputc('\0', find_out_stream);
            return 1;
        case ACTION_LS: {
            /* GNU find -ls format: inode blocks perm links owner group size month day HH:MM path */
            char timebuf[32] = "";
            if (st->mtime > 0) {
                struct tm *tm = localtime(&st->mtime);
                if (tm) {
                    /* GNU find uses "%b %e %H:%M"; %e is not portable (MSVCRT),
                       so format the day as space-padded manually */
                    char mon[8] = "";
                    strftime(mon, sizeof(mon), "%b", tm);
                    (void)snprintf(timebuf, sizeof(timebuf), "%s %2d %02d:%02d",
                                   mon, tm->tm_mday, tm->tm_hour, tm->tm_min);
                }
            }
            /* Build permission string like "drwxr-xr-x" */
            char permstr[12];
            permstr[0] = (st->mode == FIND_MODE_DIR) ? FIND_MODE_DIR :
                         (st->mode == FIND_MODE_LINK) ? FIND_MODE_LINK : '-';
            /* perms is a 3-digit octal string (e.g. "755"); parse as base 8 */
            int perms_val = (int)strtol(st->perms, NULL, 8);
            permstr[1] = (perms_val & 0400) ? 'r' : '-';
            permstr[2] = (perms_val & 0200) ? 'w' : '-';
            permstr[3] = (perms_val & 0100) ? 'x' : '-';
            permstr[4] = (perms_val & 0040) ? 'r' : '-';
            permstr[5] = (perms_val & 0020) ? 'w' : '-';
            permstr[6] = (perms_val & 0010) ? 'x' : '-';
            permstr[7] = (perms_val & 0004) ? 'r' : '-';
            permstr[8] = (perms_val & 0002) ? 'w' : '-';
            permstr[9] = (perms_val & 0001) ? 'x' : '-';
            permstr[10] = '\0';
            /* On Windows, owner/group are empty; use placeholders */
            const char *owner = st->owner[0] ? st->owner : FIND_DEFAULT_OWNER;
            const char *group = st->group[0] ? st->group : FIND_DEFAULT_OWNER;
            /* On Windows, we don't have inode/blocks; use 0 */
#ifdef FIND_PLATFORM_WINDOWS
            find_printf("      0  0 %s  1 %s %s %10" PRId64 " %s %s\n",
                        permstr, owner, group, (int64_t)st->size, timebuf, path);
#else /* FIND_PLATFORM_POSIX */
            find_printf("%8" PRIu64 " %4" PRId64 " %s  1 %s %s %10" PRId64 " %s %s\n",
                        (uint64_t)0, (int64_t)0, permstr,
                        owner, group, (int64_t)st->size, timebuf, path);
#endif /* FIND_PLATFORM_WINDOWS */
            return 1;
        }
        case ACTION_DELETE: {
#ifdef FIND_PLATFORM_WINDOWS
            char native[MAX_PATH];
            (void)snprintf(native, sizeof(native), "%s", path);
            for (char *c = native; *c; c++) {
                if (*c == '/') {
                    *c = '\\';
                }
            }
            if (st->mode == FIND_MODE_DIR) {
                char cmd[MAX_PATH + 32];
                (void)snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", native);
                (void)system(cmd);
            }
            else {
                (void)_unlink(native);
            }
#else /* FIND_PLATFORM_POSIX */
            if (st->mode == FIND_MODE_DIR) {
                char cmd[FIND_MAX_PATH_LEN + FIND_PERMS_BUF_LEN];
                (void)snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
                (void)system(cmd);
            }
            else {
                (void)unlink(path);
            }
#endif /* FIND_PLATFORM_WINDOWS */
            return 1;
        }
        case ACTION_EXEC:
            _find_exec_cmd(node->action_data.cmd, path);
            return 1;
        case ACTION_OK: {
            find_err_printf("< %s ... %s > ? ", path, node->action_data.cmd);
            int c = getchar();
            if (c == 'y' || c == 'Y') {
                _find_exec_cmd(node->action_data.cmd, path);
            }
            /* consume rest of line */
            while (c != '\n' && c != EOF) {
                c = getchar();
            }
            return 1;
        }
        case ACTION_PRUNE:
            ctx->prune = 1;
            return 1;
        case ACTION_QUIT:
            ctx->quit = 1;
            return 1;
    }
    return 1;
}

/**
 * @brief Evaluate an expression tree for a given file
 * @param path  file path (POSIX format)
 * @param st  file stat info
 * @param node  root of the expression tree to evaluate
 * @param ctx  evaluation context (output: prune/quit flags)
 *
 * Recursively evaluates the expression tree with short-circuit semantics:
 *   - AND: returns 0 immediately if left child is false
 *   - OR:  returns 1 immediately if left child is true
 *   - NOT: negates the result of its operand
 *
 * @return 1 if the file matches (expression is true), 0 otherwise.
 * Side effects (printing, deletion, etc.) happen during evaluation of
 * action nodes.
 */
static int _find_eval_node(const char *path, const file_stat_t *st,
                           node_t *node, eval_ctx_t *ctx)
{
    if (!node) {
        return 1;
    }

    switch (node->type) {
        case NODE_TRUE:
            return 1;
        case NODE_FALSE:
            return 0;
        case NODE_TEST:
            return _find_eval_test(path, st, node);
        case NODE_ACTION:
            return _find_eval_action(path, st, node, ctx);
        case NODE_NOT:
            return !_find_eval_node(path, st, node->operand, ctx);
        case NODE_AND:
            /* Short-circuit AND: if left is false, don't evaluate right */
            if (!_find_eval_node(path, st, node->left, ctx)) {
                return 0;
            }
            return _find_eval_node(path, st, node->right, ctx);
        case NODE_OR:
            /* Short-circuit OR: if left is true, don't evaluate right */
            if (_find_eval_node(path, st, node->left, ctx)) {
                return 1;
            }
            return _find_eval_node(path, st, node->right, ctx);
    }
    return 1;
}

/**
 * @brief Recursively walk a directory tree and evaluate the expression
 * @param path  current path being visited (POSIX format)
 * @param depth  current depth (0 for starting paths)
 * @param wctx  walker context
 *
 * Algorithm (depth-first, pre-order):
 *   1. Stat the current path
 *   2. If depth >= mindepth, evaluate the expression:
 *      - If true AND no explicit action was found, print the path
 *      - Check for quit/prune side effects
 *   3. If not maxdepth and is a directory and not pruned, recurse into children
 *
 * The default -print behavior matches GNU find: if the expression
 * contains no explicit actions (except -prune/-quit), a matching
 * file is printed.
 */
static void _find_walk(const char *path, int depth, walk_ctx_t *wctx)
{
    if (!path || !wctx) {
        return;
    }
    file_stat_t st;
    _find_stat_path(path, &st);
    if (!st.exists) {
        return;
    }

    /* Check if we should evaluate at this depth (mindepth gate) */
    int do_eval = (wctx->mindepth < 0 || depth >= wctx->mindepth);

    if (do_eval) {
        if (!wctx->tree) {
            /* No expression, just print (default behavior) */
            find_printf("%s\n", path);
        }
        else {
            eval_ctx_t ctx = {0};
            int result = _find_eval_node(path, &st, wctx->tree, &ctx);

            /* Default -print: only if expression matched and no
               explicit action was present in the tree */
            if (result && !wctx->has_print) {
                find_printf("%s\n", path);
            }

            /* -quit: set global flag and unwind all recursion */
            if (ctx.quit) {
                g_quit = 1;
                return;
            }

            /* -prune: skip descending into this directory */
            if (ctx.prune) {
                return;
            }
        }
    }

    /* Don't descend if maxdepth reached */
    if (wctx->maxdepth >= 0 && depth >= wctx->maxdepth) {
        return;
    }

    /* Only recurse into directories */
    if (st.mode != 'd') {
        return;
    }

    dir_list_t dl;
    _find_read_directory(path, &dl);

    /* Process each child entry, checking quit flag each iteration */
    for (int i = 0; i < dl.count; i++) {
        if (g_quit) {
            break;
        }
        char child[FIND_MAX_PATH_LEN];
        (void)_find_join_path(child, sizeof(child), path, dl.entries[i]);
        _find_walk(child, depth + 1, wctx);
    }

    _find_dir_list_free(&dl);
}

/**
 * @brief Print usage/help information (GNU find-style)
 */
static void _find_print_help(void)
{
    find_printf(
        "Usage: find [path...] [expression]\n"
        "\n"
        "Search for files in a directory hierarchy.\n"
        "\n"
        "Tests:\n"
        "  -name PATTERN      Match file name (glob)\n"
        "  -iname PATTERN     Match file name case-insensitively\n"
        "  -path PATTERN      Match path (glob)\n"
        "  -ipath PATTERN     Match path case-insensitively\n"
        "  -regex PATTERN     Match path (regex)\n"
        "  -iregex PATTERN    Match path case-insensitively\n"
        "  -type [fdlbcps]    File type: f=file, d=dir, l=link, b=block, c=char, p=pipe, s=socket\n"
        "  -size [+-]N[ckMGTP]  File size\n"
        "  -empty             Empty file or directory\n"
        "  -mtime [+-]N[smhdw]  Modification time\n"
        "  -atime [+-]N[smhdw]  Access time\n"
        "  -ctime [+-]N[smhdw]  Change time\n"
        "  -perm MODE         Permissions\n"
        "  -user NAME         Owner\n"
        "  -group NAME        Group\n"
        "  -newer FILE        Modified more recently than FILE\n"
        "  -true              Always true\n"
        "  -false             Always false\n"
        "\n"
        "Actions:\n"
        "  -print             Print path (default)\n"
        "  -print0            Print with null separator\n"
        "  -ls                List in ls -dils format\n"
        "  -delete            Delete matched entries\n"
        "  -exec CMD ;        Execute command ({} = path)\n"
        "  -ok CMD ;          Execute with confirmation\n"
        "  -prune             Do not descend into directory\n"
        "  -quit              Exit immediately\n"
        "\n"
        "Operators:\n"
        "  ( )                Grouping\n"
        "  -not, !            Negate\n"
        "  -and, -a           AND (default)\n"
        "  -or, -o            OR\n"
        "\n"
        "Options:\n"
        "  -maxdepth N        Maximum descent depth\n"
        "  -mindepth N        Do not apply tests before depth N\n"
        "\n"
        "Examples:\n"
        "  find . -name \"*.c\"\n"
        "  find /tmp -type f -size +1M\n"
        "  find . -mtime -7\n"
        "  find . -name \"*.c\" -exec cat {} \\;\n"
        "  find . -type d -name \".git\" -prune -o -type f -print\n"
    );
}

/**
 * @brief Print version information
 */
static void _find_print_version(void)
{
    find_printf("find %s\n", FIND_VERSION_STR);
    find_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    find_printf("%s", "License MIT: <https://mit-license.org/>\n");
    find_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    find_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
