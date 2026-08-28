/**
 * @file time.c
 * @brief Cross-platform implementation of the GNU time command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU time (time 1.9+).
 *
 * Key behaviors:
 *   - -p, --portability:         POSIX output format (real/user/sys in seconds)
 *   - -f, --format=FORMAT:       custom output format string
 *   - -o, --output=FILE:         write timing to FILE instead of stderr
 *   - -a, --append:              append to FILE instead of overwriting (with -o)
 *   - -v, --verbose:             verbose detailed output
 *   - --help / --version:        display help or version information
 *   - Default format: "%Uuser %Ssystem %Eelapsed %PCPU (%Xtext+%Ddata %Mmax)k
 *                      %Iinputs+%Ooutputs (%Fmajor+%Rminor)pagefaults %Wswaps"
 *   - On POSIX: uses wait4() / getrusage() for resource usage
 *   - On Windows: uses GetProcessTimes() for timing info
 *
 * Format specifiers (subset of GNU time):
 *   %C  command name and arguments
 *   %D  average unshared data area size (Kbytes)
 *   %E  elapsed real time (hours:minutes:seconds or seconds)
 *   %F  number of major page faults
 *   %I  number of file system inputs
 *   %k  number of signals delivered to process
 *   %M  maximum resident set size (Kbytes)
 *   %O  number of file system outputs
 *   %P  percent of CPU used
 *   %R  number of minor (reclaimable) page faults
 *   %S  total CPU seconds used by the system (kernel)
 *   %U  total CPU seconds used by the user
 *   %W  number of times process was swapped out
 *   %X  average amount of shared text (Kbytes)
 *   %Z  system's page size (bytes)
 *   %c  number of involuntary context switches
 *   %e  elapsed real time in seconds (floating point)
 *   %k  number of signals delivered
 *   %p  average unshared stack size (Kbytes)
 *   %r  number of socket messages received
 *   %s  number of socket messages sent
 *   %t  average resident set size (Kbytes)
 *   %w  number of voluntary context switches
 *   %x  exit status of command
 *   %%  literal %
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o time.exe time.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o time time.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o time time.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o time time.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o time time.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o time time.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/time>
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
    #define TIME_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define TIME_PLATFORM_LINUX   1
    #define TIME_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define TIME_PLATFORM_MACOS   1
    #define TIME_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define TIME_PLATFORM_FREEBSD 1
    #define TIME_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define TIME_PLATFORM_OPENBSD 1
    #define TIME_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define TIME_PLATFORM_NETBSD  1
    #define TIME_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define TIME_PLATFORM_POSIX   1
#else
    #define TIME_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef TIME_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef TIME_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef TIME_PLATFORM_NETBSD
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
#include <limits.h>
#include <errno.h>

#ifdef TIME_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <process.h>
    #include <windows.h>
    #include <psapi.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <signal.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define TIME_VERSION_STR "v1.0.0"

/** @brief Default format string */
#define TIME_DEFAULT_FORMAT \
    "%Uuser %Ssystem %Eelapsed %PCPU (%Xtext+%Ddata %Mmax)k " \
    "%Iinputs+%Ooutputs (%Fmajor+%Rminor)pagefaults %Wswaps"

/** @brief Portable (POSIX) format string */
#define TIME_PORTABLE_FORMAT "real %e\nuser %U\nsys %S"

/** @brief Output buffer size for format expansion */
#define TIME_FORMAT_BUF_SIZE 4096

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options structure for time
 */
typedef struct {
    bool portable;           /**< -p: POSIX format */
    const char * format;     /**< -f: custom format string */
    const char * output_file;/**< -o: output file path */
    bool append;             /**< -a: append to output file */
    bool verbose;            /**< -v: verbose output */
} time_opts;

/**
 * @brief Timing and resource usage results
 */
typedef struct {
    double real_sec;          /**< Elapsed real (wall-clock) time in seconds */
    double user_sec;          /**< User CPU time in seconds */
    double sys_sec;           /**< System CPU time in seconds */
    long max_rss_kb;          /**< Maximum resident set size in KB */
    long page_faults_major;   /**< Major page faults */
    long page_faults_minor;   /**< Minor page faults */
    long fs_inputs;           /**< File system inputs (block operations) */
    long fs_outputs;          /**< File system outputs (block operations) */
    long swaps;               /**< Number of swaps */
    long signals;             /**< Number of signals received */
    long n_vol_ctx;           /**< Voluntary context switches */
    long n_invol_ctx;         /**< Involuntary context switches */
    long shared_text_kb;      /**< Average shared text size (KB) */
    long unshared_data_kb;    /**< Average unshared data size (KB) */
    long unshared_stack_kb;   /**< Average unshared stack size (KB) */
    long avg_rss_kb;          /**< Average resident set size (KB) */
    long sock_msgs_recv;      /**< Socket messages received */
    long sock_msgs_sent;      /**< Socket messages sent */
    long page_size;           /**< System page size in bytes */
    int exit_status;          /**< Exit status of the command */
} time_result;

/********************************
 *    static prototypes
 ********************************/
static void         _time_print_help(void);
static void         _time_print_version(void);
static bool         _time_streq(const char * a, const char * b);
static int          _time_parse_options(int argc, char ** argv, time_opts * opts,
                                       int * cmd_arg_idx);
static int          _time_run_command(char ** argv, time_result * result);
static void         _time_format_output(const time_opts * opts,
                                        const time_result * result,
                                        const char * cmd_line,
                                        char * buf, size_t buf_size);
static void         _time_format_verbose(const time_result * result,
                                         const char * cmd_line,
                                         char * buf, size_t buf_size);
static void         _time_format_elapsed(double seconds, char * buf, size_t buf_size);
static void         _time_write_output(const time_opts * opts, const char * output_str);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream.
 *        Defaults to libc @c stderr (time writes timing to stderr).
 */
#ifndef time_out_stream
    #define time_out_stream stderr
#endif

/**
 * @brief Default error stream.
 *        Defaults to libc @c stderr .
 */
#ifndef time_err_stream
    #define time_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 */
#ifndef time_printf
    #define time_printf(fmt, ...) fprintf(time_out_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef time_err_printf
    #define time_err_printf(fmt, ...) fprintf(time_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 */
#ifndef time_fflush
    #define time_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the time command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Run the specified command and measure time/resources
 *   3. Output timing results according to format options
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return exit status of the executed command (or 1 on internal error)
 */
int main(int argc, char ** argv)
{
#ifdef TIME_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    time_opts opts;
    memset(&opts, 0, sizeof(opts));

    int cmd_arg_idx = 0;
    int parse_ret = _time_parse_options(argc, argv, &opts, &cmd_arg_idx);

    if (parse_ret < 0) {
        /* --help or --version was printed */
        return 0;
    }
    if (parse_ret > 0) {
        return 1;
    }

    /* Check that we have a command to run */
    if (cmd_arg_idx >= argc) {
        time_err_printf("time: missing command\n");
        time_err_printf("Try 'time --help' for more information.\n");
        return 1;
    }

    /* Build the command line string for %C */
    char cmd_line[4096];
    cmd_line[0] = '\0';
    size_t pos = 0;
    for (int i = cmd_arg_idx; i < argc && pos < sizeof(cmd_line) - 1; i++) {
        if (i > cmd_arg_idx && pos < sizeof(cmd_line) - 1) {
            cmd_line[pos++] = ' ';
        }
        size_t arg_len = strlen(argv[i]);
        if (pos + arg_len < sizeof(cmd_line)) {
            memcpy(cmd_line + pos, argv[i], arg_len);
            pos += arg_len;
        }
        else {
            memcpy(cmd_line + pos, argv[i], sizeof(cmd_line) - pos - 1);
            pos = sizeof(cmd_line) - 1;
            break;
        }
    }
    cmd_line[pos] = '\0';

    /* Run the command and measure time */
    time_result result;
    memset(&result, 0, sizeof(result));

    int exec_ret = _time_run_command(argv + cmd_arg_idx, &result);
    if (exec_ret < 0) {
        /* Internal error running the command */
        return 127;
    }

    result.exit_status = exec_ret;

    /* Determine page size */
#ifdef TIME_PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    result.page_size = si.dwPageSize;
#else
    result.page_size = sysconf(_SC_PAGESIZE);
#endif

    /* Format and write output */
    char output_buf[TIME_FORMAT_BUF_SIZE];
    output_buf[0] = '\0';

    if (opts.verbose) {
        _time_format_verbose(&result, cmd_line, output_buf, sizeof(output_buf));
    }
    else {
        _time_format_output(&opts, &result, cmd_line, output_buf, sizeof(output_buf));
    }

    _time_write_output(&opts, output_buf);

    time_fflush(time_out_stream);

    return exec_ret;
}

/********************************
 *    static functions — option parsing
 ********************************/

/**
 * @brief Parse command-line options for time
 *
 * @param argc         argument count
 * @param argv         argument vector
 * @param opts         output options struct
 * @param cmd_arg_idx  output: index in argv where the command starts
 * @return 0 on success, -1 if help/version printed, 1 on error
 */
static int _time_parse_options(int argc, char ** argv, time_opts * opts,
                               int * cmd_arg_idx)
{
    *cmd_arg_idx = argc;

    int i;
    for (i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_time_streq(arg, "--")) {
            *cmd_arg_idx = i + 1;
            return 0;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_time_streq(arg, "--help")) {
                _time_print_help();
                return -1;
            }
            if (_time_streq(arg, "--version")) {
                _time_print_version();
                return -1;
            }
            if (_time_streq(arg, "--portability")) {
                opts->portable = true;
                continue;
            }
            if (_time_streq(arg, "--append")) {
                opts->append = true;
                continue;
            }
            if (_time_streq(arg, "--verbose")) {
                opts->verbose = true;
                continue;
            }
            /* --format=FORMAT */
            if (strncmp(arg, "--format=", 9) == 0) {
                opts->format = arg + 9;
                continue;
            }
            /* --output=FILE */
            if (strncmp(arg, "--output=", 9) == 0) {
                opts->output_file = arg + 9;
                continue;
            }

            time_err_printf("time: unrecognized option '%s'\n", arg);
            time_err_printf("Try 'time --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case 'p':
                        opts->portable = true;
                        p++;
                        break;

                    case 'a':
                        opts->append = true;
                        p++;
                        break;

                    case 'v':
                        opts->verbose = true;
                        p++;
                        break;

                    case 'f':
                        /* -f FORMAT: next argument is the format string */
                        if (*(p + 1) != '\0') {
                            opts->format = p + 1;
                            p += strlen(p + 1) + 1;
                        }
                        else {
                            if (i + 1 < argc) {
                                i++;
                                opts->format = argv[i];
                                p++;
                            }
                            else {
                                time_err_printf("time: option requires an argument -- 'f'\n");
                                time_err_printf("Try 'time --help' for more information.\n");
                                return 1;
                            }
                        }
                        break;

                    case 'o':
                        /* -o FILE: next argument is the output file */
                        if (*(p + 1) != '\0') {
                            opts->output_file = p + 1;
                            p += strlen(p + 1) + 1;
                        }
                        else {
                            if (i + 1 < argc) {
                                i++;
                                opts->output_file = argv[i];
                                p++;
                            }
                            else {
                                time_err_printf("time: option requires an argument -- 'o'\n");
                                time_err_printf("Try 'time --help' for more information.\n");
                                return 1;
                            }
                        }
                        break;

                    default:
                        time_err_printf("time: invalid option -- '%c'\n", *p);
                        time_err_printf("Try 'time --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — this is the command */
        *cmd_arg_idx = i;
        return 0;
    }

    return 0;
}

/********************************
 *    static functions — command execution and timing
 ********************************/

#ifdef TIME_PLATFORM_WINDOWS

/**
 * @brief Run a command and measure time/resources on Windows.
 *
 * Uses _spawnvp to execute the command and GetProcessTimes for
 * timing information. On Windows, process creation via spawn
 * doesn't give direct access to the process handle for timing,
 * so we use wall clock timing and approximate CPU times.
 *
 * @param argv    NULL-terminated argument vector
 * @param result  output: timing results
 * @return exit code of the command, or -1 on internal error
 */
static int _time_run_command(char ** argv, time_result * result)
{
    LARGE_INTEGER freq, start, end;

    /* Get high-resolution timer frequency */
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    int exit_code = _spawnvp(_P_WAIT, argv[0], (const char * const *)argv);

    QueryPerformanceCounter(&end);

    if (exit_code < 0) {
        time_err_printf("time: %s: %s\n", argv[0], strerror(errno));
        result->real_sec = 0.0;
        result->user_sec = 0.0;
        result->sys_sec = 0.0;
        return -1;
    }

    /* Calculate real time */
    double real_time = (double)(end.QuadPart - start.QuadPart) /
                       (double)freq.QuadPart;
    result->real_sec = real_time;

    /* On Windows with _spawnvp, we don't have direct access to the
     * process handle for GetProcessTimes. We approximate user/sys
     * time as 0 since we can't reliably get them through spawn.
     * A more accurate implementation would use CreateProcess,
     * but _spawnvp is simpler and matches the xargs pattern. */
    result->user_sec = 0.0;
    result->sys_sec = 0.0;

    /* Set other resource values to 0 (not available through spawn) */
    result->max_rss_kb = 0;
    result->page_faults_major = 0;
    result->page_faults_minor = 0;
    result->fs_inputs = 0;
    result->fs_outputs = 0;
    result->swaps = 0;
    result->signals = 0;
    result->n_vol_ctx = 0;
    result->n_invol_ctx = 0;
    result->shared_text_kb = 0;
    result->unshared_data_kb = 0;
    result->unshared_stack_kb = 0;
    result->avg_rss_kb = 0;
    result->sock_msgs_recv = 0;
    result->sock_msgs_sent = 0;

    return exit_code;
}

#else /* POSIX platforms */

/**
 * @brief Run a command and measure time/resources on POSIX systems.
 *
 * Uses fork/execvp and wait4() with rusage to get resource usage.
 *
 * @param argv    NULL-terminated argument vector
 * @param result  output: timing results
 * @return exit code of the command, or -1 on internal error
 */
static int _time_run_command(char ** argv, time_result * result)
{
    struct timeval tv_start, tv_end;

    /* Get wall clock start time */
    gettimeofday(&tv_start, NULL);

    pid_t pid = fork();
    if (pid < 0) {
        time_err_printf("time: fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        /* Child */
        execvp(argv[0], argv);
        fprintf(stderr, "time: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    /* Parent: wait for child and collect resource usage */
    int status;
    struct rusage usage;

    if (wait4(pid, &status, 0, &usage) < 0) {
        time_err_printf("time: wait4: %s\n", strerror(errno));
        return -1;
    }

    gettimeofday(&tv_end, NULL);

    /* Calculate real time */
    result->real_sec = (double)(tv_end.tv_sec - tv_start.tv_sec) +
                       (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000000.0;

    /* User CPU time */
    result->user_sec = (double)usage.ru_utime.tv_sec +
                       (double)usage.ru_utime.tv_usec / 1000000.0;

    /* System CPU time */
    result->sys_sec = (double)usage.ru_stime.tv_sec +
                      (double)usage.ru_stime.tv_usec / 1000000.0;

    /* Maximum resident set size (in KB on Linux, bytes on some BSDs) */
#ifdef __linux__
    result->max_rss_kb = usage.ru_maxrss;  /* Linux: kilobytes */
#elif defined(__APPLE__) || defined(__FreeBSD__) || \
      defined(__OpenBSD__) || defined(__NetBSD__)
    result->max_rss_kb = usage.ru_maxrss / 1024;  /* BSD: bytes -> KB */
#else
    result->max_rss_kb = usage.ru_maxrss;
#endif

    /* Page faults */
    result->page_faults_major = usage.ru_majflt;
    result->page_faults_minor = usage.ru_minflt;

    /* Block I/O operations */
    result->fs_inputs = usage.ru_inblock;
    result->fs_outputs = usage.ru_oublock;

    /* Swaps */
    result->swaps = usage.ru_nswap;

    /* Signals */
    result->signals = usage.ru_nsignals;

    /* Context switches */
    result->n_vol_ctx = usage.ru_nvcsw;
    result->n_invol_ctx = usage.ru_nivcsw;

    /* Fields not available in rusage: set to 0 */
    result->shared_text_kb = 0;
    result->unshared_data_kb = 0;
    result->unshared_stack_kb = 0;
    result->avg_rss_kb = 0;
    result->sock_msgs_recv = 0;
    result->sock_msgs_sent = 0;

    /* Determine exit status */
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

#endif /* TIME_PLATFORM_WINDOWS */

/********************************
 *    static functions — output formatting
 ********************************/

/**
 * @brief Format elapsed time as HH:MM:SS or MM:SS.
 * @param seconds   elapsed time in seconds
 * @param buf       output buffer
 * @param buf_size  buffer size
 */
static void _time_format_elapsed(double seconds, char * buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;

    long secs = (long)seconds;
    int fraction = (int)((seconds - (double)secs) * 100.0 + 0.5);
    if (fraction >= 100) { fraction = 99; }

    int hours = secs / 3600;
    int minutes = (secs % 3600) / 60;
    int sec = secs % 60;

    if (hours > 0) {
        snprintf(buf, buf_size, "%d:%02d:%02d.%02d", hours, minutes, sec, fraction);
    }
    else {
        snprintf(buf, buf_size, "%d:%02d.%02d", minutes, sec, fraction);
    }
}

/**
 * @brief Expand a format string with timing data.
 * @param opts      options (portable, format)
 * @param result    timing results
 * @param cmd_line  command line string
 * @param buf       output buffer
 * @param buf_size  buffer size
 */
static void _time_format_output(const time_opts * opts,
                                const time_result * result,
                                const char * cmd_line,
                                char * buf, size_t buf_size)
{
    const char * fmt;
    if (opts->format) {
        fmt = opts->format;
    }
    else if (opts->portable) {
        fmt = TIME_PORTABLE_FORMAT;
    }
    else {
        fmt = TIME_DEFAULT_FORMAT;
    }

    size_t pos = 0;
    char tmp[256];

    while (*fmt && pos + 1 < buf_size) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 'C':
                    snprintf(tmp, sizeof(tmp), "%s", cmd_line ? cmd_line : "");
                    break;
                case 'D':
                    snprintf(tmp, sizeof(tmp), "%ld", result->unshared_data_kb);
                    break;
                case 'E':
                    _time_format_elapsed(result->real_sec, tmp, sizeof(tmp));
                    break;
                case 'F':
                    snprintf(tmp, sizeof(tmp), "%ld", result->page_faults_major);
                    break;
                case 'I':
                    snprintf(tmp, sizeof(tmp), "%ld", result->fs_inputs);
                    break;
                case 'M':
                    snprintf(tmp, sizeof(tmp), "%ld", result->max_rss_kb);
                    break;
                case 'O':
                    snprintf(tmp, sizeof(tmp), "%ld", result->fs_outputs);
                    break;
                case 'P':
                {
                    double total_cpu = result->user_sec + result->sys_sec;
                    int pct = (result->real_sec > 0) ?
                              (int)(total_cpu / result->real_sec * 100.0) : 0;
                    snprintf(tmp, sizeof(tmp), "%d%%", pct);
                    break;
                }
                case 'R':
                    snprintf(tmp, sizeof(tmp), "%ld", result->page_faults_minor);
                    break;
                case 'S':
                    snprintf(tmp, sizeof(tmp), "%.2f", result->sys_sec);
                    break;
                case 'U':
                    snprintf(tmp, sizeof(tmp), "%.2f", result->user_sec);
                    break;
                case 'W':
                    snprintf(tmp, sizeof(tmp), "%ld", result->swaps);
                    break;
                case 'X':
                    snprintf(tmp, sizeof(tmp), "%ld", result->shared_text_kb);
                    break;
                case 'Z':
                    snprintf(tmp, sizeof(tmp), "%ld", result->page_size);
                    break;
                case 'c':
                    snprintf(tmp, sizeof(tmp), "%ld", result->n_invol_ctx);
                    break;
                case 'e':
                    snprintf(tmp, sizeof(tmp), "%.2f", result->real_sec);
                    break;
                case 'f':
                    snprintf(tmp, sizeof(tmp), "%.2f", result->real_sec);
                    break;
                case 'k':
                    snprintf(tmp, sizeof(tmp), "%ld", result->signals);
                    break;
                case 'p':
                    snprintf(tmp, sizeof(tmp), "%ld", result->unshared_stack_kb);
                    break;
                case 'r':
                    snprintf(tmp, sizeof(tmp), "%ld", result->sock_msgs_recv);
                    break;
                case 's':
                    snprintf(tmp, sizeof(tmp), "%ld", result->sock_msgs_sent);
                    break;
                case 't':
                    snprintf(tmp, sizeof(tmp), "%ld", result->avg_rss_kb);
                    break;
                case 'w':
                    snprintf(tmp, sizeof(tmp), "%ld", result->n_vol_ctx);
                    break;
                case 'x':
                    snprintf(tmp, sizeof(tmp), "%d", result->exit_status);
                    break;
                case '%':
                    tmp[0] = '%';
                    tmp[1] = '\0';
                    break;
                default:
                    /* Unknown: output % and the char */
                    tmp[0] = '%';
                    tmp[1] = *fmt;
                    tmp[2] = '\0';
                    break;
            }
            size_t tlen = strlen(tmp);
            if (pos + tlen < buf_size) {
                memcpy(buf + pos, tmp, tlen);
                pos += tlen;
            }
            else {
                memcpy(buf + pos, tmp, buf_size - pos - 1);
                pos = buf_size - 1;
            }
            fmt++;
        }
        else {
            buf[pos++] = *fmt++;
        }
    }

    /* Add newline if not already present (for default/portable formats) */
    if (pos > 0 && buf[pos - 1] != '\n' && pos + 1 < buf_size) {
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
}

/**
 * @brief Format verbose output (GNU time -v style).
 * @param result    timing results
 * @param cmd_line  command line string
 * @param buf       output buffer
 * @param buf_size  buffer size
 */
static void _time_format_verbose(const time_result * result,
                                 const char * cmd_line,
                                 char * buf, size_t buf_size)
{
    char elapsed_str[64];
    _time_format_elapsed(result->real_sec, elapsed_str, sizeof(elapsed_str));

    double total_cpu = result->user_sec + result->sys_sec;
    int pct = (result->real_sec > 0) ?
              (int)(total_cpu / result->real_sec * 100.0) : 0;

    int written = snprintf(buf, buf_size,
        "Command being timed: \"%s\"\n"
        "User time (seconds): %.2f\n"
        "System time (seconds): %.2f\n"
        "Percent of CPU this job got: %d%%\n"
        "Elapsed (wall clock) time (h:mm:ss or m:ss): %s\n"
        "Average shared text size (kbytes): %ld\n"
        "Average unshared data size (kbytes): %ld\n"
        "Average stack size (kbytes): %ld\n"
        "Average total size (kbytes): %ld\n"
        "Maximum resident set size (kbytes): %ld\n"
        "Average resident set size (kbytes): %ld\n"
        "Major (requiring I/O) page faults: %ld\n"
        "Minor (reclaiming a frame) page faults: %ld\n"
        "Voluntary context switches: %ld\n"
        "Involuntary context switches: %ld\n"
        "Swaps: %ld\n"
        "File system inputs: %ld\n"
        "File system outputs: %ld\n"
        "Socket messages sent: %ld\n"
        "Socket messages received: %ld\n"
        "Signals delivered: %ld\n"
        "Page size (bytes): %ld\n"
        "Exit status: %d\n",
        cmd_line ? cmd_line : "",
        result->user_sec,
        result->sys_sec,
        pct,
        elapsed_str,
        result->shared_text_kb,
        result->unshared_data_kb,
        result->unshared_stack_kb,
        result->shared_text_kb + result->unshared_data_kb + result->unshared_stack_kb,
        result->max_rss_kb,
        result->avg_rss_kb,
        result->page_faults_major,
        result->page_faults_minor,
        result->n_vol_ctx,
        result->n_invol_ctx,
        result->swaps,
        result->fs_inputs,
        result->fs_outputs,
        result->sock_msgs_sent,
        result->sock_msgs_recv,
        result->signals,
        result->page_size,
        result->exit_status
    );

    if (written < 0 || (size_t)written >= buf_size) {
        buf[buf_size - 1] = '\0';
    }
}

/**
 * @brief Write the timing output to the appropriate destination.
 * @param opts         options (output_file, append)
 * @param output_str   formatted output string
 */
static void _time_write_output(const time_opts * opts, const char * output_str)
{
    if (opts->output_file) {
        const char * mode = opts->append ? "a" : "w";
        FILE * f = fopen(opts->output_file, mode);
        if (!f) {
            time_err_printf("time: cannot open '%s': %s\n",
                           opts->output_file, strerror(errno));
            time_printf("%s", output_str);
            return;
        }
        fputs(output_str, f);
        fclose(f);
    }
    else {
        time_printf("%s", output_str);
    }
}

/********************************
 *    static functions — help/version
 ********************************/

/**
 * @brief Print usage/help information
 */
static void _time_print_help(void)
{
    time_printf(
        "Usage: time [OPTIONS] COMMAND [ARGS]...\n"
        "Run COMMAND and print timing statistics.\n"
        "\n"
        "  -p, --portability      print POSIX portable format\n"
        "  -f, --format=FORMAT    use FORMAT as the output format string\n"
        "  -o, --output=FILE      write timing output to FILE instead of stderr\n"
        "  -a, --append           append to FILE instead of overwriting\n"
        "  -v, --verbose          print detailed verbose output\n"
        "      --help             display this help and exit\n"
        "      --version          output version information and exit\n"
        "\n"
        "Format specifiers:\n"
        "  %%C  command name and arguments\n"
        "  %%D  average unshared data area size (Kbytes)\n"
        "  %%E  elapsed real time (hours:minutes:seconds)\n"
        "  %%F  number of major page faults\n"
        "  %%I  number of file system inputs\n"
        "  %%M  maximum resident set size (Kbytes)\n"
        "  %%O  number of file system outputs\n"
        "  %%P  percent of CPU used\n"
        "  %%R  number of minor (reclaimable) page faults\n"
        "  %%S  total CPU seconds used by the system (kernel)\n"
        "  %%U  total CPU seconds used by the user\n"
        "  %%W  number of times process was swapped out\n"
        "  %%X  average amount of shared text (Kbytes)\n"
        "  %%Z  system page size (bytes)\n"
        "  %%c  number of involuntary context switches\n"
        "  %%e  elapsed real time in seconds (floating point)\n"
        "  %%k  number of signals delivered to the process\n"
        "  %%p  average unshared stack size (Kbytes)\n"
        "  %%r  number of socket messages received\n"
        "  %%s  number of socket messages sent\n"
        "  %%t  average resident set size (Kbytes)\n"
        "  %%w  number of voluntary context switches\n"
        "  %%x  exit status of command\n"
        "  %%%%  literal %%\n"
        "\n"
        "NOTE: your shell may have its own version of time, which usually supersedes\n"
        "the version described here.  Please refer to your shell's documentation\n"
        "for details about the options it supports.\n"
    );
}

/**
 * @brief Print version information
 */
static void _time_print_version(void)
{
    time_printf("time (GNU time) %s\n", TIME_VERSION_STR);
    time_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    time_printf("%s", "License MIT: <https://mit-license.org/>\n");
    time_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    time_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _time_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}
