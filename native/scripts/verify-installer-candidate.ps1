param(
    [Parameter(Mandatory = $true)][string]$ResultPath,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Get-FullPath([string]$path, [string]$fieldName) {
    Require (-not [string]::IsNullOrWhiteSpace($path)) "$fieldName is missing."
    try { return [IO.Path]::GetFullPath($path) }
    catch { throw "$fieldName is not a valid path." }
}

function Test-SamePath([string]$left, [string]$right) {
    $leftPath = [IO.Path]::GetFullPath($left).TrimEnd('\')
    $rightPath = [IO.Path]::GetFullPath($right).TrimEnd('\')
    return [string]::Equals($leftPath, $rightPath, [StringComparison]::OrdinalIgnoreCase)
}

function Get-OrdinaryFile([string]$path, [string]$description) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$description is missing or is not an ordinary file: $path"
    }
    $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$description must not be a reparse point: $path"
    }
    return $item
}

function Get-Json([string]$path, [string]$description) {
    try {
        return Get-Content -LiteralPath $path -Raw -ErrorAction Stop |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "$description could not be parsed: $path"
    }
}

function Invoke-StageWrite([string]$stageRoot, [string]$manifestPath, [string]$logPath) {
    $helper = Join-Path $PSScriptRoot 'stage-integrity.ps1'
    Get-OrdinaryFile $helper 'Stage integrity helper' | Out-Null
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper `
            -Mode Write -StageRoot $stageRoot -ManifestPath $manifestPath *> $logPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
}

function Invoke-StageCompare(
    [string]$expectedManifestPath,
    [string]$actualManifestPath,
    [string]$diffPath,
    [string]$logPath
) {
    $helper = Join-Path $PSScriptRoot 'stage-integrity.ps1'
    Get-OrdinaryFile $helper 'Stage integrity helper' | Out-Null
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper `
            -Mode Compare -ExpectedManifestPath $expectedManifestPath `
            -ActualManifestPath $actualManifestPath -DiffPath $diffPath *> $logPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
}

function Invoke-BaselineVerifier([string]$resultFile, [string]$logPath) {
    $verifier = Join-Path $PSScriptRoot 'verify-installer-build-result.ps1'
    Get-OrdinaryFile $verifier 'Build result verifier' | Out-Null
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $verifier `
            -ResultPath $resultFile -SkipInstallerIdentity *> $logPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
}

function Get-LogText([string]$path) {
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        return Get-Content -LiteralPath $path -Raw
    }
    return ''
}

function New-VerificationResult(
    [string]$status,
    [string[]]$reasons,
    [hashtable]$data
) {
    $record = [ordered]@{
        formatVersion = 1
        verificationRunId = $script:verificationRunId
        verifiedAtUtc = [DateTime]::UtcNow.ToString('o')
        status = $status
        resultPath = $script:resultFile
        buildRunId = $null
        version = $null
        stageRoot = $null
        baselineManifestPath = $null
        baselineManifestSha256 = $null
        currentManifestPath = $null
        currentManifestSha256 = $null
        currentStage = $null
        installer = $null
        diff = [ordered]@{
            added = @()
            removed = @()
            changed = @()
        }
        reasons = @($reasons)
        baselineVerifierExitCode = $null
        baselineVerifierLog = $script:baselineLog
        currentManifestLog = $script:currentManifestLog
        compareLog = $script:compareLog
    }
    foreach ($key in $data.Keys) { $record[$key] = $data[$key] }
    return $record
}

$script:verificationRunId = [Guid]::NewGuid().ToString('N')
$script:resultFile = $null
$script:baselineLog = $null
$script:currentManifestLog = $null
$script:compareLog = $null
$script:verificationPath = $null
$script:result = $null

try {
    $evidence = Get-FullPath $EvidenceRoot 'Evidence root'
    if (Test-Path -LiteralPath $evidence) {
        throw "Evidence root already exists; use a unique directory: $evidence"
    }
    New-Item -ItemType Directory -Path $evidence | Out-Null
    $script:verificationPath = Join-Path $evidence 'candidate-verification.json'
    $script:baselineLog = Join-Path $evidence 'baseline-verifier.log'
    $script:currentManifestLog = Join-Path $evidence 'current-manifest.log'
    $script:compareLog = Join-Path $evidence 'stage-compare.log'
    $script:currentManifestPath = Join-Path $evidence 'current-stage.json'
    $script:diffPath = Join-Path $evidence 'stage-diff.json'
    $script:resultFile = Get-FullPath $ResultPath 'Build result path'

    $reasons = [Collections.Generic.List[string]]::new()
    $data = @{}

    if (-not (Test-Path -LiteralPath $script:resultFile -PathType Leaf)) {
        $record = New-VerificationResult 'insufficient_evidence' `
            @("Build result is missing or is not an ordinary file: $script:resultFile") $data
        $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8
        $record | ConvertTo-Json -Depth 10 -Compress
        exit 1
    }
    $resultItem = Get-OrdinaryFile $script:resultFile 'Build result'
    $resultRoot = $resultItem.DirectoryName

    try {
        $script:result = Get-Json $script:resultFile 'Build result JSON'
    } catch {
        $record = New-VerificationResult 'invalid_evidence' @($_.Exception.Message) $data
        $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8
        $record | ConvertTo-Json -Depth 10 -Compress
        exit 1
    }

    $data.buildRunId = if ($null -ne $script:result.runId) { [string]$script:result.runId } else { $null }
    $data.version = if ($null -ne $script:result.version) { [string]$script:result.version } else { $null }
    $data.stageRoot = if ($null -ne $script:result.stageRoot) { [string]$script:result.stageRoot } else { $null }
    $data.baselineManifestPath = if ($null -ne $script:result.preManifestPath) { [string]$script:result.preManifestPath } else { $null }

    $baselineExit = Invoke-BaselineVerifier $script:resultFile $script:baselineLog
    $data.baselineVerifierExitCode = $baselineExit
    if ($baselineExit -ne 0) {
        $baselineText = Get-LogText $script:baselineLog
        $status = if ($baselineText -match '(?i)Build result (?:formatVersion|state|runId|version|stageRoot|installerPath|preManifestPath|postManifestPath|comparisonPath|installer identity|installer byte count|installer SHA-256).*missing|invalid|not validated') {
            'invalid_evidence'
        } elseif ($baselineText -match '(?i)missing|does not exist|not found') {
            'insufficient_evidence'
        } else {
            'invalid_evidence'
        }
        $record = New-VerificationResult $status `
            @("Saved build evidence did not pass the baseline contract: $baselineText") $data
        $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8
        $record | ConvertTo-Json -Depth 10 -Compress
        exit 1
    }

    $preManifest = Get-FullPath ([string]$script:result.preManifestPath) 'Pre-Inno manifest path'
    $postManifest = Get-FullPath ([string]$script:result.postManifestPath) 'Post-Inno manifest path'
    $stageRoot = Get-FullPath ([string]$script:result.stageRoot) 'Stage root'
    $expectedInstaller = Join-Path $resultRoot "DouyuMonitor-Setup-V$($script:result.version).exe"
    $recordedInstaller = Get-FullPath ([string]$script:result.installerPath) 'Recorded installer path'

    if (-not (Test-Path -LiteralPath $preManifest -PathType Leaf)) {
        throw "Saved pre-Inno manifest is missing: $preManifest"
    }
    $data.baselineManifestSha256 = (Get-FileHash -LiteralPath $preManifest -Algorithm SHA256 -ErrorAction Stop).Hash

    $stageWriteExit = Invoke-StageWrite $stageRoot $script:currentManifestPath $script:currentManifestLog
    if ($stageWriteExit -ne 0) {
        $stageText = Get-LogText $script:currentManifestLog
        $status = if (-not (Test-Path -LiteralPath $stageRoot -PathType Container)) {
            'insufficient_evidence'
        } else {
            'verification_error'
        }
        $record = New-VerificationResult $status `
            @("Current Stage manifest could not be generated: $stageText") $data
        $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8
        $record | ConvertTo-Json -Depth 10 -Compress
        exit 1
    }

    $data.currentManifestPath = $script:currentManifestPath
    $data.currentManifestSha256 = (Get-FileHash -LiteralPath $script:currentManifestPath -Algorithm SHA256 -ErrorAction Stop).Hash
    $currentManifestData = Get-Json $script:currentManifestPath 'Current Stage manifest'
    $data.currentStage = [ordered]@{
        fileCount = [int64]$currentManifestData.fileCount
        totalBytes = [int64]$currentManifestData.totalBytes
    }

    $compareExit = Invoke-StageCompare $preManifest $script:currentManifestPath $script:diffPath $script:compareLog

    $diff = $null
    if (Test-Path -LiteralPath $script:diffPath -PathType Leaf) {
        try { $diff = Get-Json $script:diffPath 'Stage diff' } catch {}
    }
    if ($null -ne $diff) {
        $data.diff = [ordered]@{
            added = @($diff.added)
            removed = @($diff.removed)
            changed = @($diff.changed)
        }
    }

    $installerData = [ordered]@{
        expectedPath = $expectedInstaller
        recordedPath = $recordedInstaller
        expectedBytes = $null
        expectedSha256 = $null
        actualBytes = $null
        actualSha256 = $null
    }
    if ($null -ne $script:result.installer) {
        $installerData.expectedBytes = [int64]$script:result.installer.bytes
        $installerData.expectedSha256 = [string]$script:result.installer.sha256
    }
    $data.installer = $installerData

    $installerReasons = [Collections.Generic.List[string]]::new()
    $installerIoError = $false
    if (-not (Test-SamePath $recordedInstaller $expectedInstaller)) {
        $installerReasons.Add('Recorded installer path does not match the versioned output path.')
    } elseif (-not (Test-Path -LiteralPath $recordedInstaller -PathType Leaf)) {
        $installerReasons.Add("Installer output is missing or is not an ordinary file: $recordedInstaller")
    } else {
        $installerItem = Get-Item -LiteralPath $recordedInstaller -Force -ErrorAction Stop
        if (($installerItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            $installerReasons.Add('Installer output is a reparse point.')
        } else {
            $installerData.actualBytes = [int64]$installerItem.Length
            try {
                $installerData.actualSha256 = (Get-FileHash -LiteralPath $recordedInstaller -Algorithm SHA256 -ErrorAction Stop).Hash
            } catch {
                $installerIoError = $true
                $installerReasons.Add("Installer output SHA-256 could not be read: $($_.Exception.Message)")
            }
            if ($installerItem.Length -eq 0) { $installerReasons.Add('Installer output is zero bytes.') }
            if ($null -ne $installerData.expectedBytes -and $installerItem.Length -ne $installerData.expectedBytes) {
                $installerReasons.Add('Installer byte count differs from build evidence.')
            }
            if ($null -ne $installerData.actualSha256 -and
                -not [string]::Equals($installerData.actualSha256, $installerData.expectedSha256, [StringComparison]::OrdinalIgnoreCase)) {
                $installerReasons.Add('Installer SHA-256 differs from build evidence.')
            }
        }
    }

    $stageDrift = ($compareExit -ne 0 -and $null -ne $diff -and
        (@($diff.added).Count -gt 0 -or @($diff.removed).Count -gt 0 -or @($diff.changed).Count -gt 0))
    if ($compareExit -ne 0 -and -not $stageDrift) {
        throw "Stage comparison could not be completed. See $script:compareLog"
    }

    if ($stageDrift) {
        $reasons.Add('Current Stage differs from the saved build-time manifest.')
    }
    foreach ($reason in $installerReasons) { $reasons.Add($reason) }

    $status = if ($installerIoError) {
        'verification_error'
    } elseif ($reasons.Count -gt 0) {
        'drift_detected'
    } else {
        'verified'
    }
    $record = New-VerificationResult $status @($reasons.ToArray()) $data
    $record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8
    $record | ConvertTo-Json -Depth 12 -Compress
    if ($status -eq 'verified') { exit 0 }
    exit 1
} catch {
    $message = $_.Exception.Message
    if ($null -eq $script:verificationPath) {
        Write-Error $message
        exit 1
    }
    $record = New-VerificationResult 'verification_error' @($message) @{}
    try { $record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $script:verificationPath -Encoding utf8 } catch {}
    $record | ConvertTo-Json -Depth 12 -Compress
    exit 1
}
