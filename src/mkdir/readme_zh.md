# mkdir

跨平台 `mkdir` — 创建目录。

## 概要

```
mkdir [选项]... 目录...
```

## 选项

| 选项                     | 说明                                       |
| ------------------------ | ------------------------------------------ |
| `-m, --mode=模式`        | 设置文件权限模式（八进制或符号）           |
| `-p, --parents`          | 递归创建父目录；已存在时不报错             |
| `-v, --verbose`          | 为每个创建的目录打印消息                   |
| `--help`                 | 显示帮助并退出                             |
| `--version`              | 显示版本并退出                             |

## 示例

```bash
mkdir newdir
mkdir -p a/b/c/d
mkdir -m 755 public
mkdir -pv /tmp/{a,b,c}
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o mkdir mkdir.c
```