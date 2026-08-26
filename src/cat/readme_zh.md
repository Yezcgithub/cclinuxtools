# cat

跨平台 `cat` — 连接并打印文件。

## 概要

```
cat [选项]... [文件]...
```

## 选项

| 选项                        | 说明                                        |
| --------------------------- | ------------------------------------------- |
| `-A, --show-all`            | 等同于 `-vET`                               |
| `-b, --number-nonblank`     | 仅对非空输出行编号                          |
| `-e`                        | 等同于 `-vE`                                |
| `-E, --show-ends`           | 在每行末尾显示 `$`                          |
| `-n, --number`              | 对所有输出行编号                            |
| `-s, --squeeze-blank`       | 压缩连续的空行                              |
| `-t`                        | 等同于 `-vT`                                |
| `-T, --show-tabs`           | 将 TAB 字符显示为 `^I`                      |
| `-v, --show-nonprinting`    | 使用 `^` 和 `M-` 表示非打印字符            |
| `--help`                    | 显示帮助并退出                              |
| `--version`                 | 显示版本并退出                              |

## 说明

- 未给定 FILE 或 FILE 为 `-` 时从标准输入读取。
- 二进制安全 I/O；UTF-8 透传。
- Windows 宽字符路径支持。

## 示例

```bash
cat file.txt
cat -n file.txt               # 带行号
cat -A file.txt               # 显示所有特殊字符
cat file1 file2 > merged.txt
echo "hello" | cat            # 从标准输入读取
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o cat cat.c
```