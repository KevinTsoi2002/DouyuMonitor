$ErrorActionPreference = 'Stop'

$scriptRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$guardPath = Join-Path $PSScriptRoot 'stage-integrity.ps1'
$evidenceRoot = Join-Path $env:TEMP ('DouyuAuditS7-b0e669b6\step64-stage-integrity-guard\test-' + [Guid]::NewGuid().ToString('N'))
$left = Join-Path $evidenceRoot 'left'
$right = Join-Path $evidenceRoot 'right'
$leftManifest = Join-Path $evidenceRoot 'left.json'
$rightManifest = Join-Path $evidenceRoot 'right.json'
$diffPath = Join-Path $evidenceRoot 'diff.json'
$singleRoot = Join-Path $evidenceRoot 'single'
$singleManifest = Join-Path $evidenceRoot 'single.json'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw "Stage integrity test failed: $message" }
}

function Invoke-Guard([string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $guardPath @arguments *> $null
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    return [int]$code
}

function Assert-ExitCode([int]$expected, [int]$actual, [string]$message) {
    if ($actual -ne $expected) {
        throw "Stage integrity test failed: $message (expected $expected, got $actual)"
    }
}

New-Item -ItemType Directory -Path $left, $right -Force | Out-Null
Set-Content -LiteralPath (Join-Path $left 'same.txt') -Value 'same' -NoNewline
Set-Content -LiteralPath (Join-Path $left 'changed.txt') -Value 'before' -NoNewline
New-Item -ItemType Directory -Path (Join-Path $left 'nested') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $left 'nested\stable.txt') -Value 'stable' -NoNewline
Remove-Item -LiteralPath $right -Recurse -Force
Copy-Item -LiteralPath $left -Destination $right -Recurse

Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $left, '-ManifestPath', $leftManifest)) 'writing the first manifest'
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $right, '-ManifestPath', $rightManifest)) 'writing the second manifest'
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $leftManifest, '-ActualManifestPath', $rightManifest, '-DiffPath', $diffPath)) 'identical stages compare equal'

Set-Content -LiteralPath (Join-Path $right 'added.txt') -Value 'added' -NoNewline
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $right, '-ManifestPath', $rightManifest)) 'writing an added-file manifest'
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $leftManifest, '-ActualManifestPath', $rightManifest, '-DiffPath', $diffPath)) 'added files fail the gate'
$diff = Get-Content -LiteralPath $diffPath -Raw | ConvertFrom-Json
Assert-True ($diff.added -contains 'added.txt') 'added classification is preserved'

Remove-Item -LiteralPath (Join-Path $right 'added.txt') -Force
Remove-Item -LiteralPath (Join-Path $right 'nested\stable.txt') -Force
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $right, '-ManifestPath', $rightManifest)) 'writing a removed-file manifest'
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $leftManifest, '-ActualManifestPath', $rightManifest, '-DiffPath', $diffPath)) 'removed files fail the gate'
$diff = Get-Content -LiteralPath $diffPath -Raw | ConvertFrom-Json
Assert-True ($diff.removed -contains 'nested/stable.txt') 'removed classification is preserved'

New-Item -ItemType Directory -Path $singleRoot -Force | Out-Null
Set-Content -LiteralPath (Join-Path $singleRoot 'only.txt') -Value 'only' -NoNewline
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $singleRoot, '-ManifestPath', $singleManifest)) 'writing a single-file manifest'
$single = Get-Content -LiteralPath $singleManifest -Raw | ConvertFrom-Json
Assert-True ($single.fileCount -eq 1) 'single-file manifest count is one'
Assert-True (@($single.files).Count -eq 1) 'single-file manifest keeps an array record'
Assert-True ($single.totalBytes -eq 4) 'single-file manifest total bytes are correct'

Set-Content -LiteralPath (Join-Path $right 'changed.txt') -Value 'after-with-a-different-size' -NoNewline
Assert-ExitCode 0 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', $right, '-ManifestPath', $rightManifest)) 'writing a changed-file manifest'
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $leftManifest, '-ActualManifestPath', $rightManifest, '-DiffPath', $diffPath)) 'changed files fail the gate'
$diff = Get-Content -LiteralPath $diffPath -Raw | ConvertFrom-Json
Assert-True ($diff.changed -contains 'changed.txt') 'changed classification is preserved'

Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Write', '-StageRoot', (Join-Path $evidenceRoot 'missing'), '-ManifestPath', (Join-Path $evidenceRoot 'missing.json'))) 'missing stage fails safely'
Set-Content -LiteralPath (Join-Path $evidenceRoot 'invalid.json') -Value '{not-json' -NoNewline
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', (Join-Path $evidenceRoot 'invalid.json'), '-ActualManifestPath', $leftManifest, '-DiffPath', $diffPath)) 'invalid manifests fail safely'
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', (Join-Path $evidenceRoot 'missing.json'), '-ActualManifestPath', $leftManifest, '-DiffPath', $diffPath)) 'missing manifests fail safely'

$simulatedInstaller = Join-Path $evidenceRoot 'simulated-installer.exe'
Set-Content -LiteralPath $simulatedInstaller -Value 'existing installer' -NoNewline
Assert-True (Test-Path -LiteralPath $simulatedInstaller) 'existing installer fixture is present'
Assert-True (Test-Path -LiteralPath $diffPath) 'mismatch diagnostics are retained'
Assert-True ((Get-Content -LiteralPath $diffPath -Raw).Contains('added')) 'mismatch diagnostics classify the failure'
Assert-True (Test-Path -LiteralPath $simulatedInstaller) 'gate failure does not delete an existing installer'

$tamperedManifest = Join-Path $evidenceRoot 'tampered.json'
$manifestObject = Get-Content -LiteralPath $leftManifest -Raw | ConvertFrom-Json
$manifestObject.files[0].sha256 = ('0' * 64)
$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $tamperedManifest -Encoding utf8
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $leftManifest, '-ActualManifestPath', $tamperedManifest, '-DiffPath', $diffPath)) 'hash changes fail the gate'

$summaryManifest = Join-Path $evidenceRoot 'bad-summary.json'
$manifestObject = Get-Content -LiteralPath $leftManifest -Raw | ConvertFrom-Json
$manifestObject.fileCount = $manifestObject.fileCount + 1
$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryManifest -Encoding utf8
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $summaryManifest, '-ActualManifestPath', $leftManifest, '-DiffPath', $diffPath)) 'inconsistent manifest summaries fail safely'

$collisionManifest = Join-Path $evidenceRoot 'collision.json'
$manifestObject = Get-Content -LiteralPath $leftManifest -Raw | ConvertFrom-Json
$manifestObject.files = @($manifestObject.files) + [pscustomobject]@{
    path = 'NESTED/stable.txt'
    bytes = 6
    sha256 = $manifestObject.files[0].sha256
}
$manifestObject.fileCount = $manifestObject.files.Count
$manifestObject | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $collisionManifest -Encoding utf8
Assert-ExitCode 1 (Invoke-Guard @('-Mode', 'Compare', '-ExpectedManifestPath', $collisionManifest, '-ActualManifestPath', $leftManifest, '-DiffPath', $diffPath)) 'case-insensitive path collisions fail safely'

$ordered = Get-Content -LiteralPath $leftManifest -Raw | ConvertFrom-Json
$paths = @($ordered.files | ForEach-Object { $_.path })
Assert-True ((@($paths | Sort-Object) -join "`n") -eq ($paths -join "`n")) 'manifest paths are stable and sorted'
Assert-True ($paths -contains 'nested/stable.txt') 'nested paths use normalized separators'

Write-Output 'stage integrity isolated tests passed'
