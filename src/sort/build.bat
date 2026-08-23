@echo off
REM Build and test script for sort.c (Windows). Test harness embedded after the batch portion.
setlocal

echo ============================================
echo     sort.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=sort.exe"
set "SOURCE=sort.c"

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

set "SCRIPTFILE=%temp%\cclinuxtools_sort_tests_%random%.ps1"
for /f "delims=:" %%a in ('findstr /n /b /c:"@@CCTOOLS_SORT_PS1_MARKER@@" "%~f0"') do set /a SKIP=%%a
more +%SKIP% "%~f0" > "%SCRIPTFILE%" 2>nul

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -SortExe "%CD%\sort.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist sort.dSYM rmdir /s /q sort.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)

REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_SORT_PS1_MARKER@@
# Test harness for sort.c (PowerShell)
param(
    [string]$SortExe = "$PSScriptRoot\sort.exe"
)

$ErrorActionPreference = 'Continue'
$NL = [Environment]::NewLine

$tdir = Join-Path $PSScriptRoot '_build_test'
if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir }
New-Item -ItemType Directory -Path $tdir | Out-Null

[int]$script:pass = 0
[int]$script:fail = 0

function test-ok([string]$name) { Write-Host ("  [PASS] " + $name); $script:pass++ }
function test-fail([string]$name, [string]$r = '') { Write-Host ("  [FAIL] " + $name + "  " + $r); $script:fail++ }
function write-vec([string]$path, [string[]]$lines) {
    $c = ($lines -join $NL) + $NL
    [IO.File]::WriteAllText($path, $c, [Text.Encoding]::ASCII)
}
function first([string]$path) {
    if (!(Test-Path $path)) { return '' }
    $l = [IO.File]::ReadAllLines($path)
    if ($l.Length -gt 0) { return $l[0] }; return ''
}
function last([string]$path) {
    if (!(Test-Path $path)) { return '' }
    $l = [IO.File]::ReadAllLines($path)
    if ($l.Length -gt 0) { return $l[$l.Length - 1] }; return ''
}
function countlines([string]$path) {
    if (!(Test-Path $path)) { return 0 }
    $l = [IO.File]::ReadAllLines($path)
    return $l.Length
}
function lineAt([string]$path, [int]$idx) {
    if (!(Test-Path $path)) { return '' }
    $l = [IO.File]::ReadAllLines($path)
    if ($idx -lt $l.Length) { return $l[$idx] }; return ''
}

function Invoke-Sort([string[]]$a) {
    $out = & $SortExe @a 2>&1
    foreach ($o in $out) {
        if ($o -is [System.Management.Automation.ErrorRecord]) {
            [Console]::Error.WriteLine($o.ToString())
        } else {
            Write-Output $o
        }
    }
}

# Input fixtures
write-vec (Join-Path $tdir 'words.txt')      @('banana','apple','cherry')
write-vec (Join-Path $tdir 'nums.txt')       @('10','2','1','20','3')
write-vec (Join-Path $tdir 'human.txt')      @('2K','1G','500','3M')
write-vec (Join-Path $tdir 'mixed_case.txt') @('apple','Apple','banana','Banana')
write-vec (Join-Path $tdir 'two_fields.txt') @('3 1','1 5','2 3','1 2')
[IO.File]::WriteAllBytes((Join-Path $tdir 'empty.txt'), [byte[]]@())
write-vec (Join-Path $tdir 'single.txt')     @('single')
write-vec (Join-Path $tdir 'sorted.txt')     @('aaa','bbb','ccc')
write-vec (Join-Path $tdir 'reverse_sorted.txt') @('ccc','bbb','aaa')
write-vec (Join-Path $tdir 'months.txt')     @('MAR','JAN','DEC','AUG','xxx')
write-vec (Join-Path $tdir 'versions.txt')   @('v1.10','v1.2','v2.0','v1.0.1','v1.0')

$out = Join-Path $tdir 'o.txt'
$words = Join-Path $tdir 'words.txt'
$nums  = Join-Path $tdir 'nums.txt'
$human = Join-Path $tdir 'human.txt'
$mc    = Join-Path $tdir 'mixed_case.txt'
$tf    = Join-Path $tdir 'two_fields.txt'
$empt  = Join-Path $tdir 'empty.txt'
$sing  = Join-Path $tdir 'single.txt'
$srt   = Join-Path $tdir 'sorted.txt'
$rev   = Join-Path $tdir 'reverse_sorted.txt'
$mons  = Join-Path $tdir 'months.txt'
$vers  = Join-Path $tdir 'versions.txt'

Write-Host '--- T1 basic sort first ---'
Invoke-Sort @($words) > $out
if ((first $out) -eq 'apple') { test-ok T1 } else { test-fail T1 ('got=' + (first $out)) }

Write-Host '--- T2 basic sort last ---'
Invoke-Sort @($words) > $out
if ((last $out) -eq 'cherry') { test-ok T2 } else { test-fail T2 ('got=' + (last $out)) }

Write-Host '--- T3 -r reverse first ---'
Invoke-Sort @('-r', $words) > $out
if ((first $out) -eq 'cherry') { test-ok T3 } else { test-fail T3 ('got=' + (first $out)) }

Write-Host '--- T4 -n numeric first ---'
Invoke-Sort @('-n', $nums) > $out
if ((first $out) -eq '1') { test-ok T4 } else { test-fail T4 ('got=' + (first $out)) }

Write-Host '--- T5 -n numeric last ---'
Invoke-Sort @('-n', $nums) > $out
if ((last $out) -eq '20') { test-ok T5 } else { test-fail T5 ('got=' + (last $out)) }

Write-Host '--- T6 -h human first ---'
Invoke-Sort @('-h', $human) > $out
if ((first $out) -eq '500') { test-ok T6 } else { test-fail T6 ('got=' + (first $out)) }

Write-Host '--- T7 -h human last ---'
Invoke-Sort @('-h', $human) > $out
if ((last $out) -eq '1G') { test-ok T7 } else { test-fail T7 ('got=' + (last $out)) }

Write-Host '--- T8 -f ignore case ---'
Invoke-Sort @('-f', $mc) > $out; $f = (first $out).ToLower()
if ($f -eq 'apple') { test-ok T8 } else { test-fail T8 ('got=' + $f) }

Write-Host '--- T9 -k field 2 ---'
Invoke-Sort @('-k2,2', $tf) > $out
if ((first $out) -eq '3 1') { test-ok T9 } else { test-fail T9 ('got=' + (first $out)) }

Write-Host '--- T10 -t sep k2,2 ---'
Invoke-Sort @('-t ', '-k2,2', $tf) > $out
if ((first $out) -eq '3 1') { test-ok T10 } else { test-fail T10 ('got=' + (first $out)) }

Write-Host '--- T11 -u unique lines ---'
write-vec (Join-Path $tdir 'dups.txt') @('a','a','b','b','c'); Invoke-Sort @('-u', (Join-Path $tdir 'dups.txt')) > $out
if ((countlines $out) -eq 3) { test-ok T11 } else { test-fail T11 ('lines=' + (countlines $out)) }

Write-Host '--- T12 -o output with path ---'
$ofile = Join-Path $tdir 'out12.txt'; if (Test-Path $ofile) { Remove-Item $ofile }; Invoke-Sort @($words, '-o', $ofile) 2>&1 | Out-Null; $ok = (Test-Path $ofile); if ($ok) { $v = first $ofile } else { $v = '' }
if ($v -eq 'apple') { test-ok T12 } else { test-fail T12 ('got=' + $v) }

Write-Host '--- T13 -c sorted ok ---'
Invoke-Sort @('-c', $srt) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T13 } else { test-fail T13 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T14 -c unsorted non-zero ---'
Invoke-Sort @('-c', $rev) 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T14 } else { test-fail T14 }

Write-Host '--- T15 -C silent sorted ---'
Invoke-Sort @('-C', $srt) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T15 } else { test-fail T15 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T16 -C silent unsorted ---'
Invoke-Sort @('-C', $rev) 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T16 } else { test-fail T16 }

Write-Host '--- T17 --help contains Usage ---'
$r = (Invoke-Sort @('--help') 2>&1) | Out-String
if ($r -match 'Usage') { test-ok T17 } else { test-fail T17 }

Write-Host '--- T18 --version 1.0.0 ---'
$r = (Invoke-Sort @('--version') 2>&1) | Out-String
if ($r -match '1\.0\.0') { test-ok T18 } else { test-fail T18 }

Write-Host '--- T19 empty file ---'
Invoke-Sort @($empt) > $out
if ((countlines $out) -eq 0) { test-ok T19 } else { test-fail T19 ('lines=' + (countlines $out)) }

Write-Host '--- T20 single line ---'
Invoke-Sort @($sing) > $out
if ((first $out) -eq 'single') { test-ok T20 } else { test-fail T20 ('got=' + (first $out)) }

Write-Host '--- T21 stdin ---'
@('c','a','b') | & $SortExe > $out
if ((first $out) -eq 'a') { test-ok T21 } else { test-fail T21 ('got=' + (first $out)) }

Write-Host '--- T22 -rn combined first ---'
Invoke-Sort @('-rn', $nums) > $out
if ((first $out) -eq '20') { test-ok T22 } else { test-fail T22 ('got=' + (first $out)) }

Write-Host '--- T23 -k 1n ---'
write-vec (Join-Path $tdir 'keynum.txt') @('10 a','2 b','1 c'); Invoke-Sort @('-k1n', (Join-Path $tdir 'keynum.txt')) > $out
if ((first $out) -eq '1 c') { test-ok T23 } else { test-fail T23 ('got=' + (first $out)) }

Write-Host '--- T24 multi files 6 lines ---'
Invoke-Sort @($srt, $rev) > $out
if ((countlines $out) -eq 6) { test-ok T24 } else { test-fail T24 ('lines=' + (countlines $out)) }

Write-Host '--- T25 stable k1,1 ---'
write-vec (Join-Path $tdir 'stable.txt') @('b 1','a 1','a 2','b 2'); Invoke-Sort @('-s', '-k1,1', (Join-Path $tdir 'stable.txt')) > $out
if ((lineAt $out 1) -eq 'a 2') { test-ok T25 } else { test-fail T25 ('got=' + (lineAt $out 1)) }

Write-Host '--- T26 multi k1 k2n first ---'
write-vec (Join-Path $tdir 'mk.txt') @('b 2','a 2','a 1','b 1'); Invoke-Sort @('-k1,1', '-k2,2n', (Join-Path $tdir 'mk.txt')) > $out
if ((first $out) -eq 'a 1') { test-ok T26 } else { test-fail T26 ('got=' + (first $out)) }

Write-Host '--- T27 unknown long ---'
Invoke-Sort @('--unknown') 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T27 } else { test-fail T27 }

Write-Host '--- T28 unknown short ---'
Invoke-Sort @('-Z') 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T28 } else { test-fail T28 }

Write-Host '--- T29 k2,2r per-key rev ---'
write-vec (Join-Path $tdir 'kr.txt') @('3 a','1 c','2 b'); Invoke-Sort @('-k2,2r', (Join-Path $tdir 'kr.txt')) > $out
if ((first $out) -eq '1 c') { test-ok T29 } else { test-fail T29 ('got=' + (first $out)) }

Write-Host '--- T30 -m merge 6 lines ---'
Invoke-Sort @('-m', $srt, $srt) > $out
if ((countlines $out) -eq 6) { test-ok T30 } else { test-fail T30 ('lines=' + (countlines $out)) }

Write-Host '--- T32 -c -u strict dup ---'
write-vec (Join-Path $tdir 'dup_sorted.txt') @('a','a','b'); Invoke-Sort @('-c', '-u', (Join-Path $tdir 'dup_sorted.txt')) 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T32 } else { test-fail T32 }

Write-Host '--- T33 -M first non-month ---'
Invoke-Sort @('-M', $mons) > $out
if ((first $out) -eq 'xxx') { test-ok T33 } else { test-fail T33 ('got=' + (first $out)) }

Write-Host '--- T34 -M DEC last ---'
Invoke-Sort @('-M', $mons) > $out
if ((last $out) -eq 'DEC') { test-ok T34 } else { test-fail T34 ('got=' + (last $out)) }

Write-Host '--- T35 -V v2.0 last ---'
Invoke-Sort @('-V', $vers) > $out
if ((last $out) -eq 'v2.0') { test-ok T35 } else { test-fail T35 ('got=' + (last $out)) }

Write-Host '--- T36 -V 1.2 before 1.10(3) ---'
Invoke-Sort @('-V', $vers) > $out
if ((lineAt $out 2) -eq 'v1.2') { test-ok T36 } else { test-fail T36 ('got=' + (lineAt $out 2)) }

Write-Host '--- T37 --sort=numeric first ---'
Invoke-Sort @('--sort=numeric', $nums) > $out
if ((first $out) -eq '1') { test-ok T37 } else { test-fail T37 ('got=' + (first $out)) }

Write-Host '--- T38 --sort=month DEC last ---'
Invoke-Sort @('--sort=month', $mons) > $out
if ((last $out) -eq 'DEC') { test-ok T38 } else { test-fail T38 ('got=' + (last $out)) }

Write-Host '--- T39 order independence k,r ---'
$inp = @('b 3','a 1','c 2'); $a = (($inp | & $SortExe '-k2,2n' '-r') -join "`n").Trim(); $b = (($inp | & $SortExe '-r' '-k2,2n') -join "`n").Trim(); $equal = ($a -eq $b)
if ($equal) { test-ok T39 } else { test-fail T39 }

Write-Host '--- T40 --files0-from 6 lines ---'
$sf = $srt; [byte[]]$fb=[Text.Encoding]::ASCII.GetBytes($sf + [char]0 + $sf + [char]0); [IO.File]::WriteAllBytes((Join-Path $tdir 'flist.0'), $fb); Invoke-Sort @(('--files0-from=' + (Join-Path $tdir 'flist.0'))) > $out
if ((countlines $out) -eq 6) { test-ok T40 } else { test-fail T40 ('lines=' + (countlines $out)) }

Write-Host '--- T41 -k2V per-key V first ---'
write-vec (Join-Path $tdir 'vkey.txt') @('1 v1.10','2 v1.2','3 v1.0'); Invoke-Sort @('-k2V', (Join-Path $tdir 'vkey.txt')) > $out
if ((first $out) -eq '3 v1.0') { test-ok T41 } else { test-fail T41 ('got=' + (first $out)) }

Write-Host '--- T42 invalid --sort nz ---'
Invoke-Sort @('--sort=foobar') 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { test-ok T42 } else { test-fail T42 }

Write-Host '--- T43 -n implied -b lead ---'
write-vec (Join-Path $tdir 'leadb.txt') @('   5','  10','    1'); Invoke-Sort @('-n', (Join-Path $tdir 'leadb.txt')) > $out; $raw=[IO.File]::ReadAllText($out); $v=$raw.TrimStart()
if ($v.StartsWith('1')) { test-ok T43 } else { test-fail T43 ('got=' + $v) }

Write-Host '--- T44 --debug no crash ---'
Invoke-Sort @('-k2,2n', $tf, '--debug') 2> $null > $out
if ($LASTEXITCODE -eq 0) { test-ok T44 } else { test-fail T44 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T45 -R random no crash ---'
write-vec (Join-Path $tdir 'big.txt') @('zzz','aaa','mmm'); Invoke-Sort @('-R', (Join-Path $tdir 'big.txt')) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T45 } else { test-fail T45 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T46 --compress accepted ---'
Invoke-Sort @('--compress=cat', $srt) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T46 } else { test-fail T46 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T47 --parallel N accepted ---'
Invoke-Sort @('--parallel=4', $srt) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T47 } else { test-fail T47 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T48 --random-source ---'
Invoke-Sort @(('--random-source=' + (Join-Path $tdir 'nums.txt')), $srt) 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { test-ok T48 } else { test-fail T48 ('exit=' + $LASTEXITCODE) }

Write-Host '--- T49 -k -b flag ---'
write-vec (Join-Path $tdir 'kb.txt') @('a  10','b 2','c  1'); Invoke-Sort @('-k2b,2n', (Join-Path $tdir 'kb.txt')) > $out
if ((first $out) -eq 'c  1') { test-ok T49 } else { test-fail T49 ('got=' + (first $out)) }

Write-Host '--- T50 -o interleave args ---'
$of2=Join-Path $tdir 'o50.txt'; Invoke-Sort @('-o', $of2, '-k1,1', '-r', $words) 2>&1 | Out-Null; $v = first $of2
if ($v -eq 'cherry') { test-ok T50 } else { test-fail T50 ('got=' + $v) }

Write-Host '--- T31 -z zero-terminated ---'
[byte[]]$bytesT31 = 99, 0, 97, 0, 98, 0
[IO.File]::WriteAllBytes((Join-Path $tdir 'zero.txt'), $bytesT31)
$obinT31 = Join-Path $tdir 'o.bin'
$psi = New-Object Diagnostics.ProcessStartInfo
$psi.FileName = $SortExe
$psi.Arguments = '-z "' + (Join-Path $tdir 'zero.txt') + '"'
$psi.RedirectStandardOutput = $true
$psi.UseShellExecute = $false
$pr = [Diagnostics.Process]::Start($psi)
$mst = New-Object IO.MemoryStream
$pr.StandardOutput.BaseStream.CopyTo($mst)
$pr.WaitForExit()
$capturedT31 = $mst.ToArray()
$mst.Dispose()
[IO.File]::WriteAllBytes($obinT31, $capturedT31)
$readback = $capturedT31
$sbt = New-Object Text.StringBuilder
$sbt = New-Object Text.StringBuilder
foreach ($b in $readback) {
    if ($b -eq 0) { [void]$sbt.Append([char]10) } else { [void]$sbt.Append([char]$b) }
}
$text31 = $sbt.ToString()
$firstLine = ($text31 -split [char]10)[0]
if ($firstLine -eq 'a') { test-ok T31 } else { test-fail T31 ('first=' + $firstLine) }
Write-Host ''
Write-Host '============================================'
Write-Host ('  Test Results: PASS=' + $script:pass + '  FAIL=' + $script:fail)
Write-Host '============================================'
if ($script:fail -eq 0) { Write-Host '  All tests passed!'; exit 0 }
else { Write-Host '  Some tests failed!'; exit 1 }

