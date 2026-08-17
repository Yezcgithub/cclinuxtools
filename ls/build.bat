@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     ls.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=ls.exe
set SOURCE=ls.c

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

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
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
echo --- Test 1: Basic listing ---
"%OUTPUT%" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 2: Long format (-l) ---
"%OUTPUT%" -l >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 3: Show all (-a) ---
mkdir "%TDIR%\sub1" 2>nul
echo test> "%TDIR%\sub1\.hidden"
"%OUTPUT%" -a "%TDIR%\sub1" 2>&1 | findstr "\.hidden" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 4: Almost all (-A) ---
"%OUTPUT%" -A "%TDIR%\sub1" 2>&1 | findstr /v /c:"^.$" | findstr /v /c:"^..$" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 5: Human readable (-h) ---
"%OUTPUT%" -lh >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 6: One per line (-1) ---
"%OUTPUT%" -1 >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 7: Sort by size (-S) ---
"%OUTPUT%" -S >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 8: Sort by time (-t) ---
"%OUTPUT%" -t >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 9: Reverse sort (-r) ---
"%OUTPUT%" -r >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 10: Classify (-F) ---
echo hello> "%TDIR%\test.bat"
"%OUTPUT%" -F "%TDIR%" 2>&1 | findstr "\*" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 11: Quote names (-Q) ---
"%OUTPUT%" -Q > "%TDIR%\qout.txt" 2>&1
findstr /c:"\"" "%TDIR%\qout.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 12: Show inode (-i) ---
"%OUTPUT%" -i >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 13: Show blocks (-s) ---
"%OUTPUT%" -s >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 14: Recursive (-R) ---
mkdir "%TDIR%\r1\r2" 2>nul
echo f> "%TDIR%\r1\r2\f.txt"
"%OUTPUT%" -R "%TDIR%\r1" > "%TDIR%\rout.txt" 2>&1
findstr "f.txt" "%TDIR%\rout.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 15: Directory only (-d) ---
"%OUTPUT%" -d . >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 16: Slash indicator (-p) ---
"%OUTPUT%" -p "%TDIR%" > "%TDIR%\pout.txt" 2>&1
:: Use findstr with literal string and check for subdir listing
findstr /c:"sub1" "%TDIR%\pout.txt" >nul 2>&1
:: -p should exit cleanly at minimum
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 17: Version ---
"%OUTPUT%" --version 2>&1 | findstr "1.0.0" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 18: Help ---
"%OUTPUT%" --help 2>&1 | findstr "Usage" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 19: Columnar (-C) ---
"%OUTPUT%" -C >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 20: Across (-x) ---
"%OUTPUT%" -x >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

:: Cleanup
rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Build and test complete!
echo ============================================
