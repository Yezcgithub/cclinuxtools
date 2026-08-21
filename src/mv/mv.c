/**
 * @file mv.c
 * @brief Cross-platform mv command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common mv(1) implementations.
 *
 * Key behaviors:
 *   - -f/--force: overwrite without prompting
 *   - -i/--interactive: prompt before overwrite
 *   - -n/--no-clobber: do not overwrite an existing file
 *   - -u/--update: move only if source is newer than destination
 *   - -v/--verbose: explain what is being done
 *   - -b/--backup: make a backup of each existing destination file
 *   - Multiple sources may be moved into a destination directory
 *   - Recursive directory move (copy contents then remove source)
 *   - --help / --version: recognized before file arguments
 *   - Cross-platform: Windows (MoveFileA), POSIX (rename)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o mv.exe mv.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o mv mv.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o mv mv.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o mv mv.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o mv mv.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o mv mv.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/mv>
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
    #define MV_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define MV_PLATFORM_LINUX   1
    #define MV_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define MV_PLATFORM_MACOS   1
    #define MV_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define MV_PLATFORM_FREEBSD 1
    #define MV_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define MV_PLATFORM_OPENBSD 1
    #define MV_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define MV_PLATFORM_NETBSD  1
    #define MV_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define MV_PLATFORM_POSIX   1
#else
    #define MV_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef MV_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef MV_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef MV_PLATFORM_NETBSD
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
#include <stddef.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef MV_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #include <fcntl.h>
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
#else /* MV_PLATFORM_POSIX */
    #include <unistd.h>
    #include <dirent.h>
    #include <fcntl.h>
    #include <limits.h>
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define MV_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define MV_MAX_PATH_LEN 4096

/** @brief Maximum number of file arguments */
#define MV_MAX_FILES 1024

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Command-line options for mv
 */
typedef struct {
    bool force;         /* <- -f/--force: overwrite without prompt */
    bool interactive;   /* <- -i/--interactive: prompt before overwrite */
    bool no_clobber;    /* <- -n/--no-clobber: do not overwrite */
    bool update;        /* <- -u/--update: only move if source is newer */
    bool verbose;       /* <- -v/--verbose: print what is being done */
    bool backup;        /* <- -b/--backup: backup existing destination */
} mv_options_t;

/********************************
 *    static prototypes
 ********************************/
static void         _mv_print_help(void);
static void         _mv_print_version(void);
static const char * _mv_base_name(const char * path);
static bool         _mv_path_exists(const char * path);
static bool         _mv_is_directory(const char * path);
static int          _mv_get_mtime(const char * path, time_t * mtime);
static int          _mv_join_path(char * result, size_t size,
                                  const char * dir, const char * filename);
static int          _mv_safe_copy(char * dst, const char * src, size_t dst_size);
static bool         _mv_prompt_yes(void);
static int          _mv_backup_file(const char * path);
static int          _mv_move(const char * src, const char * dest);
static int          _mv_remove_dir(const char * path);
static int          _mv_recursive(const char * src, const char * dest,
                                 const mv_options_t * opts);
static int          _mv_parse_args(int argc, char ** argv, mv_options_t * opts,
                                  char ** files, int * nfiles, char ** dest);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for mv_printf / mv_fputs.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef mv_out_stream
    #define mv_out_stream stdout
#endif

/**
 * @brief Default stderr stream for mv_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef mv_err_stream
    #define mv_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef mv_printf
    #define mv_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef mv_err_printf
    #define mv_err_printf(fmt, ...) \
        do { if (mv_err_stream) { (void)fprintf((mv_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally mv_out_stream)
 */
#ifndef mv_fputs
    #define mv_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      Character (promoted to @c int ).
 * @param stream  stdio stream (normally mv_out_stream)
 */
#ifndef mv_fputc
    #define mv_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 * @param stream  stdio stream (normally mv_out_stream)
 */
#ifndef mv_fflush
    #define mv_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the mv command
 *
 * Processing flow:
 *   1. Handle --help / --version / -h as sole argument
 *   2. Parse command-line options and collect file arguments
 *   3. Determine if destination is a directory
 *   4. For each source: resolve actual destination, apply options
 *      (force, interactive, no-clobber, update, backup), then move
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) {
            _mv_print_help();
            return 0;
        }
        else if (strcmp(argv[1], "--version") == 0) {
            _mv_print_version();
            return 0;
        }
        else if (strcmp(argv[1], "-h") == 0) {
            _mv_print_help();
            return 0;
        }
    }

    if (argc < 3) {
        mv_err_printf("%s", "mv: missing operand\n");
        mv_err_printf("%s", "Try 'mv --help' for more information.\n");
        return 1;
    }

    mv_options_t opts;
    char * files[MV_MAX_FILES];
    int nfiles = 0;
    char * dest = NULL;

    memset(files, 0, sizeof(files));

    if (_mv_parse_args(argc, argv, &opts, files, &nfiles, &dest) != 0) {
        return 1;
    }

    bool dest_is_dir = _mv_is_directory(dest);
    int had_error = 0;

    for (int i = 0; i < nfiles; i++) {
        char * src = files[i];
        char actual_dest[MV_MAX_PATH_LEN];
        bool src_is_dir = _mv_is_directory(src);

        if (!_mv_path_exists(src)) {
            if (!opts.force) {
#ifdef MV_PLATFORM_WINDOWS
                mv_err_printf("mv: cannot stat '%s': No such file or directory\n",
                              src);
#else
                mv_err_printf("mv: cannot stat '%s': %s\n",
                              src, strerror(errno));
#endif
                had_error = 1;
            }
            continue;
        }

        if (dest_is_dir) {
            const char * base = _mv_base_name(src);
            if (_mv_join_path(actual_dest, sizeof(actual_dest), dest, base) != 0) {
                mv_err_printf("%s", "mv: destination path too long\n");
                had_error = 1;
                continue;
            }
        }
        else {
            if (_mv_safe_copy(actual_dest, dest, sizeof(actual_dest)) != 0) {
                mv_err_printf("mv: destination path too long: '%s'\n", dest);
                had_error = 1;
                continue;
            }
        }

        if (strcmp(src, actual_dest) == 0) {
            mv_err_printf("mv: cannot move '%s' to itself\n", src);
            had_error = 1;
            continue;
        }

        if (_mv_path_exists(actual_dest)) {
            if (opts.no_clobber) {
                continue;
            }

            if (opts.update) {
                time_t src_mtime;
                time_t dest_mtime;
                if (_mv_get_mtime(src, &src_mtime) != 0) {
                    had_error = 1;
                    continue;
                }
                if (_mv_get_mtime(actual_dest, &dest_mtime) == 0 &&
                    src_mtime <= dest_mtime) {
                    continue;
                }
            }

            if (opts.interactive && !opts.force) {
                mv_printf("mv: overwrite '%s'? ", actual_dest);
                mv_fflush(mv_out_stream);
                if (!_mv_prompt_yes()) {
                    continue;
                }
            }

            if (opts.backup) {
                (void)_mv_backup_file(actual_dest);
            }
        }

        if (src_is_dir) {
            if (!dest_is_dir && _mv_path_exists(dest)) {
                mv_err_printf("mv: cannot move '%s' to '%s': Not a directory\n",
                              src, actual_dest);
                had_error = 1;
                continue;
            }

            if (!dest_is_dir) {
                if (_mv_safe_copy(actual_dest, dest, sizeof(actual_dest)) != 0) {
                    mv_err_printf("mv: destination path too long: '%s'\n", dest);
                    had_error = 1;
                    continue;
                }
            }

            if (opts.verbose) {
                mv_printf("mv: moving '%s' to '%s'\n", src, actual_dest);
            }

            if (!_mv_path_exists(actual_dest)) {
                if (_mv_move(src, actual_dest) != 0) {
#ifdef MV_PLATFORM_WINDOWS
                    mv_err_printf(
                        "mv: cannot move '%s' to '%s': Operation failed\n",
                        src, actual_dest);
#else
                    mv_err_printf("mv: cannot move '%s' to '%s': %s\n",
                                  src, actual_dest, strerror(errno));
#endif
                    had_error = 1;
                }
            }
            else {
                if (_mv_recursive(src, actual_dest, &opts) != 0) {
#ifdef MV_PLATFORM_WINDOWS
                    mv_err_printf(
                        "mv: cannot move '%s' to '%s': Operation failed\n",
                        src, actual_dest);
#else
                    mv_err_printf("mv: cannot move '%s' to '%s': %s\n",
                                  src, actual_dest, strerror(errno));
#endif
                    had_error = 1;
                }

                (void)_mv_remove_dir(src);
            }
        }
        else {
            if (opts.verbose) {
                mv_printf("mv: moving '%s' to '%s'\n", src, actual_dest);
            }

            if (_mv_move(src, actual_dest) != 0) {
#ifdef MV_PLATFORM_WINDOWS
                mv_err_printf(
                    "mv: cannot move '%s' to '%s': Operation failed\n",
                    src, actual_dest);
#else
                mv_err_printf("mv: cannot move '%s' to '%s': %s\n",
                              src, actual_dest, strerror(errno));
#endif
                had_error = 1;
            }
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
static void _mv_print_help(void)
{
    mv_printf(
        "Usage: mv [OPTION]... [-T] SOURCE DEST\n"
        "  or:  mv [OPTION]... SOURCE... DIRECTORY\n"
        "  or:  mv [OPTION]... -t DIRECTORY SOURCE...\n"
        "Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -f, --force          do not prompt before overwriting\n"
        "  -i, --interactive    prompt before overwrite\n"
        "  -n, --no-clobber     do not overwrite an existing file\n"
        "  -u, --update         move only when the SOURCE file is newer than\n"
        "                       the destination file or when the destination\n"
        "                       file is missing\n"
        "  -v, --verbose        explain what is being done\n"
        "  -b, --backup         make a backup of each existing destination file\n"
        "      --help           display this help and exit\n"
        "      --version        output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _mv_print_version(void)
{
    mv_printf("mv %s\n", MV_VERSION_STR);
    mv_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    mv_printf("%s", "License MIT: <https://mit-license.org/>\n");
    mv_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    mv_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract the base name (last component) from a path
 * @param path  input path string (may be NULL)
 * @return pointer to the base name within the path, or empty string on NULL/empty
 */
static const char * _mv_base_name(const char * path)
{
    if (!path || path[0] == '\0') {
        return "";
    }
    const char * p = path + strlen(path) - 1;
    while (p > path && (*p == '/' || *p == '\\')) {
        p--;
    }
    while (p > path && *p != '/' && *p != '\\') {
        p--;
    }
    if (*p == '/' || *p == '\\') {
        p++;
    }
    return p;
}

/**
 * @brief Check if a path exists (file, directory, or symlink)
 * @param path  path to check
 * @return true if exists, false otherwise
 */
static bool _mv_path_exists(const char * path)
{
    if (!path) {
        return false;
    }
#ifdef MV_PLATFORM_WINDOWS
    struct _stat st;
    return (_stat(path, &st) == 0);
#else
    struct stat st;
    return (stat(path, &st) == 0);
#endif
}

/**
 * @brief Check if a path is a directory
 * @param path  path to check
 * @return true if directory, false otherwise (treats error as false to be safe)
 */
static bool _mv_is_directory(const char * path)
{
    if (!path) {
        return false;
    }
#ifdef MV_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Get the modification time of a file
 * @param path   path to file
 * @param mtime  output: modification time
 * @return 0 on success, -1 on error
 */
static int _mv_get_mtime(const char * path, time_t * mtime)
{
    if (!path || !mtime) {
        return -1;
    }
#ifdef MV_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return -1;
    }
    *mtime = (time_t)st.st_mtime;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    *mtime = st.st_mtime;
#endif
    return 0;
}

/**
 * @brief Join a directory path and a filename with bounds checking.
 * @param result    output buffer
 * @param size      size of output buffer in bytes
 * @param dir       directory path
 * @param filename  filename to append
 * @return 0 on success, -1 if output truncated or input invalid
 */
static int _mv_join_path(char * result, size_t size,
                         const char * dir, const char * filename)
{
    if (!result || size == 0 || !dir || !filename) {
        return -1;
    }
    result[0] = '\0';

    size_t dlen = strlen(dir);
    size_t flen = strlen(filename);
    int needs_sep = 0;

    if (dlen > 0) {
        char last = dir[dlen - 1];
        if (last != '/' && last != '\\') {
            needs_sep = 1;
        }
    }

    if (dlen + (size_t)needs_sep + flen + 1 > size) {
        return -1;
    }

    memcpy(result, dir, dlen);
    size_t pos = dlen;
    if (needs_sep) {
        result[pos++] = '/';
    }
    memcpy(result + pos, filename, flen);
    pos += flen;
    result[pos] = '\0';
    return 0;
}

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
static int _mv_safe_copy(char * dst, const char * src, size_t dst_size)
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
 * @brief Prompt user for confirmation (y/n) using a bounded fgets read.
 *
 * Avoids getchar() unbounded draining loops which can be exploited on
 * pathological or piped input. Always reads from stdin via a small
 * fixed-size buffer.
 *
 * @return true if user answered 'y' or 'Y', false otherwise
 *         (EOF / read error / empty input = "no")
 */
static bool _mv_prompt_yes(void)
{
    char linebuf[16];
    if (!fgets(linebuf, sizeof(linebuf), stdin)) {
        return false;
    }
    int answer = (unsigned char)linebuf[0];
    return (answer == 'y' || answer == 'Y');
}

/**
 * @brief Create a backup of a file by appending a '~' suffix
 * @param path  path to file to backup
 * @return 0 on success, -1 on error
 */
static int _mv_backup_file(const char * path)
{
    if (!path || !_mv_path_exists(path)) {
        return -1;
    }

    char backup_path[MV_MAX_PATH_LEN];
    size_t max_len = sizeof(backup_path) - 2;
    if (strlen(path) > max_len) {
        mv_err_printf("%s", "mv: path too long for backup\n");
        return -1;
    }
    (void)snprintf(backup_path, sizeof(backup_path), "%s~", path);

#ifdef MV_PLATFORM_WINDOWS
    if (!MoveFileA(path, backup_path)) {
        return -1;
    }
#else
    if (rename(path, backup_path) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief Move/rename a file using the native API
 * @param src   source path
 * @param dest  destination path
 * @return 0 on success, -1 on error
 */
static int _mv_move(const char * src, const char * dest)
{
    if (!src || !dest) {
        return -1;
    }

#ifdef MV_PLATFORM_WINDOWS
    if (!MoveFileA(src, dest)) {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS) {
            (void)DeleteFileA(dest);
            if (!MoveFileA(src, dest)) {
                return -1;
            }
        }
        else {
            return -1;
        }
    }
#else
    if (rename(src, dest) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief Remove an empty directory
 * @param path  directory to remove
 * @return 0 on success, -1 on error
 */
static int _mv_remove_dir(const char * path)
{
    if (!path) {
        return -1;
    }

#ifdef MV_PLATFORM_WINDOWS
    if (_rmdir(path) != 0) {
        return -1;
    }
#else
    if (rmdir(path) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief Recursively move a directory's contents into a destination.
 * @param src   source directory
 * @param dest  destination directory
 * @param opts  options
 * @return 0 on success, -1 on error
 */
static int _mv_recursive(const char * src, const char * dest,
                         const mv_options_t * opts)
{
    if (!src || !dest || !opts) {
        return -1;
    }

#ifdef MV_PLATFORM_WINDOWS
    WIN32_FIND_DATAA find_data;
    char search_path[MV_MAX_PATH_LEN];
    (void)snprintf(search_path, sizeof(search_path), "%s\\*", src);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return -1;
    }

    int had_error = 0;

    do {
        const char * name = find_data.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char src_child[MV_MAX_PATH_LEN];
        char dest_child[MV_MAX_PATH_LEN];
        if (_mv_join_path(src_child, sizeof(src_child), src, name) != 0) {
            had_error = 1;
            continue;
        }
        if (_mv_join_path(dest_child, sizeof(dest_child), dest, name) != 0) {
            had_error = 1;
            continue;
        }

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            (void)_mkdir(dest_child);
            if (_mv_recursive(src_child, dest_child, opts) != 0) {
                had_error = 1;
            }
            (void)_rmdir(src_child);
        }
        else {
            if (_mv_move(src_child, dest_child) != 0) {
                had_error = 1;
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);

    (void)FindClose(hFind);
    return had_error ? -1 : 0;

#else
    DIR * dir = opendir(src);
    if (!dir) {
        return -1;
    }

    struct dirent * entry;
    int had_error = 0;

    while ((entry = readdir(dir))) {
        const char * name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char src_child[MV_MAX_PATH_LEN];
        char dest_child[MV_MAX_PATH_LEN];
        if (_mv_join_path(src_child, sizeof(src_child), src, name) != 0) {
            had_error = 1;
            continue;
        }
        if (_mv_join_path(dest_child, sizeof(dest_child), dest, name) != 0) {
            had_error = 1;
            continue;
        }

        struct stat st;
        if (lstat(src_child, &st) == 0 && S_ISDIR(st.st_mode)) {
            (void)mkdir(dest_child, 0777);
            if (_mv_recursive(src_child, dest_child, opts) != 0) {
                had_error = 1;
            }
            (void)rmdir(src_child);
        }
        else {
            if (_mv_move(src_child, dest_child) != 0) {
                had_error = 1;
            }
        }
    }

    (void)closedir(dir);
    return had_error ? -1 : 0;
#endif
}

/**
 * @brief Parse command-line arguments
 * @param argc    argument count
 * @param argv    argument vector
 * @param opts    output options struct
 * @param files   output array of file paths (up to MV_MAX_FILES items)
 * @param nfiles  output number of files
 * @param dest    output pointer to destination path (last file)
 * @return 0 on success, -1 on error
 */
static int _mv_parse_args(int argc, char ** argv, mv_options_t * opts,
                          char ** files, int * nfiles, char ** dest)
{
    if (!opts || !files || !nfiles || !dest) {
        return -1;
    }
    memset(opts, 0, sizeof(*opts));
    *nfiles = 0;
    *dest = NULL;

    if (argc < 1 || !argv) {
        return -1;
    }

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

        if (strcmp(arg, "--help") == 0) {
            _mv_print_help();
            exit(0);
        }
        else if (strcmp(arg, "--version") == 0) {
            _mv_print_version();
            exit(0);
        }
        else if (strcmp(arg, "--force") == 0) {
            opts->force = true;
            opts->interactive = false;
            opts->no_clobber = false;
        }
        else if (strcmp(arg, "--interactive") == 0) {
            opts->interactive = true;
            opts->force = false;
            opts->no_clobber = false;
        }
        else if (strcmp(arg, "--no-clobber") == 0) {
            opts->no_clobber = true;
            opts->force = false;
            opts->interactive = false;
        }
        else if (strcmp(arg, "--update") == 0) {
            opts->update = true;
        }
        else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(arg, "--backup") == 0) {
            opts->backup = true;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'f':
                        opts->force = true;
                        opts->interactive = false;
                        opts->no_clobber = false;
                        break;

                    case 'i':
                        opts->interactive = true;
                        opts->force = false;
                        opts->no_clobber = false;
                        break;

                    case 'n':
                        opts->no_clobber = true;
                        opts->force = false;
                        opts->interactive = false;
                        break;

                    case 'u':
                        opts->update = true;
                        break;

                    case 'v':
                        opts->verbose = true;
                        break;

                    case 'b':
                        opts->backup = true;
                        break;

                    case 'h':
                        _mv_print_help();
                        exit(0);

                    default:
                        mv_err_printf("mv: invalid option -- '%c'\n", arg[j]);
                        mv_err_printf("%s",
                            "Try 'mv --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            if (*nfiles < MV_MAX_FILES) {
                files[(*nfiles)++] = arg;
            }
        }
        i++;
    }

    while (i < argc) {
        if (!argv[i]) {
            i++;
            continue;
        }
        if (*nfiles < MV_MAX_FILES) {
            files[(*nfiles)++] = argv[i];
        }
        i++;
    }

    if (*nfiles < 2) {
        mv_err_printf("%s", "mv: missing operand\n");
        mv_err_printf("%s", "Try 'mv --help' for more information.\n");
        return -1;
    }

    *dest = files[*nfiles - 1];
    (*nfiles)--;

    return 0;
}
