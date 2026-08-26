# 一个通用跨平台的Linux工具集

 很高兴你能看到这个开源项目，希望这个项目能帮到你。



## 一、简介

- **单文件实现**每个工具只依赖标准 C 库，复制单个 `.c` 文件即可编译，无需安装依赖。
- **纯 C99**：遵循 ISO C99 标准，使用 POSIX 特性时通过特性宏显式声明，保证最大可移植性。
- **跨平台抽象**：源文件顶部内置平台检测宏（Windows / Linux / macOS / *BSD），针对不同系统调用正确的 API。
- **POSIX 风格路径**：所有平台统一输出正斜杠路径，UTF-8 编码，Windows 端通过宽字符 API 支持非 ANSI 路径。
- **自动化测试**：构建脚本内置大量回归测试，覆盖主要 CLI 选项和边界场景。
- **性能优先**：直接调用系统 API，避免不必要的抽象层；正则编译缓存、动态字符串等优化。
- **MIT 许可证**：可自由用于商业项目、嵌入式产品，无 GPL 传染风险。



## 二、适用场景

| 场景           | 说明                                                         |
| -------------- | ------------------------------------------------------------ |
| 嵌入式 Linux   | 编译后体积小（20~200KB），适合 OpenWrt / Buildroot / Yocto   |
| Docker 最小镜像 | 替代 alpine 中的 busybox，减少镜像体积                       |
| 恢复环境       | Live CD / PE 环境中无需安装即可使用常用命令                  |
| Windows 开发   | 在 Windows 上获得类 Unix 命令行体验，无需安装 Cygwin/MSYS2  |
| 教学学习       | 单文件实现，代码清晰，适合学习系统编程和 CLI 工具开发         |
| 跨平台项目     | 为项目提供内建的工具集，不依赖系统自带命令                   |


```bash
gcc -O2 -std=c99 -Wall -o ls ls.c     # 一个文件，一行命令
```

不需要 CMake、不需要 autoconf、不需要 pkg-config、不需要安装任何第三方库。编译出来的二进制也不依赖任何动态库（纯静态链接标准 C 库）。

### 1. 纯 C99 标准

完全遵循 ISO C99 规范，不使用任何编译器扩展。需要系统调用时（文件操作、终端控制等），通过标准的 POSIX 特性宏显式声明：

```c
#define _POSIX_C_SOURCE 200809L   // Linux / 其他 Unix
#define _DARWIN_C_SOURCE          // macOS
#define _NETBSD_SOURCE            // NetBSD
```

这意味着代码可以在任何声称支持 C99 的编译器上编译——GCC、Clang、MSVC（配合 C99 模式）、TCC、Open Watcom 等。

### 2. 真正跨平台

每个源文件顶部内置平台检测，针对不同操作系统调用正确的原生 API：

| 平台     | 文件操作 API                          | 终端 API          |
| -------- | ------------------------------------- | ----------------- |
| Windows  | `FindFirstFile` / `GetFileAttributes` | Win32 Console API |
| Linux    | `opendir` / `lstat` / `d_type`       | `termios`         |
| macOS    | `opendir` / `lstat` / `dirent`       | `termios`         |
| *BSD     | `opendir` / `lstat`                  | `termios`         |

不是通过 Cygwin/MSYS2 模拟 POSIX，而是直接调用各平台的原生 API，性能和兼容性都更好。

### 3. 体积小巧，适合嵌入式

单个工具编译后通常只有 **20KB~200KB**，非常适合：
- 嵌入式 Linux 开发板（OpenWrt、Buildroot）
- Docker 最小镜像（scratch / alpine）
- 恢复环境 / Live CD
- 不方便安装软件的受限服务器

### 4. 构建脚本即测试

每个工具的 `build.sh` / `build.bat` 不只是编译脚本，还内置了 20~30+ 项自动化回归测试。运行构建脚本 = 编译 + 测试一步到位：

```bash
./build.sh     # 编译 + 跑全部测试
build.bat      # Windows 下同理
```



## 三、与其他方案对比

### vs 系统自带工具

| 对比项       | 本项目                         | GNU Coreutils / 系统自带          |
| ------------ | ------------------------------ | --------------------------------- |
| 跨平台       | Windows / Linux / macOS / *BSD | 仅 Unix-like（Windows 需 Cygwin） |
| 依赖         | 零依赖，单文件                 | 需要完整运行时环境                |
| 体积         | 单工具 20KB~200KB              | Coreutils 整包 ~1MB+              |
| 编译方式     | `gcc -o ls ls.c` 一行搞定      | autotools / configure / make      |
| Windows 支持 | 原生 Win32 API                 | 需要 MSYS2/Cygwin 模拟层         |
| 可定制性     | 源码单文件，随意裁剪           | 需要理解复杂构建系统              |

### vs BusyBox

| 对比项       | 本项目                  | BusyBox                          |
| ------------ | ----------------------- | -------------------------------- |
| 实现语言     | 纯 C99                  | C（大量 GNU 扩展）               |
| 构建复杂度   | 单文件编译              | 需要完整构建系统（Kbuild）       |
| 命令风格     | POSIX 标准 + 常用扩展   | 精简版（部分行为与 GNU 不同）    |
| Windows 支持 | 原生支持                | 不支持                           |
| 许可证       | MIT                     | GPL v2                           |
| 适用场景     | 开发工具 / 桌面环境     | 嵌入式 Linux / Alpine 最小系统   |

### vs 其他单文件实现

| 对比项       | 本项目                          | sbase (suckless) / heirloom   |
| ------------ | ------------------------------- | ------------------------------ |
| 平台支持     | Windows + Unix 全平台           | 仅 Unix-like                   |
| Windows 原生 | 是（Win32 API）                 | 否                             |
| 构建测试     | 内置自动化测试                  | 需要手动验证                   |
| 命令覆盖     | ls/cp/mv/rm/find/sed/awk/bash 等 12+ 个 | 约 20 个基础命令       |
| 中文文档     | 有                              | 无                             |



## 四、支持平台

| 平台         | 编译器                     | 特性宏                         |
| ------------ | -------------------------- | ------------------------------ |
| Windows      | GCC (MinGW / MSYS2 / TDM) | —                              |
| Linux        | GCC                        | `_POSIX_C_SOURCE=200809L`      |
| macOS        | GCC / Clang                | `_DARWIN_C_SOURCE`             |
| FreeBSD      | Clang (`cc`)               | —                              |
| OpenBSD      | Clang (`cc`)               | —                              |
| NetBSD       | Clang (`cc`)               | `_NETBSD_SOURCE`               |
| 其他 Unix    | 任意 C99 编译器            | —                              |



## 五、 许可证

七、许可证
The MIT License (MIT)

https://mit-license.org/

Copyright © 2026 <Yezc/cclinuxtools>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.