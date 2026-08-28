@echo off
REM Build and test script for realpath.c (Windows)

echo ============================================
echo     realpath.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall
set OUTPUT=realpath.exe
set SOURCE=realpath.c

echo.
echo [1/3] Cleaning previous build...
if exist %OUTPUT% del /F /Q %OUTPUT%

echo.
echo [2/3] Compiling %SOURCE%...
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE%
echo.
%CC% %CFLAGS% -o %OUTPUT% %SOURCE%
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%

echo.
echo ============================================
echo   Running full functional tests...
echo ============================================
powershell -NoProfile -ExecutionPolicy Bypass -File _run_tests.ps1
set TEST_EXIT=%ERRORLEVEL%

echo.
if %TEST_EXIT%==0 (
    echo All tests passed!
) else (
    echo Some tests failed (exit code: %TEST_EXIT%^)
)

exit /b %TEST_EXIT%
