@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     touch.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=touch.exe
set SOURCE=touch.c
set PASS=0
set FAIL=0
set TESTDIR=%TEMP%\touch_test

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

REM Create test directory
if not exist "%TESTDIR%" mkdir "%TESTDIR%"
cd /d "%TESTDIR%"

REM Clean up old test files
del /q test*.txt 2>nul

REM ========== T01: Basic create
echo(
echo --- T01: Basic create ---
"%~dp0%OUTPUT%" test1.txt
if exist test1.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: -c no create (non-existent file)
echo(
echo --- T02: -c no create ---
"%~dp0%OUTPUT%" -c test2_nonexist.txt
if exist test2_nonexist.txt (set /a FAIL+=1 & echo   [FAIL]) else (set /a PASS+=1 & echo   [PASS])

REM ========== T03: -t stamp
echo(
echo --- T03: -t 202401011200.00 ---
"%~dp0%OUTPUT%" -t 202401011200.00 test3.txt
if exist test3.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -d date
echo(
echo --- T04: -d "2024-06-15 10:30:00" ---
"%~dp0%OUTPUT%" -d "2024-06-15 10:30:00" test4.txt
if exist test4.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -r reference
echo(
echo --- T05: -r test3.txt ---
"%~dp0%OUTPUT%" -r test3.txt test5.txt
if exist test5.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: --help
echo(
echo --- T06: --help ---
"%~dp0%OUTPUT%" --help > "%TEMP%\touch_out.txt" 2>nul
findstr /c:"Usage:" "%TEMP%\touch_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: --version
echo(
echo --- T07: --version ---
"%~dp0%OUTPUT%" --version > "%TEMP%\touch_out.txt" 2>nul
findstr /c:"9.11" "%TEMP%\touch_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -a access only
echo(
echo --- T08: -a access only ---
"%~dp0%OUTPUT%" -a test1.txt
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -m modify only
echo(
echo --- T09: -m modify only ---
"%~dp0%OUTPUT%" -m test1.txt
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: multiple files
echo(
echo --- T10: multiple files ---
"%~dp0%OUTPUT%" test7.txt test8.txt test9.txt
if exist test7.txt if exist test8.txt if exist test9.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: --time=atime
echo(
echo --- T11: --time=atime ---
"%~dp0%OUTPUT%" --time=atime test1.txt
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: --time=mtime
echo(
echo --- T12: --time=mtime ---
"%~dp0%OUTPUT%" --time=mtime test1.txt
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: -d @epoch
echo(
echo --- T13: -d @epoch ---
"%~dp0%OUTPUT%" -d "@1700000000" test10.txt
if exist test10.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -d now
echo(
echo --- T14: -d now ---
"%~dp0%OUTPUT%" -d now test11.txt
if exist test11.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -d today
echo(
echo --- T15: -d today ---
"%~dp0%OUTPUT%" -d today test12.txt
if exist test12.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: -d yesterday
echo(
echo --- T16: -d yesterday ---
"%~dp0%OUTPUT%" -d yesterday test13.txt
if exist test13.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: -- delimiter
echo(
echo --- T17: -- delimiter ---
"%~dp0%OUTPUT%" -- test14.txt
if exist test14.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -d ISO date only
echo(
echo --- T18: -d 2024-03-15 ---
"%~dp0%OUTPUT%" -d "2024-03-15" test15.txt
if exist test15.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -t short stamp (MMDDhhmm)
echo(
echo --- T19: -t 01011200 ---
"%~dp0%OUTPUT%" -t 01011200 test16.txt
if exist test16.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -d "HH:MM:SS"
echo(
echo --- T20: -d "10:30:00" ---
"%~dp0%OUTPUT%" -d "10:30:00" test17.txt
if exist test17.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: -am both
echo(
echo --- T21: -am both ---
"%~dp0%OUTPUT%" -am test18.txt
if exist test18.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: -f ignored
echo(
echo --- T22: -f ignored ---
"%~dp0%OUTPUT%" -f test19.txt
if exist test19.txt (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
del /q test*.txt 2>nul
del "%TEMP%\touch_out.txt" 2>nul
cd /d "%~dp0"
rmdir /q "%TESTDIR%" 2>nul

echo(
echo ============================================
echo   Test Results: PASS=%PASS%  FAIL=%FAIL%
echo ============================================
if %FAIL% equ 0 (
    echo   All tests passed!
) else (
    echo   Some tests failed!
)

endlocal & exit /b 0
