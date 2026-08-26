# head

跨平台 `head` — 输出文件的前几行。

## 概要

```
head [选项]... [文件]...
```

## 选项

| 选项                       | 说明                                       |
| -------------------------- | ------------------------------------------ |
| `-n, --lines=[+]NUM`       | 打印前 NUM 行（默认 10）                  |
| `-c, --bytes=[+]NUM`       | 打印前 NUM 字节                           |
| `-q, --quiet, --silent`    | 不打印文件名标题                          |
| `-v, --verbose`            | 始终打印文件名标题                        |
| `-z, --zero-terminated`    | 行分隔符为 NUL，而非换行                  |
| `--help`                   | 显示帮助并退出                            |
| `--version`                | 显示版本并退出                            |

## 说明

- `+NUM`：打印前 NUM 行/字节（跳过前 NUM-1）。
- `-NUM`：除最后 NUM 行/字节外全部打印。
- 字节后缀：`b`(512)、`c`(1)、`w`(2)、`kB`(1000)、`K`(1024)、`MB`、`GB` 等。

## 示例

```bash
head file.txt                # 前 10 行
head -n 20 file.txt          # 前 20 行
head -c 100 file.txt         # 前 100 字节
head -n +5 file.txt          # 从第 5 行开始
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o head head.c
```