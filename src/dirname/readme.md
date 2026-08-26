# dirname

Cross-platform `dirname` — strip last component from file names.

## Synopsis

```
dirname NAME
dirname [OPTION] NAME...
```

## Description

Prints the directory portion of `NAME`. If `NAME` has no `/`, outputs `.`. If `NAME` is all slashes, outputs `/`.

## Options

| Option      | Description                                      |
| ----------- | ------------------------------------------------ |
| `-z, --zero`| Separate output with NUL instead of newline      |
| `--help`    | Display help and exit                            |
| `--version` | Display version and exit                         |

## Examples

```bash
dirname /usr/local/bin/ls    # /usr/local/bin
dirname file.txt             # .
dirname //a/b//              # /a
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o dirname dirname.c
```