@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     df.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=df.exe
set SOURCE=df.c
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

REM ========== T01: Basic df
echo(
echo --- T01: Basic df ---
"%OUTPUT%" > "%TEMP%\df_out.txt" 2>&1
if !ERRORLEVEL! equ 0 (
    for %%A in ("%TEMP%\df_out.txt") do set FILE_SIZE=%%~zA
    if !FILE_SIZE! gtr 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
) else (
    set /a FAIL+=1 & echo   [FAIL]
)

REM ========== T02: -h human readable
echo(
echo --- T02: -h human readable ---
"%OUTPUT%" -h > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Size" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: -H SI
echo(
echo --- T03: -H SI ---
"%OUTPUT%" -H > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Size" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -T print type
echo(
echo --- T04: -T print type ---
"%OUTPUT%" -T > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Type" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: --total
echo(
echo --- T05: --total ---
"%OUTPUT%" --total > "%TEMP%\df_out.txt" 2>&1
findstr /c:"total" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -i inodes
echo(
echo --- T06: -i inodes ---
"%OUTPUT%" -i > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Inodes" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -k 1K blocks
echo(
echo --- T07: -k 1K blocks ---
"%OUTPUT%" -k > "%TEMP%\df_out.txt" 2>&1
findstr /c:"1K-blocks" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: --help
echo(
echo --- T08: --help ---
"%OUTPUT%" --help > "%TEMP%\df_out.txt" 2>nul
findstr /c:"Usage:" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: --version
echo(
echo --- T09: --version ---
"%OUTPUT%" --version > "%TEMP%\df_out.txt" 2>nul
findstr /c:"9.11" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: -B 1M block size
echo(
echo --- T10: -B 1M block size ---
"%OUTPUT%" -B 1M > "%TEMP%\df_out.txt" 2>&1
findstr /c:"1M-blocks" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: --output with fields
echo(
echo --- T11: --output=size,used,avail,target ---
"%OUTPUT%" --output=size,used,avail,target > "%TEMP%\df_out.txt" 2>&1
findstr /c:"size" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: -hT combined
echo(
echo --- T12: -hT combined ---
"%OUTPUT%" -hT > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Type" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: -h --total combined
echo(
echo --- T13: -h --total combined ---
"%OUTPUT%" -h --total > "%TEMP%\df_out.txt" 2>&1
findstr /c:"total" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -l local only
echo(
echo --- T14: -l local only ---
"%OUTPUT%" -l > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Filesystem" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -a all
echo(
echo --- T15: -a all ---
"%OUTPUT%" -a > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Filesystem" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: -x exclude type
echo(
echo --- T16: -x ntfs ---
"%OUTPUT%" -x ntfs > "%TEMP%\df_out.txt" 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: --output without value (all fields)
echo(
echo --- T17: --output (all fields) ---
"%OUTPUT%" --output > "%TEMP%\df_out.txt" 2>&1
findstr /c:"source" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -hT --total
echo(
echo --- T18: -hT --total ---
"%OUTPUT%" -hT --total > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Type" "%TEMP%\df_out.txt" >nul
findstr /c:"total" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -B 1 byte block
echo(
echo --- T19: -B 1 block ---
"%OUTPUT%" -B 1 > "%TEMP%\df_out.txt" 2>&1
findstr /c:"1B-blocks" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -P portability
echo(
echo --- T20: -P portability ---
"%OUTPUT%" -P > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Filesystem" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: -t type filter
echo(
echo --- T21: -t NTFS ---
"%OUTPUT%" -t NTFS > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Filesystem" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: -v ignored
echo(
echo --- T22: -v (ignored) ---
"%OUTPUT%" -v > "%TEMP%\df_out.txt" 2>&1
findstr /c:"Filesystem" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: --output mutual exclusivity with -i
echo(
echo --- T23: --output incompatible with -i ---
"%OUTPUT%" --output -i > "%TEMP%\df_out.txt" 2>&1
findstr /c:"incompatible" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: --total + --output
echo(
echo --- T24: --total --output ---
"%OUTPUT%" --total --output=source,size,used,avail > "%TEMP%\df_out.txt" 2>&1
findstr /c:"total" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: --output split usage
echo(
echo --- T25: --output=target --output=pcent ---
"%OUTPUT%" --output=target --output=pcent > "%TEMP%\df_out.txt" 2>&1
findstr /c:"target" "%TEMP%\df_out.txt" >nul
findstr /c:"pcent" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T26: DF_BLOCK_SIZE env var
echo(
echo --- T26: DF_BLOCK_SIZE=1M ---
set DF_BLOCK_SIZE=1M
"%OUTPUT%" > "%TEMP%\df_out.txt" 2>&1
findstr /c:"1M-blocks" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
set DF_BLOCK_SIZE=

REM ========== T27: POSIXLY_CORRECT env var
echo(
echo --- T27: POSIXLY_CORRECT ---
set POSIXLY_CORRECT=1
"%OUTPUT%" > "%TEMP%\df_out.txt" 2>&1
findstr /c:"512-blocks" "%TEMP%\df_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
set POSIXLY_CORRECT=

REM Cleanup
del "%TEMP%\df_out.txt" 2>nul

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
