# htop

单文件、跨平台、零依赖的 GNU htop(1) C99 重写实现。提供交互式全屏进程查看器：CPU / 内存仪表条、实时排序、过滤、逐进程操作；当 stdout 不是终端时自动降级为纯文本 batch 模式，方便脚本使用。

- **纯 C99**，单文件（`htop.c`），无任何外部库。
- **支持平台：** Windows、Linux、macOS、FreeBSD、OpenBSD、NetBSD。
- **交互式 TUI**（ANSI 彩色）+ **batch 模式**（`-b`）兜底。
- **MIT** 许可。

## 功能特性

- 顶部 uptime / 负载均值头行与 `Tasks:` 任务统计行。
- 每 CPU 独立使用率条形图（≤8 核逐核显示，更多核显示聚合条），以及内存 / Swap 条（已用 / 缓存分段）。
- 进程表：`PID PPID USER PRI NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND`。
- 任意列**实时排序**，支持**反序**。
- **树形视图**（子进程归组在父进程之下）与**线程视图**。
- 大小写不敏感的**正则过滤**（作用于命令名，ERE 子集：`.` `*` `+` `?` `[]` `^` `$` 及字符类 `\s \d \w \S \D \W`）。
- 逐进程操作：**发送信号**（HUP/INT/QUIT/ABRT/KILL/TERM/CONT/STOP）与**修改 nice 值**。
- 彩色输出遵循 `--no-color`，batch / 非 TTY 输出自动关闭颜色。

## 选项

| 选项 | 说明 |
| ------ | ----------- |
| `-b, --batch` | batch 模式运行（非交互、纯文本输出） |
| `-i` | 不启动交互界面（等价于 `-b`） |
| `-d, --delay=SEC` | 刷新间隔秒数（浮点，默认 2） |
| `-n, --iterations=N` | N 次迭代后退出（batch 模式） |
| `-s, --sort-key=FIELD` | 按 FIELD 排序（`PID PPID USER PRI NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND`） |
| `-r, --reverse` | 反转排序方向 |
| `-f, --filter=PATTERN` | 只显示命令匹配 PATTERN 的进程 |
| `--follow=PID` | 以跟随模式启动（跟踪指定 PID） |
| `-t, --tree` | 树形视图显示进程 |
| `-T` | 把线程作为独立行显示 |
| `-u, --user=USER` | 只显示 USER 的进程 |
| `-p, --pid=N` | 只显示指定 PID（逗号分隔列表） |
| `-w, --wide` | 显示完整命令行 |
| `-W, --width=N` | 强制输出宽度为 N 列 |
| `--limit-rows=N` | 进程表最多显示 N 行 |
| `--fields=LIST` | 逗号分隔的列列表（当前构建接受该参数，列集合固定） |
| `--no-color` | 禁用 ANSI 颜色 |
| `--show-cumulative` | 将 %CPU 按累计口径计算（接受该参数） |
| `--show-program-path` | COMMAND 列显示完整程序路径 |
| `-h, --help` | 打印帮助并退出 |
| `-V, --version` | 打印版本信息并退出 |

## 交互按键

| 按键 | 功能 |
| --- | ------ |
| `F1` | 帮助行 |
| `F2` | 设置对话框（延迟、排序、配色、选项） |
| `F3`、`/` | 过滤进程（正则） |
| `F4` | 排序菜单 |
| `F5` | 切换树形视图 |
| `F6` | 跟随选中进程 |
| `F7`、`F9`、`k` | 发送信号 |
| `F8`、`n` | 修改 nice 值 |
| `F10`、`Esc`、`q` | 退出 |
| `F11`、`F12` | 减小 / 增大刷新延迟 |
| `r` | 反转排序 |
| `s` | 切换内核线程可见性 |
| `T` | 切换线程视图 |
| `w`、`c` | 切换完整命令行 |
| `+`、`-` | 提高 / 降低选中进程的 nice 值 |
| `Tab` | 循环切换排序列 |
| `g`、`G` | 跳到第一个 / 最后一个进程 |
| `Ctrl+L` | 强制重绘 |
| 方向键、`PgUp`、`PgDn`、`Home`、`End` | 移动选中行 |

## 示例

```sh
# 交互式全屏查看
htop

# 输出一帧（管道下自动进入 batch 模式）
htop -b

# 3 帧、0.5 秒间隔、按 RES 排序
htop -b -n 3 -d 0.5 -s RES

# 只显示命令匹配正则 "htop" 的进程
htop -b -f htop

# 树形视图 + 完整命令行
htop -t -w

# 只看 PID 123 和 456，无色、最多 20 行
htop -p 123,456 --no-color --limit-rows 20
```

## 编译

```sh
# Windows (MinGW)
gcc -O2 -std=c99 -Wall -Wextra -o htop.exe htop.c -lpsapi -ladvapi32

# Linux
gcc -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -o htop htop.c

# macOS
gcc -O2 -std=c99 -Wall -Wextra -D_DARWIN_C_SOURCE -o htop htop.c

# FreeBSD / OpenBSD
cc -O2 -std=c99 -Wall -Wextra -o htop htop.c

# NetBSD
cc -O2 -std=c99 -Wall -Wextra -D_NETBSD_SOURCE -o htop htop.c
```

也可以直接运行构建 + 回归测试套件：

```sh
./build.sh     # POSIX / Git Bash
build.bat      # Windows cmd
```

## 许可

- MIT
- https://mit-license.org/
