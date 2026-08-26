/**
 * @file base32.c
 * @brief Cross-platform implementation of the base32 command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils base32(1).
 *
 * Key behaviors:
 *   - Encodes/decodes RFC 4648 Base32 (A-Z2-7) and Base32hex (0-9A-V)
 *   - With no FILE, or when FILE is -, reads standard input
 *   - -d, --decode         decode data
 *   - -i, --ignore-garbage when decoding, ignore non-alphabet characters
 *   - -w, --wrap=COLS      wrap encoded lines after COLS chars (default 76, 0=none)
 *   -     --base32hex      use base32hex (RFC 4648 Section 7) alphabet
 *   -     --help / --version
 *   - stdin support (no file args, or - as filename)
 *   - Binary-safe I/O
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o base32.exe base32.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o base32 base32.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o base32 base32.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o base32 base32.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o base32 base32.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o base32 base32.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/base32>
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
    #define BASE32_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASE32_PLATFORM_LINUX   1
    #define BASE32_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASE32_PLATFORM_MACOS   1
    #define BASE32_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASE32_PLATFORM_FREEBSD 1
    #define BASE32_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASE32_PLATFORM_OPENBSD 1
    #define BASE32_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASE32_PLATFORM_NETBSD  1
    #define BASE32_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASE32_PLATFORM_POSIX   1
#else
    #define BASE32_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef BASE32_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef BASE32_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef BASE32_PLATFORM_NETBSD
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

#ifdef BASE32_PLATFORM_WINDOWS
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
#define BASE32_VERSION_STR "v1.0.0"

/** @brief I/O buffer size */
#define BASE32_IO_BUF_SIZE 65536

/** @brief Default line wrap width */
#define BASE32_DEFAULT_WRAP 76

/** @brief Size of a base32 quantum (5 bytes → 8 base32 chars) */
#define BASE32_QUANTUM_BYTES 5

/** @brief Number of base32 characters per quantum */
#define BASE32_QUANTUM_CHARS 8

/********************************
 *    typedefs
 ********************************/

/** @brief Command-line options */
typedef struct {
    bool decode;
    bool ignore_garbage;
    bool base32hex;
    int wrap;
} base32_opts;

/********************************
 *    static prototypes
 ********************************/
static void _base32_print_help(void);
static void _base32_print_version(void);
static int  _base32_parse_opts(int argc, char ** argv,
                               base32_opts * opts, int * file_start);
static int  _base32_do_encode(FILE * in, const base32_opts * opts);
static int  _base32_do_decode(FILE * in, const base32_opts * opts);
static void _base32_encode_block(const uint8_t in[5], uint8_t out[8],
                                 size_t in_len, bool hex);
static int  _base32_decode_block(const uint8_t in[8], uint8_t out[5],
                                 bool hex);
static int  _base32_decode_char(int c, bool hex);
static const char * _base32_basename(const char * path);

/********************************
 *    macros
 ********************************/

#ifndef base32_printf
    #define base32_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef base32_err_printf
    #define base32_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef base32_fflush
    #define base32_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

/** @brief Program name for error messages */
static const char * base32_prog_name = "base32";

/** @brief Base32 alphabet (RFC 4648 Section 4) */
static const char base32_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

/** @brief Base32hex alphabet (RFC 4648 Section 7) */
static const char base32hex_alphabet[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUV";

/** @brief Padding character */
static const char base32_pad = '=';

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the base32 command
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

    base32_prog_name = _base32_basename(argv[0]);

#ifdef BASE32_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif

    base32_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.wrap = BASE32_DEFAULT_WRAP;

    int file_start = 0;
    if (!_base32_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    int nfiles = argc - file_start;
    int exit_code = 0;

    if (nfiles == 0) {
        /* stdin */
        if (opts.decode) {
            exit_code = _base32_do_decode(stdin, &opts);
        }
        else {
            exit_code = _base32_do_encode(stdin, &opts);
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
                    base32_err_printf("%s: %s: %s\n",
                                      base32_prog_name,
                                      argv[i], strerror(errno));
                    exit_code = 1;
                    continue;
                }
            }

            int rc;
            if (opts.decode) {
                rc = _base32_do_decode(fp, &opts);
            }
            else {
                rc = _base32_do_encode(fp, &opts);
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
static void _base32_print_help(void)
{
    base32_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Base32 encode or decode FILE, or standard input, to standard output.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "\n"
        "  -d, --decode             decode data\n"
        "  -i, --ignore-garbage     when decoding, ignore non-alphabet characters\n"
        "      --base32hex          use base32hex (RFC 4648 Section 7) alphabet\n"
        "  -w, --wrap=COLS          wrap encoded lines after COLS characters\n"
        "                           (default %d, 0 to disable wrap)\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "The data are encoded as described for the base32 alphabet (RFC 4648)\n"
        "when --base32hex is not used, and decoded using the same alphabet.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        base32_prog_name, BASE32_DEFAULT_WRAP
    );
}

/**
 * @brief Print version information
 */
static void _base32_print_version(void)
{
    base32_printf("base32 %s\n", BASE32_VERSION_STR);
    base32_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    base32_printf("%s", "License MIT: <https://mit-license.org/>\n");
    base32_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    base32_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract basename from a path
 * @param path  full path (e.g. "/usr/bin/base32" or "base32.exe")
 * @return pointer to basename within path
 */
static const char * _base32_basename(const char * path)
{
    if (!path) {
        return "base32";
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
static int _base32_parse_opts(int argc, char ** argv,
                              base32_opts * opts, int * file_start)
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
                _base32_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _base32_print_version();
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
            if (strcmp(arg, "--base32hex") == 0) {
                opts->base32hex = true;
                continue;
            }
            /* --wrap=COLS */
            if (strncmp(arg, "--wrap=", 7) == 0) {
                char * end = NULL;
                errno = 0;
                long cols = strtol(arg + 7, &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    base32_err_printf("%s: invalid wrap size '%s'\n",
                                      base32_prog_name, arg + 7);
                    return false;
                }
                opts->wrap = (int)cols;
                continue;
            }
            /* --wrap COLS (separate arg) */
            if (strcmp(arg, "--wrap") == 0) {
                if (i + 1 >= argc) {
                    base32_err_printf("%s: option '--wrap' requires an argument\n",
                                      base32_prog_name);
                    return false;
                }
                char * end = NULL;
                errno = 0;
                long cols = strtol(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    base32_err_printf("%s: invalid wrap size '%s'\n",
                                      base32_prog_name, argv[i + 1]);
                    return false;
                }
                opts->wrap = (int)cols;
                i++;
                continue;
            }

            base32_err_printf("%s: unrecognized option '%s'\n",
                              base32_prog_name, arg);
            base32_err_printf("%s", "Try 'base32 --help' for more information.\n");
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
                            base32_err_printf("%s: option '-w' requires an argument\n",
                                              base32_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        errno = 0;
                        long cols = strtol(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' || cols < 0) {
                            base32_err_printf("%s: invalid wrap size '%s'\n",
                                              base32_prog_name, val);
                            return false;
                        }
                        opts->wrap = (int)cols;
                        goto next_arg;
                    }
                    default:
                        base32_err_printf("%s: invalid option -- '%c'\n",
                                          base32_prog_name, arg[j]);
                        base32_err_printf("%s", "Try 'base32 --help' for more information.\n");
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
 * @brief Encode a 5-byte block into 8 base32 characters
 *
 * Encoding maps 5 bytes (40 bits) to 8 base32 chars (5 bits each).
 * Partial blocks are padded with '=' per RFC 4648.
 *
 * @param in     input bytes (up to 5)
 * @param out    output chars (always 8 written)
 * @param in_len number of valid input bytes (1-5)
 * @param hex    if true, use base32hex alphabet
 */
static void _base32_encode_block(const uint8_t in[5], uint8_t out[8],
                                 size_t in_len, bool hex)
{
    const char * alphabet = hex ? base32hex_alphabet : base32_alphabet;

    uint64_t val = 0;
    val |= (uint64_t)in[0] << 32;
    if (in_len > 1) val |= (uint64_t)in[1] << 24;
    if (in_len > 2) val |= (uint64_t)in[2] << 16;
    if (in_len > 3) val |= (uint64_t)in[3] << 8;
    if (in_len > 4) val |= (uint64_t)in[4];

    /* Extract 8 groups of 5 bits (from MSB) */
    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t)alphabet[(val >> (35 - i * 5)) & 0x1F];
    }

    /* Pad incomplete quantum */
    int pad_count = 0;
    switch (in_len) {
        case 1: pad_count = 6; break;
        case 2: pad_count = 4; break;
        case 3: pad_count = 3; break;
        case 4: pad_count = 1; break;
        default: pad_count = 0; break;
    }

    for (int i = 0; i < pad_count; i++) {
        out[7 - i] = (uint8_t)base32_pad;
    }
}

/**
 * @brief Decode a base32 character to its 5-bit value
 *
 * @param c    input character
 * @param hex  if true, use base32hex alphabet
 * @return 0-31 if valid, -1 if invalid
 */
static int _base32_decode_char(int c, bool hex)
{
    if (c == '=') {
        return -2; /* padding */
    }

    if (hex) {
        /* base32hex: 0-9 = 0-9, A-V = 10-31 */
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'V') return c - 'A' + 10;
        if (c >= 'a' && c <= 'v') return c - 'a' + 10;
    }
    else {
        /* base32: A-Z = 0-25, 2-7 = 26-31 */
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c >= '2' && c <= '7') return c - '2' + 26;
    }

    return -1; /* invalid */
}

/**
 * @brief Decode an 8-character base32 block into up to 5 bytes
 *
 * @param in   8 base32 characters
 * @param out  output buffer (up to 5 bytes)
 * @param hex  if true, use base32hex alphabet
 * @return number of decoded bytes (1-5), or -1 on error
 */
static int _base32_decode_block(const uint8_t in[8], uint8_t out[5],
                                bool hex)
{
    int vals[8];
    int pad_start = 8;

    for (int i = 0; i < 8; i++) {
        int v = _base32_decode_char(in[i], hex);
        if (v == -2) {
            /* Padding */
            if (pad_start == 8) {
                pad_start = i;
            }
            vals[i] = 0;
        }
        else if (v < 0) {
            return -1;
        }
        else {
            if (pad_start < 8) {
                /* Non-pad char after padding = error */
                return -1;
            }
            vals[i] = v;
        }
    }

    /* Check valid padding positions per RFC 4648:
     * 1 byte → 2 chars + 6 pad  (pad_start = 2)
     * 2 bytes → 4 chars + 4 pad  (pad_start = 4)
     * 3 bytes → 5 chars + 3 pad  (pad_start = 5)
     * 4 bytes → 7 chars + 1 pad  (pad_start = 7)
     * 5 bytes → 8 chars + 0 pad  (pad_start = 8)
     */
    int out_len;
    switch (pad_start) {
        case 2:  out_len = 1; break;
        case 4:  out_len = 2; break;
        case 5:  out_len = 3; break;
        case 7:  out_len = 4; break;
        case 8:  out_len = 5; break;
        default: return -1; /* invalid padding */
    }

    /* Reconstruct bytes from 8x5-bit values */
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (uint64_t)vals[i] << (35 - i * 5);
    }

    for (int i = 0; i < out_len; i++) {
        out[i] = (uint8_t)(val >> (32 - i * 8));
    }

    return out_len;
}

/**
 * @brief Encode a stream to base32
 *
 * Reads data from the input stream and writes base32-encoded output
 * with line wrapping.
 *
 * @param in    input stream
 * @param opts  options (wrap width, alphabet)
 * @return 0 on success, -1 on error
 */
static int _base32_do_encode(FILE * in, const base32_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE32_IO_BUF_SIZE);
    if (!buf) {
        base32_err_printf("%s: out of memory\n", base32_prog_name);
        return -1;
    }

    int col = 0;
    uint8_t inbuf[BASE32_QUANTUM_BYTES];
    size_t inbuf_len = 0;

    size_t n;
    while ((n = fread(buf, 1, BASE32_IO_BUF_SIZE, in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            inbuf[inbuf_len++] = buf[i];

            if (inbuf_len == BASE32_QUANTUM_BYTES) {
                uint8_t out[BASE32_QUANTUM_CHARS];
                _base32_encode_block(inbuf, out,
                                     BASE32_QUANTUM_BYTES, opts->base32hex);

                for (int j = 0; j < BASE32_QUANTUM_CHARS; j++) {
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
        base32_err_printf("%s: read error\n", base32_prog_name);
        return -1;
    }

    /* Encode remaining partial block */
    if (inbuf_len > 0) {
        uint8_t out[BASE32_QUANTUM_CHARS];
        memset(inbuf + inbuf_len, 0, BASE32_QUANTUM_BYTES - inbuf_len);
        _base32_encode_block(inbuf, out, inbuf_len, opts->base32hex);

        for (int j = 0; j < BASE32_QUANTUM_CHARS; j++) {
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

    base32_fflush(stdout);
    return 0;
}

/**
 * @brief Decode a base32 stream to raw bytes
 *
 * Reads base32 data, ignores garbage if requested, and writes
 * decoded bytes to stdout.
 *
 * @param in    input stream
 * @param opts  options (ignore_garbage, alphabet)
 * @return 0 on success, -1 on error
 */
static int _base32_do_decode(FILE * in, const base32_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASE32_IO_BUF_SIZE);
    if (!buf) {
        base32_err_printf("%s: out of memory\n", base32_prog_name);
        return -1;
    }

    uint8_t block[BASE32_QUANTUM_CHARS];
    size_t block_len = 0;
    bool seen_pad = false;
    bool done = false;
    bool bom_checked = false;
    int exit_code = 0;

    size_t n;
    while (!done && (n = fread(buf, 1, BASE32_IO_BUF_SIZE, in)) > 0) {
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

            int val = _base32_decode_char(c, opts->base32hex);

            if (val == -2) {
                /* Padding */
                if (!seen_pad) {
                    seen_pad = true;
                }
                block[block_len++] = (uint8_t)c;

                if (block_len == BASE32_QUANTUM_CHARS) {
                    uint8_t out[5];
                    int outlen = _base32_decode_block(block, out,
                                                     opts->base32hex);
                    if (outlen < 0) {
                        base32_err_printf("%s: invalid input\n",
                                          base32_prog_name);
                        exit_code = -1;
                        done = true;
                        break;
                    }
                    if (outlen > 0) {
                        fwrite(out, 1, (size_t)outlen, stdout);
                    }
                    block_len = 0;
                    /* After a complete padded block, stop */
                    done = true;
                    break;
                }
                continue;
            }

            if (val < 0) {
                /* Invalid character */
                if (opts->ignore_garbage) {
                    continue;
                }
                base32_err_printf("%s: invalid input\n",
                                  base32_prog_name);
                exit_code = -1;
                done = true;
                break;
            }

            /* Valid alphabet character */
            if (seen_pad) {
                /* Non-pad after padding = error */
                base32_err_printf("%s: invalid input\n",
                                  base32_prog_name);
                exit_code = -1;
                done = true;
                break;
            }

            block[block_len++] = (uint8_t)c;

            if (block_len == BASE32_QUANTUM_CHARS) {
                uint8_t out[5];
                int outlen = _base32_decode_block(block, out,
                                                 opts->base32hex);
                if (outlen < 0) {
                    base32_err_printf("%s: invalid input\n",
                                      base32_prog_name);
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
        base32_err_printf("%s: read error\n", base32_prog_name);
        return -1;
    }

    /* Decode remaining partial block without padding */
    if (exit_code == 0 && block_len > 0) {
        /* base32 does not require padding on decode;
         * pad with '=' to make a full quantum */
        while (block_len < BASE32_QUANTUM_CHARS) {
            block[block_len++] = (uint8_t)'=';
        }
        uint8_t out[5];
        int outlen = _base32_decode_block(block, out, opts->base32hex);
        if (outlen < 0) {
            base32_err_printf("%s: invalid input\n",
                              base32_prog_name);
            exit_code = -1;
        }
        else if (outlen > 0) {
            fwrite(out, 1, (size_t)outlen, stdout);
        }
    }

    base32_fflush(stdout);
    return exit_code;
}
