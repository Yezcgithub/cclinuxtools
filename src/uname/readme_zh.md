# uname

打印系统信息。

## 概要
 `
uname [选项]...
` 

## 选项

| 选项                    | 说明                                       |
| ----------------------- | ------------------------------------------ |
| -a, --all             | 打印所有信息                               |
| -s, --kernel-name     | 内核名称 默认                              |
| -n, --nodename        | 网络节点主机名                             |
| -r, --kernel-release  | 内核发行版字符串                           |
| -v, --kernel-version  | 内核版本字符串                             |
| -m, --machine         | 机器硬件名称                               |
| -p, --processor       | 处理器类型                                 |
| -i, --hardware-platform | 硬件平台                                 |
| -o, --operating-system| 操作系统名称                               |
| --help                | 显示帮助并退出                             |
| --version             | 显示版本并退出                             |

## 示例
 `
uname                        # Linux 或 Windows 或 Darwin
uname -a                     # 所有信息
uname -s -r                  # 内核名 + 发行版
uname -m                     # x86_64 或 AMD64 或 ARM64
` 

## 编译
 `
gcc -O2 -std=c99 -Wall -o uname uname.c
` 
