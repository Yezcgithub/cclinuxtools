# mkdir

Cross-platform `mkdir` — make directories.

## Synopsis

```
mkdir [OPTION]... DIRECTORY...
```

## Options

| Option                   | Description                                    |
| ------------------------ | ---------------------------------------------- |
| `-m, --mode=MODE`        | Set file permission mode (octal or symbolic)   |
| `-p, --parents`          | Create parent directories; no error if existing|
| `-v, --verbose`          | Print a message for each created directory     |
| `--help`                 | Display help and exit                          |
| `--version`              | Display version and exit                       |

## Examples

```bash
mkdir newdir
mkdir -p a/b/c/d
mkdir -m 755 public
mkdir -pv /tmp/{a,b,c}
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o mkdir mkdir.c
```