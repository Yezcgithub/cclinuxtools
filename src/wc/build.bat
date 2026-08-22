@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    wc.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=wc.exe
set SOURCE=wc.c
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

:: Create test files (Windows echo adds CRLF)
echo hello world> "%TDIR%\f1.txt"
(echo line1& echo line2& echo line3)> "%TDIR%\f2.txt"
type nul > "%TDIR%\empty.txt"
:: Create file without trailing newline
echo|set /p="no newline" > "%TDIR%\nonl.txt"
echo a b c d e> "%TDIR%\f5.txt"
echo aaaaaaaaaaaaaaaaaaaaaaaa> "%TDIR%\longline.txt"

:: Test 1: Basic stdin run
echo hello world | "%OUTPUT%" > nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 1: basic stdin runs & set /a PASS+=1) else (echo   [FAIL] Test 1 & set /a FAIL+=1)

:: Test 2: --help mentions Usage
"%OUTPUT%" --help > "%TDIR%\t2.txt" 2>&1
findstr /c:"Usage" "%TDIR%\t2.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 2: --help has Usage & set /a PASS+=1) else (echo   [FAIL] Test 2 & set /a FAIL+=1)

:: Test 3: --version contains 1.0.0
"%OUTPUT%" --version > "%TDIR%\t3.txt" 2>&1
findstr /c:"1.0.0" "%TDIR%\t3.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 3: --version has 1.0.0 & set /a PASS+=1) else (echo   [FAIL] Test 3 & set /a FAIL+=1)

:: Test 4: -l counts 1 line
"%OUTPUT%" -l "%TDIR%\f1.txt" > "%TDIR%\t4.txt" 2>&1
findstr /R "^[0-9]" "%TDIR%\t4.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 4: -l runs & set /a PASS+=1) else (echo   [FAIL] Test 4 & set /a FAIL+=1)

:: Test 5: -l on f2.txt is 3
"%OUTPUT%" -l "%TDIR%\f2.txt" > "%TDIR%\t5.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t5.txt"') do set VAL=%%a
if "!VAL!"=="3" (echo   [PASS] Test 5: -l f2.txt=3 & set /a PASS+=1) else (echo   [FAIL] Test 5: -l f2.txt=!VAL! & set /a FAIL+=1)

:: Test 6: -w on f1.txt is 2
"%OUTPUT%" -w "%TDIR%\f1.txt" > "%TDIR%\t6.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t6.txt"') do set VAL=%%a
if "!VAL!"=="2" (echo   [PASS] Test 6: -w f1.txt=2 & set /a PASS+=1) else (echo   [FAIL] Test 6: -w f1.txt=!VAL! & set /a FAIL+=1)

:: Test 7: -c on f1.txt is 13 (hello world + CRLF = 13)
"%OUTPUT%" -c "%TDIR%\f1.txt" > "%TDIR%\t7.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t7.txt"') do set VAL=%%a
if "!VAL!"=="13" (echo   [PASS] Test 7: -c f1.txt=13 & set /a PASS+=1) else (echo   [FAIL] Test 7: -c f1.txt=!VAL! & set /a FAIL+=1)

:: Test 8: -c on empty.txt is 0
"%OUTPUT%" -c "%TDIR%\empty.txt" > "%TDIR%\t8.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t8.txt"') do set VAL=%%a
if "!VAL!"=="0" (echo   [PASS] Test 8: -c empty=0 & set /a PASS+=1) else (echo   [FAIL] Test 8: -c empty=!VAL! & set /a FAIL+=1)

:: Test 9: -L on longline.txt is 25 (24 a's + CR)
"%OUTPUT%" -L "%TDIR%\longline.txt" > "%TDIR%\t9.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t9.txt"') do set VAL=%%a
if "!VAL!"=="25" (echo   [PASS] Test 9: -L longline=25 & set /a PASS+=1) else (echo   [FAIL] Test 9: -L longline=!VAL! & set /a FAIL+=1)

:: Test 10: Default mode has 4 columns (3 counts + filename)
"%OUTPUT%" "%TDIR%\f1.txt" > "%TDIR%\t10.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t10.txt"') do set LINE=%%a
set COLS=0
for %%x in (!LINE!) do set /a COLS+=1
if "!COLS!"=="4" (echo   [PASS] Test 10: default 4 columns & set /a PASS+=1) else (echo   [FAIL] Test 10: default cols=!COLS! & set /a FAIL+=1)

:: Test 11: Multiple files show total
"%OUTPUT%" "%TDIR%\f1.txt" "%TDIR%\f2.txt" > "%TDIR%\t11.txt" 2>&1
findstr /c:"total" "%TDIR%\t11.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 11: multiple files has total & set /a PASS+=1) else (echo   [FAIL] Test 11 & set /a FAIL+=1)

:: Test 12: -lw shows 3 columns (2 counts + filename)
"%OUTPUT%" -lw "%TDIR%\f1.txt" > "%TDIR%\t12.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t12.txt"') do set LINE=%%a
set COLS=0
for %%x in (!LINE!) do set /a COLS+=1
if "!COLS!"=="3" (echo   [PASS] Test 12: -lw 3 columns & set /a PASS+=1) else (echo   [FAIL] Test 12: -lw cols=!COLS! & set /a FAIL+=1)

:: Test 13: -lcw shows 4 columns (3 counts + filename)
"%OUTPUT%" -lcw "%TDIR%\f1.txt" > "%TDIR%\t13.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t13.txt"') do set LINE=%%a
set COLS=0
for %%x in (!LINE!) do set /a COLS+=1
if "!COLS!"=="4" (echo   [PASS] Test 13: -lcw 4 columns & set /a PASS+=1) else (echo   [FAIL] Test 13: -lcw cols=!COLS! & set /a FAIL+=1)

:: Test 14: -clmwL shows 6 columns (5 counts + filename)
"%OUTPUT%" -clmwL "%TDIR%\f1.txt" > "%TDIR%\t14.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t14.txt"') do set LINE=%%a
set COLS=0
for %%x in (!LINE!) do set /a COLS+=1
if "!COLS!"=="6" (echo   [PASS] Test 14: -clmwL 6 columns & set /a PASS+=1) else (echo   [FAIL] Test 14: cols=!COLS! & set /a FAIL+=1)

:: Test 15: --lines long option
"%OUTPUT%" --lines "%TDIR%\f2.txt" > "%TDIR%\t15.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t15.txt"') do set VAL=%%a
if "!VAL!"=="3" (echo   [PASS] Test 15: --lines f2.txt=3 & set /a PASS+=1) else (echo   [FAIL] Test 15: --lines=!VAL! & set /a FAIL+=1)

:: Test 16: --words long option
"%OUTPUT%" --words "%TDIR%\f5.txt" > "%TDIR%\t16.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t16.txt"') do set VAL=%%a
if "!VAL!"=="5" (echo   [PASS] Test 16: --words f5.txt=5 & set /a PASS+=1) else (echo   [FAIL] Test 16: --words=!VAL! & set /a FAIL+=1)

:: Test 17: --bytes long option (13 = hello world + CRLF)
"%OUTPUT%" --bytes "%TDIR%\f1.txt" > "%TDIR%\t17.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t17.txt"') do set VAL=%%a
if "!VAL!"=="13" (echo   [PASS] Test 17: --bytes f1.txt=13 & set /a PASS+=1) else (echo   [FAIL] Test 17: --bytes=!VAL! & set /a FAIL+=1)

:: Test 18: --chars on ASCII equals bytes
"%OUTPUT%" -c "%TDIR%\f1.txt" > "%TDIR%\t18a.txt" 2>&1
"%OUTPUT%" -m "%TDIR%\f1.txt" > "%TDIR%\t18b.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t18a.txt"') do set BYTES=%%a
for /f "tokens=1" %%a in ('type "%TDIR%\t18b.txt"') do set CHARS=%%a
if "!BYTES!"=="!CHARS!" (echo   [PASS] Test 18: -m=-c on ASCII & set /a PASS+=1) else (echo   [FAIL] Test 18: -m=!CHARS! -c=!BYTES! & set /a FAIL+=1)

:: Test 19: --max-line-length long option (25 = 24 a's + CR)
"%OUTPUT%" --max-line-length "%TDIR%\longline.txt" > "%TDIR%\t19.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t19.txt"') do set VAL=%%a
if "!VAL!"=="25" (echo   [PASS] Test 19: --max-line-length=25 & set /a PASS+=1) else (echo   [FAIL] Test 19: =!VAL! & set /a FAIL+=1)

:: Test 20: - reads from stdin (13 = f1.txt with CRLF)
"%OUTPUT%" -c - < "%TDIR%\f1.txt" > "%TDIR%\t20.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t20.txt"') do set VAL=%%a
if "!VAL!"=="13" (echo   [PASS] Test 20: - reads stdin=13 & set /a PASS+=1) else (echo   [FAIL] Test 20: =!VAL! & set /a FAIL+=1)

:: Test 21: Empty file -l is 0
"%OUTPUT%" -l "%TDIR%\empty.txt" > "%TDIR%\t21.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t21.txt"') do set VAL=%%a
if "!VAL!"=="0" (echo   [PASS] Test 21: -l empty=0 & set /a PASS+=1) else (echo   [FAIL] Test 21: =!VAL! & set /a FAIL+=1)

:: Test 22: Unknown long option errors
"%OUTPUT%" --unknown > nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 22: unknown long option errors & set /a PASS+=1) else (echo   [FAIL] Test 22 & set /a FAIL+=1)

:: Test 23: Unknown short option errors
"%OUTPUT%" -Z > nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS] Test 23: unknown short option errors & set /a PASS+=1) else (echo   [FAIL] Test 23 & set /a FAIL+=1)

:: Test 24: -w on f5.txt is 5
"%OUTPUT%" -w "%TDIR%\f5.txt" > "%TDIR%\t24.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t24.txt"') do set VAL=%%a
if "!VAL!"=="5" (echo   [PASS] Test 24: -w f5.txt=5 & set /a PASS+=1) else (echo   [FAIL] Test 24: -w=!VAL! & set /a FAIL+=1)

:: Test 25: -l on f2.txt is 3
"%OUTPUT%" -l "%TDIR%\f2.txt" > "%TDIR%\t25.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t25.txt"') do set VAL=%%a
if "!VAL!"=="3" (echo   [PASS] Test 25: -l f2.txt=3 & set /a PASS+=1) else (echo   [FAIL] Test 25: -l=!VAL! & set /a FAIL+=1)

:: Test 26: Multiple files total lines
"%OUTPUT%" -l "%TDIR%\f1.txt" "%TDIR%\f2.txt" "%TDIR%\f5.txt" > "%TDIR%\t26.txt" 2>&1
findstr /c:"total" "%TDIR%\t26.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 26: total line present & set /a PASS+=1) else (echo   [FAIL] Test 26 & set /a FAIL+=1)

:: Test 27: -c on nonl.txt is 10 (no trailing newline)
"%OUTPUT%" -c "%TDIR%\nonl.txt" > "%TDIR%\t27.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t27.txt"') do set VAL=%%a
if "!VAL!"=="10" (echo   [PASS] Test 27: -c nonl=10 & set /a PASS+=1) else (echo   [FAIL] Test 27: -c=!VAL! & set /a FAIL+=1)

:: Test 28: -L on f2.txt is 6 (5 chars + CR)
"%OUTPUT%" -L "%TDIR%\f2.txt" > "%TDIR%\t28.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t28.txt"') do set VAL=%%a
if "!VAL!"=="6" (echo   [PASS] Test 28: -L f2=6 & set /a PASS+=1) else (echo   [FAIL] Test 28: -L=!VAL! & set /a FAIL+=1)

:: Test 29: --help exits 0
"%OUTPUT%" --help > nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 29: --help exit 0 & set /a PASS+=1) else (echo   [FAIL] Test 29 & set /a FAIL+=1)

:: Test 30: --version exits 0
"%OUTPUT%" --version > nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 30: --version exit 0 & set /a PASS+=1) else (echo   [FAIL] Test 30 & set /a FAIL+=1)

:: Test 31: --files0-from runs
"%OUTPUT%" --files0-from=- < nul > nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS] Test 31: --files0-from runs & set /a PASS+=1) else (echo   [FAIL] Test 31 & set /a FAIL+=1)

:: Test 32: Default mode words=2
"%OUTPUT%" "%TDIR%\f1.txt" > "%TDIR%\t32.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t32.txt"') do set LINE=%%a
set IDX=0
for %%x in (!LINE!) do (
    set /a IDX+=1
    if !IDX!==2 set VAL=%%x
)
if "!VAL!"=="2" (echo   [PASS] Test 32: default words=2 & set /a PASS+=1) else (echo   [FAIL] Test 32: words=!VAL! & set /a FAIL+=1)

:: Test 33: Default mode bytes=13 (hello world + CRLF)
"%OUTPUT%" "%TDIR%\f1.txt" > "%TDIR%\t33.txt" 2>&1
set LINE=
for /f "delims=" %%a in ('type "%TDIR%\t33.txt"') do set LINE=%%a
set IDX=0
for %%x in (!LINE!) do (
    set /a IDX+=1
    if !IDX!==3 set VAL=%%x
)
if "!VAL!"=="13" (echo   [PASS] Test 33: default bytes=13 & set /a PASS+=1) else (echo   [FAIL] Test 33: bytes=!VAL! & set /a FAIL+=1)

:: Test 34: -w on empty is 0
"%OUTPUT%" -w "%TDIR%\empty.txt" > "%TDIR%\t34.txt" 2>&1
for /f "tokens=1" %%a in ('type "%TDIR%\t34.txt"') do set VAL=%%a
if "!VAL!"=="0" (echo   [PASS] Test 34: -w empty=0 & set /a PASS+=1) else (echo   [FAIL] Test 34: -w=!VAL! & set /a FAIL+=1)

:: Cleanup
if exist "%TDIR%" rmdir /s /q "%TDIR%"

echo.
echo ============================================
echo   Test Results: PASS=!PASS!  FAIL=!FAIL!
echo ============================================

if !FAIL! equ 0 (
    echo   All tests passed!
) else (
    echo   Some tests failed!
    exit /b 1
)
