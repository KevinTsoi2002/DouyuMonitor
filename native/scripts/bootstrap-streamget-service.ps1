param(
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$venvRoot = Join-Path $nativeRoot '.venv'
$pythonExe = Join-Path $venvRoot 'Scripts\python.exe'

if (-not (Test-Path $pythonExe)) {
    & $Python -m venv $venvRoot
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $pythonExe)) {
        throw 'Failed to create native Python virtual environment'
    }
}

foreach ($requirements in @('service\requirements.txt', 'service\requirements-build.txt')) {
    $requirementsPath = Join-Path $nativeRoot $requirements
    & $pythonExe -m pip install --requirement $requirementsPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install $requirements"
    }
}

Write-Output $pythonExe
