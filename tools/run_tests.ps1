# Runs the existing test suites. Used by the pre-push hook and handy by hand.
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1             # everything
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 -Cpp        # only C++
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 -Flutter    # only Flutter
# Exits non-zero as soon as a step fails.
param(
    [switch]$Cpp,
    [switch]$Flutter,
    [switch]$Python
)

$ErrorActionPreference = "Stop"
if (-not $Cpp -and -not $Flutter -and -not $Python) { $Cpp = $true; $Flutter = $true; $Python = $true }
$repo = Split-Path -Parent $PSScriptRoot

function Step([string]$name, [scriptblock]$block) {
    Write-Host ""
    Write-Host "==> $name" -ForegroundColor Cyan
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$name FAILED (exit code $LASTEXITCODE)" }
}

try {
    if ($Cpp) {
        # Builds, then runs the rules tests (basic, perft, scenarios, pgn) through CTest.
        Step "C++ build and rules tests" {
            powershell -NoProfile -ExecutionPolicy Bypass -File "$repo\cpp\build.ps1" -Test
        }
        # Drives the real input handlers (clicks, drags, promotion, undo, castling, themes).
        Step "C++ app self-test" {
            $exe = Join-Path $repo "build\app\chess.exe"
            $work = Join-Path $env:TEMP "chess_selftest"
            New-Item -ItemType Directory -Force $work | Out-Null
            Remove-Item "$work\selftest.log" -ErrorAction SilentlyContinue
            $p = Start-Process $exe -ArgumentList "--selftest" -WorkingDirectory $work -Wait -PassThru
            if (Test-Path "$work\selftest.log") { Get-Content "$work\selftest.log" | Where-Object { $_ -match "FAIL|PASSED" } }
            $global:LASTEXITCODE = $p.ExitCode
        }
    }

    if ($Flutter) {
        if (-not (Get-Command flutter -ErrorAction SilentlyContinue)) {
            foreach ($dir in @("C:\src\flutter\bin", "$env:USERPROFILE\flutter\bin")) {
                if (Test-Path "$dir\flutter.bat") { $env:Path = "$dir;$env:Path"; break }
            }
        }
        if (-not (Get-Command flutter -ErrorAction SilentlyContinue)) {
            throw "Flutter not found on PATH (or in C:\src\flutter\bin)."
        }
        Push-Location "$repo\flutter"
        try {
            Step "Flutter pub get" { flutter pub get }
            Step "Flutter analyze" { flutter analyze }
            Step "Flutter tests" { flutter test }
        } finally {
            Pop-Location
        }
    }

    if ($Python) {
        if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw "Python not found on PATH." }
        Push-Location "$repo\python"
        try {
            # Installs pygame-ce and pytest only when they are missing.
            python -c "import pygame, pytest" 2>$null
            if ($LASTEXITCODE -ne 0) { Step "Python requirements" { python -m pip install -q -r requirements.txt } }
            Step "Python tests" { python -m pytest -q }
        } finally {
            Pop-Location
        }
    }
} catch {
    Write-Host ""
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "All requested tests passed." -ForegroundColor Green
exit 0
