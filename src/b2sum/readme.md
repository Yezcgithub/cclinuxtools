# b2sum

Compute BLAKE2b checksums (default 512 bits) per RFC 7693.

## Synopsis
`
b2sum [OPTION]... [FILE]...
b2sum -c [OPTION]... [FILE]...
`

## Options

| Option                      | Description                                    |
| --------------------------- | ---------------------------------------------- |
| -l, --length=BITS         | Digest length in bits (0-512, multiple of 8)   |
| -b, --binary              | Read in binary mode                            |
| -c, --check               | Verify checksums from FILE                     |
| ` --tag`                   | Create BSD-style output                        |
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
b2sum file.txt
b2sum -l 256 file.txt          # 256-bit digest
b2sum -c checksums.b2
`

## Build
`
gcc -O2 -std=c99 -Wall -o b2sum b2sum.c
`
