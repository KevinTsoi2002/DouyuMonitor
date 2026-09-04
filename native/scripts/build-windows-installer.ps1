$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$releaseDir = Join-Path $nativeRoot 'out\build\windows-x64-release'
$installerRoot = Join-Path $nativeRoot 'out\installer'
$stageRoot = Join-Path $installerRoot 'stage'
$stageDir = Join-Path $stageRoot 'DouyuMonitor'
$installerScript = Join-Path $nativeRoot 'installer\DouyuMonitor.iss'
$appIconPath = Join-Path $nativeRoot 'app\assets\douyu_monitor.ico'
$cmakeListPath = Join-Path $nativeRoot 'CMakeLists.txt'

foreach ($requiredFile in @(
    (Join-Path $releaseDir 'douyu_monitor_native.exe'),
    (Join-Path $releaseDir 'streamget_service.exe'),
    $appIconPath,
    $installerScript
)) {
    if (-not (Test-Path $requiredFile)) { throw "Required installer input was not found: $requiredFile" }
}

$cmakeText = Get-Content -Raw $cmakeListPath
$versionMatch = [regex]::Match(
    $cmakeText,
    'project\s*\(\s*DouyuMonitorNative\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
if (-not $versionMatch.Success) { throw "Could not read the DouyuMonitor version from $cmakeListPath." }
$projectVersion = $versionMatch.Groups[1].Value

$innoCandidates = @(
    $env:INNO_SETUP_COMPILER,
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe',
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
$innoCompiler = $innoCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($null -eq $innoCompiler) {
    throw 'Inno Setup 6 compiler (ISCC.exe) was not found. Install Inno Setup 6 or set INNO_SETUP_COMPILER.'
}

Remove-Item $installerRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Get-ChildItem $releaseDir -File | Where-Object {
    $_.Name -in @('douyu_monitor_native.exe', 'streamget_service.exe') -or
    ($_.Extension -eq '.dll' -and $_.Name -notlike 'test*')
} | Copy-Item -Destination $stageDir
Copy-Item $appIconPath -Destination $stageDir

foreach ($directory in @('platforms', 'qml', 'imageformats', 'iconengines', 'styles', 'tls', 'networkinformation', 'generic')) {
    $source = Join-Path $releaseDir $directory
    if (Test-Path $source) { Copy-Item $source -Destination $stageDir -Recurse }
}

if (-not (Test-Path (Join-Path $stageDir 'platforms\qwindows.dll'))) {
    throw 'Release Qt platform plugin qwindows.dll is missing from the installer stage.'
}

$dumpbin = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022' -Filter dumpbin.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\Hostx64\\x64\\dumpbin\.exe$' } |
    Select-Object -First 1
if ($null -eq $dumpbin) { throw 'Visual Studio x64 dumpbin.exe is required to validate the installer stage.' }
cmake -DRUNTIME_DIR="$stageDir" -DRUNTIME_EXE="$stageDir\douyu_monitor_native.exe" `
    -DDUMPBIN_EXECUTABLE="$($dumpbin.FullName)" `
    -P (Join-Path $nativeRoot 'cmake\verify_runtime_dependencies.cmake') | Out-Null

& $innoCompiler "/DSourceDir=$stageDir" "/DOutputDir=$installerRoot" "/DAppVersion=$projectVersion" $installerScript
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed with exit code $LASTEXITCODE." }
$installerExe = Join-Path $installerRoot "DouyuMonitor-Setup-V$projectVersion.exe"
if (-not (Test-Path $installerExe)) { throw "Inno Setup did not create $installerExe." }

Write-Output $installerExe
