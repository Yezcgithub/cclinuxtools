/**
 * @file sha224sum.c
 * @brief Cross-platform implementation of the sha224sum command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils sha224sum(1).
 *
 * Key behaviors:
 *   - -b, --binary:   read in binary mode
 *   - -c, --check:    read SHA224 sums from files and check them
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
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o sha224sum.exe sha224sum.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -o sha224sum sha224sum.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -o sha224sum sha224sum.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o sha224sum sha224sum.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o sha224sum sha224sum.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -o sha224sum sha224sum.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/sha224sum>
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
    #define SHA224SUM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define SHA224SUM_PLATFORM_LINUX   1
    #define SHA224SUM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define SHA224SUM_PLATFORM_MACOS   1
    #define SHA224SUM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define SHA224SUM_PLATFORM_FREEBSD 1
    #define SHA224SUM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define SHA224SUM_PLATFORM_OPENBSD 1
    #define SHA224SUM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define SHA224SUM_PLATFORM_NETBSD  1
    #define SHA224SUM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define SHA224SUM_PLATFORM_POSIX   1
#else
    #define SHA224SUM_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef SHA224SUM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef SHA224SUM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef SHA224SUM_PLATFORM_NETBSD
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

#ifdef SHA224SUM_PLATFORM_WINDOWS
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
#define SHA224SUM_VERSION_STR "v1.0.0"

/** @brief SHA-224 digest size in bytes */
#define SHA224_DIGEST_SIZE 28

/** @brief SHA-224 block size in bytes (same as SHA-256) */
#define SHA224_BLOCK_SIZE 64

/** @brief Read buffer size for file processing */
#define SHA224SUM_READ_BUF_SIZE 65536

/** @brief Maximum line size for check file parsing */
#define SHA224SUM_MAX_LINE 8192

/** @brief Maximum filename length in check file */
#define SHA224SUM_MAX_FILENAME 4096

/** @brief Maximum digest string length (hex chars) */
#define SHA224SUM_MAX_DIGEST_STR 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief SHA-224 context structure
 *
 * SHA-224 uses the same state layout as SHA-256 (8 x uint32_t),
 * but with different initial values and the final digest is
 * truncated to 28 bytes (H0..H6).
 */
typedef struct {
    uint32_t state[8];      /**< Hash state (H0..H7) */
    uint64_t count;         /**< Total bytes processed */
    uint8_t  buffer[SHA224_BLOCK_SIZE];  /**< Input buffer */
} sha224_ctx;

/**
 * @brief Options structure for sha224sum
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
} sha224sum_opts;

/********************************
 *    static prototypes
 ********************************/
static void   _sha224sum_print_help(void);
static void   _sha224sum_print_version(void);
static bool   _sha224sum_streq(const char * a, const char * b);
static int    _sha224sum_compute(const char * filename, uint8_t * digest);
static int    _sha224sum_process_file(const char * filename, const sha224sum_opts * opts);
static int    _sha224sum_check_file(const char * filename, const sha224sum_opts * opts);
static void   _sha224sum_output_hex(const uint8_t * digest, size_t len);
static int    _sha224sum_hex_val(char c);
static int    _sha224sum_hex_to_bytes(const char * hex, uint8_t * out,
                                      size_t out_size, size_t * out_len);

/* SHA-224 algorithm functions */
static void _sha224_init(sha224_ctx * ctx);
static void _sha224_transform(uint32_t state[8], const uint8_t block[64]);
static void _sha224_update(sha224_ctx * ctx, const uint8_t * data, size_t len);
static void _sha224_final(sha224_ctx * ctx, uint8_t * digest);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for sha224sum_fputs / sha224sum_fflush.
 *        Defaults to libc @c stdout .
 */
#ifndef sha224sum_out_stream
    #define sha224sum_out_stream stdout
#endif

/**
 * @brief Default error output stream.
 *        Defaults to libc @c stderr .
 */
#ifndef sha224sum_err_stream
    #define sha224sum_err_stream stderr
#endif

/**
 * @brief Formatted print to standard output (printf-compatible).
 */
#ifndef sha224sum_printf
    #define sha224sum_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to standard error.
 */
#ifndef sha224sum_err_printf
    #define sha224sum_err_printf(fmt, ...) fprintf(sha224sum_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 */
#ifndef sha224sum_putchar
    #define sha224sum_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream
 */
#ifndef sha224sum_fputs
    #define sha224sum_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream
 */
#ifndef sha224sum_fflush
    #define sha224sum_fflush(stream) (void)fflush(stream)
#endif

/** @brief 32-bit right rotation */
#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/** @brief SHA-224 Ch function: Choice (same as SHA-256) */
#define SHA224_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))

/** @brief SHA-224 Maj function: Majority (same as SHA-256) */
#define SHA224_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/** @brief SHA-224 Sigma0 (upper-case): big sigma 0 (same as SHA-256) */
#define SHA224_SIGMA0(x)  (ROTR32((x), 2)  ^ ROTR32((x), 13) ^ ROTR32((x), 22))

/** @brief SHA-224 Sigma1 (upper-case): big sigma 1 (same as SHA-256) */
#define SHA224_SIGMA1(x)  (ROTR32((x), 6)  ^ ROTR32((x), 11) ^ ROTR32((x), 25))

/** @brief SHA-224 sigma0 (lower-case): small sigma 0 (same as SHA-256) */
#define SHA224_sigma0(x)  (ROTR32((x), 7)  ^ ROTR32((x), 18) ^ ((x) >> 3))

/** @brief SHA-224 sigma1 (lower-case): small sigma 1 (same as SHA-256) */
#define SHA224_sigma1(x)  (ROTR32((x), 17) ^ ROTR32((x), 19) ^ ((x) >> 10))

/********************************
 *    static variables
 ********************************/

/** @brief SHA-224 round constants (K table) — same as SHA-256:
 *         first 32 bits of the fractional parts of the cube roots
 *         of the first 64 primes (2..311). */
static const uint32_t sha224_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the sha224sum command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If -c/--check: read checksum files and verify
 *   3. Otherwise: compute SHA-224 checksums for input files
 *   4. Output results in the appropriate format
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error or checksum mismatch
 */
int main(int argc, char ** argv)
{
    sha224sum_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.text = true;  /* default is text mode */

    int file_start = argc;

    /* Parse options */
    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_sha224sum_streq(arg, "--")) {
            file_start = i + 1;
            break;
        }

        if (_sha224sum_streq(arg, "-b") || _sha224sum_streq(arg, "--binary")) {
            opts.binary = true;
            opts.text = false;
            continue;
        }

        if (_sha224sum_streq(arg, "-c") || _sha224sum_streq(arg, "--check")) {
            opts.check = true;
            continue;
        }

        if (_sha224sum_streq(arg, "-t") || _sha224sum_streq(arg, "--text")) {
            opts.text = true;
            opts.binary = false;
            continue;
        }

        if (_sha224sum_streq(arg, "--tag")) {
            opts.tag = true;
            continue;
        }

        if (_sha224sum_streq(arg, "-z") || _sha224sum_streq(arg, "--zero")) {
            opts.zero = true;
            continue;
        }

        if (_sha224sum_streq(arg, "--ignore-missing")) {
            opts.ignore_missing = true;
            continue;
        }

        if (_sha224sum_streq(arg, "--quiet")) {
            opts.quiet = true;
            continue;
        }

        if (_sha224sum_streq(arg, "--status")) {
            opts.status = true;
            continue;
        }

        if (_sha224sum_streq(arg, "--strict")) {
            opts.strict = true;
            continue;
        }

        if (_sha224sum_streq(arg, "-w") || _sha224sum_streq(arg, "--warn")) {
            opts.warn = true;
            continue;
        }

        if (_sha224sum_streq(arg, "--help")) {
            _sha224sum_print_help();
            return 0;
        }

        if (_sha224sum_streq(arg, "--version")) {
            _sha224sum_print_version();
            return 0;
        }

        /* Not an option — treat as first file */
        if (arg[0] == '-' && arg[1] != '\0') {
            sha224sum_err_printf("sha224sum: unrecognized option '%s'\n", arg);
            sha224sum_err_printf("Try 'sha224sum --help' for more information.\n");
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
            int rc = _sha224sum_check_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _sha224sum_check_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }
    else {
        /* Compute mode */
        if (num_files <= 0) {
            /* Read from stdin */
            int rc = _sha224sum_process_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _sha224sum_process_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }

    sha224sum_fflush(sha224sum_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/* ---- SHA-224 algorithm ---- */

/**
 * @brief Initialize SHA-224 context
 * @param ctx  SHA-224 context (must not be NULL)
 *
 * FIPS 180-4 section 5.3.2: initial hash values for SHA-224.
 * These are the first 32 bits of the fractional parts of the
 * square roots of the 9th through 16th primes (23, 29, 31, 37,
 * 41, 43, 47, 53).
 */
static void _sha224_init(sha224_ctx * ctx)
{
    ctx->state[0] = 0xc1059ed8;
    ctx->state[1] = 0x367cd507;
    ctx->state[2] = 0x3070dd17;
    ctx->state[3] = 0xf70e5939;
    ctx->state[4] = 0xffc00b31;
    ctx->state[5] = 0x68581511;
    ctx->state[6] = 0x64f98fa7;
    ctx->state[7] = 0xbefa4fa4;
    ctx->count = 0;
}

/**
 * @brief SHA-224 core transformation: process one 64-byte block
 *
 * SHA-224 uses the same transformation function as SHA-256.
 *
 * @param state  SHA-224 state (8 x uint32)
 * @param block  64-byte input block
 */
static void _sha224_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t w[64];

    /* Prepare message schedule: W[0..15] from the block (big-endian) */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4]     << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    }

    /* Extend to W[16..63] */
    for (int i = 16; i < 64; i++) {
        w[i] = SHA224_sigma1(w[i - 2]) + w[i - 7]
             + SHA224_sigma0(w[i - 15]) + w[i - 16];
    }

    /* Compression function — 64 rounds */
    for (int i = 0; i < 64; i++) {
        uint32_t T1 = h + SHA224_SIGMA1(e) + SHA224_CH(e, f, g)
                    + sha224_k[i] + w[i];
        uint32_t T2 = SHA224_SIGMA0(a) + SHA224_MAJ(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    /* Add compressed chunk to current hash value */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

/**
 * @brief Update SHA-224 context with input data
 * @param ctx   SHA-224 context (must not be NULL)
 * @param data  Input data bytes
 * @param len   Number of bytes
 */
static void _sha224_update(sha224_ctx * ctx, const uint8_t * data, size_t len)
{
    size_t used = (size_t)(ctx->count % SHA224_BLOCK_SIZE);
    ctx->count += len;

    if (used > 0) {
        size_t space = SHA224_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(ctx->buffer + used, data, len);
            return;
        }
        memcpy(ctx->buffer + used, data, space);
        _sha224_transform(ctx->state, ctx->buffer);
        data += space;
        len -= space;
    }

    while (len >= SHA224_BLOCK_SIZE) {
        _sha224_transform(ctx->state, data);
        data += SHA224_BLOCK_SIZE;
        len -= SHA224_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

/**
 * @brief Finalize SHA-224 hash and produce digest
 * @param ctx     SHA-224 context (must not be NULL)
 * @param digest  Output buffer (at least SHA224_DIGEST_SIZE bytes)
 *
 * SHA-224 output is truncated to 28 bytes (H0..H6, 7 state variables).
 */
static void _sha224_final(sha224_ctx * ctx, uint8_t * digest)
{
    size_t used = (size_t)(ctx->count % SHA224_BLOCK_SIZE);
    uint64_t bit_count = ctx->count * 8;

    /* Append padding: 0x80 followed by zeros */
    ctx->buffer[used++] = 0x80;
    if (used > 56) {
        memset(ctx->buffer + used, 0, SHA224_BLOCK_SIZE - used);
        _sha224_transform(ctx->state, ctx->buffer);
        used = 0;
    }
    memset(ctx->buffer + used, 0, 56 - used);

    /* Append length in bits (big-endian, 64-bit) */
    ctx->buffer[56] = (uint8_t)(bit_count >> 56);
    ctx->buffer[57] = (uint8_t)(bit_count >> 48);
    ctx->buffer[58] = (uint8_t)(bit_count >> 40);
    ctx->buffer[59] = (uint8_t)(bit_count >> 32);
    ctx->buffer[60] = (uint8_t)(bit_count >> 24);
    ctx->buffer[61] = (uint8_t)(bit_count >> 16);
    ctx->buffer[62] = (uint8_t)(bit_count >> 8);
    ctx->buffer[63] = (uint8_t)(bit_count);

    _sha224_transform(ctx->state, ctx->buffer);

    /* Encode state to digest (big-endian) — SHA-224: only first 7 words (28 bytes) */
    for (int i = 0; i < 7; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* ---- output helpers ---- */

/**
 * @brief Output a byte array as lowercase hexadecimal
 * @param digest  Byte array
 * @param len     Number of bytes
 */
static void _sha224sum_output_hex(const uint8_t * digest, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        sha224sum_putchar(hex[digest[i] >> 4]);
        sha224sum_putchar(hex[digest[i] & 0x0F]);
    }
}

/* ---- hex helpers ---- */

/**
 * @brief Convert a hex character to its integer value
 * @param c  Hex character (0-9, a-f, A-F)
 * @return Value 0-15, or -1 if not a hex digit
 */
static int _sha224sum_hex_val(char c)
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
static int _sha224sum_hex_to_bytes(const char * hex, uint8_t * out,
                                   size_t out_size, size_t * out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t n = len / 2;
    if (n > out_size) return -1;

    for (size_t i = 0; i < n; i++) {
        int hi = _sha224sum_hex_val(hex[i * 2]);
        int lo = _sha224sum_hex_val(hex[i * 2 + 1]);
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
static bool _sha224sum_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/* ---- compute digest for a file ---- */

/**
 * @brief Compute SHA-224 digest of a file
 * @param filename  Path to file, or "-" for stdin
 * @param digest    Output buffer (SHA224_DIGEST_SIZE bytes)
 * @return 0 on success, -1 on error
 */
static int _sha224sum_compute(const char * filename, uint8_t * digest)
{
    FILE * fp;
    bool is_stdin = _sha224sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
#ifdef SHA224SUM_PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            return -1;
        }
    }

    sha224_ctx ctx;
    _sha224_init(&ctx);

    uint8_t buf[SHA224SUM_READ_BUF_SIZE];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        _sha224_update(&ctx, buf, n);
    }

    _sha224_final(&ctx, digest);

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
static int _sha224sum_process_file(const char * filename, const sha224sum_opts * opts)
{
    uint8_t digest[SHA224_DIGEST_SIZE];
    int rc = _sha224sum_compute(filename, digest);
    if (rc != 0) {
        sha224sum_err_printf("sha224sum: %s: %s\n", filename, strerror(errno));
        return 1;
    }

    bool is_stdin = _sha224sum_streq(filename, "-");

    if (opts->tag) {
        /* BSD-style tagged format: SHA224 (filename) = hash */
        sha224sum_fputs("SHA224 (", sha224sum_out_stream);
        if (is_stdin) {
            sha224sum_fputs("stdin", sha224sum_out_stream);
        }
        else {
            sha224sum_fputs(filename, sha224sum_out_stream);
        }
        sha224sum_fputs(") = ", sha224sum_out_stream);
        _sha224sum_output_hex(digest, SHA224_DIGEST_SIZE);
    }
    else {
        /* Default untagged format: hash  filename or hash *filename (binary) */
        _sha224sum_output_hex(digest, SHA224_DIGEST_SIZE);
        if (is_stdin) {
            sha224sum_fputs("  -", sha224sum_out_stream);
        }
        else {
            if (opts->binary) {
                sha224sum_fputs(" *", sha224sum_out_stream);
                sha224sum_fputs(filename, sha224sum_out_stream);
            }
            else {
                sha224sum_fputs("  ", sha224sum_out_stream);
                sha224sum_fputs(filename, sha224sum_out_stream);
            }
        }
    }

    if (opts->zero) {
        sha224sum_putchar('\0');
    }
    else {
        sha224sum_putchar('\n');
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
static int _sha224sum_check_file(const char * filename, const sha224sum_opts * opts)
{
    FILE * fp;
    bool is_stdin = _sha224sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (!fp) {
            sha224sum_err_printf("sha224sum: %s: %s\n", filename, strerror(errno));
            return 1;
        }
    }

    char line[SHA224SUM_MAX_LINE];
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

        char parsed_file[SHA224SUM_MAX_FILENAME] = "";
        char parsed_digest[SHA224SUM_MAX_DIGEST_STR] = "";
        bool is_tagged = false;

        /* Try tagged format: SHA224 (filename) = digest */
        char * paren = strchr(line, '(');
        char * eq = strchr(line, '=');
        if (paren && eq && paren < eq) {
            /* Check that the tag starts with "SHA224" */
            size_t alg_len = (size_t)(paren - line);
            while (alg_len > 0 && line[alg_len - 1] == ' ') alg_len--;
            if (alg_len == 6 && strncasecmp(line, "SHA224", 6) == 0) {
                is_tagged = true;
            }
        }

        if (is_tagged) {
            /* Extract filename from between parentheses */
            char * close_paren = strchr(paren + 1, ')');
            if (!close_paren || close_paren > eq) {
                if (opts->warn) {
                    sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
                                         filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t file_len = (size_t)(close_paren - paren - 1);
            if (file_len >= sizeof(parsed_file)) {
                if (opts->warn) {
                    sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
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
                    sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
                                         filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t digest_len = (size_t)(space - line);
            if (digest_len >= sizeof(parsed_digest)) {
                if (opts->warn) {
                    sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
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

        /* Validate digest is exactly 56 hex chars (SHA-224 = 28 bytes) */
        if (strlen(parsed_digest) != 56) {
            if (opts->warn) {
                sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
                                     filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        uint8_t expected[SHA224_DIGEST_SIZE];
        size_t expected_len = 0;
        if (_sha224sum_hex_to_bytes(parsed_digest, expected, sizeof(expected), &expected_len) != 0) {
            if (opts->warn) {
                sha224sum_err_printf("sha224sum: %s: %d: improperly formatted line\n",
                                     filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        any_parsed = true;

        /* Compute actual digest */
        uint8_t actual[SHA224_DIGEST_SIZE];
        int rc = _sha224sum_compute(parsed_file, actual);
        if (rc != 0) {
            if (!opts->ignore_missing) {
                sha224sum_err_printf("sha224sum: %s: %s\n", parsed_file, strerror(errno));
                exit_code = 1;
            }
            continue;
        }

        bool match = (memcmp(actual, expected, SHA224_DIGEST_SIZE) == 0);

        if (!opts->status) {
            if (match) {
                if (!opts->quiet) {
                    sha224sum_printf("%s: OK\n", parsed_file);
                }
            }
            else {
                sha224sum_printf("%s: FAILED\n", parsed_file);
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
        sha224sum_err_printf("sha224sum: %s: no properly formatted SHA224 checksum lines found\n",
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
static void _sha224sum_print_help(void)
{
    sha224sum_printf(
        "Usage: sha224sum [OPTION]... [FILE]...\n"
        "Print or check SHA224 (224-bit) checksums.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "  -b, --binary          read in binary mode\n"
        "  -c, --check           read SHA224 sums from the FILEs and check them\n"
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
        "The sums are computed as described in FIPS-180-4.  When checking, the input\n"
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
static void _sha224sum_print_version(void)
{
    sha224sum_printf("sha224sum %s\n", SHA224SUM_VERSION_STR);
    sha224sum_printf("%s", "Copyright (C) 2025-2026 Yezc/sha224sum\n");
    sha224sum_printf("%s", "License MIT: <https://mit-license.org/>\n");
    sha224sum_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    sha224sum_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
