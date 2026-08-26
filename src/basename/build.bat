@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     basename.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=basename.exe
set SOURCE=basename.c

echo(
echo [1/3] Cleaning previous build...
if exist "%OUTPUT%" (
    del "%OUTPUT%"
    echo   Removed %OUTPUT%
)

echo(
echo [2/3] Compiling %SOURCE%...
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE%
echo(
%CC% %CFLAGS% -o %OUTPUT% %SOURCE%

if %ERRORLEVEL% neq 0 (
    echo(
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo(
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%

echo(
echo ============================================
echo   Running full functional tests...
echo ============================================

REM Run all tests via a single PowerShell script block
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='Continue';" ^
    "$exe='./%OUTPUT%';" ^
    "$PASS=0; $FAIL=0;" ^
    "function check($cond, $name) { if ($cond) { Write-Host '  [PASS]'; $script:PASS++ } else { Write-Host '  [FAIL]'; $script:FAIL++ } };" ^
    "Write-Host '';" ^
    "Write-Host '--- T01: basic ---';" ^
    "$out = (& $exe /usr/bin/sort).Trim();" ^
    "check ($out -eq 'sort') 'basic';" ^
    "Write-Host '';" ^
    "Write-Host '--- T02: suffix ---';" ^
    "$out = (& $exe include/stdio.h .h).Trim();" ^
    "check ($out -eq 'stdio') 'suffix';" ^
    "Write-Host '';" ^
    "Write-Host '--- T03: -s suffix ---';" ^
    "$out = (& $exe -s .h include/stdio.h).Trim();" ^
    "check ($out -eq 'stdio') '-s suffix';" ^
    "Write-Host '';" ^
    "Write-Host '--- T04: -a multiple ---';" ^
    "$out = (& $exe -a any/str1 any/str2).Trim();" ^
    "$lines = $out -split \"`n\";" ^
    "check (($lines.Count -eq 2) -and ($lines[0] -eq 'str1') -and ($lines[1] -eq 'str2')) '-a multiple';" ^
    "Write-Host '';" ^
    "Write-Host '--- T05: trailing slash ---';" ^
    "$out = (& $exe /usr/bin/).Trim();" ^
    "check ($out -eq 'bin') 'trailing slash';" ^
    "Write-Host '';" ^
    "Write-Host '--- T06: root ---';" ^
    "$out = (& $exe /).Trim();" ^
    "check ($out -eq '/') 'root';" ^
    "Write-Host '';" ^
    "Write-Host '--- T07: no args ---';" ^
    "& $exe 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'no args fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T08: extra args ---';" ^
    "& $exe a b c 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'extra args fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T09: --help ---';" ^
    "$out = & $exe --help;" ^
    "check ($out -match 'Usage:') 'help';" ^
    "Write-Host '';" ^
    "Write-Host '--- T10: --version ---';" ^
    "$out = & $exe --version;" ^
    "check ($out -match 'basename') 'version';" ^
    "Write-Host '';" ^
    "Write-Host '--- T11: just slashes ---';" ^
    "$out = (& $exe ///).Trim();" ^
    "check ($out -eq '/') 'just slashes';" ^
    "Write-Host '';" ^
    "Write-Host '--- T12: -s with -a ---';" ^
    "$out = (& $exe -a -s .txt file1.txt file2.txt).Trim();" ^
    "$lines = $out -split \"`n\";" ^
    "check (($lines.Count -eq 2) -and ($lines[0] -eq 'file1') -and ($lines[1] -eq 'file2')) '-s with -a';" ^
    "Write-Host '';" ^
    "Write-Host '--- T13: no suffix match ---';" ^
    "$out = (& $exe hello.txt .c).Trim();" ^
    "check ($out -eq 'hello.txt') 'no suffix match';" ^
    "Write-Host '';" ^
    "Write-Host '--- T14: Windows backslash ---';" ^
    "$out = (& $exe 'C:\Users\test\file.txt').Trim();" ^
    "check ($out -eq 'file.txt') 'Windows backslash';" ^
    "Write-Host '';" ^
    "Write-Host '--- T15: suffix equals name ---';" ^
    "$out = (& $exe .txt .txt).Trim();" ^
    "check ($out -eq '.txt') 'suffix equals name';" ^
    "Write-Host '';" ^
    "Write-Host '--- T16: single char ---';" ^
    "$out = (& $exe a).Trim();" ^
    "check ($out -eq 'a') 'single char';" ^
    "Write-Host '';" ^
    "Write-Host '--- T17: no suffix single name ---';" ^
    "$out = (& $exe hello.txt).Trim();" ^
    "check ($out -eq 'hello.txt') 'no suffix single name';" ^
    "Write-Host '';" ^
    "Write-Host '--- T18: --suffix= form ---';" ^
    "$out = (& $exe --suffix=.h include/stdio.h).Trim();" ^
    "check ($out -eq 'stdio') '--suffix= form';" ^
    "Write-Host '';" ^
    "Write-Host '--- T19: -z zero separator ---';" ^
    "$out = & $exe -z any/str1 any/str2 2>`$null;" ^
    "$bytes = [System.Text.Encoding]::Default.GetBytes($out);" ^
    "check ($bytes[4] -eq 0) 'zero separator';" ^
    "Write-Host '';" ^
    "Write-Host '--- T20: --multiple form ---';" ^
    "$out = (& $exe --multiple any/str1 any/str2).Trim();" ^
    "$lines = $out -split \"`n\";" ^
    "check (($lines.Count -eq 2) -and ($lines[0] -eq 'str1') -and ($lines[1] -eq 'str2')) '--multiple form';" ^
    "Write-Host '';" ^
    "Write-Host '--- T21: --zero form ---';" ^
    "$out = & $exe --zero any/str1 any/str2 2>`$null;" ^
    "$bytes = [System.Text.Encoding]::Default.GetBytes($out);" ^
    "check ($bytes[4] -eq 0) '--zero form';" ^
    "Write-Host '';" ^
    "Write-Host '--- T22: mixed path separators ---';" ^
    "$out = (& $exe 'C:/Users/test/file.txt').Trim();" ^
    "check ($out -eq 'file.txt') 'mixed path separators';" ^
    "Write-Host '';" ^
    "Write-Host '--- T23: relative path ---';" ^
    "$out = (& $exe ./src/main.c).Trim();" ^
    "check ($out -eq 'main.c') 'relative path';" ^
    "Write-Host '';" ^
    "Write-Host '--- T24: double dot ---';" ^
    "$out = (& $exe ../dir/file.txt).Trim();" ^
    "check ($out -eq 'file.txt') 'double dot';" ^
    "Write-Host '';" ^
    "Write-Host '--- T25: suffix longer than name ---';" ^
    "$out = (& $exe ab .abcdef).Trim();" ^
    "check ($out -eq 'ab') 'suffix longer than name';" ^
    "Write-Host '';" ^
    "Write-Host ('============================================');" ^
    "Write-Host ('  Test Results: PASS=' + $PASS + '  FAIL=' + $FAIL);" ^
    "Write-Host ('============================================');" ^
    "if ($FAIL -eq 0) { Write-Host '  All tests passed.' } else { Write-Host '  Some tests failed.' }; " ^
    "exit $FAIL"

set /a TESTFAIL=%ERRORLEVEL%
echo(
echo ============================================
if %TESTFAIL% equ 0 (
    echo   All tests passed.
) else (
    echo   Some tests failed.
)
echo ============================================

endlocal & exit /b %TESTFAIL%
