/**
 * @file date.c
 * @brief Cross-platform date command implementation
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils date(1).
 *
 * Key behaviors:
 *   - Display current (or parsed/-referenced) time in +FORMAT
 *   - Full GNU %X specifier set with padding/case modifiers
 *     (_ - 0 ^ #) and ':' modifier for %z
 *   - -u/--utc/--universal: UTC zone
 *   - -d/--date=STRING: parse a pragmatic subset of GNU date strings:
 *       @epoch[.frac], YYYY-MM-DD, YYYY-MM-DDTHH:MM:SS[.f],
 *       YYYY-MM-DD HH:MM:SS, YYYYMMDDHHMMSS, YYYYMMDD,
 *       YYYY/MM/DD, HH:MM:SS (today+time), and relative items:
 *       now/today/yesterday/tomorrow, N unit[s] [ago],
 *       next/last <unit|weekday>, bare weekday name.
 *     Units: second,minute,hour,day,week,month,year,fortnight
 *     (with common abbreviations). Dates are interpreted as local
 *     time unless -u is given (then UTC). Zone offsets in the string
 *     are not supported.
 *   - -I[FMT]/--iso-8601[=FMT]: date|hours|minutes|seconds|ns
 *   - -R/--rfc-email/--rfc-2822: RFC 5322
 *   - --rfc-3339=FMT: date|seconds|ns
 *   - -r/--reference=FILE: use file mtime
 *   - -s/--set=STRING or positional MMDDhhmm[[CC]YY][.ss]: set system
 *     time (requires privileges; POSIX clock_settime, Windows
 *     SetSystemTime). GNU semantics: -s prints nothing on success
 *     unless a +FORMAT is also given.
 *   - --help / --version
 *
 * Output is C-locale (English weekday/month names); %Z uses strftime.
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o date.exe date.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o date date.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o date date.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o date date.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o date date.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o date date.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/date>
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
    #define DATE_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define DATE_PLATFORM_LINUX   1
    #define DATE_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define DATE_PLATFORM_MACOS   1
    #define DATE_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define DATE_PLATFORM_FREEBSD 1
    #define DATE_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define DATE_PLATFORM_OPENBSD 1
    #define DATE_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define DATE_PLATFORM_NETBSD  1
    #define DATE_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define DATE_PLATFORM_POSIX   1
#else
    #define DATE_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef DATE_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef DATE_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef DATE_PLATFORM_NETBSD
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
#include <errno.h>
#include <limits.h>
#include <time.h>

#ifdef DATE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <sys/types.h>
    #include <sys/stat.h>
#else /* DATE_PLATFORM_POSIX */
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define DATE_VERSION_STR "v1.0.0"

/** @brief Buffer for parsed/tz names */
#define DATE_TZ_NAME 64

/** @brief Max tokens in a -d/--set date string */
#define DATE_MAX_TOKENS 64

/** @brief Default output format (matches GNU `date` with no args) */
#define DATE_DEFAULT_FMT "%a %b %e %H:%M:%S %Z %Y"

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Resolved time context for formatting.
 */
typedef struct {
    time_t      t;        /* epoch seconds (the instant, UTC) */
    long        nsec;     /* sub-second nanoseconds [0..999999999] */
    long        gmtoff;   /* offset of the chosen zone from UTC, in seconds */
    bool        utc;      /* UTC zone selected? */
    struct tm   tm;       /* broken-down time in the chosen zone */
} date_ctx_t;

/**
 * @brief Parsed options.
 */
typedef struct {
    bool        utc;
    bool        rfc_email;     /* -R */
    bool        iso8601;        /* -I */
    bool        rfc3339;        /* --rfc-3339 */
    bool        set;            /* -s */
    bool        reference;      /* -r */
    bool        dateref;        /* -d */
    char        iso_fmt[16];    /* date|hours|minutes|seconds|ns */
    char        rfc3339_fmt[16];/* date|seconds|ns */
    const char * date_str;      /* -d value (points into argv) */
    const char * set_str;       /* -s value */
    const char * ref_file;      /* -r value */
    const char * fmt;           /* +FORMAT value (points into argv) */
} date_opts_t;

/********************************
 *    static prototypes
 ********************************/
/* ---- time math ---- */
static bool _date_is_leap(int year);
static int  _date_days_in_month(int year, int month); /* month 1..12 */
static long _date_days_from_civil(int y, int m, int d); /* y/m/d, days since 1970-01-01 */
static int  _date_weekday(int y, int m, int d); /* 0=Sun..6=Sat */
static time_t _date_timegm(struct tm * tm);
static long _date_local_gmtoff(time_t t);

static int  _date_iso_weeks_in_year(int y);
static void _date_iso_week_year(const struct tm * tm, int * iso_year, int * iso_week);

/* ---- context ---- */
static void _date_now(date_ctx_t * ctx, bool utc);
static void _date_from_epoch(date_ctx_t * ctx, time_t t, long nsec, bool utc);
static void _date_refill(date_ctx_t * ctx);

/* ---- formatting ---- */
static void _date_emit_int(FILE * out, long long val, int width, char padchar);
static void _date_emit_str(FILE * out, const char * s, bool upper, bool swap);
static void _date_format(const date_ctx_t * ctx, const char * fmt, FILE * out);
static const char * _date_tz_name(const date_ctx_t * ctx);

/* ---- date string parsing ---- */
static int  _date_parse(const char * str, date_ctx_t * out, bool utc);
static int  _date_parse_setpos(const char * s, time_t * out, long * nsec);
static bool _date_is_setpos(const char * s);

/* ---- set system time ---- */
static int  _date_set_system(const date_ctx_t * ctx);

/* ---- file reference ---- */
static int  _date_stat_mtime(const char * path, time_t * t, long * nsec);

/* ---- help/version/args ---- */
static void _date_print_help(void);
static void _date_print_version(void);
static int  _date_parse_args(int argc, char ** argv, date_opts_t * opts);

/********************************
 *    static variables
 ********************************/

static const char * date_wday_short[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char * date_wday_full[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char * date_mon_short[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char * date_mon_full[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/********************************
 *    macros
 ********************************/

#ifndef date_out_stream
    #define date_out_stream stdout
#endif

#ifndef date_err_stream
    #define date_err_stream stderr
#endif

#ifndef date_printf
    #define date_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef date_err_printf
    #define date_err_printf(fmt, ...) \
        do { if (date_err_stream) { (void)fprintf((date_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef date_fflush
    #define date_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

#ifndef date_safe_free
    #define date_safe_free(p) \
        do { if ((p)) { free((p)); (p) = NULL; } } while (0)
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the date command
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    date_opts_t opts;

    memset(&opts, 0, sizeof(opts));
    if (_date_parse_args(argc, argv, &opts) != 0) {
        return 1;
    }

    date_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* ---- set mode: -s or positional MMDDhhmm[...] ---- */
    if (opts.set) {
        if (_date_parse(opts.set_str, &ctx, opts.utc) != 0) {
            date_err_printf("date: invalid date '%s'\n", opts.set_str);
            return 1;
        }
        if (_date_set_system(&ctx) != 0) {
            date_err_printf("date: cannot set date: %s\n", strerror(errno));
            return 1;
        }
        if (opts.fmt) {
            _date_format(&ctx, opts.fmt, date_out_stream);
            date_printf("%s", "\n");
            date_fflush(date_out_stream);
        }
        return 0;
    }

    /* ---- display mode: choose the instant ---- */
    if (opts.reference) {
        time_t t = 0;
        long ns = 0;
        if (_date_stat_mtime(opts.ref_file, &t, &ns) != 0) {
            date_err_printf("date: %s: %s\n", opts.ref_file, strerror(errno));
            return 1;
        }
        _date_from_epoch(&ctx, t, ns, opts.utc);
    }
    else if (opts.dateref) {
        if (_date_parse(opts.date_str, &ctx, opts.utc) != 0) {
            date_err_printf("date: invalid date '%s'\n", opts.date_str);
            return 1;
        }
    }
    else {
        _date_now(&ctx, opts.utc);
    }

    /* ---- choose format ---- */
    const char * fmt = opts.fmt;
    char iso[32];
    if (!fmt) {
        if (opts.iso8601) {
            const char * p = opts.iso_fmt[0] ? opts.iso_fmt : "date";
            if (strcmp(p, "date") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%d");
            }
            else if (strcmp(p, "hours") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%dT%H%z");
            }
            else if (strcmp(p, "minutes") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%dT%H:%M%z");
            }
            else if (strcmp(p, "seconds") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%dT%H:%M:%S%z");
            }
            else if (strcmp(p, "ns") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%dT%H:%M:%S.%N%z");
            }
            else {
                date_err_printf("date: invalid argument '%s' for '--iso-8601'\n", p);
                return 1;
            }
            fmt = iso;
        }
        else if (opts.rfc3339) {
            const char * p = opts.rfc3339_fmt;
            if (strcmp(p, "date") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%d");
            }
            else if (strcmp(p, "seconds") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%d %H:%M:%S%z");
            }
            else if (strcmp(p, "ns") == 0) {
                snprintf(iso, sizeof(iso), "%s", "%Y-%m-%d %H:%M:%S.%N%z");
            }
            else {
                date_err_printf("date: invalid argument '%s' for '--rfc-3339'\n", p);
                return 1;
            }
            fmt = iso;
        }
        else if (opts.rfc_email) {
            fmt = "%a, %d %b %Y %H:%M:%S %z";
        }
        else {
            fmt = DATE_DEFAULT_FMT;
        }
    }

    _date_format(&ctx, fmt, date_out_stream);
    date_printf("%s", "\n");
    date_fflush(date_out_stream);
    return 0;
}

/********************************
 *    static functions
 ********************************/

/* ---- portable reentrant time helpers ----
 * localtime_r / gmtime_r are POSIX. On Windows (MinGW) they are not
 * exposed by default, so provide thin wrappers that copy the result of
 * the non-reentrant localtime / gmtime into the caller's buffer. The
 * date command is single-threaded, so the shared static buffer is safe.
 */
#ifndef DATE_HAVE_LOCALTIME_R
    #if defined(DATE_PLATFORM_POSIX) && !defined(DATE_PLATFORM_WINDOWS)
        #define DATE_HAVE_LOCALTIME_R 1
    #else
        #define DATE_HAVE_LOCALTIME_R 0
    #endif
#endif

#if !DATE_HAVE_LOCALTIME_R
static struct tm * _date_localtime_r(const time_t * t, struct tm * out)
{
    if (!t || !out) {
        return NULL;
    }
    struct tm * p = localtime(t);
    if (!p) {
        return NULL;
    }
    *out = *p;
    return out;
}

static struct tm * _date_gmtime_r(const time_t * t, struct tm * out)
{
    if (!t || !out) {
        return NULL;
    }
    struct tm * p = gmtime(t);
    if (!p) {
        return NULL;
    }
    *out = *p;
    return out;
}
#define localtime_r _date_localtime_r
#define gmtime_r    _date_gmtime_r
#endif /* !DATE_HAVE_LOCALTIME_R */

/* ---- time math ---- */

static bool _date_is_leap(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int _date_days_in_month(int year, int month)
{
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && _date_is_leap(year)) {
        return 29;
    }
    return dim[month - 1];
}

/**
 * @brief Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant).
 *        Year/month/day are NOT clamped; m can be any int (months roll into years).
 */
static long _date_days_from_civil(int y, int m, int d)
{
    y += (m <= 2) ? -1 : 0;
    long era = ((y >= 0 ? y : y - 399) / 400);
    long yoe = y - era * 400;
    long mp = (m > 2) ? (m - 3) : (m + 9);
    long doy = (153 * mp + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int _date_weekday(int y, int m, int d)
{
    /* 1970-01-01 was Thursday (4). */
    long days = _date_days_from_civil(y, m, d);
    long w = (days % 7) + 4;
    w %= 7;
    if (w < 0) {
        w += 7;
    }
    return (int)w;
}

/**
 * @brief Inverse of gmtime: UTC broken-down → epoch seconds (no normalization).
 */
static time_t _date_timegm(struct tm * tm)
{
    long days = _date_days_from_civil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    long secs = days * 86400L
                + tm->tm_hour * 3600L
                + tm->tm_min * 60L
                + tm->tm_sec;
    return (time_t)secs;
}

/**
 * @brief Local UTC offset (seconds east of UTC) for a given instant.
 *        Portable trick: interpret local wall-clock fields as UTC and
 *        subtract the true epoch.
 */
static long _date_local_gmtoff(time_t t)
{
    struct tm local;
    if (!localtime_r(&t, &local)) {
        return 0;
    }
    struct tm copy = local;
    copy.tm_isdst = 0;
    time_t as_utc = _date_timegm(&copy);
    return (long)(as_utc - t);
}

static int _date_iso_weeks_in_year(int y)
{
    int w = _date_weekday(y, 1, 1);          /* 0=Sun..6=Sat */
    int iso = (w == 0) ? 7 : w;              /* 1=Mon..7=Sun */
    if (iso == 4 || (_date_is_leap(y) && iso == 3)) {
        return 53;
    }
    return 52;
}

static void _date_iso_week_year(const struct tm * tm, int * iso_year, int * iso_week)
{
    int y = tm->tm_year + 1900;
    int doy = tm->tm_yday + 1;
    int w = tm->tm_wday;
    int iso_wday = (w == 0) ? 7 : w;          /* 1=Mon..7=Sun */
    int week = (10 + doy - iso_wday) / 7;

    if (week < 1) {
        y -= 1;
        week = _date_iso_weeks_in_year(y);
    }
    else if (week > _date_iso_weeks_in_year(y)) {
        y += 1;
        week = 1;
    }
    *iso_year = y;
    *iso_week = week;
}

/* ---- context ---- */

static void _date_refill(date_ctx_t * ctx)
{
    if (ctx->utc) {
        gmtime_r(&ctx->t, &ctx->tm);
        ctx->gmtoff = 0;
    }
    else {
        localtime_r(&ctx->t, &ctx->tm);
        ctx->gmtoff = _date_local_gmtoff(ctx->t);
    }
}

static void _date_from_epoch(date_ctx_t * ctx, time_t t, long nsec, bool utc)
{
    ctx->t = t;
    ctx->nsec = nsec;
    ctx->utc = utc;
    _date_refill(ctx);
}

static void _date_now(date_ctx_t * ctx, bool utc)
{
    time_t t = 0;
    long ns = 0;
#ifdef DATE_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    /* 100-ns intervals since 1601-01-01; shift to 1970 epoch. */
    long long epoch100ns = (long long)u.QuadPart - 116444736000000000LL;
    if (epoch100ns < 0) {
        epoch100ns = 0;
    }
    t = (time_t)(epoch100ns / 10000000LL);
    ns = (long)((epoch100ns % 10000000LL) * 100);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        t = ts.tv_sec;
        ns = ts.tv_nsec;
    }
    else {
        t = time(NULL);
        ns = 0;
    }
#endif
    _date_from_epoch(ctx, t, ns, utc);
}

/* ---- formatting ---- */

static void _date_emit_int(FILE * out, long long val, int width, char padchar)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", val);
    if (len < 0) {
        return;
    }
    if (padchar != 0 && len < width) {
        for (int i = 0; i < width - len; i++) {
            fputc(padchar, out);
        }
    }
    fputs(buf, out);
}

static void _date_emit_str(FILE * out, const char * s, bool upper, bool swap)
{
    if (!s) {
        return;
    }
    for (const char * p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (upper) {
            c = (unsigned char)toupper(c);
        }
        else if (swap) {
            if (isupper(c)) {
                c = (unsigned char)tolower(c);
            }
            else if (islower(c)) {
                c = (unsigned char)toupper(c);
            }
        }
        fputc((int)c, out);
    }
}

static const char * _date_tz_name(const date_ctx_t * ctx)
{
    static char buf[DATE_TZ_NAME];
    buf[0] = '\0';
    if (ctx->utc) {
        return "UTC";
    }
    /* strftime gives the platform's tz abbreviation (locale-dependent). */
    if (strftime(buf, sizeof(buf), "%Z", &ctx->tm) == 0) {
        buf[0] = '\0';
    }
    return buf;
}

/**
 * @brief Format @p ctx according to @p fmt (GNU date semantics).
 *        Trailing newline is NOT added here.
 */
static void _date_format(const date_ctx_t * ctx, const char * fmt, FILE * out)
{
    const struct tm * tm = &ctx->tm;
    int iso_year = tm->tm_year + 1900;
    int iso_week = 0;
    _date_iso_week_year(tm, &iso_year, &iso_week);

    for (const char * p = fmt; *p; ) {
        if (*p != '%') {
            fputc(*p, out);
            p++;
            continue;
        }
        p++; /* skip % */

        /* Parse flags */
        char pad = 0;        /* 0=default, ' ', '-', '0' */
        bool upper = false;
        bool swap = false;
        int colons = 0;
        for (;;) {
            if (*p == '_') { pad = ' ';  p++; }
            else if (*p == '-') { pad = '-'; p++; }
            else if (*p == '0') { pad = '0'; p++; }
            else if (*p == '^') { upper = true; p++; }
            else if (*p == '#') { swap = true; p++; }
            else if (*p == 'E' || *p == 'O') { p++; } /* accepted, ignored */
            else if (*p == ':') { colons++; p++; }
            else { break; }
        }
        char spec = *p;
        if (spec == '\0') {
            break;
        }
        p++;

        /* Resolve default pad for numeric specifiers */
        char dflt_pad = 0;     /* 0 = no pad */
        int  width = 0;
        switch (spec) {
            case 'd': case 'H': case 'I': case 'm': case 'M': case 'S':
            case 'y': case 'Y': case 'C': case 'G': case 'g':
            case 'U': case 'V': case 'W':
                dflt_pad = '0';
                width = (spec == 'Y' || spec == 'G') ? 4
                        : ((spec == 'C' || spec == 'y' || spec == 'g') ? 2 : 2);
                break;
            case 'j': dflt_pad = '0'; width = 3; break;
            case 'N': dflt_pad = '0'; width = 9; break;
            case 'e': case 'k': case 'l': dflt_pad = ' '; width = 2; break;
            default: break;
        }
        char eff_pad = dflt_pad;
        int  eff_width = width;
        if (pad == '-') { eff_width = 0; eff_pad = 0; }
        else if (pad == ' ') { eff_pad = ' '; }
        else if (pad == '0') { eff_pad = '0'; }

        switch (spec) {
            /* literals */
            case '%': fputc('%', out); break;
            case 'n': fputc('\n', out); break;
            case 't': fputc('\t', out); break;

            /* weekday/month names (case modifiers apply) */
            case 'a': _date_emit_str(out, date_wday_short[tm->tm_wday], upper, swap); break;
            case 'A': _date_emit_str(out, date_wday_full[tm->tm_wday], upper, swap); break;
            case 'h': _date_emit_str(out, date_mon_short[tm->tm_mon], upper, swap); break;
            case 'b': _date_emit_str(out, date_mon_short[tm->tm_mon], upper, swap); break;
            case 'B': _date_emit_str(out, date_mon_full[tm->tm_mon], upper, swap); break;
            case 'p': _date_emit_str(out, tm->tm_hour < 12 ? "AM" : "PM", upper, swap); break;
            case 'P': _date_emit_str(out, tm->tm_hour < 12 ? "am" : "pm", upper, swap); break;
            case 'Z': _date_emit_str(out, _date_tz_name(ctx), upper, swap); break;

            /* numeric */
            case 'd': _date_emit_int(out, tm->tm_mday, eff_width, eff_pad); break;
            case 'e': _date_emit_int(out, tm->tm_mday, 2, (eff_pad ? eff_pad : ' ')); break;
            case 'H': _date_emit_int(out, tm->tm_hour, eff_width, eff_pad); break;
            case 'k': _date_emit_int(out, tm->tm_hour, 2, (eff_pad ? eff_pad : ' ')); break;
            case 'I': {
                int h = tm->tm_hour % 12;
                if (h == 0) { h = 12; }
                _date_emit_int(out, h, eff_width, eff_pad);
                break;
            }
            case 'l': {
                int h = tm->tm_hour % 12;
                if (h == 0) { h = 12; }
                _date_emit_int(out, h, 2, (eff_pad ? eff_pad : ' '));
                break;
            }
            case 'm': _date_emit_int(out, tm->tm_mon + 1, eff_width, eff_pad); break;
            case 'M': _date_emit_int(out, tm->tm_min, eff_width, eff_pad); break;
            case 'S': _date_emit_int(out, tm->tm_sec, eff_width, eff_pad); break;
            case 'j': _date_emit_int(out, tm->tm_yday + 1, eff_width, eff_pad); break;
            case 'y': _date_emit_int(out, (tm->tm_year + 1900) % 100, eff_width, eff_pad); break;
            case 'Y': _date_emit_int(out, tm->tm_year + 1900, eff_width, eff_pad); break;
            case 'C': _date_emit_int(out, (tm->tm_year + 1900) / 100, eff_width, eff_pad); break;
            case 'N': _date_emit_int(out, ctx->nsec, eff_width, eff_pad); break;
            case 'u': _date_emit_int(out, (tm->tm_wday == 0) ? 7 : tm->tm_wday, 0, 0); break;
            case 'w': _date_emit_int(out, tm->tm_wday, 0, 0); break;
            case 'q': {
                int q = (tm->tm_mon / 3) + 1;
                _date_emit_int(out, q, 0, 0);
                break;
            }

            /* week numbers */
            case 'U': {
                int u = (tm->tm_yday + 7 - tm->tm_wday) / 7;
                _date_emit_int(out, u, eff_width, eff_pad);
                break;
            }
            case 'W': {
                int mwday = (tm->tm_wday == 0) ? 6 : (tm->tm_wday - 1);
                int w = (tm->tm_yday + 7 - mwday) / 7;
                _date_emit_int(out, w, eff_width, eff_pad);
                break;
            }
            case 'V': _date_emit_int(out, iso_week, eff_width, eff_pad); break;
            case 'G': _date_emit_int(out, iso_year, eff_width, eff_pad); break;
            case 'g': _date_emit_int(out, iso_year % 100, eff_width, eff_pad); break;

            /* epoch */
            case 's': _date_emit_int(out, (long long)ctx->t, 0, 0); break;

            /* timezone offset with optional ':' modifier */
            case 'z': {
                long off = ctx->gmtoff;
                char sign = (off < 0) ? '-' : '+';
                long a = (off < 0) ? -off : off;
                int hh = (int)(a / 3600);
                int mm = (int)((a / 60) % 60);
                int ss = (int)(a % 60);
                fputc(sign, out);
                char zbuf[16];
                switch (colons) {
                    case 0:
                        snprintf(zbuf, sizeof(zbuf), "%02d%02d", hh, mm);
                        break;
                    case 1:
                        snprintf(zbuf, sizeof(zbuf), "%02d:%02d", hh, mm);
                        break;
                    case 2:
                        snprintf(zbuf, sizeof(zbuf), "%02d:%02d:%02d", hh, mm, ss);
                        break;
                    default:
                        if (mm == 0 && ss == 0) {
                            snprintf(zbuf, sizeof(zbuf), "%02d", hh);
                        }
                        else if (ss == 0) {
                            snprintf(zbuf, sizeof(zbuf), "%02d:%02d", hh, mm);
                        }
                        else {
                            snprintf(zbuf, sizeof(zbuf), "%02d:%02d:%02d", hh, mm, ss);
                        }
                        break;
                }
                fputs(zbuf, out);
                break;
            }

            /* composite formats */
            case 'D': _date_format(ctx, "%m/%d/%y", out); break;
            case 'F': _date_format(ctx, "%Y-%m-%d", out); break;
            case 'R': _date_format(ctx, "%H:%M", out); break;
            case 'T': _date_format(ctx, "%H:%M:%S", out); break;
            case 'r': _date_format(ctx, "%I:%M:%S %p", out); break;
            case 'c': _date_format(ctx, "%a %b %e %T %Y", out); break;
            case 'x': _date_format(ctx, "%m/%d/%y", out); break;
            case 'X': _date_format(ctx, "%H:%M:%S", out); break;

            default:
                /* Unknown specifier: emit verbatim %spec */
                fputc('%', out);
                fputc(spec, out);
                break;
        }
    }
}

/* ---- date string parsing ---- */

/* Classify a unit token. Returns:
 *   0 = not a unit
 *   1 = duration unit (seconds value via *seconds)
 *   2 = field unit (months value via *months)
 */
static int _date_unit(const char * tok, long * seconds, long * months)
{
    struct entry { const char * name; int kind; long v; };
    static const struct entry tbl[] = {
        {"second",     1, 1},
        {"seconds",    1, 1},
        {"sec",        1, 1},
        {"secs",       1, 1},
        {"s",          1, 1},
        {"minute",     1, 60},
        {"minutes",    1, 60},
        {"min",        1, 60},
        {"mins",       1, 60},
        {"hour",       1, 3600},
        {"hours",      1, 3600},
        {"hr",         1, 3600},
        {"hrs",        1, 3600},
        {"h",          1, 3600},
        {"day",        1, 86400},
        {"days",       1, 86400},
        {"d",          1, 86400},
        {"week",       1, 604800},
        {"weeks",      1, 604800},
        {"wk",         1, 604800},
        {"wks",        1, 604800},
        {"w",          1, 604800},
        {"fortnight", 1, 1209600},
        {"fortnights", 1, 1209600},
        {"month",      2, 1},
        {"months",     2, 1},
        {"mon",        2, 1},
        {"mons",       2, 1},
        {"M",          2, 1},
        {"year",       2, 12},
        {"years",      2, 12},
        {"yr",         2, 12},
        {"yrs",        2, 12},
        {"y",          2, 12},
        {NULL,         0, 0}
    };
    for (const struct entry * e = tbl; e->name; e++) {
        if (strcmp(tok, e->name) == 0) {
            if (e->kind == 1) {
                *seconds = e->v;
            }
            else {
                *months = e->v;
            }
            return e->kind;
        }
    }
    return 0;
}

/* Weekday name → 0..6 (Sun=0); -1 if not a weekday. */
static int _date_weekday_name(const char * tok)
{
    static const char * short_[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char * full_[]  = {"Sunday","Monday","Tuesday","Wednesday",
                                    "Thursday","Friday","Saturday"};
    for (int i = 0; i < 7; i++) {
        if (strcmp(tok, short_[i]) == 0 || strcmp(tok, full_[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/* Lowercase-compare two strings (ASCII). */
static bool _date_ieq(const char * a, const char * b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Apply ±N months to the context's instant (field math, zone-aware). */
static void _date_apply_months(date_ctx_t * ctx, long months)
{
    struct tm tm = ctx->tm;
    int y = tm.tm_year + 1900;
    int m = tm.tm_mon + 1;          /* 1..12 */
    long total = (long)y * 12 + (m - 1) + months;
    long q = total / 12;
    long r = total % 12;
    if (r < 0) { r += 12; q -= 1; }
    int ny = (int)q;
    int nm = (int)r + 1;
    int dim = _date_days_in_month(ny, nm);
    if (tm.tm_mday > dim) {
        tm.tm_mday = dim;
    }
    tm.tm_year = ny - 1900;
    tm.tm_mon = nm - 1;
    if (ctx->utc) {
        ctx->t = _date_timegm(&tm);
    }
    else {
        tm.tm_isdst = -1;
        ctx->t = mktime(&tm);
    }
    _date_refill(ctx);
}

/* Add a pure duration (seconds) to the instant (DST-safe). */
static void _date_apply_seconds(date_ctx_t * ctx, long long secs)
{
    ctx->t += (time_t)secs;
    _date_refill(ctx);
}

/* Set the time-of-day on the context (today's date + h:m:s). */
static void _date_set_time(date_ctx_t * ctx, int h, int m, int s, long ns)
{
    struct tm tm = ctx->tm;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    tm.tm_isdst = -1;
    if (ctx->utc) {
        ctx->t = _date_timegm(&tm);
    }
    else {
        ctx->t = mktime(&tm);
    }
    ctx->nsec = ns;
    _date_refill(ctx);
}

/**
 * @brief Parse an absolute ISO/numeric date token (optionally with a
 *        following time token). On success fills *tm fields and flags.
 * @param a        primary token (always the date candidate)
 * @param b        optional following token (may supply the time half)
 * @param used_b   set true if @p b supplied the time-of-day
 * @return 1 if a date was parsed, 0 otherwise.
 */
static int _date_parse_abs(const char * a, const char * b,
                           struct tm * tm, bool * has_time, long * nsec,
                           bool * used_b)
{
    int Y = -1, Mo = -1, D = -1, h = -1, mi = -1, s = -1;
    *has_time = false;
    *nsec = 0;
    *used_b = false;

    /* @epoch[.frac] handled by caller. */

    /* YYYY-MM-DD[Thh:mm:ss[.f]] or YYYY-MM-DD<space>hh:mm:ss[.f] */
    if (sscanf(a, "%d-%d-%d", &Y, &Mo, &D) == 3) {
        const char * rest = a;
        while (*rest && (*rest == '-' || isdigit((unsigned char)*rest))) {
            rest++;
        }
        if (*rest == 'T' || *rest == ' ') {
            rest++;
            if (sscanf(rest, "%d:%d:%d", &h, &mi, &s) >= 2) {
                *has_time = true;
                const char * dot = strchr(rest, '.');
                if (dot) {
                    long frac = 0;
                    int ndig = 0;
                    for (const char * p = dot + 1; isdigit((unsigned char)*p); p++) {
                        if (ndig < 9) {
                            frac = frac * 10 + (*p - '0');
                        }
                        ndig++;
                    }
                    for (; ndig < 9; ndig++) {
                        frac *= 10;
                    }
                    *nsec = frac;
                }
            }
        }
    }
    /* YYYYMMDDHHMMSS (14) or YYYYMMDD (8) */
    else if (strlen(a) == 14 && strspn(a, "0123456789") == 14) {
        sscanf(a, "%4d%2d%2d%2d%2d%2d", &Y, &Mo, &D, &h, &mi, &s);
        *has_time = true;
    }
    else if (strlen(a) == 8 && strspn(a, "0123456789") == 8) {
        sscanf(a, "%4d%2d%2d", &Y, &Mo, &D);
    }
    /* YYYY/MM/DD */
    else if (sscanf(a, "%d/%d/%d", &Y, &Mo, &D) == 3) {
        /* ok */
    }
    /* bare time "hh:mm:ss" (today's date) */
    else if (sscanf(a, "%d:%d:%d", &h, &mi, &s) == 3) {
        tm->tm_hour = h;
        tm->tm_min = mi;
        tm->tm_sec = s;
        *has_time = true;
        return 1;
    }
    else {
        return 0;
    }

    if (Y >= 0) {
        tm->tm_year = Y - 1900;
        tm->tm_mon = Mo - 1;
        tm->tm_mday = D;
    }
    if (h >= 0) {
        tm->tm_hour = h;
        tm->tm_min = (mi >= 0 ? mi : 0);
        tm->tm_sec = (s >= 0 ? s : 0);
        *has_time = true;
    }
    /* If the date token carried no time and a following token is a time,
     * consume it as the time half (with optional fractional seconds). */
    if (!*has_time && b && sscanf(b, "%d:%d:%d", &h, &mi, &s) == 3) {
        tm->tm_hour = h;
        tm->tm_min = mi;
        tm->tm_sec = s;
        *has_time = true;
        *used_b = true;
        const char * dot = strchr(b, '.');
        if (dot) {
            long frac = 0;
            int ndig = 0;
            for (const char * p = dot + 1; isdigit((unsigned char)*p); p++) {
                if (ndig < 9) {
                    frac = frac * 10 + (*p - '0');
                }
                ndig++;
            }
            for (; ndig < 9; ndig++) {
                frac *= 10;
            }
            *nsec = frac;
        }
    }
    return 1;
}

/**
 * @brief Parse a -d/--set date string into @p out.
 *        Starts from "now" and applies an absolute base (if any) plus
 *        relative adjustments. Returns 0 on success, -1 if nothing
 *        recognized at all.
 */
static int _date_parse(const char * str, date_ctx_t * out, bool utc)
{
    if (!str || !*str) {
        return -1;
    }
    _date_now(out, utc);

    char * dup = strdup(str);
    if (!dup) {
        return -1;
    }

    char * tokens[DATE_MAX_TOKENS];
    int ntok = 0;
    char * save = NULL;
    char * tok = strtok_r(dup, " \t,", &save);
    while (tok && ntok < DATE_MAX_TOKENS) {
        tokens[ntok++] = tok;
        tok = strtok_r(NULL, " \t,", &save);
    }

    bool base_set = false;
    int rc = 0;

    for (int i = 0; i < ntok; i++) {
        char * t = tokens[i];

        /* @epoch[.frac] */
        if (t[0] == '@') {
            char * end = NULL;
            double d = strtod(t + 1, &end);
            if (end != t + 1) {
                time_t et = (time_t)d;
                long ns = (long)((d - (double)et) * 1e9);
                if (ns < 0) { ns = 0; }
                _date_from_epoch(out, et, ns, utc);
                base_set = true;
                continue;
            }
        }

        /* now / today / yesterday / tomorrow */
        if (_date_ieq(t, "now")) {
            _date_now(out, utc);
            base_set = true;
            continue;
        }
        if (_date_ieq(t, "today")) {
            /* today = now with time at 00:00 */
            _date_set_time(out, 0, 0, 0, out->nsec);
            base_set = true;
            continue;
        }
        if (_date_ieq(t, "yesterday")) {
            _date_apply_seconds(out, -86400);
            _date_set_time(out, 0, 0, 0, 0);
            base_set = true;
            continue;
        }
        if (_date_ieq(t, "tomorrow")) {
            _date_apply_seconds(out, 86400);
            _date_set_time(out, 0, 0, 0, 0);
            base_set = true;
            continue;
        }

        /* next/last <unit|weekday> */
        if (_date_ieq(t, "next") || _date_ieq(t, "last")) {
            int sign = _date_ieq(t, "next") ? +1 : -1;
            if (i + 1 < ntok) {
                char * u = tokens[++i];
                long sec = 0, mon = 0;
                int wd = _date_weekday_name(u);
                if (wd >= 0) {
                    /* next/last weekday */
                    int cur = out->tm.tm_wday;
                    int delta;
                    if (sign > 0) {
                        delta = (wd - cur + 7) % 7;
                        if (delta == 0) {
                            delta = 7;
                        }
                    }
                    else {
                        delta = -((cur - wd + 7) % 7);
                        if (delta == 0) {
                            delta = -7;
                        }
                    }
                    _date_apply_seconds(out, delta * 86400LL);
                    continue;
                }
                int kind = _date_unit(u, &sec, &mon);
                if (kind == 1) {
                    _date_apply_seconds(out, sign * sec);
                    continue;
                }
                if (kind == 2) {
                    _date_apply_months(out, sign * mon);
                    continue;
                }
                /* unknown after next/last: error */
                rc = -1;
                goto done;
            }
            rc = -1;
            goto done;
        }

        /* bare weekday name → next occurrence forward */
        {
            int wd = _date_weekday_name(t);
            if (wd >= 0) {
                int cur = out->tm.tm_wday;
                int delta = (wd - cur + 7) % 7;
                _date_apply_seconds(out, delta * 86400LL);
                continue;
            }
        }

        /* N <unit> [ago]  OR  +N <unit> / -N <unit> */
        {
            char * end = NULL;
            long val = strtol(t, &end, 10);
            if (end != t && (*end == '\0' || _date_ieq(end, "ago"))) {
                bool ago = (end != t && _date_ieq(end, "ago"));
                /* need a unit next (unless "ago" was the unit-less form) */
                long sec = 0, mon = 0;
                if (ago) {
                    /* "N ago" with no unit is invalid */
                    rc = -1;
                    goto done;
                }
                if (i + 1 < ntok) {
                    char * u = tokens[++i];
                    int kind = _date_unit(u, &sec, &mon);
                    if (kind == 0) {
                        rc = -1;
                        goto done;
                    }
                    /* check trailing "ago" */
                    if (i + 1 < ntok && _date_ieq(tokens[i + 1], "ago")) {
                        i++;
                        val = -val;
                    }
                    if (kind == 1) {
                        _date_apply_seconds(out, (long long)val * sec);
                    }
                    else {
                        _date_apply_months(out, val * mon);
                    }
                    continue;
                }
                rc = -1;
                goto done;
            }
            /* "+N unit" / "-N unit" (leading sign) */
            if ((t[0] == '+' || t[0] == '-') && isdigit((unsigned char)t[1])) {
                long long val = strtoll(t, &end, 10);
                if (end != t && *end == '\0' && i + 1 < ntok) {
                    char * u = tokens[++i];
                    long sec = 0, mon = 0;
                    int kind = _date_unit(u, &sec, &mon);
                    if (kind == 1) {
                        _date_apply_seconds(out, val * sec);
                        continue;
                    }
                    else if (kind == 2) {
                        _date_apply_months(out, val * mon);
                        continue;
                    }
                }
            }
        }

        /* absolute date/time token(s) */
        {
            struct tm tm = out->tm;
            bool has_time = false;
            bool used_b = false;
            long ns = out->nsec;
            const char * next = (i + 1 < ntok) ? tokens[i + 1] : NULL;
            if (_date_parse_abs(t, next, &tm, &has_time, &ns, &used_b)) {
                if (used_b) {
                    i++; /* consume the time token */
                }
                tm.tm_isdst = -1;
                if (utc) {
                    out->t = _date_timegm(&tm);
                }
                else {
                    out->t = mktime(&tm);
                }
                out->nsec = ns;
                _date_refill(out);
                base_set = true;
                continue;
            }
        }

        /* "epoch" literal or unknown token */
        if (_date_ieq(t, "epoch")) {
            _date_from_epoch(out, 0, 0, utc);
            base_set = true;
            continue;
        }

        /* Unknown token: GNU errors here. */
        rc = -1;
        goto done;
    }

    if (!base_set && ntok == 0) {
        rc = -1;
    }

done:
    date_safe_free(dup);
    return rc;
}

/* ---- positional set-time form MMDDhhmm[[CC]YY][.ss] ---- */

static bool _date_is_setpos(const char * s)
{
    if (!s || !*s) {
        return false;
    }
    int dots = 0;
    for (const char * p = s; *p; p++) {
        if (*p == '.') {
            dots++;
            if (dots > 1) {
                return false;
            }
        }
        else if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    /* total digit count must be 8, 10, or 12 (optionally +2 after dot) */
    int digits = (int)strspn(s, "0123456789");
    /* digits covers leading run; recount properly */
    int total = 0;
    for (const char * p = s; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            total++;
        }
    }
    (void)digits;
    return total == 8 || total == 10 || total == 12 || total == 14;
}

static int _date_parse_setpos(const char * s, time_t * out, long * nsec)
{
    /* Strip digits (and an optional '.' separator) from the input.
     *   MMDDhhmm            → 8 digits
     *   MMDDhhmmYY          → 10 digits
     *   MMDDhhmmCCYY        → 12 digits
     *   MMDDhhmmCCYY.ss     → 12 digits + .ss (ss parsed from after the dot)
     *   MMDDhhmmYY.ss       → 10 digits + .ss
     *   MMDDhhmm.ss         → 8 digits  + .ss
     */
    char digits[32];
    int nd = 0;
    const char * dotp = NULL;
    for (const char * p = s; *p && nd < 31; p++) {
        if (*p == '.') {
            if (dotp) {
                return -1;
            }
            dotp = p;
            break;            /* main digit run ends at the dot */
        }
        else if (isdigit((unsigned char)*p)) {
            digits[nd++] = *p;
        }
        else {
            return -1;
        }
    }
    digits[nd] = '\0';
    if (nd != 8 && nd != 10 && nd != 12) {
        return -1;
    }

    /* Pull fields two digits at a time. */
    int a[6] = {0, 0, 0, 0, 0, 0};
    for (int k = 0; k < 6; k++) {
        if (k * 2 + 1 < nd) {
            a[k] = (digits[k * 2] - '0') * 10 + (digits[k * 2 + 1] - '0');
        }
    }
    int MM = a[0];
    int DD = a[1];
    int hh = a[2];
    int mm = a[3];
    int ss = 0;

    int YY = -1, CCYY = -1;
    if (nd == 10) {
        YY = a[4];
    }
    else if (nd == 12) {
        CCYY = a[4] * 100 + a[5];
    }

    /* optional .ss after the dot */
    if (dotp) {
        ss = 0;
        for (const char * p = dotp + 1; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                return -1;
            }
            ss = ss * 10 + (*p - '0');
        }
        if (ss > 60) {
            return -1;
        }
    }

    /* resolve year */
    int year;
    if (CCYY >= 0) {
        year = CCYY;
    }
    else if (YY >= 0) {
        /* 2-digit year: 69-99 → 19xx, 00-68 → 20xx (POSIX) */
        year = (YY >= 69) ? (1900 + YY) : (2000 + YY);
    }
    else {
        /* use current year */
        time_t now = time(NULL);
        struct tm nt;
        localtime_r(&now, &nt);
        year = nt.tm_year + 1900;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = MM - 1;
    tm.tm_mday = DD;
    tm.tm_hour = hh;
    tm.tm_min = mm;
    tm.tm_sec = ss;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    if (t == (time_t)-1) {
        return -1;
    }
    *out = t;
    *nsec = 0;
    return 0;
}

/* ---- set system time ---- */

static int _date_set_system(const date_ctx_t * ctx)
{
#ifdef DATE_PLATFORM_WINDOWS
    SYSTEMTIME st;
    struct tm utc;
    time_t t = ctx->t;
    gmtime_r(&t, &utc);
    st.wYear = (WORD)(utc.tm_year + 1900);
    st.wMonth = (WORD)(utc.tm_mon + 1);
    st.wDay = (WORD)utc.tm_mday;
    st.wHour = (WORD)utc.tm_hour;
    st.wMinute = (WORD)utc.tm_min;
    st.wSecond = (WORD)utc.tm_sec;
    st.wMilliseconds = (WORD)(ctx->nsec / 1000000);
    st.wDayOfWeek = (WORD)utc.tm_wday;
    if (!SetSystemTime(&st)) {
        errno = EPERM;
        return -1;
    }
    return 0;
#else
    struct timespec ts;
    ts.tv_sec = ctx->t;
    ts.tv_nsec = ctx->nsec;
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        return -1;
    }
    return 0;
#endif
}

/* ---- file reference ---- */

static int _date_stat_mtime(const char * path, time_t * t, long * nsec)
{
    *t = 0;
    *nsec = 0;
#ifdef DATE_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return -1;
    }
    *t = st.st_mtime;
    return 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    *t = st.st_mtime;
    *nsec = (long)st.st_mtim.tv_nsec;
    return 0;
#endif
}

/* ---- help/version ---- */

static void _date_print_help(void)
{
    date_printf(
        "Usage: date [OPTION]... [+FORMAT]\n"
        "  or:  date [-u|--utc|--universal] [MMDDhhmm[[CC]YY][.ss]] [+FORMAT]\n"
        "Display the current time in the given FORMAT, or set the system date.\n"
        "\n"
        "  -d, --date=STRING        display time described by STRING, not 'now'\n"
        "  -I[FMT], --iso-8601[=FMT]  ISO 8601 date/time\n"
        "                             FMT: date,hours,minutes,seconds,ns\n"
        "  -R, --rfc-email          RFC 5322 date and time\n"
        "      --rfc-3339=FMT       RFC 3339 date (FMT: date,seconds,ns)\n"
        "  -r, --reference=FILE     display last modification time of FILE\n"
        "  -s, --set=STRING         set time described by STRING\n"
        "  -u, --utc, --universal   print or set Coordinated Universal Time (UTC)\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "FORMAT controls the output. Interpreted sequences include:\n"
        "  %%%%  %%  %%a %%A %%b %%B %%c %%C %%d %%D %%e %%F %%g %%G %%h %%H %%I %%j\n"
        "  %%k %%l %%m %%M %%n %%N %%p %%P %%q %%r %%R %%s %%S %%t %%T %%u %%U %%V\n"
        "  %%w %%W %%x %%X %%y %%Y %%z %%Z\n"
        "Padding modifiers: %%_ (space) %%- (none) %%0 (zero); case: %%^ %%#;\n"
        "  %%:z %%::z %%:::z for offset with separators.\n"
        "\n"
        "-d STRING supports: @epoch, YYYY-MM-DD[THH:MM:SS], YYYYMMDD[HHMMSS],\n"
        "  YYYY/MM/DD, HH:MM:SS, now/today/yesterday/tomorrow,\n"
        "  N unit[s] [ago], next/last <unit|weekday>, weekday name.\n"
        "Units: second,minute,hour,day,week,month,year,fortnight.\n"
        "Names are C-locale (English); zone offsets in -d strings are ignored.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

static void _date_print_version(void)
{
    date_printf("date %s\n", DATE_VERSION_STR);
    date_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    date_printf("%s", "License MIT: <https://mit-license.org/>\n");
    date_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    date_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/* ---- option parsing ---- */

/**
 * @brief Extract the value of a long option: "--name=value" or "--name value".
 *        On entry @p arg is the full argv element; @p *i is its index.
 *        If the value is in the next argv element, *i is advanced.
 * @return pointer to the value (may be NULL if missing).
 */
static const char * _date_long_value(int argc, char ** argv,
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

static int _date_parse_args(int argc, char ** argv, date_opts_t * opts)
{
    if (argc < 1 || !argv) {
        return -1;
    }

    /* Non-option operands collected after the first non-option. */
    const char * operands[16];
    int nops = 0;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            /* Everything after is an operand. */
            for (int j = i + 1; j < argc && nops < 16; j++) {
                operands[nops++] = argv[j];
            }
            i = argc;
            break;
        }

        if (strncmp(arg, "--", 2) == 0) {
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[40];
            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _date_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _date_print_version();
                exit(0);
            }
            if (strcmp(name, "utc") == 0 || strcmp(name, "universal") == 0) {
                opts->utc = true;
                continue;
            }
            if (strcmp(name, "rfc-email") == 0 || strcmp(name, "rfc-2822") == 0) {
                opts->rfc_email = true;
                continue;
            }
            if (strcmp(name, "iso-8601") == 0) {
                opts->iso8601 = true;
                const char * v = eq ? eq + 1 : NULL;
                if (v) {
                    snprintf(opts->iso_fmt, sizeof(opts->iso_fmt), "%s", v);
                }
                continue;
            }
            if (strcmp(name, "rfc-3339") == 0) {
                opts->rfc3339 = true;
                const char * v = _date_long_value(argc, argv, arg, &i);
                if (!v) {
                    date_err_printf("date: option '--rfc-3339' requires an argument\n");
                    return -1;
                }
                snprintf(opts->rfc3339_fmt, sizeof(opts->rfc3339_fmt), "%s", v);
                continue;
            }
            if (strcmp(name, "date") == 0) {
                opts->dateref = true;
                const char * v = _date_long_value(argc, argv, arg, &i);
                if (!v) {
                    date_err_printf("date: option '--date' requires an argument\n");
                    return -1;
                }
                opts->date_str = v;
                continue;
            }
            if (strcmp(name, "set") == 0) {
                opts->set = true;
                const char * v = _date_long_value(argc, argv, arg, &i);
                if (!v) {
                    date_err_printf("date: option '--set' requires an argument\n");
                    return -1;
                }
                opts->set_str = v;
                continue;
            }
            if (strcmp(name, "reference") == 0) {
                opts->reference = true;
                const char * v = _date_long_value(argc, argv, arg, &i);
                if (!v) {
                    date_err_printf("date: option '--reference' requires an argument\n");
                    return -1;
                }
                opts->ref_file = v;
                continue;
            }
            date_err_printf("date: unrecognized option '%s'\n", arg);
            date_err_printf("%s", "Try 'date --help' for more information.\n");
            return -1;
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* Short options may be bundled; -d/-s/-r/-I take an argument */
            for (int j = 1; arg[j]; j++) {
                char c = arg[j];
                switch (c) {
                    case 'u':
                        opts->utc = true;
                        break;
                    case 'R':
                        opts->rfc_email = true;
                        break;
                    case 'I':
                        opts->iso8601 = true;
                        if (arg[j + 1] != '\0') {
                            snprintf(opts->iso_fmt, sizeof(opts->iso_fmt),
                                     "%s", arg + j + 1);
                            j = (int)strlen(arg) - 1;
                        }
                        break;
                    case 'd':
                    case 's':
                    case 'r': {
                        bool need = (c == 'd') || (c == 's') || (c == 'r');
                        if (!need) {
                            break;
                        }
                        const char * v = NULL;
                        if (arg[j + 1] != '\0') {
                            v = arg + j + 1;
                            j = (int)strlen(arg) - 1;
                        }
                        else if (i + 1 < argc) {
                            v = argv[++i];
                        }
                        else {
                            date_err_printf("date: option requires an argument -- '%c'\n", c);
                            return -1;
                        }
                        if (c == 'd') {
                            opts->dateref = true;
                            opts->date_str = v;
                        }
                        else if (c == 's') {
                            opts->set = true;
                            opts->set_str = v;
                        }
                        else {
                            opts->reference = true;
                            opts->ref_file = v;
                        }
                        break;
                    }
                    default:
                        date_err_printf("date: invalid option -- '%c'\n", c);
                        date_err_printf("%s", "Try 'date --help' for more information.\n");
                        return -1;
                }
            }
        }
        else if (nops < 16) {
            operands[nops++] = arg;
        }
        else {
            date_err_printf("date: extra operand '%s'\n", arg);
            return -1;
        }
    }

    /* Resolve operands: a leading '+' is the format; else setpos. */
    for (int k = 0; k < nops; k++) {
        const char * op = operands[k];
        if (op[0] == '+') {
            if (opts->fmt) {
                date_err_printf("date: extra operand '%s'\n", op);
                return -1;
            }
            opts->fmt = op + 1;
            if (k != 0 || nops > 1) {
                /* GNU allows only the format as a sole operand (unless setting) */
                /* If we also have a setpos or set_str, that's an error. */
            }
            continue;
        }
        /* non-'+' operand */
        if (opts->set || opts->dateref || opts->reference) {
            /* extra operand alongside -s/-d/-r */
            date_err_printf("date: extra operand '%s'\n", op);
            return -1;
        }
        if (!opts->set && _date_is_setpos(op)) {
            time_t t = 0;
            long ns = 0;
            if (_date_parse_setpos(op, &t, &ns) != 0) {
                date_err_printf("date: invalid date '%s'\n", op);
                return -1;
            }
            /* set mode via positional form */
            opts->set = true;
            opts->set_str = NULL; /* use ctx directly: stash via a static? */
            /* We need to carry t/ns into main; simplest: build a date_ctx
               here and store on a static for main. Instead, re-parse path:
               convert t into a "@N" string and reuse -s. */
            char buf[32];
            snprintf(buf, sizeof(buf), "@%ld", (long)t);
            opts->set_str = strdup(buf);
            if (k + 1 < nops) {
                date_err_printf("date: extra operand '%s'\n", operands[k + 1]);
                return -1;
            }
            continue;
        }
        date_err_printf("date: invalid date '%s'\n", op);
        return -1;
    }

    /* mutual-exclusion sanity */
    int sources = (opts->dateref ? 1 : 0) + (opts->reference ? 1 : 0)
                 + (opts->set ? 1 : 0);
    if (sources > 1) {
        date_err_printf("%s", "date: the options "
                        "'--date', '--reference' and '--set' are mutually exclusive\n");
        return -1;
    }
    if (opts->iso8601 + opts->rfc3339 + (opts->rfc_email ? 1 : 0) > 1) {
        date_err_printf("%s", "date: multiple output formats requested\n");
        return -1;
    }
    if (opts->set && opts->fmt == NULL) {
        /* -s with no format: GNU prints nothing; allowed. */
    }
    return 0;
}
