#!/usr/bin/env pwsh
#requires -Version 7.0
#
# Self-test for Changelog.ps1: the tool that decides what a release says, and whether a pull
# request may merge without a fragment, must fail what it should fail. Each case builds a small
# repository in a temporary directory.
#
# Usage:  ./tools/release/test_changelog.ps1
# Exit:   0 = all cases behaved, 1 = at least one did not.

$ErrorActionPreference = 'Stop'

$tool = Join-Path $PSScriptRoot 'Changelog.ps1'
$pwsh = (Get-Process -Id $PID).Path
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("adl-changelog-test-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null

$failures = [System.Collections.Generic.List[string]]::new()
$count = 0

function Invoke-Tool([string]$Root, [string[]]$Arguments) {
    $output = & $pwsh -NoProfile -NonInteractive -File $tool -Root $Root @Arguments 2>&1
    return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = ($output -join "`n") }
}

function Test-Case([string]$Name, [int]$Expected, $Result, [string]$MustContain) {
    $script:count++
    $ok = $Result.ExitCode -eq $Expected -and (-not $MustContain -or $Result.Output.Contains($MustContain))
    if ($ok) {
        Write-Host ("  ok    {0}" -f $Name)
    } else {
        Write-Host ("  FAIL  {0}: expected exit {1} containing '{2}', got {3}" -f $Name, $Expected, $MustContain, $Result.ExitCode) -ForegroundColor Red
        Write-Host $Result.Output
        $script:failures.Add($Name)
    }
}

function New-Tree([string]$Name) {
    $root = Join-Path $work $Name
    New-Item -ItemType Directory -Path (Join-Path $root 'changelog.d') -Force | Out-Null
    @'
# Changelog

Intro.

## [Unreleased]

Pointer text.

'@ | Set-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -NoNewline
    return $root
}

function Set-Fragment([string]$Root, [string]$Name, [string]$Text) {
    Set-Content -LiteralPath (Join-Path $Root "changelog.d/$Name") -Value $Text -NoNewline
}

try {
    # --- Check ---------------------------------------------------------------
    $root = New-Tree 'check-ok'
    Set-Fragment $root 'one.added.md' 'A new thing.'
    Set-Fragment $root 'two.fixed.md' "A fixed thing,`nover two lines."
    Test-Case 'a well formed tree passes' 0 (Invoke-Tool $root @('-Mode', 'Check'))

    $root = New-Tree 'check-bad-category'
    Set-Fragment $root 'one.improved.md' 'A thing.'
    Test-Case 'an unknown category fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'one.improved.md'

    $root = New-Tree 'check-no-category'
    Set-Fragment $root 'one.md' 'A thing.'
    Test-Case 'a name without a category fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'one.md'

    $root = New-Tree 'check-upper'
    Set-Fragment $root 'One.added.md' 'A thing.'
    Test-Case 'an upper-case name fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'One.added.md'

    $root = New-Tree 'check-empty'
    Set-Fragment $root 'one.added.md' "  `n"
    Test-Case 'an empty fragment fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'empty'

    $root = New-Tree 'check-bullet'
    Set-Fragment $root 'one.added.md' '- A thing.'
    Test-Case 'a leading bullet fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) "leading '- '"

    $root = New-Tree 'check-two-paragraphs'
    Set-Fragment $root 'one.added.md' "First.`n`nSecond."
    Test-Case 'two paragraphs fail' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'one paragraph'

    $root = New-Tree 'check-unreleased-entry'
    Add-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -Value "### Added`n`n- By hand.`n"
    Test-Case 'a hand-written entry under Unreleased fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'holds no entries'

    $root = New-Tree 'check-no-unreleased'
    Set-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -Value "# Changelog`n"
    Test-Case 'a changelog without Unreleased fails' 1 (Invoke-Tool $root @('-Mode', 'Check')) 'Unreleased'

    # --- Assemble and Notes --------------------------------------------------
    $root = New-Tree 'assemble'
    Set-Fragment $root 'summary.md' 'The summary.'
    Set-Fragment $root 'b.fixed.md' 'Fixed B.'
    Set-Fragment $root 'a.added.md' "Added A,`nwrapped."
    Set-Fragment $root 'c.security.md' 'Secured C.'
    Set-Content -LiteralPath (Join-Path $root 'changelog.d/README.md') -Value 'readme'
    $result = Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '0.1.0', '-Date', '2026-01-02')
    Test-Case 'assembling the first release works' 0 $result
    $text = (Get-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -Raw) -replace "`r`n", "`n"
    $script:count++
    $expected = @'
## [0.1.0] - 2026-01-02

The summary.

### Added

- Added A,
  wrapped.

### Fixed

- Fixed B.

### Security

- Secured C.

[Unreleased]: https://github.com/iainchesworthlabs/arinc-data-loader/compare/v0.1.0...develop
[0.1.0]: https://github.com/iainchesworthlabs/arinc-data-loader/releases/tag/v0.1.0
'@
    $expectedLf = $expected -replace "`r`n", "`n"
    if ($text.Contains("Pointer text.`n`n" + $expectedLf.Split("`n")[0]) -and $text.Contains($expectedLf.TrimEnd())) {
        Write-Host '  ok    the section lists the categories in order, with the links'
    } else {
        Write-Host '  FAIL  the assembled section is not what was expected' -ForegroundColor Red
        Write-Host $text
        $failures.Add('assembled section')
    }
    $left = @(Get-ChildItem -LiteralPath (Join-Path $root 'changelog.d') -Name)
    $script:count++
    if ($left.Count -eq 1 -and $left[0] -eq 'README.md') {
        Write-Host '  ok    the fragments are removed and the README stays'
    } else {
        Write-Host "  FAIL  changelog.d still holds: $($left -join ', ')" -ForegroundColor Red
        $failures.Add('fragments removed')
    }
    $notes = Invoke-Tool $root @('-Mode', 'Notes', '-Version', '0.1.0')
    Test-Case 'the notes carry the section body' 0 $notes '- Fixed B.'
    $script:count++
    if ($notes.Output -notmatch '## \[0\.1\.0\]') {
        Write-Host '  ok    the notes leave out the heading'
    } else {
        Write-Host '  FAIL  the notes repeat the heading' -ForegroundColor Red
        $failures.Add('notes heading')
    }
    Test-Case 'notes for a missing version fail' 1 (Invoke-Tool $root @('-Mode', 'Notes', '-Version', '9.9.9')) 'no section'

    Set-Fragment $root 'd.changed.md' 'Changed D.'
    Test-Case 'a second release works' 0 (Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '0.2.0-rc.1', '-Date', '2026-02-03'))
    $text = (Get-Content -LiteralPath (Join-Path $root 'CHANGELOG.md') -Raw) -replace "`r`n", "`n"
    $script:count++
    if ($text -match '(?s)## \[0\.2\.0-rc\.1\] - 2026-02-03.*## \[0\.1\.0\] - 2026-01-02' -and
        $text.Contains('[0.2.0-rc.1]: https://github.com/iainchesworthlabs/arinc-data-loader/compare/v0.1.0...v0.2.0-rc.1') -and
        $text.Contains('[Unreleased]: https://github.com/iainchesworthlabs/arinc-data-loader/compare/v0.2.0-rc.1...develop')) {
        Write-Host '  ok    the newer release goes first, with comparison links'
    } else {
        Write-Host '  FAIL  second release layout' -ForegroundColor Red
        Write-Host $text
        $failures.Add('second release')
    }
    Test-Case 'a release listed twice is refused' 1 (Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '0.1.0')) 'no fragments'

    $root = New-Tree 'assemble-empty'
    Test-Case 'assembling with no fragments fails' 1 (Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '1.0.0')) 'nothing to release'
    Test-Case 'a non-semantic version is refused' 2 (Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '1.0'))

    $root = New-Tree 'assemble-whatif'
    Set-Fragment $root 'a.added.md' 'A.'
    Test-Case 'WhatIf writes nothing' 0 (Invoke-Tool $root @('-Mode', 'Assemble', '-Version', '1.0.0', '-WhatIf'))
    $script:count++
    if (Test-Path -LiteralPath (Join-Path $root 'changelog.d/a.added.md')) {
        Write-Host '  ok    WhatIf leaves the fragments'
    } else {
        Write-Host '  FAIL  WhatIf removed a fragment' -ForegroundColor Red
        $failures.Add('whatif')
    }

    # --- PullRequest ---------------------------------------------------------
    $root = New-Tree 'pr'
    git -C $root init --quiet --initial-branch=develop
    git -C $root config user.name 'Test'
    git -C $root config user.email 'test@example.invalid'
    git -C $root add -A
    git -C $root commit --quiet -m 'base'
    git -C $root checkout --quiet -b feature/x
    Test-Case 'a change with no fragment fails' 1 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'feature/x')) 'no changelog fragment'
    Test-Case 'the opt-out label passes' 0 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'feature/x', '-Labels', 'no-changelog,docs'))
    Test-Case 'a release branch is exempt' 0 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'release/0.1.0'))
    Test-Case 'a dependency branch is exempt' 0 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'dependabot/pip/x'))
    Set-Fragment $root 'x.added.md' 'X.'
    git -C $root add -A
    git -C $root commit --quiet -m 'add x'
    Test-Case 'an added fragment passes' 0 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'feature/x')) 'x.added.md'
    Set-Fragment $root 'y.nonsense.md' 'Y.'
    Test-Case 'a malformed fragment fails even with the label' 1 (Invoke-Tool $root @('-Mode', 'PullRequest', '-Base', 'develop', '-HeadRef', 'feature/x', '-Labels', 'no-changelog')) 'y.nonsense.md'
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ''
if ($failures.Count -gt 0) {
    Write-Host ("{0} of {1} case(s) failed: {2}" -f $failures.Count, $count, ($failures -join '; ')) -ForegroundColor Red
    exit 1
}
Write-Host "all $count case(s) behaved"
