# cat

Cross-platform `cat` — concatenate and print files.

## Synopsis

```
cat [OPTION]... [FILE]...
```

## Options

| Option                      | Description                                         |
| --------------------------- | --------------------------------------------------- |
| `-A, --show-all`            | Equivalent to `-vET`                                |
| `-b, --number-nonblank`     | Number non-empty output lines                       |
| `-e`                        | Equivalent to `-vE`                                 |
| `-E, --show-ends`           | Display `$` at end of each line                     |
| `-n, --number`              | Number all output lines                             |
| `-s, --squeeze-blank`       | Suppress repeated empty output lines                |
| `-t`                        | Equivalent to `-vT`                                 |
| `-T, --show-tabs`           | Display TAB characters as `^I`                      |
| `-v, --show-nonprinting`    | Use `^` and `M-` notation for non-printing chars    |
| `--help`                    | Display help and exit                               |
| `--version`                 | Display version and exit                            |

## Notes

- Reads from stdin when no FILE is given or FILE is `-`.
- Binary-safe I/O; UTF-8 pass-through.
- Windows wide-character path support.

## Examples

```bash
cat file.txt
cat -n file.txt               # numbered lines
cat -A file.txt               # show all special chars
cat file1 file2 > merged.txt
echo "hello" | cat            # read from stdin
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o cat cat.c
```