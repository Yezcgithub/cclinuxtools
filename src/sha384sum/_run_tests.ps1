# Comprehensive test suite for sha384sum.c
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'sha384sum.exe'
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
    $files = @('abc.txt','empty.txt','big.bin','check_ok.txt','check_tag.txt','check_bad.txt',
               'check_missing.txt','check_bin.txt','check_warn.txt','check_empty.txt','check_novalid.txt',
               'out.tmp','err.tmp','stdin.tmp')
    foreach ($f in $files) { if (Test-Path $f) { Remove-Item $f -Force } }
}

Cleanup
[byte[]]$abcBytes = [System.Text.Encoding]::ASCII.GetBytes('abc')
[System.IO.File]::WriteAllBytes('abc.txt', $abcBytes)
[System.IO.File]::WriteAllBytes('empty.txt', [byte[]]@())
[byte[]]$bigData = New-Object byte[] 100000
for ($i = 0; $i -lt 100000; $i++) { $bigData[$i] = $i % 256 }
[System.IO.File]::WriteAllBytes('big.bin', $bigData)

$ABC_HEX = 'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7'
$EMPTY_HEX = '38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b'
$BAD_HEX = '000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'
$TAG = 'SHA384'

Write-Host ''; Write-Host '--- T01: abc default ---'
$r = RunExe 'abc.txt'; check ($r.StdOut.Trim() -eq "$ABC_HEX  abc.txt") 'abc default'

Write-Host ''; Write-Host '--- T02: empty file ---'
$r = RunExe 'empty.txt'; check ($r.StdOut.Trim() -eq "$EMPTY_HEX  empty.txt") 'empty default'

Write-Host ''; Write-Host '--- T03: stdin ---'
$r = RunExeStdin '-' $abcBytes; check ($r.StdOut.Trim() -eq "$ABC_HEX  -") 'stdin'

Write-Host ''; Write-Host '--- T04: -b binary mode ---'
$r = RunExe '-b abc.txt'; check ($r.StdOut.Trim() -eq "$ABC_HEX *abc.txt") 'binary mode'

Write-Host ''; Write-Host '--- T05: --tag mode ---'
$r = RunExe '--tag abc.txt'; check ($r.StdOut.Trim() -eq "$TAG (abc.txt) = $ABC_HEX") 'tag mode'

Write-Host ''; Write-Host '--- T06: --tag stdin ---'
$r = RunExeStdin '--tag -' $abcBytes; check ($r.StdOut.Trim() -eq "$TAG (stdin) = $ABC_HEX") 'tag stdin'

Write-Host ''; Write-Host '--- T07: --check untagged OK ---'
[System.IO.File]::WriteAllText('check_ok.txt', "$ABC_HEX  abc.txt`n")
$r = RunExe '-c check_ok.txt'; check (($r.StdOut.Trim() -eq 'abc.txt: OK') -and ($r.ExitCode -eq 0)) 'check untagged OK'

Write-Host ''; Write-Host '--- T08: --check tagged OK ---'
[System.IO.File]::WriteAllText('check_tag.txt', "$TAG (abc.txt) = $ABC_HEX`n")
$r = RunExe '-c check_tag.txt'; check (($r.StdOut.Trim() -eq 'abc.txt: OK') -and ($r.ExitCode -eq 0)) 'check tagged OK'

Write-Host ''; Write-Host '--- T09: --check FAILED ---'
[System.IO.File]::WriteAllText('check_bad.txt', "$BAD_HEX  abc.txt`n")
$r = RunExe '-c check_bad.txt'; check (($r.StdOut.Trim() -eq 'abc.txt: FAILED') -and ($r.ExitCode -eq 1)) 'check FAILED'

Write-Host ''; Write-Host '--- T10: --check --status OK ---'
$r = RunExe '-c --status check_ok.txt'; check ($r.ExitCode -eq 0) 'check status OK'

Write-Host ''; Write-Host '--- T11: --check --status FAILED ---'
$r = RunExe '-c --status check_bad.txt'; check ($r.ExitCode -eq 1) 'check status FAILED'

Write-Host ''; Write-Host '--- T12: --check --quiet OK ---'
$r = RunExe '-c --quiet check_ok.txt'; check (($r.StdOut.Trim() -eq '') -and ($r.ExitCode -eq 0)) 'check quiet OK'

Write-Host ''; Write-Host '--- T13: --check --ignore-missing ---'
[System.IO.File]::WriteAllText('check_missing.txt', "$ABC_HEX  nonexistent.txt`n")
$r = RunExe '-c --ignore-missing check_missing.txt'; check ($r.ExitCode -eq 0) 'ignore-missing'

Write-Host ''; Write-Host '--- T14: --check missing file fails ---'
$r = RunExe '-c check_missing.txt'; check ($r.ExitCode -ne 0) 'missing file fails'

Write-Host ''; Write-Host '--- T15: multiple files ---'
$r = RunExe 'abc.txt empty.txt'
check (($r.StdOut -match [regex]::Escape($ABC_HEX)) -and ($r.StdOut -match [regex]::Escape($EMPTY_HEX))) 'multiple files'

Write-Host ''; Write-Host '--- T16: --help exits 0 ---'
$r = RunExe '--help'; check (($r.ExitCode -eq 0) -and ($r.StdOut -match 'Usage:')) 'help'

Write-Host ''; Write-Host '--- T17: --version exits 0 ---'
$r = RunExe '--version'; check (($r.ExitCode -eq 0) -and ($r.StdOut -match 'sha384sum')) 'version'

Write-Host ''; Write-Host '--- T18: big file ---'
$r = RunExe 'big.bin'; check ($r.StdOut -match 'big\.bin') 'big file exists'

Write-Host ''; Write-Host '--- T19: big file consistency ---'
$r1 = RunExe 'big.bin'; $r2 = RunExe 'big.bin'
check ($r1.StdOut.Trim() -eq $r2.StdOut.Trim()) 'big file consistency'

Write-Host ''; Write-Host '--- T20: --check binary format ---'
[System.IO.File]::WriteAllText('check_bin.txt', "$ABC_HEX *abc.txt`n")
$r = RunExe '-c check_bin.txt'; check (($r.StdOut.Trim() -eq 'abc.txt: OK') -and ($r.ExitCode -eq 0)) 'check binary format'

Write-Host ''; Write-Host '--- T21: -w warn on bad line ---'
[System.IO.File]::WriteAllText('check_warn.txt', "not_a_valid_line`n$ABC_HEX  abc.txt`n")
$r = RunExe '-c -w check_warn.txt'; check (($r.StdErr -match 'improperly formatted') -and ($r.ExitCode -eq 0)) 'warn bad line'

Write-Host ''; Write-Host '--- T22: --strict bad line fails ---'
$r = RunExe '-c --strict check_warn.txt'; check ($r.ExitCode -eq 1) 'strict bad line fails'

Write-Host ''; Write-Host '--- T23: -t text mode ---'
$r = RunExe '-t abc.txt'; check ($r.StdOut.Trim() -eq "$ABC_HEX  abc.txt") 'text mode'

Write-Host ''; Write-Host '--- T24: --check empty checkfile ---'
New-Item -Path check_empty.txt -ItemType File -Force | Out-Null
$r = RunExe '-c check_empty.txt'; check ($r.ExitCode -eq 1) 'empty checkfile fails'

Write-Host ''; Write-Host '--- T25: --check no valid lines ---'
[System.IO.File]::WriteAllText('check_novalid.txt', "invalid line 1`ninvalid line 2`n")
$r = RunExe '-c check_novalid.txt'
check (($r.ExitCode -eq 1) -and ($r.StdErr -match 'no properly formatted')) 'no valid lines'

Write-Host ''; Write-Host '--- T26: --tag with -b ---'
$r = RunExe '--tag -b abc.txt'; check ($r.StdOut.Trim() -eq "$TAG (abc.txt) = $ABC_HEX") 'tag + binary'

Write-Host ''; Write-Host '--- T27: --zero output ---'
$r = RunExe '--zero abc.txt'; check ($r.StdOut -match "`0") 'zero output'

Write-Host ''; Write-Host '--- T28: unrecognized option fails ---'
$r = RunExe '--foobar'; check (($r.ExitCode -ne 0) -and ($r.StdErr -match 'unrecognized')) 'unrecognized option'

Write-Host ''; Write-Host '--- T29: nonexistent file fails ---'
$r = RunExe 'nonexistent.txt'; check (($r.ExitCode -ne 0) -and ($r.StdErr -match 'No such file')) 'nonexistent file'

Write-Host ''; Write-Host '--- T30: --check --quiet FAILED ---'
$r = RunExe '-c --quiet check_bad.txt'
check (($r.StdOut.Trim() -eq 'abc.txt: FAILED') -and ($r.ExitCode -eq 1)) 'quiet FAILED prints'

Cleanup
Write-Host ''; Write-Host '============================================'
Write-Host "  Results: $PASS passed, $FAIL failed"
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }