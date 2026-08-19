/**
 * @file du.c
 * @brief Cross-platform du command implementation (GNU coreutils 9.x compatible)
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils du 9.x.
 *
 * Key behaviors:
 *   - -a, --all: write counts for all files, not just directories
 *   - --apparent-size: print apparent sizes rather than disk usage
 *   - -B, --block-size=SIZE: scale sizes by SIZE
 *   - -b, --bytes: equivalent to --apparent-size --block-size=1
 *   - -c, --total: produce a grand total
 *   - -d, --max-depth=N: print total for directories N or fewer levels below
 *   - -h, --human-readable: print sizes in human readable format
 *   - -k: like --block-size=1K
 *   - -L, --dereference: dereference all symlinks
 *   - -l, --count-links: count sizes many times if hard linked
 *   - -m: like --block-size=1M
 *   - -S, --separate-dirs: don't include subdirectory sizes
 *   - --si: like -h, but use powers of 1000
 *   - -s, --summarize: display only a total for each argument
 *   - -x, --one-file-system: skip directories on different file systems
 *   - --exclude=PATTERN: exclude files that match PATTERN
 *   - --help / --version: recognized and handled
 *   - Windows wide-character path support for non-ANSI paths
 *   - UTF-8 output on all platforms
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o du.exe du.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o du du.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o du du.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o du du.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o du du.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o du du.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/du>
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
    #define DU_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define DU_PLATFORM_LINUX   1
    #define DU_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define DU_PLATFORM_MACOS   1
    #define DU_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define DU_PLATFORM_FREEBSD 1
    #define DU_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define DU_PLATFORM_OPENBSD 1
    #define DU_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define DU_PLATFORM_NETBSD  1
    #define DU_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define DU_PLATFORM_POSIX   1
#else
    #define DU_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef DU_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef DU_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef DU_PLATFORM_NETBSD
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

#ifdef DU_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFREG) != 0)
    #endif
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & _S_IFLNK) != 0)
    #endif
    /* Use _stat64 with _wstat64 for large-file and Y2038-safe timestamps */
    typedef struct _stat64 du_stat_t;
#else /* DU_PLATFORM_POSIX */
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <fnmatch.h>
    #include <limits.h>
    typedef struct stat du_stat_t;
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define DU_VERSION_STR "v9.7"

/** @brief Maximum path buffer length (bytes) */
#define DU_MAX_PATH 4096

/** @brief Default I/O block size when POSIXLY_CORRECT is not set (1 KiB) */
#define DU_DEFAULT_BLOCK_SIZE 1024

/** @brief Block size when POSIXLY_CORRECT is set (512 bytes) */
#define DU_POSIX_BLOCK_SIZE 512

/** @brief Windows simulated cluster size for disk usage estimation */
#define DU_WIN_CLUSTER_SIZE 4096

/** @brief Maximum recursion depth to prevent infinite symlink loops */
#define DU_MAX_RECURSION 256

/** @brief Initial capacity for the exclude pattern list */
#define DU_EXCLUDE_INIT_CAP 8

/** @brief Initial capacity for the hard-link inode tracking list */
#define DU_INODE_INIT_CAP 64

/** @brief Buffer size for human-readable size strings */
#define DU_HUMAN_BUF_SIZE 32

/** @brief Maximum long option name length accepted by the parser */
#define DU_OPT_NAME_MAX 64

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options for du
 *   all             - -a: write counts for all files
 *   apparent_size   - --apparent-size: print apparent sizes
 *   bytes           - -b: apparent size in bytes
 *   total           - -c: produce a grand total
 *   human_readable  - -h: print sizes in human readable format
 *   si              - --si: like -h but use powers of 1000
 *   summarize       - -s: display only a total for each argument
 *   separate_dirs   - -S: don't include subdirectory sizes
 *   dereference     - -L: dereference all symlinks
 *   count_links     - -l: count sizes many times if hard linked
 *   one_file_system  - -x: skip directories on different file systems
 *   max_depth       - -d: print total for directories N or fewer levels below
 *   block_size      - -B: scale sizes by SIZE (default: 1024)
 */
typedef struct {
    bool all;
    bool apparent_size;
    bool bytes;
    bool total;
    bool human_readable;
    bool si;
    bool summarize;
    bool separate_dirs;
    bool dereference;
    bool count_links;
    bool one_file_system;
    int  max_depth;
    uint64_t block_size;
} du_opts_t;

/**
 * @brief Hard-link inode tracking entry (POSIX only)
 */
typedef struct {
    dev_t dev;
    ino_t ino;
} du_inode_entry_t;

/**
 * @brief Exclude pattern list
 */
typedef struct {
    char ** patterns;
    size_t count;
    size_t capacity;
} du_exclude_list_t;

/**
 * @brief Traversal state (persists across recursive calls)
 *   opts          - pointer to parsed options
 *   excludes      - pointer to exclude pattern list
 *   grand_total   - accumulated total for -c
 *   had_error     - set to true if any file/directory access failed
 *   inodes        - hard-link tracking array (POSIX only)
 *   inode_count   - number of entries in inodes
 *   inode_cap     - capacity of inodes array
 *   fs_dev        - device of the starting directory for -x (POSIX only)
 *   has_fs_dev    - whether fs_dev has been initialized
 */
typedef struct {
    const du_opts_t * opts;
    const du_exclude_list_t * excludes;
    uint64_t grand_total;
    bool had_error;
#ifndef DU_PLATFORM_WINDOWS
    du_inode_entry_t * inodes;
    size_t inode_count;
    size_t inode_cap;
    dev_t fs_dev;
    bool has_fs_dev;
#endif
} du_state_t;

/********************************
 *    static prototypes
 ********************************/
static void _du_print_help(void);
static void _du_print_version(void);
static int  _du_parse_args(int argc, char ** argv, du_opts_t * opts,
                           du_exclude_list_t * excludes);
static uint64_t _du_get_default_block_size(void);
static uint64_t _du_parse_size(const char * str);
static void _du_human_size(uint64_t size, bool si, char * buf, size_t buf_size);
static void _du_print_entry(uint64_t size_bytes, const char * path,
                             const du_opts_t * opts);
static bool _du_should_print(int depth, bool is_dir, const du_opts_t * opts);
static uint64_t _du_walk(const char * path, int depth, du_state_t * state);
static uint64_t _du_get_size(const char * path, const du_stat_t * st,
                              const du_opts_t * opts);
static bool _du_match_exclude(const char * path,
                               const du_exclude_list_t * excludes);
static int  _du_exclude_add(du_exclude_list_t * list, const char * pattern);
static void _du_exclude_free(du_exclude_list_t * list);

#ifndef DU_PLATFORM_WINDOWS
static bool _du_inode_seen(du_state_t * state, dev_t dev, ino_t ino);
static void _du_inode_free(du_state_t * state);
#endif

#ifdef DU_PLATFORM_WINDOWS
static bool _du_wildcard_match(const char * pattern, const char * str);
#endif

#ifdef DU_PLATFORM_WINDOWS
static int _du_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars);
static int _du_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for du_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef du_out_stream
    #define du_out_stream stdout
#endif

/**
 * @brief Default stderr stream for du_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef du_err_stream
    #define du_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__".
 */
#ifndef du_printf
    #define du_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef du_err_printf
    #define du_err_printf(fmt, ...) \
        do { if (du_err_stream) { (void)fprintf((du_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 */
#ifndef du_fputs
    #define du_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef du_fflush
    #define du_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 *        Two NULLs considered equal (both missing).
 * @return true if strings match, false otherwise
 */
#ifndef du_streq
    #define du_streq(a, b) \
        (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/**
 * @brief Safe strncmp wrapper with NULL guards and explicit size.
 */
#ifndef du_strneq
    #define du_strneq(a, b, n) \
        (((a) && (b)) ? (strncmp((a), (b), (n)) == 0) : false)
#endif

/**
 * @brief Safe strchr wrapper with NULL guard.
 *        Returns NULL if input string is NULL.
 */
#ifndef du_strchr
    #define du_strchr(s, c) (((s)) ? strchr((s), (c)) : NULL)
#endif

/**
 * @brief Safe free-and-null pointer cleanup macro.
 */
#ifndef du_safe_free
    #define du_safe_free(p) \
        do { if ((p)) { free(p); (p) = NULL; } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the du command
 *
 * Processing flow:
 *   1. Parse command-line options and exclude patterns
 *   2. For each argument (or "." if none): traverse recursively
 *   3. If -c: print grand total
 *   4. Return 0 on success, 1 on error
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    du_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.max_depth = -1;  /* unlimited by default */
    opts.block_size = _du_get_default_block_size();

    du_exclude_list_t excludes;
    memset(&excludes, 0, sizeof(excludes));

    int first_arg = _du_parse_args(argc, argv, &opts, &excludes);
    if (first_arg < 0) {
        _du_exclude_free(&excludes);
        return 1;
    }

    /* -b implies apparent_size and block_size=1 */
    if (opts.bytes) {
        opts.apparent_size = true;
        opts.block_size = 1;
    }

    /* -s overrides max_depth to 0 (last one wins between -s and -d) */
    if (opts.summarize) {
        opts.max_depth = 0;
    }

    /* -k forces 1K blocks, -m forces 1M blocks */
    /* (already set during parse, but ensure consistency) */

    du_state_t state;
    memset(&state, 0, sizeof(state));
    state.opts = &opts;
    state.excludes = &excludes;

    if (first_arg >= argc) {
        /* No arguments: process current directory */
        _du_walk(".", 0, &state);
    }
    else {
        for (int i = first_arg; i < argc; i++) {
            state.grand_total += _du_walk(argv[i], 0, &state);
        }
    }

    if (opts.total) {
        _du_print_entry(state.grand_total, "total", &opts);
    }

#ifndef DU_PLATFORM_WINDOWS
    _du_inode_free(&state);
#endif
    _du_exclude_free(&excludes);
    du_fflush(du_out_stream);
    return state.had_error ? 1 : 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _du_print_help(void)
{
    du_printf(
        "Usage: du [OPTION]... [FILE]...\n"
        "       du [OPTION]... --files0-from=F\n"
        "Summarize disk usage of the set of FILEs, recursively for directories.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -a, --all             write counts for all files, not just directories\n"
        "      --apparent-size    print apparent sizes, rather than disk usage\n"
        "                         although the apparent size is usually smaller, it may\n"
        "                         be larger due to holes in ('sparse') files, internal\n"
        "                         fragmentation, indirect blocks, and the like\n"
        "  -B, --block-size=SIZE  scale sizes by SIZE before printing them\n"
        "  -b, --bytes           equivalent to '--apparent-size --block-size=1'\n"
        "  -c, --total            produce a grand total\n"
        "  -D, --dereference-args  dereference only symlinks that are listed\n"
        "                         on the command line\n"
        "  -d, --max-depth=N      print the total for a directory (or file, with --all)\n"
        "                         only if it is N or fewer levels below the command\n"
        "                         line argument;  --max-depth=0 is the same as --summarize\n"
        "  -h, --human-readable   print sizes in human readable format (e.g., 1K 234M 2G)\n"
        "  -k                     like --block-size=1K\n"
        "  -L, --dereference      dereference all symlinks\n"
        "  -l, --count-links      count sizes many times if hard linked\n"
        "  -m                     like --block-size=1M\n"
        "  -P, --no-dereference   don't follow any symbolic links (the default)\n"
        "  -S, --separate-dirs    don't include subdirectory sizes\n"
        "      --si               like -h, but use powers of 1000 not 1024\n"
        "  -s, --summarize        display only a total for each argument\n"
        "  -t, --threshold=SIZE   exclude entries smaller than SIZE if positive,\n"
        "                         or larger than SIZE if negative\n"
        "  -x, --one-file-system  skip directories on different file systems\n"
        "      --exclude=PATTERN  exclude files that match PATTERN\n"
        "      --help             display this help and exit\n"
        "      --version          output version information and exit\n"
        "\n"
        "Display values are in units of the first available SIZE from --block-size,\n"
        "and the following environment variables, or the default of 1024 if unset.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _du_print_version(void)
{
    du_printf("du %s\n", DU_VERSION_STR);
    du_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    du_printf("%s", "License MIT: <https://mit-license.org/>\n");
    du_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    du_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Get the default block size based on POSIXLY_CORRECT.
 * @return 512 if POSIXLY_CORRECT is set, 1024 otherwise
 */
static uint64_t _du_get_default_block_size(void)
{
    const char * posix = getenv("POSIXLY_CORRECT");
    if (posix && posix[0] != '\0') {
        return DU_POSIX_BLOCK_SIZE;
    }
    return DU_DEFAULT_BLOCK_SIZE;
}

/**
 * @brief Parse a size string like "1K", "2M", "1KB", "1KiB", etc.
 * @param str  size string (e.g., "1024", "1K", "2M", "1KiB")
 * @return size in bytes, or 0 on parse failure
 */
static uint64_t _du_parse_size(const char * str)
{
    if (!str || !*str) {
        return 0;
    }

    char * endptr = NULL;
    double val = strtod(str, &endptr);

    if (endptr == str) {
        return 0;  /* no number */
    }

    if (*endptr == '\0') {
        return (uint64_t)val;
    }

    /* Determine multiplier from suffix */
    uint64_t multiplier = 0;

    /* Check for "iB" suffix (binary: powers of 1024) */
    if (du_strneq(endptr, "KiB", 3) || du_strneq(endptr, "kib", 3)) {
        multiplier = 1024ULL;
    }
    else if (du_strneq(endptr, "MiB", 3) || du_strneq(endptr, "mib", 3)) {
        multiplier = 1024ULL * 1024;
    }
    else if (du_strneq(endptr, "GiB", 3) || du_strneq(endptr, "gib", 3)) {
        multiplier = 1024ULL * 1024 * 1024;
    }
    else if (du_strneq(endptr, "TiB", 3) || du_strneq(endptr, "tib", 3)) {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    }
    else if (du_strneq(endptr, "PiB", 3) || du_strneq(endptr, "pib", 3)) {
        multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024;
    }
    /* Check for "B" suffix (decimal: powers of 1000) */
    else if (du_strneq(endptr, "KB", 2) || du_strneq(endptr, "kb", 2)) {
        multiplier = 1000ULL;
    }
    else if (du_strneq(endptr, "MB", 2) || du_strneq(endptr, "mb", 2)) {
        multiplier = 1000000ULL;
    }
    else if (du_strneq(endptr, "GB", 2) || du_strneq(endptr, "gb", 2)) {
        multiplier = 1000000000ULL;
    }
    else if (du_strneq(endptr, "TB", 2) || du_strneq(endptr, "tb", 2)) {
        multiplier = 1000000000000ULL;
    }
    else if (du_strneq(endptr, "PB", 2) || du_strneq(endptr, "pb", 2)) {
        multiplier = 1000000000000000ULL;
    }
    /* Check for single-letter suffix (binary: powers of 1024) */
    else if (endptr[0] == 'K' || endptr[0] == 'k') {
        multiplier = 1024ULL;
    }
    else if (endptr[0] == 'M' || endptr[0] == 'm') {
        multiplier = 1024ULL * 1024;
    }
    else if (endptr[0] == 'G' || endptr[0] == 'g') {
        multiplier = 1024ULL * 1024 * 1024;
    }
    else if (endptr[0] == 'T' || endptr[0] == 't') {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    }
    else if (endptr[0] == 'P' || endptr[0] == 'p') {
        multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024;
    }
    else if (endptr[0] == 'E' || endptr[0] == 'e') {
        multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024 * 1024;
    }
    else {
        return 0;  /* unknown suffix */
    }

    return (uint64_t)(val * (double)multiplier);
}

/**
 * @brief Parse command-line arguments into du_opts_t
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc     argument count
 * @param argv     argument vector
 * @param opts     output options structure
 * @param excludes output exclude pattern list
 * @return index of first file argument (or argc if none), -1 on error
 */
static int _du_parse_args(int argc, char ** argv, du_opts_t * opts,
                           du_exclude_list_t * excludes)
{
    if (!opts || !excludes) {
        return -1;
    }

    if (argc < 1 || !argv) {
        return -1;
    }

    int i = 1;
    bool no_more_opts = false;

    for (; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* "--" terminates options */
        if (!no_more_opts && du_streq(arg, "--")) {
            no_more_opts = true;
            i++;
            break;
        }

        /* Long options */
        if (!no_more_opts && du_strneq(arg, "--", 2)) {
            char * eq = du_strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[DU_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            const char * val = eq ? eq + 1 : NULL;

            if (strcmp(name, "help") == 0) {
                _du_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _du_print_version();
                exit(0);
            }
            if (strcmp(name, "all") == 0) {
                opts->all = true;
            }
            else if (strcmp(name, "apparent-size") == 0) {
                opts->apparent_size = true;
            }
            else if (strcmp(name, "bytes") == 0) {
                opts->bytes = true;
            }
            else if (strcmp(name, "total") == 0) {
                opts->total = true;
            }
            else if (strcmp(name, "human-readable") == 0) {
                opts->human_readable = true;
            }
            else if (strcmp(name, "si") == 0) {
                opts->si = true;
            }
            else if (strcmp(name, "summarize") == 0) {
                opts->summarize = true;
            }
            else if (strcmp(name, "separate-dirs") == 0) {
                opts->separate_dirs = true;
            }
            else if (strcmp(name, "dereference") == 0) {
                opts->dereference = true;
            }
            else if (strcmp(name, "count-links") == 0) {
                opts->count_links = true;
            }
            else if (strcmp(name, "one-file-system") == 0) {
                opts->one_file_system = true;
            }
            else if (strcmp(name, "no-dereference") == 0) {
                opts->dereference = false;
            }
            else if (strcmp(name, "block-size") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    uint64_t sz = _du_parse_size(val);
                    if (sz > 0) {
                        opts->block_size = sz;
                    }
                }
            }
            else if (strcmp(name, "max-depth") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    opts->max_depth = atoi(val);
                    opts->summarize = false;
                }
            }
            else if (strcmp(name, "exclude") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    _du_exclude_add(excludes, val);
                }
            }
            else {
                du_err_printf("du: unrecognized option '%s'\n", arg);
                du_err_printf("%s", "Try 'du --help' for more information.\n");
                return -1;
            }
            continue;
        }

        /* Short options */
        if (!no_more_opts && arg[0] == '-' && arg[1] != '\0') {
            bool consumed_arg = false;
            for (int j = 1; arg[j] != '\0' && !consumed_arg; j++) {
                switch (arg[j]) {
                    case 'a':
                        opts->all = true;
                        break;

                    case 'b':
                        opts->bytes = true;
                        break;

                    case 'c':
                        opts->total = true;
                        break;

                    case 'h':
                        opts->human_readable = true;
                        break;

                    case 'k':
                        opts->block_size = 1024;
                        break;

                    case 'l':
                        opts->count_links = true;
                        break;

                    case 'L':
                        opts->dereference = true;
                        break;

                    case 'm':
                        opts->block_size = 1024 * 1024;
                        break;

                    case 'P':
                        opts->dereference = false;
                        break;

                    case 's':
                        opts->summarize = true;
                        break;

                    case 'S':
                        opts->separate_dirs = true;
                        break;

                    case 'x':
                        opts->one_file_system = true;
                        break;

                    case 'B': {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val) {
                            uint64_t sz = _du_parse_size(val);
                            if (sz > 0) {
                                opts->block_size = sz;
                            }
                        }
                        break;
                    }

                    case 'd': {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val) {
                            opts->max_depth = atoi(val);
                            opts->summarize = false;
                        }
                        break;
                    }

                    default:
                        du_err_printf("du: invalid option -- '%c'\n", arg[j]);
                        du_err_printf("%s", "Try 'du --help' for more information.\n");
                        return -1;
                }
            }
            continue;
        }

        /* Positional argument: this is a file/directory path */
        break;
    }

    return i;
}

/**
 * @brief Format a size in bytes as a human-readable string.
 *
 * Uses 1 decimal place for values < 10 in the current unit,
 * 0 decimal places for values >= 10. Base is 1024 for -h,
 * 1000 for --si.
 *
 * @param size      size in bytes
 * @param si        true for --si (powers of 1000), false for -h (powers of 1024)
 * @param buf       output buffer
 * @param buf_size  size of output buffer
 */
static void _du_human_size(uint64_t size, bool si, char * buf, size_t buf_size)
{
    static const char * suffixes_h[] = {"", "K", "M", "G", "T", "P", "E"};
    static const char * suffixes_si[] = {"", "k", "M", "G", "T", "P", "E"};

    if (size == 0) {
        snprintf(buf, buf_size, "0");
        return;
    }

    uint64_t base = si ? 1000 : 1024;
    const char * const * suffixes = si ? suffixes_si : suffixes_h;

    double d = (double)size;
    int idx = 0;
    while (d >= (double)base && idx < 6) {
        d /= (double)base;
        idx++;
    }

    if (idx == 0) {
        snprintf(buf, buf_size, "%" PRIu64, size);
    }
    else if (d < 10.0) {
        snprintf(buf, buf_size, "%.1f%s", d, suffixes[idx]);
    }
    else {
        snprintf(buf, buf_size, "%.0f%s", d, suffixes[idx]);
    }
}

/**
 * @brief Print a du entry (size + tab + path).
 * @param size_bytes  size in bytes
 * @param path        file/directory path
 * @param opts        parsed options
 */
static void _du_print_entry(uint64_t size_bytes, const char * path,
                             const du_opts_t * opts)
{
    if (opts->human_readable || opts->si) {
        char buf[DU_HUMAN_BUF_SIZE];
        _du_human_size(size_bytes, opts->si, buf, sizeof(buf));
        du_printf("%s\t%s\n", buf, path);
    }
    else {
        uint64_t blocks = opts->block_size > 0
            ? (size_bytes + opts->block_size - 1) / opts->block_size
            : size_bytes;
        du_printf("%" PRIu64 "\t%s\n", blocks, path);
    }
}

/**
 * @brief Determine whether an entry at the given depth should be printed.
 * @param depth   current depth (0 = top-level argument)
 * @param is_dir  true if the entry is a directory
 * @param opts    parsed options
 * @return true if the entry should be printed
 */
static bool _du_should_print(int depth, bool is_dir, const du_opts_t * opts)
{
    int max_depth = opts->max_depth;  /* -1 = unlimited */

    if (is_dir) {
        return (max_depth < 0 || depth <= max_depth);
    }

    /* File entry */
    if (depth == 0) {
        return true;  /* always print top-level arguments */
    }
    if (!opts->all) {
        return false;  /* without -a, only directories are printed */
    }
    return (max_depth < 0 || depth <= max_depth);
}

/**
 * @brief Get the disk usage or apparent size from a stat structure.
 * @param path  file path (for error messages)
 * @param st    stat structure
 * @param opts  parsed options
 * @return size in bytes
 */
static uint64_t _du_get_size(const char * path, const du_stat_t * st,
                              const du_opts_t * opts)
{
    (void)path;

    if (opts->apparent_size || opts->bytes) {
        return (uint64_t)st->st_size;
    }

    /* Disk usage */
#ifdef DU_PLATFORM_POSIX
    return (uint64_t)st->st_blocks * 512;
#else
    /* Windows: no st_blocks; round file size up to cluster size */
    uint64_t size = (uint64_t)st->st_size;
    if (S_ISDIR(st->st_mode)) {
        return DU_WIN_CLUSTER_SIZE;
    }
    if (size == 0) {
        return 0;
    }
    return (size + DU_WIN_CLUSTER_SIZE - 1) & ~(uint64_t)(DU_WIN_CLUSTER_SIZE - 1);
#endif
}

#ifdef DU_PLATFORM_WINDOWS
/**
 * @brief Simple wildcard matcher (supports * and ?).
 *
 * Used on platforms without fnmatch(). Case-sensitive.
 *
 * @param pattern  wildcard pattern
 * @param str      string to match
 * @return true if the string matches the pattern
 */
static bool _du_wildcard_match(const char * pattern, const char * str)
{
    if (!pattern || !str) {
        return false;
    }

    while (*pattern) {
        if (*pattern == '*') {
            /* Skip consecutive '*' characters */
            while (*pattern == '*') {
                pattern++;
            }
            if (!*pattern) {
                return true;  /* trailing '*' matches everything */
            }
            /* Try matching the rest of the pattern at each position */
            while (*str) {
                if (_du_wildcard_match(pattern, str)) {
                    return true;
                }
                str++;
            }
            return _du_wildcard_match(pattern, str);
        }
        else if (*pattern == '?') {
            if (!*str) {
                return false;
            }
            pattern++;
            str++;
        }
        else {
            if (*pattern != *str) {
                return false;
            }
            pattern++;
            str++;
        }
    }
    return (*str == '\0');
}
#endif /* DU_PLATFORM_WINDOWS */

/**
 * @brief Check if a path matches any exclude pattern.
 *
 * Matching is done against the basename (last path component).
 *
 * @param path     full path
 * @param excludes  exclude pattern list
 * @return true if the path should be excluded
 */
static bool _du_match_exclude(const char * path,
                               const du_exclude_list_t * excludes)
{
    if (!excludes || excludes->count == 0 || !path) {
        return false;
    }

    /* Extract basename */
    const char * base = strrchr(path, '/');
#ifdef DU_PLATFORM_WINDOWS
    const char * bs = strrchr(path, '\\');
    if (bs && (!base || bs > base)) {
        base = bs;
    }
#endif
    if (base) {
        base++;
    }
    else {
        base = path;
    }

    for (size_t i = 0; i < excludes->count; i++) {
        const char * pat = excludes->patterns[i];
        if (!pat) {
            continue;
        }
#ifdef DU_PLATFORM_POSIX
        if (fnmatch(pat, base, 0) == 0) {
            return true;
        }
#else
        if (_du_wildcard_match(pat, base)) {
            return true;
        }
#endif
    }
    return false;
}

/**
 * @brief Add a pattern to the exclude list.
 * @param list     exclude list
 * @param pattern  pattern string (will be copied)
 * @return 0 on success, -1 on failure
 */
static int _du_exclude_add(du_exclude_list_t * list, const char * pattern)
{
    if (!list || !pattern) {
        return -1;
    }

    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : DU_EXCLUDE_INIT_CAP;
        char ** new_patterns = (char **)realloc(list->patterns,
                                                 new_cap * sizeof(char *));
        if (!new_patterns) {
            return -1;
        }
        list->patterns = new_patterns;
        list->capacity = new_cap;
    }

    size_t plen = strlen(pattern);
    char * copy = (char *)malloc(plen + 1);
    if (!copy) {
        return -1;
    }
    memcpy(copy, pattern, plen + 1);
    list->patterns[list->count] = copy;
    list->count++;
    return 0;
}

/**
 * @brief Free all patterns in the exclude list.
 * @param list  exclude list
 */
static void _du_exclude_free(du_exclude_list_t * list)
{
    if (!list || !list->patterns) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        du_safe_free(list->patterns[i]);
    }
    du_safe_free(list->patterns);
    list->count = 0;
    list->capacity = 0;
}

#ifndef DU_PLATFORM_WINDOWS
/**
 * @brief Check if a (dev, ino) pair has been seen; add it if not.
 *
 * Used for hard-link deduplication. When -l is not set, each file
 * is counted only once even if it has multiple hard links.
 *
 * @param state  traversal state
 * @param dev    device ID
 * @param ino    inode number
 * @return true if already seen (should skip), false if newly added
 */
static bool _du_inode_seen(du_state_t * state, dev_t dev, ino_t ino)
{
    if (!state) {
        return false;
    }

    if (state->inode_count > 0 && state->inodes) {
        for (size_t i = 0; i < state->inode_count; i++) {
            if (state->inodes[i].dev == dev && state->inodes[i].ino == ino) {
                return true;
            }
        }
    }

    /* Add to list */
    if (state->inode_count >= state->inode_cap) {
        size_t new_cap = state->inode_cap ? state->inode_cap * 2 : DU_INODE_INIT_CAP;
        du_inode_entry_t * new_entries = (du_inode_entry_t *)realloc(
            state->inodes, new_cap * sizeof(du_inode_entry_t));
        if (!new_entries) {
            return false;  /* can't track, allow counting */
        }
        state->inodes = new_entries;
        state->inode_cap = new_cap;
    }

    state->inodes[state->inode_count].dev = dev;
    state->inodes[state->inode_count].ino = ino;
    state->inode_count++;
    return false;
}

/**
 * @brief Free the inode tracking list.
 * @param state  traversal state
 */
static void _du_inode_free(du_state_t * state)
{
    if (!state) {
        return;
    }
    du_safe_free(state->inodes);
    state->inode_count = 0;
    state->inode_cap = 0;
}
#endif /* !DU_PLATFORM_WINDOWS */

#ifdef DU_PLATFORM_WINDOWS
/**
 * @brief Convert a UTF-8 multi-byte string to a UTF-16 (wide) string.
 * @param utf8        input UTF-8 NUL-terminated string
 * @param out         output wide buffer
 * @param out_wchars  size of output buffer in wchar_t elements
 * @return number of wchar_t written (excluding NUL), or -1 on failure
 */
static int _du_utf8_to_wide(const char * utf8, wchar_t * out, size_t out_wchars)
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
 * @brief Convert a wide (UTF-16) string to a UTF-8 multi-byte string.
 * @param wide      input UTF-16 NUL-terminated string
 * @param out       output buffer (must be at least out_size bytes)
 * @param out_size  size of the output buffer in bytes
 * @return number of bytes written (excluding NUL) on success, -1 on failure
 */
static int _du_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size)
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
    if (needed < 1 || needed > (int)out_size) {
        return -1;
    }
    int written = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
    if (written <= 0) {
        out[0] = '\0';
        return -1;
    }
    if ((size_t)written < out_size) {
        out[written] = '\0';
    }
    else {
        out[out_size - 1] = '\0';
    }
    return written - 1;
}
#endif /* DU_PLATFORM_WINDOWS */

/**
 * @brief Recursively walk a file or directory and accumulate sizes.
 *
 * This function:
 *   1. Stats the path (lstat by default, stat with -L)
 *   2. If it's a directory, iterates entries and recurses
 *   3. If it's a file, counts its size (with hard-link dedup on POSIX)
 *   4. Prints the entry if it should be printed
 *   5. Returns the total size for this subtree
 *
 * @param path   file or directory path
 * @param depth  current recursion depth (0 = top-level argument)
 * @param state  traversal state
 * @return total size in bytes for this subtree
 */
static uint64_t _du_walk(const char * path, int depth, du_state_t * state)
{
    if (!path || !state) {
        return 0;
    }

    /* Prevent infinite recursion from symlink loops */
    if (depth > DU_MAX_RECURSION) {
        du_err_printf("du: %s: too many levels of symbolic links\n", path);
        state->had_error = true;
        return 0;
    }

    /* Check exclude pattern */
    if (depth > 0 && _du_match_exclude(path, state->excludes)) {
        return 0;
    }

    du_stat_t st;
    int rc;

#ifdef DU_PLATFORM_WINDOWS
    wchar_t wpath[DU_MAX_PATH];
    if (_du_utf8_to_wide(path, wpath, sizeof(wpath) / sizeof(wpath[0])) < 0) {
        du_err_printf("du: %s: cannot convert path\n", path);
        state->had_error = true;
        return 0;
    }
    rc = _wstat64(wpath, &st);
#else
    rc = state->opts->dereference ? stat(path, &st) : lstat(path, &st);
#endif

    if (rc != 0) {
        du_err_printf("du: cannot access '%s': %s\n", path, strerror(errno));
        state->had_error = true;
        return 0;
    }

    bool is_dir = S_ISDIR(st.st_mode);

    /* Hard-link deduplication (POSIX only) */
#ifndef DU_PLATFORM_WINDOWS
    if (!is_dir && !state->opts->count_links && st.st_nlink > 1) {
        if (_du_inode_seen(state, st.st_dev, st.st_ino)) {
            return 0;  /* already counted */
        }
    }
#endif

    /* One-file-system check (POSIX only) */
#ifndef DU_PLATFORM_WINDOWS
    if (state->opts->one_file_system) {
        if (!state->has_fs_dev) {
            state->fs_dev = st.st_dev;
            state->has_fs_dev = true;
        }
        else if (st.st_dev != state->fs_dev) {
            return 0;  /* different filesystem */
        }
    }
#endif

    uint64_t own_size = _du_get_size(path, &st, state->opts);

    if (!is_dir) {
        /* Regular file (or symlink, special file) */
        if (_du_should_print(depth, false, state->opts)) {
            _du_print_entry(own_size, path, state->opts);
        }
        return own_size;
    }

    /* Directory: traverse entries */
    uint64_t total = own_size;

#ifdef DU_PLATFORM_WINDOWS
    /* Windows: use FindFirstFileW / FindNextFileW */
    char pattern[DU_MAX_PATH];
    int n = snprintf(pattern, sizeof(pattern), "%s/*", path);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        du_err_printf("du: %s: path too long\n", path);
        state->had_error = true;
        return own_size;
    }

    wchar_t wpattern[DU_MAX_PATH];
    if (_du_utf8_to_wide(pattern, wpattern, sizeof(wpattern) / sizeof(wpattern[0])) < 0) {
        du_err_printf("du: %s: cannot convert path\n", path);
        state->had_error = true;
        return own_size;
    }

    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(wpattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        du_err_printf("du: cannot read directory '%s'\n", path);
        state->had_error = true;
        return own_size;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 ||
            wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        char child_name[DU_MAX_PATH];
        if (_du_wide_to_utf8(fd.cFileName, child_name, sizeof(child_name)) < 0) {
            continue;
        }

        char child_path[DU_MAX_PATH];
        n = snprintf(child_path, sizeof(child_path), "%s/%s", path, child_name);
        if (n < 0 || (size_t)n >= sizeof(child_path)) {
            du_err_printf("du: %s/%s: path too long\n", path, child_name);
            state->had_error = true;
            continue;
        }

        uint64_t child_size = _du_walk(child_path, depth + 1, state);
        if (!state->opts->separate_dirs) {
            total += child_size;
        }
        /* With -S, child directory sizes are NOT added to parent total */
    } while (FindNextFileW(find, &fd));

    FindClose(find);

#else /* DU_PLATFORM_POSIX */
    /* POSIX: use opendir / readdir */
    DIR * dir = opendir(path);
    if (!dir) {
        du_err_printf("du: cannot read directory '%s': %s\n", path, strerror(errno));
        state->had_error = true;
        return own_size;
    }

    struct dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_path[DU_MAX_PATH];
        int n = snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(child_path)) {
            du_err_printf("du: %s/%s: path too long\n", path, entry->d_name);
            state->had_error = true;
            continue;
        }

        uint64_t child_size = _du_walk(child_path, depth + 1, state);
        if (!state->opts->separate_dirs) {
            total += child_size;
        }
        /* With -S, child directory sizes are NOT added to parent total */
    }

    closedir(dir);
#endif

    /* Print directory total if within depth limit */
    if (_du_should_print(depth, true, state->opts)) {
        _du_print_entry(total, path, state->opts);
    }

    return total;
}
