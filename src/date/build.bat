@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    date.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=date.exe
set SOURCE=date.c
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

:: Reference file for -r tests
echo hello > "%TDIR%\ref.txt"

echo.
echo --- Test 1: Basic date runs ---
"%OUTPUT%" > "%TDIR%\t1.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 1 & set /a PASS+=1) else (echo   [FAIL] Test 1 & set /a FAIL+=1)

echo.
echo --- Test 2: Output non-empty ---
for %%I in ("%TDIR%\t1.txt") do set SZ=%%~zI
if !SZ! gtr 0 (echo   [PASS] Test 2 & set /a PASS+=1) else (echo   [FAIL] Test 2 & set /a FAIL+=1)

echo.
echo --- Test 3: --help mentions Usage ---
"%OUTPUT%" --help > "%TDIR%\t3.txt" 2>&1
findstr /c:"Usage" "%TDIR%\t3.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 3 & set /a PASS+=1) else (echo   [FAIL] Test 3 & set /a FAIL+=1)

echo.
echo --- Test 4: --version contains 1.0.0 ---
"%OUTPUT%" --version > "%TDIR%\t4.txt" 2>&1
findstr /c:"1.0.0" "%TDIR%\t4.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 4 & set /a PASS+=1) else (echo   [FAIL] Test 4 & set /a FAIL+=1)

echo.
echo --- Test 5: +%%Y returns 4-digit year ---
"%OUTPUT%" +%%Y > "%TDIR%\t5.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 5 & set /a PASS+=1) else (echo   [FAIL] Test 5 & set /a FAIL+=1)

echo.
echo --- Test 6: +%%s is numeric ---
"%OUTPUT%" +%%s > "%TDIR%\t6.txt" 2>&1
findstr /R "^[0-9][0-9]*" "%TDIR%\t6.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 6 & set /a PASS+=1) else (echo   [FAIL] Test 6 & set /a FAIL+=1)

echo.
echo --- Test 7: -u / --utc / --universal all accepted ---
"%OUTPUT%" -u >nul 2>&1
set R1=!ERRORLEVEL!
"%OUTPUT%" --utc >nul 2>&1
set R2=!ERRORLEVEL!
"%OUTPUT%" --universal >nul 2>&1
set R3=!ERRORLEVEL!
if "!R1!!R2!!R3!"=="000" (echo   [PASS] Test 7 & set /a PASS+=1) else (echo   [FAIL] Test 7 & set /a FAIL+=1)

echo.
echo --- Test 8: -u +%%z is +0000 ---
"%OUTPUT%" -u +%%z > "%TDIR%\t8.txt" 2>&1
findstr /R "^+0000" "%TDIR%\t8.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 8 & set /a PASS+=1) else (echo   [FAIL] Test 8 & set /a FAIL+=1)

echo.
echo --- Test 9: -d @0 +%%s yields 0 ---
"%OUTPUT%" -d @0 +%%s > "%TDIR%\t9.txt" 2>&1
findstr /R "^0" "%TDIR%\t9.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 9 & set /a PASS+=1) else (echo   [FAIL] Test 9 & set /a FAIL+=1)

echo.
echo --- Test 10: -d 2020-01-01 +%%Y-%%m-%%d yields 2020-01-01 ---
"%OUTPUT%" -d 2020-01-01 +%%Y-%%m-%%d > "%TDIR%\t10.txt" 2>&1
findstr /c:"2020-01-01" "%TDIR%\t10.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 10 & set /a PASS+=1) else (echo   [FAIL] Test 10 & set /a FAIL+=1)

echo.
echo --- Test 11: -I default outputs YYYY-MM-DD ---
"%OUTPUT%" -I > "%TDIR%\t11.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]" "%TDIR%\t11.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 11 & set /a PASS+=1) else (echo   [FAIL] Test 11 & set /a FAIL+=1)

echo.
echo --- Test 12: -Iseconds contains T separator ---
"%OUTPUT%" -Iseconds > "%TDIR%\t12.txt" 2>&1
findstr /c:"T" "%TDIR%\t12.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 12 & set /a PASS+=1) else (echo   [FAIL] Test 12 & set /a FAIL+=1)

echo.
echo --- Test 13: -Ins contains nanoseconds (dot) ---
"%OUTPUT%" -Ins > "%TDIR%\t13.txt" 2>&1
findstr /c:"." "%TDIR%\t13.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 13 & set /a PASS+=1) else (echo   [FAIL] Test 13 & set /a FAIL+=1)

echo.
echo --- Test 14: -R produces a weekday name prefix ---
"%OUTPUT%" -R > "%TDIR%\t14.txt" 2>&1
findstr /R "^Mon ^Tue ^Wed ^Thu ^Fri ^Sat ^Sun" "%TDIR%\t14.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 14 & set /a PASS+=1) else (echo   [FAIL] Test 14 & set /a FAIL+=1)

echo.
echo --- Test 15: --rfc-email alias works ---
"%OUTPUT%" --rfc-email > "%TDIR%\t15.txt" 2>&1
findstr /R "^Mon ^Tue ^Wed ^Thu ^Fri ^Sat ^Sun" "%TDIR%\t15.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 15 & set /a PASS+=1) else (echo   [FAIL] Test 15 & set /a FAIL+=1)

echo.
echo --- Test 16: --rfc-3339=seconds ---
"%OUTPUT%" --rfc-3339=seconds > "%TDIR%\t16.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]" "%TDIR%\t16.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 16 & set /a PASS+=1) else (echo   [FAIL] Test 16 & set /a FAIL+=1)

echo.
echo --- Test 17: --rfc-3339=date ---
"%OUTPUT%" --rfc-3339=date > "%TDIR%\t17.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]" "%TDIR%\t17.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 17 & set /a PASS+=1) else (echo   [FAIL] Test 17 & set /a FAIL+=1)

echo.
echo --- Test 18: -r / --reference file mtime ---
"%OUTPUT%" -r "%TDIR%\ref.txt" +%%Y > "%TDIR%\t18.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]" "%TDIR%\t18.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 18 & set /a PASS+=1) else (echo   [FAIL] Test 18 & set /a FAIL+=1)

echo.
echo --- Test 19: -d yesterday parses ---
"%OUTPUT%" -d yesterday +%%Y > "%TDIR%\t19.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]" "%TDIR%\t19.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 19 & set /a PASS+=1) else (echo   [FAIL] Test 19 & set /a FAIL+=1)

echo.
echo --- Test 20: -d tomorrow parses ---
"%OUTPUT%" -d tomorrow +%%Y > "%TDIR%\t20.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]" "%TDIR%\t20.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 20 & set /a PASS+=1) else (echo   [FAIL] Test 20 & set /a FAIL+=1)

echo.
echo --- Test 21: -d 2020-02-29 (leap day) ---
"%OUTPUT%" -d 2020-02-29 +%%Y-%%m-%%d > "%TDIR%\t21.txt" 2>&1
findstr /c:"2020-02-29" "%TDIR%\t21.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 21 & set /a PASS+=1) else (echo   [FAIL] Test 21 & set /a FAIL+=1)

echo.
echo --- Test 22: +%%F outputs YYYY-MM-DD ---
"%OUTPUT%" +%%F > "%TDIR%\t22.txt" 2>&1
findstr /R "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]" "%TDIR%\t22.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 22 & set /a PASS+=1) else (echo   [FAIL] Test 22 & set /a FAIL+=1)

echo.
echo --- Test 23: +%%z yields +-HHMM ---
"%OUTPUT%" +%%z > "%TDIR%\t23.txt" 2>&1
findstr /R "^[+] ^-" "%TDIR%\t23.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 23 & set /a PASS+=1) else (echo   [FAIL] Test 23 & set /a FAIL+=1)

echo.
echo --- Test 24: Unknown long option is an error ---
"%OUTPUT%" --no-such-option >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 24 & set /a PASS+=1) else (echo   [FAIL] Test 24 & set /a FAIL+=1)

echo.
echo --- Test 25: Unknown short option is an error ---
"%OUTPUT%" -Z >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 25 & set /a PASS+=1) else (echo   [FAIL] Test 25 & set /a FAIL+=1)

echo.
echo --- Test 26: -d without argument is an error ---
"%OUTPUT%" -d >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 26 & set /a PASS+=1) else (echo   [FAIL] Test 26 & set /a FAIL+=1)

echo.
echo --- Test 27: -s without argument is an error ---
"%OUTPUT%" -s >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 27 & set /a PASS+=1) else (echo   [FAIL] Test 27 & set /a FAIL+=1)

echo.
echo --- Test 28: Mutually exclusive -d and -s error ---
"%OUTPUT%" -d now -s now >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 28 & set /a PASS+=1) else (echo   [FAIL] Test 28 & set /a FAIL+=1)

echo.
echo --- Test 29: Multiple output formats (-I -R) error ---
"%OUTPUT%" -I -R >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 29 & set /a PASS+=1) else (echo   [FAIL] Test 29 & set /a FAIL+=1)

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
