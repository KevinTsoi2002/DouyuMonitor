param(
    [ValidateSet('Write', 'Compare')]
    [string]$Mode,
    [string]$StageRoot = '',
    [string]$ManifestPath = '',
    [string]$ExpectedManifestPath = '',
    [string]$ActualManifestPath = '',
    [string]$DiffPath = ''
)

$ErrorActionPreference = 'Stop'
$formatVersion = 1

function Normalize-RelativePath([string]$path) {
    return $path.Replace('\', '/').TrimStart('/')
}

function Get-StageFiles([string]$root) {
    if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Stage root does not exist or is not a directory: $root"
    }
    $rootItem = Get-Item -LiteralPath $root -Force
    if ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Stage root must not be a reparse point: $root"
    }
    $rootFull = $rootItem.FullName.TrimEnd('\')
    $records = @()
    $allEntries = @(Get-ChildItem -LiteralPath $rootFull -Recurse -Force)
    foreach ($entry in $allEntries) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Stage contains an unsupported reparse point: $($entry.FullName)"
        }
    }
    foreach ($file in @($allEntries | Where-Object { -not $_.PSIsContainer })) {
        $full = $file.FullName
        if (-not $full.StartsWith("$rootFull\", [StringComparison]::OrdinalIgnoreCase)) {
            throw "Stage file escaped its root: $full"
        }
        $relative = Normalize-RelativePath $full.Substring($rootFull.Length + 1)
        if ([string]::IsNullOrWhiteSpace($relative)) {
            throw "Stage file has an empty relative path: $full"
        }
        $records += [ordered]@{
            path = $relative
            bytes = [int64]$file.Length
            sha256 = (Get-FileHash -LiteralPath $full -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
        }
    }
    # Sort by an ASCII hex encoding of UTF-8 bytes so ordering is invariant
    # across PowerShell culture settings and Windows path case rules.
    $records = @($records | Sort-Object { [BitConverter]::ToString([Text.Encoding]::UTF8.GetBytes($_['path'])) })
    $seen = @{}
    foreach ($record in $records) {
        $key = $record.path.ToLowerInvariant()
        if ($seen.ContainsKey($key) -and $seen[$key] -ne $record.path) {
            throw "Stage has a case-insensitive path collision: $($seen[$key]) / $($record.path)"
        }
        $seen[$key] = $record.path
    }
    return ,$records
}

function Write-StageManifest([string]$root, [string]$outputPath) {
    if ([string]::IsNullOrWhiteSpace($outputPath)) { throw 'ManifestPath is required in Write mode.' }
    $records = Get-StageFiles $root
    $parent = Split-Path -Parent $outputPath
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $manifest = [ordered]@{
        formatVersion = $formatVersion
        generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
        fileCount = $records.Count
        totalBytes = [int64](($records | ForEach-Object { $_.bytes } | Measure-Object -Sum).Sum)
        files = $records
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding utf8
}

function Read-StageManifest([string]$path) {
    if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Stage manifest does not exist: $path"
    }
    try { $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "Stage manifest could not be parsed: $path" }
    if ($manifest.formatVersion -ne $formatVersion -or $null -eq $manifest.files -or
        $null -eq ($manifest.fileCount -as [long]) -or $null -eq ($manifest.totalBytes -as [long])) {
        throw "Unsupported or incomplete stage manifest: $path"
    }
    $map = @{}
    $totalBytes = [int64]0
    foreach ($file in @($manifest.files)) {
        if ([string]::IsNullOrWhiteSpace($file.path) -or $file.path.Contains('\') -or
            $file.path.StartsWith('/') -or $file.path.Split('/') -contains '..' -or
            $file.path.Split('/') -contains '') {
            throw "Stage manifest contains an invalid path: $($file.path)"
        }
        $key = $file.path.ToLowerInvariant()
        if ($map.ContainsKey($key)) { throw "Stage manifest has a path collision: $($file.path)" }
        if ($file.bytes -lt 0 -or $file.sha256 -notmatch '^[0-9a-fA-F]{64}$') {
            throw "Stage manifest contains invalid file metadata: $($file.path)"
        }
        $map[$key] = [ordered]@{ path = $file.path; bytes = [int64]$file.bytes; sha256 = $file.sha256.ToLowerInvariant() }
        $totalBytes += [int64]$file.bytes
    }
    if ([int]$manifest.fileCount -ne $map.Count -or [int64]$manifest.totalBytes -ne $totalBytes) {
        throw "Stage manifest summary does not match its file records: $path"
    }
    return $map
}

function Compare-StageManifests([string]$expectedPath, [string]$actualPath, [string]$outputPath) {
    $expected = Read-StageManifest $expectedPath
    $actual = Read-StageManifest $actualPath
    $added = @($actual.Keys | Where-Object { -not $expected.ContainsKey($_) } | ForEach-Object { $actual[$_].path } | Sort-Object)
    $removed = @($expected.Keys | Where-Object { -not $actual.ContainsKey($_) } | ForEach-Object { $expected[$_].path } | Sort-Object)
    $changed = @($actual.Keys | Where-Object { $expected.ContainsKey($_) -and ($expected[$_].bytes -ne $actual[$_].bytes -or $expected[$_].sha256 -ne $actual[$_].sha256) } | ForEach-Object { $actual[$_].path } | Sort-Object)
    $result = [ordered]@{
        formatVersion = $formatVersion
        expectedManifest = (Resolve-Path -LiteralPath $expectedPath).Path
        actualManifest = (Resolve-Path -LiteralPath $actualPath).Path
        added = $added
        removed = $removed
        changed = $changed
        identical = ($added.Count -eq 0 -and $removed.Count -eq 0 -and $changed.Count -eq 0)
    }
    if (-not [string]::IsNullOrWhiteSpace($outputPath)) {
        $parent = Split-Path -Parent $outputPath
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding utf8
    }
    if (-not $result.identical) {
        Write-Error ("Stage manifest mismatch: added={0}, removed={1}, changed={2}" -f $added.Count, $removed.Count, $changed.Count)
        foreach ($kind in @('added', 'removed', 'changed')) { foreach ($path in @($result[$kind])) { Write-Error ("{0}: {1}" -f $kind, $path) } }
        return 1
    }
    return 0
}

try {
    if ($Mode -eq 'Write') { Write-StageManifest $StageRoot $ManifestPath; exit 0 }
    if ($Mode -eq 'Compare') { exit (Compare-StageManifests $ExpectedManifestPath $ActualManifestPath $DiffPath) }
    throw 'Mode must be Write or Compare.'
}
catch {
    Write-Error $_
    exit 1
}
