@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     grep.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=grep.exe
set SOURCE=grep.c
set PASS=0
set FAIL=0

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

set WD=%TEMP%\greptest
if exist "%WD%" rd /s /q "%WD%"
mkdir "%WD%" >nul 2>&1
set SRCDIR=%CD%
pushd "%WD%"
copy /y "%SRCDIR%\%OUTPUT%" . >nul 2>&1

REM ---- fixtures ----
> f1.txt echo hello world
>> f1.txt echo foo bar
>> f1.txt echo hello again
>> f1.txt echo end
> nums.txt echo one
>> nums.txt echo two
>> nums.txt echo three
>> nums.txt echo 42
>> nums.txt echo x9y
> ctx.txt echo a
>> ctx.txt echo b
>> ctx.txt echo c
>> ctx.txt echo MATCH
>> ctx.txt echo d
>> ctx.txt echo e
>> ctx.txt echo f
>> ctx.txt echo g
>> ctx.txt echo MATCH2
>> ctx.txt echo h
> re.txt echo aaa
>> re.txt echo abab
>> re.txt echo word
>> re.txt echo x123x
>> re.txt echo hello world
> met.txt echo a.c
>> met.txt echo abc
>> met.txt echo axc
> m.txt echo hit1
>> m.txt echo miss
>> m.txt echo hit2
>> m.txt echo hit3
type nul > empty.txt
> pats.txt echo hello
>> pats.txt echo end
> bin.hex echo 414200430A68690A
certutil -decodehex -f bin.hex bin.dat >nul 2>&1

mkdir t35\sub
> t35\top.txt echo nothing here
> t35\sub\deep.txt echo needle deep
mkdir t36
> t36\root.txt echo needle in txt
> t36\root.log echo needle in log
mkdir t38

REM ========== T01: Basic match
echo(
echo --- T01: Basic match ---
"%OUTPUT%" hello f1.txt > got.txt 2>&1
set RC=!ERRORLEVEL!
findstr /c:"hello world" got.txt >nul
if !ERRORLEVEL! equ 0 if !RC! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T02: No match exits 1
echo(
echo --- T02: No match exit 1 ---
"%OUTPUT%" zebra f1.txt >nul 2>&1
if !ERRORLEVEL! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T03: Missing file exits 2
echo(
echo --- T03: Missing file exit 2 ---
"%OUTPUT%" hello nosuchfile.txt >nul 2>&1
if !ERRORLEVEL! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T04: -n line numbers
echo(
echo --- T04: -n line numbers ---
"%OUTPUT%" -n foo f1.txt > got.txt
findstr /x /c:"2:foo bar" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T05: -c count
echo(
echo --- T05: -c count ---
"%OUTPUT%" -c hello f1.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T06: -v invert count
echo(
echo --- T06: -v invert ---
"%OUTPUT%" -vc e f1.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T07: -i ignore case
echo(
echo --- T07: -i ignore case ---
"%OUTPUT%" -ic HELLO f1.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T08: -w whole word
echo(
echo --- T08: -w whole word ---
"%OUTPUT%" -cw word re.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T09: -x whole line
echo(
echo --- T09: -x whole line ---
"%OUTPUT%" -cx end f1.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    "%OUTPUT%" -cx en f1.txt > got.txt
    findstr /x /c:"0" got.txt >nul
    if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T10: -E alternation
echo(
echo --- T10: -E alternation ---
"%OUTPUT%" -Ec "hello|foo" f1.txt > got.txt
findstr /x /c:"3" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T11: -F fixed strings
echo(
echo --- T11: -F fixed strings ---
"%OUTPUT%" -Fc "a.c" met.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    "%OUTPUT%" -Ec "a.c" met.txt > got.txt
    findstr /x /c:"3" got.txt >nul
    if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T12: BRE interval
echo(
echo --- T12: BRE interval ---
"%OUTPUT%" -c "a\{2,\}" re.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T13: BRE backreference
echo(
echo --- T13: BRE backreference ---
"%OUTPUT%" -c "\(ab\)\1" re.txt > got.txt
findstr /x /c:"1" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T14: ERE class plus
echo(
echo --- T14: ERE class plus ---
"%OUTPUT%" -Ec "[0-9]+" nums.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T15: POSIX :digit: class
echo(
echo --- T15: POSIX class ---
"%OUTPUT%" -c "[[:digit:]]" nums.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T16: anchors
echo(
echo --- T16: Anchors ---
"%OUTPUT%" -c "^hello" f1.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    "%OUTPUT%" -c "world$" f1.txt > got.txt
    findstr /x /c:"1" got.txt >nul
    if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T17: -o only matching
echo(
echo --- T17: -o only matching ---
echo abc abc|"%OUTPUT%" -o abc > got.txt
for /f %%i in ('type got.txt^|find /c /v ""') do set LINES=%%i
if "!LINES!"=="2" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T18: -b byte offset (fixture lines end CRLF: 13 raw bytes)
echo(
echo --- T18: -b byte offset ---
"%OUTPUT%" -b foo f1.txt > got.txt
findstr /x /c:"13:foo bar" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T19: -q quiet exit codes
echo(
echo --- T19: -q quiet ---
"%OUTPUT%" -q hello f1.txt >nul 2>&1
set A=!ERRORLEVEL!
"%OUTPUT%" -q zebra f1.txt >nul 2>&1
set B=!ERRORLEVEL!
if !A! equ 0 if !B! equ 1 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T20: -l files-with-matches
echo(
echo --- T20: -l list ---
"%OUTPUT%" -l hello f1.txt nums.txt > got.txt
findstr /c:"f1.txt" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /c:"nums.txt" got.txt >nul
    if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T21: -L files-without-match
echo(
echo --- T21: -L list ---
"%OUTPUT%" -L hello f1.txt nums.txt > got.txt
findstr /c:"nums.txt" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /c:"f1.txt" got.txt >nul
    if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T22: -m max-count
echo(
echo --- T22: -m max-count ---
"%OUTPUT%" -cm2 hit m.txt > got.txt
findstr /x /c:"2" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T23: -e multiple patterns
echo(
echo --- T23: -e multiple ---
"%OUTPUT%" -c -e hello -e foo f1.txt > got.txt
findstr /x /c:"3" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T24: -f pattern file
echo(
echo --- T24: -f pattern file ---
"%OUTPUT%" -cf pats.txt f1.txt > got.txt
findstr /x /c:"3" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T25: -A after-context
echo(
echo --- T25: -A after-context ---
"%OUTPUT%" -n -A1 MATCH ctx.txt > got.txt
findstr /x /c:"5-d" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /x /c:"10-h" got.txt >nul
    if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T26: -B before-context
echo(
echo --- T26: -B before-context ---
"%OUTPUT%" -n -B1 MATCH ctx.txt > got.txt
findstr /x /c:"3-c" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T27: -C context with -- separator
echo(
echo --- T27: -C context ---
"%OUTPUT%" -n -C1 MATCH ctx.txt > got.txt
findstr /x /c:"--" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /x /c:"4:MATCH" got.txt >nul
    if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
        findstr /x /c:"9:MATCH2" got.txt >nul
        if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
    )
)

REM ========== T28: --group-separator
echo(
echo --- T28: --group-separator ---
"%OUTPUT%" -C1 --group-separator=%% MATCH ctx.txt > got.txt
findstr /x /c:"%%" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T29: stdin input
echo(
echo --- T29: stdin ---
type f1.txt|"%OUTPUT%" hello > got.txt
findstr /x /c:"hello world" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T30: multiple files show labels
echo(
echo --- T30: filename labels ---
"%OUTPUT%" hello f1.txt nums.txt > got.txt
for /f %%i in ('type got.txt^|find /c ":hello"') do set CNT=%%i
if "!CNT!"=="2" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T31: -h suppress labels
echo(
echo --- T31: -h no labels ---
"%OUTPUT%" -h hello f1.txt nums.txt > got.txt
findstr /x /c:"hello world" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T32: -H force labels
echo(
echo --- T32: -H force labels ---
"%OUTPUT%" -H hello f1.txt > got.txt
findstr /c:"f1.txt:hello world" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T33: binary file report
echo(
echo --- T33: Binary file matches ---
"%OUTPUT%" hi bin.dat > got.txt 2>&1
set RC=!ERRORLEVEL!
findstr /c:"Binary file" got.txt >nul
if !ERRORLEVEL! equ 0 if !RC! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T34: -a text mode on binary
echo(
echo --- T34: -a text mode ---
"%OUTPUT%" -a hi bin.dat > got.txt
findstr /x /c:"hi" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T35: -r recursion
echo(
echo --- T35: -r recursion ---
"%OUTPUT%" -r needle t35 > got.txt 2>&1
findstr /c:"deep.txt:needle deep" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T36: --include glob
echo(
echo --- T36: --include ---
"%OUTPUT%" -r --include=*.txt needle t36 > got.txt 2>&1
findstr /c:"root.txt" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /c:"root.log" got.txt >nul
    if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T37: --exclude glob
echo(
echo --- T37: --exclude ---
"%OUTPUT%" -r --exclude=*.log needle t36 > got.txt 2>&1
findstr /c:"root.txt" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /c:"root.log" got.txt >nul
    if !ERRORLEVEL! neq 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

REM ========== T38: directory operand error
echo(
echo --- T38: Is a directory ---
"%OUTPUT%" hello t38 >nul 2> err.txt
set RC=!ERRORLEVEL!
findstr /c:"Is a directory" err.txt >nul
if !ERRORLEVEL! equ 0 if !RC! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T39: -d skip directories
echo(
echo --- T39: -d skip ---
"%OUTPUT%" -d skip hello t38 > got.txt 2> err.txt
set RC=!ERRORLEVEL!
for %%A in (got.txt) do set OUTSZ=%%~zA
for %%A in (err.txt) do set ERRSZ=%%~zA
if !RC! equ 1 if "!OUTSZ!"=="0" if "!ERRSZ!"=="0" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T40: --help
echo(
echo --- T40: --help ---
"%OUTPUT%" --help > got.txt
findstr /c:"Usage:" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T41: --version
echo(
echo --- T41: --version ---
"%OUTPUT%" --version > got.txt
findstr /c:"v1.0.0" got.txt >nul
if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T42: invalid regex exits 2
echo(
echo --- T42: invalid regex ---
"%OUTPUT%" "[" f1.txt >nul 2>&1
if !ERRORLEVEL! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T43: invalid option exits 2
echo(
echo --- T43: invalid option ---
"%OUTPUT%" -Z hello f1.txt >nul 2>&1
if !ERRORLEVEL! equ 2 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T44: empty pattern matches all
echo(
echo --- T44: empty pattern ---
echo a> two_lines.txt
>> two_lines.txt echo b
type two_lines.txt|"%OUTPUT%" "" > got.txt
for /f %%i in ('type got.txt^|find /c /v ""') do set LINES=%%i
if "!LINES!"=="2" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T45: -s suppresses errors
echo(
echo --- T45: -s suppress ---
"%OUTPUT%" -s hello nosuchfile.txt >nul 2> err.txt
set RC=!ERRORLEVEL!
for %%A in (err.txt) do set ERRSZ=%%~zA
if !RC! equ 2 if "!ERRSZ!"=="0" (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL]) else (set /a FAIL+=1 & echo   [FAIL])

REM ========== T46: -o -b ascending offsets
echo(
echo --- T46: -o -b offsets ---
echo abc abc|"%OUTPUT%" -ob abc > got.txt
findstr /x /c:"0:abc" got.txt >nul
if !ERRORLEVEL! neq 0 (set /a FAIL+=1 & echo   [FAIL]) else (
    findstr /x /c:"4:abc" got.txt >nul
    if !ERRORLEVEL! equ 0 (set /a PASS+=1 & echo   [PASS]) else (set /a FAIL+=1 & echo   [FAIL])
)

popd

echo(
echo ============================================
echo   Test Results: PASS=%PASS%  FAIL=%FAIL%
echo ============================================
if %FAIL% equ 0 (
    echo   All tests passed!
) else (
    echo   Some tests failed!
)

endlocal
exit /b 0
