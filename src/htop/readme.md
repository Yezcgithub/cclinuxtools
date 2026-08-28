# htop

A single-file, cross-platform, zero-dependency reimplementation of GNU htop(1)
in C99. It provides an interactive, full-screen process viewer with CPU /
memory gauges, live sorting, filtering, and per-process actions, and degrades
to a plain batch mode for scripting when stdout is not a terminal.

- **Pure C99**, one file (`htop.c`), no external libraries.
- **Platforms:** Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD.
- **Interactive TUI** with ANSI colors and a fallback **batch mode** (`-b`).
- **MIT** licensed.

## Features

- Uptime / load-average header and a `Tasks:` summary line.
- Per-CPU usage bars (up to 8 CPUs shown individually; a single aggregate
  bar for larger machines), plus memory and swap bars with used / cache
  segments.
- Process table: `PID PPID USER PRI NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND`.
- Live **sorting** by any column, with **reverse** order.
- **Tree view** (children grouped under their parent) and **thread view**.
- Case-insensitive **regex filter** on the command (ERE subset: `.` `*` `+`
  `?` `[]` `^` `$` and the classes `\s \d \w \S \D \W`).
- Per-process actions: **send a signal** (HUP/INT/QUIT/ABRT/KILL/TERM/CONT/STOP)
  and **change the nice value**.
- Color output that respects `--no-color` and is automatically disabled in
  batch / non-TTY output.

## Options

| Option | Description |
| ------ | ----------- |
| `-b, --batch` | run in batch mode (non-interactive, plain output) |
| `-i` | do not start the interactive UI (implies `-b`) |
| `-d, --delay=SEC` | seconds between updates (float, default 2) |
| `-n, --iterations=N` | exit after N iterations (batch mode) |
| `-s, --sort-key=FIELD` | sort by FIELD (`PID PPID USER PRI NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND`) |
| `-r, --reverse` | reverse the sort order |
| `-f, --filter=PATTERN` | show only processes whose command matches PATTERN |
| `--follow=PID` | start in follow mode on PID |
| `-t, --tree` | show processes in a tree view |
| `-T` | show threads as separate rows |
| `-u, --user=USER` | show only USER's processes |
| `-p, --pid=N` | show only PIDs (comma-separated list) |
| `-w, --wide` | show the full command line |
| `-W, --width=N` | force the output width to N columns |
| `--limit-rows=N` | limit the process table to N rows |
| `--fields=LIST` | comma-separated column list (accepted; the column set is fixed in this build) |
| `--no-color` | disable ANSI colors |
| `--show-cumulative` | treat %CPU as cumulative (accepted) |
| `--show-program-path` | show the full program path in COMMAND |
| `-h, --help` | print help and exit |
| `-V, --version` | print version information and exit |

## Interactive keys

| Key | Action |
| --- | ------ |
| `F1` | help line |
| `F2` | setup dialog (delay, sort, colors, options) |
| `F3`, `/` | filter processes (regex) |
| `F4` | sort menu |
| `F5` | toggle tree view |
| `F6` | follow the selected process |
| `F7`, `F9`, `k` | send a signal |
| `F8`, `n` | change the nice value |
| `F10`, `Esc`, `q` | quit |
| `F11`, `F12` | decrease / increase the delay |
| `r` | reverse the sort order |
| `s` | toggle kernel threads |
| `T` | toggle the thread view |
| `w`, `c` | toggle the wide command / command line |
| `+`, `-` | raise / lower the selected process's nice value |
| `Tab` | cycle the sort column |
| `g`, `G` | jump to the first / last process |
| `Ctrl+L` | force a redraw |
| arrows, `PgUp`, `PgDn`, `Home`, `End` | move the selection |

## Examples

```sh
# interactive full-screen viewer
htop

# one frame, plain output (auto-batch when piped)
htop -b

# three frames, 0.5 s apart, sorted by RES
htop -b -n 3 -d 0.5 -s RES

# only processes whose command matches the regex "htop"
htop -b -f htop

# tree view, wide command line
htop -t -w

# only PIDs 123 and 456, no colors, at most 20 rows
htop -p 123,456 --no-color --limit-rows 20
```

## Build

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

Or run the build + regression suite directly:

```sh
./build.sh     # POSIX / Git Bash
build.bat      # Windows cmd
```

## License

- MIT
- https://mit-license.org/
