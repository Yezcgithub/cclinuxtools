# rm

Cross-platform `rm` — remove files or directories.

## Synopsis

```
rm [OPTION]... FILE...
```

## Options

| Option                    | Description                                  |
| ------------------------- | -------------------------------------------- |
| `-f, --force`             | Force, ignore nonexistent, never prompt      |
| `-i`                      | Prompt before every removal                  |
| `-I`                      | Prompt once before removing >3 files or recursively |
| `-r, -R, --recursive`     | Remove directories and their contents        |
| `-d, --dir`               | Remove empty directories                     |
| `-v, --verbose`           | Explain what is being done                   |
| `--preserve-root`         | Refuse to operate on `/` (default)           |
| `--no-preserve-root`      | Allow operating on `/`                       |
| `--one-file-system`       | Skip directories on different file systems   |
| `--help`                  | Display help and exit                        |
| `--version`               | Display version and exit                     |

## Notes

- Never removes `.` or `..` components.
- Default: refuses to operate on `/`.

## Examples

```bash
rm file.txt
rm -f tempfile
rm -rf old_project/
rm -i *.bak
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o rm rm.c
```