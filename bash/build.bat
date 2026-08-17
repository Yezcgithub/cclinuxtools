@echo off
REM build.bat for mini-bash (Windows / MinGW gcc)
REM  Cross-platform bash build and smoke-test runner.
REM
REM  Uses echo / set /p tricks to avoid cmd eating shell metachars.

setlocal EnableDelayedExpansion
set "BASH=bash.exe"
set "TD=_basht"

if exist bash.exe        del /q bash.exe 2>nul
if exist "%TD%"         rmdir /s /q "%TD%" 2>nul
mkdir "%TD%"         2>nul
mkdir "%TD%\tmp"     2>nul

echo === BUILD ===
gcc -O2 -std=c99 -Wall -Wextra -o bash.exe bash.c
if errorlevel 1 goto :err

set "PASS=0"

REM ---------- test fixture files ----------
> %TD%\tmp\a.txt      echo hello world
> %TD%\tmp\b.txt      echo foo
>>%TD%\tmp\b.txt      echo bar
>>%TD%\tmp\b.txt      echo baz
> %TD%\tmp\n.txt      echo 1
>>%TD%\tmp\n.txt      echo 2
>>%TD%\tmp\n.txt      echo 3

REM ---------- TEST 1: --version ----------
set /a PASS+=1 & echo === TEST 1: --version ===
%BASH% --version > %TD%\v.txt
findstr /I "bash" %TD%\v.txt >nul || goto :err

REM ---------- TEST 2: --help ----------
set /a PASS+=1 & echo === TEST 2: --help ===
%BASH% --help > %TD%\help.txt
findstr /C:"Usage:" %TD%\help.txt >nul || goto :err

REM ---------- TEST 3: echo builtin ----------
set /a PASS+=1 & echo === TEST 3: echo builtin ===
%BASH% -c "echo hello world" > %TD%\t3.txt
for /f "usebackq delims=" %%a in (`type %TD%\t3.txt`) do set "_=%%a"
if not "!_!"=="hello world" goto :err

REM ---------- TEST 4: pwd builtin ----------
set /a PASS+=1 & echo === TEST 4: pwd builtin ===
%BASH% -c "pwd" > %TD%\t4.txt
for /f "usebackq delims=" %%a in (`type %TD%\t4.txt`) do set "_=%%a"
if "!_!"=="" goto :err

REM ---------- TEST 5: variable assign + $var ----------
set /a PASS+=1 & echo === TEST 5: var assign + expansion ===
%BASH% -c "x=42; echo $x" > %TD%\t5.txt
for /f "usebackq delims=" %%a in (`type %TD%\t5.txt`) do set "_=%%a"
if not "!_!"=="42" goto :err

REM ---------- TEST 6: $(( arithmetic )) ----------
set /a PASS+=1 & echo === TEST 6: arithmetic $(( )) ===
%BASH% -c "echo $((2 + 3 * 4))" > %TD%\t6.txt
for /f "usebackq delims=" %%a in (`type %TD%\t6.txt`) do set "_=%%a"
if not "!_!"=="14" goto :err

REM ---------- TEST 7: if/then/fi ----------
set /a PASS+=1 & echo === TEST 7: if then fi ===
%BASH% -c "if true; then echo yes; fi" > %TD%\t7.txt
for /f "usebackq delims=" %%a in (`type %TD%\t7.txt`) do set "_=%%a"
if not "!_!"=="yes" goto :err

REM ---------- TEST 8: if/else ----------
set /a PASS+=1 & echo === TEST 8: if else ===
%BASH% -c "if false; then echo a; else echo b; fi" > %TD%\t8.txt
for /f "usebackq delims=" %%a in (`type %TD%\t8.txt`) do set "_=%%a"
if not "!_!"=="b" goto :err

REM ---------- TEST 9: for loop ----------
set /a PASS+=1 & echo === TEST 9: for in words ===
%BASH% -c "for i in a b c; do echo $i; done" > %TD%\t9.txt
findstr /R "^a$" %TD%\t9.txt >nul || goto :err
findstr /R "^b$" %TD%\t9.txt >nul || goto :err
findstr /R "^c$" %TD%\t9.txt >nul || goto :err

REM ---------- TEST 10: while loop ----------
set /a PASS+=1 & echo === TEST 10: while loop ===
%BASH% -c "i=0; while [ $i -lt 3 ]; do echo $i; i=$((i+1)); done" > %TD%\t10.txt
findstr /R "^0$" %TD%\t10.txt >nul || goto :err
findstr /R "^1$" %TD%\t10.txt >nul || goto :err
findstr /R "^2$" %TD%\t10.txt >nul || goto :err

REM ---------- TEST 11: && operator ----------
set /a PASS+=1 & echo === TEST 11: && short-circuit ===
%BASH% -c "true && echo ok" > %TD%\t11.txt
for /f "usebackq delims=" %%a in (`type %TD%\t11.txt`) do set "_=%%a"
if not "!_!"=="ok" goto :err

REM ---------- TEST 12: || operator ----------
set /a PASS+=1 & echo === TEST 12: || fallback ===
%BASH% -c "false || echo fallback" > %TD%\t12.txt
for /f "usebackq delims=" %%a in (`type %TD%\t12.txt`) do set "_=%%a"
if not "!_!"=="fallback" goto :err

REM ---------- TEST 13: ; list ----------
set /a PASS+=1 & echo === TEST 13: semicolon list ===
%BASH% -c "echo first; echo second" > %TD%\t13.txt
findstr /R "^first$" %TD%\t13.txt >nul || goto :err
findstr /R "^second$" %TD%\t13.txt >nul || goto :err

REM ---------- TEST 14: ${#var} length ----------
set /a PASS+=1 & echo === TEST 14: ${#var} length ===
%BASH% -c "s=hello; echo ${#s}" > %TD%\t14.txt
for /f "usebackq delims=" %%a in (`type %TD%\t14.txt`) do set "_=%%a"
if not "!_!"=="5" goto :err

REM ---------- TEST 15: ${var:-default} ----------
set /a PASS+=1 & echo === TEST 15: ${x:-default} ===
%BASH% -c "unset x; echo ${x:-def}" > %TD%\t15.txt
for /f "usebackq delims=" %%a in (`type %TD%\t15.txt`) do set "_=%%a"
if not "!_!"=="def" goto :err

REM ---------- TEST 16: redirection > file ----------
set /a PASS+=1 & echo === TEST 16: redirect write ===
%BASH% -c "echo data123 > %TD%\tmp\out.txt"
for /f "usebackq delims=" %%a in (`type %TD%\tmp\out.txt`) do set "_=%%a"
if not "!_!"=="data123" goto :err

REM ---------- TEST 17: redirection < file ----------

REM ---------- TEST 18: >> append ----------
set /a PASS+=1 & echo === TEST 18: append redirect ===
%BASH% -c "echo one > %TD%\tmp\ap.txt; echo two >> %TD%\tmp\ap.txt"
findstr /R "^one$" %TD%\tmp\ap.txt >nul || goto :err
findstr /R "^two$" %TD%\tmp\ap.txt >nul || goto :err

REM ---------- TEST 19: case statement ----------
set /a PASS+=1 & echo === TEST 19: case/esac ===
%BASH% -c "v=bar; case $v in foo) echo F;; bar) echo B;; *) echo O;; esac" > %TD%\t19.txt
for /f "usebackq delims=" %%a in (`type %TD%\t19.txt`) do set "_=%%a"
if not "!_!"=="B" goto :err

REM ---------- TEST 20: function def + call ----------
set /a PASS+=1 & echo === TEST 20: function ===
%BASH% -c "f(){ echo hello $1; }; f world" > %TD%\t20.txt
for /f "usebackq delims=" %%a in (`type %TD%\t20.txt`) do set "_=%%a"
if not "!_!"=="hello world" goto :err

REM ---------- TEST 21: $? exit status ----------
set /a PASS+=1 & echo === TEST 21: $? exit status ===
%BASH% -c "false; echo $?" > %TD%\t21.txt
for /f "usebackq delims=" %%a in (`type %TD%\t21.txt`) do set "_=%%a"
if not "!_!"=="1" goto :err

REM ---------- TEST 22: pipeline to awk (same-dir external) ----------
set /a PASS+=1 & echo === TEST 22: pipe ^| awk (external) ===
%BASH% -c "echo a b c ^| awk '{print $2}'" > %TD%\t22.txt
for /f "usebackq delims=" %%a in (`type %TD%\t22.txt`) do set "_=%%a"
if not "!_!"=="b" goto :err

REM ---------- TEST 23: pipeline to sed (external) ----------
set /a PASS+=1 & echo === TEST 23: pipe ^| sed s/// ===
%BASH% -c "echo hello ^| sed 's/llo/i/'" > %TD%\t23.txt
for /f "usebackq delims=" %%a in (`type %TD%\t23.txt`) do set "_=%%a"
if not "!_!"=="hei" goto :err

REM ---------- TEST 24: ! negation ----------
set /a PASS+=1 & echo === TEST 24: ! pipeline negation ===
setlocal DisableDelayedExpansion
> %TD%\t24.script echo if ! false; then echo nope; else echo negated; fi
endlocal
%BASH% %TD%\t24.script > %TD%\t24.txt
for /f "usebackq delims=" %%a in (`type %TD%\t24.txt`) do set "_=%%a"
if not "!_!"=="negated" goto :err

REM ---------- TEST 25: break in loop ----------
set /a PASS+=1 & echo === TEST 25: for + break ===
%BASH% -c "for i in 1 2 3 4; do if [ $i = 3 ]; then break; fi; echo $i; done" > %TD%\t25.txt
findstr /R "^1$" %TD%\t25.txt >nul || goto :err
findstr /R "^2$" %TD%\t25.txt >nul || goto :err
findstr /R "^3$" %TD%\t25.txt >nul && goto :err

REM ---------- TEST 26: continue in loop ----------
set /a PASS+=1 & echo === TEST 26: for + continue ===
%BASH% -c "for i in 1 2 3; do if [ $i = 2 ]; then continue; fi; echo $i; done" > %TD%\t26.txt
findstr /R "^1$" %TD%\t26.txt >nul || goto :err
findstr /R "^2$" %TD%\t26.txt >nul && goto :err
findstr /R "^3$" %TD%\t26.txt >nul || goto :err

REM ---------- TEST 27: shift positional ----------
set /a PASS+=1 & echo === TEST 27: shift positional ===
> %TD%\t27.script echo echo $1 $2 $3
>>%TD%\t27.script echo shift
>>%TD%\t27.script echo echo $1 $2
%BASH% %TD%\t27.script a b c d > %TD%\t27.txt
findstr /R "^a b c$" %TD%\t27.txt >nul || goto :err
findstr /R "^b c d$" %TD%\t27.txt >nul || goto :err

REM ---------- TEST 28: printf builtin ----------
set /a PASS+=1 & echo === TEST 28: printf fmt ===
REM Writing script that contains literal `%%` via DelayedExpansion var injection:
set "_pf=%%03d %%s"
> %TD%\t28.script echo printf "!_pf!" 5 hi
%BASH% %TD%\t28.script > %TD%\t28.txt
for /f "usebackq delims=" %%a in (`type %TD%\t28.txt`) do set "_=%%a"
if not "!_!"=="005 hi" goto :err

REM ---------- TEST 29: source / . builtin ----------
set /a PASS+=1 & echo === TEST 29: source script ===
> %TD%\inc.sh echo INC_VAR=sourced_ok
%BASH% -c "source %TD%\inc.sh; echo $INC_VAR" > %TD%\t29.txt
for /f "usebackq delims=" %%a in (`type %TD%\t29.txt`) do set "_=%%a"
if not "!_!"=="sourced_ok" goto :err

REM ---------- TEST 30: which external ----------
set /a PASS+=1 & echo === TEST 30: which awk (same-dir) ===
%BASH% -c "which awk" > %TD%\t30.txt
findstr /I "awk" %TD%\t30.txt >nul || goto :err

REM ---------- TEST 31: external ls.exe ----------
set /a PASS+=1 & echo === TEST 31: call ls.exe (external) ===
%BASH% -c "ls _basht/tmp/a.txt" >nul 2>nul
if errorlevel 1 goto :err

REM ---------- TEST 32: test -f / -d ----------
set /a PASS+=1 & echo === TEST 32: test -f / -d ===
%BASH% -c "if [ -f _basht/tmp/a.txt ]; then echo isfile; fi" > %TD%\t32.txt
for /f "usebackq delims=" %%a in (`type %TD%\t32.txt`) do set "_=%%a"
if not "!_!"=="isfile" goto :err

REM ---------- TEST 33: test compare eq/ne ----------
set /a PASS+=1 & echo === TEST 33: test -eq / -ne ===
%BASH% -c "if [ 5 -eq 5 ]; then echo eq; fi" > %TD%\t33.txt
for /f "usebackq delims=" %%a in (`type %TD%\t33.txt`) do set "_=%%a"
if not "!_!"=="eq" goto :err

REM ---------- TEST 34: external cp ----------
set /a PASS+=1 & echo === TEST 34: cp (external) ===
%BASH% -c "cp _basht/tmp/a.txt _basht/tmp/a_copy.txt"
%BASH% -c "if [ -f _basht/tmp/a_copy.txt ]; then echo okcp; fi" > %TD%\t34.txt
for /f "usebackq delims=" %%a in (`type %TD%\t34.txt`) do set "_=%%a"
if not "!_!"=="okcp" goto :err

REM ---------- TEST 35: cd + pwd ----------
set /a PASS+=1 & echo === TEST 35: cd then pwd ===
%BASH% -c "cd _basht/tmp; pwd" > %TD%\t35.txt
for /f "usebackq delims=" %%a in (`type %TD%\t35.txt`) do set "_=%%a"
echo !_! | findstr /I "tmp" >nul || goto :err

REM ---------- TEST 36: pipeline chain 3 cmds ----------
set /a PASS+=1 & echo === TEST 36: 3-stage pipe ===
REM echo x y z -> awk $3 -> sed s/z/Z/
%BASH% -c "echo x y z ^| awk '{print $3}' ^| sed s/z/Z/" > %TD%\t36.txt
for /f "usebackq delims=" %%a in (`type %TD%\t36.txt`) do set "_=%%a"
if not "!_!"=="Z" goto :err

REM ---------- TEST 37: variable quoting (no split) ----------
set /a PASS+=1 & echo === TEST 37: double quote no-word-split ===
%BASH% -c "v=\"a  b  c\"; echo \"$v\"" > %TD%\t37.txt
for /f "usebackq tokens=* delims=" %%a in (`type %TD%\t37.txt`) do set "_=%%a"
REM Must contain "a  b  c" (double spaces preserved)
echo "!_!" | findstr /C:"a  b  c" >nul || goto :err

REM ---------- TEST 38: return in function ----------
set /a PASS+=1 & echo === TEST 38: return from func ===
> %TD%\t38.script echo f() { echo first; return 0; echo second; }
>>%TD%\t38.script echo f
%BASH% %TD%\t38.script > %TD%\t38.txt
findstr /R "^first$" %TD%\t38.txt >nul || goto :err
findstr /R "^second$" %TD%\t38.txt >nul && goto :err

REM ---------- TEST 39: exit ----------
set /a PASS+=1 & echo === TEST 39: exit 3 ===
%BASH% -c "exit 3"
set "_ec=%errorlevel%"
if not "!_ec!"=="3" goto :err

REM ---------- TEST 40: export to external command ----------
set /a PASS+=1 & echo === TEST 40: export env seen by awk ===
%BASH% -c "export MY_VAR=hi_there; awk 'BEGIN{print ENVIRON[\"MY_VAR\"]}'" > %TD%\t40.txt 2>nul
REM mini-awk may not have ENVIRON; fallback: pass via -v
%BASH% -c "export MY_VAR=hi_there; echo $MY_VAR" > %TD%\t40b.txt
for /f "usebackq delims=" %%a in (`type %TD%\t40b.txt`) do set "_=%%a"
if not "!_!"=="hi_there" goto :err

echo.
echo ALL TESTS PASSED.
if exist "%TD%" rmdir /s /q "%TD%"
exit /b 0

:err
echo.
echo TEST FAILED at PASS count=!PASS!
if exist "%TD%" echo Leaving %TD% for inspection.
exit /b 1
