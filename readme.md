# A Cross-Platform Linux Tool Collection

Glad you found this open-source project. Hope it helps you.


## 1. Introduction

- **Single-file implementation**: Each tool depends only on the standard C library. Copy a single `.c` file and compile it — no dependencies to install.
- **Pure C99**: Fully compliant with ISO C99. POSIX features are declared via standard feature macros, ensuring maximum portability.
- **Cross-platform abstraction**: Each source file has built-in platform detection at the top, calling the correct native API for each OS.
- **POSIX-style paths**: All platforms output forward-slash paths with UTF-8 encoding. Windows uses wide-character APIs for non-ANSI path support.
- **Automated testing**: Build scripts include extensive regression tests covering major CLI options and edge cases.
- **Performance first**: Direct system API calls with no unnecessary abstraction layers; regex compile caching, dynamic strings, and other optimizations.
- **MIT License**: Free to use in commercial and embedded products with no GPL contamination.


## 2. Use Cases

| Scenario          | Description                                                     |
| ----------------- | --------------------------------------------------------------- |
| Embedded Linux    | Small binaries (20-200KB), ideal for OpenWrt / Buildroot / Yocto |
| Docker minimal image | Replace busybox in alpine, reduce image size                  |
| Recovery environment | Use common commands in Live CD / PE without installation      |
| Windows development | Unix-like CLI experience on Windows, no Cygwin/MSYS2 needed  |
| Education         | Single-file implementation, clean code, great for learning systems programming and CLI development |
| Cross-platform projects | Built-in toolset that doesn't depend on system-provided commands |

```bash
gcc -O2 -std=c99 -Wall -o ls ls.c     # One file, one command
```

No CMake, no autoconf, no pkg-config, no third-party libraries needed. The compiled binary has no dynamic library dependencies (statically linked against the standard C library only).

### 1. Pure C99 Standard

Fully compliant with ISO C99 with no compiler extensions. System calls (file operations, terminal control, etc.) are declared via standard POSIX feature macros:

```c
#define _POSIX_C_SOURCE 200809L   // Linux / other Unix
#define _DARWIN_C_SOURCE          // macOS
#define _NETBSD_SOURCE            // NetBSD
```

This means the code compiles on any C99-conforming compiler — GCC, Clang, MSVC (with C99 mode), TCC, Open Watcom, etc.

### 2. Truly Cross-Platform

Each source file has built-in platform detection, calling the correct native API for each OS:

| Platform | File Operation API                     | Terminal API       |
| -------- | -------------------------------------- | ------------------ |
| Windows  | `FindFirstFile` / `GetFileAttributes`  | Win32 Console API  |
| Linux    | `opendir` / `lstat` / `d_type`        | `termios`          |
| macOS    | `opendir` / `lstat` / `dirent`        | `termios`          |
| *BSD     | `opendir` / `lstat`                   | `termios`          |

No Cygwin/MSYS2 POSIX simulation — native API calls on each platform for better performance and compatibility.

### 3. Small Footprint, Ideal for Embedded

Each tool compiles to just **20KB~200KB**, making it perfect for:

- Embedded Linux boards (OpenWrt, Buildroot)
- Docker minimal images (scratch / alpine)
- Recovery environments / Live CD
- Restricted servers where installing software is inconvenient

### 4. Build Scripts Double as Tests

Each tool's `build.sh` / `build.bat` is not just a build script — it includes 20-30+ automated regression tests. Run the build script = compile + test in one step:

```bash
./build.sh     # Compile + run all tests
build.bat      # Same on Windows
```


## 3. Comparison with Alternatives

### vs System Built-in Tools

| Aspect          | This Project                      | GNU Coreutils / System Built-in    |
| --------------- | --------------------------------- | ---------------------------------- |
| Cross-platform  | Windows / Linux / macOS / *BSD    | Unix-like only (Windows needs Cygwin) |
| Dependencies    | Zero, single file                 | Full runtime environment required  |
| Size            | Single tool 20KB~200KB            | Coreutils bundle ~1MB+             |
| Build           | `gcc -o ls ls.c` one line         | autotools / configure / make       |
| Windows support | Native Win32 API                  | Requires MSYS2/Cygwin emulation    |
| Customizability | Single source file, easy to trim  | Must understand complex build system |

### vs BusyBox

| Aspect          | This Project            | BusyBox                        |
| --------------- | ----------------------- | ------------------------------ |
| Implementation  | Pure C99                | C (with GNU extensions)        |
| Build complexity| Single file compile     | Full build system (Kbuild)     |
| Command style   | POSIX standard + extensions | Simplified (some GNU differences) |
| Windows support | Native                  | None                           |
| License         | MIT                     | GPL v2                         |
| Use case        | Dev tools / desktop     | Embedded Linux / Alpine minimal|

### vs Other Single-File Implementations

| Aspect          | This Project                              | sbase (suckless) / heirloom |
| --------------- | ----------------------------------------- | --------------------------- |
| Platform support| Windows + Unix full platform              | Unix-like only              |
| Windows native  | Yes (Win32 API)                           | No                          |
| Build testing   | Built-in automated tests                  | Manual verification needed  |
| Command coverage| ls/cp/mv/rm/find/sed/awk/bash etc. 12+    | ~20 basic commands          |
| Documentation   | English + Chinese                         | English only                |


## 4. Supported Platforms

| Platform   | Compiler                  | Feature Macro              |
| ---------- | ------------------------- | -------------------------- |
| Windows    | GCC (MinGW / MSYS2 / TDM) | —                          |
| Linux      | GCC                       | `_POSIX_C_SOURCE=200809L`  |
| macOS      | GCC / Clang               | `_DARWIN_C_SOURCE`         |
| FreeBSD    | Clang (`cc`)              | —                          |
| OpenBSD    | Clang (`cc`)              | —                          |
| NetBSD     | Clang (`cc`)              | `_NETBSD_SOURCE`           |
| Other Unix | Any C99 compiler          | —                          |


## 5. License

The MIT License (MIT)

https://mit-license.org/

Copyright © 2026 <Yezc/cclinuxtools>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.