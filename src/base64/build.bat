@echo off
setlocal enabledelayedexpansion

echo ============================================
echo     base64.c Build Script for Windows
echo ============================================

set CC=gcc
set CFLAGS=-O2 -std=c99 -Wall -Wextra
set OUTPUT=base64.exe
set SOURCE=base64.c

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
    "Set-Content -Value '' -Path empty.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'f' -Path f.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'fo' -Path fo.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'foo' -Path foo.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'foob' -Path foob.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'fooba' -Path fooba.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value 'foobar' -Path foobar.bin -NoNewline -Encoding Ascii;" ^
    "Set-Content -Value ('A'*1000) -Path big.bin -NoNewline -Encoding Ascii;" ^
    "Write-Host '';" ^
    "Write-Host '--- T01: encode empty ---';" ^
    "$out = (& $exe empty.bin).Trim();" ^
    "check ($out -eq '') 'encode empty';" ^
    "Write-Host '';" ^
    "Write-Host '--- T02: encode f ---';" ^
    "$out = (& $exe f.bin).Trim();" ^
    "check ($out -eq 'Zg==') 'encode f';" ^
    "Write-Host '';" ^
    "Write-Host '--- T03: encode fo ---';" ^
    "$out = (& $exe fo.bin).Trim();" ^
    "check ($out -eq 'Zm8=') 'encode fo';" ^
    "Write-Host '';" ^
    "Write-Host '--- T04: encode foo ---';" ^
    "$out = (& $exe foo.bin).Trim();" ^
    "check ($out -eq 'Zm9v') 'encode foo';" ^
    "Write-Host '';" ^
    "Write-Host '--- T05: encode foob ---';" ^
    "$out = (& $exe foob.bin).Trim();" ^
    "check ($out -eq 'Zm9vYg==') 'encode foob';" ^
    "Write-Host '';" ^
    "Write-Host '--- T06: encode fooba ---';" ^
    "$out = (& $exe fooba.bin).Trim();" ^
    "check ($out -eq 'Zm9vYmE=') 'encode fooba';" ^
    "Write-Host '';" ^
    "Write-Host '--- T07: encode foobar ---';" ^
    "$out = (& $exe foobar.bin).Trim();" ^
    "check ($out -eq 'Zm9vYmFy') 'encode foobar';" ^
    "Write-Host '';" ^
    "Write-Host '--- T08: decode f ---';" ^
    "WriteNoBom 'dec_f.txt' ('Zg=='+\"`n\");" ^
    "$out = & $exe -d dec_f.txt;" ^
    "check ($out -eq 'f') 'decode f';" ^
    "Write-Host '';" ^
    "Write-Host '--- T09: decode foobar ---';" ^
    "WriteNoBom 'dec_fb.txt' ('Zm9vYmFy'+\"`n\");" ^
    "$out = & $exe -d dec_fb.txt;" ^
    "check ($out -eq 'foobar') 'decode foobar';" ^
    "Write-Host '';" ^
    "Write-Host '--- T10: roundtrip foobar ---';" ^
    "$enc = & $exe foobar.bin;" ^
    "WriteNoBom 'rt.txt' ($enc);" ^
    "$out = & $exe -d rt.txt;" ^
    "check ($out -eq 'foobar') 'roundtrip foobar';" ^
    "Write-Host '';" ^
    "Write-Host '--- T11: no padding decode ---';" ^
    "WriteNoBom 'nopad.txt' ('Zm9vYmFy'+\"`n\");" ^
    "$out = & $exe -d nopad.txt;" ^
    "check ($out -eq 'foobar') 'no padding decode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T12: -w 0 (no wrap) ---';" ^
    "$out = & $exe -w 0 big.bin;" ^
    "$lines = ($out -split \"`n\").Count;" ^
    "check ($lines -eq 1) 'wrap 0 single line';" ^
    "Write-Host '';" ^
    "Write-Host '--- T13: -w 10 ---';" ^
    "$out = & $exe -w 10 big.bin;" ^
    "$lines = ($out -split \"`n\").Count;" ^
    "check ($lines -gt 1) 'wrap 10 multi line';" ^
    "Write-Host '';" ^
    "Write-Host '--- T14: ignore-garbage ---';" ^
    "WriteNoBom 'garbage.txt' ('Zg@=='+\"`n\");" ^
    "& $exe -d garbage.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'garbage without -i fails';" ^
    "$out = & $exe -d -i garbage.txt 2>`$null;" ^
    "check ($out -eq 'f') 'garbage with -i decodes';" ^
    "Write-Host '';" ^
    "Write-Host '--- T15: invalid input ---';" ^
    "WriteNoBom 'invalid.txt' ('@@@@@@@@'+\"`n\");" ^
    "& $exe -d invalid.txt 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid input fails';" ^
    "Write-Host '';" ^
    "Write-Host '--- T16: --help ---';" ^
    "$out = & $exe --help;" ^
    "check ($out -match 'Usage:') 'help';" ^
    "Write-Host '';" ^
    "Write-Host '--- T17: --version ---';" ^
    "$out = & $exe --version;" ^
    "check ($out -match 'base64') 'version';" ^
    "Write-Host '';" ^
    "Write-Host '--- T18: default wrap 76 ---';" ^
    "Set-Content -Value ('A'*1000) -Path big76.bin -NoNewline -Encoding Ascii;" ^
    "$out = & $exe big76.bin;" ^
    "$maxlen = ($out -split \"`n\" | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum;" ^
    "check ($maxlen -le 76) 'default wrap 76';" ^
    "Write-Host '';" ^
    "Write-Host '--- T19: multi-file encode ---';" ^
    "$out = & $exe f.bin fo.bin;" ^
    "$lines = ($out -split \"`n\").Count;" ^
    "check ($lines -eq 2) 'multi-file encode 2 lines';" ^
    "Write-Host '';" ^
    "Write-Host '--- T20: invalid -w value ---';" ^
    "& $exe -w -1 f.bin 2>`$null | Out-Null;" ^
    "check ($LASTEXITCODE -ne 0) 'invalid -w -1';" ^
    "Write-Host '';" ^
    "Write-Host '--- T21: stdin encode ---';" ^
    "Set-Content -Value 'foo' -Path stdin_test.bin -NoNewline -Encoding Ascii;" ^
    "$out = & $exe - stdin_test.bin;" ^
    "$out2 = $out -split \"`n\";" ^
    "check ($out2[1].Trim() -eq 'Zm9v') 'stdin encode via -';" ^
    "Write-Host '';" ^
    "Write-Host '--- T22: lowercase is valid (different value) ---';" ^
    "WriteNoBom 'lower.txt' ('Zm9vYg=='+\"`n\");" ^
    "$out = & $exe -d lower.txt;" ^
    "check ($out -eq 'foob') 'valid case-sensitive decode';" ^
    "Write-Host '';" ^
    "Write-Host '--- T23: binary roundtrip via process ---';" ^
    "$rawbytes = New-Object byte[] 256; for ($i=0; $i -lt 256; $i++) { $rawbytes[$i] = $i };" ^
    "[System.IO.File]::WriteAllBytes('Y:\gitee\cclinuxtools\src\base64\allbytes.bin', $rawbytes);" ^
    "$enc = & $exe -w 0 allbytes.bin;" ^
    "WriteNoBom 'allbytes_enc.txt' ($enc + \"`n\");" ^
    "$p = New-Object System.Diagnostics.Process; $p.StartInfo.FileName = (Resolve-Path $exe).Path;" ^
    "$p.StartInfo.Arguments = '-d allbytes_enc.txt'; $p.StartInfo.UseShellExecute = $false;" ^
    "$p.StartInfo.RedirectStandardOutput = $true;" ^
    "$p.Start() | Out-Null;" ^
    "$decStream = $p.StandardOutput.BaseStream; $ms = New-Object System.IO.MemoryStream;" ^
    "$buf = New-Object byte[] 4096; while (($rd = $decStream.Read($buf, 0, 4096)) -gt 0) { $ms.Write($buf, 0, $rd) };" ^
    "$p.WaitForExit(); $decBytes = $ms.ToArray(); $ms.Dispose();" ^
    "$match = ($decBytes.Length -eq 256); if ($match) { for ($i=0; $i -lt 256; $i++) { if ($decBytes[$i] -ne $rawbytes[$i]) { $match = $false; break } } };" ^
    "check $match 'binary roundtrip all 256 byte values';" ^
    "Remove-Item empty.bin, f.bin, fo.bin, foo.bin, foob.bin, fooba.bin, foobar.bin, stdin_test.bin, allbytes.bin -ErrorAction SilentlyContinue;" ^
    "Remove-Item big.bin, big76.bin -ErrorAction SilentlyContinue;" ^
    "Remove-Item dec_f.txt, dec_fb.txt, rt.txt, nopad.txt, garbage.txt, invalid.txt, lower.txt, allbytes_enc.txt, allbytes_dec.bin -ErrorAction SilentlyContinue;" ^
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
