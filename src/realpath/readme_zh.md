# realpath

跨平台 `realpath` — 打印解析后的绝对路径名。

## 概要

```
realpath [选项]... 文件...
```

## 选项

| 选项                         | 说明                                         |
| ---------------------------- | -------------------------------------------- |
| `-E, --canonicalize`         | 除最后一个组件外都必须存在（默认）           |
| `-e, --canonicalize-existing`| 路径的所有组件必须存在                       |
| `-m, --canonicalize-missing` | 路径组件无需存在或为目录                     |
| `-L, --logical`              | 在解析符号链接前解析 `..` 组件               |
| `-P, --physical`             | 遇到符号链接时立即解析（默认）               |
| `-q, --quiet`                | 抑制大多数错误信息                           |
| `-s, --strip, --no-symlinks` | 不展开符号链接                               |
| `-z, --zero`                 | 每行以 NUL 结尾，而非换行                    |
| `--relative-to=目录`         | 打印相对于指定目录的解析路径                 |
| `--relative-base=目录`       | 除非在指定目录下，否则打印绝对路径           |
| `--help`                     | 显示帮助并退出                               |
| `--version`                  | 显示版本并退出                               |

## 说明

- 解析 `.`、`..` 和多余的斜杠。
- POSIX 上通过 `realpath(3)` 解析符号链接。
- Windows 上通过 `GetFinalPathNameByHandleA` 解析。
- 所有平台输出 POSIX 风格的正斜杠路径。

## 示例

```bash
realpath file.txt               # /home/user/project/file.txt
realpath ../foo                 # /home/user/foo
realpath -m /a/b/../../../c    # /c
realpath --relative-to=/a/b /a/b/c/d  # c/d
realpath -q nonexistent.txt     # 抑制错误
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o realpath.exe realpath.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o realpath realpath.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o realpath realpath.c
```
