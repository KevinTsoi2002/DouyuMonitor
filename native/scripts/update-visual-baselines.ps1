param(
    [switch]$Approve
)

$ErrorActionPreference = 'Stop'

if (-not $Approve) {
    throw 'Visual baselines may only be updated after explicit review. Re-run with -Approve.'
}

$nativeRoot = Split-Path -Parent $PSScriptRoot
$env:DOUYU_UPDATE_VISUAL_BASELINES = '1'

Push-Location $nativeRoot
try {
    ctest --preset windows-x64-release -R '^qml_visual_regression_test$' --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Visual baseline update failed with exit code $LASTEXITCODE"
    }
} finally {
    Remove-Item Env:\DOUYU_UPDATE_VISUAL_BASELINES -ErrorAction SilentlyContinue
    Pop-Location
}

Write-Host 'Visual baselines updated.' -ForegroundColor Green
Write-Host 'Review the changed PNG files in native/tests/visual/baselines before committing.'

