# Comprehensive test suite for id.c (Windows)
$ErrorActionPreference = 'Continue'
$exe = Join-Path $PSScriptRoot 'id.exe'
$PASS = 0
$FAIL = 0

function check($cond, $name) {
    if ($cond) { Write-Host '  [PASS]'; $script:PASS++ }
    else { Write-Host '  [FAIL]'; $script:FAIL++ }
}

function RunId($argArray) {
    if ($null -eq $argArray -or $argArray.Count -eq 0) {
        $p = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
    }
    else {
        $p = Start-Process -FilePath $exe -ArgumentList $argArray -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput out.tmp -RedirectStandardError err.tmp
    }
    $stdout = ''
    if (Test-Path out.tmp) { $raw = Get-Content out.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stdout = $raw } }
    $stderr = ''
    if (Test-Path err.tmp) { $raw = Get-Content err.tmp -Raw -ErrorAction SilentlyContinue; if ($raw) { $stderr = $raw } }
    Remove-Item out.tmp, err.tmp -ErrorAction SilentlyContinue
    return @{ ExitCode = $p.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

# Get current username for testing
$currentUser = $env:USERNAME

Write-Host ''; Write-Host '--- T01: default output format ---'
$r = RunId @()
check ($r.StdOut -match 'uid=\d+') 'default output contains uid=N'
check ($r.StdOut -match 'gid=\d+') 'default output contains gid=N'
check ($r.StdOut -match 'groups=') 'default output contains groups='

Write-Host ''; Write-Host '--- T02: -u prints numeric UID ---'
$r = RunId @('-u')
check ($r.StdOut -match '^\d+\s*$') "-u output is numeric ($($r.StdOut.Trim()))"

Write-Host ''; Write-Host '--- T03: -g prints numeric GID ---'
$r = RunId @('-g')
check ($r.StdOut -match '^\d+\s*$') "-g output is numeric ($($r.StdOut.Trim()))"

Write-Host ''; Write-Host '--- T04: -un prints username ---'
$r = RunId @('-un')
$uname = $r.StdOut.Trim()
check ($uname.Length -gt 0) "-un produces non-empty output ($uname)"

Write-Host ''; Write-Host '--- T05: -gn prints group name ---'
$r = RunId @('-gn')
$gname = $r.StdOut.Trim()
check ($gname.Length -gt 0) "-gn produces non-empty output ($gname)"

Write-Host ''; Write-Host '--- T06: -G prints all group IDs ---'
$r = RunId @('-G')
$gids = $r.StdOut.Trim() -split '\s+'
check ($gids.Count -ge 1) "-G produces at least 1 group ID (got $($gids.Count))"
check ($gids[0] -match '^\d+$') "-G first entry is numeric ($($gids[0]))"

Write-Host ''; Write-Host '--- T07: -Gn prints all group names ---'
$r = RunId @('-Gn')
$gnames = $r.StdOut.Trim() -split '\s+'
check ($gnames.Count -ge 1) "-Gn produces at least 1 group name (got $($gnames.Count))"

Write-Host ''; Write-Host '--- T08: -u and --user produce same output ---'
$r1 = RunId @('-u')
$r2 = RunId @('--user')
check ($r1.StdOut -eq $r2.StdOut) "--user matches -u"

Write-Host ''; Write-Host '--- T09: -g and --group produce same output ---'
$r1 = RunId @('-g')
$r2 = RunId @('--group')
check ($r1.StdOut -eq $r2.StdOut) "--group matches -g"

Write-Host ''; Write-Host '--- T10: -G and --groups produce same output ---'
$r1 = RunId @('-G')
$r2 = RunId @('--groups')
check ($r1.StdOut -eq $r2.StdOut) "--groups matches -G"

Write-Host ''; Write-Host '--- T11: -n and --name produce same output ---'
$r1 = RunId @('-u', '-n')
$r2 = RunId @('--user', '--name')
check ($r1.StdOut -eq $r2.StdOut) "--user --name matches -un"

Write-Host ''; Write-Host '--- T12: -r and --real produce same output ---'
$r1 = RunId @('-u', '-r')
$r2 = RunId @('--user', '--real')
check ($r1.StdOut -eq $r2.StdOut) "--user --real matches -ur"

Write-Host ''; Write-Host '--- T13: -ur equals -u (real==effective on Windows) ---'
$r1 = RunId @('-u')
$r2 = RunId @('-u', '-r')
check ($r1.StdOut -eq $r2.StdOut) "-ur equals -u on Windows"

Write-Host ''; Write-Host '--- T14: -gr equals -g (real==effective on Windows) ---'
$r1 = RunId @('-g')
$r2 = RunId @('-g', '-r')
check ($r1.StdOut -eq $r2.StdOut) "-gr equals -g on Windows"

Write-Host ''; Write-Host '--- T15: -a is ignored (compatibility) ---'
$r1 = RunId @('-u')
$r2 = RunId @('-a', '-u')
check ($r1.StdOut -eq $r2.StdOut) "-a does not change -u output"

Write-Host ''; Write-Host '--- T16: combined -ug prints both ---'
$r = RunId @('-u', '-g')
$parts = $r.StdOut.Trim() -split '\s+'
check ($parts.Count -eq 2) "-ug prints two values (got $($parts.Count))"
check ($parts[0] -match '^\d+$') "-ug first value is numeric ($($parts[0]))"
check ($parts[1] -match '^\d+$') "-ug second value is numeric ($($parts[1]))"

Write-Host ''; Write-Host '--- T17: -ugn prints both names ---'
$r = RunId @('-u', '-g', '-n')
$parts = $r.StdOut.Trim() -split '\s+'
check ($parts.Count -eq 2) "-ugn prints two values (got $($parts.Count))"

Write-Host ''; Write-Host '--- T18: --zero delimits with NUL ---'
$r = RunId @('-G', '-z')
$bytes = [System.Text.Encoding]::ASCII.GetBytes($r.StdOut)
$nulCount = ($bytes | Where-Object { $_ -eq 0 }).Count
check ($nulCount -gt 0) "--zero produces NUL characters ($nulCount NULs)"

Write-Host ''; Write-Host '--- T19: -Gz delimits with NUL ---'
$r = RunId @('-Gz')
$bytes = [System.Text.Encoding]::ASCII.GetBytes($r.StdOut)
$nulCount = ($bytes | Where-Object { $_ -eq 0 }).Count
check ($nulCount -gt 0) "-Gz produces NUL characters ($nulCount NULs)"

Write-Host ''; Write-Host '--- T20: --help exits 0 ---'
$r = RunId @('--help')
check ($r.ExitCode -eq 0) "--help exits 0"
check ($r.StdOut -match 'Usage:') "--help contains Usage:"

Write-Host ''; Write-Host '--- T21: --version exits 0 ---'
$r = RunId @('--version')
check ($r.ExitCode -eq 0) "--version exits 0"
check ($r.StdOut -match 'id v') "--version contains version string"

Write-Host ''; Write-Host '--- T22: invalid option exits 1 ---'
$r = RunId @('-Q')
check ($r.ExitCode -eq 1) "invalid option -Q exits 1"

Write-Host ''; Write-Host '--- T23: -n without -ugG is error ---'
$r = RunId @('-n')
check ($r.ExitCode -eq 1) "-n alone exits 1"

Write-Host ''; Write-Host '--- T24: -r without -ugG is error ---'
$r = RunId @('-r')
check ($r.ExitCode -eq 1) "-r alone exits 1"

Write-Host ''; Write-Host '--- T25: extra operand is error ---'
$r = RunId @('user1', 'user2')
check ($r.ExitCode -eq 1) "extra operand exits 1"

Write-Host ''; Write-Host '--- T26: id with current username ---'
$r = RunId @($currentUser)
check ($r.ExitCode -eq 0) "id $currentUser exits 0"
check ($r.StdOut -match 'uid=\d+') "id $currentUser contains uid="

Write-Host ''; Write-Host '--- T27: id nonexistent user fails ---'
$r = RunId @('nosuchuser12345')
check ($r.ExitCode -eq 1) "id nosuchuser12345 exits 1"
check ($r.StdErr -match 'no such user') "error message contains 'no such user'"

Write-Host ''; Write-Host '--- T28: -z with default format ---'
$r = RunId @('-z')
$bytes = [System.Text.Encoding]::ASCII.GetBytes($r.StdOut)
$nulCount = ($bytes | Where-Object { $_ -eq 0 }).Count
check ($nulCount -gt 0) "-z with default produces NUL at end"

Write-Host ''; Write-Host '--- T29: -- separator ---'
$r = RunId @('--', $currentUser)
check ($r.ExitCode -eq 0) "-- before username works"

Write-Host ''; Write-Host '--- T30: -urn prints real username ---'
$r = RunId @('-u', '-r', '-n')
$unameR = $r.StdOut.Trim()
$r2 = RunId @('-u', '-n')
$unameE = $r2.StdOut.Trim()
check ($unameR -eq $unameE) "-urn equals -un on Windows (real==effective)"

Write-Host ''; Write-Host '============================================'
Write-Host "  Test Results: $PASS passed, $FAIL failed"
Write-Host '============================================'

if ($FAIL -gt 0) { exit 1 }
exit 0
