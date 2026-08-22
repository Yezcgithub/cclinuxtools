@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    whoami.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=whoami.exe
set SOURCE=whoami.c

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

if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Build failed with exit code !ERRORLEVEL!
    exit /b !ERRORLEVEL!
)

echo.
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%
echo.
echo ============================================
echo   Running basic tests...
echo ============================================

:: Setup test directory
set TDIR=_build_test
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"

echo.
echo --- Test 1: Basic whoami ---
"%OUTPUT%" > "%TDIR%\t1.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 2: Output matches %%USERNAME%% ---
"%OUTPUT%" > "%TDIR%\t2.txt" 2>&1
findstr /c:"%USERNAME%" "%TDIR%\t2.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 3: Output non-empty ---
for %%I in ("%TDIR%\t1.txt") do set SZ=%%~zI
if !SZ! gtr 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 4: Single-line output ---
for /f %%L in ('type "%TDIR%\t1.txt" ^| find /c /v ""') do set LINES=%%L
if !LINES! equ 1 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 5: --help ---
"%OUTPUT%" --help > "%TDIR%\t5.txt"
findstr /c:"Usage" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 6: --version ---
"%OUTPUT%" --version > "%TDIR%\t6.txt"
findstr /c:"1.0.0" "%TDIR%\t6.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 7: '--' alone is accepted ---
"%OUTPUT%" -- > "%TDIR%\t7.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 8: Unknown long option is an error ---
"%OUTPUT%" --thisoptiondoesnotexist >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 9: Unknown short option is an error ---
"%OUTPUT%" -Z >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 10: Extra operand is an error ---
"%OUTPUT%" foo >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 11: --help takes precedence over a following operand ---
"%OUTPUT%" --help foo > "%TDIR%\t11.txt" 2>&1
if !ERRORLEVEL! equ 0 (
    findstr /c:"Usage" "%TDIR%\t11.txt" >nul
    if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])
) else (
    echo   [FAIL]
)

echo.
echo --- Test 12: '--' makes following token an extra operand error ---
"%OUTPUT%" -- foo >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

:: Cleanup
rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Build and test complete!
echo ============================================
