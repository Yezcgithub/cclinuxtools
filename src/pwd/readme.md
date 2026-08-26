# pwd

Cross-platform `pwd` — print name of current/working directory.

## Synopsis

```
pwd [OPTION]
```

## Options

| Option               | Description                                  |
| -------------------- | -------------------------------------------- |
| `-L, --logical`      | Use `$PWD` from environment when valid       |
| `-P, --physical`     | Resolve all symlinks (default)               |
| `--help`             | Display help and exit                        |
| `--version`          | Display version and exit                     |

## Notes

- All platforms output POSIX-style forward-slash paths.
- Windows supports wide-character paths for non-ANSI directories.
- UTF-8 encoded output on all platforms.

## Examples

```bash
pwd             # /home/user/project
cd /tmp && pwd  # /tmp
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o pwd pwd.c
```