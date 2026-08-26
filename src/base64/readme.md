# base64

Encode/decode data using RFC 4648 Base64 (A-Za-z0-9+/).

## Synopsis
`
base64 [OPTION]... [FILE]...
`

## Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| -d, --decode            | Decode data                                    |
| -i, --ignore-garbage    | Ignore non-alphabet characters when decoding   |
| -w, --wrap=COLS         | Wrap after COLS chars (default 76, 0=no wrap)  |
| ` --help`                | Display help and exit                          |
| ` --version`             | Display version and exit                       |

## Examples
`
echo "Hello" | base64           # SGVsbG8=
echo "SGVsbG8=" | base64 -d    # Hello
base64 -d encoded.txt > raw.bin
`

## Build
`
gcc -O2 -std=c99 -Wall -o base64 base64.c
`
