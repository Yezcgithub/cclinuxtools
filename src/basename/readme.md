# basename

Cross-platform `basename` — strip directory and suffix from filenames.

## Synopsis

```
basename NAME [SUFFIX]
basename [OPTION]... NAME...
```

## Description

Prints `NAME` with any leading directory components removed. If `SUFFIX` is specified, also removes the trailing suffix.

## Options

| Option               | Description                         |
| -------------------- | ----------------------------------- |
| `-a, --multiple`     | Support multiple NAME arguments     |
| `-s, --suffix=SUFFIX`| Remove trailing SUFFIX              |
| `-z, --zero`         | Separate output with NUL, not newline |
| `--help`             | Display help and exit               |
| `--version`          | Display version and exit            |

## Examples

```bash
basename /usr/local/bin/ls        # ls
basename /usr/local/bin/ls .c     # ls (no .c suffix to remove)
basename -s .c main.c utils.c     # main\nutils
basename -a /a/b/c /x/y/z        # c\nz
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o basename basename.c
```