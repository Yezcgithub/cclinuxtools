# cut

Cross-platform `cut` — remove sections from each line of files.

## Synopsis

```
cut OPTION... [FILE]...
```

## Options

| Option                              | Description                                      |
| ----------------------------------- | ------------------------------------------------ |
| `-b, --bytes=LIST`                  | Select only these byte positions                 |
| `-c, --characters=LIST`             | Select only these character positions            |
| `-d, --delimiter=DELIM`             | Use DELIM instead of TAB for field delimiter     |
| `-f, --fields=LIST`                 | Select only these fields                         |
| `-F LIST`                           | Like -f, but also implies -w and output delimiter is space |
| `-n, --no-partial`                  | With -b, don't output partial multi-byte chars   |
| `-O, --output-delimiter=STRING`     | Use STRING as output delimiter                   |
| `-s, --only-delimited`              | Do not print lines not containing delimiters     |
| `-w, --whitespace-delimited`        | Use whitespace runs as delimiter                 |
| `--complement`                      | Complement the set of selected bytes/chars/fields|
| `-z, --zero-terminated`             | Line delimiter is NUL, not newline               |
| `--help`                            | Display help and exit                            |
| `--version`                         | Display version and exit                         |

## LIST Format

LIST consists of one range separated by a comma, or a single number:
- `N`       — Nth byte/char/field, starting at 1
- `N-`      — from Nth to end of line
- `N-M`     — from Nth to Mth (inclusive)
- `-M`      — from 1st to Mth

## Examples

```bash
cut -d: -f1,3 /etc/passwd       # fields 1 and 3, colon-delimited
cut -c1-5 file.txt               # first 5 characters of each line
cut -b1-3 file.txt               # first 3 bytes
echo "a,b,c" | cut -d, -f2      # b
cut -d: -f1 --complement /etc/passwd  # all fields except 1st
cut -w -f1 file.txt              # first whitespace-delimited field
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o cut.exe cut.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o cut cut.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o cut cut.c
```
