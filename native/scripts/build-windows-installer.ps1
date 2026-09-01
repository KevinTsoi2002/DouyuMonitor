$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$releaseDir = Join-Path $nativeRoot 'out\build\windows-x64-release'
$installerRoot = Join-Path $nativeRoot 'out\installer'
$stageRoot = Join-Path $installerRoot 'stage'
$stageDir = Join-Path $stageRoot 'DouyuMonitor'
$payloadZip = Join-Path $installerRoot 'DouyuMonitor-runtime.zip'
$sedPath = Join-Path $installerRoot 'DouyuMonitor-setup.sed'
$installerExe = Join-Path $installerRoot 'DouyuMonitor-Setup.exe'
$appIconPath = Join-Path $nativeRoot 'app\assets\douyu_monitor.ico'

if (-not (Test-Path (Join-Path $releaseDir 'douyu_monitor_native.exe'))) {
    throw "Release executable not found: $releaseDir\douyu_monitor_native.exe"
}
if (-not (Test-Path (Join-Path $releaseDir 'streamget_service.exe'))) {
    throw "Packaged StreamGet service not found: $releaseDir\streamget_service.exe"
}
if (-not (Test-Path (Join-Path $releaseDir 'douyu_monitor_uninstaller.exe'))) {
    throw "Release uninstaller not found: $releaseDir\douyu_monitor_uninstaller.exe"
}
if (-not (Test-Path $appIconPath)) { throw "Application icon not found: $appIconPath" }

Remove-Item $installerRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Get-ChildItem $releaseDir -File | Where-Object {
    $_.Name -in @('douyu_monitor_native.exe', 'streamget_service.exe') -or
    ($_.Extension -eq '.dll' -and $_.Name -notlike 'test*')
} | Copy-Item -Destination $stageDir
Copy-Item (Join-Path $releaseDir 'douyu_monitor_uninstaller.exe') `
    -Destination (Join-Path $stageDir 'Uninstall DouyuMonitor.exe')
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

tar.exe -a -c -f $payloadZip -C $stageRoot DouyuMonitor
if (-not (Test-Path $payloadZip)) { throw 'Failed to create runtime payload archive.' }

$installScriptPath = Join-Path $installerRoot 'install.ps1'
$installLines = @(
    '$ErrorActionPreference = ''Stop''',
    'Add-Type -AssemblyName System.Windows.Forms',
    '$programRoot = Join-Path $env:LOCALAPPDATA ''Programs''',
    '$selectedRoot = $env:DOUYU_INSTALL_ROOT',
    'if ([string]::IsNullOrWhiteSpace($selectedRoot)) { $dialog = New-Object System.Windows.Forms.FolderBrowserDialog; $dialog.Description = ''Choose the DouyuMonitor installation folder''; $dialog.ShowNewFolderButton = $true; $dialog.SelectedPath = $programRoot; if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit 1 }; $selectedRoot = $dialog.SelectedPath }',
    'if ([IO.Path]::GetFileName($selectedRoot).Equals(''DouyuMonitor'', [StringComparison]::OrdinalIgnoreCase)) { $target = $selectedRoot; $extractRoot = Split-Path -Parent $target } else { $target = Join-Path $selectedRoot ''DouyuMonitor''; $extractRoot = $selectedRoot }',
    'New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null',
    'if (Test-Path $target) { Remove-Item $target -Recurse -Force }',
    'Expand-Archive -LiteralPath (Join-Path $PSScriptRoot ''DouyuMonitor-runtime.zip'') -DestinationPath $extractRoot -Force',
    '$exe = Join-Path $target ''douyu_monitor_native.exe''',
    'if (-not (Test-Path $exe)) { throw ''Installed executable was not found.'' }',
    '$uninstaller = Join-Path $target ''Uninstall DouyuMonitor.exe''',
    'if (-not (Test-Path $uninstaller)) { throw ''Installed uninstaller was not found.'' }',
    '$iconPath = Join-Path $target ''douyu_monitor.ico''',
    'if (-not (Test-Path $iconPath)) { throw ''Installed application icon was not found.'' }',
    '$startMenuDir = Join-Path ([Environment]::GetFolderPath(''StartMenu'')) ''Programs\DouyuMonitor''',
    '$desktopShortcutPath = Join-Path ([Environment]::GetFolderPath(''Desktop'')) ''DouyuMonitor.lnk''',
    '$startMenuShortcutPath = Join-Path $startMenuDir ''DouyuMonitor.lnk''',
    '$uninstallShortcutPath = Join-Path $startMenuDir ''Uninstall DouyuMonitor.lnk''',
    '$uninstallRegistryPath = ''HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\DouyuMonitor''',
    'New-Item -Path $uninstallRegistryPath -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''DisplayName'' -Value ''DouyuMonitor'' -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''DisplayVersion'' -Value ''0.2.1'' -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''Publisher'' -Value ''DouyuMonitor'' -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''InstallLocation'' -Value $target -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''DisplayIcon'' -Value $iconPath -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''UninstallString'' -Value (''"{0}"'' -f $uninstaller) -PropertyType String -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''NoModify'' -Value 1 -PropertyType DWord -Force | Out-Null; New-ItemProperty -Path $uninstallRegistryPath -Name ''NoRepair'' -Value 1 -PropertyType DWord -Force | Out-Null',
    'if ($env:DOUYU_SKIP_SHORTCUTS -ne ''1'') { New-Item -ItemType Directory -Force -Path $startMenuDir | Out-Null; $shell = New-Object -ComObject WScript.Shell; $shortcut = $shell.CreateShortcut($startMenuShortcutPath); $shortcut.TargetPath = $exe; $shortcut.WorkingDirectory = $target; $shortcut.IconLocation = $iconPath; $shortcut.Description = ''DouyuMonitor''; $shortcut.Save(); $desktopShortcut = $shell.CreateShortcut($desktopShortcutPath); $desktopShortcut.TargetPath = $exe; $desktopShortcut.WorkingDirectory = $target; $desktopShortcut.IconLocation = $iconPath; $desktopShortcut.Description = ''DouyuMonitor''; $desktopShortcut.Save(); $uninstallShortcut = $shell.CreateShortcut($uninstallShortcutPath); $uninstallShortcut.TargetPath = $uninstaller; $uninstallShortcut.WorkingDirectory = $target; $uninstallShortcut.IconLocation = $iconPath; $uninstallShortcut.Description = ''Uninstall DouyuMonitor''; $uninstallShortcut.Save() }',
    'if ($env:DOUYU_SKIP_LAUNCH -ne ''1'') { [System.Windows.Forms.MessageBox]::Show("Installed to:`n$target", "DouyuMonitor") | Out-Null; Start-Process -FilePath $exe -WorkingDirectory $target }'
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
    'AppLaunched=powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File install.ps1',
    'PostInstallCmd=<None>',
    'AdminQuietInstCmd=powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File install.ps1',
    'UserQuietInstCmd=powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File install.ps1',
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

$iexpress = Start-Process -FilePath 'iexpress.exe' -ArgumentList @('/N', $sedPath) -Wait -PassThru
if ($iexpress.ExitCode -ne 0) { throw "IExpress failed with exit code $($iexpress.ExitCode)." }
$deadline = (Get-Date).AddMinutes(10)
while (-not (Test-Path $installerExe) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
if (-not (Test-Path $installerExe)) { throw 'IExpress failed to create DouyuMonitor-Setup.exe.' }

Write-Output $installerExe

