# tail

跨平台 `tail` — 输出文件的最后部分。

## 概要

```
tail [选项]... [文件]...
```

## 选项

| 选项                        | 说明                                       |
| --------------------------- | ------------------------------------------ |
| `-n, --lines=[+]NUM`        | 输出最后 NUM 行（默认 10）                |
| `-c, --bytes=[+]NUM`        | 输出最后 NUM 字节                         |
| `-f, --follow[={name|descriptor}]` | 文件增长时追加输出                |
| `-F`                        | 等同于 `--follow=name --retry`             |
| `--pid=PID`                 | 配合 -f，PID 死亡后终止                   |
| `-q, --quiet, --silent`     | 不打印文件名标题                          |
| `--retry`                   | 持续尝试打开无法访问的文件                |
| `-s, --sleep-interval=N`    | 配合 -f，每次迭代间隔 N 秒                |
| `-v, --verbose`             | 始终打印文件名标题                        |
| `-z, --zero-terminated`     | 行分隔符为 NUL，而非换行                  |
| `--help`                    | 显示帮助并退出                            |
| `--version`                 | 显示版本并退出                            |

## 说明

- `+NUM`：从第 NUM 行开始。
- `-NUM`：最后 NUM 行。
- 字节后缀：`b`(512)、`c`(1)、`w`(2)、`kB`(1000)、`K`(1024)、`MB`、`GB` 等。

## 示例

```bash
tail file.txt                # 最后 10 行
tail -n 50 file.txt          # 最后 50 行
tail -f /var/log/syslog      # 跟踪日志文件
tail -c 200 file.txt         # 最后 200 字节
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o tail tail.c
```