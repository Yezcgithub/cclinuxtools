/**
 * @file cksum.c
 * @brief Cross-platform implementation of the cksum command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils cksum(1).
 *
 * Supported algorithms:
 *   -a crc      (default) POSIX CRC-32 (Ethernet polynomial)
 *   -a crc32b   standard CRC-32 (zlib/PNG)
 *   -a sysv     System V checksum (sum -s)
 *   -a bsd      BSD checksum (sum -r)
 *   -a md5      MD5
 *   -a sha1     SHA-1
 *   -a sha224   SHA-224
 *   -a sha256   SHA-256
 *   -a sha384   SHA-384
 *   -a sha512   SHA-512
 *   -a blake2b  BLAKE2b (default 512 bits, -l for other sizes)
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -Wextra -o cksum.exe cksum.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o cksum cksum.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o cksum cksum.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -Wextra -o cksum cksum.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -Wextra -o cksum cksum.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o cksum cksum.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/cksum>
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
    #define CKSUM_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define CKSUM_PLATFORM_LINUX   1
    #define CKSUM_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define CKSUM_PLATFORM_MACOS   1
    #define CKSUM_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define CKSUM_PLATFORM_FREEBSD 1
    #define CKSUM_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define CKSUM_PLATFORM_OPENBSD 1
    #define CKSUM_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define CKSUM_PLATFORM_NETBSD  1
    #define CKSUM_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define CKSUM_PLATFORM_POSIX   1
#else
    #define CKSUM_PLATFORM_POSIX   1
#endif

#ifdef CKSUM_PLATFORM_LINUX
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef CKSUM_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef CKSUM_PLATFORM_NETBSD
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

#ifdef CKSUM_PLATFORM_WINDOWS
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

#define CKSUM_VERSION_STR "v1.0.0"
#define CKSUM_IO_BUF_SIZE 65536
#define CKSUM_MAX_DIGEST 64

/* Algorithm IDs */
#define ALG_CRC      0
#define ALG_CRC32B   1
#define ALG_SYSV     2
#define ALG_BSD      3
#define ALG_MD5      4
#define ALG_SHA1     5
#define ALG_SHA224   6
#define ALG_SHA256   7
#define ALG_SHA384   8
#define ALG_SHA512   9
#define ALG_BLAKE2B  10

/* SHA-256 constants */
#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

/* SHA-512 constants */
#define SHA512_BLOCK_SIZE 128
#define SHA512_DIGEST_SIZE 64

/* MD5 constants */
#define MD5_BLOCK_SIZE 64
#define MD5_DIGEST_SIZE 16

/* SHA-1 constants */
#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

/* BLAKE2b constants */
#define BLAKE2B_BLOCK_SIZE 128
#define BLAKE2B_OUT_BYTES_MAX 64

/********************************
 *    typedefs
 ********************************/

typedef struct {
    int algorithm;
    int length;          /* digest length in bits (for blake2b) */
    bool check;
    bool binary;
    bool tag;
    bool untagged;
    bool base64;
    bool raw;
    bool zero;
    bool ignore_missing;
    bool quiet;
    bool status;
    bool strict;
    bool warn;
    bool debug;
} cksum_opts;

/* MD5 context */
typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[MD5_BLOCK_SIZE];
} md5_ctx;

/* SHA-1 context */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[SHA1_BLOCK_SIZE];
} sha1_ctx;

/* SHA-256 context (also used for SHA-224) */
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
} sha256_ctx;

/* SHA-512 context (also used for SHA-384) */
typedef struct {
    uint64_t state[8];
    uint64_t count_lo;
    uint64_t count_hi;
    uint8_t buffer[SHA512_BLOCK_SIZE];
    size_t buflen;
} sha512_ctx;

/* BLAKE2b context */
typedef struct {
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t buf[BLAKE2B_BLOCK_SIZE];
    size_t buflen;
    size_t outlen;
} blake2b_ctx;

/* Generic hash context */
typedef struct {
    union {
        md5_ctx md5;
        sha1_ctx sha1;
        sha256_ctx sha256;
        sha512_ctx sha512;
        blake2b_ctx blake2b;
    } u;
} hash_ctx;

/* Algorithm descriptor */
typedef struct {
    const char *name;       /* display name (e.g. "MD5", "SHA-256") */
    const char *cli_name;   /* CLI name (e.g. "md5", "sha256") */
    int id;
    size_t digest_size;     /* in bytes */
    bool is_legacy;         /* legacy format (no --check, no --tag) */
    void (*init)(hash_ctx *ctx, int bits);
    void (*update)(hash_ctx *ctx, const uint8_t *data, size_t len);
    void (*final)(hash_ctx *ctx, uint8_t *digest);
} alg_info;

/********************************
 *    static prototypes
 ********************************/
static void _cksum_print_help(void);
static void _cksum_print_version(void);
static const char * _cksum_basename(const char * path);
static int _cksum_parse_opts(int argc, char ** argv,
                             cksum_opts * opts, int * file_start);
static const alg_info * _cksum_get_alg(int algorithm, int length);
static int _cksum_process_file(const char * filename, const cksum_opts * opts,
                               const alg_info * alg);
static int _cksum_check_file(const char * filename, const cksum_opts * opts,
                             const alg_info * alg);
static void _cksum_output_hex(const uint8_t *digest, size_t len);
static void _cksum_output_base64(const uint8_t *digest, size_t len);
static void _cksum_print_escaped(const char *name);

/* CRC functions */
static void _crc32_init_table(void);
static uint32_t _crc32_posix(const uint8_t *data, size_t len);
static uint32_t _crc32b(const uint8_t *data, size_t len);

/* SYSV/BSD checksums */
static uint32_t _sysv_checksum(const uint8_t *data, size_t len);
static uint32_t _bsd_checksum(const uint8_t *data, size_t len);

/* MD5 */
static void _md5_init(hash_ctx *ctx, int bits);
static void _md5_update(hash_ctx *ctx, const uint8_t *data, size_t len);
static void _md5_final(hash_ctx *ctx, uint8_t *digest);

/* SHA-1 */
static void _sha1_init(hash_ctx *ctx, int bits);
static void _sha1_update(hash_ctx *ctx, const uint8_t *data, size_t len);
static void _sha1_final(hash_ctx *ctx, uint8_t *digest);

/* SHA-256/224 */
static void _sha256_init(hash_ctx *ctx, int bits);
static void _sha256_update(hash_ctx *ctx, const uint8_t *data, size_t len);
static void _sha256_final(hash_ctx *ctx, uint8_t *digest);

/* SHA-512/384 */
static void _sha512_init(hash_ctx *ctx, int bits);
static void _sha512_update(hash_ctx *ctx, const uint8_t *data, size_t len);
static void _sha512_final(hash_ctx *ctx, uint8_t *digest);

/* BLAKE2b */
static void _blake2b_init(hash_ctx *ctx, int bits);
static void _blake2b_update(hash_ctx *ctx, const uint8_t *data, size_t len);
static void _blake2b_final(hash_ctx *ctx, uint8_t *digest);

/********************************
 *    macros
 ********************************/

#ifndef cksum_printf
    #define cksum_printf(fmt, ...) (void)printf((fmt), ##__VA_ARGS__)
#endif

#ifndef cksum_err_printf
    #define cksum_err_printf(fmt, ...) \
        do { if (stderr) { (void)fprintf(stderr, (fmt), ##__VA_ARGS__); } } while (0)
#endif

#ifndef cksum_fflush
    #define cksum_fflush(stream) \
        do { if ((stream)) { (void)fflush((stream)); } } while (0)
#endif

/* ROTate operations */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/* Endian helpers */
static uint32_t _le32_decode(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void _le32_encode(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void _be32_encode(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static void _be64_encode(uint8_t *p, uint64_t v) {
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >> 8);
    p[7] = (uint8_t)(v);
}

/********************************
 *    static variables
 ********************************/

static const char * cksum_prog_name = "cksum";

/* Algorithm table */
static const alg_info alg_table[] = {
    { "CRC",     "crc",     ALG_CRC,     4,  true,  NULL,         NULL,        NULL        },
    { "CRC32B",  "crc32b",  ALG_CRC32B,  4,  true,  NULL,         NULL,        NULL        },
    { "SYSV",    "sysv",    ALG_SYSV,    2,  true,  NULL,         NULL,        NULL        },
    { "BSD",     "bsd",     ALG_BSD,     2,  true,  NULL,         NULL,        NULL        },
    { "MD5",     "md5",     ALG_MD5,     16, false, _md5_init,    _md5_update, _md5_final  },
    { "SHA1",    "sha1",    ALG_SHA1,    20, false, _sha1_init,   _sha1_update,_sha1_final },
    { "SHA224",  "sha224",  ALG_SHA224,  28, false, _sha256_init, _sha256_update, _sha256_final },
    { "SHA256",  "sha256",  ALG_SHA256,  32, false, _sha256_init, _sha256_update, _sha256_final },
    { "SHA384",  "sha384",  ALG_SHA384,  48, false, _sha512_init, _sha512_update, _sha512_final },
    { "SHA512",  "sha512",  ALG_SHA512,  64, false, _sha512_init, _sha512_update, _sha512_final },
    { "BLAKE2b", "blake2b", ALG_BLAKE2B, 64, false, _blake2b_init,_blake2b_update,_blake2b_final },
};

static const int alg_table_size = (int)(sizeof(alg_table) / sizeof(alg_table[0]));

/* CRC-32 table (POSIX polynomial 0x04C11DB7) */
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

/* CRC-32B table (reversed polynomial 0xEDB88320) */
static uint32_t crc32b_table[256];
static bool crc32b_table_init = false;

/* Base64 alphabet */
static const char b64_alpha[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* BLAKE2b constants */
static const uint64_t blake2b_iv[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
};

static const uint8_t blake2b_sigma[12][16] = {
    { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
    { 14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3 },
    { 11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4 },
    { 7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8 },
    { 9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13 },
    { 2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9 },
    { 12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11 },
    { 13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10 },
    { 6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5 },
    { 10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0 },
    { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
    { 14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3 }
};

/* SHA-256 constants */
static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

/* SHA-512 constants */
static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

/* MD5 constants */
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

static const uint32_t md5_s[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

/********************************
 *    global functions
 ********************************/

int main(int argc, char ** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return 1;
    }

    cksum_prog_name = _cksum_basename(argv[0]);

#ifdef CKSUM_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif

    cksum_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.algorithm = ALG_CRC;
    opts.length = 0;
    opts.binary = false;
    opts.tag = false;
    opts.untagged = false;

    int file_start = 0;
    if (!_cksum_parse_opts(argc, argv, &opts, &file_start)) {
        return 1;
    }

    const alg_info * alg = _cksum_get_alg(opts.algorithm, opts.length);
    if (!alg) {
        return 1;
    }

    /* For non-legacy algorithms, default to tagged output */
    bool use_tag = opts.tag;
    bool use_untagged = opts.untagged;
    if (!alg->is_legacy && !use_tag && !use_untagged) {
        use_tag = true;  /* default for modern algorithms */
    }

    int nfiles = argc - file_start;
    int exit_code = 0;

    if (opts.check) {
        if (nfiles == 0) {
            exit_code = _cksum_check_file("-", &opts, alg);
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _cksum_check_file(argv[i], &opts, alg);
                if (rc != 0) {
                    exit_code = 1;
                }
            }
        }
    }
    else {
        /* Compute mode */
        if (opts.raw && nfiles != 1) {
            cksum_err_printf("%s: the --raw option is only supported with a single input\n",
                             cksum_prog_name);
            return 1;
        }

        if (nfiles == 0) {
            exit_code = _cksum_process_file("-", &opts, alg);
        }
        else {
            for (int i = file_start; i < argc; i++) {
                int rc = _cksum_process_file(argv[i], &opts, alg);
                if (rc != 0) {
                    exit_code = 1;
                }
            }
        }
    }

    return exit_code;
}

/********************************
 *    static functions
 ********************************/

static void _cksum_print_help(void)
{
    cksum_printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Print or verify checksums. By default use the 32 bit CRC algorithm.\n"
        "\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "\n"
        "  -a, --algorithm=TYPE    select the digest type to use. See DIGEST below\n"
        "      --base64            emit base64-encoded digests, not hexadecimal\n"
        "  -c, --check             read checksums from the FILEs and check them\n"
        "  -l, --length=BITS       digest length in bits; must not exceed the max for\n"
        "                          the blake2 algorithm and must be a multiple of 8\n"
        "      --raw               emit a raw binary digest, not hexadecimal\n"
        "      --tag               create a BSD-style checksum (the default)\n"
        "      --untagged          create a reversed style checksum, without digest type\n"
        "  -z, --zero              end each output line with NUL, not newline\n"
        "\n"
        "The following five options are useful only when verifying checksums:\n"
        "      --ignore-missing    don't fail or report status for missing files\n"
        "      --quiet             don't print OK for each successfully verified file\n"
        "      --status            don't output anything, status code shows success\n"
        "      --strict            exit non-zero for improperly formatted checksum lines\n"
        "  -w, --warn              warn about improperly formatted checksum lines\n"
        "      --debug             indicate which implementation used\n"
        "      --help              display this help and exit\n"
        "      --version           output version information and exit\n"
        "\n"
        "DIGEST determines the digest algorithm and default output format:\n"
        "  sysv        (equivalent to sum -s)\n"
        "  bsd         (equivalent to sum -r)\n"
        "  crc         (equivalent to cksum)\n"
        "  crc32b      (only available through cksum)\n"
        "  md5         (equivalent to md5sum)\n"
        "  sha1        (equivalent to sha1sum)\n"
        "  sha224      (equivalent to sha224sum)\n"
        "  sha256      (equivalent to sha256sum)\n"
        "  sha384      (equivalent to sha384sum)\n"
        "  sha512      (equivalent to sha512sum)\n"
        "  blake2b     (equivalent to b2sum)\n"
        "\n"
        "Supported platforms: Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD\n",
        cksum_prog_name
    );
}

static void _cksum_print_version(void)
{
    cksum_printf("cksum %s\n", CKSUM_VERSION_STR);
    cksum_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    cksum_printf("%s", "License MIT: <https://mit-license.org/>\n");
    cksum_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    cksum_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

static const char * _cksum_basename(const char * path)
{
    if (!path) {
        return "cksum";
    }

    const char * base = path;
    for (const char * p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static const alg_info * _cksum_get_alg(int algorithm, int length)
{
    (void)length;
    for (int i = 0; i < alg_table_size; i++) {
        if (alg_table[i].id == algorithm) {
            return &alg_table[i];
        }
    }
    return NULL;
}

static int _cksum_parse_opts(int argc, char ** argv,
                             cksum_opts * opts, int * file_start)
{
    *file_start = 1;

    for (int i = 1; i < argc; i++) {
        char * arg = argv[i];
        if (!arg) {
            continue;
        }

        /* Long options */
        if (strncmp(arg, "--", 2) == 0) {
            if (strcmp(arg, "--help") == 0) {
                _cksum_print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0) {
                _cksum_print_version();
                exit(0);
            }
            if (strcmp(arg, "--check") == 0) {
                opts->check = true;
                continue;
            }
            if (strcmp(arg, "--binary") == 0) {
                opts->binary = true;
                continue;
            }
            if (strcmp(arg, "--tag") == 0) {
                opts->tag = true;
                continue;
            }
            if (strcmp(arg, "--untagged") == 0) {
                opts->untagged = true;
                continue;
            }
            if (strcmp(arg, "--base64") == 0) {
                opts->base64 = true;
                continue;
            }
            if (strcmp(arg, "--raw") == 0) {
                opts->raw = true;
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
            if (strcmp(arg, "--debug") == 0) {
                opts->debug = true;
                continue;
            }
            if (strcmp(arg, "--text") == 0) {
                opts->binary = false;
                continue;
            }
            /* --algorithm=TYPE */
            if (strncmp(arg, "--algorithm=", 12) == 0) {
                const char * val = arg + 12;
                int alg_id = -1;
                int len = 0;
                if (strcmp(val, "crc") == 0) alg_id = ALG_CRC;
                else if (strcmp(val, "crc32b") == 0) alg_id = ALG_CRC32B;
                else if (strcmp(val, "sysv") == 0) alg_id = ALG_SYSV;
                else if (strcmp(val, "bsd") == 0) alg_id = ALG_BSD;
                else if (strcmp(val, "md5") == 0) alg_id = ALG_MD5;
                else if (strcmp(val, "sha1") == 0) alg_id = ALG_SHA1;
                else if (strcmp(val, "sha224") == 0) alg_id = ALG_SHA224;
                else if (strcmp(val, "sha256") == 0) alg_id = ALG_SHA256;
                else if (strcmp(val, "sha384") == 0) alg_id = ALG_SHA384;
                else if (strcmp(val, "sha512") == 0) alg_id = ALG_SHA512;
                else if (strcmp(val, "blake2b") == 0) { alg_id = ALG_BLAKE2B; len = 512; }
                else if (strcmp(val, "sha2") == 0) {
                    cksum_err_printf("%s: --algorithm=sha2 requires --length\n", cksum_prog_name);
                    return false;
                }
                else {
                    cksum_err_printf("%s: invalid argument '%s' for '--algorithm'\n",
                                     cksum_prog_name, val);
                    return false;
                }
                opts->algorithm = alg_id;
                if (len > 0) opts->length = len;
                continue;
            }
            /* --algorithm TYPE */
            if (strcmp(arg, "--algorithm") == 0) {
                if (i + 1 >= argc) {
                    cksum_err_printf("%s: option '--algorithm' requires an argument\n",
                                     cksum_prog_name);
                    return false;
                }
                const char * val = argv[++i];
                int alg_id = -1;
                int len = 0;
                if (strcmp(val, "crc") == 0) alg_id = ALG_CRC;
                else if (strcmp(val, "crc32b") == 0) alg_id = ALG_CRC32B;
                else if (strcmp(val, "sysv") == 0) alg_id = ALG_SYSV;
                else if (strcmp(val, "bsd") == 0) alg_id = ALG_BSD;
                else if (strcmp(val, "md5") == 0) alg_id = ALG_MD5;
                else if (strcmp(val, "sha1") == 0) alg_id = ALG_SHA1;
                else if (strcmp(val, "sha224") == 0) alg_id = ALG_SHA224;
                else if (strcmp(val, "sha256") == 0) alg_id = ALG_SHA256;
                else if (strcmp(val, "sha384") == 0) alg_id = ALG_SHA384;
                else if (strcmp(val, "sha512") == 0) alg_id = ALG_SHA512;
                else if (strcmp(val, "blake2b") == 0) { alg_id = ALG_BLAKE2B; len = 512; }
                else {
                    cksum_err_printf("%s: invalid argument '%s' for '--algorithm'\n",
                                     cksum_prog_name, val);
                    return false;
                }
                opts->algorithm = alg_id;
                if (len > 0) opts->length = len;
                continue;
            }
            /* --length=BITS */
            if (strncmp(arg, "--length=", 9) == 0) {
                char * end = NULL;
                long bits = strtol(arg + 9, &end, 10);
                if (errno != 0 || !end || *end != '\0' || bits <= 0 || bits % 8 != 0) {
                    cksum_err_printf("%s: invalid length '%s'\n", cksum_prog_name, arg + 9);
                    return false;
                }
                opts->length = (int)bits;
                continue;
            }
            if (strcmp(arg, "--length") == 0) {
                if (i + 1 >= argc) {
                    cksum_err_printf("%s: option '--length' requires an argument\n",
                                     cksum_prog_name);
                    return false;
                }
                char * end = NULL;
                long bits = strtol(argv[i + 1], &end, 10);
                if (errno != 0 || !end || *end != '\0' || bits <= 0 || bits % 8 != 0) {
                    cksum_err_printf("%s: invalid length '%s'\n", cksum_prog_name, argv[i + 1]);
                    return false;
                }
                opts->length = (int)bits;
                i++;
                continue;
            }

            cksum_err_printf("%s: unrecognized option '%s'\n", cksum_prog_name, arg);
            cksum_err_printf("%s", "Try 'cksum --help' for more information.\n");
            return false;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                switch (arg[j]) {
                    case 'c':
                        opts->check = true;
                        break;
                    case 'b':
                        opts->binary = true;
                        break;
                    case 't':
                        opts->binary = false;
                        break;
                    case 'z':
                        opts->zero = true;
                        break;
                    case 'w':
                        opts->warn = true;
                        break;
                    case 'a':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = &arg[j + 1];
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        else {
                            cksum_err_printf("%s: option '-a' requires an argument\n",
                                             cksum_prog_name);
                            return false;
                        }
                        int alg_id = -1;
                        int len = 0;
                        if (strcmp(val, "crc") == 0) alg_id = ALG_CRC;
                        else if (strcmp(val, "crc32b") == 0) alg_id = ALG_CRC32B;
                        else if (strcmp(val, "sysv") == 0) alg_id = ALG_SYSV;
                        else if (strcmp(val, "bsd") == 0) alg_id = ALG_BSD;
                        else if (strcmp(val, "md5") == 0) alg_id = ALG_MD5;
                        else if (strcmp(val, "sha1") == 0) alg_id = ALG_SHA1;
                        else if (strcmp(val, "sha224") == 0) alg_id = ALG_SHA224;
                        else if (strcmp(val, "sha256") == 0) alg_id = ALG_SHA256;
                        else if (strcmp(val, "sha384") == 0) alg_id = ALG_SHA384;
                        else if (strcmp(val, "sha512") == 0) alg_id = ALG_SHA512;
                        else if (strcmp(val, "blake2b") == 0) { alg_id = ALG_BLAKE2B; len = 512; }
                        else {
                            cksum_err_printf("%s: invalid argument '%s' for '-a'\n",
                                             cksum_prog_name, val);
                            return false;
                        }
                        opts->algorithm = alg_id;
                        if (len > 0) opts->length = len;
                        goto next_arg;
                    }
                    case 'l':
                    {
                        const char * val = NULL;
                        if (arg[j + 1] != '\0') {
                            val = &arg[j + 1];
                        }
                        else if (i + 1 < argc) {
                            val = argv[++i];
                        }
                        else {
                            cksum_err_printf("%s: option '-l' requires an argument\n",
                                             cksum_prog_name);
                            return false;
                        }
                        char * end = NULL;
                        long bits = strtol(val, &end, 10);
                        if (errno != 0 || !end || *end != '\0' || bits <= 0 || bits % 8 != 0) {
                            cksum_err_printf("%s: invalid length '%s'\n", cksum_prog_name, val);
                            return false;
                        }
                        opts->length = (int)bits;
                        goto next_arg;
                    }
                    default:
                        cksum_err_printf("%s: invalid option -- '%c'\n", cksum_prog_name, arg[j]);
                        cksum_err_printf("%s", "Try 'cksum --help' for more information.\n");
                        return false;
                }
            }
            next_arg:
            continue;
        }

        *file_start = i;
        break;
    }

    return true;
}

/* ---- CRC-32 (POSIX) ---- */

static void _crc32_init_table(void)
{
    if (crc32_table_init) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++) {
            if (c & 0x80000000U) {
                c = (c << 1) ^ 0x04C11DB7U;
            }
            else {
                c <<= 1;
            }
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

static uint32_t _crc32_posix(const uint8_t *data, size_t len)
{
    _crc32_init_table();
    uint32_t crc = 0;

    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ data[i]];
    }

    /* Process the length */
    uint64_t n = len;
    do {
        crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ (uint32_t)(n & 0xFF)];
        n >>= 8;
    } while (n > 0);

    return ~crc;
}

/* ---- CRC-32B (standard, zlib/PNG) ---- */

static void _crc32b_init_table(void)
{
    if (crc32b_table_init) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = (c >> 1) ^ 0xEDB88320U;
            }
            else {
                c >>= 1;
            }
        }
        crc32b_table[i] = c;
    }
    crc32b_table_init = true;
}

static uint32_t _crc32b(const uint8_t *data, size_t len)
{
    _crc32b_init_table();
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32b_table[(crc ^ data[i]) & 0xFF];
    }

    return ~crc;
}

/* ---- SYSV checksum ---- */

static uint32_t _sysv_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum & 0xFFFF;
}

/* ---- BSD checksum ---- */

static uint32_t _bsd_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (sum >> 1) | ((sum & 1) << 15);
        sum += data[i];
        sum &= 0xFFFF;
    }
    return sum;
}

/* ---- MD5 ---- */

static void _md5_init(hash_ctx *ctx, int bits)
{
    (void)bits;
    ctx->u.md5.state[0] = 0x67452301;
    ctx->u.md5.state[1] = 0xEFCDAB89;
    ctx->u.md5.state[2] = 0x98BADCFE;
    ctx->u.md5.state[3] = 0x10325476;
    ctx->u.md5.count = 0;
}

static void _md5_transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
        M[i] = _le32_decode(block + i * 4);
    }

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        }
        else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        }
        else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        }
        else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
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

static void _md5_update(hash_ctx *ctx, const uint8_t *data, size_t len)
{
    md5_ctx *s = &ctx->u.md5;
    size_t used = (size_t)(s->count % MD5_BLOCK_SIZE);
    s->count += len;

    if (used > 0) {
        size_t space = MD5_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(s->buffer + used, data, len);
            return;
        }
        memcpy(s->buffer + used, data, space);
        _md5_transform(s->state, s->buffer);
        data += space;
        len -= space;
    }

    while (len >= MD5_BLOCK_SIZE) {
        _md5_transform(s->state, data);
        data += MD5_BLOCK_SIZE;
        len -= MD5_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(s->buffer, data, len);
    }
}

static void _md5_final(hash_ctx *ctx, uint8_t *digest)
{
    md5_ctx *s = &ctx->u.md5;
    size_t used = (size_t)(s->count % MD5_BLOCK_SIZE);
    uint64_t bit_count = s->count * 8;

    s->buffer[used++] = 0x80;
    if (used > 56) {
        memset(s->buffer + used, 0, MD5_BLOCK_SIZE - used);
        _md5_transform(s->state, s->buffer);
        used = 0;
    }
    memset(s->buffer + used, 0, 56 - used);
    _le32_encode(s->buffer + 56, (uint32_t)(bit_count));
    _le32_encode(s->buffer + 60, (uint32_t)(bit_count >> 32));
    _md5_transform(s->state, s->buffer);

    for (int i = 0; i < 4; i++) {
        _le32_encode(digest + i * 4, s->state[i]);
    }
}

/* ---- SHA-1 ---- */

static void _sha1_init(hash_ctx *ctx, int bits)
{
    (void)bits;
    ctx->u.sha1.state[0] = 0x67452301;
    ctx->u.sha1.state[1] = 0xEFCDAB89;
    ctx->u.sha1.state[2] = 0x98BADCFE;
    ctx->u.sha1.state[3] = 0x10325476;
    ctx->u.sha1.state[4] = 0xC3D2E1F0;
    ctx->u.sha1.count = 0;
}

static void _sha1_transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = ROTL32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        }
        else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        }
        else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        }
        else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = ROTL32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROTL32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void _sha1_update(hash_ctx *ctx, const uint8_t *data, size_t len)
{
    sha1_ctx *s = &ctx->u.sha1;
    size_t used = (size_t)(s->count % SHA1_BLOCK_SIZE);
    s->count += len;

    if (used > 0) {
        size_t space = SHA1_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(s->buffer + used, data, len);
            return;
        }
        memcpy(s->buffer + used, data, space);
        _sha1_transform(s->state, s->buffer);
        data += space;
        len -= space;
    }

    while (len >= SHA1_BLOCK_SIZE) {
        _sha1_transform(s->state, data);
        data += SHA1_BLOCK_SIZE;
        len -= SHA1_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(s->buffer, data, len);
    }
}

static void _sha1_final(hash_ctx *ctx, uint8_t *digest)
{
    sha1_ctx *s = &ctx->u.sha1;
    size_t used = (size_t)(s->count % SHA1_BLOCK_SIZE);
    uint64_t bit_count = s->count * 8;

    s->buffer[used++] = 0x80;
    if (used > 56) {
        memset(s->buffer + used, 0, SHA1_BLOCK_SIZE - used);
        _sha1_transform(s->state, s->buffer);
        used = 0;
    }
    memset(s->buffer + used, 0, 56 - used);
    _be64_encode(s->buffer + 56, bit_count);
    _sha1_transform(s->state, s->buffer);

    for (int i = 0; i < 5; i++) {
        _be32_encode(digest + i * 4, s->state[i]);
    }
}

/* ---- SHA-256/224 ---- */

static void _sha256_init(hash_ctx *ctx, int bits)
{
    sha256_ctx *s = &ctx->u.sha256;
    if (bits == 224) {
        s->state[0] = 0xc1059ed8; s->state[1] = 0x367cd507;
        s->state[2] = 0x3070dd17; s->state[3] = 0xf70e5939;
        s->state[4] = 0xffc00b31; s->state[5] = 0x68581511;
        s->state[6] = 0x64f98fa7; s->state[7] = 0xbefa4fa4;
    }
    else {
        s->state[0] = 0x6a09e667; s->state[1] = 0xbb67ae85;
        s->state[2] = 0x3c6ef372; s->state[3] = 0xa54ff53a;
        s->state[4] = 0x510e527f; s->state[5] = 0x9b05688c;
        s->state[6] = 0x1f83d9ab; s->state[7] = 0x5be0cd19;
    }
    s->count = 0;
}

static void _sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR32(w[i-15], 7) ^ ROTR32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROTR32(w[i-2], 17) ^ ROTR32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint32_t e=state[4], f=state[5], g=state[6], h=state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void _sha256_update(hash_ctx *ctx, const uint8_t *data, size_t len)
{
    sha256_ctx *s = &ctx->u.sha256;
    size_t used = (size_t)(s->count % SHA256_BLOCK_SIZE);
    s->count += len;

    if (used > 0) {
        size_t space = SHA256_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(s->buffer + used, data, len);
            return;
        }
        memcpy(s->buffer + used, data, space);
        _sha256_transform(s->state, s->buffer);
        data += space;
        len -= space;
    }

    while (len >= SHA256_BLOCK_SIZE) {
        _sha256_transform(s->state, data);
        data += SHA256_BLOCK_SIZE;
        len -= SHA256_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(s->buffer, data, len);
    }
}

static void _sha256_final(hash_ctx *ctx, uint8_t *digest)
{
    sha256_ctx *s = &ctx->u.sha256;
    size_t used = (size_t)(s->count % SHA256_BLOCK_SIZE);
    uint64_t bit_count = s->count * 8;

    s->buffer[used++] = 0x80;
    if (used > 56) {
        memset(s->buffer + used, 0, SHA256_BLOCK_SIZE - used);
        _sha256_transform(s->state, s->buffer);
        used = 0;
    }
    memset(s->buffer + used, 0, 56 - used);
    _be64_encode(s->buffer + 56, bit_count);
    _sha256_transform(s->state, s->buffer);

    for (int i = 0; i < 8; i++) {
        _be32_encode(digest + i * 4, s->state[i]);
    }
}

/* ---- SHA-512/384 ---- */

static void _sha512_init(hash_ctx *ctx, int bits)
{
    sha512_ctx *s = &ctx->u.sha512;
    if (bits == 384) {
        s->state[0] = 0xcbbb9d5dc1059ed8ULL; s->state[1] = 0x629a292a367cd507ULL;
        s->state[2] = 0x9159015a3070dd17ULL; s->state[3] = 0x152fecd8f70e5939ULL;
        s->state[4] = 0x67332667ffc00b31ULL; s->state[5] = 0x8eb44a8768581511ULL;
        s->state[6] = 0xdb0c2e0d64f98fa7ULL; s->state[7] = 0x47b5481dbefa4fa4ULL;
    }
    else {
        s->state[0] = 0x6a09e667f3bcc908ULL; s->state[1] = 0xbb67ae8584caa73bULL;
        s->state[2] = 0x3c6ef372fe94f82bULL; s->state[3] = 0xa54ff53a5f1d36f1ULL;
        s->state[4] = 0x510e527fade682d1ULL; s->state[5] = 0x9b05688c2b3e6c1fULL;
        s->state[6] = 0x1f83d9abfb41bd6bULL; s->state[7] = 0x5be0cd19137e2179ULL;
    }
    s->count_lo = 0;
    s->count_hi = 0;
    s->buflen = 0;
}

static void _sha512_transform(uint64_t state[8], const uint8_t block[128])
{
    uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = 0;
        for (int j = 0; j < 8; j++) {
            w[i] = (w[i] << 8) | block[i * 8 + j];
        }
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = ROTR64(w[i-15], 1) ^ ROTR64(w[i-15], 8) ^ (w[i-15] >> 7);
        uint64_t s1 = ROTR64(w[i-2], 19) ^ ROTR64(w[i-2], 61) ^ (w[i-2] >> 6);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint64_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint64_t e=state[4], f=state[5], g=state[6], h=state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t S1 = ROTR64(e, 14) ^ ROTR64(e, 18) ^ ROTR64(e, 41);
        uint64_t ch = (e & f) ^ (~e & g);
        uint64_t temp1 = h + S1 + ch + sha512_k[i] + w[i];
        uint64_t S0 = ROTR64(a, 28) ^ ROTR64(a, 34) ^ ROTR64(a, 39);
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void _sha512_update(hash_ctx *ctx, const uint8_t *data, size_t len)
{
    sha512_ctx *s = &ctx->u.sha512;

    /* Update bit count (128-bit, but we only track 128-bit in two 64-bit halves) */
    uint64_t bits = (uint64_t)len * 8;
    s->count_lo += bits;
    if (s->count_lo < bits) {
        s->count_hi++;
    }
    /* Also need to account for the high part of the byte count */
    s->count_hi += (uint64_t)len >> 61;

    size_t used = s->buflen;
    s->buflen = 0;

    if (used > 0) {
        size_t space = SHA512_BLOCK_SIZE - used;
        if (len < space) {
            memcpy(s->buffer + used, data, len);
            s->buflen = used + len;
            return;
        }
        memcpy(s->buffer + used, data, space);
        _sha512_transform(s->state, s->buffer);
        data += space;
        len -= space;
    }

    while (len >= SHA512_BLOCK_SIZE) {
        _sha512_transform(s->state, data);
        data += SHA512_BLOCK_SIZE;
        len -= SHA512_BLOCK_SIZE;
    }

    if (len > 0) {
        memcpy(s->buffer, data, len);
        s->buflen = len;
    }
}

static void _sha512_final(hash_ctx *ctx, uint8_t *digest)
{
    sha512_ctx *s = &ctx->u.sha512;
    size_t used = s->buflen;

    s->buffer[used++] = 0x80;
    if (used > 112) {
        memset(s->buffer + used, 0, SHA512_BLOCK_SIZE - used);
        _sha512_transform(s->state, s->buffer);
        used = 0;
    }
    memset(s->buffer + used, 0, 112 - used);
    _be64_encode(s->buffer + 112, s->count_hi);
    _be64_encode(s->buffer + 120, s->count_lo);
    _sha512_transform(s->state, s->buffer);

    for (int i = 0; i < 8; i++) {
        _be64_encode(digest + i * 8, s->state[i]);
    }
}

/* ---- BLAKE2b ---- */

static void _blake2b_set_lastnode(blake2b_ctx *ctx)
{
    ctx->f[0] = (uint64_t)-1;
}

static void _blake2b_increment_counter(blake2b_ctx *ctx, uint64_t inc)
{
    ctx->t[0] += inc;
    if (ctx->t[0] < inc) {
        ctx->t[1]++;
    }
}

static uint64_t _blake2b_load64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (uint64_t)p[i] << (i * 8);
    }
    return v;
}

static void _blake2b_store64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)(v >> (i * 8));
    }
}

/* BLAKE2b compress - proper implementation */
static void _blake2b_compress(blake2b_ctx *ctx, const uint8_t block[BLAKE2B_BLOCK_SIZE])
{
    uint64_t m[16];
    uint64_t v[16];

    for (int i = 0; i < 16; i++) {
        m[i] = _blake2b_load64(block + i * 8);
    }

    for (int i = 0; i < 8; i++) {
        v[i] = ctx->h[i];
    }
    v[8]  = blake2b_iv[0];
    v[9]  = blake2b_iv[1];
    v[10] = blake2b_iv[2];
    v[11] = blake2b_iv[3];
    v[12] = blake2b_iv[4] ^ ctx->t[0];
    v[13] = blake2b_iv[5] ^ ctx->t[1];
    v[14] = blake2b_iv[6] ^ ctx->f[0];
    v[15] = blake2b_iv[7] ^ ctx->f[1];

#define G(r, i, a, b, c, d) \
    do { \
        a = a + b + m[blake2b_sigma[r][2*i]]; \
        d = ROTR64(d ^ a, 32); \
        c = c + d; \
        b = ROTR64(b ^ c, 24); \
        a = a + b + m[blake2b_sigma[r][2*i+1]]; \
        d = ROTR64(d ^ a, 16); \
        c = c + d; \
        b = ROTR64(b ^ c, 63); \
    } while (0)

#define ROUND(r) \
    do { \
        G(r, 0, v[0], v[4], v[8],  v[12]); \
        G(r, 1, v[1], v[5], v[9],  v[13]); \
        G(r, 2, v[2], v[6], v[10], v[14]); \
        G(r, 3, v[3], v[7], v[11], v[15]); \
        G(r, 4, v[0], v[5], v[10], v[15]); \
        G(r, 5, v[1], v[6], v[11], v[12]); \
        G(r, 6, v[2], v[7], v[8],  v[13]); \
        G(r, 7, v[3], v[4], v[9],  v[14]); \
    } while (0)

    ROUND(0); ROUND(1); ROUND(2); ROUND(3);
    ROUND(4); ROUND(5); ROUND(6); ROUND(7);
    ROUND(8); ROUND(9); ROUND(10); ROUND(11);

#undef G
#undef ROUND

    for (int i = 0; i < 8; i++) {
        ctx->h[i] ^= v[i] ^ v[i + 8];
    }
}

static void _blake2b_init(hash_ctx *ctx, int bits)
{
    blake2b_ctx *s = &ctx->u.blake2b;
    size_t outlen;

    if (bits <= 0) {
        outlen = BLAKE2B_OUT_BYTES_MAX;
    }
    else {
        outlen = (size_t)bits / 8;
        if (outlen == 0 || outlen > BLAKE2B_OUT_BYTES_MAX) {
            outlen = BLAKE2B_OUT_BYTES_MAX;
        }
    }

    memset(s, 0, sizeof(*s));
    for (int i = 0; i < 8; i++) {
        s->h[i] = blake2b_iv[i];
    }
    s->h[0] ^= 0x01010000U ^ (uint64_t)outlen;
    s->outlen = outlen;
}

static void _blake2b_update(hash_ctx *ctx, const uint8_t *data, size_t len)
{
    blake2b_ctx *s = &ctx->u.blake2b;

    while (len > 0) {
        size_t need = BLAKE2B_BLOCK_SIZE - s->buflen;
        size_t take = len < need ? len : need;

        memcpy(s->buf + s->buflen, data, take);
        s->buflen += take;
        data += take;
        len -= take;

        if (s->buflen == BLAKE2B_BLOCK_SIZE && len > 0) {
            _blake2b_increment_counter(s, BLAKE2B_BLOCK_SIZE);
            _blake2b_compress(s, s->buf);
            s->buflen = 0;
        }
    }
}

static void _blake2b_final(hash_ctx *ctx, uint8_t *digest)
{
    blake2b_ctx *s = &ctx->u.blake2b;

    /* Pad remaining buffer with zeros */
    memset(s->buf + s->buflen, 0, BLAKE2B_BLOCK_SIZE - s->buflen);
    _blake2b_increment_counter(s, (uint64_t)s->buflen);
    _blake2b_set_lastnode(s);
    _blake2b_compress(s, s->buf);

    /* Output */
    uint8_t tmp[BLAKE2B_OUT_BYTES_MAX];
    for (int i = 0; i < 8; i++) {
        _blake2b_store64(tmp + i * 8, s->h[i]);
    }
    memcpy(digest, tmp, s->outlen);
}

/* ---- Output helpers ---- */

static void _cksum_output_hex(const uint8_t *digest, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        putchar(hex[digest[i] >> 4]);
        putchar(hex[digest[i] & 0x0F]);
    }
}

static void _cksum_output_base64(const uint8_t *digest, size_t len)
{
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = 0;
        int rem = (int)(len - i);
        int chars;

        if (rem >= 3) {
            v = ((uint32_t)digest[i] << 16) | ((uint32_t)digest[i+1] << 8) | digest[i+2];
            chars = 4;
        }
        else if (rem == 2) {
            v = ((uint32_t)digest[i] << 16) | ((uint32_t)digest[i+1] << 8);
            chars = 3;
        }
        else {
            v = (uint32_t)digest[i] << 16;
            chars = 2;
        }

        for (int j = 0; j < 4; j++) {
            if (j < chars) {
                putchar(b64_alpha[(v >> (18 - j * 6)) & 0x3F]);
            }
            else {
                putchar('=');
            }
        }
    }
}

static void _cksum_print_escaped(const char *name)
{
    bool need_escape = false;
    for (const char *p = name; *p; p++) {
        if (*p == '\\' || *p == '\n' || *p == '\r') {
            need_escape = true;
            break;
        }
    }

    if (need_escape) {
        putchar('\\');
        for (const char *p = name; *p; p++) {
            if (*p == '\\' || *p == '\n' || *p == '\r') {
                putchar('\\');
            }
            putchar(*p);
        }
    }
    else {
        fputs(name, stdout);
    }
}

/* ---- File processing ---- */

static int _cksum_process_file(const char * filename, const cksum_opts * opts,
                               const alg_info * alg)
{
    FILE * fp;
    bool is_stdin;

    if (strcmp(filename, "-") == 0) {
        fp = stdin;
        is_stdin = true;
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            cksum_err_printf("%s: %s: %s\n", cksum_prog_name,
                             filename, strerror(errno));
            return 1;
        }
        is_stdin = false;
    }

    uint8_t * buf = (uint8_t *)malloc(CKSUM_IO_BUF_SIZE);
    if (!buf) {
        cksum_err_printf("%s: out of memory\n", cksum_prog_name);
        if (!is_stdin) fclose(fp);
        return 1;
    }

    int exit_code = 0;

    if (alg->is_legacy) {
        /* Legacy algorithms: read entire file, compute checksum */
        size_t total_len = 0;
        /* For large files, we need to stream. Use dynamic buffer. */
        size_t cap = CKSUM_IO_BUF_SIZE;
        uint8_t * all = (uint8_t *)malloc(cap);
        size_t all_len = 0;

        if (!all && cap > 0) {
            cksum_err_printf("%s: out of memory\n", cksum_prog_name);
            free(buf);
            if (!is_stdin) fclose(fp);
            return 1;
        }

        size_t n;
        while ((n = fread(buf, 1, CKSUM_IO_BUF_SIZE, fp)) > 0) {
            if (all_len + n > cap) {
                while (all_len + n > cap) {
                    cap *= 2;
                }
                uint8_t * tmp = (uint8_t *)realloc(all, cap);
                if (!tmp) {
                    cksum_err_printf("%s: out of memory\n", cksum_prog_name);
                    free(all);
                    free(buf);
                    if (!is_stdin) fclose(fp);
                    return 1;
                }
                all = tmp;
            }
            memcpy(all + all_len, buf, n);
            all_len += n;
        }
        total_len = all_len;

        if (ferror(fp)) {
            cksum_err_printf("%s: %s: read error\n", cksum_prog_name, filename);
            exit_code = 1;
        }

        if (exit_code == 0) {
            if (alg->id == ALG_CRC) {
                uint32_t crc = _crc32_posix(all, total_len);
                if (is_stdin) {
                    cksum_printf("%" PRIu32 " %" PRIu64 "\n", crc, (uint64_t)total_len);
                }
                else {
                    cksum_printf("%" PRIu32 " %" PRIu64 " %s\n", crc, (uint64_t)total_len, filename);
                }
            }
            else if (alg->id == ALG_CRC32B) {
                uint32_t crc = _crc32b(all, total_len);
                if (is_stdin) {
                    cksum_printf("%" PRIu32 " %" PRIu64 "\n", crc, (uint64_t)total_len);
                }
                else {
                    cksum_printf("%" PRIu32 " %" PRIu64 " %s\n", crc, (uint64_t)total_len, filename);
                }
            }
            else if (alg->id == ALG_SYSV) {
                uint32_t sum = _sysv_checksum(all, total_len);
                uint64_t blocks = (total_len + 511) / 512;
                if (total_len == 0) blocks = 0;
                if (is_stdin) {
                    cksum_printf("%" PRIu32 " %" PRIu64 "\n", sum, blocks);
                }
                else {
                    cksum_printf("%" PRIu32 " %" PRIu64 " %s\n", sum, blocks, filename);
                }
            }
            else if (alg->id == ALG_BSD) {
                uint32_t sum = _bsd_checksum(all, total_len);
                uint64_t blocks = (total_len + 1023) / 1024;
                if (total_len == 0) blocks = 0;
                if (is_stdin) {
                    cksum_printf("%05" PRIu32 " %5" PRIu64 "\n", sum, blocks);
                }
                else {
                    cksum_printf("%05" PRIu32 " %5" PRIu64 " %s\n", sum, blocks, filename);
                }
            }
        }

        free(all);
    }
    else {
        /* Modern hash algorithms: stream through hash function */
        hash_ctx hctx;
        int init_bits = 0;

        if (alg->id == ALG_SHA224) init_bits = 224;
        else if (alg->id == ALG_SHA384) init_bits = 384;
        else if (alg->id == ALG_BLAKE2B) {
            init_bits = opts->length > 0 ? opts->length : 512;
        }

        alg->init(&hctx, init_bits);

        size_t n;
        while ((n = fread(buf, 1, CKSUM_IO_BUF_SIZE, fp)) > 0) {
            alg->update(&hctx, buf, n);
        }

        if (ferror(fp)) {
            cksum_err_printf("%s: %s: read error\n", cksum_prog_name, filename);
            exit_code = 1;
        }

        if (exit_code == 0) {
            size_t digest_size = alg->digest_size;
            if (alg->id == ALG_BLAKE2B && opts->length > 0) {
                digest_size = (size_t)opts->length / 8;
            }

            uint8_t digest[CKSUM_MAX_DIGEST];
            alg->final(&hctx, digest);

            if (opts->raw) {
                fwrite(digest, 1, digest_size, stdout);
            }
            else {
                bool use_tag = opts->tag;
                bool use_untagged = opts->untagged;
                if (!use_tag && !use_untagged) {
                    use_tag = true;  /* default for modern algorithms */
                }

                if (use_tag) {
                    /* Tagged format: ALG (filename) = digest */
                    cksum_printf("%s ", alg->name);
                    if (!is_stdin) {
                        if (opts->zero) {
                            cksum_printf("(%s) = ", filename);
                        }
                        else {
                            cksum_printf("(");
                            _cksum_print_escaped(filename);
                            cksum_printf(") = ");
                        }
                    }
                    else {
                        cksum_printf("= ");
                    }

                    if (opts->base64) {
                        _cksum_output_base64(digest, digest_size);
                    }
                    else {
                        _cksum_output_hex(digest, digest_size);
                    }

                    if (opts->zero) {
                        putchar('\0');
                    }
                    else {
                        putchar('\n');
                    }
                }
                else {
                    /* Untagged format: digest  filename */
                    if (opts->base64) {
                        _cksum_output_base64(digest, digest_size);
                    }
                    else {
                        _cksum_output_hex(digest, digest_size);
                    }

                    if (!is_stdin) {
                        putchar(' ');
                        putchar(opts->binary ? '*' : ' ');
                        if (opts->zero) {
                            fputs(filename, stdout);
                            putchar('\0');
                        }
                        else {
                            _cksum_print_escaped(filename);
                            putchar('\n');
                        }
                    }
                    else {
                        if (opts->zero) {
                            putchar('\0');
                        }
                        else {
                            putchar('\n');
                        }
                    }
                }
            }
        }
    }

    free(buf);
    if (!is_stdin) {
        fclose(fp);
    }

    cksum_fflush(stdout);
    return exit_code;
}

/* ---- Check mode ---- */

static int _cksum_compute_digest(const char * filename, const alg_info * alg,
                                 int length, uint8_t *digest, size_t *digest_size)
{
    FILE * fp;
    if (strcmp(filename, "-") == 0) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "rb");
        if (!fp) {
            return -1;
        }
    }

    uint8_t buf[CKSUM_IO_BUF_SIZE];
    hash_ctx hctx;
    int init_bits = 0;

    if (alg->id == ALG_SHA224) init_bits = 224;
    else if (alg->id == ALG_SHA384) init_bits = 384;
    else if (alg->id == ALG_BLAKE2B) {
        init_bits = length > 0 ? length : 512;
    }

    alg->init(&hctx, init_bits);

    size_t n;
    while ((n = fread(buf, 1, CKSUM_IO_BUF_SIZE, fp)) > 0) {
        alg->update(&hctx, buf, n);
    }

    int rc = 0;
    if (ferror(fp)) {
        rc = -1;
    }

    if (fp != stdin) {
        fclose(fp);
    }

    if (rc == 0) {
        alg->final(&hctx, digest);
        *digest_size = alg->digest_size;
        if (alg->id == ALG_BLAKE2B && length > 0) {
            *digest_size = (size_t)length / 8;
        }
    }

    return rc;
}

static int _hex_to_bytes(const char *hex, uint8_t *bytes, size_t max_len, size_t *out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0) {
        return -1;
    }
    size_t n = len / 2;
    if (n > max_len) {
        return -1;
    }

    for (size_t i = 0; i < n; i++) {
        int hi = -1, lo = -1;
        char c = hex[i * 2];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
        c = hex[i * 2 + 1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;

        if (hi < 0 || lo < 0) {
            return -1;
        }
        bytes[i] = (uint8_t)((hi << 4) | lo);
    }

    *out_len = n;
    return 0;
}

static int _cksum_check_file(const char * filename, const cksum_opts * opts,
                             const alg_info * alg)
{
    FILE * fp;
    if (strcmp(filename, "-") == 0) {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (!fp) {
            cksum_err_printf("%s: %s: %s\n", cksum_prog_name,
                             filename, strerror(errno));
            return 1;
        }
    }

    char line[8192];
    int exit_code = 0;
    int line_num = 0;
    bool any_parsed = false;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        }

        if (len == 0) {
            if (opts->warn) {
                cksum_err_printf("%s: %s: %d: no properly formatted checksum lines found\n",
                                 cksum_prog_name, filename, line_num);
            }
            if (opts->strict) {
                exit_code = 1;
            }
            continue;
        }

        /* Skip BOM */
        if (line_num == 1 && len >= 3 &&
            (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF) {
            memmove(line, line + 3, len - 3 + 1);
            len -= 3;
            if (len == 0) continue;
        }

        /* Try to parse tagged format: ALG (filename) = digest */
        char parsed_alg[32] = "";
        char parsed_file[4096] = "";
        char parsed_digest[256] = "";
        bool is_tagged = false;
        bool is_base64 = false;

        /* Check for tagged format */
        char * paren = strchr(line, '(');
        char * eq = strchr(line, '=');
        if (paren && eq && paren < eq) {
            /* Extract algorithm name */
            size_t alg_len = (size_t)(paren - line);
            while (alg_len > 0 && line[alg_len - 1] == ' ') {
                alg_len--;
            }
            if (alg_len > 0 && alg_len < sizeof(parsed_alg)) {
                memcpy(parsed_alg, line, alg_len);
                parsed_alg[alg_len] = '\0';
                is_tagged = true;
            }
        }

        if (is_tagged) {
            /* Extract filename from between parentheses */
            char * close_paren = strchr(paren + 1, ')');
            if (!close_paren || close_paren > eq) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t file_len = (size_t)(close_paren - paren - 1);
            if (file_len < sizeof(parsed_file)) {
                memcpy(parsed_file, paren + 1, file_len);
                parsed_file[file_len] = '\0';
            }

            /* Extract digest */
            char * digest_start = eq + 1;
            while (*digest_start == ' ') digest_start++;
            strncpy(parsed_digest, digest_start, sizeof(parsed_digest) - 1);
            parsed_digest[sizeof(parsed_digest) - 1] = '\0';
            /* Trim trailing whitespace */
            size_t dl = strlen(parsed_digest);
            while (dl > 0 && (parsed_digest[dl-1] == ' ' || parsed_digest[dl-1] == '\r')) {
                parsed_digest[--dl] = '\0';
            }

            /* Auto-detect algorithm from tag name */
            const alg_info * check_alg = alg;
            for (int i = 0; i < alg_table_size; i++) {
                if (strcasecmp(alg_table[i].name, parsed_alg) == 0) {
                    check_alg = &alg_table[i];
                    break;
                }
            }

            if (check_alg->is_legacy) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            /* Check if digest is base64 or hex */
            bool looks_hex = true;
            for (size_t i = 0; i < strlen(parsed_digest); i++) {
                char c = parsed_digest[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    looks_hex = false;
                    break;
                }
            }

            uint8_t expected[CKSUM_MAX_DIGEST];
            size_t expected_len = 0;

            if (looks_hex) {
                if (_hex_to_bytes(parsed_digest, expected, sizeof(expected), &expected_len) != 0) {
                    if (opts->warn) {
                        cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                         cksum_prog_name, filename, line_num);
                    }
                    if (opts->strict) exit_code = 1;
                    continue;
                }
            }
            else {
                is_base64 = true;
                /* Decode base64 */
                size_t dl2 = strlen(parsed_digest);
                if (dl2 % 4 != 0) {
                    if (opts->warn) {
                        cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                         cksum_prog_name, filename, line_num);
                    }
                    if (opts->strict) exit_code = 1;
                    continue;
                }
                /* Simple base64 decode */
                static const int b64_dec[256] = {
                    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
                    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
                    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
                    ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
                    ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
                    ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
                    ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
                    ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
                };
                size_t out_i = 0;
                for (size_t i = 0; i < dl2; i += 4) {
                    int v[4];
                    int pad = 0;
                    for (int j = 0; j < 4; j++) {
                        if (parsed_digest[i + j] == '=') {
                            v[j] = 0;
                            pad++;
                        }
                        else {
                            v[j] = b64_dec[(unsigned char)parsed_digest[i + j]];
                            if (v[j] == 0 && parsed_digest[i + j] != 'A') {
                                out_i = 0;
                                break;
                            }
                        }
                    }
                    if (out_i == 0 && i > 0) break;
                    if (out_i + 3 - pad > sizeof(expected)) break;
                    expected[out_i++] = (uint8_t)((v[0] << 2) | (v[1] >> 4));
                    if (pad < 2) expected[out_i++] = (uint8_t)(((v[1] & 0xF) << 4) | (v[2] >> 2));
                    if (pad < 1) expected[out_i++] = (uint8_t)(((v[2] & 0x3) << 6) | v[3]);
                }
                expected_len = out_i;
            }

            (void)is_base64;

            any_parsed = true;

            /* Compute actual digest */
            uint8_t actual[CKSUM_MAX_DIGEST];
            size_t actual_len = 0;
            int rc = _cksum_compute_digest(parsed_file, check_alg, opts->length,
                                           actual, &actual_len);
            if (rc != 0) {
                if (!opts->ignore_missing) {
                    cksum_err_printf("%s: %s: %s: %s\n", cksum_prog_name,
                                     parsed_file, "No such file or directory",
                                     strerror(errno));
                    exit_code = 1;
                }
                continue;
            }

            bool match = (actual_len == expected_len &&
                         memcmp(actual, expected, actual_len) == 0);

            if (!opts->status) {
                if (match) {
                    if (!opts->quiet) {
                        cksum_printf("%s: OK\n", parsed_file);
                    }
                }
                else {
                    cksum_printf("%s: FAILED\n", parsed_file);
                    exit_code = 1;
                }
            }
            else {
                if (!match) {
                    exit_code = 1;
                }
            }
        }
        else {
            /* Untagged format: digest  filename or digest * filename */
            /* Find the first space after the digest */
            char * space = strchr(line, ' ');
            if (!space) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            size_t digest_len = (size_t)(space - line);
            if (digest_len >= sizeof(parsed_digest)) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }
            memcpy(parsed_digest, line, digest_len);
            parsed_digest[digest_len] = '\0';

            /* Skip the binary/text indicator */
            char * file_start = space + 1;
            if (*file_start == ' ' || *file_start == '*') {
                file_start++;
            }

            strncpy(parsed_file, file_start, sizeof(parsed_file) - 1);
            parsed_file[sizeof(parsed_file) - 1] = '\0';

            if (alg->is_legacy) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            uint8_t expected[CKSUM_MAX_DIGEST];
            size_t expected_len = 0;

            if (_hex_to_bytes(parsed_digest, expected, sizeof(expected), &expected_len) != 0) {
                if (opts->warn) {
                    cksum_err_printf("%s: %s: %d: improperly formatted line\n",
                                     cksum_prog_name, filename, line_num);
                }
                if (opts->strict) exit_code = 1;
                continue;
            }

            any_parsed = true;

            uint8_t actual[CKSUM_MAX_DIGEST];
            size_t actual_len = 0;
            int rc = _cksum_compute_digest(parsed_file, alg, opts->length,
                                           actual, &actual_len);
            if (rc != 0) {
                if (!opts->ignore_missing) {
                    cksum_err_printf("%s: %s: %s\n", cksum_prog_name,
                                     parsed_file, "No such file or directory");
                    exit_code = 1;
                }
                continue;
            }

            bool match = (actual_len == expected_len &&
                         memcmp(actual, expected, actual_len) == 0);

            if (!opts->status) {
                if (match) {
                    if (!opts->quiet) {
                        cksum_printf("%s: OK\n", parsed_file);
                    }
                }
                else {
                    cksum_printf("%s: FAILED\n", parsed_file);
                    exit_code = 1;
                }
            }
            else {
                if (!match) {
                    exit_code = 1;
                }
            }
        }
    }

    if (!any_parsed && exit_code == 0) {
        cksum_err_printf("%s: %s: no properly formatted checksum lines found\n",
                         cksum_prog_name, filename);
        exit_code = 1;
    }

    if (fp != stdin) {
        fclose(fp);
    }

    return exit_code;
}
