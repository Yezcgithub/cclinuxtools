@echo off
REM Build and test script for dirname.c (Windows). PowerShell harness embedded.
setlocal

echo ============================================
echo     dirname.c Build Script for Windows
echo ============================================

set "CC=gcc"
set "CFLAGS=-O2 -std=c99 -Wall -Wextra -DWIN32_LEAN_AND_MEAN"
set "OUTPUT=dirname.exe"
set "SOURCE=dirname.c"

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
echo   Running tests (40 cases, self-extracted harness)...
echo ============================================

set "SCRIPTFILE=%temp%\cclinuxtools_dirname_tests_%random%.ps1"
powershell -NoProfile -Command "$c=Get-Content -Raw -LiteralPath '%~f0'; $m='@@CCTOOLS_DIRNAME_PS1_MARKER@@'; $i=$c.LastIndexOf($m); if($i -lt 0){exit 1}; $h=$c.Substring($i+$m.Length); [IO.File]::WriteAllText('%SCRIPTFILE%',$h,(New-Object Text.UTF8Encoding $false))"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPTFILE%" -DirnameExe "%CD%\dirname.exe"
set "TESTRC=%errorlevel%"

del /f /q "%SCRIPTFILE%" 2>nul
if exist _build_test rmdir /s /q _build_test 2>nul
if exist dirname.dSYM rmdir /s /q dirname.dSYM 2>nul

if "%TESTRC%"=="0" (exit /b 0) else (exit /b 1)
REM ==========================================================================
REM  PowerShell test harness follows (extracted automatically).
REM  DO NOT EDIT MANUALLY.
REM ==========================================================================
@@CCTOOLS_DIRNAME_PS1_MARKER@@
# Test harness for dirname.c (PowerShell) - byte-level compare.
param(
    [string]$DirnameExe = ".\dirname.exe"
)
$ErrorActionPreference = "Continue"
$UTF8NoBOM = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $UTF8NoBOM
[Console]::OutputEncoding = $UTF8NoBOM
if ([string]::IsNullOrEmpty($DirnameExe)) { $DirnameExe = "dirname.exe" }
try {
    $__item = Get-Item -LiteralPath $DirnameExe -ErrorAction Stop
    [string]$ExeAbs = $__item.FullName
} catch {
    try {
        $__alt = Join-Path (Get-Location).Path $DirnameExe
        $__item = Get-Item -LiteralPath $__alt -ErrorAction Stop
        [string]$ExeAbs = $__item.FullName
    } catch {
        [string]$ExeAbs = $DirnameExe
    }
}
Write-Host ("Harness: DirnameExe = " + $ExeAbs)
$tdir = Join-Path $env:TEMP ("cclinuxtools_dirname_" + [IO.Path]::GetRandomFileName().Substring(0,8))
if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Path $tdir -Force | Out-Null
Write-Host ("Harness: tdir = " + $tdir)
[int]$script:pass = 0
[int]$script:fail = 0
function test-ok([string]$name) { Write-Host ("  [PASS] " + $name); $script:pass++ }
function test-fail([string]$name, [string]$r = "") { Write-Host ("  [FAIL] " + $name + "  " + $r); $script:fail++ }
function A([string]$s) { return ,$UTF8NoBOM.GetBytes($s) }
function BytesEq([byte[]]$a, [byte[]]$b) {
    if ($null -eq $a -or $null -eq $b) { return $false }
    if ($a.Length -ne $b.Length) { return $false }
    for ($i = 0; $i -lt $a.Length; $i++) { if ($a[$i] -ne $b[$i]) { return $false } }
    return $true
}
function Run-Dirname([string[]]$a) {
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $ExeAbs
    $sbb = New-Object Text.StringBuilder
    foreach ($t in $a) {
        if ($t -match '[\s"]' -or $t -eq "") {
            [void]$sbb.Append('"' + $t.Replace('"','\"') + '" ')
        } else {
            [void]$sbb.Append($t + ' ')
        }
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
function chk([string]$name, [string[]]$argsA, [byte[]]$expectedBytes) {
    $r = Run-Dirname $argsA
    if ($r.exit -ne 0) {
        test-fail $name ("exit=" + $r.exit)
        return
    }
    if (BytesEq $r.bytes $expectedBytes) { test-ok $name }
    else { test-fail $name ("gotLen=" + $r.bytes.Length + " expLen=" + $expectedBytes.Length) }
}
function chk-err([string]$name, [string[]]$argsA) {
    $r = Run-Dirname $argsA
    if ($r.exit -ne 0) { test-ok $name }
    else { test-fail $name ("expected non-zero exit, got " + $r.exit) }
}
function chk-contains([string]$name, [string[]]$argsA, [string]$needle, [int]$wantExit = 0) {
    $r = Run-Dirname $argsA
    $txt = $UTF8NoBOM.GetString($r.bytes)
    if ($r.exit -eq $wantExit -and $txt.Contains($needle)) { test-ok $name }
    else { test-fail $name ("exit=" + $r.exit + " contains=" + (-not -not $txt.Contains($needle))) }
}

# Build str + LF (newline)
function AL([string]$s) {
    $b = [byte[]]@(($UTF8NoBOM.GetBytes($s)) + ,10)
    return ,$b
}
# Build str + NUL
function AZ([string]$s) {
    $b = [byte[]]@(($UTF8NoBOM.GetBytes($s)) + ,0)
    return ,$b
}

Write-Host "--- T01 /usr/bin/sort ---"
chk "T01 usr-bin-sort" @("/usr/bin/sort") (AL "/usr/bin")

Write-Host "--- T02 stdio.h ---"
chk "T02 stdio.h" @("stdio.h") (AL ".")

Write-Host "--- T03 dir1/str dir2/str ---"
chk "T03 two names" @("dir1/str","dir2/str") ([byte[]](($UTF8NoBOM.GetBytes("dir1`ndir2`n"))))

Write-Host "--- T04 root / ---"
chk "T04 root" @("/") (AL "/")

Write-Host "--- T05 // ---"
chk "T05 dbl-slash" @("//") (AL "/")

Write-Host "--- T06 /// ---"
chk "T06 tpl-slash" @("///") (AL "/")

Write-Host "--- T07 //// ---"
chk "T07 quad-slash" @("////") (AL "/")

Write-Host "--- T08 a ---"
chk "T08 a" @("a") (AL ".")

Write-Host "--- T09 a/b ---"
chk "T09 a-b" @("a/b") (AL "a")

Write-Host "--- T10 a/b/c ---"
chk "T10 a-b-c" @("a/b/c") (AL "a/b")

Write-Host "--- T11 a/b/c/ ---"
chk "T11 trailing" @("a/b/c/") (AL "a/b")

Write-Host "--- T12 a/b/c// ---"
chk "T12 dbl-trail" @("a/b/c//") (AL "a/b")

Write-Host "--- T13 /a ---"
chk "T13 slash-a" @("/a") (AL "/")

Write-Host "--- T14 /a/b ---"
chk "T14 slash-a-b" @("/a/b") (AL "/a")

Write-Host "--- T15 /a/b/ ---"
chk "T15 slash-a-b-trail" @("/a/b/") (AL "/a")

Write-Host "--- T16 d/f ---"
chk "T16 d-f" @("d/f") (AL "d")

Write-Host "--- T17 /d/f ---"
chk "T17 slash-d-f" @("/d/f") (AL "/d")

Write-Host "--- T18 d/f/ ---"
chk "T18 d-f-trail" @("d/f/") (AL "d")

Write-Host "--- T19 . ---"
chk "T19 dot" @(".") (AL ".")

Write-Host "--- T20 .. ---"
chk "T20 dotdot" @("..") (AL ".")

Write-Host "--- T21 empty ---"
chk "T21 empty" @("") (AL ".")

Write-Host "--- T22 foo/bar ---"
chk "T22 foo-bar" @("foo/bar") (AL "foo")

Write-Host "--- T23 foo/bar/ ---"
chk "T23 foo-bar-trail" @("foo/bar/") (AL "foo")

Write-Host "--- T24 foo/bar// ---"
chk "T24 foo-bar-dbl-trail" @("foo/bar//") (AL "foo")

Write-Host "--- T25 -z /a/b ---"
chk "T25 -z" @("-z","/a/b") (AZ "/a")

Write-Host "--- T26 --zero /a/b ---"
chk "T26 --zero" @("--zero","/a/b") (AZ "/a")

Write-Host "--- T27 --help ---"
chk-contains "T27 --help" @("--help") "Usage: dirname"

Write-Host "--- T28 --version ---"
chk-contains "T28 --version" @("--version") "dirname"

Write-Host "--- T29 no operand ---"
chk-err "T29 no operand" @()

Write-Host "--- T30 unknown option ---"
chk-err "T30 unknown opt" @("--thisopt")

Write-Host "--- T31 invalid short ---"
chk-err "T31 invalid short" @("-X")

Write-Host "--- T32 -- /a/b ---"
chk "T32 dashdash" @("--","/a/b") (AL "/a")

Write-Host "--- T33 - ---"
chk "T33 dash" @("-") (AL ".")

Write-Host "--- T34 -- - ---"
chk "T34 dashdash-dash" @("--","-") (AL ".")

Write-Host "--- T35 a/b a/c a/d ---"
chk "T35 three names" @("a/b","a/c","a/d") ([byte[]](($UTF8NoBOM.GetBytes("a`na`na`n"))))

Write-Host "--- T36 -z a/b c/d ---"
chk "T36 -z two" @("-z","a/b","c/d") ([byte[]](($UTF8NoBOM.GetBytes("a")) + ,0 + ($UTF8NoBOM.GetBytes("c")) + ,0))

Write-Host "--- T37 two abs paths ---"
chk "T37 two abs" @("/usr/bin/sort","/usr/lib/libc.so") ([byte[]](($UTF8NoBOM.GetBytes("/usr/bin`n/usr/lib`n"))))

Write-Host "--- T38 a/. ---"
chk "T38 a-dot" @("a/.") (AL "a")

Write-Host "--- T39 a/.. ---"
chk "T39 a-dotdot" @("a/..") (AL "a")

Write-Host "--- T40 ./a ---"
chk "T40 dot-slash-a" @("./a") (AL ".")

Write-Host ""
Write-Host "============================================"
Write-Host ("  Results: PASS=" + $script:pass + "  FAIL=" + $script:fail)
Write-Host "============================================"

if (Test-Path $tdir) { Remove-Item -Recurse -Force $tdir -ErrorAction SilentlyContinue }
if ($script:fail -gt 0) { exit 1 } else { exit 0 }
