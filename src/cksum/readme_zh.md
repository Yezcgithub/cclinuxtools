# cksum

计算并校验校验和。

## 概要
 ``ncksum [选项]... [文件]...
`` 

## 通过 -a 指定算法

| 算法        | 说明                                          |
| ----------- | --------------------------------------------- |
| `crc`       | 默认 POSIX CRC-32（以太网多项式）            |
| `crc32b`    | 标准 CRC-32（zlib/PNG）                       |
| `sysv`      | System V 校验和（sum -s）                     |
| `bsd`       | BSD 校验和（sum -r）                          |
| `md5`       | MD5 哈希                                      |
| `sha1`      | SHA-1 哈希                                    |
| `sha256`    | SHA-256 哈希                                  |
| `blake2b`   | BLAKE2b（默认 512 位，-l 指定其他长度）      |

## 选项

| 选项                    | 说明                                       |
| ----------------------- | ------------------------------------------ |
| `-a, --algorithm=算法`| 使用指定算法                              |
| `-l, --length=位数`   | blake2b 摘要长度                          |
| `--tag`               | 创建 BSD 风格输出                         |
| `-z, --zero`          | 以 NUL 结尾                               |
| `--help`              | 显示帮助并退出                            |
| `--version`           | 显示版本并退出                            |

## 示例
 ``ncksum file.txt                       # 默认 CRC-32
cksum -a sha256 file.txt             # SHA-256
cksum -a md5 file.txt                # MD5
`` 

## 编译
 ``ngcc -O2 -std=c99 -Wall -o cksum cksum.c
`` 
