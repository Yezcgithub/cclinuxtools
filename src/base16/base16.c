/**
 * @file base16.c
 * @brief Cross-platform implementation of the base16 command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils basenc --base16.
 *
 * Key behaviors:
 *   - Encodes/decodes RFC 4648 Base16 (hexadecimal: 0-9A-F)
 *   - Encoding always outputs uppercase hex digits
 *   - Decoding accepts both upper and lower case hex digits
 *   - No padding required (unlike base32/base64)
 *   - With no FILE, or when FILE is -, reads standard input
 *   - -d, --decode         decode data
 *   - -i, --ignore-garbage when decoding, ignore non-alphabet characters
 *   - -w, --wrap=COLS      wrap encoded lines after COLS chars (default 76, 0=none)
 *   -     --help / --version
 *   - stdin support (no file args, or - as filename)
 *   - Binary-safe I/O
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o base16.exe base16.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o base16 base16.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o base16 base16.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o base16 base16.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o base16 base16.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o base16 base16.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/base16>
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
    #define BASE16_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASE16_PLATFORM_LINUX   1
    #define BASE16_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASE16_PLATFORM_MACOS   1
    #define BASE16_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASE16_PLATFORM_FREEBSD 1
    #define BASE16_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASE16_PLATFORM_OPENBSD 1
    #define BASE16_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASE16_PLATFORM_NETBSD  1
    #define BASE16_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASE16_PLATFORM_POSIX   1
#else
    #define BASE16_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef BASE16_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef BASE16_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef BASE16_PLATFORM_NETBSD
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
#include <errno.h>

#ifdef BASE16_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <sys/stat.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define BASE16_VERSION_STR "v1.0.0"

/** @brief I/O buffer size */
#define BASE16_IO_BUF_SIZE 65536

/** @brief Default line wrap width */
#define BASE16_DEFAULT_WRAP 76

/********************************
 *    typedefs
 ********************************/

/** @brief Command-line options */
typedef struct {
    bool decode;
    bool ignore_garbage;
    int wrap;
} base16_opts;

/********************************
 *    static prototypes
 ********************************/
static void _base16_print_help(void);
static void _base16_print_version(void);
static int  _base16_parse_opts(int argc, char ** argv,
                               base16_opts * opts, int * file_start);
static int  _base16_do_encode(FILE * in, const base16_opts * opts);
static int  _base16_do_decode(FILE * in, const base16_opts * opts);
static int  _base16_decode_char(int c);
static const char * _base16_basename(const char * path);

/********************************
 *    macros
 ********************************/

#ifndef base16_printf
    #define base16_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef base16_err_printf
    #define base16_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef base16_fflush
    #define base16_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Program name for error messages */
static const char * base16_prog_name = "base16";

/** @brief Base16 alphabet (RFC 4648 Section 8) — uppercase */
static const char base16_alphabet[] = "0123456789ABCDEF";

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the base16 command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Open input file(s) or stdin
 *   3. Encode or decode as requested
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 1;
    }

    base16_prog_name = _base16_basename(argv[0]);

#ifdef BASE16_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif

    base16_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.wrap = BASE16_DEFAULT_WRAP;

    int file_start = 0;
    if (!_base16_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    int nfiles = argc - file_start;
    int exit_code = 0;

    if (nfiles == 0) {
        /* stdin */
        if (opts.decode) {
            exit_code = _base16_do_decode(stdin, &opts);
        }
        else {
            exit_code = _base16_do_encode(stdin, &opts);
        }
    }
    else {
        for (int i = file_start; i < argc; i++) {
            FILE * fp;
            if (strcmp(argv[i], "-") == 0) {
                fp = stdin;
            }
            else {
                fp = fopen(argv[i], "rb");
                if (!fp) {
                    base16_err_printf("%s: %s: %s\n",
                                      base16_prog_name,
                                      argv[i], strerror(errno));
                    exit_code = 1;
                    continue;
                }
            }

            int rc;
            if (opts.decode) {
                rc = _base16_do_decode(fp, &opts);
            }
            else {
                rc = _base16_do_encode(fp, &opts);
            }

            if (rc != 0) {
                exit_code = 1;
            }

            if (fp != stdin) {
                fclose(fp);
            }
        }
    }

    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Print usage/help information
 */
static void _base16_print_help(void)
{
    base16_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Base16 encode or decode FILE, or standard input, to standard output.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "\n"
        "  -d, --decode             decode data\n"
        "  -i, --ignore-garbage     when decoding, ignore non-alphabet characters\n"
        "  -w, --wrap=COLS          wrap encoded lines after COLS characters\n"
        "                           (default %d, 0 to disable wrap)\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "The data are encoded as described for the base16 alphabet in RFC 4648.\n"
        "Encoding outputs uppercase hexadecimal digits (0-9A-F).\n"
        "When decoding, the input may contain newlines in addition to the\n"
        "bytes of the formal alphabet. Use --ignore-garbage to ignore\n"
        "other non-alphabet bytes.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        base16_prog_name, BASE16_DEFAULT_WRAP
    );
}

/**
 * @brief Print version information
 */
static void _base16_print_version(void)
{
    base16_printf("base16 %s\n", BASE16_VERSION_STR);
    base16_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    base16_printf("%s", "License MIT: <https://mit-license.org/>\n");
    base16_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    base16_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract basename from a path
 * @param path  full path (e.g. "/usr/bin/base16" or "base16.exe")
 * @return pointer to basename within path
 */
static const char * _base16_basename(const char * path)
{
    if (!path) {
        return "base16";
    }

    const char * base = path;
    for (const char * p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

/**
 * @brief Parse command-line options
 *
 * Supports style long options (--wrap=0) and short options (-w 0).
 *
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        output options struct
 * @param file_start  index in argv where files begin
 * @return true on success, false on error
 */
static int _base16_parse_opts(int argc, char ** argv,
                              base16_opts * opts, int * file_start)
{
    if (!opts || !file_start) {
        return false;
    }

    *file_start = 1;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* Long options */
        if (strncmp(arg, "--", 2) == 0) {
            if (strcmp(arg, "--help") == 0) {
                _base16_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _base16_print_version();
                exit(0);
            }
            if (strcmp(arg, "--decode") == 0) {
                opts->decode = true;
                continue;
            }
            if (strcmp(arg, "--ignore-garbage") == 0) {
                opts->ignore_garbage = true;
                continue;
            }
            /* --wrap=COLS */
            if (strncmp(arg, "--wrap=", 7) == 0) {
                char * end = NULL;
                errno = 0;
                long cols = strtol(arg + 7, &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    base16_err_printf("%s: invalid wrap size '%s'\n",
                                      base16_prog_name, arg + 7);
                    return false;
                }
                opts->wrap = (int)cols;
                continue;
            }
            /* --wrap COLS (separate arg) */
            if (strcmp(arg, "--wrap") == 0) {
                if (i + 1 >= argc) {
                    base16_err_printf("%s: option '--wrap' requires an argument\n",
                                      base16_prog_name);
                    return false;
                }
                char * end = NULL;
                errno = 0;
                long cols = strtol(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    base16_err_printf("%s: invalid wrap size '%s'\n",
                                      base16_prog_name, argv[i + 1]);
                    return false;
                }
                opts->wrap = (int)cols;
                i++;
                continue;
            }

            base16_err_printf("%s: unrecognized option '%s'\n",
                              base16_prog_name, arg);
            base16_err_printf("%s", "Try 'base16 --help' for more information.\n");
            return false;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'd':
                        opts->decode = true;
                        break;
                    case 'i':
                        opts->ignore_garbage = true;
                        break;
                    case 'w':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = &arg[j + 1];
                        }
                        else if (i + 1 < argc) {
                            val = argv[i + 1];
                            i++;
                        }
                        else {
                            base16_err_printf("%s: option '-w' requires an argument\n",
                                              base16_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        errno = 0;
                        long cols = strtol(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' || cols < 0) {
                            base16_err_printf("%s: invalid wrap size '%s'\n",
                                              base16_prog_name, val);
                            return false;
                        }
                        opts->wrap = (int)cols;
                        goto next_arg;
                    }
                    default:
                        base16_err_printf("%s: invalid option -- '%c'\n",
                                          base16_prog_name, arg[j]);
                        base16_err_printf("%s", "Try 'base16 --help' for more information.\n");
                        return false;
                }
            }
            next_arg:
            continue;
        }

        /* This is a file argument; all remaining args are files */
        *file_start = i;
        break;
    }

    return true;
}

/**
 * @brief Decode a hex character to its 4-bit value
 *
 * Accepts both upper and lower case hex digits.
 *
 * @param c  input character
 * @return 0-15 if valid, -1 if invalid
 */
static int _base16_decode_char(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/**
 * @brief Encode a stream to base16 (hexadecimal)
 *
 * Reads data from the input stream and writes base16-encoded output
 * with line wrapping. Each byte produces two uppercase hex chars.
 *
 * @param in    input stream
 * @param opts  options (wrap width)
 * @return 0 on success, -1 on error
 */
static int _base16_do_encode(FILE * in, const base16_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE16_IO_BUF_SIZE);
    if (!buf) {
        base16_err_printf("%s: out of memory\n", base16_prog_name);
        return -1;
    }

    int col = 0;
    size_t n;

    while ((n = fread(buf, 1, BASE16_IO_BUF_SIZE, in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            /* High nibble */
            if (opts->wrap > 0 && col >= opts->wrap) {
                putchar('\n');
                col = 0;
            }
            putchar(base16_alphabet[buf[i] >> 4]);
            col++;

            /* Low nibble */
            if (opts->wrap > 0 && col >= opts->wrap) {
                putchar('\n');
                col = 0;
            }
            putchar(base16_alphabet[buf[i] & 0x0F]);
            col++;
        }
    }

    free(buf);

    if (ferror(in)) {
        base16_err_printf("%s: read error\n", base16_prog_name);
        return -1;
    }

    /* Final newline */
    putchar('\n');

    base16_fflush(stdout);
    return 0;
}

/**
 * @brief Decode a base16 (hexadecimal) stream to raw bytes
 *
 * Reads hex data, ignores garbage if requested, and writes
 * decoded bytes to stdout. Handles odd-length input by buffering
 * the last nibble (matching behavior where the finalize step
 * checks for a pending nibble).
 *
 * @param in    input stream
 * @param opts  options (ignore_garbage)
 * @return 0 on success, -1 on error
 */
static int _base16_do_decode(FILE * in, const base16_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE16_IO_BUF_SIZE);
    if (!buf) {
        base16_err_printf("%s: out of memory\n", base16_prog_name);
        return -1;
    }

    int nibble = -1;  /* -1 = no pending nibble, 0-15 = pending high nibble */
    bool bom_checked = false;
    int exit_code = 0;
    size_t n;

    while ((n = fread(buf, 1, BASE16_IO_BUF_SIZE, in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            int c = buf[i];

            /* Skip UTF-8 BOM on first bytes */
            if (!bom_checked) {
                if (i + 2 < n &&
                    (uint8_t)buf[i] == 0xEF &&
                    (uint8_t)buf[i + 1] == 0xBB &&
                    (uint8_t)buf[i + 2] == 0xBF) {
                    bom_checked = true;
                    i += 2;
                    continue;
                }
                bom_checked = true;
            }

            /* Skip whitespace always (newlines, spaces, tabs, etc.) */
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                continue;
            }

            int val = _base16_decode_char(c);

            if (val < 0) {
                /* Invalid character */
                if (opts->ignore_garbage) {
                    continue;
                }
                base16_err_printf("%s: invalid input\n",
                                  base16_prog_name);
                exit_code = -1;
                goto done;
            }

            /* Valid hex digit */
            if (nibble < 0) {
                nibble = val;
            }
            else {
                /* Combine two nibbles into a byte */
                uint8_t byte = (uint8_t)((nibble << 4) | val);
                putchar(byte);
                nibble = -1;
            }
        }
    }

done:
    free(buf);

    if (ferror(in)) {
        base16_err_printf("%s: read error\n", base16_prog_name);
        return -1;
    }

    /* Check for leftover nibble (odd-length input) */
    if (exit_code == 0 && nibble >= 0) {
        base16_err_printf("%s: invalid input (odd length)\n",
                          base16_prog_name);
        exit_code = -1;
    }

    base16_fflush(stdout);
    return exit_code;
}
