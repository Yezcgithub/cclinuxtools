# id

Cross-platform `id` — print user and group information.

## Synopsis

```
id [OPTION]... [USERNAME]
```

## Options

| Option            | Description                                      |
| ----------------- | ------------------------------------------------ |
| `-u, --user`      | Print only the effective user ID                 |
| `-g, --group`     | Print only the effective group ID                |
| `-G, --groups`    | Print all group IDs                              |
| `-n, --name`      | Print names instead of numbers (with -u, -g, -G) |
| `-r, --real`      | Print real ID instead of effective ID            |
| `-z, --zero`      | Delimit entries with NUL instead of whitespace   |
| `-a`              | Ignored (for compatibility)                      |
| `--help`          | Display help and exit                            |
| `--version`       | Display version and exit                         |

## Description

With no options, prints the full identity in the default format:

```
uid=1000(user) gid=1000(user) groups=1000(user),4(adm),27(sudo)
```

When the optional USERNAME is given, print information for that user instead of the current user.

## Examples

```bash
id                              # uid=1000(user) gid=1000(user) groups=...
id -u                           # 1000 (effective user ID)
id -g                           # 1000 (effective group ID)
id -G                           # 1000 4 27 (all group IDs)
id -un                          # user (effective user name)
id -Gn                          # user adm sudo (all group names)
id -u root                      # 0 (for another user)
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o id.exe id.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o id id.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o id id.c
```
