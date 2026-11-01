param(
    [Parameter(Mandatory = $true)][string]$ResultPath,
    [switch]$SkipInstallerIdentity
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

function Require-OrdinaryFile([string]$path, [string]$description) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$description is missing or is not an ordinary file: $path"
    }
    $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$description must not be a reparse point: $path"
    }
    return $item
}

function Require-ContainedFile([string]$path, [string]$root, [string]$description) {
    $fullPath = Get-FullPath $path $description
    $rootPath = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$description is outside the build result directory."
    }
    Require-OrdinaryFile $fullPath $description | Out-Null
    return $fullPath
}

try {
    $resultFile = Get-FullPath $ResultPath 'Build result path'
    Require (([IO.Path]::GetFileName($resultFile) -ceq 'build-result.json')) `
        'Build result must be named build-result.json.'
    $resultItem = Require-OrdinaryFile $resultFile 'Build result'
    $resultRoot = $resultItem.DirectoryName
    $rootItem = Get-Item -LiteralPath $resultRoot -Force -ErrorAction Stop
    Require (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) `
        'Build result directory must not be a reparse point.'

    try {
        $result = Get-Content -LiteralPath $resultFile -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw 'Build result JSON could not be parsed.'
    }
    Require ($null -ne $result -and $null -ne $result.PSObject) 'Build result must contain a JSON object.'
    Require ($result.formatVersion -is [int] -or $result.formatVersion -is [long]) `
        'Build result formatVersion must be an integer.'
    Require ($result.formatVersion -eq 1) 'Unsupported build result formatVersion.'
    Require ($result.state -ceq 'validated') 'Build result state is not validated.'
    Require ($result.runId -match '^[0-9a-fA-F]{32}$') 'Build result runId is missing or invalid.'
    Require ($result.version -match '^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$') `
        'Build result version is missing or invalid.'
    Require (-not [string]::IsNullOrWhiteSpace($result.stageRoot)) 'Build result stageRoot is missing.'

    foreach ($field in @('compilerExitCode', 'preManifestExitCode', 'postManifestExitCode', 'compareExitCode')) {
        $value = $result.$field
        Require (($value -is [int] -or $value -is [long]) -and $value -eq 0) `
            "Build result $field is not zero."
    }
    Require ($null -ne $result.PSObject.Properties['errors']) 'Build result errors field is missing.'
    Require (@($result.errors).Count -eq 0) 'Build result contains recorded errors.'

    $expectedInstaller = Join-Path $resultRoot "DouyuMonitor-Setup-V$($result.version).exe"
    $installerPath = Get-FullPath $result.installerPath 'Build result installerPath'
    Require (Test-SamePath $installerPath $expectedInstaller) `
        'Installer path does not match the expected installer path.'
    Require ($null -ne $result.installer) 'Build result installer identity is missing.'
    Require ($result.installer.bytes -is [int] -or $result.installer.bytes -is [long]) `
        'Build result installer byte count must be an integer.'
    Require ($result.installer.bytes -gt 0) 'Build result installer byte count must be positive.'
    Require ($result.installer.sha256 -match '^[0-9a-fA-F]{64}$') `
        'Build result installer SHA-256 is missing or invalid.'
    $installer = $null
    $actualHash = $null
    if (-not $SkipInstallerIdentity) {
        $installer = Require-OrdinaryFile $installerPath 'Installer output'
        Require ($result.installer.bytes -eq $installer.Length) 'Installer byte count does not match the file.'
        try {
            $actualHash = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256 -ErrorAction Stop).Hash
        } catch {
            throw "Installer output cannot be read for SHA-256 verification: $($_.Exception.Message)"
        }
        Require ([string]::Equals($result.installer.sha256, $actualHash, [StringComparison]::OrdinalIgnoreCase)) `
            'Installer SHA-256 does not match the file.'
    }

    $manifestRoot = Join-Path $resultRoot 'stage-manifests'
    Require (Test-Path -LiteralPath $manifestRoot -PathType Container) 'Stage manifest directory is missing.'
    $manifestDirectory = Get-Item -LiteralPath $manifestRoot -Force -ErrorAction Stop
    Require (($manifestDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) `
        'Stage manifest directory must not be a reparse point.'
    $expectedPre = Join-Path $manifestRoot 'stage-before-inno.json'
    $expectedPost = Join-Path $manifestRoot 'stage-after-inno.json'
    $expectedComparison = Join-Path $manifestRoot 'stage-compare.json'
    $preManifest = Require-ContainedFile $result.preManifestPath $resultRoot 'Pre-Inno manifest'
    $postManifest = Require-ContainedFile $result.postManifestPath $resultRoot 'Post-Inno manifest'
    $comparisonPath = Require-ContainedFile $result.comparisonPath $resultRoot 'Stage comparison'
    Require (Test-SamePath $preManifest $expectedPre) 'Pre-Inno manifest path is unexpected.'
    Require (Test-SamePath $postManifest $expectedPost) 'Post-Inno manifest path is unexpected.'
    Require (Test-SamePath $comparisonPath $expectedComparison) 'Stage comparison path is unexpected.'

    try {
        $comparison = Get-Content -LiteralPath $comparisonPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw 'Saved Stage comparison JSON could not be parsed.'
    }
    Require ($null -ne $comparison -and $comparison.formatVersion -eq 1) `
        'Saved Stage comparison format is unsupported.'
    Require (Test-SamePath $comparison.expectedManifest $preManifest) `
        'Saved Stage comparison record is inconsistent with its expected manifest.'
    Require (Test-SamePath $comparison.actualManifest $postManifest) `
        'Saved Stage comparison record is inconsistent with its actual manifest.'
    Require ($comparison.identical -eq $true -and @($comparison.added).Count -eq 0 -and
        @($comparison.removed).Count -eq 0 -and @($comparison.changed).Count -eq 0) `
        'Saved Stage comparison record is inconsistent.'

    $helper = Join-Path $PSScriptRoot 'stage-integrity.ps1'
    Require-OrdinaryFile $helper 'Stage integrity helper' | Out-Null
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper `
        -Mode Compare -ExpectedManifestPath $preManifest -ActualManifestPath $postManifest
    $comparisonExitCode = [int]$LASTEXITCODE
    Require ($comparisonExitCode -eq 0) `
        "Referenced Stage manifests do not compare as identical (exit code $comparisonExitCode)."

    [ordered]@{
        valid = $true
        state = $result.state
        runId = $result.runId
        version = $result.version
        installerPath = $installerPath
        bytes = if ($null -eq $installer) { $null } else { [int64]$installer.Length }
        sha256 = if ($null -eq $actualHash) { $null } else { $actualHash.ToLowerInvariant() }
        installerIdentityChecked = (-not $SkipInstallerIdentity)
        stageComparison = 'identical'
    } | ConvertTo-Json -Compress
    exit 0
} catch {
    [ordered]@{
        valid = $false
        error = $_.Exception.Message
    } | ConvertTo-Json -Compress
    exit 1
}
