/**
 * @file b2sum.c
 * @brief Cross-platform implementation of the b2sum command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils b2sum(1).
 *
 * Key behaviors:
 *   - Computes BLAKE2b checksums (default 512 bits = 64 bytes)
 *   - -l, --length=BITS   digest length in bits (0-512, multiple of 8)
 *   - -b, --binary        read in binary mode
 *   - -c, --check         verify checksums from a file
 *   -     --tag           create BSD-style checksum output
 *   - -t, --text          read in text mode (default)
 *   - -z, --zero          end each output line with NUL
 *   -     --ignore-missing  don't fail for missing files (with --check)
 *   -     --quiet         don't print OK for verified files (with --check)
 *   -     --status        don't output, use exit code (with --check)
 *   -     --strict        exit non-zero for malformed checksum lines
 *   - -w, --warn          warn about improperly formatted lines
 *   -     --help / --version
 *   - stdin support (no file args, or - as filename)
 *   - Binary-safe I/O
 *
 * The BLAKE2b algorithm is implemented from scratch per RFC 7693.
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o b2sum.exe b2sum.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o b2sum b2sum.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o b2sum b2sum.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o b2sum b2sum.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o b2sum b2sum.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o b2sum b2sum.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/b2sum>
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
    #define B2SUM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define B2SUM_PLATFORM_LINUX   1
    #define B2SUM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define B2SUM_PLATFORM_MACOS   1
    #define B2SUM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define B2SUM_PLATFORM_FREEBSD 1
    #define B2SUM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define B2SUM_PLATFORM_OPENBSD 1
    #define B2SUM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define B2SUM_PLATFORM_NETBSD  1
    #define B2SUM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define B2SUM_PLATFORM_POSIX   1
#else
    #define B2SUM_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef B2SUM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef B2SUM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef B2SUM_PLATFORM_NETBSD
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

#ifdef B2SUM_PLATFORM_WINDOWS
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
#define B2SUM_VERSION_STR "v1.0.0"

/** @brief Maximum digest length in bytes (BLAKE2b = 512 bits = 64 bytes) */
#define B2SUM_MAX_DIGEST 64

/** @brief BLAKE2b block size in bytes (1024 bits = 128 bytes) */
#define B2SUM_BLOCK_SIZE 128

/** @brief Number of BLAKE2b rounds (12 for BLAKE2b) */
#define B2SUM_ROUNDS 12

/** @brief I/O buffer size for file reading */
#define B2SUM_IO_BUF_SIZE 65536

/** @brief Maximum line length for --check files */
#define B2SUM_MAX_LINE 8192

/** @brief Number of 64-bit words in a BLAKE2b block */
#define B2SUM_BLOCK_WORDS 16

/** @brief Number of chaining values */
#define B2SUM_CHAIN_WORDS 8

/********************************
 *    typedefs
 ********************************/

/** @brief BLAKE2b hash state */
typedef struct {
    uint64_t h[B2SUM_CHAIN_WORDS];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t buf[B2SUM_BLOCK_SIZE];
    size_t buflen;
    size_t outlen;
} blake2b_state;

/** @brief Command-line options */
typedef struct {
    size_t length;
    bool binary;
    bool check;
    bool tag;
    bool text;
    bool zero;
    bool ignore_missing;
    bool quiet;
    bool status;
    bool strict;
    bool warn;
} b2sum_opts;

/********************************
 *    static prototypes — BLAKE2b
 ********************************/
static void _b2sum_blake2b_init(blake2b_state * S, size_t outlen);
static void _b2sum_blake2b_update(blake2b_state * S,
                                  const uint8_t * data, size_t len);
static void _b2sum_blake2b_final(blake2b_state * S, uint8_t * out);
static void _b2sum_blake2b_compress(blake2b_state * S,
                                    const uint8_t block[B2SUM_BLOCK_SIZE]);

/********************************
 *    static prototypes — b2sum
 ********************************/
static void _b2sum_print_help(void);
static void _b2sum_print_version(void);
static int  _b2sum_parse_opts(int argc, char ** argv,
                              b2sum_opts * opts, int * file_start);
static int  _b2sum_hash_file(const char * filename, const b2sum_opts * opts,
                             uint8_t * digest);
static int  _b2sum_hash_stream(FILE * fp, size_t outlen, uint8_t * digest);
static int  _b2sum_do_checksum(const b2sum_opts * opts,
                               char ** files, int nfiles);
static int  _b2sum_do_check(const b2sum_opts * opts,
                            char ** files, int nfiles);
static void _b2sum_format_hex(const uint8_t * digest, size_t len,
                              char * out);
static int  _b2sum_parse_hex(const char * hex, uint8_t * out, size_t max_len,
                             size_t * out_len);
static const char * _b2sum_basename(const char * path);

/********************************
 *    macros
 ********************************/

#ifndef b2sum_printf
    #define b2sum_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef b2sum_err_printf
    #define b2sum_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef b2sum_fputs
    #define b2sum_fputs(str, stream) (void)fputs((str), (stream))
#endif

#ifndef b2sum_fflush
    #define b2sum_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/* Rotation macro for 64-bit integers */
#define B2SUM_ROTR64(x, n) \
    (((x) >> (n)) | ((x) << (64 - (n))))

/* BLAKE2b G function */
#define B2SUM_G(r, i, a, b, c, d) \
    do { \
        a = a + b + m[blake2b_sigma[r][2*i]];   \
        d = B2SUM_ROTR64(d ^ a, 32);            \
        c = c + d;                               \
        b = B2SUM_ROTR64(b ^ c, 24);            \
        a = a + b + m[blake2b_sigma[r][2*i+1]]; \
        d = B2SUM_ROTR64(d ^ a, 16);            \
        c = c + d;                               \
        b = B2SUM_ROTR64(b ^ c, 63);            \
    } while (0)

/* BLAKE2b round */
#define B2SUM_ROUND(r) \
    do { \
        B2SUM_G(r, 0, v[0], v[4], v[8],  v[12]); \
        B2SUM_G(r, 1, v[1], v[5], v[9],  v[13]); \
        B2SUM_G(r, 2, v[2], v[6], v[10], v[14]); \
        B2SUM_G(r, 3, v[3], v[7], v[11], v[15]); \
        B2SUM_G(r, 4, v[0], v[5], v[10], v[15]); \
        B2SUM_G(r, 5, v[1], v[6], v[11], v[12]); \
        B2SUM_G(r, 6, v[2], v[7], v[8],  v[13]); \
        B2SUM_G(r, 7, v[3], v[4], v[9],  v[14]); \
    } while (0)

/********************************
 *    static variables
 ********************************/

/** @brief BLAKE2b initialization vector (IV) */
static const uint64_t blake2b_iv[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
};

/** @brief BLAKE2b sigma permutation schedule (12 rounds) */
static const uint8_t blake2b_sigma[12][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3  },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4  },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8  },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9  },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5  },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0  },
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3  }
};

/** @brief Program name for error messages */
static const char * b2sum_prog_name = "b2sum";

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the b2sum command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. If --check, verify checksums from files
 *   3. Otherwise, compute checksums for each file (or stdin)
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error/verification failure
 */
int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 1;
    }

    b2sum_prog_name = _b2sum_basename(argv[0]);

#ifdef B2SUM_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
#endif

    b2sum_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.length = 512; /* default: 512 bits */
    opts.text = true;  /* default: text mode */

    int file_start = 0;
    if (!_b2sum_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    int nfiles = argc - file_start;
    char ** files = (nfiles > 0) ? &argv[file_start] : NULL;

    if (opts.check) {
        return _b2sum_do_check(&opts, files, nfiles);
    }

    return _b2sum_do_checksum(&opts, files, nfiles);
}

/********************************
 *    static functions — BLAKE2b
 ********************************/

/**
 * @brief Initialize BLAKE2b state
 *
 * Sets the chaining value to IV XOR parameter block (only outlen
 * and key length=0 are relevant for unkeyed hashing).
 *
 * @param S       hash state to initialize
 * @param outlen  desired output length in bytes (1-64)
 */
static void _b2sum_blake2b_init(blake2b_state * S, size_t outlen)
{
    if (!S || outlen == 0 || outlen > B2SUM_MAX_DIGEST) {
        return;
    }

    memcpy(S->h, blake2b_iv, sizeof(blake2b_iv));
    /* Parameter block: digest_length | key_length=0 | fanout=1 | depth=1 */
    S->h[0] ^= 0x01010000ULL | (uint64_t)outlen;

    S->t[0] = 0;
    S->t[1] = 0;
    S->f[0] = 0;
    S->f[1] = 0;
    S->buflen = 0;
    S->outlen = outlen;
}

/**
 * @brief Feed data into BLAKE2b hash
 *
 * Buffers data and processes full blocks through the compression
 * function. The counter is incremented BEFORE compression (per RFC 7693).
 * The final partial block is handled by _b2sum_blake2b_final.
 *
 * @param S     hash state
 * @param data  input data
 * @param len   input length in bytes
 */
static void _b2sum_blake2b_update(blake2b_state * S,
                                  const uint8_t * data, size_t len)
{
    if (!S || !data || len == 0) {
        return;
    }

    while (len > 0) {
        size_t left = S->buflen;
        size_t fill = B2SUM_BLOCK_SIZE - left;

        if (len > fill) {
            /* Data exceeds buffer capacity */
            if (left > 0) {
                /* Fill buffer and compress */
                memcpy(S->buf + left, data, fill);
                S->buflen = 0;
                S->t[0] += B2SUM_BLOCK_SIZE;
                if (S->t[0] < B2SUM_BLOCK_SIZE) {
                    S->t[1]++;
                }
                _b2sum_blake2b_compress(S, S->buf);
                data += fill;
                len -= fill;
            }
            else {
                /* Buffer empty; process full blocks directly */
                while (len > B2SUM_BLOCK_SIZE) {
                    S->t[0] += B2SUM_BLOCK_SIZE;
                    if (S->t[0] < B2SUM_BLOCK_SIZE) {
                        S->t[1]++;
                    }
                    _b2sum_blake2b_compress(S, data);
                    data += B2SUM_BLOCK_SIZE;
                    len -= B2SUM_BLOCK_SIZE;
                }
            }
        }
        else {
            /* Remaining data fits in buffer */
            memcpy(S->buf + left, data, len);
            S->buflen += len;
            len = 0;
        }
    }
}

/**
 * @brief Finalize BLAKE2b hash and extract digest
 *
 * Pads the last block with zeros, sets the finalization flag,
 * compresses, and extracts the first outlen bytes of the state.
 *
 * @param S    hash state
 * @param out  output buffer (must hold at least outlen bytes)
 */
static void _b2sum_blake2b_final(blake2b_state * S, uint8_t * out)
{
    if (!S || !out) {
        return;
    }

    /* Set finalization flag */
    S->f[0] = (uint64_t)-1;

    /* Pad remaining buffer with zeros */
    memset(S->buf + S->buflen, 0, B2SUM_BLOCK_SIZE - S->buflen);

    /* Update counter with remaining bytes */
    S->t[0] += S->buflen;
    if (S->t[0] < S->buflen) {
        S->t[1]++;
    }

    /* Final compression */
    _b2sum_blake2b_compress(S, S->buf);

    /* Extract output from chaining value */
    for (size_t i = 0; i < S->outlen; i++) {
        out[i] = (uint8_t)(S->h[i / 8] >> ((i % 8) * 8));
    }
}

/**
 * @brief BLAKE2b compression function F
 *
 * Processes one 128-byte block using 12 rounds of the G function.
 *
 * @param S      hash state (counter and finalization flags used)
 * @param block  128-byte message block
 */
static void _b2sum_blake2b_compress(blake2b_state * S,
                                    const uint8_t block[B2SUM_BLOCK_SIZE])
{
    uint64_t m[B2SUM_BLOCK_WORDS];
    uint64_t v[B2SUM_BLOCK_WORDS];
    int i;

    /* Load message words (little-endian) */
    for (i = 0; i < B2SUM_BLOCK_WORDS; i++) {
        m[i] = (uint64_t)block[i * 8]
             | ((uint64_t)block[i * 8 + 1] << 8)
             | ((uint64_t)block[i * 8 + 2] << 16)
             | ((uint64_t)block[i * 8 + 3] << 24)
             | ((uint64_t)block[i * 8 + 4] << 32)
             | ((uint64_t)block[i * 8 + 5] << 40)
             | ((uint64_t)block[i * 8 + 6] << 48)
             | ((uint64_t)block[i * 8 + 7] << 56);
    }

    /* Initialize working vector: v[0..7] = h, v[8..15] = IV */
    for (i = 0; i < 8; i++) {
        v[i] = S->h[i];
        v[i + 8] = blake2b_iv[i];
    }

    /* Mix in counter and finalization flags */
    v[12] ^= S->t[0];
    v[13] ^= S->t[1];
    v[14] ^= S->f[0];
    v[15] ^= S->f[1];

    /* 12 rounds */
    B2SUM_ROUND(0);
    B2SUM_ROUND(1);
    B2SUM_ROUND(2);
    B2SUM_ROUND(3);
    B2SUM_ROUND(4);
    B2SUM_ROUND(5);
    B2SUM_ROUND(6);
    B2SUM_ROUND(7);
    B2SUM_ROUND(8);
    B2SUM_ROUND(9);
    B2SUM_ROUND(10);
    B2SUM_ROUND(11);

    /* Finalize: h[i] ^= v[i] ^ v[i+8] */
    for (i = 0; i < 8; i++) {
        S->h[i] ^= v[i] ^ v[i + 8];
    }
}

/********************************
 *    static functions — b2sum
 ********************************/

/**
 * @brief Print usage/help information
 */
static void _b2sum_print_help(void)
{
    b2sum_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Print or check BLAKE2 (512-bit) checksums.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "  -l, --length=BITS     digest length in bits; must not exceed 512\n"
        "                        and must be a multiple of 8\n"
        "  -b, --binary          read in binary mode\n"
        "  -c, --check           read checksums from the FILEs and check them\n"
        "      --tag              create a BSD-style checksum\n"
        "  -t, --text            read in text mode (default)\n"
        "  -z, --zero            end each output line with NUL, not newline,\n"
        "                        and disable file name escaping\n"
        "      --ignore-missing   don't fail or report status for missing files\n"
        "      --quiet            don't print OK for each successfully verified file\n"
        "      --status           don't output anything; status code shows success\n"
        "      --strict           exit non-zero for improperly formatted\n"
        "                        checksum lines\n"
        "  -w, --warn            warn about improperly formatted checksum lines\n"
        "      --help            display this help and exit\n"
        "      --version         output version information and exit\n"
        "\n"
        "The following five options are useful only when verifying checksums:\n"
        "      --ignore-missing  don't fail or report status for missing files\n"
        "      --quiet           don't print OK for each successfully verified file\n"
        "      --status          don't output anything; status code shows success\n"
        "      --strict          exit non-zero for improperly formatted lines\n"
        "  -w, --warn            warn about improperly formatted lines\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        b2sum_prog_name
    );
}

/**
 * @brief Print version information
 */
static void _b2sum_print_version(void)
{
    b2sum_printf("b2sum %s\n", B2SUM_VERSION_STR);
    b2sum_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    b2sum_printf("%s", "License MIT: <https://mit-license.org/>\n");
    b2sum_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    b2sum_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Extract basename from a path
 * @param path  full path (e.g. "/usr/bin/b2sum" or "b2sum.exe")
 * @return pointer to basename within path
 */
static const char * _b2sum_basename(const char * path)
{
    if (!path) {
        return "b2sum";
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
 * Supports style long options (--length=256) and short options (-l 256).
 *
 * @param argc        argument count
 * @param argv        argument vector
 * @param opts        output options struct
 * @param file_start  index in argv where files begin
 * @return true on success, false on error
 */
static int _b2sum_parse_opts(int argc, char ** argv,
                             b2sum_opts * opts, int * file_start)
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
                _b2sum_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _b2sum_print_version();
                exit(0);
            }
            if (strcmp(arg, "--binary") == 0) {
                opts->binary = true;
                opts->text = false;
                continue;
            }
            if (strcmp(arg, "--check") == 0) {
                opts->check = true;
                continue;
            }
            if (strcmp(arg, "--tag") == 0) {
                opts->tag = true;
                continue;
            }
            if (strcmp(arg, "--text") == 0) {
                opts->text = true;
                opts->binary = false;
                continue;
            }
            if (strcmp(arg, "--zero") == 0) {
                opts->zero = true;
                continue;
            }
            if (strcmp(arg, "--ignore-missing") == 0) {
                opts->ignore_missing = true;
                continue;
            }
            if (strcmp(arg, "--quiet") == 0) {
                opts->quiet = true;
                continue;
            }
            if (strcmp(arg, "--status") == 0) {
                opts->status = true;
                continue;
            }
            if (strcmp(arg, "--strict") == 0) {
                opts->strict = true;
                continue;
            }
            if (strcmp(arg, "--warn") == 0) {
                opts->warn = true;
                continue;
            }
            /* --length=BITS */
            if (strncmp(arg, "--length=", 9) == 0) {
                char * end = NULL;
                errno = 0;
                unsigned long bits = strtoul(arg + 9, &end, 10);
                if (errno != 0 || !end || *end != '\0' ||
                    bits == 0 || bits > 512 || (bits % 8) != 0) {
                    b2sum_err_printf("%s: invalid --length value '%s'\n",
                                     b2sum_prog_name, arg + 9);
                    return false;
                }
                opts->length = (size_t)bits;
                continue;
            }
            /* --length BITS (separate arg) */
            if (strcmp(arg, "--length") == 0) {
                if (i + 1 >= argc) {
                    b2sum_err_printf("%s: option '--length' requires an argument\n",
                                     b2sum_prog_name);
                    return false;
                }
                char * end = NULL;
                errno = 0;
                unsigned long bits = strtoul(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' ||
                    bits == 0 || bits > 512 || (bits % 8) != 0) {
                    b2sum_err_printf("%s: invalid --length value '%s'\n",
                                     b2sum_prog_name, argv[i + 1]);
                    return false;
                }
                opts->length = (size_t)bits;
                i++;
                continue;
            }

            b2sum_err_printf("%s: unrecognized option '%s'\n",
                             b2sum_prog_name, arg);
            b2sum_err_printf("%s", "Try 'b2sum --help' for more information.\n");
            return false;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            /* Handle combined short options like -bt, -cz, etc. */
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'b':
                        opts->binary = true;
                        opts->text = false;
                        break;
                    case 'c':
                        opts->check = true;
                        break;
                    case 't':
                        opts->text = true;
                        opts->binary = false;
                        break;
                    case 'z':
                        opts->zero = true;
                        break;
                    case 'w':
                        opts->warn = true;
                        break;
                    case 'l':
                        /* -l takes an argument: either rest of this arg or next arg */
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
                            b2sum_err_printf("%s: option '-l' requires an argument\n",
                                             b2sum_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        errno = 0;
                        unsigned long bits = strtoul(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' ||
                            bits == 0 || bits > 512 || (bits % 8) != 0) {
                            b2sum_err_printf("%s: invalid length '%s'\n",
                                             b2sum_prog_name, val);
                            return false;
                        }
                        opts->length = (size_t)bits;
                        goto next_arg;
                    }
                    default:
                        b2sum_err_printf("%s: invalid option -- '%c'\n",
                                         b2sum_prog_name, arg[j]);
                        b2sum_err_printf("%s", "Try 'b2sum --help' for more information.\n");
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
 * @brief Compute BLAKE2b digest of a file
 *
 * @param filename  file path ("-" means stdin)
 * @param opts      options (length, binary mode)
 * @param digest    output buffer (must hold at least B2SUM_MAX_DIGEST bytes)
 * @return 0 on success, -1 on error
 */
static int _b2sum_hash_file(const char * filename, const b2sum_opts * opts,
                            uint8_t * digest)
{
    if (!filename || !opts || !digest) {
        return -1;
    }

    size_t outlen = opts->length / 8;

    /* stdin */
    if (strcmp(filename, "-") == 0) {
        return _b2sum_hash_stream(stdin, outlen, digest);
    }

    FILE * fp = fopen(filename, "rb");
    if (!fp) {
        b2sum_err_printf("%s: %s: %s\n", b2sum_prog_name,
                         filename, strerror(errno));
        return -1;
    }

    int rc = _b2sum_hash_stream(fp, outlen, digest);
    fclose(fp);
    return rc;
}

/**
 * @brief Compute BLAKE2b digest from a stream
 *
 * Reads data in blocks and feeds it to BLAKE2b.
 *
 * @param fp      input stream (must be opened in binary mode)
 * @param outlen  output length in bytes
 * @param digest  output buffer
 * @return 0 on success, -1 on read error
 */
static int _b2sum_hash_stream(FILE * fp, size_t outlen, uint8_t * digest)
{
    if (!fp || !digest || outlen == 0 || outlen > B2SUM_MAX_DIGEST) {
        return -1;
    }

    blake2b_state S;
    _b2sum_blake2b_init(&S, outlen);

    uint8_t * buf = (uint8_t *)malloc(B2SUM_IO_BUF_SIZE);
    if (!buf) {
        b2sum_err_printf("%s: out of memory\n", b2sum_prog_name);
        return -1;
    }

    size_t n;
    while ((n = fread(buf, 1, B2SUM_IO_BUF_SIZE, fp)) > 0) {
        _b2sum_blake2b_update(&S, buf, n);
    }

    free(buf);

    if (ferror(fp)) {
        b2sum_err_printf("%s: read error\n", b2sum_prog_name);
        return -1;
    }

    _b2sum_blake2b_final(&S, digest);
    return 0;
}

/**
 * @brief Format a digest as a lowercase hex string
 * @param digest  raw bytes
 * @param len     number of bytes
 * @param out     output buffer (must hold at least len*2+1 bytes)
 */
static void _b2sum_format_hex(const uint8_t * digest, size_t len,
                              char * out)
{
    static const char hexchars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hexchars[digest[i] >> 4];
        out[i * 2 + 1] = hexchars[digest[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/**
 * @brief Parse a hex string into raw bytes
 * @param hex     hex string
 * @param out     output buffer
 * @param max_len maximum bytes to write
 * @param out_len actual number of bytes parsed
 * @return true on success, false on invalid hex
 */
static int _b2sum_parse_hex(const char * hex, uint8_t * out, size_t max_len,
                            size_t * out_len)
{
    if (!hex || !out || !out_len) {
        return false;
    }

    size_t hlen = strlen(hex);
    if (hlen == 0 || (hlen % 2) != 0 || (hlen / 2) > max_len) {
        return false;
    }

    *out_len = hlen / 2;

    for (size_t i = 0; i < *out_len; i++) {
        int hi = -1, lo = -1;
        char c = hex[i * 2];
        if (c >= '0' && c <= '9') { hi = c - '0'; }
        else if (c >= 'a' && c <= 'f') { hi = c - 'a' + 10; }
        else if (c >= 'A' && c <= 'F') { hi = c - 'A' + 10; }

        c = hex[i * 2 + 1];
        if (c >= '0' && c <= '9') { lo = c - '0'; }
        else if (c >= 'a' && c <= 'f') { lo = c - 'a' + 10; }
        else if (c >= 'A' && c <= 'F') { lo = c - 'A' + 10; }

        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

/**
 * @brief Compute and print checksums for files
 *
 * Output formats:
 *   Standard:  HASH  FILENAME  (text mode)  or  HASH *FILENAME  (binary mode)
 *   Tag mode:  BLAKE2b (FILENAME) = HASH
 *
 * @param opts    options
 * @param files   file list (NULL = stdin)
 * @param nfiles  number of files
 * @return 0 on success, 1 on any error
 */
static int _b2sum_do_checksum(const b2sum_opts * opts,
                              char ** files, int nfiles)
{
    if (!opts) {
        return 1;
    }

    size_t outlen = opts->length / 8;
    int exit_code = 0;

    if (nfiles == 0 || !files) {
        /* stdin */
        uint8_t digest[B2SUM_MAX_DIGEST];
        if (_b2sum_hash_stream(stdin, outlen, digest) != 0) {
            return 1;
        }

        char hex[B2SUM_MAX_DIGEST * 2 + 1];
        _b2sum_format_hex(digest, outlen, hex);

        if (opts->tag) {
            b2sum_printf("BLAKE2b (-) = %s\n", hex);
        }
        else {
            char sep = opts->binary ? '*' : ' ';
            b2sum_printf("%s %c%s", hex, sep, "-");
            if (opts->zero) {
                b2sum_printf("%c", '\0');
            }
            else {
                b2sum_printf("%c", '\n');
            }
        }
        return 0;
    }

    for (int i = 0; i < nfiles; i++) {
        uint8_t digest[B2SUM_MAX_DIGEST];
        if (_b2sum_hash_file(files[i], opts, digest) != 0) {
            exit_code = 1;
            continue;
        }

        char hex[B2SUM_MAX_DIGEST * 2 + 1];
        _b2sum_format_hex(digest, outlen, hex);

        if (opts->tag) {
            b2sum_printf("BLAKE2b (%s) = %s\n", files[i], hex);
        }
        else {
            char sep = opts->binary ? '*' : ' ';
            b2sum_printf("%s %c%s", hex, sep, files[i]);
            if (opts->zero) {
                b2sum_printf("%c", '\0');
            }
            else {
                b2sum_printf("%c", '\n');
            }
        }
    }

    return exit_code;
}

/**
 * @brief Verify checksums from a file
 *
 * Reads lines in the format: HASH  FILENAME
 * or BSD tag format: BLAKE2b (FILENAME) = HASH
 *
 * @param opts    options
 * @param files    list of checksum files (NULL = stdin)
 * @param nfiles  number of files
 * @return 0 if all match, 1 if any fail or error
 */
static int _b2sum_do_check(const b2sum_opts * opts,
                           char ** files, int nfiles)
{
    if (!opts) {
        return 1;
    }

    int exit_code = 0;

    /* If no files, read from stdin */
    if (nfiles == 0 || !files) {
        char * stdin_name = (char *)"-";
        return _b2sum_do_check(opts, &stdin_name, 1);
    }

    for (int fi = 0; fi < nfiles; fi++) {
        FILE * fp;
        if (strcmp(files[fi], "-") == 0) {
            fp = stdin;
        }
        else {
            fp = fopen(files[fi], "r");
            if (!fp) {
                b2sum_err_printf("%s: %s: %s\n", b2sum_prog_name,
                                 files[fi], strerror(errno));
                exit_code = 1;
                continue;
            }
        }

        char line[B2SUM_MAX_LINE];
        int line_num = 0;
        bool bom_checked = false;

        while (fgets(line, sizeof(line), fp)) {
            line_num++;
            size_t linelen = strlen(line);

            /* Skip UTF-8 BOM on first line */
            if (!bom_checked) {
                bom_checked = true;
                if (linelen >= 3 &&
                    (uint8_t)line[0] == 0xEF &&
                    (uint8_t)line[1] == 0xBB &&
                    (uint8_t)line[2] == 0xBF) {
                    memmove(line, line + 3, linelen - 3 + 1);
                    linelen -= 3;
                }
            }

            /* Remove trailing newline */
            if (linelen > 0 && line[linelen - 1] == '\n') {
                line[--linelen] = '\0';
            }
            if (linelen > 0 && line[linelen - 1] == '\r') {
                line[--linelen] = '\0';
            }

            if (linelen == 0) {
                if (opts->warn) {
                    b2sum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     b2sum_prog_name, files[fi], line_num);
                }
                if (opts->strict) {
                    exit_code = 1;
                }
                continue;
            }

            /* Parse the line: HASH  FILENAME or BSD tag format */
            char hash_hex[B2SUM_MAX_DIGEST * 2 + 1];
            char filename[B2SUM_MAX_LINE];
            size_t expected_len = 0;

            /* Try BSD tag format: BLAKE2b (FILENAME) = HASH */
            if (strncmp(line, "BLAKE2b (", 9) == 0) {
                char * close = strchr(line + 9, ')');
                if (!close || strncmp(close + 1, " = ", 3) != 0) {
                    if (opts->warn) {
                        b2sum_err_printf("%s: %s: %d: improperly formatted line\n",
                                         b2sum_prog_name, files[fi], line_num);
                    }
                    if (opts->strict) {
                        exit_code = 1;
                    }
                    continue;
                }
                size_t fnlen = (size_t)(close - (line + 9));
                if (fnlen >= sizeof(filename)) {
                    fnlen = sizeof(filename) - 1;
                }
                memcpy(filename, line + 9, fnlen);
                filename[fnlen] = '\0';

                const char * hash_start = close + 4;
                size_t hashlen = strlen(hash_start);
                if (hashlen >= sizeof(hash_hex)) {
                    hashlen = sizeof(hash_hex) - 1;
                }
                memcpy(hash_hex, hash_start, hashlen);
                hash_hex[hashlen] = '\0';
            }
            else {
                /* Standard format: HASH  FILENAME */
                /* Skip leading spaces in hash */
                char * p = line;
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                /* Find end of hash (space or asterisk) */
                char * sep = p;
                while (*sep && *sep != ' ' && *sep != '\t') {
                    sep++;
                }
                if (!*sep) {
                    if (opts->warn) {
                        b2sum_err_printf("%s: %s: %d: improperly formatted line\n",
                                         b2sum_prog_name, files[fi], line_num);
                    }
                    if (opts->strict) {
                        exit_code = 1;
                    }
                    continue;
                }
                size_t hashlen = (size_t)(sep - p);
                if (hashlen >= sizeof(hash_hex)) {
                    hashlen = sizeof(hash_hex) - 1;
                }
                memcpy(hash_hex, p, hashlen);
                hash_hex[hashlen] = '\0';

                /* Skip separator(s) */
                while (*sep == ' ' || *sep == '\t' || *sep == '*') {
                    sep++;
                }
                /* Rest is filename */
                strncpy(filename, sep, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';
            }

            /* Parse expected hash */
            uint8_t expected[B2SUM_MAX_DIGEST];
            if (!_b2sum_parse_hex(hash_hex, expected, B2SUM_MAX_DIGEST,
                                  &expected_len)) {
                if (opts->warn) {
                    b2sum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     b2sum_prog_name, files[fi], line_num);
                }
                if (opts->strict) {
                    exit_code = 1;
                }
                continue;
            }

            /* Compute actual hash */
            uint8_t actual[B2SUM_MAX_DIGEST];
            b2sum_opts hash_opts;
            memset(&hash_opts, 0, sizeof(hash_opts));
            hash_opts.length = expected_len * 8;
            hash_opts.binary = true;

            if (_b2sum_hash_file(filename, &hash_opts, actual) != 0) {
                if (!opts->ignore_missing) {
                    if (!opts->status) {
                        b2sum_printf("%s: FAILED open or read\n", filename);
                    }
                    exit_code = 1;
                }
                continue;
            }

            /* Compare */
            if (memcmp(expected, actual, expected_len) == 0) {
                if (!opts->quiet && !opts->status) {
                    b2sum_printf("%s: OK\n", filename);
                }
            }
            else {
                if (!opts->status) {
                    b2sum_printf("%s: FAILED\n", filename);
                }
                exit_code = 1;
            }
        }

        if (fp != stdin) {
            fclose(fp);
        }
    }

    b2sum_fflush(stdout);
    return exit_code;
}
