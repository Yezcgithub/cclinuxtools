@echo off
REM Build and test script for tail.c (Windows). PowerShell harness embedded.
setlocal

echo ============================================
echo     tail.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=tail.exe"
set "SOURCE=tail.c"

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
    cl /O2 /std:c11 /W4 /DWIN32_LEAN_AND_MEAN %SOURCE% /Fe:%OUTPUT% /link kernel32.lib advapi32.lib shell32.lib user32.lib
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

set "SCRIPTFILE=%temp%\cclinuxtools_tail_tests_%random%.ps1"
powershell -NoProfile -Command "$c=Get-Content -Raw -LiteralPath '%~f0'; $m='@@CCTOOLS_TAIL_PS1_MARKER@@'; $i=$c.LastIndexOf($m); if($i -lt 0){exit 1}; $h=$c.Substring($i+$m.Length); [IO.File]::WriteAllText('%SCRIPTFILE%',$h,(New-Object Text.UTF8Encoding $false))"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -TailExe "%CD%\tail.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist tail.dSYM rmdir /s /q tail.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)
REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_TAIL_PS1_MARKER@@
# Test harness for tail.c (PowerShell) - byte-level compare. Fixtures in $env:TEMP.
param(
    [string]$TailExe = ".\tail.exe"
)
$ErrorActionPreference = "Continue"
$UTF8NoBOM = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $UTF8NoBOM
[Console]::OutputEncoding = $UTF8NoBOM
if ([string]::IsNullOrEmpty($TailExe)) { $TailExe = "tail.exe" }
try {
    $__item = Get-Item -LiteralPath $TailExe -ErrorAction Stop
    [string]$TailAbs = $__item.FullName
} catch {
    try {
        $__alt = Join-Path (Get-Location).Path $TailExe
        $__item = Get-Item -LiteralPath $__alt -ErrorAction Stop
        [string]$TailAbs = $__item.FullName
    } catch {
        [string]$TailAbs = $TailExe
    }
}
Write-Host ("Harness: TailExe = " + $TailAbs)
$tdir = Join-Path $env:TEMP ("cclinuxtools_tail_" + [IO.Path]::GetRandomFileName().Substring(0,8))
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
function Run-Tail([string[]]$a, [string]$stdin = "") {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $TailAbs
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
    $r = Run-Tail $argsA
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
$fa = Join-Path $tdir "fa.txt"
write-vec-lf $fa @("a1","a2","a3")
$fb = Join-Path $tdir "fb.txt"
write-vec-lf $fb @("b1","b2")
$fc = Join-Path $tdir "fc.txt"
write-vec-lf $fc @("c1","c2","c3","c4")
$empt = Join-Path $tdir "empty.txt"
[IO.File]::WriteAllBytes($empt, [byte[]]@())
$sing = Join-Path $tdir "single.txt"
write-vec-lf $sing @("only-line")
$nonl = Join-Path $tdir "nonl.txt"
[IO.File]::WriteAllText($nonl, "no newline at end", $UTF8NoBOM)
$zfile = Join-Path $tdir "z.txt"
[IO.File]::WriteAllBytes($zfile, [byte[]](120,0,121,0,122,0))   # x\0y\0z\0
$byte10 = Join-Path $tdir "bytetest.txt"
[IO.File]::WriteAllText($byte10, "helloworld", $UTF8NoBOM)
# long.txt: 100000-byte single line spanning read chunks (regression for chunk bug)
$long = Join-Path $tdir "long.txt"
$sbl = New-Object Text.StringBuilder
[void]$sbl.Append("a" * 50000)
[void]$sbl.Append("`n")
[void]$sbl.Append("a" * 49999)
[IO.File]::WriteAllText($long, $sbl.ToString(), $UTF8NoBOM)
$long2 = Join-Path $tdir "long2.txt"
$sbl2 = New-Object Text.StringBuilder
for ($i = 1; $i -le 50; $i++) { [void]$sbl2.Append("line" + $i + "`n") }
[IO.File]::WriteAllText($long2, $sbl2.ToString(), $UTF8NoBOM)

Write-Host "--- T01 default 10 lines (last 10) ---"
chk "T01 default" @($in12) (A "3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T02 -n 3 (last 3 lines) ---"
chk "T02 -n3" @("-n","3",$in12) (A "10`n11`n12`n")
Write-Host "--- T03 -n 0 (no output) ---"
chk "T03 -n0" @("-n","0",$in12) ([byte[]]@())
Write-Host "--- T04 -n +3 (start at line 3 to end) ---"
chk "T04 -n+3" @("-n","+3",$fa) (A "a3`n")
Write-Host "--- T05 -n +1 on 100k long line (chunk regression) ---"
$r5 = Run-Tail @("-n","+1",$long)
if ($r5.exit -eq 0 -and $r5.bytes.Length -eq 100000) { test-ok "T05 long line intact" } else { test-fail "T05 long line intact" ("exit=$($r5.exit) len=$($r5.bytes.Length)") }
Write-Host "--- T06 -c 5 (last 5 bytes) ---"
chk "T06 -c5" @("-c","5",$byte10) (A "world")
Write-Host "--- T07 -c +6 (start at byte 6) ---"
chk "T07 -c+6" @("-c","+6",$byte10) (A "world")
Write-Host "--- T08 -c 1K (last 1024 bytes of 100k file) ---"
$r8 = Run-Tail @("-c","1K",$long)
if ($r8.exit -eq 0 -and $r8.bytes.Length -eq 1024) { test-ok "T08 -c1K" } else { test-fail "T08 -c1K" ("exit=$($r8.exit) len=$($r8.bytes.Length)") }
Write-Host "--- T09 obsolete -3 (last 3 lines) ---"
chk "T09 obs -3" @("-3",$in12) (A "10`n11`n12`n")
Write-Host "--- T10 obsolete +2l (start at line 2) ---"
chk "T10 obs +2l" @("+2l",$fa) (A "a2`na3`n")
Write-Host "--- T11 obsolete -5c (last 5 bytes) ---"
chk "T11 obs -5c" @("-5c",$byte10) (A "world")
Write-Host "--- T12 obsolete -3c (last 3 bytes) ---"
chk "T12 obs -3c" @("-3c",$byte10) (A "rld")
Write-Host "--- T13 stdin default (last 10) ---"
$r13 = Run-Tail @("-") "p1`np2`np3`n"
if ($r13.exit -eq 0 -and (BytesEq $r13.bytes (A "p1`np2`np3`n"))) { test-ok "T13 stdin" } else { test-fail "T13 stdin" ("exit=$($r13.exit)") }
Write-Host "--- T14 stdin via - ---"
$r14 = Run-Tail @("-") "x`n"
if ($r14.exit -eq 0 -and (BytesEq $r14.bytes (A "x`n"))) { test-ok "T14 stdin dash" } else { test-fail "T14 stdin dash" ("exit=$($r14.exit)") }
Write-Host "--- T15 multi-file headers ---"
$r15 = Run-Tail @("-n","1",$fa,$fb)
$exp15 = A ("==> " + $fa + " <==`na3`n`n==> " + $fb + " <==`nb2`n")
if ($r15.exit -eq 0 -and (BytesEq $r15.bytes $exp15)) { test-ok "T15 multi-file" } else { test-fail "T15 multi-file" ("exit=$($r15.exit)") }
Write-Host "--- T16 -q suppresses headers ---"
chk "T16 -q" @("-q","-n","1",$fa,$fb) (A "a3`nb2`n")
Write-Host "--- T17 -v forces headers even single file ---"
chk "T17 -v single" @("-v","-n","1",$fa) (A ("==> " + $fa + " <==`na3`n"))
Write-Host "--- T18 empty file ---"
chk "T18 empty" @($empt) ([byte[]]@())
Write-Host "--- T19 no trailing newline ---"
chk "T19 nonl" @($nonl) (A "no newline at end")
Write-Host "--- T20 -z last 1 record ---"
chk "T20 -z n1" @("-z","-n","1",$zfile) ([byte[]](122,0))
Write-Host "--- T21 --help ---"
$r21 = Run-Tail @("--help")
$h21 = $UTF8NoBOM.GetString($r21.bytes)
if ($r21.exit -eq 0 -and $h21.Contains("Usage: tail")) { test-ok "T21 --help" } else { test-fail "T21 --help" ("exit=$($r21.exit)") }
Write-Host "--- T22 --version ---"
$r22 = Run-Tail @("--version")
$h22 = $UTF8NoBOM.GetString($r22.bytes) + " " + $r22.err
if ($r22.exit -eq 0 -and $h22.ToLowerInvariant().Contains("tail")) { test-ok "T22 --version" } else { test-fail "T22 --version" ("exit=$($r22.exit)") }
Write-Host "--- T23 nonexistent file error ---"
$r23 = Run-Tail @("does_not_exist_zzz.txt")
if ($r23.exit -ne 0 -and $r23.err.Contains("cannot open")) { test-ok "T23 missing" } else { test-fail "T23 missing" ("exit=$($r23.exit) err=$($r23.err)") }
Write-Host "--- T24 invalid -n value rejected ---"
$r24 = Run-Tail @("-n","abc",$in12)
if ($r24.exit -ne 0) { test-ok "T24 invalid -n" } else { test-fail "T24 invalid -n" ("exit=$($r24.exit) (expected non-zero)") }
Write-Host "--- T25 -n 50 long2 (all lines) ---"
$r25 = Run-Tail @("-n","50",$long2)
$e25 = $sbl2.ToString()
if ($r25.exit -eq 0 -and (BytesEq $r25.bytes (A $e25))) { test-ok "T25 -n50 long2" } else { test-fail "T25 -n50 long2" ("exit=$($r25.exit) len=$($r25.bytes.Length)") }
Write-Host "--- T26 -n +45 long2 (last 6) ---"
$r26 = Run-Tail @("-n","+45",$long2)
$e26 = New-Object Text.StringBuilder
for ($i = 45; $i -le 50; $i++) { [void]$e26.Append("line" + $i + "`n") }
if ($r26.exit -eq 0 -and (BytesEq $r26.bytes (A $e26.ToString()))) { test-ok "T26 -n+45 long2" } else { test-fail "T26 -n+45 long2" ("exit=$($r26.exit)") }
Write-Host "--- T27 -c 5b (last 5*512=2560, clamped to file size 341) ---"
$r27 = Run-Tail @("-c","5b",$long2)
if ($r27.exit -eq 0 -and $r27.bytes.Length -eq 341) { test-ok "T27 -c5b clamp" } else { test-fail "T27 -c5b clamp" ("exit=$($r27.exit) len=$($r27.bytes.Length)") }
Write-Host "--- T28 -c 2w (last 4 bytes) ---"
chk "T28 -c2w" @("-c","2w",$byte10) (A "orld")
Write-Host "--- T29 -c 1c (last 1 byte) ---"
chk "T29 -c1c" @("-c","1c",$byte10) (A "d")
Write-Host "--- T30 --lines=2 ---"
chk "T30 --lines=2" @("--lines=2",$in12) (A "11`n12`n")
Write-Host "--- T31 --bytes=3 ---"
chk "T31 --bytes=3" @("--bytes=3",$byte10) (A "rld")
Write-Host "--- T32 -n 15 (more than file lines, all) ---"
chk "T32 -n15" @("-n","15",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T33 -n +15 (start beyond file => empty) ---"
chk "T33 -n+15" @("-n","+15",$fa) ([byte[]]@())
Write-Host "--- T34 three files headers ---"
$r34 = Run-Tail @("-n","1",$fa,$fb,$fc)
$exp34 = A ("==> " + $fa + " <==`na3`n`n==> " + $fb + " <==`nb2`n`n==> " + $fc + " <==`nc4`n")
if ($r34.exit -eq 0 -and (BytesEq $r34.bytes $exp34)) { test-ok "T34 three files" } else { test-fail "T34 three files" ("exit=$($r34.exit)") }
Write-Host "--- T35 -- end of options ---"
chk "T35 -- file" @("--",$in12) (A "3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T36 -n5 attached ---"
chk "T36 -n5 attached" @("-n5",$in12) (A "8`n9`n10`n11`n12`n")
Write-Host "--- T37 -c5 attached ---"
chk "T37 -c5 attached" @("-c5",$byte10) (A "world")
Write-Host "--- T38 combined -qn1 ---"
chk "T38 -qn1" @("-qn1",$fa,$fb) (A "a3`nb2`n")
Write-Host "--- T39 -c 1KB (clamped) ---"
chk "T39 -c1KB" @("-c","1KB",$in12) (A "1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n")
Write-Host "--- T40 -c 1M (clamped) ---"
chk "T40 -c1M" @("-c","1M",$byte10) (A "helloworld")
Write-Host "--- T41 multiple -n (last wins) ---"
chk "T41 multi -n last wins" @("-n","2","-n","3",$in12) (A "10`n11`n12`n")
Write-Host "--- T42 -n1 -c4 (last unit wins => last 4 bytes) ---"
chk "T42 -n1 -c4 last wins" @("-n","1","-c","4",$byte10) (A "orld")
Write-Host "--- T43 -q -v single (verbose overrides quiet) ---"
chk "T43 -q -v single" @("-q","-v","-n","1",$fa) (A ("==> " + $fa + " <==`na3`n"))
Write-Host "--- T44 -n -1 (last 1 line) ---"
chk "T44 -n-1" @("-n","-1",$in12) (A "12`n")
Write-Host "--- T45 -c -1 (last 1 byte) ---"
chk "T45 -c-1" @("-c","-1",$byte10) (A "d")
Write-Host "--- T46 -n 1 single ---"
chk "T46 -n1 single" @("-n","1",$sing) (A "only-line`n")
Write-Host "--- T47 -c 5 nonl (last 5 bytes) ---"
chk "T47 -c5 nonl" @("-c","5",$nonl) (A "t end")
Write-Host "--- T48 --zero-terminated n2 (last 2 records) ---"
chk "T48 --zero n2" @("--zero-terminated","-n","2",$zfile) ([byte[]](121,0,122,0))
Write-Host "--- T49 -v two files ---"
$r49 = Run-Tail @("-v","-n","1",$fa,$fb)
$exp49 = A ("==> " + $fa + " <==`na3`n`n==> " + $fb + " <==`nb2`n")
if ($r49.exit -eq 0 -and (BytesEq $r49.bytes $exp49)) { test-ok "T49 -v two" } else { test-fail "T49 -v two" ("exit=$($r49.exit)") }
Write-Host "--- T50 -n 0 -c 0 (last wins => empty) ---"
chk "T50 -n0 -c0" @("-n","0","-c","0",$in12) ([byte[]]@())

# Helper: start tail in background, perform an action, kill, capture output.
function Run-Tail-Follow([string[]]$a, [scriptblock]$action, [int]$waitMs = 1200) {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $TailAbs
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
    $pr = [Diagnostics.Process]::Start($psi)
    $outMst = New-Object IO.MemoryStream
    $copyTask = $pr.StandardOutput.BaseStream.CopyToAsync($outMst)
    $errTask  = $pr.StandardError.ReadToEndAsync()
    Start-Sleep -Milliseconds 400
    if ($action) { & $action }
    Start-Sleep -Milliseconds $waitMs
    $b = $outMst.ToArray()
    try { if (-not $pr.HasExited) { $pr.Kill(); $pr.WaitForExit(2000) } } catch {}
    try { [void][Threading.Tasks.Task]::WaitAll(@($copyTask, $errTask), 2000) } catch {}
    $e = $errTask.Result
    try { $outMst.Dispose() } catch {}
    return [pscustomobject]@{ exit=$pr.ExitCode; bytes=$b; err=$e }
}

Write-Host "--- T51 -f append (follow new content) ---"
$ff = Join-Path $tdir "follow.txt"
write-vec-lf $ff @("init1","init2")
$r51 = Run-Tail-Follow @("-n","2","-f","--sleep-interval","0.1",$ff) ({
    [IO.File]::AppendAllText($ff, "appended`n", $UTF8NoBOM)
}) 600
$exp51 = A "init1`ninit2`nappended`n"
if (BytesEq $r51.bytes $exp51) { test-ok "T51 -f append" } else { test-fail "T51 -f append" ("gotLen=" + $r51.bytes.Length) }

Write-Host "--- T52 --retry -F (file appears later) ---"
$fmis = Join-Path $tdir "missing_retry.txt"
if (Test-Path $fmis) { Remove-Item -Force $fmis }
$r52 = Run-Tail-Follow @("--retry","-F","--sleep-interval","0.1",$fmis) ({
    [IO.File]::WriteAllText($fmis, "appeared`n", $UTF8NoBOM)
}) 800
$exp52 = A "appeared`n"
if (BytesEq $r52.bytes $exp52) {
    test-ok "T52 --retry -F"
} else {
    test-fail "T52 --retry -F" ("gotLen=" + $r52.bytes.Length + " expLen=" + $exp52.Length)
}

Write-Host "--- T53 --pid terminates follow ---"
$dummy = Start-Process -FilePath "powershell" -ArgumentList "-NoProfile","-Command","Start-Sleep","-Seconds","30" -PassThru -WindowStyle Hidden
Start-Sleep -Milliseconds 200
$fp = Join-Path $tdir "pidfile.txt"
write-vec-lf $fp @("line1")
$r53 = Run-Tail-Follow @("-f","--pid",$dummy.Id,"--sleep-interval","0.1",$fp) ({
    [IO.File]::AppendAllText($fp, "line2`n", $UTF8NoBOM)
    Start-Sleep -Milliseconds 100
    try { if (-not $dummy.HasExited) { $dummy.Kill() } } catch {}
}) 600
if ($r53.bytes.Length -ge 0) {
    test-ok "T53 --pid term"
} else {
    test-fail "T53 --pid term" ("exit=" + $r53.exit)
}

Write-Host ""
Write-Host ("============================================")
Write-Host ("  Results: PASS=" + $script:pass + "  FAIL=" + $script:fail)
Write-Host ("============================================")
if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir -ErrorAction SilentlyContinue }
if ($script:fail -gt 0) { exit 1 } else { exit 0 }
