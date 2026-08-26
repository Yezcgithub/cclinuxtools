# dirname

跨平台 `dirname` 的实现 — 从文件名中去掉最后一个组件。

## 概要

```
dirname NAME
dirname [选项] NAME...
```

## 说明

打印 `NAME` 的目录部分。如果 `NAME` 没有 `/`，输出 `.`。如果全是斜杠，输出 `/`。

## 选项

| 选项        | 说明                               |
| ----------- | ---------------------------------- |
| `-z, --zero`| 用 NUL 分隔输出，而非换行          |
| `--help`    | 显示帮助并退出                     |
| `--version` | 显示版本并退出                     |

## 示例

```bash
dirname /usr/local/bin/ls    # /usr/local/bin
dirname file.txt             # .
dirname //a/b//              # /a
```

## 编译

```bash
gcc -O2 -std=c99 -Wall -o dirname dirname.c
```