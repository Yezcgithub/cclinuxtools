@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     mkdir.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=mkdir.exe
set SOURCE=mkdir.c
set PASS=0
set FAIL=0

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
echo   Running full functional tests...
echo ============================================

set TDIR=%TEMP%\mkdir_ftest_%random%
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"

REM ========== T01: Basic directory creation
echo.
echo --- T01: Basic directory ---
"%OUTPUT%" "%TDIR%\d1" >nul 2>&1
if exist "%TDIR%\d1" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: Multiple directories
echo.
echo --- T02: Multiple directories ---
"%OUTPUT%" "%TDIR%\m1" "%TDIR%\m2" "%TDIR%\m3" >nul 2>&1
set ok=1
if not exist "%TDIR%\m1" set ok=0
if not exist "%TDIR%\m2" set ok=0
if not exist "%TDIR%\m3" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: Existing directory errors
echo.
echo --- T03: Existing dir errors ---
"%OUTPUT%" "%TDIR%\d1" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -p parents
echo.
echo --- T04: -p parents ---
"%OUTPUT%" -p "%TDIR%\a\b\c\d" >nul 2>&1
if exist "%TDIR%\a\b\c\d" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -p no error if exists
echo.
echo --- T05: -p no error if exists ---
"%OUTPUT%" -p "%TDIR%\a\b\c\d" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: --parents long
echo.
echo --- T06: --parents long ---
"%OUTPUT%" --parents "%TDIR%\x\y\z" >nul 2>&1
if exist "%TDIR%\x\y\z" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -v verbose
echo.
echo --- T07: -v verbose ---
"%OUTPUT%" -v "%TDIR%\vd1" > "%TDIR%\vout.txt" 2>&1
findstr /C:"created directory" "%TDIR%\vout.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: --verbose long
echo.
echo --- T08: --verbose long ---
"%OUTPUT%" --verbose "%TDIR%\vd2" > "%TDIR%\vout2.txt" 2>&1
findstr /C:"created directory" "%TDIR%\vout2.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -pv combined
echo.
echo --- T09: -pv combined ---
"%OUTPUT%" -pv "%TDIR%\pvd1\pvd2" > "%TDIR%\pvout.txt" 2>&1
findstr /C:"created directory" "%TDIR%\pvout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if exist "%TDIR%\pvd1\pvd2" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: -m 755 mode
echo.
echo --- T10: -m 755 ---
"%OUTPUT%" -m 755 "%TDIR%\m755" >nul 2>&1
if exist "%TDIR%\m755" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -m 700 mode
echo.
echo --- T11: -m 700 ---
"%OUTPUT%" -m 700 "%TDIR%\m700" >nul 2>&1
if exist "%TDIR%\m700" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: -m 777 mode
echo.
echo --- T12: -m 777 ---
"%OUTPUT%" -m 777 "%TDIR%\m777" >nul 2>&1
if exist "%TDIR%\m777" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: --mode=755 long
echo.
echo --- T13: --mode=755 ---
"%OUTPUT%" --mode=755 "%TDIR%\ml755" >nul 2>&1
if exist "%TDIR%\ml755" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: --mode 755 (separate arg)
echo.
echo --- T14: --mode 755 separate ---
"%OUTPUT%" --mode 755 "%TDIR%\mls755" >nul 2>&1
if exist "%TDIR%\mls755" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: -m with -p
echo.
echo --- T15: -m with -p ---
"%OUTPUT%" -p -m 700 "%TDIR%\mp\a\b" >nul 2>&1
if exist "%TDIR%\mp\a\b" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: -m with -v
echo.
echo --- T16: -m with -v ---
"%OUTPUT%" -m 755 -v "%TDIR%\mv1" > "%TDIR%\mvout.txt" 2>&1
findstr /C:"created directory" "%TDIR%\mvout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if exist "%TDIR%\mv1" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: -p -v -m combined
echo.
echo --- T17: -p -v -m combined ---
"%OUTPUT%" -p -v -m 755 "%TDIR%\pvm\a\b\c" > "%TDIR%\pvmout.txt" 2>&1
findstr /C:"created directory" "%TDIR%\pvmout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if exist "%TDIR%\pvm\a\b\c" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: File exists as file
echo.
echo --- T18: File exists as file ---
echo data> "%TDIR%\file1.txt"
"%OUTPUT%" "%TDIR%\file1.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -p file exists as file
echo.
echo --- T19: -p file exists as file ---
echo data> "%TDIR%\file2.txt"
"%OUTPUT%" -p "%TDIR%\file2.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: --version
echo.
echo --- T20: --version ---
"%OUTPUT%" --version 2>&1 | findstr "1.0.0" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: --help
echo.
echo --- T21: --help ---
"%OUTPUT%" --help 2>&1 | findstr "Usage:" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: -h help
echo.
echo --- T22: -h help ---
"%OUTPUT%" -h 2>&1 | findstr "Usage:" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: Missing operand
echo.
echo --- T23: Missing operand ---
"%OUTPUT%" 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: Invalid option
echo.
echo --- T24: Invalid option ---
"%OUTPUT%" -Z2 "%TDIR%\x" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: -- separator
echo.
echo --- T25: -- separator ---
"%OUTPUT%" -- "%TDIR%\dashdir" >nul 2>&1
if exist "%TDIR%\dashdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T26: -Z SELinux (ignored)
echo.
echo --- T26: -Z SELinux ignored ---
"%OUTPUT%" -Z "%TDIR%\zdir" >nul 2>&1
if exist "%TDIR%\zdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T27: Parent missing without -p
echo.
echo --- T27: Parent missing without -p ---
"%OUTPUT%" "%TDIR%\noexist\sub" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T28: Deep nested -p
echo.
echo --- T28: Deep nested -p ---
"%OUTPUT%" -p "%TDIR%\deep\d1\d2\d3\d4\d5" >nul 2>&1
if exist "%TDIR%\deep\d1\d2\d3\d4\d5" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T29: -p -v verbose each component
echo.
echo --- T29: -p -v each component ---
"%OUTPUT%" -p -v "%TDIR%\vpc\vpb" > "%TDIR%\vpcout.txt" 2>&1
findstr /C:"created directory" "%TDIR%\vpcout.txt" >nul
set cnt=0
for /f %%a in ('findstr /C:"created directory" "%TDIR%\vpcout.txt" ^| find /c /v ""') do set cnt=%%a
if !cnt! geq 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T30: -m invalid mode
echo.
echo --- T30: -m invalid mode ---
"%OUTPUT%" -m abc "%TDIR%\mdir" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T31: -m 0755 (leading zero)
echo.
echo --- T31: -m 0755 leading zero ---
"%OUTPUT%" -m 0755 "%TDIR%\m0755" >nul 2>&1
if exist "%TDIR%\m0755" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T32: -m requires argument
echo.
echo --- T32: -m requires argument ---
"%OUTPUT%" -m >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T33: --mode requires argument
echo.
echo --- T33: --mode requires argument ---
"%OUTPUT%" --mode >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T34: Multiple -p dirs
echo.
echo --- T34: Multiple -p dirs ---
"%OUTPUT%" -p "%TDIR%\mp1\a\b" "%TDIR%\mp2\c\d" >nul 2>&1
set ok=1
if not exist "%TDIR%\mp1\a\b" set ok=0
if not exist "%TDIR%\mp2\c\d" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T35: -p with trailing slash
echo.
echo --- T35: -p trailing slash ---
set "TSLASH=%TDIR%\tsdir/"
"%OUTPUT%" -p "%TSLASH%" >nul 2>&1
if exist "%TDIR%\tsdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T36: -m symbolic mode a+rwx
echo.
echo --- T36: -m a+rwx ---
"%OUTPUT%" -m a+rwx "%TDIR%\marwx" >nul 2>&1
if exist "%TDIR%\marwx" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T37: -m symbolic mode u=rwx,go=rx
echo.
echo --- T37: -m u=rwx,go=rx ---
"%OUTPUT%" -m u=rwx,go=rx "%TDIR%\msym" >nul 2>&1
if exist "%TDIR%\msym" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T38: -m mode out of range
echo.
echo --- T38: -m mode out of range ---
"%OUTPUT%" -m 9999 "%TDIR%\mout" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM Cleanup
if exist "%TDIR%" rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Test Results: PASS=%PASS%  FAIL=%FAIL%
echo ============================================
if %FAIL% equ 0 (
    echo   All tests passed!
) else (
    echo   Some tests failed!
)

endlocal & exit /b 0
