$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$pythonExe = Join-Path $nativeRoot '.venv\Scripts\python.exe'
$serviceScript = Join-Path $nativeRoot 'service\streamget_service.py'
$distPath = Join-Path $nativeRoot 'out\service'
$workPath = Join-Path $nativeRoot 'out\pyinstaller'
$outputExe = Join-Path $distPath 'streamget_service.exe'

if (-not (Test-Path $pythonExe)) {
    throw "Python virtual environment not found: $pythonExe"
}

New-Item -ItemType Directory -Force -Path $distPath, $workPath | Out-Null
& $pythonExe -m PyInstaller --noconfirm --clean --onefile --name streamget_service `
    --distpath $distPath --workpath $workPath --specpath $workPath $serviceScript *> $null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $outputExe)) {
    throw 'PyInstaller failed to build streamget_service.exe'
}

Write-Output $outputExe
