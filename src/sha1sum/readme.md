# sha1sum

Compute and check SHA-1 message digests (coreutils compatible).

## Synopsis
`
sha1sum [OPTION]... [FILE]...
`

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| -b, --binary              | Read in binary mode                            |
| -c, --check               | Read checksums from FILE and verify            |
| ` --tag`                   | Create BSD-style checksum output               |
| -t, --text                | Read in text mode (default)                    |
| -z, --zero                | End each output line with NUL                  |
| ` --ignore-missing`        | Don't fail for missing files                   |
| ` --quiet`                 | Don't print OK for verified files              |
| ` --status`                | Don't output; use exit code only               |
| ` --strict`                | Exit non-zero for malformed lines              |
| -w, --warn                | Warn about improperly formatted lines          |
| ` --help`                  | Display help and exit                          |
| ` --version`               | Display version and exit                       |

## Examples
`
sha1sum file.txt
sha1sum -c checksums.sha1
`

## Build
`
gcc -O2 -std=c99 -Wall -o sha1sum sha1sum.c
`
