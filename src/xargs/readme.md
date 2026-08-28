# xargs

Cross-platform `xargs` — build and execute command lines from standard input.

## Synopsis

```
xargs [OPTION]... [COMMAND [ARG...] ]
```

## Options

| Option                      | Description                                      |
| --------------------------- | ------------------------------------------------ |
| `-0, --null`                | Input items are NUL-terminated                   |
| `-a, --arg-file=FILE`       | Read items from FILE instead of stdin            |
| `-d, --delimiter=CHAR`      | Input item delimiter                             |
| `-E EOF-STR`                | Set end-of-file string                           |
| `-I REPLACE-STR`            | Replace occurrences of REPLACE-STR in command    |
| `-L MAX-LINES`              | Max lines per command line                       |
| `-n MAX-ARGS`               | Max arguments per command line                   |
| `-P MAX-PROCS`              | Max parallel processes                           |
| `-r, --no-run-if-empty`     | Don't run command if no input                    |
| `-s MAX-CHARS`              | Max chars per command line                       |
| `-t, --verbose`             | Print command to stderr before executing         |
| `-p, --interactive`         | Prompt before each command                       |
| `-x, --exit`                | Exit if command line length exceeded             |
| `--help`                    | Display help and exit                            |
| `--version`                 | Display version and exit                         |

## Notes

- Default delimiter is whitespace (space, tab, newline).
- Without COMMAND, uses `echo`.
- Command line length limit is platform-dependent.
- Use `-I {}` for placeholder replacement in commands.
- Use `-0` with `find -print0` for safe handling of filenames with special characters.

## Examples

```bash
find . -name "*.tmp" | xargs rm
find . -name "*.log" | xargs -I {} mv {} /backup/
cat urls.txt | xargs -n 1 wget
ls *.txt | xargs -P 4 -I {} gzip {}
echo "a b c" | xargs -n 1           # run: echo a; echo b; echo c
find . -name "*.c" -print0 | xargs -0 grep "TODO"
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o xargs.exe xargs.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o xargs xargs.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o xargs xargs.c
```
