/**
 * @file cp.c
 * @brief Cross-platform cp command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common cp(1) implementations.
 *
 * Key design features:
 *   - -r/-R/--recursive: recursive directory copy
 *   - -p/--preserve: preserve mode, timestamps, owner
 *   - -i/--interactive: prompt before overwrite
 *   - -n/--no-clobber: never overwrite existing files
 *   - -u/--update: copy only when source is newer
 *   - -v/--verbose: explain what is being done
 *   - -a/--archive: archive mode (-r -p -d)
 *   - -s/--symbolic-link: make symlinks instead of copying
 *   - -l/--link: hard link files instead of copying
 *   - -L/--dereference / -P/--no-dereference: symlink handling
 *   - -t/--target-directory / -T/--no-target-directory
 *   - Direct system calls for performance and portability
 *   - POSIX-style path handling (forward slashes on all platforms)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o cp.exe cp.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o cp cp.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o cp cp.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o cp cp.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o cp cp.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o cp cp.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/cp>
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
    #define CP_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define CP_PLATFORM_LINUX   1
    #define CP_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define CP_PLATFORM_MACOS   1
    #define CP_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define CP_PLATFORM_FREEBSD 1
    #define CP_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define CP_PLATFORM_OPENBSD 1
    #define CP_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define CP_PLATFORM_NETBSD  1
    #define CP_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define CP_PLATFORM_POSIX   1
#else
    #define CP_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef CP_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef CP_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef CP_PLATFORM_NETBSD
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

#ifdef CP_PLATFORM_WINDOWS
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
#else /* CP_PLATFORM_POSIX */
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include <utime.h>
    #include <pwd.h>
    #include <grp.h>
    #include <sys/time.h>
    #include <limits.h>
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define CP_VERSION_STR "v1.0.0"

/** @brief Default copy buffer size (64 KB) */
#define CP_BUF_SIZE (64 * 1024)

/** @brief Maximum path buffer length (bytes) */
#define CP_MAX_PATH_LEN 4096

/** @brief Maximum number of source files */
#define CP_MAX_SOURCES 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Cross-platform file metadata.
 *
 * Holds file stat information in a uniform structure regardless of platform.
 */
typedef struct {
    int     exists;      /* <- 1 if file exists, 0 otherwise */
    int     is_dir;      /* <- 1 if directory */
    int     is_symlink;  /* <- 1 if symbolic link */
    int     is_regular;  /* <- 1 if regular file */
    int64_t size;        /* <- File size in bytes */
    time_t  mtime;       /* <- Last modification time */
    time_t  atime;       /* <- Last access time */
    mode_t  mode;        /* <- File mode/permissions */
#ifdef CP_PLATFORM_POSIX
    uid_t   uid;         /* <- Owner user ID (POSIX only) */
    gid_t   gid;         /* <- Owner group ID (POSIX only) */
#endif
} cp_stat_t;

/**
 * @brief Dynamically growing list of directory entry names
 */
typedef struct {
    char ** entries;  /* <- Array of entry name pointers */
    int     count;    /* <- Number of entries stored */
    int     capacity; /* <- Allocated capacity of entries array */
} dir_list_t;

/**
 * @brief Command-line options for cp
 */
typedef struct {
    bool        recursive;          /* <- -r/-R: recursive copy */
    bool        preserve;           /* <- -p: preserve attributes */
    bool        force;              /* <- -f: force overwrite */
    bool        interactive;        /* <- -i: prompt before overwrite */
    bool        verbose;            /* <- -v: verbose output */
    bool        no_clobber;         /* <- -n: do not overwrite existing */
    bool        update;             /* <- -u: copy only if source newer */
    bool        archive;            /* <- -a: archive mode (-r -p -d) */
    bool        dereference;       /* <- -L: follow symlinks */
    bool        no_dereference;    /* <- -P/-d: preserve symlinks */
    bool        symbolic_link;     /* <- -s: create symlinks */
    bool        hard_link;         /* <- -l: create hard links */
    bool        target_dir;        /* <- -t: target directory specified */
    bool        no_target_dir;     /* <- -T: treat dest as normal file */
    const char *target_directory;  /* <- Target directory path (-t) */
} cp_options_t;

/********************************
 *    static prototypes
 ********************************/
static int          _cp_safe_copy(char * dst, const char * src, size_t dst_size);
static const char * _cp_base_name(const char * path);
static int          _cp_join_path(char * dst, size_t dst_size,
                                  const char * dir, const char * name);
static int          _cp_is_directory(const char * path);
static int          _cp_path_exists(const char * path);
static int          _cp_same_file(const char * path1, const char * path2);
static void         _cp_stat(const char * path, cp_stat_t * st);
static int          _cp_mkdir(const char * path, mode_t mode);
static void         _cp_chmod(const char * path, mode_t mode);
static void         _cp_set_time(const char * path, time_t atime, time_t mtime);
static void         _cp_preserve_owner(const char * dest, const cp_stat_t * st);
static int          _cp_symlink(const char * target, const char * linkpath);
static int          _cp_link(const char * oldpath, const char * newpath);
static int          _cp_readlink(const char * linkpath, char * buf, size_t bufsize);
static int          _cp_remove(const char * path);
static void         _cp_dir_list_init(dir_list_t * dl);
static void         _cp_dir_list_add(dir_list_t * dl, const char * name);
static void         _cp_dir_list_free(dir_list_t * dl);
static void         _cp_read_directory(const char * path, dir_list_t * dl);
static void         _cp_print_help(void);
static void         _cp_print_version(void);
static int          _cp_copy_file_data(const char * src, const char * dst,
                                       const cp_options_t * opts);
static int          _cp_copy_regular_file(const char * src, const char * dst,
                                          const cp_options_t * opts);
static int          _cp_copy_symlink(const char * src, const char * dst,
                                     const cp_options_t * opts);
static int          _cp_copy_directory(const char * src, const char * dst,
                                       const cp_options_t * opts);
static int          _cp_copy_one(const char * src, const char * dst,
                                 const cp_options_t * opts);
static int          _cp_copy_to_directory(char ** sources, int nsrc,
                                          const char * target_dir,
                                          const cp_options_t * opts);
static int          _cp_parse_args(int argc, char ** argv, cp_options_t * opts,
                                   char ** sources, int * nsrc,
                                   const char ** target_dir);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for cp_printf / cp_fputs.
 *        Defaults to libc @c stdout.
 *        Define externally to redirect all output.
 */
#ifndef cp_out_stream
    #define cp_out_stream stdout
#endif

/**
 * @brief Default error stream for cp_err_printf.
 *        Defaults to libc @c stderr.
 */
#ifndef cp_err_stream
    #define cp_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string.
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef cp_printf
    #define cp_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr (error messages and prompts).
 *        Requires explicit format string.
 */
#ifndef cp_err_printf
    #define cp_err_printf(fmt, ...) fprintf(cp_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs().
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally cp_out_stream)
 */
#ifndef cp_fputs
    #define cp_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      Character (promoted to int).
 * @param stream  stdio stream (normally cp_out_stream)
 */
#ifndef cp_fputc
    #define cp_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/**
 * @brief Safe stdio stream flush.
 * @param stream  stdio stream (normally cp_err_stream for prompts)
 */
#ifndef cp_fflush
    #define cp_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the cp command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Determine copy mode:
 *      - -t DIR: copy all sources into DIR
 *      - Single source + single dest: copy to dest
 *      - Multiple sources: last arg is target directory
 *   3. Dispatch to _cp_copy_one or _cp_copy_to_directory
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    cp_options_t opts;
    char * sources[CP_MAX_SOURCES];
    int nsrc = 0;
    const char * target_dir = NULL;

    memset(&opts, 0, sizeof(opts));
    memset(sources, 0, sizeof(sources));

    if (argc < 2) {
        cp_err_printf("%s", "cp: missing file operand\n");
        cp_err_printf("%s", "Try 'cp --help' for more information.\n");
        return 1;
    }

    if (_cp_parse_args(argc, argv, &opts, sources, &nsrc, &target_dir) != 0) {
        return 1;
    }

    if (nsrc == 0) {
        cp_err_printf("%s", "cp: missing file operand\n");
        cp_err_printf("%s", "Try 'cp --help' for more information.\n");
        return 1;
    }

    /* -t: target directory mode */
    if (opts.target_dir) {
        if (nsrc == 0) {
            cp_err_printf("cp: missing file operand after '--target-directory=%s'\n",
                          target_dir);
            return 1;
        }
        return _cp_copy_to_directory(sources, nsrc, target_dir, &opts) == 0 ? 0 : 1;
    }

    /* Single source + single destination */
    if (nsrc == 1 && !opts.no_target_dir) {
        if (_cp_is_directory(sources[0])) {
            if (!opts.recursive && !opts.archive) {
                cp_err_printf("cp: -r not specified; omitting directory '%s'\n",
                              sources[0]);
                return 1;
            }
        }
        cp_err_printf("cp: missing destination file operand after '%s'\n",
                      sources[0]);
        return 1;
    }

    if (nsrc == 1) {
        cp_err_printf("cp: missing destination file operand after '%s'\n",
                      sources[0]);
        return 1;
    }

    /* Two operands: SOURCE DEST */
    if (nsrc == 2) {
        const char * src = sources[0];
        const char * dst = sources[1];

        /* -T: treat dest as normal file (never as directory) */
        if (opts.no_target_dir && _cp_is_directory(dst)) {
            cp_err_printf("cp: cannot overwrite directory '%s' with non-directory\n",
                          dst);
            return 1;
        }

        /* If dest is a directory (and not -T), copy source into it */
        if (_cp_is_directory(dst) && !opts.no_target_dir) {
            char dst_path[CP_MAX_PATH_LEN];
            const char * bname = _cp_base_name(src);
            if (_cp_join_path(dst_path, sizeof(dst_path), dst, bname) != 0) {
                cp_err_printf("cp: target path too long: '%s'\n", bname);
                return 1;
            }
            return _cp_copy_one(src, dst_path, &opts) == 0 ? 0 : 1;
        }

        /* Copy source to dest (file to file) */
        return _cp_copy_one(src, dst, &opts) == 0 ? 0 : 1;
    }

    /* More than 2 sources: last must be a directory */
    if (nsrc > 2) {
        const char * dst = sources[nsrc - 1];
        if (opts.no_target_dir) {
            cp_err_printf("cp: extra operand '%s'\n", dst);
            return 1;
        }
        if (!_cp_is_directory(dst)) {
            cp_err_printf("cp: target '%s' is not a directory\n", dst);
            return 1;
        }
        return _cp_copy_to_directory(sources, nsrc - 1, dst, &opts) == 0 ? 0 : 1;
    }

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
static int _cp_safe_copy(char * dst, const char * src, size_t dst_size)
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
 * @brief Extract the base name (last component) from a path
 * @param path  input path string (may be NULL)
 * @return pointer to the base name within the path, or empty string on NULL
 */
static const char * _cp_base_name(const char * path)
{
    if (!path || path[0] == '\0') {
        return "";
    }
    const char * base = path;
    for (const char * p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

/**
 * @brief Join a directory path and a filename with bounds checking.
 * @param dst       output buffer
 * @param dst_size  size of output buffer in bytes
 * @param dir       directory path
 * @param name      filename to append
 * @return 0 on success, -1 if output truncated or input invalid
 */
static int _cp_join_path(char * dst, size_t dst_size,
                         const char * dir, const char * name)
{
    if (!dst || dst_size == 0 || !dir || !name) {
        return -1;
    }
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    if (dlen > (CP_MAX_PATH_LEN * 2) || nlen > CP_MAX_PATH_LEN) {
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
 * @brief Check if a path is a directory
 * @param path  path to check
 * @return 1 if directory, 0 otherwise
 */
static int _cp_is_directory(const char * path)
{
    if (!path) {
        return 0;
    }
#ifdef CP_PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) &&
           ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Check if a path exists (file, directory, or symlink)
 * @param path  path to check
 * @return 1 if exists, 0 otherwise
 */
static int _cp_path_exists(const char * path)
{
    if (!path) {
        return 0;
    }
#ifdef CP_PLATFORM_WINDOWS
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return lstat(path, &st) == 0;
#endif
}

/**
 * @brief Check if two paths refer to the same file
 * @param path1  first path
 * @param path2  second path
 * @return 1 if same file, 0 otherwise
 */
static int _cp_same_file(const char * path1, const char * path2)
{
    if (!path1 || !path2) {
        return 0;
    }
#ifdef CP_PLATFORM_WINDOWS
    /* On Windows, compare by resolving full paths */
    char full1[CP_MAX_PATH_LEN];
    char full2[CP_MAX_PATH_LEN];
    DWORD len1 = GetFullPathNameA(path1, sizeof(full1), full1, NULL);
    DWORD len2 = GetFullPathNameA(path2, sizeof(full2), full2, NULL);
    if (len1 == 0 || len2 == 0) {
        return 0;
    }
    /* Normalize to lowercase for case-insensitive comparison */
    for (char * p = full1; *p != '\0'; p++) {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '\\') {
            *p = '/';
        }
    }
    for (char * p = full2; *p != '\0'; p++) {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '\\') {
            *p = '/';
        }
    }
    return strcmp(full1, full2) == 0;
#else
    struct stat st1, st2;
    if (lstat(path1, &st1) != 0) {
        return 0;
    }
    if (lstat(path2, &st2) != 0) {
        return 0;
    }
    return (st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino);
#endif
}

/**
 * @brief Get file metadata (uses lstat on POSIX; does not follow symlinks)
 * @param path  path to stat
 * @param st    output metadata struct
 */
static void _cp_stat(const char * path, cp_stat_t * st)
{
    if (!path || !st) {
        return;
    }
    memset(st, 0, sizeof(*st));

#ifdef CP_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA fad;
    char native[CP_MAX_PATH_LEN];
    if (_cp_safe_copy(native, path, sizeof(native)) != 0) {
        return;
    }
    for (char * p = native; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
    if (GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) {
        st->exists = 1;
        st->is_dir = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        st->is_regular = !st->is_dir;
        st->is_symlink = 0; /* Windows symlinks need special handling */
        st->size = ((int64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        /* Convert FILETIME to Unix time_t */
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        st->mtime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        uli.LowPart = fad.ftLastAccessTime.dwLowDateTime;
        uli.HighPart = fad.ftLastAccessTime.dwHighDateTime;
        st->atime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        st->mode = st->is_dir ? 0755 : 0644;
    }
#else
    struct stat sb;
    if (lstat(path, &sb) == 0) {
        st->exists = 1;
        st->is_dir = S_ISDIR(sb.st_mode);
        st->is_regular = S_ISREG(sb.st_mode);
        st->is_symlink = S_ISLNK(sb.st_mode);
        st->size = (int64_t)sb.st_size;
        st->mtime = sb.st_mtime;
        st->atime = sb.st_atime;
        st->mode = sb.st_mode;
        st->uid = sb.st_uid;
        st->gid = sb.st_gid;
    }
#endif
}

/**
 * @brief Create a directory
 * @param path  directory path to create
 * @param mode  permission mode (ignored on Windows)
 * @return 0 on success, -1 on error
 */
static int _cp_mkdir(const char * path, mode_t mode)
{
    if (!path) {
        return -1;
    }
#ifdef CP_PLATFORM_WINDOWS
    (void)mode;
    return _mkdir(path);
#else
    return mkdir(path, mode);
#endif
}

/**
 * @brief Set file permissions (POSIX only, no-op on Windows)
 * @param path  file path
 * @param mode  permission mode
 */
static void _cp_chmod(const char * path, mode_t mode)
{
    if (!path) {
        return;
    }
#ifndef CP_PLATFORM_WINDOWS
    (void)chmod(path, mode);
#else
    (void)mode;
#endif
}

/**
 * @brief Set file timestamps (preserve -p)
 * @param path   file path
 * @param atime  access time to set
 * @param mtime  modification time to set
 */
static void _cp_set_time(const char * path, time_t atime, time_t mtime)
{
    if (!path) {
        return;
    }
#ifdef CP_PLATFORM_WINDOWS
    HANDLE h = CreateFileA(path, FILE_WRITE_ATTRIBUTES, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        ULARGE_INTEGER uli;
        FILETIME ftAccess, ftWrite;
        uli.QuadPart = (ULONGLONG)(atime * 10000000ULL + 116444736000000000ULL);
        ftAccess.dwLowDateTime = uli.LowPart;
        ftAccess.dwHighDateTime = uli.HighPart;
        uli.QuadPart = (ULONGLONG)(mtime * 10000000ULL + 116444736000000000ULL);
        ftWrite.dwLowDateTime = uli.LowPart;
        ftWrite.dwHighDateTime = uli.HighPart;
        (void)SetFileTime(h, NULL, &ftAccess, &ftWrite);
        (void)CloseHandle(h);
    }
#else
    struct utimbuf times;
    times.actime = atime;
    times.modtime = mtime;
    (void)utime(path, &times);
#endif
}

/**
 * @brief Copy file owner/group (POSIX only, no-op on Windows)
 * @param dest  destination file path
 * @param st    source file metadata
 */
static void _cp_preserve_owner(const char * dest, const cp_stat_t * st)
{
    if (!dest || !st) {
        return;
    }
#ifdef CP_PLATFORM_POSIX
    (void)chown(dest, st->uid, st->gid);
#else
    (void)dest;
    (void)st;
#endif
}

/**
 * @brief Create a symbolic link (POSIX only; fails on Windows)
 * @param target    link target path
 * @param linkpath  link path to create
 * @return 0 on success, -1 on error
 */
static int _cp_symlink(const char * target, const char * linkpath)
{
    if (!target || !linkpath) {
        return -1;
    }
#ifdef CP_PLATFORM_WINDOWS
    /* Windows symlinks require admin privileges; fall back to copy */
    errno = ENOSYS;
    return -1;
#else
    return symlink(target, linkpath);
#endif
}

/**
 * @brief Create a hard link
 * @param oldpath  existing file path
 * @param newpath  new link path
 * @return 0 on success, -1 on error
 */
static int _cp_link(const char * oldpath, const char * newpath)
{
    if (!oldpath || !newpath) {
        return -1;
    }
#ifdef CP_PLATFORM_WINDOWS
    return (CreateHardLinkA(newpath, oldpath, NULL) != 0) ? 0 : -1;
#else
    return link(oldpath, newpath);
#endif
}

/**
 * @brief Read a symbolic link target (POSIX only)
 * @param linkpath  symlink path
 * @param buf       output buffer
 * @param bufsize   buffer size
 * @return 0 on success, -1 on error
 */
static int _cp_readlink(const char * linkpath, char * buf, size_t bufsize)
{
    if (!linkpath || !buf || bufsize == 0) {
        return -1;
    }
#ifdef CP_PLATFORM_WINDOWS
    errno = ENOSYS;
    return -1;
#else
    ssize_t len = readlink(linkpath, buf, bufsize - 1);
    if (len < 0) {
        return -1;
    }
    buf[len] = '\0';
    return 0;
#endif
}

/**
 * @brief Remove a file
 * @param path  file to remove
 * @return 0 on success, -1 on error
 */
static int _cp_remove(const char * path)
{
    if (!path) {
        return -1;
    }
#ifdef CP_PLATFORM_WINDOWS
    return _unlink(path);
#else
    return unlink(path);
#endif
}

/**
 * @brief Initialize an empty directory list (always succeeds)
 * @param dl  directory list instance
 */
static void _cp_dir_list_init(dir_list_t * dl)
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
 * @param dl    directory list instance
 * @param name  entry name (will be strdup'd)
 */
static void _cp_dir_list_add(dir_list_t * dl, const char * name)
{
    if (!dl || !name) {
        return;
    }
    if (dl->count >= dl->capacity) {
        int new_cap = dl->capacity ? dl->capacity * 2 : 32;
        char ** tmp = (char **)realloc(dl->entries,
                                       (size_t)new_cap * sizeof(char *));
        if (!tmp) {
            return;
        }
        dl->entries = tmp;
        dl->capacity = new_cap;
    }
    char * dup = strdup(name);
    if (!dup) {
        return;
    }
    dl->entries[dl->count] = dup;
    dl->count++;
}

/**
 * @brief Free all memory in a directory list and NULL all pointers.
 * @param dl  directory list instance (may be NULL, may be called twice safely)
 */
static void _cp_dir_list_free(dir_list_t * dl)
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
 * @brief Read directory contents into a list
 * @param path  directory path
 * @param dl    output directory list (caller must call _cp_dir_list_free)
 */
static void _cp_read_directory(const char * path, dir_list_t * dl)
{
    if (!dl) {
        return;
    }
    _cp_dir_list_init(dl);
    if (!path) {
        return;
    }

#ifdef CP_PLATFORM_WINDOWS
    char pattern[CP_MAX_PATH_LEN];
    int w = snprintf(pattern, sizeof(pattern), "%s\\*", path);
    if (w < 0 || (size_t)w >= sizeof(pattern)) {
        return; /* path too long */
    }
    for (char * p = pattern; *p != '\0'; p++) {
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
        if (strcmp(fd.cFileName, ".") == 0 ||
            strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        _cp_dir_list_add(dl, fd.cFileName);
    } while (FindNextFileA(hfind, &fd) != 0);
    (void)FindClose(hfind);
#else
    DIR * dir = opendir(path);
    if (!dir) {
        return;
    }
    struct dirent * de;
    for (;;) {
        de = readdir(dir);
        if (!de) {
            break;
        }
        if (strcmp(de->d_name, ".") == 0 ||
            strcmp(de->d_name, "..") == 0) {
            continue;
        }
        _cp_dir_list_add(dl, de->d_name);
    }
    (void)closedir(dir);
#endif
}

/**
 * @brief Print usage/help information
 */
static void _cp_print_help(void)
{
    cp_printf(
        "Usage: cp [OPTION]... [-T] SOURCE DEST\n"
        "  or:  cp [OPTION]... SOURCE... DIRECTORY\n"
        "  or:  cp [OPTION]... -t DIRECTORY SOURCE...\n"
        "\n"
        "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
        "\n"
        "Options:\n"
        "  -a, --archive          same as -r -p -d\n"
        "  -d                     same as --no-dereference --preserve=links\n"
        "  -f, --force            if an existing destination file cannot be\n"
        "                           opened, remove it and create a new file\n"
        "  -i, --interactive      prompt before overwrite\n"
        "  -l, --link             hard link files instead of copying\n"
        "  -L, --dereference      always follow symbolic links in SOURCE\n"
        "  -n, --no-clobber       do not overwrite an existing file\n"
        "  -P, --no-dereference   never follow symbolic links in SOURCE\n"
        "  -p                     same as --preserve=mode,timestamps\n"
        "      --preserve         preserve the specified attributes\n"
        "  -r, -R, --recursive    copy directories recursively\n"
        "  -s, --symbolic-link    make symbolic links instead of copying\n"
        "  -t, --target-directory  copy all SOURCE arguments into DIRECTORY\n"
        "  -T, --no-target-directory  treat DEST as a normal file\n"
        "  -u, --update           copy only when SOURCE is newer than dest\n"
        "  -v, --verbose          explain what is being done\n"
        "      --help             display this help and exit\n"
        "      --version          output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _cp_print_version(void)
{
    cp_printf("cp %s\n", CP_VERSION_STR);
    cp_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    cp_printf("%s", "License MIT: <https://mit-license.org/>\n");
    cp_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    cp_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Copy file contents from source to destination
 * @param src   source file path
 * @param dst   destination file path
 * @param opts  copy options
 * @return 0 on success, -1 on error
 *
 * Opens the source file for reading and creates the destination file for
 * writing, then copies data in chunks of CP_BUF_SIZE bytes.
 */
static int _cp_copy_file_data(const char * src, const char * dst,
                              const cp_options_t * opts)
{
    if (!src || !dst || !opts) {
        return -1;
    }

    char buf[CP_BUF_SIZE];

    FILE * fin = fopen(src, "rb");
    if (!fin) {
        cp_err_printf("cp: cannot open '%s' for reading: %s\n",
                      src, strerror(errno));
        return -1;
    }

    FILE * fout = fopen(dst, "wb");
    if (!fout) {
        /* If -f, try to remove the destination and retry */
        if (opts->force) {
            (void)_cp_remove(dst);
            fout = fopen(dst, "wb");
        }
        if (!fout) {
            cp_err_printf("cp: cannot create '%s': %s\n",
                          dst, strerror(errno));
            (void)fclose(fin);
            return -1;
        }
    }

    if (opts->verbose) {
        cp_printf("'%s' -> '%s'\n", src, dst);
    }

    /* Copy data in chunks */
    size_t nread;
    while ((nread = fread(buf, 1, sizeof(buf), fin)) > 0) {
        size_t nwritten = fwrite(buf, 1, nread, fout);
        if (nwritten != nread) {
            cp_err_printf("cp: write error: %s\n", strerror(errno));
            (void)fclose(fin);
            (void)fclose(fout);
            return -1;
        }
    }

    if (ferror(fin)) {
        cp_err_printf("cp: read error: %s\n", strerror(errno));
        (void)fclose(fin);
        (void)fclose(fout);
        return -1;
    }

    (void)fclose(fin);
    (void)fclose(fout);

    return 0;
}

/**
 * @brief Copy a single regular file
 * @param src   source file path
 * @param dst   destination file path
 * @param opts  copy options
 * @return 0 on success, -1 on error
 */
static int _cp_copy_regular_file(const char * src, const char * dst,
                                 const cp_options_t * opts)
{
    if (!src || !dst || !opts) {
        return -1;
    }

    cp_stat_t src_st;
    _cp_stat(src, &src_st);

    /* Check -u: only copy if source is newer */
    if (opts->update && _cp_path_exists(dst)) {
        cp_stat_t dst_st;
        _cp_stat(dst, &dst_st);
        if (src_st.mtime <= dst_st.mtime) {
            if (opts->verbose) {
                cp_printf("cp: '%s' - not overwritten (older or same age)\n", src);
            }
            return 0;
        }
    }

    /* Check -n: no clobber */
    if (opts->no_clobber && _cp_path_exists(dst)) {
        if (opts->verbose) {
            cp_printf("cp: '%s' - not overwritten\n", dst);
        }
        return 0;
    }

    /* Check -i: interactive */
    if (opts->interactive && _cp_path_exists(dst)) {
        cp_err_printf("cp: overwrite '%s'? ", dst);
        cp_fflush(cp_err_stream);
        char linebuf[16];
        int answer = 0;
        if (fgets(linebuf, sizeof(linebuf), stdin)) {
            answer = (unsigned char)linebuf[0];
        }
        if (answer != 'y' && answer != 'Y') {
            return 0;
        }
    }

    /* -l: hard link instead of copy */
    if (opts->hard_link) {
        if (_cp_path_exists(dst)) {
            (void)_cp_remove(dst);
        }
        if (_cp_link(src, dst) == 0) {
            if (opts->verbose) {
                cp_printf("'%s' => '%s'\n", src, dst);
            }
            return 0;
        }
        cp_err_printf("cp: cannot create hard link '%s': %s\n",
                      dst, strerror(errno));
        return -1;
    }

    /* -s: symbolic link instead of copy */
    if (opts->symbolic_link) {
        if (_cp_symlink(src, dst) == 0) {
            if (opts->verbose) {
                cp_printf("'%s' -> '%s'\n", src, dst);
            }
            return 0;
        }
        cp_err_printf("cp: cannot create symbolic link '%s': %s\n",
                      dst, strerror(errno));
        return -1;
    }

    /* Copy file data */
    if (_cp_copy_file_data(src, dst, opts) != 0) {
        return -1;
    }

    /* -p: preserve attributes */
    if (opts->preserve) {
        _cp_chmod(dst, src_st.mode);
        _cp_set_time(dst, src_st.atime, src_st.mtime);
        _cp_preserve_owner(dst, &src_st);
    }

    return 0;
}

/**
 * @brief Copy a symbolic link (preserve the link itself)
 * @param src   source symlink path
 * @param dst   destination symlink path
 * @param opts  copy options
 * @return 0 on success, -1 on error
 */
static int _cp_copy_symlink(const char * src, const char * dst,
                            const cp_options_t * opts)
{
    if (!src || !dst || !opts) {
        return -1;
    }

    char target[CP_MAX_PATH_LEN];

    if (_cp_readlink(src, target, sizeof(target)) != 0) {
        cp_err_printf("cp: cannot read link '%s': %s\n",
                      src, strerror(errno));
        return -1;
    }

    /* Remove existing destination if it exists */
    if (_cp_path_exists(dst)) {
        if (opts->no_clobber) {
            return 0;
        }
        if (opts->interactive) {
            cp_err_printf("cp: overwrite '%s'? ", dst);
            cp_fflush(cp_err_stream);
            char linebuf[16];
            int answer = 0;
            if (fgets(linebuf, sizeof(linebuf), stdin)) {
                answer = (unsigned char)linebuf[0];
            }
            if (answer != 'y' && answer != 'Y') {
                return 0;
            }
        }
        (void)_cp_remove(dst);
    }

    if (_cp_symlink(target, dst) != 0) {
        cp_err_printf("cp: cannot create symlink '%s': %s\n",
                      dst, strerror(errno));
        return -1;
    }

    if (opts->verbose) {
        cp_printf("'%s' -> '%s'\n", src, dst);
    }

    return 0;
}

/**
 * @brief Recursively copy a directory tree
 * @param src   source directory path
 * @param dst   destination directory path
 * @param opts  copy options
 * @return 0 on success, -1 on error
 *
 * Algorithm (depth-first):
 *   1. Create the destination directory
 *   2. Read source directory entries
 *   3. For each entry, recursively copy (file or subdirectory)
 *   4. Preserve directory attributes if -p
 */
static int _cp_copy_directory(const char * src, const char * dst,
                              const cp_options_t * opts)
{
    if (!src || !dst || !opts) {
        return -1;
    }

    /* Create destination directory */
    if (!_cp_path_exists(dst)) {
        cp_stat_t src_st;
        _cp_stat(src, &src_st);
        if (_cp_mkdir(dst, src_st.mode) != 0) {
            cp_err_printf("cp: cannot create directory '%s': %s\n",
                          dst, strerror(errno));
            return -1;
        }
    }
    else if (!_cp_is_directory(dst)) {
        cp_err_printf("cp: '%s' is not a directory\n", dst);
        return -1;
    }

    if (opts->verbose) {
        cp_printf("cp: entering directory '%s'\n", src);
    }

    /* Read source directory */
    dir_list_t dl;
    _cp_read_directory(src, &dl);

    /* Copy each entry */
    for (int i = 0; i < dl.count; i++) {
        char src_child[CP_MAX_PATH_LEN];
        char dst_child[CP_MAX_PATH_LEN];
        if (!dl.entries[i]) {
            continue;
        }
        if (_cp_join_path(src_child, sizeof(src_child), src, dl.entries[i]) != 0) {
            cp_err_printf("cp: skipping overlong path in '%s'\n", src);
            continue;
        }
        if (_cp_join_path(dst_child, sizeof(dst_child), dst, dl.entries[i]) != 0) {
            cp_err_printf("cp: skipping overlong path in '%s'\n", dst);
            continue;
        }

        cp_stat_t child_st;
        _cp_stat(src_child, &child_st);

        if (!child_st.exists) {
            continue;
        }

        if (child_st.is_dir) {
            (void)_cp_copy_directory(src_child, dst_child, opts);
        }
        else if (child_st.is_symlink && !opts->dereference) {
            /* Preserve symlink unless -L */
            (void)_cp_copy_symlink(src_child, dst_child, opts);
        }
        else {
            (void)_cp_copy_regular_file(src_child, dst_child, opts);
        }
    }

    _cp_dir_list_free(&dl);

    /* Preserve directory attributes if -p */
    if (opts->preserve) {
        cp_stat_t src_st;
        _cp_stat(src, &src_st);
        _cp_chmod(dst, src_st.mode);
        _cp_set_time(dst, src_st.atime, src_st.mtime);
        _cp_preserve_owner(dst, &src_st);
    }

    return 0;
}

/**
 * @brief Copy a single source (file or directory) to a destination
 * @param src   source path
 * @param dst   destination path
 * @param opts  copy options
 * @return 0 on success, -1 on error
 */
static int _cp_copy_one(const char * src, const char * dst,
                        const cp_options_t * opts)
{
    if (!src || !dst || !opts) {
        return -1;
    }

    cp_stat_t src_st;
    _cp_stat(src, &src_st);

    if (!src_st.exists) {
        cp_err_printf("cp: cannot stat '%s': %s\n", src, strerror(errno));
        return -1;
    }

    /* Prevent copying file to itself */
    if (_cp_same_file(src, dst)) {
        cp_err_printf("cp: '%s' and '%s' are the same file\n", src, dst);
        return -1;
    }

    /* Handle symlinks */
    if (src_st.is_symlink && !opts->dereference) {
        return _cp_copy_symlink(src, dst, opts);
    }

    /* Handle directories */
    if (src_st.is_dir) {
        if (!opts->recursive && !opts->archive) {
            cp_err_printf("cp: -r not specified; omitting directory '%s'\n", src);
            return -1;
        }
        return _cp_copy_directory(src, dst, opts);
    }

    /* Regular file */
    return _cp_copy_regular_file(src, dst, opts);
}

/**
 * @brief Copy multiple sources to a target directory
 * @param sources      array of source paths
 * @param nsrc         number of sources
 * @param target_dir   target directory
 * @param opts         copy options
 * @return 0 on success, -1 on error
 */
static int _cp_copy_to_directory(char ** sources, int nsrc,
                                 const char * target_dir,
                                 const cp_options_t * opts)
{
    if (!sources || !target_dir || !opts) {
        return -1;
    }

    if (!_cp_is_directory(target_dir)) {
        cp_err_printf("cp: target '%s' is not a directory\n", target_dir);
        return -1;
    }

    for (int i = 0; i < nsrc; i++) {
        if (!sources[i]) {
            continue;
        }
        char dst[CP_MAX_PATH_LEN];
        const char * bname = _cp_base_name(sources[i]);
        if (_cp_join_path(dst, sizeof(dst), target_dir, bname) != 0) {
            cp_err_printf("cp: target path too long: '%s'\n", bname);
            continue;
        }
        (void)_cp_copy_one(sources[i], dst, opts);
    }
    return 0;
}

/**
 * @brief Parse command-line options
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        output options struct
 * @param sources     output array of source file paths
 * @param nsrc        output number of sources
 * @param target_dir  output target directory (for -t)
 * @return 0 on success, -1 on error
 */
static int _cp_parse_args(int argc, char ** argv, cp_options_t * opts,
                          char ** sources, int * nsrc, const char ** target_dir)
{
    if (!opts || !sources || !nsrc || !target_dir) {
        return -1;
    }
    memset(opts, 0, sizeof(*opts));
    *nsrc = 0;
    *target_dir = NULL;

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

        /* Long options */
        if (strcmp(arg, "--help") == 0) {
            _cp_print_help();
            exit(0);
        }
        else if (strcmp(arg, "--version") == 0) {
            _cp_print_version();
            exit(0);
        }
        else if (strcmp(arg, "--recursive") == 0) {
            opts->recursive = true;
        }
        else if (strcmp(arg, "--force") == 0) {
            opts->force = true;
        }
        else if (strcmp(arg, "--interactive") == 0) {
            opts->interactive = true;
        }
        else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(arg, "--no-clobber") == 0) {
            opts->no_clobber = true;
        }
        else if (strcmp(arg, "--update") == 0) {
            opts->update = true;
        }
        else if (strcmp(arg, "--archive") == 0) {
            opts->archive = true;
        }
        else if (strcmp(arg, "--dereference") == 0) {
            opts->dereference = true;
        }
        else if (strcmp(arg, "--no-dereference") == 0) {
            opts->no_dereference = true;
        }
        else if (strcmp(arg, "--symbolic-link") == 0) {
            opts->symbolic_link = true;
        }
        else if (strcmp(arg, "--link") == 0) {
            opts->hard_link = true;
        }
        else if (strcmp(arg, "--preserve") == 0) {
            opts->preserve = true;
        }
        else if (strcmp(arg, "--no-target-directory") == 0) {
            opts->no_target_dir = true;
        }
        else if (strcmp(arg, "--target-directory") == 0) {
            if (i + 1 >= argc) {
                cp_err_printf("%s",
                    "cp: option '--target-directory' requires an argument\n");
                return -1;
            }
            opts->target_dir = true;
            *target_dir = argv[i + 1];
            i += 2;
            continue;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options: process each character */
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'r':
                    case 'R':
                        opts->recursive = true;
                        break;

                    case 'p':
                        opts->preserve = true;
                        break;

                    case 'f':
                        opts->force = true;
                        break;

                    case 'i':
                        opts->interactive = true;
                        break;

                    case 'v':
                        opts->verbose = true;
                        break;

                    case 'n':
                        opts->no_clobber = true;
                        break;

                    case 'u':
                        opts->update = true;
                        break;

                    case 'a':
                        opts->archive = true;
                        opts->recursive = true;
                        opts->preserve = true;
                        opts->no_dereference = true;
                        break;

                    case 'd':
                        opts->no_dereference = true;
                        break;

                    case 'L':
                        opts->dereference = true;
                        break;

                    case 'P':
                        opts->no_dereference = true;
                        break;

                    case 's':
                        opts->symbolic_link = true;
                        break;

                    case 'l':
                        opts->hard_link = true;
                        break;

                    case 'T':
                        opts->no_target_dir = true;
                        break;

                    case 't':
                        if (i + 1 >= argc) {
                            cp_err_printf("%s",
                                "cp: option '-t' requires an argument\n");
                            return -1;
                        }
                        opts->target_dir = true;
                        *target_dir = argv[i + 1];
                        i++;
                        break;

                    case 'h':
                        _cp_print_help();
                        exit(0);

                    default:
                        cp_err_printf("cp: invalid option -- '%c'\n", arg[j]);
                        cp_err_printf("%s",
                            "Try 'cp --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            /* Non-option argument: collect as source */
            if (*nsrc < CP_MAX_SOURCES) {
                sources[(*nsrc)++] = arg;
            }
        }
        i++;
    }

    /* Collect remaining arguments after -- */
    while (i < argc) {
        if (!argv[i]) {
            i++;
            continue;
        }
        if (*nsrc < CP_MAX_SOURCES) {
            sources[(*nsrc)++] = argv[i];
        }
        i++;
    }

    return 0;
}
