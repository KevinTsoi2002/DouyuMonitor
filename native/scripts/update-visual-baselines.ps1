param(
    [switch]$Approve,
    [string]$BuildDirectory
)

$ErrorActionPreference = 'Stop'

if (-not $Approve) {
    throw 'Visual baselines may only be updated after explicit review. Re-run with -Approve.'
}

$nativeRoot = Split-Path -Parent $PSScriptRoot

if ($BuildDirectory) {
    $buildDirectoryPath = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
        [System.IO.Path]::GetFullPath($BuildDirectory)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $nativeRoot $BuildDirectory))
    }
    if (-not (Test-Path -LiteralPath (Join-Path $buildDirectoryPath 'CMakeCache.txt'))) {
        throw "CMake build directory is not configured: $buildDirectoryPath"
    }
}

$env:DOUYU_UPDATE_VISUAL_BASELINES = '1'
Push-Location $nativeRoot
try {
    if ($BuildDirectory) {
        cmake --build $buildDirectoryPath --target qml_visual_regression_test
    } else {
        cmake --build --preset windows-x64-release --target qml_visual_regression_test
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Visual test build failed with exit code $LASTEXITCODE"
    }
    if ($BuildDirectory) {
        ctest --test-dir $buildDirectoryPath -R '^qml_visual_regression_test$' --output-on-failure
    } else {
        ctest --preset windows-x64-release -R '^qml_visual_regression_test$' --output-on-failure
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Visual baseline update failed with exit code $LASTEXITCODE"
    }
} finally {
    Remove-Item Env:\DOUYU_UPDATE_VISUAL_BASELINES -ErrorAction SilentlyContinue
    Pop-Location
}

Write-Host 'Visual baselines updated.' -ForegroundColor Green
Write-Host 'Review the changed PNG files in native/tests/visual/baselines before committing.'

