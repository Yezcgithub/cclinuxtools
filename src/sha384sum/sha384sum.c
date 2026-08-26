/**
 * @file sha384sum.c
 * @brief Cross-platform implementation of the sha384sum command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils sha384sum(1).
 *
 * Key behaviors:
 *   - -b, --binary:   read in binary mode
 *   - -c, --check:    read SHA384 sums from files and check them
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
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o sha384sum.exe sha384sum.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -o sha384sum sha384sum.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -o sha384sum sha384sum.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o sha384sum sha384sum.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o sha384sum sha384sum.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -o sha384sum sha384sum.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright © 2025-2026 <Yezc/sha384sum>
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
    #define SHA384SUM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define SHA384SUM_PLATFORM_LINUX   1
    #define SHA384SUM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define SHA384SUM_PLATFORM_MACOS   1
    #define SHA384SUM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define SHA384SUM_PLATFORM_FREEBSD 1
    #define SHA384SUM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define SHA384SUM_PLATFORM_OPENBSD 1
    #define SHA384SUM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define SHA384SUM_PLATFORM_NETBSD  1
    #define SHA384SUM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define SHA384SUM_PLATFORM_POSIX   1
#else
    #define SHA384SUM_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef SHA384SUM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef SHA384SUM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef SHA384SUM_PLATFORM_NETBSD
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

#ifdef SHA384SUM_PLATFORM_WINDOWS
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
#define SHA384SUM_VERSION_STR "v1.0.0"

/** @brief SHA-384 digest size in bytes */
#define SHA384_DIGEST_SIZE 48

/** @brief SHA-384 block size in bytes */
#define SHA384_BLOCK_SIZE 128

/** @brief Read buffer size for file processing */
#define SHA384SUM_READ_BUF_SIZE 65536

/** @brief Maximum line size for check file parsing */
#define SHA384SUM_MAX_LINE 8192

/** @brief Maximum filename length in check file */
#define SHA384SUM_MAX_FILENAME 4096

/** @brief Maximum digest string length (hex chars) */
#define SHA384SUM_MAX_DIGEST_STR 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief SHA-384 context structure
 */
typedef struct {
    uint64_t state[8];      /**< Hash state (H0..H7) */
    uint64_t count;         /**< Total bytes processed */
    uint8_t  buffer[SHA384_BLOCK_SIZE];  /**< Input buffer */
} sha384_ctx;

/**
 * @brief Options structure for sha384sum
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
} sha384sum_opts;

/********************************
 *    static prototypes
 ********************************/
static void   _sha384sum_print_help(void);
static void   _sha384sum_print_version(void);
static bool   _sha384sum_streq(const char * a, const char * b);
static int    _sha384sum_compute(const char * filename, uint8_t * digest);
static int    _sha384sum_process_file(const char * filename, const sha384sum_opts * opts);
static int    _sha384sum_check_file(const char * filename, const sha384sum_opts * opts);
static void   _sha384sum_output_hex(const uint8_t * digest, size_t len);
static int    _sha384sum_hex_val(char c);
static int    _sha384sum_hex_to_bytes(const char * hex, uint8_t * out,
                                      size_t out_size, size_t * out_len);

/* SHA-384 algorithm functions */
static void _sha384_init(sha384_ctx * ctx);
static void _sha384_transform(uint64_t state[8], const uint8_t block[128]);
static void _sha384_update(sha384_ctx * ctx, const uint8_t * data, size_t len);
static void _sha384_final(sha384_ctx * ctx, uint8_t * digest);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream for sha384sum_fputs / sha384sum_fflush.
 *        Defaults to libc @c stdout .
 */
#ifndef sha384sum_out_stream
    #define sha384sum_out_stream stdout
#endif

/**
 * @brief Default error output stream.
 *        Defaults to libc @c stderr .
 */
#ifndef sha384sum_err_stream
    #define sha384sum_err_stream stderr
#endif

/**
 * @brief Formatted print to standard output (printf-compatible).
 */
#ifndef sha384sum_printf
    #define sha384sum_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to standard error.
 */
#ifndef sha384sum_err_printf
    #define sha384sum_err_printf(fmt, ...) fprintf(sha384sum_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 * @param ch  Character (promoted from @c unsigned char to @c int ).
 */
#ifndef sha384sum_putchar
    #define sha384sum_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Write a NUL-terminated string to a stdio stream.
 * @param str     NUL-terminated string; must not be NULL.
 * @param stream  stdio stream
 */
#ifndef sha384sum_fputs
    #define sha384sum_fputs(str, stream) (void)fputs((str), (stream))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 * @param stream  stdio stream
 */
#ifndef sha384sum_fflush
    #define sha384sum_fflush(stream) (void)fflush(stream)
#endif

/** @brief 64-bit right rotation */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/** @brief SHA-512/384 Ch function: Choice */
#define SHA512_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))

/** @brief SHA-512/384 Maj function: Majority */
#define SHA512_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/** @brief SHA-512/384 Sigma0 (upper-case): big sigma 0 */
#define SHA512_SIGMA0(x)  (ROTR64((x), 28) ^ ROTR64((x), 34) ^ ROTR64((x), 39))

/** @brief SHA-512/384 Sigma1 (upper-case): big sigma 1 */
#define SHA512_SIGMA1(x)  (ROTR64((x), 14) ^ ROTR64((x), 18) ^ ROTR64((x), 41))

/** @brief SHA-512/384 sigma0 (lower-case): small sigma 0 */
#define SHA512_sigma0(x)  (ROTR64((x), 1)  ^ ROTR64((x), 8)  ^ ((x) >> 7))

/** @brief SHA-512/384 sigma1 (lower-case): small sigma 1 */
#define SHA512_sigma1(x)  (ROTR64((x), 19) ^ ROTR64((x), 61) ^ ((x) >> 6))

/********************************
 *    static variables
 ********************************/

/** @brief SHA-512 round constants (K table) — first 64 bits of the fractional
 *         parts of the cube roots of the first 80 primes. */
static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the sha384sum command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If -c/--check: read checksum files and verify
 *   3. Otherwise: compute SHA-384 checksums for input files
 *   4. Output results in the appropriate format
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error or checksum mismatch
 */
int main(int argc, char ** argv)
{
    sha384sum_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.text = true;  /* default is text mode */

    int file_start = argc;

    /* Parse options */
    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_sha384sum_streq(arg, "--")) {
            file_start = i + 1;
            break;
        }

        if (_sha384sum_streq(arg, "-b") || _sha384sum_streq(arg, "--binary")) {
            opts.binary = true;
            opts.text = false;
            continue;
        }

        if (_sha384sum_streq(arg, "-c") || _sha384sum_streq(arg, "--check")) {
            opts.check = true;
            continue;
        }

        if (_sha384sum_streq(arg, "-t") || _sha384sum_streq(arg, "--text")) {
            opts.text = true;
            opts.binary = false;
            continue;
        }

        if (_sha384sum_streq(arg, "--tag")) {
            opts.tag = true;
            continue;
        }

        if (_sha384sum_streq(arg, "-z") || _sha384sum_streq(arg, "--zero")) {
            opts.zero = true;
            continue;
        }

        if (_sha384sum_streq(arg, "--ignore-missing")) {
            opts.ignore_missing = true;
            continue;
        }

        if (_sha384sum_streq(arg, "--quiet")) {
            opts.quiet = true;
            continue;
        }

        if (_sha384sum_streq(arg, "--status")) {
            opts.status = true;
            continue;
        }

        if (_sha384sum_streq(arg, "--strict")) {
            opts.strict = true;
            continue;
        }

        if (_sha384sum_streq(arg, "-w") || _sha384sum_streq(arg, "--warn")) {
            opts.warn = true;
            continue;
        }

        if (_sha384sum_streq(arg, "--help")) {
            _sha384sum_print_help();
            return 0;
        }

        if (_sha384sum_streq(arg, "--version")) {
            _sha384sum_print_version();
            return 0;
        }

        /* Not an option — treat as first file */
        if (arg[0] == '-' && arg[1] != '\0') {
            sha384sum_err_printf("sha384sum: unrecognized option '%s'\n", arg);
            sha384sum_err_printf("Try 'sha384sum --help' for more information.\n");
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
            int rc = _sha384sum_check_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _sha384sum_check_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }
    else {
        /* Compute mode */
        if (num_files <= 0) {
            /* Read from stdin */
            int rc = _sha384sum_process_file("-", &opts);
            if (rc != 0) exit_code = 1;
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _sha384sum_process_file(argv[i], &opts);
                if (rc != 0) exit_code = 1;
            }
        }
    }

    sha384sum_fflush(sha384sum_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/* ---- SHA-384 algorithm ---- */

/**
 * @brief Initialize SHA-384 context
 * @param ctx  SHA-384 context (must not be NULL)
 */
static void _sha384_init(sha384_ctx * ctx)
{
    /* FIPS 180-4 5.3.4: initial hash values for SHA-384
     * (first 64 bits of the fractional parts of the square roots
     *  of the 9th through 16th primes 23..53). */
    ctx->state[0] = 0xcbbb9d5dc1059ed8;
    ctx->state[1] = 0x629a292a367cd507;
    ctx->state[2] = 0x9159015a3070dd17;
    ctx->state[3] = 0x152fecd8f70e5939;
    ctx->state[4] = 0x67332667ffc00b31;
    ctx->state[5] = 0x8eb44a8768581511;
    ctx->state[6] = 0xdb0c2e0d64f98fa7;
    ctx->state[7] = 0x47b5481dbefa4fa4;
    ctx->count = 0;
}

/**
 * @brief SHA-384 core transformation: process one 128-byte block
 * @param state  SHA-384 state (8 x uint64)
 * @param block  128-byte input block
 */
static void _sha384_transform(uint64_t state[8], const uint8_t block[128])
{
    uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint64_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint64_t w[80];

    /* Prepare message schedule: W[0..15] from the block (big-endian, 64-bit words) */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint64_t)block[i * 8]     << 56)
             | ((uint64_t)block[i * 8 + 1] << 48)
             | ((uint64_t)block[i * 8 + 2] << 40)
             | ((uint64_t)block[i * 8 + 3] << 32)
             | ((uint64_t)block[i * 8 + 4] << 24)
             | ((uint64_t)block[i * 8 + 5] << 16)
             | ((uint64_t)block[i * 8 + 6] << 8)
             | ((uint64_t)block[i * 8 + 7]);
    }

    /* Extend to W[16..79] */
    for (int i = 16; i < 80; i++) {
        w[i] = SHA512_sigma1(w[i - 2]) + w[i - 7]
             + SHA512_sigma0(w[i - 15]) + w[i - 16];
    }

    /* Compression function — 80 rounds */
    for (int i = 0; i < 80; i++) {
        uint64_t T1 = h + SHA512_SIGMA1(e) + SHA512_CH(e, f, g)
                    + sha512_k[i] + w[i];
        uint64_t T2 = SHA512_SIGMA0(a) + SHA512_MAJ(a, b, c);

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
 * @brief Update SHA-384 context with input data
 * @param ctx   SHA-384 context (must not be NULL)
 * @param data  Input data bytes
 * @param len   Number of bytes
 */
static void _sha384_update(sha384_ctx * ctx, const uint8_t * data, size_t len)
{
    size_t used = (size_t)(ctx->count % SHA384_BLOCK_SIZE);
    ctx->count += len;

    if (used > 0) {
        size_t space = SHA384_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(ctx->buffer + used, data, len);
            return;
        }
        memcpy(ctx->buffer + used, data, space);
        _sha384_transform(ctx->state, ctx->buffer);
        data += space;
        len -= space;
    }

    while (len >= SHA384_BLOCK_SIZE) {
        _sha384_transform(ctx->state, data);
        data += SHA384_BLOCK_SIZE;
        len -= SHA384_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

/**
 * @brief Finalize SHA-384 hash and produce digest
 * @param ctx     SHA-384 context (must not be NULL)
 * @param digest  Output buffer (at least SHA384_DIGEST_SIZE bytes)
 */
static void _sha384_final(sha384_ctx * ctx, uint8_t * digest)
{
    size_t used = (size_t)(ctx->count % SHA384_BLOCK_SIZE);
    uint64_t bit_count_lo = ctx->count * 8;
    uint64_t bit_count_hi = 0;

    /* Append padding: 0x80 followed by zeros */
    ctx->buffer[used++] = 0x80;
    if (used > 112) {
        memset(ctx->buffer + used, 0, SHA384_BLOCK_SIZE - used);
        _sha384_transform(ctx->state, ctx->buffer);
        used = 0;
    }
    memset(ctx->buffer + used, 0, 112 - used);

    /* Append length in bits (big-endian, 128-bit: high 64 bits + low 64 bits) */
    ctx->buffer[112] = (uint8_t)(bit_count_hi >> 56);
    ctx->buffer[113] = (uint8_t)(bit_count_hi >> 48);
    ctx->buffer[114] = (uint8_t)(bit_count_hi >> 40);
    ctx->buffer[115] = (uint8_t)(bit_count_hi >> 32);
    ctx->buffer[116] = (uint8_t)(bit_count_hi >> 24);
    ctx->buffer[117] = (uint8_t)(bit_count_hi >> 16);
    ctx->buffer[118] = (uint8_t)(bit_count_hi >> 8);
    ctx->buffer[119] = (uint8_t)(bit_count_hi);
    ctx->buffer[120] = (uint8_t)(bit_count_lo >> 56);
    ctx->buffer[121] = (uint8_t)(bit_count_lo >> 48);
    ctx->buffer[122] = (uint8_t)(bit_count_lo >> 40);
    ctx->buffer[123] = (uint8_t)(bit_count_lo >> 32);
    ctx->buffer[124] = (uint8_t)(bit_count_lo >> 24);
    ctx->buffer[125] = (uint8_t)(bit_count_lo >> 16);
    ctx->buffer[126] = (uint8_t)(bit_count_lo >> 8);
    ctx->buffer[127] = (uint8_t)(bit_count_lo);

    _sha384_transform(ctx->state, ctx->buffer);

    /* Encode state to digest (big-endian), output first 48 bytes (6 words) for SHA-384 */
    for (int i = 0; i < 6; i++) {
        digest[i * 8]     = (uint8_t)(ctx->state[i] >> 56);
        digest[i * 8 + 1] = (uint8_t)(ctx->state[i] >> 48);
        digest[i * 8 + 2] = (uint8_t)(ctx->state[i] >> 40);
        digest[i * 8 + 3] = (uint8_t)(ctx->state[i] >> 32);
        digest[i * 8 + 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 8 + 5] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 8 + 6] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 8 + 7] = (uint8_t)(ctx->state[i]);
    }
}

/* ---- output helpers ---- */

/**
 * @brief Output a byte array as lowercase hexadecimal
 * @param digest  Byte array
 * @param len     Number of bytes
 */
static void _sha384sum_output_hex(const uint8_t * digest, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        sha384sum_putchar(hex[digest[i] >> 4]);
        sha384sum_putchar(hex[digest[i] & 0x0F]);
    }
}

/* ---- hex helpers ---- */

/**
 * @brief Convert a hex character to its integer value
 * @param c  Hex character (0-9, a-f, A-F)
 * @return Value 0-15, or -1 if not a hex digit
 */
static int _sha384sum_hex_val(char c)
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
static int _sha384sum_hex_to_bytes(const char * hex, uint8_t * out,
                                   size_t out_size, size_t * out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t n = len / 2;
    if (n > out_size) return -1;

    for (size_t i = 0; i < n; i++) {
        int hi = _sha384sum_hex_val(hex[i * 2]);
        int lo = _sha384sum_hex_val(hex[i * 2 + 1]);
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
static bool _sha384sum_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}

/* ---- compute digest for a file ---- */

/**
 * @brief Compute SHA-384 digest of a file
 * @param filename  Path to file, or "-" for stdin
 * @param digest    Output buffer (SHA384_DIGEST_SIZE bytes)
 * @return 0 on success, -1 on error
 */
static int _sha384sum_compute(const char * filename, uint8_t * digest)
{
    FILE * fp;
    bool is_stdin = _sha384sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
#ifdef SHA384SUM_PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            return -1;
        }
    }

    sha384_ctx ctx;
    _sha384_init(&ctx);

    uint8_t buf[SHA384SUM_READ_BUF_SIZE];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        _sha384_update(&ctx, buf, n);
    }

    _sha384_final(&ctx, digest);

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
static int _sha384sum_process_file(const char * filename, const sha384sum_opts * opts)
{
    uint8_t digest[SHA384_DIGEST_SIZE];
    int rc = _sha384sum_compute(filename, digest);
    if (rc != 0) {
        sha384sum_err_printf("sha384sum: %s: %s\n", filename, strerror(errno));
        return 1;
    }

    bool is_stdin = _sha384sum_streq(filename, "-");

    if (opts->tag) {
        /* BSD-style tagged format: SHA384 (filename) = hash */
        sha384sum_fputs("SHA384 (", sha384sum_out_stream);
        if (is_stdin) {
            sha384sum_fputs("stdin", sha384sum_out_stream);
        }
        else {
            sha384sum_fputs(filename, sha384sum_out_stream);
        }
        sha384sum_fputs(") = ", sha384sum_out_stream);
        _sha384sum_output_hex(digest, SHA384_DIGEST_SIZE);
    }
    else {
        /* Default untagged format: hash  filename or hash *filename (binary) */
        _sha384sum_output_hex(digest, SHA384_DIGEST_SIZE);
        if (is_stdin) {
            sha384sum_fputs("  -", sha384sum_out_stream);
        }
        else {
            if (opts->binary) {
                sha384sum_fputs(" *", sha384sum_out_stream);
                sha384sum_fputs(filename, sha384sum_out_stream);
            }
            else {
                sha384sum_fputs("  ", sha384sum_out_stream);
                sha384sum_fputs(filename, sha384sum_out_stream);
            }
        }
    }

    if (opts->zero) {
        sha384sum_putchar('\0');
    }
    else {
        sha384sum_putchar('\n');
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
static int _sha384sum_check_file(const char * filename, const sha384sum_opts * opts)
{
    FILE * fp;
    bool is_stdin = _sha384sum_streq(filename, "-");

    if (is_stdin) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (!fp) {
            sha384sum_err_printf("sha384sum: %s: %s\n", filename, strerror(errno));
            return 1;
        }
    }

    char line[SHA384SUM_MAX_LINE];
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

        char parsed_file[SHA384SUM_MAX_FILENAME] = "";
        char parsed_digest[SHA384SUM_MAX_DIGEST_STR] = "";
        bool is_tagged = false;

        /* Try tagged format: SHA384 (filename) = digest */
        char * paren = strchr(line, '(');
        char * eq = strchr(line, '=');
        if (paren && eq && paren < eq) {
            /* Check that the tag starts with "SHA384" */
            size_t alg_len = (size_t)(paren - line);
            while (alg_len > 0 && line[alg_len - 1] == ' ') alg_len--;
            if (alg_len == 6 && strncasecmp(line, "SHA384", 6) == 0) {
                is_tagged = true;
            }
        }

        if (is_tagged) {
            /* Extract filename from between parentheses */
            char * close_paren = strchr(paren + 1, ')');
            if (!close_paren || close_paren > eq) {
                if (opts->warn) {
                    sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
                                         filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t file_len = (size_t)(close_paren - paren - 1);
            if (file_len >= sizeof(parsed_file)) {
                if (opts->warn) {
                    sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
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
                    sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
                                         filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t digest_len = (size_t)(space - line);
            if (digest_len >= sizeof(parsed_digest)) {
                if (opts->warn) {
                    sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
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

        /* Validate digest is exactly 96 hex chars (SHA-384 = 48 bytes) */
        if (strlen(parsed_digest) != 96) {
            if (opts->warn) {
                sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
                                     filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        uint8_t expected[SHA384_DIGEST_SIZE];
        size_t expected_len = 0;
        if (_sha384sum_hex_to_bytes(parsed_digest, expected, sizeof(expected), &expected_len) != 0) {
            if (opts->warn) {
                sha384sum_err_printf("sha384sum: %s: %d: improperly formatted line\n",
                                     filename, line_num);
            }
            if (opts->strict) exit_code = 1;
            continue;
        }

        any_parsed = true;

        /* Compute actual digest */
        uint8_t actual[SHA384_DIGEST_SIZE];
        int rc = _sha384sum_compute(parsed_file, actual);
        if (rc != 0) {
            if (!opts->ignore_missing) {
                sha384sum_err_printf("sha384sum: %s: %s\n", parsed_file, strerror(errno));
                exit_code = 1;
            }
            continue;
        }

        bool match = (memcmp(actual, expected, SHA384_DIGEST_SIZE) == 0);

        if (!opts->status) {
            if (match) {
                if (!opts->quiet) {
                    sha384sum_printf("%s: OK\n", parsed_file);
                }
            }
            else {
                sha384sum_printf("%s: FAILED\n", parsed_file);
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
        sha384sum_err_printf("sha384sum: %s: no properly formatted SHA384 checksum lines found\n",
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
static void _sha384sum_print_help(void)
{
    sha384sum_printf(
        "Usage: sha384sum [OPTION]... [FILE]...\n"
        "Print or check SHA384 (384-bit) checksums.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "  -b, --binary          read in binary mode\n"
        "  -c, --check           read SHA384 sums from the FILEs and check them\n"
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
static void _sha384sum_print_version(void)
{
    sha384sum_printf("sha384sum %s\n", SHA384SUM_VERSION_STR);
    sha384sum_printf("%s", "Copyright (C) 2025-2026 Yezc/sha384sum\n");
    sha384sum_printf("%s", "License MIT: <https://mit-license.org/>\n");
    sha384sum_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    sha384sum_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}
