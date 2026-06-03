#!/usr/bin/env pwsh
#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

# Canonical version source: CMakeLists.txt project(... VERSION X.Y.Z).
$cmake = Join-Path $projectDir 'CMakeLists.txt'
$match = Select-String -Path $cmake -Pattern 'project\([^)]*VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
if (-not $match) { throw "Could not parse project VERSION from $cmake" }
$version = $match.Matches[0].Groups[1].Value

Write-Host "=== Yakuza0HeadTracking - Package Release ===" -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan

$asi = Join-Path $projectDir 'bin/Release/Yakuza0HeadTracking.asi'
if (-not (Test-Path $asi)) { throw "Yakuza0HeadTracking.asi not found at: $asi - run 'pixi run build' first" }

# The committed vendor tree is the install-time source of truth; the installer
# ZIP is incomplete without it. Packaging never refreshes it - that is
# update-deps' job (manual dev action + commit).
$vendorAsi = Join-Path $projectDir 'vendor/ultimate-asi-loader/dinput8.dll'
if (-not (Test-Path $vendorAsi)) {
    throw "Vendored ASI loader missing at $vendorAsi - run 'pixi run update-deps' and commit the result"
}

$releaseDir = Join-Path $projectDir 'release'
if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

# ---------- Installer ZIP ----------
# build.yml uploads release/artifact-contents/ as the per-push artifact;
# keep this staging dir name in sync with the workflow.
$stageDir = Join-Path $releaseDir 'artifact-contents'
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

$pluginsDir = Join-Path $stageDir 'plugins'
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asi $pluginsDir

Copy-Item (Join-Path $scriptDir 'install.cmd') $stageDir
Copy-Item (Join-Path $scriptDir 'uninstall.cmd') $stageDir

$vendorStage = Join-Path $stageDir 'vendor/ultimate-asi-loader'
New-Item -ItemType Directory -Path $vendorStage -Force | Out-Null
Copy-Item $vendorAsi $vendorStage
foreach ($f in 'LICENSE','README.md') {
    $src = Join-Path $projectDir "vendor/ultimate-asi-loader/$f"
    if (Test-Path $src) { Copy-Item $src $vendorStage }
}

# find-game.ps1's release-ZIP layout expects GamePathDetection.psm1 and
# games.json co-located in shared/ (see layout 2 in find-game.ps1).
$sharedDir = Join-Path $stageDir 'shared'
New-Item -ItemType Directory -Path $sharedDir -Force | Out-Null
foreach ($rel in 'scripts/find-game.ps1','powershell/GamePathDetection.psm1','data/games.json') {
    $src = Join-Path $projectDir "cameraunlock-core/$rel"
    if (-not (Test-Path $src)) { throw "Shared bundle file missing: $src - cameraunlock-core checkout is incomplete" }
    Copy-Item $src $sharedDir
}

foreach ($doc in 'README.md','LICENSE','CHANGELOG.md','THIRD-PARTY-NOTICES.md') {
    $src = Join-Path $projectDir $doc
    if (Test-Path $src) { Copy-Item $src $stageDir }
}

$installerZip = Join-Path $releaseDir "Yakuza0HeadTracking-v$version-installer.zip"
if (Test-Path $installerZip) { Remove-Item $installerZip -Force }
Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $installerZip -Force

Write-Host "Installer ZIP: $installerZip" -ForegroundColor Green

# ---------- Nexus ZIP (extract-to-game-folder) ----------
# The .asi lands next to Yakuza0.exe (media\) - see executable_relpath for
# yakuza-0 in cameraunlock-core/data/games.json. No loader, no scripts, no
# docs: Nexus users manage their own ASI loader.
$nexusStage = Join-Path $releaseDir 'nexus-stage'
if (Test-Path $nexusStage) { Remove-Item $nexusStage -Recurse -Force }
$nexusMediaDir = Join-Path $nexusStage 'media'
New-Item -ItemType Directory -Path $nexusMediaDir -Force | Out-Null
Copy-Item $asi $nexusMediaDir

$nexusZip = Join-Path $releaseDir "Yakuza0HeadTracking-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
Compress-Archive -Path (Join-Path $nexusStage '*') -DestinationPath $nexusZip -Force
Remove-Item $nexusStage -Recurse -Force

Write-Host "Nexus ZIP:     $nexusZip" -ForegroundColor Green
