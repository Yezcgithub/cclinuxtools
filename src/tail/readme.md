# tail

Cross-platform `tail` — output the last part of files.

## Synopsis

```
tail [OPTION]... [FILE]...
```

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| `-n, --lines=[+]NUM`        | Output last NUM lines (default 10)             |
| `-c, --bytes=[+]NUM`        | Output last NUM bytes                          |
| `-f, --follow[={name|descriptor}]` | Output appended data as file grows     |
| `-F`                        | Same as `--follow=name --retry`                |
| `--pid=PID`                 | With -f, terminate after PID dies              |
| `-q, --quiet, --silent`     | Never print file name headers                  |
| `--retry`                   | Keep trying to open inaccessible files         |
| `-s, --sleep-interval=N`    | With -f, sleep ~N seconds between iterations   |
| `-v, --verbose`             | Always print file name headers                 |
| `-z, --zero-terminated`     | Line delimiter is NUL, not newline             |
| `--help`                    | Display help and exit                          |
| `--version`                 | Display version and exit                       |

## Notes

- `+NUM`: start at line NUM.
- `-NUM`: last NUM lines.
- Byte suffixes: `b`(512), `c`(1), `w`(2), `kB`(1000), `K`(1024), `MB`, `GB`, etc.

## Examples

```bash
tail file.txt                # last 10 lines
tail -n 50 file.txt          # last 50 lines
tail -f /var/log/syslog      # follow log file
tail -c 200 file.txt         # last 200 bytes
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o tail tail.c
```