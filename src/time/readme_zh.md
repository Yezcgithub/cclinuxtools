# time

跨平台 `time` — 测量命令的执行时间。

## 概要

```
time [选项]... 命令 [参数...]
```

## 选项

| 选项                     | 说明                                         |
| ------------------------ | -------------------------------------------- |
| `-p, --portability`      | POSIX 输出格式（real/user/sys 秒数）         |
| `-f, --format=格式`      | 自定义输出格式字符串                         |
| `-o, --output=文件`      | 将计时写入文件而非 stderr                    |
| `-a, --append`           | 追加到文件而非覆盖（配合 -o）                |
| `-v, --verbose`          | 详细输出                                     |
| `--help`                 | 显示帮助并退出                               |
| `--version`              | 显示版本并退出                               |

## 格式说明符

| 说明符 | 说明                                             |
| ------ | ------------------------------------------------ |
| `%C`   | 命令名称和参数                                   |
| `%D`   | 平均非共享数据区大小（KB）                       |
| `%E`   | 实际用时（时:分:秒）                             |
| `%F`   | 主要页面错误次数                                 |
| `%I`   | 文件系统输入次数                                 |
| `%k`   | 发送给进程的信号次数                             |
| `%M`   | 最大常驻集大小（KB）                             |
| `%O`   | 文件系统输出次数                                 |
| `%P`   | CPU 使用百分比                                  |
| `%R`   | 次要页面错误次数                                 |
| `%S`   | 内核态 CPU 时间（秒）                            |
| `%U`   | 用户态 CPU 时间（秒）                            |
| `%W`   | 进程被换出的次数                                 |
| `%X`   | 平均共享文本量（KB）                             |
| `%Z`   | 系统页面大小（字节）                             |
| `%c`   | 非自愿上下文切换次数                             |
| `%e`   | 实际用时（秒，浮点数）                           |
| `%r`   | 收到的套接字消息数                               |
| `%s`   | 发送的套接字消息数                               |

## 说明

- 默认格式：`%Uuser %Ssystem %Eelapsed %PCPU (%Xtext+%Ddata %Mmax)k ...`
- POSIX 上使用 `wait4()` / `getrusage()` 获取资源使用情况。
- Windows 上使用 `GetProcessTimes()` 获取计时信息。

## 示例

```bash
time ls -la
time -p sleep 1
time -f "Elapsed: %E, CPU: %P" some_command
time -o timing.txt -a make
time -v heavy_command
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o time.exe time.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o time time.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o time time.c
```
