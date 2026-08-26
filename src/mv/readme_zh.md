# mv

跨平台 `mv` — 移动（重命名）文件和目录。

## 概要

```
mv [选项]... 源文件 目标文件
mv [选项]... 源文件... 目录
```

## 选项

| 选项         | 说明                                   |
| ------------ | -------------------------------------- |
| `-f, --force`| 强制覆盖，不提示                       |
| `-i, --interactive` | 覆盖前提示                      |
| `-n, --no-clobber` | 不覆盖已有文件                   |
| `-u, --update` | 仅在源文件更新时移动                 |
| `-v, --verbose` | 显示操作过程                         |
| `-b, --backup` | 为每个目标文件创建备份               |
| `--help`     | 显示帮助并退出                        |
| `--version`  | 显示版本并退出                        |

## 示例

```bash
mv old.txt new.txt
mv -i file.txt /backup/
mv -f *.log /tmp/
mv -v src/ dst/
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o mv mv.c
```