@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     md5sum.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall
set OUTPUT=md5sum.exe
set SOURCE=md5sum.c

echo(
echo [1/3] Cleaning previous build...
if exist "%OUTPUT%" (
    del "%OUTPUT%"
    echo   Removed %OUTPUT%
)

echo(
echo [2/3] Compiling %SOURCE%...
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE%
echo(
%CC% %CFLAGS% -o %OUTPUT% %SOURCE%

if %ERRORLEVEL% neq 0 (
    echo(
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo(
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%

echo(
echo ============================================
echo   Running full functional tests...
echo ============================================

powershell -NoProfile -ExecutionPolicy Bypass -File _run_tests.ps1

set EXITCODE=%ERRORLEVEL%
echo(
if %EXITCODE% equ 0 (
    echo All tests passed!
) else (
    echo Some tests failed!
)
exit /b %EXITCODE%
