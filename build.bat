@echo off
::============================
:: -Project Information
::============================
:: @file build.bat
:: @General build script file
:: @Coding format UTF-8
:: @Description : Cross-platform build script for cclinuxtools
:: Auto-discovers every .c file under src\ (and its subdirectories) and
:: compiles each one into an executable under build\, following the
:: current build layout. You can freely add or remove .c files and src
:: subdirectories; they are picked up automatically on the next run.

::============================
:: -License
::============================
:: https://mit-license.org/
:: The MIT License (MIT)
:: Copyright © 2025-2026 <Yezc/cclinuxtools>
:: Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
:: and associated documentation files (the “Software”), to deal in the Software without restriction, 
:: including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
:: and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
:: subject to the following conditions:
:: 
:: The above copyright notice and this permission notice shall be included in all copies or 
:: substantial portions of the Software.
:: 
:: THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
:: BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
:: NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
:: DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
:: OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

setlocal EnableDelayedExpansion

::----------------------------
:: - Build Configuration
::----------------------------
::  Maps any .c file under src\ to an executable under build\:
::    src\bash\bash.c   -> build\bash.exe, build\sh.exe
::    src\test\test.c   -> build\cmdtools\test.exe, build\cmdtools\[.exe
::    src\<tool>\<t>.c  -> build\cmdtools\<t>.exe
::  Programs are built as release versions (no debug information).
set "PROJECT_NAME=cclinuxtools"
set "PROJECT_VERSION=V1.0.0"
set "CC="
set "CFLAGS=-O2 -std=c99 -Wall -Wextra"
set "LINK_LIBS=-lpsapi -ladvapi32 -lws2_32 -lnetapi32 -lm -lgdi32 -luser32 -lshell32 -lole32"
set "M32_FLAG="
set "SPECIFY="

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "SRC_DIR=%SCRIPT_DIR%\src"
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "CMDTOOLS_DIR=%BUILD_DIR%\cmdtools"

set "PASS_COUNT=0"
set "FAIL_COUNT=0"

::----------------------------
:: - command line argument processing
::----------------------------
:parse_args
if "%~1"=="" goto :parse_done
if /i "%~1"=="-cc" goto :handle_cc
if /i "%~1"=="-m32" goto :handle_m32
if /i "%~1"=="-s" goto :handle_specify
if /i "%~1"=="--specify" goto :handle_specify
if /i "%~1"=="-v" goto :print_version
if /i "%~1"=="--version" goto :print_version
if /i "%~1"=="-h" goto :print_help
if /i "%~1"=="--help" goto :print_help
echo [ERROR] Unknown option: %~1 1>&2
call :help
exit /b 1

:handle_cc
shift
if "%~1"=="" (
    echo [ERROR] -cc requires a compiler name or path. 1>&2
    exit /b 1
)
set "CC=%~1"
shift
goto :parse_args

:handle_m32
set "M32_FLAG=-m32"
shift
goto :parse_args

:handle_specify
shift
goto :collect_specify

:collect_specify
if "%~1"=="" goto :specify_done
set "_chk=%~1"
if "!_chk:~0,1!"=="-" goto :specify_done
if not "%SPECIFY%"=="" set "SPECIFY=%SPECIFY%,%~1"
if "%SPECIFY%"=="" set "SPECIFY=%~1"
shift
goto :collect_specify

:specify_done
if not "%SPECIFY%"=="" goto :parse_args
echo [ERROR] -s/--specify requires tool name(s), e.g. bash,cat,ls. 1>&2
exit /b 1

:print_version
call :version
exit /b 0

:print_help
call :help
exit /b 0

:parse_done
call :all

if %FAIL_COUNT% gtr 0 exit /b 1
exit /b 0

::----------------------------
:: - Target: all (compile every tool)
::----------------------------
:all
call :detect_cc
if %ERRORLEVEL% neq 0 exit /b 1
if not exist "%BUILD_DIR%"    mkdir "%BUILD_DIR%"
if not exist "%CMDTOOLS_DIR%" mkdir "%CMDTOOLS_DIR%"

echo(
echo   Compiler: %CC%
echo   Flags   : %CFLAGS% %M32_FLAG%
echo   Libs    : %LINK_LIBS%
echo(

if not "%SPECIFY%"=="" (
    for %%d in (%SPECIFY%) do (
        set "_found=0"
        for /f "delims=" %%D in ('dir /b /ad "%SRC_DIR%" 2^>nul') do (
            if /i "%%D"=="%%d" set "_found=1"
        )
        if "!_found!"=="0" echo [WARN] Unknown tool, not found in src\: %%d
    )
)

set "PASS_COUNT=0"
set "FAIL_COUNT=0"

for /r "%SRC_DIR%" %%f in (*.c) do (
    set "FULL_PATH=%%f"
    set "REL_PATH=!FULL_PATH:%SRC_DIR%\=!"
    for /f "tokens=1 delims=\" %%d in ("!REL_PATH!") do set "TOOL_DIR=%%d"
    for %%n in ("%%f") do set "BASE_NAME=%%~nn"
    call :handle "!FULL_PATH!" "!TOOL_DIR!" "!BASE_NAME!"
)

echo(
echo   Success: %PASS_COUNT%   Failed: %FAIL_COUNT%
goto :eof

::----------------------------
:: - Target: detect_cc (find a usable compiler)
::----------------------------
:detect_cc
if not "%CC%"=="" goto :eof
where gcc >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set "CC=gcc"
    goto :eof
)
where clang >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set "CC=clang"
    goto :eof
)
where cc >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set "CC=cc"
    goto :eof
)
echo [ERROR] No C compiler found (gcc/clang/cc). 1>&2
echo [ERROR] Install a compiler or pass "-cc <toolchain>". 1>&2
exit /b 1

::----------------------------
:: - Target: clean (remove build outputs)
::----------------------------
:clean
echo(
echo   Clean: %BUILD_DIR%
if exist "%BUILD_DIR%\bash.exe" ( del /q "%BUILD_DIR%\bash.exe" & echo   Remove bash.exe )
if exist "%BUILD_DIR%\sh.exe"    ( del /q "%BUILD_DIR%\sh.exe"    & echo   Remove sh.exe )
if exist "%CMDTOOLS_DIR%"        ( rmdir /s /q "%CMDTOOLS_DIR%"    & echo   Remove cmdtools\ )
echo   done
goto :eof

::----------------------------
:: - Target: start_main (run the built shell)
::----------------------------
:start_main
if not exist "%BUILD_DIR%\bash.exe" (
    echo [ERROR] build\bash.exe not found, run "build.bat all" first.
    exit /b 1
)
"%BUILD_DIR%\bash.exe"
goto :eof

::----------------------------
:: - Target: version
::----------------------------
:version
echo %PROJECT_NAME% %PROJECT_VERSION% (MIT License)
goto :eof

::----------------------------
:: - Target: infoprint (print configuration info)
::----------------------------
:infoprint
echo(
echo =============================================
echo   %PROJECT_NAME% build script
echo   A cross-platform Linux tool collection
echo =============================================
echo   Version : %PROJECT_VERSION%
echo   Platform: Windows
echo   Script  : build.bat
echo   Source  : %SRC_DIR%
echo   Output  : %BUILD_DIR%
echo(
goto :eof

::----------------------------
:: - Target: help
::----------------------------
:help
echo Usage: build.bat [options]
echo Options:
echo   (none)         compile all tools into build\
echo   -cc ^<cc^>       use a specific compiler name or cross toolchain path
echo   -m32           build 32-bit programs
echo   -s ^<tools^>     compile only the given tools, comma-separated (e.g. bash,cat,ls)
echo   --specify ^<tools^>  same as -s
echo   -v, --version   print version
echo   -h, --help      print this help
goto :eof

::----------------------------
:: - Intermediate stages (make-based workflow only)
::----------------------------
:stage_notice
echo [INFO] '%1' is an intermediate stage used by the make-based workflow.
echo [INFO] This script builds finished executables directly; use "build.bat all".
goto :eof

::----------------------------
:: - Decide where one .c file goes, then compile it
::----------------------------
:handle
set "_src=%~1"
set "_tool=%~2"
set "_base=%~3"

if not "%SPECIFY%"=="" (
    set "SPECIFIED=0"
    for %%t in (%SPECIFY%) do (
        if /i "%%t"=="!_tool!" set "SPECIFIED=1"
    )
    if "!SPECIFIED!"=="0" goto :eof
)

if "%_tool%"=="sh"  goto :eof
if "%_tool%"=="vi"  goto :eof
if "%_tool%"=="vim" goto :eof

if "%_tool%"=="bash" (
    call :compile "!_src!" "%BUILD_DIR%\bash.exe"
    call :compile "!_src!" "%BUILD_DIR%\sh.exe"
    goto :eof
)

if "%_tool%"=="test" (
    call :compile "!_src!" "%CMDTOOLS_DIR%\test.exe"
    if exist "%CMDTOOLS_DIR%\test.exe" (
        copy /y "%CMDTOOLS_DIR%\test.exe" "%CMDTOOLS_DIR%\[.exe" >nul
        echo   COPY [.exe  from  test.exe
    )
    goto :eof
)

call :compile "!_src!" "%CMDTOOLS_DIR%\!_base!.exe"
goto :eof

::----------------------------
:: - Compile one source file (with library retry)
::----------------------------
:compile
set "_src=%~1"
set "_out=%~2"
echo   CC %~nx2

REM Link with common Windows system libraries (must follow the source file)
"%CC%" %CFLAGS% %M32_FLAG% -o "%_out%" "%_src%" %LINK_LIBS% >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set /a PASS_COUNT+=1
    goto :eof
)

REM Link failed - retry without extra libs as last resort
"%CC%" %CFLAGS% %M32_FLAG% -o "%_out%" "%_src%" >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set /a PASS_COUNT+=1
) else (
    echo   [FAILED] %~nx2
    set /a FAIL_COUNT+=1
)
goto :eof
