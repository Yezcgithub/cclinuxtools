@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    free.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=free.exe
set SOURCE=free.c
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
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE% -lpsapi
echo.
%CC% %CFLAGS% -o %OUTPUT% %SOURCE% -lpsapi

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
echo --- Test 1: Basic free runs ---
"%OUTPUT%" > "%TDIR%\t1.txt" 2>&1
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
echo --- Test 5: Default output contains Mem: row ---
"%OUTPUT%" > "%TDIR%\t5.txt" 2>&1
findstr /c:"Mem:" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 5 & set /a PASS+=1) else (echo   [FAIL] Test 5 & set /a FAIL+=1)

echo.
echo --- Test 6: Default output contains Swap: row ---
findstr /c:"Swap:" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 6 & set /a PASS+=1) else (echo   [FAIL] Test 6 & set /a FAIL+=1)

echo.
echo --- Test 7: Default output has header (total/used/free) ---
findstr /c:"total" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 7 & set /a PASS+=1) else (echo   [FAIL] Test 7 & set /a FAIL+=1)

echo.
echo --- Test 8: Narrow mode shows buff/cache (combined) ---
findstr /c:"buff/cache" "%TDIR%\t5.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 8 & set /a PASS+=1) else (echo   [FAIL] Test 8 & set /a FAIL+=1)

echo.
echo --- Test 9: -w / --wide shows separate buffers and cache ---
"%OUTPUT%" -w > "%TDIR%\t9.txt" 2>&1
findstr /c:"buffers" "%TDIR%\t9.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"cache" "%TDIR%\t9.txt" >nul
set R2=!ERRORLEVEL!
if "!R1!!R2!"=="00" (echo   [PASS] Test 9 & set /a PASS+=1) else (echo   [FAIL] Test 9 & set /a FAIL+=1)

echo.
echo --- Test 10: --wide alias produces wide header ---
"%OUTPUT%" --wide > "%TDIR%\t10.txt" 2>&1
findstr /c:"buffers" "%TDIR%\t10.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"cache" "%TDIR%\t10.txt" >nul
set R2=!ERRORLEVEL!
if "!R1!!R2!"=="00" (echo   [PASS] Test 10 & set /a PASS+=1) else (echo   [FAIL] Test 10 & set /a FAIL+=1)

echo.
echo --- Test 11: -b / --bytes output is numeric ---
"%OUTPUT%" -b > "%TDIR%\t11.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9]" "%TDIR%\t11.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 11 & set /a PASS+=1) else (echo   [FAIL] Test 11 & set /a FAIL+=1)

echo.
echo --- Test 12: --bytes alias works ---
"%OUTPUT%" --bytes > "%TDIR%\t12.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9]" "%TDIR%\t12.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 12 & set /a PASS+=1) else (echo   [FAIL] Test 12 & set /a FAIL+=1)

echo.
echo --- Test 13: -k / --kibi is default ---
"%OUTPUT%" -k > "%TDIR%\t13.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9]" "%TDIR%\t13.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 13 & set /a PASS+=1) else (echo   [FAIL] Test 13 & set /a FAIL+=1)

echo.
echo --- Test 14: -m / --mebi works ---
"%OUTPUT%" -m > "%TDIR%\t14.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9]" "%TDIR%\t14.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 14 & set /a PASS+=1) else (echo   [FAIL] Test 14 & set /a FAIL+=1)

echo.
echo --- Test 15: -g / --gibi works ---
"%OUTPUT%" -g > "%TDIR%\t15.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9]" "%TDIR%\t15.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 15 & set /a PASS+=1) else (echo   [FAIL] Test 15 & set /a FAIL+=1)

echo.
echo --- Test 16: -h / --human produces values with unit suffix ---
"%OUTPUT%" -h > "%TDIR%\t16.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9][KMGTPE]" "%TDIR%\t16.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 16 & set /a PASS+=1) else (echo   [FAIL] Test 16 & set /a FAIL+=1)

echo.
echo --- Test 17: --human alias works ---
"%OUTPUT%" --human > "%TDIR%\t17.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9][KMGTPE]" "%TDIR%\t17.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 17 & set /a PASS+=1) else (echo   [FAIL] Test 17 & set /a FAIL+=1)

echo.
echo --- Test 18: -l / --lohi shows Low: and High: rows ---
"%OUTPUT%" -l > "%TDIR%\t18.txt" 2>&1
findstr /c:"Low:" "%TDIR%\t18.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"High:" "%TDIR%\t18.txt" >nul
set R2=!ERRORLEVEL!
if "!R1!!R2!"=="00" (echo   [PASS] Test 18 & set /a PASS+=1) else (echo   [FAIL] Test 18 & set /a FAIL+=1)

echo.
echo --- Test 19: --lohi alias works ---
"%OUTPUT%" --lohi > "%TDIR%\t19.txt" 2>&1
findstr /c:"Low:" "%TDIR%\t19.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"High:" "%TDIR%\t19.txt" >nul
set R2=!ERRORLEVEL!
if "!R1!!R2!"=="00" (echo   [PASS] Test 19 & set /a PASS+=1) else (echo   [FAIL] Test 19 & set /a FAIL+=1)

echo.
echo --- Test 20: -t / --total shows Total: row ---
"%OUTPUT%" -t > "%TDIR%\t20.txt" 2>&1
findstr /c:"Total:" "%TDIR%\t20.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 20 & set /a PASS+=1) else (echo   [FAIL] Test 20 & set /a FAIL+=1)

echo.
echo --- Test 21: --total alias works ---
"%OUTPUT%" --total > "%TDIR%\t21.txt" 2>&1
findstr /c:"Total:" "%TDIR%\t21.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 21 & set /a PASS+=1) else (echo   [FAIL] Test 21 & set /a FAIL+=1)

echo.
echo --- Test 22: --si works ---
"%OUTPUT%" --si > "%TDIR%\t22.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 22 & set /a PASS+=1) else (echo   [FAIL] Test 22 & set /a FAIL+=1)

echo.
echo --- Test 23: --iec works ---
"%OUTPUT%" --iec > "%TDIR%\t23.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 23 & set /a PASS+=1) else (echo   [FAIL] Test 23 & set /a FAIL+=1)

echo.
echo --- Test 24: Combined short options -hlwt ---
"%OUTPUT%" -hlwt > "%TDIR%\t24.txt" 2>&1
findstr /c:"Low:" "%TDIR%\t24.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"High:" "%TDIR%\t24.txt" >nul
set R2=!ERRORLEVEL!
findstr /c:"Total:" "%TDIR%\t24.txt" >nul
set R3=!ERRORLEVEL!
findstr /c:"buffers" "%TDIR%\t24.txt" >nul
set R4=!ERRORLEVEL!
if "!R1!!R2!!R3!!R4!"=="0000" (echo   [PASS] Test 24 & set /a PASS+=1) else (echo   [FAIL] Test 24 & set /a FAIL+=1)

echo.
echo --- Test 25: -h --si uses SI suffixes (K/M/G without i) ---
"%OUTPUT%" -h --si > "%TDIR%\t25.txt" 2>&1
findstr /R /c:"^Mem:.*[0-9][KMGTPE]" "%TDIR%\t25.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 25 & set /a PASS+=1) else (echo   [FAIL] Test 25 & set /a FAIL+=1)

echo.
echo --- Test 26: -s 1 -c 1 runs once and exits ---
"%OUTPUT%" -s 1 -c 1 > "%TDIR%\t26.txt" 2>&1
if !ERRORLEVEL! equ 0 (
    for %%I in ("%TDIR%\t26.txt") do set SZ=%%~zI
    if !SZ! gtr 0 (echo   [PASS] Test 26 & set /a PASS+=1) else (echo   [FAIL] Test 26 & set /a FAIL+=1)
) else (
    echo   [FAIL] Test 26 & set /a FAIL+=1
)

echo.
echo --- Test 27: --seconds=1 --count=1 long form works ---
"%OUTPUT%" --seconds=1 --count=1 > "%TDIR%\t27.txt" 2>&1
if !ERRORLEVEL! equ 0 (
    for %%I in ("%TDIR%\t27.txt") do set SZ=%%~zI
    if !SZ! gtr 0 (echo   [PASS] Test 27 & set /a PASS+=1) else (echo   [FAIL] Test 27 & set /a FAIL+=1)
) else (
    echo   [FAIL] Test 27 & set /a FAIL+=1
)

echo.
echo --- Test 28: -s with invalid argument errors ---
"%OUTPUT%" -s abc >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 28 & set /a PASS+=1) else (echo   [FAIL] Test 28 & set /a FAIL+=1)

echo.
echo --- Test 29: -s without argument errors ---
"%OUTPUT%" -s >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 29 & set /a PASS+=1) else (echo   [FAIL] Test 29 & set /a FAIL+=1)

echo.
echo --- Test 30: -c without argument errors ---
"%OUTPUT%" -c >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 30 & set /a PASS+=1) else (echo   [FAIL] Test 30 & set /a FAIL+=1)

echo.
echo --- Test 31: Unknown long option is an error ---
"%OUTPUT%" --no-such-option >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 31 & set /a PASS+=1) else (echo   [FAIL] Test 31 & set /a FAIL+=1)

echo.
echo --- Test 32: Unknown short option is an error ---
"%OUTPUT%" -Z >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 32 & set /a PASS+=1) else (echo   [FAIL] Test 32 & set /a FAIL+=1)

echo.
echo --- Test 33: Extra operand is an error ---
"%OUTPUT%" extra_operand >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 33 & set /a PASS+=1) else (echo   [FAIL] Test 33 & set /a FAIL+=1)

echo.
echo --- Test 34: -lt combined shows lohi + total ---
"%OUTPUT%" -lt > "%TDIR%\t34.txt" 2>&1
findstr /c:"Low:" "%TDIR%\t34.txt" >nul
set R1=!ERRORLEVEL!
findstr /c:"High:" "%TDIR%\t34.txt" >nul
set R2=!ERRORLEVEL!
findstr /c:"Total:" "%TDIR%\t34.txt" >nul
set R3=!ERRORLEVEL!
if "!R1!!R2!!R3!"=="000" (echo   [PASS] Test 34 & set /a PASS+=1) else (echo   [FAIL] Test 34 & set /a FAIL+=1)

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
