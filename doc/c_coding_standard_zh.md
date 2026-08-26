# C 语言编程规范

**版本**: 1.0.0

---

## 目录

1. [文件结构](#1-文件结构)
2. [命名规范](#2-命名规范)
3. [代码格式](#3-代码格式)
4. [注释规范](#4-注释规范)
5. [预处理指令](#5-预处理指令)
6. [类型定义](#6-类型定义)
7. [函数设计](#7-函数设计)
8. [变量使用](#8-变量使用)
9. [错误处理](#9-错误处理)
10. [内存管理](#10-内存管理)
11. [性能优化](#11-性能优化)
12. [可移植性](#12-可移植性)
13. [构建系统](#13-构建系统)
14. [示例模板](#14-示例模板)

---

## 1. 文件结构

### 1.1 头文件 (.h)

每个头文件必须遵循以下结构：

```c
/**
 * @file filename.h
 * @brief 一句话描述文件功能
 *
 * 详细说明文件职责、使用场景、注意事项等
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

**规则**:
- 顶部必须有 Doxygen 风格的 `@file` 注释块
- 必须使用 `#ifndef/#define/#endif` 防止多重包含
- 守卫宏命名：`__<MODULE>_<FILE>_H_`，全大写，不含路径
- C++ 兼容的 `extern "C"` 包裹所有声明
- 四个分区注释必须完整：INCLUDES / DEFINES / TYPEDEFS / GLOBAL PROTOTYPES / MACROS

### 1.2 源文件 (.c)

每个源文件必须遵循以下结构：

```c
/**
 * @file filename.c
 * @brief 一句话描述文件功能
 *
 */

/********************************
 *    Includes
 ********************************/
#include "filename.h"
/* 系统头文件按字母顺序，项目头文件按依赖顺序 */

/********************************
 *    Defines
 ********************************/
/* 文件内部使用的宏定义 */

/********************************
 *    Typedefs
 ********************************/
/* 文件内部使用的类型定义 */

/********************************
 *    Static Prototypes
 ********************************/
/* 文件内部静态函数声明，按字母顺序 */

/********************************
 *    Static Variables
 ********************************/
/* 文件内部静态变量 */

/********************************
 *    Macros
 ********************************/
/* 文件内部使用的宏 */

/********************************
 *    Global Functions
 ********************************/
/* 对外函数实现，按头文件中声明顺序 */

/********************************
 *    Static Functions
 ********************************/
/* 内部静态函数实现，按声明顺序 */
```

### 1.3 包含顺序

头文件包含必须遵循以下顺序，各组之间用空行分隔：

1. 系统标准库头文件
2. 第三方库头文件
3. 项目内其他模块头文件
4. 当前模块对应的头文件（.c 中第一个 include）

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party.h"

#include "other_module.h"

#include "module_name.h"

```

---

## 2. 命名规范

### 2.1 通用规则

- 全部使用 **snake_case**（小写下划线）风格
- 使用英文单词，避免拼音
- 名称需具有描述性，避免缩写（除广为接受的缩写：len, num, cnt, ptr, idx, str, buf, cfg, ctx 等）

### 2.2 文件命名

- 全部小写，用下划线分隔
- 前缀为模块名，如：`ring_buf.c`, `ring_buf.h`

### 2.3 宏与常量

- 全大写，用下划线分隔
- 必须带模块前缀，避免命名冲突

```c
/* 好 */
#define RING_BUF_SIZE_DEFAULT    256U
#define RING_BUF_FLAG_OVERWRITE  C_STD_BIT(0)

/* 坏 */
#define BUF_SIZE    256
#define FLAG1       0x01
```

### 2.4 类型定义

- 枚举：`<module>_<name>_t`，枚举值：`<MODULE>_<NAME>_<VAL>`
- 结构体：`<module>_<name>_t`
- 联合体：`<module>_<name>_t`
- 函数指针：`<module>_<name>_cb_t`

```c
/* 枚举 */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,
    RING_BUF_MODE_OVERWRITE,
    RING_BUF_MODE_DROP,
} ring_buf_mode_t;

/* 结构体 */
typedef struct {
    uint8_t * buf;
    size_t capacity;
    size_t head;
    size_t tail;
    ring_buf_mode_t mode;
} ring_buf_t;

/* 函数指针 */
typedef void (*ring_buf_overflow_cb_t)(ring_buf_t * rb, void * ctx);
```

### 2.5 函数命名

- 对外函数：`<module>_<action>[_<detail>]`
- 内部静态函数：`_<module>_<action>[_<detail>]`（双下划线前缀）

```c
/* 对外函数 */
ring_buf_t * ring_buf_create(size_t capacity);
void         ring_buf_destroy(ring_buf_t * rb);
size_t       ring_buf_write(ring_buf_t * rb, const void * data, size_t len);
size_t       ring_buf_read(ring_buf_t * rb, void * data, size_t len);

/* 内部静态函数 */
static size_t _ring_buf_advance_ptr(size_t ptr, size_t len, size_t cap);
static bool   _ring_buf_is_full(const ring_buf_t * rb);
```

### 2.6 变量命名

- 局部变量、函数参数：`snake_case`
- 全局变量：`<sg|g|s>_<module>_<name>`（sg_ 静态文件域，g_ 全局，s_ 静态函数域）
- 指针变量：p_ 前缀（可选，但推荐）

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

## 3. 代码格式

### 3.1 缩进

- 使用 **4 个空格**，禁止使用 Tab
- 每级缩进 +4 空格

### 3.2 大括号 (Allman 风格)

**函数定义**：大括号独占一行，与函数名对齐

```c
static bool ring_buf__is_empty(const ring_buf_t * rb)
{
    return (rb != NULL) && (rb->head == rb->tail);
}
```

**控制结构**：大括号同一行，与关键字对齐

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

**强制规则**：即使单条语句也必须加大括号

```c
/* 好 */
if (ptr == NULL) {
    return NULL;
}

/* 坏 */
if (ptr == NULL)
    return NULL;
```

### 3.3 空格规则

```c
/* 运算符两侧加空格 */
a = b + c;
if (a > b)
while (i < len)

/* 函数名与左括号之间无空格 */
memcpy(dst, src, len);
int32_t result = calculate(a, b, c);

/* 逗号后面加空格 */
func(a, b, c);
int32_t array[4] = {1, 2, 3, 4};

/* 括号内侧无空格 */
if (a == b)    /* 好 */
if ( a == b )  /* 坏 */
```

### 3.4 换行

- 每行不超过 **120 列**
- 超过时换行，参数换行后缩进对齐到左括号后

```c
c_std_res_t c_std_process_data(const void * data,
                               size_t len,
                               c_std_cb_t cb,
                               void * user_data)
{
    /* ... */
}

/* 函数调用换行 */
int32_t ret = some_long_function_name(param1,
                                      param2,
                                      param3,
                                      param4);
```

### 3.5 空行

- 函数之间 **2 个空行**
- 结构体/枚举字段间逻辑分组可加 1 空行
- 函数内部逻辑块之间加 1 空行

```c
void func_a(void)
{
    /* ... */
}


void func_b(void)
{
    int32_t ret = 0;

    /* 初始化阶段 */
    ret = init_subsystem();
    if (ret != 0) {
        return;
    }

    /* 处理阶段 */
    process_data();

    /* 清理阶段 */
    cleanup_resources();
}
```

### 3.6 指针声明

`*` 贴近变量名，而不是类型

```c
uint8_t * buf;        /* 好 */
uint8_t* buf;         /* 坏 */
const uint8_t * p;    /* 好 */
uint8_t * const p;    /* 指针常量 */
const uint8_t * const p; /* 双重const */
```

---

## 4. 注释规范

### 4.1 Doxygen 注释

**文件注释**（文件顶部）：

```c
/**
 * @file ring_buf.h
 * @brief 环形缓冲区实现
 *
 * 提供任意大小数据的 FIFO 读写，支持覆盖/丢弃/阻塞三种模式。
 * 线程安全需由外层保证。
 *
 */
```

**函数注释**：
```c
/**
 * @brief 创建环形缓冲区
 * @param capacity  缓冲区容量（字节）
 * @param mode      满时行为模式
 * @return 成功返回缓冲区指针，失败返回NULL
 *
 * @note 调用 ring_buf_destroy() 释放
 * @note capacity 内部会向上对齐到 4 字节边界
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

**结构体注释**：
```c
/**
 * @brief 环形缓冲区控制结构
 */
typedef struct {
    uint8_t * buf;              /* <- 数据存储区指针 */
    size_t capacity;            /* <- 总容量（字节）*/
    size_t head;                /* <- 写入位置索引 */
    size_t tail;                /* <- 读取位置索引 */
    ring_buf_mode_t mode;       /* <- 满时处理模式 */
    ring_buf_overflow_cb_t cb;  /* <- 溢出回调（可NULL）*/
    void * cb_ctx;              /* <- 回调上下文 */
} ring_buf_t;
```

**枚举注释**：
```c
/**
 * @brief 缓冲区满时处理模式
 */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,    /* <- 阻塞：拒绝写入 */
    RING_BUF_MODE_OVERWRITE,    /* <- 覆盖：覆盖最旧数据 */
    RING_BUF_MODE_DROP,         /* <- 丢弃：丢弃新数据 */
} ring_buf_mode_t;
```

**宏注释**：
```c
/**
 * @brief 计算数组元素个数
 * @param arr  静态数组名（不能是指针）
 */
#define C_STD_ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
```

### 4.2 代码内注释

- 使用 `//` 单行注释或 `/* ... */` 多行注释
- 注释放在代码上方或行尾，避免放在代码下方
- 注释解释 **为什么** 而不是 **做什么**

```c
/* 好：说明原因 */
/* 用 tmp 避免大端小端问题，直接按字节拼装 */
val = ((uint32_t)buf[0] << 24) |
      ((uint32_t)buf[1] << 16) |
      ((uint32_t)buf[2] <<  8) |
      ((uint32_t)buf[3]);

/* 坏：描述显而易见的代码 */
/* 把 buf[0] 移 24 位，buf[1] 移 16 位... */
```

### 4.3 禁止项

- 禁止注释掉的代码（应直接删除，用版本控制回溯）
- 禁止 `// TODO: xxx` 不写具体负责人和时间
- 正确写法：`// TODO(zhang.san 2026-08-12): 处理边界溢出情况`

---

## 5. 预处理指令

### 5.1 守卫宏

```c
/* 好：一致且明确 */
#ifndef __RING_BUF_H_
#define __RING_BUF_H_
/* ... */
#endif /* __RING_BUF_H_ */
```

### 5.2 条件编译

- `#if`, `#ifdef`, `#ifndef` 后必须加注释说明条件含义
- 嵌套时缩进对齐，`#endif` 后加注释标出匹配项

```c
#define CONFIG_RING_BUF_DEBUG    1

#if CONFIG_RING_BUF_DEBUG
/* 调试模式下启用统计信息 */
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

### 5.3 宏函数

- 所有参数必须加括号
- 整个表达式必须加括号
- 多行使用 `do { ... } while(0)` 包裹

```c
/* 好 */
#define C_STD_ABS(x)                 ((x) < 0 ? -(x) : (x))

#define C_STD_SAFE_FREE(p)           \
    do {                             \
        if ((p) != NULL) {           \
            free(p);                 \
            (p) = NULL;              \
        }                            \
    } while(0)

/* 坏 */
#define BAD_ABS(x)     x < 0 ? -x : x     /* 运算符优先级问题 */
#define BAD_SQR(x)     x * x              /* 展开后会出问题 */
```

### 5.4 可配置值使用宏

所有可配置的数字、字符串、上限值必须定义为宏，禁止在代码中出现魔法数。

```c
/* 好 */
#define UART_RX_BUF_SIZE        2048U
#define UART_DEFAULT_BAUDRATE   115200UL
#define UART_TIMEOUT_MS         1000U

if (elapsed >= UART_TIMEOUT_MS) {
    /* ... */
}

/* 坏 */
if (elapsed >= 1000) {
    /* 1000 是什么？秒还是毫秒？ */
}
```

---

## 6. 类型定义

### 6.1 使用 C99 标准整数类型

```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

int8_t, int16_t, int32_t, int64_t      /* 有符号 */
uint8_t, uint16_t, uint32_t, uint64_t  /* 无符号 */
size_t, ssize_t, ptrdiff_t             /* 大小/指针差 */
bool, true, false                      /* 布尔 */
```

**禁止**：`unsigned int`, `short`, `long` 等模糊宽度类型。

### 6.2 结构体设计

- 按字段大小从大到小排列，减少填充
- 相同类型的字段相邻放置
- 使用显式填充确保对齐（如有必要）

```c
/* 好：合理的字段排列 */
typedef struct {
    uint8_t * buf;          /* 8 bytes (64-bit) */
    size_t capacity;        /* 8 bytes */
    uint64_t timestamp;     /* 8 bytes */
    uint32_t flags;         /* 4 bytes */
    uint32_t write_cnt;     /* 4 bytes */
    uint16_t crc;           /* 2 bytes */
    ring_buf_mode_t mode;   /* 4 bytes (枚举一般为int) */
    uint8_t  ref_cnt;       /* 1 byte  */
    uint8_t  reserved[3];   /* 填充对齐到8字节边界 */
} ring_buf_t;
```

### 6.3 不透明类型

对外隐藏内部结构，使用前向声明：

```c
/* ---- module.h ---- */
typedef struct module_ctx module_ctx_t;    /* 前向声明 */

module_ctx_t * module_create(const module_cfg_t * cfg);
void           module_destroy(module_ctx_t * ctx);
c_std_res_t    module_start(module_ctx_t * ctx);

/* ---- module.c ---- */
struct module_ctx {
    /* 内部字段不对外暴露 */
    uint32_t state;
    int32_t  fd;
    /* ... */
};
```

---

## 7. 函数设计

### 7.1 函数长度

- 一般函数不超过 **50 行**（不含空行和注释）
- 超过则拆分内部逻辑为独立静态函数

### 7.2 参数设计

- 参数个数不超过 **6 个**
- 超过时应将参数打包为结构体
- 输入参数在前，输出参数在后
- 指针输入参数加 `const`

```c
/* 好 */
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

/* 坏：参数太多 */
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

### 7.3 返回值设计

- 默认统一返回 `<module>_res_t` 或 `c_std_res_t`
- 创建类函数失败返回 NULL
- 输出通过指针参数返回

```c
/* 通用结果码 */
typedef enum {
    C_STD_RES_OK = 0,      /**< 成功 */
    C_STD_RES_ERR,         /**< 通用错误 */
    C_STD_RES_INV_PARAM,   /**< 无效参数 */
    C_STD_RES_NO_MEM,      /**< 内存不足 */
    C_STD_RES_NOT_READY,   /**< 未就绪 */
    C_STD_RES_TIMEOUT,     /**< 超时 */
} c_std_res_t;

/* 使用 */
c_std_res_t data_parser_process(const uint8_t * in, size_t in_len,
                                uint8_t * out, size_t * out_len);
```

### 7.4 参数校验

- 所有对外函数必须校验入参
- 指针对 NULL，整型对范围

```c
c_std_res_t ring_buf_write(ring_buf_t * rb, const void * data, size_t len)
{
    if (!rb || !data) {
        return C_STD_RES_INV_PARAM;
    }

    if (len == 0) {
        return C_STD_RES_OK;    /* 0 长度视为成功，无需操作 */
    }

    if (len > rb->capacity) {
        return C_STD_RES_INV_PARAM;
    }

    /* 正常逻辑 */
    return C_STD_RES_OK;
}
```

### 7.5 回调与上下文

回调必须支持 user_data 指针：

```c
typedef void (*async_op_cb_t)(c_std_res_t result, void * user_data);

c_std_res_t async_read(const char * path,
                       uint8_t * buf,
                       size_t len,
                       async_op_cb_t cb,
                       void * user_data);
```

---

## 8. 变量使用

### 8.1 声明位置

- C99 允许在代码块任意位置声明
- **推荐**：就近声明，首次使用前声明

```c
c_std_res_t process_packet(const uint8_t * data, size_t len)
{
    if (len < PACKET_HDR_SIZE) {
        return C_STD_RES_INV_PARAM;
    }

    /* 头部校验通过后再解析其他字段，变量就近声明 */
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

### 8.2 初始化

- 声明时必须初始化
- 指针初始化为 NULL
- 数值初始化为 0 或有效值

```c
/* 好 */
uint32_t flags = 0U;
int32_t ret = 0;
uint8_t * buf = NULL;
ring_buf_t rb = {0};    /* 结构体清零 */

/* 坏 */
uint32_t flags;
int32_t ret;
uint8_t * buf;
```

### 8.3 全局变量

- **禁止**使用非静态全局变量（跨文件共享用 getter/setter）
- 文件域变量必须加 `static`

```c
/* 好 */
static ring_buf_t * sg_uart_rx_buf = NULL;
static uint32_t sg_init_done = 0U;

ring_buf_t * uart_get_rx_buf(void)
{
    return sg_uart_rx_buf;
}
```

---

## 9. 错误处理

### 9.1 统一错误返回路径

使用 goto 跳转到统一清理出口（唯一允许使用 goto 的场景）：

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

    /* 正常业务逻辑 ... */
    res = C_STD_RES_OK;

out:
    if (fd >= 0)          close_file(fd);
    if (buf != NULL)      free(buf);
    if (ctx != NULL)      free_context(ctx);
    return res;
}
```

### 9.2 禁止静默失败

- 错误必须处理或向上传递
- 禁止忽略返回值（除非文档明确说明可忽略）

```c
/* 好 */
c_std_res_t res = send_packet(pkt);
if (res != C_STD_RES_OK) {
    LOG_E("send_packet failed: %d", res);
    return res;
}

/* 允许显式忽略（需说明原因） */
(void)send_heartbeat();    /* 心跳失败不影响主流程，忽略返回 */

/* 坏 */
send_packet(pkt);          /* 静默忽略，出问题无法定位 */
```

### 9.3 断言使用

- 使用断言捕获开发阶段的逻辑错误
- 运行时仍需保留正常校验

```c
/* 好：断言 + 运行时检查并存 */
c_std_res_t module_send_msg(module_ctx_t * ctx, msg_t * msg)
{
    /* 断言：开发期应保证 ctx 非空（调用者错误） */
    C_STD_ASSERT(ctx != NULL);
    C_STD_ASSERT(msg != NULL);

    /* 运行时检查：release 版本仍要防护 */
    if (ctx == NULL || msg == NULL) {
        return C_STD_RES_INV_PARAM;
    }

    /* 逻辑断言：内部状态必须一致 */
    C_STD_ASSERT(ctx->state == STATE_READY);
    /* ... */
    return C_STD_RES_OK;
}
```

---

## 10. 内存管理

### 10.1 分配/释放配对

- 谁分配谁释放（同一模块层级）
- 提供 create/destroy 配对 API
- destroy 函数中指针置 NULL（防 double-free）

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

### 10.2 缓冲区操作

- 所有写入操作必须传入缓冲区大小
- 使用 `snprintf` 而非 `sprintf`
- 使用 `memcpy_s`/`strncpy` 等安全版本（如可用）

```c
/* 好 */
char buf[64];
int32_t n = snprintf(buf, sizeof(buf), "%s-%u", name, index);
if (n < 0 || (size_t)n >= sizeof(buf)) {
    /* 截断了，处理错误 */
}

memcpy(dst, src, C_STD_MIN(dst_len, src_len));

/* 坏 */
sprintf(buf, "%s-%u", name, index);
memcpy(dst, src, src_len);    /* dst 可能不够大 */
```

### 10.3 避免栈溢出

- 大数组（> 256 字节）动态分配或静态
- 递归深度必须可控

```c
/* 好 */
static uint8_t sg_parse_buf[4096];   /* BSS 段，不占栈 */

void parser_init(void)
{
    uint8_t * tmp = (uint8_t *)malloc(8192);
    /* ... */
}

/* 坏 */
void parser_bad(void)
{
    uint8_t parse_buf[8192];       /* 栈上放8KB，风险高 */
}
```

---

## 11. 性能优化

### 11.1 const 使用

所有只读指针、变量都加 `const`：

```c
uint32_t calc_crc(const uint8_t * data, size_t len);
int32_t find_index(const char * const * str_table, uint32_t count, const char * key);
```

### 11.2 内联小函数

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

### 11.3 热点循环优化

- 避免循环内重复计算相同值
- 避免循环内调用虚函数/回调
- 数组按行优先访问（C 语言行优先）

```c
/* 好：提升不变量到循环外 */
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

### 11.4 位操作

```c
/* 宏定义避免魔法数 */
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

## 12. 可移植性

### 12.1 字节序

```c
/* 使用字节序转换函数，不要依赖平台 */
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

### 12.2 结构体序列化

禁止直接 memcpy 结构体到网络/文件，必须手动按字段序列化：

```c
/* 好：显式序列化 */
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

/* 坏：依赖平台对齐和字节序 */
memcpy(out, pkt, sizeof(packet_t));
```

### 12.3 平台抽象

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

## 13. 构建系统

### 13.1 警告选项

```makefile
# GCC / Clang 推荐编译选项
CFLAGS += -std=c99 -O2 -g
CFLAGS += -Wall -Wextra -Wpedantic
CFLAGS += -Wshadow -Wconversion -Wsign-conversion
CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wimplicit-function-declaration
CFLAGS += -Wundef -Wunreachable-code
CFLAGS += -Wformat=2
CFLAGS += -Werror    /* 警告视为错误，提交前必须开启 */
```

### 13.2 模块划分

```
# 看具体工程
```

---

## 14. 示例模板

### 14.1 ring_buf.h

```c
/**
 * @file ring_buf.h
 * @brief 环形缓冲区实现
 *
 * 提供任意大小数据的 FIFO 读写。
 * 非线程安全，多线程环境下外层需加锁。
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
 * @brief 满时处理模式
 */
typedef enum {
    RING_BUF_MODE_BLOCK = 0,    /* <- 阻塞：拒绝写入 */
    RING_BUF_MODE_OVERWRITE,    /* <- 覆盖：覆盖最旧数据 */
    RING_BUF_MODE_DROP,         /* <- 丢弃：丢弃新数据 */
} ring_buf_mode_t;

/** 前向声明 */
typedef struct ring_buf ring_buf_t;

/**
 * @brief 溢出回调
 * @param rb        缓冲区实例
 * @param user_data 用户数据
 */
typedef void (*ring_buf_overflow_cb_t)(ring_buf_t * rb, void * user_data);

/********************************
 *    Global Prototypes
 ********************************/

/**
 * @brief 创建环形缓冲区
 * @param capacity  容量（字节，>=4）
 * @param mode      满时模式
 * @return 成功返回指针，失败返回 NULL
 */
ring_buf_t * ring_buf_create(size_t capacity, ring_buf_mode_t mode);

/**
 * @brief 销毁缓冲区
 * @param rb  缓冲区实例（允许 NULL）
 */
void ring_buf_destroy(ring_buf_t * rb);

/**
 * @brief 写入数据
 * @param rb    缓冲区实例
 * @param data  数据源
 * @param len   写入长度
 * @return 实际写入的字节数
 */
size_t ring_buf_write(ring_buf_t * rb, const void * data, size_t len);

/**
 * @brief 读取数据
 * @param rb    缓冲区实例
 * @param out   输出缓冲区
 * @param len   想要读取的长度
 * @return 实际读取的字节数
 */
size_t ring_buf_read(ring_buf_t * rb, void * out, size_t len);

/**
 * @brief 获取已使用字节数
 * @param rb  缓冲区实例
 * @return 已使用字节数
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
 * @brief 环形缓冲区实现
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
 * @brief 环形缓冲区结构
 */
struct ring_buf {
    uint8_t * buf;              /* <- 存储区 */
    size_t capacity;            /* <- 总容量 */
    size_t head;                /* <- 写入位置 */
    size_t tail;                /* <- 读取位置 */
    ring_buf_mode_t mode;       /* <- 满时行为 */
    ring_buf_overflow_cb_t cb;  /* <- 溢出回调 */
    void * cb_data;             /* <- 回调用户数据 */
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

    /* 处理满时策略 */
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
                /* 丢弃足够的旧数据 */
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
 * @brief 前向推进索引（环形）
 * @param ptr   当前索引
 * @param n     推进字节数
 * @param cap   容量
 * @return 新索引
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

**文档结束**
