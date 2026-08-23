@echo off
REM Build and test script for less.c (Windows). PowerShell harness embedded.
setlocal

echo ============================================
echo     less.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=less.exe"
set "SOURCE=less.c"

where gcc >nul 2>nul && goto :cc_found
where clang >nul 2>nul && set "CC=clang" && goto :cc_found
where cl >nul 2>nul && goto :msvc_ok
echo [ERROR] No C compiler found (gcc, clang, or cl). Exiting.
exit /b 1

:cc_found
:msvc_ok

echo.
echo [1/3] Cleaning previous build...
if exist %OUTPUT% del /f /q %OUTPUT%
echo   Removed %OUTPUT%

echo.
echo [2/3] Detecting platform and compiling...
echo   Platform: Windows
echo   Compiler: %CC%

if "%CC%"=="cl" (
    cl /O2 /std:c11 /W4 /DWIN32_LEAN_AND_MEAN %SOURCE% /Fe:%OUTPUT% /link kernel32.lib advapi32.lib shell32.lib
    set "BERR=%errorlevel%"
    if exist %SOURCE:.c=.obj% del /f /q %SOURCE:.c=.obj%
    goto :build_done
)

%CC% %CFLAGS% -o %OUTPUT% %SOURCE%
set "BERR=%errorlevel%"

:build_done
if not "%BERR%"=="0" (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%
echo.
echo ============================================
echo   Running tests (>= 30 cases, self-extracted harness)...
echo ============================================

set "SCRIPTFILE=%temp%\cclinuxtools_less_tests_%random%.ps1"
for /f "delims=:" %%a in ('findstr /n /b /c:"@@CCTOOLS_LESS_PS1_MARKER@@" "%~f0"') do set /a SKIP=%%a
more +%SKIP% "%~f0" > "%SCRIPTFILE%" 2>nul

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -LessExe "%CD%\less.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist less.dSYM rmdir /s /q less.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)
REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_LESS_PS1_MARKER@@
# Test harness for less.c (PowerShell) - byte-level compare. Fixtures in $env:TEMP.
param(
    [string]$LessExe = ".\less.exe"
)
$ErrorActionPreference = "Continue"
$UTF8NoBOM = New-Object System.Text.UTF8Encoding($false)
if ([string]::IsNullOrEmpty($LessExe)) { $LessExe = "less.exe" }
try {
    $__item = Get-Item -LiteralPath $LessExe -ErrorAction Stop
    [string]$LessAbs = $__item.FullName
} catch {
    try {
        $__alt = Join-Path (Get-Location).Path $LessExe
        $__item = Get-Item -LiteralPath $__alt -ErrorAction Stop
        [string]$LessAbs = $__item.FullName
    } catch {
        [string]$LessAbs = $LessExe
    }
}
Write-Host ("Harness: LessExe = " + $LessAbs)
$tdir = Join-Path $env:TEMP ("cclinuxtools_less_" + [IO.Path]::GetRandomFileName().Substring(0,8))
if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Path $tdir -Force | Out-Null
Write-Host ("Harness: tdir = " + $tdir)
[int]$script:pass = 0
[int]$script:fail = 0
function test-ok([string]$name) { Write-Host ("  [PASS] " + $name); $script:pass++ }
function test-fail([string]$name, [string]$r = "") { Write-Host ("  [FAIL] " + $name + "  " + $r); $script:fail++ }
function A([string]$s) { return ,$UTF8NoBOM.GetBytes($s) }
function write-vec-lf([string]$path, [string[]]$lines) {
    $c = ($lines -join "`n") + "`n"
    [IO.File]::WriteAllText($path, $c, $UTF8NoBOM)
}
function BytesEq([byte[]]$a, [byte[]]$b) {
    if ($null -eq $a -or $null -eq $b) { return $false }
    if ($a.Length -ne $b.Length) { return $false }
    for ($i = 0; $i -lt $a.Length; $i++) { if ($a[$i] -ne $b[$i]) { return $false } }
    return $true
}
function Run-Less([string[]]$a) {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $LessAbs
    $sbb = New-Object Text.StringBuilder
    foreach ($t in $a) {
        if ($t -match '[\s"]') { [void]$sbb.Append('"' + $t.Replace('"','\"') + '" ') }
        else { [void]$sbb.Append($t + ' ') }
    }
    $psi.Arguments = $sbb.ToString().TrimEnd()
    $psi.WorkingDirectory = $tdir
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    try { $pr = [Diagnostics.Process]::Start($psi) }
    catch { return [pscustomobject]@{ exit=999; bytes=[byte[]]@(); err=($_.Exception.Message) } }
    $outMst = New-Object IO.MemoryStream
    $copyTask = $pr.StandardOutput.BaseStream.CopyToAsync($outMst)
    $errTask  = $pr.StandardError.ReadToEndAsync()
    [void][Threading.Tasks.Task]::WaitAll($copyTask, $errTask)
    if (-not $pr.HasExited) { [void]$pr.WaitForExit(10000) }
    $b = $outMst.ToArray(); $e = $errTask.Result
    try { $outMst.Dispose() } catch {}
    return [pscustomobject]@{ exit=$pr.ExitCode; bytes=$b; err=$e }
}
# Fixtures
$in7 = Join-Path $tdir "in7.txt"
write-vec-lf $in7 @("line1","line2","line3","line4")
$blanks = Join-Path $tdir "bl.txt"
write-vec-lf $blanks @("a","","","","","b","c")
$tabbed = Join-Path $tdir "tab.txt"
[IO.File]::WriteAllBytes($tabbed, [byte[]](97,9,48,49,9,49,50,51,52,53,54,55,56,9,69,78,68,10))
$multi = Join-Path $tdir "mf.txt"
write-vec-lf $multi @("one","two","three")
$f2 = Join-Path $tdir "f2.txt"
write-vec-lf $f2 @("header")
$needle = Join-Path $tdir "needle.txt"
write-vec-lf $needle @("skip1","skip2","find ME here","keep1","keep2")
$empt = Join-Path $tdir "empty.txt"
[IO.File]::WriteAllBytes($empt, [byte[]]@())
$sing = Join-Path $tdir "single.txt"
write-vec-lf $sing @("only-line")
$cn = Join-Path $tdir "cn.txt"
[IO.File]::WriteAllText($cn, "??`n??`n????`n", $UTF8NoBOM)
$long = Join-Path $tdir "long.txt"
write-vec-lf $long @("ABCDEFGHIJKLMNOPQRSTUVWXYZ","0123456789")
$logF = Join-Path $tdir "log.bin"
$logOverwrite = Join-Path $tdir "log_over.bin"
$ansi = Join-Path $tdir "ansi.txt"
[IO.File]::WriteAllBytes($ansi, [byte[]](65,27,91,51,49,109,66,27,91,48,109,67,10))
function chk($name, $argsA, $expectedBytes, $note="") {
    $r = Run-Less $argsA
    $eq = BytesEq $r.bytes $expectedBytes
    if ($r.exit -ne 0 -and -not $note.Contains("nonzero")) {
        test-fail $name ("exit=$($r.exit); len=$($r.bytes.Length)/$($expectedBytes.Length)")
        return
    }
    if ($eq) { test-ok $name }
    else {
        $msg = ("exit=" + $r.exit + " gotLen=" + $r.bytes.Length + " expLen=" + $expectedBytes.Length)
        if ($note -ne "") { $msg = $msg + "  " + $note }
        test-fail $name $msg
    }
}Write-Host "--- T01 basic -E ---"
chk "T01 basic -E" @("-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T02 -N numbers ---"
chk "T02 -N" @("-N","-E",$in7) (A ("      1 line1`n" + "      2 line2`n" + "      3 line3`n" + "      4 line4`n"))
Write-Host "--- T03 --LINE-NUMBERS ---"
chk "T03 --LINE-NUMBERS" @("--LINE-NUMBERS","-E",$in7) (A ("      1 line1`n" + "      2 line2`n" + "      3 line3`n" + "      4 line4`n"))
Write-Host "--- T04 -n default ---"
chk "T04 -n default" @("-n","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T05 -s squeeze ---"
chk "T05 -s" @("-s","-E",$blanks) (A "a`n`nb`nc`n")
Write-Host "--- T06 --squeeze-blank-lines ---"
chk "T06 --squeeze-blank-lines" @("--squeeze-blank-lines","-E",$blanks) (A "a`n`nb`nc`n")
Write-Host "--- T07 -x4 tabs ---"
chk "T07 -x4" @("-x","4","-E",$tabbed) (A "a   01  12345678    END`n")
Write-Host "--- T08 --tabs=8 ---"
chk "T08 --tabs=8" @("--tabs=8","-E",$tabbed) (A "a       01      12345678        END`n")
Write-Host "--- T09 cat two ---"
$r09 = Run-Less @("-E",$f2,$multi)
$exp09 = A "header`none`ntwo`nthree`n"
if ($r09.exit -eq 0 -and (BytesEq $r09.bytes $exp09)) { test-ok "T09 cat two files" } else { test-fail "T09 cat two files" ("exit=$($r09.exit) len=$($r09.bytes.Length)/$($exp09.Length)") }
Write-Host "--- T10 -e ---"
chk "T10 -e" @("-e",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T11 -F ---"
chk "T11 -F" @("-F",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T12 empty ---"
chk "T12 empty" @("-E",$empt) ([byte[]]@())
Write-Host "--- T13 single ---"
chk "T13 single" @("-E",$sing) (A "only-line`n")
Write-Host "--- T14 --help ---"
$r14 = Run-Less @("--help")
$h14 = $UTF8NoBOM.GetString($r14.bytes)
if ($r14.exit -eq 0 -and $h14.Contains("Usage: less")) { test-ok "T14 --help" } else { test-fail "T14 --help" ("exit=$($r14.exit)") }
Write-Host "--- T15 --version ---"
$r15 = Run-Less @("--version")
$h15 = $UTF8NoBOM.GetString($r15.bytes) + " " + $r15.err
if ($r15.exit -eq 0 -and $h15.ToLowerInvariant().Contains("less")) { test-ok "T15 --version" } else { test-fail "T15 --version" ("exit=$($r15.exit)") }
Write-Host "--- T16 -J ---"
chk "T16 -J" @("-J","-E",$in7) (A " line1`n line2`n line3`n line4`n")
Write-Host "--- T17 -J -N ---"
chk "T17 -J -N" @("-J","-N","-E",$in7) (A ("       1 line1`n" + "       2 line2`n" + "       3 line3`n" + "       4 line4`n"))
Write-Host "--- T18 -p ---"
chk "T18 -p find ME" @("-p","find ME","-E",$needle) (A "find ME here`nkeep1`nkeep2`n")
Write-Host "--- T19 -p case default ---"
chk "T19 -p case default" @("-p","find me","-E",$needle) ([byte[]]@())
Write-Host "--- T20 -i soft ---"
chk "T20 -i soft" @("-i","-p","find me","-E",$needle) (A "find ME here`nkeep1`nkeep2`n")
Write-Host "--- T21 -I hard ---"
chk "T21 -I hard" @("-I","-p","Find Me","-E",$needle) (A "find ME here`nkeep1`nkeep2`n")
Write-Host "--- T22 -i + upper exact ---"
chk "T22 -i + upper exact" @("-i","-p","find ME","-E",$needle) (A "find ME here`nkeep1`nkeep2`n")
Write-Host "--- T23 -p no match ---"
chk "T23 -p no match" @("-p","NOPE","-E",$needle) ([byte[]]@())
Write-Host "--- T24 100 lines ---"
$lst24 = @(); for ($k=0; $k -lt 100; $k++) { $lst24 += ("L" + $k) }
$t24f = Join-Path $tdir "h.txt"; write-vec-lf $t24f $lst24
$r24 = Run-Less @("-E",$t24f); $nls24 = 0
foreach ($by in $r24.bytes) { if ($by -eq 10) { $nls24++ } }
if ($r24.exit -eq 0 -and $nls24 -eq 100) { test-ok "T24 100 lines" } else { test-fail "T24 100 lines" ("nls=$nls24/100") }
Write-Host "--- T25 -S large cols ---"
chk "T25 -S" @("-S","-E",$long) (A "ABCDEFGHIJKLMNOPQRSTUVWXYZ`n0123456789`n")
Write-Host "--- T26 -Ns -x2 ---"
$r26 = Run-Less @("-Ns","-x","2","-E",$blanks)
$lines26 = ($UTF8NoBOM.GetString($r26.bytes) -split "`n", -1, "SimpleMatch")
if ($r26.exit -eq 0 -and $lines26.Count -eq 5) { test-ok "T26 -Ns -x2" } else { test-fail "T26 -Ns -x2" ("lines=$($lines26.Count)") }
Write-Host "--- T27 CJK byte exact ---"
$cnBytes = [IO.File]::ReadAllBytes($cn)
$r27 = Run-Less @("-E",$cn)
if ($r27.exit -eq 0 -and (BytesEq $r27.bytes $cnBytes)) { test-ok "T27 CJK exact" } else { test-fail "T27 CJK exact" ("len=$($r27.bytes.Length)/$($cnBytes.Length)") }
Write-Host "--- T28 invalid option ---"
$r28 = Run-Less @("--not-a-real-option-xyz","-E",$in7)
if ($r28.exit -ne 0) { test-ok "T28 invalid option" } else { test-fail "T28 invalid option" ("exit=$($r28.exit)") }
Write-Host "--- T29 -f ---"
chk "T29 -f" @("-f","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T30 -R ANSI ---"
$exp30 = [IO.File]::ReadAllBytes($ansi)
chk "T30 -R ANSI" @("-R","-E",$ansi) $exp30
Write-Host "--- T31 -r raw ---"
chk "T31 -r raw" @("-r","-E",$ansi) $exp30
Write-Host "--- T32 -~ ---"
chk "T32 -~" @("-~","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T33 +F ---"
chk "T33 +F" @("+F","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T34 +N ---"
chk "T34 +N" @("+N","-E",$in7) (A ("      1 line1`n" + "      2 line2`n" + "      3 line3`n" + "      4 line4`n"))
Write-Host "--- T35 missing file ---"
$r35 = Run-Less @("-E", (Join-Path $tdir "MISSING_NOPE.txt"))
if ($r35.exit -ne 0) { test-ok "T35 missing file" } else { test-fail "T35 missing file" ("exit=$($r35.exit)") }
Write-Host "--- T36 -o log ---"
if (Test-Path $logF) { Remove-Item $logF -Force -ErrorAction SilentlyContinue }
$r36 = Run-Less @("-o",$logF,"-E",$in7); $in36 = [IO.File]::ReadAllBytes($in7)
$logged36 = if (Test-Path $logF) { [IO.File]::ReadAllBytes($logF) } else { [byte[]]@() }
if ($r36.exit -eq 0 -and (BytesEq $r36.bytes $in36) -and (BytesEq $logged36 $in36)) { test-ok "T36 -o log" } else { test-fail "T36 -o log" ("out=$($r36.bytes.Length)/$($in36.Length) log=$($logged36.Length)") }
Write-Host "--- T37 -o refuse overwrite ---"
if (-not (Test-Path $logF)) { [IO.File]::WriteAllBytes($logF, [byte[]](65,10)) }
$r37 = Run-Less @("-o",$logF,"-E",$in7)
if ($r37.exit -ne 0) { test-ok "T37 -o refuse" } else { test-fail "T37 -o refuse" ("exit=$($r37.exit)") }
Write-Host "--- T38 -O overwrite ---"
if (Test-Path $logOverwrite) { Remove-Item $logOverwrite -Force -ErrorAction SilentlyContinue }
[IO.File]::WriteAllBytes($logOverwrite, [byte[]](88,88,88))
$r38 = Run-Less @("-O",$logOverwrite,"-E",$in7)
$logged38 = if (Test-Path $logOverwrite) { [IO.File]::ReadAllBytes($logOverwrite) } else { [byte[]]@() }
if ($r38.exit -eq 0 -and (BytesEq $logged38 $in36)) { test-ok "T38 -O overwrite" } else { test-fail "T38 -O overwrite" ("len=$($logged38.Length)") }
Write-Host "--- T39 --quit-at-eof ---"
chk "T39 --quit-at-eof" @("--quit-at-eof",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T40 --QUIT-AT-EOF ---"
chk "T40 --QUIT-AT-EOF" @("--QUIT-AT-EOF",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T41 --quit-if-one-screen ---"
chk "T41 --quit-if-one-screen" @("--quit-if-one-screen",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T42 -Rr ---"
chk "T42 -Rr" @("-R","-r","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T43 8192 lines ---"
$big = Join-Path $tdir "big.txt"; $sb43 = New-Object Text.StringBuilder
for ($k = 0; $k -lt 8192; $k++) { [void]$sb43.Append("x`n") }
[IO.File]::WriteAllText($big, $sb43.ToString(), [Text.Encoding]::ASCII)
$r43 = Run-Less @("-E",$big); $bigBytes = [IO.File]::ReadAllBytes($big)
if ($r43.exit -eq 0 -and (BytesEq $r43.bytes $bigBytes)) { test-ok "T43 8192 lines" } else { test-fail "T43 8192 lines" ("len=$($r43.bytes.Length)/$($bigBytes.Length)") }
[GC]::Collect()
Write-Host "--- T44 no trailing newline ---"
$t44p = Join-Path $tdir "tail.bin"; [IO.File]::WriteAllBytes($t44p, [byte[]](65,66,67))
$r44 = Run-Less @("-E",$t44p)
$exp44 = A "ABC"
if ($r44.exit -eq 0 -and (BytesEq $r44.bytes $exp44)) { test-ok "T44 no trailing nl" } else { test-fail "T44 no trailing nl" ("len=$($r44.bytes.Length)/3") }
Write-Host "--- T45 --force ---"
chk "T45 --force" @("--force","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T46 -K stub ---"
chk "T46 -K stub" @("-K","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T47 -L stub ---"
chk "T47 -L stub" @("-L","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T48 -gG stubs ---"
chk "T48 -gG stubs" @("-gG","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T49 --buffers=64 ---"
chk "T49 --buffers=64" @("--buffers=64","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T50 --max-back-scroll=2 ---"
chk "T50 --max-back-scroll=2" @("--max-back-scroll=2","-E",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host "--- T51 -P prompt ---"
chk "T51 -P prompt" @("-P","?f%f .?ltlines %ltL?ln (END)",$in7) (A "line1`nline2`nline3`nline4`n")
Write-Host ""
Write-Host "============================================"
Write-Host ("  PASS: " + $script:pass + "   FAIL: " + $script:fail)
Write-Host "============================================"
Remove-Item -Recurse -Force $tdir -ErrorAction SilentlyContinue
if ($script:fail -gt 0) { exit 1 } else { exit 0 }
