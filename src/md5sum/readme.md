# md5sum

Compute and check MD5 message digests (coreutils compatible).

## Synopsis
`
md5sum [OPTION]... [FILE]...
md5sum -c [OPTION]... [FILE]...
`

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| -b, --binary              | Read in binary mode                            |
| -c, --check               | Read checksums from FILE and verify            |
| ` --tag`                   | Create BSD-style checksum output               |
| -t, --text                | Read in text mode (default)                    |
| -z, --zero                | End each output line with NUL                  |
| ` --ignore-missing`        | Don't fail for missing files (with --check)    |
| ` --quiet`                 | Don't print OK for verified files              |
| ` --status`                | Don't output; use exit code only               |
| ` --strict`                | Exit non-zero for malformed lines              |
| -w, --warn                | Warn about improperly formatted lines          |
| ` --help`                  | Display help and exit                          |
| ` --version`               | Display version and exit                       |

## Examples
`
md5sum file.txt
md5sum file1.txt file2.txt
md5sum -c checksums.md5        # verify checksums
echo "hello" | md5sum          # read from stdin
`

## Build
`
gcc -O2 -std=c99 -Wall -o md5sum md5sum.c
`
