# uniq

跨平台 `uniq` — 过滤输入中的相邻重复行。

## 概要

```
uniq [选项]... [输入 [输出]]
```

## 选项

| 选项                       | 说明                                       |
| -------------------------- | ------------------------------------------ |
| `-c, --count`              | 在行前添加出现次数                         |
| `-d, --repeated`           | 仅打印重复行的第一行                       |
| `-D, --all-repeated[=METHOD]` | 打印重复行的所有副本                    |
| `-u, --unique`             | 仅打印唯一（非重复）行                     |
| `-f N, --skip-fields=N`    | 比较前跳过 N 个前导字段                    |
| `-s N, --skip-chars=N`     | 比较前跳过 N 个字符                        |
| `-i, --ignore-case`        | 比较时忽略大小写                           |
| `-w N, --check-chars=N`    | 每行最多比较 N 个字符                      |
| `-z, --zero-terminated`    | NUL 分隔（而非换行）                       |
| `--group[=METHOD]`         | 打印所有行，每组之间加分隔                 |
| `--help`                   | 显示帮助并退出                             |
| `--version`                | 显示版本并退出                             |

## 示例

```bash
sort file.txt | uniq            # 去除相邻重复行
sort file.txt | uniq -c         # 统计出现次数
sort file.txt | uniq -d         # 仅打印重复行
sort file.txt | uniq -u         # 仅打印唯一行
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o uniq uniq.c
```