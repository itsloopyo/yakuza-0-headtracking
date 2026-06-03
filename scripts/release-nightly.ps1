[CmdletBinding()]
param([switch]$AllowDirty)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$cmakePath = Join-Path $ProjectRoot 'CMakeLists.txt'
$match = Select-String -Path $cmakePath -Pattern 'project\([^)]*VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
if (-not $match) { throw "Could not parse project VERSION from $cmakePath" }
$version = $match.Matches[0].Groups[1].Value

Publish-NightlyBuild `
    -ModId 'yakuza-0' `
    -ModName 'Yakuza0HeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -BuildCommand 'pixi run build' `
    -AllowDirty:$AllowDirty
