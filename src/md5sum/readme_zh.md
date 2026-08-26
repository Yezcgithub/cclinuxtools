# md5sum

计算并校验 MD5 消息摘要（兼容 coreutils）。

## 概要
`
md5sum [选项]... [文件]...
md5sum -c [选项]... [文件]...
`

## 选项

| 选项                        | 说明                                       |
| --------------------------- | ------------------------------------------ |
| -b, --binary              | 以二进制模式读取                           |
| -c, --check               | 从文件读取校验和并验证                     |
| ` --tag`                   | 创建 BSD 风格校验和输出                    |
| -t, --text                | 以文本模式读取（默认）                     |
| -z, --zero                | 每行以 NUL 结尾                           |
| ` --ignore-missing`        | 不因缺失文件而失败（配合 --check）         |
| ` --quiet`                 | 不打印验证成功的 OK                        |
| ` --status`                | 不输出；仅用退出码                         |
| ` --strict`                | 格式错误的校验和行时非零退出               |
| -w, --warn                | 对格式不正确的行发出警告                   |
| ` --help`                  | 显示帮助并退出                             |
| ` --version`               | 显示版本并退出                             |

## 示例
`
md5sum file.txt
md5sum file1.txt file2.txt
md5sum -c checksums.md5        # 验证校验和
echo "hello" | md5sum          # 从标准输入读取
`

## 编译
`
gcc -O2 -std=c99 -Wall -o md5sum md5sum.c
`
