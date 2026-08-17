@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     mv.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=mv.exe
set SOURCE=mv.c
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

set TDIR=%TEMP%\mv_ftest_%random%
if exist "%TDIR%" rmdir /s /q "%TDIR%"
mkdir "%TDIR%"

REM ========== T01: Basic file move (rename)
echo.
echo --- T01: Basic file move ---
echo test> "%TDIR%\f1.txt"
"%OUTPUT%" "%TDIR%\f1.txt" "%TDIR%\f2.txt" >nul 2>&1
set ok=0
if not exist "%TDIR%\f1.txt" if exist "%TDIR%\f2.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: Multiple files to directory
echo.
echo --- T02: Multiple files to directory ---
echo a> "%TDIR%\a.txt"
echo b> "%TDIR%\b.txt"
mkdir "%TDIR%\d1"
"%OUTPUT%" "%TDIR%\a.txt" "%TDIR%\b.txt" "%TDIR%\d1" >nul 2>&1
set ok=0
if exist "%TDIR%\d1\a.txt" if exist "%TDIR%\d1\b.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: Directory move
echo.
echo --- T03: Directory move ---
mkdir "%TDIR%\srcdir"
echo c> "%TDIR%\srcdir\c.txt"
mkdir "%TDIR%\destdir"
"%OUTPUT%" "%TDIR%\srcdir" "%TDIR%\destdir" >nul 2>&1
set ok=0
if exist "%TDIR%\destdir\srcdir\c.txt" if not exist "%TDIR%\srcdir" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -v verbose
echo.
echo --- T04: -v verbose ---
echo v> "%TDIR%\vf.txt"
"%OUTPUT%" -v "%TDIR%\vf.txt" "%TDIR%\vf2.txt" > "%TDIR%\vout.txt" 2>&1
findstr /c:"moving" "%TDIR%\vout.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: --verbose long
echo.
echo --- T05: --verbose long ---
echo v2> "%TDIR%\v2f.txt"
"%OUTPUT%" --verbose "%TDIR%\v2f.txt" "%TDIR%\v2f2.txt" > "%TDIR%\v2out.txt" 2>&1
findstr /c:"moving" "%TDIR%\v2out.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -f force overwrite
echo.
echo --- T06: -f force overwrite ---
echo old> "%TDIR%\over.txt"
echo new> "%TDIR%\newf.txt"
"%OUTPUT%" -f "%TDIR%\newf.txt" "%TDIR%\over.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\over.txt" if not exist "%TDIR%\newf.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: --force long
echo.
echo --- T07: --force long ---
echo old2> "%TDIR%\over2.txt"
echo new2> "%TDIR%\newf2.txt"
"%OUTPUT%" --force "%TDIR%\newf2.txt" "%TDIR%\over2.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\over2.txt" if not exist "%TDIR%\newf2.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -n no clobber
echo.
echo --- T08: -n no clobber ---
echo keep> "%TDIR%\keep.txt"
echo replace> "%TDIR%\repl.txt"
"%OUTPUT%" -n "%TDIR%\repl.txt" "%TDIR%\keep.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\keep.txt" if exist "%TDIR%\repl.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: --no-clobber long
echo.
echo --- T09: --no-clobber long ---
echo keep2> "%TDIR%\keep2.txt"
echo replace2> "%TDIR%\repl2.txt"
"%OUTPUT%" --no-clobber "%TDIR%\repl2.txt" "%TDIR%\keep2.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\keep2.txt" if exist "%TDIR%\repl2.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T10: -u update (dest older)
echo.
echo --- T10: -u update dest older ---
echo oldcontent> "%TDIR%\oldf.txt"
ping -n 2 -w 1000 127.0.0.1 >nul
echo newcontent> "%TDIR%\newf.txt"
"%OUTPUT%" -u "%TDIR%\newf.txt" "%TDIR%\oldf.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\oldf.txt" if not exist "%TDIR%\newf.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -u update (dest newer - skip)
echo.
echo --- T11: -u update dest newer ---
echo oldcontent> "%TDIR%\oldf2.txt"
ping -n 2 -w 1000 127.0.0.1 >nul
echo newcontent> "%TDIR%\newf2.txt"
"%OUTPUT%" -u "%TDIR%\oldf2.txt" "%TDIR%\newf2.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\oldf2.txt" if exist "%TDIR%\newf2.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T12: --update long
echo.
echo --- T12: --update long ---
echo old3> "%TDIR%\oldf3.txt"
ping -n 2 -w 1000 127.0.0.1 >nul
echo new3> "%TDIR%\newf3.txt"
"%OUTPUT%" --update "%TDIR%\newf3.txt" "%TDIR%\oldf3.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\oldf3.txt" if not exist "%TDIR%\newf3.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: -b backup
echo.
echo --- T13: -b backup ---
echo orig> "%TDIR%\orig.txt"
echo new> "%TDIR%\newb.txt"
"%OUTPUT%" -b "%TDIR%\newb.txt" "%TDIR%\orig.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\orig.txt" if exist "%TDIR%\orig.txt~" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: --backup long
echo.
echo --- T14: --backup long ---
echo orig2> "%TDIR%\orig2.txt"
echo new2> "%TDIR%\newb2.txt"
"%OUTPUT%" --backup "%TDIR%\newb2.txt" "%TDIR%\orig2.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\orig2.txt" if exist "%TDIR%\orig2.txt~" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: --help
echo.
echo --- T15: --help ---
"%OUTPUT%" --help > "%TDIR%\help.txt"
findstr /c:"Usage:" "%TDIR%\help.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: --version
echo.
echo --- T16: --version ---
"%OUTPUT%" --version > "%TDIR%\ver.txt"
findstr /c:"1.0.0" "%TDIR%\ver.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T17: -h help
echo.
echo --- T17: -h help ---
"%OUTPUT%" -h > "%TDIR%\hhelp.txt"
findstr /c:"Usage:" "%TDIR%\hhelp.txt" >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: Missing operand
echo.
echo --- T18: Missing operand ---
"%OUTPUT%" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: Invalid option
echo.
echo --- T19: Invalid option ---
"%OUTPUT%" -X "%TDIR%\x" "%TDIR%\y" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -- separator
echo.
echo --- T20: -- separator ---
echo sep> "%TDIR%\sep.txt"
"%OUTPUT%" -- "%TDIR%\sep.txt" "%TDIR%\sep2.txt" >nul 2>&1
if exist "%TDIR%\sep2.txt" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T21: Cannot move to itself
echo.
echo --- T21: Cannot move to itself ---
echo self> "%TDIR%\self.txt"
"%OUTPUT%" "%TDIR%\self.txt" "%TDIR%\self.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T22: Cannot move dir to file
echo.
echo --- T22: Cannot move dir to file ---
mkdir "%TDIR%\dir2file"
echo content> "%TDIR%\targetfile.txt"
"%OUTPUT%" "%TDIR%\dir2file" "%TDIR%\targetfile.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: Deep directory move
echo.
echo --- T23: Deep directory move ---
mkdir "%TDIR%\deep\a\b\c"
echo d> "%TDIR%\deep\a\b\c\d.txt"
mkdir "%TDIR%\destdeep"
"%OUTPUT%" "%TDIR%\deep" "%TDIR%\destdeep" >nul 2>&1
set ok=0
if exist "%TDIR%\destdeep\deep\a\b\c\d.txt" if not exist "%TDIR%\deep" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: -f cancels -i (if order)
echo.
echo --- T24: -f cancels -i ---
echo fi> "%TDIR%\fi.txt"
echo newfi> "%TDIR%\newfi.txt"
"%OUTPUT%" -if "%TDIR%\newfi.txt" "%TDIR%\fi.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\fi.txt" if not exist "%TDIR%\newfi.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: -n cancels -f
echo.
echo --- T25: -n cancels -f ---
echo nf> "%TDIR%\nf.txt"
echo newnf> "%TDIR%\newnf.txt"
"%OUTPUT%" -fn "%TDIR%\newnf.txt" "%TDIR%\nf.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\nf.txt" if exist "%TDIR%\newnf.txt" set ok=1
if !ok! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T26: File does not exist
echo.
echo --- T26: File does not exist ---
"%OUTPUT%" "%TDIR%\nonexistent.txt" "%TDIR%\dest.txt" >nul 2>&1
if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T27: -f ignores nonexistent
echo.
echo --- T27: -f ignores nonexistent ---
"%OUTPUT%" -f "%TDIR%\nonexistent2.txt" "%TDIR%\dest2.txt" >nul 2>&1
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T28: -u with missing dest
echo.
echo --- T28: -u with missing dest ---
echo umiss> "%TDIR%\umiss.txt"
"%OUTPUT%" -u "%TDIR%\umiss.txt" "%TDIR%\unew.txt" >nul 2>&1
set ok=0
if exist "%TDIR%\unew.txt" if not exist "%TDIR%\umiss.txt" set ok=1
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
