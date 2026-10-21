#requires -Version 7
<#
.SYNOPSIS
    Checks that the repository paths the Markdown files name in backticks exist.

.DESCRIPTION
    check_doc_links.ps1 checks what a link points at. A path written in a code span is not a link,
    so nothing read it, and a page could go on naming a file for years after it moved. This script
    reads every Markdown file (the ones check_doc_links.ps1 reads, plus the changelog fragments) and
    takes each word of each inline code span that

      * contains a slash, and
      * starts with the name of something at the top of the repository (docs/, src/, tools/,
        .github/, CMakePresets.json/..., whatever the repository has there),

    to be a claim that the file or directory exists. A claim is checked against the list git gives
    for the repository (tracked files, and new files that are not ignored), not against the disk, so
    a path that exists only as a build output or an ignored file is reported on a developer's machine
    exactly as it is on CI. The comparison is case-sensitive, as GitHub's is.

    Not checked, because they are not claims about the tree:
      * fenced code blocks (shell transcripts, listings, file layouts);
      * a word with a placeholder or a pattern in it (<name>, *, ?, $, %, braces, brackets);
      * a path that does not start at the repository root (a bare file name, ../x, /usr/lib/x), and a
        first segment that is not in the repository at all (build/, a path on the target machine);
      * deps/, a submodule that a documentation job does not check out;
      * the paths and prefixes in the allow-list below, and the files in $UncheckedFiles.
    A trailing #fragment, :line number and punctuation are removed first.

    Exit status 0 when every claim holds, 1 with a list of the stale paths otherwise.

.PARAMETER Root
    The repository root. Defaults to two levels above this script.
#>
[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path $Root).Path

# ---------------------------------------------------------------------------------------------------
# The allow-list. Every entry is a deliberate exception and says why; a path that is simply stale is
# fixed in the page, not listed here. An exact path (a trailing slash does not matter) ...
$AllowedPaths = [ordered]@{
    # The standards: third-party documents that are kept out of the repository (.gitignore,
    # docs/specs/README.md). A developer who has them puts them in this folder.
    'docs/specs/615A-3.pdf'    = 'a standard that is not in the repository'
    'docs/specs/665-3.pdf'     = 'a standard that is not in the repository'
    # A folder the rules allow and nothing uses yet (check_platform_isolation.ps1 exempts it).
    'cmake/triplets'           = 'vcpkg triplets would go here; there are none yet'
    # A folder the roadmap proposes.
    'src/target_app'           = 'planned: the target-side program of docs/roadmap.md'
    # A file inside a package, not in the tree.
    'tools/adl_write_payloads' = 'inside the macOS disk image'
    # The name a new picture would have.
    'docs/images/name.png'     = 'a placeholder in docs/screenshots.md'
}

# ... or everything below a directory.
$AllowedPrefixes = [ordered]@{
    # The vcpkg submodule: a documentation job does not check it out, and its content is not ours.
    'deps/' = 'a submodule'
}

# Files that are not read for paths at all.
$UncheckedFiles = [ordered]@{
    # Released entries describe the tree as it was; they are history and are not edited to follow it.
    'CHANGELOG.md' = 'released entries are history'
}
# ---------------------------------------------------------------------------------------------------

# What is in the repository, by git's reckoning (tracked, and new but not ignored); without git, the
# files on disk outside the usual generated directories.
function Get-RepositoryFiles {
    if (Get-Command git -ErrorAction SilentlyContinue) {
        $listed = & git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard 2>$null
        if ($LASTEXITCODE -eq 0 -and $listed) { return @($listed) }
    }
    return Get-ChildItem -Path $Root -Recurse -File -Force |
        Where-Object { $_.FullName -notmatch '[\\/](deps|build|out|\.git)[\\/]' } |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\', '/') }
}

$files = Get-RepositoryFiles
$fileSet = [System.Collections.Generic.HashSet[string]]::new([string[]]$files, [System.StringComparer]::Ordinal)
$dirSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
# The names at the top of the repository, compared without regard to case, so that Docs/ci.md is a
# claim about docs/ci.md (which GitHub does not find) and not a path that is none of our business.
$topLevel = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($f in $files) {
    $parts = $f.Split('/')
    [void]$topLevel.Add($parts[0])
    for ($i = 1; $i -lt $parts.Count; $i++) { [void]$dirSet.Add(($parts[0..($i - 1)] -join '/')) }
}

function Get-MarkdownFiles {
    return $files | Where-Object { $_ -like '*.md' -and -not $_.StartsWith('deps/') } |
        Where-Object { -not $UncheckedFiles.Contains($_) }
}

# A word of a code span as a repository path, or $null when it is not a claim about one.
function ConvertTo-PathClaim {
    param([string]$Word)
    # Punctuation round the word. Not a leading dot: .github/ starts with one.
    $w = $Word.Trim().TrimStart('"', "'", '(').TrimEnd('"', "'", ')', ',', ';', ':', '.')
    # A line number or a fragment names a place in the file, not another file.
    $w = $w -replace '#.*$', ''
    $w = $w -replace '(:\d+)+$', ''
    if ($w.StartsWith('./')) { $w = $w.Substring(2) }
    if ($w -notmatch '/') { return $null }
    if ($w -match '[<>*?$%{}\[\]|=~\\@`]' -or $w -match '\.\.\.|…') { return $null }
    if ($w -match '^[A-Za-z][A-Za-z0-9+.-]*:') { return $null }
    $w = $w.TrimEnd('/')
    if (-not $w) { return $null }
    if (-not $topLevel.Contains($w.Split('/')[0])) { return $null }
    return $w
}

function Test-Allowed {
    param([string]$Claim)
    if ($AllowedPaths.Contains($Claim)) { return $true }
    foreach ($prefix in $AllowedPrefixes.Keys) {
        if (($Claim + '/').StartsWith($prefix)) { return $true }
    }
    return $false
}

$problems = New-Object System.Collections.Generic.List[string]
$checked = 0

foreach ($relative in Get-MarkdownFiles) {
    $path = Join-Path $Root $relative
    if (-not (Test-Path -LiteralPath $path)) { continue }
    $lines = [System.IO.File]::ReadAllLines($path)

    # Prose, a paragraph at a time, so that a code span that wraps over a line break is still one span.
    $paragraph = New-Object System.Text.StringBuilder
    $paragraphStart = 0
    $fence = $null
    $blocks = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($null -ne $fence) {
            # The closing fence: the same character, at least as long, nothing after it.
            if ($line -match ('^\s*' + [regex]::Escape($fence[0]) + '{' + $fence.Length + ',}\s*$')) { $fence = $null }
            continue
        }
        if ($line -match '^\s*(`{3,}|~{3,})') {
            if ($paragraph.Length -gt 0) { $blocks.Add(@($paragraphStart, $paragraph.ToString())); [void]$paragraph.Clear() }
            $fence = $Matches[1]
            continue
        }
        if ($line.Trim() -eq '') {
            if ($paragraph.Length -gt 0) { $blocks.Add(@($paragraphStart, $paragraph.ToString())); [void]$paragraph.Clear() }
            continue
        }
        if ($paragraph.Length -eq 0) { $paragraphStart = $i + 1 } else { [void]$paragraph.Append("`n") }
        [void]$paragraph.Append($line)
    }
    if ($paragraph.Length -gt 0) { $blocks.Add(@($paragraphStart, $paragraph.ToString())) }

    foreach ($block in $blocks) {
        $start = [int]$block[0]
        $text = [string]$block[1]
        foreach ($span in [regex]::Matches($text, '(`+)(.+?)(?<!`)\1(?!`)', 'Singleline')) {
            $lineNumber = $start + ($text.Substring(0, $span.Index).Split("`n").Count - 1)
            foreach ($word in ($span.Groups[2].Value -split '\s+')) {
                $claim = ConvertTo-PathClaim $word
                if (-not $claim -or (Test-Allowed $claim)) { continue }
                $checked++
                if (-not ($fileSet.Contains($claim) -or $dirSet.Contains($claim))) {
                    $problems.Add("${relative}:${lineNumber}: '$claim' is not in the repository")
                }
            }
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host ("{0} stale path(s) among {1} checked" -f $problems.Count, $checked)
    Write-Host 'Fix the page. If the path is not meant to exist (a placeholder, a build output, a file on the target machine), add it to the allow-list at the top of tools/checks/check_doc_paths.ps1 with the reason.'
    exit 1
}
Write-Host ("All {0} repository paths named in the Markdown files exist" -f $checked)
exit 0
