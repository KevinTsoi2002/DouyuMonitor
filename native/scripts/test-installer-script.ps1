$ErrorActionPreference = 'Stop'

$sourceText = Get-Content -Raw (Join-Path $PSScriptRoot 'build-windows-installer.ps1')

function Assert-Contains([string]$name, [string]$needle) {
    if (-not $sourceText.Contains($needle)) {
        throw "Installer regression check failed: missing $name."
    }
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
Assert-Contains 'synchronous IExpress build' 'Start-Process -FilePath ''iexpress.exe'' -ArgumentList @(''/N'', $sedPath) -Wait'
Assert-Contains 'IExpress output wait' 'while (-not (Test-Path $installerExe)'

Write-Output 'installer script regression test passed'
