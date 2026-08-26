# arch

跨平台 `arch` 命令 — 打印机器硬件名称。

## 概要

```
arch
arch --help
arch --version
```

## 说明

显示机器硬件名称，等同于 `uname -m`。

- POSIX 系统从 `uname(2)` 的 machine 字段读取。
- Windows 从 `PROCESSOR_ARCHITECTURE` 环境变量读取。

## 选项

| 选项        | 说明           |
| ----------- | -------------- |
| `--help`    | 显示帮助并退出 |
| `--version` | 显示版本并退出 |

## 示例

```bash
arch          # 例如 x86_64, i686, ARM64
uname -m      # 等效命令
```

## 编译

```bash
# Linux / macOS / BSD
gcc -O2 -std=c99 -Wall -o arch arch.c

# Windows
gcc -O2 -std=c99 -Wall -o arch.exe arch.c
```