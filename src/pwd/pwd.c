/**
 * @file pwd.c
 * @brief Cross-platform pwd command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common pwd(1) implementations.
 *
 * Key behaviors:
 *   - -L/--logical: use $PWD from environment when valid
 *   - -P/--physical: resolve all symlinks (default)
 *   - --help / --version: recognized and handled
 *   - Wide-character support on Windows for non-ANSI paths
 *   - UTF-8 output on all platforms
 *   - POSIX-style path handling (forward slashes on all platforms)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o pwd.exe pwd.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o pwd pwd.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o pwd pwd.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o pwd pwd.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o pwd pwd.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o pwd pwd.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/pwd>
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
    #define PWD_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define PWD_PLATFORM_LINUX   1
    #define PWD_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define PWD_PLATFORM_MACOS   1
    #define PWD_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define PWD_PLATFORM_FREEBSD 1
    #define PWD_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define PWD_PLATFORM_OPENBSD 1
    #define PWD_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define PWD_PLATFORM_NETBSD  1
    #define PWD_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define PWD_PLATFORM_POSIX   1
#else
    #define PWD_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef PWD_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef PWD_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef PWD_PLATFORM_NETBSD
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
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef PWD_PLATFORM_WINDOWS
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
#else /* PWD_PLATFORM_POSIX */
    #include <unistd.h>
    #include <fcntl.h>
    #include <limits.h>
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
    #endif
    #ifndef S_ISSOCK
        #define S_ISSOCK(m) (((m) & S_IFSOCK) == S_IFSOCK)
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define PWD_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) - internal defensive cap */
#define PWD_MAX_PATH_LEN 4096

/** @brief Absolute upper bound for path buffers (fail-safe against huge pathconf) */
#define PWD_PATH_HARD_LIMIT (size_t)(1U << 24)

/** @brief Path separator character (POSIX-style output) */
#define PWD_PATH_SEP_CHAR '/'

/** @brief Windows native path separator character */
#define PWD_WIN_SEP_CHAR '\\'

/** @brief Maximum path component length accepted by validation helpers */
#define PWD_PATH_COMPONENT_MAX 4096

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options for pwd
 */
typedef struct {
    bool logical;   /* <- -L, --logical: use $PWD when valid */
    bool physical;  /* <- -P, --physical: real path (default) */
} pwd_opts_t;

/********************************
 *    static prototypes
 ********************************/
static void _pwd_to_posix_slashes(char * path);
static void _pwd_strip_trailing_slashes(char * path);
static int  _pwd_safe_copy(char * dst, const char * src, size_t dst_size);
static int  _pwd_get_physical_pwd(char * buf, size_t size);
static bool _pwd_paths_are_same(const char * a, const char * b);
static bool _pwd_env_is_valid(const char * env_pwd, const char * physical);
static int  _pwd_get_logical_pwd(char * buf, size_t size);
static void _pwd_print_help(void);
static void _pwd_print_version(void);
static int  _pwd_parse_args(int argc, char ** argv, pwd_opts_t * opts);
static bool _pwd_path_component_sane(const char * s);

#ifdef PWD_PLATFORM_WINDOWS
static int _pwd_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size);
static int _pwd_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars);
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for pwd_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef pwd_out_stream
    #define pwd_out_stream stdout
#endif

/**
 * @brief Default stderr stream for pwd_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef pwd_err_stream
    #define pwd_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef pwd_printf
    #define pwd_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef pwd_err_printf
    #define pwd_err_printf(fmt, ...) \
        do { if (pwd_err_stream) { (void)fprintf((pwd_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef pwd_fflush
    #define pwd_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 *        Two NULLs considered equal (both missing).
 * @return true if strings match, false otherwise
 */
#ifndef pwd_streq
    #define pwd_streq(a, b) \
        (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/**
 * @brief Safe strncmp wrapper with NULL guards and explicit size.
 */
#ifndef pwd_strneq
    #define pwd_strneq(a, b, n) \
        (((a) && (b)) ? (strncmp((a), (b), (n)) == 0) : false)
#endif

/**
 * @brief Safe strchr wrapper with NULL guard.
 *        Returns NULL if input string is NULL.
 */
#ifndef pwd_strchr
    #define pwd_strchr(s, c) (((s)) ? strchr((s), (c)) : NULL)
#endif

/**
 * @brief Safe getenv wrapper — returns empty string "" instead of NULL
 *        so downstream string operations are always NULL-safe.
 */
#ifndef pwd_getenv_safe
    #define pwd_getenv_safe(name) \
        ({ const char * _v = getenv((name)); _v ? _v : ""; })
#endif

/**
 * @brief Safe free-and-null pointer cleanup macro.
 */
#ifndef pwd_safe_free
    #define pwd_safe_free(p) \
        do { if ((p)) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the pwd command
 *
 * Processing flow:
 *   1. Parse command-line options (-L / -P)
 *   2. Retrieve logical or physical cwd
 *   3. Print the path
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error, 2 on bad options
 */
int main(int argc, char ** argv)
{
    pwd_opts_t opts;

    memset(&opts, 0, sizeof(opts));
    opts.physical = true;

    if (_pwd_parse_args(argc, argv, &opts) != 0) {
        return 2;
    }

    char buf[PWD_MAX_PATH_LEN];
    memset(buf, 0, sizeof(buf));
    int rc;
    if (opts.logical) {
        rc = _pwd_get_logical_pwd(buf, sizeof(buf));
    }
    else {
        rc = _pwd_get_physical_pwd(buf, sizeof(buf));
    }

    if (rc != 0) {
        int e = errno ? errno : ENOENT;
        pwd_err_printf("pwd: error retrieving current directory: %s\n",
                       strerror(e));
        return 1;
    }

    /* Defensive: ensure NUL termination before write. */
    if (buf[sizeof(buf) - 1] != '\0') {
        buf[sizeof(buf) - 1] = '\0';
    }
    pwd_printf("%s\n", buf);
    pwd_fflush(pwd_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Validate per-component length of a path.
 * @param s  path string
 * @return true if every component fits within PWD_PATH_COMPONENT_MAX
 */
static bool _pwd_path_component_sane(const char * s)
{
    if (!s) {
        return false;
    }
    size_t run = 0;
    for (const char * p = s; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\' || *p == ':') {
            run = 0;
        }
        else {
            run++;
            if (run > PWD_PATH_COMPONENT_MAX) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Replace every Windows backslash separator with a POSIX forward slash,
 *        in-place.
 * @param path  mutable path string (NUL-terminated)
 */
static void _pwd_to_posix_slashes(char * path)
{
    if (!path) {
        return;
    }
    for (char * p = path; *p != '\0'; p++) {
        if (*p == PWD_WIN_SEP_CHAR) {
            *p = PWD_PATH_SEP_CHAR;
        }
    }
}

/**
 * @brief Strip trailing path separators from a path string in-place.
 *        Preserves a single leading "/" for the root directory.
 * @param path  mutable path string (NUL-terminated)
 */
static void _pwd_strip_trailing_slashes(char * path)
{
    if (!path) {
        return;
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == PWD_PATH_SEP_CHAR) {
        path[len - 1] = '\0';
        len--;
    }
    /* Also remove trailing Windows separators if any remain */
    len = strlen(path);
    while (len > 1 && path[len - 1] == PWD_WIN_SEP_CHAR) {
        path[len - 1] = '\0';
        len--;
    }
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
 * @return 0 on success, -1 if dst_size is too small
 */
static int _pwd_safe_copy(char * dst, const char * src, size_t dst_size)
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
        /* Exact size boundary: cannot store NUL if slen == dst_size */
        return -1;
    }
    memcpy(dst, src, slen);
    dst[slen] = '\0';
    return 0;
}

#ifdef PWD_PLATFORM_WINDOWS

/**
 * @brief Convert a wide (UTF-16) string to a UTF-8 multi-byte string.
 * @param wide      input UTF-16 NUL-terminated string
 * @param out       output buffer (must be at least out_size bytes)
 * @param out_size  size of the output buffer in bytes
 * @return number of bytes written (excluding NUL) on success, -1 on failure
 */
static int _pwd_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size)
{
    if (!out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!wide) {
        return -1;
    }
    int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) {
        return -1;
    }
    /* needed includes trailing NUL; we also need room for a NUL even if
     * needed fits exactly. */
    if (needed < 1 || needed > (int)out_size) {
        return -1;
    }
    int written = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
    if (written <= 0) {
        out[0] = '\0';
        return -1;
    }
    /* Paranoia: explicit NUL placement in case a future API change
     * fails to write the terminator for an edge-case input. */
    if ((size_t)written < out_size) {
        out[written] = '\0';
    }
    else {
        out[out_size - 1] = '\0';
    }
    return written - 1;
}

/**
 * @brief Convert a UTF-8 multi-byte string to a UTF-16 (wide) string.
 * @param utf8        input UTF-8 NUL-terminated string
 * @param out         output wide buffer
 * @param out_wchars  size of output buffer in wchar_t elements
 * @return number of wchar_t written (excluding NUL), or -1 on failure
 */
static int _pwd_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars)
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
    if (needed < 1 || needed > (int)out_wchars) {
        return -1;
    }
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_wchars);
    if (written <= 0) {
        out[0] = L'\0';
        return -1;
    }
    if ((size_t)written < out_wchars) {
        out[written] = L'\0';
    }
    else {
        out[out_wchars - 1] = L'\0';
    }
    return written - 1;
}

/**
 * @brief Get the current working directory as a UTF-8 POSIX-style path.
 * @param buf   output buffer (NUL-terminated UTF-8, forward slashes)
 * @param size  size of output buffer in bytes
 * @return 0 on success, -1 on failure
 */
static int _pwd_get_physical_pwd(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';

    /* Bounds-check size before casting to DWORD to prevent narrowing.
     * DWORD_MAX is 0xFFFFFFFF, which is larger than any reasonable
     * allocation, but we still want to catch the impossible truncation. */
    if (size > (size_t)0x7FFFFFFF) {
        return -1;
    }

    wchar_t wbuf[PWD_MAX_PATH_LEN + 2];
    DWORD buf_wchars = (DWORD)(sizeof(wbuf) / sizeof(wbuf[0]));
    DWORD n = GetCurrentDirectoryW(buf_wchars, wbuf);
    if (n == 0) {
        return -1;
    }
    /* If the exact returned size equals or exceeds the buffer size, the
     * cwd was truncated and is unreliable — must fail. */
    if (n >= buf_wchars) {
        return -1;
    }
    /* n == 0 is already caught above; this check is for the impossible
     * case where GetCurrentDirectoryW returns 1 for "empty" strings. */
    if (n >= (sizeof(wbuf) / sizeof(wbuf[0]))) {
        return -1;
    }

    char utf8[PWD_MAX_PATH_LEN + 4];
    if (_pwd_wide_to_utf8(wbuf, utf8, sizeof(utf8)) < 0) {
        return -1;
    }
    if (!_pwd_path_component_sane(utf8)) {
        return -1;
    }
    _pwd_to_posix_slashes(utf8);
    _pwd_strip_trailing_slashes(utf8);
    /* Preserve drive letter root form like C:/ */
    size_t ul = strlen(utf8);
    if (ul == 2 && utf8[1] == ':') {
        if (ul + 2 > sizeof(utf8)) {
            return -1;
        }
        utf8[ul++] = PWD_PATH_SEP_CHAR;
        utf8[ul] = '\0';
    }
    return _pwd_safe_copy(buf, utf8, size);
}

/**
 * @brief Check if two paths refer to the same directory/volume entry.
 *        Opens both handles, compares volume + file index,
 *        closes both handles before returning (no leak on early exit).
 * @param a  first path (UTF-8)
 * @param b  second path (UTF-8)
 * @return true if equivalent, false otherwise
 */
static bool _pwd_paths_are_same(const char * a, const char * b)
{
    if (!a || !b) {
        return false;
    }
    if (!_pwd_path_component_sane(a) || !_pwd_path_component_sane(b)) {
        return false;
    }
    if (strcmp(a, b) == 0) {
        return true;
    }
    wchar_t wa[PWD_MAX_PATH_LEN + 4];
    wchar_t wb[PWD_MAX_PATH_LEN + 4];
    if (_pwd_utf8_to_wide(a, wa, (sizeof(wa) / sizeof(wa[0]))) < 0) {
        return false;
    }
    if (_pwd_utf8_to_wide(b, wb, (sizeof(wb) / sizeof(wb[0]))) < 0) {
        return false;
    }
    /* Convert to native backslashes for Windows API */
    for (wchar_t * p = wa; *p != L'\0'; p++) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }
    for (wchar_t * p = wb; *p != L'\0'; p++) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }
    HANDLE ha = CreateFileW(wa, 0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (ha == INVALID_HANDLE_VALUE) {
        return false;
    }
    HANDLE hb = CreateFileW(wb, 0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hb == INVALID_HANDLE_VALUE) {
        (void)CloseHandle(ha);
        return false;
    }
    BY_HANDLE_FILE_INFORMATION ia, ib;
    BOOL oka = GetFileInformationByHandle(ha, &ia);
    BOOL okb = GetFileInformationByHandle(hb, &ib);
    bool equal = false;
    if (oka && okb) {
        if (ia.dwVolumeSerialNumber == ib.dwVolumeSerialNumber &&
            ia.nFileIndexHigh == ib.nFileIndexHigh &&
            ia.nFileIndexLow == ib.nFileIndexLow) {
            equal = true;
        }
    }
    (void)CloseHandle(ha);
    (void)CloseHandle(hb);
    return equal;
}

#else /* PWD_PLATFORM_POSIX */

/**
 * @brief Get the current working directory as a POSIX path (physical).
 *
 * Uses a heap buffer sized via pathconf(_PC_PATH_MAX) if available,
 * bounded by PWD_PATH_HARD_LIMIT so pathological systems cannot
 * cause allocations measured in gigabytes.
 *
 * @param buf   output buffer
 * @param size  size of output buffer in bytes
 * @return 0 on success, -1 on failure
 */
static int _pwd_get_physical_pwd(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';

    size_t alloc_sz = PWD_MAX_PATH_LEN;
    long path_max = pathconf("/", _PC_PATH_MAX);
    if (path_max > 0) {
        /* Clamp against hard limit and caller buffer size. */
        size_t pms = (size_t)path_max;
        if (pms > PWD_PATH_HARD_LIMIT) {
            pms = PWD_PATH_HARD_LIMIT;
        }
        if (pms > alloc_sz) {
            alloc_sz = pms;
        }
    }
    if (alloc_sz < size) {
        alloc_sz = size;
    }
    /* Final cap to prevent any runaway allocation. */
    if (alloc_sz > PWD_PATH_HARD_LIMIT) {
        alloc_sz = PWD_PATH_HARD_LIMIT;
    }

    void * raw = malloc(alloc_sz);
    if (!raw) {
        /* Fallback: try direct call with provided buffer (may still succeed) */
        if (getcwd(buf, size) != NULL) {
            _pwd_strip_trailing_slashes(buf);
            if (!_pwd_path_component_sane(buf)) {
                buf[0] = '\0';
                return -1;
            }
            return 0;
        }
        return -1;
    }
    char * tmp = (char *)raw;
    if (getcwd(tmp, alloc_sz) == NULL) {
        pwd_safe_free(raw);
        return -1;
    }
    if (!_pwd_path_component_sane(tmp)) {
        pwd_safe_free(raw);
        return -1;
    }
    _pwd_strip_trailing_slashes(tmp);
    int rc = _pwd_safe_copy(buf, tmp, size);
    pwd_safe_free(raw);
    return rc;
}

/**
 * @brief Check if two paths refer to the same directory entry (dev + inode).
 * @param a  first path
 * @param b  second path
 * @return true if equivalent, false otherwise
 */
static bool _pwd_paths_are_same(const char * a, const char * b)
{
    if (!a || !b) {
        return false;
    }
    if (!_pwd_path_component_sane(a) || !_pwd_path_component_sane(b)) {
        return false;
    }
    if (strcmp(a, b) == 0) {
        return true;
    }
    struct stat sa, sb;
    if (stat(a, &sa) != 0) {
        return false;
    }
    if (stat(b, &sb) != 0) {
        return false;
    }
    return (sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino);
}

#endif /* PWD_PLATFORM_POSIX */

/**
 * @brief Validate whether $PWD points at the same directory as the physical cwd.
 *
 * POSIX 2013: $PWD is valid only if:
 *   1. It is an absolute path
 *   2. It contains no "." or ".." components
 *   3. It refers to the same directory as cwd
 *
 * @param env_pwd   candidate $PWD value
 * @param physical  real physical cwd
 * @return true if valid logical pwd, false otherwise
 */
static bool _pwd_env_is_valid(const char * env_pwd, const char * physical)
{
    if (!env_pwd || env_pwd[0] == '\0') {
        return false;
    }
    if (!physical || physical[0] == '\0') {
        return false;
    }
    if (!_pwd_path_component_sane(env_pwd)) {
        return false;
    }

    /* 1. Must be absolute path */
    bool absolute = false;
    if (env_pwd[0] == PWD_PATH_SEP_CHAR) {
        absolute = true;
    }
#ifdef PWD_PLATFORM_WINDOWS
    /* Also accept C:/ or C:\ style absolute on Windows */
    size_t el = strlen(env_pwd);
    if (el >= 3 && isalpha((unsigned char)env_pwd[0]) && env_pwd[1] == ':' &&
        (env_pwd[2] == PWD_PATH_SEP_CHAR || env_pwd[2] == PWD_WIN_SEP_CHAR)) {
        absolute = true;
    }
#endif
    if (!absolute) {
        return false;
    }

    /* 2. Reject if any component is "." or "..", or contains weird bytes. */
    char copy[PWD_MAX_PATH_LEN];
    if (_pwd_safe_copy(copy, env_pwd, sizeof(copy)) != 0) {
        return false;
    }
    _pwd_to_posix_slashes(copy);

    char * saveptr = NULL;
    char * tok = strtok_r(copy, "/", &saveptr);
    while (tok != NULL) {
        size_t tlen = strlen(tok);
        if (tlen == 0) {
            tok = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        if (tlen > PWD_PATH_COMPONENT_MAX) {
            return false;
        }
        if (strcmp(tok, ".") == 0) {
            return false;
        }
        if (strcmp(tok, "..") == 0) {
            return false;
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }

    /* 3. Same directory as physical */
    return _pwd_paths_are_same(env_pwd, physical);
}

/**
 * @brief Retrieve "logical" pwd using $PWD when valid, else fall back to physical.
 * @param buf   output buffer (NUL-terminated, POSIX-style)
 * @param size  output buffer size
 * @return 0 on success, -1 on failure
 */
static int _pwd_get_logical_pwd(char * buf, size_t size)
{
    char physical[PWD_MAX_PATH_LEN];
    if (_pwd_get_physical_pwd(physical, sizeof(physical)) != 0) {
        return -1;
    }
    const char * env_p = getenv("PWD");
    if (_pwd_env_is_valid(env_p, physical)) {
        char tmp[PWD_MAX_PATH_LEN];
        if (_pwd_safe_copy(tmp, env_p, sizeof(tmp)) != 0) {
            return -1;
        }
        if (!_pwd_path_component_sane(tmp)) {
            return -1;
        }
        _pwd_to_posix_slashes(tmp);
        _pwd_strip_trailing_slashes(tmp);
#ifdef PWD_PLATFORM_WINDOWS
        size_t ul = strlen(tmp);
        if (ul == 2 && tmp[1] == ':') {
            if (ul + 2 > sizeof(tmp)) {
                return -1;
            }
            tmp[ul++] = PWD_PATH_SEP_CHAR;
            tmp[ul] = '\0';
        }
#endif
        return _pwd_safe_copy(buf, tmp, size);
    }
    /* Fallback to physical */
    return _pwd_safe_copy(buf, physical, size);
}

/**
 * @brief Print usage/help information
 */
static void _pwd_print_help(void)
{
    pwd_printf(
        "Usage: pwd [OPTION]...\n"
        "Print the full filename of the current working directory.\n"
        "\n"
        "  -L, --logical   use PWD from environment, even if it contains symlinks\n"
        "  -P, --physical  avoid all symlinks\n"
        "      --help      display this help and exit\n"
        "      --version   output version information and exit\n"
        "\n"
        "If no option is specified, -P is assumed.\n"
        "If both -L and -P are given, the last one takes effect.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _pwd_print_version(void)
{
    pwd_printf("pwd %s\n", PWD_VERSION_STR);
    pwd_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    pwd_printf("%s", "License MIT: <https://mit-license.org/>\n");
    pwd_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    pwd_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments into pwd_opts_t
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options structure
 * @return 0 on success, -1 on unknown option
 */
static int _pwd_parse_args(int argc, char ** argv, pwd_opts_t * opts)
{
    if (!opts) {
        return -1;
    }
    opts->physical = true;  /* default: physical */

    if (argc < 1 || !argv) {
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            continue;
        }

        if (strncmp(arg, "--", 2) == 0) {
            /* Long option: extract name before '=' if present */
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[64];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _pwd_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _pwd_print_version();
                exit(0);
            }
            if (strcmp(name, "logical") == 0) {
                opts->logical = true;
                opts->physical = false;
            }
            else if (strcmp(name, "physical") == 0) {
                opts->physical = true;
                opts->logical = false;
            }
            else {
                pwd_err_printf("pwd: unrecognized option '%s'\n", arg);
                pwd_err_printf("%s", "Try 'pwd --help' for more information.\n");
                return -1;
            }
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options */
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'L':
                        opts->logical = true;
                        opts->physical = false;
                        break;

                    case 'P':
                        opts->physical = true;
                        opts->logical = false;
                        break;

                    default:
                        pwd_err_printf("pwd: invalid option -- '%c'\n", arg[j]);
                        pwd_err_printf("%s",
                            "Try 'pwd --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            /* Positional argument: error */
            pwd_err_printf("pwd: extra operand '%s'\n", arg);
            pwd_err_printf("%s", "Try 'pwd --help' for more information.\n");
            return -1;
        }
    }

    return 0;
}
