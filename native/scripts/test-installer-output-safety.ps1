param(
    [string]$EvidenceRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $env:TEMP ('installer-output-safety-' + [Guid]::NewGuid().ToString('N'))
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw "Evidence root already exists: $EvidenceRoot" }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$publisher = Join-Path $PSScriptRoot 'build-windows-installer.ps1'
$fakeBin = Join-Path $EvidenceRoot 'fake-bin'
New-Item -ItemType Directory -Path $fakeBin | Out-Null
$script:failures = [Collections.Generic.List[string]]::new()
$script:checks = 0
$script:results = [Collections.Generic.List[object]]::new()

function Check([bool]$condition, [string]$message) {
    $script:checks++
    if (-not $condition) {
        $script:failures.Add($message)
        Write-Output "FAIL: $message"
    }
}

function File-Identity([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    $file = Get-Item -LiteralPath $path
    return [pscustomobject]@{
        path = $file.FullName
        bytes = $file.Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
}

function New-Release([string]$path) {
    New-Item -ItemType Directory -Path $path | Out-Null
    $runtime = @(
        'douyu_monitor_native.exe', 'mpv.dll', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Network.dll',
        'Qt6OpenGL.dll', 'Qt6Qml.dll', 'Qt6QmlMeta.dll', 'Qt6QmlModels.dll', 'Qt6QmlWorkerScript.dll',
        'Qt6Quick.dll', 'Qt6QuickControls2.dll', 'Qt6QuickControls2Basic.dll',
        'Qt6QuickControls2BasicStyleImpl.dll', 'Qt6QuickControls2Impl.dll', 'Qt6QuickTemplates2.dll',
        'Qt6QuickEffects.dll', 'Qt6QuickLayouts.dll', 'Qt6QuickShapes.dll', 'Qt6Svg.dll',
        'Qt6WebSockets.dll', 'd3dcompiler_47.dll', 'opengl32sw.dll'
    )
    foreach ($name in $runtime) {
        Set-Content -LiteralPath (Join-Path $path $name) -Value "synthetic-$name" -NoNewline
    }
    foreach ($dir in @('platforms', 'qml\QtQuick\Controls\Basic', 'streamget_service')) {
        New-Item -ItemType Directory -Path (Join-Path $path $dir) -Force | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $path 'platforms\qwindows.dll') -Value 'synthetic-platform' -NoNewline
    Set-Content -LiteralPath (Join-Path $path 'qml\QtQuick\Controls\Basic\Button.qml') -Value 'synthetic-qml' -NoNewline
    Set-Content -LiteralPath (Join-Path $path 'streamget_service\streamget_service.exe') -Value 'synthetic-service' -NoNewline
    Set-Content -LiteralPath (Join-Path $path 'streamget_service\build-provenance.json') -Value '{}' -NoNewline
}

function Run-Publisher([string]$name, [string]$mode, [bool]$existingOutput = $false, [bool]$useDefault = $false) {
    $root = Join-Path $EvidenceRoot $name
    New-Item -ItemType Directory -Path $root | Out-Null
    $release = Join-Path $root 'release'
    New-Release $release
    $scriptPath = $publisher
    $stage = Join-Path $root 'stage'
    $output = Join-Path $root 'output'
    if ($useDefault) {
        # Run the identical publisher in a tiny native layout, never the real default output.
        $sandbox = Join-Path $root 'native'
        foreach ($dir in @('scripts', 'installer', 'app\assets', 'cmake')) {
            New-Item -ItemType Directory -Path (Join-Path $sandbox $dir) -Force | Out-Null
        }
        foreach ($scriptName in @('build-windows-installer.ps1', 'stage-integrity.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $scriptName) -Destination (Join-Path $sandbox 'scripts')
        }
        Copy-Item -LiteralPath (Join-Path $nativeRoot 'CMakeLists.txt') -Destination $sandbox
        Copy-Item -LiteralPath (Join-Path $nativeRoot 'installer\DouyuMonitor.iss') -Destination (Join-Path $sandbox 'installer')
        Copy-Item -LiteralPath (Join-Path $nativeRoot 'app\assets\douyu_monitor.ico') -Destination (Join-Path $sandbox 'app\assets')
        $scriptPath = Join-Path $sandbox 'scripts\build-windows-installer.ps1'
        Check ((Get-FileHash -LiteralPath $scriptPath).Hash -eq (Get-FileHash -LiteralPath $publisher).Hash) "$name uses the unchanged publisher snapshot"
        $output = Join-Path $sandbox 'out\installer'
        $stage = Join-Path $output 'stage'
    }
    $installer = Join-Path $output 'DouyuMonitor-Setup-V0.2.20.exe'
    $sentinel = $null
    $priorEvidence = $null
    if ($existingOutput) {
        New-Item -ItemType Directory -Path $output -Force | Out-Null
        Set-Content -LiteralPath $installer -Value 'pre-existing-candidate-sentinel' -NoNewline
        Set-Content -LiteralPath (Join-Path $output 'prior-diagnostic.json') -Value '{"prior":true}' -NoNewline
        $sentinel = File-Identity $installer
        $priorEvidence = File-Identity (Join-Path $output 'prior-diagnostic.json')
    }
    $log = Join-Path $root 'publisher.log'
    $env:STEP66_CASE_ROOT = $root
    $env:STEP66_MODE = $mode
    $env:STEP66_COMPILER_LOG = Join-Path $root 'compiler.json'
    $env:STEP67_LOCK_MODE = if ($mode -eq 'hash-locked') { 'hash' } elseif ($mode -eq 'result-locked') { 'result' } else { '' }
    $env:STEP67_LOCK_READY = Join-Path $root 'lock-ready.json'
    $env:STEP67_LOCK_RELEASE = Join-Path $root 'lock-release.signal'
    $env:STEP67_LOCK_TARGET = if ($mode -eq 'hash-locked') { $installer } else { Join-Path $output 'build-result.json' }
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $scriptPath, '-ReleaseDir', $release, '-Version', '0.2.20')
    if (-not $useDefault) { $arguments += @('-StageRoot', $stage, '-InstallerOutputDir', $output) }
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe @arguments *> $log
        $code = [int]$LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    $lockInfo = if (Test-Path -LiteralPath $env:STEP67_LOCK_READY -PathType Leaf) {
        Get-Content -LiteralPath $env:STEP67_LOCK_READY -Raw | ConvertFrom-Json
    } else { $null }
    if ($null -ne $lockInfo -and $lockInfo.processId) {
        Set-Content -LiteralPath $env:STEP67_LOCK_RELEASE -Value 'release' -NoNewline
        $waitUntil = [DateTime]::UtcNow.AddSeconds(10)
        while ([DateTime]::UtcNow -lt $waitUntil -and (Get-Process -Id ([int]$lockInfo.processId) -ErrorAction SilentlyContinue)) {
            Start-Sleep -Milliseconds 50
        }
        if (Get-Process -Id ([int]$lockInfo.processId) -ErrorAction SilentlyContinue) {
            Stop-Process -Id ([int]$lockInfo.processId) -Force -ErrorAction SilentlyContinue
            $script:failures.Add("$name test-owned lock process did not exit after release.")
        }
    }
    $comparisonPath = Join-Path $output 'stage-manifests\stage-compare.json'
    $comparison = if (Test-Path -LiteralPath $comparisonPath) { Get-Content -LiteralPath $comparisonPath -Raw | ConvertFrom-Json } else { $null }
    $resultPath = Join-Path $output 'build-result.json'
    $result = if (Test-Path -LiteralPath $resultPath) { Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json } else { $null }
    $identity = File-Identity $installer
    $afterPrior = File-Identity (Join-Path $output 'prior-diagnostic.json')
    $record = [pscustomobject]@{
        name = $name; mode = $mode; exitCode = $code; output = $output; stage = $stage
        installer = $identity; beforeSentinel = $sentinel; afterSentinel = $identity
        beforeDiagnostic = $priorEvidence; afterDiagnostic = $afterPrior
        result = $result; comparison = $comparison; log = $log; command = @('powershell.exe') + $arguments
    }
    $script:results.Add($record)
    if ($existingOutput) {
        Check ($code -ne 0) "$name rejects an existing output"
        Check ($null -ne $identity -and $identity.bytes -eq $sentinel.bytes -and $identity.sha256 -eq $sentinel.sha256) "$name preserves sentinel size and SHA256"
        Check ($null -ne $afterPrior -and $afterPrior.sha256 -eq $priorEvidence.sha256) "$name preserves prior diagnostics"
        Check (-not (Test-Path -LiteralPath $env:STEP66_COMPILER_LOG)) "$name refuses before calling the compiler"
        Check (-not (Test-Path -LiteralPath $stage)) "$name refuses before creating Stage"
        return
    }
    Check ($null -ne $comparison) "$name keeps comparison diagnostics"
    foreach ($manifest in @('stage-before-inno.json', 'stage-after-inno.json')) {
        Check (Test-Path -LiteralPath (Join-Path $output "stage-manifests\$manifest") -PathType Leaf) "$name keeps $manifest"
    }
    $expectSuccess = $mode -eq 'success'
    Check (($code -eq 0) -eq $expectSuccess) "$name has the expected publisher exit status"
    Check ($null -ne $result) "$name has an explicit candidate result"
    if ($null -ne $result) {
        Check ($result.formatVersion -eq 1) "$name result has a format version"
        $expectedState = if ($expectSuccess) {
            'validated'
        } elseif ($mode -eq 'result-locked') {
            'generated-unverified'
        } else {
            'failed'
        }
        Check ($result.state -eq $expectedState) "$name classifies candidate as $expectedState"
        Check ($result.installerPath -eq $installer) "$name identifies the expected output path"
        $compilerCode = if ($mode -in @('partial', 'both-fail')) { 7 } else { 0 }
        Check ($result.compilerExitCode -eq $compilerCode) "$name records the compiler exit code"
        $gateCode = if ($mode -in @('drift', 'both-fail')) { 1 } else { 0 }
        if ($mode -eq 'result-locked') {
            Check ($null -eq $result.compareExitCode) "$name leaves final comparison status uncommitted after result persistence fails"
        } else {
            Check ($result.compareExitCode -eq $gateCode) "$name records the independent Stage gate exit code"
        }
        Check (-not [string]::IsNullOrWhiteSpace($result.runId)) "$name identifies this run"
        if ($null -ne $identity -and $mode -notin @('hash-locked', 'result-locked')) {
            Check ($result.installer.bytes -eq $identity.bytes -and $result.installer.sha256 -eq $identity.sha256) "$name binds the result to the output size and hash"
        }
    }
    if ($mode -in @('drift', 'both-fail')) {
        Check ($null -ne $comparison -and -not $comparison.identical -and @($comparison.changed) -contains 'douyu_monitor_native.exe') "$name records the changed Stage file"
        Check ($null -ne $identity) "$name retains the failed output"
    } else {
        Check ($null -ne $comparison -and $comparison.identical) "$name does not misclassify compiler/output errors as Stage drift"
    }
    $logText = Get-Content -LiteralPath $log -Raw
    if ($mode -in @('partial', 'both-fail')) {
        Check ($null -ne $identity) "$name keeps partial output"
        Check ($logText -match 'exit code 7') "$name reports compiler exit code 7"
    }
    if ($mode -eq 'hash-locked') {
        Check ($null -ne $lockInfo -and $lockInfo.state -eq 'locked') "$name acquired the output hash test lock"
        Check ($null -ne $identity -and $result.state -eq 'failed' -and $null -eq $result.installer.sha256) "$name marks an unreadable output hash as failed"
        Check ($result.compilerExitCode -eq 0 -and $result.compareExitCode -eq 0) "$name distinguishes output hash failure from compiler and Stage failures"
    }
    if ($mode -eq 'result-locked') {
        Check ($null -ne $lockInfo -and $lockInfo.state -eq 'locked') "$name acquired the final result write test lock"
        Check ($code -ne 0 -and $result.state -ne 'validated') "$name does not return success when final result persistence fails"
        Check ($logText -match 'Could not save failed candidate status') "$name reports that failure status persistence also failed"
        Check (Test-Path -LiteralPath $comparisonPath -PathType Leaf) "$name reached the completed Stage comparison before final status persistence failed"
    }
    if ($mode -in @('missing', 'empty', 'directory')) {
        Check (($logText -replace '\s', '').Contains(($installer -replace '\s', ''))) "$name reports the expected output path"
    }
    if ($mode -eq 'success') {
        Check ($null -ne $identity -and $identity.bytes -gt 0) "$name has a nonempty placeholder"
        $sourceIcon = Join-Path $nativeRoot 'app\assets\douyu_monitor.ico'
        $iconHash = (Get-FileHash -LiteralPath $sourceIcon -Algorithm SHA256).Hash.ToLowerInvariant()
        $iconName = 'cstg-app-{0}.ico' -f $iconHash.Substring(0, 16)
        $stagedIcon = File-Identity (Join-Path (Join-Path $stage 'DouyuMonitor') $iconName)
        Check ($null -ne $stagedIcon -and $stagedIcon.sha256.ToLowerInvariant() -eq $iconHash) "$name stages a brand icon with a content-specific cache identity"
        $compiler = Get-Content -LiteralPath $env:STEP66_COMPILER_LOG -Raw | ConvertFrom-Json
        Check (@($compiler.arguments) -contains "/DAppIconName=$iconName") "$name supplies the exact staged icon name to Inno"
        Check (-not (Test-Path -LiteralPath (Join-Path (Join-Path $stage 'DouyuMonitor') 'douyu_monitor.ico'))) "$name avoids the legacy shortcut icon cache path"
    }
}

@'
@echo off
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'cmake.cmd') -Encoding ascii
@'
@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fake-iscc.ps1" %*
exit /b %ERRORLEVEL%
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'iscc.cmd') -Encoding ascii
@'
$ErrorActionPreference = 'Stop'
$values = @{}
foreach ($arg in $args) {
    if ($arg -match '^/D(SourceDir|OutputDir|AppVersion)=(.*)$') { $values[$Matches[1]] = $Matches[2] }
}
$root = [IO.Path]::GetFullPath($env:STEP66_CASE_ROOT).TrimEnd('\') + '\'
foreach ($key in @('SourceDir', 'OutputDir')) {
    if (-not $values.ContainsKey($key)) { throw "Missing compiler argument $key" }
    $path = [IO.Path]::GetFullPath($values[$key])
    if (-not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe synthetic path: $path" }
}
$source = $values.SourceDir
$output = $values.OutputDir
$mode = $env:STEP66_MODE
$exe = Join-Path $output ("DouyuMonitor-Setup-V{0}.exe" -f $values.AppVersion)
New-Item -ItemType Directory -Path $output -Force | Out-Null
if ($mode -eq 'directory') {
    New-Item -ItemType Directory -Path $exe | Out-Null
} elseif ($mode -ne 'missing') {
    $content = if ($mode -eq 'empty') { '' } else { "SIMULATED-NOT-AN-INSTALLER:$mode" }
    [IO.File]::WriteAllText($exe, $content)
}
if ($mode -in @('drift', 'both-fail')) {
    Add-Content -LiteralPath (Join-Path $source 'douyu_monitor_native.exe') -Value 'modified-during-compile'
}
$lockStatus = $null
$holder = $null
if ($env:STEP67_LOCK_MODE) {
    $argumentLine = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -Target "{1}" -ReadyPath "{2}" -ReleasePath "{3}" -Mode {4}' -f `
        (Join-Path $PSScriptRoot 'hold-file-lock.ps1'), $env:STEP67_LOCK_TARGET,
        $env:STEP67_LOCK_READY, $env:STEP67_LOCK_RELEASE, $env:STEP67_LOCK_MODE
    $holder = Start-Process -FilePath 'powershell.exe' -ArgumentList $argumentLine -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    $expectedLockState = if ($env:STEP67_LOCK_MODE -eq 'hash') { 'locked' } else { 'watching' }
    do {
        Start-Sleep -Milliseconds 20
        $lockStatus = if (Test-Path -LiteralPath $env:STEP67_LOCK_READY -PathType Leaf) {
            Get-Content -LiteralPath $env:STEP67_LOCK_READY -Raw | ConvertFrom-Json
        } else { $null }
        if ($null -ne $lockStatus -and $lockStatus.state -in @('error', 'timeout')) {
            throw "Test lock helper failed: $($lockStatus.error)"
        }
    } until (($null -ne $lockStatus -and $lockStatus.state -eq $expectedLockState) -or [DateTime]::UtcNow -ge $deadline)
    if ($null -eq $lockStatus -or $lockStatus.state -ne $expectedLockState) {
        throw "Test lock helper did not reach state $expectedLockState."
    }
}
$code = if ($mode -in @('partial', 'both-fail')) { 7 } else { 0 }
[ordered]@{
    mode = $mode; arguments = $args; source = $source; output = $output; exitCode = $code
    lock = $lockStatus; lockProcessId = if ($null -ne $holder) { $holder.Id } else { $null }
} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $env:STEP66_COMPILER_LOG -Encoding utf8
exit $code
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'fake-iscc.ps1') -Encoding ascii

@'
param(
    [Parameter(Mandatory = $true)][string]$Target,
    [Parameter(Mandatory = $true)][string]$ReadyPath,
    [Parameter(Mandatory = $true)][string]$ReleasePath,
    [Parameter(Mandatory = $true)][ValidateSet('hash', 'result')][string]$Mode
)
$ErrorActionPreference = 'Stop'
if ($Mode -eq 'result') {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    Set-Content -LiteralPath $ReadyPath -Value (@{ state = 'watching'; processId = $PID } | ConvertTo-Json -Compress)
    $comparisonPath = Join-Path (Join-Path (Split-Path -Parent $Target) 'stage-manifests') 'stage-compare.json'
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $comparisonPath -PathType Leaf) { break }
        Start-Sleep -Milliseconds 10
    }
    if ([DateTime]::UtcNow -ge $deadline) {
        Set-Content -LiteralPath $ReadyPath -Value (@{ state = 'timeout'; processId = $PID; comparisonPath = $comparisonPath } | ConvertTo-Json -Compress)
        exit 2
    }
}
$stream = $null
try {
    $stream = [IO.File]::Open($Target, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    Set-Content -LiteralPath $ReadyPath -Value (@{ state = 'locked'; processId = $PID; target = $Target } | ConvertTo-Json -Compress)
    while (-not (Test-Path -LiteralPath $ReleasePath)) { Start-Sleep -Milliseconds 20 }
} catch {
    Set-Content -LiteralPath $ReadyPath -Value (@{ state = 'error'; processId = $PID; error = $_.Exception.Message } | ConvertTo-Json -Compress)
    exit 3
} finally {
    if ($null -ne $stream) { $stream.Dispose() }
}
exit 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'hold-file-lock.ps1') -Encoding ascii

$environmentNames = @(
    'PATH', 'INNO_SETUP_COMPILER', 'STEP66_CASE_ROOT', 'STEP66_MODE', 'STEP66_COMPILER_LOG',
    'STEP67_LOCK_MODE', 'STEP67_LOCK_READY', 'STEP67_LOCK_RELEASE', 'STEP67_LOCK_TARGET'
)
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:PATH = "$fakeBin;$env:PATH"
    $env:INNO_SETUP_COMPILER = Join-Path $fakeBin 'iscc.cmd'
    Run-Publisher 'a-success' 'success'
    Run-Publisher 'b-stage-drift' 'drift'
    Run-Publisher 'c-partial-output' 'partial'
    Run-Publisher 'd-existing-explicit' 'drift' $true
    Run-Publisher 'd-existing-default-sandbox' 'partial' $true $true
    Run-Publisher 'e-missing-output' 'missing'
    # A is the prior validated output; F must not touch any of its files.
    $previousRoot = Join-Path $EvidenceRoot 'a-success\output'
    $previousFiles = @(Get-ChildItem -LiteralPath $previousRoot -File -Recurse -Force | ForEach-Object { File-Identity $_.FullName })
    Run-Publisher 'f-separate-failed-run' 'partial'
    foreach ($file in $previousFiles) {
        $after = File-Identity $file.path
        Check ($null -ne $after -and $after.bytes -eq $file.bytes -and $after.sha256 -eq $file.sha256) "F preserves prior output: $($file.path)"
    }
    $previousFiles | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'f-prior-output-identities.json') -Encoding utf8
    Run-Publisher 'extra-empty-output' 'empty'
    Run-Publisher 'extra-directory-output' 'directory'
    Run-Publisher 'extra-compiler-and-drift' 'both-fail'
    Run-Publisher 'extra-output-hash-read-failure' 'hash-locked'
    Run-Publisher 'extra-final-result-write-failure' 'result-locked'
} finally {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
$summary = [ordered]@{
    simulated = $true; checks = $script:checks; passed = $script:checks - $script:failures.Count
    failed = $script:failures.Count; failures = @($script:failures.ToArray()); scenarios = @($script:results.ToArray())
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'test-results.json') -Encoding utf8
Write-Output ("Installer output safety: {0} checks, {1} passed, {2} failed. Evidence: {3}" -f $summary.checks, $summary.passed, $summary.failed, $EvidenceRoot)
if ($script:failures.Count -gt 0) { exit 1 }
Write-Output 'installer output safety isolated tests passed (simulated compiler only)'
