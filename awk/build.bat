@echo off
REM build.bat for awk (Windows / MinGW gcc)
REM  Cross-platform mini-awk build and smoke-test runner.
REM
REM  All awk scripts that contain `%` or pipe/ampersand redirection
REM  characters are written via :lineappend (SET /P prompt redirection)
REM  or encoded into equivalent awk idioms that avoid cmd metachars.

setlocal EnableDelayedExpansion
set "AWK=awk.exe"
set "TD=_awkt"

if exist awk.exe        del /q awk.exe 2>nul
if exist "%TD%"         rmdir /s /q "%TD%" 2>nul
mkdir "%TD%"         2>nul
mkdir "%TD%\tmp"     2>nul

echo === BUILD ===
gcc -O2 -std=c99 -Wall -Wextra -o awk.exe awk.c -lm
if errorlevel 1 goto :err

REM ----------------- test inputs ----------------------
> %TD%\tmp\a.txt      echo hello world
> %TD%\tmp\b.txt      echo a b c d
> %TD%\tmp\n.txt      echo 10
>>%TD%\tmp\n.txt      echo 20
>>%TD%\tmp\n.txt      echo 30
> %TD%\pw.txt         echo root:x:0:0:root:/root:/bin/bash
>>%TD%\pw.txt         echo alice:x:1001:1000:alice:/home/alice:/bin/bash
>>%TD%\pw.txt         echo bob:x:500:100:bob:/home/bob:/bin/sh
>>%TD%\pw.txt         echo nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
> %TD%\tmp\s1.txt     echo foobar
> %TD%\tmp\s2.txt     echo abcdef
> %TD%\tmp\c.txt      echo 5
> %TD%\tmp\fs.txt     echo a,b-c,d
> %TD%\tmp\ofs.txt    echo 1 2 3
> %TD%\tmp\sub.txt    echo ababa
> %TD%\tmp\gs.txt     echo aabbaa
> %TD%\tmp\c2.txt     echo Hello

REM ----------------- TEST 1: version -----------------
echo === TEST 1: --version ===
awk.exe --version > %TD%\v.txt
findstr /I "awk" %TD%\v.txt >nul || goto :err

REM ----------------- TEST 2: help --------------------
echo === TEST 2: --help contains Usage ===
awk.exe --help > %TD%\help.txt
findstr /C:"Usage:" %TD%\help.txt >nul || goto :err

REM ----------------- TEST 3: print $0 ----------------
echo === TEST 3: print $0 ^(file input^) ===
> %TD%\p1.script echo { print }
awk.exe -f %TD%\p1.script %TD%\tmp\a.txt > %TD%\p1.txt
for /f "usebackq delims=" %%a in (`type %TD%\p1.txt`) do set "_=%%a"
if not "%_%"=="hello world" goto :err

REM ----------------- TEST 4: $1/$3 -------------------
echo === TEST 4: print $1 and $3 ===
> %TD%\p2.script echo { print $1, $3 }
awk.exe -f %TD%\p2.script %TD%\tmp\b.txt > %TD%\p2.txt
for /f "usebackq delims=" %%a in (`type %TD%\p2.txt`) do set "_=%%a"
if not "%_%"=="a c" goto :err

REM ----------------- TEST 5: $3 >= 1000 --------------
echo === TEST 5: -F colon + $3 ^>= 1000 ===
> %TD%\p3.script echo $3 ^>= 1000 { print $1 }
awk.exe -F: -f %TD%\p3.script %TD%\pw.txt > %TD%\p3.txt
findstr /R "^alice" %TD%\p3.txt >nul || goto :err
findstr /R "^nobody" %TD%\p3.txt >nul || goto :err

REM ----------------- TEST 6: NF NR FNR ---------------
echo === TEST 6: NF NR FNR builtins ===
> %TD%\p4.script echo {print NR, NF, FILENAME}
awk.exe -f %TD%\p4.script %TD%\pw.txt > %TD%\p4.txt
findstr /R "^4 7 _awkt" %TD%\p4.txt >nul || goto :err

REM ----------------- TEST 7: BEGIN/END ---------------
echo === TEST 7: BEGIN / END accumulate ===
awk.exe "BEGIN{s=0} {s+=$1} END{print s}" %TD%\tmp\n.txt > %TD%\p5.txt
for /f "usebackq" %%a in (`type %TD%\p5.txt`) do set "_=%%a"
if not "%_%"=="60" goto :err

REM ----------------- TEST 8: /ro/ --------------------
echo === TEST 8: regex pattern /ro/ ===
awk.exe "/ro/" %TD%\pw.txt > %TD%\p6.txt
findstr "root" %TD%\p6.txt >nul || goto :err

REM ----------------- TEST 9: ~ RSTART RLENGTH --------
echo === TEST 9: ~ match + RSTART/RLENGTH ===
> %TD%\p7.script echo { if ($0 ~ /wo/) print RSTART, RLENGTH }
awk.exe -f %TD%\p7.script %TD%\tmp\a.txt > %TD%\p7.txt
findstr /R "^7 2" %TD%\p7.txt >nul || goto :err

REM ----------------- TEST 10: substr/index -----------
echo === TEST 10: substr / length / index ===
> %TD%\p8.script echo { print substr($0,2,3) }
awk.exe -f %TD%\p8.script %TD%\tmp\s1.txt > %TD%\p8.txt
for /f "usebackq" %%a in (`type %TD%\p8.txt`) do set "_=%%a"
if not "%_%"=="oob" goto :err
> %TD%\p9.script echo {print index($0, "de")}
awk.exe -f %TD%\p9.script %TD%\tmp\s2.txt > %TD%\p9.txt
for /f "usebackq" %%a in (`type %TD%\p9.txt`) do set "_=%%a"
if not "%_%"=="4" goto :err

REM ----------------- TEST 11: printf -----------------
echo === TEST 11: printf d f s ===
REM Writing a script that contains literal `%%` via `set /p` trick:
<nul > %TD%\p10.script set /p "=BEGIN{printf "
>>    %TD%\p10.script echo " %%%%03d %%%%0.2f %%%%s", 5, 3.14159, "ok"}
REM Above still leaks `%0` through `echo`. Instead use a here-line with vars:
set "_fmt_1=%%03d %%0.2f %%s"
> %TD%\p10.script echo BEGIN{printf "!_fmt_1!", 5, 3.14159, "ok"}
awk.exe -f %TD%\p10.script > %TD%\p10.txt
findstr "005 3.14 ok" %TD%\p10.txt >nul || goto :err

REM ----------------- TEST 12: if/cmp -----------------
echo === TEST 12: if + numeric compare ===
> %TD%\p11.script echo { if ($1 ^>= 20) print "big"; else print "small" }
awk.exe -f %TD%\p11.script %TD%\tmp\n.txt > %TD%\p11.txt
findstr /R "small" %TD%\p11.txt >nul || goto :err
findstr /R "big"   %TD%\p11.txt >nul || goto :err

REM ----------------- TEST 13: -v ---------------------
echo === TEST 13: -v var=VAL assignment ===
awk.exe -v N=3 "BEGIN{print N, N+1}" > %TD%\p12.txt
findstr /R "^3 4" %TD%\p12.txt >nul || goto :err

REM ----------------- TEST 14: unknown option ---------
echo === TEST 14: unknown option exits 2 ===
awk.exe --we-dont-have-this-option 2>nul
if not errorlevel 2 goto :err

REM ----------------- TEST 15: -f ---------------------
echo === TEST 15: -f progfile ===
> %TD%\swp.awk echo { print $2, $1 }
awk.exe -f %TD%\swp.awk %TD%\tmp\b.txt > %TD%\p13.txt
for /f "usebackq delims=" %%a in (`type %TD%\p13.txt`) do set "_=%%a"
if not "%_%"=="b a" goto :err

REM ----------------- TEST 16: compound assign --------
echo === TEST 16: compound assign + arithmetic ===
> %TD%\p14.script echo { s=$1; s+=2; s*=3; print s }
awk.exe -f %TD%\p14.script %TD%\tmp\c.txt > %TD%\p14.txt
for /f "usebackq" %%a in (`type %TD%\p14.txt`) do set "_=%%a"
if not "%_%"=="21" goto :err

REM ----------------- TEST 17: FS regex ----------------
echo === TEST 17: FS regex /[,-]/ ===
> %TD%\p15.script echo {print NF, $2}
awk.exe -F"[,-]" -f %TD%\p15.script %TD%\tmp\fs.txt > %TD%\p15.txt
findstr /R "^4 b" %TD%\p15.txt >nul || goto :err

REM ----------------- TEST 18: OFS ORS ----------------
echo === TEST 18: OFS ORS ===
REM Build a script with `|` inside without cmd eating it:
set "_OFSV=|"
set "_ORSV=-"
> %TD%\p16.script echo BEGIN{OFS="!_OFSV!"; ORS="!_ORSV!"} {print $1,$2,$3}
awk.exe -f %TD%\p16.script %TD%\tmp\ofs.txt > %TD%\p16.txt
for /f "usebackq delims=" %%a in (`type %TD%\p16.txt`) do set "_=%%a"
if not "%_%"=="1|2|3-" goto :err

REM ----------------- TEST 19: sub --------------------
echo === TEST 19: sub replaces first ===
> %TD%\p17.script echo { sub(/aba/, "X"); print }
awk.exe -f %TD%\p17.script %TD%\tmp\sub.txt > %TD%\p17.txt
for /f "usebackq" %%a in (`type %TD%\p17.txt`) do set "_=%%a"
if not "%_%"=="Xba" goto :err

REM ----------------- TEST 20: gsub -------------------
echo === TEST 20: gsub replaces global ===
> %TD%\p18.script echo { gsub(/aa/, "Z"); print }
awk.exe -f %TD%\p18.script %TD%\tmp\gs.txt > %TD%\p18.txt
for /f "usebackq" %%a in (`type %TD%\p18.txt`) do set "_=%%a"
if not "%_%"=="ZbbZ" goto :err

REM ----------------- TEST 21: and+~ ------------------
echo === TEST 21: and + ~ filter ===
REM Avoid literal && on echo line. Use nested ifs in awk:
> %TD%\p19.script echo { if ($1 ~ /o/) if ($3 ^>= 0) print $1 }
awk.exe -F: -f %TD%\p19.script %TD%\pw.txt > %TD%\p19.txt
findstr /R "^root" %TD%\p19.txt >nul || goto :err

REM ----------------- TEST 22: not filter -------------
echo === TEST 22: ^! filter ===
REM Write `!` character literally via set /p (DelayedExpansion-safe):
<nul > %TD%\p20.script set /p "={ if ("
<nul >>%TD%\p20.script set /p "=^!"
>>    %TD%\p20.script echo ($3 == 0)) print $1 }
awk.exe -F: -f %TD%\p20.script %TD%\pw.txt > %TD%\p20.txt
findstr /R "^alice" %TD%\p20.txt >nul || goto :err

REM ----------------- TEST 23: case/length ------------
echo === TEST 23: tolower / toupper / length ===
> %TD%\p21.script echo { print tolower($0), toupper($0), length($0) }
awk.exe -f %TD%\p21.script %TD%\tmp\c2.txt > %TD%\p21.txt
findstr "hello HELLO 5" %TD%\p21.txt >nul || goto :err

echo.
echo ALL TESTS PASSED.
if exist "%TD%" rmdir /s /q "%TD%"
exit /b 0

:err
echo.
echo TEST FAILED.
if exist "%TD%" echo Leaving %TD% for inspection.
exit /b 1
