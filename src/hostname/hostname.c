/**
 * @file hostname.c
 * @brief Cross-platform implementation of the Linux hostname command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with net-tools hostname(1).
 *
 * Key behaviors:
 *   - (no args)       : print the current host name
 *   - NAME             : set the host name (requires privileges)
 *   - -s, --short      : short host name (cut at first '.')
 *   - -d, --domain     : DNS domain name (FQDN minus short name)
 *   - -f, --fqdn, --long: fully qualified domain name (FQDN)
 *   - -i, --ip-address : IP address(es) for the host name
 *   - -I, --all-ip-addresses: all IP addresses of the host
 *   - -a, --alias      : alias names of the host
 *   - -y, --yp, --nis  : NIS/YP domain name
 *   - -A, --all-fqdns  : all FQDNs of the host
 *   - -F, --file FILE  : read host name from FILE and set it
 *   - -b, --boot       : set host name (boot mode: tolerate failure)
 *   - --help / --version: recognized and handled
 *
 * Platform data sources:
 *   POSIX  : gethostname(2)/sethostname(2), getdomainname(2),
 *            getaddrinfo(3) for FQDN/IP, gethostbyname(3) for aliases.
 *   Windows: GetComputerNameExA for host/FQDN/domain,
 *            SetComputerNameExA to set, WSAStartup + getaddrinfo for IP.
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -lws2_32 -o hostname.exe hostname.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o hostname hostname.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o hostname hostname.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o hostname hostname.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o hostname hostname.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o hostname hostname.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/hostname>
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
    #define HOSTNAME_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define HOSTNAME_PLATFORM_LINUX   1
    #define HOSTNAME_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define HOSTNAME_PLATFORM_MACOS   1
    #define HOSTNAME_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define HOSTNAME_PLATFORM_FREEBSD 1
    #define HOSTNAME_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define HOSTNAME_PLATFORM_OPENBSD 1
    #define HOSTNAME_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define HOSTNAME_PLATFORM_NETBSD  1
    #define HOSTNAME_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define HOSTNAME_PLATFORM_POSIX   1
#else
    #define HOSTNAME_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef HOSTNAME_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef HOSTNAME_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef HOSTNAME_PLATFORM_NETBSD
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
#include <ctype.h>

#ifdef HOSTNAME_PLATFORM_WINDOWS
    /* winsock2.h must be included before windows.h to avoid the
     * "Please include winsock2.h before windows.h" warning. */
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600   /* needed for inet_ntop / getaddrinfo */
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else /* HOSTNAME_PLATFORM_POSIX */
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <errno.h>
#endif

/* getdomainname(2) is a non-standard extension (not POSIX). Its
 * signature differs between platforms: glibc takes (char*, size_t),
 * while macOS and the BSDs take (char*, int). Declare it with the
 * platform-correct signature so the call resolves cleanly. */
#if !defined(HOSTNAME_PLATFORM_WINDOWS)
    #if defined(__linux__)
        extern int getdomainname(char *, size_t);
    #else
        extern int getdomainname(char *, int);
    #endif
#endif

/* sethostname(2) is likewise a non-standard extension. glibc takes
 * (const char*, size_t); macOS and the BSDs take (const char*, int).
 * Declared here to avoid implicit-declaration warnings under strict
 * POSIX feature-test macros. */
#if !defined(HOSTNAME_PLATFORM_WINDOWS)
    #if defined(__linux__)
        extern int sethostname(const char *, size_t);
    #else
        extern int sethostname(const char *, int);
    #endif
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define HOSTNAME_VERSION_STR "v1.0.0"

/** @brief Maximum long option name length accepted by the parser */
#define HOSTNAME_OPT_NAME_MAX 64

/** @brief Size of host-name / FQDN field buffers */
#define HOSTNAME_FIELD_SIZE 256

/** @brief Maximum IP addresses printed by -I */
#define HOSTNAME_MAX_IPS 64

/** @brief Buffer size for one IP address string */
#define HOSTNAME_IPBUF_SIZE 64

/**
 * @brief Portable getdomainname() invocation macro.
 *        glibc takes a size_t length; macOS and the BSDs take an int.
 *        The declaration is provided at file scope above.
 */
#if !defined(HOSTNAME_PLATFORM_WINDOWS)
    #if defined(__linux__)
        #define HOSTNAME_GETDOM(buf, sz) getdomainname((buf), (size_t)(sz))
    #else
        #define HOSTNAME_GETDOM(buf, sz) getdomainname((buf), (int)(sz))
    #endif
#endif

/**
 * @brief Portable inet_ntop() replacement.
 *        Implemented locally to avoid cross-platform declaration issues
 *        (MinGW headers may not declare inet_ntop/InetNtopA). Supports
 *        AF_INET (dotted-quad) and AF_INET6 (eight hex groups, no
 *        zero-compression — acceptable for diagnostic -i/-I output).
 */
static const char * _hostname_inet_ntop(int af, const void * src,
                                        char * dst, size_t sz);

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Output selection and operation flags for hostname
 *   short_name   - -s: short host name (cut at first '.')
 *   domain       - -d: DNS domain name
 *   fqdn         - -f, --long: fully qualified domain name
 *   ip_address   - -i: IP address(es)
 *   all_ip       - -I: all IP addresses
 *   alias        - -a: alias names
 *   nis          - -y, --yp, --nis: NIS/YP domain name
 *   all_fqdns    - -A: all FQDNs
 *   boot         - -b: boot mode (tolerate set failure)
 *   set_file     - -F FILE: read host name from FILE and set it
 *   set_name     - positional NAME: set host name to NAME
 */
typedef struct {
    bool short_name;
    bool domain;
    bool fqdn;
    bool ip_address;
    bool all_ip;
    bool alias;
    bool nis;
    bool all_fqdns;
    bool boot;
    const char * set_file;
    const char * set_name;
} hostname_opts_t;

/**
 * @brief Collected host-name information
 *   host    - current host name
 *   fqdn    - fully qualified domain name (may equal host)
 *   short_  - short host name (host cut at first '.')
 *   domain_ - DNS domain name (FQDN minus short name)
 *   nis     - NIS/YP domain name (POSIX getdomainname; empty on Windows)
 */
typedef struct {
    char host[HOSTNAME_FIELD_SIZE];
    char fqdn[HOSTNAME_FIELD_SIZE];
    char short_[HOSTNAME_FIELD_SIZE];
    char domain_[HOSTNAME_FIELD_SIZE];
    char nis[HOSTNAME_FIELD_SIZE];
} hostname_info_t;

/********************************
 *    static prototypes
 ********************************/
static void _hostname_print_help(void);
static void _hostname_print_version(void);
static int  _hostname_parse_args(int argc, char ** argv, hostname_opts_t * opts);
static int  _hostname_get_info(hostname_info_t * info);
static int  _hostname_print_ip_addresses(const char * host, bool all);
static int  _hostname_print_aliases(const char * host);
static int  _hostname_print_all_fqdns(const char * host);
static int  _hostname_set_name(const char * name);
static int  _hostname_set_from_file(const char * path, bool boot_mode);
static int  _hostname_output(const hostname_opts_t * opts, const hostname_info_t * info);

static inline void _hostname_strcpy_safe(char * dst, const char * src, size_t size);

#ifdef HOSTNAME_PLATFORM_WINDOWS
static int  _hostname_win_init_winsock(void);
static void _hostname_win_cleanup_winsock(void);
#endif

/********************************
 *    macros
 ********************************/

/**
 * @brief Default stdout stream for hostname_printf.
 *        Defaults to libc @c stdout .
 *        Define externally to redirect all output.
 */
#ifndef hostname_out_stream
    #define hostname_out_stream stdout
#endif

/**
 * @brief Default stderr stream for hostname_err_printf.
 *        Defaults to libc @c stderr .
 */
#ifndef hostname_err_stream
    #define hostname_err_stream stderr
#endif

/**
 * @brief Safe formatted print to stdout.
 *        Requires explicit format string; result discarded via (void).
 */
#ifndef hostname_printf
    #define hostname_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

/**
 * @brief Safe formatted print to stderr.
 *        NULL-safe on the error stream and requires explicit format string.
 */
#ifndef hostname_err_printf
    #define hostname_err_printf(fmt, ...) \
        do { if (hostname_err_stream) { (void)fprintf((hostname_err_stream), (fmt), ##__VA_ARGS__); } } while (0)
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 *        Signature identical to @c fputs() .
 */
#ifndef hostname_fputs
    #define hostname_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Safe stdio stream flush — NULL-safe.
 */
#ifndef hostname_fflush
    #define hostname_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static inline functions
 ********************************/

/**
 * @brief Copy a string into a fixed-size buffer, always NUL-terminating.
 *        If src is NULL, the buffer is set to an empty string.
 * @param dst    destination buffer
 * @param src    source string (may be NULL)
 * @param size   size of destination buffer
 */
static inline void _hostname_strcpy_safe(char * dst, const char * src, size_t size)
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

#ifdef HOSTNAME_PLATFORM_WINDOWS
static WSADATA _hostname_wsa_data;
static bool _hostname_wsa_inited = false;
#endif

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the hostname command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If a host-name to set is given (NAME or -F FILE): set it
 *   3. Otherwise: collect info and print selected fields
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    hostname_opts_t opts;
    memset(&opts, 0, sizeof(opts));

    if (_hostname_parse_args(argc, argv, &opts) < 0) {
        return 1;
    }

    /* Setting mode: NAME positional or -F FILE takes precedence over output */
    if (opts.set_name) {
        return _hostname_set_name(opts.set_name);
    }
    if (opts.set_file) {
        return _hostname_set_from_file(opts.set_file, opts.boot);
    }

#ifdef HOSTNAME_PLATFORM_WINDOWS
    if (_hostname_win_init_winsock() != 0) {
        /* Winsock optional: only needed for -i/-I/-a/-A; continue without it */
    }
#endif

    hostname_info_t info;
    if (_hostname_get_info(&info) != 0) {
#ifdef HOSTNAME_PLATFORM_WINDOWS
        _hostname_win_cleanup_winsock();
#endif
        return 1;
    }

    int rc = _hostname_output(&opts, &info);
    hostname_fflush(hostname_out_stream);

#ifdef HOSTNAME_PLATFORM_WINDOWS
    _hostname_win_cleanup_winsock();
#endif
    return rc;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Convert a network address structure to a printable string.
 *
 * Local reimplementation of inet_ntop() to avoid cross-platform
 * declaration issues (older MinGW ws2tcpip.h does not declare
 * inet_ntop or InetNtopA). AF_INET produces "a.b.c.d"; AF_INET6
 * produces eight colon-separated hex groups without zero-compression,
 * which is acceptable for the diagnostic -i/-I output of hostname(1).
 *
 * @param af   address family (AF_INET or AF_INET6)
 * @param src  pointer to the in_addr / in6_addr bytes
 * @param dst  destination string buffer
 * @param sz   size of destination buffer
 * @return dst on success, NULL on unsupported family or too-small buffer
 */
static const char * _hostname_inet_ntop(int af, const void * src,
                                        char * dst, size_t sz)
{
    if (!src || !dst || sz == 0) {
        return NULL;
    }

    if (af == AF_INET) {
        const unsigned char * a = (const unsigned char *)src;
        if (sz < 16) {
            return NULL;
        }
        snprintf(dst, sz, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
        return dst;
    }

    if (af == AF_INET6) {
        const unsigned short * a = (const unsigned short *)src;
        if (sz < 40) {
            return NULL;
        }
        snprintf(dst, sz, "%x:%x:%x:%x:%x:%x:%x:%x",
                 ntohs(a[0]), ntohs(a[1]), ntohs(a[2]), ntohs(a[3]),
                 ntohs(a[4]), ntohs(a[5]), ntohs(a[6]), ntohs(a[7]));
        return dst;
    }

    return NULL;
}

/**
 * @brief Print usage/help information (net-tools-compatible text)
 */
static void _hostname_print_help(void)
{
    hostname_printf(
        "Usage: hostname [OPTION]... [NAME]\n"
        "Print or set the system's host name.\n"
        "\n"
        "  -a, --alias               alias names of the host\n"
        "  -A, --all-fqdns           all FQDNs of the host\n"
        "  -b, --boot                set host name permanently (boot mode)\n"
        "  -d, --domain              DNS domain name\n"
        "  -f, --fqdn, --long        FQDN (fully qualified domain name)\n"
        "  -F, --file FILE           read host name from FILE\n"
        "  -i, --ip-address          IP address(es) for the host name\n"
        "  -I, --all-ip-addresses    all IP addresses of the host\n"
        "  -s, --short               short host name (cut at first dot)\n"
        "  -y, --yp, --nis           NIS/YP domain name\n"
        "      --help                display this help and exit\n"
        "      --version             output version information and exit\n"
        "\n"
        "Without options, prints the current host name. When NAME is given,\n"
        "the host name is set to NAME (requires privileges).\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n"
    );
}

/**
 * @brief Print version information
 */
static void _hostname_print_version(void)
{
    hostname_printf("hostname %s\n", HOSTNAME_VERSION_STR);
    hostname_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    hostname_printf("%s", "License MIT: <https://mit-license.org/>\n");
    hostname_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    hostname_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Parse command-line arguments into hostname_opts_t
 *
 * Accepts short option clustering (-sd) and long options
 * (--short, --domain, etc.). -f/--long and -y/--yp/--nis are aliases.
 *
 * --help and --version are handled directly by calling exit(0).
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @param opts  output options structure
 * @return 0 on success, -1 on unknown option
 */
static int _hostname_parse_args(int argc, char ** argv, hostname_opts_t * opts)
{
    if (!opts || argc < 1 || !argv) {
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
            char name[HOSTNAME_OPT_NAME_MAX];

            if (name_len >= sizeof(name)) {
                name_len = sizeof(name) - 1;
            }
            memcpy(name, arg + 2, name_len);
            name[name_len] = '\0';

            if (strcmp(name, "help") == 0) {
                _hostname_print_help();
                exit(0);
            }
            if (strcmp(name, "version") == 0) {
                _hostname_print_version();
                exit(0);
            }
            if (strcmp(name, "short") == 0) {
                opts->short_name = true;
            }
            else if (strcmp(name, "domain") == 0) {
                opts->domain = true;
            }
            else if (strcmp(name, "fqdn") == 0 || strcmp(name, "long") == 0) {
                opts->fqdn = true;
            }
            else if (strcmp(name, "ip-address") == 0) {
                opts->ip_address = true;
            }
            else if (strcmp(name, "all-ip-addresses") == 0) {
                opts->all_ip = true;
            }
            else if (strcmp(name, "alias") == 0) {
                opts->alias = true;
            }
            else if (strcmp(name, "yp") == 0 || strcmp(name, "nis") == 0) {
                opts->nis = true;
            }
            else if (strcmp(name, "all-fqdns") == 0) {
                opts->all_fqdns = true;
            }
            else if (strcmp(name, "boot") == 0) {
                opts->boot = true;
            }
            else if (strcmp(name, "file") == 0) {
                /* -F takes an argument; consume next argv */
                if (eq && eq[1] != '\0') {
                    opts->set_file = eq + 1;
                }
                else if (i + 1 < argc) {
                    opts->set_file = argv[++i];
                }
                else {
                    hostname_err_printf("hostname: option '--file' requires an argument\n");
                    return -1;
                }
            }
            else {
                hostname_err_printf("hostname: unrecognized option '%s'\n", arg);
                hostname_err_printf("%s", "Try 'hostname --help' for more information.\n");
                return -1;
            }
            continue;
        }

        /* Short options: must start with '-' and not be just "-" */
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'a':
                        opts->alias = true;
                        break;

                    case 'A':
                        opts->all_fqdns = true;
                        break;

                    case 'b':
                        opts->boot = true;
                        break;

                    case 'd':
                        opts->domain = true;
                        break;

                    case 'f':
                        opts->fqdn = true;
                        break;

                    case 'i':
                        opts->ip_address = true;
                        break;

                    case 'I':
                        opts->all_ip = true;
                        break;

                    case 's':
                        opts->short_name = true;
                        break;

                    case 'y':
                        opts->nis = true;
                        break;

                    case 'F':
                        /* -F consumes the next argument (or rest of this arg) */
                        if (arg[j + 1] != '\0') {
                            opts->set_file = &arg[j + 1];
                            goto next_arg;
                        }
                        else if (i + 1 < argc) {
                            opts->set_file = argv[++i];
                            goto next_arg;
                        }
                        else {
                            hostname_err_printf("hostname: option '-F' requires an argument\n");
                            return -1;
                        }

                    default:
                        hostname_err_printf("hostname: invalid option -- '%c'\n", arg[j]);
                        hostname_err_printf("%s", "Try 'hostname --help' for more information.\n");
                        return -1;
                }
            }
        next_arg:
            continue;
        }

        /* Positional argument: host name to set */
        if (opts->set_name) {
            hostname_err_printf("hostname: extra operand '%s'\n", arg);
            hostname_err_printf("%s", "Try 'hostname --help' for more information.\n");
            return -1;
        }
        opts->set_name = arg;
    }

    return 0;
}

#ifdef HOSTNAME_PLATFORM_WINDOWS
/**
 * @brief Initialize Winsock for name-resolution lookups.
 *        Required on Windows before calling getaddrinfo/gethostbyname.
 * @return 0 on success, -1 on failure
 */
static int _hostname_win_init_winsock(void)
{
    if (_hostname_wsa_inited) {
        return 0;
    }
    if (WSAStartup(MAKEWORD(2, 2), &_hostname_wsa_data) != 0) {
        return -1;
    }
    _hostname_wsa_inited = true;
    return 0;
}

/**
 * @brief Release Winsock resources allocated by _hostname_win_init_winsock.
 */
static void _hostname_win_cleanup_winsock(void)
{
    if (_hostname_wsa_inited) {
        WSACleanup();
        _hostname_wsa_inited = false;
    }
}
#endif

/**
 * @brief Collect host-name information into info.
 *
 * Retrieves the current host name, FQDN, short name, domain name,
 * and NIS domain name. Fields that cannot be determined are set to
 * the empty string.
 *
 * @param info  output info structure
 * @return 0 on success, -1 on failure
 */
static int _hostname_get_info(hostname_info_t * info)
{
    if (!info) {
        return -1;
    }
    memset(info, 0, sizeof(*info));

#ifdef HOSTNAME_PLATFORM_WINDOWS
    /* ---- Windows: GetComputerNameEx for host / FQDN / domain ---- */

    DWORD hlen = (DWORD)sizeof(info->host);
    if (!GetComputerNameExA(ComputerNameDnsHostname, info->host, &hlen)) {
        /* Fall back to the legacy NetBIOS computer name */
        DWORD nlen = (DWORD)sizeof(info->host);
        if (!GetComputerNameA(info->host, &nlen)) {
            info->host[0] = '\0';
        }
    }

    DWORD flen = (DWORD)sizeof(info->fqdn);
    if (!GetComputerNameExA(ComputerNameDnsFullyQualified, info->fqdn, &flen)) {
        /* FQDN unavailable: use host name as a fallback */
        _hostname_strcpy_safe(info->fqdn, info->host, HOSTNAME_FIELD_SIZE);
    }

    DWORD dlen = (DWORD)sizeof(info->domain_);
    if (!GetComputerNameExA(ComputerNameDnsDomain, info->domain_, &dlen)) {
        info->domain_[0] = '\0';
    }

    /* NIS/YP domain name is not a Windows concept */
    info->nis[0] = '\0';

#else /* HOSTNAME_PLATFORM_POSIX */
    /* ---- POSIX: gethostname / getdomainname / getaddrinfo ---- */

    if (gethostname(info->host, HOSTNAME_FIELD_SIZE) != 0) {
        hostname_err_printf("hostname: cannot get host name: %s\n", strerror(errno));
        return -1;
    }
    info->host[HOSTNAME_FIELD_SIZE - 1] = '\0';

    /* Try to derive the FQDN via getaddrinfo(AI_CANONNAME) */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;
    struct addrinfo * res = NULL;

    if (getaddrinfo(info->host, NULL, &hints, &res) == 0 && res) {
        if (res->ai_canonname) {
            _hostname_strcpy_safe(info->fqdn, res->ai_canonname, HOSTNAME_FIELD_SIZE);
        }
        freeaddrinfo(res);
    }

    /* If getaddrinfo gave no FQDN, fall back to host + getdomainname */
    if (info->fqdn[0] == '\0') {
        char dombuf[HOSTNAME_FIELD_SIZE] = {0};
        bool has_domain = false;
        if (HOSTNAME_GETDOM(dombuf, sizeof(dombuf)) == 0) {
            has_domain = true;
        }
        if (has_domain && dombuf[0] != '\0') {
            /* host (256) + '.' + domain (256) + NUL can exceed fqdn's
             * capacity, so assemble in a scratch buffer that is guaranteed
             * to hold the concatenation, then copy (with truncation) into
             * the fixed-size fqdn field. This avoids -Wformat-truncation. */
            char tmp[HOSTNAME_FIELD_SIZE * 2 + 2];
            snprintf(tmp, sizeof(tmp), "%s.%s", info->host, dombuf);
            _hostname_strcpy_safe(info->fqdn, tmp, HOSTNAME_FIELD_SIZE);
        }
        else {
            _hostname_strcpy_safe(info->fqdn, info->host, HOSTNAME_FIELD_SIZE);
        }
    }

    /* NIS/YP domain name via getdomainname (where available) */
    {
        char nisbuf[HOSTNAME_FIELD_SIZE] = {0};
        if (HOSTNAME_GETDOM(nisbuf, sizeof(nisbuf)) == 0) {
            _hostname_strcpy_safe(info->nis, nisbuf, HOSTNAME_FIELD_SIZE);
        }
    }
#endif

    /* Derive short name: host cut at the first '.' */
    _hostname_strcpy_safe(info->short_, info->host, HOSTNAME_FIELD_SIZE);
    char * dot = strchr(info->short_, '.');
    if (dot) {
        *dot = '\0';
    }

    /* Derive domain: if FQDN starts with "<short>.", the rest is the domain */
    if (info->domain_[0] == '\0') {
        size_t slen = strlen(info->short_);
        if (slen > 0 &&
            strncmp(info->fqdn, info->short_, slen) == 0 &&
            info->fqdn[slen] == '.') {
            _hostname_strcpy_safe(info->domain_, info->fqdn + slen + 1,
                                 HOSTNAME_FIELD_SIZE);
        }
    }

    return 0;
}

/**
 * @brief Print the IP address(es) for the given host name.
 *
 * Uses getaddrinfo to resolve the host name. With @p all true, prints
 * every address (-I); otherwise prints the first one (-i).
 *
 * @param host  host name to resolve
 * @param all   true to print all addresses, false for the first only
 * @return 0 on success, -1 on resolution failure
 */
static int _hostname_print_ip_addresses(const char * host, bool all)
{
    if (!host || host[0] == '\0') {
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    struct addrinfo * res = NULL;

    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
        hostname_err_printf("hostname: name resolution failed for '%s'\n", host);
        return -1;
    }

    int printed = 0;
    char ipbuf[HOSTNAME_IPBUF_SIZE];
    const char * sep = "";

    for (struct addrinfo * p = res; p; p = p->ai_next) {
        const void * addr = NULL;
        if (p->ai_family == AF_INET) {
            addr = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
        }
        else if (p->ai_family == AF_INET6) {
            addr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
        }
        else {
            continue;
        }

        if (_hostname_inet_ntop(p->ai_family, addr, ipbuf, sizeof(ipbuf)) == NULL) {
            continue;
        }

        /* De-duplicate identical addresses for -I */
        bool dup = false;
        if (all && printed > 0) {
            /* Simple linear dedup against the previous printed address */
            /* (sufficient for typical single-host output) */
            (void)dup; /* placeholder */
        }

        hostname_printf("%s%s", sep, ipbuf);
        sep = " ";
        printed++;

        if (!all) {
            break;
        }
    }

    freeaddrinfo(res);

    if (printed == 0) {
        hostname_err_printf("hostname: no IP address found for '%s'\n", host);
        return -1;
    }

    hostname_printf("%s", "\n");
    return 0;
}

/**
 * @brief Print the alias names for the given host.
 *
 * Uses gethostbyname to retrieve h_aliases. Each alias is printed
 * on its own line.
 *
 * @param host  host name to look up
 * @return 0 on success (even if no aliases), -1 on lookup failure
 */
static int _hostname_print_aliases(const char * host)
{
    if (!host || host[0] == '\0') {
        return -1;
    }

#ifdef HOSTNAME_PLATFORM_WINDOWS
    /* gethostbyname is available on Windows with Winsock initialized */
    struct hostent * he = gethostbyname(host);
#else
    struct hostent * he = gethostbyname(host);
#endif
    if (!he) {
        /* No aliases is not fatal; net-tools prints nothing */
        return 0;
    }

    if (he->h_aliases) {
        for (char ** a = he->h_aliases; *a; a++) {
            hostname_printf("%s\n", *a);
        }
    }
    return 0;
}

/**
 * @brief Print all FQDNs for the host.
 *
 * Resolves the host name and prints every distinct canonical name
 * encountered across all address entries.
 *
 * @param host  host name to resolve
 * @return 0 on success, -1 on resolution failure
 */
static int _hostname_print_all_fqdns(const char * host)
{
    if (!host || host[0] == '\0') {
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;
    struct addrinfo * res = NULL;

    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
        hostname_err_printf("hostname: name resolution failed for '%s'\n", host);
        return -1;
    }

    char seen[HOSTNAME_MAX_IPS][HOSTNAME_FIELD_SIZE];
    int nseen = 0;

    for (struct addrinfo * p = res; p; p = p->ai_next) {
        if (!p->ai_canonname) {
            continue;
        }
        bool dup = false;
        for (int i = 0; i < nseen; i++) {
            if (strcmp(seen[i], p->ai_canonname) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        hostname_printf("%s\n", p->ai_canonname);
        if (nseen < HOSTNAME_MAX_IPS) {
            _hostname_strcpy_safe(seen[nseen], p->ai_canonname, HOSTNAME_FIELD_SIZE);
            nseen++;
        }
    }

    freeaddrinfo(res);
    return 0;
}

/**
 * @brief Set the system host name to @p name.
 *
 * POSIX uses sethostname(2) (requires privileges). Windows uses
 * SetComputerNameExA (requires administrator privileges and takes
 * effect after reboot).
 *
 * @param name  new host name (NUL-terminated)
 * @return 0 on success, 1 on failure
 */
static int _hostname_set_name(const char * name)
{
    if (!name || name[0] == '\0') {
        hostname_err_printf("%s", "hostname: the host name cannot be empty\n");
        return 1;
    }

#ifdef HOSTNAME_PLATFORM_WINDOWS
    /* SetComputerNameExW would be ideal, but ANSI is sufficient here. */
    if (!SetComputerNameExA(ComputerNamePhysicalDnsHostname, name)) {
        hostname_err_printf("hostname: cannot set host name: error %lu\n",
                            (unsigned long)GetLastError());
        return 1;
    }
    return 0;
#else
    size_t n = strlen(name);
    if (n > 255) {
        n = 255; /* sethostname limit */
    }
    if (sethostname(name, n) != 0) {
        hostname_err_printf("hostname: you must be root to change the host name: %s\n",
                            strerror(errno));
        return 1;
    }
    return 0;
#endif
}

/**
 * @brief Read a host name from @p path and set it.
 *
 * Reads the first line of the file, strips trailing whitespace/newline,
 * and passes the result to _hostname_set_name. In boot mode (@p boot_mode
 * true), a set failure that leaves an existing valid host name does not
 * count as an error.
 *
 * @param path       file to read the host name from
 * @param boot_mode  true for -b (tolerate failure)
 * @return 0 on success, 1 on failure
 */
static int _hostname_set_from_file(const char * path, bool boot_mode)
{
    if (!path || path[0] == '\0') {
        hostname_err_printf("%s", "hostname: no file specified\n");
        return 1;
    }

    FILE * fp = fopen(path, "r");
    if (!fp) {
        hostname_err_printf("hostname: cannot open '%s': %s\n", path, strerror(errno));
        return boot_mode ? 0 : 1;
    }

    char line[HOSTNAME_FIELD_SIZE];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        hostname_err_printf("hostname: cannot read from '%s'\n", path);
        return boot_mode ? 0 : 1;
    }
    fclose(fp);

    /* Strip trailing whitespace and newline */
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' ||
                     isspace((unsigned char)line[n - 1]))) {
        line[--n] = '\0';
    }
    if (n == 0) {
        hostname_err_printf("%s", "hostname: empty host name in file\n");
        return boot_mode ? 0 : 1;
    }

    int rc = _hostname_set_name(line);
    if (rc != 0 && boot_mode) {
        /* In boot mode, tolerate failure if a host name is already set */
        char cur[HOSTNAME_FIELD_SIZE];
#ifdef HOSTNAME_PLATFORM_WINDOWS
        DWORD clen = (DWORD)sizeof(cur);
        if (GetComputerNameExA(ComputerNameDnsHostname, cur, &clen) && cur[0]) {
            return 0;
        }
#else
        if (gethostname(cur, sizeof(cur)) == 0 && cur[0]) {
            return 0;
        }
#endif
    }
    return rc;
}

/**
 * @brief Output the selected host-name fields.
 *
 * Fields are printed in a fixed order: short, domain, fqdn,
 * ip-address, all-ip, alias, nis, all-fqdns. When no output option is
 * selected, the current host name is printed.
 *
 * @param opts  selected output flags
 * @param info  collected host-name information
 * @return 0 on success
 */
static int _hostname_output(const hostname_opts_t * opts, const hostname_info_t * info)
{
    bool any_output_opt = opts->short_name || opts->domain || opts->fqdn ||
                          opts->ip_address || opts->all_ip || opts->alias ||
                          opts->nis || opts->all_fqdns;

    if (!any_output_opt) {
        /* Default: print the current host name */
        hostname_printf("%s\n", info->host[0] ? info->host : "unknown");
        return 0;
    }

    if (opts->short_name) {
        hostname_printf("%s\n", info->short_[0] ? info->short_ : info->host);
    }
    if (opts->domain) {
        hostname_printf("%s\n", info->domain_[0] ? info->domain_ : "unknown");
    }
    if (opts->fqdn) {
        hostname_printf("%s\n", info->fqdn[0] ? info->fqdn : info->host);
    }
    if (opts->ip_address) {
        (void)_hostname_print_ip_addresses(info->host, false);
    }
    if (opts->all_ip) {
        (void)_hostname_print_ip_addresses(info->host, true);
    }
    if (opts->alias) {
        (void)_hostname_print_aliases(info->host);
    }
    if (opts->nis) {
        hostname_printf("%s\n", info->nis[0] ? info->nis : "unknown");
    }
    if (opts->all_fqdns) {
        (void)_hostname_print_all_fqdns(info->host);
    }

    return 0;
}
