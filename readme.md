# Cross-platform Linux Toolset

## 1. Introduction

Thanks for checking out this project — hopefully it proves useful.

Every tool here is fully rewritten from scratch and fairly feature-complete, backed by extensive test files and scripts.

- **Single-file implementation**: every tool depends only on the standard C library. Copy a single `.c` file and compile — no dependencies to install.
- **Pure C99**: follows ISO C99, with POSIX features declared explicitly through feature-test macros for maximum portability.
- **Cross-platform abstraction**: per-source platform detection macros (Windows / Linux / macOS / *BSD) call the correct native API on each system.
- **POSIX-style paths**: forward-slash output everywhere, UTF-8 encoding; Windows uses wide-character APIs for non-ANSI paths.
- **Automated testing**: the build scripts embed 20~30+ regression tests covering major CLI options and edge cases.
- **Performance first**: calls system APIs directly, avoids unnecessary abstraction layers; regex compile caching, dynamic strings, and other optimizations.
- **MIT License**: free to use in commercial products and embedded projects, no GPL contamination.

## 2. Build

One-click scripts at the repo root auto-discover every `.c` file under `src/` and compile each one into `build/`:

| Platform             | Script         | Notes                        |
| -------------------- | -------------- | ---------------------------- |
| Linux / macOS / *BSD | `./build.sh`   | auto-detects gcc / cc / clang |
| Windows              | `build.bat`    | requires MinGW-w64 etc.      |

Run with no arguments to compile every tool:

```bash
./build.sh     # compile everything
build.bat      # same on Windows
```

Output layout:

```
build/
├── bash          # main shell (also produced as an sh alias)
└── cmdtools/
    ├── test      # also produced as a [ alias
    ├── ls
    └── ...
```

### Options

| Option                  | Description                                 | Example                                     |
| ----------------------- | ------------------------------------------- | ------------------------------------------- |
| `-s, --specify <tools>` | build only the given tools (comma- or space-separated) | `./build.sh -s bash,cat,ls`      |
| `-cc <toolchain>`       | use a specific compiler name or a cross-compiler path | `./build.sh -cc arm-linux-gnueabihf-gcc` |
| `-m32`                  | build 32-bit programs                       | `./build.sh -m32`                           |
| `-v, --version`         | print version                               | `./build.sh -v`                             |
| `-h, --help`            | print help                                  | `./build.sh -h`                             |

- Without `-cc`, the script probes `gcc` → `cc` → `clang` on the system; if none is found it prints a message and exits.
- Tools named in `--specify` that do not exist produce an `Unknown tool` warning and are skipped, without affecting the others.
- Common standard libraries are already part of the link line (`-lm`, `-lrt`, `-lpthread`, Windows `psapi/advapi32/ws2_32`, ...), so no manual `-l...` flags are needed — even for cross builds.

### Single-file build

Every tool is a single file and can also be compiled standalone:

```bash
gcc -O2 -std=c99 -Wall -Wextra -o ls ls.c -lm
```

No CMake, no autoconf, no pkg-config, no third-party libraries. The resulting binary has no dynamic dependencies (pure static link against the standard C library).

### Per-tool build scripts

Each `src/<tool>/` directory ships its own `build.sh` / `build.bat`; running it compiles the tool and runs the full regression test suite:

```bash
cd src/awk && ./build.sh
```

## 3. Use Cases

| Scenario               | Description                                                |
| ---------------------- | ---------------------------------------------------------- |
| Embedded Linux         | supports cross-compilation; small binary size; suits systems with incomplete toolkits |
| Old systems            | great for older Linux / macOS systems with incomplete tools |
| Minimal Docker images  | replace busyless-laden build stages and shrink images      |
| Recovery environments  | common commands available on Live CD / PE without install  |
| Windows development    | compile native Windows binaries; Unix-like CLI without Cygwin/MSYS2 |
| Teaching / learning    | single-file implementation, clear code, ideal for studying systems programming |
| Cross-platform projects| ship a built-in toolset that does not depend on system commands |

## 4. Comparison

### vs System Built-ins

| Item          | This project                       | GNU Coreutils / Built-in     |
| ------------- | ---------------------------------- | ---------------------------- |
| Platforms     | Windows / Linux / macOS / *BSD     | Unix-like only (Cygwin on Windows) |
| Dependencies  | zero, single-file                  | needs a full runtime        |
| Size          | 20KB~200KB per tool                | Coreutils bundle ~1MB+       |
| Build         | `gcc -o ls ls.c` in one line       | autotools / configure / make |
| Windows       | native Win32 API                   | needs MSYS2/Cygwin layer     |
| Customizing   | single-file source, easy to trim   | requires complex build system |

### vs BusyBox

| Item          | This project          | BusyBox                     |
| ------------- | --------------------- | --------------------------- |
| Language      | pure C99              | C (heavy GNU extensions)    |
| Build        | single-file compile    | full Kbuild system          |
| Behavior      | POSIX + common extras  | trimmed (differs from GNU)  |
| Windows       | native                | not supported               |
| License       | MIT                   | GPL v2                      |

### vs Other single-file implementations

| Item          | This project                    | sbase / heirloom             |
| ------------- | ------------------------------- | ---------------------------- |
| Platforms     | Windows + Linux + Unix          | Unix-like only               |
| Native Windows| yes (Win32 API)                 | no                           |
| Build & test  | built-in automated tests        | manual verification          |
| Command set   | aims at a complete Linux toolkit| ~20 basic commands           |

## 5. Supported Platforms

| Platform   | Compiler                | Feature macro                 |
| ---------- | ----------------------- | ----------------------------- |
| Windows    | GCC (MinGW / MSYS2 / TDM) | —                           |
| Linux      | GCC                     | `_POSIX_C_SOURCE=200809L`     |
| macOS      | GCC / Clang             | `_DARWIN_C_SOURCE`            |
| FreeBSD    | Clang (`cc`)            | —                             |
| OpenBSD    | Clang (`cc`)            | —                             |
| NetBSD     | Clang (`cc`)            | `_NETBSD_SOURCE`              |
| Other Unix | any C99 compiler        | —                             |

## 6. License

The MIT License (MIT)

https://mit-license.org/

Copyright © 2026 <Yezc/cclinuxtools>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.