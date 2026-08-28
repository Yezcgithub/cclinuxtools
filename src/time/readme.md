# time

Cross-platform `time` — time the execution of a command.

## Synopsis

```
time [OPTION]... COMMAND [ARG...]
```

## Options

| Option                   | Description                                      |
| ------------------------ | ------------------------------------------------ |
| `-p, --portability`      | POSIX output format (real/user/sys in seconds)   |
| `-f, --format=FORMAT`    | Custom output format string                      |
| `-o, --output=FILE`      | Write timing to FILE instead of stderr           |
| `-a, --append`           | Append to FILE instead of overwriting (with -o)  |
| `-v, --verbose`          | Verbose detailed output                          |
| `--help`                 | Display help and exit                            |
| `--version`              | Display version and exit                         |

## Format Specifiers

| Spec | Description                                          |
| ---- | ---------------------------------------------------- |
| `%C` | Command name and arguments                           |
| `%D` | Average unshared data area size (Kbytes)             |
| `%E` | Elapsed real time (hours:minutes:seconds)            |
| `%F` | Number of major page faults                          |
| `%I` | Number of file system inputs                         |
| `%k` | Number of signals delivered to process               |
| `%M` | Maximum resident set size (Kbytes)                   |
| `%O` | Number of file system outputs                        |
| `%P` | Percent of CPU used                                  |
| `%R` | Number of minor page faults                          |
| `%S` | Total CPU seconds used by the system (kernel)        |
| `%U` | Total CPU seconds used by the user                   |
| `%W` | Number of times process was swapped out              |
| `%X` | Average amount of shared text (Kbytes)               |
| `%Z` | System's page size (bytes)                           |
| `%c` | Number of involuntary context switches               |
| `%e` | Elapsed real time in seconds (floating point)        |
| `%r` | Number of socket messages received                   |
| `%s` | Number of socket messages sent                       |

## Notes

- Default format: `%Uuser %Ssystem %Eelapsed %PCPU (%Xtext+%Ddata %Mmax)k ...`
- On POSIX: uses `wait4()` / `getrusage()` for resource usage.
- On Windows: uses `GetProcessTimes()` for timing info.

## Examples

```bash
time ls -la
time -p sleep 1
time -f "Elapsed: %E, CPU: %P" some_command
time -o timing.txt -a make
time -v heavy_command
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o time.exe time.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o time time.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o time time.c
```
