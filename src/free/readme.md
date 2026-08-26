# free

Display amount of free and used memory in the system.

## Synopsis
 `
free [OPTION]
` 

## Options

| Option           | Description                                    |
| ---------------- | ---------------------------------------------- |
| -b, --bytes    | Display in bytes SI 1000-based                 |
| -k, --kibi     | Display in KiB 1024 default                    |
| -m, --mebi     | Display in MiB                                 |
| -g, --gibi     | Display in GiB                                 |
| -h, --human    | Auto-scale with unit suffix                    |
| --si           | Use 1000-based scaling                         |
| -l, --lohi     | Show detailed low/high memory stats            |
| -t, --total    | Show total mem + swap line                     |
| -w, --wide     | Wide mode separate buffers and cache columns   |
| -s N           | Repeat every N seconds                         |
| -c N           | Repeat N times then exit requires -s           |
| --help         | Display help and exit                          |
| --version      | Display version and exit                       |

## Platform Sources

| Platform | Source                                       |
| -------- | -------------------------------------------- |
| Linux    | /proc/meminfo                                |
| Windows  | GlobalMemoryStatusEx + GetPerformanceInfo     |
| macOS    | sysctl + mach host_statistics64              |
| FreeBSD  | sysctl variants                              |

## Examples
 `
free                         # default KiB
free -h                      # human-readable
free -m                      # in MiB
free -s 5                    # repeat every 5 seconds
` 

## Build
 `
gcc -O2 -std=c99 -Wall -o free free.c
` 
