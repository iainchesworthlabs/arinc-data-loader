#!/usr/bin/env pwsh
# SPDX-License-Identifier: GPL-3.0-or-later
#requires -Version 7.0
#
# Creates (or updates) the two environments the release workflow uses and, if asked, stores the signing
# key in the first (docs/releasing.md, "One-time setup"):
#
#   release-signing   holds RELEASE_GPG_PRIVATE_KEY and RELEASE_GPG_PASSPHRASE; GitHub gives it only to
#                     jobs on release/* branches and on v* tags. No reviewer.
#   release           holds nothing; gates the job that drafts the GitHub release. Required reviewer:
#                     the user named by -Reviewer (default: the signed-in user); v* tags only.
#
# A dry run by default, like apply_rulesets.ps1: it prints what it would send and sends nothing. The
# key and passphrase are read from files and given to `gh secret set` on its standard input, never as
# an argument and never printed.
#
#   ./tools/repo/apply_release_environments.ps1                      # dry run
#   ./tools/repo/apply_release_environments.ps1 -Apply               # create the environments
#   ./tools/repo/apply_release_environments.ps1 -Apply -KeyFile key.asc -PassphraseFile pass.txt
#
# Needs the GitHub CLI signed in as a repository admin. Exit: 0 = done (or dry run), 1 = a call failed.

[CmdletBinding()]
param(
    [switch]$Apply,
    [string]$Repo = 'iainchesworthlabs/arinc-data-loader',
    [string]$Reviewer,
    [string]$KeyFile,
    [string]$PassphraseFile
)

$ErrorActionPreference = 'Stop'

function Invoke-Gh([string[]]$Arguments, [string]$Body) {
    if ($Body) { $output = $Body | & gh @Arguments 2>&1 } else { $output = & gh @Arguments 2>&1 }
    if ($LASTEXITCODE -ne 0) { throw "gh $($Arguments -join ' ') failed: $output" }
    return $output
}

if (-not $Reviewer) { $Reviewer = (Invoke-Gh @('api', 'user', '--jq', '.login')) -join '' }
$reviewerId = [int]((Invoke-Gh @('api', "users/$Reviewer", '--jq', '.id')) -join '')

$environments = @(
    @{ Name = 'release-signing'; Reviewers = @(); Policies = @(@{ name = 'release/*'; type = 'branch' }, @{ name = 'v*'; type = 'tag' }) },
    @{ Name = 'release'; Reviewers = @(@{ type = 'User'; id = $reviewerId }); Policies = @(@{ name = 'v*'; type = 'tag' }) }
)

foreach ($environment in $environments) {
    $body = @{
        deployment_branch_policy = @{ protected_branches = $false; custom_branch_policies = $true }
        reviewers                = @($environment.Reviewers)
        prevent_self_review      = $false   # a solo maintainer approves their own release
    } | ConvertTo-Json -Depth 5
    Write-Host "environment $($environment.Name): reviewers=$(@($environment.Reviewers).Count), refs=$(($environment.Policies | ForEach-Object { "$($_.type) $($_.name)" }) -join ', ')"
    if (-not $Apply) { continue }
    Invoke-Gh @('api', '--method', 'PUT', "repos/$Repo/environments/$($environment.Name)", '--input', '-') $body | Out-Null
    $existing = (Invoke-Gh @('api', "repos/$Repo/environments/$($environment.Name)/deployment-branch-policies", '--jq', '.branch_policies[] | "\(.type) \(.name)"')) -split "`n"
    foreach ($policy in $environment.Policies) {
        if ($existing -contains "$($policy.type) $($policy.name)") { continue }
        Invoke-Gh @('api', '--method', 'POST', "repos/$Repo/environments/$($environment.Name)/deployment-branch-policies",
            '-f', "name=$($policy.name)", '-f', "type=$($policy.type)") | Out-Null
    }
}

if ($KeyFile -or $PassphraseFile) {
    if (-not ($KeyFile -and $PassphraseFile)) { throw 'give both -KeyFile and -PassphraseFile' }
    Write-Host "secrets RELEASE_GPG_PRIVATE_KEY (from $KeyFile) and RELEASE_GPG_PASSPHRASE (from $PassphraseFile) in release-signing"
    if ($Apply) {
        Invoke-Gh @('secret', 'set', 'RELEASE_GPG_PRIVATE_KEY', '--repo', $Repo, '--env', 'release-signing') (Get-Content -LiteralPath $KeyFile -Raw) | Out-Null
        Invoke-Gh @('secret', 'set', 'RELEASE_GPG_PASSPHRASE', '--repo', $Repo, '--env', 'release-signing') ((Get-Content -LiteralPath $PassphraseFile -Raw).TrimEnd("`r", "`n")) | Out-Null
    }
}

if (-not $Apply) { Write-Host "`nDry run: nothing was sent. Add -Apply to create them." }
