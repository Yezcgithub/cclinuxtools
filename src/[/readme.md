# [

Check file types and compare values.

## Synopsis
 `
test EXPRESSION
[ EXPRESSION ]
` 

## File Tests

| Test    | Description                                       |
| ------- | ------------------------------------------------- |
| -e FILE| FILE exists                                      |
| -f FILE| FILE is regular file                             |
| -d FILE| FILE is directory                                |
| -r FILE| FILE is readable                                |
| -w FILE| FILE is writable                                |
| -x FILE| FILE is executable                              |
| -s FILE| FILE exists and has size greater than zero       |
| -L FILE| FILE is symbolic link                           |
| -S FILE| FILE is socket                                  |
| -b FILE| FILE is block special                           |
| -c FILE| FILE is character special                        |
| -p FILE| FILE is named pipe                              |
| -u FILE| FILE has setuid bit set                         |
| -g FILE| FILE has setgid bit set                         |
| -k FILE| FILE has sticky bit set                         |

## String Tests

| Test         | Description                                     |
| ------------ | ----------------------------------------------- |
| -z STRING   | STRING is empty                                |
| -n STRING   | STRING is non-empty                            |
| STRING = STRING2 | Strings are equal                        |
| STRING != STRING2 | Strings are not equal                  |

## Integer Tests

| Test            | Description                                  |
| --------------- | -------------------------------------------- |
| INT1 -eq INT2 | Equal                                        |
| INT1 -ne INT2 | Not equal                                    |
| INT1 -lt INT2 | Less than                                    |
| INT1 -le INT2 | Less than or equal                           |
| INT1 -gt INT2 | Greater than                                 |
| INT1 -ge INT2 | Greater than or equal                        |

## Logical Operators

| Operator   | Description                                      |
| ---------- | ------------------------------------------------ |
| ! EXPR   | Negate                                           |
| EXPR -a EXPR | AND                                        |
| EXPR -o EXPR | OR                                         |
| ( EXPR ) | Grouping                                        |

## Exit Codes

| Code | Meaning  |
| ---- | -------- |
| 0    | True     |
| 1    | False    |
| 2    | Error    |

## Examples
 `
test -f file.txt && echo exists
test -z '`' && echo empty
[ 5 -gt 3 ] && echo yes
` 

## Build
 `
gcc -O2 -std=c99 -Wall -o test test.c
` 
