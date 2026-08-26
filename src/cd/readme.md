# cd

Cross-platform cd -- change the working directory.

## Synopsis
 ``ncd [OPTION]... [DIR]
`` 

## Options

| Option             | Description                                    |
| ------------------ | ---------------------------------------------- |
| `-L, --logical`    | Use PWD when traversing dotdot (default)     |
| `-P, --physical`   | Resolve symlinks in the final path             |

## Behavior

- cd with no argument goes to HOME.
- cd dash returns to OLDPWD and prints it.
- Tilde expansion supported.
- CDPATH search supported.

## Examples
 ``ncd /usr/local
cd -                # return to previous directory
cd ~                # go to home directory
cd ../src           # relative path
`` 

## Build
 ``ngcc -O2 -std=c99 -Wall -o cd cd.c
`` 
