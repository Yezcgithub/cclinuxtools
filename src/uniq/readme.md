# uniq

Cross-platform `uniq` — filter adjacent matching lines from input.

## Synopsis

```
uniq [OPTION]... [INPUT [OUTPUT]]
```

## Options

| Option                     | Description                                    |
| -------------------------- | ---------------------------------------------- |
| `-c, --count`              | Prefix lines with occurrence count             |
| `-d, --repeated`           | Print only first copy of repeated lines        |
| `-D, --all-repeated[=METHOD]` | Print all copies of repeated lines          |
| `-u, --unique`             | Print only unique (non-repeated) lines         |
| `-f N, --skip-fields=N`    | Skip N leading fields before compare           |
| `-s N, --skip-chars=N`     | Skip N characters before compare               |
| `-i, --ignore-case`        | Ignore case in comparisons                     |
| `-w N, --check-chars=N`    | Compare at most N chars per line               |
| `-z, --zero-terminated`    | NUL-delimited items (not newlines)             |
| `--group[=METHOD]`         | Print all lines, delimiting each group         |
| `--help`                   | Display help and exit                          |
| `--version`                | Display version and exit                       |

## Examples

```bash
sort file.txt | uniq            # remove adjacent duplicates
sort file.txt | uniq -c         # count occurrences
sort file.txt | uniq -d         # print only duplicates
sort file.txt | uniq -u         # print only unique lines
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o uniq uniq.c
```