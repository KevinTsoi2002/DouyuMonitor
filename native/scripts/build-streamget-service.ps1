$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $nativeRoot '..')).Path
$pythonExe = Join-Path $nativeRoot '.venv\Scripts\python.exe'
$serviceScript = Join-Path $nativeRoot 'service\streamget_service.py'
$distPath = Join-Path $nativeRoot 'out\service'
$workPath = Join-Path $nativeRoot 'out\pyinstaller'
$outputExe = Join-Path $distPath 'streamget_service.exe'

if (-not (Test-Path $pythonExe)) {
    throw "Python virtual environment not found: $pythonExe"
}

New-Item -ItemType Directory -Force -Path $distPath, $workPath | Out-Null
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $pythonExe -m PyInstaller --noconfirm --clean --onefile --log-level WARN --name streamget_service `
    --paths $repoRoot --distpath $distPath --workpath $workPath --specpath $workPath $serviceScript 2>&1 | Out-Null
$pyInstallerExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorActionPreference
if ($pyInstallerExitCode -ne 0 -or -not (Test-Path $outputExe)) {
    throw 'PyInstaller failed to build streamget_service.exe'
}

Write-Output $outputExe
