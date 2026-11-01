param(
    [string]$DistPath = '',
    [string]$WorkPath = ''
)

$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $nativeRoot '..')).Path
$pythonExe = Join-Path $nativeRoot '.venv\Scripts\python.exe'
$verifyPath = Join-Path $nativeRoot 'service\verify_dependency_lock.py'
$provenanceScript = Join-Path $nativeRoot 'service\package_provenance.py'
$serviceScript = Join-Path $nativeRoot 'service\streamget_service.py'
$distPath = if ($DistPath) { $DistPath } else { Join-Path $nativeRoot 'out\service' }
$workPath = if ($WorkPath) { $WorkPath } else { Join-Path $nativeRoot 'out\pyinstaller' }
$outputDir = Join-Path $distPath 'streamget_service'
$outputExe = Join-Path $outputDir 'streamget_service.exe'

if (-not (Test-Path $pythonExe)) {
    throw "Python virtual environment not found: $pythonExe"
}

& $pythonExe $verifyPath
if ($LASTEXITCODE -ne 0) {
    throw 'StreamGet dependency lock verification failed; run bootstrap-streamget-service.ps1'
}

New-Item -ItemType Directory -Force -Path $distPath, $workPath | Out-Null
$provenancePath = Join-Path $outputDir 'build-provenance.json'
if (Test-Path -LiteralPath $provenancePath) {
    Remove-Item -LiteralPath $provenancePath -Force
}
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $pythonExe -m PyInstaller --noconfirm --clean --onedir --log-level WARN --name streamget_service `
    --paths $repoRoot --distpath $distPath --workpath $workPath --specpath $workPath `
    --add-data "$nativeRoot\app\resources\hamster_agent_roster.json;native/app/resources" `
    $serviceScript 2>&1 | Out-Null
$pyInstallerExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorActionPreference
if ($pyInstallerExitCode -ne 0 -or -not (Test-Path $outputExe)) {
    throw 'PyInstaller failed to build the streamget_service directory'
}

& $pythonExe $provenanceScript --native-root $nativeRoot --package-dir $outputDir
if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $outputDir 'build-provenance.json'))) {
    throw 'Failed to write the StreamGet package provenance manifest'
}

Write-Output $outputExe
Write-Output $provenancePath
