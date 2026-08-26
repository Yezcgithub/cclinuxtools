# hostname

Show or set the system host name.

## Synopsis
 `
hostname [OPTION]... [NAME]
` 

## Options

| Option                   | Description                                  |
| ------------------------ | -------------------------------------------- |
| no args                  | Print current host name                      |
| NAME                     | Set host name requires privileges            |
| -s, --short            | Short host name cut at first dot             |
| -d, --domain           | DNS domain name                              |
| -f, --fqdn, --long     | Fully qualified domain name                  |
| -i, --ip-address       | IP address for the host name                 |
| -I, --all-ip-addresses | All IP addresses of the host                 |
| -a, --alias            | Alias names                                  |
| -F, --file FILE        | Read host name from FILE                     |
| --help                 | Display help and exit                        |
| --version              | Display version and exit                     |

## Examples
 `
hostname                     # print hostname
hostname -s                  # short name
hostname -f                  # FQDN
hostname -i                  # IP addresses
` 

## Build
 `
gcc -O2 -std=c99 -Wall -o hostname hostname.c
` 
