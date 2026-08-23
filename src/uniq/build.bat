@echo off
REM Build and test script for uniq.c (Windows). Test harness embedded after the batch portion.
setlocal

echo ============================================
echo     uniq.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=uniq.exe"
set "SOURCE=uniq.c"

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
echo   Running tests (50 cases, self-extracted harness)...
echo ============================================

set "SCRIPTFILE=%temp%\cclinuxtools_uniq_tests_%random%.ps1"
for /f "delims=:" %%a in ('findstr /n /b /c:"@@CCTOOLS_UNIQ_PS1_MARKER@@" "%~f0"') do set /a SKIP=%%a
more +%SKIP% "%~f0" > "%SCRIPTFILE%" 2>nul

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -UniqExe "%CD%\uniq.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist uniq.dSYM rmdir /s /q uniq.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)

REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_UNIQ_PS1_MARKER@@
# Test harness for uniq.c (PowerShell) - byte-level compare to avoid PS5 encoding mess
param(
    [string]$UniqExe = "$PSScriptRoot\uniq.exe"
)
$ErrorActionPreference = 'Continue'

$tdir = Join-Path $PSScriptRoot '_build_test'
if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir }
New-Item -ItemType Directory -Path $tdir | Out-Null

[int]$script:pass = 0
[int]$script:fail = 0

function test-ok([string]$name) { Write-Host ("  [PASS] " + $name); $script:pass++ }
function test-fail([string]$name, [string]$r = '') { Write-Host ("  [FAIL] " + $name + "  " + $r); $script:fail++ }
function A([string]$s) { return [Text.Encoding]::ASCII.GetBytes($s) }

function write-vec-lf([string]$path, [string[]]$lines) {
    $c = ($lines -join "`n") + "`n"
    [IO.File]::WriteAllText($path, $c, [Text.Encoding]::ASCII)
}

function BytesEq([byte[]]$a, [byte[]]$b) {
    if ($null -eq $a -or $null -eq $b) { return $false }
    if ($a.Length -ne $b.Length) { return $false }
    for ($i = 0; $i -lt $a.Length; $i++) { if ($a[$i] -ne $b[$i]) { return $false } }
    return $true
}

function Run-Uniq([string[]]$a, [byte[]]$inBytes=$null) {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $UniqExe
    $sb = New-Object Text.StringBuilder
    foreach ($t in $a) {
        if ($t -match '[\s"]') { [void]$sb.Append('"' + $t.Replace('"','\"') + '" ') }
        else { [void]$sb.Append($t + ' ') }
    }
    $psi.Arguments = $sb.ToString().TrimEnd()
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardInput = $inBytes -ne $null
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $pr = $null
    try { $pr = [Diagnostics.Process]::Start($psi) }
    catch { return [pscustomobject]@{ exit=999; bytes=[byte[]]@() } }

    # Start async readers for stdout (byte-exact) and stderr before touching
    # stdin so the child process never deadlocks on pipe buffer pressure.
    $outMst = New-Object IO.MemoryStream
    $copyTask = $pr.StandardOutput.BaseStream.CopyToAsync($outMst)
    $errTask  = $pr.StandardError.ReadToEndAsync()

    if ($inBytes -ne $null) {
        try {
            $pr.StandardInput.BaseStream.Write($inBytes, 0, $inBytes.Length)
            $pr.StandardInput.BaseStream.Flush()
        } catch {}
        try { $pr.StandardInput.BaseStream.Close() } catch {}
    }

    [void][Threading.Tasks.Task]::WaitAll($copyTask, $errTask)
    if (-not $pr.HasExited) { [void]$pr.WaitForExit(10000) }
    $b = $outMst.ToArray()
    try { $outMst.Dispose() } catch {}
    return [pscustomobject]@{ exit=$pr.ExitCode; bytes=$b }
}
# ---------- Fixtures ----------
$in7 = Join-Path $tdir 'in7.txt'
write-vec-lf $in7 @('a','a','b','c','c','c','d')

$fc = Join-Path $tdir 'fc.txt'
write-vec-lf $fc @('  1 foo','  2 foo','  3 bar','  4 bar')

$sc = Join-Path $tdir 'sc.txt'
write-vec-lf $sc @('01abc','02abc','03xyz')

$ic = Join-Path $tdir 'ic.txt'
write-vec-lf $ic @('Apple','apple','APPLE','Banana')

$wc = Join-Path $tdir 'wc.txt'
write-vec-lf $wc @('apple pie','apple sauce','banana')

$tr = Join-Path $tdir 'tr.txt'
write-vec-lf $tr @('a b c 1','x y z 1','p q r 2')

$empt = Join-Path $tdir 'empty.txt'
[IO.File]::WriteAllBytes($empt, [byte[]]@())

$sing = Join-Path $tdir 'single.txt'
write-vec-lf $sing @('only')

$zb = Join-Path $tdir 'z.bin'
[IO.File]::WriteAllBytes($zb, [byte[]]([char]'x',0,[char]'x',0,[char]'y',0))

$op  = Join-Path $tdir 'o.txt'

function chk($name, $argsA, $expectedBytes, $note='') {
    $r = Run-Uniq $argsA
    $eq = BytesEq $r.bytes $expectedBytes
    if ($eq) { test-ok $name }
    else {
        $msg = ('exit=' + $r.exit + ' gotLen=' + $r.bytes.Length + ' expLen=' + $expectedBytes.Length)
        if ($note -ne '') { $msg = $msg + '  ' + $note }
        test-fail $name $msg
    }
}

Write-Host '--- T1  basic default ---'
chk 'T1' @($in7) (A "a`nb`nc`nd`n")

Write-Host '--- T2  -c count ---'
chk 'T2' @('-c',$in7) (A "      2 a`n      1 b`n      3 c`n      1 d`n")

Write-Host '--- T3  -d repeated only, first copy ---'
chk 'T3' @('-d',$in7) (A "a`nc`n")

Write-Host '--- T4  -D all repeated copies ---'
chk 'T4' @('-D',$in7) (A "a`na`nc`nc`nc`n")

Write-Host '--- T5  -u unique only ---'
chk 'T5' @('-u',$in7) (A "b`nd`n")

Write-Host '--- T6  -f1 skip fields ---'
chk 'T6' @('-f1',$fc) (A "  1 foo`n  3 bar`n")

Write-Host '--- T7  -f 1 (space form) ---'
chk 'T7' @('-f','1',$fc) (A "  1 foo`n  3 bar`n")

Write-Host '--- T8  --skip-fields=1 long form ---'
chk 'T8' @('--skip-fields=1',$fc) (A "  1 foo`n  3 bar`n")

Write-Host '--- T9  -s2 skip chars ---'
chk 'T9' @('-s2',$sc) (A "01abc`n03xyz`n")

Write-Host '--- T10 --skip-chars=2 ---'
chk 'T10' @('--skip-chars=2',$sc) (A "01abc`n03xyz`n")

Write-Host '--- T11 -i ignore case ---'
chk 'T11' @('-i',$ic) (A "Apple`nBanana`n")

Write-Host '--- T12 --ignore-case ---'
chk 'T12' @('--ignore-case',$ic) (A "Apple`nBanana`n")

Write-Host '--- T13 -w5 check chars ---'
chk 'T13' @('-w5',$wc) (A "apple pie`nbanana`n")

Write-Host '--- T14 --check-chars=5 ---'
chk 'T14' @('--check-chars=5',$wc) (A "apple pie`nbanana`n")

Write-Host '--- T15 -3 traditional skip-fields ---'
chk 'T15' @('-3',$tr) (A "a b c 1`np q r 2`n")

Write-Host '--- T16 --all-repeated + positional OUTPUT ---'
$p16out = Join-Path $tdir 't16_out.txt'
if (Test-Path $p16out) { Remove-Item $p16out }
$r16 = Run-Uniq @('--all-repeated=separate',$in7,$p16out)
$raw16 = if (Test-Path $p16out) { [IO.File]::ReadAllBytes($p16out) } else { [byte[]]@() }
$exp16 = A "a`na`n`nc`nc`nc`n"
if ($r16.exit -eq 0 -and (BytesEq $raw16 $exp16)) { test-ok 'T16' }
else { test-fail 'T16' ('len=' + $raw16.Length + '/' + $exp16.Length) }

Write-Host '--- T17 -z + -u (unique only, NUL-delimited) ---'
$p17 = Join-Path $tdir 't17.bin'
[IO.File]::WriteAllBytes($p17, [byte[]]([char]'p',0,[char]'q',0,[char]'q',0,[char]'r',0,[char]'r',0,[char]'s',0))
chk 'T17' @('-z','-u',$p17) ([byte[]]([char]'p',0,[char]'s',0))

Write-Host '--- T18 was positional OUTPUT file ---'
if ($true) { <# placeholder to keep numbering stable #> }
Write-Host '--- T18b skip-chars + check-chars combined (-s N -w N) ---'
$p18b = Join-Path $tdir 't18b.txt'
write-vec-lf $p18b @('__alpha__','__alphB__','__beta1__','__beta2__')
chk 'T18b' @('-s2','-w4',$p18b) (A "__alpha__`n__beta1__`n")
Write-Host '--- T18 positional OUTPUT file ---'
if (Test-Path $op) { Remove-Item $op }
$r = Run-Uniq @($in7,$op)
$raw = if (Test-Path $op) { [IO.File]::ReadAllBytes($op) } else { [byte[]]@() }
if ((BytesEq $raw (A "a`nb`nc`nd`n")) -and $r.exit -eq 0) { test-ok 'T18' } else { test-fail 'T18' ('len=' + $raw.Length + ' exit=' + $r.exit) }

Write-Host '--- T19 empty input ---'
chk 'T19' @($empt) ([byte[]]@())

Write-Host '--- T20 single record ---'
chk 'T20' @($sing) (A "only`n")

Write-Host '--- T21 -z --zero-terminated with -c ---'
chk 'T21' @('-z','-c',$zb) (A "      2 x`0      1 y`0")

Write-Host '--- T22 -z -D ---'
chk 'T22' @('-z','-D',$zb) ([byte[]]([char]'x',0,[char]'x',0))

Write-Host '--- T23 -z -d (single copy of repeated) ---'
chk 'T23' @('-z','-d',$zb) ([byte[]]([char]'x',0))

Write-Host '--- T24 -z -u (unique) ---'
chk 'T24' @('-z','-u',$zb) ([byte[]]([char]'y',0))

Write-Host '--- T25 --group (default separate) ---'
chk 'T25' @('--group',$in7) (A "a`na`n`nb`n`nc`nc`nc`n`nd`n")

Write-Host '--- T26 --group=prepend ---'
chk 'T26' @('--group=prepend',$in7) (A "`na`na`n`nb`n`nc`nc`nc`n`nd`n")

Write-Host '--- T27 --group=append ---'
chk 'T27' @('--group=append',$in7) (A "a`na`n`nb`n`nc`nc`nc`n`nd`n`n")

Write-Host '--- T28 --group=both ---'
chk 'T28' @('--group=both',$in7) (A "`na`na`n`nb`n`nc`nc`nc`n`nd`n`n")

Write-Host '--- T29 --all-repeated (default none) === -D ---'
chk 'T29' @('--all-repeated',$in7) (A "a`na`nc`nc`nc`n")

Write-Host '--- T30 --all-repeated=prepend ---'
chk 'T30' @('--all-repeated=prepend',$in7) (A "`na`na`n`nc`nc`nc`n")

Write-Host '--- T31 --all-repeated=separate ---'
chk 'T31' @('--all-repeated=separate',$in7) (A "a`na`n`nc`nc`nc`n")

Write-Host '--- T32 --count + glued count -c -f1 ---'
chk 'T32' @('-c','-f1',$fc) (A "      2   1 foo`n      2   3 bar`n")

Write-Host '--- T33 -D + -f1 (all repeated copies with skip-fields) ---'
$p33 = Join-Path $tdir 't33.txt'
write-vec-lf $p33 @('1 a','2 a','3 b','x b','5 c')
chk 'T33' @('-D','-f1',$p33) (A "1 a`n2 a`n3 b`nx b`n")

Write-Host '--- T34 --help ---'
$r = Run-Uniq @('--help')
$txt = [Text.Encoding]::UTF8.GetString($r.bytes)
if ($txt -match 'Usage' -and $r.exit -eq 0) { test-ok 'T34' } else { test-fail 'T34' ('exit=' + $r.exit) }

Write-Host '--- T35 --version ---'
$r = Run-Uniq @('--version')
$txt = [Text.Encoding]::UTF8.GetString($r.bytes)
if ($txt -match '1\.0\.0' -and $r.exit -eq 0) { test-ok 'T35' } else { test-fail 'T35' ('exit=' + $r.exit) }

Write-Host '--- T36 unknown long option -> non-zero ---'
$r = Run-Uniq @('--nosuchopt')
if ($r.exit -ne 0) { test-ok 'T36' } else { test-fail 'T36' ('exit=' + $r.exit) }

Write-Host '--- T37 unknown short option -> non-zero ---'
$r = Run-Uniq @('-XX')
if ($r.exit -ne 0) { test-ok 'T37' } else { test-fail 'T37' ('exit=' + $r.exit) }

Write-Host '--- T38 incompatible -c -d ---'
$r = Run-Uniq @('-c','-d',$in7)
if ($r.exit -ne 0) { test-ok 'T38' } else { test-fail 'T38' ('exit=' + $r.exit) }

Write-Host '--- T39 -cdiu multi-mode conflict ---'
$r = Run-Uniq @('-cdiu',$in7)
if ($r.exit -ne 0) { test-ok 'T39' } else { test-fail 'T39' ('exit=' + $r.exit) }

Write-Host '--- T40 --skip-fields invalid (missing arg) ---'
$r = Run-Uniq @('--skip-fields')
if ($r.exit -ne 0) { test-ok 'T40' } else { test-fail 'T40' ('exit=' + $r.exit) }

Write-Host '--- T41 --skip-chars invalid number ---'
$r = Run-Uniq @('--skip-chars=abc')
if ($r.exit -ne 0) { test-ok 'T41' } else { test-fail 'T41' ('exit=' + $r.exit) }

Write-Host '--- T42 extra positional operand ---'
$r = Run-Uniq @($in7, $op, 'junk')
if ($r.exit -ne 0) { test-ok 'T42' } else { test-fail 'T42' ('exit=' + $r.exit) }

Write-Host '--- T43 -w2 short glued ---'
$tmp = Join-Path $tdir 't43.txt'
write-vec-lf $tmp @('ab1','ab2','ab3','cd1')
chk 'T43' @('-w2',$tmp) (A "ab1`ncd1`n")

Write-Host '--- T44 combined -if1 (ignore case + skip 1 field) ---'
$tmp = Join-Path $tdir 't44.txt'
write-vec-lf $tmp @('1 APPLE','2 apple','3 Banana','4 banana')
chk 'T44' @('-if1',$tmp) (A "1 APPLE`n3 Banana`n")

Write-Host '--- T45 non-existent input file ---'
$r = Run-Uniq @((Join-Path $tdir 'nope.txt'))
if ($r.exit -ne 0) { test-ok 'T45' } else { test-fail 'T45' ('exit=' + $r.exit) }

Write-Host '--- T46 single group all repeated via -D ---'
$tmp = Join-Path $tdir 't46.txt'
write-vec-lf $tmp @('q','q','q')
chk 'T46' @('-D',$tmp) (A "q`nq`nq`n")

Write-Host '--- T47 -d on all-unique input => empty ---'
$tmp = Join-Path $tdir 't47.txt'
write-vec-lf $tmp @('p','q','r')
chk 'T47' @('-d',$tmp) ([byte[]]@())

Write-Host '--- T48 -u on all-unique input => same input ---'
chk 'T48' @('-u',$tmp) (A "p`nq`nr`n")

Write-Host '--- T49 --all-repeated=invalid method ---'
$r = Run-Uniq @('--all-repeated=nosuch',$in7)
if ($r.exit -ne 0) { test-ok 'T49' } else { test-fail 'T49' ('exit=' + $r.exit) }

Write-Host '--- T50 --group=invalid method ---'
$r = Run-Uniq @('--group=nosuch',$in7)
if ($r.exit -ne 0) { test-ok 'T50' } else { test-fail 'T50' ('exit=' + $r.exit) }

Write-Host ''
Write-Host '============================================'
Write-Host ('  Test Results: PASS=' + $script:pass + '  FAIL=' + $script:fail)
Write-Host '============================================'
if ($script:fail -eq 0) { Write-Host '  All tests passed!'; exit 0 }
else { Write-Host '  Some tests failed!'; exit 1 }
