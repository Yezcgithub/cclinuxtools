# touch

跨平台 `touch` — 更改文件时间戳或创建空文件。

## 概要

```
touch [选项]... 文件...
```

## 选项

| 选项                       | 说明                                       |
| -------------------------- | ------------------------------------------ |
| `-a`                       | 仅更改访问时间                             |
| `-m`                       | 仅更改修改时间                             |
| `-c, --no-create`          | 不创建任何文件                             |
| `-d, --date=字符串`        | 将字符串解析为日期，替代当前时间           |
| `-r, --reference=文件`     | 使用该文件的时间戳                         |
| `-t 时间戳`                | 使用 `[[CC]YY]MMDDhhmm[.ss]` 格式         |
| `-h, --no-dereference`     | 影响符号链接本身，而非目标                 |
| `--time=WORD`              | 更改指定的时间（access/modify）            |
| `--help`                   | 显示帮助并退出                             |
| `--version`                | 显示版本并退出                             |

## 示例

```bash
touch newfile                # 创建空文件
touch -r ref.txt target.txt  # 复制时间戳
touch -d "2025-01-01" f.txt  # 设置特定日期
touch *.log                  # 更新时间戳
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o touch touch.c
```