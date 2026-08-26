# find

Search for files in a directory hierarchy.

## Synopsis
`
find [PATH...] [EXPRESSION]
`

## Tests

| Test                  | Description                              |
| --------------------- | ---------------------------------------- |
| -name PATTERN       | Match filename (glob)                    |
| -iname PATTERN      | Match filename (case-insensitive)        |
| -path PATTERN       | Match full path                          |
| -regex PATTERN      | Match path against regex                 |
| -type TYPE          | File type (f/d/l/b/c/s/p)               |
| -size N[cwbkMG]     | File size test                           |
| -empty              | Empty file or directory                  |
| -mtime N            | Modified N days ago                      |
| -atime N            | Accessed N days ago                      |
| -ctime N            | Changed N days ago                       |
| -perm MODE          | Permission test                          |
| -newer REF          | Modified more recently than REF          |
| -true / -false    | Always true / false                      |

## Actions

| Action                | Description                              |
| --------------------- | ---------------------------------------- |
| -print              | Print path (default)                     |
| -print0             | Print path with NUL delimiter            |
| -ls                 | ls -dils format                          |
| -delete             | Delete found files                       |
| -exec CMD {} ;      | Execute command                          |
| -ok CMD {} ;        | Execute with confirmation                |
| -prune              | Don't descend into directory             |
| -quit               | Exit immediately                         |

## Options

| Option                  | Description                              |
| ----------------------- | ---------------------------------------- |
| -maxdepth N           | Max search depth                         |
| -mindepth N           | Min search depth                         |
| ` ! EXPR`             | Negate expression                        |
| ` ( EXPR )`           | Group expressions                        |
| ` EXPR -a EXPR`       | AND (default)                            |
| ` EXPR -o EXPR`       | OR                                        |

## Examples
`
find . -name "*.c" -type f
find /tmp -mtime -7 -delete
find . -name "*.log" -exec grep -l error {} +
find . -maxdepth 2 -type d
`

## Build
`
gcc -O2 -std=c99 -Wall -o find find.c
`
