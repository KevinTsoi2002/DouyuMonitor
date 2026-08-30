$ErrorActionPreference = 'Stop'

$sourceText = Get-Content -Raw (Join-Path $PSScriptRoot 'build-windows-installer.ps1')
$cmakeText = Get-Content -Raw (Join-Path $PSScriptRoot '..\CMakeLists.txt')
$notificationSourceText = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\app\windows_notification_service.cpp')
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Assert-Contains([string]$name, [string]$needle) {
    if (-not $sourceText.Contains($needle)) {
        throw "Installer regression check failed: missing $name."
    }
}

function Assert-FileExists([string]$name, [string]$path) {
    if (-not (Test-Path $path)) {
        throw "Installer regression check failed: missing $name."
    }
}

Assert-FileExists 'app icon SVG' (Join-Path $nativeRoot 'app\assets\douyu_monitor.svg')
Assert-FileExists 'app icon ICO' (Join-Path $nativeRoot 'app\assets\douyu_monitor.ico')
if (-not $cmakeText.Contains('app/resources/app_icon.rc')) {
    throw 'Installer regression check failed: missing app icon resource embedding.'
}
if (-not $cmakeText.Contains('add_executable(douyu_monitor_uninstaller')) {
    throw 'Installer regression check failed: missing native uninstaller target.'
}
if (-not $notificationSourceText.Contains('MAKEINTRESOURCEW(101)')) {
    throw 'Installer regression check failed: notification tray does not load the app icon resource.'
}

Assert-Contains 'folder picker' 'FolderBrowserDialog'
Assert-Contains 'automated install root override' 'DOUYU_INSTALL_ROOT'
Assert-Contains 'selected install folder' '$selectedRoot'
Assert-Contains 'start menu shortcut' 'Programs\DouyuMonitor'
Assert-Contains 'desktop shortcut' "GetFolderPath(''Desktop'')"
Assert-Contains 'post-install launch' 'Start-Process -FilePath $exe'
Assert-Contains 'install path message' 'Installed to:`n$target'
Assert-Contains 'uninstaller start menu path' '$startMenuShortcutPath'
Assert-Contains 'uninstaller desktop path' '$desktopShortcutPath'
Assert-Contains 'native uninstaller payload' 'douyu_monitor_uninstaller.exe'
Assert-Contains 'uninstall start menu shortcut' 'Uninstall DouyuMonitor.lnk'
Assert-Contains 'Windows uninstall registration' 'CurrentVersion\Uninstall\DouyuMonitor'
Assert-Contains 'uninstall command registration' 'UninstallString'
Assert-Contains 'synchronous IExpress build' 'Start-Process -FilePath ''iexpress.exe'' -ArgumentList @(''/N'', $sedPath) -Wait'
Assert-Contains 'IExpress output wait' 'while (-not (Test-Path $installerExe)'

Write-Output 'installer script regression test passed'
