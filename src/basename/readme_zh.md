# basename

跨平台 `basename` — 从文件名中去除目录和后缀。

## 概要

```
basename NAME [SUFFIX]
basename [OPTION]... NAME...
```

## 说明

打印去掉所有前导目录组件后的 `NAME`。如果指定了 `SUFFIX`，同时去除尾部后缀。

## 选项

| 选项                  | 说明                              |
| --------------------- | --------------------------------- |
| `-a, --multiple`      | 支持多个 NAME 参数                |
| `-s, --suffix=SUFFIX` | 去除尾部 SUFFIX                   |
| `-z, --zero`          | 用 NUL 分隔输出，而非换行         |
| `--help`              | 显示帮助并退出                    |
| `--version`           | 显示版本并退出                    |

## 示例

```bash
basename /usr/local/bin/ls        # ls
basename /usr/local/bin/ls .c     # ls（没有 .c 后缀可去除）
basename -s .c main.c utils.c     # main\nutils
basename -a /a/b/c /x/y/z        # c\nz
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o basename basename.c
```