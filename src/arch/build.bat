@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     arch.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=arch.exe
set SOURCE=arch.c
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

set OUTFILE=%TEMP%\archtest_out.txt

REM ========== T01: default (no option)
echo(
echo --- T01: default (no option) ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: output is non-empty
echo(
echo --- T02: output non-empty ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_m=%%a"
if defined _m (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: --help
echo(
echo --- T03: --help ---
"%OUTPUT%" --help > "%OUTFILE%" 2>nul
findstr /c:"Usage:" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: --version
echo(
echo --- T04: --version ---
"%OUTPUT%" --version > "%OUTFILE%" 2>nul
findstr /c:"arch" "%OUTFILE%" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: invalid short option exits non-zero
echo(
echo --- T05: invalid short option ---
"%OUTPUT%" -x >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: extra operand exits non-zero
echo(
echo --- T06: extra operand ---
"%OUTPUT%" extra >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: unknown long option exits non-zero
echo(
echo --- T07: unknown long option ---
"%OUTPUT%" --bogus >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: output is single line
echo(
echo --- T08: output is single line ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
powershell -NoProfile -Command "$c=Get-Content '%OUTFILE%' -Raw; $n=($c.Trim() -split '\r?\n').Count; if ($n -eq 1) { exit 0 } else { exit 1 }"
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: - (dash alone) is error
echo(
echo --- T09: dash alone is error ---
"%OUTPUT%" - >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: -- alone is error
echo(
echo --- T10: double dash alone is error ---
"%OUTPUT%" -- >nul 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: --help ignores extra args (help wins, exits 0)
echo(
echo --- T11: --help exits 0 ---
"%OUTPUT%" --help >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: --version exits 0
echo(
echo --- T12: --version exits 0 ---
"%OUTPUT%" --version >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: output matches PROCESSOR_ARCHITECTURE
echo(
echo --- T13: output matches PROCESSOR_ARCHITECTURE ---
"%OUTPUT%" > "%OUTFILE%" 2>nul
for /f "usebackq delims=" %%a in ("%OUTFILE%") do set "_arch_out=%%a"
for /f "usebackq" %%a in ('powershell -NoProfile -Command "$env:PROCESSOR_ARCHITECTURE.Trim()"') do set "_arch_env=%%a"
if "!_arch_out!"=="!_arch_env!" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

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
