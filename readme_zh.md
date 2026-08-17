# cclinuxtools

**用纯 C99 实现的跨平台 Linux 常用命令工具集**

每个工具均为独立单文件，零外部依赖，可在 Windows、Linux、macOS、FreeBSD、OpenBSD、NetBSD 等系统上编译运行。

---

## 目录结构

```
cclinuxtools/
├── awk/       # AWK 解释器
├── bash/      # Bash 风格 Shell
├── cd/        # 切换目录
├── cp/        # 复制文件/目录
├── echo/      # 文本输出
├── find/      # 文件搜索
├── ls/        # 目录列表
├── mkdir/     # 创建目录
├── mv/        # 移动/重命名
├── pwd/       # 显示当前目录
├── rm/        # 删除文件/目录
├── sed/       # 流编辑器
├── LICENSE
└── README.md
```

每个工具目录下包含：
- `*.c` — 工具源码（单文件实现）
- `build.sh` — Unix/Linux/macOS/BSD 构建脚本（含自动化测试）
- `build.bat` — Windows 构建脚本（含自动化测试）

---

## 支持平台

| 平台         | 编译器                     | 特性宏                         |
| ------------ | -------------------------- | ------------------------------ |
| Windows      | GCC (MinGW / MSYS2)        | —                              |
| Linux        | GCC                        | `_POSIX_C_SOURCE=200809L`      |
| macOS        | GCC / Clang                | `_DARWIN_C_SOURCE`             |
| FreeBSD      | Clang (`cc`)               | —                              |
| OpenBSD      | Clang (`cc`)               | —                              |
| NetBSD       | Clang (`cc`)               | `_NETBSD_SOURCE`               |
| 其他 Unix    | 任意 C99 编译器            | —                              |

---

## 工具一览

### 文件系统操作

| 工具    | 主要功能                                                                 |
| ------- | ------------------------------------------------------------------------ |
| **ls**  | 目录列表：长格式 `-l`、着色 `--color`、递归 `-R`、多种排序、人读尺寸 `-h`、inode `-i`、文件类型指示 `-F/-p` |
| **find**| 文件搜索：`-name/-iname/-path/-regex/-type/-size/-mtime` 等测试，`-print/-print0/-ls/-delete/-exec/-ok` 动作，AND/OR/NOT 表达式 |
| **cp**  | 文件/目录复制：递归 `-r`、保留属性 `-p`、归档 `-a`、交互 `-i`、强制覆盖 `-n`、更新 `-u`、符号链接 `-s`、硬链接 `-l` |
| **mv**  | 移动/重命名：强制 `-f`、交互 `-i`、不覆盖 `-n`、更新 `-u`、备份 `-b`、目录递归移动 |
| **rm**  | 删除文件/目录：强制 `-f`、交互 `-i/-I`、递归 `-r`、删除空目录 `-d`、根目录保护 `--preserve-root` |
| **mkdir**| 创建目录：设置权限 `-m`、递归创建父目录 `-p`、详细输出 `-v` |
| **cd**  | 切换目录：逻辑路径 `-L`、物理路径 `-P`、`cd -` 返回、`~` 展开、`CDPATH` 搜索 |
| **pwd** | 显示当前目录：逻辑路径 `-L`、物理路径 `-P`、Windows 宽字符路径支持 |
| **echo**| 文本输出：`-n` 不换行、`-e/-E` 转义解释、POSIXLY_CORRECT 兼容模式 |

### 文本处理

| 工具    | 主要功能                                                                 |
| ------- | ------------------------------------------------------------------------ |
| **awk** | AWK 解释器：模式-动作规则、字段 `$1..$NF`、关联数组、正则、内置函数（sub/gsub/match/split/substr/printf 等）、控制流、用户自定义函数 |
| **sed** | 流编辑器：地址（行号/正则/范围/first~step/+N）、`s///` 替换（g/p/i/Nth）、完整命令集（a/i/c/d/D/p/P/n/N/h/H/g/G/x/b/t/T/q/Q/r/w/l/=/y/{} 等）、内置微型 NFA 正则引擎（Windows 端）、就地编辑 `-i` |

### Shell 环境

| 工具    | 主要功能                                                                 |
| ------- | ------------------------------------------------------------------------ |
| **bash**| Bash 风格 Shell：交互/脚本（`-c`/文件）模式、管道/重定向/heredoc、40+ 内置命令、变量/参数/命令/算术展开、通配符、控制流（if/for/while/until/case/select/函数）、局部变量 |

---

## 快速开始

### 方式一：使用构建脚本（推荐）

每个工具目录下均提供了自动化构建脚本，包含平台检测、编译和内置测试。

**Unix/Linux/macOS/BSD：**
```bash
cd ls
chmod +x build.sh
./build.sh
```

**Windows（MinGW/MSYS2）：**
```cmd
cd ls
build.bat
```

构建脚本会自动：
1. 清理旧产物
2. 检测目标平台并设置正确编译标志
3. 编译生成可执行文件
4. 运行 20+ 项自动化功能测试

### 方式二：手动编译

以 GCC 为例（各平台等效命令见各源文件头部注释）：

**Windows：**
```cmd
gcc -O2 -std=c99 -Wall -o ls.exe ls/ls.c
```

**Linux：**
```bash
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o ls ls/ls.c
```

**macOS：**
```bash
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o ls ls/ls.c
```

**FreeBSD / OpenBSD：**
```bash
cc -O2 -std=c99 -Wall -o ls ls/ls.c
```

**NetBSD：**
```bash
cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o ls ls/ls.c
```

---

## 使用示例

```bash
# ls - 长格式列出当前目录，按时间倒序，人类可读尺寸
./ls -lht

# find - 在 src 目录下查找 7 天内修改过的 .c 文件并删除
./find src -name "*.c" -mtime -7 -delete

# awk - 统计 /etc/passwd 中各 shell 的使用数
./awk -F: '{cnt[$7]++} END {for (s in cnt) print s, cnt[s]}' /etc/passwd

# sed - 将文件中所有 foo 替换为 bar，就地备份为 .bak
./sed -i.bak 's/foo/bar/g' config.txt

# cp - 归档模式复制目录，保留所有属性
./cp -a project/ /backup/project_2025/

# bash - 执行内联脚本
./bash -c 'for i in 1 2 3; do echo "num=$i"; done'
```

---

## 设计特点

- **单文件实现**：每个工具只依赖标准 C 库，复制单个 `.c` 文件即可编译，无需安装依赖。
- **纯 C99**：遵循 ISO C99 标准，使用 POSIX 特性时通过特性宏显式声明，保证最大可移植性。
- **跨平台抽象**：源文件顶部内置平台检测宏（Windows / Linux / macOS / *BSD），针对不同系统调用正确的 API（如 Windows 用 `FindFirstFile`，POSIX 用 `opendir/lstat`）。
- **POSIX 风格路径**：所有平台统一输出正斜杠路径，UTF-8 编码，Windows 端通过宽字符 API 支持非 ANSI 路径。
- **Doxygen 注释**：每个源文件头部包含完整的功能说明、构建命令和版权信息，符合 LVGL 代码风格。
- **自动化测试**：构建脚本内置大量回归测试，覆盖主要 CLI 选项和边界场景。
- **性能优先**：直接调用系统 API，避免不必要的抽象层；正则编译缓存、动态字符串等优化。

---

## 许可证

MIT License — 详见 LICENSE

Copyright © 2025-2026 Yezc / cclinuxtools
