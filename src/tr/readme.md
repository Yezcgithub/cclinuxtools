# tr

Cross-platform `tr` — translate or delete characters.

## Synopsis

```
tr OPTION... SET1 [SET2]
```

## Options

| Option                  | Description                                      |
| ----------------------- | ------------------------------------------------ |
| `-c, -C, --complement`  | Use the complement of SET1                       |
| `-d, --delete`          | Delete characters in SET1                        |
| `-s, --squeeze-repeats` | Replace each sequence of a repeated character with a single occurrence |
| `-t, --truncate-set1`   | Truncate SET1 to length of SET2                  |
| `--help`                | Display help and exit                            |
| `--version`             | Display version and exit                         |

## SET Specification

| Spec         | Description                                      |
| ------------ | ------------------------------------------------ |
| `\NNN`       | Character with octal value NNN                   |
| `\\`         | Backslash                                        |
| `\a`         | Audible BEL                                      |
| `\b`         | Backspace                                        |
| `\f`         | Form feed                                        |
| `\n`         | Newline                                          |
| `\r`         | Carriage return                                  |
| `\t`         | Horizontal tab                                   |
| `\v`         | Vertical tab                                     |
| `CHAR1-CHAR2`| All characters from CHAR1 to CHAR2               |
| `[CHAR*]`    | In SET2, copies of CHAR until length of SET1     |
| `[CHAR*REPEAT]` | REPEAT copies of CHAR                          |
| `[:alnum:]`  | All letters and digits                           |
| `[:alpha:]`  | All letters                                      |
| `[:blank:]`  | All horizontal whitespace                        |
| `[:digit:]`  | All digits                                       |
| `[:lower:]`  | All lower case letters                           |
| `[:upper:]`  | All upper case letters                           |
| `[:print:]`  | All printable characters                         |
| `[:punct:]`  | All punctuation characters                       |
| `[:space:]`  | All horizontal or vertical whitespace            |
| `[=CHAR=]`   | All characters equivalent to CHAR                |

## Examples

```bash
echo "hello" | tr 'a-z' 'A-Z'           # HELLO
echo "hello" | tr -d 'l'                # heo
echo "aaabbbccc" | tr -s 'a-c'          # abc
echo "abc123" | tr -cd '0-9\n'          # 123
echo "one  two   three" | tr -s ' '     # one two three
tr '(' ')' < file.txt                    # replace ( with )
```

## Build

```bash
# Windows
gcc -O2 -std=c99 -Wall -o tr.exe tr.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o tr tr.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o tr tr.c
```
