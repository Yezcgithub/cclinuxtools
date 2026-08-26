# pwd

跨平台 `pwd` — 打印当前工作目录名称。

## 概要

```
pwd [选项]
```

## 选项

| 选项                 | 说明                                     |
| -------------------- | ---------------------------------------- |
| `-L, --logical`      | 使用环境变量 `$PWD`（当有效时）          |
| `-P, --physical`     | 解析所有符号链接（默认）                 |
| `--help`             | 显示帮助并退出                           |
| `--version`          | 显示版本并退出                           |

## 说明

- 所有平台输出 POSIX 风格的正斜杠路径。
- Windows 支持宽字符路径以处理非 ANSI 目录名。
- 所有平台使用 UTF-8 编码输出。

## 示例

```bash
pwd             # /home/user/project
cd /tmp && pwd  # /tmp
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o pwd pwd.c
```