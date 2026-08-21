/**
 * @file df.c
 * @brief Cross-platform df command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 *
 * Key behaviors:
 *   - -a, --all: include pseudo, duplicate, inaccessible file systems
 *   - -B, --block-size=SIZE: scale sizes by SIZE
 *   - -h, --human-readable: print sizes in powers of 1024 (e.g., 1K, 234M, 2G)
 *   - -H, --si: print sizes in powers of 1000 (e.g., 1k, 234M, 2G)
 *   - -i, --inodes: list inode information instead of block usage
 *   - -k: like --block-size=1K
 *   - -l, --local: limit listing to local file systems
 *   - --no-sync: do not invoke sync before getting usage info (default)
 *   - --output[=FIELD_LIST]: use the output format defined by FIELD_LIST
 *   - -P, --portability: use the POSIX output format
 *   - --sync: invoke sync before getting usage info
 *   - --total: elide insignificant entries and produce a grand total
 *   - -t, --type=TYPE: limit listing to file systems of type TYPE
 *   - -T, --print-type: print file system type
 *   - -v: ignored (System V compatibility)
 *   - -x, --exclude-type=TYPE: exclude file systems of type TYPE
 *   - --help / --version: recognized and handled
 *   - Environment: DF_BLOCK_SIZE, BLOCK_SIZE, BLOCKSIZE, POSIXLY_CORRECT
 *   - Dynamic column width calculation (GNU df compatible)
 *   - --output mutually exclusive with -i, -P, -T
 *   - --output field list can be split across multiple uses
 *   - --total + --output interaction (source="total", target="-")
 *   - Windows wide-character path support for non-ANSI paths
 *   - UTF-8 output on all platforms
 *   - Forward-slash paths on all platforms
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o df.exe df.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o df df.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o df df.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o df df.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o df df.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o df df.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/df>
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
    #define DF_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define DF_PLATFORM_LINUX   1
    #define DF_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define DF_PLATFORM_MACOS   1
    #define DF_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define DF_PLATFORM_FREEBSD 1
    #define DF_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define DF_PLATFORM_OPENBSD 1
    #define DF_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define DF_PLATFORM_NETBSD  1
    #define DF_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define DF_PLATFORM_POSIX   1
#else
    #define DF_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef DF_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
    /* _DEFAULT_SOURCE exposes sync() and other glibc extensions */
    #ifndef _DEFAULT_SOURCE
        #define _DEFAULT_SOURCE
    #endif
#endif

#ifdef DF_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef DF_PLATFORM_NETBSD
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

#ifdef DF_PLATFORM_WINDOWS
    #include <windows.h>
    #include <fileapi.h>
#else /* DF_PLATFORM_POSIX */
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #ifdef DF_PLATFORM_LINUX
        #include <sys/statfs.h>
        #include <mntent.h>
    #endif
    #ifdef DF_PLATFORM_MACOS
        #include <sys/param.h>
        #include <sys/mount.h>
    #endif
    #if defined(DF_PLATFORM_FREEBSD) || defined(DF_PLATFORM_NETBSD) || defined(DF_PLATFORM_OPENBSD)
        #include <sys/param.h>
        #include <sys/mount.h>
    #endif
    #include <sys/statvfs.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define DF_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define DF_MAX_PATH 4096

/** @brief Maximum filesystem type string length */
#define DF_FS_TYPE_MAX 64

/** @brief Maximum number of filesystem entries we can collect */
#define DF_MAX_ENTRIES 256

/** @brief Default I/O block size (1 KiB) */
#define DF_DEFAULT_BLOCK_SIZE 1024

/** @brief Block size when POSIXLY_CORRECT is set (512 bytes) */
#define DF_POSIX_BLOCK_SIZE 512

/** @brief Buffer size for human-readable size strings */
#define DF_HUMAN_BUF_SIZE 32

/** @brief Maximum long option name length accepted by the parser */
#define DF_OPT_NAME_MAX 64

/** @brief Maximum number of include/exclude-type patterns */
#define DF_TYPE_CAP 8

/** @brief Maximum number of --output field columns */
#define DF_MAX_OUTPUT_FIELDS 32

/** @brief Maximum length of a single field name */
#define DF_FIELD_NAME_MAX 32

/** @brief Buffer for rendering a single table cell */
#define DF_CELL_BUF 64

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options for df
 *   all             - -a: include pseudo, duplicate, inaccessible file systems
 *   human_readable  - -h: print sizes in powers of 1024
 *   si              - -H: print sizes in powers of 1000
 *   inodes          - -i: list inode information instead of block usage
 *   local_only      - -l: limit listing to local file systems
 *   print_type      - -T: print file system type
 *   portability     - -P: use POSIX output format
 *   total           - --total: produce a grand total
 *   sync            - --sync: invoke sync before getting usage info
 *   no_sync         - --no-sync: do not invoke sync (default)
 *   block_size      - -B: scale sizes by SIZE (default: 1024)
 *   include_count   - number of included types (-t)
 *   exclude_count   - number of excluded types (-x)
 *   output_fields   - list of field names for --output
 *   output_count    - number of output fields (0 = default columns)
 */
typedef struct {
    bool all;
    bool human_readable;
    bool si;
    bool inodes;
    bool local_only;
    bool print_type;
    bool portability;
    bool total;
    bool sync;
    bool no_sync;
    uint64_t block_size;
    char include_types[DF_TYPE_CAP][DF_FS_TYPE_MAX];
    int  include_count;
    char exclude_types[DF_TYPE_CAP][DF_FS_TYPE_MAX];
    int  exclude_count;
    char output_fields[DF_MAX_OUTPUT_FIELDS][DF_FIELD_NAME_MAX];
    int  output_count;
} df_opts_t;

/**
 * @brief A single filesystem entry
 *   device        - device / source path (e.g., "/dev/sda1" or "C:")
 *   mount_point   - mount directory (e.g., "/" or "C:\")
 *   fs_type       - filesystem type (e.g., "ext4" or "ntfs")
 *   blocks_total  - total 1K-blocks
 *   blocks_used   - used 1K-blocks
 *   blocks_avail  - available 1K-blocks (for non-root)
 *   blocks_free   - free 1K-blocks (including reserved)
 *   inodes_total  - total inodes
 *   inodes_used   - used inodes
 *   inodes_free   - free inodes
 *   is_local      - true if this is a local filesystem
 *   is_pseudo     - true if pseudo filesystem (tmpfs, proc, etc.)
 *   is_network    - true if network filesystem (nfs, cifs, etc.)
 */
typedef struct {
    char device[DF_MAX_PATH];
    char mount_point[DF_MAX_PATH];
    char fs_type[DF_FS_TYPE_MAX];
    uint64_t blocks_total;
    uint64_t blocks_used;
    uint64_t blocks_avail;
    uint64_t blocks_free;
    uint64_t inodes_total;
    uint64_t inodes_used;
    uint64_t inodes_free;
    bool is_local;
    bool is_pseudo;
    bool is_network;
} df_fs_entry_t;

/**
 * @brief Array of filesystem entries (heap-allocated to avoid stack overflow)
 */
typedef struct {
    df_fs_entry_t * entries;
    int count;
    int capacity;
} df_fs_list_t;

/********************************
 *    static prototypes
 ********************************/
static void _df_print_help(void);
static void _df_print_version(void);
static int  _df_parse_args(int argc, char ** argv, df_opts_t * opts);
static uint64_t _df_get_default_block_size(void);
static uint64_t _df_parse_size(const char * str);
static void _df_human_size(uint64_t size, bool si, char * buf, size_t buf_size);
static void _df_format_size(uint64_t blocks, uint64_t block_size,
                            bool human, bool si, char * buf, size_t buf_size);
static int  _df_get_filesystems(df_fs_list_t * list, const df_opts_t * opts);
static bool _df_should_show(const df_fs_entry_t * e, const df_opts_t * opts);
static void _df_print_table(df_fs_list_t * list, const df_opts_t * opts);
static void _df_print_output_table(df_fs_list_t * list, const df_opts_t * opts);
static bool _df_is_excluded_type(const char * fs_type, const df_opts_t * opts);
static bool _df_is_included_type(const char * fs_type, const df_opts_t * opts);
static bool _df_strcaseeq(const char * a, const char * b);
#ifndef DF_PLATFORM_WINDOWS
static bool _df_is_network_type(const char * fs_type);
static bool _df_is_pseudo_type(const char * fs_type);
#endif

#ifdef DF_PLATFORM_WINDOWS
static int  _df_get_windows_filesystems(df_fs_list_t * list);
static int  _df_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size);
#else
static int  _df_get_posix_filesystems(df_fs_list_t * list);
#endif

static int  _df_parse_output_fields(const char * field_list, df_opts_t * opts);
static int  _df_get_field_id(const char * name);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for df_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef df_out_stream
    #define df_out_stream stdout
#endif

/**
 * @brief Default stderr stream for df_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef df_err_stream
    #define df_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 */
#ifndef df_printf
    #define df_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Wrapper for putchar.
 *        Defaults to libc @c putchar .
 *        Define externally to redirect.
 */
#ifndef df_putchar
    #define df_putchar(c) (void)putchar((c))
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef df_err_printf
    #define df_err_printf(fmt, ...) \
        do { if (df_err_stream) { (void)fprintf((df_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 */
#ifndef df_fputs
    #define df_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef df_fflush
    #define df_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe free-and-null pointer cleanup macro.
 */
#ifndef df_safe_free
    #define df_safe_free(p) \
        do { if ((p)) { free(p); (p) = NULL; } } while (0)
#endif

/** @brief Convert gigabytes to bytes (for --total calculations) */
#define DF_GB_TO_BYTES(n) ((uint64_t)(n) * 1024ULL * 1024ULL * 1024ULL)

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the df command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Enumerate filesystems (platform-specific)
 *   3. Filter by options (-l, -x, -a)
 *   4. Print header
 *   5. Print each matching filesystem
 *   6. If --total: print grand total line
 *   7. Return 0 on success, 1 on error
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    df_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.block_size = _df_get_default_block_size();

    int first_arg = _df_parse_args(argc, argv, &opts);
    if (first_arg < 0) {
        return 1;
    }

    /* -k forces 1K blocks */
    /* (handled in parser, but ensure default is 1024 if not set) */
    if (opts.block_size == 0) {
        opts.block_size = DF_DEFAULT_BLOCK_SIZE;
    }

    /* --sync: flush filesystem buffers before reading stats */
#ifdef DF_PLATFORM_POSIX
    if (opts.sync && !opts.no_sync) {
        sync();
    }
#endif

    df_fs_list_t fs_list;
    memset(&fs_list, 0, sizeof(fs_list));

    int rc = _df_get_filesystems(&fs_list, &opts);
    if (rc != 0) {
        df_err_printf("df: failed to read filesystem information\n");
        return 1;
    }

    /* If we have file arguments, filter to matching mount points */
    if (first_arg < argc) {
        df_fs_list_t filtered;
        memset(&filtered, 0, sizeof(filtered));
        filtered.entries = (df_fs_entry_t *)calloc(
            DF_MAX_ENTRIES, sizeof(df_fs_entry_t));
        if (filtered.entries) {
            filtered.capacity = DF_MAX_ENTRIES;
            for (int i = 0; i < fs_list.count; i++) {
                for (int j = first_arg; j < argc; j++) {
                    /* Check if the argument is under this mount point */
                    size_t mp_len = strlen(fs_list.entries[i].mount_point);
                    if (strncmp(fs_list.entries[i].mount_point, argv[j], mp_len) == 0) {
                        if (filtered.count < filtered.capacity) {
                            filtered.entries[filtered.count++] = fs_list.entries[i];
                        }
                        break;
                    }
                }
            }
            df_safe_free(fs_list.entries);
            fs_list = filtered;
        }
    }

    if (opts.output_count != 0) {
        /* --output mode (specified fields or all fields via -1) */
        _df_print_output_table(&fs_list, &opts);
    }
    else {
        _df_print_table(&fs_list, &opts);
    }

    df_safe_free(fs_list.entries);
    df_fflush(df_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _df_print_help(void)
{
    df_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Show information about the file system on which each FILE resides,\n"
        "or all file systems by default.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -a, --all             include pseudo, duplicate, inaccessible file systems\n"
        "  -B, --block-size=SIZE  scale sizes by SIZE before printing them\n"
        "  -h, --human-readable  print sizes in powers of 1024 (e.g., 1023M)\n"
        "  -H, --si              print sizes in powers of 1000 (e.g., 1.1G)\n"
        "  -i, --inodes          list inode information instead of block usage\n"
        "  -k                     like --block-size=1K\n"
        "  -l, --local           limit listing to local file systems\n"
        "      --no-sync         do not invoke sync before getting usage info (default)\n"
        "      --output[=FIELD_LIST]  use the output format defined by FIELD_LIST,\n"
        "                             or print all fields if FIELD_LIST is omitted.\n"
        "  -P, --portability     use the POSIX output format\n"
        "      --sync            invoke sync before getting usage info\n"
        "      --total           elide insignificant entries and produce a grand total\n"
        "  -t, --type=TYPE       limit listing to file systems of type TYPE\n"
        "  -T, --print-type      print file system type\n"
        "  -v                     (ignored for System V compatibility)\n"
        "  -x, --exclude-type=TYPE   exclude file systems of type TYPE\n"
        "      --help            display this help and exit\n"
        "      --version         output version information and exit\n"
        "\n"
        "Display values are in units of the first available SIZE from\n"
        "--block-size, and the environment variables DF_BLOCK_SIZE, BLOCK_SIZE\n"
        "and BLOCKSIZE.  The default is 1024 (or 512 if POSIXLY_CORRECT is set).\n"
        "SIZE is an integer and optional unit (e.g., 10M = 10*1024*1024).\n"
        "Units are: K,M,G,T,P,E (powers of 1024) or KB,MB,... (powers of 1000).\n"
        "\n"
        "FIELD_LIST is a comma-separated list of columns to be displayed.\n"
        "Valid field names are: source fstype itotal iused iavail ipcent\n"
        "size used avail pcent file target\n"
        "This list can be split across multiple --output uses.\n"
        "--output is mutually exclusive with -i, -P and -T.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
        , "df"
    );
}

/**
 * @brief Print version information
 */
static void _df_print_version(void)
{
    df_printf("df %s\n", DF_VERSION_STR);
    df_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    df_printf("%s", "License MIT: <https://mit-license.org/>\n");
    df_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    df_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Get the default block size from environment variables.
 *
 * Priority: DF_BLOCK_SIZE > BLOCK_SIZE > BLOCKSIZE > POSIXLY_CORRECT > 1024.
 * POSIXLY_CORRECT sets 512 bytes; other env vars set custom sizes.
 * @return resolved block size in bytes
 */
static uint64_t _df_get_default_block_size(void)
{
    const char * posix = getenv("POSIXLY_CORRECT");

    /* Check environment variables in priority order */
    const char * env_names[] = {"DF_BLOCK_SIZE", "BLOCK_SIZE", "BLOCKSIZE", NULL};
    for (int i = 0; env_names[i] != NULL; i++) {
        const char * val = getenv(env_names[i]);
        if (val && val[0] != '\0') {
            uint64_t sz = _df_parse_size(val);
            if (sz > 0) {
                return sz;
            }
        }
    }

    if (posix && posix[0] != '\0') {
        return DF_POSIX_BLOCK_SIZE;
    }
    return DF_DEFAULT_BLOCK_SIZE;
}

/**
 * @brief Parse a size string like "1K", "2M", "1KB", "1KiB", etc.
 * @param str  size string
 * @return size in bytes, or 0 on parse failure
 */
static uint64_t _df_parse_size(const char * str)
{
    if (!str || !*str) {
        return 0;
    }

    char * endptr = NULL;
    double val = strtod(str, &endptr);

    if (endptr == str) {
        return 0;
    }

    if (*endptr == '\0') {
        return (uint64_t)val;
    }

    uint64_t multiplier = 0;

    if (strncmp(endptr, "KiB", 3) == 0 || strncmp(endptr, "kib", 3) == 0) {
        multiplier = 1024ULL;
    }
    else if (strncmp(endptr, "MiB", 3) == 0 || strncmp(endptr, "mib", 3) == 0) {
        multiplier = 1024ULL * 1024;
    }
    else if (strncmp(endptr, "GiB", 3) == 0 || strncmp(endptr, "gib", 3) == 0) {
        multiplier = 1024ULL * 1024 * 1024;
    }
    else if (strncmp(endptr, "TiB", 3) == 0 || strncmp(endptr, "tib", 3) == 0) {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    }
    else if (strncmp(endptr, "PiB", 3) == 0 || strncmp(endptr, "pib", 3) == 0) {
        multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024;
    }
    else if (strncmp(endptr, "KB", 2) == 0 || strncmp(endptr, "kb", 2) == 0) {
        multiplier = 1000ULL;
    }
    else if (strncmp(endptr, "MB", 2) == 0 || strncmp(endptr, "mb", 2) == 0) {
        multiplier = 1000000ULL;
    }
    else if (strncmp(endptr, "GB", 2) == 0 || strncmp(endptr, "gb", 2) == 0) {
        multiplier = 1000000000ULL;
    }
    else if (strncmp(endptr, "TB", 2) == 0 || strncmp(endptr, "tb", 2) == 0) {
        multiplier = 1000000000000ULL;
    }
    else if (strncmp(endptr, "PB", 2) == 0 || strncmp(endptr, "pb", 2) == 0) {
        multiplier = 1000000000000000ULL;
    }
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
        return 0;
    }

    return (uint64_t)(val * (double)multiplier);
}

/**
 * @brief Parse command-line arguments into df_opts_t
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options structure
 * @return index of first file argument (or argc if none), -1 on error
 */
static int _df_parse_args(int argc, char ** argv, df_opts_t * opts)
{
    if (!opts) {
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
        if (!no_more_opts && strcmp(arg, "--") == 0) {
            no_more_opts = true;
            i++;
            break;
        }

        /* Long options */
        if (!no_more_opts && strncmp(arg, "--", 2) == 0) {
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[DF_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            const char * val = eq ? eq + 1 : NULL;

            if (strcmp(name, "help") == 0) {
                _df_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _df_print_version();
                exit(0);
            }
            if (strcmp(name, "all") == 0) {
                opts->all = true;
            }
            else if (strcmp(name, "human-readable") == 0) {
                opts->human_readable = true;
            }
            else if (strcmp(name, "si") == 0) {
                opts->si = true;
            }
            else if (strcmp(name, "inodes") == 0) {
                opts->inodes = true;
            }
            else if (strcmp(name, "local") == 0) {
                opts->local_only = true;
            }
            else if (strcmp(name, "print-type") == 0) {
                opts->print_type = true;
            }
            else if (strcmp(name, "portability") == 0) {
                opts->portability = true;
            }
            else if (strcmp(name, "total") == 0) {
                opts->total = true;
            }
            else if (strcmp(name, "sync") == 0) {
                opts->sync = true;
            }
            else if (strcmp(name, "no-sync") == 0) {
                opts->no_sync = true;
            }
            else if (strcmp(name, "block-size") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val) {
                    uint64_t sz = _df_parse_size(val);
                    if (sz > 0) {
                        opts->block_size = sz;
                    }
                }
            }
            else if (strcmp(name, "exclude-type") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val && opts->exclude_count < DF_TYPE_CAP) {
                    strncpy(opts->exclude_types[opts->exclude_count], val,
                            DF_FS_TYPE_MAX - 1);
                    opts->exclude_types[opts->exclude_count][DF_FS_TYPE_MAX - 1] = '\0';
                    opts->exclude_count++;
                }
            }
            else if (strcmp(name, "type") == 0) {
                if (!val && i + 1 < argc) {
                    val = argv[++i];
                }
                if (val && opts->include_count < DF_TYPE_CAP) {
                    strncpy(opts->include_types[opts->include_count], val,
                            DF_FS_TYPE_MAX - 1);
                    opts->include_types[opts->include_count][DF_FS_TYPE_MAX - 1] = '\0';
                    opts->include_count++;
                }
            }
            else if (strcmp(name, "output") == 0) {
                if (val) {
                    _df_parse_output_fields(val, opts);
                }
                else {
                    /* All fields: signal with -1 */
                    opts->output_count = -1;
                }
            }
            else {
                df_err_printf("df: unrecognized option '%s'\n", arg);
                df_err_printf("%s", "Try 'df --help' for more information.\n");
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

                    case 'h':
                        opts->human_readable = true;
                        break;

                    case 'H':
                        opts->si = true;
                        break;

                    case 'i':
                        opts->inodes = true;
                        break;

                    case 'k':
                        opts->block_size = 1024;
                        break;

                    case 'l':
                        opts->local_only = true;
                        break;

                    case 'T':
                        opts->print_type = true;
                        break;

                    case 'P':
                        opts->portability = true;
                        break;

                    case 't': {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val && opts->include_count < DF_TYPE_CAP) {
                            strncpy(opts->include_types[opts->include_count], val,
                                    DF_FS_TYPE_MAX - 1);
                            opts->include_types[opts->include_count][DF_FS_TYPE_MAX - 1] = '\0';
                            opts->include_count++;
                        }
                        break;
                    }

                    case 'v':
                        /* Ignored (System V compatibility) */
                        break;

                    case 'x': {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = arg + j + 1;
                            consumed_arg = true;
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        if (val && opts->exclude_count < DF_TYPE_CAP) {
                            strncpy(opts->exclude_types[opts->exclude_count], val,
                                    DF_FS_TYPE_MAX - 1);
                            opts->exclude_types[opts->exclude_count][DF_FS_TYPE_MAX - 1] = '\0';
                            opts->exclude_count++;
                        }
                        break;
                    }

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
                            uint64_t sz = _df_parse_size(val);
                            if (sz > 0) {
                                opts->block_size = sz;
                            }
                        }
                        break;
                    }

                    default:
                        df_err_printf("df: invalid option -- '%c'\n", arg[j]);
                        df_err_printf("%s", "Try 'df --help' for more information.\n");
                        return -1;
                }
            }
            continue;
        }

        /* Positional argument: this is a file path */
        break;
    }

    /* --output is mutually exclusive with -i, -P, -T */
    if (opts->output_count != 0) {
        if (opts->inodes) {
            df_err_printf("%s", "df: --output is incompatible with -i\n");
            return -1;
        }
        if (opts->portability) {
            df_err_printf("%s", "df: --output is incompatible with -P\n");
            return -1;
        }
        if (opts->print_type) {
            df_err_printf("%s", "df: --output is incompatible with -T\n");
            return -1;
        }
    }

    return i;
}

/**
 * @brief Format a size in bytes as a human-readable string.
 *
 * Uses 1 decimal place for values < 10 in the current unit,
 * 0 decimal places for values >= 10.
 *
 * @param size      size in bytes
 * @param si        true for --si (powers of 1000), false for -h (powers of 1024)
 * @param buf       output buffer
 * @param buf_size  size of output buffer
 */
static void _df_human_size(uint64_t size, bool si, char * buf, size_t buf_size)
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
 * @brief Format a block count for display.
 *
 * If human-readable mode is on, converts to human format.
 * Otherwise, scales by block_size and prints as integer.
 *
 * @param blocks     size in 1K-blocks
 * @param block_size scaling block size
 * @param human      true if human-readable mode
 * @param si         true for --si (1000), false for -h (1024)
 * @param buf        output buffer
 * @param buf_size   buffer size
 */
static void _df_format_size(uint64_t blocks, uint64_t block_size,
                            bool human, bool si, char * buf, size_t buf_size)
{
    if (human) {
        /* blocks are in 1K units; convert to bytes for human_size */
        _df_human_size(blocks * 1024ULL, si, buf, buf_size);
    }
    else {
        uint64_t scaled = (block_size > 0)
            ? (blocks * 1024ULL + block_size - 1) / block_size
            : blocks;
        snprintf(buf, buf_size, "%" PRIu64, scaled);
    }
}

/**
 * @brief Case-insensitive string equality check.
 * @param a  first string
 * @param b  second string
 * @return true if strings are equal ignoring case
 */
static bool _df_strcaseeq(const char * a, const char * b)
{
    if (!a || !b) {
        return a == b;
    }
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

#ifndef DF_PLATFORM_WINDOWS
/**
 * @brief Check if a filesystem type is network-based.
 * @param fs_type  filesystem type string
 * @return true if the type is a known network filesystem
 */
static bool _df_is_network_type(const char * fs_type)
{
    if (!fs_type || !*fs_type) {
        return false;
    }

    static const char * net_types[] = {
        "nfs", "nfs4", "cifs", "smb", "smbfs", "smb2", "smb3",
        "fuse.sshfs", "sshfs", "webdav", "davfs", "davfs2",
        "ftp", "ftps", "curl", "9p", "ceph", "glusterfs",
        "lustre", "ocfs2", "gfs", "gfs2", NULL
    };

    for (int i = 0; net_types[i] != NULL; i++) {
        if (_df_strcaseeq(fs_type, net_types[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if a filesystem type is pseudo (virtual).
 * @param fs_type  filesystem type string
 * @return true if the type is a known pseudo filesystem
 */
static bool _df_is_pseudo_type(const char * fs_type)
{
    if (!fs_type || !*fs_type) {
        return false;
    }

    static const char * pseudo_types[] = {
        "proc", "sysfs", "devpts", "tmpfs", "devtmpfs",
        "securityfs", "cgroup", "cgroup2", "pstore",
        "debugfs", "tracefs", "configfs", "fusectl",
        "hugetlbfs", "mqueue", "rpc_pipefs", "bpf",
        "autofs", "binfmt_misc", "selinuxfs", "ramfs",
        "none", "overlay", "squashfs", NULL
    };

    for (int i = 0; pseudo_types[i] != NULL; i++) {
        if (_df_strcaseeq(fs_type, pseudo_types[i])) {
            return true;
        }
    }
    return false;
}
#endif /* !DF_PLATFORM_WINDOWS */

/**
 * @brief Check if a filesystem type is in the exclude list.
 * @param fs_type  filesystem type string
 * @param opts     parsed options
 * @return true if the type should be excluded
 */
static bool _df_is_excluded_type(const char * fs_type, const df_opts_t * opts)
{
    if (!fs_type || !opts || opts->exclude_count <= 0) {
        return false;
    }

    for (int i = 0; i < opts->exclude_count; i++) {
        if (_df_strcaseeq(fs_type, opts->exclude_types[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if a filesystem type is in the include list (-t).
 * @param fs_type  filesystem type string
 * @param opts     parsed options
 * @return true if the type is in the include list (or list is empty)
 */
static bool _df_is_included_type(const char * fs_type, const df_opts_t * opts)
{
    if (!fs_type || !opts || opts->include_count <= 0) {
        return true;  /* no include filter = show all types */
    }

    for (int i = 0; i < opts->include_count; i++) {
        if (_df_strcaseeq(fs_type, opts->include_types[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Determine whether a filesystem entry should be displayed.
 *
 * Applies -t (include type), -x (exclude type), -l (local), -a (all) filters.
 *
 * @param e     filesystem entry
 * @param opts  parsed options
 * @return true if the entry should be shown
 */
static bool _df_should_show(const df_fs_entry_t * e, const df_opts_t * opts)
{
    if (!e || !opts) {
        return false;
    }

    /* Include by type (-t): if specified, only show matching types */
    if (!_df_is_included_type(e->fs_type, opts)) {
        return false;
    }

    /* Exclude by type (-x) */
    if (_df_is_excluded_type(e->fs_type, opts)) {
        return false;
    }

    /* -l: local only */
    if (opts->local_only && (e->is_network || !e->is_local)) {
        return false;
    }

    /* -a: show everything (including pseudo and inaccessible) */
    if (opts->all) {
        return true;
    }

    /* Default: skip pseudo filesystems and network */
    if (e->is_pseudo) {
        return false;
    }

    /* Skip entries with zero total blocks (inaccessible) */
    if (e->blocks_total == 0) {
        return false;
    }

    return true;
}

/**
 * @brief Column indices for default (non-output) mode.
 */
enum {
    DF_COL_DEV,
    DF_COL_TYPE,
    DF_COL_SIZE,
    DF_COL_USED,
    DF_COL_AVAIL,
    DF_COL_PCT,
    DF_COL_MOUNT,
    DF_MAX_COLS
};

/**
 * @brief Build the block-size label (e.g., "1K-blocks", "1M-blocks").
 * @param opts  parsed options
 * @param buf   output buffer
 * @param buf_size  buffer size
 */
static void _df_block_label(const df_opts_t * opts, char * buf, size_t buf_size)
{
    if (!opts || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    uint64_t bs = opts->block_size;
    if (opts->human_readable || opts->si) {
        snprintf(buf, buf_size, "Size");
    }
    else if (bs == 1024) {
        snprintf(buf, buf_size, "1K-blocks");
    }
    else if (bs == 1024 * 1024) {
        snprintf(buf, buf_size, "1M-blocks");
    }
    else if (bs == 1024 * 1024 * 1024) {
        snprintf(buf, buf_size, "1G-blocks");
    }
    else if (bs == 512) {
        snprintf(buf, buf_size, "512-blocks");
    }
    else if (bs == 1) {
        snprintf(buf, buf_size, "1B-blocks");
    }
    else {
        snprintf(buf, buf_size, "%" PRIu64 "-blocks", bs);
    }
}

/**
 * @brief Safely copy a string into a fixed-size buffer.
 * Uses strlen+memcpy to avoid -Wstringop-truncation warnings.
 * @param dst       destination buffer
 * @param dst_size  destination buffer size
 * @param src       source string (NULL = empty)
 */
static void _df_str_set(char * dst, size_t dst_size, const char * src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/**
 * @brief Format a filesystem entry into a row of pre-formatted cell strings.
 *
 * @param e       filesystem entry (NULL = total row)
 * @param opts    parsed options
 * @param cells   output: array of cell strings
 * @param ncells  output: number of cells written
 */
static void _df_format_row(const df_fs_entry_t * e, const df_opts_t * opts,
                           char cells[DF_MAX_COLS][DF_CELL_BUF], int * ncells)
{
    if (!opts || !cells || !ncells) {
        if (ncells) *ncells = 0;
        return;
    }

    /* Determine column layout */
    bool has_type = opts->print_type;

    /* Device */
    if (e) {
        _df_str_set(cells[DF_COL_DEV], DF_CELL_BUF, e->device);
    }
    else {
        _df_str_set(cells[DF_COL_DEV], DF_CELL_BUF, "total");
    }

    /* Type (optional) */
    if (has_type) {
        if (e) {
            _df_str_set(cells[DF_COL_TYPE], DF_CELL_BUF, e->fs_type);
        }
        else {
            _df_str_set(cells[DF_COL_TYPE], DF_CELL_BUF, "-");
        }
    }

    /* Size / Used / Avail / Pct */
    bool human = opts->human_readable || opts->si;
    char size_buf[DF_CELL_BUF];
    char used_buf[DF_CELL_BUF];
    char avail_buf[DF_CELL_BUF];
    char pct_buf[DF_CELL_BUF];

    if (opts->inodes) {
        /* Inode display */
        uint64_t itotal = e ? e->inodes_total : 0;
        uint64_t ifree  = e ? e->inodes_free : 0;
        uint64_t iused  = (itotal >= ifree) ? (itotal - ifree) : 0;
        uint64_t ipct   = itotal > 0 ? (iused * 100 / itotal) : 0;

        snprintf(size_buf,  sizeof(size_buf),  "%" PRIu64, itotal);
        snprintf(used_buf,  sizeof(used_buf),  "%" PRIu64, iused);
        snprintf(avail_buf, sizeof(avail_buf), "%" PRIu64, ifree);
        snprintf(pct_buf,   sizeof(pct_buf),   "%" PRIu64 "%%", ipct);
    }
    else {
        /* Block display */
        uint64_t btotal = e ? e->blocks_total : 0;
        uint64_t bfree  = e ? e->blocks_free : 0;
        uint64_t bavail = e ? e->blocks_avail : 0;
        uint64_t used   = (btotal >= bfree) ? (btotal - bfree) : 0;
        uint64_t avail  = bavail;
        uint64_t cap    = (used + avail) > 0 ? (used * 100 / (used + avail)) : 0;

        _df_format_size(btotal, opts->block_size, human, opts->si,
                        size_buf, sizeof(size_buf));
        _df_format_size(used, opts->block_size, human, opts->si,
                        used_buf, sizeof(used_buf));
        _df_format_size(avail, opts->block_size, human, opts->si,
                        avail_buf, sizeof(avail_buf));
        snprintf(pct_buf, sizeof(pct_buf), "%" PRIu64 "%%", cap);
    }

    /* Map cells to columns (accounting for optional Type column offset) */
    int size_idx  = has_type ? 2 : 1;
    int used_idx  = has_type ? 3 : 2;
    int avail_idx = has_type ? 4 : 3;
    int pct_idx   = has_type ? 5 : 4;
    int mount_idx = has_type ? 6 : 5;

    _df_str_set(cells[size_idx],  DF_CELL_BUF, size_buf);
    _df_str_set(cells[used_idx],  DF_CELL_BUF, used_buf);
    _df_str_set(cells[avail_idx], DF_CELL_BUF, avail_buf);
    _df_str_set(cells[pct_idx],   DF_CELL_BUF, pct_buf);

    /* Mount point */
    if (e) {
        _df_str_set(cells[mount_idx], DF_CELL_BUF, e->mount_point);
    }
    else {
        _df_str_set(cells[mount_idx], DF_CELL_BUF, "-");
    }

    *ncells = mount_idx + 1;
}

/**
 * @brief Print the entire table with dynamic column widths.
 *
 * Two-pass approach:
 *  1. Format all rows (header + entries + optional total) into cell strings
 *  2. Calculate max width per column
 *  3. Print with right-aligned numbers, left-aligned text
 *
 * @param list  filesystem list
 * @param opts  parsed options
 */
static void _df_print_table(df_fs_list_t * list, const df_opts_t * opts)
{
    if (!opts) {
        return;
    }

    bool has_type = opts->print_type;
    int mount_idx = has_type ? 6 : 5;
    int size_idx  = has_type ? 2 : 1;
    int used_idx  = has_type ? 3 : 2;
    int avail_idx = has_type ? 4 : 3;
    int pct_idx   = has_type ? 5 : 4;
    int ncells_per_row = mount_idx + 1;

    /* Allocate rows on stack (max entries is bounded) */
    char rows[DF_MAX_ENTRIES + 2][DF_MAX_COLS][DF_CELL_BUF];
    int nrows = 0;

    /* Build header row */
    {
        char block_label[DF_CELL_BUF];
        _df_block_label(opts, block_label, sizeof(block_label));

        _df_str_set(rows[nrows][DF_COL_DEV], DF_CELL_BUF, "Filesystem");

        if (has_type) {
            _df_str_set(rows[nrows][DF_COL_TYPE], DF_CELL_BUF, "Type");
        }

        if (opts->inodes) {
            _df_str_set(rows[nrows][size_idx],  DF_CELL_BUF, "Inodes");
            _df_str_set(rows[nrows][used_idx],  DF_CELL_BUF, "IUsed");
            _df_str_set(rows[nrows][avail_idx], DF_CELL_BUF, "IFree");
            _df_str_set(rows[nrows][pct_idx],   DF_CELL_BUF, "IUse%");
        }
        else {
            _df_str_set(rows[nrows][size_idx],  DF_CELL_BUF, block_label);
            if (opts->human_readable || opts->si) {
                _df_str_set(rows[nrows][used_idx],  DF_CELL_BUF, "Used");
                _df_str_set(rows[nrows][avail_idx], DF_CELL_BUF, "Avail");
            }
            else {
                _df_str_set(rows[nrows][used_idx],  DF_CELL_BUF, "Used");
                _df_str_set(rows[nrows][avail_idx], DF_CELL_BUF, "Available");
            }
            if (opts->portability) {
                _df_str_set(rows[nrows][pct_idx], DF_CELL_BUF, "Capacity");
            }
            else {
                _df_str_set(rows[nrows][pct_idx], DF_CELL_BUF, "Use%");
            }
        }

        _df_str_set(rows[nrows][mount_idx], DF_CELL_BUF, "Mounted on");
        nrows++;
    }

    /* Build entry rows */
    if (list) {
        for (int i = 0; i < list->count && nrows < (int)(sizeof(rows)/sizeof(rows[0])); i++) {
            if (!_df_should_show(&list->entries[i], opts)) {
                continue;
            }
            int nc = 0;
            _df_format_row(&list->entries[i], opts, rows[nrows], &nc);
            nrows++;
        }
    }

    /* Build total row */
    if (opts->total && list && nrows < (int)(sizeof(rows)/sizeof(rows[0]))) {
        df_fs_entry_t sum;
        memset(&sum, 0, sizeof(sum));

        for (int i = 0; i < list->count; i++) {
            if (!_df_should_show(&list->entries[i], opts)) {
                continue;
            }
            sum.blocks_total += list->entries[i].blocks_total;
            sum.blocks_free  += list->entries[i].blocks_free;
            sum.blocks_avail += list->entries[i].blocks_avail;
            sum.inodes_total += list->entries[i].inodes_total;
            sum.inodes_free  += list->entries[i].inodes_free;
        }

        int nc = 0;
        _df_format_row(&sum, opts, rows[nrows], &nc);
        /* Override device and mount for total row */
        _df_str_set(rows[nrows][DF_COL_DEV], DF_CELL_BUF, "total");
        _df_str_set(rows[nrows][mount_idx], DF_CELL_BUF, "-");
        nrows++;
    }

    /* Calculate column widths */
    int widths[DF_MAX_COLS];
    for (int c = 0; c < DF_MAX_COLS; c++) {
        widths[c] = 0;
    }
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncells_per_row; c++) {
            int len = (int)strlen(rows[r][c]);
            if (len > widths[c]) {
                widths[c] = len;
            }
        }
    }

    /* Right-align: size, used, avail, pct columns */
    bool right_align[DF_MAX_COLS] = {false};
    right_align[size_idx]  = true;
    right_align[used_idx]  = true;
    right_align[avail_idx] = true;
    right_align[pct_idx]   = true;

    /* Print all rows */
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncells_per_row; c++) {
            if (c > 0) {
                df_putchar(' ');
            }
            int len = (int)strlen(rows[r][c]);
            int pad = widths[c] - len;
            if (right_align[c] && pad > 0) {
                for (int s = 0; s < pad; s++) {
                    df_putchar(' ');
                }
            }
            df_printf("%s", rows[r][c]);
            if (!right_align[c] && pad > 0) {
                for (int s = 0; s < pad; s++) {
                    df_putchar(' ');
                }
            }
        }
        df_printf("%s", "\n");
    }
}

/**
 * @brief Get the field ID for a named --output field.
 * @param name  field name
 * @return field ID (0-based), or -1 if unknown
 */
static int _df_get_field_id(const char * name)
{
    if (!name || !*name) {
        return -1;
    }

    if (strcmp(name, "source") == 0)      return 0;
    if (strcmp(name, "fstype") == 0)     return 1;
    if (strcmp(name, "size") == 0)       return 2;
    if (strcmp(name, "used") == 0)       return 3;
    if (strcmp(name, "avail") == 0)      return 4;
    if (strcmp(name, "pcent") == 0)      return 5;
    if (strcmp(name, "target") == 0)     return 6;
    if (strcmp(name, "mount") == 0)      return 6;
    if (strcmp(name, "file") == 0)       return 7;
    if (strcmp(name, "itotal") == 0)     return 8;
    if (strcmp(name, "iused") == 0)      return 9;
    if (strcmp(name, "iavail") == 0)     return 10;
    if (strcmp(name, "ipcent") == 0)     return 11;
    return -1;
}

/**
 * @brief Parse a comma-separated field list for --output.
 *
 * Appends to existing fields to support split --output uses.
 * Detects duplicate fields and unknown fields.
 *
 * @param field_list  comma-separated field names
 * @param opts        output options
 * @return 0 on success, -1 on error
 */
static int _df_parse_output_fields(const char * field_list, df_opts_t * opts)
{
    if (!field_list || !opts) {
        return -1;
    }

    char buf[DF_MAX_PATH];
    strncpy(buf, field_list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char * saveptr = NULL;
    char * token = strtok_r(buf, ",", &saveptr);

    while (token && opts->output_count < DF_MAX_OUTPUT_FIELDS) {
        /* Trim whitespace */
        while (*token && isspace((unsigned char)*token)) token++;
        char * end = token + strlen(token);
        while (end > token && isspace((unsigned char)*(end - 1))) {
            end--;
            *end = '\0';
        }

        if (*token) {
            if (_df_get_field_id(token) < 0) {
                df_err_printf("df: unknown field: %s\n", token);
                return -1;
            }
            /* Check for duplicate field */
            for (int i = 0; i < opts->output_count; i++) {
                if (strcmp(opts->output_fields[i], token) == 0) {
                    df_err_printf("df: field %s used more than once\n", token);
                    return -1;
                }
            }
            _df_str_set(opts->output_fields[opts->output_count], DF_FIELD_NAME_MAX, token);
            opts->output_count++;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

/**
 * @brief Resolve the active field list for --output mode.
 *
 * If output_count is -1 (bare --output), use the full default list.
 * Otherwise use the user-specified fields.
 *
 * @param opts     parsed options
 * @param out_arr  output array of field name strings (caller-allocated)
 * @param max      maximum number of fields
 * @return number of fields
 */
static int _df_resolve_output_fields(const df_opts_t * opts,
                                     char out_arr[][DF_FIELD_NAME_MAX], int max)
{
    static const char * all_fields[] = {
        "source", "fstype", "itotal", "iused", "iavail", "ipcent",
        "size", "used", "avail", "pcent", "file", "target"
    };
    int count = 12;

    if (opts->output_count > 0) {
        count = opts->output_count;
        for (int i = 0; i < count && i < max; i++) {
            _df_str_set(out_arr[i], DF_FIELD_NAME_MAX, opts->output_fields[i]);
        }
        return count;
    }

    /* All fields (output_count == -1) */
    for (int i = 0; i < count && i < max; i++) {
        _df_str_set(out_arr[i], DF_FIELD_NAME_MAX, all_fields[i]);
    }
    return count;
}

/**
 * @brief Check if a field should be right-aligned in output.
 * @param fname  field name
 * @return true if numeric (right-aligned), false if text (left-aligned)
 */
static bool _df_field_is_numeric(const char * fname)
{
    if (!fname) {
        return false;
    }
    if (strcmp(fname, "size") == 0)   return true;
    if (strcmp(fname, "used") == 0)   return true;
    if (strcmp(fname, "avail") == 0)  return true;
    if (strcmp(fname, "pcent") == 0)  return true;
    if (strcmp(fname, "itotal") == 0) return true;
    if (strcmp(fname, "iused") == 0)  return true;
    if (strcmp(fname, "iavail") == 0) return true;
    if (strcmp(fname, "ipcent") == 0) return true;
    return false;
}

/**
 * @brief Format a single --output cell into a string.
 *
 * @param fname   field name
 * @param e       filesystem entry (NULL = total row)
 * @param opts    parsed options
 * @param buf     output buffer
 * @param buf_size  buffer size
 */
static void _df_format_output_cell(const char * fname,
                                   const df_fs_entry_t * e,
                                   const df_opts_t * opts,
                                   char * buf, size_t buf_size)
{
    if (!fname || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    bool human = opts->human_readable || opts->si;
    buf[0] = '\0';

    if (strcmp(fname, "source") == 0) {
        _df_str_set(buf, buf_size, e ? e->device : "total");
    }
    else if (strcmp(fname, "fstype") == 0) {
        _df_str_set(buf, buf_size, e ? e->fs_type : "-");
    }
    else if (strcmp(fname, "file") == 0) {
        _df_str_set(buf, buf_size, "-");
    }
    else if (strcmp(fname, "target") == 0) {
        _df_str_set(buf, buf_size, e ? e->mount_point : "-");
    }
    else if (strcmp(fname, "size") == 0) {
        uint64_t v = e ? e->blocks_total : 0;
        _df_format_size(v, opts->block_size, human, opts->si, buf, buf_size);
    }
    else if (strcmp(fname, "used") == 0) {
        uint64_t btotal = e ? e->blocks_total : 0;
        uint64_t bfree  = e ? e->blocks_free : 0;
        uint64_t v = (btotal >= bfree) ? (btotal - bfree) : 0;
        _df_format_size(v, opts->block_size, human, opts->si, buf, buf_size);
    }
    else if (strcmp(fname, "avail") == 0) {
        uint64_t v = e ? e->blocks_avail : 0;
        _df_format_size(v, opts->block_size, human, opts->si, buf, buf_size);
    }
    else if (strcmp(fname, "pcent") == 0) {
        uint64_t btotal = e ? e->blocks_total : 0;
        uint64_t bfree  = e ? e->blocks_free : 0;
        uint64_t bavail = e ? e->blocks_avail : 0;
        uint64_t used = (btotal >= bfree) ? (btotal - bfree) : 0;
        uint64_t cap  = (used + bavail) > 0 ? (used * 100 / (used + bavail)) : 0;
        snprintf(buf, buf_size, "%" PRIu64 "%%", cap);
    }
    else if (strcmp(fname, "itotal") == 0) {
        snprintf(buf, buf_size, "%" PRIu64, e ? e->inodes_total : 0);
    }
    else if (strcmp(fname, "iused") == 0) {
        uint64_t itotal = e ? e->inodes_total : 0;
        uint64_t ifree  = e ? e->inodes_free : 0;
        uint64_t v = (itotal >= ifree) ? (itotal - ifree) : 0;
        snprintf(buf, buf_size, "%" PRIu64, v);
    }
    else if (strcmp(fname, "iavail") == 0) {
        snprintf(buf, buf_size, "%" PRIu64, e ? e->inodes_free : 0);
    }
    else if (strcmp(fname, "ipcent") == 0) {
        uint64_t itotal = e ? e->inodes_total : 0;
        uint64_t ifree  = e ? e->inodes_free : 0;
        uint64_t iused  = (itotal >= ifree) ? (itotal - ifree) : 0;
        uint64_t ipct   = itotal > 0 ? (iused * 100 / itotal) : 0;
        snprintf(buf, buf_size, "%" PRIu64 "%%", ipct);
    }
    else {
        _df_str_set(buf, buf_size, "-");
    }
}

/**
 * @brief Print the entire --output table with dynamic column widths.
 *
 * Handles --total interaction: source="total", target="-".
 *
 * @param list  filesystem list
 * @param opts  parsed options
 */
static void _df_print_output_table(df_fs_list_t * list, const df_opts_t * opts)
{
    if (!opts) {
        return;
    }

    /* Resolve fields */
    char fields[DF_MAX_OUTPUT_FIELDS][DF_FIELD_NAME_MAX];
    int nfields = _df_resolve_output_fields(opts, fields, DF_MAX_OUTPUT_FIELDS);
    if (nfields <= 0) {
        return;
    }

    /* Stack-allocate row/cell matrix */
    char rows[DF_MAX_ENTRIES + 2][DF_MAX_OUTPUT_FIELDS][DF_CELL_BUF];
    int nrows = 0;

    /* Build header row */
    for (int c = 0; c < nfields; c++) {
        _df_str_set(rows[nrows][c], DF_CELL_BUF, fields[c]);
    }
    nrows++;

    /* Build entry rows */
    if (list) {
        for (int i = 0; i < list->count && nrows < (int)(sizeof(rows)/sizeof(rows[0])); i++) {
            if (!_df_should_show(&list->entries[i], opts)) {
                continue;
            }
            for (int c = 0; c < nfields; c++) {
                _df_format_output_cell(fields[c], &list->entries[i], opts,
                                        rows[nrows][c], DF_CELL_BUF);
            }
            nrows++;
        }
    }

    /* Build total row */
    if (opts->total && list && nrows < (int)(sizeof(rows)/sizeof(rows[0]))) {
        df_fs_entry_t sum;
        memset(&sum, 0, sizeof(sum));

        for (int i = 0; i < list->count; i++) {
            if (!_df_should_show(&list->entries[i], opts)) {
                continue;
            }
            sum.blocks_total += list->entries[i].blocks_total;
            sum.blocks_free  += list->entries[i].blocks_free;
            sum.blocks_avail += list->entries[i].blocks_avail;
            sum.inodes_total += list->entries[i].inodes_total;
            sum.inodes_free  += list->entries[i].inodes_free;
        }

        for (int c = 0; c < nfields; c++) {
            _df_format_output_cell(fields[c], &sum, opts,
                                    rows[nrows][c], DF_CELL_BUF);
        }
        /* Override source/target for total row */
        for (int c = 0; c < nfields; c++) {
            if (strcmp(fields[c], "source") == 0) {
                _df_str_set(rows[nrows][c], DF_CELL_BUF, "total");
            }
            else if (strcmp(fields[c], "target") == 0 && nfields > 0) {
                /* If source field absent, put "total" in target */
                bool has_source = false;
                for (int k = 0; k < nfields; k++) {
                    if (strcmp(fields[k], "source") == 0) {
                        has_source = true;
                        break;
                    }
                }
                if (!has_source) {
                    _df_str_set(rows[nrows][c], DF_CELL_BUF, "total");
                }
                else {
                    _df_str_set(rows[nrows][c], DF_CELL_BUF, "-");
                }
            }
        }
        nrows++;
    }

    /* Calculate column widths */
    int widths[DF_MAX_OUTPUT_FIELDS];
    for (int c = 0; c < nfields; c++) {
        widths[c] = 0;
    }
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < nfields; c++) {
            int len = (int)strlen(rows[r][c]);
            if (len > widths[c]) {
                widths[c] = len;
            }
        }
    }

    /* Print all rows */
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < nfields; c++) {
            if (c > 0) {
                df_putchar(' ');
            }
            int len = (int)strlen(rows[r][c]);
            int pad = widths[c] - len;
            bool right = _df_field_is_numeric(fields[c]);
            if (right && pad > 0) {
                for (int s = 0; s < pad; s++) {
                    df_putchar(' ');
                }
            }
            df_printf("%s", rows[r][c]);
            if (!right && pad > 0) {
                for (int s = 0; s < pad; s++) {
                    df_putchar(' ');
                }
            }
        }
        df_printf("%s", "\n");
    }
}

/* ============================================================
 *  Platform-specific filesystem enumeration
 * ============================================================ */

#ifdef DF_PLATFORM_WINDOWS

/**
 * @brief Convert a wide (UTF-16) string to a UTF-8 multi-byte string.
 * @param wide      input UTF-16 NUL-terminated string
 * @param out       output buffer
 * @param out_size  size of the output buffer in bytes
 * @return number of bytes written (excluding NUL) on success, -1 on failure
 */
static int _df_wide_to_utf8(const wchar_t * wide, char * out, size_t out_size)
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
    if (needed < 1 || (size_t)needed > out_size) {
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

/**
 * @brief Enumerate filesystems on Windows using Win32 API.
 *
 * Uses GetLogicalDriveStringsW to enumerate all drives, then
 * GetDiskFreeSpaceExW for space info and GetVolumeInformationW
 * for label and filesystem type.
 *
 * @param list  output filesystem list
 * @return 0 on success, -1 on failure
 */
static int _df_get_windows_filesystems(df_fs_list_t * list)
{
    if (!list) {
        return -1;
    }

    wchar_t drives[512];
    DWORD len = GetLogicalDriveStringsW(
        sizeof(drives) / sizeof(drives[0]), drives);
    if (len == 0 || len >= sizeof(drives) / sizeof(drives[0])) {
        return -1;
    }

    wchar_t * p = drives;
    while (*p && list->count < list->capacity) {
        size_t dlen = wcslen(p);
        UINT drive_type = GetDriveTypeW(p);

        df_fs_entry_t * e = &list->entries[list->count];
        memset(e, 0, sizeof(*e));

        /* Device name: drive root like "C:/" (forward slashes on all platforms) */
        _df_wide_to_utf8(p, e->device, sizeof(e->device) - 1);
        _df_wide_to_utf8(p, e->mount_point, sizeof(e->mount_point) - 1);
        for (char * s = e->device; *s; s++) {
            if (*s == '\\') *s = '/';
        }
        for (char * s = e->mount_point; *s; s++) {
            if (*s == '\\') *s = '/';
        }

        /* Filesystem type string */
        wchar_t fs_name[DF_FS_TYPE_MAX];
        fs_name[0] = L'\0';
        DWORD fs_flags;
        wchar_t vol_name[256];
        vol_name[0] = L'\0';
        BOOL ok = GetVolumeInformationW(
            p, vol_name, sizeof(vol_name) / sizeof(vol_name[0]),
            NULL, NULL, &fs_flags,
            fs_name, sizeof(fs_name) / sizeof(fs_name[0]));

        if (ok && fs_name[0]) {
            _df_wide_to_utf8(fs_name, e->fs_type, sizeof(e->fs_type) - 1);
        }
        else {
            /* Drive not ready (CD/DVD with no media, etc.) */
            if (drive_type == DRIVE_CDROM) {
                strncpy(e->fs_type, "iso9660", sizeof(e->fs_type) - 1);
            }
            else if (drive_type == DRIVE_REMOVABLE) {
                strncpy(e->fs_type, "removable", sizeof(e->fs_type) - 1);
            }
            else if (drive_type == DRIVE_REMOTE) {
                strncpy(e->fs_type, "network", sizeof(e->fs_type) - 1);
            }
            else {
                strncpy(e->fs_type, "unknown", sizeof(e->fs_type) - 1);
            }
        }

        /* Drive type classification */
        switch (drive_type) {
            case DRIVE_FIXED:
                e->is_local = true;
                e->is_network = false;
                break;
            case DRIVE_REMOVABLE:
            case DRIVE_RAMDISK:
                e->is_local = true;
                e->is_network = false;
                break;
            case DRIVE_REMOTE:
                e->is_local = false;
                e->is_network = true;
                break;
            case DRIVE_CDROM:
                e->is_local = true;
                e->is_network = false;
                break;
            default:
                e->is_local = true;
                e->is_network = false;
                break;
        }

        e->is_pseudo = false;

        /* Get disk space info */
        ULARGE_INTEGER free_bytes_avail;
        ULARGE_INTEGER total_bytes;
        ULARGE_INTEGER total_free;

        ok = GetDiskFreeSpaceExW(
            p,
            &free_bytes_avail,
            &total_bytes,
            &total_free);

        if (ok) {
            /* Convert bytes to 1K-blocks */
            e->blocks_total  = total_bytes.QuadPart / 1024ULL;
            e->blocks_free   = total_free.QuadPart / 1024ULL;
            e->blocks_avail  = free_bytes_avail.QuadPart / 1024ULL;

            /* Windows doesn't expose inode counts; estimate from free space */
            e->inodes_total = 0;
            e->inodes_used = 0;
            e->inodes_free = 0;
        }
        else {
            /* Drive not accessible */
            e->blocks_total = 0;
            e->blocks_free = 0;
            e->blocks_avail = 0;
        }

        list->count++;
        p += dlen + 1;
    }

    return 0;
}

#else /* DF_PLATFORM_POSIX */

#ifdef DF_PLATFORM_LINUX
/**
 * @brief Enumerate filesystems on Linux by parsing /proc/mounts.
 *
 * Uses statvfs() for each mount point to get space info.
 *
 * @param list  output filesystem list
 * @return 0 on success, -1 on failure
 */
static int _df_get_posix_filesystems(df_fs_list_t * list)
{
    if (!list) {
        return -1;
    }

    FILE * f = setmntent("/proc/mounts", "r");
    if (!f) {
        f = setmntent("/etc/mtab", "r");
    }
    if (!f) {
        /* Fallback: try /etc/fstab */
        f = setmntent("/etc/fstab", "r");
    }
    if (!f) {
        return -1;
    }

    struct mntent * me;
    while ((me = getmntent(f)) != NULL && list->count < list->capacity) {
        df_fs_entry_t * e = &list->entries[list->count];
        memset(e, 0, sizeof(*e));

        strncpy(e->device, me->mnt_fsname, sizeof(e->device) - 1);
        strncpy(e->mount_point, me->mnt_dir, sizeof(e->mount_point) - 1);
        strncpy(e->fs_type, me->mnt_type, sizeof(e->fs_type) - 1);

        e->is_pseudo = _df_is_pseudo_type(e->fs_type);
        e->is_network = _df_is_network_type(e->fs_type);
        e->is_local = !e->is_network;

        struct statvfs sv;
        if (statvfs(e->mount_point, &sv) == 0) {
            uint64_t bsize = sv.f_frsize > 0 ? sv.f_frsize : sv.f_bsize;
            e->blocks_total = (uint64_t)sv.f_blocks * bsize / 1024ULL;
            e->blocks_free  = (uint64_t)sv.f_bfree * bsize / 1024ULL;
            e->blocks_avail = (uint64_t)sv.f_bavail * bsize / 1024ULL;
            e->inodes_total = sv.f_files;
            e->inodes_free  = sv.f_ffree;
            e->inodes_used  = sv.f_files - sv.f_ffree;
        }
        else {
            e->blocks_total = 0;
            e->blocks_free = 0;
            e->blocks_avail = 0;
            e->inodes_total = 0;
            e->inodes_free = 0;
            e->inodes_used = 0;
        }

        list->count++;
    }

    endmntent(f);
    return 0;
}

#elif defined(DF_PLATFORM_MACOS) || defined(DF_PLATFORM_FREEBSD) || \
      defined(DF_PLATFORM_NETBSD) || defined(DF_PLATFORM_OPENBSD)

/**
 * @brief Enumerate filesystems on macOS/BSD using getmntinfo/getfsstat.
 *
 * Uses statvfs() for each mount point to get space info.
 *
 * @param list  output filesystem list
 * @return 0 on success, -1 on failure
 */
static int _df_get_posix_filesystems(df_fs_list_t * list)
{
    if (!list) {
        return -1;
    }

    struct statfs * mounts = NULL;
    int count = getmntinfo(&mounts, MNT_WAIT);

    #ifdef DF_PLATFORM_OPENBSD
    /* OpenBSD uses getfsstat directly */
    mounts = NULL;
    int buf_size = 0;
    count = getfsstat(NULL, 0, MNT_WAIT);
    if (count <= 0) {
        return -1;
    }
    buf_size = count * (int)sizeof(struct statfs);
    mounts = (struct statfs *)malloc(buf_size);
    if (!mounts) {
        return -1;
    }
    count = getfsstat(mounts, buf_size, MNT_WAIT);
    #endif

    if (count <= 0) {
        #ifdef DF_PLATFORM_OPENBSD
        free(mounts);
        #endif
        return -1;
    }

    for (int i = 0; i < count && list->count < list->capacity; i++) {
        df_fs_entry_t * e = &list->entries[list->count];
        memset(e, 0, sizeof(*e));

        strncpy(e->device, mounts[i].f_mntfromname, sizeof(e->device) - 1);
        strncpy(e->mount_point, mounts[i].f_mntonname, sizeof(e->mount_point) - 1);
        strncpy(e->fs_type, mounts[i].f_fstypename, sizeof(e->fs_type) - 1);

        e->is_pseudo = _df_is_pseudo_type(e->fs_type);
        e->is_network = _df_is_network_type(e->fs_type);
        e->is_local = !e->is_network;

        /* Use statvfs for portable block info */
        struct statvfs sv;
        if (statvfs(e->mount_point, &sv) == 0) {
            uint64_t bsize = sv.f_frsize > 0 ? sv.f_frsize : sv.f_bsize;
            e->blocks_total = (uint64_t)sv.f_blocks * bsize / 1024ULL;
            e->blocks_free  = (uint64_t)sv.f_bfree * bsize / 1024ULL;
            e->blocks_avail = (uint64_t)sv.f_bavail * bsize / 1024ULL;
            e->inodes_total = sv.f_files;
            e->inodes_free  = sv.f_ffree;
            e->inodes_used  = sv.f_files - sv.f_ffree;
        }
        else {
            /* Fall back to statfs data */
            uint64_t bsize = mounts[i].f_bsize;
            e->blocks_total = (uint64_t)mounts[i].f_blocks * bsize / 1024ULL;
            e->blocks_free  = (uint64_t)mounts[i].f_bfree * bsize / 1024ULL;
            e->blocks_avail = (uint64_t)mounts[i].f_bavail * bsize / 1024ULL;
            e->inodes_total = mounts[i].f_files;
            e->inodes_free  = mounts[i].f_ffree;
            e->inodes_used  = mounts[i].f_files - mounts[i].f_ffree;
        }

        list->count++;
    }

    #ifdef DF_PLATFORM_OPENBSD
    free(mounts);
    #endif

    return 0;
}

#else /* Generic POSIX fallback */

/**
 * @brief Enumerate filesystems on generic POSIX by parsing /etc/mtab.
 * @param list  output filesystem list
 * @return 0 on success, -1 on failure
 */
static int _df_get_posix_filesystems(df_fs_list_t * list)
{
    if (!list) {
        return -1;
    }

    FILE * f = setmntent("/etc/mtab", "r");
    if (!f) {
        f = setmntent("/etc/fstab", "r");
    }
    if (!f) {
        /* Last resort: try /proc/mounts */
        f = fopen("/proc/mounts", "r");
        if (!f) {
            return -1;
        }
        /* Parse manually */
        char line[DF_MAX_PATH * 2];
        while (fgets(line, sizeof(line), f) && list->count < list->capacity) {
            char dev[DF_MAX_PATH];
            char mp[DF_MAX_PATH];
            char type[DF_FS_TYPE_MAX];
            if (sscanf(line, "%s %s %s", dev, mp, type) != 3) {
                continue;
            }
            df_fs_entry_t * e = &list->entries[list->count];
            memset(e, 0, sizeof(*e));
            strncpy(e->device, dev, sizeof(e->device) - 1);
            strncpy(e->mount_point, mp, sizeof(e->mount_point) - 1);
            strncpy(e->fs_type, type, sizeof(e->fs_type) - 1);
            e->is_pseudo = _df_is_pseudo_type(e->fs_type);
            e->is_network = _df_is_network_type(e->fs_type);
            e->is_local = !e->is_network;
            struct statvfs sv;
            if (statvfs(e->mount_point, &sv) == 0) {
                uint64_t bsize = sv.f_frsize > 0 ? sv.f_frsize : sv.f_bsize;
                e->blocks_total = (uint64_t)sv.f_blocks * bsize / 1024ULL;
                e->blocks_free  = (uint64_t)sv.f_bfree * bsize / 1024ULL;
                e->blocks_avail = (uint64_t)sv.f_bavail * bsize / 1024ULL;
                e->inodes_total = sv.f_files;
                e->inodes_free  = sv.f_ffree;
                e->inodes_used  = sv.f_files - sv.f_ffree;
            }
            list->count++;
        }
        fclose(f);
        return 0;
    }

    struct mntent * me;
    while ((me = getmntent(f)) != NULL && list->count < list->capacity) {
        df_fs_entry_t * e = &list->entries[list->count];
        memset(e, 0, sizeof(*e));
        strncpy(e->device, me->mnt_fsname, sizeof(e->device) - 1);
        strncpy(e->mount_point, me->mnt_dir, sizeof(e->mount_point) - 1);
        strncpy(e->fs_type, me->mnt_type, sizeof(e->fs_type) - 1);
        e->is_pseudo = _df_is_pseudo_type(e->fs_type);
        e->is_network = _df_is_network_type(e->fs_type);
        e->is_local = !e->is_network;
        struct statvfs sv;
        if (statvfs(e->mount_point, &sv) == 0) {
            uint64_t bsize = sv.f_frsize > 0 ? sv.f_frsize : sv.f_bsize;
            e->blocks_total = (uint64_t)sv.f_blocks * bsize / 1024ULL;
            e->blocks_free  = (uint64_t)sv.f_bfree * bsize / 1024ULL;
            e->blocks_avail = (uint64_t)sv.f_bavail * bsize / 1024ULL;
            e->inodes_total = sv.f_files;
            e->inodes_free  = sv.f_ffree;
            e->inodes_used  = sv.f_files - sv.f_ffree;
        }
        list->count++;
    }

    endmntent(f);
    return 0;
}

#endif /* Platform-specific POSIX implementations */

#endif /* DF_PLATFORM_WINDOWS */

/**
 * @brief Dispatch to the platform-specific filesystem enumerator.
 * @param list  output filesystem list
 * @param opts  parsed options (unused for now, reserved for future)
 * @return 0 on success, -1 on failure
 */
static int _df_get_filesystems(df_fs_list_t * list, const df_opts_t * opts)
{
    (void)opts;

    if (!list) {
        return -1;
    }

    /* Allocate the entries array on the heap to avoid stack overflow */
    list->entries = (df_fs_entry_t *)calloc(
        DF_MAX_ENTRIES, sizeof(df_fs_entry_t));
    if (!list->entries) {
        return -1;
    }
    list->capacity = DF_MAX_ENTRIES;
    list->count = 0;

#ifdef DF_PLATFORM_WINDOWS
    return _df_get_windows_filesystems(list);
#else
    return _df_get_posix_filesystems(list);
#endif
}
