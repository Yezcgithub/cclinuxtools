# bash

可移植 C99 实现的 Bash 风格 Shell。

## 概要

``bash
bash [选项]... [脚本 [参数...] ]
bash -c 字符串
bash -s
`` 

## 选项

| 选项          | 说明                                        |
| ------------- | ------------------------------------------- |
| ` -c 字符串`   | 将字符串作为命令执行                        |
| ` -s`          | 从标准输入读取命令                          |
| ` --login`     | 以登录 Shell 启动                           |
| ` -i`          | 交互模式                                    |
| ` 脚本`        | 执行脚本文件及其参数                        |
| ` --help`      | 显示帮助并退出                              |
| ` --version`   | 显示版本并退出                              |

## 主要特性

- 简单命令、管道和列表
- 重定向：输入输出重定向和 here 文档
- 30+ 内置命令：cd, pwd, echo, printf, export, source, alias, read, test, type, command, declare, local, jobs, bg, fg, kill, pushd, popd, mktemp, which 等
- 参数展开和命令替换
- 算术展开和特殊参数
- 通配符：* ? [abc]
- 控制流：if/else、for、while、until、case、select、支持局部变量的函数

## 示例

``bash
bash -c 'echo Hello World'
bash script.sh arg1 arg2
bash -s < commands.txt
`` 

## 编译

``bash
gcc -O2 -std=c99 -Wall -o bash bash.c
`` 
