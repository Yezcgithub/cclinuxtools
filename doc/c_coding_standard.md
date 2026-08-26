# C Language Coding Standard

**Version**: 1.0.0

---

## Table of Contents

1. [File Structure](#1-file-structure)
2. [Naming Conventions](#2-naming-conventions)
3. [Code Formatting](#3-code-formatting)
4. [Comment Standards](#4-comment-standards)
5. [Preprocessor Directives](#5-preprocessor-directives)
6. [Type Definitions](#6-type-definitions)
7. [Function Design](#7-function-design)
8. [Variable Usage](#8-variable-usage)
9. [Error Handling](#9-error-handling)
10. [Memory Management](#10-memory-management)
11. [Performance Optimization](#11-performance-optimization)
12. [Portability](#12-portability)
13. [Build System](#13-build-system)
14. [Example Template](#14-example-template)

---

## 1. File Structure

### 1.1 Header Files (.h)

Every header file must follow this structure:

```c
/**
 * @file filename.h
 * @brief One-line description of the file's purpose
 *
 * Detailed description of responsibilities, use cases, notes, etc.
 *
 */

#ifndef __FILENAME_H_
#define __FILENAME_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************
 *    Includes
 ********************************/

/********************************
 *    Defines
 ********************************/

/********************************
 *    Typedefs
 ********************************/

/********************************
 *    Global Prototypes
 ********************************/

/********************************
 *    Macros
 ********************************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __FILENAME_H_ */
```

**Rules**:
- Must begin with a Doxygen-style `@file` comment block at the top
- Must use `#ifndef/#define/#endif` include guards
- Guard macro naming: `__<MODULE>_<FILE>_H_`, all uppercase, without path
- `extern "C"` wrapping for C++ compatibility around all declarations
- All four section comments must be complete: INCLUDES / DEFINES / TYPEDEFS / GLOBAL PROTOTYPES / MACROS

### 1.2 Source Files (.c)

Every source file must follow this structure:

```c
/**
 * @file filename.c
 * @brief One-line description of the file's purpose
 *
 */

/********************************
 *    Includes
 ********************************/
#include "filename.h"
/* System headers in alphabetical order, project headers in dependency order */

/********************************
 *    Defines
 ********************************/
/* File-internal macro definitions */

/********************************
 *    Typedefs
 ********************************/
/* File-internal type definitions */

/********************************
 *    Static Prototypes
 ********************************/
/* File-internal static function declarations, in alphabetical order */

/********************************
 *    Static Variables
 ********************************/
/* File-internal static variables */

/********************************
 *    Macros
 ********************************/
/* File-internal macros */

/********************************
 *    Global Functions
 ********************************/
/* Public function implementations, in order declared in header */

/********************************
 *    Static Functions
 ********************************/
/* Internal static function implementations, in declaration order */
```

### 1.3 Include Order

Header file includes must follow this order, with blank lines separating groups:

1. System standard library headers
2. Third-party library headers
3. Other project module headers
4. Current module's own header (first include in .c)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party.h"

#include "other_module.h"

#include "module_name.h"

```

---

## 2. Naming Conventions

### 2.1 General Rules

- All names use **snake_case** (lowercase with underscores)
- Use English words; no pinyin
- Names must be descriptive; avoid abbreviations (except widely accepted ones: len, num, cnt, ptr, idx, str, buf, cfg, ctx, etc.)

### 2.2 File Names

- All lowercase, underscore-separated
- Prefixed with module name, e.g.: `ring_buf.c`, `ring_buf.h`

### 2.3 Macros and Constants

- All uppercase, underscore-separated
- Must include module prefix to avoid naming conflicts

```c
/* Good */
#define RING_BUF_SIZE_DEFAULT    256U
#define RING_BUF_FLAG_OVERWRITE  C_STD_BIT(0)

/* Bad */
#define BUF_SIZE    256
#define FLAG1       0x01
```

### 2.4 Type Definitions

- Enums: `<module>_<name>_t`, enum values: `<MODULE>_<NAME>_<VAL>`
- Structs: `<module>_<name>_t`
- Unions: `<module>_<name>_t`
- Function pointers: `<module>_<name>_cb_t`

```c
/* Enum */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,
    RING_BUF_MODE_OVERWRITE,
    RING_BUF_MODE_DROP,
} ring_buf_mode_t;

/* Struct */
typedef struct {
    uint8_t * buf;
    size_t capacity;
    size_t head;
    size_t tail;
    ring_buf_mode_t mode;
} ring_buf_t;

/* Function pointer */
typedef void (*ring_buf_overflow_cb_t)(ring_buf_t * rb, void * ctx);
```

### 2.5 Function Naming

- Public functions: `<module>_<action>[_<detail>]`
- Internal static functions: `_<module>_<action>[_<detail>]` (double underscore prefix)

```c
/* Public functions */
ring_buf_t * ring_buf_create(size_t capacity);
void         ring_buf_destroy(ring_buf_t * rb);
size_t       ring_buf_write(ring_buf_t * rb, const void * data, size_t len);
size_t       ring_buf_read(ring_buf_t * rb, void * data, size_t len);

/* Internal static functions */
static size_t _ring_buf_advance_ptr(size_t ptr, size_t len, size_t cap);
static bool   _ring_buf_is_full(const ring_buf_t * rb);
```

### 2.6 Variable Naming

- Local variables, function parameters: `snake_case`
- Global variables: `<sg|g|s>_<module>_<name>` (sg_ static file scope, g_ global, s_ static function scope)
- Pointer variables: p_ prefix (optional but recommended)

```c
static ring_buf_t * sg_rb_default = NULL;
int g_ring_buf_status = 0;

ring_buf_t * ring_buf_create(size_t capacity)
{
    ring_buf_t * p_rb = NULL;
    size_t total_size = 0;
    static int s_status = 0;

    p_rb = (ring_buf_t *)malloc(sizeof(ring_buf_t));
    if (p_rb == NULL) {
        return NULL;
    }
    /* ... */
    return p_rb;
}
```

---

## 3. Code Formatting

### 3.1 Indentation

- Use **4 spaces**; tabs are forbidden
- Each indent level adds +4 spaces

### 3.2 Brace Style (Allman)

**Function definitions**: Opening brace on its own line, aligned with function name

```c
static bool ring_buf__is_empty(const ring_buf_t * rb)
{
    return (rb != NULL) && (rb->head == rb->tail);
}
```

**Control structures**: Opening brace on same line as keyword

```c
if (condition) {
    /* ... */
}
else if (other_condition) {
    /* ... */
}
else {
    /* ... */
}

for (size_t i = 0; i < count; i++) {
    /* ... */
}

while (condition) {
    /* ... */
}

do {
    /* ... */
} while (condition);

switch (value) {
    case CASE_A:
        /* ... */
        break;

    case CASE_B:
        /* ... */
        break;

    default:
        /* ... */
        break;
}
```

**Mandatory rule**: Even single statements must use braces

```c
/* Good */
if (ptr == NULL) {
    return NULL;
}

/* Bad */
if (ptr == NULL)
    return NULL;
```

### 3.3 Spacing Rules

```c
/* Spaces around operators */
a = b + c;
if (a > b)
while (i < len)

/* No space between function name and opening parenthesis */
memcpy(dst, src, len);
int32_t result = calculate(a, b, c);

/* Space after comma */
func(a, b, c);
int32_t array[4] = {1, 2, 3, 4};

/* No space inside parentheses */
if (a == b)    /* Good */
if ( a == b )  /* Bad */
```

### 3.4 Line Breaks

- Maximum **120 columns** per line
- When exceeded, break and align parameters to the opening parenthesis

```c
c_std_res_t c_std_process_data(const void * data,
                               size_t len,
                               c_std_cb_t cb,
                               void * user_data)
{
    /* ... */
}

/* Function call line break */
int32_t ret = some_long_function_name(param1,
                                      param2,
                                      param3,
                                      param4);
```

### 3.5 Blank Lines

- **2 blank lines** between functions
- 1 blank line between logical groupings of struct/enum fields
- 1 blank line between logical blocks within a function

```c
void func_a(void)
{
    /* ... */
}


void func_b(void)
{
    int32_t ret = 0;

    /* Initialization phase */
    ret = init_subsystem();
    if (ret != 0) {
        return;
    }

    /* Processing phase */
    process_data();

    /* Cleanup phase */
    cleanup_resources();
}
```

### 3.6 Pointer Declarations

`*` binds to the variable name, not the type

```c
uint8_t * buf;        /* Good */
uint8_t* buf;         /* Bad */
const uint8_t * p;    /* Good */
uint8_t * const p;    /* Pointer constant */
const uint8_t * const p; /* Double const */
```

---

## 4. Comment Standards

### 4.1 Doxygen Comments

**File comment** (top of file):

```c
/**
 * @file ring_buf.h
 * @brief Ring buffer implementation
 *
 * Provides FIFO read/write for data of arbitrary size.
 * Thread safety must be ensured by the caller.
 *
 */
```

**Function comment**:
```c
/**
 * @brief Create a ring buffer
 * @param capacity  Buffer capacity (bytes)
 * @param mode      Behavior when full
 * @return Buffer pointer on success, NULL on failure
 *
 * @note Call ring_buf_destroy() to release
 * @note capacity is internally rounded up to 4-byte boundary
 *
 * Example:
 * @code
 * ring_buf_t * rb = ring_buf_create(1024, RING_BUF_MODE_OVERWRITE);
 * if (rb) {
 *     ring_buf_write(rb, data, 100);
 *     ring_buf_destroy(rb);
 * }
 * @endcode
 */
ring_buf_t * ring_buf_create(size_t capacity, ring_buf_mode_t mode);
```

**Struct comment**:
```c
/**
 * @brief Ring buffer control structure
 */
typedef struct {
    uint8_t * buf;              /* <- Pointer to data storage */
    size_t capacity;            /* <- Total capacity (bytes) */
    size_t head;                /* <- Write position index */
    size_t tail;                /* <- Read position index */
    ring_buf_mode_t mode;       /* <- Behavior when full */
    ring_buf_overflow_cb_t cb;  /* <- Overflow callback (may be NULL) */
    void * cb_ctx;              /* <- Callback context */
} ring_buf_t;
```

**Enum comment**:
```c
/**
 * @brief Buffer-full handling modes
 */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,    /* <- Block: reject writes */
    RING_BUF_MODE_OVERWRITE,    /* <- Overwrite: overwrite oldest data */
    RING_BUF_MODE_DROP,         /* <- Drop: discard new data */
} ring_buf_mode_t;
```

**Macro comment**:
```c
/**
 * @brief Count the number of elements in an array
 * @param arr  Static array name (must not be a pointer)
 */
#define C_STD_ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
```

### 4.2 In-Code Comments

- Use `//` single-line or `/* ... */` multi-line comments
- Place comments above the code or at line end; never below the code
- Comments should explain **why**, not **what**

```c
/* Good: explains reasoning */
/* Use tmp to avoid endianness issues; assemble directly by byte */
val = ((uint32_t)buf[0] << 24) |
      ((uint32_t)buf[1] << 16) |
      ((uint32_t)buf[2] <<  8) |
      ((uint32_t)buf[3]);

/* Bad: describes the obvious */
/* shift buf[0] by 24, buf[1] by 16... */
```

### 4.3 Prohibitions

- No commented-out code (delete it; use version control to look back)
- No `// TODO: xxx` without specifying owner and date
- Correct format: `// TODO(zhang.san 2026-08-12): Handle boundary overflow case`

---

## 5. Preprocessor Directives

### 5.1 Include Guards

```c
/* Good: consistent and explicit */
#ifndef __RING_BUF_H_
#define __RING_BUF_H_
/* ... */
#endif /* __RING_BUF_H_ */
```

### 5.2 Conditional Compilation

- `#if`, `#ifdef`, `#ifndef` must have a comment explaining the condition
- When nested, indent consistently; `#endif` must have a comment marking what it matches

```c
#define CONFIG_RING_BUF_DEBUG    1

#if CONFIG_RING_BUF_DEBUG
/* Enable statistics in debug mode */
typedef struct {
    uint32_t write_cnt;
    uint32_t read_cnt;
    uint32_t overflow_cnt;
} ring_buf_stats_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* ... */
#ifdef __cplusplus
} /* extern "C" */
#endif
```

### 5.3 Macro Functions

- All parameters must be parenthesized
- The entire expression must be parenthesized
- Multi-line macros must be wrapped in `do { ... } while(0)`

```c
/* Good */
#define C_STD_ABS(x)                 ((x) < 0 ? -(x) : (x))

#define C_STD_SAFE_FREE(p)           \
    do {                             \
        if ((p) != NULL) {           \
            free(p);                 \
            (p) = NULL;              \
        }                            \
    } while(0)

/* Bad */
#define BAD_ABS(x)     x < 0 ? -x : x     /* Operator precedence bug */
#define BAD_SQR(x)     x * x              /* Expands incorrectly */
```

### 5.4 Configurable Values as Macros

All configurable numbers, strings, and limits must be defined as macros. Magic numbers are forbidden in code.

```c
/* Good */
#define UART_RX_BUF_SIZE        2048U
#define UART_DEFAULT_BAUDRATE   115200UL
#define UART_TIMEOUT_MS         1000U

if (elapsed >= UART_TIMEOUT_MS) {
    /* ... */
}

/* Bad */
if (elapsed >= 1000) {
    /* What is 1000? Seconds or milliseconds? */
}
```

---

## 6. Type Definitions

### 6.1 Use C99 Standard Integer Types

```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

int8_t, int16_t, int32_t, int64_t      /* Signed */
uint8_t, uint16_t, uint32_t, uint64_t  /* Unsigned */
size_t, ssize_t, ptrdiff_t             /* Size / pointer difference */
bool, true, false                      /* Boolean */
```

**Forbidden**: `unsigned int`, `short`, `long`, and other ambiguous-width types.

### 6.2 Struct Design

- Order fields from largest to smallest to minimize padding
- Group fields of the same type together
- Use explicit padding to ensure alignment (if necessary)

```c
/* Good: reasonable field ordering */
typedef struct {
    uint8_t * buf;          /* 8 bytes (64-bit) */
    size_t capacity;        /* 8 bytes */
    uint64_t timestamp;     /* 8 bytes */
    uint32_t flags;         /* 4 bytes */
    uint32_t write_cnt;     /* 4 bytes */
    uint16_t crc;           /* 2 bytes */
    ring_buf_mode_t mode;   /* 4 bytes (enum is typically int) */
    uint8_t  ref_cnt;       /* 1 byte  */
    uint8_t  reserved[3];   /* Padding to 8-byte boundary */
} ring_buf_t;
```

### 6.3 Opaque Types

Hide internal structure from external users via forward declarations:

```c
/* ---- module.h ---- */
typedef struct module_ctx module_ctx_t;    /* Forward declaration */

module_ctx_t * module_create(const module_cfg_t * cfg);
void           module_destroy(module_ctx_t * ctx);
c_std_res_t    module_start(module_ctx_t * ctx);

/* ---- module.c ---- */
struct module_ctx {
    /* Internal fields not exposed */
    uint32_t state;
    int32_t  fd;
    /* ... */
};
```

---

## 7. Function Design

### 7.1 Function Length

- Generally no more than **50 lines** (excluding blank lines and comments)
- If exceeded, split internal logic into separate static functions

### 7.2 Parameter Design

- No more than **6 parameters**
- If exceeded, pack parameters into a struct
- Input parameters first, output parameters last
- Pointer input parameters should have `const`

```c
/* Good */
typedef struct {
    uint32_t baudrate;
    uint8_t  data_bits;
    uint8_t  stop_bits;
    uint8_t  parity;
    bool     flow_ctrl;
    bool     rx_enable;
    bool     tx_enable;
    uint32_t timeout_ms;
} uart_cfg_t;

c_std_res_t uart_init(uart_port_t port, const uart_cfg_t * cfg);

/* Bad: too many parameters */
c_std_res_t uart_init_bad(uart_port_t port,
                          uint32_t baudrate,
                          uint8_t  data_bits,
                          uint8_t  stop_bits,
                          uint8_t  parity,
                          bool     flow_ctrl,
                          bool     rx_enable,
                          bool     tx_enable,
                          uint32_t timeout_ms);
```

### 7.3 Return Value Design

- Default to a unified `<module>_res_t` or `c_std_res_t` return type
- Creation functions return NULL on failure
- Output is returned through pointer parameters

```c
/* Unified result code */
typedef enum {
    C_STD_RES_OK = 0,      /**< Success */
    C_STD_RES_ERR,         /**< General error */
    C_STD_RES_INV_PARAM,   /**< Invalid parameter */
    C_STD_RES_NO_MEM,      /**< Out of memory */
    C_STD_RES_NOT_READY,   /**< Not ready */
    C_STD_RES_TIMEOUT,     /**< Timeout */
} c_std_res_t;

/* Usage */
c_std_res_t data_parser_process(const uint8_t * in, size_t in_len,
                                uint8_t * out, size_t * out_len);
```

### 7.4 Parameter Validation

- All public functions must validate input parameters
- Check pointers for NULL; check integers for valid range

```c
c_std_res_t ring_buf_write(ring_buf_t * rb, const void * data, size_t len)
{
    if (!rb || !data) {
        return C_STD_RES_INV_PARAM;
    }

    if (len == 0) {
        return C_STD_RES_OK;    /* Zero length is success, no operation needed */
    }

    if (len > rb->capacity) {
        return C_STD_RES_INV_PARAM;
    }

    /* Normal logic */
    return C_STD_RES_OK;
}
```

### 7.5 Callbacks and Context

Callbacks must support a user_data pointer:

```c
typedef void (*async_op_cb_t)(c_std_res_t result, void * user_data);

c_std_res_t async_read(const char * path,
                       uint8_t * buf,
                       size_t len,
                       async_op_cb_t cb,
                       void * user_data);
```

---

## 8. Variable Usage

### 8.1 Declaration Position

- C99 allows declaration anywhere within a block
- **Recommended**: declare close to first use, immediately before first use

```c
c_std_res_t process_packet(const uint8_t * data, size_t len)
{
    if (len < PACKET_HDR_SIZE) {
        return C_STD_RES_INV_PARAM;
    }

    /* Declare after header validation passes */
    uint16_t payload_len = (uint16_t)(data[2] << 8) | data[3];
    if (payload_len + PACKET_HDR_SIZE > len) {
        return C_STD_RES_INV_PARAM;
    }

    const uint8_t * payload = data + PACKET_HDR_SIZE;
    uint16_t checksum = calc_checksum(data, len - 2);
    /* ... */
    return C_STD_RES_OK;
}
```

### 8.2 Initialization

- Always initialize at declaration
- Initialize pointers to NULL
- Initialize numeric values to 0 or a valid value

```c
/* Good */
uint32_t flags = 0U;
int32_t ret = 0;
uint8_t * buf = NULL;
ring_buf_t rb = {0};    /* Zero-initialize struct */

/* Bad */
uint32_t flags;
int32_t ret;
uint8_t * buf;
```

### 8.3 Global Variables

- **Forbidden**: non-static globals (use getter/setter for cross-file sharing)
- File-scope variables must have `static`

```c
/* Good */
static ring_buf_t * sg_uart_rx_buf = NULL;
static uint32_t sg_init_done = 0U;

ring_buf_t * uart_get_rx_buf(void)
{
    return sg_uart_rx_buf;
}
```

---

## 9. Error Handling

### 9.1 Unified Error Return Path

Use `goto` to jump to a unified cleanup exit (the only scenario where `goto` is allowed):

```c
c_std_res_t do_something(const char * name)
{
    c_std_res_t res = C_STD_RES_ERR;
    void * ctx = NULL;
    void * buf = NULL;
    int32_t fd = -1;

    if (name == NULL) {
        res = C_STD_RES_INV_PARAM;
        goto out;
    }

    ctx = alloc_context();
    if (ctx == NULL) {
        res = C_STD_RES_NO_MEM;
        goto out;
    }

    buf = malloc(1024);
    if (buf == NULL) {
        res = C_STD_RES_NO_MEM;
        goto out;
    }

    fd = open_file(name);
    if (fd < 0) {
        res = C_STD_RES_ERR;
        goto out;
    }

    /* Normal business logic ... */
    res = C_STD_RES_OK;

out:
    if (fd >= 0)          close_file(fd);
    if (buf != NULL)      free(buf);
    if (ctx != NULL)      free_context(ctx);
    return res;
}
```

### 9.2 No Silent Failures

- Errors must be handled or propagated upward
- Return values must not be ignored (unless documentation explicitly says they can be)

```c
/* Good */
c_std_res_t res = send_packet(pkt);
if (res != C_STD_RES_OK) {
    LOG_E("send_packet failed: %d", res);
    return res;
}

/* Explicitly ignoring is allowed (with explanation) */
(void)send_heartbeat();    /* Heartbeat failure doesn't affect main flow, safe to ignore */

/* Bad */
send_packet(pkt);          /* Silently ignored; issues will be hard to trace */
```

### 9.3 Assertions

- Use assertions to catch logic errors during development
- Runtime checks must still be preserved for release builds

```c
/* Good: assertions + runtime checks coexist */
c_std_res_t module_send_msg(module_ctx_t * ctx, msg_t * msg)
{
    /* Assertion: caller should guarantee ctx is non-NULL */
    C_STD_ASSERT(ctx != NULL);
    C_STD_ASSERT(msg != NULL);

    /* Runtime check: still needed in release builds */
    if (ctx == NULL || msg == NULL) {
        return C_STD_RES_INV_PARAM;
    }

    /* Logic assertion: internal state must be consistent */
    C_STD_ASSERT(ctx->state == STATE_READY);
    /* ... */
    return C_STD_RES_OK;
}
```

---

## 10. Memory Management

### 10.1 Allocation/Deallocation Pairing

- Whoever allocates must free (at the same module level)
- Provide create/destroy paired APIs
- Set pointer to NULL in destroy function (prevent double-free)

```c
ring_buf_t * ring_buf_create(size_t capacity)
{
    ring_buf_t * rb = (ring_buf_t *)calloc(1, sizeof(ring_buf_t));
    if (rb == NULL) {
        return NULL;
    }

    rb->buf = (uint8_t *)malloc(capacity);
    if (rb->buf == NULL) {
        free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    return rb;
}

void ring_buf_destroy(ring_buf_t * rb)
{
    if (rb == NULL) {
        return;
    }

    free(rb->buf);
    rb->buf = NULL;

    free(rb);
}
```

### 10.2 Buffer Operations

- All write operations must pass the buffer size
- Use `snprintf` instead of `sprintf`
- Use `memcpy_s`/`strncpy` and other safe versions (if available)

```c
/* Good */
char buf[64];
int32_t n = snprintf(buf, sizeof(buf), "%s-%u", name, index);
if (n < 0 || (size_t)n >= sizeof(buf)) {
    /* Truncated, handle error */
}

memcpy(dst, src, C_STD_MIN(dst_len, src_len));

/* Bad */
sprintf(buf, "%s-%u", name, index);
memcpy(dst, src, src_len);    /* dst may not be large enough */
```

### 10.3 Avoiding Stack Overflow

- Large arrays (> 256 bytes) should be dynamically allocated or static
- Recursion depth must be controlled

```c
/* Good */
static uint8_t sg_parse_buf[4096];   /* In BSS segment, no stack usage */

void parser_init(void)
{
    uint8_t * tmp = (uint8_t *)malloc(8192);
    /* ... */
}

/* Bad */
void parser_bad(void)
{
    uint8_t parse_buf[8192];       /* 8KB on stack, high risk */
}
```

---

## 11. Performance Optimization

### 11.1 Use of const

Add `const` to all read-only pointers and variables:

```c
uint32_t calc_crc(const uint8_t * data, size_t len);
int32_t find_index(const char * const * str_table, uint32_t count, const char * key);
```

### 11.2 Inline Small Functions

```c
static inline size_t ring_buf_used(const ring_buf_t * rb)
{
    C_STD_ASSERT(rb != NULL);
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    }
    return rb->capacity - (rb->tail - rb->head);
}
```

### 11.3 Hot Loop Optimization

- Hoist loop-invariant computations outside the loop
- Avoid calling virtual functions/callbacks inside loops
- Access arrays in row-major order (C is row-major)

```c
/* Good: hoist invariants outside the loop */
size_t stride = get_stride();
uint32_t width = image->w;
uint8_t * row_ptr = image->data;

for (uint32_t y = 0; y < image->h; y++) {
    uint8_t * pixel = row_ptr;
    for (uint32_t x = 0; x < width; x++) {
        process_pixel(pixel);
        pixel += 3;
    }
    row_ptr += stride;
}
```

### 11.4 Bit Operations

```c
/* Macros to avoid magic numbers */
#define CMD_FLAG_RTS     C_STD_BIT(0)
#define CMD_FLAG_CTS     C_STD_BIT(1)
#define CMD_FLAG_NACK    C_STD_BIT(7)

uint32_t flags = 0U;

C_STD_BIT_SET(flags, CMD_FLAG_RTS);
if (C_STD_BIT_ISSET(flags, CMD_FLAG_CTS)) {
    /* ... */
}
C_STD_BIT_CLR(flags, CMD_FLAG_RTS);
```

---

## 12. Portability

### 12.1 Endianness

```c
/* Use byte-order conversion functions; do not rely on platform */
static inline uint16_t be16_to_cpu(const uint8_t * p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t le32_to_cpu(const uint8_t * p)
{
    return ((uint32_t)p[0])       |
           ((uint32_t)p[1] <<  8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
```

### 12.2 Struct Serialization

Do not directly memcpy structs to network/file; always serialize field by field manually:

```c
/* Good: explicit serialization */
size_t packet_serialize(const packet_t * pkt, uint8_t * out, size_t out_len)
{
    if (out_len < 8) return 0;

    out[0] = (uint8_t)(pkt->type);
    out[1] = (uint8_t)(pkt->ver);
    out[2] = (uint8_t)(pkt->len >> 8);
    out[3] = (uint8_t)(pkt->len);
    out[4] = (uint8_t)(pkt->seq >> 24);
    out[5] = (uint8_t)(pkt->seq >> 16);
    out[6] = (uint8_t)(pkt->seq >> 8);
    out[7] = (uint8_t)(pkt->seq);
    return 8;
}

/* Bad: depends on platform alignment and endianness */
memcpy(out, pkt, sizeof(packet_t));
```

### 12.3 Platform Abstraction

```c
/* platform_port.h */
typedef int32_t platform_fd_t;
#define PLATFORM_FD_INVALID    (-1)

platform_fd_t platform_open(const char * path, uint32_t flags);
c_std_res_t   platform_close(platform_fd_t fd);
int32_t       platform_read(platform_fd_t fd, void * buf, size_t len);
c_std_res_t   platform_sleep_ms(uint32_t ms);
uint64_t      platform_get_tick_ms(void);
```

---

## 13. Build System

### 13.1 Warning Options

```makefile
# Recommended GCC / Clang compilation flags
CFLAGS += -std=c99 -O2 -g
CFLAGS += -Wall -Wextra -Wpedantic
CFLAGS += -Wshadow -Wconversion -Wsign-conversion
CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wimplicit-function-declaration
CFLAGS += -Wundef -Wunreachable-code
CFLAGS += -Wformat=2
CFLAGS += -Werror    /* Treat warnings as errors; must be enabled before commit */
```

### 13.2 Module Organization

```
# Depends on the specific project
```

---

## 14. Example Template

### 14.1 ring_buf.h

```c
/**
 * @file ring_buf.h
 * @brief Ring buffer implementation
 *
 * Provides FIFO read/write for data of arbitrary size.
 * Not thread-safe; external locking required in multi-threaded environments.
 *
 */

#ifndef __RING_BUF_H_
#define __RING_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************
 *    Includes
 ********************************/
#include "c_coding_standard.h"

/********************************
 *    Defines
 ********************************/
#define RING_BUF_SIZE_MIN    4U

/********************************
 *    Typedefs
 ********************************/

/**
 * @brief Buffer-full handling modes
 */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,    /* <- Block: reject writes */
    RING_BUF_MODE_OVERWRITE,    /* <- Overwrite: overwrite oldest data */
    RING_BUF_MODE_DROP,         /* <- Drop: discard new data */
} ring_buf_mode_t;

/** Forward declaration */
typedef struct ring_buf ring_buf_t;

/**
 * @brief Overflow callback
 * @param rb         Buffer instance
 * @param user_data  User data
 */
typedef void (*ring_buf_overflow_cb_t)(ring_buf_t * rb, void * user_data);

/********************************
 *    Global Prototypes
 ********************************/

/**
 * @brief Create a ring buffer
 * @param capacity  Capacity (bytes, >= 4)
 * @param mode      Behavior when full
 * @return Buffer pointer on success, NULL on failure
 */
ring_buf_t * ring_buf_create(size_t capacity, ring_buf_mode_t mode);

/**
 * @brief Destroy a buffer
 * @param rb  Buffer instance (NULL is allowed)
 */
void ring_buf_destroy(ring_buf_t * rb);

/**
 * @brief Write data
 * @param rb    Buffer instance
 * @param data  Data source
 * @param len   Number of bytes to write
 * @return Number of bytes actually written
 */
size_t ring_buf_write(ring_buf_t * rb, const void * data, size_t len);

/**
 * @brief Read data
 * @param rb    Buffer instance
 * @param out   Output buffer
 * @param len   Number of bytes to read
 * @return Number of bytes actually read
 */
size_t ring_buf_read(ring_buf_t * rb, void * out, size_t len);

/**
 * @brief Get number of bytes in use
 * @param rb  Buffer instance
 * @return Number of bytes in use
 */
size_t ring_buf_used(const ring_buf_t * rb);

/********************************
 *    Macros
 ********************************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RING_BUF_H_ */
```

### 14.2 ring_buf.c

```c
/**
 * @file ring_buf.c
 * @brief Ring buffer implementation
 *
 */

/********************************
 *    Includes
 ********************************/
#include <string.h>
#include <stdlib.h>

#include "ring_buf.h"
/********************************
 *    Defines
 ********************************/

/********************************
 *    Typedefs
 ********************************/

/**
 * @brief Ring buffer structure
 */
struct ring_buf {
    uint8_t * buf;              /* <- Storage */
    size_t capacity;            /* <- Total capacity */
    size_t head;                /* <- Write position */
    size_t tail;                /* <- Read position */
    ring_buf_mode_t mode;       /* <- Behavior when full */
    ring_buf_overflow_cb_t cb;  /* <- Overflow callback */
    void * cb_data;             /* <- Callback user data */
};

/********************************
 *    Static Prototypes
 ********************************/
static size_t _ring_buf_advance(size_t ptr, size_t n, size_t cap);

/********************************
 *    Static Variables
 ********************************/

/********************************
 *    Macros
 ********************************/

/********************************
 *    Global Functions
 ********************************/

ring_buf_t * ring_buf_create(size_t capacity, ring_buf_mode_t mode)
{
    if (capacity < RING_BUF_SIZE_MIN) {
        return NULL;
    }

    ring_buf_t * rb = (ring_buf_t *)calloc(1, sizeof(ring_buf_t));
    if (!rb) {
        return NULL;
    }

    rb->buf = (uint8_t *)malloc(capacity);
    if (!rb->buf) {
        free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->mode = mode;
    rb->head = 0;
    rb->tail = 0;
    return rb;
}

void ring_buf_destroy(ring_buf_t * rb)
{
    if (!rb) {
        return;
    }

    free(rb->buf);
    rb->buf = NULL;
    free(rb);
}

size_t ring_buf_write(ring_buf_t * rb, const void * data, size_t len)
{
    if (!rb || !data || len <= 0) {
        return 0;
    }

    const uint8_t * src = (const uint8_t *)data;
    size_t free_bytes = rb->capacity - ring_buf_used(rb);

    /* Handle full-buffer policy */
    if (len > free_bytes) {
        if (rb->cb) {
            rb->cb(rb, rb->cb_data);
        }

        switch (rb->mode) {
            case RING_BUF_MODE_BLOCK:
                return 0;

            case RING_BUF_MODE_DROP:
                return 0;

            case RING_BUF_MODE_OVERWRITE:
                /* Discard enough old data */
                while (len > (rb->capacity - ring_buf_used(rb))) {
                    size_t drop_bytes = C_STD_MIN(rb->capacity,
                                                  len - (rb->capacity - ring_buf_used(rb)) + 1);
                    rb->tail = _ring_buf_advance(rb->tail, drop_bytes, rb->capacity);
                }
                break;

            default:
                return 0;
        }
    }

    size_t written = 0;
    while (written < len) {
        size_t to_end = rb->capacity - rb->head;
        size_t now = C_STD_MIN(to_end, len - written);

        memcpy(rb->buf + rb->head, src + written, now);
        rb->head = _ring_buf_advance(rb->head, now, rb->capacity);
        written += now;
    }

    return written;
}


size_t ring_buf_read(ring_buf_t * rb, void * out, size_t len)
{
    if (!rb || !out || len <= 0) {
        return 0;
    }

    uint8_t * dst = (uint8_t *)out;
    size_t used = ring_buf_used(rb);
    size_t to_read = C_STD_MIN(used, len);
    size_t read_cnt = 0;

    while (read_cnt < to_read) {
        size_t to_end = rb->capacity - rb->tail;
        size_t now = C_STD_MIN(to_end, to_read - read_cnt);

        memcpy(dst + read_cnt, rb->buf + rb->tail, now);
        rb->tail = _ring_buf_advance(rb->tail, now, rb->capacity);
        read_cnt += now;
    }

    return read_cnt;
}


size_t ring_buf_used(const ring_buf_t * rb)
{
    if (!rb) {
        return 0;
    }

    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    }
    return rb->capacity - (rb->tail - rb->head);
}

/********************************
 *    Static Functions
 ********************************/

/**
 * @brief Advance index circularly
 * @param ptr  Current index
 * @param n    Bytes to advance
 * @param cap  Capacity
 * @return New index
 */
static size_t _ring_buf_advance(size_t ptr, size_t n, size_t cap)
{
    ptr += n;
    if (ptr >= cap) {
        ptr -= cap;
    }
    return ptr;
}
```

---

**End of Document**
