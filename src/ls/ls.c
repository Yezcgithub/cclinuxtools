/**
 * @file ls.c
 * @brief Cross-platform ls command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with common ls(1) implementations.
 *
 * Key design features:
 *   - Long format (-l) with permissions, owner, group, size, time
 *   - Multiple sorting options: name, size, time, extension, version
 *   - Columnar output (-C) with terminal width detection
 *   - Recursive directory listing (-R)
 *   - Colored output (--color) for different file types
 *   - File type indicators (-F, -p): /, *, @, =, |, >
 *   - Human-readable sizes (-h): 1K, 243M, 2G
 *   - Hidden file control: -a (all), -A (skip . and ..)
 *   - Cross-platform symlink handling (on POSIX systems)
 *   - POSIX-style path handling (forward slashes on all platforms)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o ls.exe ls.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o ls ls.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o ls ls.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o ls ls.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o ls ls.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o ls ls.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/ls>
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
    #define LS_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define LS_PLATFORM_LINUX   1
    #define LS_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define LS_PLATFORM_MACOS   1
    #define LS_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define LS_PLATFORM_FREEBSD 1
    #define LS_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define LS_PLATFORM_OPENBSD 1
    #define LS_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define LS_PLATFORM_NETBSD  1
    #define LS_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define LS_PLATFORM_POSIX   1
#else
    #define LS_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef LS_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef LS_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef LS_PLATFORM_NETBSD
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

#ifdef LS_PLATFORM_WINDOWS
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
    #ifndef S_ISFIFO
        #define S_ISFIFO(m) 0
    #endif
    #ifndef S_ISBLK
        #define S_ISBLK(m) 0
    #endif
    #ifndef S_ISCHR
        #define S_ISCHR(m) 0
    #endif
    #ifndef S_ISSOCK
        #define S_ISSOCK(m) 0
    #endif
    #ifndef S_IRUSR
        #define S_IRUSR _S_IREAD
    #endif
    #ifndef S_IWUSR
        #define S_IWUSR _S_IWRITE
    #endif
    #ifndef S_IXUSR
        #define S_IXUSR _S_IEXEC
    #endif
    #ifndef S_IRGRP
        #define S_IRGRP (S_IRUSR >> 3)
    #endif
    #ifndef S_IWGRP
        #define S_IWGRP (S_IWUSR >> 3)
    #endif
    #ifndef S_IXGRP
        #define S_IXGRP (S_IXUSR >> 3)
    #endif
    #ifndef S_IROTH
        #define S_IROTH (S_IRUSR >> 6)
    #endif
    #ifndef S_IWOTH
        #define S_IWOTH (S_IWUSR >> 6)
    #endif
    #ifndef S_IXOTH
        #define S_IXOTH (S_IXUSR >> 6)
    #endif
    #ifndef S_ISUID
        #define S_ISUID 04000
    #endif
    #ifndef S_ISGID
        #define S_ISGID 02000
    #endif
    #ifndef S_ISVTX
        #define S_ISVTX 01000
    #endif
    /* Windows: file attribute mapping for hidden files */
    #define LS_FILE_ATTRIBUTE_HIDDEN FILE_ATTRIBUTE_HIDDEN
    #define LS_FILE_ATTRIBUTE_SYSTEM FILE_ATTRIBUTE_SYSTEM
    #define LS_FILE_ATTRIBUTE_DIRECTORY FILE_ATTRIBUTE_DIRECTORY
    #define LS_FILE_ATTRIBUTE_REPARSE_POINT FILE_ATTRIBUTE_REPARSE_POINT
#else /* LS_PLATFORM_POSIX */
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include <utime.h>
    #include <pwd.h>
    #include <grp.h>
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <sys/time.h>
    #ifndef S_ISLNK
        #define S_ISLNK(m) (((m) & S_IFLNK) == S_IFLNK)
    #endif
    #ifndef S_ISSOCK
        #define S_ISSOCK(m) (((m) & S_IFSOCK) == S_IFSOCK)
    #endif
    /* Some strict POSIX feature-test macro settings hide the XSI permission bits
     * (S_ISUID / S_ISGID / S_ISVTX) and group/other write/exec bits.  Provide
     * fallback definitions so the build succeeds everywhere. */
    #ifndef S_ISUID
        #define S_ISUID 04000
    #endif
    #ifndef S_ISGID
        #define S_ISGID 02000
    #endif
    #ifndef S_ISVTX
        #define S_ISVTX 01000
    #endif
    #ifndef S_IRGRP
        #define S_IRGRP 00040
    #endif
    #ifndef S_IWGRP
        #define S_IWGRP 00020
    #endif
    #ifndef S_IXGRP
        #define S_IXGRP 00010
    #endif
    #ifndef S_IROTH
        #define S_IROTH 00004
    #endif
    #ifndef S_IWOTH
        #define S_IWOTH 00002
    #endif
    #ifndef S_IXOTH
        #define S_IXOTH 00001
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Program version string */
#define LS_VERSION_STR "v1.0.0"

/** @brief Maximum path buffer length (bytes) */
#define LS_MAX_PATH_LEN 4096

/** @brief Default terminal width for columnar output */
#define LS_DEFAULT_TERMINAL_WIDTH 80

/** @brief Default directory listing buffer size */
#define LS_DIR_BUF_SIZE 32

/** @brief ANSI reset color escape sequence */
#define LS_COLOR_RESET "\033[0m"

/** @brief ANSI bold attribute escape sequence */
#define LS_COLOR_BOLD "\033[1m"

/** @brief ANSI dim attribute escape sequence */
#define LS_COLOR_DIM "\033[2m"

/** @brief ANSI underline attribute escape sequence */
#define LS_COLOR_UNDERLINE "\033[4m"

/** @brief ANSI blink attribute escape sequence */
#define LS_COLOR_BLINK "\033[5m"

/** @brief ANSI reverse video escape sequence */
#define LS_COLOR_REVERSE "\033[7m"

/** @brief ANSI foreground black escape sequence */
#define LS_COLOR_BLACK "\033[30m"

/** @brief ANSI foreground red escape sequence */
#define LS_COLOR_RED "\033[31m"

/** @brief ANSI foreground green escape sequence */
#define LS_COLOR_GREEN "\033[32m"

/** @brief ANSI foreground yellow escape sequence */
#define LS_COLOR_YELLOW "\033[33m"

/** @brief ANSI foreground blue escape sequence */
#define LS_COLOR_BLUE "\033[34m"

/** @brief ANSI foreground magenta escape sequence */
#define LS_COLOR_MAGENTA "\033[35m"

/** @brief ANSI foreground cyan escape sequence */
#define LS_COLOR_CYAN "\033[36m"

/** @brief ANSI foreground white escape sequence */
#define LS_COLOR_WHITE "\033[37m"

/** @brief ANSI background black escape sequence */
#define LS_COLOR_BG_BLACK "\033[40m"

/** @brief ANSI background red escape sequence */
#define LS_COLOR_BG_RED "\033[41m"

/** @brief ANSI background green escape sequence */
#define LS_COLOR_BG_GREEN "\033[42m"

/** @brief ANSI background yellow escape sequence */
#define LS_COLOR_BG_YELLOW "\033[43m"

/** @brief ANSI background blue escape sequence */
#define LS_COLOR_BG_BLUE "\033[44m"

/** @brief ANSI background magenta escape sequence */
#define LS_COLOR_BG_MAGENTA "\033[45m"

/** @brief ANSI background cyan escape sequence */
#define LS_COLOR_BG_CYAN "\033[46m"

/** @brief ANSI background white escape sequence */
#define LS_COLOR_BG_WHITE "\033[47m"

/********************************
 *    typedefs
 ********************************/

/**
 * @brief File type enumeration for color classification
 */
typedef enum {
    LS_TYPE_FILE = 0,                /**< Regular file */
    LS_TYPE_DIR,                     /**< Directory */
    LS_TYPE_SYMLINK,                 /**< Symbolic link */
    LS_TYPE_FIFO,                    /**< FIFO/pipe */
    LS_TYPE_SOCKET,                  /**< Socket */
    LS_TYPE_BLOCK,                   /**< Block device */
    LS_TYPE_CHAR,                    /**< Character device */
    LS_TYPE_EXEC,                    /**< Executable */
    LS_TYPE_SETUID,                  /**< Setuid */
    LS_TYPE_SETGID,                  /**< Setgid */
    LS_TYPE_STICKY,                  /**< Sticky bit */
    LS_TYPE_OTHER_WRITABLE,          /**< Other-writable */
    LS_TYPE_STICKY_OTHER_WRITABLE    /**< Sticky and other-writable */
} ls_file_type_t;

/**
 * @brief Cross-platform file metadata
 */
typedef struct {
    int exists;          /**< 1 if file exists, 0 otherwise */
    int is_dir;          /**< 1 if directory */
    int is_symlink;      /**< 1 if symbolic link */
    int is_regular;      /**< 1 if regular file */
    int is_fifo;         /**< 1 if FIFO/pipe */
    int is_sock;         /**< 1 if socket */
    int is_blk;          /**< 1 if block device */
    int is_chr;          /**< 1 if character device */
    int is_exec;         /**< 1 if executable (regular file with +x) */
    int is_setuid;       /**< 1 if setuid */
    int is_setgid;       /**< 1 if setgid */
    int is_sticky;       /**< 1 if sticky bit */
    int is_other_writable; /**< 1 if other-writable */
    int is_hidden;       /**< 1 if hidden file (Windows or dotfile) */
    int64_t size;        /**< File size in bytes */
    time_t mtime;        /**< Last modification time */
    time_t atime;        /**< Last access time */
    time_t ctime;        /**< Last change time */
    mode_t mode;         /**< File mode/permissions */
    uint64_t inode;      /**< Inode number */
    uint32_t nlink;      /**< Number of hard links */
    uint64_t blocks;     /**< Number of 512-byte blocks */
    int rdev_maj;        /**< Major device number */
    int rdev_min;        /**< Minor device number */
#ifdef LS_PLATFORM_POSIX
    uid_t uid;           /**< Owner user ID (POSIX only) */
    gid_t gid;           /**< Owner group ID (POSIX only) */
    char owner_name[64]; /**< Cached owner name */
    char group_name[64]; /**< Cached group name */
    char symlink_target[LS_MAX_PATH_LEN]; /**< Symlink target */
#endif
#ifdef LS_PLATFORM_WINDOWS
    DWORD win_attr;      /**< Windows file attributes */
#endif
    char fullpath[LS_MAX_PATH_LEN]; /**< Full path to the file */
    char name[LS_MAX_PATH_LEN];     /**< Display name (base name) */
} ls_stat_t;

/**
 * @brief Dynamically growing list of directory entries
 */
typedef struct {
    ls_stat_t *entries;  /**< Array of entry stat structs */
    int count;           /**< Number of entries stored */
    int capacity;        /**< Allocated capacity of entries array */
} dir_list_t;

/**
 * @brief Filter options for directory reading
 */
typedef struct {
    int all;             /**< -a: show all including . and .. */
    int almost_all;      /**< -A: show all except . and .. */
    int follow;          /**< -L: follow symlinks */
    int follow_cli;      /**< -H: follow symlinks on command line */
    int show_hidden_win; /**< on Windows: show system/hidden files */
} ls_filter_opts_t;

/** @brief Sort mode enumeration */
typedef enum {
    LS_SORT_NAME = 0,      /**< Sort by name (default) */
    LS_SORT_SIZE,          /**< Sort by size */
    LS_SORT_TIME,          /**< Sort by modification time */
    LS_SORT_EXTENSION,     /**< Sort by extension */
    LS_SORT_VERSION,       /**< Version/natural sort */
    LS_SORT_NONE           /**< No sorting */
} ls_sort_t;

/** @brief Time field to use for display/sort */
typedef enum {
    LS_TIME_MTIME = 0,   /**< Modification time (default) */
    LS_TIME_ATIME,       /**< Access time */
    LS_TIME_CTIME        /**< Change time */
} ls_time_field_t;

/** @brief Color mode enumeration */
typedef enum {
    LS_COLOR_AUTO = 0,   /**< Color when output is a terminal */
    LS_COLOR_ALWAYS,     /**< Always colorize */
    LS_COLOR_NEVER       /**< Never colorize */
} ls_color_t;

/**
 * @brief Command-line options for ls
 */
typedef struct {
    /* Display format */
    bool long_format;       /**< -l: long format */
    bool one_per_line;      /**< -1: one per line */
    bool columnar;          /**< -C: columnar (default for tty) */
    bool across;            /**< -x: across instead of down */
    bool comma_separated;   /**< -m: comma separated list */
    bool no_group;          /**< -G: suppress group column (long) */
    bool no_owner;          /**< -g: suppress owner column (long) */
    bool numeric_ids;       /**< -n: numeric UID/GID */
    bool show_inode;        /**< -i: show inode */
    bool show_blocks;       /**< -s: show block count */
    int human_readable;     /**< -h: human-readable sizes (0/1/2) */
    bool size_bytes;        /**< -b: size in bytes (default) */

    /* Filtering */
    bool all;               /**< -a: show all including . and .. */
    bool almost_all;        /**< -A: show all except . and .. */
    bool directory_only;    /**< -d: list directory names, not contents */
    bool dereference;       /**< -L: follow symlinks */
    bool dereference_cli;   /**< -H: follow symlinks on command line only */
    int show_hidden_win;    /**< -aa or -AA on Windows: show hidden/system */

    /* Sorting */
    ls_sort_t sort_mode;        /**< Sort type */
    bool reverse;               /**< -r: reverse sort */
    bool dirs_first;            /**< --group-directories-first */
    ls_time_field_t time_field; /**< -c, -u: which time to use */

    /* Recursion */
    bool recursive;         /**< -R: recursive listing */

    /* File type indicators */
    int classify;           /**< -F: classify with indicators (0/1/2) */
    bool indicator_style_slash; /**< -p: append / to directories */

    /* Misc */
    ls_color_t color_mode;  /**< --color=when */
    int width;              /**< -w: output width, 0 = auto */
    bool ignore_backups;    /**< -B: ignore files ending with ~ */
    bool quote_names;       /**< -Q: quote names */
    bool hide_control;      /**< -q: hide control chars as ? */
    bool literal;           /**< -N: don't quote control chars */
    bool context;           /**< -Z: SELinux context (ignored stub) */

    bool tty_out;           /**< is stdout a terminal? */
    time_t current_time;    /**< current time for age-based time display */
} ls_options_t;

/**
 * @brief Column width cache for formatted output
 */
typedef struct {
    int inode_w;    /**< Inode column width */
    int blocks_w;   /**< Blocks column width */
    int mode_w;     /**< Mode column width */
    int nlink_w;    /**< Nlink column width */
    int owner_w;    /**< Owner column width */
    int group_w;    /**< Group column width */
    int size_w;     /**< Size column width */
    int time_w;     /**< Time column width */
    int name_w;     /**< max name display width */
    int total_w;    /**< max total line width for -l */
} ls_col_widths_t;

/********************************
 *    static prototypes
 ********************************/
static int          _ls_safe_copy(char * dst, const char * src, size_t dst_size);
static const char * _ls_base_name(const char * path);
static void         _ls_join_path(char * dst, size_t dst_size,
                                  const char * dir, const char * name);
static int          _ls_isatty(void);
static int          _ls_get_terminal_width(void);
static void         _ls_stat(const char * path, ls_stat_t * st, int follow);
static void         _ls_dir_list_init(dir_list_t * dl);
static void         _ls_dir_list_add(dir_list_t * dl, const ls_stat_t * entry);
static void         _ls_dir_list_free(dir_list_t * dl);
static void         _ls_read_directory(const char * path, dir_list_t * dl,
                                       const ls_filter_opts_t * fopts);
static void         _ls_print_help(void);
static void         _ls_print_version(void);
static void         _ls_format_human_size(int64_t size, char * buf, size_t bufsz, int si);
static void         _ls_format_mode(mode_t mode, char * buf);
static void         _ls_format_time(time_t t, time_t now, char * buf, size_t bufsz);
static char         _ls_classify_char(const ls_stat_t * st, int style);
static const char * _ls_get_color_for_file(const ls_stat_t * st);
static int          _ls_calc_name_width(const ls_options_t * opts, const char * name, char ind);
static int          _ls_print_name(const ls_options_t * opts, const char * name, char ind);
static int          _ls_strverscmp_custom(const char * a, const char * b);
static int          _ls_compare_entries(const void * a, const void * b);
static void         _ls_sort_entries(dir_list_t * dl, const ls_options_t * opts);
static void         _ls_calc_widths(const dir_list_t * dl, const ls_options_t * opts,
                                    ls_col_widths_t * w);
static void         _ls_display_long(const dir_list_t * dl, const ls_options_t * opts,
                                     const ls_col_widths_t * w, int show_total);
static void         _ls_display_columnar(const dir_list_t * dl, const ls_options_t * opts,
                                         const ls_col_widths_t * w);
static void         _ls_display_one(const dir_list_t * dl, const ls_options_t * opts,
                                    const ls_col_widths_t * w);
static void         _ls_display_comma(const dir_list_t * dl, const ls_options_t * opts,
                                      const ls_col_widths_t * w);
static void         _ls_display_files(const dir_list_t * dl, const ls_options_t * opts,
                                      int show_total);
static int          _ls_process_directory(const char * path, const ls_options_t * opts,
                                          int show_dirname, int print_blank_before);
static int          _ls_process_path(const char * path, const ls_options_t * opts,
                                     int show_dirname, int show_total,
                                     int print_blank_before);
static int          _ls_parse_args(int argc, char ** argv, ls_options_t * opts,
                                   char ** paths, int * npaths);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for ls_fputs / ls_putchar.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all stream output.
 */
#ifndef ls_out_stream
    #define ls_out_stream stdout
#endif

/**
 * @brief Default error stream for ls_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef ls_err_stream
    #define ls_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 *
 * Supports zero variadic arguments via ", ##__VA_ARGS__"
 * (works on GCC, Clang, MSVC, MinGW).
 */
#ifndef ls_printf
    #define ls_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr (error messages).
 *        Requires explicit format string.
 */
#ifndef ls_err_printf
    #define ls_err_printf(fmt, ...) fprintf(ls_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream (normally ls_out_stream)
 */
#ifndef ls_fputs
    #define ls_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Write a single character to a stdio stream.
 * @param ch      Character to write.
 * @param stream  stdio stream (normally ls_out_stream)
 */
#ifndef ls_fputc
    #define ls_fputc(ch, stream) (void)fputc((int)(ch), (stream))
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from unsigned char to int).
 *
 * Note: we cast to unsigned char first so values with the MSB set do
 *       not trigger undefined behavior in putchar's int argument
 *       when char is signed on the host platform.
 */
#ifndef ls_putchar
    #define ls_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Shared sort-options pointer used by the qsort comparator */
static const ls_options_t * g_sort_opts = NULL;

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the ls command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Default to current directory if no paths given
 *   3. List each path (file or directory contents)
 *   4. Recursive listing (-R) descends into subdirectories
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on listing error, 2 on bad options
 */
int main(int argc, char ** argv)
{
    ls_options_t opts;
    char * paths[1024];
    int npaths = 0;

    if (_ls_parse_args(argc, argv, &opts, paths, &npaths) != 0) {
        return 2;
    }

    int had_error = 0;
    int first = 1;

    for (int i = 0; i < npaths; i++) {
        int show_dirname;
        int need_blank;
        if (npaths == 1 && !opts.recursive) {
            show_dirname = 0;
            need_blank = 0;
        } else {
            show_dirname = 1;
            need_blank = !first;
        }
        if (_ls_process_path(paths[i], &opts, show_dirname, 1, need_blank) != 0) {
            had_error = 1;
        }
        first = 0;
    }

    return had_error ? 1 : 0;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Safer string copy that cannot trigger truncation warnings.
 *
 * Uses memcpy followed by explicit NUL termination so compilers see the
 * bounded copy as safe. Returns -1 if the destination is too small or
 * inputs are invalid; on failure dst[0] is set to '\0' when possible.
 *
 * @param dst       destination buffer
 * @param src       NUL-terminated source (may be NULL)
 * @param dst_size  size of dst in bytes
 * @return 0 on success, -1 on failure
 */
static int _ls_safe_copy(char * dst, const char * src, size_t dst_size)
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
 * @return pointer to the base name within the path, or "" on NULL
 */
static const char * _ls_base_name(const char * path)
{
    if (!path) {
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
 * @brief Join a directory path and a filename with bounds checking
 * @param dst       output buffer
 * @param dst_size  size of output buffer
 * @param dir       directory path
 * @param name      filename to append
 */
static void _ls_join_path(char * dst, size_t dst_size, const char * dir, const char * name)
{
    if (!dst || dst_size == 0 || !dir || !name) {
        return;
    }
    size_t dlen = strlen(dir);
    /* Strip trailing separator from dir */
    while (dlen > 0 && (dir[dlen - 1] == '/' || dir[dlen - 1] == '\\')) {
        dlen--;
    }
    (void)snprintf(dst, dst_size, "%.*s/%s", (int)dlen, dir, name);
}

/**
 * @brief Check if stdout is a terminal
 * @return 1 if tty, 0 otherwise
 */
static int _ls_isatty(void)
{
#ifdef LS_PLATFORM_WINDOWS
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

/**
 * @brief Get terminal width (columns)
 * @return terminal width in columns, or default if unknown
 */
static int _ls_get_terminal_width(void)
{
#ifdef LS_PLATFORM_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return LS_DEFAULT_TERMINAL_WIDTH;
#else
    struct winsize ws;
    if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return LS_DEFAULT_TERMINAL_WIDTH;
#endif
}

/**
 * @brief Get file metadata
 * @param path     path to stat
 * @param st       output metadata struct
 * @param follow   1 = follow symlinks (stat), 0 = don't follow (lstat)
 */
static void _ls_stat(const char * path, ls_stat_t * st, int follow)
{
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    if (!path) {
        return;
    }
    (void)_ls_safe_copy(st->fullpath, path, sizeof(st->fullpath));
    const char * bn = _ls_base_name(path);
    (void)_ls_safe_copy(st->name, bn, sizeof(st->name));
    if (st->name[0] == '.') {
        st->is_hidden = 1;
    }

#ifdef LS_PLATFORM_WINDOWS
    (void)follow;  /* Windows doesn't support symlink following distinction yet */
    WIN32_FILE_ATTRIBUTE_DATA fad;
    WIN32_FIND_DATAA fd;
    char native[LS_MAX_PATH_LEN];
    (void)snprintf(native, sizeof(native), "%s", path);
    for (char * p = native; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    /* Try to get find data first */
    HANDLE hFind = FindFirstFileA(native, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        st->exists = 1;
        st->win_attr = fd.dwFileAttributes;
        st->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        st->is_regular = !st->is_dir;
        st->size = ((int64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        /* Use a hash of filename+size as pseudo-inode since WIN32_FIND_DATAA
         * doesn't include nFileIndexHigh/Low on all Windows versions */
        st->inode = (uint64_t)st->size;
        st->nlink = 1;
        ULARGE_INTEGER uli;
        uli.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        st->mtime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        uli.LowPart = fd.ftLastAccessTime.dwLowDateTime;
        uli.HighPart = fd.ftLastAccessTime.dwHighDateTime;
        st->atime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        uli.LowPart = fd.ftCreationTime.dwLowDateTime;
        uli.HighPart = fd.ftCreationTime.dwHighDateTime;
        st->ctime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        st->mode = st->is_dir ? (0755 | S_IFDIR) : (0644 | S_IFREG);
        /* Check if executable based on extension */
        if (st->is_regular) {
            const char * ext = strrchr(fd.cFileName, '.');
            if (ext) {
                if (_stricmp(ext, ".exe") == 0 || _stricmp(ext, ".com") == 0 ||
                    _stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0 ||
                    _stricmp(ext, ".msi") == 0 || _stricmp(ext, ".ps1") == 0) {
                    st->is_exec = 1;
                    st->mode |= S_IXUSR | S_IXGRP | S_IXOTH;
                }
            }
        }
        st->blocks = (uint64_t)((st->size + 511) / 512);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            st->is_hidden = 1;
        }
        (void)FindClose(hFind);
    } else if (GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) {
        st->exists = 1;
        st->win_attr = fad.dwFileAttributes;
        st->is_dir = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        st->is_regular = !st->is_dir;
        st->size = ((int64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        st->mtime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        uli.LowPart = fad.ftLastAccessTime.dwLowDateTime;
        uli.HighPart = fad.ftLastAccessTime.dwHighDateTime;
        st->atime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        uli.LowPart = fad.ftCreationTime.dwLowDateTime;
        uli.HighPart = fad.ftCreationTime.dwHighDateTime;
        st->ctime = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
        st->mode = st->is_dir ? (0755 | S_IFDIR) : (0644 | S_IFREG);
        st->blocks = (uint64_t)((st->size + 511) / 512);
        if (fad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            st->is_hidden = 1;
        }
        st->nlink = 1;
        st->inode = (uint64_t)st->size;
    }
    st->rdev_maj = 0;
    st->rdev_min = 0;
#else
    struct stat sb;
    int rc = follow ? stat(path, &sb) : lstat(path, &sb);
    if (rc != 0) {
        /* try stat if lstat fails */
        if (!follow && stat(path, &sb) == 0) {
            rc = 0;
        }
    }
    if (rc == 0) {
        st->exists = 1;
        st->is_dir = S_ISDIR(sb.st_mode);
        st->is_regular = S_ISREG(sb.st_mode);
        st->is_symlink = S_ISLNK(sb.st_mode);
        st->is_fifo = S_ISFIFO(sb.st_mode);
        st->is_sock = S_ISSOCK(sb.st_mode);
        st->is_blk = S_ISBLK(sb.st_mode);
        st->is_chr = S_ISCHR(sb.st_mode);
        st->size = (int64_t)sb.st_size;
        st->mtime = sb.st_mtime;
        st->atime = sb.st_atime;
        st->ctime = sb.st_ctime;
        st->mode = sb.st_mode;
        st->inode = sb.st_ino;
        st->nlink = sb.st_nlink;
        st->blocks = sb.st_blocks;
        st->uid = sb.st_uid;
        st->gid = sb.st_gid;
        if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
#ifdef major
            st->rdev_maj = major(sb.st_rdev);
#else
            st->rdev_maj = 0;
#endif
#ifdef minor
            st->rdev_min = minor(sb.st_rdev);
#else
            st->rdev_min = 0;
#endif
        } else {
            st->rdev_maj = 0;
            st->rdev_min = 0;
        }
        /* Check setuid/setgid/sticky/other-writable */
        st->is_setuid = (sb.st_mode & S_ISUID) != 0;
        st->is_setgid = (sb.st_mode & S_ISGID) != 0;
        st->is_sticky = (sb.st_mode & S_ISVTX) != 0;
        st->is_other_writable = (sb.st_mode & S_IWOTH) != 0;
        if (st->is_regular && (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            st->is_exec = 1;
        }
        /* Cache user/group names */
        struct passwd * pw = getpwuid(sb.st_uid);
        if (pw) {
            (void)_ls_safe_copy(st->owner_name, pw->pw_name, sizeof(st->owner_name));
        } else {
            (void)snprintf(st->owner_name, sizeof(st->owner_name),
                           "%u", (unsigned)sb.st_uid);
        }
        struct group * gr = getgrgid(sb.st_gid);
        if (gr) {
            (void)_ls_safe_copy(st->group_name, gr->gr_name, sizeof(st->group_name));
        } else {
            (void)snprintf(st->group_name, sizeof(st->group_name),
                           "%u", (unsigned)sb.st_gid);
        }
        /* Read symlink target */
        if (st->is_symlink) {
            ssize_t len = readlink(path, st->symlink_target,
                                   sizeof(st->symlink_target) - 1);
            if (len > 0) {
                st->symlink_target[len] = '\0';
            }
        }
    }
#endif
}

/**
 * @brief Initialize an empty directory list
 * @param dl  directory list instance (may be NULL)
 */
static void _ls_dir_list_init(dir_list_t * dl)
{
    if (!dl) {
        return;
    }
    dl->entries = NULL;
    dl->count = 0;
    dl->capacity = 0;
}

/**
 * @brief Append an entry to the directory list (grows capacity as needed)
 * @param dl     directory list instance
 * @param entry  entry to append (copied by value)
 */
static void _ls_dir_list_add(dir_list_t * dl, const ls_stat_t * entry)
{
    if (!dl || !entry) {
        return;
    }
    if (dl->count >= dl->capacity) {
        int new_cap = dl->capacity ? dl->capacity * 2 : LS_DIR_BUF_SIZE;
        ls_stat_t * tmp = (ls_stat_t *)realloc(dl->entries,
                                               (size_t)new_cap * sizeof(ls_stat_t));
        if (!tmp) {
            return;  /* leave list unchanged on allocation failure */
        }
        dl->entries = tmp;
        dl->capacity = new_cap;
    }
    dl->entries[dl->count] = *entry;
    dl->count++;
}

/**
 * @brief Free all memory in a directory list
 * @param dl  directory list instance (may be NULL, may be called twice safely)
 */
static void _ls_dir_list_free(dir_list_t * dl)
{
    if (!dl) {
        return;
    }
    free(dl->entries);
    dl->entries = NULL;
    dl->count = 0;
    dl->capacity = 0;
}

/**
 * @brief Read directory contents into a list
 * @param path     directory path
 * @param dl       output directory list (caller must free)
 * @param fopts    options for filtering hidden files
 */
static void _ls_read_directory(const char * path, dir_list_t * dl,
                               const ls_filter_opts_t * fopts)
{
    if (!dl || !fopts || !path) {
        return;
    }
    _ls_dir_list_init(dl);

#ifdef LS_PLATFORM_WINDOWS
    char pattern[LS_MAX_PATH_LEN];
    (void)snprintf(pattern, sizeof(pattern), "%s\\*", path);
    for (char * p = pattern; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        int skip = 0;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            if (!fopts->all) {
                skip = 1;
            }
        } else {
            if (!fopts->all && !fopts->almost_all) {
                /* Skip dotfiles */
                if (fd.cFileName[0] == '.') {
                    skip = 1;
                }
                /* Skip Windows hidden/system files unless -a/-A */
                if (!skip && !fopts->show_hidden_win &&
                    (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
                    skip = 1;
                }
            } else if (fopts->almost_all) {
                /* -A: skip Windows hidden files unless double -A */
                if (!fopts->show_hidden_win &&
                    (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
                    skip = 1;
                }
            }
        }
        if (skip) {
            continue;
        }

        char fullpath[LS_MAX_PATH_LEN];
        _ls_join_path(fullpath, sizeof(fullpath), path, fd.cFileName);
        ls_stat_t st;
        _ls_stat(fullpath, &st, fopts->follow);
        if (st.exists) {
            _ls_dir_list_add(dl, &st);
        }
    } while (FindNextFileA(hFind, &fd));
    (void)FindClose(hFind);
#else
    DIR * dir = opendir(path);
    if (!dir) {
        return;
    }
    struct dirent * de;
    while ((de = readdir(dir))) {
        int skip = 0;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            if (!fopts->all) {
                skip = 1;
            }
        } else {
            if (!fopts->all && !fopts->almost_all) {
                if (de->d_name[0] == '.') {
                    skip = 1;
                }
            }
        }
        if (skip) {
            continue;
        }

        char fullpath[LS_MAX_PATH_LEN];
        _ls_join_path(fullpath, sizeof(fullpath), path, de->d_name);
        ls_stat_t st;
        _ls_stat(fullpath, &st, fopts->follow);
        if (st.exists) {
            _ls_dir_list_add(dl, &st);
        }
    }
    (void)closedir(dir);
#endif
}

/**
 * @brief Print usage/help information
 */
static void _ls_print_help(void)
{
    ls_printf(
        "Usage: ls [OPTION]... [FILE]...\n"
        "List information about the FILEs (the current directory by default).\n"
        "Sort entries alphabetically if none of -cftuvSUX nor --sort is specified.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -a, --all                  do not ignore entries starting with .\n"
        "  -A, --almost-all           do not list implied . and ..\n"
        "  -b, --escape               print C-style escapes for nongraphic characters\n"
        "  -B, --ignore-backups       do not list implied entries ending with ~\n"
        "  -c                         with -lt: sort by, and show, ctime\n"
        "  -C                         list entries by columns\n"
        "      --color[=WHEN]         colorize the output; WHEN can be 'always',\n"
        "                               'auto', or 'never'\n"
        "  -d, --directory            list directories themselves, not their contents\n"
        "  -f                         do not sort, enable -aU, disable -ls --color\n"
        "  -F, --classify             append indicator (one of */=@|) to entries\n"
        "      --file-type            likewise, except do not append '*'\n"
        "      --format=WORD          across -x, long -l, single-column -1,\n"
        "                               verbose -l, vertical -C\n"
        "  -g                         like -l, but do not list owner\n"
        "      --group-directories-first\n"
        "                             group directories before files\n"
        "  -G, --no-group             in a long listing, don't print group names\n"
        "  -h, --human-readable       with -l and -s, print sizes like 1K 234M etc.\n"
        "      --si                   likewise, but use powers of 1000 not 1024\n"
        "  -H, --dereference-command-line\n"
        "                             follow symbolic links listed on the command line\n"
        "      --indicator-style=WORD  append indicator with style WORD:\n"
        "                               none, slash (-p), file-type, classify (-F)\n"
        "  -i, --inode                print the index number of each file\n"
        "  -k, --kibibytes            default to 1024-byte blocks\n"
        "  -l                         use a long listing format\n"
        "  -L, --dereference          when showing file info for a symbolic\n"
        "                               link, show info for the referenced file\n"
        "  -m                         fill width with a comma separated list\n"
        "  -n, --numeric-uid-gid      like -l, but list numeric user and group IDs\n"
        "  -N, --literal              print entry names without quoting\n"
        "  -o                         like -l, but do not list group information\n"
        "  -p, --indicator-style=slash\n"
        "                             append / indicator to directories\n"
        "  -q, --hide-control-chars   print ? instead of nongraphic characters\n"
        "      --show-control-chars   show nongraphic characters as-is\n"
        "  -Q, --quote-name           enclose entry names in double quotes\n"
        "  -r, --reverse              reverse order while sorting\n"
        "  -R, --recursive            list subdirectories recursively\n"
        "  -s, --size                 print the allocated size of each file\n"
        "  -S                         sort by file size, largest first\n"
        "      --sort=WORD            sort by WORD instead of name: none (-U),\n"
        "                               size (-S), time (-t), version (-v),\n"
        "                               extension (-X)\n"
        "      --time=WORD            with -l, show time as WORD: atime/access/use\n"
        "                               (-u); ctime/status (-c)\n"
        "  -t                         sort by modification time, newest first\n"
        "  -u                         with -lt: sort by, and show, access time\n"
        "  -U                         do not sort; list entries in directory order\n"
        "  -v                         natural sort of (version) numbers within text\n"
        "  -w, --width=COLS           set output width to COLS. 0 means no limit\n"
        "  -x                         list entries by lines instead of by columns\n"
        "  -X                         sort alphabetically by entry extension\n"
        "  -Z, --context              print any security context of each file\n"
        "  -1                         list one file per line\n"
        "      --help        display this help and exit\n"
        "      --version     output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _ls_print_version(void)
{
    ls_printf("ls %s\n", LS_VERSION_STR);
    ls_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    ls_printf("%s", "License MIT: <https://mit-license.org/>\n");
    ls_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    ls_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Format a size to human-readable string (1K 243M 2G)
 * @param size   size in bytes
 * @param buf    output buffer
 * @param bufsz  output buffer size
 * @param si     0 = 1024 based, 1 = 1000 based
 */
static void _ls_format_human_size(int64_t size, char * buf, size_t bufsz, int si)
{
    static const char *suffixes_1024[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};
    static const char *suffixes_1000[] = {"", "k", "M", "G", "T", "P", "E", "Z", "Y"};
    uint64_t base = si ? 1000ULL : 1024ULL;
    const char **suffixes = si ? suffixes_1000 : suffixes_1024;
    uint64_t val;
    int idx = 0;
    double dval;

    if (size < 0) {
        (void)snprintf(buf, bufsz, "-");
        return;
    }
    val = (uint64_t)size;
    if (val < base) {
        (void)snprintf(buf, bufsz, "%" PRIu64, val);
        return;
    }
    dval = (double)val;
    while (dval >= (double)base && idx < 8) {
        dval /= (double)base;
        idx++;
    }
    if (dval >= 10 || (dval >= 1 && si && idx == 1)) {
        (void)snprintf(buf, bufsz, "%.0f%s", dval, suffixes[idx]);
    } else if (dval >= 1) {
        (void)snprintf(buf, bufsz, "%.1f%s", dval, suffixes[idx]);
    } else {
        (void)snprintf(buf, bufsz, "%.0f%s", dval, suffixes[idx]);
    }
}

/**
 * @brief Format mode string into 10-char buffer: "drwxr-xr-x"
 * @param mode  file mode
 * @param buf   output buffer (at least 11 chars)
 */
static void _ls_format_mode(mode_t mode, char * buf)
{
    /* File type character */
    if (S_ISDIR(mode)) {
        buf[0] = 'd';
    } else if (S_ISLNK(mode)) {
        buf[0] = 'l';
    } else if (S_ISFIFO(mode)) {
        buf[0] = 'p';
    } else if (S_ISSOCK(mode)) {
        buf[0] = 's';
    } else if (S_ISBLK(mode)) {
        buf[0] = 'b';
    } else if (S_ISCHR(mode)) {
        buf[0] = 'c';
    } else {
        buf[0] = '-';
    }

    /* Owner */
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID) {
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    } else {
        buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    }
    /* Group */
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID) {
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    } else {
        buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    }
    /* Other */
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX) {
        buf[9] = (mode & S_IXOTH) ? 't' : 'T';
    } else {
        buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    }
    buf[10] = '\0';
}

/**
 * @brief Format time in coreutils style
 *
 * Recent (< 6 months): "mmm dd hh:mm"
 * Old:                "mmm dd  yyyy"
 *
 * @param t      time to format
 * @param now    current time (for age comparison)
 * @param buf    output buffer
 * @param bufsz  output buffer size
 */
static void _ls_format_time(time_t t, time_t now, char * buf, size_t bufsz)
{
    struct tm *tm_info = localtime(&t);
    if (!tm_info) {
        (void)snprintf(buf, bufsz, "?");
        return;
    }
    /* Build parts manually to avoid %e (non-portable, not on MinGW) */
    static const char *mons[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char *month = (tm_info->tm_mon >= 0 && tm_info->tm_mon < 12)
                        ? mons[tm_info->tm_mon] : "???";
    int day = tm_info->tm_mday;
    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;
    int year = 1900 + tm_info->tm_year;

    double age_seconds = difftime(now, t);
    const double six_months = 183.0 * 86400.0;
    if (age_seconds < 0) {
        age_seconds = -age_seconds;  /* future */
    }

    if (age_seconds <= six_months) {
        (void)snprintf(buf, bufsz, "%s %2d %02d:%02d", month, day, hour, minute);
    } else {
        (void)snprintf(buf, bufsz, "%s %2d  %d", month, day, year);
    }
}

/**
 * @brief Get the classification character for a file
 * @param st     file stat
 * @param style  0=none, 1=-p (slash only), 2=-F (full), 3=--file-type (no *)
 * @return classification character, or '\0' if none
 */
static char _ls_classify_char(const ls_stat_t * st, int style)
{
    if (style == 0) {
        return '\0';
    }
    if (st->is_dir) {
        return '/';
    }
    if (style == 1) {
        return '\0';
    }
    if (st->is_symlink) {
        return '@';
    }
    if (st->is_fifo) {
        return '|';
    }
    if (st->is_sock) {
        return '=';
    }
#ifdef LS_PLATFORM_POSIX
    if (st->is_blk || st->is_chr) {
        /* Coreutils doesn't mark devices by default */
    }
#endif
    if (st->is_exec && style == 2) {
        return '*';
    }
    return '\0';
}

/**
 * @brief Get ANSI color string for a file type
 * @param st  file stat
 * @return pointer to color string (may be "")
 */
static const char * _ls_get_color_for_file(const ls_stat_t * st)
{
    /* Directory */
    if (st->is_dir) {
        if (st->is_sticky && st->is_other_writable) {
            return LS_COLOR_BOLD LS_COLOR_BLUE LS_COLOR_BG_GREEN;
        }
        if (st->is_other_writable) {
            return LS_COLOR_BOLD LS_COLOR_BLUE LS_COLOR_BG_YELLOW;
        }
        if (st->is_sticky) {
            return LS_COLOR_BOLD LS_COLOR_BLUE LS_COLOR_BG_BLACK;
        }
        return LS_COLOR_BOLD LS_COLOR_BLUE;
    }
    /* Symlink */
    if (st->is_symlink) {
        return LS_COLOR_BOLD LS_COLOR_CYAN;
    }
    /* FIFO/pipe */
    if (st->is_fifo) {
        return LS_COLOR_YELLOW LS_COLOR_BG_BLACK;
    }
    /* Socket */
    if (st->is_sock) {
        return LS_COLOR_BOLD LS_COLOR_MAGENTA;
    }
    /* Block device */
    if (st->is_blk) {
        return LS_COLOR_BOLD LS_COLOR_YELLOW LS_COLOR_BG_BLACK;
    }
    /* Character device */
    if (st->is_chr) {
        return LS_COLOR_BOLD LS_COLOR_YELLOW LS_COLOR_BG_BLACK;
    }
    /* Setuid */
    if (st->is_setuid) {
        return LS_COLOR_BOLD LS_COLOR_WHITE LS_COLOR_BG_RED;
    }
    /* Setgid */
    if (st->is_setgid) {
        return LS_COLOR_BLACK LS_COLOR_BG_YELLOW;
    }
    /* Executable (regular file) */
    if (st->is_exec) {
        return LS_COLOR_BOLD LS_COLOR_GREEN;
    }
    return "";
}

/**
 * @brief Calculate the display width of a name (column count)
 *
 * -q: non-printable -> ? (width 1)
 * -Q: quotes add 2
 *
 * @param opts  options
 * @param name  entry name
 * @param ind   indicator character ('\0' for none)
 * @return display width in columns
 */
static int _ls_calc_name_width(const ls_options_t * opts, const char * name, char ind)
{
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (opts->hide_control && *p < 0x20) {
            w++;
        } else if (*p == 0x7f && opts->hide_control) {
            w++;
        } else {
            w++;
        }
    }
    if (opts->quote_names) {
        w += 2;
    }
    if (ind) {
        w += 1;
    }
    return w;
}

/**
 * @brief Print a name with options applied, return width used
 * @param opts  options
 * @param name  entry name
 * @param ind   indicator character ('\0' for none)
 * @return number of columns printed
 */
static int _ls_print_name(const ls_options_t * opts, const char * name, char ind)
{
    int w = 0;
    if (opts->quote_names) {
        ls_putchar('"');
        w++;
    }
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        unsigned char ch = *p;
        if (opts->hide_control && (ch < 0x20 || ch == 0x7f)) {
            ls_putchar('?');
        } else {
            ls_putchar(ch);
        }
        w++;
    }
    if (opts->quote_names) {
        ls_putchar('"');
        w++;
    }
    if (ind) {
        ls_putchar(ind);
        w++;
    }
    return w;
}

/**
 * @brief Natural/version string compare
 * @param a  first string
 * @param b  second string
 * @return -1, 0, or 1
 */
static int _ls_strverscmp_custom(const char * a, const char * b)
{
    /* Simple version sort: compare numeric substrings numerically */
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            while (*a == '0') {
                a++;
            }
            while (*b == '0') {
                b++;
            }
            long na = 0, nb = 0;
            int la = 0, lb = 0;
            while (isdigit((unsigned char)*a)) {
                na = na * 10 + (*a - '0');
                a++;
                la++;
            }
            while (isdigit((unsigned char)*b)) {
                nb = nb * 10 + (*b - '0');
                b++;
                lb++;
            }
            if (na != nb) {
                return (na < nb) ? -1 : 1;
            }
            if (la != lb) {
                return (la < lb) ? -1 : 1;
            }
        } else {
            unsigned char ca = (unsigned char)*a;
            unsigned char cb = (unsigned char)*b;
            if (ca != cb) {
                return (ca < cb) ? -1 : 1;
            }
            a++;
            b++;
        }
    }
    if (*a) {
        return 1;
    }
    if (*b) {
        return -1;
    }
    return 0;
}

/**
 * @brief qsort comparator for two directory entries
 *
 * Uses the shared @ref g_sort_opts pointer for sort options.
 *
 * @param a  pointer to first ls_stat_t
 * @param b  pointer to second ls_stat_t
 * @return comparison result
 */
static int _ls_compare_entries(const void * a, const void * b)
{
    const ls_stat_t *sa = (const ls_stat_t *)a;
    const ls_stat_t *sb = (const ls_stat_t *)b;
    int cmp = 0;

    /* --group-directories-first */
    if (g_sort_opts->dirs_first) {
        int da = sa->is_dir ? 0 : 1;
        int db = sb->is_dir ? 0 : 1;
        if (da != db) {
            return da - db;
        }
    }

    switch (g_sort_opts->sort_mode) {
        case LS_SORT_SIZE:
            if (sa->size != sb->size) {
                cmp = (sa->size > sb->size) ? -1 : 1;
            }
            break;
        case LS_SORT_TIME: {
            time_t ta, tb;
            switch (g_sort_opts->time_field) {
                case LS_TIME_ATIME:
                    ta = sa->atime;
                    tb = sb->atime;
                    break;
                case LS_TIME_CTIME:
                    ta = sa->ctime;
                    tb = sb->ctime;
                    break;
                default:
                    ta = sa->mtime;
                    tb = sb->mtime;
                    break;
            }
            if (ta != tb) {
                cmp = (ta > tb) ? -1 : 1;
            }
            break;
        }
        case LS_SORT_EXTENSION: {
            const char *ea = strrchr(sa->name, '.');
            const char *eb = strrchr(sb->name, '.');
            if (!ea) {
                ea = "";
            } else {
                ea++;
            }
            if (!eb) {
                eb = "";
            } else {
                eb++;
            }
            cmp = strcmp(ea, eb);
            break;
        }
        case LS_SORT_VERSION:
            cmp = _ls_strverscmp_custom(sa->name, sb->name);
            break;
        case LS_SORT_NONE:
            cmp = 0;
            break;
        default:
            cmp = strcmp(sa->name, sb->name);
            break;
    }
    /* Name as tie-breaker (unless sort is none) */
    if (cmp == 0 && g_sort_opts->sort_mode != LS_SORT_NONE) {
        cmp = strcmp(sa->name, sb->name);
    }
    if (g_sort_opts->reverse) {
        cmp = -cmp;
    }
    return cmp;
}

/**
 * @brief Sort a directory list in-place according to options
 * @param dl    directory list (modified in place)
 * @param opts  options (sort mode, reverse, dirs-first)
 */
static void _ls_sort_entries(dir_list_t * dl, const ls_options_t * opts)
{
    if (!dl || !opts) {
        return;
    }
    if (opts->sort_mode == LS_SORT_NONE) {
        return;
    }
    g_sort_opts = opts;
    qsort(dl->entries, (size_t)dl->count, sizeof(ls_stat_t), _ls_compare_entries);
}

/**
 * @brief Calculate column widths for formatted output
 * @param dl    directory list
 * @param opts  options
 * @param w     output widths struct (zeroed and filled)
 */
static void _ls_calc_widths(const dir_list_t * dl, const ls_options_t * opts,
                            ls_col_widths_t * w)
{
    char tmp[64];
    memset(w, 0, sizeof(*w));
    w->mode_w = 10;

    for (int i = 0; i < dl->count; i++) {
        const ls_stat_t *st = &dl->entries[i];
        int len;

        if (opts->show_inode) {
            len = snprintf(tmp, sizeof(tmp), "%" PRIu64, st->inode);
            if (len > w->inode_w) {
                w->inode_w = len;
            }
        }
        if (opts->show_blocks) {
            if (opts->human_readable) {
                _ls_format_human_size((int64_t)(st->blocks * 512), tmp, sizeof(tmp), 0);
            } else {
                len = snprintf(tmp, sizeof(tmp), "%" PRIu64, (st->blocks + 1) / 2);
            }
            len = (int)strlen(tmp);
            if (len > w->blocks_w) {
                w->blocks_w = len;
            }
        }
        if (opts->long_format) {
            len = snprintf(tmp, sizeof(tmp), "%u", st->nlink);
            if (len > w->nlink_w) {
                w->nlink_w = len;
            }

#ifdef LS_PLATFORM_POSIX
            if (opts->numeric_ids) {
                len = snprintf(tmp, sizeof(tmp), "%u", (unsigned)st->uid);
            } else {
                len = (int)strlen(st->owner_name);
            }
            if (!opts->no_owner && len > w->owner_w) {
                w->owner_w = len;
            }

            if (opts->numeric_ids) {
                len = snprintf(tmp, sizeof(tmp), "%u", (unsigned)st->gid);
            } else {
                len = (int)strlen(st->group_name);
            }
            if (!opts->no_group && len > w->group_w) {
                w->group_w = len;
            }
#endif
            if (st->is_blk || st->is_chr) {
                len = snprintf(tmp, sizeof(tmp), "%d, %3d", st->rdev_maj, st->rdev_min);
            } else if (opts->human_readable) {
                _ls_format_human_size(st->size, tmp, sizeof(tmp), 0);
                len = (int)strlen(tmp);
            } else {
                len = snprintf(tmp, sizeof(tmp), "%" PRId64, st->size);
            }
            if (len > w->size_w) {
                w->size_w = len;
            }
        }
        /* Name width (for -C or -1) */
        char ind = _ls_classify_char(st,
            opts->classify ? 2 : (opts->indicator_style_slash ? 1 : 0));
        len = _ls_calc_name_width(opts, st->name, ind);
        if (len > w->name_w) {
            w->name_w = len;
        }
    }
    if (opts->show_inode) {
        w->inode_w++;
    }
    if (opts->show_blocks) {
        w->blocks_w++;
    }
    if (opts->long_format) {
        w->nlink_w++;
        if (!opts->no_owner) {
            w->owner_w++;
        }
        if (!opts->no_group) {
            w->group_w++;
        }
    }
}

/**
 * @brief Display directory list in long format (-l)
 * @param dl          directory list
 * @param opts        options
 * @param w           pre-computed column widths
 * @param show_total  if nonzero, print "total N" header
 */
static void _ls_display_long(const dir_list_t * dl, const ls_options_t * opts,
                             const ls_col_widths_t * w, int show_total)
{
    if (show_total) {
        uint64_t total_blocks = 0;
        for (int i = 0; i < dl->count; i++) {
            total_blocks += dl->entries[i].blocks;
        }
        uint64_t display = (total_blocks + 1) / 2;
        if (opts->human_readable) {
            char buf[32];
            _ls_format_human_size((int64_t)total_blocks * 512, buf, sizeof(buf), 0);
            ls_printf("total %s\n", buf);
        } else {
            ls_printf("total %" PRIu64 "\n", display);
        }
    }

    char modestr[16];
    char sizestr[64];
    char timestr[64];

    for (int i = 0; i < dl->count; i++) {
        const ls_stat_t *st = &dl->entries[i];
        int col = 0;

        if (opts->show_inode) {
            col += ls_printf("%*" PRIu64 " ", w->inode_w - 1, st->inode);
        }
        if (opts->show_blocks) {
            if (opts->human_readable) {
                _ls_format_human_size((int64_t)(st->blocks * 512), sizestr, sizeof(sizestr), 0);
                col += ls_printf("%*s ", w->blocks_w - 1, sizestr);
            } else {
                col += ls_printf("%*" PRIu64 " ", w->blocks_w - 1, (st->blocks + 1) / 2);
            }
        }
        if (opts->long_format) {
            _ls_format_mode(st->mode, modestr);
            col += ls_printf("%s ", modestr);
            col += ls_printf("%*u ", w->nlink_w - 1, st->nlink);
#ifdef LS_PLATFORM_POSIX
            if (!opts->no_owner) {
                if (opts->numeric_ids) {
                    col += ls_printf("%-*u ", w->owner_w - 1, (unsigned)st->uid);
                } else {
                    col += ls_printf("%-*s ", w->owner_w - 1, st->owner_name);
                }
            }
            if (!opts->no_group) {
                if (opts->numeric_ids) {
                    col += ls_printf("%-*u ", w->group_w - 1, (unsigned)st->gid);
                } else {
                    col += ls_printf("%-*s ", w->group_w - 1, st->group_name);
                }
            }
#else
            (void)w;
            if (!opts->no_owner) {
                col += ls_printf("%-*s ", w->owner_w > 0 ? w->owner_w - 1 : 8, "unknown");
            }
            if (!opts->no_group) {
                col += ls_printf("%-*s ", w->group_w > 0 ? w->group_w - 1 : 8, "unknown");
            }
#endif
            if (st->is_blk || st->is_chr) {
                col += ls_printf("%*d, %3d ", w->size_w >= 8 ? w->size_w - 5 : 3,
                                 st->rdev_maj, st->rdev_min);
            } else {
                if (opts->human_readable) {
                    _ls_format_human_size(st->size, sizestr, sizeof(sizestr), 0);
                    col += ls_printf("%*s ", w->size_w - 1, sizestr);
                } else {
                    col += ls_printf("%*" PRId64 " ", w->size_w - 1, st->size);
                }
            }
            time_t t;
            switch (opts->time_field) {
                case LS_TIME_ATIME:
                    t = st->atime;
                    break;
                case LS_TIME_CTIME:
                    t = st->ctime;
                    break;
                default:
                    t = st->mtime;
                    break;
            }
            _ls_format_time(t, opts->current_time, timestr, sizeof(timestr));
            col += ls_printf("%s ", timestr);
        }

        /* Color support */
        int use_color = (opts->color_mode == LS_COLOR_ALWAYS) ||
                        (opts->color_mode == LS_COLOR_AUTO && opts->tty_out);
        char ind_char = _ls_classify_char(st,
            opts->classify ? 2 : (opts->indicator_style_slash ? 1 : 0));
        const char *color = "";
        if (use_color) {
            color = _ls_get_color_for_file(st);
        }

        if (color[0]) {
            ls_fputs(color, ls_out_stream);
        }
        col += _ls_print_name(opts, st->name, ind_char);
        if (use_color && color[0]) {
            ls_fputs(LS_COLOR_RESET, ls_out_stream);
        }

        /* Symlink target */
        if (opts->long_format && st->is_symlink) {
#ifdef LS_PLATFORM_POSIX
            ls_printf(" -> ");
            if (use_color) {
                ls_stat_t target_st;
                _ls_stat(st->fullpath, &target_st, 1);
                const char *tc = "";
                if (target_st.exists) {
                    tc = _ls_get_color_for_file(&target_st);
                }
                if (tc[0]) {
                    ls_fputs(tc, ls_out_stream);
                }
                ls_fputs(st->symlink_target, ls_out_stream);
                if (tc[0]) {
                    ls_fputs(LS_COLOR_RESET, ls_out_stream);
                }
            } else {
                ls_fputs(st->symlink_target, ls_out_stream);
            }
#endif
        }
        (void)col;
        ls_putchar('\n');
    }
}

/**
 * @brief Display directory list in columns (-C / -x)
 * @param dl    directory list
 * @param opts  options
 * @param w     pre-computed column widths
 */
static void _ls_display_columnar(const dir_list_t * dl, const ls_options_t * opts,
                                 const ls_col_widths_t * w)
{
    if (dl->count == 0) {
        return;
    }
    int name_w = w->name_w;
    if (name_w == 0) {
        name_w = 1;
    }
    int col_w = name_w + 2;  /* 2 spaces between columns, coreutils style */
    int term_w = opts->width ? opts->width : _ls_get_terminal_width();

    int ncols = term_w / col_w;
    if (ncols < 1) {
        ncols = 1;
    }
    if (ncols > dl->count) {
        ncols = dl->count;
    }

    int nrows = (dl->count + ncols - 1) / ncols;

    int use_color = (opts->color_mode == LS_COLOR_ALWAYS) ||
                    (opts->color_mode == LS_COLOR_AUTO && opts->tty_out);

    for (int row = 0; row < nrows; row++) {
        for (int col = 0; col < ncols; col++) {
            int idx;
            if (opts->across) {
                idx = row * ncols + col;
            } else {
                idx = col * nrows + row;
            }
            if (idx >= dl->count) {
                continue;
            }

            const ls_stat_t *st = &dl->entries[idx];
            char ind = _ls_classify_char(st,
                opts->classify ? 2 : (opts->indicator_style_slash ? 1 : 0));

            if (opts->show_inode) {
                ls_printf("%*" PRIu64 " ", w->inode_w - 1, st->inode);
            }
            if (opts->show_blocks) {
                char bstr[32];
                if (opts->human_readable) {
                    _ls_format_human_size((int64_t)(st->blocks * 512), bstr, sizeof(bstr), 0);
                } else {
                    (void)snprintf(bstr, sizeof(bstr), "%" PRIu64, (st->blocks + 1) / 2);
                }
                ls_printf("%*s ", w->blocks_w - 1, bstr);
            }

            const char *color = "";
            if (use_color) {
                color = _ls_get_color_for_file(st);
            }
            int nw = _ls_calc_name_width(opts, st->name, ind);

            if (color[0]) {
                ls_fputs(color, ls_out_stream);
            }
            (void)_ls_print_name(opts, st->name, ind);
            if (use_color && color[0]) {
                ls_fputs(LS_COLOR_RESET, ls_out_stream);
            }

            if (col != ncols - 1) {
                int pad = col_w - nw;
                if (pad < 1) {
                    pad = 1;
                }
                for (int p = 0; p < pad; p++) {
                    ls_putchar(' ');
                }
            }
        }
        ls_putchar('\n');
    }
}

/**
 * @brief Display directory list one entry per line (-1)
 * @param dl    directory list
 * @param opts  options
 * @param w     pre-computed column widths
 */
static void _ls_display_one(const dir_list_t * dl, const ls_options_t * opts,
                            const ls_col_widths_t * w)
{
    int use_color = (opts->color_mode == LS_COLOR_ALWAYS) ||
                    (opts->color_mode == LS_COLOR_AUTO && opts->tty_out);
    for (int i = 0; i < dl->count; i++) {
        const ls_stat_t *st = &dl->entries[i];
        char ind = _ls_classify_char(st,
            opts->classify ? 2 : (opts->indicator_style_slash ? 1 : 0));

        if (opts->show_inode) {
            ls_printf("%*" PRIu64 " ", w->inode_w - 1, st->inode);
        }
        if (opts->show_blocks) {
            char bstr[32];
            if (opts->human_readable) {
                _ls_format_human_size((int64_t)(st->blocks * 512), bstr, sizeof(bstr), 0);
            } else {
                (void)snprintf(bstr, sizeof(bstr), "%" PRIu64, (st->blocks + 1) / 2);
            }
            ls_printf("%*s ", w->blocks_w - 1, bstr);
        }

        const char *color = "";
        if (use_color) {
            color = _ls_get_color_for_file(st);
        }
        if (color[0]) {
            ls_fputs(color, ls_out_stream);
        }
        (void)_ls_print_name(opts, st->name, ind);
        if (use_color && color[0]) {
            ls_fputs(LS_COLOR_RESET, ls_out_stream);
        }
        ls_putchar('\n');
    }
}

/**
 * @brief Display directory list as a comma-separated list (-m)
 * @param dl    directory list
 * @param opts  options
 * @param w     pre-computed column widths (unused)
 */
static void _ls_display_comma(const dir_list_t * dl, const ls_options_t * opts,
                              const ls_col_widths_t * w)
{
    (void)w;
    /* -m: comma separated list */
    int term_w = opts->width ? opts->width : _ls_get_terminal_width();
    int cur_w = 0;
    int use_color = (opts->color_mode == LS_COLOR_ALWAYS) ||
                    (opts->color_mode == LS_COLOR_AUTO && opts->tty_out);
    for (int i = 0; i < dl->count; i++) {
        const ls_stat_t *st = &dl->entries[i];
        char ind = _ls_classify_char(st,
            opts->classify ? 2 : (opts->indicator_style_slash ? 1 : 0));
        int nw = _ls_calc_name_width(opts, st->name, ind);
        int add = nw + 2;  /* ", " */
        if (i == dl->count - 1) {
            add = nw;
        }
        if (cur_w > 0 && cur_w + add > term_w) {
            ls_printf(",\n");
            cur_w = 0;
        } else if (i > 0) {
            ls_printf(", ");
            cur_w += 2;
        }
        const char *color = "";
        if (use_color) {
            color = _ls_get_color_for_file(st);
        }
        if (color[0]) {
            ls_fputs(color, ls_out_stream);
        }
        (void)_ls_print_name(opts, st->name, ind);
        if (use_color && color[0]) {
            ls_fputs(LS_COLOR_RESET, ls_out_stream);
        }
        cur_w += nw;
    }
    ls_putchar('\n');
}

/**
 * @brief Main display dispatcher — selects layout based on options
 * @param dl          directory list
 * @param opts        options
 * @param show_total  if nonzero, print "total N" header (long format only)
 */
static void _ls_display_files(const dir_list_t * dl, const ls_options_t * opts,
                              int show_total)
{
    ls_col_widths_t w;
    _ls_calc_widths(dl, opts, &w);

    if (opts->long_format) {
        _ls_display_long(dl, opts, &w, show_total);
    } else if (opts->comma_separated) {
        _ls_display_comma(dl, opts, &w);
    } else if (opts->one_per_line) {
        _ls_display_one(dl, opts, &w);
    } else if (opts->columnar || opts->across) {
        _ls_display_columnar(dl, opts, &w);
    } else {
        /* default for non-tty: one per line. default for tty: columnar */
        if (opts->tty_out) {
            _ls_display_columnar(dl, opts, &w);
        } else {
            _ls_display_one(dl, opts, &w);
        }
    }
}

/**
 * @brief Process a directory: read, filter, sort, display, recurse
 *
 * @param path                directory path
 * @param opts                options
 * @param show_dirname        if nonzero, print "path:" header
 * @param print_blank_before  if nonzero, print a blank line before output
 * @return 0 on success
 */
static int _ls_process_directory(const char * path, const ls_options_t * opts,
                                 int show_dirname, int print_blank_before)
{
    dir_list_t dl;
    ls_filter_opts_t fopts;
    memset(&fopts, 0, sizeof(fopts));
    fopts.all = opts->all;
    fopts.almost_all = opts->almost_all;
    fopts.follow = opts->dereference;
    fopts.show_hidden_win = opts->show_hidden_win;

    if (print_blank_before) {
        ls_putchar('\n');
    }
    if (show_dirname) {
        ls_printf("%s:\n", path);
    }

    _ls_read_directory(path, &dl, &fopts);

    /* Apply ignore-backups (-B) */
    if (opts->ignore_backups && dl.count > 0) {
        int write = 0;
        for (int i = 0; i < dl.count; i++) {
            size_t l = strlen(dl.entries[i].name);
            if (l > 0 && dl.entries[i].name[l - 1] == '~') {
                continue;
            }
            if (write != i) {
                dl.entries[write] = dl.entries[i];
            }
            write++;
        }
        dl.count = write;
    }

    _ls_sort_entries(&dl, opts);

    int need_total = (opts->long_format || opts->show_blocks) && dl.count > 0;
    _ls_display_files(&dl, opts, need_total);

    /* If recursive (-R), process subdirectories */
    if (opts->recursive) {
        /* Build a list of subdir paths to recurse into */
        dir_list_t subdirs;
        _ls_dir_list_init(&subdirs);
        for (int i = 0; i < dl.count; i++) {
            if (dl.entries[i].is_dir &&
                strcmp(dl.entries[i].name, ".") != 0 &&
                strcmp(dl.entries[i].name, "..") != 0) {
                _ls_dir_list_add(&subdirs, &dl.entries[i]);
            }
        }
        /* Sort subdirs for consistent output */
        if (subdirs.count > 0) {
            g_sort_opts = opts;
            qsort(subdirs.entries, (size_t)subdirs.count, sizeof(ls_stat_t), _ls_compare_entries);
            for (int i = 0; i < subdirs.count; i++) {
                /* Don't follow symlinks during recursion unless -L */
                if (!opts->dereference && subdirs.entries[i].is_symlink) {
                    continue;
                }
                _ls_process_directory(subdirs.entries[i].fullpath, opts, 1, 1);
            }
        }
        _ls_dir_list_free(&subdirs);
    }

    _ls_dir_list_free(&dl);
    return 0;
}

/**
 * @brief Process a single path (file or directory)
 *
 * @param path                path to list
 * @param opts                options
 * @param show_dirname        if nonzero and path is a dir, print "path:" header
 * @param show_total          unused (total handled internally)
 * @param print_blank_before  if nonzero, print a blank line before output
 * @return 0 on success, 1 on error (e.g. file not found)
 */
static int _ls_process_path(const char * path, const ls_options_t * opts,
                            int show_dirname, int show_total,
                            int print_blank_before)
{
    (void)show_total;  /* total display is handled inside display_long via count */
    ls_filter_opts_t fopts;
    memset(&fopts, 0, sizeof(fopts));
    fopts.all = opts->all;
    fopts.almost_all = opts->almost_all;
    fopts.follow = opts->dereference || opts->dereference_cli;
    fopts.show_hidden_win = opts->show_hidden_win;

    ls_stat_t st;
    _ls_stat(path, &st, fopts.follow);

    if (!st.exists) {
        ls_err_printf("ls: cannot access '%s': %s\n",
                      path, strerror(errno ? errno : ENOENT));
        return 1;
    }

    /* If -d, or it's not a directory, list as file */
    if (opts->directory_only || !st.is_dir) {
        dir_list_t dl;
        _ls_dir_list_init(&dl);
        _ls_dir_list_add(&dl, &st);
        _ls_sort_entries(&dl, opts);
        if (print_blank_before) {
            ls_putchar('\n');
        }
        _ls_display_files(&dl, opts, 0);
        _ls_dir_list_free(&dl);
        return 0;
    }

    /* Directory: list its contents */
    return _ls_process_directory(path, opts, show_dirname, print_blank_before);
}

/**
 * @brief Parse command-line arguments into options and path list
 *
 * Supports both short option clusters (-ahl) and long options
 * (--all, --color=always, --sort=time, ...).  Recognizes "--" as the
 * end-of-options marker.  If no paths are given, defaults to ".".
 *
 * @param argc   argument count (from main)
 * @param argv   argument vector (from main)
 * @param opts   output options struct (zeroed and filled)
 * @param paths  output array of path pointers (caller-allocated)
 * @param npaths output path count
 * @return 0 on success, -1 on parse error
 */
static int _ls_parse_args(int argc, char ** argv, ls_options_t * opts,
                          char ** paths, int * npaths)
{
    memset(opts, 0, sizeof(*opts));
    opts->tty_out = _ls_isatty();
    opts->current_time = time(NULL);
    if (opts->tty_out) {
        opts->columnar = 1;      /* default for tty */
        opts->hide_control = 1;
        opts->color_mode = LS_COLOR_AUTO;
    } else {
        opts->one_per_line = 1;  /* default for non-tty */
        opts->color_mode = LS_COLOR_NEVER;
    }
    *npaths = 0;

    int i = 1;
    while (i < argc) {
        char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }

        /* Long options */
        if (strncmp(arg, "--", 2) == 0) {
            char *eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[64];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';
            char *val = eq ? eq + 1 : NULL;

            if (strcmp(name, "help") == 0) {
                _ls_print_help();
                exit(0);
            } else if (strcmp(name, "version") == 0) {
                _ls_print_version();
                exit(0);
            } else if (strcmp(name, "all") == 0) {
                opts->all = true;
            } else if (strcmp(name, "almost-all") == 0) {
                opts->almost_all = true;
            } else if (strcmp(name, "escape") == 0) {
                opts->hide_control = 0;
                opts->literal = true;
            } else if (strcmp(name, "ignore-backups") == 0) {
                opts->ignore_backups = true;
            } else if (strcmp(name, "classify") == 0) {
                opts->classify = 1;
                opts->indicator_style_slash = false;
            } else if (strcmp(name, "file-type") == 0) {
                opts->classify = 2;
            } else if (strcmp(name, "directory") == 0) {
                opts->directory_only = true;
            } else if (strcmp(name, "dereference") == 0) {
                opts->dereference = true;
            } else if (strcmp(name, "dereference-command-line") == 0) {
                opts->dereference_cli = true;
            } else if (strcmp(name, "group-directories-first") == 0) {
                opts->dirs_first = true;
            } else if (strcmp(name, "no-group") == 0) {
                opts->no_group = true;
            } else if (strcmp(name, "human-readable") == 0) {
                opts->human_readable = 1;
            } else if (strcmp(name, "si") == 0) {
                opts->human_readable = 2;
            } else if (strcmp(name, "inode") == 0) {
                opts->show_inode = true;
            } else if (strcmp(name, "numeric-uid-gid") == 0) {
                opts->numeric_ids = true;
                opts->long_format = true;
            } else if (strcmp(name, "literal") == 0) {
                opts->literal = true;
                opts->hide_control = 0;
            } else if (strcmp(name, "quote-name") == 0) {
                opts->quote_names = true;
            } else if (strcmp(name, "hide-control-chars") == 0) {
                opts->hide_control = 1;
            } else if (strcmp(name, "show-control-chars") == 0) {
                opts->hide_control = 0;
            } else if (strcmp(name, "reverse") == 0) {
                opts->reverse = true;
            } else if (strcmp(name, "recursive") == 0) {
                opts->recursive = true;
            } else if (strcmp(name, "size") == 0) {
                opts->show_blocks = true;
            } else if (strcmp(name, "indicator-style") == 0) {
                if (val) {
                    if (strcmp(val, "slash") == 0) {
                        opts->indicator_style_slash = true;
                    } else if (strcmp(val, "file-type") == 0) {
                        opts->classify = 2;
                    } else if (strcmp(val, "classify") == 0) {
                        opts->classify = 1;
                    }
                }
            } else if (strcmp(name, "color") == 0) {
                if (!val) {
                    opts->color_mode = LS_COLOR_ALWAYS;
                } else if (strcmp(val, "always") == 0 ||
                           strcmp(val, "yes") == 0 ||
                           strcmp(val, "force") == 0) {
                    opts->color_mode = LS_COLOR_ALWAYS;
                } else if (strcmp(val, "never") == 0 ||
                           strcmp(val, "no") == 0 ||
                           strcmp(val, "none") == 0) {
                    opts->color_mode = LS_COLOR_NEVER;
                } else {
                    opts->color_mode = LS_COLOR_AUTO;
                }
            } else if (strcmp(name, "sort") == 0) {
                if (val) {
                    if (strcmp(val, "none") == 0) {
                        opts->sort_mode = LS_SORT_NONE;
                    } else if (strcmp(val, "size") == 0) {
                        opts->sort_mode = LS_SORT_SIZE;
                    } else if (strcmp(val, "time") == 0) {
                        opts->sort_mode = LS_SORT_TIME;
                    } else if (strcmp(val, "version") == 0) {
                        opts->sort_mode = LS_SORT_VERSION;
                    } else if (strcmp(val, "extension") == 0) {
                        opts->sort_mode = LS_SORT_EXTENSION;
                    }
                }
            } else if (strcmp(name, "time") == 0) {
                if (val) {
                    if (strcmp(val, "atime") == 0 ||
                        strcmp(val, "access") == 0 ||
                        strcmp(val, "use") == 0) {
                        opts->time_field = LS_TIME_ATIME;
                    } else if (strcmp(val, "ctime") == 0 ||
                               strcmp(val, "status") == 0) {
                        opts->time_field = LS_TIME_CTIME;
                    }
                }
            } else if (strcmp(name, "width") == 0) {
                if (val) {
                    opts->width = atoi(val);
                }
            } else if (strcmp(name, "format") == 0) {
                if (val) {
                    if (strcmp(val, "long") == 0 || strcmp(val, "verbose") == 0) {
                        opts->long_format = true;
                        opts->columnar = false;
                        opts->one_per_line = false;
                    } else if (strcmp(val, "single-column") == 0) {
                        opts->one_per_line = true;
                        opts->long_format = false;
                        opts->columnar = false;
                    } else if (strcmp(val, "across") == 0 ||
                               strcmp(val, "horizontal") == 0) {
                        opts->across = true;
                        opts->columnar = true;
                        opts->long_format = false;
                    }
                }
            } else if (strcmp(name, "full-time") == 0) {
                opts->long_format = true;
            } else if (strcmp(name, "context") == 0) {
                opts->context = true;
            } else if (strcmp(name, "kibibytes") == 0) {
                /* default already */
            } else if (strcmp(name, "tabsize") == 0) {
                /* ignore */
            } else {
                ls_err_printf("ls: unrecognized option '%s'\n", arg);
                ls_err_printf("Try 'ls --help' for more information.\n");
                return -1;
            }
            i++;
            continue;
        }

        if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options */
            for (int j = 1; arg[j]; j++) {
                switch (arg[j]) {
                    case 'a':
                        opts->all = true;
#ifdef LS_PLATFORM_WINDOWS
                        opts->show_hidden_win++;
#endif
                        break;
                    case 'A':
                        opts->almost_all = true;
#ifdef LS_PLATFORM_WINDOWS
                        opts->show_hidden_win++;
#endif
                        break;
                    case 'b':
                        opts->hide_control = 0;
                        break;
                    case 'B':
                        opts->ignore_backups = true;
                        break;
                    case 'c':
                        opts->time_field = LS_TIME_CTIME;
                        opts->sort_mode = LS_SORT_TIME;
                        break;
                    case 'C':
                        opts->columnar = true;
                        opts->long_format = false;
                        opts->one_per_line = false;
                        opts->across = false;
                        break;
                    case 'd':
                        opts->directory_only = true;
                        break;
                    case 'D':
                        /* dired mode: ignore */
                        break;
                    case 'f':
                        opts->sort_mode = LS_SORT_NONE;
                        opts->all = true;
                        opts->color_mode = LS_COLOR_NEVER;
                        opts->long_format = false;
                        opts->show_blocks = false;
                        break;
                    case 'F':
                        opts->classify = 1;
                        opts->indicator_style_slash = false;
                        break;
                    case 'g':
                        opts->no_owner = true;
                        opts->long_format = true;
                        break;
                    case 'G':
                        opts->no_group = true;
                        break;
                    case 'h':
                        opts->human_readable = 1;
                        break;
                    case 'H':
                        opts->dereference_cli = true;
                        break;
                    case 'i':
                        opts->show_inode = true;
                        break;
                    case 'I':
                        /* -I PATTERN: ignore pattern (stub, skip arg) */
                        if (arg[j + 1]) {
                            j = (int)strlen(arg);
                        } else if (i + 1 < argc) {
                            i++;
                        }
                        break;
                    case 'k':
                        /* default blocks already 1k */
                        break;
                    case 'l':
                        opts->long_format = true;
                        opts->columnar = false;
                        opts->one_per_line = false;
                        break;
                    case 'L':
                        opts->dereference = true;
                        break;
                    case 'm':
                        opts->comma_separated = true;
                        opts->one_per_line = false;
                        opts->long_format = false;
                        opts->columnar = false;
                        break;
                    case 'n':
                        opts->numeric_ids = true;
                        opts->long_format = true;
                        break;
                    case 'N':
                        opts->literal = true;
                        opts->hide_control = 0;
                        break;
                    case 'o':
                        opts->no_group = true;
                        opts->long_format = true;
                        break;
                    case 'p':
                        opts->indicator_style_slash = true;
                        break;
                    case 'q':
                        opts->hide_control = 1;
                        break;
                    case 'Q':
                        opts->quote_names = true;
                        break;
                    case 'r':
                        opts->reverse = true;
                        break;
                    case 'R':
                        opts->recursive = true;
                        break;
                    case 's':
                        opts->show_blocks = true;
                        break;
                    case 'S':
                        opts->sort_mode = LS_SORT_SIZE;
                        break;
                    case 't':
                        opts->sort_mode = LS_SORT_TIME;
                        break;
                    case 'T':
                        /* tabsize */
                        break;
                    case 'u':
                        opts->time_field = LS_TIME_ATIME;
                        opts->sort_mode = LS_SORT_TIME;
                        break;
                    case 'U':
                        opts->sort_mode = LS_SORT_NONE;
                        break;
                    case 'v':
                        opts->sort_mode = LS_SORT_VERSION;
                        break;
                    case 'w':
                        if (arg[j + 1]) {
                            opts->width = atoi(arg + j + 1);
                            j = (int)strlen(arg);
                        } else if (i + 1 < argc) {
                            opts->width = atoi(argv[i + 1]);
                            i++;
                        }
                        break;
                    case 'x':
                        opts->across = true;
                        opts->columnar = true;
                        opts->long_format = false;
                        break;
                    case 'X':
                        opts->sort_mode = LS_SORT_EXTENSION;
                        break;
                    case 'Z':
                        opts->context = true;
                        break;
                    case '1':
                        opts->one_per_line = true;
                        opts->long_format = false;
                        opts->columnar = false;
                        break;
                    default:
                        ls_err_printf("ls: invalid option -- '%c'\n", arg[j]);
                        ls_err_printf("Try 'ls --help' for more information.\n");
                        return -1;
                }
            }
        } else {
            /* Non-option argument */
            paths[(*npaths)++] = arg;
        }
        i++;
    }

    /* Collect remaining args after -- */
    while (i < argc) {
        paths[(*npaths)++] = argv[i++];
    }

    /* Default: no paths -> current dir */
    if (*npaths == 0) {
        paths[0] = ".";
        *npaths = 1;
    }

    return 0;
}