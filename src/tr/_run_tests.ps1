# Comprehensive test suite for tr.c
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'tr.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function RunTr($argArray, $stdinData) {
    [System.IO.File]::WriteAllBytes('stdin.tmp', $stdinData)
    $p = Start-Process -FilePath $exe -ArgumentList $argArray -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp -RedirectStandardInput stdin.tmp
    $stdout = ''
    if (Test-Path out.tmp) { $raw = [System.IO.File]::ReadAllBytes('out.tmp'); if ($raw.Length -gt 0) { $stdout = [System.Text.Encoding]::ASCII.GetString($raw) } }
    $stderr = ''
    if (Test-Path err.tmp) { $raw = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stderr = $raw } }
    Remove-Item out.tmp, err.tmp, stdin.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

function RunTrNoStdin($argArray) {
    $p = Start-Process -FilePath $exe -ArgumentList $argArray -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
    $stdout = ''
    if (Test-Path out.tmp) { $raw = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stdout = $raw } }
    $stderr = ''
    if (Test-Path err.tmp) { $raw = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stderr = $raw } }
    Remove-Item out.tmp, err.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

function Enc($s) { return [System.Text.Encoding]::ASCII.GetBytes($s) }

Write-Host ''; Write-Host '--- T01: basic translation ---'
$r = RunTr @('abc','xyz') (Enc 'abc')
check ($r.StdOut -eq 'xyz') 'basic translation'

Write-Host ''; Write-Host '--- T02: range translation ---'
$r = RunTr @('a-z','A-Z') (Enc 'hello')
check ($r.StdOut -eq 'HELLO') 'range a-z to A-Z'

Write-Host ''; Write-Host '--- T03: delete chars ---'
$r = RunTr @('-d','0-9') (Enc 'hello123')
check ($r.StdOut -eq 'hello') 'delete digits'

Write-Host ''; Write-Host '--- T04: squeeze repeats ---'
$r = RunTr @('-s','abc') (Enc 'aaabbbccc')
check ($r.StdOut -eq 'abc') 'squeeze repeats'

Write-Host ''; Write-Host '--- T05: complement delete ---'
$r = RunTr @('-c','-d','0-9') (Enc 'abc123')
check ($r.StdOut -eq '123') 'complement delete'

Write-Host ''; Write-Host '--- T06: complement translate ---'
$r = RunTr @('-c','a-z','X') (Enc 'a1b2c3')
check ($r.StdOut -eq 'aXbXcX') 'complement translate'

Write-Host ''; Write-Host '--- T07: truncate set1 ---'
$r = RunTr @('-t','abcde','xyz') (Enc 'abcdef')
check ($r.StdOut -eq 'xyzdef') 'truncate set1'

Write-Host ''; Write-Host '--- T08: set2 extended ---'
$r = RunTr @('abcde','xy') (Enc 'abcde')
check ($r.StdOut -eq 'xyyyy') 'set2 extended by last char'

Write-Host ''; Write-Host '--- T09: lower to upper class ---'
$r = RunTr @('[:lower:]','[:upper:]') (Enc 'hello')
check ($r.StdOut -eq 'HELLO') 'lower to upper class'

Write-Host ''; Write-Host '--- T10: delete digit class ---'
$r = RunTr @('-d','[:digit:]') (Enc 'hello world 123')
check ($r.StdOut -eq 'hello world ') 'delete digit class'

Write-Host ''; Write-Host '--- T11: squeeze space class ---'
$r = RunTr @('-s','[:space:]') (Enc 'hello   world')
check ($r.StdOut -eq 'hello world') 'squeeze space class'

Write-Host ''; Write-Host '--- T12: tab to space escape ---'
$r = RunTr @('\t','\040') (Enc "a`tb")
check ($r.StdOut -eq 'a b') 'tab to space escape'

Write-Host ''; Write-Host '--- T13: octal tab to space ---'
$r = RunTr @('\011','\040') (Enc "a`tb")
check ($r.StdOut -eq 'a b') 'octal tab to space'

Write-Host ''; Write-Host '--- T14: delete + squeeze ---'
$r = RunTr @('-ds','0-9','abc') (Enc 'aaabbbccc123')
check ($r.StdOut -eq 'abc') 'delete and squeeze'

Write-Host ''; Write-Host '--- T15: translate + squeeze ---'
$r = RunTr @('-s','ab','xy') (Enc 'aaabbb')
check ($r.StdOut -eq 'xy') 'translate and squeeze'

Write-Host ''; Write-Host '--- T16: backslash to slash ---'
$r = RunTr @('\\','/') (Enc 'a\b')
check ($r.StdOut -eq 'a/b') 'backslash to slash'

Write-Host ''; Write-Host '--- T17: bell escape ---'
$r = RunTr @('\a','B') (Enc ([string][char]7))
check ($r.StdOut -eq 'B') 'bell escape'

Write-Host ''; Write-Host '--- T18: newline to underscore ---'
$r = RunTr @('\n','_') (Enc "a`nb")
check ($r.StdOut -eq 'a_b') 'newline to underscore'

Write-Host ''; Write-Host '--- T19: delete alnum ---'
$r = RunTr @('-d','[:alnum:]') (Enc 'a1!b2@')
check ($r.StdOut -eq '!@') 'delete alnum'

Write-Host ''; Write-Host '--- T20: delete punct ---'
$r = RunTr @('-d','[:punct:]') (Enc 'hello, world!')
check ($r.StdOut -eq 'hello world') 'delete punct'

Write-Host ''; Write-Host '--- T21: [x*] repeat ---'
$r = RunTr @('abcdef','[x*]') (Enc 'abcdef')
check ($r.StdOut -eq 'xxxxxx') 'repeat construct [x*]'

Write-Host ''; Write-Host '--- T22: [x*3] repeat ---'
$r = RunTr @('abc','[x*3]y') (Enc 'abc')
check ($r.StdOut -eq 'xxx') 'repeat construct [x*3]'

Write-Host ''; Write-Host '--- T23: --help ---'
$r = RunTrNoStdin @('--help')
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'Usage:') 'help exits 0'

Write-Host ''; Write-Host '--- T24: --version ---'
$r = RunTrNoStdin @('--version')
check ($r.ExitCode -eq 0 -and $r.StdOut -match 'tr') 'version exits 0'

Write-Host ''; Write-Host '--- T25: missing operand ---'
$r = RunTrNoStdin @('-d')
check ($r.ExitCode -ne 0) 'missing operand fails'

Write-Host ''; Write-Host '--- T26: translate missing operand ---'
$r = RunTr @('a') (Enc 'test')
check ($r.ExitCode -ne 0) 'missing second operand'

Write-Host ''; Write-Host '--- T27: -- separator ---'
$r = RunTr @('--','abc','xyz') (Enc 'abc')
check ($r.StdOut -eq 'xyz') 'double dash separator'

Write-Host ''; Write-Host '--- T28: reverse range error ---'
$r = RunTr @('z-a','a-z') (Enc 'test')
check ($r.ExitCode -ne 0) 'reverse range error'

Write-Host ''; Write-Host '--- T29: mixed range and literal ---'
$r = RunTr @('a-c1-3','xyzABC') (Enc 'abc123')
check ($r.StdOut -eq 'xyzABC') 'mixed ranges'

Write-Host ''; Write-Host '--- T30: complement squeeze ---'
$r = RunTr @('-c','-s','0-9') (Enc 'aaa111bbb')
check ($r.StdOut -eq 'a111b') 'complement squeeze'

Write-Host ''; Write-Host '============================================'
Write-Host "  Results: $PASS passed, $FAIL failed"
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
