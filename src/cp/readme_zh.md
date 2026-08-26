# cp

跨平台 `cp` 命令 — 复制文件和目录。

## 概要

```
cp [选项]... 源文件 目标文件
cp [选项]... 源文件... 目录
```

## 选项

| 选项                          | 说明                                     |
| ----------------------------- | ---------------------------------------- |
| `-a, --archive`               | 归档模式（`-r -p -d`），保留所有属性     |
| `-f, --force`                 | 强制覆盖，不提示                         |
| `-i, --interactive`           | 覆盖前提示                               |
| `-n, --no-clobber`            | 不覆盖已有文件                           |
| `-p, --preserve`              | 保留模式、时间戳、所有者、组             |
| `-r, -R, --recursive`         | 递归复制目录                             |
| `-s, --symbolic-link`         | 创建符号链接而非复制                     |
| `-l, --link`                  | 创建硬链接而非复制                       |
| `-u, --update`                | 仅在源文件更新时复制                     |
| `-v, --verbose`               | 显示操作过程                             |
| `-L, --dereference`           | 始终跟随 SOURCE 中的符号链接             |
| `-P, --no-dereference`        | 不跟随符号链接                           |
| `-t, --target-directory=DIR`  | 将所有源文件复制到 DIR                   |
| `-T, --no-target-directory`   | 将 DEST 视为普通文件                     |
| `--help`                      | 显示帮助并退出                           |
| `--version`                   | 显示版本并退出                           |

## 示例

```bash
cp file.txt backup.txt
cp -r project/ backup/project/
cp -a src/ /backup/src/
cp -i *.txt /backup/           # 覆盖前提示
cp -u config.yml /etc/         # 仅在更新时复制
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o cp cp.c
```