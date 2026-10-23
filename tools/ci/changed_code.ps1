#requires -Version 7
<#
.SYNOPSIS
    Says whether a change is more than documentation.

.DESCRIPTION
    "Plan build legs" in .github/workflows/ci.yml runs this (through .github/actions/changed-code) on a
    pull request or a merge queue entry. When every changed file is one that no build and no test reads,
    the jobs that build and test say "nothing to build" and finish as successes in seconds, instead of
    running three operating systems, the coverage run and the simulators for a changed sentence
    (docs/ci.md, "Documentation-only changes").

    A file is documentation when it is
      * under docs/, except docs/release-signing-key.asc, which the release workflow and
        tools/release/*.sh read;
      * mkdocs.yml, or under changelog.d/;
      * a Markdown file at the root of the repository (README.md, CHANGELOG.md, ...).
    Everything else is code, which includes anything CTest reads: the Markdown files under libs/, apps/ and
    tools/, LICENSE (the notices test), the scripts the documentation tests are made of, the workflows.

    The answer is "code=true" or "code=false", written to the output and, when GITHUB_OUTPUT is set, to
    that file. It is "code=true" whenever this cannot tell: no usable diff, a failed fetch, an event that
    is not a pull request or a merge queue entry (a push, a manual run, a schedule). A push to develop or
    main is what warms the caches and gates releases, and always builds everything.

.PARAMETER EventName
    The workflow event (github.event_name).

.PARAMETER MergeGroupBase
    For a merge queue entry, the commit the entry is built on (github.event.merge_group.base_sha).

.PARAMETER Files
    Classify these file names (relative to the repository, with forward slashes) and read nothing from
    git. For the self-test, tools/ci/test_changed_code.ps1.
#>
[CmdletBinding()]
param(
    [string]$EventName = '',
    [string]$MergeGroupBase = '',
    [string[]]$Files
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Case-sensitive, as the file systems the jobs run on are.
function Test-DocumentationFile {
    param([string]$Path)
    switch -Wildcard -CaseSensitive ($Path) {
        'docs/release-signing-key.asc' { return $false }
        'docs/*' { return $true }
        'mkdocs.yml' { return $true }
        'changelog.d/*' { return $true }
        '*/*' { return $false }
        '*.md' { return $true }
        default { return $false }
    }
}

$changed = $null
$base = ''
if ($PSBoundParameters.ContainsKey('Files')) {
    $changed = @($Files | Where-Object { $_ })
} else {
    switch ($EventName) {
        # The merge commit of the pull request: its first parent is the base branch.
        'pull_request' { $base = 'HEAD^1' }
        # The queue builds the entry on a base of its own, which a shallow checkout does not have.
        'merge_group' {
            $base = $MergeGroupBase
            if ($base) {
                & git fetch --no-tags --depth=1 origin $base *> $null
                if ($LASTEXITCODE -ne 0) { $base = '' }
            }
        }
    }
    if ($base) {
        # --no-renames: a rename lists the new name only, and moving a source file into docs/ is a
        # deletion of code.
        $names = & git -c core.quotepath=false diff --name-only --no-renames $base HEAD 2>$null
        if ($LASTEXITCODE -eq 0 -and $names) { $changed = @($names) }
    }
}

$code = $true
if ($null -eq $changed -or $changed.Count -eq 0) {
    Write-Host "no usable diff against '$base' for '$EventName'; assuming code"
} else {
    $code = $false
    foreach ($file in $changed) {
        if (-not (Test-DocumentationFile $file)) {
            $code = $true
            Write-Host "$($changed.Count) changed file(s); $file is not documentation"
            break
        }
    }
    if (-not $code) { Write-Host "$($changed.Count) changed file(s), all documentation" }
}

$line = 'code=' + $(if ($code) { 'true' } else { 'false' })
Write-Output $line
if ($env:GITHUB_OUTPUT) { Add-Content -LiteralPath $env:GITHUB_OUTPUT -Value $line }
# Not the exit status of the last git command that ran: the answer is the output, and a failed fetch
# only means "code".
exit 0
