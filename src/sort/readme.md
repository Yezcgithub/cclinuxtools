# sort

Cross-platform `sort` — sort lines of text files.

## Synopsis

```
sort [OPTION]... [FILE]...
sort [OPTION]... -o OUTPUT [FILE]...
```

## Options

| Option                     | Description                                    |
| -------------------------- | ---------------------------------------------- |
| `-f, --ignore-case`        | Fold lower to upper case                       |
| `-n, --numeric-sort`       | Compare by string numeric value                |
| `-g, --general-numeric-sort` | Compare by general numeric value             |
| `-h, --human-numeric-sort` | Compare by human readable size                 |
| `-V, --version-sort`       | Natural version number sort                    |
| `-r, --reverse`            | Reverse the result                             |
| `-R, --random-sort`        | Sort by random hash of keys                    |
| `-M, --month-sort`         | Compare JAN < ... < DEC                        |
| `-d, --dictionary-order`   | Only blanks and alphanumeric                   |
| `-b, --ignore-leading-blanks` | Ignore leading blanks                        |
| `-i, --ignore-nonprinting` | Only printable and blanks                      |
| `-k, --key=KEYDEF`         | Sort by key definition                         |
| `-t, --field-separator=SEP`| Use SEP as field separator                     |
| `-u, --unique`             | Only output unique lines                       |
| `-c, --check`              | Check for sorted input                         |
| `-m, --merge`              | Merge already sorted files                     |
| `-o, --output=FILE`        | Output to FILE                                 |
| `-s, --stable`             | Stabilize sort                                 |
| `-z, --zero-terminated`    | Line delimiter is NUL                          |
| `--debug`                  | Annotate sort keys to stderr                   |
| `--help`                   | Display help and exit                          |
| `--version`                | Display version and exit                       |

## Examples

```bash
sort file.txt                      # lexicographic sort
sort -n numbers.txt                 # numeric sort
sort -k2,2 -t: /etc/passwd         # sort by 2nd field (colon-separated)
sort -u file.txt                    # sort and deduplicate
sort -h sizes.txt                   # human-readable size sort
sort -V versions.txt                # version sort (v1 < v2 < v10)
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o sort sort.c
```