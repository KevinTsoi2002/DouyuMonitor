param(
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$venvRoot = Join-Path $nativeRoot '.venv'
$pythonExe = Join-Path $venvRoot 'Scripts\python.exe'
$lockPath = Join-Path $nativeRoot 'service\requirements-lock-win-py313.txt'
$verifyPath = Join-Path $nativeRoot 'service\verify_dependency_lock.py'

if (-not (Test-Path $pythonExe)) {
    & $Python -m venv $venvRoot
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $pythonExe)) {
        throw 'Failed to create native Python virtual environment'
    }
}

& $pythonExe -c 'import platform,sys; sys.exit(0 if platform.system()=="Windows" and sys.version_info[:3]==(3,13,15) else 1)'
if ($LASTEXITCODE -ne 0) {
    throw 'StreamGet requires Windows and CPython 3.13.15'
}

& $pythonExe -m pip install --constraint $lockPath --requirement $lockPath `
    --requirement (Join-Path $nativeRoot 'service\requirements.txt') `
    --requirement (Join-Path $nativeRoot 'service\requirements-build.txt')
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to install locked StreamGet runtime and build dependencies'
}

& $pythonExe $verifyPath
if ($LASTEXITCODE -ne 0) {
    throw 'StreamGet dependency lock verification failed'
}

Write-Output $pythonExe
