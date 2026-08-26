# date

Display or set the system date and time.

## Synopsis
`
date [OPTION]... [+FORMAT]
date -s STRING
`

## Options

| Option                        | Description                                    |
| ----------------------------- | ---------------------------------------------- |
| -u, --utc, --universal      | Use UTC                                         |
| -d, --date=STRING           | Parse STRING as date                            |
| -I[FMT], --iso-8601[=FMT]  | ISO 8601 format (date/hours/minutes/seconds/ns) |
| -R, --rfc-email             | RFC 5322 format                                 |
| ` --rfc-3339=FMT`           | RFC 3339 format (date/seconds/ns)               |
| -r, --reference=FILE        | Use file's modification time                    |
| -s, --set=STRING            | Set system time                                 |
| ` --help`                   | Display help and exit                           |
| ` --version`                | Display version and exit                        |

## Format Specifiers

| Spec | Description              | Spec | Description              |
| ---- | ------------------------ | ---- | ------------------------ |
| %Y  | Year (YYYY)             | %m  | Month (01-12)            |
| %d  | Day (01-31)             | %H  | Hour (00-23)             |
| %M  | Minute (00-59)          | %S  | Second (00-60)           |
| %s  | Seconds since epoch     | %A  | Weekday name             |
| %B  | Month name              | %F  | YYYY-MM-DD               |
| %T  | HH:MM:SS                | %R  | HH:MM                    |

## Examples
`
date                          # Wed Jan 15 14:30:00 UTC 2025
date +%Y-%m-%d                # 2025-01-15
date +%s                     # 1736961000
date -d @1736961000          # convert epoch to date
date -R                      # RFC 5322 format
`

## Build
`
gcc -O2 -std=c99 -Wall -o date date.c
`
