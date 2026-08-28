# id

跨平台 `id` — 打印用户和组信息。

## 概要

```
id [选项]... [用户名]
```

## 选项

| 选项              | 说明                                         |
| ----------------- | -------------------------------------------- |
| `-u, --user`      | 仅打印有效用户 ID                            |
| `-g, --group`     | 仅打印有效组 ID                              |
| `-G, --groups`    | 打印所有组 ID                                |
| `-n, --name`      | 打印名称而非数字（配合 -u、-g、-G）          |
| `-r, --real`      | 打印真实 ID 而非有效 ID                      |
| `-z, --zero`      | 用 NUL 而非空白分隔条目                      |
| `-a`              | 忽略（为兼容性保留）                         |
| `--help`          | 显示帮助并退出                               |
| `--version`       | 显示版本并退出                               |

## 说明

无选项时，以默认格式打印完整身份信息：

```
uid=1000(user) gid=1000(user) groups=1000(user),4(adm),27(sudo)
```

当指定可选的用户名时，打印该用户的信息而非当前用户。

## 示例

```bash
id                              # uid=1000(user) gid=1000(user) groups=...
id -u                           # 1000（有效用户 ID）
id -g                           # 1000（有效组 ID）
id -G                           # 1000 4 27（所有组 ID）
id -un                          # user（有效用户名）
id -Gn                          # user adm sudo（所有组名）
id -u root                      # 0（指定其他用户）
```

## 编译

```bash
# Windows
gcc -O2 -std=c99 -Wall -o id.exe id.c

# Linux
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o id id.c

# macOS
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o id id.c
```
