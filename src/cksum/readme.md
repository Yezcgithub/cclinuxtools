# cksum

Compute and verify checksums.

## Synopsis
 ``ncksum [OPTION]... [FILE]...
`` 

## Algorithms via -a

| Algorithm   | Description                                     |
| ----------- | ----------------------------------------------- |
| `crc`       | Default POSIX CRC-32 Ethernet polynomial       |
| `crc32b`    | Standard CRC-32 zlib/PNG                        |
| `sysv`      | System V checksum sum -s                        |
| `bsd`       | BSD checksum sum -r                             |
| `md5`       | MD5 hash                                        |
| `sha1`      | SHA-1 hash                                      |
| `sha256`    | SHA-256 hash                                    |
| `blake2b`   | BLAKE2b default 512 bits use -l for other sizes|

## Options

| Option                  | Description                                    |
| ----------------------- | ---------------------------------------------- |
| `-a, --algorithm=ALGO`| Use specified algorithm                        |
| `-l, --length=BITS`   | Digest length for blake2b                      |
| `--tag`               | Create BSD-style output                        |
| `-z, --zero`          | End lines with NUL                             |
| `--help`              | Display help and exit                          |
| `--version`           | Display version and exit                       |

## Examples
 ``ncksum file.txt                       # default CRC-32
cksum -a sha256 file.txt             # SHA-256
cksum -a md5 file.txt                # MD5
`` 

## Build
 ``ngcc -O2 -std=c99 -Wall -o cksum cksum.c
`` 
