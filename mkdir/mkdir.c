/**
 * @file mkdir.c
 * @brief Cross-platform mkdir command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common mkdir(1) implementations.
 *
 * Key behaviors:
 *   - -m/--mode: set permission mode (octal or symbolic)
 *   - -p/--parents: create parent directories, no error if exists
 *   - -v/--verbose: print a message for each created directory
 *   - -Z: SELinux context (silently ignored)
 *   - --help / --version: recognized and handled
 *   - Safe C interfaces (bounded copies, NULL checks, strerror)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o mkdir.exe mkdir.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o mkdir mkdir.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o mkdir mkdir.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o mkdir mkdir.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o mkdir mkdir.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o mkdir mkdir.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/mkdir>
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
    #define MKDIR_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define MKDIR_PLATFORM_LINUX   1
    #define MKDIR_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define MKDIR_PLATFORM_MACOS   1
    #define MKDIR_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define MKDIR_PLATFORM_FREEBSD 1
    #define MKDIR_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define MKDIR_PLATFORM_OPENBSD 1
    #define MKDIR_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define MKDIR_PLATFORM_NETBSD  1
    #define MKDIR_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define MKDIR_PLATFORM_POSIX   1
#else
    #define MKDIR_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef MKDIR_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef MKDIR_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef MKDIR_PLATFORM_NETBSD
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

#ifdef MKDIR_PLATFORM_WINDOWS
    #include <direct.h>
    #include <io.h>
    #include <sys/stat.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & S_IFDIR) != 0)
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define MKDIR_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define MKDIR_MAX_PATH_LEN 4096

/** @brief Maximum number of directory arguments */
#define MKDIR_MAX_ARGS 1024

/** @brief Default directory mode (rwxrwxrwx & ~umask) */
#define MKDIR_DEFAULT_MODE 0777

/** @brief Path separator character for component walking */
#define MKDIR_PATH_SEP '/'

#ifdef MKDIR_PLATFORM_WINDOWS
    #undef MKDIR_PATH_SEP
    #define MKDIR_PATH_SEP '\\'
#endif

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Command-line options for mkdir
 */
typedef struct {
    bool parents;   /* <- -p/--parents: create parent dirs, no error if exists */
    bool verbose;   /* <- -v/--verbose: print message for each created dir */
    bool mode_set;  /* <- whether -m/--mode was specified */
    int  mode;      /* <- permission mode (octal) */
} mkdir_opts_t;

/********************************
 *    static prototypes
 ********************************/
static void _mkdir_print_help(void);
static void _mkdir_print_version(void);
static int  _mkdir_safe_copy(char * dst, const char * src, size_t dst_size);
static int  _mkdir_parse_mode(const char * mode_str);
static int  _mkdir_path_exists(const char * path);
static int  _mkdir_is_directory(const char * path);
static int  _mkdir_chmod_dir(const char * path, int mode);
static int  _mkdir_make_dir(const char * path, int mode);
static int  _mkdir_mkdir_one(const char * path, const mkdir_opts_t * opts);
static int  _mkdir_parse_args(int argc, char ** argv, mkdir_opts_t * opts,
                              char ** dirs, int * ndirs);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for mkdir_printf.
 *        Defaults to libc @c stdout.
 *        Define externally to redirect all stream output.
 */
#ifndef mkdir_out_stream
    #define mkdir_out_stream stdout
#endif

/**
 * @brief Default error stream for mkdir_err_printf.
 *        Defaults to libc @c stderr.
 *        Define externally to redirect all error output.
 */
#ifndef mkdir_err_stream
    #define mkdir_err_stream stderr
#endif

/**
 * @brief Formatted print to the output stream (printf-compatible).
 *        Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef mkdir_printf
    #define mkdir_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to the error stream (fprintf-compatible).
 *        Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef mkdir_err_printf
    #define mkdir_err_printf(fmt, ...) fprintf(mkdir_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs().
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream
 */
#ifndef mkdir_fputs
    #define mkdir_fputs(str, stream) (void)fputs((str), (stream))
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the mkdir command
 *
 * Processing flow:
 *   1. Parse command-line options (-m, -p, -v, -Z, --help, --version)
 *   2. Error if no directory operands given
 *   3. Create each directory, honoring -p (parents) and -m (mode)
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    mkdir_opts_t opts;
    char * dirs[MKDIR_MAX_ARGS];
    int ndirs = 0;

    if (argc < 2 || !argv) {
        mkdir_err_printf("%s", "mkdir: missing operand\n");
        mkdir_err_printf("%s", "Try 'mkdir --help' for more information.\n");
        return 1;
    }

    if (_mkdir_parse_args(argc, argv, &opts, dirs, &ndirs) != 0) {
        return 1;
    }

    if (ndirs == 0) {
        mkdir_err_printf("%s", "mkdir: missing operand\n");
        mkdir_err_printf("%s", "Try 'mkdir --help' for more information.\n");
        return 1;
    }

    int had_error = 0;
    for (int i = 0; i < ndirs; i++) {
        if (_mkdir_mkdir_one(dirs[i], &opts) != 0) {
            had_error = 1;
        }
    }

    return had_error ? 1 : 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information
 */
static void _mkdir_print_help(void)
{
    mkdir_printf(
        "Usage: mkdir [OPTION]... DIRECTORY...\n"
        "Create the DIRECTORY(ies), if they do not already exist.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -m, --mode=MODE   set file mode (as in chmod), not a=rwx - umask\n"
        "  -p, --parents     no error if existing, make parent directories as needed\n"
        "  -v, --verbose     print a message for each created directory\n"
        "  -Z                set SELinux security context (ignored on this platform)\n"
        "      --help        display this help and exit\n"
        "      --version     output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _mkdir_print_version(void)
{
    mkdir_printf("mkdir %s\n", MKDIR_VERSION_STR);
    mkdir_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    mkdir_printf("%s", "License MIT: <https://mit-license.org/>\n");
    mkdir_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    mkdir_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Bounded string copy with explicit NUL termination.
 *        Rejects truncation so callers can detect overflow safely.
 * @param dst       destination buffer
 * @param src       source string (may be NULL)
 * @param dst_size  size of destination buffer in bytes
 * @return 0 on success, -1 on NULL/empty buffer or truncation
 */
static int _mkdir_safe_copy(char * dst, const char * src, size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return -1;
    }
    dst[0] = '\0';
    if (!src) {
        return -1;
    }
    size_t slen = strlen(src);
    if (slen + 1 > dst_size) {
        return -1;
    }
    memcpy(dst, src, slen);
    dst[slen] = '\0';
    return 0;
}

/**
 * @brief Parse an octal or symbolic mode string to a permission mode integer.
 * @param mode_str  mode string (e.g., "755", "0755", "a+rwx")
 * @return parsed mode value, or -1 on error
 *
 * Supports:
 *   - Octal: "755", "0755", "0777"
 *   - Symbolic (limited): combinations of [ugoa][+-=][rwxst]
 *     e.g., "a+rwx", "u=rwx,go=rx", "og+w"
 */
static int _mkdir_parse_mode(const char * mode_str)
{
    if (!mode_str || mode_str[0] == '\0') {
        return -1;
    }

    /* Octal mode: starts with a digit */
    if (mode_str[0] >= '0' && mode_str[0] <= '9') {
        char * endptr = NULL;
        long val = strtol(mode_str, &endptr, 8);
        if (!endptr || *endptr != '\0' || val < 0 || val > 07777) {
            return -1;
        }
        return (int)val;
    }

    /* Symbolic mode parsing */
    int mode = 0;
    int i = 0;

    while (mode_str[i] != '\0') {
        int who = 0;
        int perm = 0;
        int action = 0;

        while (mode_str[i] == 'u' || mode_str[i] == 'g' ||
               mode_str[i] == 'o' || mode_str[i] == 'a') {
            switch (mode_str[i]) {
                case 'u':
                    who |= 04700; /* user + setuid */
                    break;
                case 'g':
                    who |= 02070; /* group + setgid */
                    break;
                case 'o':
                    who |= 00007; /* other */
                    break;
                case 'a':
                    who |= 07777; /* all */
                    break;
                default:
                    break;
            }
            i++;
        }
        if (who == 0) {
            who = 07777; /* default: all */
        }

        if (mode_str[i] != '+' && mode_str[i] != '-' && mode_str[i] != '=') {
            return -1;
        }
        action = mode_str[i];
        i++;

        while (mode_str[i] == 'r' || mode_str[i] == 'w' ||
               mode_str[i] == 'x' || mode_str[i] == 's' ||
               mode_str[i] == 't') {
            switch (mode_str[i]) {
                case 'r':
                    perm |= 0444;
                    break;
                case 'w':
                    perm |= 0222;
                    break;
                case 'x':
                    perm |= 0111;
                    break;
                case 's':
                    perm |= 06000; /* setuid + setgid */
                    break;
                case 't':
                    perm |= 01000; /* sticky */
                    break;
                default:
                    break;
            }
            i++;
        }

        switch (action) {
            case '+':
                mode |= (who & perm);
                break;
            case '-':
                mode &= ~(who & perm);
                break;
            case '=':
                mode = (mode & ~who) | (who & perm);
                break;
            default:
                break;
        }

        if (mode_str[i] == ',') {
            i++;
        }
    }

    return mode;
}

/**
 * @brief Check if a path exists (as file or directory).
 * @param path  path to check
 * @return 1 if exists, 0 if not
 */
static int _mkdir_path_exists(const char * path)
{
    if (!path) {
        return 0;
    }
#ifdef MKDIR_PLATFORM_WINDOWS
    struct _stat st;
    return (_stat(path, &st) == 0) ? 1 : 0;
#else
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
#endif
}

/**
 * @brief Check if a path is an existing directory.
 * @param path  path to check
 * @return 1 if is directory, 0 if not
 */
static int _mkdir_is_directory(const char * path)
{
    if (!path) {
        return 0;
    }
#ifdef MKDIR_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return 0;
    }
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
#endif
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/**
 * @brief Change permissions of a directory.
 *        On Windows only the read-only bit is meaningful.
 * @param path  directory path
 * @param mode  permission mode
 * @return 0 on success, -1 on error
 */
static int _mkdir_chmod_dir(const char * path, int mode)
{
    if (!path) {
        return -1;
    }
#ifdef MKDIR_PLATFORM_WINDOWS
    int wmode = (mode & 0222) ? 0 : 1; /* 0 = read/write, 1 = readonly */
    if (_chmod(path, wmode) != 0) {
        return -1;
    }
    return 0;
#else
    if (chmod(path, (mode_t)mode) != 0) {
        return -1;
    }
    return 0;
#endif
}

/**
 * @brief Create a single directory with the given mode.
 * @param path  directory path to create
 * @param mode  permission mode
 * @return 0 on success, -1 on error
 */
static int _mkdir_make_dir(const char * path, int mode)
{
    if (!path) {
        return -1;
    }
#ifdef MKDIR_PLATFORM_WINDOWS
    (void)mode; /* Windows _mkdir does not take a mode parameter */
    if (_mkdir(path) != 0) {
        return -1;
    }
    return 0;
#else
    if (mkdir(path, (mode_t)mode) != 0) {
        return -1;
    }
    return 0;
#endif
}

/**
 * @brief Create a directory, optionally with parent directories.
 * @param path  directory path to create
 * @param opts  options
 * @return 0 on success, -1 on error
 *
 * With -p (--parents):
 *   - Create all missing parent directories
 *   - No error if the target already exists
 *   - Apply mode only to the final directory (GNU behavior)
 *
 * Without -p:
 *   - Error if parent doesn't exist
 *   - Error if target already exists
 */
static int _mkdir_mkdir_one(const char * path, const mkdir_opts_t * opts)
{
    if (!path || path[0] == '\0' || !opts) {
        mkdir_err_printf("%s", "mkdir: missing operand\n");
        return -1;
    }

    if (strlen(path) >= MKDIR_MAX_PATH_LEN) {
        mkdir_err_printf("mkdir: path too long: '%s'\n", path);
        return -1;
    }

    int mode = opts->mode_set ? opts->mode : MKDIR_DEFAULT_MODE;

    if (opts->parents) {
        char buf[MKDIR_MAX_PATH_LEN];
        if (_mkdir_safe_copy(buf, path, sizeof(buf)) != 0) {
            mkdir_err_printf("mkdir: path too long: '%s'\n", path);
            return -1;
        }

        /* Remove trailing separator(s) unless it's the root */
        size_t len = strlen(buf);
        while (len > 1 && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
            buf[--len] = '\0';
        }

        /* Already exists as a directory: no error with -p */
        if (_mkdir_is_directory(buf)) {
            return 0;
        }

        /* Exists as a file: error */
        if (_mkdir_path_exists(buf) && !_mkdir_is_directory(buf)) {
            mkdir_err_printf("mkdir: cannot create directory '%s': File exists\n", path);
            return -1;
        }

        /* Determine start index for path component walk.
         * On POSIX, start at 1 (skip leading '/').
         * On Windows, skip drive letter ("C:\") and UNC ("\\s\sh\"). */
        size_t start = 1;

#ifdef MKDIR_PLATFORM_WINDOWS
        if (len >= 3 && ((buf[0] >= 'A' && buf[0] <= 'Z') ||
                         (buf[0] >= 'a' && buf[0] <= 'z')) &&
            buf[1] == ':' && (buf[2] == '\\' || buf[2] == '/')) {
            start = 3;
        }
        if (len >= 2 && buf[0] == '\\' && buf[1] == '\\') {
            int sep_count = 0;
            for (size_t j = 2; buf[j] != '\0'; j++) {
                if (buf[j] == '\\' || buf[j] == '/') {
                    sep_count++;
                    if (sep_count == 2) {
                        start = j + 1;
                        break;
                    }
                }
            }
        }
#endif

        bool created_any = false;

        for (size_t i = start; buf[i] != '\0'; i++) {
            if (buf[i] == '/' || buf[i] == '\\') {
                char save = buf[i];
                buf[i] = '\0';

                if (buf[0] != '\0' && !_mkdir_path_exists(buf)) {
                    if (_mkdir_make_dir(buf, MKDIR_DEFAULT_MODE) != 0) {
                        mkdir_err_printf("mkdir: cannot create directory '%s': %s\n",
                                         buf, strerror(errno));
                        buf[i] = save;
                        return -1;
                    }
                    if (opts->verbose) {
                        mkdir_printf("mkdir: created directory '%s'\n", buf);
                    }
                    created_any = true;
                }

                buf[i] = save;
            }
        }

        /* Create the final directory */
        if (!_mkdir_path_exists(buf)) {
            if (_mkdir_make_dir(buf, mode) != 0) {
                mkdir_err_printf("mkdir: cannot create directory '%s': %s\n",
                                 buf, strerror(errno));
                return -1;
            }
            if (opts->verbose) {
                mkdir_printf("mkdir: created directory '%s'\n", buf);
            }
            created_any = true;
        }

        /* Apply mode to final directory (even if it already existed) */
        if (opts->mode_set && created_any) {
            _mkdir_chmod_dir(buf, mode);
        }

        return 0;
    }

    /* Without -p: simple creation */
    if (_mkdir_path_exists(path)) {
        mkdir_err_printf("mkdir: cannot create directory '%s': File exists\n", path);
        return -1;
    }

    if (_mkdir_make_dir(path, mode) != 0) {
        mkdir_err_printf("mkdir: cannot create directory '%s': %s\n",
                         path, strerror(errno));
        return -1;
    }

    if (opts->verbose) {
        mkdir_printf("mkdir: created directory '%s'\n", path);
    }

    /* Apply mode after creation (umask may have altered mkdir's mode) */
    if (opts->mode_set) {
        _mkdir_chmod_dir(path, mode);
    }

    return 0;
}

/**
 * @brief Parse command-line arguments.
 * @param argc   argument count
 * @param argv   argument vector
 * @param opts   options structure to fill
 * @param dirs   array to collect directory arguments
 * @param ndirs  pointer to store number of directories
 * @return 0 on success, -1 on error
 */
static int _mkdir_parse_args(int argc, char ** argv, mkdir_opts_t * opts,
                             char ** dirs, int * ndirs)
{
    if (!argv || !opts || !dirs || !ndirs) {
        return -1;
    }

    memset(opts, 0, sizeof(*opts));
    *ndirs = 0;

    int i = 1;
    while (i < argc) {
        char * arg = argv[i];
        if (!arg) {
            i++;
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }

        /* Long options */
        if (strcmp(arg, "--help") == 0) {
            _mkdir_print_help();
            exit(0);
        }
        else if (strcmp(arg, "--version") == 0) {
            _mkdir_print_version();
            exit(0);
        }
        else if (strcmp(arg, "--parents") == 0) {
            opts->parents = true;
        }
        else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(arg, "--mode") == 0) {
            i++;
            if (i >= argc) {
                mkdir_err_printf("%s", "mkdir: option '--mode' requires an argument\n");
                return -1;
            }
            int m = _mkdir_parse_mode(argv[i]);
            if (m < 0) {
                mkdir_err_printf("mkdir: invalid mode '%s'\n", argv[i]);
                return -1;
            }
            opts->mode_set = true;
            opts->mode = m;
        }
        else if (strncmp(arg, "--mode=", 7) == 0) {
            const char * val = arg + 7;
            int m = _mkdir_parse_mode(val);
            if (m < 0) {
                mkdir_err_printf("mkdir: invalid mode '%s'\n", val);
                return -1;
            }
            opts->mode_set = true;
            opts->mode = m;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options */
            for (int j = 1; arg[j]; j++) {
                switch (arg[j]) {
                    case 'p':
                        opts->parents = true;
                        break;
                    case 'v':
                        opts->verbose = true;
                        break;
                    case 'm':
                        i++;
                        if (i >= argc) {
                            mkdir_err_printf("%s", "mkdir: option '-m' requires an argument\n");
                            return -1;
                        }
                        {
                            int mm = _mkdir_parse_mode(argv[i]);
                            if (mm < 0) {
                                mkdir_err_printf("mkdir: invalid mode '%s'\n", argv[i]);
                                return -1;
                            }
                            opts->mode_set = true;
                            opts->mode = mm;
                        }
                        break;
                    case 'Z':
                        /* SELinux context: silently ignored */
                        break;
                    case 'h':
                        _mkdir_print_help();
                        exit(0);
                    default:
                        mkdir_err_printf("mkdir: invalid option -- '%c'\n", arg[j]);
                        mkdir_err_printf("%s", "Try 'mkdir --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            /* Non-option argument: collect as directory */
            if (*ndirs < MKDIR_MAX_ARGS) {
                dirs[(*ndirs)++] = arg;
            }
        }
        i++;
    }

    /* Collect remaining arguments after -- */
    while (i < argc) {
        if (*ndirs < MKDIR_MAX_ARGS) {
            dirs[(*ndirs)++] = argv[i];
        }
        i++;
    }

    return 0;
}
