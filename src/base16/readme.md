# base16

Encode/decode data using RFC 4648 Base16 (hexadecimal: 0-9A-F).

## Synopsis
`
base16 [OPTION]... [FILE]...
`

## Options

| Option                    | Description                                    |
| ------------------------- | ---------------------------------------------- |
| -d, --decode            | Decode data                                    |
| -i, --ignore-garbage    | Ignore non-alphabet characters when decoding   |
| -w, --wrap=COLS         | Wrap after COLS chars (default 76, 0=no wrap)  |
| ` --help`                | Display help and exit                          |
| ` --version`             | Display version and exit                       |

## Notes

- Encoding always outputs uppercase hex digits.
- Decoding accepts both upper and lowercase.

## Examples
`
echo "Hello" | base16           # 48656C6C6F
echo "48656C6C6F" | base16 -d   # Hello
`

## Build
`
gcc -O2 -std=c99 -Wall -o base16 base16.c
`
