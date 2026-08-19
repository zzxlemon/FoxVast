@echo off
rem ============================================================
rem  FoxVast - Convert all text files to GBK encoding
rem  Idempotent. Binary files, already-GBK files and files that
rem  cannot be represented in GBK (would lose data) are skipped.
rem  Author: Lemon_Chicken
rem ============================================================
setlocal
chcp 437 >nul
set "PSFILE=%TEMP%\fox_convert_gbk.ps1"
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
for /f "delims=:" %%a in ('findstr /n "^rem ====PS-BEGIN====" "%~f0"') do set "SKIP=%%a"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Content -LiteralPath '%~f0' -Encoding ASCII | Select-Object -Skip %SKIP% | Set-Content -LiteralPath '%PSFILE%' -Encoding UTF8"
powershell -NoProfile -ExecutionPolicy Bypass -File "%PSFILE%" "%ROOT%"
set "RC=%ERRORLEVEL%"
del "%PSFILE%" >nul 2>nul
endlocal
exit /b %RC%

rem ====PS-BEGIN====
param([string]$Root = (Get-Location).Path)

$skipExt = @('.fc','.exe','.dll','.a','.lib','.png','.ttf','.crt','.o','.obj','.zip','.far','.fz','.fx','.ico','.bmp','.jpg','.jpeg','.gif','.woff','.woff2','.dat','.db','.pdb','.ilk','.exp','.res','.jar','.pdf','.doc','.docx','.xls','.xlsx','.pyc','.class','.so','.dylib','.pcm','.wav','.mp3','.zip','.gz','.xz','.zst','.bz2','.tar','.7z','.rar')
$skipDir = @('.git','cmake-build','build','node_modules','.vs','.idea')

$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$gbkStrict = [Text.Encoding]::GetEncoding(936,
              (New-Object System.Text.EncoderExceptionFallback),
              (New-Object System.Text.DecoderExceptionFallback))

$converted = 0
$already   = 0
$skipped   = 0
$lost      = @()

$files = Get-ChildItem -Recurse -File -LiteralPath $Root | Where-Object {
    if ($skipExt -contains $_.Extension.ToLower()) { return $false }
    $rel = $_.FullName.Substring($Root.Length).TrimStart('\','/')
    foreach ($d in $skipDir) {
        if ($rel.StartsWith($d + '\') -or $rel.StartsWith($d + '/')) { return $false }
    }
    return $true
}

foreach ($f in $files) {
    $raw = [IO.File]::ReadAllBytes($f.FullName)
    if ($raw.Length -eq 0) { continue }
    $hasHigh = $false
    foreach ($b in $raw) { if ($b -gt 127) { $hasHigh = $true; break } }
    if (-not $hasHigh) { $already++; continue }

    try { $text = $utf8Strict.GetString($raw) }
    catch { $already++; continue }

    try {
        $bytes = $gbkStrict.GetBytes($text)
        $check = $gbkStrict.GetString($bytes)
        if ($check -ne $text) { $skipped++; $lost += $f.FullName; continue }
        [IO.File]::WriteAllBytes($f.FullName, $bytes)
        $converted++
    }
    catch { $skipped++; $lost += $f.FullName }
}

Write-Host "Converted to GBK: $converted"
Write-Host "Already ASCII/GBK/binary: $already"
Write-Host "Skipped (GBK-unrepresentable, no data loss): $skipped"
if ($lost.Count -gt 0) {
    Write-Host '--- Skipped files:'
    foreach ($p in $lost) { Write-Host "  $p" }
}
