@echo off
REM Build and test script for head.c (Windows). PowerShell harness embedded.
setlocal

echo ============================================
echo     head.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=head.exe"
set "SOURCE=head.c"

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
    cl /O2 /std:c11 /W4 /DWIN32_LEAN_AND_MEAN %SOURCE% /Fe:%OUTPUT% /link kernel32.lib advapi32.lib
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
echo   Running tests (>= 45 cases, self-extracted harness)...
echo ============================================

set "SCRIPTFILE=%temp%\cclinuxtools_head_tests_%random%.ps1"
powershell -NoProfile -Command "$c=Get-Content -Raw -LiteralPath '%~f0'; $m='@@CCTOOLS_HEAD_PS1_MARKER@@'; $i=$c.LastIndexOf($m); if($i -lt 0){exit 1}; $h=$c.Substring($i+$m.Length); [IO.File]::WriteAllText('%SCRIPTFILE%',$h,(New-Object Text.UTF8Encoding $false))"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -HeadExe "%CD%\head.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist head.dSYM rmdir /s /q head.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)
REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_HEAD_PS1_MARKER@@
# Test harness for head.c (PowerShell) - byte-level compare. Fixtures in $env:TEMP.
param(
    [string]$HeadExe = ".\head.exe"
)
$ErrorActionPreference = "Continue"
$UTF8NoBOM = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $UTF8NoBOM
[Console]::OutputEncoding = $UTF8NoBOM
if ([string]::IsNullOrEmpty($HeadExe)) { $HeadExe = "head.exe" }
try {
    $__item = Get-Item -LiteralPath $HeadExe -ErrorAction Stop
    [string]$HeadAbs = $__item.FullName
} catch {
    try {
        $__alt = Join-Path (Get-Location).Path $HeadExe
        $__item = Get-Item -LiteralPath $__alt -ErrorAction Stop
        [string]$HeadAbs = $__item.FullName
    } catch {
        [string]$HeadAbs = $HeadExe
    }
}
Write-Host ("Harness: HeadExe = " + $HeadAbs)
$tdir = Join-Path $env:TEMP ("cclinuxtools_head_" + [IO.Path]::GetRandomFileName().Substring(0,8))
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
function Run-Head([string[]]$a, [string]$stdin = "") {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $HeadAbs
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
    if ($stdin -ne "") { $psi.RedirectStandardInput = $true }
    try { $pr = [Diagnostics.Process]::Start($psi) }
    catch { return [pscustomobject]@{ exit=999; bytes=[byte[]]@(); err=($_.Exception.Message) } }
    if ($stdin -ne "") {
        $inBytes = $UTF8NoBOM.GetBytes($stdin)
        $pr.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
        $pr.StandardInput.BaseStream.Flush()
        $pr.StandardInput.BaseStream.Close()
    }
    $outMst = New-Object IO.MemoryStream
    $copyTask = $pr.StandardOutput.BaseStream.CopyToAsync($outMst)
    $errTask  = $pr.StandardError.ReadToEndAsync()
    [void][Threading.Tasks.Task]::WaitAll($copyTask, $errTask)
    if (-not $pr.HasExited) { [void]$pr.WaitForExit(10000) }
    $b = $outMst.ToArray(); $e = $errTask.Result
    try { $outMst.Dispose() } catch {}
    return [pscustomobject]@{ exit=$pr.ExitCode; bytes=$b; err=$e }
}
function chk($name, $argsA, $expectedBytes, $note="") {
    $r = Run-Head $argsA
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
}

# Fixtures
$in12 = Join-Path $tdir "in12.txt"
write-vec-lf $in12 @("1","2","3","4","5","6","7","8","9","10","11","12")
$f2a = Join-Path $tdir "fa.txt"
write-vec-lf $f2a @("a1","a2","a3")
$f2b = Join-Path $tdir "fb.txt"
write-vec-lf $f2b @("b1","b2")
$f3c = Join-Path $tdir "fc.txt"
write-vec-lf $f3c @("c1","c2","c3","c4")
$empt = Join-Path $tdir "empty.txt"
[IO.File]::WriteAllBytes($empt, [byte[]]@())
$sing = Join-Path $tdir "single.txt"
write-vec-lf $sing @("only-line")
$nonl = Join-Path $tdir "nonl.txt"
[IO.File]::WriteAllText($nonl, "no newline at end", $UTF8NoBOM)
$zfile = Join-Path $tdir "z.txt"
[IO.File]::WriteAllBytes($zfile, [byte[]](120,0,121,0,122,0))   # x\0y\0z\0
$cn = Join-Path $tdir "cn.txt"
[IO.File]::WriteAllText($cn, "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n", $UTF8NoBOM)
$long = Join-Path $tdir "long.txt"
$sbl = New-Object Text.StringBuilder
for ($i = 1; $i -le 50; $i++) { [void]$sbl.Append("line" + $i + "`n") }
[IO.File]::WriteAllText($long, $sbl.ToString(), $UTF8NoBOM)

Write-Host "--- T01 default 10 lines ---"
chk "T01 default" @($in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n")
Write-Host "--- T02 -n 3 ---"
chk "T02 -n3" @("-n","3",$in12) (A "1`n2`n3`n")
Write-Host "--- T03 -n 0 ---"
chk "T03 -n0" @("-n","0",$in12) ([byte[]]@())
Write-Host "--- T04 -n -3 (all but last 3) ---"
chk "T04 -n-3" @("-n","-3",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n")
Write-Host "--- T05 -n +4 (first 4) ---"
chk "T05 -n+4" @("-n","+4",$in12) (A "1`n2`n3`n4`n")
Write-Host "--- T06 -n 15 (more than file) ---"
chk "T06 -n15" @("-n","15",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T07 --lines=3 ---"
chk "T07 --lines=3" @("--lines=3",$in12) (A "1`n2`n3`n")
Write-Host "--- T08 --lines=-2 ---"
chk "T08 --lines=-2" @("--lines=-2",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n")
Write-Host "--- T09 -c 5 ---"
chk "T09 -c5" @("-c","5",$in12) (A "1`n2`n3")
Write-Host "--- T10 -c 0 ---"
chk "T10 -c0" @("-c","0",$in12) ([byte[]]@())
Write-Host "--- T11 -c -5 (all but last 5 bytes) ---"
chk "T11 -c-5" @("-c","-5",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n1")
Write-Host "--- T12 -c +3 (first 3 bytes) ---"
chk "T12 -c+3" @("-c","+3",$in12) (A "1`n2")
Write-Host "--- T13 --bytes=4 ---"
chk "T13 --bytes=4" @("--bytes=4",$in12) (A "1`n2`n")
Write-Host "--- T14 -c 1b (512, clamped) ---"
chk "T14 -c1b" @("-c","1b",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T15 -c 1c (1 byte) ---"
chk "T15 -c1c" @("-c","1c",$in12) (A "1")
Write-Host "--- T16 -c 1w (2 bytes) ---"
chk "T16 -c1w" @("-c","1w",$in12) (A "1`n")
Write-Host "--- T17 -c 2kB (2000, clamped) ---"
chk "T17 -c2kB" @("-c","2kB",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T18 -c 1K (1024, clamped) ---"
chk "T18 -c1K" @("-c","1K",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T19 obsolete -5 (first 5 lines) ---"
chk "T19 obs -5" @("-5",$in12) (A "1`n2`n3`n4`n5`n")
Write-Host "--- T20 obsolete -5c (first 5 bytes) ---"
chk "T20 obs -5c" @("-5c",$in12) (A "1`n2`n3")
Write-Host "--- T21 obsolete -5q (quiet first 5) ---"
chk "T21 obs -5q" @("-5q",$in12) (A "1`n2`n3`n4`n5`n")
Write-Host "--- T22 obsolete -5v (verbose single) ---"
chk "T22 obs -5v" @("-5v",$in12) (A ("==> " + $in12 + " <==`n1`n2`n3`n4`n5`n"))
Write-Host "--- T23 two files headers ---"
$r23 = Run-Head @($f2a,$f2b)
$exp23 = A ("==> " + $f2a + " <==`na1`na2`na3`n==> " + $f2b + " <==`nb1`nb2`n")
if ($r23.exit -eq 0 -and (BytesEq $r23.bytes $exp23)) { test-ok "T23 two files" } else { test-fail "T23 two files" ("exit=$($r23.exit) len=$($r23.bytes.Length)/$($exp23.Length)") }
Write-Host "--- T24 -q two files (no headers) ---"
chk "T24 -q two" @("-q",$f2a,$f2b) (A "a1`na2`na3`nb1`nb2`n")
Write-Host "--- T25 --quiet two files ---"
chk "T25 --quiet two" @("--quiet",$f2a,$f2b) (A "a1`na2`na3`nb1`nb2`n")
Write-Host "--- T26 -v single (force header) ---"
chk "T26 -v single" @("-v",$f2a) (A ("==> " + $f2a + " <==`na1`na2`na3`n"))
Write-Host "--- T27 --verbose single ---"
chk "T27 --verbose" @("--verbose",$f2a) (A ("==> " + $f2a + " <==`na1`na2`na3`n"))
Write-Host "--- T28 three files headers ---"
$r28 = Run-Head @($f2a,$f2b,$f3c)
$exp28 = A ("==> " + $f2a + " <==`na1`na2`na3`n==> " + $f2b + " <==`nb1`nb2`n==> " + $f3c + " <==`nc1`nc2`nc3`nc4`n")
if ($r28.exit -eq 0 -and (BytesEq $r28.bytes $exp28)) { test-ok "T28 three files" } else { test-fail "T28 three files" ("exit=$($r28.exit) len=$($r28.bytes.Length)/$($exp28.Length)") }
Write-Host "--- T29 stdin via - ---"
$r29b = Run-Head @("-n","2","-") "x1`nx2`nx3`n"
if ($r29b.exit -eq 0 -and (BytesEq $r29b.bytes (A "x1`nx2`n"))) { test-ok "T29 stdin -" } else { test-fail "T29 stdin -" ("exit=$($r29b.exit)") }
Write-Host "--- T30 stdin default ---"
$r30 = Run-Head @("-") "a`nb`n"
if ($r30.exit -eq 0 -and (BytesEq $r30.bytes (A "a`nb`n"))) { test-ok "T30 stdin default" } else { test-fail "T30 stdin default" ("exit=$($r30.exit)") }
Write-Host "--- T31 -z first 2 records ---"
chk "T31 -z n2" @("-z","-n","2",$zfile) ([byte[]](120,0,121,0))
Write-Host "--- T32 --zero-terminated n1 ---"
chk "T32 --zero n1" @("--zero-terminated","-n","1",$zfile) ([byte[]](120,0))
Write-Host "--- T33 empty file ---"
chk "T33 empty" @($empt) ([byte[]]@())
Write-Host "--- T34 single line ---"
chk "T34 single" @($sing) (A "only-line`n")
Write-Host "--- T35 -n 1 single ---"
chk "T35 -n1 single" @("-n","1",$sing) (A "only-line`n")
Write-Host "--- T36 no trailing newline ---"
chk "T36 nonl" @($nonl) (A "no newline at end")
Write-Host "--- T37 -c 5 nonl ---"
chk "T37 -c5 nonl" @("-c","5",$nonl) (A "no ne")
Write-Host "--- T38 -n -1 (all but last 1) ---"
chk "T38 -n-1" @("-n","-1",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n")
Write-Host "--- T39 -c -1 (all but last byte) ---"
chk "T39 -c-1" @("-c","-1",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12")
Write-Host "--- T40 -n 50 long file (all) ---"
$r40 = Run-Head @("-n","50",$long)
$e40 = New-Object Text.StringBuilder
for ($i = 1; $i -le 50; $i++) { [void]$e40.Append("line" + $i + "`n") }
if ($r40.exit -eq 0 -and (BytesEq $r40.bytes (A $e40.ToString()))) { test-ok "T40 -n50 long" } else { test-fail "T40 -n50 long" ("exit=$($r40.exit) len=$($r40.bytes.Length)") }
Write-Host "--- T41 -n -10 long (all but last 10) ---"
$r41 = Run-Head @("-n","-10",$long)
$e41 = New-Object Text.StringBuilder
for ($i = 1; $i -le 40; $i++) { [void]$e41.Append("line" + $i + "`n") }
if ($r41.exit -eq 0 -and (BytesEq $r41.bytes (A $e41.ToString()))) { test-ok "T41 -n-10 long" } else { test-fail "T41 -n-10 long" ("exit=$($r41.exit) len=$($r41.bytes.Length)") }
Write-Host "--- T42 attached -n5 ---"
chk "T42 -n5 attached" @("-n5",$in12) (A "1`n2`n3`n4`n5`n")
Write-Host "--- T43 attached -c5 ---"
chk "T43 -c5 attached" @("-c5",$in12) (A "1`n2`n3")
Write-Host "--- T44 combined -qn5 ---"
chk "T44 -qn5" @("-qn5",$f2a,$f2b) (A "a1`na2`na3`nb1`nb2`n")
Write-Host "--- T45 -n5q (invalid, should error) ---"
$r45 = Run-Head @("-n5q",$in12)
if ($r45.exit -ne 0) { test-ok "T45 -n5q err" } else { test-fail "T45 -n5q err" ("exit=$($r45.exit) (expected non-zero)") }
Write-Host "--- T46 -- ---"
chk "T46 -- file" @("--",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n")
Write-Host "--- T47 -c 1KB ---"
chk "T47 -c1KB" @("-c","1KB",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T48 -c 1MiB (clamped) ---"
chk "T48 -c1MiB" @("-c","1MiB",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T49 multiple -n (last wins) ---"
chk "T49 multi -n last wins" @("-n","2","-n","5",$in12) (A "1`n2`n3`n4`n5`n")
Write-Host "--- T50 -n 1 -c 4 (last unit wins) ---"
chk "T50 -n1 -c4 last wins" @("-n","1","-c","4",$in12) (A "1`n2`n")
Write-Host "--- T51 --help ---"
$r51 = Run-Head @("--help")
$h51 = $UTF8NoBOM.GetString($r51.bytes)
if ($r51.exit -eq 0 -and $h51.Contains("Usage: head")) { test-ok "T51 --help" } else { test-fail "T51 --help" ("exit=$($r51.exit)") }
Write-Host "--- T52 --version ---"
$r52 = Run-Head @("--version")
$h52 = $UTF8NoBOM.GetString($r52.bytes) + " " + $r52.err
if ($r52.exit -eq 0 -and $h52.ToLowerInvariant().Contains("head")) { test-ok "T52 --version" } else { test-fail "T52 --version" ("exit=$($r52.exit)") }
Write-Host "--- T53 missing file ---"
$r53 = Run-Head @("does_not_exist_zzz.txt")
if ($r53.exit -eq 1 -and $r53.bytes.Length -eq 0 -and $r53.err.Contains("cannot open")) { test-ok "T53 missing" } else { test-fail "T53 missing" ("exit=$($r53.exit) bytes=$($r53.bytes.Length) err=$($r53.err)") }
Write-Host "--- T54 -q -v (verbose overrides quiet, single file) ---"
chk "T54 -q -v single" @("-q","-v",$f2a) (A ("==> " + $f2a + " <==`na1`na2`na3`n"))
Write-Host "--- T55 -v two files ---"
$r55 = Run-Head @("-v",$f2a,$f2b)
$exp55 = A ("==> " + $f2a + " <==`na1`na2`na3`n==> " + $f2b + " <==`nb1`nb2`n")
if ($r55.exit -eq 0 -and (BytesEq $r55.bytes $exp55)) { test-ok "T55 -v two" } else { test-fail "T55 -v two" ("exit=$($r55.exit)") }
Write-Host "--- T56 -c 0 (empty output) ---"
chk "T56 -c0" @("-c","0",$in12) ([byte[]]@())
Write-Host "--- T57 -n 0 (empty output) ---"
chk "T57 -n0" @("-n","0",$in12) ([byte[]]@())

Write-Host ""
Write-Host ("============================================")
Write-Host ("  Results: PASS=" + $script:pass + "  FAIL=" + $script:fail)
Write-Host ("============================================")
if ($script:fail -gt 0) { exit 1 } else { exit 0 }
