# whoami

Print the effective user name.

## Synopsis
 `
whoami
` 

## Options

| Option      | Description                   |
| ----------- | ----------------------------- |
| --help    | Display help and exit         |
| --version | Display version and exit      |

## Notes

- POSIX uses geteuid + getpwuid.
- Windows uses GetUserNameW converted to UTF-8.
- Equivalent to id -un.

## Examples
 `
whoami                        # e.g. root or administrator
` 

## Build
 `
gcc -O2 -std=c99 -Wall -o whoami whoami.c
` 
