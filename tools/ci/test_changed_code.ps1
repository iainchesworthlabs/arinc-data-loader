#requires -Version 7
<#
.SYNOPSIS
    Self-test of tools/ci/changed_code.ps1 (CTest: ci.changed_code).

.DESCRIPTION
    A change that is wrongly called documentation skips the builds of a pull request, so the lists in
    changed_code.ps1 are held here: the files that are documentation, the files that are not, and the
    way the changed files are found (a rename, a deletion, a merge queue entry).
#>
[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script = Join-Path $Root 'tools/ci/changed_code.ps1'
# A run inside a workflow step has GITHUB_OUTPUT set, and the script would write into that step's output.
Remove-Item Env:GITHUB_OUTPUT -ErrorAction SilentlyContinue

$failures = New-Object System.Collections.Generic.List[string]
$count = 0

function Assert-Code {
    param([string]$Case, [string]$Expected, [hashtable]$Arguments)
    $script:count++
    $result = @(& $script @Arguments 6>$null)[-1]
    if ($result -ne "code=$Expected") {
        $script:failures.Add("${Case}: expected code=$Expected, got '$result'")
    }
}

# --- which files are documentation --------------------------------------------------------------
$documentation = @(
    'docs/ci.md', 'docs/images/app-about.png', 'docs/user-guide/installing.md',
    'docs/design/screenshots/before-mock.png', 'docs/stylesheets/adl.css', 'docs/specs/README.md',
    'README.md', 'CHANGELOG.md', 'CONTRIBUTING.md', 'SECURITY.md', 'CODE_OF_CONDUCT.md',
    'mkdocs.yml', 'changelog.d/user-guide.added.md', 'changelog.d/README.md'
)
foreach ($file in $documentation) {
    Assert-Code "documentation: $file" 'false' @{ Files = @($file) }
}
Assert-Code 'documentation: all of them together' 'false' @{ Files = $documentation }

$code = @(
    # Read by tools/release/*.sh and release.yml.
    'docs/release-signing-key.asc',
    # What CTest reads or runs.
    'tests/CMakeLists.txt', 'tools/checks/check_doc_links.ps1', 'tools/checks/check_doc_paths.ps1',
    'LICENSE', 'CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json', 'deps/vcpkg',
    'src/app/main.cpp', 'src/arinc615a/include/adl/arinc615a/limits.hpp', 'notices/README.md',
    'cmake/Docs.cmake', 'packaging/README.txt.in',
    # Markdown below the root can be anything's input; it is not the documentation of the site.
    'src/app/README.md', 'tools/network-access/README.md', 'tests/golden/README.md',
    # The workflows and what the documentation job installs.
    '.github/workflows/ci.yml', '.github/actions/docs-site/action.yml', '.github/requirements/docs.txt',
    'tools/docs/mkdocs_hooks.py',
    # Root files that are not Markdown.
    '.gitattributes', '.gitignore', 'sonar-project.properties', 'README.txt',
    # Paths are case-sensitive.
    'Docs/ci.md', 'MKDOCS.yml'
)
foreach ($file in $code) {
    Assert-Code "code: $file" 'true' @{ Files = @($file) }
    Assert-Code "code with a page: $file" 'true' @{ Files = @('docs/ci.md', $file, 'README.md') }
}
Assert-Code 'an empty list' 'true' @{ Files = @() }

# --- how the changed files are found ------------------------------------------------------------
$git = (Get-Command git -ErrorAction SilentlyContinue)
if (-not $git) {
    Write-Host 'git is not available; the way the changed files are found was not tested'
} else {
    $work = Join-Path ([System.IO.Path]::GetTempPath()) ('adl-changed-code-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    try {
        function Invoke-Git {
            & git -c user.name=test -c user.email=test@example.invalid -c commit.gpgsign=false `
                -c core.autocrlf=false -C $work @args *> $null
            if ($LASTEXITCODE -ne 0) { throw "git $args failed in $work" }
        }
        function Get-Head { (& git -C $work rev-parse HEAD).Trim() }
        function Save-Commit {
            param([string]$Message)
            Invoke-Git add -A
            Invoke-Git commit -q -m $Message
        }
        function Assert-Event {
            param([string]$Case, [string]$Expected, [string]$EventName, [string]$Base = '')
            Push-Location $work
            try { Assert-Code $Case $Expected @{ EventName = $EventName; MergeGroupBase = $Base } }
            finally { Pop-Location }
        }

        function Merge-Topic {
            param([string]$Name, [scriptblock]$Change)
            Invoke-Git checkout -q -b $Name
            & $Change
            Save-Commit $Name
            Invoke-Git checkout -q -
            Invoke-Git merge -q --no-ff -m "merge $Name" $Name
        }

        Invoke-Git init -q
        New-Item -ItemType Directory -Path (Join-Path $work 'src'), (Join-Path $work 'docs') | Out-Null
        Set-Content (Join-Path $work 'src/a.cpp') 'int a;'
        Set-Content (Join-Path $work 'docs/a.md') '# A'
        Set-Content (Join-Path $work 'README.md') '# R'
        Save-Commit 'base'
        $first = Get-Head
        # A merge queue entry names the commit it is built on and fetches it.
        Invoke-Git remote add origin $work

        # The merge commit of a pull request has the base branch as its first parent.
        Merge-Topic 'page' { Set-Content (Join-Path $work 'docs/a.md') '# A, changed' }
        Assert-Event 'pull request: a page' 'false' 'pull_request'
        Assert-Event 'merge group: a page' 'false' 'merge_group' $first

        Merge-Topic 'source' { Set-Content (Join-Path $work 'src/a.cpp') 'int a2;' }
        Assert-Event 'pull request: a source file' 'true' 'pull_request'
        Assert-Event 'merge group: a source file' 'true' 'merge_group' $first

        # A source file moved into docs/ is a deleted source file, and a rename lists only the new name.
        Merge-Topic 'move' { Move-Item (Join-Path $work 'src/a.cpp') (Join-Path $work 'docs/a.cpp.md') }
        Assert-Event 'pull request: a source file renamed into docs' 'true' 'pull_request'

        Merge-Topic 'delete' { Remove-Item (Join-Path $work 'docs/a.cpp.md') }
        Assert-Event 'pull request: a page deleted' 'false' 'pull_request'

        # When it cannot tell, it builds.
        Assert-Event 'merge group: a base that cannot be fetched' 'true' 'merge_group' ('0' * 40)
        Assert-Event 'merge group: no base' 'true' 'merge_group'

        # Anything else builds everything.
        Assert-Event 'push' 'true' 'push'
        Assert-Event 'manual run' 'true' 'workflow_dispatch'
        Assert-Event 'no event' 'true' ''
    } finally {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Host $_ }
    Write-Host ("{0} of {1} checks failed" -f $failures.Count, $count)
    exit 1
}
Write-Host ("All {0} checks of tools/ci/changed_code.ps1 passed" -f $count)
exit 0
