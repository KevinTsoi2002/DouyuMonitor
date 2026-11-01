$ErrorActionPreference = 'Stop'

$evidenceRoot = if ($env:STEP65_EVIDENCE_ROOT) {
    [System.IO.Path]::GetFullPath($env:STEP65_EVIDENCE_ROOT)
} else {
    'D:\DouyuAuditS7-b0e669b6-step65-stage-drift-injection'
}
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildInstaller = Join-Path $PSScriptRoot 'build-windows-installer.ps1'
$fakeBin = Join-Path $evidenceRoot 'fake-bin'
$fakeCmake = Join-Path $fakeBin 'cmake.cmd'
$fakeIscc = Join-Path $fakeBin 'iscc.cmd'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw "Stage drift integration test failed: $message" }
}

function New-SyntheticRelease([string]$releaseDir) {
    New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
    $runtimeFiles = @(
        'douyu_monitor_native.exe', 'mpv.dll', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Network.dll',
        'Qt6OpenGL.dll', 'Qt6Qml.dll', 'Qt6QmlMeta.dll', 'Qt6QmlModels.dll',
        'Qt6QmlWorkerScript.dll', 'Qt6Quick.dll', 'Qt6QuickControls2.dll',
        'Qt6QuickControls2Basic.dll', 'Qt6QuickControls2BasicStyleImpl.dll',
        'Qt6QuickControls2Impl.dll', 'Qt6QuickTemplates2.dll', 'Qt6QuickEffects.dll',
        'Qt6QuickLayouts.dll', 'Qt6QuickShapes.dll', 'Qt6Svg.dll', 'Qt6WebSockets.dll',
        'd3dcompiler_47.dll', 'opengl32sw.dll'
    )
    foreach ($name in $runtimeFiles) {
        Set-Content -LiteralPath (Join-Path $releaseDir $name) -Value "synthetic-$name" -NoNewline
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $releaseDir 'platforms'), (Join-Path $releaseDir 'qml\QtQuick\Controls\Basic'), (Join-Path $releaseDir 'streamget_service') | Out-Null
    Set-Content -LiteralPath (Join-Path $releaseDir 'platforms\qwindows.dll') -Value 'synthetic-qwindows' -NoNewline
    Set-Content -LiteralPath (Join-Path $releaseDir 'qml\QtQuick\Controls\Basic\Button.qml') -Value 'synthetic-button' -NoNewline
    Set-Content -LiteralPath (Join-Path $releaseDir 'streamget_service\streamget_service.exe') -Value 'synthetic-service' -NoNewline
    Set-Content -LiteralPath (Join-Path $releaseDir 'streamget_service\build-provenance.json') -Value '{}' -NoNewline
}

function Invoke-Scenario([string]$name, [string]$mode, [bool]$expectSuccess) {
    $scenarioRoot = Join-Path $evidenceRoot $name
    $releaseDir = Join-Path $scenarioRoot 'release'
    $stageRoot = Join-Path $scenarioRoot 'stage'
    $outputDir = Join-Path $scenarioRoot 'installer-output'
    $logPath = Join-Path $scenarioRoot 'publisher.log'
    New-Item -ItemType Directory -Force -Path $scenarioRoot | Out-Null
    New-SyntheticRelease $releaseDir
    $env:STAGE_DRIFT_MODE = $mode
    $env:STAGE_DRIFT_LOG = (Join-Path $scenarioRoot 'fake-iscc.log')
    $env:STAGE_DRIFT_STAGE = (Join-Path $stageRoot 'DouyuMonitor')
    $env:STAGE_DRIFT_OUTPUT = $outputDir
    $env:INNO_SETUP_COMPILER = $fakeIscc
    $oldPath = $env:PATH
    $env:PATH = "$fakeBin;$oldPath"
    try {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $buildInstaller `
            -ReleaseDir $releaseDir -Version '0.2.20' -StageRoot $stageRoot -InstallerOutputDir $outputDir *> $logPath
        $exitCode = [int]$LASTEXITCODE
        $ErrorActionPreference = $previous
    } finally {
        $env:PATH = $oldPath
        Remove-Item Env:INNO_SETUP_COMPILER, Env:STAGE_DRIFT_MODE, Env:STAGE_DRIFT_LOG, Env:STAGE_DRIFT_STAGE, Env:STAGE_DRIFT_OUTPUT -ErrorAction SilentlyContinue
    }

    $manifestDir = Join-Path $outputDir 'stage-manifests'
    $pre = Join-Path $manifestDir 'stage-before-inno.json'
    $post = Join-Path $manifestDir 'stage-after-inno.json'
    $diff = Join-Path $manifestDir 'stage-compare.json'
    Assert-True (Test-Path -LiteralPath $pre) "$name keeps the pre-Inno manifest"
    Assert-True (Test-Path -LiteralPath $post) "$name keeps the post-Inno manifest"
    Assert-True (Test-Path -LiteralPath $diff) "$name keeps the comparison evidence"
    Assert-True (Test-Path -LiteralPath (Join-Path $scenarioRoot 'fake-iscc.log')) "$name keeps fake compiler evidence"
    $comparison = Get-Content -LiteralPath $diff -Raw | ConvertFrom-Json
    $fakeLog = Get-Content -LiteralPath (Join-Path $scenarioRoot 'fake-iscc.log') -Raw
    Assert-True ($fakeLog.Contains('mode=' + $mode)) "$name records the compiler mode"
    Assert-True (($fakeLog.Contains('result=success') -or $fakeLog.Contains('result=failure')) -or $mode -eq 'fail') "$name records the compiler exit result"
    if ($expectSuccess) {
        Assert-True ($exitCode -eq 0) "$name returns success"
        Assert-True ($comparison.identical) "$name reports identical=true"
        Assert-True (@($comparison.added).Count -eq 0 -and @($comparison.removed).Count -eq 0 -and @($comparison.changed).Count -eq 0) "$name has no differences"
    } else {
        Assert-True ($exitCode -ne 0) "$name returns non-zero"
        if ($mode -eq 'modify') { Assert-True (@($comparison.changed) -contains 'douyu_monitor_native.exe') "$name reports changed application file" }
        if ($mode -eq 'add') { Assert-True (@($comparison.added) -contains 'added-during-compile.txt') "$name reports the added file" }
        if ($mode -eq 'remove') { Assert-True (@($comparison.removed) -contains 'douyu_monitor_native.exe') "$name reports the removed file" }
        if ($mode -eq 'fail') { Assert-True ($comparison.identical) "$name does not misclassify compiler failure as drift" }
    }
    [pscustomobject]@{ name = $name; mode = $mode; exitCode = $exitCode; identical = [bool]$comparison.identical; added = @($comparison.added); removed = @($comparison.removed); changed = @($comparison.changed); pre = $pre; post = $post; diff = $diff; fakeLog = (Join-Path $scenarioRoot 'fake-iscc.log'); publisherLog = $logPath }
}

if (Test-Path -LiteralPath $evidenceRoot) {
    $existing = @(Get-ChildItem -LiteralPath $evidenceRoot -Force)
    if ($existing.Count -gt 0) { throw "Evidence root already contains files: $evidenceRoot" }
} else {
    New-Item -ItemType Directory -Path $evidenceRoot | Out-Null
}
New-Item -ItemType Directory -Force -Path $fakeBin | Out-Null

@'
@echo off
setlocal
set "RUNTIME="
for %%A in (%*) do call :parse "%%~A"
if not "%RUNTIME%"=="" (
  if not exist "%RUNTIME%\app-test.txt" echo baseline>"%RUNTIME%\app-test.txt"
  if not exist "%RUNTIME%\qml\test.qml" (mkdir "%RUNTIME%\qml" 2>nul & echo baseline-qml>"%RUNTIME%\qml\test.qml")
  if not exist "%RUNTIME%\plugins\platform-test.txt" (mkdir "%RUNTIME%\plugins" 2>nul & echo baseline-plugin>"%RUNTIME%\plugins\platform-test.txt")
)
exit /b 0
:parse
set "ARG=%~1"
if /I "%ARG:~0,18%"=="-DRUNTIME_DIR:PATH=" set "RUNTIME=%ARG:~18%"
exit /b 0
'@ | Set-Content -LiteralPath $fakeCmake -Encoding ascii

@'
@echo off
setlocal
set "SRC=%STAGE_DRIFT_STAGE%"
set "OUT=%STAGE_DRIFT_OUTPUT%"
if "%OUT%"=="" exit /b 20
if not exist "%OUT%" mkdir "%OUT%"
set "LOG=%STAGE_DRIFT_LOG%"
echo mode=%STAGE_DRIFT_MODE%>"%LOG%"
echo source=%SRC%>>"%LOG%"
echo output=%OUT%>>"%LOG%"
if /I "%STAGE_DRIFT_MODE%"=="modify" (echo modified-by-fake-iscc>>"%SRC%\douyu_monitor_native.exe")
if /I "%STAGE_DRIFT_MODE%"=="add" echo added-by-fake-iscc>"%SRC%\added-during-compile.txt"
if /I "%STAGE_DRIFT_MODE%"=="remove" del /q "%SRC%\douyu_monitor_native.exe"
echo fake-installer>"%OUT%\DouyuMonitor-Setup-V0.2.20.exe"
if /I "%STAGE_DRIFT_MODE%"=="fail" (echo result=failure>>"%LOG%" & exit /b 7)
echo result=success>>"%LOG%"
echo args=%*>>"%LOG%"
exit /b 0
'@ | Set-Content -LiteralPath $fakeIscc -Encoding ascii

$results = @(
    (Invoke-Scenario 'scenario-a-modify' 'modify' $false)
    (Invoke-Scenario 'scenario-b-add' 'add' $false)
    (Invoke-Scenario 'scenario-c-remove' 'remove' $false)
    (Invoke-Scenario 'scenario-d-identical' 'none' $true)
    (Invoke-Scenario 'scenario-e-compiler-failure' 'fail' $false)
)
$results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'scenario-results.json') -Encoding utf8
$results | Format-Table name,mode,exitCode,identical -AutoSize
Write-Output 'stage drift injection integration tests passed'
