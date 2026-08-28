@echo off
rem Build and run regression tests for htop (Windows).
rem Usage: build.bat
rem NOTE: all subroutine labels must stay at the bottom of this file.

setlocal enabledelayedexpansion
cd /d "%~dp0"

set TDIR=%TEMP%\htop_test_%RANDOM%
if not exist "%TDIR%" mkdir "%TDIR%"
set PASS=0
set FAIL=0
set TOTAL=0

set OUT=htop.exe
set HTOP=%~dp0%OUT%

echo == 1. build ==
where gcc >nul 2>&1
if errorlevel 1 (
    echo error: gcc not found in PATH
    exit /b 1
)
set CCOPTS=-O2 -std=c99 -Wall -Wextra
set LDFLAGS=-lpsapi -ladvapi32

del /q "%OUT%" >nul 2>&1
gcc %CCOPTS% -o "%OUT%" htop.c %LDFLAGS% 2> "%TDIR%\build.log"
if errorlevel 1 (
    type "%TDIR%\build.log"
    echo error: build failed
    exit /b 1
)
call :ok "compiled without errors"
findstr /I "warning" "%TDIR%\build.log" >nul 2>&1
if errorlevel 1 (
    call :ok "no compiler warnings"
) else (
    call :fail "no compiler warnings"
)

echo == 2. version and help ==
"%HTOP%" --version > "%TDIR%\v.txt" 2>&1
set "CHECKCMD=%HTOP% --version"
call :check "htop --version exits 0"
call :checkgrep "version string present" "htop v" "%TDIR%\v.txt"
call :checkgrep "MIT license mentioned" "MIT" "%TDIR%\v.txt"

"%HTOP%" -V > "%TDIR%\v2.txt" 2>&1
call :checkgrep "-V alias works" "htop v" "%TDIR%\v2.txt"

"%HTOP%" --help > "%TDIR%\h.txt" 2>&1
call :checkgrep "help shows usage" "Usage: htop" "%TDIR%\h.txt"
call :checkgrep "help shows --batch" "--batch" "%TDIR%\h.txt"
call :checkgrep "help shows --delay" "--delay" "%TDIR%\h.txt"
call :checkgrep "help shows --filter" "--filter" "%TDIR%\h.txt"
call :checkgrep "help shows --sort-key" "--sort-key" "%TDIR%\h.txt"
call :checkgrep "help shows --tree" "--tree" "%TDIR%\h.txt"
call :checkgrep "help shows interactive keys" "F3" "%TDIR%\h.txt"

"%HTOP%" -h > "%TDIR%\h2.txt" 2>&1
call :checkgrep "-h alias works" "Usage: htop" "%TDIR%\h2.txt"

echo == 3. batch output (auto-batch when not a TTY) ==
"%HTOP%" -b -n 1 > "%TDIR%\t1.txt" 2>&1
call :checkgrep "header line 1 (htop/uptime)" "load average" "%TDIR%\t1.txt"
call :checkgrep "tasks line" "Tasks:" "%TDIR%\t1.txt"
call :checkgrep "CPU bar" "CPU" "%TDIR%\t1.txt"
call :checkgrep "Mem bar" "Mem:" "%TDIR%\t1.txt"
call :checkgrep "process table header PID" "PID" "%TDIR%\t1.txt"
call :checkgrep "process table header COMMAND" "COMMAND" "%TDIR%\t1.txt"
rem %% is a for-variable escape in batch files, so build the literal
rem percent sign with the standard %=% trick instead.
set "PCTCPU=%=%CPU"
set "PCTMEM=%=%MEM"
call :checkgrep "process table header percent CPU" "%PCTCPU%" "%TDIR%\t1.txt"
call :checkgrep "process table header percent MEM" "%PCTMEM%" "%TDIR%\t1.txt"
call :checkgrep "at least one process row" "COMMAND" "%TDIR%\t1.txt"

echo == 4. sorting ==
"%HTOP%" -b -n 1 -s PID > "%TDIR%\s1.txt" 2>&1
call :checkgrep "sort by PID accepted" "COMMAND" "%TDIR%\s1.txt"
"%HTOP%" -b -n 1 -s PID -r > "%TDIR%\s2.txt" 2>&1
call :checkgrep "sort by PID descending (-r) accepted" "COMMAND" "%TDIR%\s2.txt"
"%HTOP%" -b -n 1 -s RES > "%TDIR%\s3.txt" 2>&1
call :checkgrep "sort by RES accepted" "COMMAND" "%TDIR%\s3.txt"
set "PCTCPU=%=%CPU"
"%HTOP%" -b -n 1 -s "%PCTCPU%" > "%TDIR%\s4.txt" 2>&1
call :checkgrep "sort by percent CPU accepted" "COMMAND" "%TDIR%\s4.txt"

echo == 5. filters ==
"%HTOP%" -b -n 1 -f htop > "%TDIR%\f1.txt" 2>&1
call :checkgrep "-f filter produces output" "COMMAND" "%TDIR%\f1.txt"

"%HTOP%" -b -n 1 -f "^[n]ope$" > "%TDIR%\f3.txt" 2>&1
findstr "^[ ]*[0-9]" "%TDIR%\f3.txt" >nul 2>&1
if errorlevel 1 (
    call :ok "regex anchor filter matches nothing"
) else (
    call :fail "regex anchor filter matches nothing"
)

echo == 6. options ==
"%HTOP%" -b -n 1 -w > "%TDIR%\w1.txt" 2>&1
call :checkgrep "-w wide command accepted" "COMMAND" "%TDIR%\w1.txt"

"%HTOP%" -b -n 1 --no-color > "%TDIR%\w2.txt" 2>&1
call :checkgrep "--no-color accepted" "COMMAND" "%TDIR%\w2.txt"

"%HTOP%" -b -n 1 --limit-rows 5 > "%TDIR%\w3.txt" 2>&1
call :checkgrep "--limit-rows accepted" "COMMAND" "%TDIR%\w3.txt"

"%HTOP%" -b -n 2 -d 0.2 > "%TDIR%\w4.txt" 2>&1
call :checkgrep "-n 2 produces iterations" "load average" "%TDIR%\w4.txt"

"%HTOP%" -b -n 1 -t > "%TDIR%\w5.txt" 2>&1
call :checkgrep "-t tree view accepted" "COMMAND" "%TDIR%\w5.txt"

echo == 7. error handling ==
set "CHECKCMD=%HTOP% --no-such-option"
call :checknot "unknown long option fails"
set "CHECKCMD=%HTOP% -Z"
call :checknot "unknown short option fails"
"%HTOP%" -d -b -n 1 > "%TDIR%\e1.txt" 2>&1
if errorlevel 1 (
    call :ok "missing --delay argument fails"
) else (
    call :fail "missing --delay argument fails"
)
"%HTOP%" -- garbage > "%TDIR%\e2.txt" 2>&1
if errorlevel 1 (
    call :ok "extra operand fails"
) else (
    call :fail "extra operand fails"
)

echo == summary ==
echo   passed: %PASS% / %TOTAL%
rmdir /s /q "%TDIR%" >nul 2>&1
if %FAIL%==0 (
    echo   ALL TESTS PASSED
    exit /b 0
)
echo   %FAIL% TEST(S) FAILED
exit /b 1

rem ===================== subroutines (must stay at bottom) =====================

:ok
set /a PASS+=1
set /a TOTAL+=1
echo   [OK]   %~1
goto :eof

:fail
set /a FAIL+=1
set /a TOTAL+=1
echo   [FAIL] %~1
goto :eof

rem the command to check is supplied via %CHECKCMD%
:check
set "DESC=%~1"
%CHECKCMD% >nul 2>&1
if errorlevel 1 (
    call :fail "%DESC%"
) else (
    call :ok "%DESC%"
)
goto :eof

:checknot
set "DESC=%~1"
%CHECKCMD% >nul 2>&1
if errorlevel 1 (
    call :ok "%DESC%"
) else (
    call :fail "%DESC%"
)
goto :eof

:checkgrep
set "DESC=%~1"
findstr /C:"%~2" "%~3" >nul 2>&1
if errorlevel 1 (
    call :fail "%DESC%"
) else (
    call :ok "%DESC%"
)
goto :eof
