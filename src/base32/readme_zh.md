# base32

使用 RFC 4648 Base32（A-Z2-7）或 Base32hex（0-9A-V）编码/解码数据。

## 概要
`
base32 [选项]... [文件]...
`

## 选项

| 选项                      | 说明                                       |
| ------------------------- | ------------------------------------------ |
| -d, --decode            | 解码数据                                   |
| -i, --ignore-garbage    | 解码时忽略非字母字符                       |
| -w, --wrap=列数         | 在指定列数后换行（默认 76，0=不换行）     |
| ` --base32hex`           | 使用 Base32hex 字母表（RFC 4648 第 7 节）  |
| ` --help`                | 显示帮助并退出                             |
| ` --version`             | 显示版本并退出                             |

## 示例
`
echo "Hello" | base32            # JBSWY3DP
echo "JBSWY3DP" | base32 -d      # Hello
echo "Hello" | base32 --base32hex # 91IMOR3F
`

## 编译
`
gcc -O2 -std=c99 -Wall -o base32 base32.c
`
