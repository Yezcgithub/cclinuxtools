# awk

跨平台 AWK 解释器 — AWK 的主要子集。

## 概要

```
awk [选项]... '程序' [文件]...
awk [选项]... -f 程序文件 [文件]...
```

## 选项

| 选项            | 说明                               |
| --------------- | ---------------------------------- |
| `-F fs`         | 设置字段分隔符                     |
| `-f 程序文件`   | 从文件读取程序                     |
| `-v 变量=值`    | 执行前赋值变量                     |
| `--help`        | 显示帮助并退出                     |
| `--version`     | 显示版本并退出                     |

## 主要特性

- **模式-动作规则**，带 BEGIN/END 块
- **字段**：`$0`（整行）、`$1..$NF`（各字段）
- **关联数组**：`数组[键]`
- **内置变量**：`NR`、`NF`、`FS`、`OFS`、`RS`、`FILENAME`、`ENVIRON`
- **内置函数**：`print`、`printf`、`sprintf`、`length`、`sub`、`gsub`、`match`、`split`、`index`、`substr`、`tolower`、`toupper`、`system`、`getline`
- **控制流**：`if/else`、`while`、`do-while`、`for`、`for-in`、`switch`
- **用户自定义函数**
- **正则表达式**：`~`、`!~`、`/正则/`

## 示例

```bash
awk '{print $1}' file.txt                    # 打印第一个字段
awk -F: '{print $1, $3}' /etc/passwd         # 用户名:UID
awk '{sum += $1} END {print sum}' nums.txt   # 求和
awk '/error/ {count++} END {print count}' log.txt
awk 'BEGIN {OFS=","} {print $1,$2}' data.txt
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o awk awk.c
```