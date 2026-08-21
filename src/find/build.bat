@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     find.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=find.exe
set SOURCE=find.c

echo.
echo [1/3] Cleaning previous build...
if exist "%OUTPUT%" (
    del "%OUTPUT%"
    echo   Removed %OUTPUT%
)

echo.
echo [2/3] Compiling %SOURCE%...
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE%
echo.
%CC% %CFLAGS% -o %OUTPUT% %SOURCE%

if %ERRORLEVEL% equ 0 (
    echo.
    echo [3/3] Build succeeded!
    echo   Output: %CD%\%OUTPUT%
    echo.
    echo ============================================
    echo   Testing basic functionality...
    echo ============================================
    echo.
    %OUTPUT% --version
    echo.
    %OUTPUT% . -maxdepth 1 -name "*.c"
) else (
    echo.
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
