@echo off
REM build.bat for sed (Windows / MinGW gcc)
REM  Cross-platform mini-sed build and smoke-test runner.
REM
REM  Scripts containing `!` (address negation) are written via set /p
REM  to avoid DelayedExpansion mangling.

setlocal EnableDelayedExpansion
set "SED=sed.exe"
set "TD=_sedt"

if exist sed.exe        del /q sed.exe 2>nul
if exist "%TD%"         rmdir /s /q "%TD%" 2>nul
mkdir "%TD%"         2>nul

echo === BUILD ===
gcc -O2 -std=c99 -Wall -Wextra -o sed.exe sed.c
if errorlevel 1 goto :err

set "PASS=0"

REM ----------------- test inputs ----------------------
> %TD%\a.txt  echo foo
>>%TD%\a.txt  echo bar
>>%TD%\a.txt  echo baz
> %TD%\n.txt  echo 1
>>%TD%\n.txt  echo 2
>>%TD%\n.txt  echo 3
>>%TD%\n.txt  echo 4
>>%TD%\n.txt  echo 5
> %TD%\n6.txt echo 1
>>%TD%\n6.txt echo 2
>>%TD%\n6.txt echo 3
>>%TD%\n6.txt echo 4
>>%TD%\n6.txt echo 5
>>%TD%\n6.txt echo 6
> %TD%\fb.txt  echo foo
>>%TD%\fb.txt  echo bar

REM ----------------- TEST 1: --version -----------------
set /a PASS+=1 & echo === TEST 1: --version ===
%SED% --version > %TD%\v.txt
findstr /I "sed" %TD%\v.txt >nul || goto :err

REM ----------------- TEST 2: --help --------------------
set /a PASS+=1 & echo === TEST 2: --help ===
%SED% --help > %TD%\help.txt
findstr /C:"Usage:" %TD%\help.txt >nul || goto :err

REM ----------------- TEST 3: basic s/// ----------------
set /a PASS+=1 & echo === TEST 3: basic s/// ===
echo hello world|%SED% "s/world/sed/" > %TD%\t3.txt
for /f "usebackq delims=" %%a in (`type %TD%\t3.txt`) do set "_=%%a"
if not "!_!"=="hello sed" goto :err

REM ----------------- TEST 4: s///g ---------------------
set /a PASS+=1 & echo === TEST 4: s///g global ===
echo aaa|%SED% "s/a/b/g" > %TD%\t4.txt
for /f "usebackq delims=" %%a in (`type %TD%\t4.txt`) do set "_=%%a"
if not "!_!"=="bbb" goto :err

REM ----------------- TEST 5: s///2 ---------------------
set /a PASS+=1 & echo === TEST 5: s///2 nth ===
echo aaa|%SED% "s/a/X/2" > %TD%\t5.txt
for /f "usebackq delims=" %%a in (`type %TD%\t5.txt`) do set "_=%%a"
if not "!_!"=="aXa" goto :err

REM ----------------- TEST 6: s///p ---------------------
set /a PASS+=1 & echo === TEST 6: s///p flag ===
echo abc|%SED% -n "s/b/B/p" > %TD%\t6.txt
for /f "usebackq delims=" %%a in (`type %TD%\t6.txt`) do set "_=%%a"
if not "!_!"=="aBc" goto :err

REM ----------------- TEST 7: s///i ---------------------
set /a PASS+=1 & echo === TEST 7: s///i case-insensitive ===
echo HELLO|%SED% "s/hello/world/i" > %TD%\t7.txt
for /f "usebackq delims=" %%a in (`type %TD%\t7.txt`) do set "_=%%a"
if not "!_!"=="world" goto :err

REM ----------------- TEST 8: d delete -------------------
set /a PASS+=1 & echo === TEST 8: d delete ===
%SED% "2d" %TD%\a.txt > %TD%\t8.txt
findstr /R "^foo$" %TD%\t8.txt >nul || goto :err
findstr /R "^baz$" %TD%\t8.txt >nul || goto :err
findstr /R "^bar$" %TD%\t8.txt >nul && goto :err

REM ----------------- TEST 9: -n p ----------------------
set /a PASS+=1 & echo === TEST 9: -n p ===
%SED% -n "2p" %TD%\a.txt > %TD%\t9.txt
for /f "usebackq delims=" %%a in (`type %TD%\t9.txt`) do set "_=%%a"
if not "!_!"=="bar" goto :err

REM ----------------- TEST 10: range --------------------
set /a PASS+=1 & echo === TEST 10: line range 2,3 ===
%SED% -n "2,3p" %TD%\n.txt > %TD%\t10.txt
findstr /R "^2$" %TD%\t10.txt >nul || goto :err
findstr /R "^3$" %TD%\t10.txt >nul || goto :err

REM ----------------- TEST 11: regex addr ---------------
set /a PASS+=1 & echo === TEST 11: regex address /b/ ===
%SED% -n "/b/p" %TD%\a.txt > %TD%\t11.txt
findstr /R "^bar$" %TD%\t11.txt >nul || goto :err
findstr /R "^baz$" %TD%\t11.txt >nul || goto :err

REM ----------------- TEST 12: $ last line --------------
set /a PASS+=1 & echo === TEST 12: $ last line ===
%SED% -n "$p" %TD%\a.txt > %TD%\t12.txt
for /f "usebackq delims=" %%a in (`type %TD%\t12.txt`) do set "_=%%a"
if not "!_!"=="baz" goto :err

REM ----------------- TEST 13: a append -----------------
set /a PASS+=1 & echo === TEST 13: a append ===
echo hello|%SED% "a\appended" > %TD%\t13.txt
findstr /R "^hello$" %TD%\t13.txt >nul || goto :err
findstr /R "^appended$" %TD%\t13.txt >nul || goto :err

REM ----------------- TEST 14: i insert -----------------
set /a PASS+=1 & echo === TEST 14: i insert ===
echo hello|%SED% "i\inserted" > %TD%\t14.txt
findstr /R "^inserted$" %TD%\t14.txt >nul || goto :err
findstr /R "^hello$" %TD%\t14.txt >nul || goto :err

REM ----------------- TEST 15: c change -----------------
set /a PASS+=1 & echo === TEST 15: c change ===
%SED% "2c\changed" %TD%\a.txt > %TD%\t15.txt
findstr /R "^changed$" %TD%\t15.txt >nul || goto :err
findstr /R "^foo$" %TD%\t15.txt >nul || goto :err

REM ----------------- TEST 16: y translit ---------------
set /a PASS+=1 & echo === TEST 16: y translit ===
echo abc|%SED% "y/abc/ABC/" > %TD%\t16.txt
for /f "usebackq delims=" %%a in (`type %TD%\t16.txt`) do set "_=%%a"
if not "!_!"=="ABC" goto :err

REM ----------------- TEST 17: N multi-line --------------
set /a PASS+=1 & echo === TEST 17: N multi-line ===
%SED% "N;s/\n/ /" %TD%\fb.txt > %TD%\t17.txt
for /f "usebackq delims=" %%a in (`type %TD%\t17.txt`) do set "_=%%a"
if not "!_!"=="foo bar" goto :err

REM ----------------- TEST 18: {} block ------------------
set /a PASS+=1 & echo === TEST 18: {} block ===
%SED% "{s/1/one/;s/3/three/}" %TD%\n.txt > %TD%\t18.txt
findstr /R "^one$" %TD%\t18.txt >nul || goto :err
findstr /R "^three$" %TD%\t18.txt >nul || goto :err

REM ----------------- TEST 19: = line number -------------
set /a PASS+=1 & echo === TEST 19: = line number ===
%SED% "=" %TD%\a.txt > %TD%\t19.txt
findstr /R "^1$" %TD%\t19.txt >nul || goto :err
findstr /R "^2$" %TD%\t19.txt >nul || goto :err

REM ----------------- TEST 20: -E extended regex ---------
set /a PASS+=1 & echo === TEST 20: -E extended regex ===
echo hello world|%SED% -E "s/(o)/\1\1/g" > %TD%\t20.txt
for /f "usebackq delims=" %%a in (`type %TD%\t20.txt`) do set "_=%%a"
if not "!_!"=="helloo woorld" goto :err

REM ----------------- TEST 21: backreference \1 ----------
set /a PASS+=1 & echo === TEST 21: backreference ===
echo hello|%SED% "s/\(l\(o\)\)/X\1Y/" > %TD%\t21.txt
for /f "usebackq delims=" %%a in (`type %TD%\t21.txt`) do set "_=%%a"
if not "!_!"=="helXloY" goto :err

REM ----------------- TEST 22: -f script file ------------
set /a PASS+=1 & echo === TEST 22: -f script file ===
> %TD%\scr.sed echo s/foo/FOO/
echo foo bar|%SED% -f %TD%\scr.sed > %TD%\t22.txt
for /f "usebackq delims=" %%a in (`type %TD%\t22.txt`) do set "_=%%a"
if not "!_!"=="FOO bar" goto :err

REM ----------------- TEST 23: -e multiple expr ----------
set /a PASS+=1 & echo === TEST 23: -e multiple expressions ===
echo abc|%SED% -e "s/a/A/" -e "s/c/C/" > %TD%\t23.txt
for /f "usebackq delims=" %%a in (`type %TD%\t23.txt`) do set "_=%%a"
if not "!_!"=="AbC" goto :err

REM ----------------- TEST 24: b branch ------------------
set /a PASS+=1 & echo === TEST 24: b branch ===
%SED% "2b skip; s/./X/; :skip" %TD%\n.txt > %TD%\t24.txt
findstr /R "^X$" %TD%\t24.txt >nul
set "_hit=!errorlevel!"
REM first line should be X
findstr /N "^X$" %TD%\t24.txt >nul || goto :err
REM second line should be 2 (not X)
findstr /N "^2$" %TD%\t24.txt >nul || goto :err

REM ----------------- TEST 25: t branch ------------------
set /a PASS+=1 & echo === TEST 25: t branch ===
%SED% "s/./X/; t end; s/X/Y/; :end" %TD%\n.txt > %TD%\t25.txt
findstr /R "^X$" %TD%\t25.txt >nul || goto :err
findstr /R "^Y$" %TD%\t25.txt >nul && goto :err

REM ----------------- TEST 26: q quit --------------------
set /a PASS+=1 & echo === TEST 26: q quit ===
%SED% "2q" %TD%\n.txt > %TD%\t26.txt
findstr /R "^1$" %TD%\t26.txt >nul || goto :err
findstr /R "^2$" %TD%\t26.txt >nul || goto :err
findstr /R "^3$" %TD%\t26.txt >nul && goto :err

REM ----------------- TEST 27: Q quit no print -----------
set /a PASS+=1 & echo === TEST 27: Q quit no print ===
%SED% "2Q" %TD%\n.txt > %TD%\t27.txt
findstr /R "^1$" %TD%\t27.txt >nul || goto :err
findstr /R "^2$" %TD%\t27.txt >nul && goto :err

REM ----------------- TEST 28: join lines ----------------
set /a PASS+=1 & echo === TEST 28: join lines ===
setlocal DisableDelayedExpansion
> %TD%\t28.sed echo :a;N;$!ba;s/\n/-/g
endlocal
%SED% -f %TD%\t28.sed %TD%\a.txt > %TD%\t28.txt
for /f "usebackq delims=" %%a in (`type %TD%\t28.txt`) do set "_=%%a"
if not "!_!"=="foo-bar-baz" goto :err

REM ----------------- TEST 29: character class -----------
set /a PASS+=1 & echo === TEST 29: character class ===
echo Hello123|%SED% "s/[0-9]//g" > %TD%\t29.txt
for /f "usebackq delims=" %%a in (`type %TD%\t29.txt`) do set "_=%%a"
if not "!_!"=="Hello" goto :err

REM ----------------- TEST 30: s with & ------------------
set /a PASS+=1 & echo === TEST 30: s with & ===
echo abc|%SED% "s/b/[&]/" > %TD%\t30.txt
for /f "usebackq delims=" %%a in (`type %TD%\t30.txt`) do set "_=%%a"
if not "!_!"=="a[b]c" goto :err

REM ----------------- TEST 31: addr~step -----------------
set /a PASS+=1 & echo === TEST 31: addr~step ===
%SED% -n "0~2p" %TD%\n6.txt > %TD%\t31.txt
findstr /R "^2$" %TD%\t31.txt >nul || goto :err
findstr /R "^4$" %TD%\t31.txt >nul || goto :err
findstr /R "^6$" %TD%\t31.txt >nul || goto :err

REM ----------------- TEST 32: r read file ----------------
set /a PASS+=1 & echo === TEST 32: r read file ===
> %TD%\rfile.txt echo INSERTED
%SED% "2r %TD%\rfile.txt" %TD%\a.txt > %TD%\t32.txt
findstr /R "^INSERTED$" %TD%\t32.txt >nul || goto :err

REM ----------------- TEST 33: w write file ---------------
set /a PASS+=1 & echo === TEST 33: w write file ===
%SED% -n "1,2w %TD%\wout.txt" %TD%\a.txt
findstr /R "^foo$" %TD%\wout.txt >nul || goto :err
findstr /R "^bar$" %TD%\wout.txt >nul || goto :err

REM ----------------- TEST 34: l list ---------------------
set /a PASS+=1 & echo === TEST 34: l list ===
echo abc|%SED% -n "l" > %TD%\t34.txt
findstr /C:"abc$" %TD%\t34.txt >nul || goto :err

REM ----------------- TEST 35: -i in-place ----------------
set /a PASS+=1 & echo === TEST 35: -i in-place ===
> %TD%\ip.txt echo foo
>>%TD%\ip.txt echo bar
%SED% -i "s/foo/FOO/" %TD%\ip.txt
findstr /R "^FOO$" %TD%\ip.txt >nul || goto :err
findstr /R "^bar$" %TD%\ip.txt >nul || goto :err

REM ----------------- TEST 36: -i with backup -------------
set /a PASS+=1 & echo === TEST 36: -i with backup ===
> %TD%\ipb.txt echo foo
>>%TD%\ipb.txt echo bar
%SED% -i".bak" "s/foo/FOO/" %TD%\ipb.txt
findstr /R "^FOO$" %TD%\ipb.txt >nul || goto :err
findstr /R "^foo$" %TD%\ipb.txt.bak >nul || goto :err

REM ----------------- TEST 37: multiple files -------------
set /a PASS+=1 & echo === TEST 37: multiple files ===
> %TD%\f1.txt echo a
> %TD%\f2.txt echo b
%SED% "s/./X/" %TD%\f1.txt %TD%\f2.txt > %TD%\t37.txt
findstr /C:"X" %TD%\t37.txt >nul
findstr /N "^X$" %TD%\t37.txt > %TD%\t37c.txt 2>nul
for /f %%a in ('type %TD%\t37c.txt ^| find /c ":"') do set "_cnt=%%a"
if not "!_cnt!"=="2" goto :err

REM ----------------- TEST 38: h/G hold space --------------
set /a PASS+=1 & echo === TEST 38: h/G hold space ===
setlocal DisableDelayedExpansion
> %TD%\t38.sed echo 1!G;h;$p
endlocal
%SED% -n -f %TD%\t38.sed %TD%\a.txt > %TD%\t38.txt
findstr /R "^baz$" %TD%\t38.txt >nul || goto :err
findstr /R "^bar$" %TD%\t38.txt >nul || goto :err
findstr /R "^foo$" %TD%\t38.txt >nul || goto :err

REM ----------------- TEST 39: ! negation ------------------
set /a PASS+=1 & echo === TEST 39: ! negation ===
setlocal DisableDelayedExpansion
> %TD%\t39.sed echo 2!p
endlocal
%SED% -n -f %TD%\t39.sed %TD%\a.txt > %TD%\t39.txt
findstr /R "^foo$" %TD%\t39.txt >nul || goto :err
findstr /R "^baz$" %TD%\t39.txt >nul || goto :err
findstr /R "^bar$" %TD%\t39.txt >nul && goto :err

REM ----------------- TEST 40: addr,+N range ---------------
set /a PASS+=1 & echo === TEST 40: addr,+N range ===
%SED% -n "2,+1p" %TD%\n.txt > %TD%\t40.txt
findstr /R "^2$" %TD%\t40.txt >nul || goto :err
findstr /R "^3$" %TD%\t40.txt >nul || goto :err

REM ----------------- TEST 41: P print first line ----------
set /a PASS+=1 & echo === TEST 41: P print first line ===
%SED% -n "N;P" %TD%\fb.txt > %TD%\t41.txt
for /f "usebackq delims=" %%a in (`type %TD%\t41.txt`) do set "_=%%a"
if not "!_!"=="foo" goto :err

REM ----------------- TEST 42: D delete first line ---------
set /a PASS+=1 & echo === TEST 42: D delete first line ===
%SED% "N;D" %TD%\a.txt > %TD%\t42.txt
for /f "usebackq delims=" %%a in (`type %TD%\t42.txt`) do set "_=%%a"
if not "!_!"=="baz" goto :err

REM ----------------- TEST 43: s with \n in repl -----------
set /a PASS+=1 & echo === TEST 43: s with \n in replacement ===
echo abc|%SED% "s/b/\n/" > %TD%\t43.txt
findstr /R "^a$" %TD%\t43.txt >nul || goto :err
findstr /R "^c$" %TD%\t43.txt >nul || goto :err

REM ----------------- TEST 44: unknown option --------------
set /a PASS+=1 & echo === TEST 44: unknown option exits 2 ===
%SED% --we-dont-have-this-option 2>nul
if not errorlevel 2 goto :err

REM ----------------- TEST 45: l command -------------------
set /a PASS+=1 & echo === TEST 45: l command output ===
echo abc|%SED% "l" > %TD%\t45.txt
findstr /C:"abc$" %TD%\t45.txt >nul || goto :err

echo.
echo ALL TESTS PASSED.
if exist "%TD%" rmdir /s /q "%TD%"
exit /b 0

:err
echo.
echo TEST FAILED at PASS count=!PASS!
if exist "%TD%" echo Leaving %TD% for inspection.
exit /b 1
