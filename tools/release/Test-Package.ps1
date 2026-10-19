#!/usr/bin/env pwsh
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

if ($Settle -gt 0) { Start-Sleep -Seconds $Settle }

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
