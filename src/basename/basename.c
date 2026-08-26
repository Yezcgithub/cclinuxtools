/**
 * @file basename.c
 * @brief Cross-platform implementation of the basename command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils basename(1).
 *
 * Key behaviors:
 *   - Print NAME with any leading directory components removed
 *   - If specified, also remove a trailing SUFFIX
 *   - Multiple NAMEs supported with -a/--multiple
 *   - Suffix removal for all names with -s/--suffix=SUFFIX
 *   - Zero separator output with -z/--zero
 *   - --help / --version
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o basename.exe basename.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o basename basename.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o basename basename.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o basename basename.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o basename basename.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o basename basename.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/basename>
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
    #define BASENAME_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASENAME_PLATFORM_LINUX   1
    #define BASENAME_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASENAME_PLATFORM_MACOS   1
    #define BASENAME_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASENAME_PLATFORM_FREEBSD 1
    #define BASENAME_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASENAME_PLATFORM_OPENBSD 1
    #define BASENAME_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASENAME_PLATFORM_NETBSD  1
    #define BASENAME_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASENAME_PLATFORM_POSIX   1
#else
    #define BASENAME_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef BASENAME_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef BASENAME_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef BASENAME_PLATFORM_NETBSD
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

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define BASENAME_VERSION_STR "v1.0.0"

/********************************
 *    typedefs
 ********************************/

/** @brief Command-line options */
typedef struct {
    bool multiple;      /**< -a, --multiple: support multiple arguments */
    char * suffix;      /**< -s, --suffix=SUFFIX: remove trailing suffix */
    bool zero;          /**< -z, --zero: NUL separator instead of newline */
} basename_opts;

/********************************
 *    static prototypes
 ********************************/
static void _basename_print_help(void);
static void _basename_print_version(void);
static int  _basename_parse_opts(int argc, char ** argv,
                                 basename_opts * opts, int * name_start);
static void _basename_strip(const char * name, char * out, size_t out_size);
static void _basename_remove_suffix(char * name, const char * suffix);
static const char * _basename_basename(const char * path);

/********************************
 *    macros
 ********************************/

#ifndef basename_printf
    #define basename_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef basename_err_printf
    #define basename_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef basename_fflush
    #define basename_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Program name for error messages */
static const char * basename_prog_name = "basename";

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the basename command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Determine mode: single-name, multiple-name, or suffix-removal
 *   3. Strip directory components and optional suffix from each name
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 1;
    }

    basename_prog_name = _basename_basename(argv[0]);

    basename_opts opts;
    memset(&opts, 0, sizeof(opts));

    int name_start = 0;
    if (!_basename_parse_opts(argc, argv, &opts, &name_start)) {
        return 1;
    }

    int nnames = argc - name_start;

    if (nnames == 0) {
        basename_err_printf("%s: missing operand\n", basename_prog_name);
        basename_err_printf("%s", "Try 'basename --help' for more information.\n");
        return 1;
    }

    /*
     * basename has two modes:
     *
     * 1. Traditional mode (no -a): basename NAME [SUFFIX]
     *    - If two operands and no -s, second is SUFFIX
     *    - If one operand, just strip directory
     *
     * 2. Multiple mode (-a): basename -a NAME... [-s SUFFIX]
     *    - All operands are NAMEs
     *    - -s provides optional suffix
     */
    const char * suffix = NULL;
    int actual_names = nnames;

    if (!opts.multiple) {
        /* Traditional mode */
        if (nnames > 2) {
            basename_err_printf("%s: extra operand '%s'\n",
                                basename_prog_name, argv[name_start + 2]);
            basename_err_printf("%s", "Try 'basename --help' for more information.\n");
            return 1;
        }

        if (opts.suffix) {
            suffix = opts.suffix;
        }
        else if (nnames == 2) {
            suffix = argv[name_start + 1];
            actual_names = 1;  /* second arg is suffix, not a name */
        }
    }
    else {
        /* Multiple mode: suffix comes from -s if specified */
        if (opts.suffix) {
            suffix = opts.suffix;
        }
    }

    /* Process each name */
    int exit_code = 0;
    const char * sep = opts.zero ? "\0" : "\n";
    size_t sep_len = opts.zero ? 1 : 1;

    for (int i = name_start; i < name_start + actual_names; i++) {
        char buf[4096];

        _basename_strip(argv[i], buf, sizeof(buf));

        if (suffix) {
            _basename_remove_suffix(buf, suffix);
        }

        /* Print result */
        fputs(buf, stdout);
        fwrite(sep, 1, sep_len, stdout);
    }

    basename_fflush(stdout);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information
 */
static void _basename_print_help(void)
{
    basename_printf(
        "Usage: %s NAME [SUFFIX]\n"
        "  or:  %s OPTION... NAME...\n"
        "Print NAME with any leading directory components removed.\n"
        "If specified, also remove a trailing SUFFIX.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "\n"
        "  -a, --multiple              support multiple arguments and treat each as a NAME\n"
        "  -s, --suffix=SUFFIX         remove a trailing SUFFIX; implies -a\n"
        "  -z, --zero                  end each output line with NUL, not newline\n"
        "      --help                  display this help and exit\n"
        "      --version               output version information and exit\n"
        "\n"
        "Examples:\n"
        "  %s /usr/bin/sort          Output \"sort\".\n"
        "  %s include/stdio.h .h     Output \"stdio\".\n"
        "  %s -s .h include/stdio.h  Output \"stdio\".\n"
        "  %s -a any/str1 any/str2   Output \"str1\" followed by \"str2\".\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        basename_prog_name, basename_prog_name, basename_prog_name,
        basename_prog_name, basename_prog_name, basename_prog_name
    );
}

/**
 * @brief Print version information
 */
static void _basename_print_version(void)
{
    basename_printf("basename %s\n", BASENAME_VERSION_STR);
    basename_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    basename_printf("%s", "License MIT: <https://mit-license.org/>\n");
    basename_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    basename_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract basename from a path (for program name)
 * @param path  full path (e.g. "/usr/bin/basename" or "basename.exe")
 * @return pointer to basename within path
 */
static const char * _basename_basename(const char * path)
{
    if (!path) {
        return "basename";
    }

    const char * base = path;
    for (const char * p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

/**
 * @brief Parse command-line options
 *
 * Supports style long options and short options.
 *
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        output options struct
 * @param name_start  index in argv where names begin
 * @return true on success, false on error
 */
static int _basename_parse_opts(int argc, char ** argv,
                                basename_opts * opts, int * name_start)
{
    if (!opts || !name_start) {
        return false;
    }

    *name_start = 1;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* Long options */
        if (strncmp(arg, "--", 2) == 0) {
            if (strcmp(arg, "--help") == 0) {
                _basename_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _basename_print_version();
                exit(0);
            }
            if (strcmp(arg, "--multiple") == 0) {
                opts->multiple = true;
                continue;
            }
            if (strcmp(arg, "--zero") == 0) {
                opts->zero = true;
                continue;
            }
            /* --suffix=SUFFIX */
            if (strncmp(arg, "--suffix=", 9) == 0) {
                opts->suffix = arg + 9;
                opts->multiple = true;  /* -s implies -a */
                continue;
            }
            /* --suffix SUFFIX (separate arg) */
            if (strcmp(arg, "--suffix") == 0) {
                if (i + 1 >= argc) {
                    basename_err_printf("%s: option '--suffix' requires an argument\n",
                                        basename_prog_name);
                    return false;
                }
                opts->suffix = argv[i + 1];
                opts->multiple = true;  /* -s implies -a */
                i++;
                continue;
            }

            basename_err_printf("%s: unrecognized option '%s'\n",
                                basename_prog_name, arg);
            basename_err_printf("%s", "Try 'basename --help' for more information.\n");
            return false;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'a':
                        opts->multiple = true;
                        break;
                    case 'z':
                        opts->zero = true;
                        break;
                    case 's':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = &arg[j + 1];
                        }
                        else if (i + 1 < argc) {
                            val = argv[i + 1];
                            i++;
                        }
                        else {
                            basename_err_printf("%s: option '-s' requires an argument\n",
                                                basename_prog_name);
                            return false;
                        }
                        opts->suffix = (char *)val;
                        opts->multiple = true;  /* -s implies -a */
                        goto next_arg;
                    }
                    default:
                        basename_err_printf("%s: invalid option -- '%c'\n",
                                            basename_prog_name, arg[j]);
                        basename_err_printf("%s", "Try 'basename --help' for more information.\n");
                        return false;
                }
            }
            next_arg:
            continue;
        }

        /* This is a NAME argument; all remaining args are names */
        *name_start = i;
        break;
    }

    return true;
}

/**
 * @brief Strip leading directory components from a path
 *
 * Handles both '/' and '\' as path separators for cross-platform
 * compatibility. Also handles the special case of all-slash paths
 * (e.g. "/" -> "/"). Trailing separators are removed before extracting
 * the basename (e.g. "/usr/bin/" -> "bin").
 *
 * @param name      input path
 * @param out       output buffer
 * @param out_size  size of output buffer
 */
static void _basename_strip(const char * name, char * out, size_t out_size)
{
    if (!name || !out || out_size == 0) {
        return;
    }

    /* Handle empty string */
    if (*name == '\0') {
        out[0] = '\0';
        return;
    }

    /* Make a working copy and strip trailing separators */
    size_t len = strlen(name);
    while (len > 0 && (name[len - 1] == '/' || name[len - 1] == '\\')) {
        len--;
    }

    /* If path was all separators, return "/" */
    if (len == 0) {
        if (out_size >= 2) {
            out[0] = '/';
            out[1] = '\0';
        }
        return;
    }

    /* Find the last path separator in the trimmed path */
    size_t base_start = 0;
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '/' || name[i] == '\\') {
            base_start = i + 1;
        }
    }

    size_t base_len = len - base_start;
    if (base_len >= out_size) {
        base_len = out_size - 1;
    }
    memcpy(out, name + base_start, base_len);
    out[base_len] = '\0';
}

/**
 * @brief Remove a trailing suffix from a string
 *
 * If the string ends with the given suffix, remove it.
 * The suffix removal only happens if the result would be non-empty
 * (matching behavior where basename /path/to/file.txt .txt gives "file",
 * but basename .txt .txt gives ".txt").
 *
 * @param name    string to modify (modified in place)
 * @param suffix  suffix to remove
 */
static void _basename_remove_suffix(char * name, const char * suffix)
{
    if (!name || !suffix || *suffix == '\0') {
        return;
    }

    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > name_len) {
        return;
    }

    /* Check if name ends with suffix */
    if (strcmp(name + name_len - suffix_len, suffix) == 0) {
        /* behavior: only remove suffix if the result is non-empty */
        if (name_len > suffix_len) {
            name[name_len - suffix_len] = '\0';
        }
    }
}
