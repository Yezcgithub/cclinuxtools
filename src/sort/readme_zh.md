# sort

跨平台 `sort` — 排序文本文件的行。

## 概要

```
sort [选项]... [文件]...
sort [选项]... -o 输出文件 [文件]...
```

## 选项

| 选项                       | 说明                                       |
| -------------------------- | ------------------------------------------ |
| `-f, --ignore-case`        | 忽略大小写                                 |
| `-n, --numeric-sort`       | 按字符串数值比较                           |
| `-g, --general-numeric-sort` | 按通用数值比较                          |
| `-h, --human-numeric-sort` | 按人类可读大小比较                         |
| `-V, --version-sort`       | 自然版本号排序                             |
| `-r, --reverse`            | 反转结果                                   |
| `-R, --random-sort`        | 按键的随机哈希排序                         |
| `-M, --month-sort`         | 按月份 JAN < ... < DEC                     |
| `-d, --dictionary-order`   | 仅空白和字母数字                           |
| `-b, --ignore-leading-blanks` | 忽略前导空白                            |
| `-i, --ignore-nonprinting` | 仅可打印字符和空白                         |
| `-k, --key=KEYDEF`         | 按键定义排序                               |
| `-t, --field-separator=分隔符` | 使用分隔符作为字段分隔                 |
| `-u, --unique`             | 仅输出唯一行                               |
| `-c, --check`              | 检查是否已排序                             |
| `-m, --merge`              | 合并已排序文件                             |
| `-o, --output=文件`        | 输出到文件                                 |
| `-s, --stable`             | 稳定排序                                   |
| `-z, --zero-terminated`    | 行分隔符为 NUL                             |
| `--debug`                  | 将排序键注释输出到 stderr                  |
| `--help`                   | 显示帮助并退出                             |
| `--version`                | 显示版本并退出                             |

## 示例

```bash
sort file.txt                      # 字典序排序
sort -n numbers.txt                 # 数值排序
sort -k2,2 -t: /etc/passwd         # 按第 2 个字段排序（冒号分隔）
sort -u file.txt                    # 排序并去重
sort -h sizes.txt                   # 人类可读大小排序
sort -V versions.txt                # 版本号排序（v1 < v2 < v10）
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o sort sort.c
```