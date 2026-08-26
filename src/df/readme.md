# df

Report file system disk space usage.

## Synopsis
`
df [OPTION]... [FILE]...
`

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| -a, --all                 | Include pseudo/duplicate/inaccessible systems  |
| -B, --block-size=SIZE     | Scale sizes by SIZE                            |
| -h, --human-readable      | Sizes in powers of 1024                        |
| -H, --si                  | Sizes in powers of 1000                        |
| -i, --inodes              | List inode info instead of blocks               |
| -k                        | Like --block-size=1K                         |
| -l, --local               | Limit to local file systems                    |
| ` --total`                | Produce grand total                            |
| -t, --type=TYPE           | Limit to TYPE                                  |
| -T, --print-type          | Print file system type                         |
| -x, --exclude-type=TYPE   | Exclude TYPE                                   |
| -P, --portability         | POSIX output format                            |
| ` --help`                 | Display help and exit                          |
| ` --version`              | Display version and exit                       |

## Examples
`
df -h                        # human-readable sizes
df -h /home                  # for specific mount point
df -T                        # show filesystem type
df -i                        # inode usage
`

## Build
`
gcc -O2 -std=c99 -Wall -o df df.c
`
