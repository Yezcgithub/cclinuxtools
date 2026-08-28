/**
 * @file realpath.c
 * @brief Cross-platform implementation of the coreutils realpath command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils realpath(1) (coreutils 9.11+).
 *
 * Key behaviors:
 *   - -E, --canonicalize:          all but the last component must exist (default)
 *   -e, --canonicalize-existing:   all components of the path must exist
 *   -m, --canonicalize-missing:    no path components need exist or be a directory
 *   -L, --logical:                 resolve '..' components before symlinks
 *   -P, --physical:                resolve symlinks as encountered (default)
 *   -q, --quiet:                   suppress most error messages
 *   -s, --strip, --no-symlinks:    don't expand symlinks
 *   -z, --zero:                    end each output line with NUL, not newline
 *   --relative-to=DIR:             print resolved path relative to DIR
 *   --relative-base=DIR:           print absolute paths unless below DIR
 *   --help / --version:            display help or version information
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o realpath.exe realpath.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o realpath realpath.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o realpath realpath.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o realpath realpath.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o realpath realpath.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o realpath realpath.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/realpath>
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
    #define REALPATH_PLATFORM_WINDOWS 1
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#elif defined(__linux__)
    #define REALPATH_PLATFORM_LINUX   1
    #define REALPATH_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define REALPATH_PLATFORM_MACOS   1
    #define REALPATH_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define REALPATH_PLATFORM_FREEBSD 1
    #define REALPATH_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define REALPATH_PLATFORM_OPENBSD 1
    #define REALPATH_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define REALPATH_PLATFORM_NETBSD  1
    #define REALPATH_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define REALPATH_PLATFORM_POSIX   1
#else
    #define REALPATH_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef REALPATH_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef REALPATH_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef REALPATH_PLATFORM_NETBSD
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

#ifdef REALPATH_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <direct.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define REALPATH_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer size */
#define REALPATH_MAX_PATH 4096

/** @brief Path separator on the current platform */
#ifdef REALPATH_PLATFORM_WINDOWS
    #define REALPATH_SEP '\\'
    #define REALPATH_SEP_STR "\\"
    #define REALPATH_OTHER_SEP '/'
#else
    #define REALPATH_SEP '/'
    #define REALPATH_SEP_STR "/"
#endif

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Canonicalization mode for realpath
 */
typedef enum {
    REALPATH_CANON_DEFAULT,   /**< -E: all but last component must exist */
    REALPATH_CANON_EXISTING,  /**< -e: all components must exist */
    REALPATH_CANON_MISSING    /**< -m: no components need exist */
} realpath_canon_mode;

/**
 * @brief Options structure for realpath
 */
typedef struct {
    realpath_canon_mode canon_mode;  /**< Canonicalization mode */
    bool logical;                    /**< -L: resolve '..' before symlinks */
    bool physical;                   /**< -P: resolve symlinks as encountered */
    bool quiet;                      /**< -q: suppress error messages */
    bool no_symlinks;                /**< -s: don't expand symlinks */
    bool zero_terminated;            /**< -z: NUL line terminator */
    const char * relative_to;        /**< --relative-to=DIR */
    const char * relative_base;      /**< --relative-base=DIR */
} realpath_opts;

/********************************
 *    static prototypes
 ********************************/
static void _realpath_print_help(void);
static void _realpath_print_version(void);
static bool _realpath_streq(const char * a, const char * b);
static int  _realpath_process(const char * file, const realpath_opts * opts);
static bool _realpath_resolve(const char * path, const realpath_opts * opts,
                              char * out, size_t out_size);
static bool _realpath_canonicalize(const char * path, const realpath_opts * opts,
                                   char * out, size_t out_size);
static bool _realpath_file_exists(const char * path);
static bool _realpath_is_dir(const char * path);
static bool _realpath_is_symlink(const char * path);
static bool _realpath_readlink(const char * path, char * buf, size_t buf_size,
                               ssize_t * out_len);
static bool _realpath_getcwd(char * buf, size_t buf_size);
static char * _realpath_normalize_sep(char * path);
static bool _realpath_join(const char * base, const char * rel,
                           char * out, size_t out_size);
static bool _realpath_make_relative(const char * abs_path,
                                    const char * base_dir,
                                    char * out, size_t out_size);
static bool _realpath_clean_path(const char * path, char * out, size_t out_size);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream.
 *        Defaults to libc @c stdout .
 */
#ifndef realpath_out_stream
    #define realpath_out_stream stdout
#endif

/**
 * @brief Default error stream.
 *        Defaults to libc @c stderr .
 */
#ifndef realpath_err_stream
    #define realpath_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 */
#ifndef realpath_printf
    #define realpath_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef realpath_err_printf
    #define realpath_err_printf(fmt, ...) fprintf(realpath_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef realpath_fputs
    #define realpath_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 */
#ifndef realpath_fflush
    #define realpath_fflush(stream) (void)fflush(stream)
#endif

/**
 * @brief Safe free wrapper.
 */
#ifndef realpath_safe_free
    #define realpath_safe_free(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the realpath command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Validate option combinations
 *   3. Process each file argument
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
#ifdef REALPATH_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    realpath_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.canon_mode = REALPATH_CANON_DEFAULT;
    opts.physical = true;

    int file_start = argc;
    bool relative_to_set = false;
    bool relative_base_set = false;

    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_realpath_streq(arg, "--")) {
            file_start = i + 1;
            break;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_realpath_streq(arg, "--help")) {
                _realpath_print_help();
                return 0;
            }
            if (_realpath_streq(arg, "--version")) {
                _realpath_print_version();
                return 0;
            }
            if (_realpath_streq(arg, "--canonicalize")) {
                opts.canon_mode = REALPATH_CANON_DEFAULT;
                continue;
            }
            if (_realpath_streq(arg, "--canonicalize-existing")) {
                opts.canon_mode = REALPATH_CANON_EXISTING;
                continue;
            }
            if (_realpath_streq(arg, "--canonicalize-missing")) {
                opts.canon_mode = REALPATH_CANON_MISSING;
                continue;
            }
            if (_realpath_streq(arg, "--logical")) {
                opts.logical = true;
                opts.physical = false;
                continue;
            }
            if (_realpath_streq(arg, "--physical")) {
                opts.physical = true;
                opts.logical = false;
                continue;
            }
            if (_realpath_streq(arg, "--quiet")) {
                opts.quiet = true;
                continue;
            }
            if (_realpath_streq(arg, "--strip") ||
                _realpath_streq(arg, "--no-symlinks")) {
                opts.no_symlinks = true;
                continue;
            }
            if (_realpath_streq(arg, "--zero")) {
                opts.zero_terminated = true;
                continue;
            }
            if (strncmp(arg, "--relative-to=", 14) == 0) {
                opts.relative_to = arg + 14;
                relative_to_set = true;
                continue;
            }
            if (_realpath_streq(arg, "--relative-to")) {
                if (i + 1 >= argc) {
                    realpath_err_printf("realpath: option '--relative-to' requires an argument\n");
                    return 1;
                }
                opts.relative_to = argv[++i];
                relative_to_set = true;
                continue;
            }
            if (strncmp(arg, "--relative-base=", 16) == 0) {
                opts.relative_base = arg + 16;
                relative_base_set = true;
                continue;
            }
            if (_realpath_streq(arg, "--relative-base")) {
                if (i + 1 >= argc) {
                    realpath_err_printf("realpath: option '--relative-base' requires an argument\n");
                    return 1;
                }
                opts.relative_base = argv[++i];
                relative_base_set = true;
                continue;
            }

            realpath_err_printf("realpath: unrecognized option '%s'\n", arg);
            realpath_err_printf("Try 'realpath --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case 'E':
                        opts.canon_mode = REALPATH_CANON_DEFAULT;
                        p++;
                        break;

                    case 'e':
                        opts.canon_mode = REALPATH_CANON_EXISTING;
                        p++;
                        break;

                    case 'm':
                        opts.canon_mode = REALPATH_CANON_MISSING;
                        p++;
                        break;

                    case 'L':
                        opts.logical = true;
                        opts.physical = false;
                        p++;
                        break;

                    case 'P':
                        opts.physical = true;
                        opts.logical = false;
                        p++;
                        break;

                    case 'q':
                        opts.quiet = true;
                        p++;
                        break;

                    case 's':
                        opts.no_symlinks = true;
                        p++;
                        break;

                    case 'z':
                        opts.zero_terminated = true;
                        p++;
                        break;

                    default:
                        realpath_err_printf("realpath: invalid option -- '%c'\n", *p);
                        realpath_err_printf("Try 'realpath --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — first file */
        file_start = i;
        break;
    }

    /* If no files specified */
    int num_files = argc - file_start;
    if (num_files <= 0) {
        realpath_err_printf("realpath: missing operand\n");
        realpath_err_printf("Try 'realpath --help' for more information.\n");
        return 1;
    }

    /* If --relative-base is set but --relative-to is not,
     * use relative_base as relative_to as well. */
    if (relative_base_set && !relative_to_set) {
        opts.relative_to = opts.relative_base;
    }

    int exit_code = 0;
    for (int i = file_start; i < argc; i++) {
        if (_realpath_process(argv[i], &opts) != 0) {
            exit_code = 1;
        }
    }

    realpath_fflush(realpath_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _realpath_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/**
 * @brief Check if a file exists (any type).
 * @param path  File path
 * @return true if file exists
 */
static bool _realpath_file_exists(const char * path)
{
    if (!path || !path[0]) {
        return false;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
#else
    struct stat st;
    return (lstat(path, &st) == 0);
#endif
}

/**
 * @brief Check if a path is a directory.
 * @param path  File path
 * @return true if path is a directory
 */
static bool _realpath_is_dir(const char * path)
{
    if (!path || !path[0]) {
        return false;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Check if a path is a symbolic link.
 * @param path  File path
 * @return true if path is a symlink
 */
static bool _realpath_is_symlink(const char * path)
{
    if (!path || !path[0]) {
        return false;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    /* Check for reparse point (junction/symlink) */
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    /* On Windows, symlinks and junctions have FILE_ATTRIBUTE_REPARSE_POINT */
    return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
#endif
}

/**
 * @brief Read the target of a symbolic link.
 * @param path      Symlink path
 * @param buf       Output buffer
 * @param buf_size  Buffer size
 * @param out_len   Receives the length of the link target
 * @return true on success
 */
static bool _realpath_readlink(const char * path, char * buf,
                               size_t buf_size, ssize_t * out_len)
{
    if (!path || !buf || buf_size == 0) {
        return false;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    /* Windows: use GetFinalPathNameByHandleA or read reparse point */
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS |
                           FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD len = GetFinalPathNameByHandleA(h, buf, (DWORD)buf_size,
                                           FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (len == 0 || len >= buf_size) {
        return false;
    }
    /* Strip the \\?\ prefix if present */
    if (len >= 4 && buf[0] == '\\' && buf[1] == '\\' &&
        buf[2] == '?' && buf[3] == '\\') {
        memmove(buf, buf + 4, len - 4 + 1);
        len -= 4;
    }
    /* Convert backslashes to forward slashes for consistency */
    for (DWORD i = 0; i < len; i++) {
        if (buf[i] == '\\') {
            buf[i] = '/';
        }
    }
    *out_len = (ssize_t)len;
    return true;
#else
    ssize_t len = readlink(path, buf, buf_size - 1);
    if (len < 0) {
        return false;
    }
    buf[len] = '\0';
    *out_len = len;
    return true;
#endif
}

/**
 * @brief Get the current working directory.
 * @param buf       Output buffer
 * @param buf_size  Buffer size
 * @return true on success
 */
static bool _realpath_getcwd(char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return false;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    DWORD len = GetCurrentDirectoryA((DWORD)buf_size, buf);
    if (len == 0 || len >= buf_size) {
        return false;
    }
    /* Convert backslashes to forward slashes for internal processing */
    for (DWORD i = 0; i < len; i++) {
        if (buf[i] == '\\') {
            buf[i] = '/';
        }
    }
    return true;
#else
    if (getcwd(buf, buf_size) == NULL) {
        return false;
    }
    return true;
#endif
}

/**
 * @brief Normalize path separators to '/'.
 *        On Windows, converts '\\' to '/'.
 * @param path  Path to normalize (modified in place)
 * @return Pointer to the modified path
 */
static char * _realpath_normalize_sep(char * path)
{
    if (!path) {
        return NULL;
    }
#ifdef REALPATH_PLATFORM_WINDOWS
    for (char * p = path; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
#endif
    return path;
}

/**
 * @brief Join a base path and a relative path.
 * @param base      Base path
 * @param rel       Relative path
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _realpath_join(const char * base, const char * rel,
                           char * out, size_t out_size)
{
    if (!base || !rel || !out || out_size == 0) {
        return false;
    }

    size_t base_len = strlen(base);
    size_t rel_len = strlen(rel);

    /* If rel is absolute, just copy it */
    if (rel_len > 0 && rel[0] == '/') {
        if (rel_len >= out_size) {
            return false;
        }
        strcpy(out, rel);
        return true;
    }

    /* Handle Windows drive letter */
#ifdef REALPATH_PLATFORM_WINDOWS
    if (rel_len >= 2 && rel[1] == ':') {
        if (rel_len >= out_size) {
            return false;
        }
        strcpy(out, rel);
        return true;
    }
#endif

    /* Join base + '/' + rel */
    bool need_sep = false;
    if (base_len > 0 && base[base_len - 1] != '/') {
        need_sep = true;
    }

    size_t total = base_len + (need_sep ? 1 : 0) + rel_len;
    if (total >= out_size) {
        return false;
    }

    strcpy(out, base);
    if (need_sep) {
        strcat(out, "/");
    }
    strcat(out, rel);
    return true;
}

/**
 * @brief Clean a path by resolving '.', '..', and duplicate separators.
 *        This does NOT resolve symlinks — purely lexical processing.
 * @param path      Input path
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _realpath_clean_path(const char * path, char * out, size_t out_size)
{
    if (!path || !out || out_size == 0) {
        return false;
    }

    /* Make a working copy with normalized separators */
    char tmp[REALPATH_MAX_PATH];
    size_t plen = strlen(path);
    if (plen >= sizeof(tmp)) {
        return false;
    }
    strcpy(tmp, path);
    _realpath_normalize_sep(tmp);

    /* Handle Windows drive letter */
    size_t prefix_len = 0;
#ifdef REALPATH_PLATFORM_WINDOWS
    if (plen >= 2 && tmp[1] == ':') {
        prefix_len = 2;
    }
#endif

    /* Determine if path is absolute */
    bool is_absolute = (plen > prefix_len && tmp[prefix_len] == '/');

    /* Tokenize and build clean path using a stack approach */
    char * components[512];
    int comp_count = 0;

    char * p = tmp + prefix_len;
    if (is_absolute) {
        p++;  /* skip leading '/' */
    }

    char * saveptr = NULL;
    char * tok = strtok_r(p, "/", &saveptr);
    while (tok != NULL) {
        if (strcmp(tok, ".") == 0) {
            /* Skip '.' components */
        }
        else if (strcmp(tok, "..") == 0) {
            if (comp_count > 0) {
                /* Pop the last component */
                comp_count--;
            }
            else if (!is_absolute) {
                /* For relative paths, keep leading '..' */
                if (comp_count < 512) {
                    components[comp_count++] = tok;
                }
            }
            /* For absolute paths, '..' at root is silently dropped */
        }
        else {
            if (comp_count < 512) {
                components[comp_count++] = tok;
            }
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }

    /* Rebuild the path */
    size_t pos = 0;
    if (prefix_len > 0) {
        if (pos + prefix_len >= out_size) {
            return false;
        }
        memcpy(out + pos, tmp, prefix_len);
        pos += prefix_len;
    }
    if (is_absolute) {
        if (pos + 1 >= out_size) {
            return false;
        }
        out[pos++] = '/';
    }

    for (int i = 0; i < comp_count; i++) {
        size_t clen = strlen(components[i]);
        if (pos + clen + 1 >= out_size) {
            return false;
        }
        if (i > 0 || is_absolute) {
            if (pos > 0 && out[pos - 1] != '/') {
                out[pos++] = '/';
            }
            else if (!is_absolute && i == 0) {
                /* relative path first component, no leading slash */
            }
        }
        memcpy(out + pos, components[i], clen);
        pos += clen;
    }

    /* Handle empty result */
    if (pos == 0) {
        if (1 >= out_size) {
            return false;
        }
        out[pos++] = '.';
    }

    out[pos] = '\0';
    return true;
}

/**
 * @brief Canonicalize a path: resolve symlinks, '.', '..', etc.
 * @param path      Input path
 * @param opts      Options
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _realpath_canonicalize(const char * path,
                                   const realpath_opts * opts,
                                   char * out, size_t out_size)
{
    if (!path || !out || out_size == 0) {
        return false;
    }

    /* If --strip / --no-symlinks, just clean the path lexically */
    if (opts->no_symlinks) {
        /* First, make the path absolute if needed */
        char abs_path[REALPATH_MAX_PATH];

        /* Check if already absolute */
        bool is_abs = (path[0] == '/');
#ifdef REALPATH_PLATFORM_WINDOWS
        if (strlen(path) >= 2 && path[1] == ':') {
            is_abs = true;
        }
#endif

        if (is_abs) {
            if (strlen(path) >= sizeof(abs_path)) {
                return false;
            }
            strcpy(abs_path, path);
        }
        else {
            char cwd[REALPATH_MAX_PATH];
            if (!_realpath_getcwd(cwd, sizeof(cwd))) {
                return false;
            }
            if (!_realpath_join(cwd, path, abs_path, sizeof(abs_path))) {
                return false;
            }
        }

        _realpath_normalize_sep(abs_path);

        /* For -m mode with -s, we just clean lexically */
        if (opts->canon_mode == REALPATH_CANON_MISSING) {
            if (!_realpath_clean_path(abs_path, out, out_size)) {
                return false;
            }
            return true;
        }

        /* For -E (default) or -e, clean then verify existence */
        if (!_realpath_clean_path(abs_path, out, out_size)) {
            return false;
        }

        /* For -e, all components must exist */
        if (opts->canon_mode == REALPATH_CANON_EXISTING) {
            if (!_realpath_file_exists(out)) {
                return false;
            }
        }
        else {
            /* Default -E: all but last must exist */
            /* Find last component */
            char * last_sep = strrchr(out, '/');
            if (last_sep && last_sep != out) {
                /* Check parent exists */
                char parent[REALPATH_MAX_PATH];
                size_t parent_len = (size_t)(last_sep - out);
                if (parent_len >= sizeof(parent)) {
                    return false;
                }
                memcpy(parent, out, parent_len);
                parent[parent_len] = '\0';
                if (!_realpath_file_exists(parent)) {
                    return false;
                }
            }
        }

        return true;
    }

    /* Full canonicalization with symlink resolution */
    /* Build the initial absolute path */
    char abs_path[REALPATH_MAX_PATH];

    bool is_abs = (path[0] == '/');
#ifdef REALPATH_PLATFORM_WINDOWS
    if (strlen(path) >= 2 && path[1] == ':') {
        is_abs = true;
    }
#endif

    if (is_abs) {
        if (strlen(path) >= sizeof(abs_path)) {
            return false;
        }
        strcpy(abs_path, path);
    }
    else {
        char cwd[REALPATH_MAX_PATH];
        if (!_realpath_getcwd(cwd, sizeof(cwd))) {
            return false;
        }
        if (!_realpath_join(cwd, path, abs_path, sizeof(abs_path))) {
            return false;
        }
    }

    _realpath_normalize_sep(abs_path);

    /* Now resolve component by component, handling symlinks */
    char resolved[REALPATH_MAX_PATH];
    resolved[0] = '\0';

    /* Track prefix for Windows drive letter */
    size_t prefix_len = 0;
#ifdef REALPATH_PLATFORM_WINDOWS
    if (strlen(abs_path) >= 2 && abs_path[1] == ':') {
        prefix_len = 2;
        memcpy(resolved, abs_path, prefix_len);
        resolved[prefix_len] = '\0';
    }
#endif

    /* If absolute, start with root */
    if (abs_path[prefix_len] == '/') {
        if (prefix_len + 1 >= sizeof(resolved)) {
            return false;
        }
        resolved[prefix_len] = '/';
        resolved[prefix_len + 1] = '\0';
    }

    /* Process each component */
    char work[REALPATH_MAX_PATH];
    strcpy(work, abs_path);

    char * p = work + prefix_len;
    if (*p == '/') {
        p++;
    }

    int symlink_depth = 0;
    const int MAX_SYMLINK_DEPTH = 40;

    while (*p) {
        /* Extract next component */
        char * next_slash = strchr(p, '/');
        size_t comp_len;
        if (next_slash) {
            comp_len = (size_t)(next_slash - p);
        }
        else {
            comp_len = strlen(p);
        }

        char component[REALPATH_MAX_PATH];
        if (comp_len >= sizeof(component)) {
            return false;
        }
        memcpy(component, p, comp_len);
        component[comp_len] = '\0';

        /* Skip empty components (from double slashes) */
        if (comp_len == 0) {
            if (next_slash) {
                p = next_slash + 1;
            }
            else {
                break;
            }
            continue;
        }

        /* Handle '.' and '..' */
        if (strcmp(component, ".") == 0) {
            /* Skip '.' */
        }
        else if (strcmp(component, "..") == 0) {
            /* Go up one directory */
            size_t rlen = strlen(resolved);
            /* Remove trailing slash */
            if (rlen > 0 && resolved[rlen - 1] == '/') {
                rlen--;
            }
            /* Find last '/' */
            char * last_slash = NULL;
            for (size_t i = rlen; i > prefix_len; i--) {
                if (resolved[i - 1] == '/') {
                    last_slash = &resolved[i - 1];
                    break;
                }
            }
            if (last_slash) {
                *last_slash = '\0';
                /* Re-add root slash for absolute paths */
                if (prefix_len > 0 && strlen(resolved) == prefix_len) {
                    resolved[prefix_len] = '/';
                    resolved[prefix_len + 1] = '\0';
                }
            }
            else if (prefix_len > 0 && strlen(resolved) == prefix_len) {
                /* At drive root on Windows, can't go up */
                resolved[prefix_len] = '/';
                resolved[prefix_len + 1] = '\0';
            }
            else {
                /* Relative '..' at root */
                if (strlen(resolved) + 3 < sizeof(resolved)) {
                    if (strlen(resolved) > 0) {
                        strcat(resolved, "/");
                    }
                    strcat(resolved, "..");
                }
            }
        }
        else {
            /* Normal component: append to resolved */
            size_t rlen = strlen(resolved);
            if (rlen > 0 && resolved[rlen - 1] != '/') {
                if (rlen + 1 >= sizeof(resolved)) {
                    return false;
                }
                resolved[rlen] = '/';
                resolved[rlen + 1] = '\0';
                rlen++;
            }
            if (rlen + comp_len + 1 >= sizeof(resolved)) {
                return false;
            }
            strcat(resolved, component);

            /* Check if this component is a symlink */
            bool is_symlink = _realpath_is_symlink(resolved);

            if (is_symlink && !opts->no_symlinks) {
                if (++symlink_depth > MAX_SYMLINK_DEPTH) {
                    if (!opts->quiet) {
                        realpath_err_printf("realpath: %s: too many levels of symbolic links\n",
                                            path);
                    }
                    return false;
                }

                /* Read the symlink target */
                char link_target[REALPATH_MAX_PATH];
                ssize_t link_len = 0;
                if (!_realpath_readlink(resolved, link_target,
                                        sizeof(link_target), &link_len)) {
                    if (!opts->quiet) {
                        realpath_err_printf("realpath: %s: %s\n",
                                            resolved, strerror(errno));
                    }
                    return false;
                }
                link_target[link_len] = '\0';
                _realpath_normalize_sep(link_target);

                /* Determine if the link target is absolute */
                bool target_abs = (link_target[0] == '/');
#ifdef REALPATH_PLATFORM_WINDOWS
                if (strlen(link_target) >= 2 && link_target[1] == ':') {
                    target_abs = true;
                }
#endif

                if (target_abs) {
                    /* Replace resolved entirely with link target */
                    if ((size_t)link_len + 1 >= sizeof(resolved)) {
                        return false;
                    }
                    strcpy(resolved, link_target);
                }
                else {
                    /* Replace the last component with the link target */
                    /* Remove the symlink component from resolved */
                    size_t rlen2 = strlen(resolved);
                    char * last_slash = NULL;
                    for (size_t i = rlen2; i > prefix_len; i--) {
                        if (resolved[i - 1] == '/') {
                            last_slash = &resolved[i - 1];
                            break;
                        }
                    }
                    if (last_slash) {
                        *(last_slash + 1) = '\0';
                    }
                    else {
                        resolved[prefix_len] = '\0';
                    }

                    /* Append the link target */
                    if (!_realpath_join(resolved, link_target,
                                        resolved, sizeof(resolved))) {
                        return false;
                    }
                }

                /* Re-process the remaining path from the resolved location */
                /* Construct the remaining path */
                char remaining[REALPATH_MAX_PATH];
                if (next_slash && next_slash[1]) {
                    /* There are more components after the symlink */
                    if (!_realpath_join(resolved, next_slash + 1,
                                        remaining, sizeof(remaining))) {
                        return false;
                    }
                    /* Restart processing with the combined path */
                    _realpath_normalize_sep(remaining);
                    strcpy(work, remaining);
                    p = work + prefix_len;
                    if (*p == '/') {
                        p++;
                    }
                    /* Reset resolved to root or drive */
                    resolved[prefix_len] = '\0';
                    if (work[prefix_len] == '/') {
                        resolved[prefix_len] = '/';
                        resolved[prefix_len + 1] = '\0';
                    }
                    continue;
                }
                /* No more components after the symlink, continue loop */
                if (next_slash) {
                    p = next_slash + 1;
                }
                else {
                    p += comp_len;
                }
                continue;
            }
            else {
                /* Not a symlink or no_symlinks is set */
                /* Check existence based on canon_mode */
                bool is_last = (!next_slash || next_slash[1] == '\0');

                if (opts->canon_mode == REALPATH_CANON_EXISTING) {
                    if (!_realpath_file_exists(resolved)) {
                        if (!opts->quiet) {
                            realpath_err_printf("realpath: %s: %s\n",
                                                resolved, strerror(ENOENT));
                        }
                        return false;
                    }
                }
                else if (opts->canon_mode == REALPATH_CANON_DEFAULT) {
                    if (!is_last) {
                        /* Non-last component must exist and be a directory */
                        if (!_realpath_file_exists(resolved)) {
                            if (!opts->quiet) {
                                realpath_err_printf("realpath: %s: %s\n",
                                                    resolved, strerror(ENOENT));
                            }
                            return false;
                        }
                        if (!_realpath_is_dir(resolved)) {
                            if (!opts->quiet) {
                                realpath_err_printf("realpath: %s: %s\n",
                                                    resolved, strerror(ENOTDIR));
                            }
                            return false;
                        }
                    }
                }
                /* REALPATH_CANON_MISSING: no checks */
            }
        }

        /* Advance to next component */
        if (next_slash) {
            p = next_slash + 1;
        }
        else {
            break;
        }
    }

    /* If resolved is empty, use root */
    if (strlen(resolved) == 0 ||
        (prefix_len > 0 && strlen(resolved) == prefix_len)) {
        if (prefix_len + 2 >= out_size) {
            return false;
        }
        memcpy(out, resolved, prefix_len);
        out[prefix_len] = '/';
        out[prefix_len + 1] = '\0';
    }
    else {
        /* Remove trailing slash unless it's the root */
        size_t rlen = strlen(resolved);
        if (rlen > prefix_len + 1 && resolved[rlen - 1] == '/') {
            resolved[rlen - 1] = '\0';
        }
        if (strlen(resolved) >= out_size) {
            return false;
        }
        strcpy(out, resolved);
    }

    return true;
}

/**
 * @brief Compute a relative path from base_dir to abs_path.
 *        Both paths must be absolute.
 * @param abs_path  Target absolute path
 * @param base_dir  Base directory (absolute)
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _realpath_make_relative(const char * abs_path,
                                    const char * base_dir,
                                    char * out, size_t out_size)
{
    if (!abs_path || !base_dir || !out || out_size == 0) {
        return false;
    }

    /* Normalize both paths (using '/' internally) */
    char norm_abs[REALPATH_MAX_PATH];
    char norm_base[REALPATH_MAX_PATH];

    if (!_realpath_clean_path(abs_path, norm_abs, sizeof(norm_abs))) {
        return false;
    }
    if (!_realpath_clean_path(base_dir, norm_base, sizeof(norm_base))) {
        return false;
    }

    _realpath_normalize_sep(norm_abs);
    _realpath_normalize_sep(norm_base);

    /* If they are identical, return '.' */
    if (strcmp(norm_abs, norm_base) == 0) {
        if (2 >= out_size) {
            return false;
        }
        strcpy(out, ".");
        return true;
    }

    /* Check for different Windows drives */
#ifdef REALPATH_PLATFORM_WINDOWS
    if (strlen(norm_abs) >= 2 && strlen(norm_base) >= 2 &&
        norm_abs[1] == ':' && norm_base[1] == ':' &&
        tolower((unsigned char)norm_abs[0]) !=
            tolower((unsigned char)norm_base[0])) {
        /* Different drives — cannot make relative, return absolute */
        if (strlen(norm_abs) >= out_size) {
            return false;
        }
        strcpy(out, norm_abs);
        return true;
    }
#endif

    /* Split both paths into components */
    const char * abs_comps[512];
    const char * base_comps[512];
    int abs_count = 0;
    int base_count = 0;

    /* Skip drive prefix on Windows */
    const char * a = norm_abs;
    const char * b = norm_base;
#ifdef REALPATH_PLATFORM_WINDOWS
    if (strlen(a) >= 2 && a[1] == ':') {
        a += 2;
    }
    if (strlen(b) >= 2 && b[1] == ':') {
        b += 2;
    }
#endif

    /* Skip leading '/' */
    if (*a == '/') {
        a++;
    }
    if (*b == '/') {
        b++;
    }

    /* Tokenize abs */
    char abs_copy[REALPATH_MAX_PATH];
    strcpy(abs_copy, a);
    char * saveptr = NULL;
    char * tok = strtok_r(abs_copy, "/", &saveptr);
    while (tok != NULL) {
        if (abs_count < 512) {
            abs_comps[abs_count++] = tok;
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }

    /* Tokenize base */
    char base_copy[REALPATH_MAX_PATH];
    strcpy(base_copy, b);
    saveptr = NULL;
    tok = strtok_r(base_copy, "/", &saveptr);
    while (tok != NULL) {
        if (base_count < 512) {
            base_comps[base_count++] = tok;
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }

    /* Find common prefix length */
    int common = 0;
    while (common < abs_count && common < base_count) {
        if (strcmp(abs_comps[common], base_comps[common]) == 0) {
            common++;
        }
        else {
            break;
        }
    }

    /* Count how many '..' we need */
    int up_count = base_count - common;

    /* Build the relative path */
    char result[REALPATH_MAX_PATH];
    result[0] = '\0';
    size_t pos = 0;

    /* Add '..' components */
    for (int i = 0; i < up_count; i++) {
        if (pos > 0) {
            result[pos++] = '/';
        }
        result[pos++] = '.';
        result[pos++] = '.';
        result[pos] = '\0';
    }

    /* Add remaining abs components */
    for (int i = common; i < abs_count; i++) {
        if (pos > 0) {
            result[pos++] = '/';
        }
        size_t clen = strlen(abs_comps[i]);
        if (pos + clen + 1 >= sizeof(result)) {
            return false;
        }
        memcpy(result + pos, abs_comps[i], clen);
        pos += clen;
        result[pos] = '\0';
    }

    if (pos == 0) {
        if (2 >= out_size) {
            return false;
        }
        strcpy(out, ".");
        return true;
    }

    if (strlen(result) >= out_size) {
        return false;
    }
    strcpy(out, result);

    return true;
}

/**
 * @brief Resolve a path according to realpath options.
 * @param path      Input path
 * @param opts      Options
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @return true on success
 */
static bool _realpath_resolve(const char * path, const realpath_opts * opts,
                              char * out, size_t out_size)
{
    char resolved[REALPATH_MAX_PATH];

    if (!_realpath_canonicalize(path, opts, resolved, sizeof(resolved))) {
        return false;
    }

    /* If --relative-to is specified, compute relative path */
    if (opts->relative_to) {
        char base_resolved[REALPATH_MAX_PATH];
        realpath_opts base_opts = *opts;
        base_opts.no_symlinks = opts->no_symlinks;

        if (!_realpath_canonicalize(opts->relative_to, &base_opts,
                                    base_resolved, sizeof(base_resolved))) {
            if (!opts->quiet) {
                realpath_err_printf("realpath: %s: %s\n",
                                    opts->relative_to, strerror(errno));
            }
            return false;
        }

        /* Check --relative-base: only make relative if path is below base */
        if (opts->relative_base) {
            char base_base_resolved[REALPATH_MAX_PATH];
            realpath_opts rb_opts = *opts;
            rb_opts.no_symlinks = opts->no_symlinks;

            if (!_realpath_canonicalize(opts->relative_base, &rb_opts,
                                        base_base_resolved,
                                        sizeof(base_base_resolved))) {
                if (!opts->quiet) {
                    realpath_err_printf("realpath: %s: %s\n",
                                        opts->relative_base, strerror(errno));
                }
                return false;
            }

            /* Check if resolved starts with base_base_resolved */
            _realpath_normalize_sep(resolved);
            _realpath_normalize_sep(base_base_resolved);

            size_t base_len = strlen(base_base_resolved);
            bool is_below = false;

            if (strcmp(resolved, base_base_resolved) == 0) {
                is_below = true;
            }
            else if (strlen(resolved) > base_len &&
                     strncmp(resolved, base_base_resolved, base_len) == 0 &&
                     resolved[base_len] == '/') {
                is_below = true;
            }

            if (!is_below) {
                /* Print absolute path instead */
                if (strlen(resolved) >= out_size) {
                    return false;
                }
                strcpy(out, resolved);
                return true;
            }
        }

        /* Compute relative path */
        _realpath_normalize_sep(resolved);
        _realpath_normalize_sep(base_resolved);

        if (!_realpath_make_relative(resolved, base_resolved, out, out_size)) {
            return false;
        }
        return true;
    }

    /* No relative options — output the resolved path */
    if (strlen(resolved) >= out_size) {
        return false;
    }
    strcpy(out, resolved);
    return true;
}

/**
 * @brief Process a single file: resolve and print its path.
 * @param file   File path
 * @param opts   Options
 * @return 0 on success, 1 on error
 */
static int _realpath_process(const char * file, const realpath_opts * opts)
{
    char result[REALPATH_MAX_PATH];

    if (!_realpath_resolve(file, opts, result, sizeof(result))) {
        if (!opts->quiet) {
            realpath_err_printf("realpath: %s: %s\n", file, strerror(errno));
        }
        return 1;
    }

    realpath_fputs(result, realpath_out_stream);
    if (opts->zero_terminated) {
        realpath_fputs("\0", realpath_out_stream);
    }
    else {
        realpath_fputs("\n", realpath_out_stream);
    }

    return 0;
}

/**
 * @brief Print usage/help information
 */
static void _realpath_print_help(void)
{
    realpath_printf(
        "Usage: realpath [OPTION]... FILE...\n"
        "Print the resolved absolute file name.\n"
        "\n"
        "  -E, --canonicalize         all but the last component must exist (default)\n"
        "  -e, --canonicalize-existing  all components of the path must exist\n"
        "  -m, --canonicalize-missing  no path components need exist or be a directory\n"
        "  -L, --logical              resolve '..' components before symlinks\n"
        "  -P, --physical             resolve symlinks as encountered (default)\n"
        "  -q, --quiet                suppress most error messages\n"
        "      --relative-to=DIR      print the resolved path relative to DIR\n"
        "      --relative-base=DIR    print absolute paths unless paths below DIR\n"
        "  -s, --strip, --no-symlinks don't expand symlinks\n"
        "  -z, --zero                 end each output line with NUL, not newline\n"
        "\n"
        "      --help                 display this help and exit\n"
        "      --version              output version information and exit\n"
    );
}

/**
 * @brief Print version information
 */
static void _realpath_print_version(void)
{
    realpath_printf("realpath %s\n", REALPATH_VERSION_STR);
    realpath_printf("%s", "Copyright (C) 2025-2026 Yezc/realpath\n");
    realpath_printf("%s", "License MIT: <https://mit-license.org/>\n");
    realpath_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    realpath_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
