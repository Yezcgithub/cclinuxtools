/**
 * @file md5sum.c
 * @brief Cross-platform implementation of the md5sum command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils md5sum(1).
 *
 * Key behaviors:
 *   - -b, --binary:   read in binary mode
 *   - -c, --check:    read MD5 sums from files and check them
 *   - --tag:          create a BSD-style checksum
 *   - -t, --text:     read in text mode (default)
 *   - -z, --zero:     end each output line with NUL, not newline
 *   - --ignore-missing: don't fail or report status for missing files
 *   - --quiet:        don't print OK for each successfully verified file
 *   - --status:       don't output anything, status code shows success
 *   - --strict:       exit non-zero for improperly formatted checksum lines
 *   - -w, --warn:     warn about improperly formatted checksum lines
 *   - --help / --version: display help or version information
 *   - With no FILE, or when FILE is -, read standard input
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o md5sum.exe md5sum.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -o md5sum md5sum.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -o md5sum md5sum.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o md5sum md5sum.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o md5sum md5sum.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -o md5sum md5sum.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/md5sum>
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
    #define MD5SUM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define MD5SUM_PLATFORM_LINUX   1
    #define MD5SUM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define MD5SUM_PLATFORM_MACOS   1
    #define MD5SUM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define MD5SUM_PLATFORM_FREEBSD 1
    #define MD5SUM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define MD5SUM_PLATFORM_OPENBSD 1
    #define MD5SUM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define MD5SUM_PLATFORM_NETBSD  1
    #define MD5SUM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define MD5SUM_PLATFORM_POSIX   1
#else
    #define MD5SUM_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef MD5SUM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef MD5SUM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef MD5SUM_PLATFORM_NETBSD
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
#include <inttypes.h>
#include <errno.h>

#ifdef MD5SUM_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #define strcasecmp _stricmp
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <strings.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define MD5SUM_VERSION_STR "v1.0.0"

/** @brief MD5 digest size in bytes */
#define MD5_DIGEST_SIZE 16

/** @brief MD5 block size in bytes */
#define MD5_BLOCK_SIZE 64

/** @brief Read buffer size for file processing */
#define MD5SUM_READ_BUF_SIZE 65536

/** @brief Maximum line size for check file parsing */
#define MD5SUM_MAX_LINE 8192

/** @brief Maximum filename length in check file */
#define MD5SUM_MAX_FILENAME 4096

/** @brief Maximum digest string length (hex chars) */
#define MD5SUM_MAX_DIGEST_STR 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief MD5 context structure
 */
typedef struct {
    uint32_t state[4];      /**< Hash state (A, B, C, D) */
    uint64_t count;         /**< Total bytes processed */
    uint8_t  buffer[MD5_BLOCK_SIZE];  /**< Input buffer */
} md5_ctx;

/**
 * @brief Options structure for md5sum
 */
typedef struct {
    bool check;           /**< -c, --check: verify checksums */
    bool binary;          /**< -b, --binary: binary mode */
    bool text;            /**< -t, --text: text mode (default) */
    bool tag;             /**< --tag: BSD-style output */
    bool zero;            /**< -z, --zero: NUL-terminated output */
    bool ignore_missing;  /**< --ignore-missing: skip missing files */
    bool quiet;           /**< --quiet: suppress OK messages */
    bool status;          /**< --status: no output, status only */
    bool strict;          /**< --strict: fail on malformed lines */
    bool warn;            /**< -w, --warn: warn about malformed lines */
} md5sum_opts;

/********************************
 *    static prototypes
 ********************************/
static void   _md5sum_print_help(void);
static void   _md5sum_print_version(void);
static bool   _md5sum_streq(const char * a, const char * b);
static int    _md5sum_compute(const char * filename, uint8_t * digest);
static int    _md5sum_process_file(const char * filename, const md5sum_opts * opts);
static int    _md5sum_check_file(const char * filename, const md5sum_opts * opts);
static void   _md5sum_output_hex(const uint8_t * digest, size_t len);
static int    _md5sum_hex_val(char c);
static int    _md5sum_hex_to_bytes(const char * hex, uint8_t * out,
                                   size_t out_size, size_t * out_len);

/* MD5 algorithm functions */
static void _md5_init(md5_ctx * ctx);
static void _md5_transform(uint32_t state[4], const uint8_t block[64]);
static void _md5_update(md5_ctx * ctx, const uint8_t * data, size_t len);
static void _md5_final(md5_ctx * ctx, uint8_t * digest);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for md5sum_fputs / md5sum_fflush.
 *        Defaults to libc @c stdout .
 */
#ifndef md5sum_out_stream
    #define md5sum_out_stream stdout
#endif

/**
 * @brief Default error output stream.
 *        Defaults to libc @c stderr .
 */
#ifndef md5sum_err_stream
    #define md5sum_err_stream stderr
#endif

/**
 * @brief Formatted print to standard output (printf-compatible).
 */
#ifndef md5sum_printf
    #define md5sum_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to standard error.
 */
#ifndef md5sum_err_printf
    #define md5sum_err_printf(fmt, ...) fprintf(md5sum_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 */
#ifndef md5sum_putchar
    #define md5sum_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream
 */
#ifndef md5sum_fputs
    #define md5sum_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream
 */
#ifndef md5sum_fflush
    #define md5sum_fflush(stream) (void)fflush(stream)
#endif

/** @brief 32-bit left rotation */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/********************************
 *    static variables
 ********************************/

/** @brief MD5 round constants (K table) */
static const uint32_t md5_k[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

/** @brief MD5 shift amounts per round */
static const uint32_t md5_s[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the md5sum command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If -c/--check: read checksum files and verify
 *   3. Otherwise: compute MD5 checksums for input files
 *   4. Output results in the appropriate format
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error or checksum mismatch
 */
int main(int argc, char ** argv)
{
    md5sum_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.text = true;  /* default is text mode */

    int file_start = argc;

    /* Parse options */
    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_md5sum_streq(arg, "--")) {
            file_start = i + 1;
            break;
        }

        if (_md5sum_streq(arg, "-b") || _md5sum_streq(arg, "--binary")) {
            opts.binary = true;
            opts.text = false;
            continue;
        }

        if (_md5sum_streq(arg, "-c") || _md5sum_streq(arg, "--check")) {
            opts.check = true;
            continue;
        }

        if (_md5sum_streq(arg, "-t") || _md5sum_streq(arg, "--text")) {
            opts.text = true;
            opts.binary = false;
            continue;
        }

        if (_md5sum_streq(arg, "--tag")) {
            opts.tag = true;
            continue;
        }

        if (_md5sum_streq(arg, "-z") || _md5sum_streq(arg, "--zero")) {
            opts.zero = true;
            continue;
        }

        if (_md5sum_streq(arg, "--ignore-missing")) {
            opts.ignore_missing = true;
            continue;
        }

        if (_md5sum_streq(arg, "--quiet")) {
            opts.quiet = true;
            continue;
        }

        if (_md5sum_streq(arg, "--status")) {
            opts.status = true;
            continue;
        }

        if (_md5sum_streq(arg, "--strict")) {
            opts.strict = true;
            continue;
        }

        if (_md5sum_streq(arg, "-w") || _md5sum_streq(arg, "--warn")) {
            opts.warn = true;
            continue;
        }

        if (_md5sum_streq(arg, "--help")) {
            _md5sum_print_help();
            return 0;
        }

        if (_md5sum_streq(arg, "--version")) {
            _md5sum_print_version();
            return 0;
        }

        /* Not an option — treat as first file */
        if (arg[0] == '-' && arg[1] != '\0') {
            md5sum_err_printf("md5sum: unrecognized option '%s'\n", arg);
            md5sum_err_printf("Try 'md5sum --help' for more information.\n");
            return 1;
        }

        file_start = i;
        break;
    }

    int exit_code = 0;
    int num_files = argc - file_start;

    if (opts.check) {
        /* Check mode: read checksums from files and verify */
        if (num_files <= 0) {
            /* Read from stdin */
            int rc = _md5sum_check_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _md5sum_check_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }
    else {
        /* Compute mode */
        if (num_files <= 0) {
            /* Read from stdin */
            int rc = _md5sum_process_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _md5sum_process_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }

    md5sum_fflush(md5sum_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/* ---- MD5 algorithm ---- */

/**
 * @brief Initialize MD5 context
 * @param ctx  MD5 context (must not be NULL)
 */
static void _md5_init(md5_ctx * ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->count = 0;
}

/**
 * @brief MD5 core transformation: process one 64-byte block
 * @param state  MD5 state (4 x uint32)
 * @param block  64-byte input block
 */
static void _md5_transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t M[16];

    /* Decode little-endian 32-bit words */
    for (int i = 0; i < 16; i++) {
        M[i] = (uint32_t)block[i * 4]
             | ((uint32_t)block[i * 4 + 1] << 8)
             | ((uint32_t)block[i * 4 + 2] << 16)
             | ((uint32_t)block[i * 4 + 3] << 24);
    }

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = (uint32_t)i;
        }
        else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (uint32_t)((5 * i + 1) & 15);
        }
        else if (i < 48) {
            f = b ^ c ^ d;
            g = (uint32_t)((3 * i + 5) & 15);
        }
        else {
            f = c ^ (b | ~d);
            g = (uint32_t)((7 * i) & 15);
        }
        uint32_t temp = d;
        d = c;
        c = b;
        uint32_t val = a + f + md5_k[i] + M[g];
        b = b + ROTL32(val, md5_s[i]);
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

/**
 * @brief Update MD5 context with input data
 * @param ctx   MD5 context (must not be NULL)
 * @param data  Input data bytes
 * @param len   Number of bytes
 */
static void _md5_update(md5_ctx * ctx, const uint8_t * data, size_t len)
{
    size_t used = (size_t)(ctx->count % MD5_BLOCK_SIZE);
    ctx->count += len;

    if (used > 0) {
        size_t space = MD5_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(ctx->buffer + used, data, len);
            return;
        }
        memcpy(ctx->buffer + used, data, space);
        _md5_transform(ctx->state, ctx->buffer);
        data += space;
        len -= space;
    }

    while (len >= MD5_BLOCK_SIZE) {
        _md5_transform(ctx->state, data);
        data += MD5_BLOCK_SIZE;
        len -= MD5_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

/**
 * @brief Finalize MD5 hash and produce digest
 * @param ctx     MD5 context (must not be NULL)
 * @param digest  Output buffer (at least MD5_DIGEST_SIZE bytes)
 */
static void _md5_final(md5_ctx * ctx, uint8_t * digest)
{
    size_t used = (size_t)(ctx->count % MD5_BLOCK_SIZE);
    uint64_t bit_count = ctx->count * 8;

    /* Append padding: 0x80 followed by zeros */
    ctx->buffer[used++] = 0x80;
    if (used > 56) {
        memset(ctx->buffer + used, 0, MD5_BLOCK_SIZE - used);
        _md5_transform(ctx->state, ctx->buffer);
        used = 0;
    }
    memset(ctx->buffer + used, 0, 56 - used);

    /* Append length in bits (little-endian, 64-bit) */
    ctx->buffer[56] = (uint8_t)(bit_count);
    ctx->buffer[57] = (uint8_t)(bit_count >> 8);
    ctx->buffer[58] = (uint8_t)(bit_count >> 16);
    ctx->buffer[59] = (uint8_t)(bit_count >> 24);
    ctx->buffer[60] = (uint8_t)(bit_count >> 32);
    ctx->buffer[61] = (uint8_t)(bit_count >> 40);
    ctx->buffer[62] = (uint8_t)(bit_count >> 48);
    ctx->buffer[63] = (uint8_t)(bit_count >> 56);

    _md5_transform(ctx->state, ctx->buffer);

    /* Encode state to digest (little-endian) */
    for (int i = 0; i < 4; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

/* ---- output helpers ---- */

/**
 * @brief Output a byte array as lowercase hexadecimal
 * @param digest  Byte array
 * @param len     Number of bytes
 */
static void _md5sum_output_hex(const uint8_t * digest, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        md5sum_putchar(hex[digest[i] >> 4]);
        md5sum_putchar(hex[digest[i] & 0x0F]);
    }
}

/* ---- hex helpers ---- */

/**
 * @brief Convert a hex character to its integer value
 * @param c  Hex character (0-9, a-f, A-F)
 * @return Value 0-15, or -1 if not a hex digit
 */
static int _md5sum_hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Convert a hex string to a byte array
 * @param hex       Hex string (NUL-terminated)
 * @param out       Output buffer
 * @param out_size  Output buffer size
 * @param out_len   Receives number of decoded bytes
 * @return 0 on success, -1 on error
 */
static int _md5sum_hex_to_bytes(const char * hex, uint8_t * out,
                                size_t out_size, size_t * out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t n = len / 2;
    if (n > out_size) return -1;

    for (size_t i = 0; i < n; i++) {
        int hi = _md5sum_hex_val(hex[i * 2]);
        int lo = _md5sum_hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = n;
    return 0;
}

/* ---- string helper ---- */

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _md5sum_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/* ---- compute digest for a file ---- */

/**
 * @brief Compute MD5 digest of a file
 * @param filename  Path to file, or "-" for stdin
 * @param digest    Output buffer (MD5_DIGEST_SIZE bytes)
 * @return 0 on success, -1 on error
 */
static int _md5sum_compute(const char * filename, uint8_t * digest)
{
    FILE * fp;
    bool is_stdin = _md5sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
#ifdef MD5SUM_PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            return -1;
        }
    }

    md5_ctx ctx;
    _md5_init(&ctx);

    uint8_t buf[MD5SUM_READ_BUF_SIZE];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        _md5_update(&ctx, buf, n);
    }

    _md5_final(&ctx, digest);

    if (!is_stdin) {
        fclose(fp);
    }

    return 0;
}

/* ---- process a single file (compute mode) ---- */

/**
 * @brief Process a single file in compute mode
 * @param filename  File path or "-" for stdin
 * @param opts      Options structure
 * @return 0 on success, 1 on error
 */
static int _md5sum_process_file(const char * filename, const md5sum_opts * opts)
{
    uint8_t digest[MD5_DIGEST_SIZE];
    int rc = _md5sum_compute(filename, digest);
    if (rc != 0) {
        md5sum_err_printf("md5sum: %s: %s\n", filename, strerror(errno));
        return 1;
    }

    bool is_stdin = _md5sum_streq(filename, "-");

    if (opts->tag) {
        /* BSD-style tagged format: MD5 (filename) = hash */
        md5sum_fputs("MD5 (", md5sum_out_stream);
        if (is_stdin) {
            md5sum_fputs("stdin", md5sum_out_stream);
        }
        else {
            md5sum_fputs(filename, md5sum_out_stream);
        }
        md5sum_fputs(") = ", md5sum_out_stream);
        _md5sum_output_hex(digest, MD5_DIGEST_SIZE);
    }
    else {
        /* Default untagged format: hash  filename or hash *filename (binary) */
        _md5sum_output_hex(digest, MD5_DIGEST_SIZE);
        if (is_stdin) {
            md5sum_fputs("  -", md5sum_out_stream);
        }
        else {
            if (opts->binary) {
                md5sum_fputs(" *", md5sum_out_stream);
                md5sum_fputs(filename, md5sum_out_stream);
            }
            else {
                md5sum_fputs("  ", md5sum_out_stream);
                md5sum_fputs(filename, md5sum_out_stream);
            }
        }
    }

    if (opts->zero) {
        md5sum_putchar('\0');
    }
    else {
        md5sum_putchar('\n');
    }

    return 0;
}

/* ---- check file mode ---- */

/**
 * @brief Check checksums from a file
 * @param filename  Path to checksum file, or "-" for stdin
 * @param opts      Options structure
 * @return 0 if all OK, 1 if any failure or error
 */
static int _md5sum_check_file(const char * filename, const md5sum_opts * opts)
{
    FILE * fp;
    bool is_stdin = _md5sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (!fp) {
            md5sum_err_printf("md5sum: %s: %s\n", filename, strerror(errno));
            return 1;
        }
    }

    char line[MD5SUM_MAX_LINE];
    int exit_code = 0;
    int line_num = 0;
    bool any_parsed = false;

    while (fgets(line, (int)sizeof(line), fp)) {
        line_num++;
        size_t len = strlen(line);

        /* Strip trailing newline */
        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        }
        /* Strip trailing \r (for Windows / CRLF files) */
        if (len > 0 && line[len - 1] == '\r') {
            line[--len] = '\0';
        }

        if (len == 0) {
            continue;
        }

        /* Skip UTF-8 BOM on first line */
        if (line_num == 1 && len >= 3 &&
            (uint8_t)line[0] == 0xEF &&
            (uint8_t)line[1] == 0xBB &&
            (uint8_t)line[2] == 0xBF) {
            memmove(line, line + 3, len - 3 + 1);
            len -= 3;
            if (len == 0) continue;
        }

        char parsed_file[MD5SUM_MAX_FILENAME] = "";
        char parsed_digest[MD5SUM_MAX_DIGEST_STR] = "";
        bool is_tagged = false;

        /* Try tagged format: MD5 (filename) = digest */
        char * paren = strchr(line, '(');
        char * eq = strchr(line, '=');
        if (paren && eq && paren < eq) {
            /* Check that the tag starts with "MD5" */
            size_t alg_len = (size_t)(paren - line);
            while (alg_len > 0 && line[alg_len - 1] == ' ') alg_len--;
            if (alg_len == 3 && strncasecmp(line, "MD5", 3) == 0) {
                is_tagged = true;
            }
        }

        if (is_tagged) {
            /* Extract filename from between parentheses */
            char * close_paren = strchr(paren + 1, ')');
            if (!close_paren || close_paren > eq) {
                if (opts->warn) {
                    md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                      filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t file_len = (size_t)(close_paren - paren - 1);
            if (file_len >= sizeof(parsed_file)) {
                if (opts->warn) {
                    md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                      filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }
            memcpy(parsed_file, paren + 1, file_len);
            parsed_file[file_len] = '\0';

            /* Extract digest after '=' */
            char * digest_start = eq + 1;
            while (*digest_start == ' ') digest_start++;

            strncpy(parsed_digest, digest_start, sizeof(parsed_digest) - 1);
            parsed_digest[sizeof(parsed_digest) - 1] = '\0';

            /* Trim trailing whitespace */
            size_t dl = strlen(parsed_digest);
            while (dl > 0 && (parsed_digest[dl - 1] == ' ' || parsed_digest[dl - 1] == '\r')) {
                parsed_digest[--dl] = '\0';
            }
        }
        else {
            /* Untagged format: digest  filename or digest * filename */
            char * space = strchr(line, ' ');
            if (!space) {
                if (opts->warn) {
                    md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                      filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t digest_len = (size_t)(space - line);
            if (digest_len >= sizeof(parsed_digest)) {
                if (opts->warn) {
                    md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                      filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }
            memcpy(parsed_digest, line, digest_len);
            parsed_digest[digest_len] = '\0';

            /* Skip binary/text indicator and spaces */
            char * file_start = space + 1;
            while (*file_start == ' ') file_start++;
            if (*file_start == '*') {
                file_start++;  /* binary mode indicator */
            }

            strncpy(parsed_file, file_start, sizeof(parsed_file) - 1);
            parsed_file[sizeof(parsed_file) - 1] = '\0';
        }

        /* Validate digest is exactly 32 hex chars (MD5 = 16 bytes) */
        if (strlen(parsed_digest) != 32) {
            if (opts->warn) {
                md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                  filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        uint8_t expected[MD5_DIGEST_SIZE];
        size_t expected_len = 0;
        if (_md5sum_hex_to_bytes(parsed_digest, expected, sizeof(expected), &expected_len) != 0) {
            if (opts->warn) {
                md5sum_err_printf("md5sum: %s: %d: improperly formatted line\n",
                                  filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        any_parsed = true;

        /* Compute actual digest */
        uint8_t actual[MD5_DIGEST_SIZE];
        int rc = _md5sum_compute(parsed_file, actual);
        if (rc != 0) {
            if (!opts->ignore_missing) {
                md5sum_err_printf("md5sum: %s: %s\n", parsed_file, strerror(errno));
                exit_code = 1;
            }
            continue;
        }

        bool match = (memcmp(actual, expected, MD5_DIGEST_SIZE) == 0);

        if (!opts->status) {
            if (match) {
                if (!opts->quiet) {
                    md5sum_printf("%s: OK\n", parsed_file);
                }
            }
            else {
                md5sum_printf("%s: FAILED\n", parsed_file);
                exit_code = 1;
            }
        }
        else {
            if (!match) {
                exit_code = 1;
            }
        }
    }

    if (!any_parsed && exit_code == 0) {
        md5sum_err_printf("md5sum: %s: no properly formatted MD5 checksum lines found\n",
                          filename);
        exit_code = 1;
    }

    if (!is_stdin) {
        fclose(fp);
    }

    return exit_code;
}

/* ---- help / version ---- */

/**
 * @brief Print usage/help information
 */
static void _md5sum_print_help(void)
{
    md5sum_printf(
        "Usage: md5sum [OPTION]... [FILE]...\n"
        "Print or check MD5 (128-bit) checksums.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "  -b, --binary          read in binary mode\n"
        "  -c, --check           read MD5 sums from the FILEs and check them\n"
        "      --tag             create a BSD-style checksum\n"
        "  -t, --text            read in text mode (default)\n"
        "  -z, --zero            end each output line with NUL, not newline,\n"
        "                        and disable file name escaping\n"
        "\n"
        "The following five options are useful only when verifying checksums:\n"
        "      --ignore-missing  don't fail or report status for missing files\n"
        "      --quiet           don't print OK for each successfully verified file\n"
        "      --status          don't output anything, status code shows success\n"
        "      --strict          exit non-zero for improperly formatted checksum lines\n"
        "  -w, --warn            warn about improperly formatted checksum lines\n"
        "\n"
        "      --help            display this help and exit\n"
        "      --version         output version information and exit\n"
        "\n"
        "The sums are computed as described in RFC 1321.  When checking, the input\n"
        "should be a former output of this program.  The default mode is to print a\n"
        "line with: checksum, a space, a character indicating input mode ('*' for binary,\n"
        "' ' for text or where binary is insignificant), and name for each FILE.\n"
        "\n"
        "Note: There is no difference between binary mode and text mode on systems.\n"
    );
}

/**
 * @brief Print version information
 */
static void _md5sum_print_version(void)
{
    md5sum_printf("md5sum %s\n", MD5SUM_VERSION_STR);
    md5sum_printf("%s", "Copyright (C) 2025-2026 Yezc/md5sum\n");
    md5sum_printf("%s", "License MIT: <https://mit-license.org/>\n");
    md5sum_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    md5sum_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
