# top

Display and update sorted information about processes.

## Synopsis
 ``ntop [OPTION]
`` 

## Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| `-b, --batch`           | Batch mode no interaction                       |
| `-d, --delay=SEC`       | Seconds between updates                        |
| `-n, --iterations=N`    | Run N iterations then exit                     |
| `-p, --pid=N`           | Monitor specific PIDs                          |
| `-u, --user=USER`       | Show only USER processes                       |
| `-o, --order-field=FIELD`| Sort by FIELD                                 |
| `-c, --cmd-line-toggle` | Toggle full command line                       |
| `-H, --thread`          | Show threads                                   |
| `-S, --cumulative`      | Toggle cumulative time                         |
| `--help`                | Display help and exit                          |
| `--version`             | Display version and exit                       |

## Interactive Keys

| Key     | Action                    |
| ------- | ------------------------- |
| q Esc   | Quit                      |
| Space   | Refresh screen            |
| h       | Toggle help display       |
| P       | Sort by CPU               |
| M       | Sort by memory            |
| T       | Sort by time              |
| 1       | Toggle single CPU view    |
| k       | Kill a process            |
| r       | Renice a process          |

## Platform Sources

| Platform | Source                                      |
| -------- | ------------------------------------------- |
| Linux    | /proc/[pid]/stat, /proc/[pid]/status        |
| Windows  | CreateToolhelp32Snapshot + GetProcessTimes  |
| macOS    | sysctl(KERN_PROC) + proc_pidinfo            |

## Examples
 ``ntop                          # interactive process viewer
top -b -n 1                  # one-shot batch output
top -d 2                     # update every 2 seconds
top -p 1234,5678             # monitor specific PIDs
`` 

## Build
 ``ngcc -O2 -std=c99 -Wall -o top top.c
`` 
