# head

Cross-platform `head` — output the first part of files.

## Synopsis

```
head [OPTION]... [FILE]...
```

## Options

| Option                     | Description                                    |
| -------------------------- | ---------------------------------------------- |
| `-n, --lines=[+]NUM`       | Print first NUM lines (default 10)             |
| `-c, --bytes=[+]NUM`       | Print first NUM bytes                          |
| `-q, --quiet, --silent`    | Never print file name headers                  |
| `-v, --verbose`            | Always print file name headers                 |
| `-z, --zero-terminated`    | Line delimiter is NUL, not newline             |
| `--help`                   | Display help and exit                          |
| `--version`                | Display version and exit                       |

## Notes

- `+NUM`: print first NUM lines/bytes (skip first NUM-1).
- `-NUM`: print all but last NUM lines/bytes.
- Byte suffixes: `b`(512), `c`(1), `w`(2), `kB`(1000), `K`(1024), `MB`, `GB`, etc.

## Examples

```bash
head file.txt                # first 10 lines
head -n 20 file.txt          # first 20 lines
head -c 100 file.txt         # first 100 bytes
head -n +5 file.txt          # from line 5 onward
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o head head.c
```