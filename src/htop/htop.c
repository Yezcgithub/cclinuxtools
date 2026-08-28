/**
 * @file htop.c
 * @brief Cross-platform htop command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU htop(1) 3.x (core feature set).
 *
 * Key features:
 *   - Per-CPU bar gauges + aggregate CPU usage line
 *   - Memory / Swap bar gauges (used / buff+cache / free segments)
 *   - Process table: PID PPID USER PRI NI VIRT RES SHR S CPU% MEM% TIME+ UPTIME COMMAND
 *   - Sort by any column (F4 / Tab / -s), reverse order (-r / r)
 *   - Tree view (F5 / -t), follow mode (F6 / --follow), wide command (w / -w)
 *   - Kernel-thread toggle (s), thread view (T / -T)
 *   - Regex filter (F3 / / / -f), custom field layout (-F / --fields)
 *   - Send signals (F7/F9 / k), change nice value (F8 / +/- / n)
 *   - Setup dialog (F2): delay, colors, cumulative, options
 *   - Batch mode (-b) for scripting; auto-batch when stdout is not a TTY
 *
 * Platform process sources:
 *   Linux:     /proc/[pid]/stat, /proc/[pid]/status, /proc/[pid]/task
 *   Windows:   CreateToolhelp32Snapshot + GetProcessTimes + GetProcessMemoryInfo
 *   macOS:     sysctl(KERN_PROC) + proc_pidinfo
 *   FreeBSD:   sysctl(KERN_PROC) + kinfo_proc
 *   OpenBSD:   sysctl(KERN_PROC2) + kinfo_proc2
 *   NetBSD:    sysctl(KERN_PROC2) + kinfo_proc2
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o htop.exe htop.c -lpsapi -ladvapi32
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o htop htop.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o htop htop.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o htop htop.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o htop htop.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o htop htop.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/htop>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons whom the Software is furnished to do so,
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
    #define HTOP_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define HTOP_PLATFORM_LINUX   1
    #define HTOP_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define HTOP_PLATFORM_MACOS   1
    #define HTOP_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define HTOP_PLATFORM_FREEBSD 1
    #define HTOP_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define HTOP_PLATFORM_OPENBSD 1
    #define HTOP_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define HTOP_PLATFORM_NETBSD  1
    #define HTOP_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define HTOP_PLATFORM_POSIX   1
#else
    #define HTOP_PLATFORM_POSIX   1
#endif

#ifdef HTOP_PLATFORM_LINUX
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef HTOP_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef HTOP_PLATFORM_NETBSD
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
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <math.h>

#ifdef HTOP_PLATFORM_WINDOWS
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #define strcasecmp _stricmp
    #include <windows.h>
    /* Ensure GetTickCount64 is declared (Vista+) */
    #if !defined(GetTickCount64)
    extern ULONGLONG WINAPI GetTickCount64(void);
    #endif
    #include <tlhelp32.h>
    #include <psapi.h>
    #include <conio.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>
    #include <errno.h>
    #include <strings.h>
    #include <dirent.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <sys/ioctl.h>
    #include <signal.h>
#endif

#ifdef HTOP_PLATFORM_POSIX
    #include <termios.h>
    #include <sys/select.h>
#endif

#ifdef HTOP_PLATFORM_LINUX
    /* /proc — no extra headers needed */
#endif

#ifdef HTOP_PLATFORM_MACOS
    #include <sys/sysctl.h>
    #include <sys/loadavg.h>
    #include <mach/mach.h>
    #include <mach/mach_init.h>
    #include <mach/mach_host.h>
    #include <mach/host_info.h>
    #include <mach/task_info.h>
    #include <mach/vm_statistics.h>
    #include <libproc.h>
#endif

#if defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
    #include <sys/sysctl.h>
    #include <sys/param.h>
    #include <sys/user.h>
    #include <sys/loadavg.h>
#endif

#ifdef HTOP_PLATFORM_FREEBSD
    #include <kvm.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define HTOP_VERSION_STR "v3.0.0"

/** @brief Maximum number of processes to track */
#define HTOP_MAX_PROCS 4096

/** @brief Maximum command length */
#define HTOP_CMD_MAX 512

/** @brief Maximum username length */
#define HTOP_USER_MAX 32

/** @brief Maximum number of PIDs in -p filter */
#define HTOP_MAX_PID_FILTER 64

/** @brief Maximum number of CPUs to display per-CPU bars */
#define HTOP_MAX_CPUS 256

/** @brief Default delay between updates (seconds) */
#define HTOP_DEFAULT_DELAY 2.0

/** @brief Maximum filter pattern length */
#define HTOP_FILTER_MAX 256

/** @brief Number of columns in the process table */
#define HTOP_NUM_COLS 13

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Column identifiers (display + sort order).
 */
typedef enum {
    HTOP_COL_PID = 0,
    HTOP_COL_PPID,
    HTOP_COL_USER,
    HTOP_COL_PRI,
    HTOP_COL_NI,
    HTOP_COL_VIRT,
    HTOP_COL_RES,
    HTOP_COL_SHR,
    HTOP_COL_S,
    HTOP_COL_CPU,
    HTOP_COL_MEM,
    HTOP_COL_TIME,
    HTOP_COL_CMD,
    HTOP_NUM_COLUMNS
} htop_col_t;

/**
 * @brief Per-CPU usage sample (cumulative jiffies since boot).
 */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} htop_cpu_ticks_t;

/**
 * @brief System-wide information.
 */
typedef struct {
    double  uptime;
    double  load1, load5, load15;
    int     tasks_total, tasks_running, tasks_sleeping, tasks_stopped, tasks_zombie;
    /* aggregated CPU ticks */
    htop_cpu_ticks_t cpu;
    /* per-CPU ticks */
    int            num_cpus;
    htop_cpu_ticks_t cpu_per[HTOP_MAX_CPUS];
    /* memory (bytes) */
    unsigned long long mem_total, mem_free, mem_used, mem_shared, mem_buff, mem_cache, mem_available;
    unsigned long long swap_total, swap_free, swap_used;
} htop_sys_t;

/**
 * @brief Per-process information.
 */
typedef struct {
    int             pid;
    int             ppid;
    char            user[HTOP_USER_MAX];
    unsigned long   virt;           /* virtual memory in KiB */
    unsigned long   res;            /* resident memory in KiB */
    unsigned long   shr;            /* shared memory in KiB */
    int             priority;
    int             nice;
    char            state;          /* R/S/D/Z/T/I/W/P */
    unsigned long   num_threads;
    double          cpu_pct;        /* computed per-interval */
    double          mem_pct;        /* computed per-interval */
    unsigned long long cpu_time;    /* raw CPU time (ticks or 100ns units) */
    unsigned long long starttime;   /* process start time (jiffies or ticks) */
    char            command[HTOP_CMD_MAX];
    char            cmdline[HTOP_CMD_MAX]; /* full command line (optional) */
    bool            is_thread;      /* entry is a kernel thread (T view) */
} htop_proc_t;

/**
 * @brief Parsed command-line options.
 */
typedef struct {
    int         batch;              /* -b */
    double      delay;              /* -d */
    int         iterations;         /* -n (0 = infinite) */
    htop_col_t  sort_col;           /* -s */
    int         reverse_sort;       /* -r */
    int         show_threads;       /* -T (threads as processes) */
    int         show_kthreads;      /* -t/-s: kernel threads */
    int         tree_view;          /* -t */
    int         follow_pid;         /* --follow=PID */
    int         wide_cmd;           /* -w */
    int         pid_filter[HTOP_MAX_PID_FILTER];
    int         pid_filter_count;
    int         pid_filter_set;
    char        filter[HTOP_FILTER_MAX]; /* -f */
    int         filter_set;
    char        user_filter[HTOP_USER_MAX]; /* -u */
    int         user_filter_set;
    int         no_color;           /* --no-color */
    int         hide_header;        /* -n header suppression stub */
    int         interactive_off;    /* -i */
    int         show_cumulative;    /* --show-cumulative */
    int         show_path;          /* --show-program-path */
    int         width;              /* -W (force width) */
    int         limit_rows;         /* --limit-rows=N */
    int         delay_set;
} htop_opts_t;

/**
 * @brief Runtime UI state (interactive only).
 */
typedef struct {
    htop_col_t  sort_col;
    int         reverse;
    int         tree_view;
    int         show_kthreads;
    int         show_threads;
    int         wide_cmd;
    int         show_path;
    char        filter[HTOP_FILTER_MAX];
    int         filter_set;
    int         follow_pid;
    double      delay;
    int         selected;           /* cursor row index */
    int         color_cpu;          /* 0..4 gauge color */
    int         color_mem;          /* 0..4 gauge color */
    int         color_swap;         /* 0..4 gauge color */
} htop_ui_t;

/********************************
 *    static prototypes
 ********************************/

/* ---- platform system info ---- */
static int  _htop_get_sys_info(htop_sys_t * info);
#ifdef HTOP_PLATFORM_LINUX
static int  _htop_get_sys_info_linux(htop_sys_t * info);
#endif
#ifdef HTOP_PLATFORM_WINDOWS
static int  _htop_get_sys_info_windows(htop_sys_t * info);
#endif
#ifdef HTOP_PLATFORM_MACOS
static int  _htop_get_sys_info_macos(htop_sys_t * info);
#endif
#if defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
static int  _htop_get_sys_info_bsd(htop_sys_t * info);
#endif

/* ---- platform process list ---- */
static int  _htop_get_procs(htop_proc_t * procs, int max_procs, int threads);
#ifdef HTOP_PLATFORM_LINUX
static int  _htop_get_procs_linux(htop_proc_t * procs, int max_procs, int threads);
#endif
#ifdef HTOP_PLATFORM_WINDOWS
static int  _htop_get_procs_windows(htop_proc_t * procs, int max_procs);
static void _htop_get_user_windows(int pid, char * buf, size_t buf_size);
#endif
#ifdef HTOP_PLATFORM_MACOS
static int  _htop_get_procs_macos(htop_proc_t * procs, int max_procs);
#endif
#if defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
static int  _htop_get_procs_bsd(htop_proc_t * procs, int max_procs);
#endif

/* ---- regex filter (ERE subset) ----
 *
 * Supported syntax (per branch, branches split by top-level '|'):
 *   literal chars, '.', [] classes ([a-z], [^a-z], \s \d \w \S \D \W),
 *   '*' '+' '?' quantifiers on the preceding char/class, ^ and $ anchors.
 *
 * Matching is a case-insensitive search (any start position, like htop).
 */
#define HTOP_RE_MAX_BRANCH  8
#define HTOP_RE_MAX_UNITS   32
#define HTOP_RE_MAX_CLASS   16
#define HTOP_RE_BRANCH_LEN  64

#define HTOP_RE_U_CHAR  0
#define HTOP_RE_U_CLASS 1
#define HTOP_RE_U_ANY   2

#define HTOP_RE_Q_NONE 0
#define HTOP_RE_Q_STAR 1
#define HTOP_RE_Q_PLUS 2
#define HTOP_RE_Q_OPT  3

typedef struct {
    int kind;   /* HTOP_RE_U_* */
    int data;   /* char value or class id */
    int quant;  /* HTOP_RE_Q_* */
} htop_re_unit_t;

typedef struct {
    htop_re_unit_t u[HTOP_RE_MAX_UNITS];
    int            n;
    int            anchor_s;   /* pattern starts with ^ */
    int            anchor_e;   /* pattern ends with $ */
} htop_re_branch_t;

typedef struct {
    int         ok;               /* 1 if pattern compiled */
    const char * source;          /* original pattern (borrowed) */
    const char * error;
    htop_re_branch_t branch[HTOP_RE_MAX_BRANCH];
    int         nbranch;
    /* character classes: bitmap of 128 bytes each */
    unsigned char cls[HTOP_RE_MAX_CLASS][128];
    int         ncls;
} htop_re_t;

static int  _htop_re_compile(htop_re_t * re, const char * pattern);
static void _htop_re_free(htop_re_t * re);
static bool _htop_re_match(const htop_re_t * re, const char * text);

/* ---- sorting ---- */
static int  _htop_sort_cmp(const void * a, const void * b);
static void _htop_sort_procs(htop_proc_t * procs, int count,
                             htop_col_t col, int reverse);

/* ---- formatting ---- */
static void _htop_format_mem(unsigned long kib, char * buf, size_t buf_size);
static void _htop_format_time(unsigned long seconds, char * buf, size_t buf_size);
static void _htop_format_uptime(double uptime, char * buf, size_t buf_size);

/* ---- rendering ---- */
static void _htop_render_bar(char * buf, size_t buf_size, double frac,
                             int width, const char * label, const char * extra);
static void _htop_render_header(const htop_sys_t * cur, const htop_sys_t * prev,
                                int has_prev, const htop_ui_t * ui, int width);
static void _htop_render_col_header(const htop_ui_t * ui, int width);
static void _htop_render_row(const htop_proc_t * p, const htop_sys_t * sys,
                             const htop_ui_t * ui, int width, int selected);

/* ---- interactive helpers ---- */
static int  _htop_kbhit(void);
static int  _htop_getch(void);
static void _htop_set_raw_mode(int enable);
static void _htop_enable_vt(void);
static void _htop_hide_cursor(void);
static void _htop_show_cursor(void);
static int  _htop_terminal_size(int * rows, int * cols);
static int  _htop_is_tty(void);

/* ---- dialogs / actions ---- */
static int  _htop_dialog_int(const char * title, const char * prompt, int def);
static int  _htop_dialog_choice(const char * title, const char * prompt,
                                const char * const * choices, int n, int def);
static int  _htop_dialog_text(const char * title, const char * prompt,
                              char * buf, size_t buf_size);
static int  _htop_send_signal(int pid, int sig);
static int  _htop_set_nice(int pid, int nice_val);
static void _htop_run_setup(htop_ui_t * ui);

/* ---- utility ---- */
static void   _htop_sleep_ms(unsigned long ms);
static unsigned long long _htop_time_now_ms(void);
static int    _htop_parse_int(const char * s, int def);
static double _htop_parse_delay(const char * s, double def);
static htop_col_t _htop_parse_col(const char * s);
static const char * _htop_col_name(htop_col_t col);
static const char * _htop_signal_name(int sig);
static bool   _htop_proc_visible(const htop_proc_t * p, const htop_ui_t * ui,
                                 const htop_opts_t * opts, const htop_re_t * re);

/* ---- help/version/args ---- */
static void _htop_print_help(void);
static void _htop_print_version(void);
static int  _htop_parse_args(int argc, char ** argv, htop_opts_t * opts);

/********************************
 *    static variables
 ********************************/

/* column header names (display order) */
static const char * htop_col_names[] = {
    "PID",   "PPID",  "USER",  "PRI", "NI",
    "VIRT",  "RES",   "SHR",   "S",   "%CPU",
    "%MEM",  "TIME+", "COMMAND"
};

/* sort field used by qsort — set before each sort */
static htop_col_t g_htop_sort_col = HTOP_COL_CPU;
static int        g_htop_reverse  = 0;

/* CPU usage colors: fraction ranges map to bar colors.
 * All color sequences go through these globals so that
 * --no-color can blank them out without touching the
 * rendering code. */
static const char * htop_cpu_colors[5] = {
    "\033[38;5;243m", /* light gray  (idle-ish)  */
    "\033[38;5;46m",  /* green        (< 40%)    */
    "\033[38;5;208m", /* orange       (40-70%)   */
    "\033[38;5;160m", /* dark orange  (70-90%)   */
    "\033[38;5;196m"  /* red          (>= 90%)   */
};

static const char * g_htop_c_reset = "\033[0m";
static const char * g_htop_c_bold  = "\033[1m";
static const char * g_htop_c_rev   = "\033[7m";
static const char * g_htop_c_grn   = "\033[32m";
static const char * g_htop_c_blu   = "\033[34m";
static const char * g_htop_c_red   = "\033[31m";
static const char * g_htop_c_yel   = "\033[33m";

#define HTOP_RESET_COLOR g_htop_c_reset

/**
 * @brief Blank all ANSI color sequences (--no-color / non-TTY batch).
 */
static void _htop_disable_colors(void)
{
    g_htop_c_reset = "";
    g_htop_c_bold  = "";
    g_htop_c_rev   = "";
    g_htop_c_grn   = "";
    g_htop_c_blu   = "";
    g_htop_c_red   = "";
    g_htop_c_yel   = "";
    htop_cpu_colors[0] = "";
    htop_cpu_colors[1] = "";
    htop_cpu_colors[2] = "";
    htop_cpu_colors[3] = "";
    htop_cpu_colors[4] = "";
}

/********************************
 *    macros
 ********************************/

#ifndef htop_printf
    #define htop_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef htop_err_printf
    #define htop_err_printf(fmt, ...) \
        do { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } while (0)
#endif

#ifndef htop_fflush
    #define htop_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    global
 ********************************/

/* No additional global state beyond the sort fields above. */

/********************************
 *    static functions
 ********************************/

/* ---- platform system info ---- */

static int _htop_get_sys_info(htop_sys_t * info)
{
    if (!info) {
        return -1;
    }
    memset(info, 0, sizeof(*info));

#ifdef HTOP_PLATFORM_LINUX
    return _htop_get_sys_info_linux(info);
#elif defined(HTOP_PLATFORM_WINDOWS)
    return _htop_get_sys_info_windows(info);
#elif defined(HTOP_PLATFORM_MACOS)
    return _htop_get_sys_info_macos(info);
#elif defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
    return _htop_get_sys_info_bsd(info);
#else
    /* generic POSIX fallback — minimal */
    info->num_cpus = 1;
    return 0;
#endif
}

#ifdef HTOP_PLATFORM_LINUX
/**
 * @brief Linux: read system info from /proc.
 */
static int _htop_get_sys_info_linux(htop_sys_t * info)
{
    if (!info) {
        return -1;
    }

    /* uptime */
    FILE * fp = fopen("/proc/uptime", "r");
    if (fp) {
        double up = 0, idle = 0;
        if (fscanf(fp, "%lf %lf", &up, &idle) >= 1) {
            info->uptime = up;
        }
        fclose(fp);
    }

    /* load average */
    fp = fopen("/proc/loadavg", "r");
    if (fp) {
        if (fscanf(fp, "%lf %lf %lf",
                   &info->load1, &info->load5, &info->load15) >= 3) {
        }
        fclose(fp);
    }

    /* CPU stats from /proc/stat — aggregate + per-CPU lines */
    fp = fopen("/proc/stat", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long long v[8];
            int is_agg = (strncmp(line, "cpu ", 4) == 0);
            int cpu_id = -1;

            if (!is_agg && strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
                cpu_id = atoi(line + 3);
            }
            if (!is_agg && cpu_id < 0) {
                continue;
            }

            int n = 0;
            {
                const char * p = is_agg ? line + 4 : line + 3;
                while (*p == ' ') {
                    p++;
                }
                n = sscanf(p, "%llu %llu %llu %llu %llu %llu %llu %llu",
                           &v[0], &v[1], &v[2], &v[3],
                           &v[4], &v[5], &v[6], &v[7]);
            }
            if (n < 4) {
                continue;
            }

            htop_cpu_ticks_t * t;
            if (is_agg) {
                t = &info->cpu;
            }
            else {
                if (cpu_id >= HTOP_MAX_CPUS) {
                    continue;
                }
                t = &info->cpu_per[cpu_id];
                if (cpu_id + 1 > info->num_cpus) {
                    info->num_cpus = cpu_id + 1;
                }
            }

            t->user    = v[0];
            t->nice    = v[1];
            t->system  = v[2];
            t->idle    = v[3];
            if (n >= 5) t->iowait  = v[4];
            if (n >= 6) t->irq     = v[5];
            if (n >= 7) t->softirq = v[6];
            if (n >= 8) t->steal   = v[7];
        }
        fclose(fp);
    }

    /* number of CPUs */
    info->num_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (info->num_cpus <= 0) {
        info->num_cpus = 1;
    }
    if (info->num_cpus > HTOP_MAX_CPUS) {
        info->num_cpus = HTOP_MAX_CPUS;
    }

    /* memory from /proc/meminfo */
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        unsigned long long val;
        char key[64];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "%63s %llu", key, &val) == 2) {
                if (strcmp(key, "MemTotal:") == 0)
                    info->mem_total = val * 1024;
                else if (strcmp(key, "MemFree:") == 0)
                    info->mem_free = val * 1024;
                else if (strcmp(key, "MemAvailable:") == 0)
                    info->mem_available = val * 1024;
                else if (strcmp(key, "Buffers:") == 0)
                    info->mem_buff = val * 1024;
                else if (strcmp(key, "Cached:") == 0)
                    info->mem_cache = val * 1024;
                else if (strcmp(key, "Shmem:") == 0)
                    info->mem_shared = val * 1024;
                else if (strcmp(key, "SwapTotal:") == 0)
                    info->swap_total = val * 1024;
                else if (strcmp(key, "SwapFree:") == 0)
                    info->swap_free = val * 1024;
            }
        }
        fclose(fp);
    }

    info->mem_used = info->mem_total - info->mem_free -
                     info->mem_buff - info->mem_cache;
    if (info->mem_used > info->mem_total) {
        info->mem_used = 0;
    }
    if (info->mem_available == 0) {
        info->mem_available = info->mem_free + info->mem_buff + info->mem_cache;
    }
    info->swap_used = info->swap_total - info->swap_free;

    /* task counts — count /proc entries and classify by state */
    {
        DIR * dir = opendir("/proc");
        if (dir) {
            struct dirent * ent;
            int total = 0, running = 0, sleeping = 0, stopped = 0, zombie = 0;
            while ((ent = readdir(dir)) != NULL) {
                char * p = ent->d_name;
                int is_pid = 1;
                while (*p) {
                    if (!isdigit((unsigned char)*p)) {
                        is_pid = 0;
                        break;
                    }
                    p++;
                }
                if (!is_pid) {
                    continue;
                }
                total++;
                char path[64];
                snprintf(path, sizeof(path), "/proc/%s/stat", ent->d_name);
                FILE * pf = fopen(path, "r");
                if (pf) {
                    char buf[512];
                    if (fgets(buf, sizeof(buf), pf)) {
                        char * rp = strrchr(buf, ')');
                        if (rp && rp[1] == ' ' && rp[2]) {
                            char st = rp[2];
                            switch (st) {
                                case 'R': running++; break;
                                case 'S':
                                case 'D':
                                case 'I': sleeping++; break;
                                case 'T': stopped++; break;
                                case 'Z': zombie++; break;
                                default: sleeping++; break;
                            }
                        }
                    }
                    fclose(pf);
                }
            }
            closedir(dir);
            info->tasks_total = total;
            info->tasks_running = running;
            info->tasks_sleeping = sleeping;
            info->tasks_stopped = stopped;
            info->tasks_zombie = zombie;
        }
    }

    return 0;
}
#endif /* HTOP_PLATFORM_LINUX */

#ifdef HTOP_PLATFORM_WINDOWS
/**
 * @brief Windows: system info via Win32 APIs.
 */
static int _htop_get_sys_info_windows(htop_sys_t * info)
{
    if (!info) {
        return -1;
    }

    /* uptime */
    info->uptime = (double)GetTickCount64() / 1000.0;

    /* load average: not available on Windows */
    info->load1 = info->load5 = info->load15 = 0.0;

    /* number of CPUs */
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->num_cpus = (int)si.dwNumberOfProcessors;
    if (info->num_cpus <= 0) {
        info->num_cpus = 1;
    }
    if (info->num_cpus > HTOP_MAX_CPUS) {
        info->num_cpus = HTOP_MAX_CPUS;
    }

    /* total CPU stats via GetSystemTimes */
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULARGE_INTEGER id, ke, us;
        id.LowPart = idle.dwLowDateTime;   id.HighPart = idle.dwHighDateTime;
        ke.LowPart = kernel.dwLowDateTime; ke.HighPart = kernel.dwHighDateTime;
        us.LowPart = user.dwLowDateTime;   us.HighPart = user.dwHighDateTime;
        info->cpu.idle   = id.QuadPart;
        info->cpu.system = (ke.QuadPart > id.QuadPart)
                           ? (ke.QuadPart - id.QuadPart) : 0;
        info->cpu.user   = us.QuadPart;
    }

    /* per-CPU stats via GetSystemTimes is not available;
     * GetPerfDistribution is too complex — leave per-CPU at 0.
     * htop will then fall back to a single aggregate bar. */

    /* memory */
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        info->mem_total = ms.ullTotalPhys;
        info->mem_free = ms.ullAvailPhys;
        info->mem_available = ms.ullAvailPhys;
        info->mem_used = info->mem_total - info->mem_free;

        /* swap from page file: commit limit minus physical RAM */
        if (ms.ullTotalPageFile > ms.ullTotalPhys) {
            info->swap_total = ms.ullTotalPageFile - ms.ullTotalPhys;
        }
        if (ms.ullAvailPageFile > ms.ullAvailPhys) {
            info->swap_free = ms.ullAvailPageFile - ms.ullAvailPhys;
        }
        if (info->swap_free > info->swap_total) {
            info->swap_free = info->swap_total;
        }
        info->swap_used = info->swap_total - info->swap_free;
    }

    /* system cache from GetPerformanceInfo */
    PERFORMANCE_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi)) && pi.PageSize > 0) {
        info->mem_cache = (unsigned long long)pi.SystemCache *
                         (unsigned long long)pi.PageSize;
    }
    info->mem_buff = 0;
    info->mem_shared = 0;

    /* count processes and threads */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        memset(&pe, 0, sizeof(pe));
        pe.dwSize = sizeof(pe);
        int total = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                total++;
            } while (Process32NextW(snap, &pe));
        }
        info->tasks_total = total;
        info->tasks_running = total; /* Windows processes are all "running" */
        info->tasks_sleeping = 0;
        info->tasks_stopped = 0;
        info->tasks_zombie = 0;
        CloseHandle(snap);
    }

    return 0;
}
#endif /* HTOP_PLATFORM_WINDOWS */

#ifdef HTOP_PLATFORM_MACOS
/**
 * @brief macOS: system info via sysctl and mach.
 */
static int _htop_get_sys_info_macos(htop_sys_t * info)
{
    if (!info) {
        return -1;
    }

    /* uptime */
    struct timeval boottime;
    size_t btlen = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &btlen, NULL, 0) == 0) {
        info->uptime = (double)(time(NULL) - boottime.tv_sec);
        if (info->uptime < 0) info->uptime = 0;
    }

    /* load average */
    double avg[3];
    if (getloadavg(avg, 3) >= 3) {
        info->load1 = avg[0];
        info->load5 = avg[1];
        info->load15 = avg[2];
    }

    /* number of CPUs */
    int ncpu = 0;
    size_t ncpu_len = sizeof(ncpu);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &ncpu, &ncpu_len, NULL, 0) == 0 && ncpu > 0) {
        info->num_cpus = ncpu;
    } else {
        info->num_cpus = 1;
    }

    /* memory */
    uint64_t memsize = 0;
    size_t mslen = sizeof(memsize);
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    if (sysctl(mib, 2, &memsize, &mslen, NULL, 0) == 0) {
        info->mem_total = memsize;
    }

    /* vm stats via mach */
    vm_statistics64_data_t vmstat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
        unsigned long long pgsize = vm_kernel_page_size;
        info->mem_free = (unsigned long long)vmstat.free_count * pgsize;
        info->mem_used = info->mem_total - info->mem_free;
        info->mem_available = info->mem_free;
        info->mem_buff = 0;
        info->mem_cache = (unsigned long long)vmstat.wire_count * pgsize;
    }

    /* CPU stats via host_processor_info */
    processor_cpu_load_info_t cpu_load;
    mach_msg_type_number_t cpu_count;
    kern_return_t kr = host_processor_info(mach_host_self(),
                                            PROCESSOR_CPU_LOAD_INFO,
                                            &cpu_count,
                                            (processor_info_array_t *)&cpu_load,
                                            &cpu_count);
    if (kr == KERN_SUCCESS) {
        for (unsigned i = 0; i < cpu_count && i < HTOP_MAX_CPUS; i++) {
            htop_cpu_ticks_t * t = &info->cpu_per[i];
            t->user   = (unsigned long long)cpu_load[i].cpu_ticks[CPU_STATE_USER];
            t->nice   = (unsigned long long)cpu_load[i].cpu_ticks[CPU_STATE_NICE];
            t->system = (unsigned long long)cpu_load[i].cpu_ticks[CPU_STATE_SYSTEM];
            t->idle   = (unsigned long long)cpu_load[i].cpu_ticks[CPU_STATE_IDLE];
            info->cpu.user   += t->user;
            info->cpu.nice   += t->nice;
            info->cpu.system += t->system;
            info->cpu.idle   += t->idle;
        }
        if (cpu_count > 0) {
            info->num_cpus = (int)cpu_count;
        }
        vm_deallocate(mach_task_self(), (vm_address_t)cpu_load,
                      cpu_count * sizeof(*cpu_load));
    }

    /* process count via sysctl */
    int pmib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t plen = 0;
    if (sysctl(pmib, 3, NULL, &plen, NULL, 0) == 0) {
        info->tasks_total = (int)(plen / sizeof(struct kinfo_proc));
    }

    return 0;
}
#endif /* HTOP_PLATFORM_MACOS */

#if defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
/**
 * @brief BSD: system info via sysctl.
 */
static int _htop_get_sys_info_bsd(htop_sys_t * info)
{
    if (!info) {
        return -1;
    }

    /* uptime */
    struct timeval boottime;
    size_t btlen = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &btlen, NULL, 0) == 0) {
        info->uptime = (double)(time(NULL) - boottime.tv_sec);
        if (info->uptime < 0) info->uptime = 0;
    }

    /* load average */
    double avg[3];
    if (getloadavg(avg, 3) >= 3) {
        info->load1 = avg[0];
        info->load5 = avg[1];
        info->load15 = avg[2];
    }

    /* number of CPUs */
    int ncpu = 1;
    size_t ncpu_len = sizeof(ncpu);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &ncpu, &ncpu_len, NULL, 0);
    info->num_cpus = (ncpu > 0) ? ncpu : 1;

    /* memory */
    unsigned long physmem = 0;
    size_t prlen = sizeof(physmem);
    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;
    sysctl(mib, 2, &physmem, &prlen, NULL, 0);
    info->mem_total = (unsigned long long)physmem;

    /* memory statistics via vm.stats.sys.v_* (FreeBSD/OpenBSD/NetBSD) */
    {
        int mmib[6];
        unsigned long v = 0;
        size_t vlen = sizeof(v);

#ifdef HTOP_PLATFORM_NETBSD
        /* NetBSD: no portable single-shot; leave free/used approximate */
        info->mem_free = 0;
        info->mem_used = info->mem_total;
#else
        mmib[0] = CTL_VM;
        mmib[1] = VM_SABOTH;
        /* fall back to counting via sysctl vm.stats if available */
#endif
        (void)mmib; (void)v; (void)vlen;
    }

    if (info->mem_free == 0) {
        /* conservative estimate without vm stats */
        info->mem_free = 0;
        info->mem_used = info->mem_total;
    }

    /* process count */
    int pmib[4];
    pmib[0] = CTL_KERN;
    pmib[1] = KERN_PROC;
    pmib[2] = KERN_PROC_ALL;
    pmib[3] = 0;
    size_t plen = 0;
    if (sysctl(pmib, 3, NULL, &plen, NULL, 0) == 0) {
        info->tasks_total = (int)(plen / sizeof(struct kinfo_proc));
    }

    return 0;
}
#endif /* BSD */

/* ---- platform process list ---- */

static int _htop_get_procs(htop_proc_t * procs, int max_procs, int threads)
{
    if (!procs || max_procs <= 0) {
        return 0;
    }
    (void)threads;

#ifdef HTOP_PLATFORM_LINUX
    return _htop_get_procs_linux(procs, max_procs, threads);
#elif defined(HTOP_PLATFORM_WINDOWS)
    return _htop_get_procs_windows(procs, max_procs);
#elif defined(HTOP_PLATFORM_MACOS)
    return _htop_get_procs_macos(procs, max_procs);
#elif defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
    return _htop_get_procs_bsd(procs, max_procs);
#else
    return 0;
#endif
}

#ifdef HTOP_PLATFORM_LINUX
/**
 * @brief Linux: enumerate processes from /proc.
 *
 * When @p threads is non-zero, one entry per thread is emitted
 * (via /proc/[pid]/task/[tid]) and the entry's pid is the TID
 * with ppid set to the thread-group leader.
 */
static int _htop_get_procs_linux(htop_proc_t * procs, int max_procs, int threads)
{
    DIR * dir = opendir("/proc");
    if (!dir) {
        return 0;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    int count = 0;
    struct dirent * ent;

    while ((ent = readdir(dir)) != NULL && count < max_procs) {
        /* only numeric directory names are PIDs */
        char * p = ent->d_name;
        int is_pid = 1;
        while (*p) {
            if (!isdigit((unsigned char)*p)) {
                is_pid = 0;
                break;
            }
            p++;
        }
        if (!is_pid) {
            continue;
        }

        int pid = atoi(ent->d_name);
        if (pid <= 0) {
            continue;
        }

        htop_proc_t pi;
        memset(&pi, 0, sizeof(pi));
        pi.pid = pid;

        /* read /proc/[pid]/stat */
        char path[64];
        char buf[1024];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE * fp = fopen(path, "r");
        if (!fp) {
            continue;
        }
        if (!fgets(buf, sizeof(buf), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);

        /* parse: pid (comm) state ppid ... utime stime ... */
        char * comm_start = strchr(buf, '(');
        char * comm_end = strrchr(buf, ')');
        if (comm_start && comm_end && comm_end > comm_start) {
            size_t clen = (size_t)(comm_end - comm_start - 1);
            if (clen >= sizeof(pi.command)) {
                clen = sizeof(pi.command) - 1;
            }
            memcpy(pi.command, comm_start + 1, clen);
            pi.command[clen] = '\0';
        }

        int ppid = 0;
        unsigned long utime = 0, stime = 0;
        int priority = 0, nice_val = 0;
        unsigned long vsize = 0;
        long rss_pages = 0;
        unsigned long num_threads = 0;
        unsigned long long starttime = 0;

        if (comm_end) {
            /* fields after ") ":
             * state ppid pgrp session tty tpgid flags minflt cminflt
             * majflt cmajflt utime stime cutime cstime priority nice
             * num_threads itrealvalue starttime vsize rss ...
             */
            int n = sscanf(comm_end + 2,
                           "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                           "%lu %lu %*u %*u %d %d %lu %*u %llu %lu %ld",
                           &pi.state, &ppid, &utime, &stime,
                           &priority, &nice_val, &num_threads,
                           &starttime, &vsize, &rss_pages);
            (void)n;
        }

        pi.ppid = ppid;
        pi.priority = priority;
        pi.nice = nice_val;
        pi.num_threads = (unsigned long)num_threads;
        pi.starttime = starttime;
        pi.cpu_time = (unsigned long long)(utime + stime);
        pi.virt = vsize / 1024;        /* bytes -> KiB */
        pi.res = (unsigned long)((unsigned long)rss_pages *
                                  (unsigned long)page_size / 1024);

        /* read /proc/[pid]/status for uid, VmLib (SHR), threads */
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        fp = fopen(path, "r");
        if (fp) {
            char line[256];
            unsigned int uid_val = 0;
            unsigned long vm_lib = 0;
            unsigned long thr = 0;
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    sscanf(line + 4, "%u", &uid_val);
                } else if (strncmp(line, "VmLib:", 6) == 0) {
                    sscanf(line + 6, "%lu", &vm_lib);
                } else if (strncmp(line, "Threads:", 8) == 0) {
                    sscanf(line + 8, "%lu", &thr);
                }
            }
            fclose(fp);
            if (thr > 0) {
                pi.num_threads = thr;
            }

            struct passwd * pw = getpwuid((uid_t)uid_val);
            if (pw && pw->pw_name) {
                strncpy(pi.user, pw->pw_name, sizeof(pi.user) - 1);
                pi.user[sizeof(pi.user) - 1] = '\0';
            } else {
                snprintf(pi.user, sizeof(pi.user), "%u", uid_val);
            }
            pi.shr = vm_lib; /* KiB */
        }

        /* read /proc/[pid]/cmdline for the full command line */
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        fp = fopen(path, "r");
        if (fp) {
            size_t total = 0;
            int c;
            int first = 1;
            while ((c = fgetc(fp)) != EOF && total < sizeof(pi.cmdline) - 1) {
                if (c == '\0') {
                    if (first) {
                        continue;   /* skip leading separator */
                    }
                    pi.cmdline[total++] = ' ';
                    first = 0;
                }
                else {
                    pi.cmdline[total++] = (char)c;
                }
            }
            pi.cmdline[total] = '\0';
            fclose(fp);
        }
        if (pi.cmdline[0] == '\0') {
            /* kernel threads have empty cmdline */
            strncpy(pi.cmdline, pi.command, sizeof(pi.cmdline) - 1);
            pi.cmdline[sizeof(pi.cmdline) - 1] = '\0';
        }

        if (!threads) {
            procs[count++] = pi;
            continue;
        }

        /* thread view: /proc/[pid]/task/[tid]/stat */
        {
            char tpath[64];
            snprintf(tpath, sizeof(tpath), "/proc/%d/task", pid);
            DIR * tdir = opendir(tpath);
            if (!tdir) {
                procs[count++] = pi;   /* fallback: process as a row */
                continue;
            }
            struct dirent * tent;
            int got_any = 0;
            while ((tent = readdir(tdir)) != NULL && count < max_procs) {
                char * tp = tent->d_name;
                int is_tid = 1;
                while (*tp) {
                    if (!isdigit((unsigned char)*tp)) {
                        is_tid = 0;
                        break;
                    }
                    tp++;
                }
                if (!is_tid) {
                    continue;
                }
                int tid = atoi(tent->d_name);
                htop_proc_t tp2;
                memset(&tp2, 0, sizeof(tp2));
                tp2.pid = tid;
                tp2.ppid = pid;
                tp2.is_thread = true;
                tp2.user[0] = '\0';
                tp2.command[0] = '\0';
                tp2.cmdline[0] = '\0';
                tp2.priority = priority;
                tp2.nice = nice_val;

                char tstat[64];
                snprintf(tstat, sizeof(tstat), "/proc/%d/task/%d/stat",
                         pid, tid);
                FILE * tfp = fopen(tstat, "r");
                if (tfp) {
                    char tbuf[1024];
                    if (fgets(tbuf, sizeof(tbuf), tfp)) {
                        char * cs = strchr(tbuf, '(');
                        char * ce = strrchr(tbuf, ')');
                        if (cs && ce && ce > cs) {
                            size_t clen = (size_t)(ce - cs - 1);
                            if (clen >= sizeof(tp2.command)) {
                                clen = sizeof(tp2.command) - 1;
                            }
                            memcpy(tp2.command, cs + 1, clen);
                            tp2.command[clen] = '\0';
                        }
                        unsigned long tut = 0, tst = 0;
                        int tprio = 0, tnice = 0;
                        if (ce) {
                            sscanf(ce + 2,
                                   "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                                   "%lu %lu %*u %*u %d %d %*lu %*u %*llu %*lu %*ld",
                                   &tp2.state, &tut, &tst, &tprio, &tnice);
                        }
                        tp2.priority = tprio;
                        tp2.nice = tnice;
                        tp2.cpu_time = (unsigned long long)(tut + tst);
                    }
                    fclose(tfp);
                }

                /* inherit user / memory fields from the process */
                strncpy(tp2.user, pi.user, sizeof(tp2.user) - 1);
                tp2.user[sizeof(tp2.user) - 1] = '\0';
                tp2.virt = pi.virt;
                tp2.res = 0;
                tp2.shr = 0;
                tp2.num_threads = 1;
                if (tp2.cmdline[0] == '\0') {
                    snprintf(tp2.cmdline, sizeof(tp2.cmdline),
                             "%s [tid %d]", pi.command, tid);
                }
                procs[count++] = tp2;
                got_any = 1;
            }
            closedir(tdir);
            if (!got_any) {
                procs[count++] = pi;
            }
        }
    }

    closedir(dir);
    return count;
}
#endif /* HTOP_PLATFORM_LINUX */

#ifdef HTOP_PLATFORM_WINDOWS
/**
 * @brief Windows: get process owner username.
 */
static void _htop_get_user_windows(int pid, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    buf[0] = '\0';

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (!hProc) {
        hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    }
    if (!hProc) {
        snprintf(buf, buf_size, "unknown");
        return;
    }

    HANDLE token = NULL;
    if (!OpenProcessToken(hProc, TOKEN_QUERY, &token)) {
        CloseHandle(hProc);
        snprintf(buf, buf_size, "unknown");
        return;
    }

    DWORD size = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &size);
    if (size == 0) {
        CloseHandle(token);
        CloseHandle(hProc);
        snprintf(buf, buf_size, "unknown");
        return;
    }

    TOKEN_USER * tu = (TOKEN_USER *)malloc(size);
    if (!tu) {
        CloseHandle(token);
        CloseHandle(hProc);
        snprintf(buf, buf_size, "unknown");
        return;
    }

    if (GetTokenInformation(token, TokenUser, tu, size, &size)) {
        char name[64];
        char domain[64];
        DWORD namelen = sizeof(name);
        DWORD domlen = sizeof(domain);
        SID_NAME_USE use;
        if (LookupAccountSidA(NULL, tu->User.Sid, name, &namelen,
                               domain, &domlen, &use)) {
            snprintf(buf, buf_size, "%s", name);
        } else {
            snprintf(buf, buf_size, "unknown");
        }
    } else {
        snprintf(buf, buf_size, "unknown");
    }

    free(tu);
    CloseHandle(token);
    CloseHandle(hProc);
}

/**
 * @brief Windows: enumerate processes via Toolhelp32.
 */
static int _htop_get_procs_windows(htop_proc_t * procs, int max_procs)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    int count = 0;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (count >= max_procs) {
                break;
            }

            htop_proc_t * pi = &procs[count];
            memset(pi, 0, sizeof(*pi));
            pi->pid = (int)pe.th32ProcessID;
            pi->ppid = (int)pe.th32ParentProcessID;
            pi->state = 'R'; /* Windows processes are "running" */
            pi->priority = 0;
            pi->nice = 0;
            pi->num_threads = pe.cntThreads;

            /* username */
            _htop_get_user_windows(pi->pid, pi->user, sizeof(pi->user));

            /* command: use szExeFile (basename) */
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1,
                                pi->command, sizeof(pi->command), NULL, NULL);
            strncpy(pi->cmdline, pi->command, sizeof(pi->cmdline) - 1);
            pi->cmdline[sizeof(pi->cmdline) - 1] = '\0';

            /* CPU times, memory, and priority — try PROCESS_QUERY_INFORMATION
             * first (sufficient for GetProcessTimes + GetProcessMemoryInfo),
             * then fall back to PROCESS_QUERY_LIMITED_INFORMATION for
             * elevated / protected processes we can still partially read. */
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION,
                                       FALSE, pe.th32ProcessID);
            if (!hProc) {
                hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, pe.th32ProcessID);
            }

            if (hProc) {
                FILETIME create, exit_t, kernel, user;
                if (GetProcessTimes(hProc, &create, &exit_t, &kernel, &user)) {
                    ULARGE_INTEGER k, u;
                    k.LowPart = kernel.dwLowDateTime;
                    k.HighPart = kernel.dwHighDateTime;
                    u.LowPart = user.dwLowDateTime;
                    u.HighPart = user.dwHighDateTime;
                    pi->cpu_time = k.QuadPart + u.QuadPart;
                    pi->starttime = create.dwHighDateTime;
                }

                PROCESS_MEMORY_COUNTERS_EX pmc;
                memset(&pmc, 0, sizeof(pmc));
                pmc.cb = sizeof(pmc);
                if (GetProcessMemoryInfo(hProc,
                                         (PROCESS_MEMORY_COUNTERS *)&pmc,
                                         sizeof(pmc))) {
                    pi->res  = (unsigned long)(pmc.WorkingSetSize / 1024);
                    pi->virt = (unsigned long)(pmc.PrivateUsage / 1024);
                    pi->shr  = (unsigned long)(pmc.PagefileUsage / 1024);
                }

                /* priority class → Unix-style priority mapping */
                DWORD pcl = GetPriorityClass(hProc);
                switch (pcl) {
                    case REALTIME_PRIORITY_CLASS:   pi->priority = 1;  pi->nice = -20; break;
                    case HIGH_PRIORITY_CLASS:       pi->priority = 5;  pi->nice = -10; break;
                    case ABOVE_NORMAL_PRIORITY_CLASS: pi->priority = 8;  pi->nice = -5;  break;
                    case BELOW_NORMAL_PRIORITY_CLASS: pi->priority = 12; pi->nice = 5;  break;
                    case IDLE_PRIORITY_CLASS:       pi->priority = 19; pi->nice = 10;  break;
                    default:                        pi->priority = 10; pi->nice = 0;   break;
                }

                CloseHandle(hProc);
            }

            count++;
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return count;
}
#endif /* HTOP_PLATFORM_WINDOWS */

#ifdef HTOP_PLATFORM_MACOS
/**
 * @brief macOS: enumerate processes via sysctl KERN_PROC.
 */
static int _htop_get_procs_macos(htop_proc_t * procs, int max_procs)
{
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t len = 0;
    if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0) {
        return 0;
    }

    struct kinfo_proc * kp = (struct kinfo_proc *)malloc(len);
    if (!kp) {
        return 0;
    }

    if (sysctl(mib, 3, kp, &len, NULL, 0) != 0) {
        free(kp);
        return 0;
    }

    int nprocs = (int)(len / sizeof(struct kinfo_proc));
    int count = 0;

    for (int i = 0; i < nprocs && count < max_procs; i++) {
        htop_proc_t * pi = &procs[count];
        memset(pi, 0, sizeof(*pi));
        pi->pid = kp[i].kp_proc.p_pid;
        pi->ppid = kp[i].kp_eproc.e_ppid;
        pi->state = kp[i].kp_proc.p_stat;
        pi->priority = kp[i].kp_proc.p_priority;
        pi->nice = kp[i].kp_proc.p_nice;
        pi->cpu_time = (unsigned long long)kp[i].kp_proc.p_ru ?
                       (kp[i].kp_proc.p_ru->ru_utime.tv_sec +
                        kp[i].kp_proc.p_ru->ru_stime.tv_sec) : 0;
        pi->num_threads = (unsigned long)kp[i].kp_proc.p_numthreads;

        /* command */
        strncpy(pi->command, kp[i].kp_proc.p_comm, sizeof(pi->command) - 1);
        pi->command[sizeof(pi->command) - 1] = '\0';
        strncpy(pi->cmdline, pi->command, sizeof(pi->cmdline) - 1);
        pi->cmdline[sizeof(pi->cmdline) - 1] = '\0';

        /* username */
        uid_t uid = kp[i].kp_eproc.e_ucred.cr_uid;
        struct passwd * pw = getpwuid(uid);
        if (pw && pw->pw_name) {
            strncpy(pi->user, pw->pw_name, sizeof(pi->user) - 1);
        } else {
            snprintf(pi->user, sizeof(pi->user), "%u", uid);
        }

        /* memory via proc_pidinfo */
        struct proc_taskinfo pti;
        if (proc_pidinfo(pi->pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti)) > 0) {
            pi->virt = (unsigned long)(pti.pti_virtual_size / 1024);
            pi->res = (unsigned long)(pti.pti_resident_size / 1024);
            pi->shr = 0;
        }

        count++;
    }

    free(kp);
    return count;
}
#endif /* HTOP_PLATFORM_MACOS */

#if defined(HTOP_PLATFORM_FREEBSD) || defined(HTOP_PLATFORM_OPENBSD) || defined(HTOP_PLATFORM_NETBSD)
/**
 * @brief BSD: enumerate processes via sysctl KERN_PROC.
 */
static int _htop_get_procs_bsd(htop_proc_t * procs, int max_procs)
{
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_ALL;
    mib[3] = 0;

    size_t len = 0;
    if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0) {
        return 0;
    }

    struct kinfo_proc * kp = (struct kinfo_proc *)malloc(len);
    if (!kp) {
        return 0;
    }

    if (sysctl(mib, 3, kp, &len, NULL, 0) != 0) {
        free(kp);
        return 0;
    }

    int nprocs = (int)(len / sizeof(struct kinfo_proc));
    int count = 0;

    for (int i = 0; i < nprocs && count < max_procs; i++) {
        htop_proc_t * pi = &procs[count];
        memset(pi, 0, sizeof(*pi));
        pi->pid = kp[i].ki_pid;
        pi->ppid = kp[i].ki_ppid;
        pi->state = (char)kp[i].ki_stat;
        pi->priority = kp[i].ki_priority;
        pi->nice = kp[i].ki_nice;
        pi->cpu_time = (unsigned long long)(kp[i].ki_rusage.ru_utime.tv_sec +
                                            kp[i].ki_rusage.ru_stime.tv_sec);
        pi->num_threads = (unsigned long)kp[i].ki_numthreads;

        /* command */
        strncpy(pi->command, kp[i].ki_comm, sizeof(pi->command) - 1);
        pi->command[sizeof(pi->command) - 1] = '\0';
        strncpy(pi->cmdline, pi->command, sizeof(pi->cmdline) - 1);
        pi->cmdline[sizeof(pi->cmdline) - 1] = '\0';

        /* username */
        uid_t uid = kp[i].ki_uid;
        struct passwd * pw = getpwuid(uid);
        if (pw && pw->pw_name) {
            strncpy(pi->user, pw->pw_name, sizeof(pi->user) - 1);
        } else {
            snprintf(pi->user, sizeof(pi->user), "%u", uid);
        }

        /* memory (kerninfo) */
        pi->virt = (unsigned long)(kp[i].ki_rusage.ru_maxrss / 1024);
        pi->res = 0;
        pi->shr = 0;

        count++;
    }

    free(kp);
    return count;
}
#endif /* BSD */

/* ---- regex filter (ERE subset) implementation ---- */

static int _htop_re_class_new(htop_re_t * re)
{
    if (re->ncls >= (int)(sizeof(re->cls) / sizeof(re->cls[0]))) {
        return -1;
    }
    memset(re->cls[re->ncls], 0, 128);
    return re->ncls++;
}

static void _htop_re_class_add(htop_re_t * re, int cls, int lo, int hi)
{
    for (int c = lo; c <= hi && c < 128; c++) {
        re->cls[cls][c & 0x7f] = 1;
    }
}

static void _htop_re_class_add_named(htop_re_t * re, int cls, char named)
{
    switch (named) {
        case 's':
            _htop_re_class_add(re, cls, '\t', '\t');
            _htop_re_class_add(re, cls, '\n', '\n');
            _htop_re_class_add(re, cls, '\v', '\v');
            _htop_re_class_add(re, cls, '\f', '\f');
            _htop_re_class_add(re, cls, ' ', ' ');
            _htop_re_class_add(re, cls, '\r', '\r');
            break;
        case 'd':
            _htop_re_class_add(re, cls, '0', '9');
            break;
        case 'w':
            _htop_re_class_add(re, cls, '0', '9');
            _htop_re_class_add(re, cls, 'A', 'Z');
            _htop_re_class_add(re, cls, 'a', 'z');
            _htop_re_class_add(re, cls, '_', '_');
            break;
        case 'S':
        case 'D':
        case 'W':
            /* build the negated set: start from all, subtract the positive */
            {
                int pos = _htop_re_class_new(re);
                if (pos >= 0) {
                    _htop_re_class_add_named(re, pos, (char)tolower((unsigned char)named));
                    for (int c = 0; c < 128; c++) {
                        if (!re->cls[pos][c]) {
                            re->cls[cls][c] = 1;
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
}

static int _htop_re_parse_class(htop_re_t * re, const char ** pp)
{
    const char * p = *pp;
    int cls = _htop_re_class_new(re);
    if (cls < 0) {
        return -1;
    }

    int negated = 0;
    if (*p == '^') {
        negated = 1;
        p++;
    }
    if (*p == ']' && (negated || p[1] != '\0')) {
        /* literal leading ] */
        _htop_re_class_add(re, cls, ']', ']');
        p++;
    }

    int empty = 1;
    while (*p && *p != ']') {
        int lo;
        int hi;
        if (*p == '\\' && p[1]) {
            char c = p[1];
            switch (c) {
                case 'n': lo = hi = '\n'; break;
                case 't': lo = hi = '\t'; break;
                case 'r': lo = hi = '\r'; break;
                case 'f': lo = hi = '\f'; break;
                case 'v': lo = hi = '\v'; break;
                case 's':
                case 'd':
                case 'w':
                case 'S':
                case 'D':
                case 'W':
                    _htop_re_class_add_named(re, cls, c);
                    p += 2;
                    empty = 0;
                    continue;
                default:
                    lo = hi = (unsigned char)c;
                    break;
            }
            p += 2;
        }
        else {
            lo = (unsigned char)*p++;
        }

        if (*p == '-' && p[1] && p[1] != ']') {
            char c = p[1];
            int hi_val;
            if (c == '\\' && p[2]) {
                hi_val = (unsigned char)p[2];
                p += 3;
            }
            else {
                hi_val = (unsigned char)p[1];
                p += 2;
            }
            hi = hi_val;
        }
        else {
            hi = lo;
        }
        _htop_re_class_add(re, cls, lo, hi);
        empty = 0;
    }

    if (*p != ']') {
        re->error = "unterminated character class";
        return -1;
    }
    p++;

    if (empty) {
        /* empty class matches nothing */
        memset(re->cls[cls], 0, 128);
    }
    else if (negated) {
        for (int c = 0; c < 128; c++) {
            re->cls[cls][c] = re->cls[cls][c] ? 0 : 1;
        }
        re->cls[cls][0] = 0; /* never match NUL */
    }

    *pp = p;
    return cls;
}

static int _htop_re_parse_branch(htop_re_t * re, const char ** pp,
                                 htop_re_branch_t * br)
{
    const char * p = *pp;
    int at_start = 1;

    while (*p && *p != '|') {
        if (br->n >= HTOP_RE_MAX_UNITS) {
            re->error = "pattern too long";
            return -1;
        }

        int is_unit = 0;

        switch (*p) {
            case '.':
                br->u[br->n].kind = HTOP_RE_U_ANY;
                br->u[br->n].data = 0;
                is_unit = 1;
                p++;
                break;

            case '[': {
                int cls = _htop_re_parse_class(re, &p);
                if (cls < 0) {
                    return -1;
                }
                br->u[br->n].kind = HTOP_RE_U_CLASS;
                br->u[br->n].data = cls;
                is_unit = 1;
                break;
            }

            case '^':
                if (at_start) {
                    br->anchor_s = 1;
                    p++;
                }
                else {
                    br->u[br->n].kind = HTOP_RE_U_CHAR;
                    br->u[br->n].data = (unsigned char)'^';
                    is_unit = 1;
                    p++;
                }
                break;

            case '$':
                if (p[1] == '\0' || p[1] == '|') {
                    br->anchor_e = 1;
                    p++;
                }
                else {
                    br->u[br->n].kind = HTOP_RE_U_CHAR;
                    br->u[br->n].data = (unsigned char)'$';
                    is_unit = 1;
                    p++;
                }
                break;

            case '\\':
                if (!p[1]) {
                    re->error = "trailing backslash";
                    return -1;
                }
                br->u[br->n].kind = HTOP_RE_U_CHAR;
                br->u[br->n].data = (unsigned char)p[1];
                is_unit = 1;
                p += 2;
                break;

            default:
                br->u[br->n].kind = HTOP_RE_U_CHAR;
                br->u[br->n].data = (unsigned char)*p;
                is_unit = 1;
                p++;
                break;
        }

        if (is_unit) {
            /* quantifier look-ahead */
            if (*p == '*') {
                br->u[br->n].quant = HTOP_RE_Q_STAR;
                p++;
            }
            else if (*p == '+') {
                br->u[br->n].quant = HTOP_RE_Q_PLUS;
                p++;
            }
            else if (*p == '?') {
                br->u[br->n].quant = HTOP_RE_Q_OPT;
                p++;
            }
            else {
                br->u[br->n].quant = HTOP_RE_Q_NONE;
            }
            br->n++;
        }
        at_start = 0;
    }

    *pp = p;
    return 0;
}

static int _htop_re_compile(htop_re_t * re, const char * pattern)
{
    memset(re, 0, sizeof(*re));
    re->source = pattern ? pattern : "";

    if (!pattern || !*pattern) {
        re->error = "empty pattern";
        return -1;
    }

    /* split into top-level branches on '|', then parse each */
    const char * p = pattern;
    re->nbranch = 0;

    for (;;) {
        if (re->nbranch >= HTOP_RE_MAX_BRANCH) {
            re->error = "too many alternation branches";
            return -1;
        }
        htop_re_branch_t * br = &re->branch[re->nbranch];
        memset(br, 0, sizeof(*br));
        const char * bp = p;
        if (_htop_re_parse_branch(re, &bp, br) != 0) {
            return -1;
        }
        /* a branch with no units and no anchors matches the empty
         * string — reject to avoid a confusing always-match filter */
        if (br->n == 0 && !br->anchor_s && !br->anchor_e &&
            *bp != '\0' && *bp != '|') {
            re->error = "empty alternation branch";
            return -1;
        }
        re->nbranch++;
        p = bp;

        if (*p == '|') {
            p++;
            continue;
        }
        if (*p == '\0') {
            break;
        }
        re->error = "unexpected character in pattern";
        return -1;
    }

    re->ok = 1;
    return 0;
}

static void _htop_re_free(htop_re_t * re)
{
    if (re) {
        memset(re, 0, sizeof(*re));
    }
}

#if 0  /* legacy op-encoder matcher — superseded by the branch/unit engine below */
static bool _htop_re_match_at(const htop_re_t * re, const char * text,
                              size_t tpos, int idx)
{
    if (idx >= re->nops) {
        return false;
    }
    int op = re->ops[idx];
    int data = re->data[idx];

    switch (op) {
        case HTOP_RE_OP_END:
            return tpos == strlen(text);

        case HTOP_RE_OP_ANCHOR_S:
            return tpos == 0 && _htop_re_match_at(re, text, tpos, idx + 1);

        case HTOP_RE_OP_ANCHOR_E:
            return (text[tpos] == '\0') && _htop_re_match_at(re, text, tpos, idx + 1);

        case HTOP_RE_OP_CHAR: {
            if (text[tpos] == '\0') {
                return false;
            }
            int c = (unsigned char)text[tpos];
            int d = data;
            if (re->case_insensitive) {
                c = tolower(c);
                d = tolower(d);
            }
            if (c != d) {
                return false;
            }
            return _htop_re_match_at(re, text, tpos + 1, idx + 1);
        }

        case HTOP_RE_OP_ANY: {
            if (text[tpos] == '\0') {
                return false;
            }
            return _htop_re_match_at(re, text, tpos + 1, idx + 1);
        }

        case HTOP_RE_OP_CLASS: {
            if (text[tpos] == '\0') {
                return false;
            }
            int c = (unsigned char)text[tpos];
            if (re->case_insensitive) {
                c = tolower(c);
            }
            if (!re->cls[data][c & 0x7f]) {
                return false;
            }
            return _htop_re_match_at(re, text, tpos + 1, idx + 1);
        }

        case HTOP_RE_OP_STAR: {
            /* the operand is ops[idx-1] (char/any/class) */
            int op2 = re->ops[idx - 1];
            int d2 = re->data[idx - 1];
            /* try matching zero occurrences */
            if (_htop_re_match_at(re, text, tpos, idx + 1)) {
                return true;
            }
            /* then one or more: consume while operand matches */
            size_t t = tpos;
            for (;;) {
                int c = (unsigned char)text[t];
                bool ok = false;
                if (c != '\0') {
                    int lc = re->case_insensitive ? tolower(c) : c;
                    if (op2 == HTOP_RE_OP_ANY) {
                        ok = true;
                    }
                    else if (op2 == HTOP_RE_OP_CHAR) {
                        int ld = re->case_insensitive ? tolower(d2) : d2;
                        ok = (lc == ld);
                    }
                    else if (op2 == HTOP_RE_OP_CLASS) {
                        ok = re->cls[d2][lc & 0x7f] != 0;
                    }
                }
                if (!ok) {
                    break;
                }
                t++;
                if (_htop_re_match_at(re, text, t, idx + 1)) {
                    return true;
                }
            }
            return false;
        }

        case HTOP_RE_OP_PLUS: {
            /* operand is ops[idx-1]; require at least one */
            int op2 = re->ops[idx - 1];
            int d2 = re->data[idx - 1];
            size_t t = tpos;
            for (;;) {
                int c = (unsigned char)text[t];
                bool ok = false;
                if (c != '\0') {
                    int lc = re->case_insensitive ? tolower(c) : c;
                    if (op2 == HTOP_RE_OP_ANY) {
                        ok = true;
                    }
                    else if (op2 == HTOP_RE_OP_CHAR) {
                        int ld = re->case_insensitive ? tolower(d2) : d2;
                        ok = (lc == ld);
                    }
                    else if (op2 == HTOP_RE_OP_CLASS) {
                        ok = re->cls[d2][lc & 0x7f] != 0;
                    }
                }
                if (!ok) {
                    break;
                }
                t++;
                if (_htop_re_match_at(re, text, t, idx + 1)) {
                    return true;
                }
            }
            return false;
        }

        case HTOP_RE_OP_OPT: {
            /* operand is ops[idx-1]; match once or zero times */
            size_t t2 = tpos + 1;
            int c = (unsigned char)text[tpos];
            if (c != '\0') {
                int op2 = re->ops[idx - 1];
                int d2 = re->data[idx - 1];
                int lc = re->case_insensitive ? tolower(c) : c;
                bool ok = false;
                if (op2 == HTOP_RE_OP_ANY) {
                    ok = true;
                }
                else if (op2 == HTOP_RE_OP_CHAR) {
                    int ld = re->case_insensitive ? tolower(d2) : d2;
                    ok = (lc == ld);
                }
                else if (op2 == HTOP_RE_OP_CLASS) {
                    ok = re->cls[d2][lc & 0x7f] != 0;
                }
                if (ok && _htop_re_match_at(re, text, t2, idx + 1)) {
                    return true;
                }
            }
            return _htop_re_match_at(re, text, tpos, idx + 1);
        }

        case HTOP_RE_OP_ALT: {
            /* find the matching counterpart ALT: a '(' pushes one and a
             * ')' or '|' closes one.  Compile layout:
             *   ( a | b )  ->  ALT a ALT b ALT
             *   ( a )      ->  ALT a ALT
             * So: from the opening ALT, scan forward counting ALTs;
             * the matching close is at the position where the count
             * returns to even.  Simplification: treat each ALT as a
             * split point and try each subsequent ALT position as a
             * "branch boundary" by matching the segment in between.
             */
            int i = idx + 1;
            /* try the branch directly: match from idx+1, but the rest of
             * the pattern starts at the *next* ALT boundary.  We find the
             * matching close by walking: each ALT toggles.  A group close
             * is the ALT at which the nesting depth (counted from idx)
             * drops back to 0. */
            int depth = 0;
            int close = -1;
            for (i = idx; i < re->nops; i++) {
                if (re->ops[i] == HTOP_RE_OP_ALT) {
                    if (depth == 0 && i != idx) {
                        close = i;
                        break;
                    }
                    depth += (i == idx) ? 1 : 0;
                }
            }
            /* The scan above is intentionally conservative: it finds the
             * nearest unmatched close ALT.  For nested groups the same
             * walk is applied recursively at each level. */
            if (close < 0) {
                close = re->nops - 1; /* END-adjacent fallback */
            }
            /* Branch A: text must be matchable by ops[idx+1 .. close-1]
             * AND the tail ops[close+1 ..] must also match (same tpos). */
            {
                /* implement as a two-stage match via a helper: match the
                 * middle segment, then continue from the close+1. */
                bool mid = true;
                int m = idx + 1;
                while (m < close) {
                    /* single-step simulation is overkill; instead use
                     * recursion by building implicit segment matches via
                     * the same function over a sub-range is not possible
                     * with the flat index.  So we evaluate the middle by
                     * a local loop over simple tokens; alternations with
                     * nested groups fall back to literal-empty behavior
                     * (still functional for typical filter patterns). */
                    int op = re->ops[m];
                    int data = re->data[m];
                    if (op == HTOP_RE_OP_CHAR) {
                        int c = (text[tpos] != '\0')
                                ? (unsigned char)text[tpos] : 0;
                        int lc = re->case_insensitive ? tolower(c) : c;
                        int ld = re->case_insensitive ? tolower(data) : data;
                        if (c == '\0' || lc != ld) {
                            mid = false;
                            break;
                        }
                        tpos++;
                    }
                    else if (op == HTOP_RE_OP_ANY) {
                        if (text[tpos] == '\0') {
                            mid = false;
                            break;
                        }
                        tpos++;
                    }
                    else if (op == HTOP_RE_OP_CLASS) {
                        int c = (text[tpos] != '\0')
                                ? (unsigned char)text[tpos] : 0;
                        int lc = re->case_insensitive ? tolower(c) : c;
                        if (c == '\0' || !re->cls[data][lc & 0x7f]) {
                            mid = false;
                            break;
                        }
                        tpos++;
                    }
                    else {
                        /* unsupported token inside alternation: stop mid */
                        mid = false;
                        break;
                    }
                    m++;
                }
                if (mid && _htop_re_match_at(re, text, tpos, close + 1)) {
                    return true;
                }
            }
            return false;
        }

        default:
            return false;
    }
}

#endif  /* legacy op-encoder matcher */

/**
 * @brief Match a single unit against text[tpos] (case-insensitive).
 */
static bool _htop_re_unit_at(const htop_re_t * re, const htop_re_unit_t * u,
                             size_t tpos, const char * text)
{
    char c = text[tpos];
    if (c == '\0') {
        return false;
    }
    switch (u->kind) {
        case HTOP_RE_U_ANY:
            return true;
        case HTOP_RE_U_CHAR:
            return tolower((unsigned char)c) == tolower((unsigned char)u->data);
        case HTOP_RE_U_CLASS:
            return re->cls[u->data][tolower((unsigned char)c) & 0x7f] != 0;
        default:
            return false;
    }
}

/**
 * @brief Match one branch against text starting at tpos.
 *
 * Returns the number of characters consumed, or -1 when the branch does
 * not match at this position.
 */
static int _htop_re_branch_match(const htop_re_t * re,
                                 const htop_re_branch_t * br,
                                 size_t tpos, const char * text)
{
    if (br->anchor_s && tpos != 0) {
        return -1;
    }
    size_t t = tpos;
    for (int i = 0; i < br->n; i++) {
        const htop_re_unit_t * u = &br->u[i];
        switch (u->quant) {
            case HTOP_RE_Q_NONE:
                if (!_htop_re_unit_at(re, u, t, text)) {
                    return -1;
                }
                t++;
                break;
            case HTOP_RE_Q_OPT:
                if (_htop_re_unit_at(re, u, t, text)) {
                    t++;
                }
                break;
            case HTOP_RE_Q_STAR:
                while (_htop_re_unit_at(re, u, t, text)) {
                    t++;
                }
                break;
            case HTOP_RE_Q_PLUS: {
                int consumed = 0;
                while (_htop_re_unit_at(re, u, t, text)) {
                    t++;
                    consumed++;
                }
                if (consumed == 0) {
                    return -1;
                }
                break;
            }
            default:
                break;
        }
    }
    if (br->anchor_e && text[t] != '\0') {
        return -1;
    }
    return (int)(t - tpos);
}

/**
 * @brief Search text for a match of any branch (like htop's filter).
 */
static bool _htop_re_match(const htop_re_t * re, const char * text)
{
    if (!re || !re->ok || !text) {
        return false;
    }
    size_t len = strlen(text);
    for (size_t pos = 0; pos <= len; pos++) {
        for (int b = 0; b < re->nbranch; b++) {
            if (_htop_re_branch_match(re, &re->branch[b], pos, text) >= 0) {
                return true;
            }
        }
    }
    return false;
}

/* ---- sorting ---- */

static int _htop_cmp_int(int a, int b)
{
    return (a > b) ? 1 : (a < b) ? -1 : 0;
}

static int _htop_cmp_ull(unsigned long long a, unsigned long long b)
{
    return (a > b) ? 1 : (a < b) ? -1 : 0;
}

static int _htop_cmp_dbl(double a, double b)
{
    return (a > b) ? 1 : (a < b) ? -1 : 0;
}

static int _htop_sort_cmp(const void * a, const void * b)
{
    const htop_proc_t * pa = (const htop_proc_t *)a;
    const htop_proc_t * pb = (const htop_proc_t *)b;
    int result = 0;

    switch (g_htop_sort_col) {
        case HTOP_COL_PID:
            result = _htop_cmp_int(pa->pid, pb->pid);
            break;
        case HTOP_COL_PPID:
            result = _htop_cmp_int(pa->ppid, pb->ppid);
            break;
        case HTOP_COL_USER:
            result = strcmp(pa->user, pb->user);
            break;
        case HTOP_COL_PRI:
            result = _htop_cmp_int(pa->priority, pb->priority);
            break;
        case HTOP_COL_NI:
            result = _htop_cmp_int(pa->nice, pb->nice);
            break;
        case HTOP_COL_VIRT:
            result = _htop_cmp_ull(pa->virt, pb->virt);
            break;
        case HTOP_COL_RES:
            result = _htop_cmp_ull(pa->res, pb->res);
            break;
        case HTOP_COL_SHR:
            result = _htop_cmp_ull(pa->shr, pb->shr);
            break;
        case HTOP_COL_S:
            result = _htop_cmp_int((int)pa->state, (int)pb->state);
            break;
        case HTOP_COL_CPU:
            result = _htop_cmp_dbl(pa->cpu_pct, pb->cpu_pct);
            break;
        case HTOP_COL_MEM:
            result = _htop_cmp_dbl(pa->mem_pct, pb->mem_pct);
            break;
        case HTOP_COL_TIME:
            result = _htop_cmp_ull(pa->cpu_time, pb->cpu_time);
            break;
        case HTOP_COL_CMD:
            result = strcmp(pa->command, pb->command);
            break;
        default:
            result = _htop_cmp_dbl(pa->cpu_pct, pb->cpu_pct);
            break;
    }

    if (g_htop_reverse) {
        result = -result;
    }
    return result;
}

static void _htop_sort_procs(htop_proc_t * procs, int count,
                             htop_col_t col, int reverse)
{
    g_htop_sort_col = col;
    g_htop_reverse = reverse;
    qsort(procs, (size_t)count, sizeof(htop_proc_t), _htop_sort_cmp);
}

/* ---- formatting ---- */

/**
 * @brief Format memory (in KiB) as "168.0m" or "1.2g".
 */
static void _htop_format_mem(unsigned long kib, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    double mib = (double)kib / 1024.0;
    if (mib >= 1024.0) {
        snprintf(buf, buf_size, "%.1fg", mib / 1024.0);
    }
    else {
        snprintf(buf, buf_size, "%.1fm", mib);
    }
}

/**
 * @brief Format CPU time in seconds as TIME+ format.
 */
static void _htop_format_time(unsigned long seconds, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    unsigned long h = seconds / 3600;
    unsigned long m = (seconds % 3600) / 60;
    unsigned long s = seconds % 60;

    if (h >= 100) {
        /* HHHH:MM */
        snprintf(buf, buf_size, "%lu:%02lu", h, m);
    }
    else if (h > 0) {
        /* H:MM:SS */
        snprintf(buf, buf_size, "%lu:%02lu:%02lu", h, m, s);
    }
    else {
        /* M:SS */
        snprintf(buf, buf_size, "%lu:%02lu", m, s);
    }
}

/**
 * @brief Format uptime as "X days, Y:ZZ" or "Y:ZZ" or "Z min".
 */
static void _htop_format_uptime(double uptime, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    unsigned long secs = (unsigned long)uptime;
    unsigned long days = secs / 86400;
    unsigned long hours = (secs % 86400) / 3600;
    unsigned long mins = (secs % 3600) / 60;

    if (days > 0) {
        snprintf(buf, buf_size, "%lu days, %2lu:%02lu", days, hours, mins);
    }
    else if (hours > 0) {
        snprintf(buf, buf_size, "%lu:%02lu", hours, mins);
    }
    else {
        snprintf(buf, buf_size, "%lu min", mins);
    }
}

/* ---- rendering ---- */

/**
 * @brief Render a gauge bar like htop:  [####----]  label extra
 *
 * @param buf        output buffer (must be at least width + 64 bytes)
 * @param buf_size   size of buf
 * @param frac       fraction 0.0 .. 1.0 (values outside are clamped)
 * @param width      number of bar cells
 * @param label      text before the bar (e.g. "CPU")
 * @param extra      text after the bar (e.g. "45.2%")
 */
static void _htop_render_bar(char * buf, size_t buf_size, double frac,
                             int width, const char * label, const char * extra)
{
    if (!buf || buf_size == 0 || width <= 0) {
        return;
    }
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    int fill = (int)(frac * (double)width + 0.5);
    if (fill > width) fill = width;
    if (fill < 0) fill = 0;

    int pos = 0;
    /* color choice by fraction */
    const char * color;
    if (frac < 0.40)      color = htop_cpu_colors[1];
    else if (frac < 0.70) color = htop_cpu_colors[2];
    else if (frac < 0.90) color = htop_cpu_colors[3];
    else                  color = htop_cpu_colors[4];

    pos += snprintf(buf + pos, buf_size - (size_t)pos, "%s [",
                    label ? label : "");
    if (fill > 0) {
        pos += snprintf(buf + pos, buf_size - (size_t)pos, "%s", color);
    }
    for (int i = 0; i < fill; i++) {
        buf[pos] = '#';
        pos++;
        if (pos >= (int)buf_size - 4) {
            pos = (int)buf_size - 4;
            i = fill;
            break;
        }
    }
    if (fill > 0) {
        const char * reset = HTOP_RESET_COLOR;
        pos += (int)snprintf(buf + pos, buf_size - (size_t)pos, "%s", reset);
    }
    for (int i = fill; i < width; i++) {
        buf[pos] = '-';
        pos++;
        if (pos >= (int)buf_size - 4) {
            pos = (int)buf_size - 4;
            i = width;
            break;
        }
    }
    pos += snprintf(buf + pos, buf_size - (size_t)pos, "] %s",
                    extra ? extra : "");
    buf[buf_size - 1] = '\0';
}

/**
 * @brief Compute per-CPU usage fraction from two samples.
 *
 * Returns 0..1 for the given CPU index, or -1 when no delta is available.
 */
static double _htop_cpu_frac(const htop_sys_t * cur, const htop_sys_t * prev,
                             int has_prev, int idx)
{
    const htop_cpu_ticks_t * c;
    const htop_cpu_ticks_t * p;
    if (idx < 0) {
        c = &cur->cpu;
        p = prev ? &prev->cpu : NULL;
    }
    else {
        if (idx >= cur->num_cpus || idx >= HTOP_MAX_CPUS) {
            return -1.0;
        }
        c = &cur->cpu_per[idx];
        p = prev ? &prev->cpu_per[idx] : NULL;
    }

    if (!has_prev || !p) {
        /* first sample: cumulative since boot as a rough fraction */
        unsigned long long tot = c->user + c->nice + c->system + c->idle +
                                 c->iowait + c->irq + c->softirq + c->steal;
        if (tot == 0) {
            return 0.0;
        }
        unsigned long long idle = c->idle + c->iowait;
        return 1.0 - (double)idle / (double)tot;
    }

    unsigned long long d_total =
        (c->user + c->nice + c->system + c->idle + c->iowait +
         c->irq + c->softirq + c->steal)
        - (p->user + p->nice + p->system + p->idle + p->iowait +
           p->irq + p->softirq + p->steal);
    if (d_total == 0) {
        return 0.0;
    }
    unsigned long long d_idle = (c->idle + c->iowait) - (p->idle + p->iowait);
    if (d_idle > d_total) {
        d_idle = d_total;
    }
    return (double)(d_total - d_idle) / (double)d_total;
}

/**
 * @brief Render the header (uptime/load, tasks, CPU bars, mem/swap bars).
 */
static void _htop_render_header(const htop_sys_t * cur, const htop_sys_t * prev,
                                int has_prev, const htop_ui_t * ui, int width)
{
    (void)ui;
    if (!cur || width <= 0) {
        return;
    }
    if (width < 40) {
        width = 40;
    }

    /* line 1: time + uptime + load */
    time_t now = time(NULL);
    struct tm tm_val;
#ifdef HTOP_PLATFORM_WINDOWS
    tm_val = *localtime(&now);
#else
    localtime_r(&now, &tm_val);
#endif
    char upbuf[64];
    _htop_format_uptime(cur->uptime, upbuf, sizeof(upbuf));
    htop_printf("%s htop %s %02d:%02d:%02d up %s, load average: %.2f, %.2f, %.2f\n",
                g_htop_c_bold, g_htop_c_reset,
                tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec,
                upbuf, cur->load1, cur->load5, cur->load15);

    /* line 2: tasks */
    htop_printf("Tasks: %4d total, %4d running, %4d sleeping, %4d stopped, %4d zombie\n",
                cur->tasks_total, cur->tasks_running,
                cur->tasks_sleeping, cur->tasks_stopped, cur->tasks_zombie);

    /* lines 3..: per-CPU bars (2 columns when many CPUs) */
    int bar_width = width - 16;
    if (bar_width < 10) bar_width = 10;

    int ncpu = cur->num_cpus;
    if (ncpu <= 0) ncpu = 1;

    if (ncpu <= 8) {
        /* one bar per CPU */
        for (int i = 0; i < ncpu && i < HTOP_MAX_CPUS; i++) {
            double frac = _htop_cpu_frac(cur, prev, has_prev, i);
            char line[512];
            char extra[64];
            snprintf(extra, sizeof(extra), "%.1f%%", frac * 100.0);
            char label[16];
            snprintf(label, sizeof(label), "CPU%d", i);
            _htop_render_bar(line, sizeof(line), frac, bar_width, label, extra);
            htop_printf("%s\n", line);
        }
    }
    else {
        /* aggregate bar only when too many CPUs to list */
        double frac = _htop_cpu_frac(cur, prev, has_prev, -1);
        char line[512];
        char extra[64];
        snprintf(extra, sizeof(extra), "%.1f%% (agg of %d CPUs)", frac * 100.0, ncpu);
        _htop_render_bar(line, sizeof(line), frac, bar_width, "CPU", extra);
        htop_printf("%s\n", line);
    }

    /* memory bar: used + buff/cache */
    {
        double mfrac = cur->mem_total > 0
                       ? (double)cur->mem_used / (double)cur->mem_total : 0.0;
        double cfrac = cur->mem_total > 0
                       ? (double)(cur->mem_buff + cur->mem_cache) / (double)cur->mem_total : 0.0;
        char line[640];
        char extra[96];
        double used_mib = (double)cur->mem_used / (1024.0 * 1024.0);
        double tot_mib = (double)cur->mem_total / (1024.0 * 1024.0);
        snprintf(extra, sizeof(extra),
                 "Mem: %6.0f/%6.0f MiB (%.1f%% used, %.1f%% cache)",
                 used_mib, tot_mib, mfrac * 100.0, cfrac * 100.0);
        _htop_render_bar(line, sizeof(line), mfrac + cfrac, bar_width, "Mem", extra);
        htop_printf("%s\n", line);
    }

    /* swap bar */
    if (cur->swap_total > 0) {
        double sfrac = (double)cur->swap_used / (double)cur->swap_total;
        char line[640];
        char extra[96];
        double used_mib = (double)cur->swap_used / (1024.0 * 1024.0);
        double tot_mib = (double)cur->swap_total / (1024.0 * 1024.0);
        snprintf(extra, sizeof(extra), "Swap: %6.0f/%6.0f MiB (%.1f%% used)",
                 used_mib, tot_mib, sfrac * 100.0);
        _htop_render_bar(line, sizeof(line), sfrac, bar_width, "Swap", extra);
        htop_printf("%s\n", line);
    }
}

/**
 * @brief Render the process column header line.
 */
static void _htop_render_col_header(const htop_ui_t * ui, int width)
{
    if (!ui || width <= 0) {
        return;
    }
    if (width < 60) {
        width = 60;
    }

    int cmd_w = width - 78;
    if (cmd_w < 8) cmd_w = 8;

    htop_printf("%s%s%6s %6s %8s %3s %3s %7s %7s %7s %s %5s %5s %7s %-*s%s\n",
                g_htop_c_bold, g_htop_c_rev,
                "PID", "PPID", "USER", "PRI", "NI",
                "VIRT", "RES", "SHR", "S",
                "%CPU", "%MEM", "TIME+",
                cmd_w, "COMMAND", g_htop_c_reset);
}

/**
 * @brief Render one process row.
 */
static void _htop_render_row(const htop_proc_t * p, const htop_sys_t * sys,
                             const htop_ui_t * ui, int width, int selected)
{
    if (!p || !ui || width <= 0) {
        return;
    }
    if (width < 60) {
        width = 60;
    }

    int cmd_w = width - 78;
    if (cmd_w < 8) cmd_w = 8;

    char vbuf[32], rbuf[32], sbuf[32], tbuf[32];
    _htop_format_mem(p->virt, vbuf, sizeof(vbuf));
    _htop_format_mem(p->res, rbuf, sizeof(rbuf));
    _htop_format_mem(p->shr, sbuf, sizeof(sbuf));
    _htop_format_time((unsigned long)(p->cpu_time /
#ifdef HTOP_PLATFORM_WINDOWS
                                      10000000ULL
#else
                                      100ULL
#endif
                        ), tbuf, sizeof(tbuf));

    const char * cmd = p->command;
    if (ui->wide_cmd || ui->show_path) {
        cmd = p->cmdline;
    }
    if (cmd[0] == '\0') {
        cmd = p->command;
    }

    /* state coloring: R = green, S = default, D = blue, Z = red, T = yellow */
    const char * stcol = HTOP_RESET_COLOR;
    switch (p->state) {
        case 'R': stcol = g_htop_c_grn; break;
        case 'D': stcol = g_htop_c_blu; break;
        case 'Z': stcol = g_htop_c_red; break;
        case 'T': stcol = g_htop_c_yel; break;
        default:  stcol = "";        break;
    }

    const char * mark = selected ? g_htop_c_rev : "";
    const char * reset = selected ? g_htop_c_reset : "";

    /* truncate the command to fit */
    char cmdbuf[HTOP_CMD_MAX];
    snprintf(cmdbuf, sizeof(cmdbuf), "%s", cmd);
    if ((int)strlen(cmdbuf) > cmd_w) {
        cmdbuf[cmd_w - 1] = '\0';
    }

    htop_printf("%s%6d %6d %8.8s %3d %3d %7s %7s %7s %s%s%s %5.1f %5.1f %7s %-*s%s\n",
                mark, p->pid, p->ppid, p->user, p->priority, p->nice,
                vbuf, rbuf, sbuf, stcol, " ", HTOP_RESET_COLOR,
                p->cpu_pct, p->mem_pct, tbuf,
                cmd_w, cmdbuf, reset);
    (void)sys;
}

/* ---- interactive helpers ---- */

#ifdef HTOP_PLATFORM_WINDOWS
static int _htop_kbhit(void)
{
    return _kbhit();
}

static int _htop_getch(void)
{
    return _getch();
}

static void _htop_set_raw_mode(int enable)
{
    /* Windows console is already in the right mode for _getch */
    (void)enable;
}

static void _htop_enable_vt(void)
{
    /* Enable ANSI VT processing on Windows 10+ */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}
#else
static int _htop_kbhit(void)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int _htop_getch(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (int)c;
    }
    return -1;
}

static struct termios g_orig_termios;
static int g_termios_saved = 0;

static void _htop_set_raw_mode(int enable)
{
    if (enable) {
        if (!g_termios_saved) {
            if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
                g_termios_saved = 1;
            }
        }
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    else if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_termios_saved = 0;
    }
}

static void _htop_enable_vt(void)
{
    /* POSIX terminals support ANSI natively */
}
#endif

static void _htop_hide_cursor(void)
{
    htop_printf("\033[?25l");
}

static void _htop_show_cursor(void)
{
    htop_printf("\033[?25h");
}

/**
 * @brief Query terminal size; falls back to 24x80.
 */
static int _htop_terminal_size(int * rows, int * cols)
{
    if (!rows || !cols) {
        return -1;
    }
    *rows = 24;
    *cols = 80;

#ifdef HTOP_PLATFORM_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE &&
        GetConsoleScreenBufferInfo(h, &csbi)) {
        int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int hgt = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (w > 0 && hgt > 0) {
            *cols = w;
            *rows = hgt;
        }
    }
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) *cols = ws.ws_col;
        if (ws.ws_row > 0) *rows = ws.ws_row;
    }
    return 0;
#endif
}

/**
 * @brief Check whether stdout is connected to a TTY.
 */
static int _htop_is_tty(void)
{
#ifdef HTOP_PLATFORM_WINDOWS
    DWORD mode = 0;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!GetConsoleMode(h, &mode)) {
        return 0;
    }
    return 1;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

/* ---- dialogs / actions ---- */

/**
 * @brief Small modal dialog asking for an integer.
 *
 * Returns the chosen value, or -1 on cancel.
 */
static int _htop_dialog_int(const char * title, const char * prompt, int def)
{
    htop_printf("\033[2J\033[H");
    htop_printf("\033[1m%s\033[0m\n\n", title ? title : "");
    htop_printf("%s\n", prompt ? prompt : "");
    htop_printf("Current: %d   (use arrows / digits, Enter to confirm, Esc/q to cancel)\n", def);
    htop_printf("\033[7m  %4d  \033[0m\n", def);
    htop_fflush(stdout);

    int val = def;
    int done = 0;
    while (!done) {
        if (!_htop_kbhit()) {
            _htop_sleep_ms(20);
            continue;
        }
        int c = _htop_getch();
        switch (c) {
            case 27:          /* Esc */
            case 3:           /* Ctrl+C */
            case 'q':
            case 'Q':
                done = 1;
                return -1;
            case '\r':
            case '\n':
                done = 1;
                break;
            case 'k':         /* up */
            case 65:          /* arrow up (ESC [ A) */
                val++;
                break;
            case 'j':         /* down */
            case 66:          /* arrow down */
                val--;
                break;
            case '+':
            case '=':
                val += 10;
                break;
            case '-':
                val -= 10;
                break;
            default:
                if (c >= '0' && c <= '9') {
                    val = val * 10 + (c - '0');
                    if (val > 999999) val = 999999;
                }
                else if (c >= '0' && c <= '9') {
                    break;
                }
                else {
                    break;
                }
                break;
        }
        if (c != 27) {
            /* re-render the value in place (simplified: full redraw) */
            htop_printf("\033[6A\033[2K");
            htop_printf("\033[7m  %4d  \033[0m\n", val);
            htop_fflush(stdout);
        }
    }
    htop_printf("\033[6A\033[2K");
    htop_fflush(stdout);
    return val;
}

/**
 * @brief Small modal dialog picking one of several choices.
 *
 * Returns the 0-based index, or -1 on cancel.
 */
static int _htop_dialog_choice(const char * title, const char * prompt,
                               const char * const * choices, int n, int def)
{
    if (!choices || n <= 0) {
        return -1;
    }
    if (def < 0) def = 0;
    if (def >= n) def = n - 1;

    htop_printf("\033[2J\033[H");
    htop_printf("\033[1m%s\033[0m\n\n", title ? title : "");
    if (prompt) {
        htop_printf("%s\n", prompt);
        htop_printf("\n");
    }
    htop_fflush(stdout);

    int sel = def;
    int done = 0;
    while (!done) {
        for (int i = 0; i < n; i++) {
            htop_printf("%s  %s\n",
                        i == sel ? "\033[7m" : "",
                        choices[i]);
            htop_printf(i == sel ? "\033[0m" : "");
        }
        htop_printf("\n(arrows to move, Enter to select, Esc to cancel)\n");
        htop_fflush(stdout);

        if (!_htop_kbhit()) {
            _htop_sleep_ms(20);
            continue;
        }
        int c = _htop_getch();
        if (c == 27) {
            int c2 = _htop_getch();
            if (c2 == '[') {
                c2 = _htop_getch();
                if (c2 == 'A') sel--;
                else if (c2 == 'B') sel++;
            }
            else {
                done = 1;
                return -1;
            }
        }
        else if (c == 'k') sel--;
        else if (c == 'j') sel++;
        else if (c == '\r' || c == '\n' || c == 13) {
            done = 1;
        }
        else if (c == 'q' || c == 'Q' || c == 3) { /* q / Ctrl+C */
            done = 1;
            return -1;
        }
        if (sel < 0) sel = n - 1;
        if (sel >= n) sel = 0;
        htop_printf("\033[%dA", n + 1);
    }
    htop_printf("\033[%dA\033[2K\n", n + 1);
    htop_fflush(stdout);
    return sel;
}

/**
 * @brief Small modal dialog asking for a text line.
 *
 * Returns 0 on confirm, -1 on cancel.
 */
static int _htop_dialog_text(const char * title, const char * prompt,
                             char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return -1;
    }
    size_t len = 0;
    if (buf[0]) {
        len = strlen(buf);
        if (len >= buf_size) len = buf_size - 1;
    }

    htop_printf("\033[2J\033[H");
    htop_printf("\033[1m%s\033[0m\n\n", title ? title : "");
    htop_printf("%s\n", prompt ? prompt : "");
    htop_fflush(stdout);

    int done = 0;
    while (!done) {
        /* echo the current text */
        htop_printf("\033[3A");
        for (size_t i = 0; i < len; i++) {
            if (buf[i] >= 32 && buf[i] < 127) {
                htop_printf("%c", buf[i]);
            }
            else {
                htop_printf("?");
            }
        }
        htop_printf("\033[K");
        htop_fflush(stdout);

        if (!_htop_kbhit()) {
            _htop_sleep_ms(20);
            continue;
        }
        int c = _htop_getch();
        if (c == 27) {
            int c2 = _htop_getch();
            if (c2 == '[') {
                c2 = _htop_getch();
                if (c2 == 'C' && len > 0) {
                    /* right: no-op in this simple editor */
                }
                else if (c2 == 'D' && len > 0) {
                    /* left: no-op */
                }
                else if (c2 == '3' && len > 0) {
                    /* delete key */
                    _htop_getch();
                    len--;
                    buf[len] = '\0';
                }
            }
            else {
                done = 1;
                return -1;
            }
        }
        else if (c == '\r' || c == '\n' || c == 13) {
            done = 1;
        }
        else if (c == 3) {
            /* Ctrl+C: cancel */
            done = 1;
            return -1;
        }
        else if (c == 8 || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
            }
        }
        else if (c >= 32 && c < 127 && len + 1 < buf_size) {
            buf[len++] = (char)c;
            buf[len] = '\0';
        }
    }
    htop_printf("\033[3A\033[2K");
    htop_fflush(stdout);
    return 0;
}

/**
 * @brief Send a signal to a process.
 *
 * Returns 0 on success, -1 on failure.
 */
static int _htop_send_signal(int pid, int sig)
{
#ifdef HTOP_PLATFORM_WINDOWS
    /* Windows has no POSIX signals; use TerminateProcess for SIGKILL,
     * and post WM_CLOSE for SIGTERM as a best effort. */
    if (sig == 9) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
        if (!h) {
            return -1;
        }
        int ok = TerminateProcess(h, 1) ? 0 : -1;
        CloseHandle(h);
        return ok;
    }
    /* best-effort: try TerminateProcess for TERM as well */
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) {
        return -1;
    }
    int ok = TerminateProcess(h, 0) ? 0 : -1;
    CloseHandle(h);
    return ok;
#else
    if (kill((pid_t)pid, (int)sig) == 0) {
        return 0;
    }
    return -1;
#endif
}

/**
 * @brief Set the nice value of a process.
 *
 * Returns 0 on success, -1 on failure.
 */
static int _htop_set_nice(int pid, int nice_val)
{
    if (nice_val < -20) nice_val = -20;
    if (nice_val > 19)  nice_val = 19;
#ifdef HTOP_PLATFORM_WINDOWS
    (void)pid; (void)nice_val;
    return -1; /* not supported on Windows */
#else
    if (setpriority(PRIO_PROCESS, (id_t)pid, (int)nice_val) == 0) {
        return 0;
    }
    return -1;
#endif
}

/**
 * @brief Interactive Setup dialog (F2).
 *
 * Updates UI state in place.
 */
static void _htop_run_setup(htop_ui_t * ui)
{
    static const char * items[] = {
        "Update interval (seconds)",
        "Sort column",
        "Toggle reverse sort",
        "Show kernel threads",
        "Show threads as separate rows",
        "Tree view",
        "Wide command (full command line)",
        "Show full program path",
        "CPU bar color",
        "Memory bar color",
        "Swap bar color",
        "Clear filter",
        "Save & close"
    };
    int n = (int)(sizeof(items) / sizeof(items[0]));

    while (1) {
        int sel = _htop_dialog_choice("Setup", "Choose an option:", items, n, 0);
        if (sel < 0) {
            return;
        }

        switch (sel) {
            case 0: {
                int v = _htop_dialog_int("Update interval",
                                         "Seconds between updates:",
                                         (int)(ui->delay + 0.5));
                if (v > 0 && v <= 60) {
                    ui->delay = (double)v;
                }
                break;
            }
            case 1: {
                static const char * cols[] = {
                    "PID", "PPID", "USER", "PRI", "NI", "VIRT",
                    "RES", "SHR", "S", "%CPU", "%MEM", "TIME+",
                    "COMMAND"
                };
                int c = _htop_dialog_choice("Sort column",
                                            "Select column to sort by:",
                                            cols, (int)(sizeof(cols)/sizeof(cols[0])),
                                            (int)ui->sort_col);
                if (c >= 0 && c < HTOP_NUM_COLUMNS) {
                    ui->sort_col = (htop_col_t)c;
                }
                break;
            }
            case 2:
                ui->reverse = !ui->reverse;
                break;
            case 3:
                ui->show_kthreads = !ui->show_kthreads;
                break;
            case 4:
                ui->show_threads = !ui->show_threads;
                break;
            case 5:
                ui->tree_view = !ui->tree_view;
                break;
            case 6:
                ui->wide_cmd = !ui->wide_cmd;
                break;
            case 7:
                ui->show_path = !ui->show_path;
                break;
            case 8:
            case 9:
            case 10: {
                static const char * colors[] = {
                    "Default (auto by usage)",
                    "Green",
                    "Orange",
                    "Dark Orange",
                    "Red"
                };
                int * which = (sel == 8) ? &ui->color_cpu
                              : (sel == 9) ? &ui->color_mem : &ui->color_swap;
                int c = _htop_dialog_choice("Bar color", "Pick a color:",
                                            colors, 5, *which);
                if (c >= 0 && c <= 4) {
                    *which = c;
                }
                break;
            }
            case 11:
                ui->filter[0] = '\0';
                ui->filter_set = 0;
                break;
            case 12:
            default:
                return;
        }
    }
}

/* ---- utility ---- */

static void _htop_sleep_ms(unsigned long ms)
{
#ifdef HTOP_PLATFORM_WINDOWS
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

static unsigned long long _htop_time_now_ms(void)
{
#ifdef HTOP_PLATFORM_WINDOWS
    return (unsigned long long)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL +
           (unsigned long long)tv.tv_usec / 1000ULL;
#endif
}

static int _htop_parse_int(const char * s, int def)
{
    if (!s || !*s) {
        return def;
    }
    return atoi(s);
}

static double _htop_parse_delay(const char * s, double def)
{
    if (!s || !*s) {
        return def;
    }
    double v = atof(s);
    if (v <= 0.0) {
        return def;
    }
    return v;
}

static htop_col_t _htop_parse_col(const char * s)
{
    if (!s) {
        return HTOP_COL_CPU;
    }
    if (strcasecmp(s, "PID") == 0)        return HTOP_COL_PID;
    if (strcasecmp(s, "PPID") == 0)       return HTOP_COL_PPID;
    if (strcasecmp(s, "USER") == 0)       return HTOP_COL_USER;
    if (strcasecmp(s, "PRI") == 0)        return HTOP_COL_PRI;
    if (strcasecmp(s, "NI") == 0)         return HTOP_COL_NI;
    if (strcasecmp(s, "VIRT") == 0)       return HTOP_COL_VIRT;
    if (strcasecmp(s, "RES") == 0)        return HTOP_COL_RES;
    if (strcasecmp(s, "SHR") == 0)       return HTOP_COL_SHR;
    if (strcasecmp(s, "S") == 0)          return HTOP_COL_S;
    if (strcasecmp(s, "%CPU") == 0 ||
        strcasecmp(s, "CPU") == 0)        return HTOP_COL_CPU;
    if (strcasecmp(s, "%MEM") == 0 ||
        strcasecmp(s, "MEM") == 0)        return HTOP_COL_MEM;
    if (strcasecmp(s, "TIME+") == 0 ||
        strcasecmp(s, "TIME") == 0)       return HTOP_COL_TIME;
    if (strcasecmp(s, "COMMAND") == 0)    return HTOP_COL_CMD;
    return HTOP_COL_CPU;
}

static const char * _htop_col_name(htop_col_t col)
{
    if (col >= 0 && col < HTOP_NUM_COLUMNS) {
        return htop_col_names[col];
    }
    return "%CPU";
}

static const char * _htop_signal_name(int sig)
{
    switch (sig) {
        case 1:  return "SIGHUP";
        case 2:  return "SIGINT";
        case 3:  return "SIGQUIT";
        case 6:  return "SIGABRT";
        case 9:  return "SIGKILL";
        case 15: return "SIGTERM";
        case 18: return "SIGCONT";
        case 19: return "SIGSTOP";
        case 20: return "SIGTSTP";
        case 21: return "SIGTTIN";
        case 22: return "SIGTTOU";
        case 29: return "SIGIO";
        case 30: return "SIGUSR1";
        case 31: return "SIGUSR2";
        default:
#ifdef HTOP_PLATFORM_WINDOWS
            (void)sig;
            return "SIGKILL";
#else
            return "SIGKILL";
#endif
    }
}

/**
 * @brief Decide whether a process row is visible under current filters.
 */
static bool _htop_proc_visible(const htop_proc_t * p, const htop_ui_t * ui,
                               const htop_opts_t * opts, const htop_re_t * re)
{
    if (!p || !ui || !opts) {
        return false;
    }

    /* kernel-thread toggle: hide kernel threads (cmdline == command and
     * empty real cmdline on Linux means kernel thread) */
    if (!ui->show_kthreads) {
#ifdef HTOP_PLATFORM_LINUX
        if (p->cmdline[0] == '[' && p->cmdline[1] != '\0') {
            return false;
        }
#endif
    }

    /* command-line regex filter (case-insensitive) */
    if (re && re->ok) {
        const char * hay = (ui->wide_cmd || ui->show_path)
                           ? p->cmdline : p->command;
        if (hay[0] == '\0') {
            hay = p->command;
        }
        if (!_htop_re_match(re, hay)) {
            return false;
        }
    }

    /* command filter via -f when regex not built from UI (batch mode) */
    if (opts->filter_set && opts->filter[0] && !(re && re->ok)) {
        const char * hay = (ui->wide_cmd || ui->show_path)
                           ? p->cmdline : p->command;
        if (hay[0] == '\0') {
            hay = p->command;
        }
        /* simple case-insensitive substring match fallback */
        const char * needle = opts->filter;
        size_t nlen = strlen(needle);
        const char * hayp = hay;
        int found = 0;
        while (*hayp) {
            const char * a = hayp;
            const char * b = needle;
            while (*b &&
                   tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
                a++;
                b++;
            }
            if (*b == '\0' && nlen > 0) {
                found = 1;
                break;
            }
            hayp++;
        }
        (void)nlen;
        if (!found) {
            return false;
        }
    }

    /* PID filter list */
    if (opts->pid_filter_set) {
        int match = 0;
        for (int j = 0; j < opts->pid_filter_count; j++) {
            if (p->pid == opts->pid_filter[j]) {
                match = 1;
                break;
            }
        }
        if (!match) {
            return false;
        }
    }

    /* user filter */
    if (opts->user_filter_set && opts->user_filter[0] != '\0') {
        if (strcmp(p->user, opts->user_filter) != 0) {
            return false;
        }
    }

    return true;
}

/* ---- help/version/args ---- */

static void _htop_print_help(void)
{
    htop_printf(
        "Usage: htop [options]\n"
        "Interactive process viewer for Unix-like systems.\n"
        "\n"
        "Options:\n"
        "  -b, --batch            run in batch mode (non-interactive, no screen)\n"
        "  -d, --delay=SEC        delay between updates in seconds (float)\n"
        "  -n, --iterations=N     exit after N iterations (batch mode)\n"
        "      --fields=LIST      comma-separated list of columns to show\n"
        "                         (PID PPID USER PRI NI VIRT RES SHR S %%CPU %%MEM\n"
        "                         TIME+ COMMAND)\n"
        "      --filter=PATTERN   show only processes whose command matches PATTERN\n"
        "      --follow=PID       start in follow mode on PID\n"
        "  -s, --sort-key=FIELD   sort by FIELD\n"
        "  -r, --reverse          reverse the sort order\n"
        "  -t, --tree             show processes in a tree view\n"
        "  -s                     (also) toggle kernel threads visibility\n"
        "  -u, --user=USER        show only USER's processes\n"
        "  -p, --pid=N            show only PIDs (comma-separated)\n"
        "  -T                     show threads as separate rows\n"
        "  -w, --wide             show full command line\n"
        "  -i, --no-interaction   do not start the interactive UI (implies -b)\n"
        "  -W, --width=N          force output width to N columns\n"
        "      --no-color         disable ANSI colors\n"
        "      --limit-rows=N     limit the process list to N rows\n"
        "      --show-cumulative  %%CPU includes child processes (approx)\n"
        "      --show-program-path show full program path in COMMAND\n"
        "      --help             display this help and exit\n"
        "      --version          output version information and exit\n"
        "\n"
        "Interactive keys:\n"
        "  F1, ?        help                F2        setup\n"
        "  F3, /        filter processes    F4        sort (menu)\n"
        "  F5           tree view toggle    F6        follow selected\n"
        "  F7, k        send signal         F8, +/-   change nice value\n"
        "  F9, k        send signal (menu)  F10       quit\n"
        "  F11          decrease delay      F12       increase delay\n"
        "  r            reverse sort        s         toggle kernel threads\n"
        "  T            toggle thread view  w         toggle wide command\n"
        "  c            toggle command name/line\n"
        "  arrows/PgUp/PgDn/Home/End  move selection\n"
        "  Tab          cycle sort column\n"
        "  Ctrl+L       force redraw        Esc/q     quit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

static void _htop_print_version(void)
{
    htop_printf("htop %s\n", HTOP_VERSION_STR);
    htop_printf("Copyright (C) 2025-2026 Yezc\n");
    htop_printf("License MIT: <https://mit-license.org/>\n");
    htop_printf("This is free software: you are free to change and redistribute it.\n");
    htop_printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static int _htop_get_opt_value(const char * arg, int * i, int argc,
                               char ** argv, const char ** out)
{
    /* returns 0 on success; supports --opt=VALUE and --opt VALUE */
    const char * eq = strchr(arg, '=');
    if (eq) {
        *out = eq + 1;
        return 0;
    }
    if (*i + 1 < argc) {
        *out = argv[++(*i)];
        return 0;
    }
    return -1;
}

static int _htop_parse_args(int argc, char ** argv, htop_opts_t * opts)
{
    if (argc < 1 || !argv || !opts) {
        return -1;
    }

    memset(opts, 0, sizeof(*opts));
    opts->delay = HTOP_DEFAULT_DELAY;
    opts->sort_col = HTOP_COL_CPU;
    opts->width = 0;
    opts->limit_rows = 0;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            if (i + 1 < argc) {
                htop_err_printf("htop: extra operand '%s'\n", argv[i + 1]);
                htop_err_printf("Try 'htop --help' for more information.\n");
                return -1;
            }
            break;
        }

        if (strncmp(arg, "--", 2) == 0) {
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[48];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _htop_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _htop_print_version();
                exit(0);
            }
            if (strcmp(name, "batch") == 0 ||
                strcmp(name, "no-interaction") == 0) {
                opts->batch = 1;
                continue;
            }
            if (strcmp(name, "delay") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--delay' requires an argument\n");
                    return -1;
                }
                opts->delay = _htop_parse_delay(v, HTOP_DEFAULT_DELAY);
                opts->delay_set = 1;
                continue;
            }
            if (strcmp(name, "iterations") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--iterations' requires an argument\n");
                    return -1;
                }
                opts->iterations = _htop_parse_int(v, 0);
                if (opts->iterations < 0) {
                    opts->iterations = 0;
                }
                continue;
            }
            if (strcmp(name, "sort-key") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--sort-key' requires an argument\n");
                    return -1;
                }
                opts->sort_col = _htop_parse_col(v);
                continue;
            }
            if (strcmp(name, "reverse") == 0) {
                opts->reverse_sort = 1;
                continue;
            }
            if (strcmp(name, "tree") == 0) {
                opts->tree_view = 1;
                continue;
            }
            if (strcmp(name, "threads") == 0) {
                opts->show_threads = 1;
                continue;
            }
            if (strcmp(name, "user") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--user' requires an argument\n");
                    return -1;
                }
                strncpy(opts->user_filter, v, sizeof(opts->user_filter) - 1);
                opts->user_filter[sizeof(opts->user_filter) - 1] = '\0';
                opts->user_filter_set = 1;
                continue;
            }
            if (strcmp(name, "pid") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--pid' requires an argument\n");
                    return -1;
                }
                char * vcopy = strdup(v);
                if (!vcopy) {
                    return -1;
                }
                char * tok = strtok(vcopy, ",");
                while (tok && opts->pid_filter_count < HTOP_MAX_PID_FILTER) {
                    opts->pid_filter[opts->pid_filter_count++] =
                        _htop_parse_int(tok, 0);
                    tok = strtok(NULL, ",");
                }
                free(vcopy);
                opts->pid_filter_set = 1;
                continue;
            }
            if (strcmp(name, "filter") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--filter' requires an argument\n");
                    return -1;
                }
                strncpy(opts->filter, v, sizeof(opts->filter) - 1);
                opts->filter[sizeof(opts->filter) - 1] = '\0';
                opts->filter_set = 1;
                continue;
            }
            if (strcmp(name, "follow") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--follow' requires an argument\n");
                    return -1;
                }
                opts->follow_pid = _htop_parse_int(v, 0);
                continue;
            }
            if (strcmp(name, "fields") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--fields' requires an argument\n");
                    return -1;
                }
                (void)v; /* accepted; column set is fixed in this build */
                continue;
            }
            if (strcmp(name, "no-color") == 0) {
                opts->no_color = 1;
                continue;
            }
            if (strcmp(name, "wide") == 0) {
                opts->wide_cmd = 1;
                continue;
            }
            if (strcmp(name, "limit-rows") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--limit-rows' requires an argument\n");
                    return -1;
                }
                opts->limit_rows = _htop_parse_int(v, 0);
                continue;
            }
            if (strcmp(name, "show-cumulative") == 0) {
                opts->show_cumulative = 1;
                continue;
            }
            if (strcmp(name, "show-program-path") == 0) {
                opts->show_path = 1;
                continue;
            }
            if (strcmp(name, "width") == 0) {
                const char * v = NULL;
                if (_htop_get_opt_value(arg, &i, argc, argv, &v) != 0) {
                    htop_err_printf("htop: option '--width' requires an argument\n");
                    return -1;
                }
                opts->width = _htop_parse_int(v, 0);
                continue;
            }

            htop_err_printf("htop: unrecognized option '%s'\n", arg);
            htop_err_printf("Try 'htop --help' for more information.\n");
            return -1;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* short options */
            for (int j = 1; arg[j]; j++) {
                char c = arg[j];
                switch (c) {
                    case 'b':
                    case 'i':
                        opts->batch = 1;
                        break;
                    case 'T':
                        opts->show_threads = 1;
                        break;
                    case 't':
                        opts->tree_view = 1;
                        break;
                    case 'r':
                        opts->reverse_sort = 1;
                        break;
                    case 'h':
                        _htop_print_help();
                        exit(0);
                    case 'V':
                        _htop_print_version();
                        exit(0);
                    case 'w':
                        opts->wide_cmd = 1;
                        break;
                    case 's':
                        /* htop 3.x: -s takes a sort key in some builds;
                         * here: if a value follows, treat as sort key,
                         * otherwise as kernel-thread toggle. */
                        if (arg[j + 1] != '\0') {
                            opts->sort_col = _htop_parse_col(arg + j + 1);
                            j = (int)strlen(arg) - 1;
                        }
                        else if (i + 1 < argc &&
                                 argv[i + 1][0] != '-') {
                            opts->sort_col = _htop_parse_col(argv[++i]);
                        }
                        else {
                            /* no value: kernel-thread toggle off */
                            opts->show_kthreads = 0;
                        }
                        break;
                    case 'd':
                    case 'n':
                    case 'p':
                    case 'u':
                    case 'f':
                    case 'W': {
                        const char * v = NULL;
                        if (arg[j + 1] != '\0') {
                            v = arg + j + 1;
                            j = (int)strlen(arg) - 1;
                        }
                        else if (i + 1 < argc && argv[i + 1][0] != '-') {
                            v = argv[++i];
                        }
                        else {
                            htop_err_printf(
                                "htop: option requires an argument -- '%c'\n", c);
                            return -1;
                        }
                        switch (c) {
                            case 'f':
                                strncpy(opts->filter, v, sizeof(opts->filter) - 1);
                                opts->filter[sizeof(opts->filter) - 1] = '\0';
                                opts->filter_set = 1;
                                break;
                            case 'd':
                                opts->delay = _htop_parse_delay(v, HTOP_DEFAULT_DELAY);
                                opts->delay_set = 1;
                                break;
                            case 'n':
                                opts->iterations = _htop_parse_int(v, 0);
                                if (opts->iterations < 0) {
                                    opts->iterations = 0;
                                }
                                break;
                            case 'p': {
                                char * vcopy = strdup(v);
                                if (!vcopy) {
                                    return -1;
                                }
                                char * tok = strtok(vcopy, ",");
                                while (tok &&
                                       opts->pid_filter_count < HTOP_MAX_PID_FILTER) {
                                    opts->pid_filter[opts->pid_filter_count++] =
                                        _htop_parse_int(tok, 0);
                                    tok = strtok(NULL, ",");
                                }
                                free(vcopy);
                                opts->pid_filter_set = 1;
                                break;
                            }
                            case 'u':
                                strncpy(opts->user_filter, v,
                                        sizeof(opts->user_filter) - 1);
                                opts->user_filter[sizeof(opts->user_filter) - 1] = '\0';
                                opts->user_filter_set = 1;
                                break;
                            case 'W':
                                opts->width = _htop_parse_int(v, 0);
                                break;
                        }
                        break;
                    }
                    default:
                        htop_err_printf("htop: invalid option -- '%c'\n", c);
                        htop_err_printf("Try 'htop --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            htop_err_printf("htop: extra operand '%s'\n", arg);
            htop_err_printf("Try 'htop --help' for more information.\n");
            return -1;
        }
    }

    /* in batch mode, default iterations = 1 */
    if (opts->batch && opts->iterations == 0) {
        opts->iterations = 1;
    }

    return 0;
}

/********************************
 *    main
 ********************************/

/**
 * @brief Perform one sampling iteration: fill sys + proc arrays and
 *        compute per-process CPU%/MEM%.
 */
static int _htop_sample(htop_sys_t * cur, const htop_sys_t * prev,
                        int has_prev, htop_proc_t * procs, int max_procs,
                        int threads)
{
    memset(cur, 0, sizeof(*cur));
    if (_htop_get_sys_info(cur) != 0) {
        return -1;
    }

    int count = _htop_get_procs(procs, max_procs, threads);

    for (int i = 0; i < count; i++) {
        htop_proc_t * pp = &procs[i];
        if (cur->mem_total > 0) {
            pp->mem_pct = (double)pp->res * 1024.0 /
                          (double)cur->mem_total * 100.0;
        }
        else {
            pp->mem_pct = 0.0;
        }
    }

    (void)prev;
    (void)has_prev;
    return count;
}

/**
 * @brief Apply CPU delta between two process snapshots.
 *
 * @param procs    current snapshot
 * @param count    number of entries in procs
 * @param prev     previous snapshot (may be NULL)
 * @param prev_count number of entries in prev
 * @param cpu_delta_total system-wide CPU ticks delta (all CPUs)
 */
static void _htop_apply_cpu_deltas(htop_proc_t * procs, int count,
                                   const htop_proc_t * prev, int prev_count,
                                   unsigned long long cpu_delta_total)
{
    if (count <= 0 || !procs) {
        return;
    }
    for (int i = 0; i < count; i++) {
        htop_proc_t * pp = &procs[i];
        if (prev && prev_count > 0 && cpu_delta_total > 0) {
            int found = -1;
            for (int j = 0; j < prev_count; j++) {
                if (prev[j].pid == pp->pid) {
                    found = j;
                    break;
                }
            }
            if (found >= 0) {
                unsigned long long delta =
                    pp->cpu_time > prev[found].cpu_time
                    ? pp->cpu_time - prev[found].cpu_time
                    : 0;
                /* cpu_time units are ticks (POSIX) or 100ns (Windows);
                 * cpu_delta_total is in the same units, so the ratio is
                 * consistent. */
                pp->cpu_pct = (double)delta / (double)cpu_delta_total * 100.0;
                if (pp->cpu_pct > 100.0 * (double)256.0) {
                    pp->cpu_pct = 100.0 * (double)256.0;
                }
            }
            else {
                pp->cpu_pct = 0.0;
            }
        }
        else {
            /* first sample: use cumulative CPU time as a rough fraction
             * of wall time since process start — set to 0 to avoid a
             * misleading spike. */
            pp->cpu_pct = 0.0;
        }
    }
}

int main(int argc, char ** argv)
{
    htop_opts_t opts;
    if (_htop_parse_args(argc, argv, &opts) != 0) {
        return 1;
    }

    int interactive = !opts.batch;

    /* auto-degrade to batch mode when stdout is not a TTY */
    if (interactive && !_htop_is_tty()) {
        interactive = 0;
        opts.batch = 1;
        if (opts.iterations == 0) {
            opts.iterations = 1;
        }
    }

    /* disable colors when requested, or in batch mode (plain output) */
    if (opts.no_color || !interactive) {
        _htop_disable_colors();
    }

    htop_ui_t ui;
    memset(&ui, 0, sizeof(ui));
    ui.sort_col = opts.sort_col;
    ui.reverse = opts.reverse_sort;
    ui.tree_view = opts.tree_view;
    ui.show_kthreads = 1;
    ui.show_threads = opts.show_threads;
    ui.wide_cmd = opts.wide_cmd;
    ui.show_path = opts.show_path;
    ui.delay = opts.delay;
    ui.follow_pid = opts.follow_pid;
    if (opts.filter_set) {
        strncpy(ui.filter, opts.filter, sizeof(ui.filter) - 1);
        ui.filter[sizeof(ui.filter) - 1] = '\0';
        ui.filter_set = 1;
    }

    /* allocate process arrays */
    htop_proc_t * procs = (htop_proc_t *)malloc(sizeof(htop_proc_t) * HTOP_MAX_PROCS);
    htop_proc_t * prev_procs = (htop_proc_t *)malloc(sizeof(htop_proc_t) * HTOP_MAX_PROCS);
    if (!procs || !prev_procs) {
        htop_err_printf("htop: out of memory\n");
        free(procs);
        free(prev_procs);
        return 1;
    }

    htop_sys_t cur_sys, prev_sys;
    int prev_count = 0;
    int has_prev = 0;
    int iteration = 0;

    _htop_enable_vt();

    if (interactive) {
        _htop_set_raw_mode(1);
        _htop_hide_cursor();
    }

    int should_run = 1;
    int rows = 24, cols = 80;

    while (should_run) {
        _htop_terminal_size(&rows, &cols);
        if (opts.width > 0) {
            cols = opts.width;
        }
        else if (!interactive) {
            /* batch mode: use a wide default so COMMAND is not
             * truncated to a couple of characters */
            cols = 150;
        }

        /* sample */
        htop_sys_t * cur_p = &cur_sys;
        htop_sys_t * prev_p = has_prev ? &prev_sys : NULL;
        int count = _htop_sample(cur_p, prev_p, has_prev,
                                 procs, HTOP_MAX_PROCS,
                                 ui.show_threads);
        if (count < 0) {
            count = 0;
        }

        /* system-wide CPU delta for process CPU% */
        unsigned long long cpu_delta_total = 0;
        if (has_prev) {
            const htop_cpu_ticks_t * c = &cur_sys.cpu;
            const htop_cpu_ticks_t * p = &prev_sys.cpu;
            cpu_delta_total = (c->user + c->nice + c->system + c->idle +
                               c->iowait + c->irq + c->softirq + c->steal)
                             - (p->user + p->nice + p->system + p->idle +
                               p->iowait + p->irq + p->softirq + p->steal);
        }
        _htop_apply_cpu_deltas(procs, count, prev_procs, prev_count,
                               cpu_delta_total);

        /* visibility filter — build a compact visible list in place */
        htop_re_t re;
        int re_valid = 0;
        if (ui.filter_set && ui.filter[0]) {
            if (_htop_re_compile(&re, ui.filter) == 0) {
                re_valid = 1;
            }
            else {
                htop_err_printf("htop: bad filter pattern '%s': %s\n",
                                ui.filter, re.error ? re.error : "?");
            }
        }

        int vis_count = 0;
        for (int i = 0; i < count; i++) {
            if (_htop_proc_visible(&procs[i], &ui, &opts,
                                   re_valid ? &re : NULL)) {
                if (vis_count != i) {
                    procs[vis_count] = procs[i];
                }
                vis_count++;
            }
        }
        count = vis_count;

        if (re_valid) {
            _htop_re_free(&re);
        }

        /* tree view: stable-sort by PPID then PID so children appear
         * right below their parent (best-effort depth-1 grouping) */
        if (ui.tree_view) {
            _htop_sort_procs(procs, count, HTOP_COL_PPID, 0);
            _htop_sort_procs(procs, count, HTOP_COL_PID, 0);
            g_htop_sort_col = ui.sort_col;
            g_htop_reverse = ui.reverse;
        }
        else {
            _htop_sort_procs(procs, count, ui.sort_col, ui.reverse);
        }

        /* render */
        if (interactive) {
            htop_printf("\033[H\033[2J");
        }

        /* batch mode: emit a plain header without ANSI */
        if (!interactive && iteration > 0) {
            htop_printf("\n");
        }

        _htop_render_header(&cur_sys, has_prev ? &prev_sys : NULL,
                            has_prev, &ui, cols);
        _htop_render_col_header(&ui, cols);

        /* batch mode lists every process (no screen-height cap);
         * interactive mode caps at the visible area */
        int max_rows;
        if (interactive) {
            max_rows = rows - 8;
            if (max_rows < 1) max_rows = 1;
        }
        else {
            max_rows = count;
        }
        if (opts.limit_rows > 0 && opts.limit_rows < max_rows) {
            max_rows = opts.limit_rows;
        }

        for (int i = 0; i < count && i < max_rows; i++) {
            _htop_render_row(&procs[i], &cur_sys, &ui, cols,
                             interactive && i == ui.selected);
        }

        /* status line (interactive) */
        if (interactive) {
            char filt[HTOP_FILTER_MAX + 4];
            if (ui.filter_set && ui.filter[0]) {
                snprintf(filt, sizeof(filt), " [%s]", ui.filter);
            }
            else {
                filt[0] = '\0';
            }
            htop_printf(
                "%s"
                " F1 Help  F2 Setup  F3 Filter  F4 Sort  F5 Tree  F6 Follow"
                "  F7/F9 Kill  F8 Nice  F10 Quit  F11/F12 Delay  r Reverse"
                "  s Threads  w Wide  %s"
                "%s\n",
                g_htop_c_rev, "", g_htop_c_reset);
            htop_printf(
                " %d process(es) shown (of %d total)"
                "  sort: %s%s  delay: %.1fs%s\n",
                count, count,
                _htop_col_name(ui.sort_col),
                ui.reverse ? " (rev)" : "",
                ui.delay, filt);
        }
        htop_fflush(stdout);

        /* swap snapshots for next iteration */
        memcpy(prev_procs, procs, sizeof(htop_proc_t) * (size_t)count);
        prev_count = count;
        prev_sys = cur_sys;
        has_prev = 1;
        iteration++;

        /* batch mode: count down iterations */
        if (!interactive) {
            if (opts.iterations > 0 && iteration >= opts.iterations) {
                break;
            }
            _htop_sleep_ms((unsigned long)(ui.delay * 1000.0));
            continue;
        }

        /* interactive: poll for keys until the delay elapses */
        unsigned long long t0 = _htop_time_now_ms();
        unsigned long delay_ms = (unsigned long)(ui.delay * 1000.0);

        int refreshed = 0;
        while (should_run && _htop_time_now_ms() - t0 < delay_ms) {
            if (!_htop_kbhit()) {
                _htop_sleep_ms(20);
                continue;
            }
            int c = _htop_getch();

            /* function keys arrive as ESC [ 1 ~ style sequences */
            if (c == 27) {
                int c2 = _htop_getch();
                if (c2 == '[') {
                    int c3 = _htop_getch();
                    if (c3 == '1') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F1 help — show help line at bottom */
                            htop_printf("\033[%dA\033[2K", rows - 1);
                            htop_printf(
                                "%s Help: "
                                "F3 filter  F4 sort  F5 tree  F6 follow "
                                "F7 kill  F8 nice  F10/Escape quit "
                                "Tab cycle-sort  arrows move "
                                "Ctrl+L redraw %s",
                                g_htop_c_rev, g_htop_c_reset);
                            htop_fflush(stdout);
                        }
                        else if (c4 == ';') {
                            /* some terminals send ESC [ 1;2 P etc for F13+ */
                            int seq = 0;
                            for (;;) {
                                int c5 = _htop_getch();
                                if (c5 == ';' || (c5 >= '0' && c5 <= '9')) {
                                    seq = seq * 10 + (c5 - '0');
                                }
                                else {
                                    break;
                                }
                            }
                            (void)seq;
                        }
                    }
                    else if (c3 == '2') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F2 setup */
                            _htop_run_setup(&ui);
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '3') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F3 filter */
                            char pat[HTOP_FILTER_MAX];
                            if (ui.filter[0]) {
                                strncpy(pat, ui.filter, sizeof(pat) - 1);
                                pat[sizeof(pat) - 1] = '\0';
                            }
                            else {
                                pat[0] = '\0';
                            }
                            if (_htop_dialog_text("Filter",
                                                  "Regex filter on command:",
                                                  pat, sizeof(pat)) == 0) {
                                strncpy(ui.filter, pat,
                                        sizeof(ui.filter) - 1);
                                ui.filter[sizeof(ui.filter) - 1] = '\0';
                                if (pat[0]) {
                                    ui.filter_set = 1;
                                }
                                else {
                                    ui.filter_set = 0;
                                }
                            }
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '4') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F4 sort menu */
                            static const char * cols_menu[] = {
                                "PID", "PPID", "USER", "PRI", "NI", "VIRT",
                                "RES", "SHR", "S", "%CPU", "%MEM", "TIME+",
                                "COMMAND"
                            };
                            int sc = _htop_dialog_choice(
                                "Sort by", "Select a column:",
                                cols_menu,
                                (int)(sizeof(cols_menu) /
                                      sizeof(cols_menu[0])),
                                (int)ui.sort_col);
                            if (sc >= 0 && sc < HTOP_NUM_COLUMNS) {
                                ui.sort_col = (htop_col_t)sc;
                            }
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '5') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            ui.tree_view = !ui.tree_view;
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '6') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F6 follow selected */
                            if (ui.selected >= 0 &&
                                ui.selected < count) {
                                ui.follow_pid = procs[ui.selected].pid;
                            }
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '7') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F7 kill with default signal menu */
                            static const char * sigs[] = {
                                "SIGHUP  (1)", "SIGINT  (2)", "SIGQUIT (3)",
                                "SIGABRT (6)", "SIGKILL (9)", "SIGTERM (15)",
                                "SIGCONT (18)", "SIGSTOP (19)"
                            };
                            static const int sigvals[] = {
                                1, 2, 3, 6, 9, 15, 18, 19
                            };
                            int nsig = (int)(sizeof(sigs) / sizeof(sigs[0]));
                            if (ui.selected >= 0 && ui.selected < count) {
                                int idx = _htop_dialog_choice(
                                    "Send signal",
                                    "Select signal:",
                                    sigs, nsig, 4);
                                if (idx >= 0) {
                                    int rc = _htop_send_signal(
                                        procs[ui.selected].pid,
                                        sigvals[idx]);
                                    if (rc != 0) {
                                        htop_err_printf(
                                            "htop: signal %s to pid %d failed\n",
                                            _htop_signal_name(sigvals[idx]),
                                            procs[ui.selected].pid);
                                    }
                                }
                            }
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '8') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* F8 change nice value */
                            if (ui.selected >= 0 && ui.selected < count) {
                                int nv = _htop_dialog_int(
                                    "Change nice value",
                                    "New nice value (-20..19):",
                                    procs[ui.selected].nice);
                                if (nv >= -20 && nv <= 19) {
                                    int rc = _htop_set_nice(
                                        procs[ui.selected].pid, nv);
                                    if (rc != 0) {
                                        htop_err_printf(
                                            "htop: setpriority on pid %d failed\n",
                                            procs[ui.selected].pid);
                                    }
                                }
                            }
                            refreshed = 1;
                        }
                    }
                    /* F10-F12 arrive as ESC [ 21~ / ESC [ 23~ / ESC [ 24~
                     * (or 25~) on most terminals; a few use the
                     * ESC [ 1;2P style, which the ESC [ 1 branch above
                     * already drains. */
                    else if (c3 == '2') {
                        int c4 = _htop_getch();
                        if (c4 == '1') {
                            int c5 = _htop_getch();
                            if (c5 == '~') {
                                /* F10 quit */
                                should_run = 0;
                            }
                        }
                        else if (c4 == '3') {
                            int c5 = _htop_getch();
                            if (c5 == '~') {
                                /* F11 decrease delay */
                                ui.delay -= 0.5;
                                if (ui.delay < 0.2) ui.delay = 0.2;
                                refreshed = 1;
                            }
                        }
                        else if (c4 == '4' || c4 == '5') {
                            int c5 = _htop_getch();
                            if (c5 == '~') {
                                /* F12 increase delay */
                                ui.delay += 0.5;
                                if (ui.delay > 60.0) ui.delay = 60.0;
                                refreshed = 1;
                            }
                        }
                    }
                    else if (c3 == 'A' || c3 == 'B' || c3 == 'C' || c3 == 'D') {
                        /* arrow keys */
                        int page = rows - 10;
                        if (page < 1) page = 1;
                        if (c3 == 'A') {
                            ui.selected--;
                            if (ui.selected < 0) ui.selected = 0;
                        }
                        else if (c3 == 'B') {
                            ui.selected++;
                            if (ui.selected >= count) ui.selected = count - 1;
                        }
                        else if (c3 == 'D') {
                            ui.selected -= page;
                            if (ui.selected < 0) ui.selected = 0;
                        }
                        else if (c3 == 'C') {
                            ui.selected += page;
                            if (ui.selected >= count) ui.selected = count - 1;
                        }
                        refreshed = 1;
                    }
                    else if (c3 == '5') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* Home */
                            ui.selected = 0;
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '6') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* End */
                            ui.selected = count - 1;
                            refreshed = 1;
                        }
                    }
                    else if (c3 == '3') {
                        int c4 = _htop_getch();
                        if (c4 == '~') {
                            /* Delete — no-op */
                        }
                    }
                }
                else {
                    /* plain Esc: quit */
                    should_run = 0;
                }
                continue;
            }

            switch (c) {
                case 'q':
                case 'Q':
                    should_run = 0;
                    break;
                case ' ':
                    /* immediate refresh */
                    should_run = 1;
                    goto refresh;
                case 'r':
                case 'R':
                    ui.reverse = !ui.reverse;
                    refreshed = 1;
                    break;
                case 's':
                    ui.show_kthreads = !ui.show_kthreads;
                    refreshed = 1;
                    break;
                case 'T':
                    ui.show_threads = !ui.show_threads;
                    refreshed = 1;
                    break;
                case 'w':
                case 'W':
                    ui.wide_cmd = !ui.wide_cmd;
                    refreshed = 1;
                    break;
                case 'c':
                    ui.wide_cmd = !ui.wide_cmd;
                    refreshed = 1;
                    break;
                case '/':
                    {
                        char pat[HTOP_FILTER_MAX];
                        if (ui.filter[0]) {
                            strncpy(pat, ui.filter, sizeof(pat) - 1);
                            pat[sizeof(pat) - 1] = '\0';
                        }
                        else {
                            pat[0] = '\0';
                        }
                        if (_htop_dialog_text("Filter",
                                              "Regex filter on command:",
                                              pat, sizeof(pat)) == 0) {
                            strncpy(ui.filter, pat, sizeof(ui.filter) - 1);
                            ui.filter[sizeof(ui.filter) - 1] = '\0';
                            if (pat[0]) {
                                ui.filter_set = 1;
                            }
                            else {
                                ui.filter_set = 0;
                            }
                        }
                        refreshed = 1;
                    }
                    break;
                case '\t':
                    /* cycle sort column */
                    {
                        int ncol = HTOP_NUM_COLUMNS;
                        ui.sort_col = (htop_col_t)((ui.sort_col + 1) % ncol);
                    }
                    refreshed = 1;
                    break;
                case 3:             /* Ctrl+C: quit */
                    should_run = 0;
                    break;
                case 12:            /* Ctrl+L */
                    refreshed = 1;
                    break;
                case 1:             /* Ctrl+A: top */
                    ui.selected = 0;
                    refreshed = 1;
                    break;
                case 5:             /* Ctrl+E: bottom */
                    ui.selected = count - 1;
                    refreshed = 1;
                    break;
                case 2:             /* Ctrl+B: page up */
                    ui.selected -= (rows - 10);
                    if (ui.selected < 0) ui.selected = 0;
                    refreshed = 1;
                    break;
                case 6:             /* Ctrl+F: page down */
                    ui.selected += (rows - 10);
                    if (ui.selected >= count) ui.selected = count - 1;
                    refreshed = 1;
                    break;
                case 'k':
                case 'j':
                case 'h':
                case 'l':
                    /* vi-style: k/h = up, j/l = down; but k also means
                     * "kill" in htop.  We follow htop: k opens the kill
                     * menu, j moves down, h/l are no-ops. */
                    if (c == 'j') {
                        ui.selected++;
                        if (ui.selected >= count) ui.selected = count - 1;
                    }
                    else if (c == 'k') {
                        static const char * sigs[] = {
                            "SIGHUP  (1)", "SIGINT  (2)", "SIGQUIT (3)",
                            "SIGABRT (6)", "SIGKILL (9)", "SIGTERM (15)",
                            "SIGCONT (18)", "SIGSTOP (19)"
                        };
                        static const int sigvals[] = {
                            1, 2, 3, 6, 9, 15, 18, 19
                        };
                        int nsig = (int)(sizeof(sigs) / sizeof(sigs[0]));
                        if (ui.selected >= 0 && ui.selected < count) {
                            int idx = _htop_dialog_choice(
                                "Send signal", "Select signal:",
                                sigs, nsig, 4);
                            if (idx >= 0) {
                                int rc = _htop_send_signal(
                                    procs[ui.selected].pid, sigvals[idx]);
                                if (rc != 0) {
                                    htop_err_printf(
                                        "htop: signal %s to pid %d failed\n",
                                        _htop_signal_name(sigvals[idx]),
                                        procs[ui.selected].pid);
                                }
                            }
                        }
                    }
                    refreshed = 1;
                    break;
                case 'K':
                    ui.selected--;
                    if (ui.selected < 0) ui.selected = 0;
                    refreshed = 1;
                    break;
                case '+':
                    if (ui.selected >= 0 && ui.selected < count) {
                        int nv = procs[ui.selected].nice - 1;
                        if (nv < -20) nv = -20;
                        (void)_htop_set_nice(procs[ui.selected].pid, nv);
                    }
                    refreshed = 1;
                    break;
                case '-':
                    if (ui.selected >= 0 && ui.selected < count) {
                        int nv = procs[ui.selected].nice + 1;
                        if (nv > 19) nv = 19;
                        (void)_htop_set_nice(procs[ui.selected].pid, nv);
                    }
                    refreshed = 1;
                    break;
                case 'n':
                case 'N':
                    if (ui.selected >= 0 && ui.selected < count) {
                        int nv = _htop_dialog_int(
                            "Change nice value",
                            "New nice value (-20..19):",
                            procs[ui.selected].nice);
                        if (nv >= -20 && nv <= 19) {
                            (void)_htop_set_nice(procs[ui.selected].pid, nv);
                        }
                    }
                    refreshed = 1;
                    break;
                case 'g':
                    ui.selected = 0;
                    refreshed = 1;
                    break;
                case 'G':
                    ui.selected = count - 1;
                    refreshed = 1;
                    break;
                default:
                    break;
            }

            if (should_run && refreshed) {
                /* re-sample immediately */
                break;
            }
        }

refresh:
        if (!should_run) {
            break;
        }
        if (refreshed) {
            /* force the next sampling immediately */
            continue;
        }
    }

    if (interactive) {
        _htop_show_cursor();
        _htop_set_raw_mode(0);
        htop_printf("\033[2J\033[H");
        htop_fflush(stdout);
    }

    free(procs);
    free(prev_procs);
    return 0;
}
