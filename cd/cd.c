/**
 * @file cd.c
 * @brief Cross-platform cd command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common cd(1) implementations for a
 * standalone helper (see note below about shell builtins).
 *
 * Key behaviors:
 *   - -L/--logical: use $PWD when traversing ".." (default)
 *   - -P/--physical: resolve symlinks in the final path
 *   - -e/--fail-exit: non-zero if final dir cannot be determined
 *   - "cd" with no operand: go to $HOME
 *   - "cd -": go to $OLDPWD and print the resulting directory
 *   - "~" / "~/foo": tilde expansion of $HOME
 *   - CDPATH search (colon on POSIX, semicolon on Windows)
 *   - POSIX-style output paths (forward slashes, UTF-8)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o cd.exe cd.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o cd cd.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o cd cd.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o cd cd.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o cd cd.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o cd cd.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/cd>
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
    #define CD_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define CD_PLATFORM_LINUX   1
    #define CD_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define CD_PLATFORM_MACOS   1
    #define CD_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define CD_PLATFORM_FREEBSD 1
    #define CD_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define CD_PLATFORM_OPENBSD 1
    #define CD_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define CD_PLATFORM_NETBSD  1
    #define CD_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define CD_PLATFORM_POSIX   1
#else
    #define CD_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef CD_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef CD_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef CD_PLATFORM_NETBSD
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
#include <sys/types.h>
#include <sys/stat.h>

#ifdef CD_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
#else
    #include <unistd.h>
    #include <sys/param.h>
    #include <limits.h>
    #ifdef CD_PLATFORM_FREEBSD
        #include <sys/syslimits.h>
    #endif
    #if defined(CD_PLATFORM_BSD) || defined(CD_PLATFORM_MACOS)
        #include <sys/sysctl.h>
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define CD_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define CD_MAX_PATH_LEN 4096

/** @brief Path separator character (POSIX-style output) */
#define CD_PATH_SEP_CHAR '/'

/** @brief Path separator as a string literal */
#define CD_PATH_SEP_STR  "/"

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Command-line options for cd
 */
typedef struct {
    bool logical;    /* <- -L/--logical (default) */
    bool physical;   /* <- -P/--physical */
    bool fail_exit;  /* <- -e/--fail-exit */
    bool help;
    bool version;
    const char * dir; /* <- directory operand (or NULL) */
    bool print_old;   /* <- "cd -" was used */
} cd_opts_t;

/********************************
 *    static prototypes
 ********************************/
static int  _cd_safe_copy(char * dst, const char * src, size_t dst_size);
static void _cd_to_posix_slashes(char * p);
static void _cd_strip_trailing_slashes(char * p);
static const char * _cd_get_env_dup(const char * name);
static int  _cd_set_env(const char * name, const char * value);
static int  _cd_path_is_dir(const char * path);
static int  _cd_is_absolute_path(const char * p);
static int  _cd_get_physical_pwd(char * buf, size_t size);
static int  _cd_do_chdir(const char * path);
static int  _cd_resolve_physical_path(const char * dir, char * buf, size_t size);
static int  _cd_expand_home(const char * input, char * out, size_t out_size);
static int  _cd_cdpath_search(const char * target, char * out, size_t out_size);
static void _cd_print_help(void);
static void _cd_print_version(void);
static void _cd_usage_err(const char * msg);
static int  _cd_parse_long_option(const char * a, cd_opts_t * opts);
static int  _cd_parse_short_options(const char * s, cd_opts_t * opts, int * consumed_arg);
static int  _cd_parse_args(int argc, char ** argv, cd_opts_t * opts);
static int  _cd_set_pwd_oldpwd(const char * new_pwd);

#ifdef CD_PLATFORM_WINDOWS
static int _cd_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size);
static int _cd_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for cd_printf / cd_fputs.
 *        Defaults to libc @c stdout.
 *        Define externally to redirect all stream output.
 */
#ifndef cd_out_stream
    #define cd_out_stream stdout
#endif

/**
 * @brief Default error stream for cd_err_printf.
 *        Defaults to libc @c stderr.
 *        Define externally to redirect all error output.
 */
#ifndef cd_err_stream
    #define cd_err_stream stderr
#endif

/**
 * @brief Formatted print to the output stream (printf-compatible).
 *        Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef cd_printf
    #define cd_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to the error stream (fprintf-compatible).
 *        Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef cd_err_printf
    #define cd_err_printf(fmt, ...) fprintf(cd_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs().
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream
 */
#ifndef cd_fputs
    #define cd_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      character (promoted to int)
 * @param stream  stdio stream
 */
#ifndef cd_fputc
    #define cd_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the cd command
 *
 * Processing flow:
 *   1. Parse options (-L/-P/-e, --help, --version) and the directory operand
 *   2. Resolve raw target (HOME if none, OLDPWD if "-")
 *   3. Tilde expansion
 *   4. CDPATH search (if not absolute / not ./.. / no slash)
 *   5. Verify target is a directory
 *   6. -P: chdir then derive physical path; -L: build logical path then chdir
 *   7. Set PWD/OLDPWD and print the resolved path
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error, 2 on unknown option
 */
int main(int argc, char ** argv)
{
    cd_opts_t opts;
    int parse_rc = _cd_parse_args(argc, argv, &opts);
    if (parse_rc != 0) {
        return parse_rc;
    }
    if (opts.help) {
        _cd_print_help();
        return 0;
    }
    if (opts.version) {
        _cd_print_version();
        return 0;
    }

    /* Step 1: determine raw target directory */
    const char * raw = opts.dir;
    char home_buf[CD_MAX_PATH_LEN];
    if (!raw) {
        /* No operand: go HOME (GNU behaviour) */
        const char * home = _cd_get_env_dup("HOME");
#ifdef CD_PLATFORM_WINDOWS
        if (!home) {
            home = _cd_get_env_dup("USERPROFILE");
        }
#endif
        if (!home) {
            _cd_usage_err("HOME not set");
            return 1;
        }
        if (_cd_safe_copy(home_buf, home, sizeof(home_buf)) != 0) {
            return 1;
        }
        raw = home_buf;
    }
    else if (strcmp(raw, "-") == 0) {
        const char * oldpwd = _cd_get_env_dup("OLDPWD");
        if (!oldpwd) {
            _cd_usage_err("OLDPWD not set");
            return 1;
        }
        if (_cd_safe_copy(home_buf, oldpwd, sizeof(home_buf)) != 0) {
            return 1;
        }
        raw = home_buf;
    }

    /* Step 2: tilde expansion */
    char expanded[CD_MAX_PATH_LEN];
    if (_cd_expand_home(raw, expanded, sizeof(expanded)) != 0) {
        cd_err_printf("cd: home directory expansion failed: %s\n", strerror(errno));
        return 1;
    }

    /* Step 3: CDPATH search, only if not absolute, not ./.., no slash */
    char final_target[CD_MAX_PATH_LEN];
    if (_cd_cdpath_search(expanded, final_target, sizeof(final_target)) != 0) {
        if (_cd_safe_copy(final_target, expanded, sizeof(final_target)) != 0) {
            return 1;
        }
    }

    /* Step 4: make sure it's a directory (before trying to chdir) */
    if (!_cd_path_is_dir(final_target)) {
        cd_err_printf("cd: %s: %s\n", final_target,
                      (errno == 0) ? "No such directory" : strerror(errno));
        return 1;
    }

    /* Step 5: physical chdir */
    char physical_result[CD_MAX_PATH_LEN];
    physical_result[0] = '\0';

    if (opts.physical) {
        if (_cd_do_chdir(final_target) != 0) {
            cd_err_printf("cd: %s: %s\n", final_target, strerror(errno));
            return 1;
        }
        if (_cd_get_physical_pwd(physical_result, sizeof(physical_result)) != 0) {
            /* -P + -e: fail if we can't get physical */
            if (opts.fail_exit) {
                cd_err_printf("%s", "cd: could not determine physical directory\n");
                return 1;
            }
        }
        /* Also attempt realpath for even-more-correct final form */
        char real[CD_MAX_PATH_LEN];
        if (_cd_resolve_physical_path(".", real, sizeof(real)) == 0) {
            _cd_safe_copy(physical_result, real, sizeof(physical_result));
        }
        if (physical_result[0] == '\0') {
            /* Fallback */
            if (_cd_get_physical_pwd(physical_result, sizeof(physical_result)) != 0) {
                cd_err_printf("cd: could not determine directory: %s\n", strerror(errno));
                return 1;
            }
        }
        _cd_set_pwd_oldpwd(physical_result);
        if (opts.print_old) {
            puts(physical_result);
        }
        else {
            cd_fputs(physical_result, cd_out_stream);
        }
        cd_fputc('\n', cd_out_stream);
        return 0;
    }

    /* Logical (-L, default): build logical path then chdir. */
    {
        char * work = (char *)malloc(CD_MAX_PATH_LEN * 2);
        if (!work) {
            cd_err_printf("%s", "cd: out of memory\n");
            return 1;
        }
        if (_cd_is_absolute_path(final_target)) {
            if (_cd_safe_copy(work, final_target, CD_MAX_PATH_LEN * 2) != 0) {
                free(work);
                return 1;
            }
        }
        else {
            const char * pwd_env = _cd_get_env_dup("PWD");
            char base[CD_MAX_PATH_LEN];
            if (pwd_env) {
                _cd_safe_copy(base, pwd_env, sizeof(base));
            }
            else if (_cd_get_physical_pwd(base, sizeof(base)) != 0) {
                free(work);
                return 1;
            }
            _cd_to_posix_slashes(base);
            _cd_strip_trailing_slashes(base);
            size_t bl = strlen(base);
            size_t tl = strlen(final_target);
            if (bl + 1 + tl + 1 > CD_MAX_PATH_LEN * 2) {
                free(work);
                return 1;
            }
            size_t u = 0;
            memcpy(work, base, bl);
            u = bl;
            if (u > 0 && work[u - 1] != CD_PATH_SEP_CHAR) {
                work[u++] = CD_PATH_SEP_CHAR;
            }
            memcpy(work + u, final_target, tl);
            u += tl;
            work[u] = '\0';
        }
        _cd_to_posix_slashes(work);

        /* Normalize the logical path: collapse . and .. entries without
         * resolving symlinks. */
        char * out = (char *)malloc(CD_MAX_PATH_LEN * 2);
        if (!out) {
            free(work);
            cd_err_printf("%s", "cd: out of memory\n");
            return 1;
        }
#ifdef CD_PLATFORM_WINDOWS
        /* Windows: preserve drive letter / UNC prefix first, then set a
         * single flag that governs whether we need a leading slash */
        char prefix[16]; /* "C:" or "//server/share" start */
        size_t prefix_len = 0;
        int has_prefix = 0;
        int need_slash_after_prefix = 0;
        if (work[0] != '\0' && work[1] == ':') {
            /* drive letter: "C:foo" => prefix="C:", need leading slash */
            prefix[0] = work[0];
            prefix[1] = ':';
            prefix[2] = 0;
            prefix_len = 2;
            has_prefix = 1;
            need_slash_after_prefix = 1;
        }
        else if ((work[0] == '/' && work[1] == '/') ||
                 (work[0] == '\\' && work[1] == '\\')) {
            /* UNC start "//"; keep verbatim as prefix */
            const char * end = work + 2;
            while (*end && (*end != '/' && *end != '\\')) {
                end++;
            }
            if (*end) {
                end++;
            }
            size_t usz = (size_t)(end - work);
            if (usz + 1 > sizeof(prefix)) {
                usz = 0;
            }
            if (usz) {
                memcpy(prefix, work, usz);
                prefix[usz] = '\0';
                prefix_len = usz;
                has_prefix = 1;
                need_slash_after_prefix = 0;
            }
        }
        int absolute = has_prefix || _cd_is_absolute_path(work);
        (void)absolute;
#else
        int absolute = _cd_is_absolute_path(work);
#endif
        char ** stack = (char **)malloc(sizeof(char *) * (strlen(work) + 1));
        size_t scount = 0;
        if (!stack) {
            free(out);
            free(work);
            return 1;
        }

        char * save = NULL;
        char * tok = strtok_r(work, "/\\", &save);
        while (tok) {
            if (tok[0] == '\0') {
                tok = strtok_r(NULL, "/\\", &save);
                continue;
            }
            if (strcmp(tok, ".") == 0) {
                tok = strtok_r(NULL, "/\\", &save);
                continue;
            }
            if (strcmp(tok, "..") == 0) {
                if (scount > 0) {
                    free(stack[--scount]);
                }
                tok = strtok_r(NULL, "/\\", &save);
                continue;
            }
#ifdef CD_PLATFORM_WINDOWS
            /* Strip trailing ':' on first token (drive letter) so it's not
             * duplicated into the path body when a prefix has been set. */
            if (scount == 0 && has_prefix && prefix_len == 2 &&
                tok[0] == prefix[0] && tok[1] == ':') {
                tok = strtok_r(NULL, "/\\", &save);
                continue;
            }
#endif
            stack[scount++] = strdup(tok);
            tok = strtok_r(NULL, "/\\", &save);
        }

        /* Reconstruct */
        size_t ou = 0;
#ifdef CD_PLATFORM_WINDOWS
        if (has_prefix) {
            memcpy(out, prefix, prefix_len);
            ou = prefix_len;
        }
        if (has_prefix && need_slash_after_prefix) {
            out[ou++] = CD_PATH_SEP_CHAR;
        }
#else
        if (absolute) {
            out[ou++] = CD_PATH_SEP_CHAR;
        }
#endif
        for (size_t i = 0; i < scount; i++) {
            size_t sl = strlen(stack[i]);
            if (ou > 0 && out[ou - 1] != CD_PATH_SEP_CHAR) {
                out[ou++] = CD_PATH_SEP_CHAR;
            }
            memcpy(out + ou, stack[i], sl);
            ou += sl;
            out[ou] = '\0';
            free(stack[i]);
        }
#ifdef CD_PLATFORM_WINDOWS
        if (ou == 0) {
            out[ou++] = '.';
            out[ou] = '\0';
        }
        if (has_prefix && prefix_len == 2 && ou == 2) {
            /* Only "C:" so far; append / to make it "C:/" */
            out[ou++] = CD_PATH_SEP_CHAR;
            out[ou] = '\0';
        }
#else
        if (ou == 0) {
            out[ou++] = CD_PATH_SEP_CHAR;
            out[ou] = '\0';
        }
#endif
        free(stack);
        free(work);

        /* Do actual chdir */
        if (_cd_do_chdir(final_target) != 0) {
            cd_err_printf("cd: %s: %s\n", final_target, strerror(errno));
            free(out);
            return 1;
        }

        /* Write results */
        char * result = out;
        _cd_strip_trailing_slashes(result);
#ifdef CD_PLATFORM_WINDOWS
        {
            size_t rl = strlen(result);
            if (rl == 2 && result[1] == ':') {
                if (rl + 2 < CD_MAX_PATH_LEN * 2) {
                    result[rl++] = CD_PATH_SEP_CHAR;
                    result[rl] = '\0';
                }
            }
        }
#endif
        /* Guard: ensure logical result still points at the right place if
         * the caller later uses -P semantics. Skip if logical dir !=
         * physical dir at the end – just fall back to physical for safety. */
        {
            char phys2[CD_MAX_PATH_LEN];
            if (_cd_get_physical_pwd(phys2, sizeof(phys2)) != 0) {
                cd_err_printf("cd: could not determine directory: %s\n", strerror(errno));
                free(out);
                return 1;
            }
            /* Use our logical result as PWD (POSIX/GNU), but set OLDPWD too. */
            _cd_set_pwd_oldpwd(result);
            if (opts.print_old) {
                puts(result);
            }
            else {
                cd_fputs(result, cd_out_stream);
            }
            cd_fputc('\n', cd_out_stream);
        }
        free(out);
    }
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Bounded string copy with explicit NUL termination.
 *        Rejects truncation so callers can detect overflow safely.
 * @param dst       destination buffer
 * @param src       source string (may be NULL)
 * @param dst_size  size of destination buffer in bytes
 * @return 0 on success, -1 on NULL/empty buffer or truncation
 */
static int _cd_safe_copy(char * dst, const char * src, size_t dst_size)
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
 * @brief Convert backslashes to POSIX forward slashes in place.
 * @param p  path buffer (may be NULL)
 */
static void _cd_to_posix_slashes(char * p)
{
    if (!p) {
        return;
    }
    for (; *p; p++) {
        if (*p == '\\') {
            *p = CD_PATH_SEP_CHAR;
        }
    }
}

/**
 * @brief Remove trailing path separators (keep root "/").
 * @param p  path buffer (may be NULL)
 */
static void _cd_strip_trailing_slashes(char * p)
{
    if (!p) {
        return;
    }
    size_t len = strlen(p);
    while (len > 1 && p[len - 1] == CD_PATH_SEP_CHAR) {
        p[--len] = '\0';
    }
}

/**
 * @brief Look up an environment variable, returning a stable duplicate.
 *        Empty values are treated as unset.
 * @param name  variable name
 * @return pointer to value, or NULL if unset/empty
 */
static const char * _cd_get_env_dup(const char * name)
{
    if (!name) {
        return NULL;
    }
#ifdef CD_PLATFORM_WINDOWS
    static char sbuf[CD_MAX_PATH_LEN];
    DWORD n = GetEnvironmentVariableA(name, sbuf, (DWORD)sizeof(sbuf));
    if (n == 0) {
        return NULL;
    }
    if (n >= (DWORD)sizeof(sbuf)) {
        return NULL;
    }
    return sbuf;
#else
    const char * v = getenv(name);
    if (!v || v[0] == '\0') {
        return NULL;
    }
    return v;
#endif
}

/**
 * @brief Set an environment variable in the current process.
 * @param name   variable name
 * @param value  value string
 * @return 0 on success, -1 on error
 */
static int _cd_set_env(const char * name, const char * value)
{
    if (!name || !value) {
        return -1;
    }
#ifdef CD_PLATFORM_WINDOWS
    return SetEnvironmentVariableA(name, value) ? 0 : -1;
#else
    return setenv(name, value, 1);
#endif
}

#ifdef CD_PLATFORM_WINDOWS
/**
 * @brief Convert a wide-character string to UTF-8.
 * @param wide      input wide string
 * @param out       output buffer
 * @param out_size  size of output buffer in bytes
 * @return number of bytes written (excluding NUL), or -1 on error
 */
static int _cd_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size)
{
    if (!out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!wide) {
        return -1;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (needed <= 0 || needed > (int)out_size) {
        return -1;
    }
    int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
    if (written <= 0) {
        out[0] = '\0';
        return -1;
    }
    return written - 1;
}

/**
 * @brief Convert a UTF-8 string to wide characters.
 * @param utf8        input UTF-8 string
 * @param out         output wide buffer
 * @param out_wchars  size of output buffer in wchar_t units
 * @return number of wide chars written (excluding NUL), or -1 on error
 */
static int _cd_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars)
{
    if (!out || out_wchars == 0) {
        return -1;
    }
    out[0] = L'\0';
    if (!utf8) {
        return -1;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0 || needed > (int)out_wchars) {
        return -1;
    }
    int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_wchars);
    if (written <= 0) {
        out[0] = L'\0';
        return -1;
    }
    return written - 1;
}
#endif /* CD_PLATFORM_WINDOWS */

/**
 * @brief Check if a path is an existing directory.
 * @param path  path to check
 * @return 1 if is directory, 0 if not
 */
static int _cd_path_is_dir(const char * path)
{
    if (!path || path[0] == '\0') {
        return 0;
    }
#ifdef CD_PLATFORM_WINDOWS
    wchar_t w[CD_MAX_PATH_LEN];
    if (_cd_utf8_to_wide(path, w, sizeof(w) / sizeof(w[0])) < 0) {
        return 0;
    }
    DWORD attr = GetFileAttributesW(w);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Check if a path is absolute (POSIX '/', Windows drive, or UNC).
 * @param p  path to check
 * @return 1 if absolute, 0 if not
 */
static int _cd_is_absolute_path(const char * p)
{
    if (!p || p[0] == '\0') {
        return 0;
    }
    if (p[0] == CD_PATH_SEP_CHAR) {
        return 1;
    }
#ifdef CD_PLATFORM_WINDOWS
    if (p[0] == '\\') {
        return 1;
    }
    if (p[1] == ':' && (p[2] == CD_PATH_SEP_CHAR || p[2] == '\\' || p[2] == '\0')) {
        return 1;
    }
    if (p[0] == '/' && p[1] == '/') {
        return 1; /* UNC */
    }
#endif
    return 0;
}

#ifdef CD_PLATFORM_WINDOWS
/**
 * @brief Windows: physical (resolved) current working directory as UTF-8 POSIX path.
 */
static int _cd_get_physical_pwd(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';
    wchar_t wbuf[CD_MAX_PATH_LEN];
    DWORD n = GetCurrentDirectoryW((DWORD)(sizeof(wbuf) / sizeof(wbuf[0])), wbuf);
    if (n == 0) {
        return -1;
    }
    if (n >= (DWORD)(sizeof(wbuf) / sizeof(wbuf[0]))) {
        return -1;
    }
    char utf8[CD_MAX_PATH_LEN];
    if (_cd_wide_to_utf8(wbuf, utf8, sizeof(utf8)) < 0) {
        return -1;
    }
    _cd_to_posix_slashes(utf8);
    _cd_strip_trailing_slashes(utf8);
    size_t ul = strlen(utf8);
    if (ul == 2 && utf8[1] == ':') {
        if (ul + 2 > sizeof(utf8)) {
            return -1;
        }
        utf8[ul++] = CD_PATH_SEP_CHAR;
        utf8[ul] = '\0';
    }
    return _cd_safe_copy(buf, utf8, size);
}

/**
 * @brief Windows: do the actual chdir via wide-char API.
 */
static int _cd_do_chdir(const char * utf8_target)
{
    if (!utf8_target || utf8_target[0] == '\0') {
        return -1;
    }
    wchar_t w[CD_MAX_PATH_LEN];
    if (_cd_utf8_to_wide(utf8_target, w, sizeof(w) / sizeof(w[0])) < 0) {
        return -1;
    }
    return SetCurrentDirectoryW(w) ? 0 : -1;
}

/**
 * @brief Windows: get the resolved (real, final) path of a directory.
 *        Uses GetFullPathNameW plus GetFinalPathNameByHandleW when available.
 */
static int _cd_resolve_physical_path(const char * dir, char * buf, size_t size)
{
    if (!dir || !buf || size == 0) {
        return -1;
    }
    wchar_t wdir[CD_MAX_PATH_LEN];
    if (_cd_utf8_to_wide(dir, wdir, sizeof(wdir) / sizeof(wdir[0])) < 0) {
        return -1;
    }
    wchar_t wfull[CD_MAX_PATH_LEN];
    DWORD n = GetFullPathNameW(wdir, (DWORD)(sizeof(wfull) / sizeof(wfull[0])), wfull, NULL);
    if (n == 0 || n >= (DWORD)(sizeof(wfull) / sizeof(wfull[0]))) {
        return -1;
    }

    /* Try harder: open handle to directory via CreateFileW then call
     * GetFinalPathNameByHandleW when available (Vista+). */
    wchar_t wfinal[CD_MAX_PATH_LEN];
    int ok = 0;
    typedef DWORD (WINAPI * fpnh_fn)(HANDLE, LPWSTR, DWORD, DWORD);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    fpnh_fn pGetFinalPathNameByHandleW = NULL;
    if (k32) {
        pGetFinalPathNameByHandleW = (fpnh_fn)GetProcAddress(k32, "GetFinalPathNameByHandleW");
    }
    if (pGetFinalPathNameByHandleW) {
        HANDLE h = CreateFileW(wfull,
                               FILE_LIST_DIRECTORY,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD rr = pGetFinalPathNameByHandleW(h, wfinal,
                       (DWORD)(sizeof(wfinal) / sizeof(wfinal[0])), VOLUME_NAME_DOS);
            CloseHandle(h);
            if (rr != 0 && rr < (DWORD)(sizeof(wfinal) / sizeof(wfinal[0]))) {
                /* Drop leading \\?\ if present */
                wchar_t * use = wfinal;
                if (wcsncmp(use, L"\\\\?\\", 4) == 0) {
                    use += 4;
                }
                char utf8[CD_MAX_PATH_LEN];
                if (_cd_wide_to_utf8(use, utf8, sizeof(utf8)) >= 0) {
                    _cd_to_posix_slashes(utf8);
                    _cd_strip_trailing_slashes(utf8);
                    size_t ul = strlen(utf8);
                    if (ul == 2 && utf8[1] == ':') {
                        if (ul + 2 > sizeof(utf8)) {
                            return -1;
                        }
                        utf8[ul++] = CD_PATH_SEP_CHAR;
                        utf8[ul] = '\0';
                    }
                    if (_cd_safe_copy(buf, utf8, size) == 0) {
                        ok = 1;
                    }
                }
            }
        }
    }
    if (!ok) {
        char utf8[CD_MAX_PATH_LEN];
        if (_cd_wide_to_utf8(wfull, utf8, sizeof(utf8)) < 0) {
            return -1;
        }
        _cd_to_posix_slashes(utf8);
        _cd_strip_trailing_slashes(utf8);
        size_t ul = strlen(utf8);
        if (ul == 2 && utf8[1] == ':') {
            if (ul + 2 > sizeof(utf8)) {
                return -1;
            }
            utf8[ul++] = CD_PATH_SEP_CHAR;
            utf8[ul] = '\0';
        }
        if (_cd_safe_copy(buf, utf8, size) != 0) {
            return -1;
        }
    }
    return 0;
}

#else  /* CD_PLATFORM_POSIX */

/**
 * @brief POSIX: physical (resolved) current working directory.
 *        Falls back to a growing buffer if pathconf reports a large value.
 */
static int _cd_get_physical_pwd(char * buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';
    long path_max = -1;
#ifdef _PC_PATH_MAX
    path_max = pathconf("/", _PC_PATH_MAX);
#endif
    size_t bufsz = (path_max > 1024) ? (size_t)path_max : (size_t)CD_MAX_PATH_LEN;
    if (bufsz < size) {
        bufsz = size;
    }
    char * tmp = (char *)malloc(bufsz);
    if (!tmp) {
        return -1;
    }
    tmp[0] = '\0';
    char * r = getcwd(tmp, bufsz);
    if (!r) {
        /* Retry with larger buffer */
        for (size_t sz = bufsz * 2; sz <= (1u << 24); sz *= 2) {
            char * p = (char *)realloc(tmp, sz);
            if (!p) {
                break;
            }
            tmp = p;
            r = getcwd(tmp, sz);
            if (r) {
                break;
            }
            if (errno != ERANGE) {
                break;
            }
        }
    }
    int rc = -1;
    if (r) {
        _cd_to_posix_slashes(tmp);
        _cd_strip_trailing_slashes(tmp);
        rc = _cd_safe_copy(buf, tmp, size);
    }
    free(tmp);
    return rc;
}

/**
 * @brief POSIX: do the actual chdir.
 */
static int _cd_do_chdir(const char * path)
{
    if (!path || path[0] == '\0') {
        return -1;
    }
    return chdir(path);
}

/**
 * @brief POSIX: get the resolved (real, final) path of a directory via realpath().
 */
static int _cd_resolve_physical_path(const char * dir, char * buf, size_t size)
{
    if (!dir || !buf || size == 0) {
        return -1;
    }
    buf[0] = '\0';
    char * r = realpath(dir, NULL);
    if (!r) {
        return -1;
    }
    _cd_to_posix_slashes(r);
    _cd_strip_trailing_slashes(r);
    int rc = _cd_safe_copy(buf, r, size);
    free(r);
    return rc;
}
#endif /* CD_PLATFORM_POSIX */

/**
 * @brief Expand a leading "~" or "~/..." to $HOME.
 *        "~user" forms are left unexpanded (no passwd lookup).
 * @param input     input path
 * @param out       output buffer
 * @param out_size  size of output buffer
 * @return 0 on success, -1 on error
 */
static int _cd_expand_home(const char * input, char * out, size_t out_size)
{
    if (!input || !out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';
    if (input[0] != '~') {
        return _cd_safe_copy(out, input, out_size);
    }
    const char * home = _cd_get_env_dup("HOME");
#ifdef CD_PLATFORM_WINDOWS
    if (!home) {
        home = _cd_get_env_dup("USERPROFILE");
    }
    if (!home) {
        /* fallback: %HOMEDRIVE%%HOMEPATH% */
        const char * hd = _cd_get_env_dup("HOMEDRIVE");
        const char * hp = _cd_get_env_dup("HOMEPATH");
        if (hd && hp) {
            char combo[CD_MAX_PATH_LEN];
            size_t hl = strlen(hd);
            size_t pl = strlen(hp);
            if (hl + pl + 1 > sizeof(combo)) {
                return -1;
            }
            memcpy(combo, hd, hl);
            memcpy(combo + hl, hp, pl);
            combo[hl + pl] = '\0';
            home = combo;
            /* fallthrough */
        }
    }
#endif
    if (!home) {
        return -1;
    }
    char home_norm[CD_MAX_PATH_LEN];
    if (_cd_safe_copy(home_norm, home, sizeof(home_norm)) != 0) {
        return -1;
    }
    _cd_to_posix_slashes(home_norm);
    _cd_strip_trailing_slashes(home_norm);
#ifdef CD_PLATFORM_WINDOWS
    size_t ul = strlen(home_norm);
    if (ul == 2 && home_norm[1] == ':') {
        if (ul + 2 > sizeof(home_norm)) {
            return -1;
        }
        home_norm[ul++] = CD_PATH_SEP_CHAR;
        home_norm[ul] = '\0';
    }
#endif
    const char * rest = input + 1; /* char after '~' */
    if (rest[0] == '\0' || rest[0] == CD_PATH_SEP_CHAR
#ifdef CD_PLATFORM_WINDOWS
        || rest[0] == '\\'
#endif
        )
    {
        if (rest[0] == '\0') {
            return _cd_safe_copy(out, home_norm, out_size);
        }
        size_t hn = strlen(home_norm);
        size_t rn = strlen(rest);
        if (hn + rn + 1 > out_size) {
            return -1;
        }
        char joined[CD_MAX_PATH_LEN];
        size_t used = 0;
        memcpy(joined, home_norm, hn);
        used = hn;
        /* Normalize separator */
        if (used > 0 && joined[used - 1] != CD_PATH_SEP_CHAR) {
            joined[used++] = CD_PATH_SEP_CHAR;
        }
        const char * rest_start = rest;
        while (*rest_start == CD_PATH_SEP_CHAR
#ifdef CD_PLATFORM_WINDOWS
               || *rest_start == '\\'
#endif
              ) {
            rest_start++;
        }
        size_t rl = strlen(rest_start);
        if (used + rl + 1 > sizeof(joined)) {
            return -1;
        }
        memcpy(joined + used, rest_start, rl);
        used += rl;
        joined[used] = '\0';
        return _cd_safe_copy(out, joined, out_size);
    }
    /* ~user form (not a prefix of HOME); leave unexpanded */
    return _cd_safe_copy(out, input, out_size);
}

/**
 * @brief Search $CDPATH for the first directory containing the target.
 *        Separator is ':' on POSIX, ';' on Windows.
 * @param target   relative target name (no slashes)
 * @param out       output buffer
 * @param out_size  size of output buffer
 * @return 0 on success, -1 if not found
 */
static int _cd_cdpath_search(const char * target, char * out, size_t out_size)
{
    if (!target || _cd_is_absolute_path(target) ||
        target[0] == '.' || strchr(target, CD_PATH_SEP_CHAR)) {
        return -1;
    }
#ifdef CD_PLATFORM_WINDOWS
    if (strchr(target, '\\')) {
        return -1;
    }
#endif
    const char * cdpath = _cd_get_env_dup("CDPATH");
    if (!cdpath) {
        return -1;
    }
    char cp[CD_MAX_PATH_LEN * 2];
    if (_cd_safe_copy(cp, cdpath, sizeof(cp)) != 0) {
        return -1;
    }
    char * save = NULL;
    const char * sep = ":";
#ifdef CD_PLATFORM_WINDOWS
    sep = ";";
#endif
    char * tok = strtok_r(cp, sep, &save);
    while (tok) {
        if (tok[0] == '\0') {
            tok = (char *)".";
        }
        char candidate[CD_MAX_PATH_LEN];
        size_t tl = strlen(tok);
        size_t rl = strlen(target);
        int need_sep = 1;
        if (tl == 0) {
            need_sep = 0;
        }
        else if (tok[tl - 1] == CD_PATH_SEP_CHAR) {
            need_sep = 0;
        }
#ifdef CD_PLATFORM_WINDOWS
        else if (tok[tl - 1] == '\\') {
            need_sep = 0;
        }
#endif
        if (tl + (need_sep ? 1 : 0) + rl + 1 > sizeof(candidate)) {
            tok = strtok_r(NULL, sep, &save);
            continue;
        }
        size_t u = 0;
        memcpy(candidate, tok, tl);
        u = tl;
        if (need_sep) {
            candidate[u++] = CD_PATH_SEP_CHAR;
        }
        memcpy(candidate + u, target, rl);
        u += rl;
        candidate[u] = '\0';
        if (_cd_path_is_dir(candidate)) {
            _cd_to_posix_slashes(candidate);
            return _cd_safe_copy(out, candidate, out_size);
        }
        tok = strtok_r(NULL, sep, &save);
    }
    return -1;
}

/**
 * @brief Print usage/help information
 */
static void _cd_print_help(void)
{
    cd_printf(
        "Usage: cd [-L|-P] [-e] [DIRECTORY]\n"
        "       cd [--help] [--version]\n"
        "Change the shell working directory.\n"
        "\n"
        "  -L, --logical    Use PWD from environment when traversing \"..\" (default).\n"
        "  -P, --physical   Use the physical directory structure (resolve symlinks).\n"
        "  -e, --fail-exit  Return non-zero if final directory cannot be determined\n"
        "                   after a successful chdir (with -P).\n"
        "                   (GNU cd / POSIX extension.)\n"
        "      --help       display this help and exit\n"
        "      --version    output version information and exit\n"
        "\n"
        "Special DIRECTORY values:\n"
        "  (none)           go to $HOME\n"
        "  -                go to $OLDPWD and print the resulting directory\n"
        "  ~                go to $HOME\n"
        "  ~/foo            go to $HOME/foo\n"
        "\n"
        "If DIRECTORY is neither absolute nor starts with \".\", \"..\" or \"/\",\n"
        "CDPATH (colon-separated on POSIX, semicolon-separated on Windows) is\n"
        "searched; the first match becomes the destination.\n"
        "\n"
        "Exit status:\n"
        "  0   directory changed successfully.\n"
        "  1   invalid options, directory not found, or not a directory.\n"
        "  2   unknown option.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _cd_print_version(void)
{
    cd_printf("cd %s\n", CD_VERSION_STR);
    cd_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    cd_printf("%s", "License MIT: <https://mit-license.org/>\n");
    cd_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    cd_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Print a usage error message to the error stream.
 * @param msg  message text (may be NULL)
 */
static void _cd_usage_err(const char * msg)
{
    cd_err_printf("cd: %s\n", msg ? msg : "");
    cd_err_printf("%s", "Try 'cd --help' for more information.\n");
}

/**
 * @brief Parse a single long option.
 * @param a     option string (e.g. "--logical")
 * @param opts  options structure to update
 * @return 0 on continue, 1 on error, -1 on --help/--version (stop)
 */
static int _cd_parse_long_option(const char * a, cd_opts_t * opts)
{
    if (strcmp(a, "--help") == 0) {
        opts->help = true;
        return -1;
    }
    if (strcmp(a, "--version") == 0) {
        opts->version = true;
        return -1;
    }
    if (strcmp(a, "--logical") == 0) {
        opts->logical = true;
        opts->physical = false;
        return 0;
    }
    if (strcmp(a, "--physical") == 0) {
        opts->physical = true;
        opts->logical = false;
        return 0;
    }
    if (strcmp(a, "--fail-exit") == 0 || strcmp(a, "--e") == 0) {
        opts->fail_exit = true;
        return 0;
    }
    /* Unknown */
    char buf[256];
    snprintf(buf, sizeof(buf), "unrecognized option '%s'", a);
    _cd_usage_err(buf);
    return 1;
}

/**
 * @brief Parse a cluster of short options (e.g. "LP").
 * @param s             option chars (past the leading '-')
 * @param opts          options structure to update
 * @param consumed_arg  set to 1 if an extra argument was consumed
 * @return 0 on success, 1 on error
 */
static int _cd_parse_short_options(const char * s, cd_opts_t * opts, int * consumed_arg)
{
    *consumed_arg = 0;
    for (const char * p = s; *p; p++) {
        switch (*p) {
            case 'L':
                opts->logical = true;
                opts->physical = false;
                break;
            case 'P':
                opts->physical = true;
                opts->logical = false;
                break;
            case 'e':
                opts->fail_exit = true;
                break;
            default: {
                char buf[128];
                snprintf(buf, sizeof(buf), "invalid option -- '%c'", *p);
                _cd_usage_err(buf);
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Parse command-line arguments.
 * @param argc   argument count
 * @param argv   argument vector
 * @param opts   options structure to fill
 * @return 0 on success, 1 on usage error, 2 on unknown option
 */
static int _cd_parse_args(int argc, char ** argv, cd_opts_t * opts)
{
    if (!argv || !opts) {
        return 2;
    }
    memset(opts, 0, sizeof(*opts));
    opts->logical = true; /* GNU cd defaults to -L */
    int have_operand = 0;
    for (int i = 1; i < argc; i++) {
        const char * a = argv[i];
        if (!a) {
            continue;
        }
        if (!have_operand) {
            if (a[0] == '-' && a[1] != '\0') {
                if (strcmp(a, "--") == 0) {
                    have_operand = 1;
                    continue;
                }
                if (a[1] == '-') {
                    int rc = _cd_parse_long_option(a, opts);
                    if (rc == 1) {
                        return 2;
                    }
                    if (rc == -1) {
                        return 0;
                    }
                    continue;
                }
                if (strcmp(a, "-") == 0) {
                    /* "cd -" operand, not option */
                    opts->dir = "-";
                    opts->print_old = true;
                    have_operand = 1;
                    continue;
                }
                int consumed = 0;
                int rc = _cd_parse_short_options(a + 1, opts, &consumed);
                if (rc != 0) {
                    return 2;
                }
                if (consumed) {
                    i++;
                }
                continue;
            }
        }
        if (have_operand) {
            _cd_usage_err("too many arguments");
            return 1;
        }
        opts->dir = a;
        have_operand = 1;
    }
    return 0;
}

/**
 * @brief Update PWD and OLDPWD environment variables.
 * @param new_pwd  new working directory path
 * @return 0 on success, -1 on error
 */
static int _cd_set_pwd_oldpwd(const char * new_pwd)
{
    const char * cur_pwd = _cd_get_env_dup("PWD");
    if (!cur_pwd) {
        char physical[CD_MAX_PATH_LEN];
        if (_cd_get_physical_pwd(physical, sizeof(physical)) == 0) {
            cur_pwd = physical;
        }
    }
    int rc1 = 0;
    if (cur_pwd) {
        rc1 = _cd_set_env("OLDPWD", cur_pwd);
    }
    int rc2 = _cd_set_env("PWD", new_pwd);
    return (rc1 == 0 && rc2 == 0) ? 0 : -1;
}
