$ErrorActionPreference = 'Continue'
$exe = './md5sum.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function WriteNoBom($path, $content) {
    [System.IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding $false))
}

function RunExe($argStr) {
    $p = Start-Process -FilePath $exe -ArgumentList $argStr -NoNewWindow -Wait -PassThru -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
    $stdout = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue
    $stderr = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue
    Remove-Item out.tmp, err.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

function RunExeStdin($argStr, $stdinData) {
    [System.IO.File]::WriteAllBytes('stdin.tmp', $stdinData)
    $p = Start-Process -FilePath $exe -ArgumentList $argStr -NoNewWindow -Wait -PassThru -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp -RedirectStandardInput stdin.tmp
    $stdout = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue
    $stderr = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue
    Remove-Item out.tmp, err.tmp, stdin.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

# Create test files
[byte[]]$abcBytes = [System.Text.Encoding]::ASCII.GetBytes('abc')
[System.IO.File]::WriteAllBytes('abc.txt', $abcBytes)
[System.IO.File]::WriteAllBytes('empty.txt', [byte[]]@())
[byte[]]$bigData = New-Object byte[] 100000
for ($i = 0; $i -lt 100000; $i++) { $bigData[$i] = $i % 256 }
[System.IO.File]::WriteAllBytes('big.bin', $bigData)

Write-Host ''
Write-Host '--- T01: abc default (untagged text mode) ---'
$r = RunExe 'abc.txt'
check ($r.StdOut.Trim() -eq '900150983cd24fb0d6963f7d28e17f72  abc.txt') 'abc default'

Write-Host ''
Write-Host '--- T02: empty file ---'
$r = RunExe 'empty.txt'
check ($r.StdOut.Trim() -eq 'd41d8cd98f00b204e9800998ecf8427e  empty.txt') 'empty default'

Write-Host ''
Write-Host '--- T03: stdin ---'
$r = RunExeStdin '-' $abcBytes
check ($r.StdOut.Trim() -eq '900150983cd24fb0d6963f7d28e17f72  -') 'stdin'

Write-Host ''
Write-Host '--- T04: -b binary mode ---'
$r = RunExe '-b abc.txt'
check ($r.StdOut.Trim() -eq '900150983cd24fb0d6963f7d28e17f72 *abc.txt') 'binary mode'

Write-Host ''
Write-Host '--- T05: --tag mode ---'
$r = RunExe '--tag abc.txt'
check ($r.StdOut.Trim() -eq 'MD5 (abc.txt) = 900150983cd24fb0d6963f7d28e17f72') 'tag mode'

Write-Host ''
Write-Host '--- T06: --tag stdin ---'
$r = RunExeStdin '--tag -' $abcBytes
check ($r.StdOut.Trim() -eq 'MD5 (stdin) = 900150983cd24fb0d6963f7d28e17f72') 'tag stdin'

Write-Host ''
Write-Host '--- T07: --check untagged OK ---'
WriteNoBom 'check_ok.txt' "900150983cd24fb0d6963f7d28e17f72  abc.txt`n"
$r = RunExe '-c check_ok.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check untagged OK'

Write-Host ''
Write-Host '--- T08: --check tagged OK ---'
WriteNoBom 'check_tag.txt' "MD5 (abc.txt) = 900150983cd24fb0d6963f7d28e17f72`n"
$r = RunExe '-c check_tag.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check tagged OK'

Write-Host ''
Write-Host '--- T09: --check FAILED ---'
WriteNoBom 'check_bad.txt' "00000000000000000000000000000000  abc.txt`n"
$r = RunExe '-c check_bad.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: FAILED' -and $r.ExitCode -eq 1) 'check FAILED'

Write-Host ''
Write-Host '--- T10: --check --status OK ---'
$r = RunExe '-c --status check_ok.txt'
$out = if ($r.StdOut) { $r.StdOut.Trim() } else { '' }
check ($r.ExitCode -eq 0 -and $out -eq '') 'check status OK'

Write-Host ''
Write-Host '--- T11: --check --status FAILED ---'
$r = RunExe '-c --status check_bad.txt'
check ($r.ExitCode -eq 1) 'check status FAILED'

Write-Host ''
Write-Host '--- T12: --check --quiet OK ---'
$r = RunExe '-c --quiet check_ok.txt'
$out = if ($r.StdOut) { $r.StdOut.Trim() } else { '' }
check ($r.ExitCode -eq 0 -and $out -eq '') 'check quiet OK'

Write-Host ''
Write-Host '--- T13: --check --ignore-missing ---'
WriteNoBom 'check_missing.txt' "900150983cd24fb0d6963f7d28e17f72  nonexistent.txt`n"
$r = RunExe '-c --ignore-missing check_missing.txt'
check ($r.ExitCode -eq 0) 'ignore-missing'

Write-Host ''
Write-Host '--- T14: --check missing file fails ---'
$r = RunExe '-c check_missing.txt'
check ($r.ExitCode -eq 1) 'missing file fails'

Write-Host ''
Write-Host '--- T15: multiple files ---'
$r = RunExe 'abc.txt empty.txt'
$lines = $r.StdOut.Trim() -split "`n"
check ($lines.Count -eq 2 -and $lines[0].Contains('900150983cd24fb0d6963f7d28e17f72') -and $lines[1].Contains('d41d8cd98f00b204e9800998ecf8427e')) 'multiple files'

Write-Host ''
Write-Host '--- T16: --help exits 0 ---'
$r = RunExe '--help'
check ($r.ExitCode -eq 0 -and $r.StdOut.Contains('Usage:')) 'help'

Write-Host ''
Write-Host '--- T17: --version exits 0 ---'
$r = RunExe '--version'
check ($r.ExitCode -eq 0 -and $r.StdOut.Contains('md5sum')) 'version'

Write-Host ''
Write-Host '--- T18: big file ---'
$r = RunExe 'big.bin'
check ($r.StdOut.StartsWith('') -and $r.StdOut.Contains('big.bin')) 'big file exists'

Write-Host ''
Write-Host '--- T19: big file consistency ---'
$r1 = RunExe 'big.bin'
$r2 = RunExe 'big.bin'
check ($r1.StdOut -eq $r2.StdOut) 'big file consistency'

Write-Host ''
Write-Host '--- T20: --check binary format (with *) ---'
WriteNoBom 'check_bin.txt' "900150983cd24fb0d6963f7d28e17f72 *abc.txt`n"
$r = RunExe '-c check_bin.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check binary format'

Write-Host ''
Write-Host '--- T21: -w warn on bad line ---'
WriteNoBom 'check_warn.txt' "not_a_valid_line`n900150983cd24fb0d6963f7d28e17f72  abc.txt`n"
$r = RunExe '-c -w check_warn.txt'
check ($r.StdErr.Contains('improperly formatted') -and $r.ExitCode -eq 0) 'warn bad line'

Write-Host ''
Write-Host '--- T22: --strict bad line fails ---'
$r = RunExe '-c --strict check_warn.txt'
check ($r.ExitCode -eq 1) 'strict bad line fails'

Write-Host ''
Write-Host '--- T23: -t text mode (explicit) ---'
$r = RunExe '-t abc.txt'
check ($r.StdOut.Trim() -eq '900150983cd24fb0d6963f7d28e17f72  abc.txt') 'text mode'

Write-Host ''
Write-Host '--- T24: --check empty checkfile fails ---'
WriteNoBom 'check_empty.txt' ''
$r = RunExe '-c check_empty.txt'
check ($r.ExitCode -eq 1) 'empty checkfile fails'

Write-Host ''
Write-Host '--- T25: --check no valid lines ---'
WriteNoBom 'check_novalid.txt' "invalid line 1`ninvalid line 2`n"
$r = RunExe '-c check_novalid.txt'
check ($r.ExitCode -eq 1 -and $r.StdErr.Contains('no properly formatted')) 'no valid lines'

Write-Host ''
Write-Host '--- T26: --tag with -b (tag overrides binary display) ---'
$r = RunExe '--tag -b abc.txt'
check ($r.StdOut.Trim() -eq 'MD5 (abc.txt) = 900150983cd24fb0d6963f7d28e17f72') 'tag + binary'

Write-Host ''
Write-Host '--- T27: --zero output ---'
$r = RunExe '--zero abc.txt'
check ($r.StdOut.Contains("`0") -and $r.StdOut.Contains('900150983cd24fb0d6963f7d28e17f72')) 'zero output'

Write-Host ''
Write-Host '--- T28: unrecognized option fails ---'
$r = RunExe '--foobar'
check ($r.ExitCode -ne 0 -and $r.StdErr.Contains('unrecognized')) 'unrecognized option'

Write-Host ''
Write-Host '--- T29: nonexistent file fails ---'
$r = RunExe 'nonexistent.txt'
check ($r.ExitCode -ne 0 -and $r.StdErr.Contains('No such file')) 'nonexistent file'

Write-Host ''
Write-Host '--- T30: --check --quiet FAILED still prints FAILED ---'
$r = RunExe '-c --quiet check_bad.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: FAILED' -and $r.ExitCode -eq 1) 'quiet FAILED prints'

# Cleanup
Remove-Item abc.txt, empty.txt, big.bin, check_ok.txt, check_tag.txt, check_bad.txt, check_missing.txt, check_bin.txt, check_warn.txt, check_empty.txt, check_novalid.txt -ErrorAction SilentlyContinue

Write-Host ''
Write-Host '============================================'
Write-Host ("  Results: $PASS passed, $FAIL failed")
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
