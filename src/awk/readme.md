# awk

Cross-platform AWK interpreter — a substantial subset of AWK.

## Synopsis

```
awk [OPTION]... 'PROGRAM' [FILE]...
awk [OPTION]... -f PROGRAM-FILE [FILE]...
```

## Options

| Option          | Description                                |
| --------------- | ------------------------------------------ |
| `-F fs`         | Set field separator                        |
| `-f progfile`   | Read program from file                     |
| `-v var=val`    | Assign variable before execution           |
| `--help`        | Display help and exit                      |
| `--version`     | Display version and exit                   |

## Key Features

- **Pattern-action rules** with BEGIN/END blocks
- **Fields**: `$0` (full line), `$1..$NF` (individual fields)
- **Associative arrays**: `array[key]`
- **Built-in variables**: `NR`, `NF`, `FS`, `OFS`, `RS`, `FILENAME`, `ENVIRON`
- **Built-in functions**: `print`, `printf`, `sprintf`, `length`, `sub`, `gsub`, `match`, `split`, `index`, `substr`, `tolower`, `toupper`, `system`, `getline`
- **Control flow**: `if/else`, `while`, `do-while`, `for`, `for-in`, `switch`
- **User-defined functions**
- **Regex**: `~`, `!~`, `/regex/`

## Examples

```bash
awk '{print $1}' file.txt                    # print first field
awk -F: '{print $1, $3}' /etc/passwd         # user:UID
awk '{sum += $1} END {print sum}' nums.txt   # sum column
awk '/error/ {count++} END {print count}' log.txt
awk 'BEGIN {OFS=","} {print $1,$2}' data.txt
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o awk awk.c
```