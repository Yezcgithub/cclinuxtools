# hostname

显示或设置系统主机名。

## 概要
 `
hostname [选项]... [名称]
` 

## 选项

| 选项                     | 说明                                     |
| ------------------------ | ---------------------------------------- |
| 无参数                   | 打印当前主机名                           |
| 名称                     | 设置主机名 需要权限                     |
| -s, --short            | 短主机名 在第一个点处截断               |
| -d, --domain           | DNS 域名                                 |
| -f, --fqdn, --long     | 完全限定域名                             |
| -i, --ip-address       | 主机名的 IP 地址                         |
| -I, --all-ip-addresses | 主机的所有 IP 地址                       |
| -a, --alias            | 别名                                     |
| -F, --file 文件        | 从文件读取主机名                         |
| --help                 | 显示帮助并退出                           |
| --version              | 显示版本并退出                           |

## 示例
 `
hostname                     # 打印主机名
hostname -s                  # 短名称
hostname -f                  # FQDN
hostname -i                  # IP 地址
` 

## 编译
 `
gcc -O2 -std=c99 -Wall -o hostname hostname.c
` 
