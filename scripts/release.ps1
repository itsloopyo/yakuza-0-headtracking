#!/usr/bin/env pwsh
#Requires -Version 5.1
param(
    [Parameter(Position = 0)]
    [string]$Version = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Version) {
    Write-Host "Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>"
    exit 1
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

# Canonical version source: CMakeLists.txt project(... VERSION X.Y.Z).
# pixi.toml and scripts/install.cmd MOD_VERSION mirror it.
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$currentMatch = Select-String -Path $cmakePath -Pattern 'project\([^)]*VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
if (-not $currentMatch) { throw "Could not parse project VERSION from $cmakePath" }
$currentVersion = $currentMatch.Matches[0].Groups[1].Value

$newVersion = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
if (-not (Test-SemanticVersion -Version $newVersion)) {
    throw "Resolved version '$newVersion' is not semver X.Y.Z. Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>"
}
Write-Host "Releasing v$newVersion (current: v$currentVersion)" -ForegroundColor Cyan

# Preconditions - deterministic checks, no prompts.
$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
if ($branch -ne 'main') { throw "Must release from 'main' branch (currently on '$branch')." }

if (-not (Test-CleanGitStatus)) { throw "Working tree is not clean. Commit or stash before releasing." }

$tag = "v$newVersion"
if (Test-GitTagExists -Tag $tag) { throw "Tag $tag already exists." }

# Update CMakeLists.txt (canonical)
$cmake = Get-Content $cmakePath -Raw
$cmake = $cmake -replace '(project\([^)]*?VERSION\s+)\d+\.\d+\.\d+', "`${1}$newVersion"
Set-Content -Path $cmakePath -Value $cmake -NoNewline

# Mirror into pixi.toml
$pixiPath = Join-Path $repoRoot 'pixi.toml'
$pixi = Get-Content $pixiPath -Raw
$pixi = $pixi -replace '(?m)^(version\s*=\s*)"[^"]+"', "`${1}`"$newVersion`""
Set-Content -Path $pixiPath -Value $pixi -NoNewline

# Mirror into scripts/install.cmd (MOD_VERSION lands in the state file at install time)
$installCmdPath = Join-Path $repoRoot 'scripts\install.cmd'
$installCmd = Get-Content $installCmdPath -Raw
if ($installCmd -notmatch 'set "MOD_VERSION=[^"]+"') { throw "MOD_VERSION line not found in $installCmdPath" }
$installCmd = $installCmd -replace 'set "MOD_VERSION=[^"]+"', "set `"MOD_VERSION=$newVersion`""
Set-Content -Path $installCmdPath -Value $installCmd -NoNewline

# Mirror into launcher-manifest.json (the file the launcher reads). The only
# version key is mod_info.version (four-space indent); stamp it in place.
$manifestPath = Join-Path $repoRoot 'launcher-manifest.json'
$manifest = Get-Content $manifestPath -Raw
if ($manifest -notmatch '(?m)^    "version":\s*"[^"]+"') { throw "mod_info.version line not found in $manifestPath" }
$manifest = $manifest -replace '(?m)^(    "version":\s*)"[^"]+"', "`${1}`"$newVersion`""
Set-Content -Path $manifestPath -Value $manifest -NoNewline

# Build
Write-Host "Building release..." -ForegroundColor Cyan
& pixi run build
if ($LASTEXITCODE -ne 0) { throw "pixi run build failed (exit $LASTEXITCODE)" }

# Changelog from commits since the last tag. ArtifactPaths matches the
# release-notes filter in .github/workflows/release.yml.
$changelogPath = Join-Path $repoRoot 'CHANGELOG.md'
New-ChangelogFromCommits -ChangelogPath $changelogPath -Version $newVersion -ArtifactPaths @(
    'src/', 'camera_hook.asm', 'CMakeLists.txt', 'cameraunlock-core',
    'scripts/install.cmd', 'scripts/uninstall.cmd'
) | Out-Null

# Commit version bump + changelog. Message must start with "Release v" so
# build.yml's skip condition leaves this commit to release.yml.
foreach ($f in @($cmakePath, $pixiPath, $installCmdPath, $manifestPath, $changelogPath)) {
    git add $f
    if ($LASTEXITCODE -ne 0) { throw "git add failed for $f" }
}
$staged = git diff --cached --name-only
if (-not $staged) { throw "No changes were staged for the release commit." }
git commit -m "Release v$newVersion"
if ($LASTEXITCODE -ne 0) { throw "git commit failed" }

# Annotated tag + push (triggers .github/workflows/release.yml)
New-ReleaseTag -Version $newVersion -Message "Release v$newVersion" -Branch 'main'

Write-Host "Release v$newVersion pushed. CI release workflow triggered on tag." -ForegroundColor Green
