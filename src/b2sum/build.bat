@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     b2sum.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=b2sum.exe
set SOURCE=b2sum.c

echo(
echo [1/3] Cleaning previous build...
if exist "%OUTPUT%" (
    del "%OUTPUT%"
    echo   Removed %OUTPUT%
)

echo(
echo [2/3] Compiling %SOURCE%...
echo   Command: %CC% %CFLAGS% -o %OUTPUT% %SOURCE%
echo(
%CC% %CFLAGS% -o %OUTPUT% %SOURCE%

if %ERRORLEVEL% neq 0 (
    echo(
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo(
echo [3/3] Build succeeded!
echo   Output: %CD%\%OUTPUT%

echo(
echo ============================================
echo   Running full functional tests...
echo ============================================

REM Run all tests via a single PowerShell script block
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='Continue';" ^
    "$exe='./%OUTPUT%';" ^
    "$PASS=0; $FAIL=0;" ^
    "function check($cond, $name) { if ($cond) { Write-Host '  [PASS]'; $script:PASS++ } else { Write-Host '  [FAIL]'; $script:FAIL++ } };" ^
    "function WriteNoBom($path, $content) { [System.IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding $false)) };" ^
    "Set-Content -Value '' -Path test_empty.txt -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'abc' -Path test_abc.txt -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'xyz' -Path test_xyz.txt -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value ('A'*1000) -Path test_large.txt -NoNewline -Encoding Ascii;" ^
    "Write-Host '';" ^
    "Write-Host '--- T01: empty file hash ---';" ^
    "$h = (& $exe test_empty.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq '786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce') 'empty BLAKE2b';" ^
    "Write-Host '';" ^
    "Write-Host '--- T02: abc file hash ---';" ^
    "$h = (& $exe test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq 'ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923') 'abc BLAKE2b';" ^
    "Write-Host '';" ^
    "Write-Host '--- T03: -l 256 ---';" ^
    "$h = (& $exe -l 256 test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq 'bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319') 'BLAKE2b-256';" ^
    "Write-Host '';" ^
    "Write-Host '--- T04: -l 128 ---';" ^
    "$h = (& $exe -l 128 test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq 'cf4ab791c62b8d2b2109c90275287816') 'BLAKE2b-128';" ^
    "Write-Host '';" ^
    "Write-Host '--- T05: --tag ---';" ^
    "$out = & $exe --tag test_abc.txt;" ^
    "check ($out -match 'BLAKE2b \(test_abc.txt\)') 'tag format';" ^
    "Write-Host '';" ^
    "Write-Host '--- T06: -b binary mode ---';" ^
    "$out = & $exe -b test_abc.txt;" ^
    "check ($out -match '\*test_abc\.txt') 'binary separator';" ^
    "Write-Host '';" ^
    "Write-Host '--- T07: --check OK ---';" ^
    "$h = (& $exe test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "WriteNoBom 'chk.txt' ($h + '  test_abc.txt' + \"`n\");" ^
    "& $exe -c chk.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'check OK';" ^
    "Write-Host '';" ^
    "Write-Host '--- T08: --check FAILED ---';" ^
    "WriteNoBom 'bad.txt' ('00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  test_abc.txt' + \"`n\");" ^
    "& $exe -c bad.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 1) 'check FAILED';" ^
    "Write-Host '';" ^
    "Write-Host '--- T09: --check --quiet ---';" ^
    "$out = & $exe --check --quiet chk.txt 2>`$null;" ^
    "check ([string]::IsNullOrWhiteSpace($out)) 'quiet no output';" ^
    "Write-Host '';" ^
    "Write-Host '--- T10: --check --status ---';" ^
    "$out = & $exe --check --status chk.txt 2>`$null;" ^
    "check ([string]::IsNullOrWhiteSpace($out) -and $LASTEXITCODE -eq 0) 'status no output';" ^
    "Write-Host '';" ^
    "Write-Host '--- T11: --help ---';" ^
    "$out = & $exe --help;" ^
    "check ($out -match 'Usage:') 'help';" ^
    "Write-Host '';" ^
    "Write-Host '--- T12: --version ---';" ^
    "$out = & $exe --version;" ^
    "check ($out -match 'b2sum') 'version';" ^
    "Write-Host '';" ^
    "Write-Host '--- T13: large file (1000 bytes) ---';" ^
    "$h = (& $exe test_large.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq 'ffc91d5b8c0451522646f640b093e6d0ba10cad123c5d1cf39a1b43fce76d51ebbe529f908571e141118adad4554769f0f3b8323174c07f94e7d333e28d334df') 'large file';" ^
    "Write-Host '';" ^
    "Write-Host '--- T14: invalid -l 100 ---';" ^
    "& $exe -l 100 test_abc.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid -l 100';" ^
    "Write-Host '';" ^
    "Write-Host '--- T15: -l 0 (invalid) ---';" ^
    "& $exe -l 0 test_abc.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid -l 0';" ^
    "Write-Host '';" ^
    "Write-Host '--- T16: -l 520 (invalid) ---';" ^
    "& $exe -l 520 test_abc.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid -l 520';" ^
    "Write-Host '';" ^
    "Write-Host '--- T17: --check tag format ---';" ^
    "$h = (& $exe test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "WriteNoBom 'tagchk.txt' ('BLAKE2b (test_abc.txt) = ' + $h + \"`n\");" ^
    "& $exe -c tagchk.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'check tag format';" ^
    "Write-Host '';" ^
    "Write-Host '--- T18: multi-file check ---';" ^
    "$h1 = (& $exe test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "$h2 = (& $exe test_xyz.txt) -split '  ' | Select-Object -First 1;" ^
    "WriteNoBom 'multi.txt' ($h1 + '  test_abc.txt' + \"`n\" + $h2 + '  test_xyz.txt' + \"`n\");" ^
    "& $exe -c multi.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'multi-file check';" ^
    "Write-Host '';" ^
    "Write-Host '--- T19: --check --ignore-missing ---';" ^
    "WriteNoBom 'miss.txt' ('00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  nonexistent_file' + \"`n\");" ^
    "& $exe -c --ignore-missing miss.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'ignore-missing';" ^
    "Write-Host '';" ^
    "Write-Host '--- T20: --check missing (no --ignore-missing) ---';" ^
    "& $exe -c miss.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 1) 'missing file fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T21: stdin hash ---';" ^
    "$bytes = [System.Text.Encoding]::ASCII.GetBytes('abc');" ^
    "$p = New-Object System.Diagnostics.Process; $p.StartInfo.FileName = (Resolve-Path $exe).Path;" ^
    "$p.StartInfo.Arguments = '-'; $p.StartInfo.UseShellExecute = $false;" ^
    "$p.StartInfo.RedirectStandardInput = $true; $p.StartInfo.RedirectStandardOutput = $true;" ^
    "$p.Start() | Out-Null; $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length); $p.StandardInput.Close();" ^
    "$out = $p.StandardOutput.ReadToEnd(); $p.WaitForExit();" ^
    "$h = $out -split '  ' | Select-Object -First 1;" ^
    "check ($h -eq 'ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923') 'stdin hash';" ^
    "Write-Host '';" ^
    "Write-Host '--- T22: -z zero terminator ---';" ^
    "$bytes = [System.IO.File]::ReadAllBytes('test_abc.txt');" ^
    "$ms = New-Object System.IO.MemoryStream;" ^
    "$ms.Write($bytes, 0, $bytes.Length); $ms.WriteByte(0);" ^
    "$stdinBytes = $ms.ToArray(); $ms.Dispose();" ^
    "$p = New-Object System.Diagnostics.Process; $p.StartInfo.FileName = (Resolve-Path $exe).Path;" ^
    "$p.StartInfo.Arguments = '-z -'; $p.StartInfo.UseShellExecute = $false;" ^
    "$p.StartInfo.RedirectStandardInput = $true; $p.StartInfo.RedirectStandardOutput = $true;" ^
    "$p.Start() | Out-Null; $p.StandardInput.BaseStream.Write($stdinBytes, 0, $stdinBytes.Length); $p.StandardInput.Close();" ^
    "$out = $p.StandardOutput.ReadToEnd(); $p.WaitForExit();" ^
    "check ($out[-1] -eq [char]0) 'zero terminator';" ^
    "Write-Host '';" ^
    "Write-Host '--- T23: -l 8 (minimum) ---';" ^
    "& $exe -l 8 test_abc.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'l 8';" ^
    "Write-Host '';" ^
    "Write-Host '--- T24: -l 512 (maximum) ---';" ^
    "& $exe -l 512 test_abc.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -eq 0) 'l 512';" ^
    "Write-Host '';" ^
    "Write-Host '--- T25: -l 512 equals default ---';" ^
    "$h1 = (& $exe test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "$h2 = (& $exe -l 512 test_abc.txt) -split '  ' | Select-Object -First 1;" ^
    "check ($h1 -eq $h2) 'l 512 == default';" ^
    "Remove-Item test_empty.txt, test_abc.txt, test_xyz.txt, test_large.txt -ErrorAction SilentlyContinue;" ^
    "Remove-Item chk.txt, bad.txt, tagchk.txt, multi.txt, miss.txt -ErrorAction SilentlyContinue;" ^
    "Write-Host '';" ^
    "Write-Host ('============================================');" ^
    "Write-Host ('  Test Results: PASS=' + $PASS + '  FAIL=' + $FAIL);" ^
    "Write-Host ('============================================');" ^
    "if ($FAIL -eq 0) { Write-Host '  All tests passed!' } else { Write-Host '  Some tests failed!' }; " ^
    "exit $FAIL"

set /a TESTFAIL=%ERRORLEVEL%
echo(
echo ============================================
if %TESTFAIL% equ 0 (
    echo   All tests passed!
) else (
    echo   Some tests failed!
)
echo ============================================

endlocal & exit /b %TESTFAIL%
