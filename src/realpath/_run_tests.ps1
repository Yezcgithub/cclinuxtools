# Comprehensive test suite for realpath.c
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'realpath.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function RunExe($argStr) {
    $p = Start-Process -FilePath $exe -ArgumentList $argStr -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
    $stdout = ''; $stderr = ''
    if (Test-Path out.tmp) { $raw = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stdout = $raw } }
    if (Test-Path err.tmp) { $raw = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stderr = $raw } }
    Remove-Item out.tmp, err.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

function ToFwd($p) { return $p -replace '\\', '/' }

function Cleanup {
    $files = @('out.tmp','err.tmp','stdin.tmp')
    foreach ($f in $files) { if (Test-Path $f) { Remove-Item $f -Force } }
    if (Test-Path '_test_dir') { Remove-Item '_test_dir' -Recurse -Force }
}

Cleanup

# Create test directory structure
$testDir = Join-Path $PSScriptRoot '_test_dir'
$subDir = Join-Path $testDir 'sub'
$deepDir = Join-Path $subDir 'deep'
New-Item -ItemType Directory -Force -Path $deepDir | Out-Null
[System.IO.File]::WriteAllText((Join-Path $testDir 'file.txt'), 'hello')
[System.IO.File]::WriteAllText((Join-Path $deepDir 'nested.txt'), 'content')

# Build expected paths using forward slashes
$fileTxt = ToFwd (Join-Path $testDir 'file.txt')
$subDirPath = ToFwd (Join-Path $testDir 'sub')
$nestedTxt = ToFwd (Join-Path $deepDir 'nested.txt')
$testDirF = ToFwd $testDir
$deepDirF = ToFwd $deepDir

Write-Host ''; Write-Host '--- T01: basic resolution ---'
$r = RunExe "`"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'basic resolution'

Write-Host ''; Write-Host '--- T02: -e canonicalize-existing ---'
$r = RunExe "-e `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'canonicalize-existing'

Write-Host ''; Write-Host '--- T03: -e fails on missing ---'
$r = RunExe "-e `"$testDir\nonexistent`""
check ($r.ExitCode -ne 0) 'canonicalize-existing fails on missing'

Write-Host ''; Write-Host '--- T04: -m canonicalize-missing ---'
$r = RunExe "-m `"$testDir\nonexistent`""
check ($r.StdOut.Trim() -eq "$testDirF/nonexistent") 'canonicalize-missing'

Write-Host ''; Write-Host '--- T05: -m with ../ in path ---'
$r = RunExe "-m `"$testDir\sub\..\nonexistent`""
check ($r.StdOut.Trim() -eq "$testDirF/nonexistent") 'canonicalize-missing with ..'

Write-Host ''; Write-Host '--- T06: default allows missing last ---'
$r = RunExe "`"$testDir\nonexistent`""
check ($r.StdOut.Trim() -eq "$testDirF/nonexistent") 'default allows missing last'

Write-Host ''; Write-Host '--- T07: default fails on missing parent ---'
$r = RunExe "`"$testDir\nodir\nonexistent`""
check ($r.ExitCode -ne 0) 'default fails on missing parent'

Write-Host ''; Write-Host '--- T08: -q quiet ---'
$r = RunExe "-q `"$testDir\nodir\file`""
check ($r.StdErr -eq '') 'quiet suppresses errors'

Write-Host ''; Write-Host '--- T09: -s no-symlinks with existing file ---'
$r = RunExe "-s `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'no-symlinks with existing file'

Write-Host ''; Write-Host '--- T10: -s -m combined ---'
$r = RunExe "-s -m `"$testDir\sub\..\nonexistent`""
check ($r.StdOut.Trim() -eq "$testDirF/nonexistent") 'strip with missing'

Write-Host ''; Write-Host '--- T11: relative path . ---'
$cwd = ToFwd (Get-Location).Path
$r = RunExe '-m .'
check ($r.StdOut.Trim() -eq $cwd) 'relative dot resolves to cwd'

Write-Host ''; Write-Host '--- T12: relative path with .. ---'
$r = RunExe "-m `"$testDir\sub\..\file.txt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'relative path with ..'

Write-Host ''; Write-Host '--- T13: --relative-to basic ---'
$r = RunExe "--relative-to=`"$testDir`" `"$fileTxt`""
check ($r.StdOut.Trim() -eq 'file.txt') 'relative-to basic'

Write-Host ''; Write-Host '--- T14: --relative-to subdirectory ---'
$r = RunExe "--relative-to=`"$testDir`" `"$nestedTxt`""
check ($r.StdOut.Trim() -eq 'sub/deep/nested.txt') 'relative-to subdir'

Write-Host ''; Write-Host '--- T15: --relative-to with .. ---'
$r = RunExe "--relative-to=`"$subDirPath`" `"$fileTxt`""
check ($r.StdOut.Trim() -eq '../file.txt') 'relative-to parent'

Write-Host ''; Write-Host '--- T16: --relative-base below base ---'
$r = RunExe "--relative-base=`"$testDir`" `"$fileTxt`""
check ($r.StdOut.Trim() -eq 'file.txt') 'relative-base below base'

Write-Host ''; Write-Host '--- T17: --relative-base outside ---'
$r = RunExe "--relative-base=`"$deepDir`" `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'relative-base outside prints absolute'

Write-Host ''; Write-Host '--- T18: multiple files ---'
$r = RunExe "`"$fileTxt`" `"$nestedTxt`""
$lines = $r.StdOut.TrimEnd() -split "`r`n|`n"
check ($lines.Count -eq 2) 'multiple files'

Write-Host ''; Write-Host '--- T19: --help ---'
$r = RunExe '--help'
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'Usage:') 'help exits 0'

Write-Host ''; Write-Host '--- T20: --version ---'
$r = RunExe '--version'
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'realpath') 'version exits 0'

Write-Host ''; Write-Host '--- T21: no operand fails ---'
$p = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
$ec = $p.ExitCode
Remove-Item out.tmp, err.tmp -ErrorAction SilentlyContinue
check ($ec -ne 0) 'no operand fails'

Write-Host ''; Write-Host '--- T22: -- separator ---'
$r = RunExe "-- $fileTxt"
check ($r.StdOut.Trim() -eq $fileTxt) 'double dash separator'

Write-Host ''; Write-Host '--- T23: -e with directory ---'
$r = RunExe "-e `"$subDirPath`""
check ($r.StdOut.Trim() -eq $subDirPath) 'canonicalize-existing directory'

Write-Host ''; Write-Host '--- T24: double backslash cleanup ---'
$r = RunExe "-m `"$testDir\\\\file.txt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'double separator cleanup'

Write-Host ''; Write-Host '--- T25: dot-slash cleanup ---'
$r = RunExe "-m `"$testDir\.\file.txt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'dot-slash cleanup'

Write-Host ''; Write-Host '--- T26: trailing slash on directory ---'
$r = RunExe "-m `"$subDirPath/`""
check ($r.StdOut.Trim() -eq $subDirPath) 'trailing slash removed'

Write-Host ''; Write-Host '--- T27: -e -s combined ---'
$r = RunExe "-e -s `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'combined -e -s'

Write-Host ''; Write-Host '--- T28: -m --relative-to ---'
$r = RunExe "-m --relative-to=`"$testDir`" `"$testDir\nonexistent`""
check ($r.StdOut.Trim() -eq 'nonexistent') 'relative-to with missing'

Write-Host ''; Write-Host '--- T29: -P physical flag ---'
$r = RunExe "-P `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'physical flag'

Write-Host ''; Write-Host '--- T30: -L logical flag ---'
$r = RunExe "-L `"$fileTxt`""
check ($r.StdOut.Trim() -eq $fileTxt) 'logical flag'

Cleanup
Write-Host ''; Write-Host '============================================'
Write-Host "  Results: $PASS passed, $FAIL failed"
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
