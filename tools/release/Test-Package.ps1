#!/usr/bin/env pwsh
# SPDX-License-Identifier: GPL-3.0-or-later
#requires -Version 7.0
#
# Proves a runtime package works on a machine with no Qt: unpacks it into an empty directory, checks
# the licence files are there, and starts each program from the unpacked copy with nothing of the build
# or of the Qt kit in its environment. A program that cannot find a Qt library, a plugin or a QML
# module exits at once, which is what this catches (the build's own smoke tests run against the kit).
#
#   Test-Package.ps1 -Package <arinc-data-loader-VERSION-SYSTEM-ARCH.zip|.tar.gz|.dmg> [-Work <dir>]
#
# The programs run offscreen with Qt's software renderer, so no display or GPU is needed. Exit: 0 = the
# package is good, 1 = it is not (each finding is printed), 2 = bad invocation.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Package,
    [string]$Work = (Join-Path ([System.IO.Path]::GetTempPath()) ("adl-package-test-" + [guid]::NewGuid().ToString('N'))),
    [int]$Seconds = 4,
    [int]$Settle = 0
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Package -PathType Leaf)) {
    [Console]::Error.WriteLine("no such package: $Package")
    exit 2
}
$Package = (Resolve-Path -LiteralPath $Package).Path
New-Item -ItemType Directory -Path $Work -Force | Out-Null
$Work = (Resolve-Path -LiteralPath $Work).Path

$problems = [System.Collections.Generic.List[string]]::new()
function Add-Problem([string]$Text) { $script:problems.Add($Text); Write-Host "  FAIL  $Text" -ForegroundColor Red }
function Add-Ok([string]$Text) { Write-Host "  ok    $Text" }

# --- unpack ----------------------------------------------------------------------
$mounted = $null
try {
    $root = Join-Path $Work 'unpacked'
    New-Item -ItemType Directory -Path $root | Out-Null
    switch -Regex ($Package) {
        '\.zip$' { Expand-Archive -LiteralPath $Package -DestinationPath $root }
        '\.tar\.gz$' { tar -xzf $Package -C $root; if ($LASTEXITCODE -ne 0) { throw "tar failed" } }
        '\.dmg$' {
            $mounted = Join-Path $Work 'dmg'
            New-Item -ItemType Directory -Path $mounted | Out-Null
            hdiutil attach -readonly -nobrowse -noverify -mountpoint $mounted $Package | Out-Null
            if ($LASTEXITCODE -ne 0) { throw 'hdiutil attach failed' }
            # ditto keeps the bundles' symlinks, permissions and extended attributes.
            ditto $mounted $root
            if ($LASTEXITCODE -ne 0) { throw 'ditto failed' }
        }
        default { throw "do not know how to unpack $Package" }
    }
} finally {
    if ($mounted) { hdiutil detach $mounted -force 2>&1 | Out-Null }
}

# The package may have one top directory (an archive of a folder) or none.
$entries = @(Get-ChildItem -LiteralPath $root -Force)
if ($entries.Count -eq 1 -and $entries[0].PSIsContainer -and $entries[0].Name -notmatch '\.app$') {
    $root = $entries[0].FullName
}
Write-Host "unpacked $(Split-Path $Package -Leaf) into $root"

# --- the licence files -------------------------------------------------------------
$licenceDirs = @($root, (Join-Path $root 'share/doc/arinc-data-loader'))
foreach ($file in 'LICENSE.txt', 'NOTICES.txt') {
    $found = $licenceDirs | Where-Object { Test-Path -LiteralPath (Join-Path $_ $file) } | Select-Object -First 1
    if ($found) {
        $text = Get-Content -LiteralPath (Join-Path $found $file) -Raw
        if ($text.Length -lt 1000) { Add-Problem "$file is suspiciously short ($($text.Length) characters)" } else { Add-Ok "$file is in the package" }
        if ($file -eq 'NOTICES.txt' -and $text -notmatch 'LGPL|Lesser General Public') { Add-Problem 'NOTICES.txt does not mention the LGPL, which Qt is used under' }
    } else {
        Add-Problem "$file is not in the package"
    }
}

# --- what signs licences stays with the owner ----------------------------------------
# The owner's licence tool, the libraries it signs with and a key file are never in a package
# (docs/licence-owner-tool.md). If the check cannot run, that is a failure too: it passes nothing it did not read.
$python = $null
foreach ($candidate in @(Get-Command python3, python -CommandType Application -ErrorAction SilentlyContinue)) {
    # The Microsoft Store's stub is called python3 and answers with a message and a failure: use one that is Python.
    $version = & $candidate.Source --version 2>&1
    if ($LASTEXITCODE -eq 0 -and "$version" -match '^Python 3') { $python = $candidate; break }
}
if (-not $python) {
    Add-Problem 'Python was not found, so the package could not be checked for the owner tool, a signing library and key files'
} else {
    $checkOutput = & $python.Source -I (Join-Path $PSScriptRoot 'check_package_contents.py') --directory $root 2>&1
    $checkOutput | ForEach-Object { Write-Host "        $_" }
    if ($LASTEXITCODE -eq 0) { Add-Ok 'no owner tool, signing library or key file is in the package' }
    else { Add-Problem 'the package holds the owner tool, a signing library or a key file, or could not be checked for them' }
}

if ($Settle -gt 0) { Start-Sleep -Seconds $Settle }

# --- a development package: its files, and a program built against it ---------------------------------
if ((Split-Path $Package -Leaf) -match '-dev-') {
    foreach ($expected in 'include/adl/arinc665/crc.hpp', 'include/adl/platform/host.hpp', 'lib/cmake/adl/adlConfig.cmake',
                          'lib/cmake/adl/adlTargets.cmake', 'lib/cmake/adl/adlConfigVersion.cmake') {
        if (Test-Path -LiteralPath (Join-Path $root $expected)) { Add-Ok "$expected is in the package" } else { Add-Problem "$expected is not in the package" }
    }
    # Built the way a user builds: find_package(adl) against the unpacked package, in an empty directory.
    $consumerSource = (Resolve-Path (Join-Path $PSScriptRoot '../../tests/packaging/consumer')).Path
    $consumerBuild = Join-Path $Work 'consumer'
    cmake -S $consumerSource -B $consumerBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$root" 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { Add-Problem 'a program could not be configured against the package' }
    else {
        cmake --build $consumerBuild 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { Add-Problem 'a program could not be built against the package' }
        else {
            $consumer = @("$consumerBuild/consumer.exe", "$consumerBuild/consumer") | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
            & $consumer | Out-Host
            if ($LASTEXITCODE -eq 0) { Add-Ok 'a program built against the package runs' } else { Add-Problem "the program built against the package failed ($LASTEXITCODE)" }
        }
    }
    Write-Host ''
    if ($problems.Count -gt 0) {
        Write-Host "$($problems.Count) problem(s) in $(Split-Path $Package -Leaf)" -ForegroundColor Red
        exit 1
    }
    Write-Host "$(Split-Path $Package -Leaf): good"
    exit 0
}


# --- the programs ----------------------------------------------------------------------
function Find-Program([string]$Name) {
    $candidates = @(
        "$root/$Name.exe", "$root/bin/$Name", "$root/$Name", "$root/$Name.app/Contents/MacOS/$Name", "$root/tools/$Name")
    return $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}

# Nothing of the build or the Qt kit is left in the environment the programs get.
$cleanEnvironment = @{}
foreach ($entry in [System.Environment]::GetEnvironmentVariables().GetEnumerator()) {
    if ($entry.Key -match '^(QT_|QML|LD_LIBRARY_PATH|DYLD_|CMAKE_|VCPKG_|VCINSTALLDIR|VSINSTALLDIR|INCLUDE$|LIB$)') { continue }
    $cleanEnvironment[$entry.Key] = $entry.Value
}
$systemPath = if ($IsWindows) { "$env:SystemRoot\System32;$env:SystemRoot" } else { '/usr/bin:/bin' }
$cleanEnvironment['PATH'] = $systemPath
$cleanEnvironment['QT_QPA_PLATFORM'] = 'offscreen'
$cleanEnvironment['QT_QUICK_BACKEND'] = 'software'
$cleanEnvironment['QT_QUICK_CONTROLS_STYLE'] = 'Basic'
# A program that cannot create its window prints why.
$cleanEnvironment['QT_LOGGING_RULES'] = '*.debug=false'
# A program with no console (Windows) writes Qt's messages to the debugger unless told otherwise.
$cleanEnvironment['QT_FORCE_STDERR_LOGGING'] = '1'

foreach ($program in 'adl_app', 'adl_media_gen', 'adl_mock_target') {
    $path = Find-Program $program
    if (-not $path) { Add-Problem "$program is not in the package"; continue }
    # A program that is going to fail does so at once, and again when it is started again; the one
    # exception worth a second try is the very first start of a freshly unpacked executable on a
    # machine that scans new files, which can be killed or slowed once. Both attempts are reported.
    $stayedUp = $false
    $lastReport = ''
    foreach ($attempt in 1, 2) {
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new($path)
        $startInfo.WorkingDirectory = $Work
        $startInfo.UseShellExecute = $false
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.Environment.Clear()
        foreach ($key in $cleanEnvironment.Keys) { $startInfo.Environment[$key] = [string]$cleanEnvironment[$key] }
        $process = [System.Diagnostics.Process]::Start($startInfo)
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if ($process.WaitForExit($Seconds * 1000)) {
            $stdout.Wait(2000) | Out-Null; $stderr.Wait(2000) | Out-Null
            $lastReport = "attempt ${attempt}: exited with $($process.ExitCode) within $Seconds s`n$($stdout.Result)`n$($stderr.Result)"
            Write-Host "        $program $lastReport"
        } else {
            $process.Kill($true)
            $process.WaitForExit()
            $stayedUp = $true
            break
        }
    }
    if ($stayedUp) {
        Add-Ok "$program started from the unpacked package and stayed up for $Seconds s"
    } else {
        Add-Problem "$program did not stay up: $lastReport"
    }
}

# --- the command-line tool -------------------------------------------------------------------
$tool = Find-Program 'adl_write_payloads'
if (-not $tool) {
    Add-Problem 'adl_write_payloads is not in the package'
} else {
    $target = Join-Path $Work 'payloads'
    & $tool $target 2>&1 | Out-Null
    $written = @(Get-ChildItem -LiteralPath $target -Recurse -File -ErrorAction SilentlyContinue)
    if ($LASTEXITCODE -eq 0 -and $written.Count -gt 0) {
        Add-Ok "adl_write_payloads wrote $($written.Count) file(s)"
    } else {
        Add-Problem "adl_write_payloads exited with $LASTEXITCODE and wrote $($written.Count) file(s)"
    }
}

# --- the command-line mock target --------------------------------------------------------------
$mock = Find-Program 'adl_mock'
if (-not $mock) {
    Add-Problem 'adl_mock is not in the package'
} else {
    $said = (& $mock --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -eq 0 -and $said -match '^adl_mock \S+ events 1$') {
        Add-Ok "adl_mock says: $said"
    } else {
        Add-Problem "adl_mock --version exited with $LASTEXITCODE and said: $said"
    }
}

# --- the command that makes media sets -----------------------------------------------------------
$maker = Find-Program 'adl_make_set'
if (-not $maker) {
    Add-Problem 'adl_make_set is not in the package'
} else {
    $line = (& $maker --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -eq 0 -and $line -match '^adl_make_set \S+ events 1$') {
        Add-Ok "adl_make_set says: $line"
    } else {
        Add-Problem "adl_make_set --version exited with $LASTEXITCODE and said: $line"
    }
}

# --- the licence command -----------------------------------------------------------------
# On an empty computer (its settings folder is a new one, so no real licence is read) it shows no licence, and says
# so with the words a script reads.
$licenceTool = Find-Program 'adl_licence'
if (-not $licenceTool) {
    Add-Problem 'adl_licence is not in the package'
} else {
    $previousSettings = $env:ADL_SETTINGS_DIR
    $env:ADL_SETTINGS_DIR = Join-Path $Work 'licence-settings'
    try {
        $shown = @(& $licenceTool show 2>&1)
        $shownCode = $LASTEXITCODE
        $helped = @(& $licenceTool --help 2>&1)
        $helpedCode = $LASTEXITCODE
    } finally {
        $env:ADL_SETTINGS_DIR = $previousSettings
    }
    if ($shownCode -eq 0 -and ($shown -contains 'State: none') -and ($shown -contains 'Edition: community')) {
        Add-Ok 'adl_licence show reports the Community edition on an empty computer'
    } else {
        Add-Problem "adl_licence show exited with $shownCode and said: $($shown -join ' | ')"
    }
    if ($helpedCode -eq 0 -and ($helped -join "`n") -match 'usage: adl_licence show') {
        Add-Ok 'adl_licence --help prints the usage'
    } else {
        Add-Problem "adl_licence --help exited with $helpedCode"
    }
}

# --- macOS: the bundles are signed and carry the licences ---------------------------------
if ($IsMacOS) {
    foreach ($bundle in Get-ChildItem -LiteralPath $root -Filter '*.app' -Directory) {
        codesign --verify --deep --strict $bundle.FullName 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { Add-Ok "$($bundle.Name) has a valid signature" } else { Add-Problem "$($bundle.Name) failed codesign --verify" }
        foreach ($file in 'LICENSE.txt', 'NOTICES.txt') {
            if (Test-Path -LiteralPath (Join-Path $bundle.FullName "Contents/Resources/$file")) { Add-Ok "$($bundle.Name) carries $file" } else { Add-Problem "$($bundle.Name) does not carry $file" }
        }
        $minimum = (& /usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' (Join-Path $bundle.FullName 'Contents/Info.plist') 2>$null)
        Write-Host "        $($bundle.Name) LSMinimumSystemVersion: $minimum"
    }
}

Write-Host ''
if ($problems.Count -gt 0) {
    Write-Host "$($problems.Count) problem(s) in $(Split-Path $Package -Leaf)" -ForegroundColor Red
    exit 1
}
Write-Host "$(Split-Path $Package -Leaf): good"
