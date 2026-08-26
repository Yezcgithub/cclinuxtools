/**
 * @file basenc.c
 * @brief Cross-platform implementation of the basenc command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils basenc(1).
 *
 * Supported encodings:
 *   --base64       RFC 4648 section 4  (A-Za-z0-9+/)
 *   --base64url    RFC 4648 section 5  (A-Za-z0-9-_)
 *   --base32       RFC 4648 section 6  (A-Z2-7)
 *   --base32hex    RFC 4648 section 7  (0-9A-V)
 *   --base16       RFC 4648 section 8  (0-9A-F)
 *   --base2msbf    bit string MSB first
 *   --base2lsbf    bit string LSB first
 *   --z85          ZeroMQ Z85 (input must be multiple of 4 bytes)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o basenc.exe basenc.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o basenc basenc.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o basenc basenc.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o basenc basenc.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o basenc basenc.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o basenc basenc.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/basenc>
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
 * Platform detection macros — must appear before any system includes.
 */
#if defined(_WIN32) || defined(_WIN64)
    #define BASENC_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define BASENC_PLATFORM_LINUX   1
    #define BASENC_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASENC_PLATFORM_MACOS   1
    #define BASENC_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define BASENC_PLATFORM_FREEBSD 1
    #define BASENC_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define BASENC_PLATFORM_OPENBSD 1
    #define BASENC_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define BASENC_PLATFORM_NETBSD  1
    #define BASENC_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define BASENC_PLATFORM_POSIX   1
#else
    #define BASENC_PLATFORM_POSIX   1
#endif

#ifdef BASENC_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef BASENC_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef BASENC_PLATFORM_NETBSD
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

#ifdef BASENC_PLATFORM_WINDOWS
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

#define BASENC_VERSION_STR "v1.0.0"
#define BASENC_IO_BUF_SIZE 65536
#define BASENC_DEFAULT_WRAP 76

/** Encoding type identifiers */
#define BASENC_ENC_BASE64     0
#define BASENC_ENC_BASE64URL  1
#define BASENC_ENC_BASE32     2
#define BASENC_ENC_BASE32HEX  3
#define BASENC_ENC_BASE16     4
#define BASENC_ENC_BASE2MSBF  5
#define BASENC_ENC_BASE2LSBF  6
#define BASENC_ENC_Z85        7

/********************************
 *    typedefs
 ********************************/

typedef struct {
    int encoding;
    bool decode;
    bool ignore_garbage;
    int wrap;
} basenc_opts;

/********************************
 *    static prototypes
 ********************************/
static void _basenc_print_help(void);
static void _basenc_print_version(void);
static const char * _basenc_basename(const char * path);
static int _basenc_parse_opts(int argc, char ** argv,
                              basenc_opts * opts, int * file_start);
static int _basenc_do_encode(FILE * in, const basenc_opts * opts);
static int _basenc_do_decode(FILE * in, const basenc_opts * opts);
static int _basenc_encode_data(const uint8_t * in, size_t in_len,
                               uint8_t * out, size_t * out_len,
                               int encoding);
static int _basenc_decode_data(const uint8_t * in, size_t in_len,
                               uint8_t * out, size_t * out_len,
                               int encoding, bool ignore_garbage);

/********************************
 *    macros
 ********************************/

#ifndef basenc_printf
    #define basenc_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef basenc_err_printf
    #define basenc_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef basenc_fflush
    #define basenc_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/********************************
 *    static variables
 ********************************/

static const char * basenc_prog_name = "basenc";

static const char b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b64url_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static const char b32_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char b32hex_alphabet[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUV";
static const char b16_alphabet[] =
    "0123456789ABCDEF";

/* Z85 alphabet from ZeroMQ spec:32/Z85 */
static const char z85_alphabet[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";

/********************************
 *    global functions
 ********************************/

int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 1;
    }

    basenc_prog_name = _basenc_basename(argv[0]);

#ifdef BASENC_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif

    basenc_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.wrap = BASENC_DEFAULT_WRAP;
    opts.encoding = -1;

    int file_start = 0;
    if (!_basenc_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    if (opts.encoding < 0) {
        basenc_err_printf("%s: missing encoding type\n", basenc_prog_name);
        basenc_err_printf("%s", "Try 'basenc --help' for more information.\n");
        return 1;
    }

    int nfiles = argc - file_start;
    int exit_code = 0;

    if (nfiles == 0) {
        if (opts.decode) {
            exit_code = _basenc_do_decode(stdin, &opts);
        }
        else {
            exit_code = _basenc_do_encode(stdin, &opts);
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
                    basenc_err_printf("%s: %s: %s\n",
                                      basenc_prog_name,
                                      argv[i], strerror(errno));
                    exit_code = 1;
                    continue;
                }
            }

            int rc;
            if (opts.decode) {
                rc = _basenc_do_decode(fp, &opts);
            }
            else {
                rc = _basenc_do_encode(fp, &opts);
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

static void _basenc_print_help(void)
{
    basenc_printf(
        "Usage: %s [OPTION]... [FILE]\n"
        "Base encode or decode FILE, or standard input, to standard output.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "\n"
        "      --base64               same as 'base64' program (RFC4648 section 4)\n"
        "      --base64url            file- and url-safe base64 (RFC4648 section 5)\n"
        "      --base32               same as 'base32' program (RFC4648 section 6)\n"
        "      --base32hex            extended hex alphabet base32 (RFC4648 section 7)\n"
        "      --base16               hex encoding (RFC4648 section 8)\n"
        "      --base2msbf            bit string with most significant bit (msb) first\n"
        "      --base2lsbf            bit string with least significant bit (lsb) first\n"
        "      --z85                  ascii85-like encoding (ZeroMQ spec:32/Z85);\n"
        "                             when encoding, input length must be a multiple of 4;\n"
        "                             when decoding, input length must be a multiple of 5\n"
        "  -d, --decode               decode data\n"
        "  -i, --ignore-garbage       when decoding, ignore non-alphabet characters\n"
        "  -w, --wrap=COLS            wrap encoded lines after COLS characters\n"
        "                             (default %d, 0 to disable wrap)\n"
        "      --help                 display this help and exit\n"
        "      --version              output version information and exit\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        basenc_prog_name, BASENC_DEFAULT_WRAP
    );
}

static void _basenc_print_version(void)
{
    basenc_printf("basenc %s\n", BASENC_VERSION_STR);
    basenc_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    basenc_printf("%s", "License MIT: <https://mit-license.org/>\n");
    basenc_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    basenc_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

static const char * _basenc_basename(const char * path)
{
    if (!path) {
        return "basenc";
    }

    const char * base = path;
    for (const char * p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static int _basenc_parse_opts(int argc, char ** argv,
                              basenc_opts * opts, int * file_start)
{
    if (!opts || !file_start) {
        return false;
    }

    *file_start = 1;
    int enc_count = 0;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        if (strncmp(arg, "--", 2) == 0) {
            if (strcmp(arg, "--help") == 0) {
                _basenc_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _basenc_print_version();
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
            if (strcmp(arg, "--base64") == 0) {
                opts->encoding = BASENC_ENC_BASE64;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base64url") == 0) {
                opts->encoding = BASENC_ENC_BASE64URL;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base32") == 0) {
                opts->encoding = BASENC_ENC_BASE32;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base32hex") == 0) {
                opts->encoding = BASENC_ENC_BASE32HEX;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base16") == 0) {
                opts->encoding = BASENC_ENC_BASE16;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base2msbf") == 0) {
                opts->encoding = BASENC_ENC_BASE2MSBF;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--base2lsbf") == 0) {
                opts->encoding = BASENC_ENC_BASE2LSBF;
                enc_count++;
                continue;
            }
            if (strcmp(arg, "--z85") == 0) {
                opts->encoding = BASENC_ENC_Z85;
                enc_count++;
                continue;
            }
            if (strncmp(arg, "--wrap=", 7) == 0) {
                char * end = NULL;
                errno = 0;
                long cols = strtol(arg + 7, &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    basenc_err_printf("%s: invalid wrap size '%s'\n",
                                      basenc_prog_name, arg + 7);
                    return false;
                }
                opts->wrap = (int)cols;
                continue;
            }
            if (strcmp(arg, "--wrap") == 0) {
                if (i + 1 >= argc) {
                    basenc_err_printf("%s: option '--wrap' requires an argument\n",
                                      basenc_prog_name);
                    return false;
                }
                char * end = NULL;
                errno = 0;
                long cols = strtol(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' || cols < 0) {
                    basenc_err_printf("%s: invalid wrap size '%s'\n",
                                      basenc_prog_name, argv[i + 1]);
                    return false;
                }
                opts->wrap = (int)cols;
                i++;
                continue;
            }

            basenc_err_printf("%s: unrecognized option '%s'\n",
                              basenc_prog_name, arg);
            basenc_err_printf("%s", "Try 'basenc --help' for more information.\n");
            return false;
        }

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
                            basenc_err_printf("%s: option '-w' requires an argument\n",
                                              basenc_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        errno = 0;
                        long cols = strtol(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' || cols < 0) {
                            basenc_err_printf("%s: invalid wrap size '%s'\n",
                                              basenc_prog_name, val);
                            return false;
                        }
                        opts->wrap = (int)cols;
                        goto next_arg;
                    }
                    default:
                        basenc_err_printf("%s: invalid option -- '%c'\n",
                                          basenc_prog_name, arg[j]);
                        basenc_err_printf("%s", "Try 'basenc --help' for more information.\n");
                        return false;
                }
            }
            next_arg:
            continue;
        }

        *file_start = i;
        break;
    }

    if (enc_count > 1) {
        basenc_err_printf("%s: extra encoding type specified\n", basenc_prog_name);
        return false;
    }

    return true;
}

/* ---- Encoding helpers ---- */

static int _b64_decode_char(int c, bool url)
{
    if (c == '=') return -2;
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (url) {
        if (c == '-') return 62;
        if (c == '_') return 63;
    }
    else {
        if (c == '+') return 62;
        if (c == '/') return 63;
    }
    return -1;
}

static int _b32_decode_char(int c, bool hex)
{
    if (c == '=') return -2;
    if (hex) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'V') return c - 'A' + 10;
        if (c >= 'a' && c <= 'v') return c - 'a' + 10;
    }
    else {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c >= '2' && c <= '7') return c - '2' + 26;
    }
    return -1;
}

static int _b16_decode_char(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int _z85_decode_char(int c)
{
    const char * p = strchr(z85_alphabet, c);
    if (!p) return -1;
    return (int)(p - z85_alphabet);
}

/**
 * @brief Encode data to the specified encoding
 *
 * @param in       input bytes
 * @param in_len   input length
 * @param out      output buffer (caller must ensure sufficient size)
 * @param out_len  output length (set by this function)
 * @param encoding encoding type
 * @return 0 on success, -1 on error
 */
static int _basenc_encode_data(const uint8_t * in, size_t in_len,
                               uint8_t * out, size_t * out_len,
                               int encoding)
{
    size_t oi = 0;

    switch (encoding) {
        case BASENC_ENC_BASE64:
        case BASENC_ENC_BASE64URL:
        {
            const char * alpha = (encoding == BASENC_ENC_BASE64URL)
                                 ? b64url_alphabet : b64_alphabet;
            size_t i;
            for (i = 0; i + 3 <= in_len; i += 3) {
                out[oi++] = alpha[in[i] >> 2];
                out[oi++] = alpha[((in[i] & 0x03) << 4) | (in[i+1] >> 4)];
                out[oi++] = alpha[((in[i+1] & 0x0F) << 2) | (in[i+2] >> 6)];
                out[oi++] = alpha[in[i+2] & 0x3F];
            }
            size_t rem = in_len - i;
            if (rem == 1) {
                out[oi++] = alpha[in[i] >> 2];
                out[oi++] = alpha[(in[i] & 0x03) << 4];
                out[oi++] = '=';
                out[oi++] = '=';
            }
            else if (rem == 2) {
                out[oi++] = alpha[in[i] >> 2];
                out[oi++] = alpha[((in[i] & 0x03) << 4) | (in[i+1] >> 4)];
                out[oi++] = alpha[(in[i+1] & 0x0F) << 2];
                out[oi++] = '=';
            }
            break;
        }

        case BASENC_ENC_BASE32:
        case BASENC_ENC_BASE32HEX:
        {
            const char * alpha = (encoding == BASENC_ENC_BASE32HEX)
                                 ? b32hex_alphabet : b32_alphabet;
            size_t i;
            for (i = 0; i + 5 <= in_len; i += 5) {
                uint64_t v = ((uint64_t)in[i] << 32) | ((uint64_t)in[i+1] << 24) |
                             ((uint64_t)in[i+2] << 16) | ((uint64_t)in[i+3] << 8) |
                             (uint64_t)in[i+4];
                for (int j = 0; j < 8; j++) {
                    out[oi++] = alpha[(v >> (35 - j * 5)) & 0x1F];
                }
            }
            size_t rem = in_len - i;
            if (rem > 0) {
                uint64_t v = 0;
                for (size_t j = 0; j < rem; j++) {
                    v |= (uint64_t)in[i + j] << (32 - j * 8);
                }
                for (int j = 0; j < 8; j++) {
                    out[oi++] = alpha[(v >> (35 - j * 5)) & 0x1F];
                }
                int pad;
                switch (rem) {
                    case 1: pad = 6; break;
                    case 2: pad = 4; break;
                    case 3: pad = 3; break;
                    case 4: pad = 1; break;
                    default: pad = 0; break;
                }
                for (int j = 0; j < pad; j++) {
                    out[oi - 1 - j] = '=';
                }
            }
            break;
        }

        case BASENC_ENC_BASE16:
        {
            for (size_t i = 0; i < in_len; i++) {
                out[oi++] = b16_alphabet[in[i] >> 4];
                out[oi++] = b16_alphabet[in[i] & 0x0F];
            }
            break;
        }

        case BASENC_ENC_BASE2MSBF:
        {
            for (size_t i = 0; i < in_len; i++) {
                for (int b = 7; b >= 0; b--) {
                    out[oi++] = (in[i] & (1 << b)) ? '1' : '0';
                }
            }
            break;
        }

        case BASENC_ENC_BASE2LSBF:
        {
            for (size_t i = 0; i < in_len; i++) {
                for (int b = 0; b < 8; b++) {
                    out[oi++] = (in[i] & (1 << b)) ? '1' : '0';
                }
            }
            break;
        }

        case BASENC_ENC_Z85:
        {
            if (in_len % 4 != 0) {
                basenc_err_printf("%s: invalid input (length must be multiple of 4)\n",
                                  basenc_prog_name);
                return -1;
            }
            for (size_t i = 0; i < in_len; i += 4) {
                uint32_t v = ((uint32_t)in[i] << 24) | ((uint32_t)in[i+1] << 16) |
                             ((uint32_t)in[i+2] << 8) | (uint32_t)in[i+3];
                uint8_t b[5];
                b[4] = v % 85; v /= 85;
                b[3] = v % 85; v /= 85;
                b[2] = v % 85; v /= 85;
                b[1] = v % 85; v /= 85;
                b[0] = v % 85;
                for (int j = 0; j < 5; j++) {
                    out[oi++] = z85_alphabet[b[j]];
                }
            }
            break;
        }

        default:
            return -1;
    }

    *out_len = oi;
    return 0;
}

/**
 * @brief Decode data from the specified encoding
 *
 * @param in              input bytes
 * @param in_len          input length
 * @param out             output buffer
 * @param out_len         output length (set by this function)
 * @param encoding        encoding type
 * @param ignore_garbage  if true, skip non-alphabet characters
 * @return 0 on success, -1 on error
 */
static int _basenc_decode_data(const uint8_t * in, size_t in_len,
                               uint8_t * out, size_t * out_len,
                               int encoding, bool ignore_garbage)
{
    /* First pass: filter out garbage if needed, collect valid chars */
    uint8_t * clean = (uint8_t *)malloc(in_len + 1);
    if (!clean) {
        return -1;
    }

    size_t ci = 0;
    bool bom_checked = false;

    for (size_t i = 0; i < in_len; i++) {
        int c = in[i];

        /* Skip UTF-8 BOM */
        if (!bom_checked) {
            if (i + 2 < in_len && in[i] == 0xEF && in[i+1] == 0xBB && in[i+2] == 0xBF) {
                bom_checked = true;
                i += 2;
                continue;
            }
            bom_checked = true;
        }

        /* Skip whitespace always */
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            continue;
        }

        /* Check if valid char for this encoding */
        bool valid = false;
        switch (encoding) {
            case BASENC_ENC_BASE64:
                valid = (_b64_decode_char(c, false) >= 0 || c == '=');
                break;
            case BASENC_ENC_BASE64URL:
                valid = (_b64_decode_char(c, true) >= 0 || c == '=');
                break;
            case BASENC_ENC_BASE32:
                valid = (_b32_decode_char(c, false) >= 0 || c == '=');
                break;
            case BASENC_ENC_BASE32HEX:
                valid = (_b32_decode_char(c, true) >= 0 || c == '=');
                break;
            case BASENC_ENC_BASE16:
                valid = (_b16_decode_char(c) >= 0);
                break;
            case BASENC_ENC_BASE2MSBF:
            case BASENC_ENC_BASE2LSBF:
                valid = (c == '0' || c == '1');
                break;
            case BASENC_ENC_Z85:
                valid = (_z85_decode_char(c) >= 0);
                break;
        }

        if (!valid) {
            if (ignore_garbage) {
                continue;
            }
            basenc_err_printf("%s: invalid input\n", basenc_prog_name);
            free(clean);
            return -1;
        }

        clean[ci++] = (uint8_t)c;
    }

    size_t oi = 0;

    switch (encoding) {
        case BASENC_ENC_BASE64:
        case BASENC_ENC_BASE64URL:
        {
            bool url = (encoding == BASENC_ENC_BASE64URL);
            /* Pad to multiple of 4 */
            while (ci % 4 != 0) {
                clean[ci++] = '=';
            }
            for (size_t i = 0; i < ci; i += 4) {
                int v[4];
                int pad = 0;
                for (int j = 0; j < 4; j++) {
                    if (clean[i + j] == '=') {
                        v[j] = 0;
                        pad++;
                    }
                    else {
                        v[j] = _b64_decode_char(clean[i + j], url);
                        if (v[j] < 0) {
                            basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                            free(clean);
                            return -1;
                        }
                    }
                }
                out[oi++] = (uint8_t)((v[0] << 2) | (v[1] >> 4));
                if (pad < 2) {
                    out[oi++] = (uint8_t)(((v[1] & 0x0F) << 4) | (v[2] >> 2));
                }
                if (pad < 1) {
                    out[oi++] = (uint8_t)(((v[2] & 0x03) << 6) | v[3]);
                }
            }
            break;
        }

        case BASENC_ENC_BASE32:
        case BASENC_ENC_BASE32HEX:
        {
            bool hex = (encoding == BASENC_ENC_BASE32HEX);
            /* Pad to multiple of 8 */
            while (ci % 8 != 0) {
                clean[ci++] = '=';
            }
            for (size_t i = 0; i < ci; i += 8) {
                int v[8];
                int pad_start = 8;
                for (int j = 0; j < 8; j++) {
                    if (clean[i + j] == '=') {
                        if (pad_start == 8) pad_start = j;
                        v[j] = 0;
                    }
                    else {
                        v[j] = _b32_decode_char(clean[i + j], hex);
                        if (v[j] < 0) {
                            basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                            free(clean);
                            return -1;
                        }
                        if (pad_start < 8) {
                            basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                            free(clean);
                            return -1;
                        }
                    }
                }
                int out_len2;
                switch (pad_start) {
                    case 2:  out_len2 = 1; break;
                    case 4:  out_len2 = 2; break;
                    case 5:  out_len2 = 3; break;
                    case 7:  out_len2 = 4; break;
                    case 8:  out_len2 = 5; break;
                    default: basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                             free(clean); return -1;
                }
                uint64_t val = 0;
                for (int j = 0; j < 8; j++) {
                    val |= (uint64_t)v[j] << (35 - j * 5);
                }
                for (int j = 0; j < out_len2; j++) {
                    out[oi++] = (uint8_t)(val >> (32 - j * 8));
                }
            }
            break;
        }

        case BASENC_ENC_BASE16:
        {
            if (ci % 2 != 0) {
                basenc_err_printf("%s: invalid input (odd length)\n", basenc_prog_name);
                free(clean);
                return -1;
            }
            for (size_t i = 0; i < ci; i += 2) {
                int hi = _b16_decode_char(clean[i]);
                int lo = _b16_decode_char(clean[i + 1]);
                if (hi < 0 || lo < 0) {
                    basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                    free(clean);
                    return -1;
                }
                out[oi++] = (uint8_t)((hi << 4) | lo);
            }
            break;
        }

        case BASENC_ENC_BASE2MSBF:
        {
            if (ci % 8 != 0) {
                basenc_err_printf("%s: invalid input (length not multiple of 8)\n", basenc_prog_name);
                free(clean);
                return -1;
            }
            for (size_t i = 0; i < ci; i += 8) {
                uint8_t b = 0;
                for (int j = 0; j < 8; j++) {
                    b = (uint8_t)(b << 1);
                    if (clean[i + j] == '1') b |= 1;
                }
                out[oi++] = b;
            }
            break;
        }

        case BASENC_ENC_BASE2LSBF:
        {
            if (ci % 8 != 0) {
                basenc_err_printf("%s: invalid input (length not multiple of 8)\n", basenc_prog_name);
                free(clean);
                return -1;
            }
            for (size_t i = 0; i < ci; i += 8) {
                uint8_t b = 0;
                for (int j = 0; j < 8; j++) {
                    if (clean[i + j] == '1') b |= (uint8_t)(1 << j);
                }
                out[oi++] = b;
            }
            break;
        }

        case BASENC_ENC_Z85:
        {
            if (ci % 5 != 0) {
                basenc_err_printf("%s: invalid input (length not multiple of 5)\n", basenc_prog_name);
                free(clean);
                return -1;
            }
            for (size_t i = 0; i < ci; i += 5) {
                uint32_t v = 0;
                for (int j = 0; j < 5; j++) {
                    int d = _z85_decode_char(clean[i + j]);
                    if (d < 0) {
                        basenc_err_printf("%s: invalid input\n", basenc_prog_name);
                        free(clean);
                        return -1;
                    }
                    v = v * 85 + (uint32_t)d;
                }
                out[oi++] = (uint8_t)(v >> 24);
                out[oi++] = (uint8_t)(v >> 16);
                out[oi++] = (uint8_t)(v >> 8);
                out[oi++] = (uint8_t)(v);
            }
            break;
        }

        default:
            free(clean);
            return -1;
    }

    free(clean);
    *out_len = oi;
    return 0;
}

/**
 * @brief Encode a stream
 */
static int _basenc_do_encode(FILE * in, const basenc_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASENC_IO_BUF_SIZE);
    uint8_t * enc = (uint8_t *)malloc(BASENC_IO_BUF_SIZE * 2 + 256);
    if (!buf || !enc) {
        basenc_err_printf("%s: out of memory\n", basenc_prog_name);
        free(buf);
        free(enc);
        return -1;
    }

    int col = 0;
    size_t n;

    while ((n = fread(buf, 1, BASENC_IO_BUF_SIZE, in)) > 0) {
        size_t enc_len = 0;
        if (_basenc_encode_data(buf, n, enc, &enc_len, opts->encoding) != 0) {
            free(buf);
            free(enc);
            return -1;
        }

        for (size_t i = 0; i < enc_len; i++) {
            if (opts->wrap > 0 && col >= opts->wrap) {
                putchar('\n');
                col = 0;
            }
            putchar(enc[i]);
            col++;
        }
    }

    free(buf);
    free(enc);

    if (ferror(in)) {
        basenc_err_printf("%s: read error\n", basenc_prog_name);
        return -1;
    }

    putchar('\n');
    basenc_fflush(stdout);
    return 0;
}

/**
 * @brief Decode a stream
 */
static int _basenc_do_decode(FILE * in, const basenc_opts * opts)
{
    if (!in || !opts) {
        return -1;
    }

    uint8_t * buf = (uint8_t *)malloc(BASENC_IO_BUF_SIZE);
    uint8_t * dec = (uint8_t *)malloc(BASENC_IO_BUF_SIZE + 256);
    if (!buf || !dec) {
        basenc_err_printf("%s: out of memory\n", basenc_prog_name);
        free(buf);
        free(dec);
        return -1;
    }

    int exit_code = 0;
    size_t n;

    while ((n = fread(buf, 1, BASENC_IO_BUF_SIZE, in)) > 0) {
        size_t dec_len = 0;
        if (_basenc_decode_data(buf, n, dec, &dec_len,
                                opts->encoding, opts->ignore_garbage) != 0) {
            exit_code = -1;
            break;
        }
        if (dec_len > 0) {
            fwrite(dec, 1, dec_len, stdout);
        }
    }

    free(buf);
    free(dec);

    if (ferror(in)) {
        basenc_err_printf("%s: read error\n", basenc_prog_name);
        return -1;
    }

    basenc_fflush(stdout);
    return exit_code;
}
