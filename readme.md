# cclinuxtools

**A collection of common Linux command-line tools reimplemented in portable C99.**

Each tool is a self-contained single-file program with zero external dependencies, compilable and runnable on Windows, Linux, macOS, FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.

---

## Directory Structure

```
cclinuxtools/
├── awk/       # AWK interpreter
├── bash/      # Bash-style shell
├── cd/        # Change directory
├── cp/        # Copy files/directories
├── echo/      # Print text
├── find/      # Search for files
├── ls/        # List directory contents
├── mkdir/     # Create directories
├── mv/        # Move/rename
├── pwd/       # Print working directory
├── rm/        # Remove files/directories
├── sed/       # Stream editor
├── LICENSE
└── README.md
```

Each tool directory contains:
- `*.c` — Tool source code (single-file implementation)
- `build.sh` — Build script for Unix/Linux/macOS/BSD (with automated tests)
- `build.bat` — Build script for Windows (with automated tests)

---

## Supported Platforms

| Platform     | Compiler                   | Feature Macro                  |
| ------------ | -------------------------- | ------------------------------ |
| Windows      | GCC (MinGW / MSYS2)        | —                              |
| Linux        | GCC                        | `_POSIX_C_SOURCE=200809L`      |
| macOS        | GCC / Clang                | `_DARWIN_C_SOURCE`             |
| FreeBSD      | Clang (`cc`)               | —                              |
| OpenBSD      | Clang (`cc`)               | —                              |
| NetBSD       | Clang (`cc`)               | `_NETBSD_SOURCE`               |
| Other Unix   | Any C99 compiler           | —                              |

---

## Tool Overview

### Filesystem Operations

| Tool     | Key Features                                                                                                          |
| -------- | --------------------------------------------------------------------------------------------------------------------- |
| **ls**   | Long format `-l`, color output `--color`, recursive `-R`, multiple sort modes, human-readable sizes `-h`, inode `-i`, file type indicators `-F/-p` |
| **find** | Tests: `-name/-iname/-path/-regex/-type/-size/-mtime`, etc. Actions: `-print/-print0/-ls/-delete/-exec/-ok`, AND/OR/NOT expressions |
| **cp**   | Recursive `-r`, preserve attributes `-p`, archive `-a`, interactive `-i`, no-clobber `-n`, update `-u`, symbolic link `-s`, hard link `-l` |
| **mv**   | Force `-f`, interactive `-i`, no-clobber `-n`, update `-u`, backup `-b`, recursive directory move                     |
| **rm**   | Force `-f`, interactive `-i/-I`, recursive `-r`, remove empty dirs `-d`, root protection `--preserve-root`           |
| **mkdir**| Set permissions `-m`, create parents `-p`, verbose `-v`                                                              |
| **cd**   | Logical path `-L`, physical path `-P`, `cd -` back, `~` expansion, `CDPATH` search                                   |
| **pwd**  | Logical path `-L`, physical path `-P`, wide-character path support on Windows                                         |
| **echo** | No trailing newline `-n`, escape interpretation `-e/-E`, POSIXLY_CORRECT compatibility mode                         |

### Text Processing

| Tool     | Key Features                                                                                                          |
| -------- | --------------------------------------------------------------------------------------------------------------------- |
| **awk**  | Pattern-action rules, fields `$1..$NF`, associative arrays, regex, built-in functions (sub/gsub/match/split/substr/printf, etc.), control flow, user-defined functions |
| **sed**  | Addresses (line numbers/regex/ranges/first~step/+N), `s///` substitution (g/p/i/Nth), full command set (a/i/c/d/D/p/P/n/N/h/H/g/G/x/b/t/T/q/Q/r/w/l/=/y/{}, etc.), built-in tiny NFA regex engine (Windows), in-place editing `-i` |

### Shell Environment

| Tool     | Key Features                                                                                                          |
| -------- | --------------------------------------------------------------------------------------------------------------------- |
| **bash** | Bash-style shell: interactive/script (`-c`/file) modes, pipelines/redirections/heredocs, 40+ builtins, variable/parameter/command/arithmetic expansion, globbing, control flow (if/for/while/until/case/select/functions), local variables |

---

## Quick Start

### Option 1: Use the Build Scripts (Recommended)

Each tool directory provides an automated build script with platform detection, compilation, and built-in tests.

**Unix/Linux/macOS/BSD:**
```bash
cd ls
chmod +x build.sh
./build.sh
```

**Windows (MinGW/MSYS2):**
```cmd
cd ls
build.bat
```

The build script will automatically:
1. Clean previous build artifacts
2. Detect the target platform and set the correct compile flags
3. Compile and produce the executable
4. Run 20+ automated functional tests

### Option 2: Manual Compilation

Using GCC as an example (equivalent commands for each platform are in the header comments of each source file):

**Windows:**
```cmd
gcc -O2 -std=c99 -Wall -o ls.exe ls/ls.c
```

**Linux:**
```bash
gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o ls ls/ls.c
```

**macOS:**
```bash
gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o ls ls/ls.c
```

**FreeBSD / OpenBSD:**
```bash
cc -O2 -std=c99 -Wall -o ls ls/ls.c
```

**NetBSD:**
```bash
cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o ls ls/ls.c
```

---

## Usage Examples

```bash
# ls - List current directory in long format, sorted by time (newest first), human-readable sizes
./ls -lht

# find - Find .c files modified within the last 7 days under src/ and delete them
./find src -name "*.c" -mtime -7 -delete

# awk - Count the number of users per shell in /etc/passwd
./awk -F: '{cnt[$7]++} END {for (s in cnt) print s, cnt[s]}' /etc/passwd

# sed - Replace all occurrences of foo with bar in-place, backing up as .bak
./sed -i.bak 's/foo/bar/g' config.txt

# cp - Archive-copy a directory, preserving all attributes
./cp -a project/ /backup/project_2025/

# bash - Execute an inline script
./bash -c 'for i in 1 2 3; do echo "num=$i"; done'
```

---

## Design Highlights

- **Single-file implementation**: Each tool depends only on the standard C library; copy a single `.c` file and compile — no dependencies to install.
- **Pure C99**: Follows ISO C99; POSIX features are explicitly declared via feature macros for maximum portability.
- **Cross-platform abstraction**: Each source file begins with platform detection macros (Windows / Linux / macOS / *BSD) and calls the correct system APIs (e.g., `FindFirstFile` on Windows, `opendir/lstat` on POSIX).
- **POSIX-style paths**: All platforms output forward-slash paths in UTF-8; the Windows port uses wide-character APIs to support non-ANSI paths.
- **Doxygen comments**: Every source file header includes a full feature description, build commands, and copyright notice, following the LVGL code style.
- **Automated testing**: Build scripts include extensive regression tests covering major CLI options and edge cases.
- **Performance-first**: Direct system API calls avoid unnecessary abstraction layers; optimizations include regex compilation caching and dynamic strings.

---

## License

MIT License — see [LICENSE](LICENSE)

Copyright © 2025-2026 Yezc / cclinuxtools
