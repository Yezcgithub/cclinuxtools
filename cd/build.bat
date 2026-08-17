@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     cd.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=cd.exe
set SOURCE=cd.c

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
echo   Running basic tests...
echo ============================================

set TDIR=_build_test
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"
mkdir "%TDIR%\sub1"
mkdir "%TDIR%\sub2"

set EXE=%CD%\%OUTPUT%

echo.
echo --- Test 1: help ---
"%EXE%" --help > "%TDIR%\t1.txt" 2>&1
findstr /c:"Usage" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 2: version ---
"%EXE%" --version > "%TDIR%\t2.txt" 2>&1
findstr /c:"1.0.0" "%TDIR%\t2.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 3: relative dir via sub1 ---
pushd "%TDIR%"
"%EXE%" sub1 > "t3.txt" 2>&1
set RC=!ERRORLEVEL!
findstr /c:"/sub1" "t3.txt" >nul
if !ERRORLEVEL! equ 0 set M=1
if %RC% equ 0 if defined M (echo   [PASS]) else (echo   [FAIL])
set M=
popd

echo.
echo --- Test 4: absolute target ---
pushd "%TDIR%"
set "ABSDIR=%CD:\=/%"
"%EXE%" "%ABSDIR%/sub2" > "t4.txt" 2>&1
set RC=!ERRORLEVEL!
findstr /c:"/sub2" "t4.txt" >nul
if !ERRORLEVEL! equ 0 set M=1
if %RC% equ 0 if defined M (echo   [PASS]) else (echo   [FAIL])
set M=
popd

echo.
echo --- Test 5: tilde to HOME ---
"%EXE%" "~" > "%TDIR%\t5.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 6: -P physical mode ---
pushd "%TDIR%"
"%EXE%" -P sub1 > "t6.txt" 2>&1
set RC=!ERRORLEVEL!
findstr /c:"/sub1" "t6.txt" >nul
if !ERRORLEVEL! equ 0 set M=1
if %RC% equ 0 if defined M (echo   [PASS]) else (echo   [FAIL])
set M=
popd

echo.
echo --- Test 7: -e option accepted ---
pushd "%TDIR%"
"%EXE%" -e -P sub1 >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])
popd

echo.
echo --- Test 8: not a directory ---
"%EXE%" "%TDIR%\t1.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 9: nonexistent dir ---
"%EXE%" "%TDIR%/no_such_dir_xyz" >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 10: unknown short option returns 2 ---
"%EXE%" -Z >nul 2>&1
if !ERRORLEVEL! equ 2 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 11: unknown long option returns 2 ---
"%EXE%" --nosuchoption >nul 2>&1
if !ERRORLEVEL! equ 2 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 12: logical collapse ".." ---
pushd "%TDIR%"
"%EXE%" "sub1/../sub2" > "t12.txt" 2>&1
set RC=!ERRORLEVEL!
findstr /c:"/sub2" "t12.txt" >nul
if !ERRORLEVEL! equ 0 set M=1
findstr /c:".." "t12.txt" >nul
if !ERRORLEVEL! neq 0 set N=1
if %RC% equ 0 if defined M if defined N (echo   [PASS]) else (echo   [FAIL])
set M=
set N=
popd

echo.
echo --- Test 13: "." stays in same dir ---
pushd "%TDIR%"
set "BEFORE=%CD%"
"%EXE%" "." > "t13.txt" 2>&1
set RC=!ERRORLEVEL!
set "AFTER=%CD%"
if %RC% equ 0 if "!BEFORE!"=="!AFTER!" (echo   [PASS]) else (echo   [FAIL])
popd

echo.
echo --- Test 14: last flag wins (LP = -P) ---
pushd "%TDIR%"
"%EXE%" -L -P sub1 > "t14a.txt" 2>&1
"%EXE%" -P sub1 > "t14b.txt" 2>&1
fc /b "t14a.txt" "t14b.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])
popd

echo.
echo --- Test 15: last flag wins (PL = -L) ---
pushd "%TDIR%"
"%EXE%" -P -L "sub1/../sub2" > "t15a.txt" 2>&1
findstr /c:"/sub2" "t15a.txt" >nul
if !ERRORLEVEL! equ 0 set M=1
findstr /c:".." "t15a.txt" >nul
if !ERRORLEVEL! neq 0 set N=1
if defined M if defined N (echo   [PASS]) else (echo   [FAIL])
set M=
set N=
popd

rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Build and test complete!
echo ============================================
