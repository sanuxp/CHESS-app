# Downloads the official Stockfish build for Windows into engine\ (git-ignored) and checks its SHA-256.
#   powershell -ExecutionPolicy Bypass -File tools\get_stockfish.ps1
# Stockfish is GPLv3 software: https://github.com/official-stockfish/Stockfish
# We run it as a separate program and do not commit the binary. If you distribute an app together with it,
# the GPL obligations (licence text, source offer) apply to that distribution.
param([string]$Dir = (Join-Path (Split-Path -Parent $PSScriptRoot) "engine"))

$ErrorActionPreference = "Stop"
$version = "sf_19"
$asset = "stockfish-windows-x86-64-universal.zip"   # runs on any x86-64 CPU, picks the best code path at start
$sha256 = "3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7"
$url = "https://github.com/official-stockfish/Stockfish/releases/download/$version/$asset"

$exe = Join-Path $Dir "stockfish.exe"
if (Test-Path $exe) { Write-Host "Already installed: $exe"; & $exe quit 2>$null | Out-Null; exit 0 }

New-Item -ItemType Directory -Force $Dir | Out-Null
$tmp = Join-Path $env:TEMP "stockfish_download"
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $tmp | Out-Null
$zip = Join-Path $tmp $asset

Write-Host "Downloading $url ..."
Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
$actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $sha256) { throw "Checksum mismatch: expected $sha256, got $actual. Not installing." }

Expand-Archive -Path $zip -DestinationPath $tmp -Force
$found = Get-ChildItem $tmp -Recurse -Filter "stockfish*.exe" | Select-Object -First 1
if (-not $found) { throw "No stockfish executable found inside the archive." }
Copy-Item $found.FullName $exe -Force
Set-Content -Path (Join-Path $Dir "VERSION.txt") -Value "$version ($asset)"
Remove-Item $tmp -Recurse -Force

Write-Host "Installed $exe"
