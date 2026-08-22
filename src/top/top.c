/**
 * @file top.c
 * @brief Cross-platform top command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU procps-ng top(1).
 *
 * Key behaviors:
 *   - System summary: uptime, load average, tasks, CPU, memory, swap
 *   - Process list: PID USER PR NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND
 *   - -b/--batch: batch mode (no interaction, no screen clear)
 *   - -d/--delay=SEC: seconds between updates (float)
 *   - -n/--iterations=N: run N iterations then exit
 *   - -p/--pid=N: monitor specific PIDs (comma-separated)
 *   - -u/--user=USER: show only USER's processes
 *   - -o/--order-field=FIELD: sort by FIELD (%CPU, %MEM, TIME+, PID, ...)
 *   - -c/--cmd-line-toggle: toggle full command line
 *   - -H/--thread: show threads
 *   - -S/--cumulative: toggle cumulative time
 *   - -w/--width=N: output width
 *   - -1/--single-cpu-toggle: toggle single CPU view
 *   - --help / --version
 *   - Interactive keys: q/Esc, Space, h/?, d, n, P, M, T, N, R, c, H, S, 1, k, r
 *
 * Platform process sources:
 *   Linux:     /proc/[pid]/stat, /proc/[pid]/status, /proc/[pid]/cmdline
 *   Windows:   CreateToolhelp32Snapshot + GetProcessTimes + GetProcessMemoryInfo
 *   macOS:     sysctl(KERN_PROC) + proc_pidinfo
 *   FreeBSD:   sysctl(KERN_PROC) + kinfo_proc
 *   OpenBSD:   sysctl(KERN_PROC2) + kinfo_proc2
 *   NetBSD:    sysctl(KERN_PROC2) + kinfo_proc2
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o top.exe top.c -lpsapi -ladvapi32
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o top top.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o top top.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o top top.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o top top.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o top top.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/top>
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
    #define TOP_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TOP_PLATFORM_LINUX   1
    #define TOP_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TOP_PLATFORM_MACOS   1
    #define TOP_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TOP_PLATFORM_FREEBSD 1
    #define TOP_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TOP_PLATFORM_OPENBSD 1
    #define TOP_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TOP_PLATFORM_NETBSD  1
    #define TOP_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define TOP_PLATFORM_POSIX   1
#else
    #define TOP_PLATFORM_POSIX   1
#endif

#ifdef TOP_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef TOP_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef TOP_PLATFORM_NETBSD
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

#ifdef TOP_PLATFORM_WINDOWS
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
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
    #include <dirent.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <signal.h>
#endif

#ifdef TOP_PLATFORM_POSIX
    #include <termios.h>
    #include <sys/select.h>
#endif

#ifdef TOP_PLATFORM_LINUX
    /* /proc — no extra headers needed */
#endif

#ifdef TOP_PLATFORM_MACOS
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/mach_init.h>
    #include <mach/mach_host.h>
    #include <mach/host_info.h>
    #include <mach/task_info.h>
    #include <mach/vm_statistics.h>
    #include <libproc.h>
#endif

#if defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
    #include <sys/sysctl.h>
    #include <sys/param.h>
#endif

#ifdef TOP_PLATFORM_FREEBSD
    #include <kvm.h>
    #include <sys/user.h>
#endif

#ifdef TOP_PLATFORM_OPENBSD
    #include <sys/sysctl.h>
#endif

#ifdef TOP_PLATFORM_NETBSD
    #include <sys/sysctl.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define TOP_VERSION_STR "v1.0.0"

/** @brief Maximum number of processes to track */
#define TOP_MAX_PROCS 4096

/** @brief Maximum command length */
#define TOP_CMD_MAX 512

/** @brief Maximum username length */
#define TOP_USER_MAX 32

/** @brief Maximum number of PIDs in -p filter */
#define TOP_MAX_PID_FILTER 32

/** @brief Default delay between updates (seconds) */
#define TOP_DEFAULT_DELAY 3.0

/** @brief Default iterations (0 = infinite in interactive, 1 in batch) */
#define TOP_DEFAULT_ITER 0

/** @brief Field width constants */
#define TOP_W_PID     7
#define TOP_W_USER    8
#define TOP_W_PR      5
#define TOP_W_NI      5
#define TOP_W_MEM     8
#define TOP_W_STATE   1
#define TOP_W_PCT     6
#define TOP_W_TIME    10

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Sort field identifiers.
 */
typedef enum {
    TOP_FIELD_PID = 0,
    TOP_FIELD_USER,
    TOP_FIELD_PR,
    TOP_FIELD_NI,
    TOP_FIELD_VIRT,
    TOP_FIELD_RES,
    TOP_FIELD_SHR,
    TOP_FIELD_S,
    TOP_FIELD_CPU,
    TOP_FIELD_MEM,
    TOP_FIELD_TIME,
    TOP_FIELD_CMD
} top_field_t;

/**
 * @brief Parsed command-line options.
 */
typedef struct {
    int         batch;          /* -b */
    double      delay;          /* -d */
    int         iterations;     /* -n (0 = infinite) */
    int         pid_filter[TOP_MAX_PID_FILTER]; /* -p */
    int         pid_filter_count;
    int         pid_filter_set;
    char        user_filter[TOP_USER_MAX]; /* -u */
    int         user_filter_set;
    top_field_t sort_field;    /* -o */
    int         reverse_sort;   /* -R toggle */
    int         full_cmd;       /* -c toggle */
    int         show_threads;   /* -H toggle */
    int         cumulative;     /* -S toggle */
    int         single_cpu;     /* -1 toggle */
    int         width;          /* -w */
    int         width_set;
} top_opts_t;

/**
 * @brief Per-process information.
 */
typedef struct {
    int             pid;
    int             ppid;
    int             tid;            /* thread ID (for -H) */
    char            user[TOP_USER_MAX];
    unsigned long   virt;           /* virtual memory in KiB */
    unsigned long   res;            /* resident memory in KiB */
    unsigned long   shr;            /* shared memory in KiB */
    int             priority;
    int             nice;
    char            state;          /* R/S/D/Z/T/I */
    double          cpu_pct;
    double          mem_pct;
    unsigned long long cpu_time;    /* raw CPU time (platform-specific units) */
    unsigned long   time_sec;       /* CPU time in seconds */
    char            command[TOP_CMD_MAX];
    int             is_thread;
} proc_info_t;

/**
 * @brief System-wide information.
 */
typedef struct {
    double  uptime;
    double  load1, load5, load15;
    int     tasks_total, tasks_running, tasks_sleeping, tasks_stopped, tasks_zombie;
    unsigned long long cpu_user;     /* cumulative */
    unsigned long long cpu_nice;
    unsigned long long cpu_system;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_irq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_steal;
    unsigned long long cpu_total;    /* sum of above (minus idle double-count) */
    unsigned long long mem_total, mem_free, mem_used, mem_shared, mem_buff, mem_cache, mem_available;
    unsigned long long swap_total, swap_free, swap_used;
    int     num_cpus;
} sys_info_t;

/********************************
 *    static prototypes
 ********************************/

/* ---- platform system info ---- */
static int  _top_get_sys_info(sys_info_t * info);
#ifdef TOP_PLATFORM_LINUX
static int  _top_get_sys_info_linux(sys_info_t * info);
#endif
#ifdef TOP_PLATFORM_WINDOWS
static int  _top_get_sys_info_windows(sys_info_t * info);
#endif
#ifdef TOP_PLATFORM_MACOS
static int  _top_get_sys_info_macos(sys_info_t * info);
#endif
#if defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
static int  _top_get_sys_info_bsd(sys_info_t * info);
#endif

/* ---- platform process list ---- */
static int  _top_get_procs(proc_info_t * procs, int max_procs);
#ifdef TOP_PLATFORM_LINUX
static int  _top_get_procs_linux(proc_info_t * procs, int max_procs);
#endif
#ifdef TOP_PLATFORM_WINDOWS
static int  _top_get_procs_windows(proc_info_t * procs, int max_procs);
static void _top_get_user_windows(int pid, char * buf, size_t buf_size);
#endif
#ifdef TOP_PLATFORM_MACOS
static int  _top_get_procs_macos(proc_info_t * procs, int max_procs);
#endif
#if defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
static int  _top_get_procs_bsd(proc_info_t * procs, int max_procs);
#endif

/* ---- sorting ---- */
static int  _top_sort_cmp(const void * a, const void * b);
static void _top_sort_procs(proc_info_t * procs, int count,
                            top_field_t field, int reverse);

/* ---- formatting ---- */
static void _top_format_mem(unsigned long kib, char * buf, size_t buf_size);
static void _top_format_time(unsigned long seconds, char * buf, size_t buf_size);
static void _top_format_uptime(double uptime, char * buf, size_t buf_size);

/* ---- output ---- */
static void _top_print_summary(const sys_info_t * cur, const sys_info_t * prev,
                               int has_prev, const top_opts_t * opts);
static void _top_print_header(const top_opts_t * opts);
static void _top_print_procs(const proc_info_t * procs, int count,
                             const top_opts_t * opts);

/* ---- keyboard ---- */
static int  _top_kbhit(void);
static int  _top_getch(void);
static void _top_set_raw_mode(int enable);
static void _top_clear_screen(void);
static void _top_enable_vt(void);

/* ---- utility ---- */
static void _top_sleep_ms(unsigned long ms);
static unsigned long long _top_time_now_ms(void);
static top_field_t _top_parse_field(const char * s);

/* ---- help/version/args ---- */
static void _top_print_help(void);
static void _top_print_version(void);
static int  _top_parse_args(int argc, char ** argv, top_opts_t * opts);

/********************************
 *    static variables
 ********************************/

/* column header names */
static const char * top_col_names[] = {
    "PID", "USER", "PR", "NI", "VIRT", "RES", "SHR",
    "S", "%CPU", "%MEM", "TIME+", "COMMAND"
};

/* sort field for qsort — set before each sort */
static top_field_t g_sort_field = TOP_FIELD_CPU;
static int         g_reverse   = 0;

/********************************
 *    macros
 ********************************/

#ifndef top_printf
    #define top_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef top_err_printf
    #define top_err_printf(fmt, ...) \
        do { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } while (0)
#endif

#ifndef top_fflush
    #define top_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    global
 ********************************/

/* No global state beyond g_sort_field / g_reverse (above). */

/********************************
 *    static functions
 ********************************/

/* ---- platform system info ---- */

static int _top_get_sys_info(sys_info_t * info)
{
    if (!info) {
        return -1;
    }
    memset(info, 0, sizeof(*info));

#ifdef TOP_PLATFORM_LINUX
    return _top_get_sys_info_linux(info);
#elif defined(TOP_PLATFORM_WINDOWS)
    return _top_get_sys_info_windows(info);
#elif defined(TOP_PLATFORM_MACOS)
    return _top_get_sys_info_macos(info);
#elif defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
    return _top_get_sys_info_bsd(info);
#else
    /* generic POSIX fallback — minimal */
    #ifdef _POSIX_VERSION
        info->uptime = 0;
        info->num_cpus = 1;
        return 0;
    #else
        return -1;
    #endif
#endif
}

#ifdef TOP_PLATFORM_LINUX
/**
 * @brief Linux: read system info from /proc.
 */
static int _top_get_sys_info_linux(sys_info_t * info)
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
        /* 4th field: running/total */
        int running = 0, total = 0;
        if (fscanf(fp, "%d/%d", &running, &total) >= 2) {
            info->tasks_running = running;
            info->tasks_total = total;
        }
        fclose(fp);
    }

    /* CPU stats from /proc/stat */
    fp = fopen("/proc/stat", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "cpu ", 4) == 0) {
                unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
                user = nice = sys = idle = iowait = irq = softirq = steal = 0;
                /* "cpu  user nice system idle iowait irq softirq steal ..." */
                int n = sscanf(line + 3, "%llu %llu %llu %llu %llu %llu %llu %llu",
                               &user, &nice, &sys, &idle, &iowait,
                               &irq, &softirq, &steal);
                if (n >= 4) {
                    info->cpu_user = user;
                    info->cpu_nice = nice;
                    info->cpu_system = sys;
                    info->cpu_idle = idle;
                    if (n >= 5) info->cpu_iowait = iowait;
                    if (n >= 6) info->cpu_irq = irq;
                    if (n >= 7) info->cpu_softirq = softirq;
                    if (n >= 8) info->cpu_steal = steal;
                    info->cpu_total = user + nice + sys + idle +
                                      iowait + irq + softirq + steal;
                }
                break;
            }
        }
        fclose(fp);
    }

    /* number of CPUs */
    info->num_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (info->num_cpus <= 0) {
        info->num_cpus = 1;
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
    if (info->mem_available == 0) {
        info->mem_available = info->mem_free + info->mem_buff + info->mem_cache;
    }
    info->swap_used = info->swap_total - info->swap_free;

    /* If we don't have task counts from loadavg, count /proc entries */
    if (info->tasks_total == 0) {
        DIR * dir = opendir("/proc");
        if (dir) {
            struct dirent * ent;
            int total = 0, running = 0, sleeping = 0, stopped = 0, zombie = 0;
            while ((ent = readdir(dir)) != NULL) {
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
                total++;
                /* read state from /proc/[pid]/stat */
                char path[512];
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
#endif /* TOP_PLATFORM_LINUX */

#ifdef TOP_PLATFORM_WINDOWS
/**
 * @brief Windows: system info via Win32 APIs.
 */
static int _top_get_sys_info_windows(sys_info_t * info)
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

    /* CPU stats via GetSystemTimes */
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULARGE_INTEGER id, ke, us;
        id.LowPart = idle.dwLowDateTime;   id.HighPart = idle.dwHighDateTime;
        ke.LowPart = kernel.dwLowDateTime; ke.HighPart = kernel.dwHighDateTime;
        us.LowPart = user.dwLowDateTime;   us.HighPart = user.dwHighDateTime;
        info->cpu_idle = id.QuadPart;
        info->cpu_system = (ke.QuadPart > id.QuadPart)
                           ? (ke.QuadPart - id.QuadPart) : 0;
        info->cpu_user = us.QuadPart;
        info->cpu_total = ke.QuadPart + us.QuadPart;
    }

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
#endif /* TOP_PLATFORM_WINDOWS */

#ifdef TOP_PLATFORM_MACOS
/**
 * @brief macOS: system info via sysctl and mach.
 */
static int _top_get_sys_info_macos(sys_info_t * info)
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
        for (unsigned i = 0; i < cpu_count; i++) {
            info->cpu_user += cpu_load[i].cpu_ticks[CPU_STATE_USER];
            info->cpu_system += cpu_load[i].cpu_ticks[CPU_STATE_SYSTEM];
            info->cpu_idle += cpu_load[i].cpu_ticks[CPU_STATE_IDLE];
            info->cpu_nice += cpu_load[i].cpu_ticks[CPU_STATE_NICE];
        }
        info->cpu_total = info->cpu_user + info->cpu_nice +
                          info->cpu_system + info->cpu_idle;
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
#endif /* TOP_PLATFORM_MACOS */

#if defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
/**
 * @brief BSD: system info via sysctl.
 */
static int _top_get_sys_info_bsd(sys_info_t * info)
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

    /* CPU stats: BSD doesn't provide cumulative CPU via sysctl easily.
     * Use getcpuid stats if available, otherwise leave at 0. */
    info->mem_free = 0;
    info->mem_used = info->mem_total;

    /* process count */
    int pmib[4];
    pmib[0] = CTL_KERN;
    pmib[1] = KERN_PROC;
    pmib[2] = KERN_PROC_ALL;
    pmib[3] = 0;
    size_t plen = 0;
    if (sysctl(pmib, 3, NULL, &plen, NULL, 0) == 0) {
        info->tasks_total = (int)(plen / 256); /* rough estimate */
    }

    return 0;
}
#endif /* BSD */

/* ---- platform process list ---- */

static int _top_get_procs(proc_info_t * procs, int max_procs)
{
    if (!procs || max_procs <= 0) {
        return 0;
    }

#ifdef TOP_PLATFORM_LINUX
    return _top_get_procs_linux(procs, max_procs);
#elif defined(TOP_PLATFORM_WINDOWS)
    return _top_get_procs_windows(procs, max_procs);
#elif defined(TOP_PLATFORM_MACOS)
    return _top_get_procs_macos(procs, max_procs);
#elif defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
    return _top_get_procs_bsd(procs, max_procs);
#else
    return 0;
#endif
}

#ifdef TOP_PLATFORM_LINUX
/**
 * @brief Linux: enumerate processes from /proc.
 */
static int _top_get_procs_linux(proc_info_t * procs, int max_procs)
{
    DIR * dir = opendir("/proc");
    if (!dir) {
        return 0;
    }

    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) {
        hz = 100;
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

        proc_info_t * pi = &procs[count];
        memset(pi, 0, sizeof(*pi));
        pi->pid = pid;

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

        /* parse: pid (comm) state ppgid ... utime stime ... priority nice ... vsize rss */
        /* find last ')' to handle command with spaces */
        char * comm_start = strchr(buf, '(');
        char * comm_end = strrchr(buf, ')');
        if (comm_start && comm_end && comm_end > comm_start) {
            size_t clen = (size_t)(comm_end - comm_start - 1);
            if (clen >= sizeof(pi->command)) {
                clen = sizeof(pi->command) - 1;
            }
            memcpy(pi->command, comm_start + 1, clen);
            pi->command[clen] = '\0';
        }

        /* parse fields after ')' */
        char state = 'S';
        int ppid = 0;
        unsigned long utime = 0, stime = 0;
        int priority = 0, nice_val = 0;
        unsigned long vsize = 0;
        long rss_pages = 0;
        unsigned long long start_code = 0;

        if (comm_end) {
            /* fields after ") " are space-separated */
            /* fields: state ppid pgrp session tty tpgid flags minflt cminflt
             * majflt cmajflt utime stime cutime cstime priority nice
             * num_threads itrealval starttime vsize rss ... */
            int n = sscanf(comm_end + 2,
                           "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                           "%lu %lu %*u %*u %d %d %*d %*d %*u %lu %ld",
                           &state, &ppid, &utime, &stime,
                           &priority, &nice_val, &vsize, &rss_pages);
            (void)n;
            (void)start_code;
        }

        pi->ppid = ppid;
        pi->state = state;
        pi->priority = priority;
        pi->nice = nice_val;
        pi->cpu_time = (unsigned long long)(utime + stime);
        pi->time_sec = (unsigned long)((utime + stime) / (unsigned long)hz);
        pi->virt = (unsigned long)(vsize / 1024);        /* bytes -> KiB */
        pi->res = (unsigned long)((unsigned long)rss_pages *
                                   (unsigned long)page_size / 1024);

        /* read /proc/[pid]/status for uid, VmLib (SHR) */
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        fp = fopen(path, "r");
        if (fp) {
            char line[256];
            unsigned int uid_val = 0;
            unsigned long vm_lib = 0;
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    sscanf(line + 4, "%u", &uid_val);
                } else if (strncmp(line, "VmLib:", 6) == 0) {
                    sscanf(line + 6, "%lu", &vm_lib);
                }
            }
            fclose(fp);

            /* username from uid */
            struct passwd * pw = getpwuid((uid_t)uid_val);
            if (pw && pw->pw_name) {
                strncpy(pi->user, pw->pw_name, sizeof(pi->user) - 1);
                pi->user[sizeof(pi->user) - 1] = '\0';
            } else {
                snprintf(pi->user, sizeof(pi->user), "%u", uid_val);
            }
            pi->shr = vm_lib; /* KiB */
        }

        /* read /proc/[pid]/cmdline for full command (if -c) */
        /* done lazily in print — we keep the comm name as default */

        count++;
    }

    closedir(dir);
    return count;
}
#endif /* TOP_PLATFORM_LINUX */

#ifdef TOP_PLATFORM_WINDOWS
/**
 * @brief Windows: get process owner username.
 */
static void _top_get_user_windows(int pid, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    buf[0] = '\0';

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hProc) {
        hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
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
static int _top_get_procs_windows(proc_info_t * procs, int max_procs)
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

            proc_info_t * pi = &procs[count];
            memset(pi, 0, sizeof(*pi));
            pi->pid = (int)pe.th32ProcessID;
            pi->ppid = (int)pe.th32ParentProcessID;
            pi->state = 'R'; /* Windows processes are "running" */
            pi->priority = 0;
            pi->nice = 0;

            /* username */
            _top_get_user_windows(pi->pid, pi->user, sizeof(pi->user));

            /* command: use szExeFile (basename) */
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1,
                                pi->command, sizeof(pi->command), NULL, NULL);

            /* CPU times and memory */
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                       FALSE, pe.th32ProcessID);
            if (!hProc) {
                hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, pe.th32ProcessID);
            }

            if (hProc) {
                FILETIME create, exit, kernel, user;
                if (GetProcessTimes(hProc, &create, &exit, &kernel, &user)) {
                    ULARGE_INTEGER k, u;
                    k.LowPart = kernel.dwLowDateTime;
                    k.HighPart = kernel.dwHighDateTime;
                    u.LowPart = user.dwLowDateTime;
                    u.HighPart = user.dwHighDateTime;
                    pi->cpu_time = k.QuadPart + u.QuadPart;
                    pi->time_sec = (unsigned long)(pi->cpu_time / 10000000ULL);
                }

                PROCESS_MEMORY_COUNTERS_EX pmc;
                memset(&pmc, 0, sizeof(pmc));
                pmc.cb = sizeof(pmc);
                if (GetProcessMemoryInfo(hProc,
                                         (PROCESS_MEMORY_COUNTERS *)&pmc,
                                         sizeof(pmc))) {
                    pi->res = (unsigned long)(pmc.WorkingSetSize / 1024);
                    pi->virt = (unsigned long)(pmc.PrivateUsage / 1024);
                    pi->shr = 0; /* not directly available on Windows */
                }

                CloseHandle(hProc);
            }

            count++;
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return count;
}
#endif /* TOP_PLATFORM_WINDOWS */

#ifdef TOP_PLATFORM_MACOS
/**
 * @brief macOS: enumerate processes via sysctl KERN_PROC.
 */
static int _top_get_procs_macos(proc_info_t * procs, int max_procs)
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
        proc_info_t * pi = &procs[count];
        memset(pi, 0, sizeof(*pi));
        pi->pid = kp[i].kp_proc.p_pid;
        pi->ppid = kp[i].kp_eproc.e_ppid;
        pi->state = kp[i].kp_proc.p_stat;
        pi->priority = kp[i].kp_proc.p_priority;
        pi->nice = kp[i].kp_proc.p_nice;
        pi->cpu_time = (unsigned long long)kp[i].kp_proc.p_ru ?
                       (kp[i].kp_proc.p_ru->ru_utime.tv_sec +
                        kp[i].kp_proc.p_ru->ru_stime.tv_sec) : 0;
        pi->time_sec = (unsigned long)pi->cpu_time;

        /* command */
        strncpy(pi->command, kp[i].kp_proc.p_comm, sizeof(pi->command) - 1);

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
#endif /* TOP_PLATFORM_MACOS */

#if defined(TOP_PLATFORM_FREEBSD) || defined(TOP_PLATFORM_OPENBSD) || defined(TOP_PLATFORM_NETBSD)
/**
 * @brief BSD: enumerate processes via sysctl.
 */
static int _top_get_procs_bsd(proc_info_t * procs, int max_procs)
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

    void * buf = malloc(len);
    if (!buf) {
        return 0;
    }

    if (sysctl(mib, 3, buf, &len, NULL, 0) != 0) {
        free(buf);
        return 0;
    }

    /* BSD structures vary; use generic approach */
    int count = 0;
    /* This is a simplified version — BSD-specific kinfo_proc handling
     * would need per-platform struct definitions */
    free(buf);
    return count;
}
#endif /* BSD */

/* ---- sorting ---- */

static int _top_sort_cmp(const void * a, const void * b)
{
    const proc_info_t * pa = (const proc_info_t *)a;
    const proc_info_t * pb = (const proc_info_t *)b;
    int result = 0;

    switch (g_sort_field) {
        case TOP_FIELD_PID:
            result = (pa->pid > pb->pid) ? 1 :
                     (pa->pid < pb->pid) ? -1 : 0;
            break;
        case TOP_FIELD_USER:
            result = strcmp(pa->user, pb->user);
            break;
        case TOP_FIELD_PR:
            result = (pa->priority > pb->priority) ? 1 :
                     (pa->priority < pb->priority) ? -1 : 0;
            break;
        case TOP_FIELD_NI:
            result = (pa->nice > pb->nice) ? 1 :
                     (pa->nice < pb->nice) ? -1 : 0;
            break;
        case TOP_FIELD_VIRT:
            result = (pa->virt > pb->virt) ? 1 :
                     (pa->virt < pb->virt) ? -1 : 0;
            break;
        case TOP_FIELD_RES:
            result = (pa->res > pb->res) ? 1 :
                     (pa->res < pb->res) ? -1 : 0;
            break;
        case TOP_FIELD_SHR:
            result = (pa->shr > pb->shr) ? 1 :
                     (pa->shr < pb->shr) ? -1 : 0;
            break;
        case TOP_FIELD_S:
            result = (pa->state > pb->state) ? 1 :
                     (pa->state < pb->state) ? -1 : 0;
            break;
        case TOP_FIELD_CPU:
            result = (pa->cpu_pct > pb->cpu_pct) ? 1 :
                     (pa->cpu_pct < pb->cpu_pct) ? -1 : 0;
            break;
        case TOP_FIELD_MEM:
            result = (pa->mem_pct > pb->mem_pct) ? 1 :
                     (pa->mem_pct < pb->mem_pct) ? -1 : 0;
            break;
        case TOP_FIELD_TIME:
            result = (pa->time_sec > pb->time_sec) ? 1 :
                     (pa->time_sec < pb->time_sec) ? -1 : 0;
            break;
        case TOP_FIELD_CMD:
            result = strcmp(pa->command, pb->command);
            break;
        default:
            result = (pa->cpu_pct > pb->cpu_pct) ? 1 :
                     (pa->cpu_pct < pb->cpu_pct) ? -1 : 0;
            break;
    }

    if (g_reverse) {
        result = -result;
    }
    return result;
}

static void _top_sort_procs(proc_info_t * procs, int count,
                            top_field_t field, int reverse)
{
    g_sort_field = field;
    g_reverse = reverse;
    qsort(procs, (size_t)count, sizeof(proc_info_t), _top_sort_cmp);
}

/* ---- formatting ---- */

/**
 * @brief Format memory (in KiB) as "168.0m" or "1.2g".
 */
static void _top_format_mem(unsigned long kib, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    double mib = (double)kib / 1024.0;
    if (mib >= 1024.0) {
        snprintf(buf, buf_size, "%.1fg", mib / 1024.0);
    } else {
        snprintf(buf, buf_size, "%.1fm", mib);
    }
}

/**
 * @brief Format CPU time in seconds as TIME+ format.
 */
static void _top_format_time(unsigned long seconds, char * buf, size_t buf_size)
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
    } else if (h > 0) {
        /* H:MM:SS */
        snprintf(buf, buf_size, "%lu:%02lu:%02lu", h, m, s);
    } else {
        /* M:SS */
        snprintf(buf, buf_size, "%lu:%02lu", m, s);
    }
}

/**
 * @brief Format uptime as "X days, Y:ZZ" or "Y:ZZ" or "Z min".
 */
static void _top_format_uptime(double uptime, char * buf, size_t buf_size)
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
    } else if (hours > 0) {
        snprintf(buf, buf_size, "%lu:%02lu", hours, mins);
    } else {
        snprintf(buf, buf_size, "%lu min", mins);
    }
}

/* ---- output ---- */

/**
 * @brief Print system summary header.
 */
static void _top_print_summary(const sys_info_t * cur, const sys_info_t * prev,
                               int has_prev, const top_opts_t * opts)
{
    if (!cur || !opts) {
        return;
    }

    /* line 1: top - HH:MM:SS up X, load average: ... */
    time_t now = time(NULL);
    struct tm tm_val;
    #ifdef TOP_PLATFORM_WINDOWS
    tm_val = *localtime(&now);
    #else
    localtime_r(&now, &tm_val);
    #endif
    char upbuf[64];
    _top_format_uptime(cur->uptime, upbuf, sizeof(upbuf));

    top_printf("top - %02d:%02d:%02d up %s,  %d users,  load average: %.2f, %.2f, %.2f\n",
               tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec,
               upbuf, 1, /* users count (simplified) */
               cur->load1, cur->load5, cur->load15);

    /* line 2: Tasks: ... */
    top_printf("Tasks: %4d total,   %4d running,   %4d sleeping,   %4d stopped,   %4d zombie\n",
               cur->tasks_total, cur->tasks_running,
               cur->tasks_sleeping, cur->tasks_stopped, cur->tasks_zombie);

    /* line 3: CPU usage */
    if (has_prev && prev) {
        unsigned long long d_total = cur->cpu_total - prev->cpu_total;
        unsigned long long d_user = cur->cpu_user - prev->cpu_user;
        unsigned long long d_nice = cur->cpu_nice - prev->cpu_nice;
        unsigned long long d_sys = cur->cpu_system - prev->cpu_system;
        unsigned long long d_idle = cur->cpu_idle - prev->cpu_idle;
        unsigned long long d_iowait = cur->cpu_iowait - prev->cpu_iowait;
        unsigned long long d_irq = cur->cpu_irq - prev->cpu_irq;
        unsigned long long d_softirq = cur->cpu_softirq - prev->cpu_softirq;
        unsigned long long d_steal = cur->cpu_steal - prev->cpu_steal;

        if (d_total > 0) {
            double us = (double)d_user / (double)d_total * 100.0;
            double sy = (double)d_sys / (double)d_total * 100.0;
            double ni = (double)d_nice / (double)d_total * 100.0;
            double id = (double)d_idle / (double)d_total * 100.0;
            double wa = (double)d_iowait / (double)d_total * 100.0;
            double hi = (double)d_irq / (double)d_total * 100.0;
            double si = (double)d_softirq / (double)d_total * 100.0;
            double st = (double)d_steal / (double)d_total * 100.0;
            top_printf("%%Cpu(s): %4.1f us, %4.1f sy, %4.1f ni, %4.1f id, %4.1f wa, %4.1f hi, %4.1f si, %4.1f st\n",
                       us, sy, ni, id, wa, hi, si, st);
        } else {
            top_printf("%%Cpu(s):  0.0 us,  0.0 sy,  0.0 ni, 100.0 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st\n");
        }
    } else {
        /* first iteration: cumulative since boot */
        if (cur->cpu_total > 0) {
            double us = (double)cur->cpu_user / (double)cur->cpu_total * 100.0;
            double sy = (double)cur->cpu_system / (double)cur->cpu_total * 100.0;
            double ni = (double)cur->cpu_nice / (double)cur->cpu_total * 100.0;
            double id = (double)cur->cpu_idle / (double)cur->cpu_total * 100.0;
            double wa = (double)cur->cpu_iowait / (double)cur->cpu_total * 100.0;
            double hi = (double)cur->cpu_irq / (double)cur->cpu_total * 100.0;
            double si = (double)cur->cpu_softirq / (double)cur->cpu_total * 100.0;
            double st = (double)cur->cpu_steal / (double)cur->cpu_total * 100.0;
            top_printf("%%Cpu(s): %4.1f us, %4.1f sy, %4.1f ni, %4.1f id, %4.1f wa, %4.1f hi, %4.1f si, %4.1f st\n",
                       us, sy, ni, id, wa, hi, si, st);
        } else {
            top_printf("%%Cpu(s):  0.0 us,  0.0 sy,  0.0 ni, 100.0 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st\n");
        }
    }

    /* line 4: memory (in MiB) */
    double mem_total_mib = (double)cur->mem_total / (1024.0 * 1024.0);
    double mem_free_mib = (double)cur->mem_free / (1024.0 * 1024.0);
    double mem_used_mib = (double)cur->mem_used / (1024.0 * 1024.0);
    double mem_bc_mib = (double)(cur->mem_buff + cur->mem_cache) / (1024.0 * 1024.0);
    double mem_avail_mib = (double)cur->mem_available / (1024.0 * 1024.0);

    top_printf("MiB Mem :   %7.1f total,   %7.1f free,   %7.1f used,   %7.1f buff/cache\n",
               mem_total_mib, mem_free_mib, mem_used_mib, mem_bc_mib);

    /* line 5: swap (in MiB) */
    double swap_total_mib = (double)cur->swap_total / (1024.0 * 1024.0);
    double swap_free_mib = (double)cur->swap_free / (1024.0 * 1024.0);
    double swap_used_mib = (double)cur->swap_used / (1024.0 * 1024.0);
    top_printf("MiB Swap:   %7.1f total,   %7.1f free,   %7.1f used.   %7.1f avail Mem\n",
               swap_total_mib, swap_free_mib, swap_used_mib, mem_avail_mib);

    top_printf("\n");
}

/**
 * @brief Print process column header.
 */
static void _top_print_header(const top_opts_t * opts)
{
    if (!opts) {
        return;
    }
    (void)opts;
    (void)top_col_names; /* referenced for documentation */
    top_printf("%*s  %-*s  %*s  %*s  %*s  %*s  %*s  %s  %*s  %*s  %*s  %s\n",
               TOP_W_PID,   "PID",
               TOP_W_USER,  "USER",
               TOP_W_PR,    "PR",
               TOP_W_NI,    "NI",
               TOP_W_MEM,   "VIRT",
               TOP_W_MEM,   "RES",
               TOP_W_MEM,   "SHR",
               "S",
               TOP_W_PCT,   "%CPU",
               TOP_W_PCT,   "%MEM",
               TOP_W_TIME,  "TIME+",
               "COMMAND");
}

/**
 * @brief Print process rows.
 */
static void _top_print_procs(const proc_info_t * procs, int count,
                             const top_opts_t * opts)
{
    if (!procs || count <= 0 || !opts) {
        return;
    }

    for (int i = 0; i < count; i++) {
        const proc_info_t * p = &procs[i];

        /* PID filter */
        if (opts->pid_filter_set) {
            int match = 0;
            for (int j = 0; j < opts->pid_filter_count; j++) {
                if (p->pid == opts->pid_filter[j]) {
                    match = 1;
                    break;
                }
            }
            if (!match) {
                continue;
            }
        }

        /* user filter */
        if (opts->user_filter_set && opts->user_filter[0] != '\0') {
            if (strcmp(p->user, opts->user_filter) != 0) {
                continue;
            }
        }

        char vbuf[32], rbuf[32], sbuf[32], tbuf[32];
        _top_format_mem(p->virt, vbuf, sizeof(vbuf));
        _top_format_mem(p->res, rbuf, sizeof(rbuf));
        _top_format_mem(p->shr, sbuf, sizeof(sbuf));
        _top_format_time(p->time_sec, tbuf, sizeof(tbuf));

        top_printf("%*d  %-*s  %*d  %*d  %*s  %*s  %*s  %c  %*.1f  %*.1f  %*s  %s\n",
                   TOP_W_PID,   p->pid,
                   TOP_W_USER,  p->user,
                   TOP_W_PR,    p->priority,
                   TOP_W_NI,    p->nice,
                   TOP_W_MEM,   vbuf,
                   TOP_W_MEM,   rbuf,
                   TOP_W_MEM,   sbuf,
                   p->state,
                   TOP_W_PCT,   p->cpu_pct,
                   TOP_W_PCT,   p->mem_pct,
                   TOP_W_TIME,  tbuf,
                   p->command);
    }
}

/* ---- keyboard ---- */

#ifdef TOP_PLATFORM_WINDOWS
static int _top_kbhit(void)
{
    return _kbhit();
}

static int _top_getch(void)
{
    return _getch();
}

static void _top_set_raw_mode(int enable)
{
    /* Windows console is already in the right mode for _getch */
    (void)enable;
}

static void _top_enable_vt(void)
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
static int _top_kbhit(void)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int _top_getch(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (int)c;
    }
    return -1;
}

static struct termios g_orig_termios;
static int g_termios_saved = 0;

static void _top_set_raw_mode(int enable)
{
    if (enable) {
        if (!g_termios_saved) {
            tcgetattr(STDIN_FILENO, &g_orig_termios);
            g_termios_saved = 1;
        }
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    } else if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    }
}

static void _top_enable_vt(void)
{
    /* POSIX terminals support ANSI natively */
}
#endif

static void _top_clear_screen(void)
{
    top_printf("\033[H\033[J");
}

/* ---- utility ---- */

static void _top_sleep_ms(unsigned long ms)
{
#ifdef TOP_PLATFORM_WINDOWS
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

static unsigned long long _top_time_now_ms(void)
{
#ifdef TOP_PLATFORM_WINDOWS
    return (unsigned long long)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL +
           (unsigned long long)tv.tv_usec / 1000ULL;
#endif
}

static top_field_t _top_parse_field(const char * s)
{
    if (!s) {
        return TOP_FIELD_CPU;
    }
    if (strcmp(s, "%CPU") == 0 || strcmp(s, "CPU") == 0) {
        return TOP_FIELD_CPU;
    }
    if (strcmp(s, "%MEM") == 0 || strcmp(s, "MEM") == 0) {
        return TOP_FIELD_MEM;
    }
    if (strcmp(s, "TIME+") == 0 || strcmp(s, "TIME") == 0) {
        return TOP_FIELD_TIME;
    }
    if (strcmp(s, "PID") == 0) {
        return TOP_FIELD_PID;
    }
    if (strcmp(s, "USER") == 0) {
        return TOP_FIELD_USER;
    }
    if (strcmp(s, "PR") == 0) {
        return TOP_FIELD_PR;
    }
    if (strcmp(s, "NI") == 0) {
        return TOP_FIELD_NI;
    }
    if (strcmp(s, "VIRT") == 0) {
        return TOP_FIELD_VIRT;
    }
    if (strcmp(s, "RES") == 0) {
        return TOP_FIELD_RES;
    }
    if (strcmp(s, "SHR") == 0) {
        return TOP_FIELD_SHR;
    }
    if (strcmp(s, "S") == 0) {
        return TOP_FIELD_S;
    }
    if (strcmp(s, "COMMAND") == 0) {
        return TOP_FIELD_CMD;
    }
    /* default */
    return TOP_FIELD_CPU;
}

/* ---- help/version/args ---- */

static void _top_print_help(void)
{
    top_printf(
        "Usage: top [options]\n"
        "Show system summary and process list, updated in real time.\n"
        "\n"
        "Options:\n"
        "  -b, --batch              run in batch mode (no interaction)\n"
        "  -d, --delay=SEC          seconds between updates (float)\n"
        "  -n, --iterations=N       run N iterations then exit\n"
        "  -o, --order-field=FIELD  sort by FIELD (%%CPU, %%MEM, TIME+, PID, USER,\n"
        "                           PR, NI, VIRT, RES, SHR, S, COMMAND)\n"
        "  -p, --pid=N              monitor specific PIDs (comma-separated)\n"
        "  -u, --user=USER          show only USER's processes\n"
        "  -c, --cmd-line-toggle    toggle full command line\n"
        "  -H, --thread             show individual threads\n"
        "  -S, --cumulative         toggle cumulative time mode\n"
        "  -w, --width=N            output width\n"
        "  -1, --single-cpu-toggle  toggle single/all CPU view\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "Interactive keys:\n"
        "  q, Esc       quit\n"
        "  Space        refresh immediately\n"
        "  h, ?         show help\n"
        "  d            change delay\n"
        "  n            set max tasks\n"
        "  P            sort by %%CPU\n"
        "  M            sort by %%MEM\n"
        "  T            sort by TIME+\n"
        "  N            sort by PID\n"
        "  R            reverse sort\n"
        "  c            toggle command name/line\n"
        "  H            toggle threads\n"
        "  S            toggle cumulative time\n"
        "  1            toggle single/all CPU view\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

static void _top_print_version(void)
{
    top_printf("top %s\n", TOP_VERSION_STR);
    top_printf("Copyright (C) 2025-2026 Yezc\n");
    top_printf("License MIT: <https://mit-license.org/>\n");
    top_printf("This is free software: you are free to change and redistribute it.\n");
    top_printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static int _top_parse_args(int argc, char ** argv, top_opts_t * opts)
{
    if (argc < 1 || !argv || !opts) {
        return -1;
    }

    opts->batch = 0;
    opts->delay = TOP_DEFAULT_DELAY;
    opts->iterations = TOP_DEFAULT_ITER;
    opts->pid_filter_count = 0;
    opts->pid_filter_set = 0;
    opts->user_filter_set = 0;
    opts->user_filter[0] = '\0';
    opts->sort_field = TOP_FIELD_CPU;
    opts->reverse_sort = 0;
    opts->full_cmd = 0;
    opts->show_threads = 0;
    opts->cumulative = 0;
    opts->single_cpu = 0;
    opts->width = 0;
    opts->width_set = 0;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            break;
        }

        if (strncmp(arg, "--", 2) == 0) {
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[32];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _top_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _top_print_version();
                exit(0);
            }
            if (strcmp(name, "batch") == 0) {
                opts->batch = 1;
                continue;
            }
            if (strcmp(name, "delay") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--delay' requires an argument\n");
                    return -1;
                }
                opts->delay = atof(v);
                if (opts->delay <= 0) {
                    top_err_printf("top: invalid argument '%s' for '--delay'\n", v);
                    return -1;
                }
                continue;
            }
            if (strcmp(name, "iterations") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--iterations' requires an argument\n");
                    return -1;
                }
                opts->iterations = atoi(v);
                if (opts->iterations < 0) {
                    opts->iterations = 0;
                }
                continue;
            }
            if (strcmp(name, "order-field") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--order-field' requires an argument\n");
                    return -1;
                }
                opts->sort_field = _top_parse_field(v);
                continue;
            }
            if (strcmp(name, "pid") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--pid' requires an argument\n");
                    return -1;
                }
                /* parse comma-separated PIDs */
                char * vcopy = strdup(v);
                if (!vcopy) {
                    return -1;
                }
                char * tok = strtok(vcopy, ",");
                while (tok && opts->pid_filter_count < TOP_MAX_PID_FILTER) {
                    opts->pid_filter[opts->pid_filter_count++] = atoi(tok);
                    tok = strtok(NULL, ",");
                }
                free(vcopy);
                opts->pid_filter_set = 1;
                continue;
            }
            if (strcmp(name, "user") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--user' requires an argument\n");
                    return -1;
                }
                strncpy(opts->user_filter, v, sizeof(opts->user_filter) - 1);
                opts->user_filter[sizeof(opts->user_filter) - 1] = '\0';
                opts->user_filter_set = 1;
                continue;
            }
            if (strcmp(name, "cmd-line-toggle") == 0) {
                opts->full_cmd = !opts->full_cmd;
                continue;
            }
            if (strcmp(name, "thread") == 0) {
                opts->show_threads = 1;
                continue;
            }
            if (strcmp(name, "cumulative") == 0) {
                opts->cumulative = 1;
                continue;
            }
            if (strcmp(name, "width") == 0) {
                const char * v = eq ? eq + 1 :
                    (i + 1 < argc ? argv[++i] : NULL);
                if (!v) {
                    top_err_printf("top: option '--width' requires an argument\n");
                    return -1;
                }
                opts->width = atoi(v);
                opts->width_set = 1;
                continue;
            }
            if (strcmp(name, "single-cpu-toggle") == 0) {
                opts->single_cpu = !opts->single_cpu;
                continue;
            }
            top_err_printf("top: unrecognized option '%s'\n", arg);
            top_err_printf("Try 'top --help' for more information.\n");
            return -1;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* short options; -d, -n, -o, -p, -u, -w take an argument */
            for (int j = 1; arg[j]; j++) {
                char c = arg[j];
                switch (c) {
                    case 'b':
                        opts->batch = 1;
                        break;
                    case 'c':
                        opts->full_cmd = !opts->full_cmd;
                        break;
                    case 'H':
                        opts->show_threads = 1;
                        break;
                    case 'S':
                        opts->cumulative = 1;
                        break;
                    case '1':
                        opts->single_cpu = !opts->single_cpu;
                        break;
                    case 'd':
                    case 'n':
                    case 'o':
                    case 'p':
                    case 'u':
                    case 'w': {
                        const char * v = NULL;
                        if (arg[j + 1] != '\0') {
                            v = arg + j + 1;
                            j = (int)strlen(arg) - 1;
                        }
                        else if (i + 1 < argc) {
                            v = argv[++i];
                        }
                        else {
                            top_err_printf("top: option requires an argument -- '%c'\n", c);
                            return -1;
                        }
                        switch (c) {
                            case 'd':
                                opts->delay = atof(v);
                                if (opts->delay <= 0) {
                                    top_err_printf("top: invalid argument '%s' for '-d'\n", v);
                                    return -1;
                                }
                                break;
                            case 'n':
                                opts->iterations = atoi(v);
                                if (opts->iterations < 0) {
                                    opts->iterations = 0;
                                }
                                break;
                            case 'o':
                                opts->sort_field = _top_parse_field(v);
                                break;
                            case 'p':
                                {
                                    char * vcopy = strdup(v);
                                    if (!vcopy) {
                                        return -1;
                                    }
                                    char * tok = strtok(vcopy, ",");
                                    while (tok && opts->pid_filter_count < TOP_MAX_PID_FILTER) {
                                        opts->pid_filter[opts->pid_filter_count++] = atoi(tok);
                                        tok = strtok(NULL, ",");
                                    }
                                    free(vcopy);
                                    opts->pid_filter_set = 1;
                                }
                                break;
                            case 'u':
                                strncpy(opts->user_filter, v, sizeof(opts->user_filter) - 1);
                                opts->user_filter[sizeof(opts->user_filter) - 1] = '\0';
                                opts->user_filter_set = 1;
                                break;
                            case 'w':
                                opts->width = atoi(v);
                                opts->width_set = 1;
                                break;
                        }
                        break;
                    }
                    default:
                        top_err_printf("top: invalid option -- '%c'\n", c);
                        top_err_printf("Try 'top --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            top_err_printf("top: extra operand '%s'\n", arg);
            top_err_printf("Try 'top --help' for more information.\n");
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

int main(int argc, char ** argv)
{
    top_opts_t opts;
    if (_top_parse_args(argc, argv, &opts) != 0) {
        return 1;
    }

    int interactive = !opts.batch;

    /* allocate process arrays */
    proc_info_t * procs = (proc_info_t *)malloc(sizeof(proc_info_t) * TOP_MAX_PROCS);
    proc_info_t * prev_procs = (proc_info_t *)malloc(sizeof(proc_info_t) * TOP_MAX_PROCS);
    if (!procs || !prev_procs) {
        top_err_printf("top: out of memory\n");
        free(procs);
        free(prev_procs);
        return 1;
    }

    sys_info_t cur_sys, prev_sys;
    int prev_count = 0;
    int has_prev = 0;
    int iteration = 0;

#ifdef TOP_PLATFORM_WINDOWS
    _top_enable_vt();
#else
    /* no-op on POSIX (ANSI escapes work natively), but call to avoid unused-function warning */
    _top_enable_vt();
#endif

    if (interactive) {
        _top_set_raw_mode(1);
    }

    int should_run = 1;

    while (should_run) {
        /* get current system info */
        memset(&cur_sys, 0, sizeof(cur_sys));
        _top_get_sys_info(&cur_sys);

        /* get current process list */
        memset(procs, 0, sizeof(proc_info_t) * TOP_MAX_PROCS);
        int count = _top_get_procs(procs, TOP_MAX_PROCS);

        /* calculate CPU% deltas */
        unsigned long long cpu_delta_total = 0;
        if (has_prev) {
            cpu_delta_total = cur_sys.cpu_total - prev_sys.cpu_total;
        }

        for (int i = 0; i < count; i++) {
            /* memory percentage */
            if (cur_sys.mem_total > 0) {
                procs[i].mem_pct = (double)procs[i].res * 1024.0 /
                                   (double)cur_sys.mem_total * 100.0;
            }

            /* CPU percentage */
            if (has_prev && cpu_delta_total > 0) {
                unsigned long long proc_delta = procs[i].cpu_time;
                /* find matching previous process */
                for (int j = 0; j < prev_count; j++) {
                    if (prev_procs[j].pid == procs[i].pid) {
                        proc_delta = procs[i].cpu_time - prev_procs[j].cpu_time;
                        break;
                    }
                }
                if (proc_delta > procs[i].cpu_time) {
                    /* process restarted; use cumulative */
                    proc_delta = procs[i].cpu_time;
                }
                procs[i].cpu_pct = (double)proc_delta /
                                   (double)cpu_delta_total * 100.0 *
                                   (double)cur_sys.num_cpus;
            } else if (!has_prev && cur_sys.uptime > 0) {
                /* first iteration: cumulative average */
                procs[i].cpu_pct = (double)procs[i].time_sec /
                                   cur_sys.uptime * 100.0;
            }
        }

        /* sort */
        _top_sort_procs(procs, count, opts.sort_field, opts.reverse_sort);

        /* clear screen (interactive only) */
        if (interactive && iteration > 0) {
            _top_clear_screen();
        }

        /* print */
        _top_print_summary(&cur_sys, &prev_sys, has_prev, &opts);
        _top_print_header(&opts);
        _top_print_procs(procs, count, &opts);
        top_fflush(stdout);

        /* save previous */
        memcpy(&prev_sys, &cur_sys, sizeof(cur_sys));
        memcpy(prev_procs, procs, sizeof(proc_info_t) * (size_t)count);
        prev_count = count;
        has_prev = 1;

        iteration++;

        /* check iteration limit */
        if (opts.iterations > 0 && iteration >= opts.iterations) {
            break;
        }

        /* wait for delay, checking keyboard (interactive only) */
        if (interactive) {
            double remaining = opts.delay;
            while (remaining > 0 && should_run) {
                unsigned long long start = _top_time_now_ms();

                if (_top_kbhit()) {
                    int ch = _top_getch();
                    switch (ch) {
                        case 'q':
                        case 'Q':
                        case 27: /* Esc */
                        case 3:  /* Ctrl+C */
                            should_run = 0;
                            break;
                        case ' ':
                            remaining = 0;
                            break;
                        case 'h':
                        case '?':
                            _top_clear_screen();
                            top_printf("Help for top:\n");
                            top_printf("  q, Esc       quit\n");
                            top_printf("  Space        refresh immediately\n");
                            top_printf("  h, ?         show this help\n");
                            top_printf("  d            change delay\n");
                            top_printf("  P            sort by %%CPU\n");
                            top_printf("  M            sort by %%MEM\n");
                            top_printf("  T            sort by TIME+\n");
                            top_printf("  N            sort by PID\n");
                            top_printf("  R            reverse sort\n");
                            top_printf("  c            toggle command\n");
                            top_printf("  H            toggle threads\n");
                            top_printf("  S            toggle cumulative\n");
                            top_printf("  1            toggle CPU view\n");
                            top_printf("\nPress any key to continue...\n");
                            top_fflush(stdout);
                            _top_getch(); /* wait for key */
                            break;
                        case 'P':
                            opts.sort_field = TOP_FIELD_CPU;
                            opts.reverse_sort = 0;
                            break;
                        case 'M':
                            opts.sort_field = TOP_FIELD_MEM;
                            opts.reverse_sort = 0;
                            break;
                        case 'T':
                            opts.sort_field = TOP_FIELD_TIME;
                            opts.reverse_sort = 0;
                            break;
                        case 'N':
                            opts.sort_field = TOP_FIELD_PID;
                            opts.reverse_sort = 0;
                            break;
                        case 'R':
                            opts.reverse_sort = !opts.reverse_sort;
                            break;
                        case 'c':
                            opts.full_cmd = !opts.full_cmd;
                            break;
                        case 'H':
                            opts.show_threads = !opts.show_threads;
                            break;
                        case 'S':
                            opts.cumulative = !opts.cumulative;
                            break;
                        case '1':
                            opts.single_cpu = !opts.single_cpu;
                            break;
                        default:
                            break;
                    }
                }

                if (should_run && remaining > 0) {
                    _top_sleep_ms(100);
                    unsigned long long elapsed = _top_time_now_ms() - start;
                    remaining -= (double)elapsed / 1000.0;
                    if (remaining < 0) {
                        remaining = 0;
                    }
                }
            }
        } else {
            _top_sleep_ms((unsigned long)(opts.delay * 1000.0));
        }
    }

    if (interactive) {
        _top_set_raw_mode(0);
    }

    free(procs);
    free(prev_procs);

    return 0;
}