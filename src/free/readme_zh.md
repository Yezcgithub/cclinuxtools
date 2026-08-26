# free

显示系统中空闲和已用内存量。

## 概要
 `
free [选项]
` 

## 选项

| 选项             | 说明                                       |
| ---------------- | ------------------------------------------ |
| -b, --bytes    | 以字节显示 SI 1000 进制                    |
| -k, --kibi     | 以 KiB 显示 1024 默认                      |
| -m, --mebi     | 以 MiB 显示                               |
| -g, --gibi     | 以 GiB 显示                               |
| -h, --human    | 自动缩放并添加单位后缀                    |
| --si           | 使用 1000 进制缩放                         |
| -l, --lohi     | 显示详细的低高端内存统计                  |
| -t, --total    | 显示总计内存加交换空间                    |
| -w, --wide     | 宽模式分离缓冲区和缓存列                  |
| -s N           | 每 N 秒重复                               |
| -c N           | 重复 N 次后退出 需配合 -s                 |
| --help         | 显示帮助并退出                            |
| --version      | 显示版本并退出                            |

## 平台数据来源

| 平台   | 来源                                         |
| ------ | -------------------------------------------- |
| Linux  | /proc/meminfo                                |
| Windows | GlobalMemoryStatusEx + GetPerformanceInfo    |
| macOS  | sysctl + mach host_statistics64              |
| FreeBSD | sysctl 变体                                 |

## 示例
 `
free                         # 默认 KiB
free -h                      # 人类可读
free -m                      # 以 MiB 显示
free -s 5                    # 每 5 秒重复
` 

## 编译
 `
gcc -O2 -std=c99 -Wall -o free free.c
` 
