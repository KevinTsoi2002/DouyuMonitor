param(
    [string]$EvidenceRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $env:TEMP ('installer-candidate-verification-' + [Guid]::NewGuid().ToString('N'))
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "Evidence root already exists: $EvidenceRoot"
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

$script:checks = 0
$script:failures = [Collections.Generic.List[string]]::new()
$script:cases = [Collections.Generic.List[object]]::new()
$helper = Join-Path $PSScriptRoot 'stage-integrity.ps1'
$verifier = Join-Path $PSScriptRoot 'verify-installer-candidate.ps1'

function Check([bool]$condition, [string]$message) {
    $script:checks++
    if (-not $condition) {
        $script:failures.Add($message)
        Write-Output "FAIL: $message"
    }
}

function Invoke-Helper([string[]]$arguments, [string]$logPath) {
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper @arguments *> $logPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
}

function New-SyntheticCandidate([string]$name) {
    $root = Join-Path $EvidenceRoot "cases\$name"
    $stage = Join-Path $root 'stage'
    $output = Join-Path $root 'output'
    $manifests = Join-Path $output 'stage-manifests'
    New-Item -ItemType Directory -Path $stage, $manifests | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stage 'nested') | Out-Null
    [IO.File]::WriteAllText((Join-Path $stage 'alpha.txt'), 'alpha-content')
    [IO.File]::WriteAllBytes((Join-Path $stage 'nested\beta.bin'), [byte[]](1, 2, 3, 4, 5))

    $pre = Join-Path $manifests 'stage-before-inno.json'
    $post = Join-Path $manifests 'stage-after-inno.json'
    $compare = Join-Path $manifests 'stage-compare.json'
    $preExit = Invoke-Helper @('-Mode', 'Write', '-StageRoot', $stage, '-ManifestPath', $pre) (Join-Path $root 'pre-manifest.log')
    $postExit = Invoke-Helper @('-Mode', 'Write', '-StageRoot', $stage, '-ManifestPath', $post) (Join-Path $root 'post-manifest.log')
    $compareExit = Invoke-Helper @('-Mode', 'Compare', '-ExpectedManifestPath', $pre, '-ActualManifestPath', $post, '-DiffPath', $compare) (Join-Path $root 'initial-compare.log')
    Check ($preExit -eq 0 -and $postExit -eq 0 -and $compareExit -eq 0) "$name synthetic manifest preparation"

    $installer = Join-Path $output 'DouyuMonitor-Setup-V1.2.3.exe'
    [IO.File]::WriteAllText($installer, 'synthetic installer placeholder')
    $installerItem = Get-Item -LiteralPath $installer -Force
    $resultPath = Join-Path $output 'build-result.json'
    [ordered]@{
        formatVersion = 1
        runId = [Guid]::NewGuid().ToString('N')
        state = 'validated'
        updatedAtUtc = [DateTime]::UtcNow.ToString('o')
        version = '1.2.3'
        stageRoot = $stage
        installerPath = $installer
        preManifestPath = $pre
        postManifestPath = $post
        comparisonPath = $compare
        compilerExitCode = 0
        preManifestExitCode = $preExit
        postManifestExitCode = $postExit
        compareExitCode = $compareExit
        installer = [ordered]@{
            bytes = [int64]$installerItem.Length
            sha256 = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
        }
        errors = @()
    } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
    return [PSCustomObject]@{
        name = $name
        root = $root
        stage = $stage
        output = $output
        result = $resultPath
        installer = $installer
        pre = $pre
        post = $post
        compare = $compare
    }
}

function Invoke-Candidate([string]$name, [string]$resultPath, [string]$expectedStatus, [string]$expectedText = '') {
    $evidence = Join-Path $EvidenceRoot "verifications\$name"
    $log = Join-Path $EvidenceRoot "$name-verifier.log"
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $verifier `
            -ResultPath $resultPath -EvidenceRoot $evidence *> $log
        $exitCode = [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    $verificationPath = Join-Path $evidence 'candidate-verification.json'
    $record = $null
    if (Test-Path -LiteralPath $verificationPath -PathType Leaf) {
        try { $record = Get-Content -LiteralPath $verificationPath -Raw | ConvertFrom-Json } catch {}
    }
    $expectedExit = if ($expectedStatus -eq 'verified') { 0 } else { 1 }
    Check ($exitCode -eq $expectedExit) "$name exit code expected=$expectedExit actual=$exitCode"
    Check ($null -ne $record -and $record.status -eq $expectedStatus) `
        "$name status expected=$expectedStatus actual=$($record.status)"
    if (-not [string]::IsNullOrWhiteSpace($expectedText)) {
        $serialized = if ($null -eq $record) { '' } else { $record | ConvertTo-Json -Depth 10 }
        Check ($serialized -like "*$expectedText*") "$name reports '$expectedText'"
    }
    $script:cases.Add([PSCustomObject]@{
        name = $name
        resultPath = $resultPath
        evidence = $evidence
        log = $log
        expectedStatus = $expectedStatus
        actualStatus = if ($null -eq $record) { $null } else { $record.status }
        exitCode = $exitCode
    })
    return $record
}

# A: complete candidate remains verified.
$a = New-SyntheticCandidate 'a-success'
$aRecord = Invoke-Candidate 'a-success' $a.result 'verified'
Check ($aRecord.currentStage.fileCount -eq 2) 'A records the current Stage file count'
Check ($aRecord.currentStage.totalBytes -gt 0) 'A records the current Stage byte count'
Check ($aRecord.diff.added.Count -eq 0 -and $aRecord.diff.removed.Count -eq 0 -and $aRecord.diff.changed.Count -eq 0) `
    'A records an empty Stage diff'

# B: a file removed after the build.
$b = New-SyntheticCandidate 'b-removed'
Remove-Item -LiteralPath (Join-Path $b.stage 'alpha.txt')
$bRecord = Invoke-Candidate 'b-removed' $b.result 'drift_detected' 'alpha.txt'
Check ($bRecord.diff.removed -contains 'alpha.txt') 'B classifies the removed file'

# C: an existing file changed and a separate candidate receives a new file.
$c = New-SyntheticCandidate 'c-changed'
[IO.File]::WriteAllText((Join-Path $c.stage 'alpha.txt'), 'changed-content')
$cRecord = Invoke-Candidate 'c-changed' $c.result 'drift_detected' 'changed'
Check ($cRecord.diff.changed -contains 'alpha.txt') 'C classifies the changed file'

$cAdded = New-SyntheticCandidate 'c-added'
[IO.File]::WriteAllText((Join-Path $cAdded.stage 'gamma.txt'), 'added-content')
$cAddedRecord = Invoke-Candidate 'c-added' $cAdded.result 'drift_detected' 'added'
Check ($cAddedRecord.diff.added -contains 'gamma.txt') 'C classifies the added file'

# D: installer bytes no longer match the build evidence.
$d = New-SyntheticCandidate 'd-installer-changed'
[IO.File]::WriteAllText($d.installer, 'changed-installer-content')
$dRecord = Invoke-Candidate 'd-installer-changed' $d.result 'drift_detected' 'Installer SHA-256'
Check ($dRecord.installer.actualSha256 -ne $dRecord.installer.expectedSha256) 'D records the changed installer identity'

# E: missing, zero-byte, and directory installer outputs.
$eMissing = New-SyntheticCandidate 'e-installer-missing'
Remove-Item -LiteralPath $eMissing.installer
Invoke-Candidate 'e-installer-missing' $eMissing.result 'drift_detected' 'missing'

$eZero = New-SyntheticCandidate 'e-installer-zero'
[IO.File]::WriteAllBytes($eZero.installer, [byte[]]@())
Invoke-Candidate 'e-installer-zero' $eZero.result 'drift_detected' 'zero bytes'

$eDirectory = New-SyntheticCandidate 'e-installer-directory'
Remove-Item -LiteralPath $eDirectory.installer
New-Item -ItemType Directory -Path $eDirectory.installer | Out-Null
Invoke-Candidate 'e-installer-directory' $eDirectory.result 'drift_detected' 'missing or is not an ordinary file'

# F: malformed or missing evidence.
$fMissing = Join-Path $EvidenceRoot 'cases\f-missing\output\build-result.json'
Invoke-Candidate 'f-missing-result' $fMissing 'insufficient_evidence' 'missing'

$fMalformedRoot = Join-Path $EvidenceRoot 'cases\f-malformed'
New-Item -ItemType Directory -Path (Join-Path $fMalformedRoot 'output') -Force | Out-Null
$fMalformed = Join-Path $fMalformedRoot 'output\build-result.json'
Set-Content -LiteralPath $fMalformed -Value '{"formatVersion":' -NoNewline
Invoke-Candidate 'f-malformed-result' $fMalformed 'invalid_evidence' 'could not be parsed'

$fField = New-SyntheticCandidate 'f-missing-field'
$fFieldResult = Get-Content -LiteralPath $fField.result -Raw | ConvertFrom-Json
$fFieldResult.stageRoot = $null
$fFieldResult | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $fField.result -Encoding utf8
Invoke-Candidate 'f-missing-field' $fField.result 'invalid_evidence' 'stageRoot is missing'

$fCompare = New-SyntheticCandidate 'f-contradictory-compare'
$fCompareJson = Get-Content -LiteralPath $fCompare.compare -Raw | ConvertFrom-Json
$fCompareJson.identical = $false
$fCompareJson | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $fCompare.compare -Encoding utf8
Invoke-Candidate 'f-contradictory-compare' $fCompare.result 'invalid_evidence' 'inconsistent'

$fHash = New-SyntheticCandidate 'f-recorded-size-mismatch'
$fHashResult = Get-Content -LiteralPath $fHash.result -Raw | ConvertFrom-Json
$fHashResult.installer.bytes = [int64]$fHashResult.installer.bytes + 1
$fHashResult | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $fHash.result -Encoding utf8
Invoke-Candidate 'f-recorded-size-mismatch' $fHash.result 'drift_detected' 'byte count'

# G: the saved baseline is reused; repeated verification never promotes drift.
$g = New-SyntheticCandidate 'g-baseline-isolation'
$baselineHash = (Get-FileHash -LiteralPath $g.pre -Algorithm SHA256).Hash
Remove-Item -LiteralPath (Join-Path $g.stage 'alpha.txt')
$gFirst = Invoke-Candidate 'g-baseline-first' $g.result 'drift_detected'
$gSecond = Invoke-Candidate 'g-baseline-second' $g.result 'drift_detected'
Check ($gFirst.baselineManifestSha256 -eq $gSecond.baselineManifestSha256 -and
    $gFirst.baselineManifestSha256 -eq $baselineHash) 'G preserves the original baseline across runs'

# H: independent candidates do not share result or evidence state.
$hValid = New-SyntheticCandidate 'h-valid-candidate'
$hInvalid = New-SyntheticCandidate 'h-invalid-candidate'
Remove-Item -LiteralPath (Join-Path $hInvalid.stage 'alpha.txt')
$hValidRecord = Invoke-Candidate 'h-valid-candidate' $hValid.result 'verified'
$hInvalidRecord = Invoke-Candidate 'h-invalid-candidate' $hInvalid.result 'drift_detected'
Check ($hValidRecord.verificationRunId -ne $hInvalidRecord.verificationRunId) 'H uses independent verification run IDs'
Check ($hValidRecord.resultPath -ne $hInvalidRecord.resultPath) 'H keeps candidate result paths separate'

# I: invalid/escaped Stage path fails without scanning an unrelated directory.
$i = New-SyntheticCandidate 'i-invalid-stage-path'
$iResult = Get-Content -LiteralPath $i.result -Raw | ConvertFrom-Json
$iResult.stageRoot = Join-Path $i.root '..\missing-stage-outside-case'
$iResult | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $i.result -Encoding utf8
Invoke-Candidate 'i-invalid-stage-path' $i.result 'insufficient_evidence' 'Stage'

$summary = [ordered]@{
    checks = $script:checks
    passed = $script:checks - $script:failures.Count
    failed = $script:failures.Count
    failures = @($script:failures.ToArray())
    evidenceRoot = $EvidenceRoot
    cases = @($script:cases.ToArray())
    reparsePointScenario = 'not attempted; invalid path and reparse rejection remain covered by stage-integrity.ps1'
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'test-results.json') -Encoding utf8
Write-Output ("Candidate verification tests: {0} checks, {1} passed, {2} failed. Evidence: {3}" -f `
    $summary.checks, $summary.passed, $summary.failed, $EvidenceRoot)
if ($script:failures.Count -gt 0) { exit 1 }
Write-Output 'installer candidate postbuild verification tests passed (synthetic files only)'
