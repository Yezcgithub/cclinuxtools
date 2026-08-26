# whoami

打印有效用户名。

## 概要
 `
whoami
` 

## 选项

| 选项        | 说明                         |
| ----------- | ---------------------------- |
| --help    | 显示帮助并退出               |
| --version | 显示版本并退出               |

## 说明

- POSIX 使用 geteuid + getpwuid。
- Windows 使用 GetUserNameW 转换为 UTF-8。
- 等同于 id -un。

## 示例
 `
whoami                        # 例如 root 或 administrator
` 

## 编译
 `
gcc -O2 -std=c99 -Wall -o whoami whoami.c
` 
