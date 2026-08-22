@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     hostname.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set LIBS=-lws2_32
set OUTPUT=hostname.exe
set SOURCE=hostname.c
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
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE% %LIBS%
echo(
%CC% %CFLAGS% -o %OUTPUT% %SOURCE% %LIBS%

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

set OUTFILE=%TEMP%\hostnametest_out.txt

REM ========== T01: default (print current host name)
echo(
echo --- T01: default (host name) ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_h=%%a"
if defined _h (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: -s short name (non-empty, no dot)
echo(
echo --- T02: -s short name ---
"%OUTPUT%" -s > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_s=%%a"
if defined _s (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
REM -s must not contain a literal dot
echo !_s!| findstr /c:"." >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: -s is prefix of default
echo(
echo --- T03: -s prefix of default ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_def=%%a"
"%OUTPUT%" -s > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_sh=%%a"
REM check _def starts with _sh
set _prefix=!_def:~0,100!
REM Use PowerShell for prefix test
powershell -NoProfile -Command "if ('%_def%'.StartsWith('%_sh%')) { exit 0 } else { exit 1 }"
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -f FQDN (non-empty)
echo(
echo --- T04: -f FQDN ---
"%OUTPUT%" -f > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_f=%%a"
if defined _f (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -d domain (non-empty, may be unknown)
echo(
echo --- T05: -d domain ---
"%OUTPUT%" -d > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_d=%%a"
if defined _d (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -i IP address (non-empty)
echo(
echo --- T06: -i IP address ---
"%OUTPUT%" -i > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_i=%%a"
if defined _i (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -I all IP addresses (non-empty)
echo(
echo --- T07: -I all IP addresses ---
"%OUTPUT%" -I > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_I=%%a"
if defined _I (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -y NIS domain (non-empty, typically unknown)
echo(
echo --- T08: -y NIS domain ---
"%OUTPUT%" -y > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_y=%%a"
if defined _y (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -a alias (runs without error)
echo(
echo --- T09: -a alias ---
"%OUTPUT%" -a > "%OUTFILE%" 2>nul
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
findstr /c:"hostname" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: long options --short == -s
echo(
echo --- T12: long options ---
"%OUTPUT%" --short > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_longshort=%%a"
"%OUTPUT%" -s > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_shortshort=%%a"
if "!_longshort!"=="!_shortshort!" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: invalid option exits non-zero
echo(
echo --- T13: invalid option ---
"%OUTPUT%" -Z >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: extra operand exits non-zero
echo(
echo --- T14: extra operand ---
"%OUTPUT%" -s extra >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -F nonexistent file exits non-zero
echo(
echo --- T15: -F nonexistent file ---
"%OUTPUT%" -F "Z:\nonexistent\xyz\host.txt" >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: -b boot mode tolerates bad file
echo(
echo --- T16: -b boot mode ---
"%OUTPUT%" -b -F "Z:\nonexistent\xyz\host.txt" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: -A all FQDNs (runs without error)
echo(
echo --- T17: -A all FQDNs ---
"%OUTPUT%" -A > "%OUTFILE%" 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: combined -sd runs
echo(
echo --- T18: combined -sd ---
"%OUTPUT%" -sd >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

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
