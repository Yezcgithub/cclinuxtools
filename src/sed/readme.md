# sed

Cross-platform `sed` — stream editor for filtering and transforming text.

## Synopsis

```
sed [OPTION]... SCRIPT [FILE]...
sed [OPTION]... -e SCRIPT [FILE]...
sed [OPTION]... -f SCRIPT-FILE [FILE]...
```

## Options

| Option              | Description                                    |
| ------------------- | ---------------------------------------------- |
| `-e SCRIPT`         | Add script to commands                         |
| `-f SCRIPT-FILE`    | Add script-file contents to commands           |
| `-n, --quiet`       | Suppress automatic printing                    |
| `-r, -E`            | Use extended regular expressions               |
| `-i[SUFFIX]`        | Edit files in place (with optional backup)     |
| `-s`                | Treat files as separate streams                |
| `--help`            | Display help and exit                          |
| `--version`         | Display version and exit                       |

## Commands

| Command | Description                                    |
| ------- | ---------------------------------------------- |
| `s/REGEX/REPLACEMENT/FLAGS` | Substitute (flags: g, p, i, Nth)   |
| `a TEXT`                | Append text after line                    |
| `i TEXT`                | Insert text before line                   |
| `c TEXT`                | Replace line with text                    |
| `d`                    | Delete line                                |
| `D`                    | Delete up to first newline                 |
| `p`                    | Print line                                 |
| `P`                    | Print up to first newline                  |
| `n`                    | Read next line into pattern space          |
| `N`                    | Append next line to pattern space          |
| `h`                    | Hold pattern space to hold space           |
| `H`                    | Append pattern space to hold space         |
| `g`                    | Hold space to pattern space                |
| `G`                    | Append hold space to pattern space         |
| `x`                    | Swap pattern and hold spaces               |
| `b LABEL`              | Branch to LABEL                            |
| `t LABEL`              | Branch if substitution succeeded           |
| `T LABEL`              | Branch if substitution failed              |
| `q [EXIT]`             | Quit                                        |
| `r FILE`               | Read file contents                         |
| `w FILE`               | Write pattern space to file                |
| `l`                    | Print non-printing characters              |
| `=`                    | Print current line number                  |
| `y/SRC/DST/`           | Transliterate characters                   |
| `: LABEL`              | Define label                               |
| `{ COMMANDS }`         | Group commands                             |

## Examples

```bash
sed 's/old/new/g' file.txt
sed -i.bak 's/foo/bar/g' config.yml
sed -n '5,10p' file.txt           # print lines 5-10
sed '/^#/d' config.txt            # remove comments
sed '1d' file.txt                 # remove first line
sed -E 's/[0-9]+/NUM/g' file.txt
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o sed sed.c
```