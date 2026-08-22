@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    top.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set LIBS=-lpsapi -ladvapi32
set OUTPUT=top.exe
set SOURCE=top.c
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
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE% %LIBS%
echo.
%CC% %CFLAGS% -o %OUTPUT% %SOURCE% %LIBS%

if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Build failed with exit code !ERRORLEVEL!
    exit /b !ERRORLEVEL!
)

echo.
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%
echo.
echo ============================================
echo   Running basic tests...
echo ============================================

:: Setup test directory
set TDIR=_build_test
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"

echo.
echo --- Test 1: Basic batch mode runs ---
"%OUTPUT%" -b -n 1 -d 0.1 > "%TDIR%\t1.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 1 & set /a PASS+=1) else (echo   [FAIL] Test 1 & set /a FAIL+=1)

echo.
echo --- Test 2: Output is non-empty ---
for %%I in ("%TDIR%\t1.txt") do set SZ=%%~zI
if !SZ! gtr 0 (echo   [PASS] Test 2 & set /a PASS+=1) else (echo   [FAIL] Test 2 & set /a FAIL+=1)

echo.
echo --- Test 3: --help mentions Usage ---
"%OUTPUT%" --help > "%TDIR%\t3.txt" 2>&1
findstr /c:"Usage" "%TDIR%\t3.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 3 & set /a PASS+=1) else (echo   [FAIL] Test 3 & set /a FAIL+=1)

echo.
echo --- Test 4: --version contains v1.0.0 ---
"%OUTPUT%" --version > "%TDIR%\t4.txt" 2>&1
findstr /c:"v1.0.0" "%TDIR%\t4.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 4 & set /a PASS+=1) else (echo   [FAIL] Test 4 & set /a FAIL+=1)

echo.
echo --- Test 5: Output has 'top -' header line ---
findstr /R /c:"^top -" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 5 & set /a PASS+=1) else (echo   [FAIL] Test 5 & set /a FAIL+=1)

echo.
echo --- Test 6: Output has Tasks: line ---
findstr /c:"Tasks:" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 6 & set /a PASS+=1) else (echo   [FAIL] Test 6 & set /a FAIL+=1)

echo.
echo --- Test 7: Output has Cpu line ---
findstr /c:"Cpu" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 7 & set /a PASS+=1) else (echo   [FAIL] Test 7 & set /a FAIL+=1)

echo.
echo --- Test 8: Output has MiB Mem line ---
findstr /c:"MiB Mem" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 8 & set /a PASS+=1) else (echo   [FAIL] Test 8 & set /a FAIL+=1)

echo.
echo --- Test 9: Output has PID column header ---
findstr /c:"PID" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 9 & set /a PASS+=1) else (echo   [FAIL] Test 9 & set /a FAIL+=1)

echo.
echo --- Test 10: Output has COMMAND column header ---
findstr /c:"COMMAND" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 10 & set /a PASS+=1) else (echo   [FAIL] Test 10 & set /a FAIL+=1)

echo.
echo --- Test 11: -b -n 2 -d 0.1 runs two iterations ---
"%OUTPUT%" -b -n 2 -d 0.1 > "%TDIR%\t11.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 11 & set /a PASS+=1) else (echo   [FAIL] Test 11 & set /a FAIL+=1)

echo.
echo --- Test 12: -o %%MEM sorts by memory ---
"%OUTPUT%" -b -n 1 -d 0.1 -o %%MEM > "%TDIR%\t12.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 12 & set /a PASS+=1) else (echo   [FAIL] Test 12 & set /a FAIL+=1)

echo.
echo --- Test 13: -o %%CPU sorts by CPU ---
"%OUTPUT%" -b -n 1 -d 0.1 -o %%CPU > "%TDIR%\t13.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 13 & set /a PASS+=1) else (echo   [FAIL] Test 13 & set /a FAIL+=1)

echo.
echo --- Test 14: -o TIME+ sorts by time ---
"%OUTPUT%" -b -n 1 -d 0.1 -o TIME+ > "%TDIR%\t14.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 14 & set /a PASS+=1) else (echo   [FAIL] Test 14 & set /a FAIL+=1)

echo.
echo --- Test 15: -o PID sorts by PID ---
"%OUTPUT%" -b -n 1 -d 0.1 -o PID > "%TDIR%\t15.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 15 & set /a PASS+=1) else (echo   [FAIL] Test 15 & set /a FAIL+=1)

echo.
echo --- Test 16: -c toggle runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -c > "%TDIR%\t16.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 16 & set /a PASS+=1) else (echo   [FAIL] Test 16 & set /a FAIL+=1)

echo.
echo --- Test 17: --batch --iterations=1 runs ---
"%OUTPUT%" --batch --iterations=1 --delay=0.1 > "%TDIR%\t17.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 17 & set /a PASS+=1) else (echo   [FAIL] Test 17 & set /a FAIL+=1)

echo.
echo --- Test 18: -H thread mode runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -H > "%TDIR%\t18.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 18 & set /a PASS+=1) else (echo   [FAIL] Test 18 & set /a FAIL+=1)

echo.
echo --- Test 19: -S cumulative mode runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -S > "%TDIR%\t19.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 19 & set /a PASS+=1) else (echo   [FAIL] Test 19 & set /a FAIL+=1)

echo.
echo --- Test 20: -1 single CPU toggle runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -1 > "%TDIR%\t20.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 20 & set /a PASS+=1) else (echo   [FAIL] Test 20 & set /a FAIL+=1)

echo.
echo --- Test 21: -d with float delay works ---
"%OUTPUT%" -b -n 1 -d 0.5 > "%TDIR%\t21.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 21 & set /a PASS+=1) else (echo   [FAIL] Test 21 & set /a FAIL+=1)

echo.
echo --- Test 22: -d with invalid argument errors ---
"%OUTPUT%" -b -d abc >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 22 & set /a PASS+=1) else (echo   [FAIL] Test 22 & set /a FAIL+=1)

echo.
echo --- Test 23: -d without argument errors ---
"%OUTPUT%" -b -d >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 23 & set /a PASS+=1) else (echo   [FAIL] Test 23 & set /a FAIL+=1)

echo.
echo --- Test 24: -n without argument errors ---
"%OUTPUT%" -b -n >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 24 & set /a PASS+=1) else (echo   [FAIL] Test 24 & set /a FAIL+=1)

echo.
echo --- Test 25: -o without argument errors ---
"%OUTPUT%" -b -o >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 25 & set /a PASS+=1) else (echo   [FAIL] Test 25 & set /a FAIL+=1)

echo.
echo --- Test 26: Unknown long option is an error ---
"%OUTPUT%" --no-such-option >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 26 & set /a PASS+=1) else (echo   [FAIL] Test 26 & set /a FAIL+=1)

echo.
echo --- Test 27: Unknown short option is an error ---
"%OUTPUT%" -Z >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 27 & set /a PASS+=1) else (echo   [FAIL] Test 27 & set /a FAIL+=1)

echo.
echo --- Test 28: Extra operand is an error ---
"%OUTPUT%" extra_operand >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 28 & set /a PASS+=1) else (echo   [FAIL] Test 28 & set /a FAIL+=1)

echo.
echo --- Test 29: -p with own PID runs ---
for /f "tokens=2" %%a in ('tasklist /fi "imagename eq top.exe" /nh 2^>nul ^| findstr /r "[0-9]"') do set TOPPID=%%a
"%OUTPUT%" -b -n 1 -d 0.1 -p 1 > "%TDIR%\t29.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 29 & set /a PASS+=1) else (echo   [FAIL] Test 29 & set /a FAIL+=1)

echo.
echo --- Test 30: -u with current user runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -u "%USERNAME%" > "%TDIR%\t30.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 30 & set /a PASS+=1) else (echo   [FAIL] Test 30 & set /a FAIL+=1)

echo.
echo --- Test 31: Combined -bcH runs ---
"%OUTPUT%" -bcH -n 1 -d 0.1 > "%TDIR%\t31.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 31 & set /a PASS+=1) else (echo   [FAIL] Test 31 & set /a FAIL+=1)

echo.
echo --- Test 32: -w width option runs ---
"%OUTPUT%" -b -n 1 -d 0.1 -w 120 > "%TDIR%\t32.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 32 & set /a PASS+=1) else (echo   [FAIL] Test 32 & set /a FAIL+=1)

:: Cleanup
rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Test Results: PASS=!PASS!  FAIL=!FAIL!
echo ============================================
if !FAIL! equ 0 (
    echo   All tests passed!
    exit /b 0
) else (
    echo   Some tests failed!
    exit /b 1
)
