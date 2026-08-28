# xargs

跨平台 `xargs` — 从标准输入构建并执行命令行。

## 概要

```
xargs [选项]... [命令 [参数...] ]
```

## 选项

| 选项                        | 说明                                         |
| --------------------------- | -------------------------------------------- |
| `-0, --null`                | 输入项以 NUL 结尾                            |
| `-a, --arg-file=文件`       | 从文件读取输入项而非标准输入                 |
| `-d, --delimiter=字符`      | 输入项分隔符                                 |
| `-E EOF字符串`              | 设置文件结束字符串                           |
| `-I 替换字符串`             | 在命令中替换出现的字符串                     |
| `-L 最大行数`               | 每个命令行的最大行数                         |
| `-n 最大参数数`             | 每个命令行的最大参数数                       |
| `-P 最大并行数`             | 最大并行进程数                               |
| `-r, --no-run-if-empty`     | 无输入时不执行命令                           |
| `-s 最大字符数`             | 每个命令行的最大字符数                       |
| `-t, --verbose`             | 执行前将命令打印到 stderr                    |
| `-p, --interactive`         | 每个命令执行前提示确认                       |
| `-x, --exit`                | 超出命令行长度限制时退出                     |
| `--help`                    | 显示帮助并退出                               |
| `--version`                 | 显示版本并退出                               |

## 说明

- 默认分隔符为空白字符（空格、制表符、换行符）。
- 未指定命令时使用 `echo`。
- 命令行长度限制取决于平台。
- 使用 `-I {}` 在命令中进行占位符替换。
- 配合 `find -print0` 使用 `-0` 可安全处理含特殊字符的文件名。

## 示例

```bash
find . -name "*.tmp" | xargs rm
find . -name "*.log" | xargs -I {} mv {} /backup/
cat urls.txt | xargs -n 1 wget
ls *.txt | xargs -P 4 -I {} gzip {}
echo "a b c" | xargs -n 1           # 执行: echo a; echo b; echo c
find . -name "*.c" -print0 | xargs -0 grep "TODO"
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o xargs.exe xargs.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o xargs xargs.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o xargs xargs.c
```
