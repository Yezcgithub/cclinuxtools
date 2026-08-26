/**
 * @file base64.c
 * @brief Cross-platform implementation of the base64 command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils base64(1).
 *
 * Key behaviors:
 *   - Encodes/decodes RFC 4648 Base64 (A-Za-z0-9+/)
 *   - With no FILE, or when FILE is -, reads standard input
 *   - -d, --decode         decode data
 *   - -i, --ignore-garbage when decoding, ignore non-alphabet characters
 *   - -w, --wrap=COLS      wrap encoded lines after COLS chars (default 76, 0=none)
 *   -     --help / --version
 *   - stdin support (no file args, or - as filename)
 *   - Binary-safe I/O
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o base64.exe base64.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o base64 base64.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o base64 base64.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o base64 base64.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o base64 base64.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o base64 base64.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/base64>
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
    #define BASE64_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASE64_PLATFORM_LINUX   1
    #define BASE64_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASE64_PLATFORM_MACOS   1
    #define BASE64_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASE64_PLATFORM_FREEBSD 1
    #define BASE64_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASE64_PLATFORM_OPENBSD 1
    #define BASE64_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASE64_PLATFORM_NETBSD  1
    #define BASE64_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASE64_PLATFORM_POSIX   1
#else
    #define BASE64_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef BASE64_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef BASE64_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef BASE64_PLATFORM_NETBSD
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

#ifdef BASE64_PLATFORM_WINDOWS
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
#define BASE64_VERSION_STR "v1.0.0"

/** @brief I/O buffer size */
#define BASE64_IO_BUF_SIZE 65536

/** @brief Default line wrap width */
#define BASE64_DEFAULT_WRAP 76

/** @brief Size of a base64 quantum (3 bytes -> 4 base64 chars) */
#define BASE64_QUANTUM_BYTES 3

/** @brief Number of base64 characters per quantum */
#define BASE64_QUANTUM_CHARS 4

/********************************
 *    typedefs
 ********************************/

/** @brief Command-line options */
typedef struct {
    bool decode;
    bool ignore_garbage;
    int wrap;
} base64_opts;

/********************************
 *    static prototypes
 ********************************/
static void _base64_print_help(void);
static void _base64_print_version(void);
static int  _base64_parse_opts(int argc, char ** argv,
                               base64_opts * opts, int * file_start);
static int  _base64_do_encode(FILE * in, const base64_opts * opts);
static int  _base64_do_decode(FILE * in, const base64_opts * opts);
static void _base64_encode_block(const uint8_t in[3], uint8_t out[4],
                                 size_t in_len);
static int  _base64_decode_block(const uint8_t in[4], uint8_t out[3]);
static int  _base64_decode_char(int c);
static const char * _base64_basename(const char * path);

/********************************
 *    macros
 ********************************/

#ifndef base64_printf
    #define base64_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef base64_err_printf
    #define base64_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef base64_fflush
    #define base64_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Program name for error messages */
static const char * base64_prog_name = "base64";

/** @brief Base64 alphabet (RFC 4648 Section 4) */
static const char base64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/** @brief Padding character */
static const char base64_pad = '=';

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the base64 command
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

    base64_prog_name = _base64_basename(argv[0]);

#ifdef BASE64_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif

    base64_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.wrap = BASE64_DEFAULT_WRAP;

    int file_start = 0;
    if (!_base64_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    int nfiles = argc - file_start;
    int exit_code = 0;

    if (nfiles == 0) {
        /* stdin */
        if (opts.decode) {
            exit_code = _base64_do_decode(stdin, &opts);
        }
        else {
            exit_code = _base64_do_encode(stdin, &opts);
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
                    base64_err_printf("%s: %s: %s\n",
                                      base64_prog_name,
                                      argv[i], strerror(errno));
                    exit_code = 1;
                    continue;
                }
            }

            int rc;
            if (opts.decode) {
                rc = _base64_do_decode(fp, &opts);
            }
            else {
                rc = _base64_do_encode(fp, &opts);
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
static void _base64_print_help(void)
{
    base64_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Base64 encode or decode FILE, or standard input, to standard output.\n"
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
        "The data are encoded as described for the base64 alphabet (RFC 4648).\n"
        "When decoding, the input may contain newlines in addition to the\n"
        "bytes of the formal alphabet. Use --ignore-garbage to ignore\n"
        "other non-alphabet bytes.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        base64_prog_name, BASE64_DEFAULT_WRAP
    );
}

/**
 * @brief Print version information
 */
static void _base64_print_version(void)
{
    base64_printf("base64 %s\n", BASE64_VERSION_STR);
    base64_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    base64_printf("%s", "License MIT: <https://mit-license.org/>\n");
    base64_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    base64_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract basename from a path
 * @param path  full path (e.g. "/usr/bin/base64" or "base64.exe")
 * @return pointer to basename within path
 */
static const char * _base64_basename(const char * path)
{
    if (!path) {
        return "base64";
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
static int _base64_parse_opts(int argc, char ** argv,
                              base64_opts * opts, int * file_start)
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
                _base64_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _base64_print_version();
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
                    base64_err_printf("%s: invalid wrap size '%s'\n",
                                      base64_prog_name, arg + 7);
                    return false;
                }
                opts->wrap = (int)cols;
                continue;
            }
            /* --wrap COLS (separate arg) */
            if (strcmp(arg, "--wrap") == 0) {
                if (i + 1 >= argc) {
                    base64_err_printf("%s: option '--wrap' requires an argument\n",
                                      base64_prog_name);
                    return false;
                }
                char * end = NULL;
                errno = 0;
                long cols = strtol(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    base64_err_printf("%s: invalid wrap size '%s'\n",
                                      base64_prog_name, argv[i + 1]);
                    return false;
                }
                opts->wrap = (int)cols;
                i++;
                continue;
            }

            base64_err_printf("%s: unrecognized option '%s'\n",
                              base64_prog_name, arg);
            base64_err_printf("%s", "Try 'base64 --help' for more information.\n");
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
                            base64_err_printf("%s: option '-w' requires an argument\n",
                                              base64_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        errno = 0;
                        long cols = strtol(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' || cols < 0) {
                            base64_err_printf("%s: invalid wrap size '%s'\n",
                                              base64_prog_name, val);
                            return false;
                        }
                        opts->wrap = (int)cols;
                        goto next_arg;
                    }
                    default:
                        base64_err_printf("%s: invalid option -- '%c'\n",
                                          base64_prog_name, arg[j]);
                        base64_err_printf("%s", "Try 'base64 --help' for more information.\n");
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
 * @brief Encode a 3-byte block into 4 base64 characters
 *
 * Encoding maps 3 bytes (24 bits) to 4 base64 chars (6 bits each).
 * Partial blocks are padded with '=' per RFC 4648.
 *
 * @param in     input bytes (up to 3)
 * @param out    output chars (always 4 written)
 * @param in_len number of valid input bytes (1-3)
 */
static void _base64_encode_block(const uint8_t in[3], uint8_t out[4],
                                 size_t in_len)
{
    /* Extract 4 groups of 6 bits from 3 bytes (24 bits) */
    out[0] = (uint8_t)base64_alphabet[in[0] >> 2];

    if (in_len == 1) {
        out[1] = (uint8_t)base64_alphabet[(in[0] & 0x03) << 4];
        out[2] = (uint8_t)base64_pad;
        out[3] = (uint8_t)base64_pad;
    }
    else if (in_len == 2) {
        out[1] = (uint8_t)base64_alphabet[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        out[2] = (uint8_t)base64_alphabet[(in[1] & 0x0F) << 2];
        out[3] = (uint8_t)base64_pad;
    }
    else {
        out[1] = (uint8_t)base64_alphabet[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        out[2] = (uint8_t)base64_alphabet[((in[1] & 0x0F) << 2) | (in[2] >> 6)];
        out[3] = (uint8_t)base64_alphabet[in[2] & 0x3F];
    }
}

/**
 * @brief Decode a base64 character to its 6-bit value
 *
 * @param c  input character
 * @return 0-63 if valid, -2 if padding, -1 if invalid
 */
static int _base64_decode_char(int c)
{
    if (c == '=') {
        return -2; /* padding */
    }

    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;

    return -1; /* invalid */
}

/**
 * @brief Decode a 4-character base64 block into up to 3 bytes
 *
 * @param in   4 base64 characters
 * @param out  output buffer (up to 3 bytes)
 * @return number of decoded bytes (1-3), or -1 on error
 */
static int _base64_decode_block(const uint8_t in[4], uint8_t out[3])
{
    int vals[4];
    int pad_start = 4;

    for (int i = 0; i < 4; i++) {
        int v = _base64_decode_char(in[i]);
        if (v == -2) {
            /* Padding */
            if (pad_start == 4) {
                pad_start = i;
            }
            vals[i] = 0;
        }
        else if (v < 0) {
            return -1;
        }
        else {
            if (pad_start < 4) {
                /* Non-pad char after padding = error */
                return -1;
            }
            vals[i] = v;
        }
    }

    /* Check valid padding positions per RFC 4648:
     * 1 byte -> 2 chars + 2 pad  (pad_start = 2)
     * 2 bytes -> 3 chars + 1 pad  (pad_start = 3)
     * 3 bytes -> 4 chars + 0 pad  (pad_start = 4)
     */
    int out_len;
    switch (pad_start) {
        case 2:  out_len = 1; break;
        case 3:  out_len = 2; break;
        case 4:  out_len = 3; break;
        default: return -1; /* invalid padding */
    }

    /* Reconstruct bytes from 4x6-bit values */
    out[0] = (uint8_t)((vals[0] << 2) | (vals[1] >> 4));

    if (out_len > 1) {
        out[1] = (uint8_t)(((vals[1] & 0x0F) << 4) | (vals[2] >> 2));
    }
    if (out_len > 2) {
        out[2] = (uint8_t)(((vals[2] & 0x03) << 6) | vals[3]);
    }

    return out_len;
}

/**
 * @brief Encode a stream to base64
 *
 * Reads data from the input stream and writes base64-encoded output
 * with line wrapping.
 *
 * @param in    input stream
 * @param opts  options (wrap width)
 * @return 0 on success, -1 on error
 */
static int _base64_do_encode(FILE * in, const base64_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE64_IO_BUF_SIZE);
    if (!buf) {
        base64_err_printf("%s: out of memory\n", base64_prog_name);
        return -1;
    }

    int col = 0;
    uint8_t inbuf[BASE64_QUANTUM_BYTES];
    size_t inbuf_len = 0;

    size_t n;
    while ((n = fread(buf, 1, BASE64_IO_BUF_SIZE, in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            inbuf[inbuf_len++] = buf[i];

            if (inbuf_len == BASE64_QUANTUM_BYTES) {
                uint8_t out[BASE64_QUANTUM_CHARS];
                _base64_encode_block(inbuf, out,
                                     BASE64_QUANTUM_BYTES);

                for (int j = 0; j < BASE64_QUANTUM_CHARS; j++) {
                    if (opts->wrap > 0 && col >= opts->wrap) {
                        putchar('\n');
                        col = 0;
                    }
                    putchar(out[j]);
                    col++;
                }
                inbuf_len = 0;
            }
        }
    }

    free(buf);

    if (ferror(in)) {
        base64_err_printf("%s: read error\n", base64_prog_name);
        return -1;
    }

    /* Encode remaining partial block */
    if (inbuf_len > 0) {
        uint8_t out[BASE64_QUANTUM_CHARS];
        memset(inbuf + inbuf_len, 0, BASE64_QUANTUM_BYTES - inbuf_len);
        _base64_encode_block(inbuf, out, inbuf_len);

        for (int j = 0; j < BASE64_QUANTUM_CHARS; j++) {
            if (opts->wrap > 0 && col >= opts->wrap) {
                putchar('\n');
                col = 0;
            }
            putchar(out[j]);
            col++;
        }
    }

    /* Final newline */
    putchar('\n');

    base64_fflush(stdout);
    return 0;
}

/**
 * @brief Decode a base64 stream to raw bytes
 *
 * Reads base64 data, ignores garbage if requested, and writes
 * decoded bytes to stdout.
 *
 * @param in    input stream
 * @param opts  options (ignore_garbage)
 * @return 0 on success, -1 on error
 */
static int _base64_do_decode(FILE * in, const base64_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE64_IO_BUF_SIZE);
    if (!buf) {
        base64_err_printf("%s: out of memory\n", base64_prog_name);
        return -1;
    }

    uint8_t block[BASE64_QUANTUM_CHARS];
    size_t block_len = 0;
    bool seen_pad = false;
    bool done = false;
    bool bom_checked = false;
    int exit_code = 0;

    size_t n;
    while (!done && (n = fread(buf, 1, BASE64_IO_BUF_SIZE, in)) > 0) {
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

            /* Skip newlines and whitespace always */
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                continue;
            }

            int val = _base64_decode_char(c);

            if (val == -2) {
                /* Padding */
                block[block_len++] = (uint8_t)c;

                if (block_len == BASE64_QUANTUM_CHARS) {
                    uint8_t out[3];
                    int outlen = _base64_decode_block(block, out);
                    if (outlen < 0) {
                        base64_err_printf("%s: invalid input\n",
                                          base64_prog_name);
                        exit_code = -1;
                        done = true;
                        break;
                    }
                    if (outlen > 0) {
                        fwrite(out, 1, (size_t)outlen, stdout);
                    }
                    block_len = 0;
                    seen_pad = true;
                }
                continue;
            }

            if (val < 0) {
                /* Invalid character */
                if (opts->ignore_garbage) {
                    continue;
                }
                base64_err_printf("%s: invalid input\n",
                                  base64_prog_name);
                exit_code = -1;
                done = true;
                break;
            }

            /* Valid alphabet character */
            if (seen_pad) {
                /* Non-pad after padding = error */
                base64_err_printf("%s: invalid input\n",
                                  base64_prog_name);
                exit_code = -1;
                done = true;
                break;
            }

            block[block_len++] = (uint8_t)c;

            if (block_len == BASE64_QUANTUM_CHARS) {
                uint8_t out[3];
                int outlen = _base64_decode_block(block, out);
                if (outlen < 0) {
                    base64_err_printf("%s: invalid input\n",
                                      base64_prog_name);
                    exit_code = -1;
                    done = true;
                    break;
                }
                if (outlen > 0) {
                    fwrite(out, 1, (size_t)outlen, stdout);
                }
                block_len = 0;
            }
        }
    }

    free(buf);

    if (ferror(in)) {
        base64_err_printf("%s: read error\n", base64_prog_name);
        return -1;
    }

    /* Decode remaining partial block without padding */
    if (exit_code == 0 && block_len > 0) {
        /* base64 does not require padding on decode;
         * pad with '=' to make a full quantum */
        while (block_len < BASE64_QUANTUM_CHARS) {
            block[block_len++] = (uint8_t)'=';
        }
        uint8_t out[3];
        int outlen = _base64_decode_block(block, out);
        if (outlen < 0) {
            base64_err_printf("%s: invalid input\n",
                              base64_prog_name);
            exit_code = -1;
        }
        else if (outlen > 0) {
            fwrite(out, 1, (size_t)outlen, stdout);
        }
    }

    base64_fflush(stdout);
    return exit_code;
}
