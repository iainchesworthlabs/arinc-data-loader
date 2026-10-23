#!/usr/bin/env pwsh
# SPDX-License-Identifier: GPL-3.0-or-later
#requires -Version 7.0
#
# Works out what a run of the release workflow is releasing, and whether it may publish, from the event
# that started it and the files at the commit it runs on; stops the run, with the reason, when the tag
# does not match them. (docs/releasing.md describes the rules in prose.)
#
#   Get-ReleaseInfo.ps1 -EventName <push|pull_request|workflow_dispatch> -RefType <tag|branch> `
#       -RefName <name> -Sign <true|false> [-GitHubOutput <file>]
#
# A tag run (a push of a tag vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE) is a real release:
#   - the tag's version is a Semantic Version whose MAJOR.MINOR.PATCH is the project version in
#     CMakeLists.txt, which vcpkg.json repeats;
#   - the tagged commit is on main (gitflow: a release branch is merged to main, and main is tagged);
#   - CHANGELOG.md has a dated section for the version, and changelog.d holds no fragment any more
#     (the release branch assembled them).
# Every other run (a release/* branch, a manual dispatch, a pull request) is a rehearsal: it builds,
# tests, packages and (on a release branch or a dispatch) signs, and publishes nothing.
#
# Outputs (and, as a table, in the step summary): version, tag, release, sign, prerelease.

[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$EventName,
    [Parameter(Mandatory)] [string]$RefType,
    [Parameter(Mandatory)] [string]$RefName,
    [string]$Sign = 'false',
    [string]$Root = (Join-Path $PSScriptRoot '../..'),
    [string]$GitHubOutput
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path -LiteralPath $Root).Path

function Stop-Run([string]$Message) {
    if ($env:GITHUB_ACTIONS -eq 'true') { Write-Host "::error::$Message" } else { [Console]::Error.WriteLine($Message) }
    exit 1
}

# --- the project version, as CMake and vcpkg have it ---------------------------------------------
$cmake = Get-Content -LiteralPath (Join-Path $Root 'CMakeLists.txt') -Raw
$match = [regex]::Match($cmake, 'project\(\s*ArincDataLoader\s+VERSION\s+(\d+\.\d+\.\d+)')
if (-not $match.Success) { Stop-Run 'CMakeLists.txt: no project(ArincDataLoader VERSION x.y.z)' }
$projectVersion = $match.Groups[1].Value
$vcpkgVersion = (Get-Content -LiteralPath (Join-Path $Root 'vcpkg.json') -Raw | ConvertFrom-Json).version
if ($vcpkgVersion -ne $projectVersion) {
    Stop-Run "vcpkg.json says version $vcpkgVersion, CMakeLists.txt says $projectVersion; they are changed together"
}

$semverPattern = '^v(?<core>(?<major>0|[1-9]\d*)\.(?<minor>0|[1-9]\d*)\.(?<patch>0|[1-9]\d*))(?:-(?<pre>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?$'

$release = $false
$tag = ''
$version = $projectVersion
$prerelease = $false
$signRun = $false

if ($EventName -eq 'push' -and $RefType -eq 'tag') {
    $release = $true
    $tag = $RefName
    $parsed = [regex]::Match($tag, $semverPattern)
    if (-not $parsed.Success) { Stop-Run "the tag '$tag' is not vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE (Semantic Versioning, with the v)" }
    if ($parsed.Groups['core'].Value -ne $projectVersion) {
        Stop-Run "the tag '$tag' is for $($parsed.Groups['core'].Value), but CMakeLists.txt and vcpkg.json say $projectVersion; the release branch changes them before it is merged"
    }
    $version = $tag.Substring(1)
    # A pre-release tag, or any 0.x version, is published as a pre-release.
    $prerelease = $parsed.Groups['pre'].Success -or $parsed.Groups['major'].Value -eq '0'
    $signRun = $true

    # The tagged commit is on main.
    git -C $Root rev-parse --verify --quiet refs/remotes/origin/main 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Run 'origin/main is not in this checkout (the release workflow needs the whole history)' }
    git -C $Root merge-base --is-ancestor HEAD refs/remotes/origin/main
    if ($LASTEXITCODE -ne 0) { Stop-Run "the commit tagged $tag is not on main; gitflow releases are merged to main first, and main is tagged" }

    # The changelog was assembled for this version, and nothing is left to assemble.
    & (Join-Path $PSScriptRoot 'Changelog.ps1') -Mode Notes -Version $version -Root $Root -OutFile (Join-Path ([System.IO.Path]::GetTempPath()) 'notes-check.md') | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Run "CHANGELOG.md has no section for $version; run tools/release/Changelog.ps1 -Mode Assemble on the release branch" }
    $left = @(Get-ChildItem -LiteralPath (Join-Path $Root 'changelog.d') -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'README.md' })
    if ($left.Count -gt 0) { Stop-Run "changelog.d still holds $($left.Count) fragment(s) ($($left[0].Name), ...); they belong in CHANGELOG.md for $version" }
} else {
    # A rehearsal. It signs on a release branch and when a dispatch asks to; never on a pull request,
    # which has no secrets and must not be able to ask for them.
    if ($EventName -eq 'push' -and $RefType -eq 'branch') { $signRun = $true }
    if ($EventName -eq 'workflow_dispatch') { $signRun = ($Sign -eq 'true') }
    if ($EventName -eq 'pull_request') { $signRun = $false }
    if ($RefType -eq 'branch' -and $RefName -match '^release/(?<named>\d+\.\d+\.\d+)') {
        if ($Matches['named'] -ne $projectVersion) {
            Write-Host "::warning::the branch $RefName is for $($Matches['named']) but the project version is $projectVersion"
        }
    }
}

$values = [ordered]@{
    version    = $version
    tag        = $tag
    release    = $release.ToString().ToLowerInvariant()
    sign       = $signRun.ToString().ToLowerInvariant()
    prerelease = $prerelease.ToString().ToLowerInvariant()
}
foreach ($key in $values.Keys) { Write-Host ("{0,-11} {1}" -f $key, $values[$key]) }
if ($GitHubOutput) {
    foreach ($key in $values.Keys) { Add-Content -LiteralPath $GitHubOutput -Value "$key=$($values[$key])" }
}
if ($env:GITHUB_STEP_SUMMARY) {
    $kind = if ($release) { 'a release' } else { 'a rehearsal (nothing is published)' }
    Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value "### Release run: $kind`n`n| | |`n|---|---|`n$(($values.Keys | ForEach-Object { "| $_ | ``$($values[$_])`` |" }) -join "`n")`n"
}
