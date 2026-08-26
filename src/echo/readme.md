# echo

Cross-platform `echo` — display a line of text.

## Synopsis

```
echo [OPTION]... [STRING]...
```

## Options

| Option      | Description                                    |
| ----------- | ---------------------------------------------- |
| `-n`        | Do not output trailing newline                 |
| `-e`        | Enable backslash escape interpretation         |
| `-E`        | Disable backslash escape interpretation (default) |
| `--help`    | Display help and exit (only as sole argument)  |
| `--version` | Display version and exit (only as sole argument)|

## Escape Sequences (`-e`)

| Sequence | Description        |
| -------- | ------------------ |
| `\a`     | Alert (bell)       |
| `\b`     | Backspace          |
| `\e`     | Escape             |
| `\f`     | Form feed          |
| `\n`     | Newline            |
| `\r`     | Carriage return    |
| `\t`     | Horizontal tab     |
| `\v`     | Vertical tab       |
| `\\`     | Backslash          |
| `\0NNN`  | Octal byte         |
| `\xHH`   | Hex byte           |
| `\uHHHH` | Unicode (16-bit)   |
| `\UHHHHHHHH` | Unicode (32-bit) |

## Notes

- POSIXLY_CORRECT: enables escape interpretation by default, disables option parsing.

## Examples

```bash
echo "Hello, World!"
echo -n "no newline"
echo -e "line1\nline2"
echo -e "\x48\x65\x6c\x6c\x6f"   # Hello
```

## Build

```bash
gcc -O2 -std=c99 -Wall -o echo echo.c
```