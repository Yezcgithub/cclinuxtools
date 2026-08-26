# Comprehensive test suite for cut.c
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'cut.exe'
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

function RunExeStdin($argStr, $stdinData) {
    [System.IO.File]::WriteAllBytes('stdin.tmp', $stdinData)
    $p = Start-Process -FilePath $exe -ArgumentList $argStr -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp -RedirectStandardInput stdin.tmp
    $stdout = ''; $stderr = ''
    if (Test-Path out.tmp) { $raw = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stdout = $raw } }
    if (Test-Path err.tmp) { $raw = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stderr = $raw } }
    Remove-Item out.tmp, err.tmp, stdin.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

function Cleanup {
    $files = @('t_bytes.txt','t_colon.txt','t_ws.txt','t_s.txt',
               'out.tmp','err.tmp','stdin.tmp')
    foreach ($f in $files) { if (Test-Path $f) { Remove-Item $f -Force } }
}

Cleanup

# Create test files
[System.IO.File]::WriteAllText('t_bytes.txt', "abcdefgh`n")
[System.IO.File]::WriteAllText('t_colon.txt', "a:b:c:d:e`n1:2:3:4:5`nhello:world:foo:bar:baz`n")
[System.IO.File]::WriteAllText('t_ws.txt', "  alpha  beta  gamma  delta`none two three four five`n  x   y   z`n")

Write-Host ''; Write-Host '--- T01: -b 1-3 ---'
$r = RunExe '-b 1-3 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abc') 'byte range 1-3'

Write-Host ''; Write-Host '--- T02: -b 1-3,5-7 ---'
$r = RunExe '-b 1-3,5-7 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abcefg') 'byte ranges 1-3,5-7'

Write-Host ''; Write-Host '--- T03: -b with output delimiter ---'
$r = RunExe '-b 1-3,5-7 --output-delimiter=/ t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abc/efg') 'byte output delimiter'

Write-Host ''; Write-Host '--- T04: -b --complement ---'
$r = RunExe '-b 1-3,5-7 --complement t_bytes.txt'
check ($r.StdOut.Trim() -eq 'dh') 'byte complement'

Write-Host ''; Write-Host '--- T05: -f 1,3 -d: ---'
$r = RunExe '-f 1,3 -d: t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'a:c') 'field select 1,3'

Write-Host ''; Write-Host '--- T06: -f 2- -d: ---'
$r = RunExe '-f 2- -d: t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'b:c:d:e') 'field range 2-'

Write-Host ''; Write-Host '--- T07: -f -2 with output delim ---'
$r = RunExe '-f -2 -d: --output-delimiter=- t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'a-b') 'field range -2 with output delim'

Write-Host ''; Write-Host '--- T08: -f 1,3 --complement ---'
$r = RunExe '-f 1,3 -d: --complement t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'b:d:e') 'field complement'

Write-Host ''; Write-Host '--- T09: -c 2-4 ---'
$r = RunExe '-c 2-4 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'bcd') 'char range 2-4'

Write-Host ''; Write-Host '--- T10: -F 1 (whitespace) ---'
$r = RunExe '-F 1 t_ws.txt'
$lines = $r.StdOut.TrimEnd() -split "`r`n|`n"
check ($lines[1] -eq 'one') 'whitespace field 1'

Write-Host ''; Write-Host '--- T11: -F 1,3 (whitespace) ---'
$r = RunExe '-F 1,3 t_ws.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[1] -eq 'one three') 'whitespace fields 1,3'

Write-Host ''; Write-Host '--- T12: trimmed whitespace -f 1 ---'
$r = RunExe '--whitespace-delimited=trimmed -f 1 t_ws.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'alpha') 'trimmed whitespace field 1'

Write-Host ''; Write-Host '--- T13: -w -f 1 ---'
$r = RunExe '-w -f 1 t_ws.txt'
$lines = $r.StdOut.TrimEnd() -split "`r`n|`n"
check ($lines[1] -eq 'one') 'whitespace delimited field 1'

Write-Host ''; Write-Host '--- T14: -s only delimited ---'
[System.IO.File]::WriteAllText('t_s.txt', "no_delim_here`na:b:c`n")
$r = RunExe '-f 1 -d: -s t_s.txt'
check ($r.StdOut.Trim() -eq 'a') 'only delimited'

Write-Host ''; Write-Host '--- T15: without -s prints undelimited ---'
$r = RunExe '-f 1 -d: t_s.txt'
$out = $r.StdOut.Trim()
check ($out -match 'no_delim_here' -and $out -match 'a') 'print undelimited lines'

Write-Host ''; Write-Host '--- T16: -O output delimiter ---'
$r = RunExe '-f 1,3 -d: -O SEP t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'aSEPc') 'output delimiter string'

Write-Host ''; Write-Host '--- T17: stdin ---'
$r = RunExeStdin '-b 1-2 -' ([System.Text.Encoding]::ASCII.GetBytes("abc`n"))
check ($r.StdOut.Trim() -eq 'ab') 'stdin input'

Write-Host ''; Write-Host '--- T18: -b 3- ---'
$r = RunExe '-b 3- t_bytes.txt'
check ($r.StdOut.Trim() -eq 'cdefgh') 'byte open range 3-'

Write-Host ''; Write-Host '--- T19: -b -3 ---'
$r = RunExe '-b -3 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abc') 'byte open range -3'

Write-Host ''; Write-Host '--- T20: -n (no-op) ---'
$r = RunExe '-b 1-3 -n t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abc') 'no-partial flag'

Write-Host ''; Write-Host '--- T21: --help ---'
$r = RunExe '--help'
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'Usage:') 'help exits 0'

Write-Host ''; Write-Host '--- T22: --version ---'
$r = RunExe '--version'
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'cut') 'version exits 0'

Write-Host ''; Write-Host '--- T23: no option fails ---'
$r = RunExe 't_bytes.txt'
check ($r.ExitCode -ne 0) 'no option fails'

Write-Host ''; Write-Host '--- T24: -b 3- --complement ---'
$r = RunExe '-b 3- --complement t_bytes.txt'
check ($r.StdOut.Trim() -eq 'ab') 'complement open range'

Write-Host ''; Write-Host '--- T25: multiple files ---'
$r = RunExe '-b 1 t_bytes.txt t_bytes.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines.Count -eq 2) 'multiple files'

Write-Host ''; Write-Host '--- T26: -f 1-5 -d: (all fields) ---'
$r = RunExe '-f 1-5 -d: t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'a:b:c:d:e') 'all fields'

Write-Host ''; Write-Host '--- T27: -O empty output delim ---'
$r = RunExe '-f 1,3 -d: --output-delimiter= t_colon.txt'
$lines = $r.StdOut.Trim() -split "`r`n|`n"
check ($lines[0] -eq 'ac') 'empty output delimiter'

Write-Host ''; Write-Host '--- T28: -b 1,1 duplicate ---'
$r = RunExe '-b 1,1 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'a') 'duplicate position'

Write-Host ''; Write-Host '--- T29: --bytes= syntax ---'
$r = RunExe '--bytes=1-3 t_bytes.txt'
check ($r.StdOut.Trim() -eq 'abc') 'long option bytes syntax'

Write-Host ''; Write-Host '--- T30: -z zero terminated ---'
$r = RunExeStdin '-b 1 -z -' ([System.Text.Encoding]::ASCII.GetBytes("abc`0def`0"))
check ($r.StdOut -match "`0") 'zero terminated'

Cleanup
Write-Host ''; Write-Host '============================================'
Write-Host "  Results: $PASS passed, $FAIL failed"
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
