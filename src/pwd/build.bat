@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     pwd.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=pwd.exe
set SOURCE=pwd.c

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

:: Setup test directory
set TDIR=_build_test
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"

echo.
echo --- Test 1: Basic pwd ---
"%OUTPUT%" > "%TDIR%\t1.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 2: Physical mode (-P, default) ---
"%OUTPUT%" -P > "%TDIR%\t2.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 3: Logical mode (-L) ---
"%OUTPUT%" -L > "%TDIR%\t3.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 4: Output contains drive letter (non-UNC) ---
findstr /R "^[A-Za-z]:" "%TDIR%\t1.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 5: Output uses forward slashes (POSIX style) ---
findstr /c:"/" "%TDIR%\t1.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 6: No trailing backslash ---
type "%TDIR%\t1.txt" | findstr /R "\\$" >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 7: -P and default outputs match ---
fc /b "%TDIR%\t1.txt" "%TDIR%\t2.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 8: Logical mode w/ explicit PWD env ---
set "BASEDIR=%CD%"
pushd "%TDIR%"
set "PWD=%CD:\=/%"
"..\%OUTPUT%" -L > "t8.txt" 2>&1
set RC=!ERRORLEVEL!
findstr /C:"%PWD%" "t8.txt" >nul
if !ERRORLEVEL! equ 0 set MATCH=1
if %RC% equ 0 if defined MATCH (echo   [PASS]) else (echo   [FAIL])
popd
set MATCH=
set BASEDIR=

echo.
echo --- Test 9: Help ---
"%OUTPUT%" --help > "%TDIR%\t9.txt"
findstr /c:"Usage" "%TDIR%\t9.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 10: Version ---
"%OUTPUT%" --version > "%TDIR%\t10.txt"
findstr /c:"1.0.0" "%TDIR%\t10.txt" >nul
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 11: Last option wins (-L then -P = physical) ---
"%OUTPUT%" -L -P > "%TDIR%\t11a.txt" 2>&1
"%OUTPUT%" -P > "%TDIR%\t11b.txt" 2>&1
fc /b "%TDIR%\t11a.txt" "%TDIR%\t11b.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 12: Long options --logical / --physical ---
"%OUTPUT%" --logical > "%TDIR%\t12a.txt" 2>&1
"%OUTPUT%" --physical > "%TDIR%\t12b.txt" 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 13: Error on unknown option ---
"%OUTPUT%" --thisoptiondoesnotexist >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 14: Error on unknown short option ---
"%OUTPUT%" -Z >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 15: Extra operand error ---
"%OUTPUT%" foo >nul 2>&1
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

:: Cleanup
rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Build and test complete!
echo ============================================
