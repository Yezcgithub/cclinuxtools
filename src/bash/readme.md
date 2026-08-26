# bash

Bash-style shell implementation in portable C99.

## Synopsis

``bash
bash [OPTION]... [SCRIPT [ARG...] ]
bash -c STRING
bash -s
`` 

## Options

| Option        | Description                                     |
| ------------- | ----------------------------------------------- |
| ` -c STRING`   | Execute STRING as commands                      |
| ` -s`          | Read commands from standard input               |
| ` --login`     | Start as a login shell                          |
| ` -i`          | Interactive mode                                 |
| ` SCRIPT`      | Execute script file with arguments              |
| ` --help`      | Display help and exit                           |
| ` --version`   | Display version and exit                        |

## Features

- Simple commands, pipelines, and lists
- Redirections: redirections and heredocs
- 30+ builtins: cd, pwd, echo, printf, export, source, alias, read, test, type, command, declare, local, jobs, bg, fg, kill, pushd, popd, mktemp, which, etc.
- Parameter expansion and command substitution
- Arithmetic expansion and special parameters: ` True, , $!, $#, $@, $* ` 
- Quoting: single, double, escape
- Globbing: * ? [abc]
- Control flow: if/else, for, while, until, case, select, functions with local variables

## Examples

``bash
bash -c 'echo Hello World'
bash script.sh arg1 arg2
bash -s < commands.txt
`` 

## Build

``bash
gcc -O2 -std=c99 -Wall -o bash bash.c
`` 
