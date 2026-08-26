# basenc

Encode/decode data using various RFC 4648 encodings.

## Synopsis
`
basenc [OPTION]... [FILE]...
`

## Encodings

| Option           | Alphabet / Description                |
| ---------------- | ------------------------------------- |
| ` --base64`     | RFC 4648 Section 4 (A-Za-z0-9+/)     |
| ` --base64url`  | RFC 4648 Section 5 (A-Za-z0-9-_)     |
| ` --base32`     | RFC 4648 Section 6 (A-Z2-7)          |
| ` --base32hex`  | RFC 4648 Section 7 (0-9A-V)          |
| ` --base16`     | RFC 4648 Section 8 (0-9A-F)          |
| ` --base2msbf`  | Bit string, MSB first                |
| ` --base2lsbf`  | Bit string, LSB first                |
| ` --z85`        | ZeroMQ Z85 (input multiple of 4 bytes) |

## General Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| -d, --decode            | Decode data                                    |
| -i, --ignore-garbage    | Ignore non-alphabet characters when decoding   |
| -w, --wrap=COLS         | Wrap after COLS chars (default 76, 0=no wrap)  |
| ` --help`                | Display help and exit                          |
| ` --version`             | Display version and exit                       |

## Build
`
gcc -O2 -std=c99 -Wall -o basenc basenc.c
`
