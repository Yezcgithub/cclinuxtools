# mv

Cross-platform `mv` — move (rename) files and directories.

## Synopsis

```
mv [OPTION]... SOURCE DEST
mv [OPTION]... SOURCE... DIRECTORY
```

## Options

| Option       | Description                                |
| ------------ | ------------------------------------------ |
| `-f, --force`| Overwrite without prompting                |
| `-i, --interactive` | Prompt before overwrite              |
| `-n, --no-clobber` | Do not overwrite existing files       |
| `-u, --update` | Move only when source is newer           |
| `-v, --verbose` | Explain what is being done              |
| `-b, --backup` | Create backup of each existing destination |
| `--help`     | Display help and exit                     |
| `--version`  | Display version and exit                  |

## Examples

```bash
mv old.txt new.txt
mv -i file.txt /backup/
mv -f *.log /tmp/
mv -v src/ dst/
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o mv mv.c
```