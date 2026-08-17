@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     rm.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=rm.exe
set SOURCE=rm.c
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

set TDIR=%TEMP%\rm_ftest_%random%
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"
if not exist "%TDIR%" (
    echo [ERROR] Cannot create test directory: %TDIR%
    exit /b 1
)

REM ========== Test 1: Basic file removal
echo.
echo --- T01: Basic file removal ---
echo Hello> "%TDIR%\f1.txt"
"%OUTPUT%" "%TDIR%\f1.txt" >nul 2>&1
if not exist "%TDIR%\f1.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 2: Multiple file removal
echo.
echo --- T02: Multiple files ---
echo A> "%TDIR%\a.txt" & echo B> "%TDIR%\b.txt" & echo C> "%TDIR%\c.txt"
"%OUTPUT%" "%TDIR%\a.txt" "%TDIR%\b.txt" "%TDIR%\c.txt" >nul 2>&1
set ok=1
if exist "%TDIR%\a.txt" set ok=0
if exist "%TDIR%\b.txt" set ok=0
if exist "%TDIR%\c.txt" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 3: -v verbose
echo.
echo --- T03: -v verbose ---
echo V> "%TDIR%\v.txt"
"%OUTPUT%" -v "%TDIR%\v.txt" > "%TDIR%\vout.txt" 2>&1
findstr /C:"removed" "%TDIR%\vout.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 4: --verbose long
echo.
echo --- T04: --verbose long ---
echo VL> "%TDIR%\vl.txt"
"%OUTPUT%" --verbose "%TDIR%\vl.txt" > "%TDIR%\vlout.txt" 2>&1
findstr /C:"removed" "%TDIR%\vlout.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 5: -f force nonexistent
echo.
echo --- T05: -f force nonexistent ---
"%OUTPUT%" -f "%TDIR%\noexist.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 6: --force long
echo.
echo --- T06: --force long ---
"%OUTPUT%" --force "%TDIR%\noexist2.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 7: Nonexistent without -f errors
echo.
echo --- T07: Nonexistent errors ---
"%OUTPUT%" "%TDIR%\noexist3.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 8: -f cancels -i
echo.
echo --- T08: -f cancels -i ---
echo FCI> "%TDIR%\fci.txt"
echo n | "%OUTPUT%" -if "%TDIR%\fci.txt" >nul 2>&1
if not exist "%TDIR%\fci.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 9: -d empty dir
echo.
echo --- T09: -d empty dir ---
mkdir "%TDIR%\emptydir"
"%OUTPUT%" -d "%TDIR%\emptydir" >nul 2>&1
if not exist "%TDIR%\emptydir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 10: --dir long
echo.
echo --- T10: --dir long ---
mkdir "%TDIR%\emptydir2"
"%OUTPUT%" --dir "%TDIR%\emptydir2" >nul 2>&1
if not exist "%TDIR%\emptydir2" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 11: -d non-empty errors
echo.
echo --- T11: -d non-empty errors ---
mkdir "%TDIR%\nonempty"
echo hi> "%TDIR%\nonempty\inner.txt"
"%OUTPUT%" -d "%TDIR%\nonempty" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
"%OUTPUT%" -rf "%TDIR%\nonempty" >nul 2>&1

REM ========== Test 12: dir without -r/-d errors
echo.
echo --- T12: dir without -r/-d errors ---
mkdir "%TDIR%\dironly"
"%OUTPUT%" "%TDIR%\dironly" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
"%OUTPUT%" -rf "%TDIR%\dironly" >nul 2>&1

REM ========== Test 13: -r recursive
echo.
echo --- T13: -r recursive ---
mkdir "%TDIR%\rdir\sub1\sub2"
echo F1> "%TDIR%\rdir\file1.txt"
echo F2> "%TDIR%\rdir\sub1\file2.txt"
echo F3> "%TDIR%\rdir\sub1\sub2\file3.txt"
"%OUTPUT%" -r "%TDIR%\rdir" >nul 2>&1
if not exist "%TDIR%\rdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 14: -R uppercase
echo.
echo --- T14: -R uppercase ---
mkdir "%TDIR%\Rdir\sub"
echo F> "%TDIR%\Rdir\f.txt"
"%OUTPUT%" -R "%TDIR%\Rdir" >nul 2>&1
if not exist "%TDIR%\Rdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 15: --recursive long
echo.
echo --- T15: --recursive long ---
mkdir "%TDIR%\recdir"
echo F> "%TDIR%\recdir\f.txt"
"%OUTPUT%" --recursive "%TDIR%\recdir" >nul 2>&1
if not exist "%TDIR%\recdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 16: -rf combined
echo.
echo --- T16: -rf combined ---
mkdir "%TDIR%\rfdir"
echo F> "%TDIR%\rfdir\f.txt"
"%OUTPUT%" -rf "%TDIR%\rfdir" >nul 2>&1
if not exist "%TDIR%\rfdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 17: -fr combined
echo.
echo --- T17: -fr combined ---
mkdir "%TDIR%\frdir"
echo F> "%TDIR%\frdir\f.txt"
"%OUTPUT%" -fr "%TDIR%\frdir" >nul 2>&1
if not exist "%TDIR%\frdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 18: -rv verbose recursive
echo.
echo --- T18: -rv verbose recursive ---
mkdir "%TDIR%\rvdir"
echo F> "%TDIR%\rvdir\f.txt"
"%OUTPUT%" -rv "%TDIR%\rvdir" > "%TDIR%\rvout.txt" 2>&1
findstr /C:"removed" "%TDIR%\rvout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if not exist "%TDIR%\rvdir" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 19: Read-only file
echo.
echo --- T19: Read-only file ---
echo RO> "%TDIR%\ro.txt"
attrib +r "%TDIR%\ro.txt"
"%OUTPUT%" "%TDIR%\ro.txt" >nul 2>&1
if not exist "%TDIR%\ro.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 20: -i yes
echo.
echo --- T20: -i yes ---
echo IY> "%TDIR%\iy.txt"
echo y | "%OUTPUT%" -i "%TDIR%\iy.txt" >nul 2>&1
if not exist "%TDIR%\iy.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 21: -i no keeps
echo.
echo --- T21: -i no keeps ---
echo IN> "%TDIR%\in.txt"
echo n | "%OUTPUT%" -i "%TDIR%\in.txt" >nul 2>&1
if exist "%TDIR%\in.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
"%OUTPUT%" -f "%TDIR%\in.txt" >nul 2>&1

REM ========== Test 22: --interactive=always
echo.
echo --- T22: --interactive=always ---
echo IA> "%TDIR%\ialways.txt"
echo y | "%OUTPUT%" --interactive=always "%TDIR%\ialways.txt" >nul 2>&1
if not exist "%TDIR%\ialways.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 23: --interactive=never
echo.
echo --- T23: --interactive=never ---
echo INE> "%TDIR%\inever.txt"
"%OUTPUT%" --interactive=never "%TDIR%\inever.txt" >nul 2>&1
if not exist "%TDIR%\inever.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 24: --interactive=invalid errors
echo.
echo --- T24: --interactive=invalid errors ---
"%OUTPUT%" --interactive=invalid "%TDIR%\x.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 25: --interactive (no value = always)
echo.
echo --- T25: --interactive no value ---
echo IAL> "%TDIR%\ival.txt"
echo y | "%OUTPUT%" --interactive "%TDIR%\ival.txt" >nul 2>&1
if not exist "%TDIR%\ival.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 26: -I yes
echo.
echo --- T26: -I yes (>3 files) ---
echo I1> "%TDIR%\i1.txt"
echo I2> "%TDIR%\i2.txt"
echo I3> "%TDIR%\i3.txt"
echo I4> "%TDIR%\i4.txt"
echo y | "%OUTPUT%" -I "%TDIR%\i1.txt" "%TDIR%\i2.txt" "%TDIR%\i3.txt" "%TDIR%\i4.txt" >nul 2>&1
set ok=1
if exist "%TDIR%\i1.txt" set ok=0
if exist "%TDIR%\i2.txt" set ok=0
if exist "%TDIR%\i3.txt" set ok=0
if exist "%TDIR%\i4.txt" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 27: -I no keeps
echo.
echo --- T27: -I no keeps (>3 files) ---
echo J1> "%TDIR%\j1.txt"
echo J2> "%TDIR%\j2.txt"
echo J3> "%TDIR%\j3.txt"
echo J4> "%TDIR%\j4.txt"
echo n | "%OUTPUT%" -I "%TDIR%\j1.txt" "%TDIR%\j2.txt" "%TDIR%\j3.txt" "%TDIR%\j4.txt" >nul 2>&1
set ok=1
if not exist "%TDIR%\j1.txt" set ok=0
if not exist "%TDIR%\j2.txt" set ok=0
if not exist "%TDIR%\j3.txt" set ok=0
if not exist "%TDIR%\j4.txt" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
"%OUTPUT%" -f "%TDIR%\j1.txt" "%TDIR%\j2.txt" "%TDIR%\j3.txt" "%TDIR%\j4.txt" >nul 2>&1

REM ========== Test 28: -I no prompt <=3
echo.
echo --- T28: -I no prompt <=3 files ---
echo K1> "%TDIR%\k1.txt"
echo K2> "%TDIR%\k2.txt"
echo K3> "%TDIR%\k3.txt"
"%OUTPUT%" -I "%TDIR%\k1.txt" "%TDIR%\k2.txt" "%TDIR%\k3.txt" <nul >nul 2>&1
set ok=1
if exist "%TDIR%\k1.txt" set ok=0
if exist "%TDIR%\k2.txt" set ok=0
if exist "%TDIR%\k3.txt" set ok=0
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 29: -I with -r prompts
echo.
echo --- T29: -I with -r prompts ---
mkdir "%TDIR%\iRdir"
echo F> "%TDIR%\iRdir\f.txt"
echo y | "%OUTPUT%" -Ir "%TDIR%\iRdir" >nul 2>&1
if not exist "%TDIR%\iRdir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 30: --interactive=once
echo.
echo --- T30: --interactive=once ---
mkdir "%TDIR%\ionce"
echo F1> "%TDIR%\ionce\f1.txt"
echo F2> "%TDIR%\ionce\f2.txt"
echo F3> "%TDIR%\ionce\f3.txt"
echo F4> "%TDIR%\ionce\f4.txt"
echo y | "%OUTPUT%" --interactive=once -r "%TDIR%\ionce" >nul 2>&1
if not exist "%TDIR%\ionce" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 31: --version
echo.
echo --- T31: --version ---
"%OUTPUT%" --version 2>&1 | findstr "1.0.0" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 32: --help
echo.
echo --- T32: --help ---
"%OUTPUT%" --help 2>&1 | findstr "Usage:" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 33: -h help
echo.
echo --- T33: -h help ---
"%OUTPUT%" -h 2>&1 | findstr "Usage:" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 34: Root protection
echo.
echo --- T34: Root protection ---
"%OUTPUT%" / >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 35: --preserve-root
echo.
echo --- T35: --preserve-root ---
"%OUTPUT%" --preserve-root / >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 36: --no-preserve-root
echo.
echo --- T36: --no-preserve-root ---
"%OUTPUT%" --no-preserve-root / 2> "%TDIR%\nopres.txt" >nul
findstr /C:"dangerous" "%TDIR%\nopres.txt" >nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 37: --preserve-root=all
echo.
echo --- T37: --preserve-root=all ---
echo PA> "%TDIR%\pa.txt"
"%OUTPUT%" --preserve-root=all "%TDIR%\pa.txt" >nul 2>&1
if not exist "%TDIR%\pa.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 38: --preserve-root=invalid
echo.
echo --- T38: --preserve-root=invalid ---
"%OUTPUT%" --preserve-root=invalid "%TDIR%\x.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 39: Refuse '.'
echo.
echo --- T39: Refuse '.' ---
"%OUTPUT%" . 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 40: Refuse '..'
echo.
echo --- T40: Refuse '..' ---
"%OUTPUT%" .. 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 41: Refusal message for './.'
echo.
echo --- T41: Refusal message './.' ---
"%OUTPUT%" ./. 2> "%TDIR%\dotmsg.txt" >nul
findstr /C:"refusing" "%TDIR%\dotmsg.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 42: Missing operand
echo.
echo --- T42: Missing operand ---
"%OUTPUT%" 2>nul
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 43: -f missing operand OK
echo.
echo --- T43: -f missing operand OK ---
"%OUTPUT%" -f 2>nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 44: Invalid option
echo.
echo --- T44: Invalid option ---
"%OUTPUT%" -Z "%TDIR%\x.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 45: -- separator
echo.
echo --- T45: -- separator ---
echo W> "%TDIR%\--weird.txt"
"%OUTPUT%" -- "%TDIR%\--weird.txt" >nul 2>&1
if not exist "%TDIR%\--weird.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 46: Deep nested
echo.
echo --- T46: Deep nested ---
mkdir "%TDIR%\deep\d1\d2\d3\d4"
echo F> "%TDIR%\deep\d1\d2\d3\d4\f.txt"
"%OUTPUT%" -rf "%TDIR%\deep" >nul 2>&1
if not exist "%TDIR%\deep" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 47: -ri interactive recursive
echo.
echo --- T47: -ri interactive recursive ---
mkdir "%TDIR%\idir"
echo F1> "%TDIR%\idir\f1.txt"
echo F2> "%TDIR%\idir\f2.txt"
(echo y & echo y & echo y) | "%OUTPUT%" -ri "%TDIR%\idir" >nul 2>&1
if not exist "%TDIR%\idir" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 48: --one-file-system
echo.
echo --- T48: --one-file-system ---
echo OFS> "%TDIR%\ofs.txt"
"%OUTPUT%" --one-file-system "%TDIR%\ofs.txt" >nul 2>&1
if not exist "%TDIR%\ofs.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 49: Mixed long options
echo.
echo --- T49: Mixed long options ---
mkdir "%TDIR%\mixdir"
echo F> "%TDIR%\mixdir\f.txt"
"%OUTPUT%" --recursive --force --verbose "%TDIR%\mixdir" > "%TDIR%\mixout.txt" 2>&1
findstr /C:"removed" "%TDIR%\mixout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if not exist "%TDIR%\mixdir" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== Test 50: Nested verbose dir
echo.
echo --- T50: Nested verbose dir ---
mkdir "%TDIR%\vdr\sub"
echo F> "%TDIR%\vdr\sub\file.txt"
"%OUTPUT%" -rv "%TDIR%\vdr" > "%TDIR%\vdrout.txt" 2>&1
findstr /C:"removed directory" "%TDIR%\vdrout.txt" >nul
set ok=0
if !ERRORLEVEL! equ 0 if not exist "%TDIR%\vdr" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

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
