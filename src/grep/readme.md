# grep

Cross-platform `grep` — search for patterns in files.

## Synopsis

```
grep [OPTION]... PATTERN [FILE]...
```

## Pattern Options

| Option      | Description                              |
| ----------- | ---------------------------------------- |
| `-G`        | Basic Regular Expressions (default)      |
| `-E`        | Extended Regular Expressions (ERE)       |
| `-F`        | Fixed strings (no regex)                 |
| `-e PATTERN`| Use PATTERN for matching                 |
| `-f FILE`   | Obtain patterns from FILE                |

## Matching Options

| Option      | Description                              |
| ----------- | ---------------------------------------- |
| `-i, -y`    | Ignore case                              |
| `-v`        | Invert match                             |
| `-w`        | Match whole words only                   |
| `-x`        | Match whole lines only                   |
| `-m NUM`    | Stop after NUM matches                   |

## Output Options

| Option      | Description                              |
| ----------- | ---------------------------------------- |
| `-n`        | Prefix each line with line number        |
| `-b`        | Prefix each line with byte offset        |
| `-c`        | Print only count of matching lines       |
| `-o`        | Print only matching parts                |
| `-l`        | Print only filenames with matches        |
| `-L`        | Print only filenames without matches     |
| `-q`        | Quiet: no output, exit status only       |
| `-H`        | Always print filename                    |
| `-h`        | Never print filename                     |
| `--color[=WHEN]` | Highlight matches                   |

## Context Options

| Option        | Description                            |
| ------------- | -------------------------------------- |
| `-A NUM`      | Print NUM lines after match            |
| `-B NUM`      | Print NUM lines before match           |
| `-C NUM`      | Print NUM lines before and after       |

## Recursive Options

| Option                 | Description                            |
| ---------------------- | -------------------------------------- |
| `-r, -R`               | Read all files under directories       |
| `--include=PATTERN`    | Only include files matching PATTERN    |
| `--exclude=PATTERN`    | Exclude files matching PATTERN         |
| `--exclude-dir=PATTERN`| Exclude directories matching PATTERN   |
| `-a, --text`           | Process binary files as text           |
| `-I`                   | Skip binary files                      |

## Examples

```bash
grep "error" log.txt
grep -r "TODO" src/
grep -i "hello" file.txt
grep -n "pattern" file.txt
grep -c "error" log.txt
grep -E "^[0-9]+" data.txt
grep -A3 -B1 "exception" app.log
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o grep grep.c
```