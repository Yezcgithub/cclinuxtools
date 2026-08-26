# ls

Cross-platform `ls` — list directory contents.

## Synopsis

```
ls [OPTION]... [FILE]...
```

## Options

| Option             | Description                                        |
| ------------------ | -------------------------------------------------- |
| `-a, --all`        | Show all entries including `.` and `..`             |
| `-A`               | Show all except `.` and `..`                       |
| `-l`               | Long format (permissions, owner, group, size, time)|
| `-h, --human`      | Human-readable sizes (1K, 243M, 2G)                |
| `-R, --recursive`  | List subdirectories recursively                    |
| `-C`               | Columnar output (terminal width detection)         |
| `--color[=WHEN]`   | Colorize output (auto/always/never)                |
| `-F, --classify`   | Append indicator (`/`, `*`, `@`, `=`, `|`)         |
| `-p`               | Append `/` indicator to directories                |
| `-i, --inode`      | Print index number of each file                    |
| `-S`               | Sort by file size (largest first)                  |
| `-t`               | Sort by modification time (newest first)           |
| `-r, --reverse`    | Reverse sort order                                 |
| `-1`               | One file per line                                  |
| `-s, --size`       | Print allocated size of each file                  |
| `--help`           | Display help and exit                              |
| `--version`        | Display version and exit                           |

## Examples

```bash
ls                  # list current directory
ls -la              # long format, all files
ls -lhtr            # human sizes, by time, reversed
ls -R               # recursive
ls --color=always   # colored output
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o ls ls.c
```