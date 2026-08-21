/**
 * @file rm.c
 * @brief Cross-platform rm command implementation
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common rm(1) implementations.
 *
 * Key behaviors:
 *   - -f: force, ignore nonexistent files, never prompt
 *   - -i: prompt before every removal
 *   - -I: prompt once before removing >3 files or recursively
 *   - -r/-R/--recursive: recursive directory removal
 *   - -d/--dir: remove empty directories
 *   - -v/--verbose: explain what is being done
 *   - --preserve-root (default): refuse to operate on '/'
 *   - --no-preserve-root: allow operating on '/'
 *   - --one-file-system: skip directories on different file systems
 *   - --help / --version: recognized only before file arguments
 *   - Refuse to remove '.' or '..' components
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o rm.exe rm.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o rm rm.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o rm rm.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o rm rm.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o rm rm.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o rm rm.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/rm>
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
    #define RM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define RM_PLATFORM_LINUX   1
    #define RM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define RM_PLATFORM_MACOS   1
    #define RM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define RM_PLATFORM_FREEBSD 1
    #define RM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define RM_PLATFORM_OPENBSD 1
    #define RM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define RM_PLATFORM_NETBSD  1
    #define RM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define RM_PLATFORM_POSIX   1
#else
    #define RM_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef RM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef RM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef RM_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef RM_PLATFORM_WINDOWS
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
#else /* RM_PLATFORM_POSIX */
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
#define RM_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define RM_MAX_PATH_LEN 4096

/** @brief Maximum number of file arguments we will actually queue */
#define RM_MAX_FILES 256

/** @brief Initial capacity for directory entry list */
#define RM_DIR_BUF_SIZE 32

/** @brief Hard upper bound for directory list dynamic capacity (protection) */
#define RM_DIR_BUF_MAX (1024 * 1024)

/** @brief Threshold for -I interactive-once behaviour (>3 files) */
#define RM_I_PROMPT_FILE_THRESHOLD 3

/** @brief Maximum path component length used when refusing obviously absurd inputs */
#define RM_PATH_COMPONENT_MAX 4096

/** @brief Maximum length for prompt / interactive message buffer */
#define RM_PROMPT_BUF_SIZE (RM_MAX_PATH_LEN + 128)

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Dynamically growing list of directory entry names
 */
typedef struct {
    char ** entries;  /* <- Array of entry name pointers */
    int     count;    /* <- Number of entries stored */
    int     capacity; /* <- Allocated capacity of entries array */
} dir_list_t;

/**
 * @brief Command-line options for rm
 */
typedef struct {
    bool force;             /* <- -f: ignore nonexistent, never prompt */
    bool interactive;       /* <- -i: prompt before every removal */
    bool interactive_once;  /* <- -I: prompt once (>3 files or recursive) */
    bool recursive;         /* <- -r/-R: recursive directory removal */
    bool verbose;           /* <- -v: explain what is being done */
    bool one_file_system;   /* <- --one-file-system: skip different fs */
    bool no_preserve_root;  /* <- --no-preserve-root: allow '/' */
    bool preserve_root;     /* <- --preserve-root: refuse '/' (default) */
    bool preserve_root_all; /* <- --preserve-root=all: reject separate dev */
    bool dir;               /* <- -d: remove empty directories */
} rm_options_t;

/********************************
 *    static prototypes
 ********************************/
static const char * _rm_base_name(const char * path);
static bool         _rm_is_directory(const char * path);
static bool         _rm_path_exists(const char * path);
static bool         _rm_is_symlink(const char * path);
static bool         _rm_is_root(const char * path);
static bool         _rm_is_dot_or_dotdot(const char * path);
static int          _rm_join_path(char * dst, size_t dst_size,
                                  const char * dir, const char * name);
static int          _rm_remove_file(const char * path, const rm_options_t * opts);
static int          _rm_rmdir(const char * path);
static int          _rm_get_dev(const char * path, dev_t * dev);
static void         _rm_dir_list_init(dir_list_t * dl);
static int          _rm_dir_list_add(dir_list_t * dl, const char * name);
static void         _rm_dir_list_free(dir_list_t * dl);
static int          _rm_read_directory(const char * path, dir_list_t * dl);
static void         _rm_print_help(void);
static void         _rm_print_version(void);
static bool         _rm_prompt_yes(const char * prompt);
static int          _rm_recursive(const char * path, const rm_options_t * opts,
                                  dev_t root_dev);
static int          _rm_remove_one(const char * path, const rm_options_t * opts);
static int          _rm_parse_args(int argc, char ** argv, rm_options_t * opts,
                                   char ** files, int * nfiles);
static bool         _rm_path_component_sane(const char * s);
static void         _rm_save_errno(const char ** msg_out, int * code_out);

#ifdef RM_PLATFORM_WINDOWS
static int _rm_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars);
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stderr stream for rm_err_stream / rm_err_printf.
 *        Defaults to libc @c stderr .
 *        Define externally to redirect all error output.
 */
#ifndef rm_err_stream
    #define rm_err_stream stderr
#endif

/**
 * @brief Default stdout stream for rm_out_stream / rm_fputs.
 *        Defaults to libc @c stdout .
 */
#ifndef rm_out_stream
    #define rm_out_stream stdout
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result cast to (void) so unused
 *        return values never produce warnings.
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef rm_printf
    #define rm_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr (error messages and prompts).
 *        Requires explicit format string; NULL-safe on stream.
 */
#ifndef rm_err_printf
    #define rm_err_printf(fmt, ...) do { if (rm_err_stream) { (void)fprintf((rm_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef rm_fflush
    #define rm_fflush(stream) do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safely free a pointer and set it to NULL.
 *        Safe to call with NULL pointer or NULL target.
 */
#ifndef rm_safe_free
    #define rm_safe_free(p) do { if ((p)) { free(p); (p) = NULL; } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 *        Two NULLs are considered equal (both missing).
 * @return true if strings match, false otherwise
 */
#ifndef rm_streq
    #define rm_streq(a, b) (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/**
 * @brief Safe strncmp wrapper with NULL guards and explicit size.
 *        NULL vs anything => false.
 */
#ifndef rm_strneq
    #define rm_strneq(a, b, n) (((a) && (b)) ? (strncmp((a), (b), (n)) == 0) : false)
#endif

/**
 * @brief Safe strdup wrapper that returns NULL on NULL input or allocation failure.
 *        Caller must still check the result.
 */
#ifndef rm_strdup_safe
    #define rm_strdup_safe(s) (((s)) ? strdup((s)) : NULL)
#endif

/**
 * @brief Safe getenv wrapper — returns empty string "" instead of NULL
 *        when variable is unset so downstream string ops never see NULL.
 */
#ifndef rm_getenv_safe
    #define rm_getenv_safe(name) ({ const char * _v = getenv((name)); _v ? _v : ""; })
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the rm command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If no files specified: error (unless -f)
 *   3. -I: prompt once if removing >3 files or recursively
 *   4. Remove each file/directory
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    rm_options_t opts;
    char * files[RM_MAX_FILES];
    int nfiles = 0;
    int had_error = 0;

    memset(&opts, 0, sizeof(opts));
    memset(files, 0, sizeof(files));
    opts.preserve_root = true;  /* default: protect root */

    if (argc < 2) {
        rm_err_printf("%s", "rm: missing operand\n");
        rm_err_printf("%s", "Try 'rm --help' for more information.\n");
        return 1;
    }

    if (_rm_parse_args(argc, argv, &opts, files, &nfiles) != 0) {
        return 1;
    }

    if (nfiles == 0) {
        if (!opts.force) {
            rm_err_printf("%s", "rm: missing operand\n");
            rm_err_printf("%s", "Try 'rm --help' for more information.\n");
            return 1;
        }
        return 0;
    }

    /* -I: prompt once before removing >3 files or when recursive */
    if (opts.interactive_once) {
        bool need_prompt = false;
        if (nfiles > RM_I_PROMPT_FILE_THRESHOLD) {
            need_prompt = true;
        }
        if (opts.recursive) {
            for (int i = 0; i < nfiles; i++) {
                if (files[i] && _rm_is_directory(files[i])) {
                    need_prompt = true;
                    break;
                }
            }
        }
        if (need_prompt) {
            rm_err_printf("rm: remove %d argument%s%s? ",
                          nfiles,
                          nfiles > 1 ? "s" : "",
                          opts.recursive ? " recursively" : "");
            rm_fflush(rm_err_stream);
            /* Use fgets (bounded line read) instead of unbounded getchar loop. */
            char linebuf[16];
            int answer = 0;
            if (fgets(linebuf, sizeof(linebuf), stdin)) {
                answer = (unsigned char)linebuf[0];
            }
            if (answer != 'y' && answer != 'Y') {
                return 0;
            }
        }
    }

    /* Remove each file */
    had_error = 0;
    for (int i = 0; i < nfiles; i++) {
        if (!files[i]) {
            continue;
        }
        if (_rm_remove_one(files[i], &opts) != 0) {
            had_error = 1;
        }
    }

    return had_error ? 1 : 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Capture errno right now into *code_out and map it to a message.
 *        If errno is 0, treat as ENOENT so callers always have a message.
 * @param msg_out   optional output pointer for strerror message
 * @param code_out  optional output pointer for the captured errno value
 */
static void _rm_save_errno(const char ** msg_out, int * code_out)
{
    int e = errno;
    if (e == 0) {
        e = ENOENT;
    }
    if (code_out) {
        *code_out = e;
    }
    if (msg_out) {
        *msg_out = strerror(e);
    }
    errno = e; /* restore for any immediate downstream checks */
}

/**
 * @brief Reject absurdly long path tokens before they ever reach the OS.
 * @param s  path string
 * @return false if any component exceeds RM_PATH_COMPONENT_MAX, true otherwise
 */
static bool _rm_path_component_sane(const char * s)
{
    if (!s) {
        return false;
    }
    size_t run = 0;
    for (const char * p = s; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '\0') {
            run = 0;
        }
        else {
            run++;
            if (run > RM_PATH_COMPONENT_MAX) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Extract the base name (last component) from a path
 * @param path  input path string (may be NULL)
 * @return pointer to the base name within the path, or empty string on NULL
 */
static const char * _rm_base_name(const char * path)
{
    if (!path || path[0] == '\0') {
        return "";
    }
    const char * base = path;
    for (const char * p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            if (p[1] != '\0') {
                base = p + 1;
            }
        }
    }
    return base;
}

/**
 * @brief Check if a path is a directory
 * @param path  path to check (must be non-NULL)
 * @return true if directory, false otherwise (treats error as false to be safe)
 */
static bool _rm_is_directory(const char * path)
{
    if (!path || !_rm_path_component_sane(path)) {
        return false;
    }
#ifdef RM_PLATFORM_WINDOWS
    wchar_t wpath[RM_MAX_PATH_LEN + 1];
    if (_rm_utf8_to_wide(path, wpath,
                         (sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        return false;
    }
    DWORD attr = GetFileAttributesW(wpath);
    return (attr != INVALID_FILE_ATTRIBUTES) &&
           ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Check if a path exists (file, directory, or symlink)
 * @param path  path to check
 * @return true if exists, false otherwise
 */
static bool _rm_path_exists(const char * path)
{
    if (!path || !_rm_path_component_sane(path)) {
        return false;
    }
#ifdef RM_PLATFORM_WINDOWS
    wchar_t wpath[RM_MAX_PATH_LEN + 1];
    if (_rm_utf8_to_wide(path, wpath,
                         (sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        return false;
    }
    return GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return (lstat(path, &st) == 0);
#endif
}

/**
 * @brief Check if a path is a symbolic link
 * @param path  path to check
 * @return true if symlink, false otherwise (always false on Windows)
 */
static bool _rm_is_symlink(const char * path)
{
#ifdef RM_PLATFORM_WINDOWS
    (void)path;
    return false;
#else
    if (!path || !_rm_path_component_sane(path)) {
        return false;
    }
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
#endif
}

/**
 * @brief Check if a path is the root directory "/" or drive root
 * @param path  path to check
 * @return true if root, false otherwise
 */
static bool _rm_is_root(const char * path)
{
    if (!path || path[0] == '\0') {
        return false;
    }
    /* Normalize: skip trailing slashes */
    size_t len = strlen(path);
    if (len == 0) {
        return false;
    }
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }
    if (len == 1 && (path[0] == '/' || path[0] == '\\')) {
        return true;
    }
#ifdef RM_PLATFORM_WINDOWS
    /* Windows drive root: "C:\", "D:\" etc. */
    if (len == 3 && isalpha((unsigned char)path[0]) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
#endif
    return false;
}

/**
 * @brief Check if the last component of a path is "." or ".."
 * @param path  path to check
 * @return true if last component is . or .., false otherwise
 */
static bool _rm_is_dot_or_dotdot(const char * path)
{
    if (!path || path[0] == '\0') {
        return false;
    }
    const char * base = _rm_base_name(path);
    if (!base || base[0] == '\0') {
        return false;
    }
    if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        return true;
    }
    return false;
}

/**
 * @brief Join a directory path and a filename with bounds checking.
 * @param dst       output buffer
 * @param dst_size  size of output buffer in bytes
 * @param dir       directory path
 * @param name      filename to append
 * @return 0 on success, -1 if output truncated or input invalid
 */
static int _rm_join_path(char * dst, size_t dst_size,
                         const char * dir, const char * name)
{
    if (!dst || dst_size == 0 || !dir || !name) {
        return -1;
    }
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    if (dlen > (RM_MAX_PATH_LEN * 2) || nlen > RM_PATH_COMPONENT_MAX) {
        return -1;
    }
    while (dlen > 0 && (dir[dlen - 1] == '/' || dir[dlen - 1] == '\\')) {
        dlen--;
    }
    /* required bytes: dlen + 1 (sep) + nlen + 1 (NUL)
     * Manual saturation arithmetic (C99-portable, no compiler builtins). */
    size_t total = 0;
    if (dlen > (size_t)-1 - 1) {
        return -1;
    }
    total = dlen + 1;
    if (nlen > (size_t)-1 - total) {
        return -1;
    }
    total += nlen;
    if (total > (size_t)-1 - 1) {
        return -1;
    }
    total += 1;
    if (total > dst_size) {
        return -1;
    }
    int written = snprintf(dst, dst_size, "%.*s/%s", (int)dlen, dir, name);
    if (written < 0 || (size_t)written >= dst_size) {
        if (dst_size > 0) {
            dst[0] = '\0';
        }
        return -1;
    }
    return 0;
}

/**
 * @brief Remove a regular file (with force mode rules, Windows attrib handling)
 * @param path  file to remove
 * @param opts  rm options (force suppresses some errors)
 * @return 0 on success, -1 on error
 */
static int _rm_remove_file(const char * path, const rm_options_t * opts)
{
    if (!path) {
        return -1;
    }
#ifdef RM_PLATFORM_WINDOWS
    wchar_t wpath[RM_MAX_PATH_LEN + 1];
    if (_rm_utf8_to_wide(path, wpath,
                         (sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        if (!opts || !opts->force) {
            rm_err_printf("rm: cannot remove '%s': %s\n",
                          path, strerror(EILSEQ));
        }
        return -1;
    }
    /* On Windows, clear read-only attribute before deleting */
    DWORD attr = GetFileAttributesW(wpath);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        if ((attr & FILE_ATTRIBUTE_READONLY) != 0) {
            (void)SetFileAttributesW(wpath,
                                     attr & ~FILE_ATTRIBUTE_READONLY);
        }
    }
    if (_wunlink(wpath) != 0) {
        if (!opts || !opts->force) {
            int e = errno ? errno : EACCES;
            rm_err_printf("rm: cannot remove '%s': %s\n",
                          path, strerror(e));
        }
        return -1;
    }
    return 0;
#else
    if (unlink(path) != 0) {
        if (!opts || !opts->force) {
            int e = errno ? errno : EACCES;
            rm_err_printf("rm: cannot remove '%s': %s\n",
                          path, strerror(e));
        }
        return -1;
    }
    return 0;
#endif
}

/**
 * @brief Remove an empty directory
 * @param path  directory to remove
 * @return 0 on success, -1 on error
 */
static int _rm_rmdir(const char * path)
{
    if (!path) {
        return -1;
    }
#ifdef RM_PLATFORM_WINDOWS
    wchar_t wpath[RM_MAX_PATH_LEN + 1];
    if (_rm_utf8_to_wide(path, wpath,
                         (sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        rm_err_printf("rm: cannot remove '%s': %s\n",
                      path, strerror(EILSEQ));
        return -1;
    }
    if (_wrmdir(wpath) != 0) {
        int e = errno ? errno : EACCES;
        rm_err_printf("rm: cannot remove directory '%s': %s\n",
                      path, strerror(e));
        return -1;
    }
    return 0;
#else
    if (rmdir(path) != 0) {
        int e = errno ? errno : EACCES;
        rm_err_printf("rm: cannot remove directory '%s': %s\n",
                      path, strerror(e));
        return -1;
    }
    return 0;
#endif
}

/**
 * @brief Get the device ID of a file system (for --one-file-system)
 * @param path  path to stat
 * @param dev   output device ID
 * @return 0 on success, -1 on error (always -1 on Windows / unsupported)
 */
static int _rm_get_dev(const char * path, dev_t * dev)
{
#ifdef RM_PLATFORM_WINDOWS
    (void)path;
    if (dev) {
        *dev = 0;
    }
    return -1;
#else
    if (!path || !dev) {
        return -1;
    }
    struct stat st;
    if (lstat(path, &st) != 0) {
        return -1;
    }
    *dev = st.st_dev;
    return 0;
#endif
}

/**
 * @brief Initialize an empty directory list (always succeeds)
 * @param dl  directory list instance
 */
static void _rm_dir_list_init(dir_list_t * dl)
{
    if (!dl) {
        return;
    }
    dl->entries = NULL;
    dl->count = 0;
    dl->capacity = 0;
}

/**
 * @brief Append an entry to the directory list.
 *        Bounds capacity to RM_DIR_BUF_MAX to avoid unbounded growth.
 * @param dl    directory list instance
 * @param name  entry name (will be strdup'd; caller owns after failure check)
 * @return 0 on success, -1 on OOM / exceeded bounds
 */
static int _rm_dir_list_add(dir_list_t * dl, const char * name)
{
    if (!dl || !name) {
        return -1;
    }
    size_t nlen = strlen(name);
    if (nlen == 0 || nlen > RM_PATH_COMPONENT_MAX) {
        return -1;
    }

    if (dl->count >= dl->capacity) {
        int new_cap = dl->capacity ? dl->capacity * 2 : RM_DIR_BUF_SIZE;
        if (new_cap > RM_DIR_BUF_MAX) {
            new_cap = RM_DIR_BUF_MAX;
        }
        if (new_cap <= dl->count) {
            return -1; /* at ceiling already */
        }
        if (((size_t)new_cap * sizeof(char *)) > (size_t)RM_DIR_BUF_MAX * 16U) {
            return -1;
        }
        char ** tmp = (char **)realloc(dl->entries,
                                       (size_t)new_cap * sizeof(char *));
        if (!tmp) {
            return -1;
        }
        dl->entries = tmp;
        dl->capacity = new_cap;
    }

    char * dup = strdup(name);
    if (!dup) {
        return -1;
    }
    dl->entries[dl->count] = dup;
    dl->count++;
    return 0;
}

/**
 * @brief Free all memory in a directory list and NULL all pointers.
 * @param dl  directory list instance (may be NULL, may be called twice safely)
 */
static void _rm_dir_list_free(dir_list_t * dl)
{
    if (!dl) {
        return;
    }
    for (int i = 0; i < dl->count; i++) {
        rm_safe_free(dl->entries[i]);
    }
    rm_safe_free(dl->entries);
    dl->count = 0;
    dl->capacity = 0;
}

#ifdef RM_PLATFORM_WINDOWS

/**
 * @brief Convert a UTF-8 multi-byte string to a UTF-16 (wide) string.
 *
 * Bounds-checked: rejects NULL inputs, rejects buffers that are too
 * small, and always NUL-terminates the output buffer on success.
 *
 * @param utf8        input UTF-8 NUL-terminated string
 * @param out         output wide buffer
 * @param out_wchars  size of output buffer in wchar_t elements
 * @return number of wchar_t written (excluding NUL), or -1 on failure
 */
static int _rm_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars)
{
    if (!out || out_wchars == 0) {
        return -1;
    }
    out[0] = L'\0';
    if (!utf8) {
        return -1;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0) {
        return -1;
    }
    if (needed > (int)out_wchars) {
        return -1;
    }
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_wchars);
    if (written <= 0) {
        out[0] = L'\0';
        return -1;
    }
    out[written <= 1 ? 0 : (size_t)(written - 1)] = L'\0';
    return written - 1;
}
#endif /* RM_PLATFORM_WINDOWS */

/**
 * @brief Read directory contents into a list
 * @param path  directory path
 * @param dl    output directory list (caller must call _rm_dir_list_free)
 * @return 0 on success, -1 on failure (caller still must free if partially populated)
 */
static int _rm_read_directory(const char * path, dir_list_t * dl)
{
    if (!dl) {
        return -1;
    }
    _rm_dir_list_init(dl);
    if (!path || !_rm_path_component_sane(path)) {
        return -1;
    }
    int ret = 0;

#ifdef RM_PLATFORM_WINDOWS
    wchar_t wpath[RM_MAX_PATH_LEN + 4];
    if (_rm_utf8_to_wide(path, wpath,
                         (sizeof(wpath) / sizeof(wpath[0])) - 4) < 0) {
        return -1;
    }
    /* append \* pattern */
    wchar_t * end = wpath;
    while (*end) {
        end++;
    }
    if ((size_t)(end - wpath) + 4 >= (sizeof(wpath) / sizeof(wpath[0]))) {
        return -1;
    }
    /* Ensure exactly one trailing backslash before '*' */
    if (end != wpath && end[-1] != L'\\' && end[-1] != L'/') {
        *end++ = L'\\';
    }
    else if (end != wpath && end[-1] == L'/') {
        end[-1] = L'\\';
    }
    *end++ = L'*';
    *end++ = L'\0';

    WIN32_FIND_DATAW fdw;
    HANDLE hfind = FindFirstFileW(wpath, &fdw);
    if (hfind == INVALID_HANDLE_VALUE) {
        DWORD le = GetLastError();
        if (le == ERROR_FILE_NOT_FOUND || le == ERROR_NO_MORE_FILES) {
            return 0; /* empty dir -> empty list is not an error here */
        }
        return -1;
    }
    do {
        if (wcscmp(fdw.cFileName, L".") == 0 ||
            wcscmp(fdw.cFileName, L"..") == 0) {
            continue;
        }
        char utf8_name[RM_PATH_COMPONENT_MAX + 1];
        int wr = WideCharToMultiByte(CP_UTF8, 0,
                                     fdw.cFileName, -1,
                                     utf8_name, (int)sizeof(utf8_name),
                                     NULL, NULL);
        if (wr <= 0 || (size_t)wr > sizeof(utf8_name)) {
            ret = -1;
            break;
        }
        if (_rm_dir_list_add(dl, utf8_name) != 0) {
            ret = -1;
            break;
        }
    } while (FindNextFileW(hfind, &fdw) != 0);

    DWORD last_find_err = GetLastError();
    if (!FindClose(hfind)) {
        ret = -1;
    }
    if (ret == 0 && last_find_err != ERROR_NO_MORE_FILES) {
        ret = -1;
    }
    return ret;
#else
    DIR * dir = opendir(path);
    if (!dir) {
        return -1;
    }
    struct dirent * de;
    for (;;) {
        errno = 0;
        de = readdir(dir);
        if (!de) {
            if (errno != 0) {
                ret = -1;
            }
            break;
        }
        if (strcmp(de->d_name, ".") == 0 ||
            strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (_rm_dir_list_add(dl, de->d_name) != 0) {
            ret = -1;
            break;
        }
    }
    if (closedir(dir) != 0) {
        ret = -1;
    }
    return ret;
#endif
}

/**
 * @brief Print usage/help information
 */
static void _rm_print_help(void)
{
    rm_printf(
        "Usage: rm [OPTION]... [FILE]...\n"
        "Remove (unlink) the FILE(s).\n"
        "\n"
        "Options:\n"
        "  -f, --force           ignore nonexistent files and arguments,\n"
        "                          never prompt\n"
        "  -i                    prompt before every removal\n"
        "  -I                    prompt once before removing more than three\n"
        "                          files, or when removing recursively;\n"
        "                          less intrusive than -i, while still giving\n"
        "                          protection against most mistakes\n"
        "      --interactive[=WHEN]  prompt according to WHEN: never, once\n"
        "                          (-I), or always (-i); without WHEN,\n"
        "                          prompt always\n"
        "      --one-file-system  when removing a hierarchy recursively,\n"
        "                          skip any directory that is on a file\n"
        "                          system different from that of the\n"
        "                          corresponding command line argument\n"
        "      --no-preserve-root  do not treat '/' specially\n"
        "      --preserve-root[=all]\n"
        "                          do not remove '/' (default);\n"
        "                          with 'all', reject any command line\n"
        "                          argument on a separate device from\n"
        "                          its parent\n"
        "  -r, -R, --recursive   remove directories and their contents\n"
        "                          recursively\n"
        "  -d, --dir             remove empty directories\n"
        "  -v, --verbose         explain what is being done\n"
        "      --help            display this help and exit\n"
        "      --version         output version information and exit\n"
        "\n"
        "By default, rm does not remove directories. Use the --recursive (-r\n"
        "or -R) option to remove each listed directory, too, along with all\n"
        "of their contents.\n"
        "\n"
        "Any attempt to remove a file whose last file component is '.'\n"
        "or '..' is rejected with a diagnostic.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _rm_print_version(void)
{
    rm_printf("rm %s\n", RM_VERSION_STR);
    rm_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    rm_printf("%s", "License MIT: <https://mit-license.org/>\n");
    rm_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    rm_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Prompt user for confirmation (y/n) using bounded fgets read.
 *
 * Avoids getchar() unbounded draining loops which can be exploited on
 * pathological or piped input. Always reads from stdin via a small
 * fixed-size buffer.
 *
 * @param prompt  prompt message to display (may be NULL)
 * @return true if user answered 'y' or 'Y', false otherwise
 *         (EOF / read error / empty input = "no")
 */
static bool _rm_prompt_yes(const char * prompt)
{
    if (prompt) {
        rm_err_printf("%s", prompt);
        rm_fflush(rm_err_stream);
    }
    char linebuf[16];
    if (!fgets(linebuf, sizeof(linebuf), stdin)) {
        return false;
    }
    int answer = (unsigned char)linebuf[0];
    return (answer == 'y' || answer == 'Y');
}

/**
 * @brief Recursively remove a directory and its contents
 * @param path      directory path to remove
 * @param opts      removal options
 * @param root_dev  device ID of root (for --one-file-system, 0 if unused)
 * @return 0 on success, -1 on error
 */
static int _rm_recursive(const char * path, const rm_options_t * opts,
                         dev_t root_dev)
{
    if (!path || !opts) {
        return -1;
    }
    dir_list_t dl;
    (void)_rm_read_directory(path, &dl);

    int had_error = 0;

    for (int i = 0; i < dl.count; i++) {
        if (!dl.entries[i]) {
            continue;
        }
        char child[RM_MAX_PATH_LEN];
        if (_rm_join_path(child, sizeof(child), path, dl.entries[i]) != 0) {
            rm_err_printf("rm: skipping overlong path in '%s'\n", path);
            had_error = 1;
            continue;
        }

        /* --one-file-system: skip entries on different device */
        if (opts->one_file_system && root_dev != 0) {
            dev_t child_dev = 0;
            if (_rm_get_dev(child, &child_dev) == 0 && child_dev != root_dev) {
                continue;
            }
        }

        if (_rm_is_directory(child) && !_rm_is_symlink(child)) {
            /* Recurse into subdirectory */
            if (_rm_recursive(child, opts, root_dev) != 0) {
                had_error = 1;
            }
        } else {
            /* Remove file or symlink */
            if (opts->interactive) {
                char pbuf[RM_PROMPT_BUF_SIZE];
                const char * type = "regular file";
                if (_rm_is_symlink(child)) {
                    type = "symbolic link";
                }
                (void)snprintf(pbuf, sizeof(pbuf),
                               "rm: remove %s '%s'? ", type, child);
                if (!_rm_prompt_yes(pbuf)) {
                    continue;
                }
            }

            if (opts->verbose) {
                rm_printf("removed '%s'\n", child);
            }

            if (_rm_remove_file(child, opts) != 0) {
                had_error = 1;
            }
        }
    }

    _rm_dir_list_free(&dl);

    /* Prompt for the directory itself */
    if (opts->interactive && had_error == 0) {
        char pbuf[RM_PROMPT_BUF_SIZE];
        (void)snprintf(pbuf, sizeof(pbuf),
                       "rm: remove directory '%s'? ", path);
        if (!_rm_prompt_yes(pbuf)) {
            return had_error;
        }
    }

    if (opts->verbose) {
        rm_printf("removed directory '%s'\n", path);
    }

    if (_rm_rmdir(path) != 0) {
        had_error = 1;
    }

    return had_error;
}

/**
 * @brief Remove a single file or directory
 * @param path  path to remove
 * @param opts  removal options
 * @return 0 on success, -1 on error
 */
static int _rm_remove_one(const char * path, const rm_options_t * opts)
{
    if (!path || !opts) {
        return -1;
    }
    if (!_rm_path_component_sane(path)) {
        rm_err_printf("rm: refusing overlong path: '%s'\n", path);
        return -1;
    }

    /* Refuse to remove '.' or '..' */
    if (_rm_is_dot_or_dotdot(path)) {
        rm_err_printf(
            "rm: refusing to remove '.' or '..' directory: skipping '%s'\n",
            path);
        return -1;
    }

    /* Check root protection */
    if (_rm_is_root(path) && !opts->no_preserve_root) {
        rm_err_printf("rm: it is dangerous to operate recursively on '%s'\n",
                      path);
        rm_err_printf("%s",
            "rm: use --no-preserve-root to override this failsafe\n");
        return -1;
    }

    /* Check if path exists */
    if (!_rm_path_exists(path)) {
        if (opts->force) {
            return 0;
        }
        int code = 0;
        const char * msg = NULL;
        _rm_save_errno(&msg, &code);
        (void)code;
        rm_err_printf("rm: cannot remove '%s': %s\n", path, msg);
        return -1;
    }

    bool is_dir = _rm_is_directory(path);
    bool is_link = _rm_is_symlink(path);

    /* Handle directories */
    if (is_dir && !is_link) {
        if (opts->recursive) {
            dev_t root_dev = 0;
            if (opts->one_file_system) {
                (void)_rm_get_dev(path, &root_dev);
            }
            return _rm_recursive(path, opts, root_dev);
        }
        if (opts->dir) {
            dir_list_t dl;
            (void)_rm_read_directory(path, &dl);
            bool empty = (dl.count == 0);
            _rm_dir_list_free(&dl);

            if (!empty) {
                rm_err_printf(
                    "rm: cannot remove '%s': Directory not empty\n", path);
                return -1;
            }

            if (opts->interactive) {
                char pbuf[RM_PROMPT_BUF_SIZE];
                (void)snprintf(pbuf, sizeof(pbuf),
                               "rm: remove directory '%s'? ", path);
                if (!_rm_prompt_yes(pbuf)) {
                    return 0;
                }
            }

            if (opts->verbose) {
                rm_printf("removed directory '%s'\n", path);
            }

            return _rm_rmdir(path);
        }
        rm_err_printf("rm: cannot remove '%s': Is a directory\n", path);
        return -1;
    }

    /* Regular file or symlink */
    if (opts->interactive) {
        char pbuf[RM_PROMPT_BUF_SIZE];
        const char * type = "regular file";
        if (is_link) {
            type = "symbolic link";
        }
        (void)snprintf(pbuf, sizeof(pbuf),
                       "rm: remove %s '%s'? ", type, path);
        if (!_rm_prompt_yes(pbuf)) {
            return 0;
        }
    }

    if (opts->verbose) {
        rm_printf("removed '%s'\n", path);
    }

    return _rm_remove_file(path, opts);
}

/**
 * @brief Parse command-line options
 * @param argc    argument count
 * @param argv    argument vector
 * @param opts    output options struct
 * @param files   output array of file paths (up to RM_MAX_FILES items)
 * @param nfiles  output number of files
 * @return 0 on success, -1 on error
 */
static int _rm_parse_args(int argc, char ** argv, rm_options_t * opts,
                          char ** files, int * nfiles)
{
    if (!opts || !files || !nfiles) {
        return -1;
    }
    *nfiles = 0;
    opts->preserve_root = true;  /* default: protect root */

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
            _rm_print_help();
            exit(0);
        }
        if (strcmp(arg, "--version") == 0) {
            _rm_print_version();
            exit(0);
        }
        if (strcmp(arg, "--force") == 0) {
            opts->force = true;
            opts->interactive = false;
            opts->interactive_once = false;
        }
        else if (strcmp(arg, "--interactive") == 0) {
            opts->interactive = true;
        }
        else if (strcmp(arg, "--recursive") == 0) {
            opts->recursive = true;
        }
        else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(arg, "--dir") == 0) {
            opts->dir = true;
        }
        else if (strcmp(arg, "--no-preserve-root") == 0) {
            opts->no_preserve_root = true;
        }
        else if (strcmp(arg, "--preserve-root") == 0) {
            opts->preserve_root = true;
            opts->preserve_root_all = false;
        }
        else if (strncmp(arg, "--preserve-root=", 16) == 0) {
            const char * val = arg + 16;
            if (strcmp(val, "all") == 0) {
                opts->preserve_root = true;
                opts->preserve_root_all = true;
            }
            else {
                rm_err_printf(
                    "rm: invalid argument '%s' for '--preserve-root'\n", val);
                return -1;
            }
        }
        else if (strcmp(arg, "--one-file-system") == 0) {
            opts->one_file_system = true;
        }
        else if (strncmp(arg, "--interactive=", 14) == 0) {
            const char * when = arg + 14;
            if (strcmp(when, "never") == 0) {
                opts->interactive = false;
                opts->interactive_once = false;
            }
            else if (strcmp(when, "once") == 0) {
                opts->interactive_once = true;
                opts->interactive = false;
            }
            else if (strcmp(when, "always") == 0) {
                opts->interactive = true;
            }
            else {
                rm_err_printf(
                    "rm: invalid argument '%s' for '--interactive'\n", when);
                return -1;
            }
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options */
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'f':
                        opts->force = true;
                        opts->interactive = false;
                        opts->interactive_once = false;
                        break;

                    case 'i':
                        opts->interactive = true;
                        opts->force = false;
                        break;

                    case 'I':
                        opts->interactive_once = true;
                        break;

                    case 'r':
                    case 'R':
                        opts->recursive = true;
                        break;

                    case 'v':
                        opts->verbose = true;
                        break;

                    case 'd':
                        opts->dir = true;
                        break;

                    case 'h':
                        _rm_print_help();
                        exit(0);

                    default:
                        rm_err_printf("rm: invalid option -- '%c'\n", arg[j]);
                        rm_err_printf("%s",
                            "Try 'rm --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            /* Non-option argument: collect as file */
            if (*nfiles >= RM_MAX_FILES) {
                rm_err_printf(
                    "rm: too many file arguments (>%d); aborting.\n",
                    RM_MAX_FILES);
                return -1;
            }
            files[*nfiles] = arg;
            (*nfiles)++;
        }
        i++;
    }

    /* Collect remaining arguments after -- */
    while (i < argc) {
        if (!argv[i]) {
            i++;
            continue;
        }
        if (*nfiles >= RM_MAX_FILES) {
            rm_err_printf(
                "rm: too many file arguments (>%d); aborting.\n",
                RM_MAX_FILES);
            return -1;
        }
        files[*nfiles] = argv[i];
        (*nfiles)++;
        i++;
    }

    return 0;
}
