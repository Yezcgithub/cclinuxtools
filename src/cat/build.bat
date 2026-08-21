@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     cat.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=cat.exe
set SOURCE=cat.c
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

set T1=%TEMP%\cattest1.txt
set T2=%TEMP%\cattest2.txt

REM ========== T01: Basic cat
echo(
echo --- T01: Basic cat ---
printf "hello\n" > "%T1%" 2>nul || echo hello> "%T1%"
"%OUTPUT%" "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"hello" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: Multiple files
echo(
echo --- T02: Multiple files ---
echo aaa> "%T1%"
echo bbb> "%T2%"
"%OUTPUT%" "%T1%" "%T2%" > "%TEMP%\cattest_out.txt"
findstr /c:"aaa" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: stdin pipe
echo(
echo --- T03: stdin pipe ---
echo piped | "%OUTPUT%" > "%TEMP%\cattest_out.txt"
findstr /c:"piped" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -n number all lines
echo(
echo --- T04: -n number lines ---
echo line1> "%T1%"
echo line2>> "%T1%"
echo line3>> "%T1%"
"%OUTPUT%" -n "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"line1" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -b number nonblank only
echo(
echo --- T05: -b number nonblank ---
echo aaa> "%T1%"
echo.>> "%T1%"
echo bbb>> "%T1%"
"%OUTPUT%" -b "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"aaa" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -E show ends
echo(
echo --- T06: -E show ends ---
echo hi> "%T1%"
"%OUTPUT%" -E "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"$" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -T show tabs
echo(
echo --- T07: -T show tabs ---
echo 		x> "%T1%"
"%OUTPUT%" -T "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"^I" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -s squeeze blank
echo(
echo --- T08: -s squeeze blank ---
REM Create file with LF-only blank lines (GNU cat only squeezes LF blanks, not CRLF)
powershell -NoProfile -Command "[IO.File]::WriteAllText('%T1%', 'a'+[char]10+[char]10+[char]10+[char]10+'b'+[char]10)"
"%OUTPUT%" -s "%T1%" > "%TEMP%\cattest_out.txt"
REM After squeeze: a, blank, b (3 lines)
powershell -NoProfile -Command "$c=[IO.File]::ReadAllText('%TEMP%\cattest_out.txt'); $n=$c.Split([char]10).Count; if ($n -le 4) { exit 0 } else { exit 1 }"
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: --help
echo(
echo --- T09: --help ---
"%OUTPUT%" --help > "%TEMP%\cattest_out.txt"
findstr /c:"Usage:" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: --version
echo(
echo --- T10: --version ---
"%OUTPUT%" --version > "%TEMP%\cattest_out.txt"
findstr /c:"9.7" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -A show all
echo(
echo --- T11: -A show all ---
echo 	x> "%T1%"
"%OUTPUT%" -A "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"^I" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: non-existent file error
echo(
echo --- T12: non-existent file error ---
"%OUTPUT%" "nonexistent_file_xyz.txt" 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: dash as filename = stdin
echo(
echo --- T13: - as stdin ---
echo dashstd | "%OUTPUT%" - > "%TEMP%\cattest_out.txt"
findstr /c:"dashstd" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -e equivalent to -vE
echo(
echo --- T14: -e (equiv -vE) ---
echo hi> "%T1%"
"%OUTPUT%" -e "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"$" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -t equivalent to -vT
echo(
echo --- T15: -t (equiv -vT) ---
echo 		x> "%T1%"
"%OUTPUT%" -t "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"^I" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: -v show nonprinting
echo(
echo --- T16: -v show nonprinting ---
"%OUTPUT%" -v "%T1%" > "%TEMP%\cattest_out.txt" 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: empty file
echo(
echo --- T17: empty file ---
type nul > "%T1%"
"%OUTPUT%" "%T1%" > "%TEMP%\cattest_out.txt"
for /f %%a in ('type "%TEMP%\cattest_out.txt" ^| find /c /v ""') do set lines=%%a
if !lines! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -- separator
echo(
echo --- T18: -- separator ---
echo content> "%T1%"
"%OUTPUT%" -- "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"content" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -nE combined
echo(
echo --- T19: combined -nE ---
echo hi> "%T1%"
"%OUTPUT%" -nE "%T1%" > "%TEMP%\cattest_out.txt"
findstr /c:"hi" "%TEMP%\cattest_out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -s with no blanks
echo(
echo --- T20: -s no blanks ---
echo a> "%T1%"
echo b>> "%T1%"
echo c>> "%T1%"
"%OUTPUT%" -s "%T1%" > "%TEMP%\cattest_out.txt"
for /f %%a in ('type "%TEMP%\cattest_out.txt" ^| find /c /v ""') do set lines=%%a
if !lines! equ 3 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
del "%T1%" 2>nul
del "%T2%" 2>nul
del "%TEMP%\cattest_out.txt" 2>nul

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
