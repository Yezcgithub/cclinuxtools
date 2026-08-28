# realpath

Cross-platform `realpath` — print the resolved absolute pathname.

## Synopsis

```
realpath [OPTION]... FILE...
```

## Options

| Option                       | Description                                      |
| ---------------------------- | ------------------------------------------------ |
| `-E, --canonicalize`         | All but the last component must exist (default)  |
| `-e, --canonicalize-existing`| All components of the path must exist            |
| `-m, --canonicalize-missing` | No path components need exist or be a directory  |
| `-L, --logical`              | Resolve `..` components before symlinks          |
| `-P, --physical`             | Resolve symlinks as encountered (default)        |
| `-q, --quiet`                | Suppress most error messages                     |
| `-s, --strip, --no-symlinks` | Don't expand symlinks                            |
| `-z, --zero`                 | End each output line with NUL, not newline       |
| `--relative-to=DIR`          | Print resolved path relative to DIR              |
| `--relative-base=DIR`        | Print absolute paths unless below DIR            |
| `--help`                     | Display help and exit                            |
| `--version`                  | Display version and exit                         |

## Notes

- Resolves `.`, `..`, and redundant slashes.
- On POSIX, resolves symlinks via `realpath(3)`.
- On Windows, resolves via `GetFinalPathNameByHandleA`.
- All platforms output POSIX-style forward-slash paths.

## Examples

```bash
realpath file.txt               # /home/user/project/file.txt
realpath ../foo                 # /home/user/foo
realpath -m /a/b/../../../c    # /c
realpath --relative-to=/a/b /a/b/c/d  # c/d
realpath -q nonexistent.txt     # suppress errors
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o realpath.exe realpath.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o realpath realpath.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o realpath realpath.c
```
