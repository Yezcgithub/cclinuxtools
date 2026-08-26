@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     base16.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=base16.exe
set SOURCE=base16.c

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

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='Continue';" ^
    "$exe='./%OUTPUT%';" ^
    "$PASS=0; $FAIL=0;" ^
    "function check($cond, $name) { if ($cond) { Write-Host '  [PASS]'; $script:PASS++ } else { Write-Host '  [FAIL]'; $script:FAIL++ } };" ^
    "function WriteNoBom($path, $content) { [System.IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding $false)) };" ^
    "function DecodeToBytes($argStr) {" ^
    "  $p = New-Object System.Diagnostics.Process;" ^
    "  $p.StartInfo.FileName = (Resolve-Path $exe).Path;" ^
    "  $p.StartInfo.Arguments = $argStr;" ^
    "  $p.StartInfo.UseShellExecute = $false;" ^
    "  $p.StartInfo.RedirectStandardOutput = $true;" ^
    "  $p.Start() | Out-Null;" ^
    "  $s = $p.StandardOutput.BaseStream;" ^
    "  $ms = New-Object System.IO.MemoryStream;" ^
    "  $buf = New-Object byte[] 4096;" ^
    "  while (($rd = $s.Read($buf,0,4096)) -gt 0) { $ms.Write($buf,0,$rd) };" ^
    "  $p.WaitForExit(); return $ms.ToArray()" ^
    "};" ^
    "$bytes = [byte[]](0xFE, 0x4F, 0x82);" ^
    "[System.IO.File]::WriteAllBytes('test.bin', $bytes);" ^
    "Set-Content -Value ('A'*1000) -Path big.bin -NoNewline -Encoding Ascii;" ^
    "Write-Host '';" ^
    "Write-Host '--- T01: encode ---';" ^
    "$out = (& $exe test.bin).Trim();" ^
    "check ($out -eq 'FE4F82') 'encode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T02: decode ---';" ^
    "WriteNoBom 'dec.txt' ('FE4F82' + \"`n\");" ^
    "$dec = DecodeToBytes '-d dec.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'decode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T03: lowercase decode ---';" ^
    "WriteNoBom 'lower.txt' ('fe4f82' + \"`n\");" ^
    "$dec = DecodeToBytes '-d lower.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'lowercase decode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T04: roundtrip ---';" ^
    "$enc = & $exe -w 0 test.bin; WriteNoBom 'rt.txt' ($enc);" ^
    "$dec = DecodeToBytes '-d rt.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'roundtrip';" ^
    "Write-Host '';" ^
    "Write-Host '--- T05: -w 0 (no wrap) ---';" ^
    "$out = & $exe -w 0 big.bin;" ^
    "$lines = ($out -split \"`n\").Count;" ^
    "check ($lines -eq 1) 'wrap 0 single line';" ^
    "Write-Host '';" ^
    "Write-Host '--- T06: -w 10 ---';" ^
    "$out = & $exe -w 10 big.bin;" ^
    "$lines = ($out -split \"`n\").Count;" ^
    "check ($lines -gt 1) 'wrap 10 multi line';" ^
    "Write-Host '';" ^
    "Write-Host '--- T07: default wrap 76 ---';" ^
    "$out = & $exe big.bin;" ^
    "$maxlen = ($out -split \"`n\" | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum;" ^
    "check ($maxlen -le 76) 'default wrap 76';" ^
    "Write-Host '';" ^
    "Write-Host '--- T08: --help ---';" ^
    "$out = & $exe --help;" ^
    "check ($out -match 'Usage:') 'help';" ^
    "Write-Host '';" ^
    "Write-Host '--- T09: --version ---';" ^
    "$out = & $exe --version;" ^
    "check ($out -match 'base16') 'version';" ^
    "Write-Host '';" ^
    "Write-Host '--- T10: odd length fails ---';" ^
    "WriteNoBom 'odd.txt' ('FE4' + \"`n\");" ^
    "& $exe -d odd.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'odd length fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T11: ignore-garbage ---';" ^
    "WriteNoBom 'garbage.txt' ('FE 4F-82' + \"`n\");" ^
    "& $exe -d garbage.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'garbage without -i fails';" ^
    "$dec = DecodeToBytes '-d -i garbage.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'garbage with -i decodes';" ^
    "Write-Host '';" ^
    "Write-Host '--- T12: invalid input ---';" ^
    "WriteNoBom 'invalid.txt' ('XYZ' + \"`n\");" ^
    "& $exe -d invalid.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid input fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T13: empty file encode ---';" ^
    "Set-Content -Value '' -Path empty.bin -NoNewline -Encoding Ascii;" ^
    "$out = (& $exe empty.bin).Trim();" ^
    "check ($out -eq '') 'empty file encode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T14: multi-file encode ---';" ^
    "Set-Content -Value 'f' -Path f1.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'o' -Path f2.bin -NoNewline -Encoding Ascii;" ^
    "$out = & $exe f1.bin f2.bin;" ^
    "$lines = $out -split \"`n\";" ^
    "check ($lines[0] -eq '66' -and $lines[1] -eq '6F') 'multi-file encode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T15: invalid -w value ---';" ^
    "& $exe -w -1 f1.bin 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid -w -1';" ^
    "Write-Host '';" ^
    "Write-Host '--- T16: stdin via - file ---';" ^
    "$out = & $exe - test.bin 2>`$null;" ^
    "check ($out -match 'FE4F82') 'stdin via - file';" ^
    "Write-Host '';" ^
    "Write-Host '--- T17: no padding needed (decode without newline) ---';" ^
    "WriteNoBom 'nopad.txt' 'FE4F82';" ^
    "$dec = DecodeToBytes '-d nopad.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'no newline decode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T18: file not found ---';" ^
    "& $exe nonexistent_file 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'file not found fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T19: binary roundtrip all 256 byte values ---';" ^
    "$rawbytes = New-Object byte[] 256; for ($i=0; $i -lt 256; $i++) { $rawbytes[$i] = $i };" ^
    "[System.IO.File]::WriteAllBytes('allbytes.bin', $rawbytes);" ^
    "$enc = & $exe -w 0 allbytes.bin;" ^
    "WriteNoBom 'allbytes_enc.txt' ($enc + \"`n\");" ^
    "$dec = DecodeToBytes '-d allbytes_enc.txt';" ^
    "$match = ($dec.Length -eq 256); if ($match) { for ($i=0; $i -lt 256; $i++) { if ($dec[$i] -ne $rawbytes[$i]) { $match = $false; break } } };" ^
    "check $match 'binary roundtrip all 256 byte values';" ^
    "Write-Host '';" ^
    "Write-Host '--- T20: mixed case decode ---';" ^
    "WriteNoBom 'mixed.txt' ('Fe4F82' + \"`n\");" ^
    "$dec = DecodeToBytes '-d mixed.txt';" ^
    "check ($dec.Length -eq 3 -and $dec[0] -eq 0xFE -and $dec[1] -eq 0x4F -and $dec[2] -eq 0x82) 'mixed case decode';" ^
    "Remove-Item test.bin, big.bin, empty.bin, f1.bin, f2.bin, allbytes.bin -ErrorAction SilentlyContinue;" ^
    "Remove-Item dec.txt, lower.txt, rt.txt, odd.txt, garbage.txt, invalid.txt, nopad.txt, mixed.txt, allbytes_enc.txt -ErrorAction SilentlyContinue;" ^
    "Write-Host '';" ^
    "Write-Host ('============================================');" ^
    "Write-Host ('  Test Results: PASS=' + $PASS + '  FAIL=' + $FAIL);" ^
    "Write-Host ('============================================');" ^
    "if ($FAIL -eq 0) { Write-Host '  All tests passed.' } else { Write-Host '  Some tests failed.' }; " ^
    "exit $FAIL"

set /a TESTFAIL=%ERRORLEVEL%
echo(
echo ============================================
if %TESTFAIL% equ 0 (
    echo   All tests passed.
) else (
    echo   Some tests failed.
)
echo ============================================

endlocal & exit /b %TESTFAIL%
