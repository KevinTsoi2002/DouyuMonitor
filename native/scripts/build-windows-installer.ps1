param(
    [string]$ReleaseDir = ''
)

$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$releaseDir = if ([string]::IsNullOrWhiteSpace($ReleaseDir)) {
    Join-Path $nativeRoot 'out\build\windows-x64-release'
} else {
    (Resolve-Path $ReleaseDir).Path
}
$installerRoot = Join-Path $nativeRoot 'out\installer'
$stageRoot = Join-Path $installerRoot 'stage'
$stageDir = Join-Path $stageRoot 'DouyuMonitor'
$installerScript = Join-Path $nativeRoot 'installer\DouyuMonitor.iss'
$appIconPath = Join-Path $nativeRoot 'app\assets\douyu_monitor.ico'
$cmakeListPath = Join-Path $nativeRoot 'CMakeLists.txt'

foreach ($requiredFile in @(
    (Join-Path $releaseDir 'douyu_monitor_native.exe'),
    (Join-Path $releaseDir 'streamget_service\streamget_service.exe'),
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

$runtimeFiles = @(
    'douyu_monitor_native.exe',
    'mpv.dll',
    'Qt6Core.dll',
    'Qt6Gui.dll',
    'Qt6Network.dll',
    'Qt6OpenGL.dll',
    'Qt6Qml.dll',
    'Qt6QmlMeta.dll',
    'Qt6QmlModels.dll',
    'Qt6QmlWorkerScript.dll',
    'Qt6Quick.dll',
    'Qt6QuickControls2.dll',
    'Qt6QuickControls2Basic.dll',
    'Qt6QuickControls2BasicStyleImpl.dll',
    'Qt6QuickControls2Impl.dll',
    'Qt6QuickTemplates2.dll',
    'Qt6QuickEffects.dll',
    'Qt6QuickLayouts.dll',
    'Qt6QuickShapes.dll',
    'Qt6Svg.dll',
    'Qt6WebSockets.dll',
    'd3dcompiler_47.dll',
    'opengl32sw.dll'
)
foreach ($runtimeFile in $runtimeFiles) {
    $source = Join-Path $releaseDir $runtimeFile
    if (-not (Test-Path $source)) { throw "Required runtime file was not found: $source" }
    Copy-Item $source -Destination $stageDir
}

foreach ($directory in @('imageformats', 'iconengines', 'tls', 'networkinformation')) {
    $source = Join-Path $releaseDir $directory
    if (Test-Path $source) { Copy-Item $source -Destination $stageDir -Recurse }
}

$platformSourceDir = Join-Path $releaseDir 'platforms'
if (-not (Test-Path (Join-Path $platformSourceDir 'qwindows.dll'))) {
    throw "Required Qt platform plugin was not found: $platformSourceDir\qwindows.dll"
}
$platformStageDir = Join-Path $stageDir 'platforms'
New-Item -ItemType Directory -Force -Path $platformStageDir | Out-Null
Copy-Item (Join-Path $platformSourceDir 'qwindows.dll') -Destination $platformStageDir

$unusedQmlStyles = @(
    'QtQuick\Controls\Fusion',
    'QtQuick\Controls\Material',
    'QtQuick\Controls\Imagine',
    'QtQuick\Controls\Universal',
    'QtQuick\Controls\FluentWinUI3',
    'QtQuick\Controls\Windows',
    'QtQuick\NativeStyle',
    'QtQuick\Dialogs',
    'QtQuick\LocalStorage',
    'QtQuick\Particles',
    'QtQuick\VectorImage',
    'QtQuick\tooling'
)
$qmlSourceDir = Join-Path $releaseDir 'qml'
if (-not (Test-Path $qmlSourceDir)) { throw "Required Qt QML directory was not found: $qmlSourceDir" }
Copy-Item $qmlSourceDir -Destination $stageDir -Recurse
$qmlStageDir = Join-Path $stageDir 'qml'
foreach ($unusedQmlStyle in $unusedQmlStyles) {
    $unusedPath = Join-Path $qmlStageDir $unusedQmlStyle
    Remove-Item $unusedPath -Recurse -Force -ErrorAction SilentlyContinue
}

$serviceSourceDir = Join-Path $releaseDir 'streamget_service'
if (-not (Test-Path (Join-Path $serviceSourceDir 'streamget_service.exe'))) {
    throw "Required StreamGet service package was not found: $serviceSourceDir"
}
Copy-Item $serviceSourceDir -Destination $stageDir -Recurse
Copy-Item $appIconPath -Destination $stageDir

if (-not (Test-Path (Join-Path $stageDir 'platforms\qwindows.dll'))) {
    throw 'Release Qt platform plugin qwindows.dll is missing from the installer stage.'
}
if (-not (Test-Path (Join-Path $stageDir 'qml\QtQuick\Controls\Basic\Button.qml'))) {
    throw 'Required Qt Quick Controls Basic style is missing from the installer stage.'
}
if (Test-Path (Join-Path $stageDir 'qml\QtQuick\Controls\Fusion')) {
    throw 'Unused Qt Quick Controls Fusion style is present in the installer stage.'
}
if (Test-Path (Join-Path $stageDir 'platforms\qoffscreen.dll')) {
    throw 'Offscreen Qt platform plugin is present in the desktop installer stage.'
}
$forbiddenStageNames = @(
    'douyu_monitor_uninstaller.exe',
    'fake_streamget_service.exe',
    'qml_engine_smoke_test.exe',
    'qml_interaction_test.exe',
    'qml_visual_smoke_test.exe',
    'qml_close_regression_test.exe',
    'app_controller_test.exe'
)
foreach ($forbiddenName in $forbiddenStageNames) {
    if (Get-ChildItem $stageDir -Filter $forbiddenName -File -Recurse -ErrorAction SilentlyContinue) {
        throw "Forbidden file is present in the installer stage: $forbiddenName"
    }
}

$forbiddenStageExtensions = @('.pdb', '.lib', '.exp', '.map', '.log', '.ilk', '.obj')
foreach ($forbiddenExtension in $forbiddenStageExtensions) {
    if (Get-ChildItem $stageDir -Filter "*$forbiddenExtension" -File -Recurse -ErrorAction SilentlyContinue) {
        throw "Forbidden debug or build file is present in the installer stage: $forbiddenExtension"
    }
}
$payloadFiles = @(Get-ChildItem $stageDir -File -Recurse | ForEach-Object {
    [PSCustomObject]@{
        RelativePath = $_.FullName.Substring($stageDir.Length + 1)
        Name = $_.Name
        Hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
$duplicatePaths = @($payloadFiles | Group-Object RelativePath | Where-Object Count -gt 1)
$duplicateNames = @($payloadFiles | Where-Object { $_.Hash.Length -gt 0 } | Group-Object Name |
    Where-Object { $_.Count -gt 1 -and ($_.Group | Select-Object -ExpandProperty Hash -Unique).Count -eq 1 })
if ($duplicatePaths.Count -gt 0 -or $duplicateNames.Count -gt 0) {
    $duplicateLabel = if ($duplicatePaths.Count -gt 0) { $duplicatePaths[0].Name } else { $duplicateNames[0].Name }
    throw "Duplicate payload files are present in the installer stage: $duplicateLabel"
}

$dumpbinRoots = @(
    $env:VSINSTALLDIR,
    'C:\Program Files\Microsoft Visual Studio\2022',
    'D:\VSBuildTools'
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path $_) }
$dumpbin = Get-ChildItem $dumpbinRoots -Filter dumpbin.exe -Recurse -ErrorAction SilentlyContinue |
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
