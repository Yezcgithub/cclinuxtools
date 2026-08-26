# sha256sum

Compute and check SHA-256 message digests (coreutils compatible).

## Synopsis
`
sha256sum [OPTION]... [FILE]...
sha256sum -c [OPTION]... [FILE]...
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
sha256sum file.txt
sha256sum -c checksums.sha256
echo "data" | sha256sum
`

## Build
`
gcc -O2 -std=c99 -Wall -o sha256sum sha256sum.c
`
