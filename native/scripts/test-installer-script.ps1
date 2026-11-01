$ErrorActionPreference = 'Stop'

$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildScriptPath = Join-Path $PSScriptRoot 'build-windows-installer.ps1'
$buildResultVerifierPath = Join-Path $PSScriptRoot 'verify-installer-build-result.ps1'
$candidateVerifierPath = Join-Path $PSScriptRoot 'verify-installer-candidate.ps1'
$innoScriptPath = Join-Path $nativeRoot 'installer\DouyuMonitor.iss'
$cmakeText = Get-Content -Raw (Join-Path $nativeRoot 'CMakeLists.txt')
$buildText = Get-Content -Raw $buildScriptPath
$buildResultVerifierText = if (Test-Path $buildResultVerifierPath) { Get-Content -Raw $buildResultVerifierPath } else { '' }
$candidateVerifierText = if (Test-Path $candidateVerifierPath) { Get-Content -Raw $candidateVerifierPath } else { '' }
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
Assert-FileExists 'build result verifier' $buildResultVerifierPath
Assert-FileExists 'candidate postbuild verifier' $candidateVerifierPath
Assert-Contains 'Inno compiler lookup' 'INNO_SETUP_COMPILER'
Assert-Contains 'Inno compiler invocation' '$innoCompiler'
Assert-Contains 'CMake version parsing' 'projectVersion'
Assert-Contains 'explicit installer version override' '[string]$Version'
Assert-Contains 'installer version validation' '^[0-9]+\.[0-9]+\.[0-9]+'
Assert-Contains 'Inno script invocation' 'DouyuMonitor.iss'
Assert-Contains 'versioned installer output' 'DouyuMonitor-Setup-V$projectVersion.exe'
Assert-Contains 'runtime dependency validation' 'verify_runtime_dependencies.cmake'
Assert-Contains 'platform plugin check' 'qwindows.dll'
Assert-Contains 'runtime file allowlist' '$runtimeFiles = @('
Assert-Contains 'unused QML style pruning' '$unusedQmlStyles = @('
Assert-Contains 'Basic style stage check' 'QtQuick\Controls\Basic\Button.qml'
Assert-Contains 'Fusion style exclusion check' 'QtQuick\Controls\Fusion'
Assert-Contains 'forbidden stage file check' '$forbiddenStageNames = @('
Assert-Contains 'desktop platform plugin allowlist' '$platformSourceDir'
Assert-Contains 'offscreen platform plugin exclusion' 'platforms\qoffscreen.dll'
Assert-Contains 'forbidden debug extension check' '$forbiddenStageExtensions = @('
Assert-Contains 'duplicate payload check' '$payloadFiles = @('
Assert-Contains 'stage integrity helper' 'stage-integrity.ps1'
Assert-Contains 'pre-Inno stage manifest' 'stage-before-inno.json'
Assert-Contains 'post-Inno stage manifest' 'stage-after-inno.json'
Assert-Contains 'stage manifest comparison' 'stage-compare.json'
Assert-Contains 'stage integrity failure' 'Stage integrity gate failed'
Assert-Contains 'fresh installer output claim' 'New-Item -ItemType Directory -Path $installerRoot'
Assert-NotContains 'existing installer output cleanup' 'Remove-Item $installerRoot'
Assert-Contains 'candidate result file' 'build-result.json'
Assert-Contains 'validated candidate state' '$buildResult.state = ''validated'''
Assert-Contains 'build result verifier requires validated state' "Build result state is not validated." $buildResultVerifierText
Assert-Contains 'build result verifier checks output SHA-256' 'Installer SHA-256 does not match the file.' $buildResultVerifierText
Assert-Contains 'build result verifier rechecks Stage manifests' 'Referenced Stage manifests do not compare as identical' $buildResultVerifierText
Assert-Contains 'baseline-only verifier mode' 'SkipInstallerIdentity' $buildResultVerifierText
Assert-Contains 'candidate verifier uses saved baseline' 'baselineManifestPath' $candidateVerifierText
Assert-Contains 'candidate verifier writes independent result' 'candidate-verification.json' $candidateVerifierText
Assert-Contains 'candidate verifier checks current Stage' 'Current Stage differs from the saved build-time manifest.' $candidateVerifierText
Assert-Contains 'ordinary installer file check' 'Test-Path -LiteralPath $installerExe -PathType Leaf'
Assert-Contains 'nonempty installer check' '$file.Length -eq 0'
Assert-NotContains 'IExpress packaging' 'iexpress.exe'
Assert-NotContains 'generated hidden install script' 'install.ps1'
Assert-NotContains 'ZIP payload packaging' 'DouyuMonitor-runtime.zip'
Assert-NotContains 'SED packaging' 'DouyuMonitor-setup.sed'

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
