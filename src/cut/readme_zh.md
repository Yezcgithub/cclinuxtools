# cut

跨平台 `cut` — 从文件的每一行中移除指定部分。

## 概要

```
cut 选项... [文件]...
```

## 选项

| 选项                                | 说明                                         |
| ----------------------------------- | -------------------------------------------- |
| `-b, --bytes=列表`                  | 仅选择指定的字节位置                         |
| `-c, --characters=列表`             | 仅选择指定的字符位置                         |
| `-d, --delimiter=分隔符`            | 使用分隔符替代 TAB 作为字段分隔符            |
| `-f, --fields=列表`                 | 仅选择指定的字段                             |
| `-F 列表`                           | 等同于 -f，同时隐含 -w，输出分隔符为空格     |
| `-n, --no-partial`                  | 配合 -b，不输出部分多字节字符                |
| `-O, --output-delimiter=字符串`     | 使用字符串作为输出分隔符                     |
| `-s, --only-delimited`              | 不打印不含分隔符的行                         |
| `-w, --whitespace-delimited`        | 使用连续空白字符作为分隔符                   |
| `--complement`                      | 取反所选的字节/字符/字段集合                 |
| `-z, --zero-terminated`             | 行分隔符为 NUL，而非换行                     |
| `--help`                            | 显示帮助并退出                               |
| `--version`                         | 显示版本并退出                               |

## 列表格式

列表由逗号分隔的一个范围或单个数字组成：
- `N`       — 第 N 个字节/字符/字段，从 1 开始
- `N-`      — 从第 N 个到行尾
- `N-M`     — 从第 N 个到第 M 个（包含）
- `-M`      — 从第 1 个到第 M 个

## 示例

```bash
cut -d: -f1,3 /etc/passwd       # 冒号分隔，取第 1 和第 3 字段
cut -c1-5 file.txt               # 每行前 5 个字符
cut -b1-3 file.txt               # 前 3 个字节
echo "a,b,c" | cut -d, -f2      # b
cut -d: -f1 --complement /etc/passwd  # 除第 1 字段外的所有字段
cut -w -f1 file.txt              # 第一个空白分隔字段
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o cut.exe cut.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o cut cut.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o cut cut.c
```
