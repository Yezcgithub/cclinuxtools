# base32

Encode/decode data using RFC 4648 Base32 (A-Z2-7) or Base32hex (0-9A-V).

## Synopsis
`
base32 [OPTION]... [FILE]...
`

## Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| -d, --decode            | Decode data                                    |
| -i, --ignore-garbage    | Ignore non-alphabet characters when decoding   |
| -w, --wrap=COLS         | Wrap after COLS chars (default 76, 0=no wrap)  |
| ` --base32hex`           | Use base32hex alphabet (RFC 4648 Section 7)    |
| ` --help`                | Display help and exit                          |
| ` --version`             | Display version and exit                       |

## Examples
`
echo "Hello" | base32            # JBSWY3DP
echo "JBSWY3DP" | base32 -d      # Hello
echo "Hello" | base32 --base32hex # 91IMOR3F
`

## Build
`
gcc -O2 -std=c99 -Wall -o base32 base32.c
`
