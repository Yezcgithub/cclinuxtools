@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     uname.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=uname.exe
set SOURCE=uname.c
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

set OUTFILE=%TEMP%\unametest_out.txt

REM Helper: run command, capture stdout to OUTFILE
REM Helper: count words in OUTFILE via PowerShell

REM ========== T01: default (no option == -s)
echo(
echo --- T01: default (== -s) ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
findstr /c:"Windows" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: -s kernel name
echo(
echo --- T02: -s kernel name ---
"%OUTPUT%" -s > "%OUTFILE%" 2>nul
findstr /c:"Windows" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: -n nodename (non-empty)
echo(
echo --- T03: -n nodename ---
"%OUTPUT%" -n > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_n=%%a"
if defined _n (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -r kernel release (non-empty)
echo(
echo --- T04: -r kernel release ---
"%OUTPUT%" -r > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_r=%%a"
if defined _r (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -v kernel version (non-empty)
echo(
echo --- T05: -v kernel version ---
"%OUTPUT%" -v > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_v=%%a"
if defined _v (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -m machine (non-empty)
echo(
echo --- T06: -m machine ---
"%OUTPUT%" -m > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_m=%%a"
if defined _m (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -o operating system == Windows
echo(
echo --- T07: -o operating system ---
"%OUTPUT%" -o > "%OUTFILE%" 2>nul
findstr /c:"Windows" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -a all information (multi-field, starts with Windows)
echo(
echo --- T08: -a all information ---
"%OUTPUT%" -a > "%OUTFILE%" 2>nul
findstr /b /c:"Windows" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
REM count fields >= 5
powershell -NoProfile -Command "$c=Get-Content '%OUTFILE%' -Raw; $n=($c.Trim() -split '\s+').Count; if ($n -ge 5) { exit 0 } else { exit 1 }"
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -srm combined (3 fields)
echo(
echo --- T09: -srm combined ---
"%OUTPUT%" -srm > "%OUTFILE%" 2>nul
powershell -NoProfile -Command "$c=Get-Content '%OUTFILE%' -Raw; $n=($c.Trim() -split '\s+').Count; if ($n -eq 3) { exit 0 } else { exit 1 }"
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: --help
echo(
echo --- T10: --help ---
"%OUTPUT%" --help > "%OUTFILE%" 2>nul
findstr /c:"Usage:" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: --version
echo(
echo --- T11: --version ---
"%OUTPUT%" --version > "%OUTFILE%" 2>nul
findstr /c:"uname" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: long options --kernel-name == -s
echo(
echo --- T12: long options ---
"%OUTPUT%" --kernel-name > "%OUTFILE%" 2>nul
findstr /c:"Windows" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
"%OUTPUT%" --machine > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_longm=%%a"
"%OUTPUT%" -m > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_shortm=%%a"
if "!_longm!"=="!_shortm!" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: -p processor (non-empty)
echo(
echo --- T13: -p processor ---
"%OUTPUT%" -p > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_p=%%a"
if defined _p (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -i hardware platform (non-empty)
echo(
echo --- T14: -i hardware platform ---
"%OUTPUT%" -i > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_i=%%a"
if defined _i (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: invalid option exits non-zero
echo(
echo --- T15: invalid option ---
"%OUTPUT%" -Z >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: extra operand exits non-zero
echo(
echo --- T16: extra operand ---
"%OUTPUT%" -s extra >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: -a first field == -s
echo(
echo --- T17: -a first field == -s ---
"%OUTPUT%" -a > "%OUTFILE%" 2>nul
for /f "usebackq tokens=1" %%a in ("%OUTFILE%") do set "_first=%%a"
"%OUTPUT%" -s > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_s=%%a"
if "!_first!"=="!_s!" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -a last field == -o
echo(
echo --- T18: -a last field == -o ---
"%OUTPUT%" -a > "%OUTFILE%" 2>nul
REM Use PowerShell to robustly grab the last token
for /f "delims=" %%a in ('powershell -NoProfile -Command "$c=Get-Content '%OUTFILE%' -Raw; ($c.Trim() -split '\s+')[-1]"') do set "_last=%%a"
"%OUTPUT%" -o > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_o=%%a"
if "!_last!"=="!_o!" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
del "%OUTFILE%" 2>nul

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
