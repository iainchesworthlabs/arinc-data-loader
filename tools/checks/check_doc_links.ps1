#requires -Version 7
<#
.SYNOPSIS
    Checks that every relative link and #anchor in the repository's Markdown files resolves.

.DESCRIPTION
    Reads every tracked *.md file (outside deps/ and build trees), finds the inline links and images,
    reference-style definitions, and checks that
      * a relative path exists (a file or a directory), and
      * a #fragment names a heading of the target Markdown file, by GitHub's rules for the slug.
    Links to other sites (http, https, mailto) are not followed: the check needs no network.
    Text inside code fences and inline code is ignored.

    Exit status 0 when everything resolves, 1 with a list of the broken links otherwise.

.PARAMETER Root
    The repository root. Defaults to two levels above this script.
#>
[CmdletBinding()]
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path $Root).Path

function Get-MarkdownFiles {
    $tracked = $null
    if (Get-Command git -ErrorAction SilentlyContinue) {
        $tracked = & git -C $Root ls-files '*.md' 2>$null
        if ($LASTEXITCODE -ne 0) { $tracked = $null }
    }
    if ($tracked) {
        # Tracked files only, but also new ones that are not committed yet.
        $untracked = & git -C $Root ls-files --others --exclude-standard '*.md' 2>$null
        # A changelog fragment is written with the links of the CHANGELOG.md it will be assembled into
        # (tools/release/Changelog.ps1), which are relative to the repository root, not to changelog.d.
        return @($tracked) + @($untracked) | Where-Object { $_ -and -not $_.StartsWith('deps/') } |
            Where-Object { -not ($_.StartsWith('changelog.d/') -and $_ -ne 'changelog.d/README.md') } |
            ForEach-Object { Join-Path $Root $_ } | Where-Object { Test-Path $_ }
    }
    return Get-ChildItem -Path $Root -Recurse -Filter '*.md' -File |
        Where-Object { $_.FullName -notmatch '[\\/](deps|build|out|\.git)[\\/]' } |
        Where-Object { $_.FullName -notmatch '[\\/]changelog\.d[\\/](?!README\.md$)' } |
        ForEach-Object { $_.FullName }
}

# The text of a heading as GitHub makes its anchor: inline markup removed, lower case, anything
# that is not a letter, digit, underscore, hyphen or space dropped, spaces turned into hyphens.
function Get-Slug {
    param([string]$Heading)
    $text = $Heading
    $text = [regex]::Replace($text, '!?\[([^\]]*)\]\([^)]*\)', '$1')
    $text = [regex]::Replace($text, '<[^>]+>', '')
    $text = $text -replace '[`*]', ''
    $text = $text.Trim().ToLowerInvariant()
    $text = [regex]::Replace($text, '[^\p{L}\p{N}\p{M}_\- ]', '')
    return $text -replace ' ', '-'
}

$slugCache = @{}
function Get-Anchors {
    param([string]$File)
    if ($slugCache.ContainsKey($File)) { return $slugCache[$File] }
    $anchors = New-Object System.Collections.Generic.HashSet[string]
    $seen = @{}
    $inFence = $false
    foreach ($line in [System.IO.File]::ReadAllLines($File)) {
        if ($line -match '^\s*(```|~~~)') { $inFence = -not $inFence; continue }
        if ($inFence) { continue }
        if ($line -match '^\s{0,3}#{1,6}\s+(.*?)\s*#*\s*$') {
            $slug = Get-Slug $Matches[1]
            if ($seen.ContainsKey($slug)) {
                $n = $seen[$slug]
                $seen[$slug] = $n + 1
                [void]$anchors.Add("$slug-$n")
            } else {
                $seen[$slug] = 1
                [void]$anchors.Add($slug)
            }
        }
        # An explicit anchor: <a id="x"> or <a name="x">
        foreach ($m in [regex]::Matches($line, '<a\s+(?:id|name)="([^"]+)"')) { [void]$anchors.Add($m.Groups[1].Value) }
    }
    $slugCache[$File] = $anchors
    return $anchors
}

$problems = New-Object System.Collections.Generic.List[string]
$checked = 0

foreach ($file in Get-MarkdownFiles) {
    $relative = $file.Substring($Root.Length + 1).Replace('\', '/')
    $directory = Split-Path -Parent $file
    $inFence = $false
    $lineNumber = 0
    foreach ($line in [System.IO.File]::ReadAllLines($file)) {
        $lineNumber++
        if ($line -match '^\s*(```|~~~)') { $inFence = -not $inFence; continue }
        if ($inFence) { continue }
        $plain = [regex]::Replace($line, '`[^`]*`', '')
        $targets = @()
        foreach ($m in [regex]::Matches($plain, '!?\[(?:[^\]\[]|\[[^\]]*\])*\]\(\s*<?([^)\s>]+)>?(?:\s+"[^"]*")?\s*\)')) {
            $targets += $m.Groups[1].Value
        }
        if ($plain -match '^\s{0,3}\[[^\]]+\]:\s+(\S+)') { $targets += $Matches[1] }
        foreach ($target in $targets) {
            if ($target -match '^(https?:|mailto:|ftp:|tel:)' -or $target.StartsWith('//')) { continue }
            $checked++
            $path = $target
            $fragment = $null
            $hash = $target.IndexOf('#')
            if ($hash -ge 0) { $path = $target.Substring(0, $hash); $fragment = $target.Substring($hash + 1) }
            $path = [System.Uri]::UnescapeDataString($path)
            if ($path -eq '') {
                $resolved = $file
            } elseif ($path.StartsWith('/')) {
                $resolved = Join-Path $Root $path.TrimStart('/')
            } else {
                $resolved = Join-Path $directory $path
            }
            if (-not (Test-Path -LiteralPath $resolved)) {
                $problems.Add("${relative}:${lineNumber}: '$target' does not exist")
                continue
            }
            if ($fragment) {
                $item = Get-Item -LiteralPath $resolved
                if ($item.PSIsContainer -or $item.Extension -ne '.md') { continue }
                $anchors = Get-Anchors $item.FullName
                if (-not $anchors.Contains([System.Uri]::UnescapeDataString($fragment).ToLowerInvariant())) {
                    $problems.Add("${relative}:${lineNumber}: '$target': no heading gives the anchor '#$fragment'")
                }
            }
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host ("{0} broken link(s) among {1} checked" -f $problems.Count, $checked)
    exit 1
}
Write-Host ("All {0} relative links resolve" -f $checked)
exit 0
