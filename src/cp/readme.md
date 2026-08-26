# cp

Cross-platform `cp` command — copy files and directories.

## Synopsis

```
cp [OPTION]... SOURCE DEST
cp [OPTION]... SOURCE... DIRECTORY
```

## Options

| Option                        | Description                                      |
| ----------------------------- | ------------------------------------------------ |
| `-a, --archive`               | Archive mode (`-r -p -d`), preserves all attributes |
| `-f, --force`                 | Force overwrite, never prompt                    |
| `-i, --interactive`           | Prompt before overwrite                          |
| `-n, --no-clobber`            | Never overwrite existing files                   |
| `-p, --preserve`              | Preserve mode, timestamps, owner, group          |
| `-r, -R, --recursive`         | Copy directories recursively                     |
| `-s, --symbolic-link`         | Create symlinks instead of copying               |
| `-l, --link`                  | Create hard links instead of copying             |
| `-u, --update`                | Copy only when source is newer                   |
| `-v, --verbose`               | Explain what is being done                       |
| `-L, --dereference`           | Always follow symbolic links in SOURCE           |
| `-P, --no-dereference`        | Never follow symbolic links                      |
| `-t, --target-directory=DIR`  | Copy all SOURCE files into DIR                   |
| `-T, --no-target-directory`   | Treat DEST as a normal file                      |
| `--help`                      | Display help and exit                            |
| `--version`                   | Display version and exit                         |

## Examples

```bash
cp file.txt backup.txt
cp -r project/ backup/project/
cp -a src/ /backup/src/
cp -i *.txt /backup/           # prompt before overwrite
cp -u config.yml /etc/         # only copy if newer
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o cp cp.c
```