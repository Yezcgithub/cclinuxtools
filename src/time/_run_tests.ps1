# Comprehensive test suite for time.c (Windows)
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'time.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function RunTime($argArray) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    if ($argArray -and $argArray.Count -gt 0) {
        $sb = New-Object System.Text.StringBuilder
        foreach ($a in $argArray) {
            if ($sb.Length -gt 0) { [void]$sb.Append(' ') }
            if ($a -match '\s|"') {
                $escaped = $a -replace '"', '\"'
                [void]$sb.Append("`"$escaped`"")
            }
            else {
                [void]$sb.Append($a)
            }
        }
        $psi.Arguments = $sb.ToString()
    }

    $p = [System.Diagnostics.Process]::Start($psi)
    $p.WaitForExit()

    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()

    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

$testCmd = 'powershell'
$testArgs = @('-NoProfile', '-Command', 'exit 0')

Write-Host ''; Write-Host '--- T01: --help exits 0 ---'
$r = RunTime @('--help')
check ($r.ExitCode -eq 0) '--help exits 0'
check ($r.StdErr -match 'Usage:') '--help contains Usage:'

Write-Host ''; Write-Host '--- T02: --version exits 0 ---'
$r = RunTime @('--version')
check ($r.ExitCode -eq 0) '--version exits 0'
check ($r.StdErr -match 'time \(GNU time\)') '--version contains version string'

Write-Host ''; Write-Host '--- T03: missing command is error ---'
$r = RunTime @()
check ($r.ExitCode -eq 1) 'missing command exits 1'
check ($r.StdErr -match 'missing command') 'error message mentions missing command'

Write-Host ''; Write-Host '--- T04: default output format ---'
$r = RunTime @($testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'user') 'default output contains user'
check ($r.StdErr -match 'system') 'default output contains system'
check ($r.StdErr -match 'elapsed') 'default output contains elapsed'

Write-Host ''; Write-Host '--- T05: -p portable format ---'
$r = RunTime @('-p', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match '(?m)^real ') '-p output contains real line'
check ($r.StdErr -match '(?m)^user ') '-p output contains user line'
check ($r.StdErr -match '(?m)^sys ') '-p output contains sys line'

Write-Host ''; Write-Host '--- T06: exit code propagation ---'
$r = RunTime @($testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.ExitCode -eq 0) 'time of exit 0 returns 0'
$r = RunTime @($testCmd, '-NoProfile', '-Command', 'exit 42')
check ($r.ExitCode -eq 42) 'time of exit 42 returns 42'

Write-Host ''; Write-Host '--- T07: --portability long option ---'
$r1 = RunTime @('-p', $testCmd, '-NoProfile', '-Command', 'exit 0')
$r2 = RunTime @('--portability', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r1.StdErr -match '(?m)^real ') '-p has real line'
check ($r2.StdErr -match '(?m)^real ') '--portability has real line'
check (($r1.StdErr -split "
").Count -eq ($r2.StdErr -split "
").Count) '-p and --portability have same line count'

Write-Host ''; Write-Host '--- T08: -f custom format ---'
$r = RunTime @('--format=elapsed=%e user=%U sys=%S', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match '^elapsed=') '-f custom format starts with elapsed='
check ($r.StdErr -match 'user=') '-f output contains user='
check ($r.StdErr -match 'sys=') '-f output contains sys='

Write-Host ''; Write-Host '--- T09: --format= long option ---'
$r1 = RunTime @('-f', 'TESTFMT=%e', $testCmd, '-NoProfile', '-Command', 'exit 0')
$r2 = RunTime @('--format=TESTFMT=%e', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r1.StdErr -match '^TESTFMT=') '-f produces correct prefix'
check ($r2.StdErr -match '^TESTFMT=') '--format= produces correct prefix'
check ($r1.StdErr -match '\d+\.\d{2}') '-f has 2 decimal time value'
check ($r2.StdErr -match '\d+\.\d{2}') '--format= has 2 decimal time value'

Write-Host ''; Write-Host '--- T10: -o output file ---'
$outFile = Join-Path $PSScriptRoot 'time_test_out.txt'
if (Test-Path $outFile) { Remove-Item $outFile -Force }
$r = RunTime @('-o', $outFile, $testCmd, '-NoProfile', '-Command', 'exit 0')
check (Test-Path $outFile) '-o creates output file'
if (Test-Path $outFile) {
    $content = Get-Content $outFile -Raw
    check ($content.Length -gt 0) 'output file is not empty'
    Remove-Item $outFile -Force
}

Write-Host ''; Write-Host '--- T11: -a append mode ---'
$outFile = Join-Path $PSScriptRoot 'time_append.txt'
if (Test-Path $outFile) { Remove-Item $outFile -Force }
$null = RunTime @('-o', $outFile, $testCmd, '-NoProfile', '-Command', 'exit 0')
$null = RunTime @('-a', '-o', $outFile, $testCmd, '-NoProfile', '-Command', 'exit 0')
if (Test-Path $outFile) {
    $lines = (Get-Content $outFile).Count
    check ($lines -ge 2) "-a appends ($lines lines)"
    Remove-Item $outFile -Force
}

Write-Host ''; Write-Host '--- T12: -v verbose output ---'
$r = RunTime @('-v', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'Command being timed') '-v contains command info'
check ($r.StdErr -match 'User time') '-v contains user time'
check ($r.StdErr -match 'System time') '-v contains system time'
check ($r.StdErr -match 'Exit status') '-v contains exit status'

Write-Host ''; Write-Host '--- T13: %C format specifier ---'
$r = RunTime @('-f', '%C', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match [regex]::Escape($testCmd)) '%C prints command name'

Write-Host ''; Write-Host '--- T14: %x exit status format ---'
$r = RunTime @('-f', 'exit=%x', $testCmd, '-NoProfile', '-Command', 'exit 7')
check ($r.StdErr -match 'exit=7') '%x prints exit code 7'

Write-Host ''; Write-Host '--- T15: %Z page size ---'
$r = RunTime @('-f', 'pagesize=%Z', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'pagesize=\d+') '%Z prints page size'

Write-Host ''; Write-Host '--- T16: %P CPU percentage ---'
$r = RunTime @('-f', 'cpu=%P', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'cpu=.*%') '%P prints CPU percentage'

Write-Host ''; Write-Host '--- T17: %E elapsed time format ---'
$r = RunTime @('-f', 'elapsed=%E', $testCmd, '-NoProfile', '-Command', 'Start-Sleep -Milliseconds 200')
check ($r.StdErr -match 'elapsed=\d+:\d+\.\d+') '%E prints MM:SS.cc format'

Write-Host ''; Write-Host '--- T18: real time is positive ---'
$r = RunTime @('-p', $testCmd, '-NoProfile', '-Command', 'Start-Sleep -Milliseconds 100')
$lines = $r.StdErr -split "`n"
$realLine = $lines | Where-Object { $_ -match '^real ' }
if ($realLine) {
    $realVal = [double]($realLine -replace '^real ', '').Trim()
    check ($realVal -gt 0) "real time > 0 ($realVal)"
} else {
    check $false 'real time line not found'
}

Write-Host ''; Write-Host '--- T19: invalid option exits 1 ---'
$r = RunTime @('-Q', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.ExitCode -eq 1) 'invalid option -Q exits 1'

Write-Host ''; Write-Host '--- T20: nonexistent command ---'
$r = RunTime @('nosuchcommand_xyz123')
check ($r.ExitCode -eq 127) 'nonexistent command returns 127'

Write-Host ''; Write-Host '--- T21: --output= long option ---'
$outFile = Join-Path $PSScriptRoot 'time_longopt.txt'
if (Test-Path $outFile) { Remove-Item $outFile -Force }
$r = RunTime @("--output=$outFile", $testCmd, '-NoProfile', '-Command', 'exit 0')
check (Test-Path $outFile) '--output= creates file'
if (Test-Path $outFile) { Remove-Item $outFile -Force }

Write-Host ''; Write-Host '--- T22: %U and %S have 2 decimal places ---'
$r = RunTime @('--format=user=%U sys=%S', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'user=\d+\.\d{2}') '%U has 2 decimal places'
check ($r.StdErr -match 'sys=\d+\.\d{2}') '%S has 2 decimal places'

Write-Host ''; Write-Host '--- T23: %e has 2 decimal places ---'
$r = RunTime @('-f', 'real=%e', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match 'real=\d+\.\d{2}') '%e has 2 decimal places'

Write-Host ''; Write-Host '--- T24: %% literal percent ---'
$r = RunTime @('-f', '100%%done', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.StdErr -match '100%done') '%% prints literal %'

Write-Host ''; Write-Host '--- T25: -- separator ---'
$r = RunTime @('--', $testCmd, '-NoProfile', '-Command', 'exit 0')
check ($r.ExitCode -eq 0) '-- before command works'

Write-Host ''; Write-Host '============================================'
Write-Host "  Test Results: $PASS passed, $FAIL failed"
Write-Host '============================================'

if ($FAIL -gt 0) { exit 1 }
exit 0
