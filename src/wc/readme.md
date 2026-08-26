# wc

Cross-platform `wc` — print newline, word, and byte counts for each file.

## Synopsis

```
wc [OPTION]... [FILE]...
```

## Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| `-c, --bytes`             | Print byte counts                              |
| `-m, --chars`             | Print character counts (multibyte-aware)       |
| `-l, --lines`             | Print newline counts                           |
| `-w, --words`             | Print word counts                              |
| `-L, --max-line-length`   | Print length of longest line                   |
| `--files0-from=F`         | Read input from files separated by NUL         |
| `--help`                  | Display help and exit                          |
| `--version`               | Display version and exit                       |

## Notes

- Without options: equivalent to `-l -w -c`.
- Multiple files: prints a totals line.
- `-m` uses `mbrtowc()` for proper multibyte character support.

## Examples

```bash
wc file.txt                  # lines  words  bytes
wc -l *.txt                  # line counts
wc -m file.txt               # character count (UTF-8)
wc -L file.txt               # longest line length
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o wc wc.c
```