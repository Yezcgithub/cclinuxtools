# arch

Cross-platform `arch` — prints the machine hardware name.

## Synopsis

```
arch
arch --help
arch --version
```

## Description

Displays the machine hardware name, equivalent to `uname -m`.

- On POSIX systems, reads from `uname(2)` machine field.
- On Windows, reads from `PROCESSOR_ARCHITECTURE` environment variable.

## Options

| Option      | Description              |
| ----------- | ------------------------ |
| `--help`    | Display help and exit    |
| `--version` | Display version and exit |

## Examples

```bash
arch          # e.g. x86_64, i686, ARM64
uname -m      # equivalent
```

## Build

```bash
# Linux / macOS / BSD
gcc -O2 -std=c99 -Wall -o arch arch.c

# Windows
gcc -O2 -std=c99 -Wall -o arch.exe arch.c
```