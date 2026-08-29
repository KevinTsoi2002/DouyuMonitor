$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$releaseDir = Join-Path $nativeRoot 'out\build\windows-x64-release'
$installerRoot = Join-Path $nativeRoot 'out\installer'
$stageRoot = Join-Path $installerRoot 'stage'
$stageDir = Join-Path $stageRoot 'DouyuMonitor'
$payloadZip = Join-Path $installerRoot 'DouyuMonitor-runtime.zip'
$sedPath = Join-Path $installerRoot 'DouyuMonitor-setup.sed'
$installerExe = Join-Path $installerRoot 'DouyuMonitor-Setup.exe'

if (-not (Test-Path (Join-Path $releaseDir 'douyu_monitor_native.exe'))) {
    throw "Release executable not found: $releaseDir\douyu_monitor_native.exe"
}
if (-not (Test-Path (Join-Path $releaseDir 'streamget_service.exe'))) {
    throw "Packaged StreamGet service not found: $releaseDir\streamget_service.exe"
}

Remove-Item $installerRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Get-ChildItem $releaseDir -File | Where-Object {
    $_.Name -in @('douyu_monitor_native.exe', 'streamget_service.exe') -or
    ($_.Extension -eq '.dll' -and $_.Name -notlike 'test*')
} | Copy-Item -Destination $stageDir

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

tar.exe -a -c -f $payloadZip -C $stageRoot DouyuMonitor
if (-not (Test-Path $payloadZip)) { throw 'Failed to create runtime payload archive.' }

$installScriptPath = Join-Path $installerRoot 'install.ps1'
$installLines = @(
    '$ErrorActionPreference = ''Stop''',
    '$programRoot = Join-Path $env:LOCALAPPDATA ''Programs''',
    '$target = Join-Path $programRoot ''DouyuMonitor''',
    'if (Test-Path $target) { Remove-Item $target -Recurse -Force }',
    'Expand-Archive -LiteralPath (Join-Path $PSScriptRoot ''DouyuMonitor-runtime.zip'') -DestinationPath $programRoot -Force',
    '$exe = Join-Path $target ''douyu_monitor_native.exe''',
    '$uninstaller = Join-Path $target ''uninstall.ps1''',
    '$uninstallLines = @(''Remove-Item (Join-Path ([Environment]::GetFolderPath(''''StartMenu'''')) ''''Programs\Douyu Monitor.lnk'''') -Force -ErrorAction SilentlyContinue'', ''Remove-Item (Split-Path -Parent $MyInvocation.MyCommand.Path) -Recurse -Force -ErrorAction SilentlyContinue'')',
    '$uninstallLines | Set-Content -LiteralPath $uninstaller -Encoding ASCII',
    '$shell = New-Object -ComObject WScript.Shell',
    '$shortcutPath = Join-Path ([Environment]::GetFolderPath(''StartMenu'')) ''Programs\Douyu Monitor.lnk''',
    '$shortcut = $shell.CreateShortcut($shortcutPath)',
    '$shortcut.TargetPath = $exe',
    '$shortcut.WorkingDirectory = $target',
    '$shortcut.Description = ''Douyu Monitor''',
    '$shortcut.Save()'
)
$installLines | Set-Content -LiteralPath $installScriptPath -Encoding ASCII

$sed = @(
    '[Version]',
    'Class=IEXPRESS',
    'SEDVersion=3',
    '[Options]',
    'PackagePurpose=InstallApp',
    'ShowInstallProgramWindow=1',
    'HideExtractAnimation=1',
    'UseLongFileName=1',
    'InsideCompressed=0',
    'CAB_FixedSize=0',
    'CAB_ResvCodeSigning=0',
    'RebootMode=I',
    'InstallPrompt="Install Douyu Monitor?"',
    'DisplayLicense=',
    'FinishMessage="Douyu Monitor was installed."',
    "TargetName=$installerExe",
    'FriendlyName=Douyu Monitor',
    'AppLaunched=powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1',
    'PostInstallCmd=<None>',
    'AdminQuietInstCmd=powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1',
    'UserQuietInstCmd=powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1',
    'SourceFiles=SourceFiles',
    '[SourceFiles]',
    "SourceFiles0=$installerRoot",
    '[SourceFiles0]',
    '%FILE0%=',
    '%FILE1%=',
    '[Strings]',
    'FILE0="DouyuMonitor-runtime.zip"',
    'FILE1="install.ps1"'
)
$sed | Set-Content -LiteralPath $sedPath -Encoding ASCII

iexpress.exe /N /Q $sedPath | Out-Null
if (-not (Test-Path $installerExe)) { throw 'IExpress failed to create DouyuMonitor-Setup.exe.' }

Write-Output $installerExe
