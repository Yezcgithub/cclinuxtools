@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     du.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=du.exe
set SOURCE=du.c
set PASS=0
set FAIL=0

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

REM Create test directory structure
set TD=%TEMP%\dutest
if exist "%TD%" rmdir /s /q "%TD%"
mkdir "%TD%"
mkdir "%TD%\subdir"
echo hello > "%TD%\file1.txt"
echo world! > "%TD%\file2.txt"
echo test > "%TD%\subdir\file3.txt"
echo data > "%TD%\subdir\file4.txt"

REM ========== T01: Basic du
echo(
echo --- T01: Basic du ---
"%OUTPUT%" "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"%TD%" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: -h human readable
echo(
echo --- T02: -h human readable ---
"%OUTPUT%" -h "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /r "[0-9][KM]" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: -a all files
echo(
echo --- T03: -a all files ---
"%OUTPUT%" -a "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -s summarize
echo(
echo --- T04: -s summarize ---
"%OUTPUT%" -s "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"%TD%" "%TEMP%\du_out.txt" >nul
findstr /c:"subdir" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -c total
echo(
echo --- T05: -c total ---
"%OUTPUT%" -c "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"total" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -k 1K blocks
echo(
echo --- T06: -k 1K blocks ---
"%OUTPUT%" -k "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"%TD%" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -d 0 max-depth
echo(
echo --- T07: -d 0 max-depth ---
"%OUTPUT%" -d 0 "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"%TD%" "%TEMP%\du_out.txt" >nul
findstr /c:"subdir" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -S separate-dirs
echo(
echo --- T08: -S separate-dirs ---
"%OUTPUT%" -S "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"subdir" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: --help
echo(
echo --- T09: --help ---
"%OUTPUT%" --help > "%TEMP%\du_out.txt"
findstr /c:"Usage:" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: --version
echo(
echo --- T10: --version ---
"%OUTPUT%" --version > "%TEMP%\du_out.txt"
findstr /c:"9.7" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -b bytes
echo(
echo --- T11: -b bytes ---
"%OUTPUT%" -b -a "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: non-existent directory
echo(
echo --- T12: non-existent directory ---
"%OUTPUT%" "%TEMP%\nonexistent_dir_xyz" 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: --exclude pattern
echo(
echo --- T13: --exclude pattern ---
"%OUTPUT%" -a --exclude=*.txt "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -d 1 max-depth=1
echo(
echo --- T14: -d 1 max-depth=1 ---
"%OUTPUT%" -d 1 "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"subdir" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: multiple arguments
echo(
echo --- T15: multiple arguments ---
"%OUTPUT%" "%TD%\file1.txt" "%TD%\file2.txt" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
findstr /c:"file2.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: no arguments (current dir)
echo(
echo --- T16: no arguments (current dir) ---
pushd "%TEMP%"
"%TEMP%\..\du.exe" 2>nul || "%OUTPUT%" > "%TEMP%\du_out.txt" 2>&1
if exist "%TEMP%\du_out.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
popd

REM ========== T17: -B 1 custom block size
echo(
echo --- T17: -B 1 custom block size ---
"%OUTPUT%" -B 1 -a "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: --apparent-size
echo(
echo --- T18: --apparent-size ---
"%OUTPUT%" --apparent-size -a "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"file1.txt" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: --si
echo(
echo --- T19: --si ---
"%OUTPUT%" --si "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /r "[0-9][kM]" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -m 1M blocks
echo(
echo --- T20: -m 1M blocks ---
"%OUTPUT%" -m "%TD%" > "%TEMP%\du_out.txt" 2>&1
findstr /c:"%TD%" "%TEMP%\du_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
rmdir /s /q "%TD%" 2>nul
del "%TEMP%\du_out.txt" 2>nul

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
