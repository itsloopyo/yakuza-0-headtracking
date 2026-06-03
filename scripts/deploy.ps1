#!/usr/bin/env pwsh
#Requires -Version 5.1
param(
    [Parameter(Position = 0)]
    [string]$GamePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\DevDeploy.psm1') -Force

$result = Invoke-DevDeployASILoader `
    -GameId 'yakuza-0' `
    -GameDisplayName 'Yakuza 0' `
    -BuildOutputPath (Join-Path $repoRoot 'bin\Release') `
    -ModDllName 'Yakuza0HeadTracking.asi' `
    -VendorLoaderDll (Join-Path $repoRoot 'vendor\ultimate-asi-loader\dinput8.dll') `
    -AsiLoaderName 'winmm.dll' `
    -GivenPath $GamePath

Write-Host ""
Write-Host "Deployed Yakuza0HeadTracking.asi to: $($result.ExeDir)" -ForegroundColor Green
