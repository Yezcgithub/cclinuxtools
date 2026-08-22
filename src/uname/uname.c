/**
 * @file uname.c
 * @brief Cross-platform implementation of the Linux uname command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with GNU coreutils uname(1).
 *
 * Key behaviors:
 *   - -a, --all:        print all information (snrvmpio order),
 *                       omitting -p/-i if they are "unknown"
 *   - -s, --kernel-name:    kernel name (sysname)         [default]
 *   - -n, --nodename:       network node hostname
 *   - -r, --kernel-release: kernel release string
 *   - -v, --kernel-version: kernel version/build string
 *   - -m, --machine:        machine hardware name
 *   - -p, --processor:      processor type (may be "unknown")
 *   - -i, --hardware-platform: hardware platform (may be "unknown")
 *   - -o, --operating-system: operating system name
 *   - --help / --version:   recognized and handled
 *
 * Platform data sources:
 *   POSIX  : uname(2) for sysname/nodename/release/version/machine;
 *            operating system derived from compile-time platform;
 *            processor/hardware-platform default to machine value.
 *   Windows: kernel-name="Windows"; nodename from GetComputerNameA;
 *            release/version from GetVersionEx; machine/processor/
 *            hardware-platform from PROCESSOR_ARCHITECTURE /
 *            PROCESSOR_IDENTIFIER environment variables;
 *            operating-system="Windows".
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o uname.exe uname.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o uname uname.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o uname uname.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o uname uname.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o uname uname.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o uname uname.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/uname>
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
    #define UNAME_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define UNAME_PLATFORM_LINUX   1
    #define UNAME_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define UNAME_PLATFORM_MACOS   1
    #define UNAME_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define UNAME_PLATFORM_FREEBSD 1
    #define UNAME_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define UNAME_PLATFORM_OPENBSD 1
    #define UNAME_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define UNAME_PLATFORM_NETBSD  1
    #define UNAME_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define UNAME_PLATFORM_POSIX   1
#else
    #define UNAME_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef UNAME_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef UNAME_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef UNAME_PLATFORM_NETBSD
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

#ifdef UNAME_PLATFORM_WINDOWS
    #include <windows.h>
#else /* UNAME_PLATFORM_POSIX */
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define UNAME_VERSION_STR "v1.0.0"

/** @brief Maximum long option name length accepted by the parser */
#define UNAME_OPT_NAME_MAX 64

/** @brief Size of each uname info field buffer */
#define UNAME_FIELD_SIZE 256

/** @brief String used when a field is unavailable (GNU-compatible) */
#define UNAME_UNKNOWN_STR "unknown"

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Output selection flags for uname
 *   kernel_name       - -s: sysname / kernel name
 *   nodename          - -n: network node hostname
 *   kernel_release    - -r: kernel release
 *   kernel_version    - -v: kernel version/build
 *   machine           - -m: machine hardware name
 *   processor         - -p: processor type
 *   hardware_platform - -i: hardware platform
 *   operating_system  - -o: operating system name
 *   all               - -a: equivalent to -snrvmpio
 */
typedef struct {
    bool kernel_name;
    bool nodename;
    bool kernel_release;
    bool kernel_version;
    bool machine;
    bool processor;
    bool hardware_platform;
    bool operating_system;
    bool all;
} uname_opts_t;

/**
 * @brief Collected system information fields
 *   Each field is a NUL-terminated string; unavailable fields are
 *   set to UNAME_UNKNOWN_STR ("unknown").
 */
typedef struct {
    char kernel_name[UNAME_FIELD_SIZE];
    char nodename[UNAME_FIELD_SIZE];
    char kernel_release[UNAME_FIELD_SIZE];
    char kernel_version[UNAME_FIELD_SIZE];
    char machine[UNAME_FIELD_SIZE];
    char processor[UNAME_FIELD_SIZE];
    char hardware_platform[UNAME_FIELD_SIZE];
    char operating_system[UNAME_FIELD_SIZE];
} uname_info_t;

/********************************
 *    static prototypes
 ********************************/
static void _uname_print_help(void);
static void _uname_print_version(void);
static int  _uname_parse_args(int argc, char ** argv, uname_opts_t * opts);
static int  _uname_get_info(uname_info_t * info);
static int  _uname_output(const uname_opts_t * opts, const uname_info_t * info);

#ifdef UNAME_PLATFORM_WINDOWS
static void _uname_win_get_env(const char * name, char * buf, size_t bufsize);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for uname_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef uname_out_stream
    #define uname_out_stream stdout
#endif

/**
 * @brief Default stderr stream for uname_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef uname_err_stream
    #define uname_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 */
#ifndef uname_printf
    #define uname_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef uname_err_printf
    #define uname_err_printf(fmt, ...) \
        do { if (uname_err_stream) { (void)fprintf((uname_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 */
#ifndef uname_fputs
    #define uname_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef uname_fflush
    #define uname_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/**
 * @brief Safe strcmp wrapper with NULL guards.
 *        Two NULLs considered equal (both missing).
 * @return true if strings match, false otherwise
 */
#ifndef uname_streq
    #define uname_streq(a, b) \
        (((a) && (b)) ? (strcmp((a), (b)) == 0) : ((!(a) && !(b)) ? true : false))
#endif

/**
 * @brief Copy a string into a fixed-size buffer, always NUL-terminating.
 *        If src is NULL, the buffer is set to an empty string.
 *        Implemented as a static inline function (rather than a macro)
 *        so that callers passing stack array addresses do not trigger
 *        -Waddress warnings from constant-address NULL checks.
 * @param dst    destination buffer
 * @param src    source string (may be NULL)
 * @param size   size of destination buffer
 */
static inline void uname_strcpy_safe(char * dst, const char * src, size_t size)
{
    if (!dst || size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

/********************************
 *    static variables
 ********************************/

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the uname command
 *
 * Processing flow:
 *   1. Parse command-line options (default is -s if none given)
 *   2. Collect system information from the host
 *   3. Print selected fields, space-separated, terminated by a newline
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on usage error
 */
int main(int argc, char ** argv)
{
    uname_opts_t opts;
    memset(&opts, 0, sizeof(opts));

    int parse_rc = _uname_parse_args(argc, argv, &opts);
    if (parse_rc < 0) {
        return 1;
    }

    /* If no specific option was selected, default to -s (kernel name) */
    bool any_selected = opts.kernel_name || opts.nodename ||
                        opts.kernel_release || opts.kernel_version ||
                        opts.machine || opts.processor ||
                        opts.hardware_platform || opts.operating_system ||
                        opts.all;
    if (!any_selected) {
        opts.kernel_name = true;
    }

    uname_info_t info;
    if (_uname_get_info(&info) != 0) {
        return 1;
    }

    int rc = _uname_output(&opts, &info);
    uname_fflush(uname_out_stream);
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information (GNU-compatible text)
 */
static void _uname_print_help(void)
{
    uname_printf(
        "Usage: uname [OPTION]...\n"
        "Print certain system information.  With no OPTION, same as -s.\n"
        "\n"
        "  -a, --all                print all information, in the following order,\n"
        "                             except omit -p and -i if unknown:\n"
        "  -s, --kernel-name        print the kernel name\n"
        "  -n, --nodename           print the network node hostname\n"
        "  -r, --kernel-release     print the kernel release\n"
        "  -v, --kernel-version     print the kernel version\n"
        "  -m, --machine            print the machine hardware name\n"
        "  -p, --processor          print the processor type (non-portable)\n"
        "  -i, --hardware-platform  print the hardware platform (non-portable)\n"
        "  -o, --operating-system   print the operating system\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _uname_print_version(void)
{
    uname_printf("uname %s\n", UNAME_VERSION_STR);
    uname_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    uname_printf("%s", "License MIT: <https://mit-license.org/>\n");
    uname_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    uname_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments into uname_opts_t
 *
 * GNU uname accepts short option clustering (-srm) and long options
 * (--kernel-name, --release, etc.). -a expands to all of -snrvmpio.
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options structure
 * @return 0 on success, -1 on unknown option
 */
static int _uname_parse_args(int argc, char ** argv, uname_opts_t * opts)
{
    if (!opts) {
        return -1;
    }

    if (argc < 1 || !argv) {
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* Long options (arg starts with "--" but is not exactly "--") */
        if (strncmp(arg, "--", 2) == 0 && arg[2] != '\0') {
            /* Extract name before '=' if present */
            char * eq = strchr(arg, '=');
            size_t name_len = eq ? (size_t)(eq - arg - 2) : strlen(arg + 2);
            char name[UNAME_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _uname_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _uname_print_version();
                exit(0);
            }
            if (strcmp(name, "all") == 0) {
                opts->all = true;
            }
            else if (strcmp(name, "kernel-name") == 0) {
                opts->kernel_name = true;
            }
            else if (strcmp(name, "nodename") == 0) {
                opts->nodename = true;
            }
            else if (strcmp(name, "kernel-release") == 0) {
                opts->kernel_release = true;
            }
            else if (strcmp(name, "kernel-version") == 0) {
                opts->kernel_version = true;
            }
            else if (strcmp(name, "machine") == 0) {
                opts->machine = true;
            }
            else if (strcmp(name, "processor") == 0) {
                opts->processor = true;
            }
            else if (strcmp(name, "hardware-platform") == 0) {
                opts->hardware_platform = true;
            }
            else if (strcmp(name, "operating-system") == 0) {
                opts->operating_system = true;
            }
            else {
                uname_err_printf("uname: unrecognized option '%s'\n", arg);
                uname_err_printf("%s", "Try 'uname --help' for more information.\n");
                return -1;
            }
            continue;
        }

        /* Short options: must start with '-' and not be just "-" */
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'a':
                        opts->all = true;
                        break;

                    case 's':
                        opts->kernel_name = true;
                        break;

                    case 'n':
                        opts->nodename = true;
                        break;

                    case 'r':
                        opts->kernel_release = true;
                        break;

                    case 'v':
                        opts->kernel_version = true;
                        break;

                    case 'm':
                        opts->machine = true;
                        break;

                    case 'p':
                        opts->processor = true;
                        break;

                    case 'i':
                        opts->hardware_platform = true;
                        break;

                    case 'o':
                        opts->operating_system = true;
                        break;

                    default:
                        uname_err_printf("uname: invalid option -- '%c'\n", arg[j]);
                        uname_err_printf("%s", "Try 'uname --help' for more information.\n");
                        return -1;
                }
            }
            continue;
        }

        /* uname does not take file/positional arguments */
        uname_err_printf("uname: extra operand '%s'\n", arg);
        uname_err_printf("%s", "Try 'uname --help' for more information.\n");
        return -1;
    }

    /* -a expands to all individual flags (in GNU output order) */
    if (opts->all) {
        opts->kernel_name       = true;
        opts->nodename          = true;
        opts->kernel_release    = true;
        opts->kernel_version    = true;
        opts->machine           = true;
        opts->processor         = true;
        opts->hardware_platform = true;
        opts->operating_system  = true;
    }

    return 0;
}

#ifdef UNAME_PLATFORM_WINDOWS
/**
 * @brief Read an environment variable into a buffer (Windows helper).
 *        On failure, leaves the buffer as an empty string.
 * @param name     environment variable name
 * @param buf      output buffer
 * @param bufsize  size of output buffer
 */
static void _uname_win_get_env(const char * name, char * buf, size_t bufsize)
{
    if (!buf || bufsize == 0) {
        return;
    }
    buf[0] = '\0';
    if (!name) {
        return;
    }
    DWORD n = GetEnvironmentVariableA(name, buf, (DWORD)bufsize);
    if (n == 0 || n >= bufsize) {
        buf[0] = '\0';
    }
}
#endif

/**
 * @brief Collect all system information fields into info.
 *
 * On POSIX, uses uname(2). On Windows, uses Win32 APIs and
 * environment variables.
 *
 * Unavailable fields are set to "unknown" (GNU-compatible).
 *
 * @param info  output info structure
 * @return 0 on success, -1 on failure
 */
static int _uname_get_info(uname_info_t * info)
{
    if (!info) {
        return -1;
    }

    /* Initialize all fields to "unknown" */
    strcpy(info->kernel_name,       UNAME_UNKNOWN_STR);
    strcpy(info->nodename,          UNAME_UNKNOWN_STR);
    strcpy(info->kernel_release,    UNAME_UNKNOWN_STR);
    strcpy(info->kernel_version,   UNAME_UNKNOWN_STR);
    strcpy(info->machine,           UNAME_UNKNOWN_STR);
    strcpy(info->processor,         UNAME_UNKNOWN_STR);
    strcpy(info->hardware_platform, UNAME_UNKNOWN_STR);
    strcpy(info->operating_system,  UNAME_UNKNOWN_STR);

#ifdef UNAME_PLATFORM_WINDOWS
    /* ---- Windows: gather from Win32 APIs and environment ---- */

    /* kernel_name: Windows platform identifier */
    strcpy(info->kernel_name, "Windows");

    /* nodename: computer name */
    {
        char computer[UNAME_FIELD_SIZE] = {0};
        DWORD size = (DWORD)sizeof(computer);
        if (GetComputerNameA(computer, &size)) {
            uname_strcpy_safe(info->nodename, computer, UNAME_FIELD_SIZE);
        }
    }

    /* kernel_release / kernel_version: from GetVersionEx */
    {
        OSVERSIONINFOEXA osv;
        memset(&osv, 0, sizeof(osv));
        osv.dwOSVersionInfoSize = sizeof(osv);
        /* GetVersionEx is deprecated but still functional on MinGW;
         * it returns the manifest-declared version. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        BOOL ok = GetVersionExA((LPOSVERSIONINFOA)&osv);
#pragma GCC diagnostic pop
        if (ok) {
            char rel[64];
            snprintf(rel, sizeof(rel), "%lu.%lu",
                     (unsigned long)osv.dwMajorVersion,
                     (unsigned long)osv.dwMinorVersion);
            uname_strcpy_safe(info->kernel_release, rel, UNAME_FIELD_SIZE);

            char ver[UNAME_FIELD_SIZE];
            snprintf(ver, sizeof(ver), "%lu.%lu.%lu Build %lu",
                     (unsigned long)osv.dwMajorVersion,
                     (unsigned long)osv.dwMinorVersion,
                     (unsigned long)osv.dwBuildNumber,
                     (unsigned long)osv.dwBuildNumber);
            uname_strcpy_safe(info->kernel_version, ver, UNAME_FIELD_SIZE);
        }
        else {
            uname_strcpy_safe(info->kernel_release, "10.0", UNAME_FIELD_SIZE);
            uname_strcpy_safe(info->kernel_version, "10.0", UNAME_FIELD_SIZE);
        }
    }

    /* machine / hardware_platform: PROCESSOR_ARCHITECTURE env var */
    {
        char arch[UNAME_FIELD_SIZE] = {0};
        _uname_win_get_env("PROCESSOR_ARCHITECTURE", arch, sizeof(arch));
        if (arch[0] != '\0') {
            uname_strcpy_safe(info->machine, arch, UNAME_FIELD_SIZE);
            uname_strcpy_safe(info->hardware_platform, arch, UNAME_FIELD_SIZE);
        }
    }

    /* processor: PROCESSOR_IDENTIFIER env var (falls back to machine) */
    {
        char id[UNAME_FIELD_SIZE] = {0};
        _uname_win_get_env("PROCESSOR_IDENTIFIER", id, sizeof(id));
        if (id[0] != '\0') {
            uname_strcpy_safe(info->processor, id, UNAME_FIELD_SIZE);
        }
        else if (strcmp(info->machine, UNAME_UNKNOWN_STR) != 0) {
            uname_strcpy_safe(info->processor, info->machine, UNAME_FIELD_SIZE);
        }
    }

    /* operating_system: Windows */
    strcpy(info->operating_system, "Windows");

#else /* UNAME_PLATFORM_POSIX */
    /* ---- POSIX: use uname(2) ---- */

    struct utsname u;
    memset(&u, 0, sizeof(u));
    if (uname(&u) != 0) {
        uname_err_printf("uname: cannot get system information\n");
        return -1;
    }

    uname_strcpy_safe(info->kernel_name,     u.sysname,  UNAME_FIELD_SIZE);
    uname_strcpy_safe(info->nodename,       u.nodename, UNAME_FIELD_SIZE);
    uname_strcpy_safe(info->kernel_release, u.release,  UNAME_FIELD_SIZE);
    uname_strcpy_safe(info->kernel_version, u.version,  UNAME_FIELD_SIZE);
    uname_strcpy_safe(info->machine,        u.machine,  UNAME_FIELD_SIZE);

    /* processor: default to the machine name (many systems report the
     * hardware class here); remains "unknown" if machine is empty. */
    if (info->machine[0] != '\0' &&
        strcmp(info->machine, UNAME_UNKNOWN_STR) != 0) {
        uname_strcpy_safe(info->processor, info->machine, UNAME_FIELD_SIZE);
    }

    /* hardware_platform: same as machine on most POSIX systems */
    if (info->machine[0] != '\0' &&
        strcmp(info->machine, UNAME_UNKNOWN_STR) != 0) {
        uname_strcpy_safe(info->hardware_platform, info->machine, UNAME_FIELD_SIZE);
    }

    /* operating_system: derived from compile-time platform */
    #ifdef UNAME_PLATFORM_LINUX
        strcpy(info->operating_system, "GNU/Linux");
    #elif defined(UNAME_PLATFORM_MACOS)
        strcpy(info->operating_system, "Darwin");
    #elif defined(UNAME_PLATFORM_FREEBSD)
        strcpy(info->operating_system, "FreeBSD");
    #elif defined(UNAME_PLATFORM_OPENBSD)
        strcpy(info->operating_system, "OpenBSD");
    #elif defined(UNAME_PLATFORM_NETBSD)
        strcpy(info->operating_system, "NetBSD");
    #else
        /* Generic Unix: fall back to sysname */
        uname_strcpy_safe(info->operating_system, u.sysname, UNAME_FIELD_SIZE);
    #endif
#endif

    return 0;
}

/**
 * @brief Output the selected fields in GNU order.
 *
 * GNU uname output order: kernel-name, nodename, kernel-release,
 * kernel-version, machine, processor, hardware-platform, operating-system.
 * Fields are space-separated; a trailing newline is always printed.
 *
 * In -a mode, "unknown" processor and hardware-platform values are
 * omitted (GNU-compatible behavior).
 *
 * @param opts  selected output flags
 * @param info  collected system information
 * @return 0 on success
 */
static int _uname_output(const uname_opts_t * opts, const uname_info_t * info)
{
    bool need_space = false;
    bool all_mode = opts->all;

    if (opts->kernel_name) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->kernel_name, uname_out_stream);
        need_space = true;
    }
    if (opts->nodename) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->nodename, uname_out_stream);
        need_space = true;
    }
    if (opts->kernel_release) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->kernel_release, uname_out_stream);
        need_space = true;
    }
    if (opts->kernel_version) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->kernel_version, uname_out_stream);
        need_space = true;
    }
    if (opts->machine) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->machine, uname_out_stream);
        need_space = true;
    }
    if (opts->processor) {
        /* In -a mode, skip "unknown" processor (GNU-compatible) */
        bool skip = all_mode && (strcmp(info->processor, UNAME_UNKNOWN_STR) == 0);
        if (!skip) {
            if (need_space) uname_fputs(" ", uname_out_stream);
            uname_fputs(info->processor, uname_out_stream);
            need_space = true;
        }
    }
    if (opts->hardware_platform) {
        /* In -a mode, skip "unknown" hardware-platform (GNU-compatible) */
        bool skip = all_mode && (strcmp(info->hardware_platform, UNAME_UNKNOWN_STR) == 0);
        if (!skip) {
            if (need_space) uname_fputs(" ", uname_out_stream);
            uname_fputs(info->hardware_platform, uname_out_stream);
            need_space = true;
        }
    }
    if (opts->operating_system) {
        if (need_space) uname_fputs(" ", uname_out_stream);
        uname_fputs(info->operating_system, uname_out_stream);
        need_space = true;
    }

    uname_fputs("\n", uname_out_stream);
    return 0;
}
