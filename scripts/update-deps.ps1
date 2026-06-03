# Bump vendored Ultimate ASI Loader (dinput8.dll) to the latest upstream
# within the pinned range and rewrite vendor/ultimate-asi-loader/{LICENSE,README.md}.
# Manual: dev runs this when they want a fresh upstream bump, then commits the
# result. CI never refreshes.
# See AGENTS.md "Vendoring Third-Party Dependencies".
#
# Special case: Ultimate-ASI-Loader ships a DLL inside a release zip, not as a
# standalone asset. We extract dinput8.dll and vendor it directly so install.cmd
# can copy it straight into the game's exe dir as the configured ASI hook slot
# (winmm.dll for Yakuza 0).

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$module = Join-Path $projectDir 'cameraunlock-core/powershell/ModLoaderSetup.psm1'
if (-not (Test-Path $module)) {
    throw "ModLoaderSetup.psm1 not found at $module. Is the cameraunlock-core submodule checked out?"
}
Import-Module $module -Force

$vendorAsiDir = Join-Path $projectDir 'vendor/ultimate-asi-loader'
$vendorAsiDll = Join-Path $vendorAsiDir 'dinput8.dll'
if (-not (Test-Path $vendorAsiDir)) {
    New-Item -ItemType Directory -Path $vendorAsiDir -Force | Out-Null
}

$tempZip = Join-Path $env:TEMP ("asi-update-" + [IO.Path]::GetRandomFileName() + ".zip")
$tempDll = Join-Path $env:TEMP ("asi-update-" + [IO.Path]::GetRandomFileName() + ".dll")
try {
    Write-Host "Refreshing vendor/ultimate-asi-loader from upstream..." -ForegroundColor Cyan
    $meta = Invoke-FetchLatestLoader `
        -OutputPath $tempZip `
        -Owner 'ThirteenAG' -Repo 'Ultimate-ASI-Loader' `
        -VersionPrefix 'v9.' `
        -AssetPattern '^Ultimate-ASI-Loader_x64\.zip$'

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $licenseText = $null
    $zip = [System.IO.Compression.ZipFile]::OpenRead($tempZip)
    try {
        $dllEntry = $zip.Entries | Where-Object { $_.Name -eq 'dinput8.dll' } | Select-Object -First 1
        if (-not $dllEntry) { throw "Upstream zip $($meta.AssetName) does not contain dinput8.dll." }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($dllEntry, $tempDll, $true)

        $licenseEntry = $zip.Entries | Where-Object { $_.Name -match '^(license|LICENSE)(\..+)?$' -and $_.FullName -notmatch '/.+/' } | Select-Object -First 1
        if ($licenseEntry) {
            $reader = New-Object System.IO.StreamReader($licenseEntry.Open())
            try { $licenseText = $reader.ReadToEnd() } finally { $reader.Dispose() }
        }
    } finally { $zip.Dispose() }

    $dllSha = (Get-FileHash -Path $tempDll -Algorithm SHA256).Hash.ToLower()

    # Idempotency: if the vendored DLL already matches upstream, leave the tree
    # alone so a routine re-run doesn't churn README.md's fetched_at timestamp.
    $licensePath = Join-Path $vendorAsiDir 'LICENSE'
    $readmePath  = Join-Path $vendorAsiDir 'README.md'
    if ((Test-Path $vendorAsiDll) -and (Test-Path $licensePath) -and (Test-Path $readmePath)) {
        $existingSha = (Get-FileHash -Path $vendorAsiDll -Algorithm SHA256).Hash.ToLower()
        if ($existingSha -eq $dllSha) {
            Write-Host "  no change (dinput8.dll sha256=$($dllSha.Substring(0,12))... matches on-disk vendor copy)" -ForegroundColor DarkGray
            return
        }
    }

    Copy-Item $tempDll $vendorAsiDll -Force

    if ($licenseText) {
        Set-Content -Path $licensePath -Value $licenseText -Encoding UTF8
    } else {
        $licenseUrl = "https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/$($meta.Tag)/license"
        Invoke-WebRequest -Uri $licenseUrl -OutFile $licensePath -UseBasicParsing -TimeoutSec 30 -Headers @{ "User-Agent" = "CameraUnlock-HeadTracking" }
    }

    $readme = @(
        '# Ultimate ASI Loader (vendored)',
        '',
        'Bundled copy of Ultimate ASI Loader, the install-time source of truth: install.cmd',
        'copies the DLL directly from here and never reaches out to the network.',
        'Refresh manually with `pixi run update-deps`, then commit.',
        '',
        '## Snapshot',
        '',
        '- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader',
        "- Tag: ``$($meta.Tag)``",
        "- Commit: ``$($meta.CommitSha)``",
        "- Asset: ``$($meta.AssetName)``",
        "- Asset URL: $($meta.AssetUrl)",
        "- dinput8.dll SHA-256: ``$dllSha``",
        "- Fetched at: $($meta.FetchedAt)",
        '',
        '`dinput8.dll` is extracted from the upstream asset untouched. install.cmd copies it to',
        'the Yakuza 0 exe dir as `winmm.dll` (the hook slot the mod loads ASI plugins through).'
    ) -join "`n"
    Set-Content -Path $readmePath -Value $readme -Encoding UTF8

    Write-Host "  tag=$($meta.Tag) sha256=$($dllSha.Substring(0,12))..." -ForegroundColor DarkGray
} finally {
    Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
    Remove-Item $tempDll -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "vendor/ultimate-asi-loader refreshed. Review and commit." -ForegroundColor Green
