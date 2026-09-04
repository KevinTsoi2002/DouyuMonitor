$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildScriptPath = Join-Path $PSScriptRoot 'build-windows-installer.ps1'
$innoScriptPath = Join-Path $nativeRoot 'installer\DouyuMonitor.iss'
$cmakeText = Get-Content -Raw (Join-Path $nativeRoot 'CMakeLists.txt')
$buildText = Get-Content -Raw $buildScriptPath
$innoText = if (Test-Path $innoScriptPath) { Get-Content -Raw $innoScriptPath } else { '' }

function Assert-Contains([string]$name, [string]$needle, [string]$text = $buildText) {
    if (-not $text.Contains($needle)) { throw "Installer regression check failed: missing $name." }
}

function Assert-NotContains([string]$name, [string]$needle, [string]$text = $buildText) {
    if ($text.Contains($needle)) { throw "Installer regression check failed: forbidden $name." }
}

function Assert-InnoContains([string]$name, [string]$needle) {
    if (-not $innoText.Contains($needle)) { throw "Installer regression check failed: missing Inno $name." }
}

function Assert-InnoNotContains([string]$name, [string]$needle) {
    if ($innoText.Contains($needle)) { throw "Installer regression check failed: forbidden Inno $name." }
}

function Assert-FileExists([string]$name, [string]$path) {
    if (-not (Test-Path $path)) { throw "Installer regression check failed: missing $name." }
}

Assert-FileExists 'app icon SVG' (Join-Path $nativeRoot 'app\assets\douyu_monitor.svg')
Assert-FileExists 'app icon ICO' (Join-Path $nativeRoot 'app\assets\douyu_monitor.ico')
Assert-FileExists 'Inno Setup installer script' $innoScriptPath
Assert-Contains 'Inno compiler lookup' 'INNO_SETUP_COMPILER'
Assert-Contains 'Inno compiler invocation' '$innoCompiler'
Assert-Contains 'CMake version parsing' 'projectVersion'
Assert-Contains 'Inno script invocation' 'DouyuMonitor.iss'
Assert-Contains 'versioned installer output' 'DouyuMonitor-Setup-V$projectVersion.exe'
Assert-Contains 'runtime dependency validation' 'verify_runtime_dependencies.cmake'
Assert-Contains 'platform plugin check' 'qwindows.dll'
Assert-NotContains 'IExpress packaging' 'iexpress.exe'
Assert-NotContains 'generated hidden install script' 'install.ps1'
Assert-NotContains 'ZIP payload packaging' 'DouyuMonitor-runtime.zip'
Assert-NotContains 'SED packaging' 'DouyuMonitor-setup.sed'
Assert-NotContains 'legacy native uninstaller payload' 'douyu_monitor_uninstaller.exe'

Assert-InnoContains 'runtime payload source' 'Source: "{#SourceDir}\*"'
Assert-InnoContains 'drive root detector' 'function IsDriveRoot'
Assert-InnoContains 'drive root normalizer' 'function NormalizeInstallDirectory'
Assert-InnoContains 'install validation' 'function PrepareToInstall'
Assert-InnoContains 'directory page validation' 'function NextButtonClick'
Assert-InnoContains 'built-in uninstaller shortcut' 'Filename: "{uninstallexe}"'
Assert-InnoContains 'default install directory' 'DefaultDirName={localappdata}\Programs\DouyuMonitor'
Assert-InnoNotContains 'unbundled Chinese language file' 'ChineseSimplified.isl'

if (-not $cmakeText.Contains('project(DouyuMonitorNative VERSION')) {
    throw 'Installer regression check failed: CMake project version is not discoverable.'
}

Write-Output 'installer script regression test passed'
