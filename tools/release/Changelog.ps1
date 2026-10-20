#!/usr/bin/env pwsh
#requires -Version 7.0
#
# The changelog tool (docs/releasing.md, CONTRIBUTING.md).
#
# A change that a person using the program would notice is recorded as a small
# file in changelog.d/, in the same pull request as the change, and not as a
# line in CHANGELOG.md: two pull requests that both add a line to the same
# section conflict, two that each add a file do not. At release time this tool
# assembles the files into one dated section of CHANGELOG.md, in the Keep a
# Changelog categories, and removes them.
#
# A fragment is changelog.d/<name>.<category>.md, where <category> is one of
#   added  changed  deprecated  removed  fixed  security
# and its text is the entry itself, without a leading "- ": one paragraph,
# wrapped as you like. changelog.d/summary.md, if present, is free text that
# opens the release section (the first release uses it to say what the project
# is); it has no category.
#
# Modes (-Mode):
#   Check        Every fragment is well formed, CHANGELOG.md is well formed, and
#                [Unreleased] holds no entries of its own.
#   Assemble     -Version <semver>: writes the release section, updates the
#                comparison links at the foot of the file, deletes the
#                fragments. -WhatIf shows what it would do.
#   Notes        -Version <semver>: prints the body of that release's section
#                (what a GitHub release shows), to stdout or to -OutFile.
#   PullRequest  -Base <ref>: the pull request adds a fragment, or carries the
#                no-changelog label (-Labels), or is a branch that does not
#                need one (-HeadRef release/*, hotfix/*, dependabot/*).
#
# Exit: 0 = fine, 1 = a rule is broken (each is reported file:line where it can
# be), 2 = bad invocation.

[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Check', 'Assemble', 'Notes', 'PullRequest')]
    [string]$Mode,

    [string]$Root = (Join-Path $PSScriptRoot '../..'),
    [string]$Version,
    [string]$Date = (Get-Date -Format 'yyyy-MM-dd'),
    [string]$Base,
    [string]$HeadRef = '',
    [string[]]$Labels = @(),
    [string]$OutFile,
    [string]$RepoUrl = 'https://github.com/iainchesworthlabs/arinc-data-loader',
    [string]$IntegrationBranch = 'develop'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = (Resolve-Path -LiteralPath $Root).Path
$fragmentDir = Join-Path $Root 'changelog.d'
$changelogPath = Join-Path $Root 'CHANGELOG.md'

# Keep a Changelog's categories, in the order a section lists them.
$categories = [ordered]@{
    added      = 'Added'
    changed    = 'Changed'
    deprecated = 'Deprecated'
    removed    = 'Removed'
    fixed      = 'Fixed'
    security   = 'Security'
}

$optOutLabel = 'no-changelog'
# Branches whose pull requests do not carry a fragment: a release branch is where the
# fragments are consumed, and a dependency update is not news to a user of the program.
$exemptHeadPattern = '^(release|hotfix|dependabot)/'

# Semantic Versioning 2.0.0, section 2 (the grammar's own regular expression).
$semverPattern = '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$'
$fragmentNamePattern = '^[a-z0-9][a-z0-9._-]*\.(?<category>[a-z]+)\.md$'

$problems = [System.Collections.Generic.List[string]]::new()
function Add-Problem([string]$Text) { $script:problems.Add($Text) }

function Read-Lf([string]$Path) {
    return ((Get-Content -LiteralPath $Path -Raw -Encoding utf8) -replace "`r`n", "`n")
}

function Write-Lf([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, ($Text -replace "`r`n", "`n"), [System.Text.UTF8Encoding]::new($false))
}

function Get-RelativePath([string]$Path) {
    return [System.IO.Path]::GetRelativePath($Root, $Path).Replace('\', '/')
}

# --- fragments ---------------------------------------------------------------

# The files in changelog.d that are fragments, each with its category and text.
# README.md is the directory's own page and is not one.
function Get-Fragments {
    $result = [System.Collections.Generic.List[object]]::new()
    if (-not (Test-Path -LiteralPath $fragmentDir)) { return , $result.ToArray() }
    $files = Get-ChildItem -LiteralPath $fragmentDir -File | Sort-Object { $_.Name }
    foreach ($file in $files) {
        if ($file.Name -ieq 'README.md' -or $file.Name -eq '.gitkeep') { continue }
        $relative = Get-RelativePath $file.FullName
        if ($file.Name -ceq 'summary.md') {
            $text = (Read-Lf $file.FullName).Trim()
            if (-not $text) { Add-Problem "${relative}: the summary is empty; delete the file or write it" }
            $result.Add([pscustomobject]@{ Name = $file.Name; Path = $relative; Category = $null; Text = $text })
            continue
        }
        $match = [regex]::Match($file.Name, $fragmentNamePattern)
        if (-not $match.Success -or -not $categories.Contains($match.Groups['category'].Value)) {
            Add-Problem ("${relative}: a fragment is named <name>.<category>.md, with a lower-case name and a category of " +
                (($categories.Keys) -join ', ') + ' (or summary.md)')
            continue
        }
        $text = (Read-Lf $file.FullName).Trim()
        if (-not $text) {
            Add-Problem "${relative}: the fragment is empty"
            continue
        }
        if ($text -match '^(?:[-*+]\s|#)') {
            Add-Problem "${relative}:1: write the entry's text only: no leading '- ' and no heading; the tool adds them"
        }
        if ($text -match '(?m)^\s*$') {
            Add-Problem "${relative}: a fragment is one paragraph (one entry); put a second entry in a second file"
        }
        $result.Add([pscustomobject]@{
            Name = $file.Name; Path = $relative; Category = $match.Groups['category'].Value; Text = $text })
    }
    return , $result.ToArray()
}

# One fragment as a list entry: "- " then the text, continuation lines indented under it.
function Format-Entry([string]$Text) {
    $lines = $Text -split "`n" | ForEach-Object { $_.TrimEnd() }
    $first = $true
    $out = foreach ($line in $lines) {
        if ($first) { $first = $false; "- $line" } else { "  $line" }
    }
    return ($out -join "`n")
}

# --- CHANGELOG.md ------------------------------------------------------------

# The file split into: everything before the [Unreleased] heading, the [Unreleased] body, the
# release sections (each starting at its "## [" heading) and the link definitions at the foot.
function Read-Changelog {
    if (-not (Test-Path -LiteralPath $changelogPath)) {
        Add-Problem 'CHANGELOG.md does not exist'
        return $null
    }
    $text = Read-Lf $changelogPath
    $lines = $text -split "`n"
    $headingAt = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^## \[') { $headingAt += $i }
    }
    $unreleasedAt = -1
    foreach ($i in $headingAt) {
        if ($lines[$i] -match '^## \[Unreleased\]\s*$') { $unreleasedAt = $i; break }
    }
    if ($unreleasedAt -lt 0) {
        Add-Problem 'CHANGELOG.md: no "## [Unreleased]" heading'
        return $null
    }

    # Link definitions: "[Unreleased]: https://..." lines at the end.
    $linkStart = $lines.Count
    while ($linkStart -gt 0 -and ($lines[$linkStart - 1] -match '^\[[^\]]+\]:\s+\S+\s*$' -or $lines[$linkStart - 1] -eq '')) {
        $linkStart--
    }
    $links = [ordered]@{}
    for ($i = $linkStart; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\[([^\]]+)\]:\s+(\S+)\s*$') { $links[$Matches[1]] = $Matches[2] }
    }

    $sections = [System.Collections.Generic.List[object]]::new()
    $after = @($headingAt | Where-Object { $_ -gt $unreleasedAt })
    for ($k = 0; $k -lt $after.Count; $k++) {
        $start = $after[$k]
        $end = if ($k + 1 -lt $after.Count) { $after[$k + 1] } else { $linkStart }
        $heading = $lines[$start]
        if ($heading -notmatch '^## \[(?<version>[^\]]+)\] - (?<date>\d{4}-\d{2}-\d{2})(?<tail>.*)$') {
            Add-Problem ("CHANGELOG.md:$($start + 1): a release heading is '## [<version>] - <yyyy-MM-dd>', not '$heading'")
            continue
        }
        $sectionVersion = $Matches['version']
        $sectionDate = $Matches['date']
        if ($sectionVersion -notmatch $semverPattern) {
            Add-Problem "CHANGELOG.md:$($start + 1): '$sectionVersion' is not a semantic version"
        }
        $sections.Add([pscustomobject]@{
            Version = $sectionVersion; Date = $sectionDate; Line = $start + 1
            Lines = $lines[$start..($end - 1)] })
    }
    $unreleasedEnd = if ($after.Count -gt 0) { $after[0] } else { $linkStart }
    return [pscustomobject]@{
        Head           = if ($unreleasedAt -gt 0) { $lines[0..($unreleasedAt - 1)] } else { @() }
        UnreleasedLine = $unreleasedAt + 1
        Unreleased     = $lines[($unreleasedAt + 1)..($unreleasedEnd - 1)]
        Sections       = $sections.ToArray()
        Links          = $links
    }
}

function Test-ChangelogShape($Changelog) {
    if ($null -eq $Changelog) { return }
    # The entries of the next release are fragments; a hand-written one under [Unreleased] would be
    # missed by the assembly (and lost by a careless merge), so none is allowed.
    $body = $Changelog.Unreleased
    for ($i = 0; $i -lt $body.Count; $i++) {
        if ($body[$i] -match '^(### |- |\* )') {
            Add-Problem ("CHANGELOG.md:$($Changelog.UnreleasedLine + 1 + $i): [Unreleased] holds no entries of its own; " +
                'add a fragment to changelog.d instead (CONTRIBUTING.md)')
            break
        }
    }
    $previous = $null
    foreach ($section in $Changelog.Sections) {
        if ($null -ne $previous -and [datetime]$section.Date -gt [datetime]$previous.Date) {
            Add-Problem "CHANGELOG.md:$($section.Line): $($section.Version) is dated after $($previous.Version), which is listed above it"
        }
        $previous = $section
    }
    $seen = @{}
    foreach ($section in $Changelog.Sections) {
        if ($seen.ContainsKey($section.Version)) { Add-Problem "CHANGELOG.md:$($section.Line): $($section.Version) is listed twice" }
        $seen[$section.Version] = $true
    }
}

function Exit-WithProblems {
    if ($problems.Count -eq 0) { return }
    foreach ($problem in $problems) {
        if ($env:GITHUB_ACTIONS -eq 'true') { Write-Host "::error::$problem" } else { [Console]::Error.WriteLine($problem) }
    }
    exit 1
}

# --- modes ---------------------------------------------------------------------

function Invoke-Check {
    $fragments = Get-Fragments
    $changelog = Read-Changelog
    Test-ChangelogShape $changelog
    Exit-WithProblems
    Write-Host "changelog: $(@($fragments).Count) fragment(s), $(@($changelog.Sections).Count) release section(s): ok"
}

function Get-ReleaseSection([string]$Wanted) {
    $changelog = Read-Changelog
    if ($null -eq $changelog) { Exit-WithProblems }
    $section = @($changelog.Sections | Where-Object { $_.Version -ceq $Wanted })
    if ($section.Count -ne 1) {
        Add-Problem "CHANGELOG.md has no section for $Wanted"
        Exit-WithProblems
    }
    return $section[0]
}

function Invoke-Notes {
    if (-not $Version -or $Version -notmatch $semverPattern) {
        [Console]::Error.WriteLine('Notes needs -Version <semver>')
        exit 2
    }
    $section = Get-ReleaseSection $Version
    # The body is everything after the heading, trimmed.
    $body = (($section.Lines | Select-Object -Skip 1) -join "`n").Trim() + "`n"
    if ($OutFile) {
        Write-Lf $OutFile $body
        Write-Host "release notes for $Version written to $OutFile"
    } else {
        [Console]::Out.Write($body)
    }
}

function Invoke-Assemble {
    if (-not $Version -or $Version -notmatch $semverPattern) {
        [Console]::Error.WriteLine('Assemble needs -Version <semver>, e.g. 0.1.0 or 0.2.0-rc.1')
        exit 2
    }
    if ($Date -notmatch '^\d{4}-\d{2}-\d{2}$') {
        [Console]::Error.WriteLine("-Date is yyyy-MM-dd, not '$Date'")
        exit 2
    }
    $fragments = Get-Fragments
    $changelog = Read-Changelog
    Test-ChangelogShape $changelog
    if ($null -ne $changelog -and @($changelog.Sections | Where-Object { $_.Version -ceq $Version }).Count -gt 0) {
        Add-Problem "CHANGELOG.md already has a section for $Version"
    }
    $entries = @($fragments | Where-Object { $null -ne $_.Category })
    if ($entries.Count -eq 0) { Add-Problem 'changelog.d holds no fragments, so there is nothing to release' }
    Exit-WithProblems

    $out = [System.Collections.Generic.List[string]]::new()
    $out.Add("## [$Version] - $Date")
    $out.Add('')
    $summary = @($fragments | Where-Object { $null -eq $_.Category })
    if ($summary.Count -gt 0) {
        $out.Add($summary[0].Text)
        $out.Add('')
    }
    foreach ($key in $categories.Keys) {
        $inCategory = @($entries | Where-Object { $_.Category -ceq $key })
        if ($inCategory.Count -eq 0) { continue }
        $out.Add("### $($categories[$key])")
        $out.Add('')
        foreach ($entry in $inCategory) { $out.Add((Format-Entry $entry.Text)) }
        $out.Add('')
    }

    # The comparison links: the new version against the one before it (or its own tag, for the
    # first release), and [Unreleased] against the new version.
    $links = [ordered]@{}
    foreach ($key in $changelog.Links.Keys) { $links[$key] = $changelog.Links[$key] }
    $previousVersion = if ($changelog.Sections.Count -gt 0) { $changelog.Sections[0].Version } else { $null }
    $links['Unreleased'] = "$RepoUrl/compare/v$Version...$IntegrationBranch"
    $links[$Version] = if ($previousVersion) { "$RepoUrl/compare/v$previousVersion...v$Version" } else { "$RepoUrl/releases/tag/v$Version" }

    $new = [System.Collections.Generic.List[string]]::new()
    $new.AddRange([string[]]$changelog.Head)
    $new.Add('## [Unreleased]')
    $new.AddRange([string[]]$changelog.Unreleased)
    if ($new[$new.Count - 1] -ne '') { $new.Add('') }
    $new.AddRange($out)
    foreach ($section in $changelog.Sections) {
        $body = [System.Collections.Generic.List[string]]::new([string[]]$section.Lines)
        while ($body.Count -gt 0 -and $body[$body.Count - 1] -eq '') { $body.RemoveAt($body.Count - 1) }
        $new.AddRange($body)
        $new.Add('')
    }
    # Unreleased first, then versions newest first, the order of the sections themselves.
    $ordered = [System.Collections.Generic.List[string]]::new()
    $ordered.Add("[Unreleased]: $($links['Unreleased'])")
    $sectionOrder = @($Version) + @($changelog.Sections | ForEach-Object { $_.Version })
    foreach ($v in $sectionOrder) {
        if ($links.Contains($v)) { $ordered.Add("[$v]: $($links[$v])") }
    }
    $new.AddRange($ordered)
    $text = (($new -join "`n").TrimEnd() + "`n")

    if ($PSCmdlet.ShouldProcess('CHANGELOG.md', "write the section for $Version from $($entries.Count) fragment(s)")) {
        Write-Lf $changelogPath $text
        foreach ($fragment in $fragments) {
            Remove-Item -LiteralPath (Join-Path $fragmentDir $fragment.Name)
        }
        Write-Host "CHANGELOG.md: $Version assembled from $($entries.Count) fragment(s); fragments removed"
    }
}

function Invoke-PullRequest {
    if (-not $Base) {
        [Console]::Error.WriteLine('PullRequest needs -Base <ref>, the branch the pull request targets')
        exit 2
    }
    # Whatever else is wrong with the fragments is reported first.
    $fragments = Get-Fragments
    Exit-WithProblems

    if ($HeadRef -match $exemptHeadPattern) {
        Write-Host "changelog: '$HeadRef' does not need a fragment"
        return
    }
    # A -File invocation passes one comma-separated string; a call from PowerShell passes a list.
    $labelNames = @($Labels | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($labelNames -contains $optOutLabel) {
        Write-Host "changelog: opted out with the '$optOutLabel' label"
        return
    }
    $previousPreference = $PSNativeCommandUseErrorActionPreference
    try {
        $PSNativeCommandUseErrorActionPreference = $false
        # Files this pull request adds (or renames in) under changelog.d, against the merge base.
        $added = @(git -C $Root diff --name-only --diff-filter=AR "$Base...HEAD" -- changelog.d 2>&1)
        if ($LASTEXITCODE -ne 0) {
            [Console]::Error.WriteLine("git diff $Base...HEAD failed: $($added -join ' ')")
            exit 2
        }
    } finally {
        $PSNativeCommandUseErrorActionPreference = $previousPreference
    }
    $names = $added | ForEach-Object { Split-Path $_ -Leaf }
    $valid = @($fragments | Where-Object { $null -ne $_.Category -and $names -contains $_.Name })
    if ($valid.Count -gt 0) {
        Write-Host "changelog: the pull request adds $($valid.Count) fragment(s): $(($valid | ForEach-Object { $_.Name }) -join ', ')"
        return
    }
    Add-Problem ("This pull request adds no changelog fragment. Add changelog.d/<name>.<category>.md " +
        "(category: $(($categories.Keys) -join ', ')) describing the change for a user of the program, " +
        "or, if nobody using it would notice (a test, a refactor, CI), give the pull request the '$optOutLabel' label. " +
        'See CONTRIBUTING.md.')
    Exit-WithProblems
}

switch ($Mode) {
    'Check' { Invoke-Check }
    'Assemble' { Invoke-Assemble }
    'Notes' { Invoke-Notes }
    'PullRequest' { Invoke-PullRequest }
}
