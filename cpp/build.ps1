# Builds the C++ project on Windows without needing a "Developer Command Prompt".
# Usage (from the repo root or from cpp/):
#   powershell -ExecutionPolicy Bypass -File cpp\build.ps1            # configure + build
#   powershell -ExecutionPolicy Bypass -File cpp\build.ps1 -Test      # ... and run the rules tests
#   powershell -ExecutionPolicy Bypass -File cpp\build.ps1 -Run       # ... and start the game
param(
    [switch]$Test,
    [switch]$Run,
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo "cpp"
$build = Join-Path $repo $BuildDir

# Find Visual Studio's C++ tools (full IDE or just the Build Tools) via vswhere.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio Build Tools not found. Install 'Desktop development with C++'." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "The C++ workload is missing from Visual Studio / Build Tools." }

# CMake and Ninja ship with that workload; use PATH versions if present.
$vsCmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $env:Path = "$vsCmake\CMake\bin;$env:Path" }
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) { $env:Path = "$vsCmake\Ninja;$env:Path" }

$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$cmd = "`"$vcvars`" >nul 2>&1 && cmake -S `"$source`" -B `"$build`" -G Ninja && cmake --build `"$build`""
if ($Test) { $cmd += " && ctest --test-dir `"$build`" --output-on-failure" }
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

if ($Run) { Start-Process (Join-Path $build "app\chess.exe") }
