# Comprehensive test suite for xargs.c
$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
Set-Location $scriptDir
$exe = Join-Path $scriptDir 'xargs.exe'
$echoExe = Join-Path $scriptDir 'test_echo.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function RunXargs($argList, $stdinStr) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argList
    $psi.WorkingDirectory = $scriptDir
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $stdinBytes = [System.Text.Encoding]::ASCII.GetBytes($stdinStr)
    if ($stdinBytes.Length -gt 0) {
        $p.StandardInput.BaseStream.Write($stdinBytes, 0, $stdinBytes.Length)
    }
    $p.StandardInput.Close()
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    $exitCode = $p.ExitCode
    $p.Close()
    return @{ ExitCode = $exitCode; StdOut = $stdout; StdErr = $stderr }
}

function RunXargsNoStdin($argList) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argList
    $psi.WorkingDirectory = $scriptDir
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    $exitCode = $p.ExitCode
    $p.Close()
    return @{ ExitCode = $exitCode; StdOut = $stdout; StdErr = $stderr }
}

function RunXargsBytes($argList, $bytes) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argList
    $psi.WorkingDirectory = $scriptDir
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    if ($bytes -and $bytes.Length -gt 0) {
        $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    }
    $p.StandardInput.Close()
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    $exitCode = $p.ExitCode
    $p.Close()
    return @{ ExitCode = $exitCode; StdOut = $stdout; StdErr = $stderr }
}

Write-Host ''; Write-Host '--- T01: basic echo ---'
$r = RunXargs "`"$echoExe`"" "hello world`n"
check ($r.StdOut -eq "hello world`n") 'basic echo'

Write-Host ''; Write-Host '--- T02: multiple items ---'
$r = RunXargs "`"$echoExe`"" "a`nb`nc`n"
check ($r.StdOut -eq "a b c`n") 'multiple items'

Write-Host ''; Write-Host '--- T03: -n 1 ---'
$r = RunXargs "-n 1 `"$echoExe`"" "a`nb`nc`n"
check ($r.StdOut -eq "a`nb`nc`n") '-n 1'

Write-Host ''; Write-Host '--- T04: -n 2 ---'
$r = RunXargs "-n 2 `"$echoExe`"" "a`nb`nc`nd`n"
check ($r.StdOut -eq "a b`nc d`n") '-n 2'

Write-Host ''; Write-Host '--- T05: -0 null mode ---'
$nullBytes = [byte[]]@([byte][char]'a', 0, [byte][char]'b', 0, [byte][char]'c', 0)
$r = RunXargsBytes "-0 `"$echoExe`"" $nullBytes
check ($r.StdOut -eq "a b c`n") '-0 null mode'

Write-Host ''; Write-Host '--- T06: -d delimiter ---'
$r = RunXargs "-d , `"$echoExe`"" "a,b,c"
check ($r.StdOut -eq "a b c`n") '-d comma'

Write-Host ''; Write-Host '--- T07: -r no-run-if-empty ---'
$r = RunXargs "-r `"$echoExe`"" ""
check ($r.StdOut -eq '') '-r empty input'

Write-Host ''; Write-Host '--- T08: default empty runs once ---'
$r = RunXargs "`"$echoExe`"" ""
check ($r.StdOut -eq "`n") 'empty runs once'

Write-Host ''; Write-Host '--- T09: -I replace ---'
$r = RunXargs "-I {} `"$echoExe`" file-{}.txt" "foo`nbar`n"
check ($r.StdOut -eq "file-foo.txt`nfile-bar.txt`n") '-I replace'

Write-Host ''; Write-Host '--- T10: -L 2 ---'
$r = RunXargs "-L 2 `"$echoExe`"" "a`nb`nc`nd`n"
check ($r.StdOut -eq "a b`nc d`n") '-L 2'

Write-Host ''; Write-Host '--- T11: -t verbose ---'
$r = RunXargs "-t `"$echoExe`"" "test`n"
check ($r.StdErr -match 'test_echo.exe test') '-t verbose'

Write-Host ''; Write-Host '--- T12: -E eof ---'
$r = RunXargs "-E EOF `"$echoExe`"" "a`nb`nEOF`nc`n"
check ($r.StdOut -eq "a b`n") '-E eof string'

Write-Host ''; Write-Host '--- T13: double quotes ---'
$r = RunXargs "`"$echoExe`"" "`"hello world`" foo`n"
check ($r.StdOut -eq "hello world foo`n") 'double quotes'

Write-Host ''; Write-Host '--- T14: single quotes ---'
$r = RunXargs "`"$echoExe`"" "'hello world' foo`n"
check ($r.StdOut -eq "hello world foo`n") 'single quotes'

Write-Host ''; Write-Host '--- T15: backslash escape ---'
$r = RunXargs "`"$echoExe`"" "hello\ world foo`n"
check ($r.StdOut -eq "hello world foo`n") 'backslash escape'

Write-Host ''; Write-Host '--- T16: --help ---'
$r = RunXargsNoStdin "--help"
check ($r.ExitCode -eq 0) '--help'

Write-Host ''; Write-Host '--- T17: --version ---'
$r = RunXargsNoStdin "--version"
check ($r.ExitCode -eq 0) '--version'

Write-Host ''; Write-Host '--- T18: initial arg ---'
$r = RunXargs "`"$echoExe`" [test]" "dummy`n"
check ($r.StdOut -eq "[test] dummy`n") 'initial arg prepended'

Write-Host ''; Write-Host '--- T19: -s max chars ---'
$r = RunXargs "-s 20 -n 1 `"$echoExe`"" "a`nb`nc`n"
check ($r.StdOut -match '^a') '-s limit'

Write-Host ''; Write-Host '--- T20: whitespace handling ---'
$r = RunXargs "`"$echoExe`"" "  a   b`tc`n"
check ($r.StdOut -eq "a b c`n") 'whitespace handling'

Write-Host ''; Write-Host '--- T21: blank lines ---'
$r = RunXargs "`"$echoExe`"" "a`n`nb`n"
check ($r.StdOut -eq "a b`n") 'blank lines'

Write-Host ''; Write-Host '--- T22: -I multiple replacements ---'
$r = RunXargs "-I {} `"$echoExe`" {}--{}--{}" "x`n"
check ($r.StdOut -eq "x--x--x`n") '-I multiple'

Write-Host ''; Write-Host '--- T23: -a arg file ---'
$testFile = Join-Path $scriptDir 'xargs_test_input.txt'
[System.IO.File]::WriteAllText($testFile, "alpha`nbeta`n")
$r = RunXargsNoStdin "-a `"$testFile`" `"$echoExe`""
Remove-Item $testFile -ErrorAction SilentlyContinue
check ($r.StdOut -eq "alpha beta`n") '-a arg file'

Write-Host ''; Write-Host '--- T24: --null long ---'
$r = RunXargsBytes "--null `"$echoExe`"" $nullBytes
check ($r.StdOut -eq "a b c`n") '--null long'

Write-Host ''; Write-Host '--- T25: --max-args long ---'
$r = RunXargs "--max-args=1 `"$echoExe`"" "a`nb`n"
check ($r.StdOut -eq "a`nb`n") '--max-args'

Write-Host ''; Write-Host '--- T26: --no-run-if-empty long ---'
$r = RunXargs "--no-run-if-empty `"$echoExe`"" ""
check ($r.StdOut -eq '') '--no-run-if-empty'

Write-Host ''; Write-Host '--- T27: --verbose long ---'
$r = RunXargs "--verbose `"$echoExe`"" "x`n"
check ($r.StdErr -match 'test_echo.exe x') '--verbose'

Write-Host ''; Write-Host '--- T28: exit code ---'
$r = RunXargs "`"$echoExe`"" "test`n"
check ($r.ExitCode -eq 0) 'exit code 0'

Write-Host ''; Write-Host '--- T29: initial args ---'
$r = RunXargs "`"$echoExe`" a" "b`nc`n"
check ($r.StdOut -eq "a b c`n") 'initial args'

Write-Host ''; Write-Host '--- T30: -n 2 remainder ---'
$r = RunXargs "-n 2 `"$echoExe`"" "a`nb`nc`n"
check ($r.StdOut -eq "a b`nc`n") '-n 2 remainder'

Write-Host ''; Write-Host '--- T31: invalid option ---'
$r = RunXargsNoStdin "--bogus"
check ($r.ExitCode -ne 0) 'invalid option'

Write-Host ''; Write-Host '--- T32: --delimiter long ---'
$r = RunXargs "--delimiter=, `"$echoExe`"" "x,y,z"
check ($r.StdOut -eq "x y z`n") '--delimiter'

Write-Host ''; Write-Host '--- T33: --replace default ---'
$r = RunXargs "--replace `"$echoExe`" {}.txt" "abc`n"
check ($r.StdOut -eq "abc.txt`n") '--replace default {}'

Write-Host ''; Write-Host '--- T34: -x exit on size ---'
$r = RunXargs "-x -s 5 -n 1 `"$echoExe`"" "abcdefghij`n"
check ($r.ExitCode -ne 0 -or $r.StdErr -match 'too long') '-x exit on size'

Write-Host ''; Write-Host '--- T35: -- separator ---'
$r = RunXargs "-- `"$echoExe`"" "test`n"
check ($r.StdOut -eq "test`n") '-- separator'

Write-Host ''; Write-Host '============================================'
Write-Host "  Results: $PASS passed, $FAIL failed"
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
