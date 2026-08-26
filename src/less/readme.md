# less

Cross-platform `less` — file pager for interactive viewing.

## Synopsis

```
less [OPTION]... [FILE]...
```

## Options

| Option              | Description                                    |
| ------------------- | ---------------------------------------------- |
| `-E, --quit-at-eof` | Quit at end of file                            |
| `-F, --quit-if-one-screen` | Quit if file fits on one screen          |
| `-N, --LINE-NUMBERS`| Display line numbers                           |
| `-s, --squeeze-blank-lines` | Squeeze multiple blank lines            |
| `-S, --chop-long-lines` | Chop long lines (no wrapping)              |
| `-R, --RAW-CONTROL-CHARS` | Pass through ANSI/control chars          |
| `-i, --ignore-case` | Case-insensitive search                        |
| `-I, --IGNORE-CASE` | Case-insensitive search (ignoring non-alpha)   |
| `-p PATTERN`        | Jump to first match of PATTERN                 |
| `-x N, --tabs=N`    | Set tab stops every N characters               |
| `-J, --status-column` | Display status column                        |
| `-f, --force`       | Force open non-regular files                   |
| `-o FILE`           | Copy input to FILE                             |
| `-P STR, --prompt=STR` | Set prompt string                          |
| `+cmd`              | Apply initial command (`+N`, `+F`, `+/pat`)   |
| `--help`            | Display help and exit                          |
| `--version`         | Display version and exit                       |

## Interactive Keys

| Key           | Action                               |
| ------------- | ------------------------------------ |
| `q` / `Esc`  | Quit                                 |
| `Space` / `f`| Scroll forward one screen            |
| `b`           | Scroll backward one screen           |
| `j` / `Enter`| Scroll forward one line              |
| `k`           | Scroll backward one line             |
| `g`           | Go to first line                     |
| `G`           | Go to last line                      |
| `/pattern`    | Search forward                       |
| `?pattern`    | Search backward                      |
| `n`           | Next search match                    |
| `N`           | Previous search match                |
| `h`           | Help                                 |

## Examples

```bash
less file.txt
less -N file.txt               # with line numbers
less +50 file.txt              # start at line 50
ls -la | less                  # pipe input
less +/error log.txt           # start at first "error"
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o less less.c
```