# uname

Print system information.

## Synopsis
 `
uname [OPTION]...
` 

## Options

| Option                  | Description                                    |
| ----------------------- | ---------------------------------------------- |
| -a, --all             | Print all info                                 |
| -s, --kernel-name     | Kernel name default                            |
| -n, --nodename        | Network node hostname                          |
| -r, --kernel-release  | Kernel release string                          |
| -v, --kernel-version  | Kernel version string                          |
| -m, --machine         | Machine hardware name                          |
| -p, --processor       | Processor type                                 |
| -i, --hardware-platform | Hardware platform                            |
| -o, --operating-system| Operating system name                          |
| --help                | Display help and exit                          |
| --version             | Display version and exit                       |

## Examples
 `
uname                        # Linux or Windows or Darwin
uname -a                     # all info
uname -s -r                  # kernel name + release
uname -m                     # x86_64 or AMD64 or ARM64
` 

## Build
 `
gcc -O2 -std=c99 -Wall -o uname uname.c
` 
