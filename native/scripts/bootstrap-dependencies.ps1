param(
    [string]$QtRoot = 'D:\Qt',
    [string]$MpvRoot = "$PSScriptRoot\..\sdk\mpv",
    [switch]$SkipQt
)

$ErrorActionPreference = 'Stop'
$nativeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lock = Get-Content (Join-Path $nativeRoot 'dependencies.lock.json') -Raw | ConvertFrom-Json
$archiveRoot = Join-Path $nativeRoot 'sdk\archives'
$mpvArchive = Join-Path $archiveRoot $lock.libmpv.archive

New-Item -ItemType Directory -Force -Path $archiveRoot, $MpvRoot | Out-Null

if (-not $SkipQt) {
    python -m pip install --requirement (Join-Path $nativeRoot 'dependencies\requirements.txt')
    python -m aqt install-qt windows desktop $lock.qt.version $lock.qt.arch --outputdir $QtRoot
}

if (-not (Test-Path $mpvArchive)) {
    curl.exe -L --fail --retry 3 --output $mpvArchive $lock.libmpv.url
}

$archiveHash = (Get-FileHash $mpvArchive -Algorithm SHA256).Hash
if ($archiveHash -ne $lock.libmpv.sha256) {
    throw "libmpv archive SHA-256 mismatch: $archiveHash"
}

tar.exe -xf $mpvArchive -C $MpvRoot
$runtime = Join-Path $MpvRoot $lock.libmpv.runtime
$runtimeHash = (Get-FileHash $runtime -Algorithm SHA256).Hash
if ($runtimeHash -ne $lock.libmpv.runtimeSha256) {
    throw "libmpv runtime SHA-256 mismatch: $runtimeHash"
}

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msvcBin = Join-Path $vsInstall 'VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64'
$dumpbin = Join-Path $msvcBin 'dumpbin.exe'
$lib = Join-Path $msvcBin 'lib.exe'
$def = Join-Path $MpvRoot 'libmpv.def'
$importLib = Join-Path $MpvRoot $lock.libmpv.importLibrary
$exports = & $dumpbin /exports $runtime | ForEach-Object {
    if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)\s*$') { $matches[1] }
} | Where-Object { $_ -like 'mpv_*' }
$defLines = [System.Collections.Generic.List[string]]::new()
$defLines.Add('LIBRARY ' + $lock.libmpv.runtime)
$defLines.Add('EXPORTS')
foreach ($export in $exports) {
    $defLines.Add("  $export")
}
$defLines | Set-Content -LiteralPath $def -Encoding ascii
& $lib /nologo /machine:x64 /def:$def /out:$importLib
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $importLib)) {
    throw "Failed to generate the MSVC libmpv import library"
}

Write-Output "QT_ROOT=$QtRoot\$($lock.qt.version)\msvc2022_64"
Write-Output "MPV_ROOT=$MpvRoot"
