/**
 * @file free.c
 * @brief Cross-platform free command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU procps-ng free(1).
 *
 * Key behaviors:
 *   - Display total/used/free/shared/buffers/cache/available memory
 *   - -b/--bytes, --kilo, --mega, --giga, --tera, --peta (SI, 1000)
 *   - -k/--kibi, -m/--mebi, -g/--gibi, --tebi, --pebi (binary, 1024)
 *   - --si: use 1000-based scaling for default and human mode
 *   - --iec: use 1024-based scaling (default)
 *   - -h/--human: auto-scale values with unit suffix
 *   - -l/--lohi: show detailed low/high memory statistics
 *   - -t/--total: show total (mem + swap) line
 *   - -w/--wide: wide mode (separate buffers and cache columns)
 *   - -s N/--seconds N: repeat every N seconds
 *   - -c N/--count N: repeat N times then exit (requires -s)
 *   - --help / --version
 *
 * Platform memory sources:
 *   Linux:     /proc/meminfo
 *   Windows:   GlobalMemoryStatusEx + GetPerformanceInfo
 *   macOS:     sysctl(hw.memsize) + mach host_statistics64 + vm.swapusage
 *   FreeBSD:   sysctl(vm.stats.vm.*) + kvm_getswapinfo
 *   OpenBSD:   sysctl(hw.physmem) + sysctl(VM_UVMEXP)
 *   NetBSD:    sysctl(hw.physmem) + sysctl(VM_UVMEXP2)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o free.exe free.c -lpsapi
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o free free.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o free free.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o free free.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o free free.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o free free.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/free>
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
    #define FREE_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define FREE_PLATFORM_LINUX   1
    #define FREE_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define FREE_PLATFORM_MACOS   1
    #define FREE_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define FREE_PLATFORM_FREEBSD 1
    #define FREE_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define FREE_PLATFORM_OPENBSD 1
    #define FREE_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define FREE_PLATFORM_NETBSD  1
    #define FREE_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define FREE_PLATFORM_POSIX   1
#else
    #define FREE_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef FREE_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef FREE_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef FREE_PLATFORM_NETBSD
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

#ifdef FREE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <psapi.h>
#else
    #include <unistd.h>
    #include <time.h>
    #include <errno.h>
#endif

#ifdef FREE_PLATFORM_LINUX
    /* /proc/meminfo — no extra headers needed */
#endif

#ifdef FREE_PLATFORM_MACOS
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/mach_init.h>
    #include <mach/mach_host.h>
    #include <mach/host_info.h>
    #include <mach/vm_statistics.h>
#endif

#if defined(FREE_PLATFORM_FREEBSD) || defined(FREE_PLATFORM_OPENBSD) || defined(FREE_PLATFORM_NETBSD)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <sys/param.h>
#endif

#ifdef FREE_PLATFORM_FREEBSD
    #include <fcntl.h>
    #include <kvm.h>
    #include <vm/vm_param.h>
#endif

#ifdef FREE_PLATFORM_OPENBSD
    #include <uvm/uvm_extern.h>
#endif

#ifdef FREE_PLATFORM_NETBSD
    #include <uvm/uvm_extern.h>
#endif

/* NetBSD swap */
#if defined(FREE_PLATFORM_NETBSD) || defined(FREE_PLATFORM_OPENBSD)
    #include <sys/swap.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define FREE_VERSION_STR "v1.0.0"

/** @brief Column field width for numeric output */
#define FREE_COL_WIDTH 11

/** @brief Label field width (e.g., "Mem:") */
#define FREE_LABEL_WIDTH 7

/** @brief Maximum unit suffix length */
#define FREE_SUFFIX_MAX 8

/** @brief Maximum number of columns */
#define FREE_MAX_COLS 7

/** @brief Line buffer size */
#define FREE_LINE_BUF 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Memory unit selection.
 */
typedef enum {
    FREE_UNIT_BYTES,    /* -b  --bytes   */
    FREE_UNIT_KILO,     /*     --kilo     (1000)  */
    FREE_UNIT_MEGA,     /*     --mega     (10^6)  */
    FREE_UNIT_GIGA,     /*     --giga     (10^9)  */
    FREE_UNIT_TERA,     /*     --tera     (10^12) */
    FREE_UNIT_PETA,     /*     --peta     (10^15) */
    FREE_UNIT_KIBI,     /* -k  --kibi     (1024)  */
    FREE_UNIT_MEBI,     /* -m  --mebi     (2^20)  */
    FREE_UNIT_GIBI,     /* -g  --gibi     (2^30)  */
    FREE_UNIT_TEBI,     /*     --tebi     (2^40)  */
    FREE_UNIT_PEBI,     /*     --pebi     (2^50)  */
    FREE_UNIT_HUMAN     /* -h  --human    (auto)  */
} free_unit_t;

/**
 * @brief Parsed options.
 */
typedef struct {
    free_unit_t unit;       /* selected unit */
    bool        unit_set;   /* explicit unit option given? */
    bool        si;         /* --si: use 1000 base */
    bool        human;       /* -h: human readable */
    bool        lohi;        /* -l: low/high memory */
    bool        total;       /* -t: show total line */
    bool        wide;        /* -w: wide output */
    unsigned long seconds;  /* -s: repeat interval */
    unsigned long count;    /* -c: repeat count */
} free_opts_t;

/**
 * @brief Resolved memory information (all in bytes).
 */
typedef struct {
    unsigned long long total;      /* total usable RAM */
    unsigned long long used;       /* used memory */
    unsigned long long free;       /* free memory */
    unsigned long long shared;     /* shared memory */
    unsigned long long buffers;    /* buffer memory */
    unsigned long long cache;      /* cache memory */
    unsigned long long available;  /* available memory */
    unsigned long long low_total;  /* low memory total */
    unsigned long long low_free;   /* low memory free */
    unsigned long long high_total; /* high memory total */
    unsigned long long high_free;  /* high memory free */
    unsigned long long swap_total; /* swap total */
    unsigned long long swap_used;  /* swap used */
    unsigned long long swap_free;  /* swap free */
} mem_info_t;

/********************************
 *    static prototypes
 ********************************/

/* ---- platform memory info ---- */
static int  _free_get_info(mem_info_t * info);
#ifndef FREE_PLATFORM_WINDOWS
static int  _free_get_info_linux(mem_info_t * info);
#endif
#ifdef FREE_PLATFORM_WINDOWS
static int  _free_get_info_windows(mem_info_t * info);
#endif
#ifdef FREE_PLATFORM_MACOS
static int  _free_get_info_macos(mem_info_t * info);
#endif
#ifdef FREE_PLATFORM_FREEBSD
static int  _free_get_info_freebsd(mem_info_t * info);
#endif
#ifdef FREE_PLATFORM_OPENBSD
static int  _free_get_info_obsd(mem_info_t * info);
#endif
#ifdef FREE_PLATFORM_NETBSD
static int  _free_get_info_nbsd(mem_info_t * info);
#endif

/* ---- formatting ---- */
static unsigned long long _free_divisor(free_unit_t unit);
static void _free_human_value(unsigned long long bytes, bool si,
                              char * buf, size_t buf_size);
static void _free_format_value(unsigned long long bytes,
                               const free_opts_t * opts,
                               char * buf, size_t buf_size);

/* ---- output ---- */
static void _free_print_header(const free_opts_t * opts);
static void _free_print_row(const char * label,
                            const unsigned long long * values,
                            int nvals, const free_opts_t * opts);
static void _free_print_output(const mem_info_t * info,
                               const free_opts_t * opts);

/* ---- utility ---- */
static void _free_sleep_ms(unsigned long ms);

/* ---- help/version/args ---- */
static void _free_print_help(void);
static void _free_print_version(void);
static int  _free_parse_args(int argc, char ** argv, free_opts_t * opts);
static unsigned long _free_parse_ulong(const char * s);

/********************************
 *    static variables
 ********************************/

/* narrow mode: total  used  free  shared  buff/cache  available */
static const char * free_hdr_narrow[] = {
    "total", "used", "free", "shared", "buff/cache", "available"
};
static const int free_hdr_narrow_count = 6;

/* wide mode: total  used  free  shared  buffers  cache  available */
static const char * free_hdr_wide[] = {
    "total", "used", "free", "shared", "buffers", "cache", "available"
};
static const int free_hdr_wide_count = 7;

/* SI unit suffixes for human mode (1000-based) */
static const char * free_suffix_si[] = {
    "B", "K", "M", "G", "T", "P"
};
static const int free_suffix_si_count = 6;

/* IEC unit suffixes for human mode (1024-based) */
static const char * free_suffix_iec[] = {
    "B", "Ki", "Mi", "Gi", "Ti", "Pi"
};
static const int free_suffix_iec_count = 6;

/********************************
 *    macros
 ********************************/

#ifndef free_out_stream
    #define free_out_stream stdout
#endif

#ifndef free_err_stream
    #define free_err_stream stderr
#endif

#ifndef free_printf
    #define free_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef free_err_printf
    #define free_err_printf(fmt, ...) \
        do { if (free_err_stream) { (void)fprintf((free_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef free_fflush
    #define free_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    global
 ********************************/

/* No global state — all data is passed via structs. */

/********************************
 *    static functions
 ********************************/

/* ---- platform memory info ---- */

static int _free_get_info(mem_info_t * info)
{
    if (!info) {
        return -1;
    }
    memset(info, 0, sizeof(*info));

#ifdef FREE_PLATFORM_LINUX
    return _free_get_info_linux(info);
#elif defined(FREE_PLATFORM_WINDOWS)
    return _free_get_info_windows(info);
#elif defined(FREE_PLATFORM_MACOS)
    return _free_get_info_macos(info);
#elif defined(FREE_PLATFORM_FREEBSD)
    return _free_get_info_freebsd(info);
#elif defined(FREE_PLATFORM_OPENBSD)
    return _free_get_info_obsd(info);
#elif defined(FREE_PLATFORM_NETBSD)
    return _free_get_info_nbsd(info);
#else
    /* Generic POSIX fallback: try /proc/meminfo */
    return _free_get_info_linux(info);
#endif
}

#ifndef FREE_PLATFORM_WINDOWS
/**
 * @brief Linux: parse /proc/meminfo for memory statistics.
 * All values in /proc/meminfo are in kB (1024 bytes).
 */
static int _free_get_info_linux(mem_info_t * info)
{
    if (!info) {
        return -1;
    }
    FILE * f = fopen("/proc/meminfo", "r");
    if (!f) {
        return -1;
    }
    char line[FREE_LINE_BUF];
    bool have_cached = false;
    bool have_sreclaimable = false;

    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned long long val;
        if (sscanf(line, " %63[^:]: %llu", key, &val) != 2) {
            continue;
        }
        /* values in kB → convert to bytes */
        unsigned long long bytes = val * 1024ULL;
        if (strcmp(key, "MemTotal") == 0) {
            info->total = bytes;
        }
        else if (strcmp(key, "MemFree") == 0) {
            info->free = bytes;
        }
        else if (strcmp(key, "MemAvailable") == 0) {
            info->available = bytes;
        }
        else if (strcmp(key, "Buffers") == 0) {
            info->buffers = bytes;
        }
        else if (strcmp(key, "Cached") == 0) {
            info->cache = bytes;
            have_cached = true;
        }
        else if (strcmp(key, "Shmem") == 0) {
            info->shared = bytes;
        }
        else if (strcmp(key, "SReclaimable") == 0) {
            info->cache += bytes;
            have_sreclaimable = true;
        }
        else if (strcmp(key, "SwapTotal") == 0) {
            info->swap_total = bytes;
        }
        else if (strcmp(key, "SwapFree") == 0) {
            info->swap_free = bytes;
        }
        else if (strcmp(key, "LowTotal") == 0) {
            info->low_total = bytes;
        }
        else if (strcmp(key, "LowFree") == 0) {
            info->low_free = bytes;
        }
        else if (strcmp(key, "HighTotal") == 0) {
            info->high_total = bytes;
        }
        else if (strcmp(key, "HighFree") == 0) {
            info->high_free = bytes;
        }
    }
    fclose(f);

    /* compute derived values */
    info->used = info->total - info->free - info->buffers - info->cache;
    info->swap_used = info->swap_total - info->swap_free;

    /* fallback for available */
    if (info->available == 0) {
        info->available = info->free + info->buffers + info->cache;
    }

    /* low/high defaults: if not present, treat all as low */
    if (info->low_total == 0 && info->high_total == 0) {
        info->low_total = info->total;
        info->low_free = info->free;
    }
    if (info->low_total > 0) {
        info->low_free = info->low_total - (info->total - info->free);
        if (info->low_free > info->low_total) {
            info->low_free = info->low_total;
        }
    }

    /* suppress unused warnings */
    (void)have_cached;
    (void)have_sreclaimable;

    return 0;
}
#endif /* !FREE_PLATFORM_WINDOWS */

#ifdef FREE_PLATFORM_WINDOWS
/**
 * @brief Windows: use GlobalMemoryStatusEx for RAM and page file,
 *        GetPerformanceInfo for system cache.
 */
static int _free_get_info_windows(mem_info_t * info)
{
    if (!info) {
        return -1;
    }

    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) {
        return -1;
    }

    info->total = ms.ullTotalPhys;
    info->free = ms.ullAvailPhys;
    info->available = ms.ullAvailPhys;
    info->used = info->total - info->free;

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

    /* system cache from GetPerformanceInfo */
    PERFORMANCE_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi)) && pi.PageSize > 0) {
        info->cache = (unsigned long long)pi.SystemCache *
                      (unsigned long long)pi.PageSize;
    }

    /* low/high not applicable on Windows */
    info->low_total = info->total;
    info->low_free = info->free;
    info->buffers = 0;
    info->shared = 0;

    return 0;
}
#endif /* FREE_PLATFORM_WINDOWS */

#ifdef FREE_PLATFORM_MACOS
/**
 * @brief macOS: sysctl for total RAM, mach for page stats,
 *        vm.swapusage for swap.
 */
static int _free_get_info_macos(mem_info_t * info)
{
    if (!info) {
        return -1;
    }

    /* total physical memory */
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t total = 0;
    size_t len = sizeof(total);
    if (sysctl(mib, 2, &total, &len, NULL, 0) != 0) {
        /* fallback: HW_PHYSMEM (32-bit) */
        mib[1] = HW_PHYSMEM;
        unsigned int physmem = 0;
        len = sizeof(physmem);
        if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0) {
            total = physmem;
        }
    }
    info->total = total;

    /* page statistics via mach */
    vm_size_t page_size = 0;
    host_page_size(mach_host_self(), &page_size);

    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    kern_return_t kr = host_statistics64(mach_host_self(),
                                         HOST_VM_INFO64,
                                         (host_info64_t)&vm_stat,
                                         &count);
    if (kr == KERN_SUCCESS && page_size > 0) {
        unsigned long long ps = (unsigned long long)page_size;
        info->free = (unsigned long long)vm_stat.free_count * ps;
        info->cache = (unsigned long long)vm_stat.inactive_count * ps;
        info->buffers = (unsigned long long)vm_stat.wire_count * ps;
        info->used = info->total - info->free - info->cache;
        info->available = info->free + info->cache;
    }
    else {
        info->used = info->total;
    }

    /* swap via vm.swapusage */
    struct xsw_usage {
        u_int64_t xsu_total;
        u_int64_t xsu_used;
        u_int64_t xsu_avail;
        u_int32_t xsu_pagesize;
        boolean_t xsu_encrypted;
    };
    struct xsw_usage swapu;
    memset(&swapu, 0, sizeof(swapu));
    mib[0] = CTL_VM;
    mib[1] = VM_SWAPUSAGE;
    len = sizeof(swapu);
    if (sysctl(mib, 2, &swapu, &len, NULL, 0) == 0) {
        info->swap_total = swapu.xsu_total;
        info->swap_used = swapu.xsu_used;
        info->swap_free = swapu.xsu_avail;
    }

    /* low/high not applicable */
    info->low_total = info->total;
    info->low_free = info->free;

    return 0;
}
#endif /* FREE_PLATFORM_MACOS */

#ifdef FREE_PLATFORM_FREEBSD
/**
 * @brief FreeBSD: sysctl vm.stats.vm.* for page counts,
 *        kvm_getswapinfo for swap.
 */
static int _free_get_info_freebsd(mem_info_t * info)
{
    if (!info) {
        return -1;
    }

    long ps = sysconf(_SC_PAGESIZE);
    int page_size = (ps > 0) ? (int)ps : 4096;

    /* query a single int sysctlbyname */
    int _freebsd_sysctl_int(const char * name) {
        int v = 0;
        size_t l = sizeof(v);
        sysctlbyname(name, &v, &l, NULL, 0);
        return v;
    }

    int page_count = _freebsd_sysctl_int("vm.stats.vm.v_page_count");
    int free_count = _freebsd_sysctl_int("vm.stats.vm.v_free_count");
    int wire_count = _freebsd_sysctl_int("vm.stats.vm.v_wire_count");
    int inactive_count = _freebsd_sysctl_int("vm.stats.vm.v_inactive_count");
    int cache_count = _freebsd_sysctl_int("vm.stats.vm.v_cache_count");

    if (page_count > 0) {
        info->total = (unsigned long long)page_count * page_size;
    }
    else {
        /* fallback: hw.realmem (bytes) */
        unsigned long long physmem = 0;
        size_t pl = sizeof(physmem);
        sysctlbyname("hw.realmem", &physmem, &pl, NULL, 0);
        if (physmem == 0) {
            int mib2[2] = {CTL_HW, HW_PHYSMEM};
            unsigned int pm = 0;
            pl = sizeof(pm);
            sysctl(mib2, 2, &pm, &pl, NULL, 0);
            physmem = pm;
        }
        info->total = physmem;
    }

    info->free = (unsigned long long)free_count * page_size;
    info->cache = (unsigned long long)(inactive_count + cache_count) * page_size;
    info->buffers = (unsigned long long)wire_count * page_size;
    info->used = info->total - info->free - info->cache;
    info->available = info->free + info->cache;

    /* swap via kvm_getswapinfo */
    kvm_t * kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);
    if (kd) {
        struct kvm_swap kswap;
        memset(&kswap, 0, sizeof(kswap));
        int nswap = kvm_getswapinfo(kd, &kswap, 1, 0);
        if (nswap > 0) {
            int sp = getpagesize();
            info->swap_total = (unsigned long long)kswap.ksw_total * sp;
            info->swap_used = (unsigned long long)kswap.ksw_used * sp;
            info->swap_free = info->swap_total - info->swap_used;
        }
        kvm_close(kd);
    }

    info->low_total = info->total;
    info->low_free = info->free;

    return 0;
}
#endif /* FREE_PLATFORM_FREEBSD */

#ifdef FREE_PLATFORM_OPENBSD
/**
 * @brief OpenBSD: sysctl hw.physmem for total, VM_UVMEXP for page stats.
 */
static int _free_get_info_obsd(mem_info_t * info)
{
    if (!info) {
        return -1;
    }

    /* total physical memory */
    int mib[2] = {CTL_HW, HW_PHYSMEM};
    unsigned long physmem = 0;
    size_t len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0) {
        info->total = physmem;
    }

    /* uvm statistics */
    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP;
    struct uvmexp uvm;
    memset(&uvm, 0, sizeof(uvm));
    len = sizeof(uvm);
    if (sysctl(mib, 2, &uvm, &len, NULL, 0) == 0) {
        int ps = uvm.pagesize;
        if (ps <= 0) {
            ps = (int)sysconf(_SC_PAGESIZE);
        }
        if (ps <= 0) {
            ps = 4096;
        }
        info->total = (unsigned long long)uvm.npages * ps;
        info->free = (unsigned long long)uvm.free * ps;
        info->cache = (unsigned long long)uvm.inactive * ps;
        info->buffers = (unsigned long long)uvm.wired * ps;
        info->used = info->total - info->free - info->cache;
        info->available = info->free + info->cache;
    }

    /* swap */
    struct swapent * swdev;
    int nswap = swapctl(SWAP_NSWAP, NULL, 0);
    if (nswap > 0) {
        swdev = calloc(nswap, sizeof(*swdev));
        if (swdev) {
            int n = swapctl(SWAP_STATS, swdev, nswap);
            for (int i = 0; i < n; i++) {
                if (swdev[i].se_flags & SWF_ENABLE) {
                    info->swap_total +=
                        (unsigned long long)swdev[i].se_nblks * DEV_BSIZE;
                    info->swap_used +=
                        (unsigned long long)swdev[i].se_inuse * DEV_BSIZE;
                }
            }
            info->swap_free = info->swap_total - info->swap_used;
            free(swdev);
        }
    }

    info->low_total = info->total;
    info->low_free = info->free;

    return 0;
}
#endif /* FREE_PLATFORM_OPENBSD */

#ifdef FREE_PLATFORM_NETBSD
/**
 * @brief NetBSD: sysctl hw.physmem for total, VM_UVMEXP2 for page stats.
 */
static int _free_get_info_nbsd(mem_info_t * info)
{
    if (!info) {
        return -1;
    }

    /* total physical memory (bytes) */
    int mib[2] = {CTL_HW, HW_PHYSMEM64};
    uint64_t physmem = 0;
    size_t len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0) {
        info->total = physmem;
    }
    else {
        /* fallback: HW_PHYSMEM (unsigned int) */
        mib[1] = HW_PHYSMEM;
        unsigned int pm = 0;
        len = sizeof(pm);
        if (sysctl(mib, 2, &pm, &len, NULL, 0) == 0) {
            info->total = pm;
        }
    }

    /* uvm statistics (uvmexp2) */
    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP2;
    struct uvmexp uvm;
    memset(&uvm, 0, sizeof(uvm));
    len = sizeof(uvm);
    if (sysctl(mib, 2, &uvm, &len, NULL, 0) == 0) {
        int ps = uvm.pagesize;
        if (ps <= 0) {
            ps = (int)sysconf(_SC_PAGESIZE);
        }
        if (ps <= 0) {
            ps = 4096;
        }
        if (info->total == 0) {
            info->total = (unsigned long long)uvm.npages * ps;
        }
        info->free = (unsigned long long)uvm.free * ps;
        info->cache = (unsigned long long)uvm.inactive * ps;
        info->buffers = (unsigned long long)uvm.wired * ps;
        info->used = info->total - info->free - info->cache;
        info->available = info->free + info->cache;
    }

    /* swap */
    struct swapent * swdev;
    int nswap = swapctl(SWAP_NSWAP, NULL, 0);
    if (nswap > 0) {
        swdev = calloc(nswap, sizeof(*swdev));
        if (swdev) {
            int n = swapctl(SWAP_STATS, swdev, nswap);
            for (int i = 0; i < n; i++) {
                if (swdev[i].se_flags & SWF_ENABLE) {
                    info->swap_total +=
                        (unsigned long long)swdev[i].se_nblks * DEV_BSIZE;
                    info->swap_used +=
                        (unsigned long long)swdev[i].se_inuse * DEV_BSIZE;
                }
            }
            info->swap_free = info->swap_total - info->swap_used;
            free(swdev);
        }
    }

    info->low_total = info->total;
    info->low_free = info->free;

    return 0;
}
#endif /* FREE_PLATFORM_NETBSD */

/* ---- formatting ---- */

static unsigned long long _free_divisor(free_unit_t unit)
{
    switch (unit) {
        case FREE_UNIT_BYTES: return 1ULL;
        case FREE_UNIT_KILO:  return 1000ULL;
        case FREE_UNIT_MEGA:  return 1000000ULL;
        case FREE_UNIT_GIGA:  return 1000000000ULL;
        case FREE_UNIT_TERA:  return 1000000000000ULL;
        case FREE_UNIT_PETA:  return 1000000000000000ULL;
        case FREE_UNIT_KIBI:  return 1024ULL;
        case FREE_UNIT_MEBI:  return 1048576ULL;
        case FREE_UNIT_GIBI:  return 1073741824ULL;
        case FREE_UNIT_TEBI:  return 1099511627776ULL;
        case FREE_UNIT_PEBI:  return 1125899906842624ULL;
        default:              return 1024ULL;
    }
}

/**
 * @brief Format a byte value into a human-readable string with auto-scaling.
 * @param bytes    value in bytes
 * @param si       true for 1000-based (K,M,G,T,P), false for 1024 (Ki,Mi,...)
 * @param buf      output buffer
 * @param buf_size buffer size
 */
static void _free_human_value(unsigned long long bytes, bool si,
                              char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    buf[0] = '\0';

    const char * const * suffixes = si ? free_suffix_si : free_suffix_iec;
    int n_suffix = si ? free_suffix_si_count : free_suffix_iec_count;
    unsigned long long base = si ? 1000ULL : 1024ULL;

    double val = (double)bytes;
    int idx = 0;
    while (val >= (double)base && idx < n_suffix - 1) {
        val /= (double)base;
        idx++;
    }

    if (idx == 0) {
        snprintf(buf, buf_size, "%llu%s",
                 (unsigned long long)bytes, suffixes[0]);
    }
    else if (val < 10.0) {
        snprintf(buf, buf_size, "%.1f%s", val, suffixes[idx]);
    }
    else {
        snprintf(buf, buf_size, "%.0f%s", val, suffixes[idx]);
    }
}

/**
 * @brief Format a byte value according to the selected unit.
 *        For non-human mode: divide by divisor, print as integer.
 *        For human mode: auto-scale with suffix.
 */
static void _free_format_value(unsigned long long bytes,
                               const free_opts_t * opts,
                               char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0 || !opts) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return;
    }

    if (opts->human) {
        _free_human_value(bytes, opts->si, buf, buf_size);
    }
    else {
        unsigned long long div = _free_divisor(opts->unit);
        unsigned long long scaled = (div > 0) ? (bytes / div) : bytes;
        snprintf(buf, buf_size, "%llu", scaled);
    }
}

/* ---- output ---- */

/**
 * @brief Print the column header row.
 */
static void _free_print_header(const free_opts_t * opts)
{
    if (!opts) {
        return;
    }

    const char * const * hdr;
    int nhdr;

    if (opts->wide) {
        hdr = free_hdr_wide;
        nhdr = free_hdr_wide_count;
    }
    else {
        hdr = free_hdr_narrow;
        nhdr = free_hdr_narrow_count;
    }

    /* indent past the label area */
    free_printf("%*s", FREE_LABEL_WIDTH, "");

    for (int i = 0; i < nhdr; i++) {
        free_printf(" %*s", FREE_COL_WIDTH, hdr[i]);
    }
    free_printf("\n");
}

/**
 * @brief Print a data row: label followed by formatted values.
 */
static void _free_print_row(const char * label,
                            const unsigned long long * values,
                            int nvals, const free_opts_t * opts)
{
    if (!label || !values || nvals <= 0 || !opts) {
        return;
    }

    free_printf("%-*s", FREE_LABEL_WIDTH, label);

    for (int i = 0; i < nvals; i++) {
        char buf[FREE_SUFFIX_MAX + 24];
        _free_format_value(values[i], opts, buf, sizeof(buf));
        free_printf(" %*s", FREE_COL_WIDTH, buf);
    }
    free_printf("\n");
}

/**
 * @brief Print the complete memory output (header + rows).
 */
static void _free_print_output(const mem_info_t * info,
                               const free_opts_t * opts)
{
    if (!info || !opts) {
        return;
    }

    _free_print_header(opts);

    if (opts->lohi) {
        /* Low/High rows instead of Mem */
        unsigned long long low_vals[7];
        unsigned long long high_vals[7];
        int nv = opts->wide ? 7 : 6;

        if (opts->wide) {
            low_vals[0] = info->low_total;
            low_vals[1] = info->low_total - info->low_free -
                          info->buffers - info->cache;
            low_vals[2] = info->low_free;
            low_vals[3] = info->shared;
            low_vals[4] = info->buffers;
            low_vals[5] = info->cache;
            low_vals[6] = info->available;
            high_vals[0] = info->high_total;
            high_vals[1] = info->high_total - info->high_free;
            high_vals[2] = info->high_free;
            high_vals[3] = 0;
            high_vals[4] = 0;
            high_vals[5] = 0;
            high_vals[6] = 0;
        }
        else {
            low_vals[0] = info->low_total;
            low_vals[1] = info->low_total - info->low_free -
                          info->buffers - info->cache;
            low_vals[2] = info->low_free;
            low_vals[3] = info->shared;
            low_vals[4] = info->buffers + info->cache;
            low_vals[5] = info->available;
            high_vals[0] = info->high_total;
            high_vals[1] = info->high_total - info->high_free;
            high_vals[2] = info->high_free;
            high_vals[3] = 0;
            high_vals[4] = 0;
            high_vals[5] = 0;
        }

        _free_print_row("Low:", low_vals, nv, opts);
        _free_print_row("High:", high_vals, nv, opts);
    }
    else {
        /* Mem row */
        unsigned long long mem_vals[7];
        int nv = opts->wide ? 7 : 6;

        if (opts->wide) {
            mem_vals[0] = info->total;
            mem_vals[1] = info->used;
            mem_vals[2] = info->free;
            mem_vals[3] = info->shared;
            mem_vals[4] = info->buffers;
            mem_vals[5] = info->cache;
            mem_vals[6] = info->available;
        }
        else {
            mem_vals[0] = info->total;
            mem_vals[1] = info->used;
            mem_vals[2] = info->free;
            mem_vals[3] = info->shared;
            mem_vals[4] = info->buffers + info->cache;
            mem_vals[5] = info->available;
        }
        _free_print_row("Mem:", mem_vals, nv, opts);
    }

    /* Swap row */
    unsigned long long swap_vals[7];
    int nsv = opts->wide ? 7 : 6;

    if (opts->wide) {
        swap_vals[0] = info->swap_total;
        swap_vals[1] = info->swap_used;
        swap_vals[2] = info->swap_free;
        swap_vals[3] = 0;
        swap_vals[4] = 0;
        swap_vals[5] = 0;
        swap_vals[6] = 0;
    }
    else {
        swap_vals[0] = info->swap_total;
        swap_vals[1] = info->swap_used;
        swap_vals[2] = info->swap_free;
        swap_vals[3] = 0;
        swap_vals[4] = 0;
        swap_vals[5] = 0;
    }
    _free_print_row("Swap:", swap_vals, nsv, opts);

    /* Total row (-t) */
    if (opts->total) {
        unsigned long long tot_vals[7];
        int ntv = opts->wide ? 7 : 6;

        if (opts->wide) {
            tot_vals[0] = info->total + info->swap_total;
            tot_vals[1] = info->used + info->swap_used;
            tot_vals[2] = info->free + info->swap_free;
            tot_vals[3] = info->shared;
            tot_vals[4] = info->buffers;
            tot_vals[5] = info->cache;
            tot_vals[6] = info->available;
        }
        else {
            tot_vals[0] = info->total + info->swap_total;
            tot_vals[1] = info->used + info->swap_used;
            tot_vals[2] = info->free + info->swap_free;
            tot_vals[3] = info->shared;
            tot_vals[4] = info->buffers + info->cache;
            tot_vals[5] = info->available;
        }
        _free_print_row("Total:", tot_vals, ntv, opts);
    }
}

/* ---- utility ---- */

static void _free_sleep_ms(unsigned long ms)
{
#ifdef FREE_PLATFORM_WINDOWS
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000UL);
    ts.tv_nsec = (long)((ms % 1000UL) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

/* ---- help/version/args ---- */

static void _free_print_help(void)
{
    free_printf(
        "Usage: free [options]\n"
        "Show the amount of free and used memory in the system.\n"
        "\n"
        "  -b, --bytes         display in bytes\n"
        "      --kilo          display in kilobytes (1000 bytes)\n"
        "      --mega          display in megabytes\n"
        "      --giga          display in gigabytes\n"
        "      --tera          display in terabytes\n"
        "      --peta          display in petabytes\n"
        "  -k, --kibi          display in kibibytes (1024 bytes, default)\n"
        "  -m, --mebi          display in mebibytes\n"
        "  -g, --gibi          display in gibibytes\n"
        "      --tebi          display in tebibytes\n"
        "      --pebi          display in pebibytes\n"
        "      --si            use power of 1000 (not 1024)\n"
        "      --iec           use power of 1024 (default)\n"
        "  -h, --human         show human-readable output\n"
        "  -l, --lohi          show detailed low and high memory statistics\n"
        "  -t, --total         show total for system + swap\n"
        "  -s N, --seconds N   repeat printing every N seconds\n"
        "  -c N, --count N     repeat printing N times, then exit\n"
        "  -w, --wide          wide output mode (separate buffers/cache)\n"
        "      --help          display this help and exit\n"
        "      --version       output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

static void _free_print_version(void)
{
    free_printf("free %s\n", FREE_VERSION_STR);
    free_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    free_printf("%s", "License MIT: <https://mit-license.org/>\n");
    free_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    free_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

static unsigned long _free_parse_ulong(const char * s)
{
    if (!s || !*s) {
        return 0;
    }
    errno = 0;
    char * end = NULL;
    unsigned long val = strtoul(s, &end, 10);
    if (end && *end != '\0') {
        return 0;
    }
    return val;
}

/**
 * @brief Extract a long option value: "--name=value" or "--name value".
 */
static const char * _free_long_value(int argc, char ** argv,
                                     const char * arg, int * i)
{
    const char * eq = strchr(arg, '=');
    if (eq) {
        return eq + 1;
    }
    if (*i + 1 < argc) {
        *i += 1;
        return argv[*i];
    }
    return NULL;
}

static int _free_parse_args(int argc, char ** argv, free_opts_t * opts)
{
    if (argc < 1 || !argv || !opts) {
        return -1;
    }

    opts->unit = FREE_UNIT_KIBI;
    opts->unit_set = false;
    opts->si = false;
    opts->human = false;
    opts->lohi = false;
    opts->total = false;
    opts->wide = false;
    opts->seconds = 0;
    opts->count = 0;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            /* remaining args are ignored (free takes no operands) */
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
                _free_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _free_print_version();
                exit(0);
            }
            if (strcmp(name, "bytes") == 0) {
                opts->unit = FREE_UNIT_BYTES;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "kilo") == 0) {
                opts->unit = FREE_UNIT_KILO;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "mega") == 0) {
                opts->unit = FREE_UNIT_MEGA;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "giga") == 0) {
                opts->unit = FREE_UNIT_GIGA;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "tera") == 0) {
                opts->unit = FREE_UNIT_TERA;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "peta") == 0) {
                opts->unit = FREE_UNIT_PETA;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "kibi") == 0) {
                opts->unit = FREE_UNIT_KIBI;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "mebi") == 0) {
                opts->unit = FREE_UNIT_MEBI;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "gibi") == 0) {
                opts->unit = FREE_UNIT_GIBI;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "tebi") == 0) {
                opts->unit = FREE_UNIT_TEBI;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "pebi") == 0) {
                opts->unit = FREE_UNIT_PEBI;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "si") == 0) {
                opts->si = true;
                continue;
            }
            if (strcmp(name, "iec") == 0) {
                opts->si = false;
                continue;
            }
            if (strcmp(name, "human") == 0) {
                opts->human = true;
                opts->unit_set = true;
                continue;
            }
            if (strcmp(name, "lohi") == 0) {
                opts->lohi = true;
                continue;
            }
            if (strcmp(name, "total") == 0) {
                opts->total = true;
                continue;
            }
            if (strcmp(name, "wide") == 0) {
                opts->wide = true;
                continue;
            }
            if (strcmp(name, "seconds") == 0) {
                const char * v = _free_long_value(argc, argv, arg, &i);
                if (!v) {
                    free_err_printf("free: option '--seconds' requires an argument\n");
                    return -1;
                }
                opts->seconds = _free_parse_ulong(v);
                if (opts->seconds == 0) {
                    free_err_printf("free: invalid argument '%s' for '--seconds'\n", v);
                    return -1;
                }
                continue;
            }
            if (strcmp(name, "count") == 0) {
                const char * v = _free_long_value(argc, argv, arg, &i);
                if (!v) {
                    free_err_printf("free: option '--count' requires an argument\n");
                    return -1;
                }
                opts->count = _free_parse_ulong(v);
                continue;
            }
            free_err_printf("free: unrecognized option '%s'\n", arg);
            free_err_printf("%s", "Try 'free --help' for more information.\n");
            return -1;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* short options, may be bundled; -s and -c take an argument */
            for (int j = 1; arg[j]; j++) {
                char c = arg[j];
                switch (c) {
                    case 'b':
                        opts->unit = FREE_UNIT_BYTES;
                        opts->unit_set = true;
                        break;
                    case 'k':
                        opts->unit = FREE_UNIT_KIBI;
                        opts->unit_set = true;
                        break;
                    case 'm':
                        opts->unit = FREE_UNIT_MEBI;
                        opts->unit_set = true;
                        break;
                    case 'g':
                        opts->unit = FREE_UNIT_GIBI;
                        opts->unit_set = true;
                        break;
                    case 'h':
                        opts->human = true;
                        opts->unit_set = true;
                        break;
                    case 'l':
                        opts->lohi = true;
                        break;
                    case 't':
                        opts->total = true;
                        break;
                    case 'w':
                        opts->wide = true;
                        break;
                    case 's':
                    case 'c': {
                        const char * v = NULL;
                        if (arg[j + 1] != '\0') {
                            v = arg + j + 1;
                            j = (int)strlen(arg) - 1;
                        }
                        else if (i + 1 < argc) {
                            v = argv[++i];
                        }
                        else {
                            free_err_printf("free: option requires an argument -- '%c'\n", c);
                            return -1;
                        }
                        unsigned long val = _free_parse_ulong(v);
                        if (c == 's') {
                            if (val == 0) {
                                free_err_printf("free: invalid argument '%s' for '--seconds'\n", v);
                                return -1;
                            }
                            opts->seconds = val;
                        }
                        else {
                            opts->count = val;
                        }
                        break;
                    }
                    default:
                        free_err_printf("free: invalid option -- '%c'\n", c);
                        free_err_printf("%s", "Try 'free --help' for more information.\n");
                        return -1;
                }
            }
        }
        else {
            /* free takes no operands */
            free_err_printf("free: extra operand '%s'\n", arg);
            free_err_printf("%s", "Try 'free --help' for more information.\n");
            return -1;
        }
    }

    /* resolve default unit when --si is set but no unit given */
    if (!opts->unit_set) {
        if (opts->si) {
            opts->unit = FREE_UNIT_KILO;
        }
        else {
            opts->unit = FREE_UNIT_KIBI;
        }
    }
    /* in human mode, the unit enum is FREE_UNIT_HUMAN */
    if (opts->human) {
        opts->unit = FREE_UNIT_HUMAN;
    }

    return 0;
}

/********************************
 *    main
 ********************************/

int main(int argc, char ** argv)
{
    free_opts_t opts;
    if (_free_parse_args(argc, argv, &opts) != 0) {
        return 1;
    }

    bool repeat = (opts.seconds > 0);
    unsigned long iteration = 0;

    do {
        mem_info_t info;
        if (_free_get_info(&info) != 0) {
            free_err_printf("free: cannot determine memory information\n");
            return 1;
        }

        if (iteration > 0 && repeat) {
            free_printf("\n");
        }

        _free_print_output(&info, &opts);
        free_fflush(free_out_stream);

        iteration++;
        if (repeat) {
            if (opts.count > 0 && iteration >= opts.count) {
                break;
            }
            _free_sleep_ms(opts.seconds * 1000UL);
        }
    } while (repeat);

    return 0;
}
