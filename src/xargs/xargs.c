/**
 * @file xargs.c
 * @brief Cross-platform implementation of the xargs command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU xargs (findutils 4.9+).
 *
 * Key behaviors:
 *   - -0, --null:               input items are NUL-terminated
 *   - -a, --arg-file=FILE:      read items from FILE instead of stdin
 *   - -d, --delimiter=CHAR:     input item delimiter
 *   - -E EOF-STR:               set end-of-file string
 *   - -I REPLACE-STR:           replace occurrences of REPLACE-STR in args
 *   - -L MAX-LINES:             max lines per command line
 *   - -n MAX-ARGS:              max arguments per command line
 *   - -P MAX-PROCS:             max parallel processes
 *   - -r, --no-run-if-empty:    don't run if no input
 *   - -s MAX-CHARS:             max chars per command line
 *   - -t, --verbose:            print command to stderr before executing
 *   - -p, --interactive:        prompt before each command
 *   - -x, --exit:               exit if size exceeded
 *   - --help / --version:       display help or version information
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o xargs.exe xargs.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o xargs xargs.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o xargs xargs.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o xargs xargs.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o xargs xargs.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o xargs xargs.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/xargs>
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
    #define XARGS_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define XARGS_PLATFORM_LINUX   1
    #define XARGS_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define XARGS_PLATFORM_MACOS   1
    #define XARGS_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define XARGS_PLATFORM_FREEBSD 1
    #define XARGS_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define XARGS_PLATFORM_OPENBSD 1
    #define XARGS_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define XARGS_PLATFORM_NETBSD  1
    #define XARGS_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define XARGS_PLATFORM_POSIX   1
#else
    #define XARGS_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef XARGS_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef XARGS_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef XARGS_PLATFORM_NETBSD
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
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#ifdef XARGS_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <process.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <signal.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define XARGS_VERSION_STR "v1.0.0"

/** @brief Default maximum command line size */
#define XARGS_DEFAULT_MAX_CHARS 131072

/** @brief Read buffer size */
#define XARGS_BUF_SIZE 65536

/** @brief Initial token buffer size */
#define XARGS_TOKEN_INIT 256

/** @brief Initial argv capacity */
#define XARGS_ARGV_INIT 64

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options structure for xargs
 */
typedef struct {
    bool null_mode;          /**< -0: NUL-terminated input */
    const char * arg_file;   /**< -a: input file */
    int delimiter;           /**< -d: delimiter character (-1 if not set) */
    const char * eof_str;    /**< -E: end-of-file string */
    const char * replace_str;/**< -I: replacement string */
    int max_lines;           /**< -L: max lines per command (-1 = unlimited) */
    int max_args;            /**< -n: max args per command (-1 = unlimited) */
    int max_procs;           /**< -P: max parallel processes */
    bool no_run_if_empty;    /**< -r: don't run if empty */
    int max_chars;           /**< -s: max chars per command line */
    bool verbose;            /**< -t: print command before executing */
    bool interactive;        /**< -p: prompt before each command */
    bool exit_on_size;       /**< -x: exit if size exceeded */
} xargs_opts;

/**
 * @brief Batch state for accumulating arguments
 */
typedef struct {
    char ** argv;            /**< Argument vector */
    int argc;                /**< Current arg count */
    int argv_cap;            /**< Allocated capacity */
    int args_in_batch;       /**< Number of args from input in this batch */
    int chars_in_batch;      /**< Total chars in this batch */
    int lines_in_batch;      /**< Number of lines in this batch */
    char ** command;         /**< Command + initial args */
    int num_initial;         /**< Number of initial args */
    int base_size;           /**< Total size of initial args + NULs */
    int exit_code;           /**< Cumulative exit code */
    const xargs_opts * opts; /**< Options */
} xargs_batch;

/**
 * @brief Parser state for input tokenization
 */
typedef struct {
    char * token;            /**< Token buffer */
    size_t tok_len;          /**< Current token length */
    size_t tok_cap;          /**< Token buffer capacity */
    bool any_items;          /**< Whether any items have been seen */
    bool eof_reached;        /**< Whether EOF string was encountered */
    bool in_single_quote;    /**< Inside single quotes */
    bool in_double_quote;    /**< Inside double quotes */
    bool backslash;          /**< Backslash escape active */
    xargs_batch * batch;     /**< Pointer to batch */
    const xargs_opts * opts; /**< Options pointer */
} xargs_parser;

#ifdef XARGS_PLATFORM_POSIX
/**
 * @brief Parallel process tracker
 */
typedef struct {
    pid_t * pids;            /**< Array of child PIDs */
    int count;               /**< Current number of children */
    int capacity;            /**< Allocated capacity */
    int max_procs;           /**< Max parallel processes */
    int exit_code;           /**< Cumulative exit code */
} xargs_parallel;
#endif

/********************************
 *    static prototypes
 ********************************/
static void _xargs_print_help(void);
static void _xargs_print_version(void);
static bool _xargs_streq(const char * a, const char * b);
static int  _xargs_exec(char ** argv);
static char ** _xargs_argv_grow(char ** argv, int * capacity, int needed);
static bool _xargs_replace_all(const char * str, const char * rep,
                               const char * with, char * out, size_t out_size);
static void _xargs_batch_init(xargs_batch * b, const xargs_opts * opts,
                              char ** command, int num_initial);
static void _xargs_batch_free(xargs_batch * b);
static void _xargs_batch_reset(xargs_batch * b);
static void _xargs_batch_free_tokens(xargs_batch * b);
static int  _xargs_batch_flush(xargs_batch * b);
static int  _xargs_batch_add(xargs_batch * b, const char * token, size_t tok_len);
static void _xargs_parser_init(xargs_parser * p, xargs_batch * b,
                               const xargs_opts * opts);
static void _xargs_parser_free(xargs_parser * p);
static int  _xargs_parser_finalize(xargs_parser * p);
static int  _xargs_parser_grow(xargs_parser * p);
static int  _xargs_run(const xargs_opts * opts, char ** command,
                       int num_initial_args);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream.
 *        Defaults to libc @c stdout .
 */
#ifndef xargs_out_stream
    #define xargs_out_stream stdout
#endif

/**
 * @brief Default error stream.
 *        Defaults to libc @c stderr .
 */
#ifndef xargs_err_stream
    #define xargs_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 */
#ifndef xargs_printf
    #define xargs_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef xargs_err_printf
    #define xargs_err_printf(fmt, ...) fprintf(xargs_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 */
#ifndef xargs_fflush
    #define xargs_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free wrapper.
 */
#ifndef xargs_safe_free
    #define xargs_safe_free(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the xargs command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Determine command and initial arguments
 *   3. Read input items and execute commands
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
#ifdef XARGS_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    xargs_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.delimiter = -1;
    opts.max_lines = -1;
    opts.max_args = -1;
    opts.max_procs = 1;
    opts.max_chars = XARGS_DEFAULT_MAX_CHARS;

    int cmd_start = argc;

    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_xargs_streq(arg, "--")) {
            cmd_start = i + 1;
            break;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_xargs_streq(arg, "--help")) {
                _xargs_print_help();
                return 0;
            }
            if (_xargs_streq(arg, "--version")) {
                _xargs_print_version();
                return 0;
            }
            if (_xargs_streq(arg, "--null")) {
                opts.null_mode = true;
                continue;
            }
            if (_xargs_streq(arg, "--no-run-if-empty")) {
                opts.no_run_if_empty = true;
                continue;
            }
            if (_xargs_streq(arg, "--verbose")) {
                opts.verbose = true;
                continue;
            }
            if (_xargs_streq(arg, "--interactive")) {
                opts.interactive = true;
                opts.verbose = true;
                continue;
            }
            if (_xargs_streq(arg, "--exit")) {
                opts.exit_on_size = true;
                continue;
            }
            if (strncmp(arg, "--arg-file=", 11) == 0) {
                opts.arg_file = arg + 11;
                continue;
            }
            if (_xargs_streq(arg, "--arg-file")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--arg-file' requires an argument\n");
                    return 1;
                }
                opts.arg_file = argv[++i];
                continue;
            }
            if (strncmp(arg, "--delimiter=", 12) == 0) {
                opts.delimiter = (unsigned char)arg[12];
                continue;
            }
            if (_xargs_streq(arg, "--delimiter")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--delimiter' requires an argument\n");
                    return 1;
                }
                opts.delimiter = (unsigned char)argv[++i][0];
                continue;
            }
            if (strncmp(arg, "--replace=", 10) == 0) {
                opts.replace_str = arg + 10;
                opts.max_lines = 1;
                opts.exit_on_size = true;
                continue;
            }
            if (_xargs_streq(arg, "--replace")) {
                opts.replace_str = "{}";
                opts.max_lines = 1;
                opts.exit_on_size = true;
                continue;
            }
            if (strncmp(arg, "--max-args=", 11) == 0) {
                opts.max_args = atoi(arg + 11);
                continue;
            }
            if (_xargs_streq(arg, "--max-args")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--max-args' requires an argument\n");
                    return 1;
                }
                opts.max_args = atoi(argv[++i]);
                continue;
            }
            if (strncmp(arg, "--max-lines=", 12) == 0) {
                opts.max_lines = atoi(arg + 12);
                opts.exit_on_size = true;
                continue;
            }
            if (_xargs_streq(arg, "--max-lines")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--max-lines' requires an argument\n");
                    return 1;
                }
                opts.max_lines = atoi(argv[++i]);
                opts.exit_on_size = true;
                continue;
            }
            if (strncmp(arg, "--max-procs=", 12) == 0) {
                opts.max_procs = atoi(arg + 12);
                continue;
            }
            if (_xargs_streq(arg, "--max-procs")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--max-procs' requires an argument\n");
                    return 1;
                }
                opts.max_procs = atoi(argv[++i]);
                continue;
            }
            if (strncmp(arg, "--max-chars=", 12) == 0) {
                opts.max_chars = atoi(arg + 12);
                continue;
            }
            if (_xargs_streq(arg, "--max-chars")) {
                if (i + 1 >= argc) {
                    xargs_err_printf("xargs: option '--max-chars' requires an argument\n");
                    return 1;
                }
                opts.max_chars = atoi(argv[++i]);
                continue;
            }

            xargs_err_printf("xargs: unrecognized option '%s'\n", arg);
            xargs_err_printf("Try 'xargs --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case '0':
                        opts.null_mode = true;
                        p++;
                        break;

                    case 'r':
                        opts.no_run_if_empty = true;
                        p++;
                        break;

                    case 't':
                        opts.verbose = true;
                        p++;
                        break;

                    case 'x':
                        opts.exit_on_size = true;
                        p++;
                        break;

                    case 'p':
                        opts.interactive = true;
                        opts.verbose = true;
                        p++;
                        break;

                    case 'a':
                        if (p[1] != '\0') {
                            opts.arg_file = p + 1;
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'a'\n");
                            return 1;
                        }
                        else {
                            opts.arg_file = argv[++i];
                            p = "";
                        }
                        break;

                    case 'd':
                        if (p[1] != '\0') {
                            opts.delimiter = (unsigned char)p[1];
                            p += 2;
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'd'\n");
                            return 1;
                        }
                        else {
                            opts.delimiter = (unsigned char)argv[++i][0];
                            p = "";
                        }
                        break;

                    case 'E':
                        if (p[1] != '\0') {
                            opts.eof_str = p + 1;
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'E'\n");
                            return 1;
                        }
                        else {
                            opts.eof_str = argv[++i];
                            p = "";
                        }
                        break;

                    case 'I':
                        if (p[1] != '\0') {
                            opts.replace_str = p + 1;
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'I'\n");
                            return 1;
                        }
                        else {
                            opts.replace_str = argv[++i];
                            p = "";
                        }
                        opts.max_lines = 1;
                        opts.exit_on_size = true;
                        break;

                    case 'L':
                        if (p[1] != '\0') {
                            opts.max_lines = atoi(p + 1);
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'L'\n");
                            return 1;
                        }
                        else {
                            opts.max_lines = atoi(argv[++i]);
                            p = "";
                        }
                        opts.exit_on_size = true;
                        break;

                    case 'n':
                        if (p[1] != '\0') {
                            opts.max_args = atoi(p + 1);
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'n'\n");
                            return 1;
                        }
                        else {
                            opts.max_args = atoi(argv[++i]);
                            p = "";
                        }
                        break;

                    case 'P':
                        if (p[1] != '\0') {
                            opts.max_procs = atoi(p + 1);
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 'P'\n");
                            return 1;
                        }
                        else {
                            opts.max_procs = atoi(argv[++i]);
                            p = "";
                        }
                        break;

                    case 's':
                        if (p[1] != '\0') {
                            opts.max_chars = atoi(p + 1);
                            p += strlen(p);
                        }
                        else if (i + 1 >= argc) {
                            xargs_err_printf("xargs: option requires an argument -- 's'\n");
                            return 1;
                        }
                        else {
                            opts.max_chars = atoi(argv[++i]);
                            p = "";
                        }
                        break;

                    default:
                        xargs_err_printf("xargs: invalid option -- '%c'\n", *p);
                        xargs_err_printf("Try 'xargs --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — start of command */
        cmd_start = i;
        break;
    }

    /* Determine command and initial arguments */
    char ** command = NULL;
    int num_initial_args = 0;

    if (cmd_start < argc) {
        command = &argv[cmd_start];
        num_initial_args = argc - cmd_start;
    }
    else {
        /* Default command: echo */
        static char * default_cmd[2];
        default_cmd[0] = "echo";
        default_cmd[1] = NULL;
        command = default_cmd;
        num_initial_args = 1;
    }

    /* Run xargs main loop */
    int rc = _xargs_run(&opts, command, num_initial_args);

    xargs_fflush(xargs_out_stream);
    xargs_fflush(xargs_err_stream);
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal
 */
static bool _xargs_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/**
 * @brief Grow an argv array if needed.
 * @param argv      Current array pointer
 * @param capacity  Current capacity (pointer to int, updated on realloc)
 * @param needed    Required capacity
 * @return New array pointer, or NULL on failure
 */
static char ** _xargs_argv_grow(char ** argv, int * capacity, int needed)
{
    if (needed <= *capacity) {
        return argv;
    }
    int new_cap = *capacity * 2;
    if (new_cap < needed) {
        new_cap = needed;
    }
    if (new_cap < 16) {
        new_cap = 16;
    }
    char ** new_argv = (char **)realloc(argv, (size_t)new_cap * sizeof(char *));
    if (!new_argv) {
        return NULL;
    }
    *capacity = new_cap;
    return new_argv;
}

/**
 * @brief Execute a command (cross-platform).
 * @param argv  NULL-terminated argument vector
 * @return Exit code of the command
 */
static int _xargs_exec(char ** argv)
{
#ifdef XARGS_PLATFORM_WINDOWS
    int result = _spawnvp(_P_WAIT, argv[0], (const char * const *)argv);
    if (result < 0) {
        xargs_err_printf("xargs: %s: %s\n", argv[0], strerror(errno));
        return 127;
    }
    return result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        xargs_err_printf("xargs: fork: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        /* Child */
        execvp(argv[0], argv);
        fprintf(stderr, "xargs: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    /* Parent */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        xargs_err_printf("xargs: waitpid: %s\n", strerror(errno));
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
#endif
}

/**
 * @brief Replace all occurrences of replace_str in a string.
 * @param str       Original string
 * @param rep       String to replace (e.g. "{}")
 * @param with      Replacement string
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _xargs_replace_all(const char * str, const char * rep,
                               const char * with, char * out, size_t out_size)
{
    size_t rep_len = strlen(rep);
    size_t with_len = strlen(with);
    size_t pos = 0;

    while (*str && pos + 1 < out_size) {
        if (strncmp(str, rep, rep_len) == 0) {
            if (pos + with_len + 1 > out_size) {
                return false;
            }
            memcpy(out + pos, with, with_len);
            pos += with_len;
            str += rep_len;
        }
        else {
            out[pos++] = *str++;
        }
    }
    out[pos] = '\0';
    return true;
}

/**
 * @brief Initialize a batch state.
 * @param b             Batch state
 * @param opts          Options
 * @param command       Command + initial args
 * @param num_initial   Number of initial args
 */
static void _xargs_batch_init(xargs_batch * b, const xargs_opts * opts,
                              char ** command, int num_initial)
{
    b->opts = opts;
    b->command = command;
    b->num_initial = num_initial;
    b->argv_cap = XARGS_ARGV_INIT;
    b->argv = (char **)malloc((size_t)b->argv_cap * sizeof(char *));
    b->exit_code = 0;

    /* Calculate base size */
    b->base_size = 0;
    for (int i = 0; i < num_initial; i++) {
        b->base_size += (int)strlen(command[i]) + 1;
    }

    _xargs_batch_reset(b);
}

/**
 * @brief Free a batch state.
 * @param b  Batch state
 */
static void _xargs_batch_free(xargs_batch * b)
{
    if (b->argv) {
        free(b->argv);
        b->argv = NULL;
    }
}

/**
 * @brief Free all dynamically allocated tokens in argv (beyond initial args).
 * @param b  Batch state
 */
static void _xargs_batch_free_tokens(xargs_batch * b)
{
    if (!b->argv) {
        return;
    }
    for (int i = b->num_initial; i < b->argc; i++) {
        if (b->argv[i]) {
            free(b->argv[i]);
            b->argv[i] = NULL;
        }
    }
}

/**
 * @brief Reset batch to initial state (command + initial args).
 * @param b  Batch state
 */
static void _xargs_batch_reset(xargs_batch * b)
{
    /* Free any dynamically allocated tokens */
    _xargs_batch_free_tokens(b);

    b->argc = 0;
    b->args_in_batch = 0;
    b->chars_in_batch = b->base_size;
    b->lines_in_batch = 0;

    b->argv = _xargs_argv_grow(b->argv, &b->argv_cap, b->num_initial + 1);
    for (int i = 0; i < b->num_initial; i++) {
        b->argv[i] = b->command[i];
    }
    b->argc = b->num_initial;
}

/**
 * @brief Print argv to stderr for verbose mode.
 * @param argv   Argument vector
 * @param argc   Number of arguments
 */
static void _xargs_print_cmd(char ** argv, int argc)
{
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            xargs_err_printf(" ");
        }
        xargs_err_printf("%s", argv[i]);
    }
    xargs_err_printf("\n");
}

/**
 * @brief Flush current batch: execute command and reset.
 * @param b  Batch state
 * @return 0 on success
 */
static int _xargs_batch_flush(xargs_batch * b)
{
    if (b->argc == 0) {
        return 0;
    }

    b->argv[b->argc] = NULL;
    const xargs_opts * opts = b->opts;

    /* If -I mode, do replacement on all initial args */
    if (opts->replace_str) {
        /* In -I mode, there should be exactly one token from input */
        const char * token = (b->args_in_batch > 0) ? b->argv[b->num_initial] : "";
        size_t token_len = strlen(token);

        char ** replaced = (char **)malloc((size_t)(b->num_initial + 1) * sizeof(char *));
        if (!replaced) {
            xargs_err_printf("xargs: memory allocation failed\n");
            return 1;
        }

        bool ok = true;
        for (int i = 0; i < b->num_initial; i++) {
            size_t out_size = strlen(b->command[i]) + token_len * 10 + 256;
            replaced[i] = (char *)malloc(out_size);
            if (!replaced[i]) {
                xargs_err_printf("xargs: memory allocation failed\n");
                ok = false;
                for (int j = 0; j < i; j++) {
                    free(replaced[j]);
                }
                free(replaced);
                return 1;
            }
            if (!_xargs_replace_all(b->command[i], opts->replace_str,
                                    token, replaced[i], out_size)) {
                xargs_err_printf("xargs: replacement buffer too small\n");
                ok = false;
                for (int j = 0; j <= i; j++) {
                    free(replaced[j]);
                }
                free(replaced);
                return 1;
            }
        }
        replaced[b->num_initial] = NULL;

        if (opts->verbose) {
            _xargs_print_cmd(replaced, b->num_initial);
        }

        if (opts->interactive) {
            xargs_err_printf("?...");
            xargs_fflush(xargs_err_stream);
            char answer[256];
            if (fgets(answer, sizeof(answer), stdin) &&
                (answer[0] == 'y' || answer[0] == 'Y')) {
                /* proceed */
            }
            else {
                for (int i = 0; i < b->num_initial; i++) {
                    free(replaced[i]);
                }
                free(replaced);
                _xargs_batch_reset(b);
                return 0;
            }
        }

        if (ok) {
            int rc = _xargs_exec(replaced);
            if (rc != 0) {
                b->exit_code = 1;
            }
        }

        for (int i = 0; i < b->num_initial; i++) {
            free(replaced[i]);
        }
        free(replaced);

        _xargs_batch_reset(b);
        return 0;
    }

    /* Normal mode */
    if (opts->verbose) {
        _xargs_print_cmd(b->argv, b->argc);
    }

    if (opts->interactive) {
        xargs_err_printf("?...");
        xargs_fflush(xargs_err_stream);
        char answer[256];
        if (fgets(answer, sizeof(answer), stdin) &&
            (answer[0] == 'y' || answer[0] == 'Y')) {
            /* proceed */
        }
        else {
            _xargs_batch_reset(b);
            return 0;
        }
    }

    int rc = _xargs_exec(b->argv);
    if (rc != 0) {
        b->exit_code = 1;
    }
    _xargs_batch_reset(b);
    return 0;
}

/**
 * @brief Add a token to the current batch.
 * @param b        Batch state
 * @param token    Token string (will be copied)
 * @param tok_len  Length of token
 * @return 0 on success, 1 on error
 */
static int _xargs_batch_add(xargs_batch * b, const char * token, size_t tok_len)
{
    const xargs_opts * opts = b->opts;
    int tok_size = (int)tok_len + 1;

    /* Check size limits */
    if (b->chars_in_batch + tok_size > opts->max_chars) {
        if (b->args_in_batch == 0) {
            if (opts->exit_on_size) {
                xargs_err_printf("xargs: argument too long\n");
                return 1;
            }
            /* Add it anyway for single long arg */
        }
        else {
            /* Flush current batch */
            _xargs_batch_flush(b);
        }
    }

    /* Check max_args limit */
    if (opts->max_args > 0 && b->args_in_batch >= opts->max_args) {
        _xargs_batch_flush(b);
    }

    /* Check max_lines limit */
    if (opts->max_lines > 0 && b->lines_in_batch >= opts->max_lines) {
        _xargs_batch_flush(b);
    }

    /* Add token to argv */
    b->argv = _xargs_argv_grow(b->argv, &b->argv_cap, b->argc + 2);
    if (!b->argv) {
        xargs_err_printf("xargs: memory allocation failed\n");
        return 1;
    }
    /* We need to copy the token since the caller reuses the buffer */
    char * copy = (char *)malloc(tok_len + 1);
    if (!copy) {
        xargs_err_printf("xargs: memory allocation failed\n");
        return 1;
    }
    memcpy(copy, token, tok_len);
    copy[tok_len] = '\0';

    b->argv[b->argc] = copy;
    b->argc++;
    b->args_in_batch++;
    b->chars_in_batch += tok_size;

    return 0;
}

/**
 * @brief Initialize the parser state.
 * @param p      Parser state
 * @param b      Batch state
 * @param opts   Options
 */
static void _xargs_parser_init(xargs_parser * p, xargs_batch * b,
                               const xargs_opts * opts)
{
    p->tok_cap = XARGS_TOKEN_INIT;
    p->token = (char *)malloc(p->tok_cap);
    p->tok_len = 0;
    p->any_items = false;
    p->eof_reached = false;
    p->in_single_quote = false;
    p->in_double_quote = false;
    p->backslash = false;
    p->batch = b;
    p->opts = opts;
}

/**
 * @brief Free the parser state.
 * @param p  Parser state
 */
static void _xargs_parser_free(xargs_parser * p)
{
    if (p->token) {
        free(p->token);
        p->token = NULL;
    }
}

/**
 * @brief Grow the token buffer.
 * @param p  Parser state
 * @return 0 on success, 1 on error
 */
static int _xargs_parser_grow(xargs_parser * p)
{
    if (p->tok_len + 1 < p->tok_cap) {
        return 0;
    }
    size_t new_cap = p->tok_cap * 2;
    char * new_tok = (char *)realloc(p->token, new_cap);
    if (!new_tok) {
        xargs_err_printf("xargs: memory allocation failed\n");
        return 1;
    }
    p->token = new_tok;
    p->tok_cap = new_cap;
    return 0;
}

/**
 * @brief Finalize current token and add to batch.
 * @param p  Parser state
 * @return 0 on success, 1 on error
 */
static int _xargs_parser_finalize(xargs_parser * p)
{
    if (p->tok_len == 0) {
        return 0;
    }

    p->any_items = true;
    p->token[p->tok_len] = '\0';

    /* Check EOF string */
    if (p->opts->eof_str && strcmp(p->token, p->opts->eof_str) == 0) {
        p->eof_reached = true;
        return 0;
    }

    int rc = _xargs_batch_add(p->batch, p->token, p->tok_len);
    p->tok_len = 0;
    return rc;
}

/**
 * @brief Main xargs processing loop.
 *
 * Reads items from input, builds command lines, and executes them.
 *
 * @param opts              Options
 * @param command           Command and initial arguments
 * @param num_initial_args  Number of initial arguments
 * @return 0 on success, 1 on error
 */
static int _xargs_run(const xargs_opts * opts, char ** command,
                      int num_initial_args)
{
    FILE * in = stdin;

    /* Open arg-file if specified */
    if (opts->arg_file) {
        in = fopen(opts->arg_file, "rb");
        if (!in) {
            xargs_err_printf("xargs: %s: %s\n", opts->arg_file, strerror(errno));
            return 1;
        }
    }

    xargs_batch batch;
    _xargs_batch_init(&batch, opts, command, num_initial_args);

    xargs_parser parser;
    _xargs_parser_init(&parser, &batch, opts);

    if (!parser.token) {
        xargs_err_printf("xargs: memory allocation failed\n");
        _xargs_batch_free(&batch);
        if (in != stdin) {
            fclose(in);
        }
        return 1;
    }

    /* Read buffer */
    unsigned char buf[XARGS_BUF_SIZE];
    size_t buf_len = 0;
    size_t buf_pos = 0;

    /* Main read loop */
    while (!parser.eof_reached) {
        /* Refill buffer */
        if (buf_pos >= buf_len) {
            buf_len = fread(buf, 1, sizeof(buf), in);
            buf_pos = 0;
            if (buf_len == 0) {
                break;  /* EOF */
            }
        }

        while (buf_pos < buf_len && !parser.eof_reached) {
            unsigned char c = buf[buf_pos++];

            /* NUL mode */
            if (opts->null_mode) {
                if (c == '\0') {
                    if (_xargs_parser_finalize(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    batch.lines_in_batch++;
                }
                else {
                    if (_xargs_parser_grow(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    parser.token[parser.tok_len++] = (char)c;
                }
                continue;
            }

            /* Custom delimiter mode */
            if (opts->delimiter >= 0) {
                if (c == (unsigned char)opts->delimiter) {
                    if (_xargs_parser_finalize(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    if (c == '\n') {
                        batch.lines_in_batch++;
                    }
                }
                else {
                    if (_xargs_parser_grow(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    parser.token[parser.tok_len++] = (char)c;
                }
                continue;
            }

            /* Default mode: whitespace/quotes/backslash */
            if (parser.backslash) {
                parser.backslash = false;
                if (_xargs_parser_grow(&parser) != 0) {
                    _xargs_parser_free(&parser);
                    _xargs_batch_free_tokens(&batch);
                    _xargs_batch_free(&batch);
                    if (in != stdin) {
                        fclose(in);
                    }
                    return 1;
                }
                parser.token[parser.tok_len++] = (char)c;
                continue;
            }

            if (parser.in_single_quote) {
                if (c == '\'') {
                    parser.in_single_quote = false;
                }
                else {
                    if (_xargs_parser_grow(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    parser.token[parser.tok_len++] = (char)c;
                }
                continue;
            }

            if (parser.in_double_quote) {
                if (c == '"') {
                    parser.in_double_quote = false;
                }
                else if (c == '\\') {
                    parser.backslash = true;
                }
                else {
                    if (_xargs_parser_grow(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                    parser.token[parser.tok_len++] = (char)c;
                }
                continue;
            }

            /* Not in any quote */
            if (c == '\'') {
                parser.in_single_quote = true;
            }
            else if (c == '"') {
                parser.in_double_quote = true;
            }
            else if (c == '\\') {
                parser.backslash = true;
            }
            else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                /* Whitespace: end of token */
                if (parser.tok_len > 0) {
                    if (_xargs_parser_finalize(&parser) != 0) {
                        _xargs_parser_free(&parser);
                        _xargs_batch_free_tokens(&batch);
                        _xargs_batch_free(&batch);
                        if (in != stdin) {
                            fclose(in);
                        }
                        return 1;
                    }
                }
                if (c == '\n') {
                    batch.lines_in_batch++;
                }
            }
            else {
                /* Normal character */
                if (_xargs_parser_grow(&parser) != 0) {
                    _xargs_parser_free(&parser);
                    _xargs_batch_free_tokens(&batch);
                    _xargs_batch_free(&batch);
                    if (in != stdin) {
                        fclose(in);
                    }
                    return 1;
                }
                parser.token[parser.tok_len++] = (char)c;
            }
        }
    }

    /* Handle remaining token */
    if (parser.tok_len > 0) {
        if (_xargs_parser_finalize(&parser) != 0) {
            _xargs_parser_free(&parser);
            _xargs_batch_free_tokens(&batch);
            _xargs_batch_free(&batch);
            if (in != stdin) {
                fclose(in);
            }
            return 1;
        }
    }

    /* Flush remaining batch */
    if (batch.args_in_batch > 0) {
        _xargs_batch_flush(&batch);
    }
    else if (!parser.any_items && !opts->no_run_if_empty && !opts->replace_str) {
        /* No input: run command once (default behavior) */
        if (opts->verbose) {
            _xargs_print_cmd(command, num_initial_args);
        }
        if (opts->interactive) {
            xargs_err_printf("?...");
            xargs_fflush(xargs_err_stream);
            char answer[256];
            if (fgets(answer, sizeof(answer), stdin) &&
                (answer[0] == 'y' || answer[0] == 'Y')) {
                /* proceed */
            }
            else {
                _xargs_parser_free(&parser);
                _xargs_batch_free_tokens(&batch);
                _xargs_batch_free(&batch);
                if (in != stdin) {
                    fclose(in);
                }
                return 0;
            }
        }
        int rc = _xargs_exec(command);
        if (rc != 0) {
            batch.exit_code = 1;
        }
    }

    int result = batch.exit_code;
    _xargs_parser_free(&parser);
    _xargs_batch_free_tokens(&batch);
    _xargs_batch_free(&batch);
    if (in != stdin) {
        fclose(in);
    }

    return result;
}

/**
 * @brief Print usage/help information
 */
static void _xargs_print_help(void)
{
    xargs_printf(
        "Usage: xargs [OPTION]... [COMMAND [INITIAL-ARGS]...]\n"
        "Build and execute command lines from standard input.\n"
        "\n"
        "  -0, --null                   input items are separated by a null, not whitespace;\n"
        "                                 disables quote and backslash processing\n"
        "  -a, --arg-file=FILE          read arguments from FILE, not standard input\n"
        "  -d, --delimiter=CHARACTER    items in input stream are separated by CHARACTER,\n"
        "                                 not by whitespace; disables quote and backslash\n"
        "                                 processing\n"
        "  -E EOF                       set logical EOF string\n"
        "  -I R                         same as --replace=R\n"
        "  -L MAX-LINES                 use at most MAX-LINES non-blank input lines per\n"
        "                                 command line\n"
        "  -n, --max-args=MAX-ARGS      use at most MAX-ARGS arguments per command line\n"
        "  -P, --max-procs=MAX-PROCS    run at most MAX-PROCS processes at a time\n"
        "  -r, --no-run-if-empty        if there are no arguments, then do not run COMMAND;\n"
        "                                 if this option is not given, COMMAND will be\n"
        "                                 run at least once\n"
        "  -s, --max-chars=MAX-CHARS    limit length of command line to MAX-CHARS\n"
        "  -t, --verbose                print commands before executing them\n"
        "  -p, --interactive            prompt before running commands\n"
        "  -x, --exit                   exit if the size (see -s) is exceeded\n"
        "\n"
        "      --help                   display this help and exit\n"
        "      --version                output version information and exit\n"
    );
}

/**
 * @brief Print version information
 */
static void _xargs_print_version(void)
{
    xargs_printf("xargs %s\n", XARGS_VERSION_STR);
    xargs_printf("%s", "Copyright (C) 2025-2026 Yezc/xargs\n");
    xargs_printf("%s", "License MIT: <https://mit-license.org/>\n");
    xargs_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    xargs_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
