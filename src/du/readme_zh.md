# du

估算文件空间使用量。

## 概要
`
du [选项]... [文件]...
du [选项]... -d 最大深度
`

## 选项

| 选项                        | 说明                                       |
| --------------------------- | ------------------------------------------ |
| -a, --all                 | 列出所有文件，不仅是目录                   |
| --apparent-size           | 打印表观大小                               |
| -B, --block-size=大小     | 按大小缩放                                 |
| -b, --bytes               | 等同于 --apparent-size --block-size=1     |
| -c, --total               | 显示总计                                   |
| -d, --max-depth=N         | 打印 N 级或更少深度的总计                  |
| -h, --human-readable      | 人类可读格式                               |
| -k                        | 等同于 --block-size=1K                   |
| -m                        | 等同于 --block-size=1M                   |
| -s, --summarize           | 仅显示每个参数的总计                       |
| -S, --separate-dirs       | 不包含子目录大小                           |
| -x, --one-file-system     | 跳过不同文件系统上的目录                   |
| ` --exclude=PATTERN`      | 排除匹配的文件                             |
| ` --help`                 | 显示帮助并退出                             |
| ` --version`              | 显示版本并退出                             |

## 示例
`
du -sh *                     # 摘要，人类可读
du -h --max-depth=1          # 当前目录明细
du -sh project/              # 目录总大小
du -sh --exclude='*.o' src/  # 排除目标文件
`

## 编译
`
gcc -O2 -std=c99 -Wall -o du du.c
`
