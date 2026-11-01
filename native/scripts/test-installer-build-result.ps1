param(
    [string]$EvidenceRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $env:TEMP ('installer-build-result-' + [Guid]::NewGuid().ToString('N'))
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw "Evidence root already exists: $EvidenceRoot" }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

$script:checks = 0
$script:failures = [Collections.Generic.List[string]]::new()
$script:cases = [Collections.Generic.List[object]]::new()
$publisherTest = Join-Path $PSScriptRoot 'test-installer-output-safety.ps1'
$verifier = Join-Path $PSScriptRoot 'verify-installer-build-result.ps1'
$publisherEvidence = Join-Path $EvidenceRoot 'publisher'

function Check([bool]$condition, [string]$message) {
    $script:checks++
    if (-not $condition) {
        $script:failures.Add($message)
        Write-Output "FAIL: $message"
    }
}

function Get-Result([string]$scenarioName) {
    $path = Join-Path $publisherEvidence "$scenarioName\output\build-result.json"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Invoke-ResultVerifier([string]$name, [string]$resultPath, [bool]$expectValid, [string]$expectedError = '') {
    $logPath = Join-Path $EvidenceRoot "$name-verifier.log"
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $verifier, '-ResultPath', $resultPath)
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe @arguments *> $logPath
        $exitCode = [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    $text = if (Test-Path -LiteralPath $logPath -PathType Leaf) { Get-Content -LiteralPath $logPath -Raw } else { '' }
    $verification = $null
    try { $verification = $text | ConvertFrom-Json -ErrorAction Stop } catch {}
    Check (($exitCode -eq 0) -eq $expectValid) "$name has expected verifier exit code; actual=$exitCode"
    Check ($null -ne $verification -and $verification.valid -eq $expectValid) "$name returns structured verification status"
    if (-not [string]::IsNullOrWhiteSpace($expectedError)) {
        Check ($null -ne $verification -and $verification.error -like "*$expectedError*") "$name reports '$expectedError'"
    }
    $script:cases.Add([PSCustomObject]@{
        name = $name
        resultPath = $resultPath
        expectedValid = $expectValid
        exitCode = $exitCode
        expectedError = $expectedError
        log = $logPath
    })
    return $exitCode
}

function Copy-ResultBundle([string]$name) {
    $source = Join-Path $publisherEvidence 'a-success\output'
    $caseRoot = Join-Path $EvidenceRoot "copies\$name"
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $caseRoot -Recurse
    $outputRoot = Join-Path $caseRoot 'output'
    $resultPath = Join-Path $outputRoot 'build-result.json'
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    $result.installerPath = Join-Path $outputRoot "DouyuMonitor-Setup-V$($result.version).exe"
    $result.preManifestPath = Join-Path $outputRoot 'stage-manifests\stage-before-inno.json'
    $result.postManifestPath = Join-Path $outputRoot 'stage-manifests\stage-after-inno.json'
    $result.comparisonPath = Join-Path $outputRoot 'stage-manifests\stage-compare.json'
    $comparison = Get-Content -LiteralPath $result.comparisonPath -Raw | ConvertFrom-Json
    $comparison.expectedManifest = $result.preManifestPath
    $comparison.actualManifest = $result.postManifestPath
    $comparison | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $result.comparisonPath -Encoding utf8
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resultPath -Encoding utf8
    return [PSCustomObject]@{ root = $caseRoot; output = $outputRoot; resultPath = $resultPath }
}

if (-not (Test-Path -LiteralPath $publisherTest -PathType Leaf)) {
    throw "Required publisher integration test is missing: $publisherTest"
}
$previous = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $publisherTest -EvidenceRoot $publisherEvidence *> (Join-Path $EvidenceRoot 'publisher-test.log')
    $publisherExitCode = [int]$LASTEXITCODE
} finally {
    $ErrorActionPreference = $previous
}
$publisherLog = Get-Content -LiteralPath (Join-Path $EvidenceRoot 'publisher-test.log') -Raw
Check ($publisherExitCode -eq 0) "publisher scenarios complete; actual exit=$publisherExitCode"
if ($publisherExitCode -ne 0) {
    $summary = [ordered]@{
        checks = $script:checks; passed = $script:checks - $script:failures.Count
        failed = $script:failures.Count; failures = @($script:failures.ToArray())
        publisherExitCode = $publisherExitCode; publisherLog = $publisherLog; cases = @()
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'test-results.json') -Encoding utf8
    exit 1
}

$publisherResultsPath = Join-Path $publisherEvidence 'test-results.json'
$publisherResults = Get-Content -LiteralPath $publisherResultsPath -Raw | ConvertFrom-Json
Check ($publisherResults.failed -eq 0) 'publisher output and failure lifecycle scenarios pass'
Check (@($publisherResults.scenarios).Count -ge 12) 'publisher harness includes the output hash and result persistence injections'

$success = Get-Result 'a-success'
$drift = Get-Result 'b-stage-drift'
$compilerFailure = Get-Result 'c-partial-output'
$missingOutput = Get-Result 'e-missing-output'
$hashFailure = Get-Result 'extra-output-hash-read-failure'
$resultWriteFailure = Get-Result 'extra-final-result-write-failure'
$priorSuccess = Get-Result 'a-success'
$separateFailure = Get-Result 'f-separate-failed-run'

if (-not (Test-Path -LiteralPath $verifier -PathType Leaf)) {
    $script:failures.Add("Required independent verifier is missing: $verifier")
    $script:checks++
    Write-Output "FAIL: Required independent verifier is missing: $verifier"
    $summary = [ordered]@{
        checks = $script:checks; passed = $script:checks - $script:failures.Count
        failed = $script:failures.Count; failures = @($script:failures.ToArray())
        publisherExitCode = $publisherExitCode; publisherLog = $publisherLog; cases = @($script:cases.ToArray())
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'test-results.json') -Encoding utf8
    exit 1
}

$validResultPath = Join-Path $publisherEvidence 'a-success\output\build-result.json'
$validInstallerPath = Join-Path $publisherEvidence 'a-success\output\DouyuMonitor-Setup-V0.2.20.exe'
$validIdentityBefore = [PSCustomObject]@{
    bytes = (Get-Item -LiteralPath $validInstallerPath).Length
    sha256 = (Get-FileHash -LiteralPath $validInstallerPath -Algorithm SHA256).Hash
}
Check ($null -ne $success -and $success.state -eq 'validated') 'A records a successful synthetic publisher run'
Check ($null -ne $success -and $success.runId -match '^[0-9a-fA-F]{32}$') 'A contains a unique-format run identifier'
Check ($null -ne $success -and $success.installer.bytes -eq $validIdentityBefore.bytes -and $success.installer.sha256 -eq $validIdentityBefore.sha256) 'A records the actual placeholder file size and SHA-256'
Check ($null -ne $success -and $success.compareExitCode -eq 0) 'A records a successful Stage comparison'
Invoke-ResultVerifier 'a-success' $validResultPath $true | Out-Null
$validIdentityAfter = [PSCustomObject]@{
    bytes = (Get-Item -LiteralPath $validInstallerPath).Length
    sha256 = (Get-FileHash -LiteralPath $validInstallerPath -Algorithm SHA256).Hash
}
Check ($validIdentityAfter.bytes -eq $validIdentityBefore.bytes -and $validIdentityAfter.sha256 -eq $validIdentityBefore.sha256) 'A verification does not modify the candidate output'

foreach ($case in @(
    @{ name = 'b-stage-drift'; result = $drift },
    @{ name = 'c-partial-output'; result = $compilerFailure },
    @{ name = 'e-missing-output'; result = $missingOutput },
    @{ name = 'extra-empty-output'; result = (Get-Result 'extra-empty-output') },
    @{ name = 'extra-directory-output'; result = (Get-Result 'extra-directory-output') },
    @{ name = 'extra-output-hash-read-failure'; result = $hashFailure },
    @{ name = 'extra-final-result-write-failure'; result = $resultWriteFailure }
)) {
    $resultPath = Join-Path $publisherEvidence "$($case.name)\output\build-result.json"
    if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
        Check ($null -ne $case.result -and $case.result.state -ne 'validated') "$($case.name) has no validated state"
        Invoke-ResultVerifier "$($case.name)-rejected" $resultPath $false | Out-Null
    }
}

$missingResultPath = Join-Path $EvidenceRoot 'missing\build-result.json'
Invoke-ResultVerifier 'missing-result' $missingResultPath $false 'Build result is missing' | Out-Null
Check ($null -ne $drift -and $drift.compareExitCode -eq 1 -and @($drift.errors).Count -gt 0) 'D records Stage drift as a build failure'
Check ($null -ne $compilerFailure -and $compilerFailure.compilerExitCode -eq 7) 'E retains the compiler failure exit code'
Check ($null -ne $hashFailure -and $hashFailure.compilerExitCode -eq 0 -and $hashFailure.compareExitCode -eq 0 -and $null -eq $hashFailure.installer) 'C distinguishes output hash failure from compile and Stage failures'
Check ($null -ne $resultWriteFailure -and $resultWriteFailure.state -eq 'generated-unverified' -and $resultWriteFailure.compareExitCode -eq $null) 'Failed final persistence leaves an explicitly unverified intermediate record'
Check ($null -ne $priorSuccess -and $null -ne $separateFailure -and $priorSuccess.runId -ne $separateFailure.runId) 'G uses distinct run identities for success and failure'
Check ($null -ne $priorSuccess -and $null -ne $separateFailure -and $priorSuccess.state -eq 'validated' -and $separateFailure.state -eq 'failed') 'G keeps success and failure states separate'

$tamperCases = @(
    @{ name = 'modified-output'; expected = 'SHA-256' },
    @{ name = 'modified-size'; expected = 'byte count' },
    @{ name = 'redirected-path'; expected = 'expected installer path' },
    @{ name = 'malformed-json'; expected = 'could not be parsed' },
    @{ name = 'modified-comparison'; expected = 'comparison record' }
)
foreach ($tamper in $tamperCases) {
    $bundle = Copy-ResultBundle $tamper.name
    $result = Get-Content -LiteralPath $bundle.resultPath -Raw | ConvertFrom-Json
    $installer = $result.installerPath
    if ($tamper.name -eq 'modified-output') {
        [IO.File]::WriteAllText($installer, 'SIMULATED-NOT-AN-INSTALLER:failure')
    } elseif ($tamper.name -eq 'modified-size') {
        $result.installer.bytes = [long]$result.installer.bytes + 1
        $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $bundle.resultPath -Encoding utf8
    } elseif ($tamper.name -eq 'redirected-path') {
        $alternate = Join-Path $bundle.output 'alternate.exe'
        Set-Content -LiteralPath $alternate -Value 'alternate synthetic output' -NoNewline
        $result.installerPath = $alternate
        $result.installer.bytes = (Get-Item -LiteralPath $alternate).Length
        $result.installer.sha256 = (Get-FileHash -LiteralPath $alternate -Algorithm SHA256).Hash
        $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $bundle.resultPath -Encoding utf8
    } elseif ($tamper.name -eq 'malformed-json') {
        Set-Content -LiteralPath $bundle.resultPath -Value '{"formatVersion":' -NoNewline
    } elseif ($tamper.name -eq 'modified-comparison') {
        $comparisonPath = $result.comparisonPath
        $comparison = Get-Content -LiteralPath $comparisonPath -Raw | ConvertFrom-Json
        $comparison.identical = $false
        $comparison | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $comparisonPath -Encoding utf8
    }
    Invoke-ResultVerifier "tamper-$($tamper.name)" $bundle.resultPath $false $tamper.expected | Out-Null
}

$summary = [ordered]@{
    checks = $script:checks
    passed = $script:checks - $script:failures.Count
    failed = $script:failures.Count
    failures = @($script:failures.ToArray())
    publisherExitCode = $publisherExitCode
    publisherHarness = $publisherResultsPath
    verifier = $verifier
    cases = @($script:cases.ToArray())
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'test-results.json') -Encoding utf8
Write-Output ("Build result evidence contract: {0} checks, {1} passed, {2} failed. Evidence: {3}" -f $summary.checks, $summary.passed, $summary.failed, $EvidenceRoot)
if ($script:failures.Count -gt 0) { exit 1 }
Write-Output 'installer build-result evidence contract tests passed (simulated compiler only)'
