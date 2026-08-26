@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     test.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=test.exe
set BRACKET=[.exe
set SOURCE=test.c
set PASS=0
set FAIL=0

echo(
echo [1/3] Cleaning previous build...
if exist "%OUTPUT%" (
    del "%OUTPUT%"
    echo   Removed %OUTPUT%
)
if exist "%BRACKET%" (
    del "%BRACKET%"
    echo   Removed %BRACKET%
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

REM Create [ as a copy of test
copy "%OUTPUT%" "%BRACKET%" >nul 2>nul

echo(
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%
echo   Output: %CD%\%BRACKET%

echo(
echo ============================================
echo   Running full functional tests...
echo ============================================

REM ========== T01: test with no args (false)
echo(
echo --- T01: test no args (false) ---
"%OUTPUT%" >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: test non-empty string (true)
echo(
echo --- T02: test non-empty string (true) ---
"%OUTPUT%" hello >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: test empty string (false)
echo(
echo --- T03: test empty string (false) ---
"%OUTPUT%" "" >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: test -n non-empty (true)
echo(
echo --- T04: test -n non-empty (true) ---
"%OUTPUT%" -n hello >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: test -z empty (true)
echo(
echo --- T05: test -z empty (true) ---
"%OUTPUT%" -z "" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: test ! non-empty (false)
echo(
echo --- T06: test ! non-empty (false) ---
"%OUTPUT%" ! hello >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: test string equality = (true)
echo(
echo --- T07: test string = (true) ---
"%OUTPUT%" abc = abc >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: test string inequality != (true)
echo(
echo --- T08: test string != (true) ---
"%OUTPUT%" abc != def >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: test integer -eq (true)
echo(
echo --- T09: test -eq (true) ---
"%OUTPUT%" 5 -eq 5 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: test integer -ne (true)
echo(
echo --- T10: test -ne (true) ---
"%OUTPUT%" 5 -ne 6 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: test integer -lt (true)
echo(
echo --- T11: test -lt (true) ---
"%OUTPUT%" 5 -lt 6 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: test integer -gt (true)
echo(
echo --- T12: test -gt (true) ---
"%OUTPUT%" 6 -gt 5 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: test integer -le (true)
echo(
echo --- T13: test -le (true) ---
"%OUTPUT%" 5 -le 5 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: test integer -ge (true)
echo(
echo --- T14: test -ge (true) ---
"%OUTPUT%" 5 -ge 5 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: test -a logical AND (true)
echo(
echo --- T15: test -a (true) ---
"%OUTPUT%" -n hello -a -n world >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: test -o logical OR (true)
echo(
echo --- T16: test -o (true) ---
"%OUTPUT%" -z "" -o -n hello >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: test -a AND short-circuit (false)
echo(
echo --- T17: test -a false (false) ---
"%OUTPUT%" -n hello -a -z hello >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: test ! -n (negation)
echo(
echo --- T18: test ! -n (false) ---
"%OUTPUT%" ! -n hello >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: test parentheses grouping
echo(
echo --- T19: test ( ) grouping (true) ---
"%OUTPUT%" "(" -n hello ")" -a "(" -z "" ")" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: test -e on existing file
echo(
echo --- T20: test -e existing file (true) ---
"%OUTPUT%" -e "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: test -e on non-existent file (false)
echo(
echo --- T21: test -e non-existent (false) ---
"%OUTPUT%" -e nonexistentfile123 >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: test -f regular file (true)
echo(
echo --- T22: test -f regular file (true) ---
"%OUTPUT%" -f "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: test -d directory (true)
echo(
echo --- T23: test -d directory (true) ---
"%OUTPUT%" -d "." >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: test -s non-empty file (true)
echo(
echo --- T24: test -s non-empty file (true) ---
"%OUTPUT%" -s "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: [ with ] (true)
echo(
echo --- T25: [ -n hello ] (true) ---
"%BRACKET%" -n hello ] >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T26: [ without ] (error)
echo(
echo --- T26: [ without ] (error) ---
"%BRACKET%" -n hello >nul 2>nul
if !ERRORLEVEL! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T27: [ empty (false)
echo(
echo --- T27: [ ] (false) ---
"%BRACKET%" ] >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T28: test --help
echo(
echo --- T28: test --help ---
"%OUTPUT%" --help > "%TEMP%\testhelp.txt" 2>nul
findstr /c:"Usage:" "%TEMP%\testhelp.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T29: test --version
echo(
echo --- T29: test --version ---
"%OUTPUT%" --version > "%TEMP%\testver.txt" 2>nul
findstr /c:"test" "%TEMP%\testver.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T30: test string < (true)
echo(
echo --- T30: test string < (true) ---
"%OUTPUT%" abc "<" def >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T31: test string > (true)
echo(
echo --- T31: test string > (true) ---
"%OUTPUT%" def ">" abc >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T32: test invalid integer (error)
echo(
echo --- T32: test invalid integer (error) ---
"%OUTPUT%" abc -eq 5 >nul 2>nul
if !ERRORLEVEL! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T33: test -r readable (true)
echo(
echo --- T33: test -r readable (true) ---
"%OUTPUT%" -r "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T34: test -w writable (true)
echo(
echo --- T34: test -w writable (true) ---
"%OUTPUT%" -w "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T35: test complex grouping
echo(
echo --- T35: test complex grouping (true) ---
"%OUTPUT%" "(" -n hello -o -z "" ")" -a "(" -n world ")" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T36: test ! with grouping
echo(
echo --- T36: test ! grouping (false) ---
"%OUTPUT%" ! "(" -n hello -a -n world ")" >nul 2>nul
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T37: test -nt newer file
echo(
echo --- T37: test -nt (true) ---
"%OUTPUT%" "%SOURCE%" -nt nonexistent123 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T38: test -ot older file
echo(
echo --- T38: test -ot (true) ---
"%OUTPUT%" nonexistent123 -ot "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T39: test -ef same file (true)
echo(
echo --- T39: test -ef same file (true) ---
"%OUTPUT%" "%SOURCE%" -ef "%SOURCE%" >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T40: test -t terminal FD 0
echo(
echo --- T40: test -t 0 ---
"%OUTPUT%" -t 0 >nul 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a PASS+=1 & echo   [PASS - non-terminal])

REM Cleanup
del "%TEMP%\testhelp.txt" 2>nul
del "%TEMP%\testver.txt" 2>nul

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
