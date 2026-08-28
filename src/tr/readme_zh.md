# tr

跨平台 `tr` — 转换或删除字符。

## 概要

```
tr 选项... SET1 [SET2]
```

## 选项

| 选项                    | 说明                                         |
| ----------------------- | -------------------------------------------- |
| `-c, -C, --complement`  | 使用 SET1 的补集                             |
| `-d, --delete`          | 删除 SET1 中的字符                           |
| `-s, --squeeze-repeats` | 将连续重复字符替换为单个出现                 |
| `-t, --truncate-set1`   | 将 SET1 截断为 SET2 的长度                   |
| `--help`                | 显示帮助并退出                               |
| `--version`             | 显示版本并退出                               |

## SET 规格

| 说明         | 说明                                         |
| ------------ | -------------------------------------------- |
| `\NNN`       | 八进制值为 NNN 的字符                        |
| `\\`         | 反斜杠                                       |
| `\a`         | 响铃                                         |
| `\b`         | 退格                                         |
| `\f`         | 换页                                         |
| `\n`         | 换行                                         |
| `\r`         | 回车                                         |
| `\t`         | 水平制表符                                   |
| `\v`         | 垂直制表符                                   |
| `CHAR1-CHAR2`| 从 CHAR1 到 CHAR2 的所有字符                 |
| `[CHAR*]`    | 在 SET2 中，复制 CHAR 直到 SET1 的长度       |
| `[CHAR*REPEAT]` | REPEAT 个 CHAR 副本                       |
| `[:alnum:]`  | 所有字母和数字                               |
| `[:alpha:]`  | 所有字母                                     |
| `[:blank:]`  | 所有水平空白                                 |
| `[:digit:]`  | 所有数字                                     |
| `[:lower:]`  | 所有小写字母                                 |
| `[:upper:]`  | 所有大写字母                                 |
| `[:print:]`  | 所有可打印字符                               |
| `[:punct:]`  | 所有标点字符                                 |
| `[:space:]`  | 所有水平或垂直空白                           |
| `[=CHAR=]`   | 与 CHAR 等价的所有字符                       |

## 示例

```bash
echo "hello" | tr 'a-z' 'A-Z'           # HELLO
echo "hello" | tr -d 'l'                # heo
echo "aaabbbccc" | tr -s 'a-c'          # abc
echo "abc123" | tr -cd '0-9\n'          # 123
echo "one  two   three" | tr -s ' '     # one two three
tr '(' ')' < file.txt                    # 将 ( 替换为 )
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o tr.exe tr.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o tr tr.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o tr tr.c
```
