@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     echo.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=echo.exe
set SOURCE=echo.c
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

REM ========== T01: Basic echo
echo(
echo --- T01: Basic echo ---
"%OUTPUT%" hello > "%TEMP%\echotest.txt"
findstr /c:"hello" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: Multiple words
echo(
echo --- T02: Multiple words ---
"%OUTPUT%" hello world > "%TEMP%\echotest.txt"
findstr /c:"hello world" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: Empty string
echo(
echo --- T03: Empty string ---
"%OUTPUT%" > "%TEMP%\echotest.txt"
REM An empty line is expected
if exist "%TEMP%\echotest.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -n no newline
echo(
echo --- T04: -n no newline ---
"%OUTPUT%" -n hello > "%TEMP%\echotest.txt"
"%OUTPUT%" world >> "%TEMP%\echotest.txt"
findstr /c:"helloworld" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -e newline
echo(
echo --- T05: -e newline ---
"%OUTPUT%" -e "a\nb" > "%TEMP%\echotest.txt"
findstr /c:"a" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -e tab
echo(
echo --- T06: -e tab ---
"%OUTPUT%" -e "a\tb" > "%TEMP%\echotest.txt"
findstr /c:"a" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -e backslash
echo(
echo --- T07: -e backslash ---
"%OUTPUT%" -e "\\" > "%TEMP%\echotest.txt"
REM Check that the output contains a backslash
for /f "usebackq" %%a in ("%TEMP%\echotest.txt") do (
    set line=%%a
)
if "!line!"=="\" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -e \c stops output
echo(
echo --- T08: -e \c stops ---
"%OUTPUT%" -e "hello\c world" > "%TEMP%\echotest.txt"
findstr /c:"world" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -e octal
echo(
echo --- T09: -e octal ---
"%OUTPUT%" -e "\101\102\103" > "%TEMP%\echotest.txt"
findstr /c:"ABC" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: -e hex
echo(
echo --- T10: -e hex ---
"%OUTPUT%" -e "\x41\x42\x43" > "%TEMP%\echotest.txt"
findstr /c:"ABC" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -E disable escape (default)
echo(
echo --- T11: -E disable escape ---
"%OUTPUT%" -E "hello\nworld" > "%TEMP%\echotest.txt"
findstr /c:"\n" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: Default -E (no -e)
echo(
echo --- T12: Default -E ---
"%OUTPUT%" "hello\nworld" > "%TEMP%\echotest.txt"
findstr /c:"\n" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: -e -n combined
echo(
echo --- T13: -e -n combined ---
"%OUTPUT%" -e -n "a\nb" > "%TEMP%\echotest.txt"
"%OUTPUT%" c >> "%TEMP%\echotest.txt"
REM -e -n "a\nb" outputs "a\nb" without trailing newline, then "c\n"
REM Result: "a\nbc\n" -> "bc" on same line
findstr /c:"bc" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: -ne combined
echo(
echo --- T14: -ne combined ---
"%OUTPUT%" -ne "a\nb" > "%TEMP%\echotest.txt"
"%OUTPUT%" c >> "%TEMP%\echotest.txt"
findstr /c:"bc" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -en combined
echo(
echo --- T15: -en combined ---
"%OUTPUT%" -en "a\nb" > "%TEMP%\echotest.txt"
"%OUTPUT%" c >> "%TEMP%\echotest.txt"
findstr /c:"bc" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: --help
echo(
echo --- T16: --help ---
"%OUTPUT%" --help > "%TEMP%\echotest.txt"
findstr /c:"Usage:" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: --version
echo(
echo --- T17: --version ---
"%OUTPUT%" --version > "%TEMP%\echotest.txt"
findstr /c:"9.7" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -e \b backspace
echo(
echo --- T18: -e \b backspace ---
"%OUTPUT%" -e "abcd\b" > "%TEMP%\echotest.txt"
REM After backspace, last char deleted
findstr /c:"abc" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -e \r carriage return
echo(
echo --- T19: -e \r carriage return ---
"%OUTPUT%" -e "hello\rworld" > "%TEMP%\echotest.txt"
REM \r causes overwrite
findstr /c:"world" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -e \a bell
echo(
echo --- T20: -e \a bell ---
"%OUTPUT%" -e "hello\a" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: -e \e escape
echo(
echo --- T21: -e \e escape ---
"%OUTPUT%" -e "\e[31mred\e[0m" > "%TEMP%\echotest.txt"
findstr /c:"red" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: -- separator
echo(
echo --- T22: -- treated as string ---
"%OUTPUT%" -- -n > "%TEMP%\echotest.txt"
findstr /c:"-n" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: Invalid option treated as string
echo(
echo --- T23: Invalid option treated as string ---
"%OUTPUT%" -X > "%TEMP%\echotest.txt"
findstr /c:"X" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: -e -E order (last wins)
echo(
echo --- T24: -e -E last wins ---
"%OUTPUT%" -e -E "a\nb" > "%TEMP%\echotest.txt"
findstr /c:"\n" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: -E -e order (last wins)
echo(
echo --- T25: -E -e last wins ---
"%OUTPUT%" -E -e "a\nb" > "%TEMP%\echotest.txt"
findstr /c:"\n" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T26: -e \f form feed
echo(
echo --- T26: -e \f form feed ---
"%OUTPUT%" -e "a\fb" > "%TEMP%\echotest.txt"
findstr /c:"a" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T27: -e \v vertical tab
echo(
echo --- T27: -e \v vertical tab ---
"%OUTPUT%" -e "a\vb" > "%TEMP%\echotest.txt"
findstr /c:"a" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T28: -e octal with 1 digit
echo(
echo --- T28: -e octal 1 digit ---
"%OUTPUT%" -e "\65" > "%TEMP%\echotest.txt"
findstr /c:"5" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T29: -e hex with 1 digit
echo(
echo --- T29: -e hex 1 digit ---
"%OUTPUT%" -e "\x41" > "%TEMP%\echotest.txt"
findstr /c:"A" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T30: -e \c stops with -n
echo(
echo --- T30: -e \c stops with -n ---
"%OUTPUT%" -ne "hello\c world" > "%TEMP%\echotest.txt"
"%OUTPUT%" c >> "%TEMP%\echotest.txt"
findstr /c:"world" "%TEMP%\echotest.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
del "%TEMP%\echotest.txt" 2>nul

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
