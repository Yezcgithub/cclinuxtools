# touch

Cross-platform `touch` — change file timestamps or create empty files.

## Synopsis

```
touch [OPTION]... FILE...
```

## Options

| Option                     | Description                                    |
| -------------------------- | ---------------------------------------------- |
| `-a`                       | Change only the access time                    |
| `-m`                       | Change only the modification time              |
| `-c, --no-create`          | Do not create any files                        |
| `-d, --date=STRING`        | Parse STRING as date, use instead of current   |
| `-r, --reference=FILE`     | Use this file's times instead of current time  |
| `-t STAMP`                 | Use `[[CC]YY]MMDDhhmm[.ss]` format            |
| `-h, --no-dereference`     | Affect each symlink itself, not the target     |
| `--time=WORD`              | Change the specified time (access/modify)      |
| `--help`                   | Display help and exit                          |
| `--version`                | Display version and exit                       |

## Examples

```bash
touch newfile                # create empty file
touch -r ref.txt target.txt  # copy timestamps
touch -d "2025-01-01" f.txt  # set specific date
touch *.log                  # update timestamps
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o touch touch.c
```