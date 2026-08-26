# top

显示和更新排序的进程信息。

## 概要
 ``ntop [选项]
`` 

## 选项

| 选项                      | 说明                                       |
| ------------------------- | ------------------------------------------ |
| `-b, --batch`           | 批处理模式 无交互                          |
| `-d, --delay=秒`        | 更新间隔秒数                               |
| `-n, --iterations=N`    | 运行 N 次后退出                            |
| `-p, --pid=N`           | 监控指定 PID                               |
| `-u, --user=用户`       | 仅显示指定用户的进程                       |
| `-o, --order-field=字段`| 按字段排序                                 |
| `-c, --cmd-line-toggle` | 切换完整命令行显示                         |
| `-H, --thread`          | 显示线程                                   |
| `-S, --cumulative`      | 切换累计时间                               |
| `--help`                | 显示帮助并退出                             |
| `--version`             | 显示版本并退出                             |

## 交互按键

| 按键    | 操作                    |
| ------- | ----------------------- |
| q Esc   | 退出                    |
| Space   | 刷新屏幕               |
| h       | 切换帮助显示           |
| P       | 按 CPU 排序            |
| M       | 按内存排序             |
| T       | 按时间排序             |
| 1       | 切换单 CPU 视图        |
| k       | 终止进程               |
| r       | 调整进程优先级         |

## 平台数据来源

| 平台   | 来源                                        |
| ------ | ------------------------------------------- |
| Linux  | /proc/[pid]/stat, /proc/[pid]/status        |
| Windows | CreateToolhelp32Snapshot + GetProcessTimes  |
| macOS  | sysctl(KERN_PROC) + proc_pidinfo            |

## 示例
 ``ntop                          # 交互式进程查看器
top -b -n 1                  # 一次性批处理输出
top -d 2                     # 每 2 秒更新
top -p 1234,5678             # 监控指定 PID
`` 

## 编译
 ``ngcc -O2 -std=c99 -Wall -o top top.c
`` 
