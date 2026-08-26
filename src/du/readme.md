# du

Estimate file space usage.

## Synopsis
`
du [OPTION]... [FILE]...
du [OPTION]... -d MAX_DEPTH
`

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| -a, --all                 | Write counts for all files, not just directories|
| --apparent-size           | Print apparent sizes                            |
| -B, --block-size=SIZE     | Scale sizes by SIZE                            |
| -b, --bytes               | Equivalent to --apparent-size --block-size=1  |
| -c, --total               | Produce grand total                            |
| -d, --max-depth=N         | Print total for dirs N or fewer levels deep    |
| -h, --human-readable      | Human readable format                          |
| -k                        | Like --block-size=1K                         |
| -m                        | Like --block-size=1M                         |
| -s, --summarize           | Display only total for each argument           |
| -S, --separate-dirs       | Don't include subdirectory sizes               |
| -x, --one-file-system     | Skip dirs on different file systems            |
| ` --exclude=PATTERN`      | Exclude files matching PATTERN                 |
| ` --help`                 | Display help and exit                          |
| ` --version`              | Display version and exit                       |

## Examples
`
du -sh *                     # summary, human-readable
du -h --max-depth=1          # current dir breakdown
du -sh project/              # total size of directory
du -sh --exclude='*.o' src/  # exclude object files
`

## Build
`
gcc -O2 -std=c99 -Wall -o du du.c
`
