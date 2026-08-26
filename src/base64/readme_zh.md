# base64

使用 RFC 4648 Base64（A-Za-z0-9+/）编码/解码数据。

## 概要
`
base64 [选项]... [文件]...
`

## 选项

| 选项                      | 说明                                       |
| ------------------------- | ------------------------------------------ |
| -d, --decode            | 解码数据                                   |
| -i, --ignore-garbage    | 解码时忽略非字母字符                       |
| -w, --wrap=列数         | 在指定列数后换行（默认 76，0=不换行）     |
| ` --help`                | 显示帮助并退出                             |
| ` --version`             | 显示版本并退出                             |

## 示例
`
echo "Hello" | base64           # SGVsbG8=
echo "SGVsbG8=" | base64 -d    # Hello
base64 -d encoded.txt > raw.bin
`

## 编译
`
gcc -O2 -std=c99 -Wall -o base64 base64.c
`
