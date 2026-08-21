@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     cp.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=cp.exe
set SOURCE=cp.c

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
echo --- Test 1: Basic file copy ---
echo Hello World> "%TDIR%\src.txt"
"%OUTPUT%" "%TDIR%\src.txt" "%TDIR%\dst.txt"
fc "%TDIR%\src.txt" "%TDIR%\dst.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 2: Verbose mode ---
"%OUTPUT%" -v "%TDIR%\src.txt" "%TDIR%\dst_v.txt" >nul 2>&1
fc "%TDIR%\src.txt" "%TDIR%\dst_v.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 3: Copy to directory ---
mkdir "%TDIR%\subdir" 2>nul
"%OUTPUT%" "%TDIR%\src.txt" "%TDIR%\subdir" >nul 2>&1
if exist "%TDIR%\subdir\src.txt" (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 4: No-clobber (-n) ---
echo OldContent> "%TDIR%\noclobber.txt"
"%OUTPUT%" -n "%TDIR%\src.txt" "%TDIR%\noclobber.txt" >nul 2>&1
findstr "Old" "%TDIR%\noclobber.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 5: Force (-f) ---
echo Old> "%TDIR%\force.txt"
"%OUTPUT%" -f "%TDIR%\src.txt" "%TDIR%\force.txt" >nul 2>&1
fc "%TDIR%\src.txt" "%TDIR%\force.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 6: Recursive copy (-r) ---
mkdir "%TDIR%\rsrc\sub" 2>nul
echo FileA> "%TDIR%\rsrc\a.txt"
echo FileB> "%TDIR%\rsrc\sub\b.txt"
"%OUTPUT%" -r "%TDIR%\rsrc" "%TDIR%\rdst" >nul 2>&1
if exist "%TDIR%\rdst\a.txt" if exist "%TDIR%\rdst\sub\b.txt" (
    echo   [PASS]
) else (
    echo   [FAIL]
)

echo.
echo --- Test 7: Archive mode (-a) ---
"%OUTPUT%" -a "%TDIR%\rsrc" "%TDIR%\adst" >nul 2>&1
if exist "%TDIR%\adst\a.txt" if exist "%TDIR%\adst\sub\b.txt" (
    echo   [PASS]
) else (
    echo   [FAIL]
)

echo.
echo --- Test 8: Target directory (-t) ---
mkdir "%TDIR%\target" 2>nul
"%OUTPUT%" -t "%TDIR%\target" "%TDIR%\src.txt" >nul 2>&1
if exist "%TDIR%\target\src.txt" (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 9: Multiple sources to dir ---
echo Extra1> "%TDIR%\extra1.txt"
echo Extra2> "%TDIR%\extra2.txt"
"%OUTPUT%" "%TDIR%\extra1.txt" "%TDIR%\extra2.txt" "%TDIR%\target" >nul 2>&1
if exist "%TDIR%\target\extra1.txt" if exist "%TDIR%\target\extra2.txt" (
    echo   [PASS]
) else (
    echo   [FAIL]
)

echo.
echo --- Test 10: Hard link (-l) ---
"%OUTPUT%" -l "%TDIR%\src.txt" "%TDIR%\hardlink.txt" >nul 2>&1
if exist "%TDIR%\hardlink.txt" (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 11: No-target-directory (-T) ---
"%OUTPUT%" -T "%TDIR%\src.txt" "%TDIR%\notdir.txt" >nul 2>&1
fc "%TDIR%\src.txt" "%TDIR%\notdir.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 12: Preserve (-p) ---
"%OUTPUT%" -p "%TDIR%\src.txt" "%TDIR%\preserved.txt" >nul 2>&1
fc "%TDIR%\src.txt" "%TDIR%\preserved.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 13: Version ---
"%OUTPUT%" --version 2>&1 | findstr "1.0.0" >nul 2>&1
if !ERRORLEVEL! equ 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 14: Error - no args ---
"%OUTPUT%" 2>nul
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

echo.
echo --- Test 15: Error - same file ---
"%OUTPUT%" "%TDIR%\src.txt" "%TDIR%\src.txt" 2>nul
if !ERRORLEVEL! neq 0 (echo   [PASS]) else (echo   [FAIL])

:: Cleanup
rmdir /s /q "%TDIR%" 2>nul

echo.
echo ============================================
echo   Build and test complete!
echo ============================================
