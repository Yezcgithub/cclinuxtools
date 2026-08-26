$ErrorActionPreference = 'Continue'
$exe = './cksum.exe'
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
Write-Host '--- T01: CRC (POSIX) abc ---'
$r = RunExe 'abc.txt'
check ($r.StdOut.Trim() -eq '1219131554 3 abc.txt') 'CRC abc'

Write-Host ''
Write-Host '--- T02: CRC empty file ---'
$r = RunExe 'empty.txt'
check ($r.StdOut.Trim() -eq '4294967295 0 empty.txt') 'CRC empty'

Write-Host ''
Write-Host '--- T03: CRC stdin ---'
$r = RunExeStdin '-' $abcBytes
check ($r.StdOut.Trim() -eq '1219131554 3') 'CRC stdin'

Write-Host ''
Write-Host '--- T04: CRC32B abc ---'
$r = RunExe '-a crc32b abc.txt'
check ($r.StdOut.Trim() -eq '891568578 3 abc.txt') 'CRC32B abc'

Write-Host ''
Write-Host '--- T05: SYSV abc ---'
$r = RunExe '-a sysv abc.txt'
check ($r.StdOut.Trim() -eq '294 1 abc.txt') 'SYSV abc'

Write-Host ''
Write-Host '--- T06: BSD abc ---'
$r = RunExe '-a bsd abc.txt'
check ($r.StdOut.Trim() -eq '16556     1 abc.txt') 'BSD abc'

Write-Host ''
Write-Host '--- T07: MD5 abc ---'
$r = RunExe '-a md5 abc.txt'
check ($r.StdOut.Trim() -eq 'MD5 (abc.txt) = 900150983cd24fb0d6963f7d28e17f72') 'MD5 abc'

Write-Host ''
Write-Host '--- T08: MD5 empty ---'
$r = RunExe '-a md5 empty.txt'
check ($r.StdOut.Trim() -eq 'MD5 (empty.txt) = d41d8cd98f00b204e9800998ecf8427e') 'MD5 empty'

Write-Host ''
Write-Host '--- T09: SHA1 abc ---'
$r = RunExe '-a sha1 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA1 (abc.txt) = a9993e364706816aba3e25717850c26c9cd0d89d') 'SHA1 abc'

Write-Host ''
Write-Host '--- T10: SHA224 abc ---'
$r = RunExe '-a sha224 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA224 (abc.txt) = 23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7') 'SHA224 abc'

Write-Host ''
Write-Host '--- T11: SHA256 abc ---'
$r = RunExe '-a sha256 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') 'SHA256 abc'

Write-Host ''
Write-Host '--- T12: SHA256 empty ---'
$r = RunExe '-a sha256 empty.txt'
check ($r.StdOut.Trim() -eq 'SHA256 (empty.txt) = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855') 'SHA256 empty'

Write-Host ''
Write-Host '--- T13: SHA384 abc ---'
$r = RunExe '-a sha384 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA384 (abc.txt) = cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7') 'SHA384 abc'

Write-Host ''
Write-Host '--- T14: SHA512 abc ---'
$r = RunExe '-a sha512 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA512 (abc.txt) = ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f') 'SHA512 abc'

Write-Host ''
Write-Host '--- T15: SHA512 empty ---'
$r = RunExe '-a sha512 empty.txt'
check ($r.StdOut.Trim() -eq 'SHA512 (empty.txt) = cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e') 'SHA512 empty'

Write-Host ''
Write-Host '--- T16: BLAKE2b abc ---'
$r = RunExe '-a blake2b abc.txt'
check ($r.StdOut.Trim() -eq 'BLAKE2b (abc.txt) = ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923') 'BLAKE2b abc'

Write-Host ''
Write-Host '--- T17: BLAKE2b empty ---'
$r = RunExe '-a blake2b empty.txt'
check ($r.StdOut.Trim() -eq 'BLAKE2b (empty.txt) = 786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce') 'BLAKE2b empty'

Write-Host ''
Write-Host '--- T18: BLAKE2b -l 256 abc ---'
$r = RunExe '-a blake2b -l 256 abc.txt'
check ($r.StdOut.Trim() -eq 'BLAKE2b (abc.txt) = bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319') 'BLAKE2b 256 abc'

Write-Host ''
Write-Host '--- T19: --untagged SHA256 ---'
$r = RunExe '-a sha256 --untagged abc.txt'
check ($r.StdOut.Trim() -eq 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt') 'untagged'

Write-Host ''
Write-Host '--- T20: --base64 SHA256 ---'
$r = RunExe '-a sha256 --base64 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA256 (abc.txt) = ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=') 'base64'

Write-Host ''
Write-Host '--- T21: --check tagged OK ---'
WriteNoBom 'check_ok.txt' ("SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`n")
$r = RunExe '-a sha256 --check check_ok.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check tagged OK'

Write-Host ''
Write-Host '--- T22: --check untagged OK ---'
WriteNoBom 'check_untag.txt' ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  abc.txt`n")
$r = RunExe '-a sha256 --check check_untag.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check untagged OK'

Write-Host ''
Write-Host '--- T23: --check FAILED ---'
WriteNoBom 'check_bad.txt' ("SHA256 (abc.txt) = 0000000000000000000000000000000000000000000000000000000000000000`n")
$r = RunExe '-a sha256 --check check_bad.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: FAILED' -and $r.ExitCode -eq 1) 'check FAILED'

Write-Host ''
Write-Host '--- T24: --check --status OK ---'
$r = RunExe '-a sha256 --check --status check_ok.txt'
check ($r.ExitCode -eq 0) 'check status OK'

Write-Host ''
Write-Host '--- T25: --check --status FAILED ---'
$r = RunExe '-a sha256 --check --status check_bad.txt'
check ($r.ExitCode -eq 1) 'check status FAILED'

Write-Host ''
Write-Host '--- T26: --check --quiet ---'
$r = RunExe '-a sha256 --check --quiet check_ok.txt'
check ($r.ExitCode -eq 0) 'check quiet'

Write-Host ''
Write-Host '--- T27: multiple files ---'
$r = RunExe '-a md5 abc.txt empty.txt'
$lines = $r.StdOut.Trim() -split "`n"
check ($lines.Count -eq 2 -and $lines[0].Contains('900150983cd24fb0d6963f7d28e17f72') -and $lines[1].Contains('d41d8cd98f00b204e9800998ecf8427e')) 'multiple files'

Write-Host ''
Write-Host '--- T28: --help exits 0 ---'
$r = RunExe '--help'
check ($r.ExitCode -eq 0 -and $r.StdOut.Contains('Usage:')) 'help'

Write-Host ''
Write-Host '--- T29: --version exits 0 ---'
$r = RunExe '--version'
check ($r.ExitCode -eq 0 -and $r.StdOut.Contains('cksum')) 'version'

Write-Host ''
Write-Host '--- T30: big file CRC ---'
$r = RunExe 'big.bin'
$parts = $r.StdOut.Trim() -split ' '
check ($parts[1] -eq '100000') 'big file size'

Write-Host ''
Write-Host '--- T31: --check --ignore-missing ---'
WriteNoBom 'check_missing.txt' ("SHA256 (nonexistent.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`n")
$r = RunExe '-a sha256 --check --ignore-missing check_missing.txt'
check ($r.ExitCode -eq 0) 'ignore-missing'

Write-Host ''
Write-Host '--- T32: --check missing file fails ---'
$r = RunExe '-a sha256 --check check_missing.txt'
check ($r.ExitCode -eq 1) 'missing file fails'

Write-Host ''
Write-Host '--- T33: CRC large file consistency ---'
$r1 = RunExe 'big.bin'
$r2 = RunExe 'big.bin'
check ($r1.StdOut -eq $r2.StdOut) 'CRC consistency'

Write-Host ''
Write-Host '--- T34: --algorithm=crc syntax ---'
$r = RunExe '--algorithm=crc abc.txt'
check ($r.StdOut.Trim() -eq '1219131554 3 abc.txt') 'algorithm=crc'

Write-Host ''
Write-Host '--- T35: --algorithm sha256 syntax ---'
$r = RunExe '--algorithm sha256 abc.txt'
check ($r.StdOut.Trim() -eq 'SHA256 (abc.txt) = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') 'algorithm sha256'

Write-Host ''
Write-Host '--- T36: MD5 --base64 ---'
$r = RunExe '-a md5 --base64 abc.txt'
check ($r.StdOut.Trim() -eq 'MD5 (abc.txt) = kAFQmDzST7DWlj99KOF/cg==') 'MD5 base64'

Write-Host ''
Write-Host '--- T37: BLAKE2b -l 128 ---'
$r = RunExe '-a blake2b -l 128 abc.txt'
check ($r.StdOut.StartsWith('BLAKE2b (abc.txt) = ') -and $r.StdOut.Trim().Length -gt 40) 'BLAKE2b 128'

Write-Host ''
Write-Host '--- T38: --check base64 tagged ---'
WriteNoBom 'check_b64.txt' ("SHA256 (abc.txt) = ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=`n")
$r = RunExe '-a sha256 --check check_b64.txt'
check ($r.StdOut.Trim() -eq 'abc.txt: OK' -and $r.ExitCode -eq 0) 'check base64'

Write-Host ''
Write-Host '--- T39: CRC32B empty ---'
$r = RunExe '-a crc32b empty.txt'
check ($r.StdOut.Trim() -eq '0 0 empty.txt') 'CRC32B empty'

Write-Host ''
Write-Host '--- T40: SHA256 stdin ---'
$r = RunExeStdin '-a sha256 -' $abcBytes
check ($r.StdOut.Trim() -eq 'SHA256 = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') 'SHA256 stdin'

# Cleanup
Remove-Item abc.txt, empty.txt, big.bin, check_ok.txt, check_untag.txt, check_bad.txt, check_missing.txt, check_b64.txt -ErrorAction SilentlyContinue

Write-Host ''
Write-Host '============================================'
Write-Host ("  Results: $PASS passed, $FAIL failed")
Write-Host '============================================'
if ($FAIL -gt 0) { exit 1 } else { exit 0 }
