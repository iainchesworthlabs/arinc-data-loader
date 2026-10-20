#requires -Version 7
<#
.SYNOPSIS
    Regenerates the pictures in docs/images from the real applications.

.DESCRIPTION
    Starts adl_media_gen, adl_mock_target and adl_app from a build tree and drives them through
    Windows UI Automation the way the user's guide (docs/user-guide) describes: a media set is
    generated from sample files, the mock target serves it, and the loader finds the target,
    reads its information, uploads the set and downloads files in both modes. Every picture is
    the client area of a real window (PrintWindow), 1280 x 900 (the generator 1280 x 1200 and the mock target 1280 x 1320 so their
    whole rail shows).

    Windows only. Use a Release build: the Debug presets run under AddressSanitizer, which
    crashes inside the system's file dialogs. The programs follow the desktop's colour scheme; the
    light pictures start them with the dark mode support of Qt's windows platform off, and the
    dark ones need the desktop itself to be in dark mode (the script says so and skips them
    otherwise).

    The scripts take the keyboard and the mouse while they run, so leave the machine alone. The
    applications' own settings (HKCU\Software\iainchesworthlabs\...) are exported first, emptied so
    every run starts at the first launch, and put back at the end.

.PARAMETER BuildDir
    The directory with the programs (build/windows-msvc-release/bin).

.PARAMETER OutDir
    Where the pictures go (docs/images).

.PARAMETER WorkDir
    Scratch space for the sample files, the generated media set and the target's storage.

.PARAMETER Scheme
    light, dark or both.

.PARAMETER NoOptimise
    Keep the pictures as the window gave them. Otherwise, when Python with Pillow is installed,
    they are re-saved with a 256-colour palette (tools/docs/optimise_png.py), a third of the size.

.PARAMETER Only
    Run only some parts: generator, mock, loader (the loader needs the generator's set and the
    mock target, so it runs them too when their files are missing).

.EXAMPLE
    pwsh tools/docs/capture_screenshots.ps1
#>
[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$OutDir,
    [string]$WorkDir,
    [ValidateSet('light', 'dark', 'both')][string]$Scheme = 'both',
    [switch]$NoOptimise,
    [string[]]$Only = @('generator', 'demo', 'faults', 'sets', 'language')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# `-Only a,b` arrives as one string through `pwsh -File`.
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
foreach ($part in $Only) {
    if ($part -notin 'generator', 'demo', 'faults', 'sets', 'language') { throw "Unknown part '$part': use generator, demo, faults, sets or language" }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build\windows-msvc-release\bin' }
if (-not $OutDir) { $OutDir = Join-Path $repo 'docs\images' }
if (-not $WorkDir) { $WorkDir = Join-Path $repo 'build\docs-screenshots' }
. (Join-Path $PSScriptRoot 'ui_automation.ps1')

foreach ($exe in 'adl_app', 'adl_media_gen', 'adl_mock_target', 'adl_write_payloads') {
    if (-not (Test-Path (Join-Path $BuildDir "$exe.exe"))) {
        throw "$exe.exe is not in ${BuildDir}; build the windows-msvc-release preset first"
    }
}

# ---- where things are ------------------------------------------------------------------------

$keys = @{
    loader = 'HKCU\Software\iainchesworthlabs\ARINC Data Loader'
    gen    = 'HKCU\Software\iainchesworthlabs\ARINC Media Set Generator'
    mock   = 'HKCU\Software\iainchesworthlabs\ARINC Mock Target'
}
$sources = Join-Path $WorkDir 'sources'
$targetDir = Join-Path $WorkDir 'target'
$samplesDir = Join-Path $WorkDir 'samples'
$downloads = Join-Path $WorkDir 'downloads'
$setName = 'ACM-SET-0001'

$screen = [System.Windows.Forms.Screen]::AllScreens |
    Sort-Object { $_.WorkingArea.Width * $_.WorkingArea.Height } -Descending | Select-Object -First 1
$winX = $screen.WorkingArea.X + 10
$winY = $screen.WorkingArea.Y + 10

$script:Pass = 'light'
$script:Current = $null
$script:Pictures = New-Object System.Collections.Generic.List[string]

function Get-DesktopIsDark {
    $v = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' `
        -Name AppsUseLightTheme -ErrorAction SilentlyContinue
    return ($null -ne $v) -and ($v.AppsUseLightTheme -eq 0)
}

# One picture. In the dark pass only the pictures marked -Dark are taken.
function Save-Shot {
    param($Process, [string]$Name, [switch]$Dark)
    if ($script:Pass -eq 'dark' -and -not $Dark) { return }
    $file = if ($script:Pass -eq 'dark') { "$Name-dark.png" } else { "$Name.png" }
    Save-WindowPicture $Process (Join-Path $OutDir $file)
    $script:Pictures.Add($file)
}

function Start-Program {
    param([string]$Exe, [int]$Height = 900, [string[]]$Arguments = @())
    $p = Start-AdlApp (Join-Path $BuildDir "$Exe.exe") $Arguments
    Set-WindowClientSize $p $winX $winY 1280 $Height
    $script:Current = $p
    return $p
}

function Click-Named {
    param($Window, [string]$Name, [string]$Type)
    $e = if ($Type) { Find-Control -Window $Window -Name $Name -Type $Type } else { Find-Control -Window $Window -Name $Name }
    Invoke-Control $e
}

function Fill-Field {
    param($Window, [string]$Name, [string]$Text)
    Set-FieldText (Find-Control -Window $Window -Name $Name -Type Edit) $Text
}

# Closes the front dialog by its footer button.
function Close-Dialog {
    param($Window, [string]$Button = 'Close')
    Click-Named $Window $Button Button
    Start-Sleep -Milliseconds 400
}

# What a window looked like when a step failed, for whoever reads the error.
function Save-Failure {
    param($Process)
    if ($Process -and -not $Process.HasExited) { Save-WindowPicture $Process (Join-Path $WorkDir 'failure.png') }
}

# ---- sample material -------------------------------------------------------------------------

function New-SampleFiles {
    New-Item -ItemType Directory -Force $sources, $targetDir, $samplesDir, $downloads | Out-Null
    # Fixed content, so a regenerated set is the same set. Even sizes: an odd one is a finding.
    $rng = [System.Random]::new(665)
    foreach ($f in @(@('NAVDATA.LUP', 16384), @('APPCODE.LUP', 8192))) {
        $bytes = New-Object byte[] $f[1]
        $rng.NextBytes($bytes)
        [System.IO.File]::WriteAllBytes((Join-Path $sources $f[0]), $bytes)
    }
    [System.IO.File]::WriteAllText((Join-Path $sources 'CONFIG.LDR'),
        "[loader]`r`nmode=normal`r`nretries=2`r`n", [System.Text.Encoding]::ASCII)
    # What a download can fetch: the target serves whatever is in <storage>\holds.
    $holds = Join-Path $targetDir 'holds'
    New-Item -ItemType Directory -Force $holds | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $holds 'FLIGHT01.LOG'), ('flight log 01, line {0}' -f 1..40 -join "`r`n"), [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $holds 'FAULTS.TXT'), "no faults recorded`r`n", [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $holds 'MAINT.DAT'), ('maintenance record {0}' -f 1..60 -join "`r`n"), [System.Text.Encoding]::ASCII)
}

# ---- the generator ---------------------------------------------------------------------------

function Invoke-GeneratorPass {
    param([string]$OutputRoot)
    $p = Start-Program 'adl_media_gen' 1200
    try {
        $w = Get-AdlWindow $p
        Save-Shot $p 'gen-empty' -Dark

        # Errors the form reports as they are typed.
        Fill-Field $w 'Media set part number' 'ACM SET 1'
        Fill-Field $w 'Load part number' 'ACM47-1234-9999'
        Save-Shot $p 'gen-errors'

        # A valid form.
        Fill-Field $w 'Media set part number' $setName
        Set-SpinValue (Find-Control -Window $w -Name 'Number of media set members') 1
        Select-ComboIndex (Find-Control -Window $w -Name 'Check value for each listed file') 3
        Select-ComboIndex (Find-Control -Window $w -Name 'Check value for FILES.LUM itself') 2
        Fill-Field $w 'Load part number' 'ACM47-1234-5678'
        Fill-Field $w 'Header file name' 'ACM47.LUH'
        Select-ComboIndex (Find-Control -Window $w -Name 'Load type') 1
        Select-ComboIndex (Find-Control -Window $w -Name 'Check value of the load header') 3
        Fill-Field $w 'New target hardware ID' 'HNPFMS'
        Click-Named $w 'Add target hardware ID' Button
        Set-Toggle (Find-Control -Window $w -Name 'Include a batch file') $true
        Fill-Field $w 'Batch part number' 'ACM4C-0100-0200'

        Add-FilesWithDialog $w 'Add files to the media set' $sources 'NAVDATA.LUP', 'APPCODE.LUP', 'CONFIG.LDR' {
            Test-Control -Window $w -Name 'Kind of CONFIG.LDR'
        }
        # CONFIG.LDR is a support file, the other two are the load's data.
        Select-ComboIndex (Find-Control -Window $w -Name 'Kind of CONFIG.LDR') 1
        foreach ($name in 'NAVDATA.LUP', 'APPCODE.LUP', 'CONFIG.LDR') {
            Select-ComboIndex (Find-Control -Window $w -Name "Check value of $name") 3
        }

        New-Item -ItemType Directory -Force $OutputRoot | Out-Null
        for ($try = 1; $try -le 4; $try++) {
            Click-Named $w 'Choose the output directory' Button
            try { Send-DialogPath $OutputRoot -Pick } catch { Write-Warning $_.Exception.Message }
            # The form is complete (the button is enabled) only when the output directory is set too.
            if (Test-Enabled $w 'Generate the media set') { break }
        }
        if (-not (Test-Enabled $w 'Generate the media set')) { throw 'The generator is not ready: the output directory was not taken' }
        Save-Shot $p 'gen-ready' -Dark

        # A file that cannot be listed: the same name twice, and a name the standard refuses.
        Add-FilesWithDialog $w 'Add files to the media set' $sources 'NAVDATA.LUP' {
            @(Get-AllNames $w | Where-Object { $_ -eq 'Kind of NAVDATA.LUP' }).Count -ge 2
        }
        Save-Shot $p 'gen-file-error'
        Invoke-Control (Find-Control -Window $w -Name 'Remove NAVDATA.LUP' -Type Button -Last)   # the second one
        Start-Sleep -Milliseconds 400

        Click-Named $w 'Generate the media set' Button
        Wait-Text $w 'GENERATED' 60000 | Out-Null
        Start-Sleep -Milliseconds 800
        Save-Shot $p 'gen-complete' -Dark

        Click-Named $w 'About the generator' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $p 'gen-about'
        Close-Dialog $w 'Close'
    } catch {
        Save-Failure $p
        throw
    } finally {
        Stop-AdlApp $p
    }
}

# ---- the mock target and the loader ------------------------------------------------------------

function Set-MockStorage {
    param($Window)
    for ($try = 1; $try -le 4; $try++) {
        Click-Named $Window 'Choose the storage directory' Button
        try { Send-DialogPath $targetDir -Pick } catch { Write-Warning $_.Exception.Message }
        # "Open folder" is enabled once a storage directory is set.
        if (Test-Enabled $Window 'Open the storage directory') { return }
    }
    throw 'The mock target did not take its storage directory'
}

# The mock target, configured and serving. $Faults is a hashtable of the injections to set first.
function Start-MockTarget {
    param([hashtable]$Faults = @{})
    $m = Start-Program 'adl_mock_target' 1320
    $w = Get-AdlWindow $m
    Set-MockStorage $w
    Set-SpinValue (Find-Control -Window $w -Name 'Target think time in milliseconds') 500
    Set-SpinValue (Find-Control -Window $w -Name 'Status file period in seconds') 2
    if ($Faults.ContainsKey('Refuse')) {
        Select-ComboIndex (Find-Control -Window $w -Name 'Operation to refuse') $Faults.Refuse
    }
    if ($Faults.ContainsKey('Wait')) { Set-SpinValue (Find-Control -Window $w -Name 'Seconds to answer the first request with WAIT') $Faults.Wait }
    if ($Faults.ContainsKey('DropEvery')) { Set-SpinValue (Find-Control -Window $w -Name 'Drop every Nth status file') $Faults.DropEvery }
    if ($Faults.ContainsKey('AbortAfter')) { Set-SpinValue (Find-Control -Window $w -Name 'Abort after this many status files') $Faults.AbortAfter }
    if ($Faults.ContainsKey('Corrupt')) { Set-Toggle (Find-Control -Window $w -Name 'Corrupt served files') $true }
    if ($Faults.ContainsKey('WrongChecksum')) { Set-Toggle (Find-Control -Window $w -Name 'Send a wrong checksum option') $true }
    if ($Faults.ContainsKey('Silent')) { Set-Toggle (Find-Control -Window $w -Name 'Go silent after the first status file') $true }
    # The fault block is at the foot of the rail: bring it into view for a picture.
    if ($Faults.Count -gt 0) { Show-Control (Find-Control -Window $w -Name 'Go silent after the first status file') }
    return $m
}

function Start-Serving {
    param($Window)
    Click-Named $Window 'Start the mock target' Button
    Wait-Control $Window -Name 'SERVING' -TimeoutMs 15000 | Out-Null
    Start-Sleep -Milliseconds 700
}

# The transfer strip says RUNNING while a job runs. Plain text such as the percentage is not in the
# accessibility tree, so a picture "in progress" is taken a little after the job is seen running.
function Wait-Running {
    param($Window, [int]$TimeoutMs = 20000, [int]$SettleMs = 0)
    Wait-Control $Window -Name 'RUNNING' -TimeoutMs $TimeoutMs | Out-Null
    Start-Sleep -Milliseconds $SettleMs
}

# The transfer strip says COMPLETE or FAILED when a job has ended.
function Wait-Result {
    param($Window, [int]$TimeoutMs = 90000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt $TimeoutMs) {
        $names = Get-AllNames $Window
        if ($names -contains 'COMPLETE') { return 'COMPLETE' }
        if ($names -contains 'FAILED') { return 'FAILED' }
        Start-Sleep -Milliseconds 400
    }
    throw 'The operation did not end'
}

function Wait-Idle {
    param($Window, [int]$TimeoutMs = 90000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt $TimeoutMs) {
        if (-not (Test-Control -Window $Window -Name 'Abort the operation' -Type Button)) { return }
        Start-Sleep -Milliseconds 400
    }
    throw 'The loader stayed busy'
}

function Choose-MediaSet {
    param($Window, [string]$Folder)
    for ($try = 1; $try -le 4; $try++) {
        Click-Named $Window 'Choose the media set directory' Button
        try { Send-DialogPath $Folder -Pick } catch { Write-Warning $_.Exception.Message }
        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        while ($watch.ElapsedMilliseconds -lt 15000) {
            $names = Get-AllNames $Window
            if ($names -contains 'CHECKED' -or $names -contains 'INVALID') { return }
            Start-Sleep -Milliseconds 300
        }
    }
    throw 'The loader did not read the media set'
}

function Set-LoaderPreferences {
    param($Window, [int]$Port, [string]$DownloadDirectory)
    Click-Named $Window 'Open preferences' Button
    Start-Sleep -Milliseconds 500
    Set-SpinValue (Find-Control -Window $Window -Name 'Loader control port') $Port
    Set-FieldText (Find-Control -Window $Window -Name 'Download directory' -Type Edit) $DownloadDirectory -Paste
}

function Invoke-DemoPass {
    param([string]$Generated)
    $setDir = Join-Path $Generated $setName
    $mock = Start-MockTarget
    $app = $null
    try {
        $m = Get-AdlWindow $mock
        Save-Shot $mock 'mock-stopped' -Dark
        Start-Serving $m
        Save-Shot $mock 'mock-serving' -Dark

        # ---- the loader's first launch
        $app = Start-Program 'adl_app'
        $a = Get-AdlWindow $app
        Save-Shot $app 'app-first-run' -Dark
        Click-Named $a 'Show the default settings' Button
        Save-Shot $app 'app-first-run-defaults'
        Click-Named $a 'Hide the default settings' Button
        Click-Named $a 'Start the loader' Button
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-workbench-empty' -Dark

        Click-Named $a 'Open preferences' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-preferences' -Dark
        Close-Dialog $a 'Cancel'

        Set-LoaderPreferences $a 5900 $downloads
        Close-Dialog $a 'Save'

        # ---- FIND: the broadcast, then the target by hand if nothing answered
        Click-Named $a 'Find targets' Button
        Start-Sleep -Seconds 4
        Wait-Idle $a 20000
        Save-Shot $app 'app-find-results' -Dark
        Fill-Field $a 'Target address' '127.0.0.1'
        Fill-Field $a 'Target hardware identifier and position' 'HNPFMS_L'

        # ---- the media set
        Choose-MediaSet $a $setDir
        Click-Named $a 'Tick every load' Button
        Start-Sleep -Milliseconds 500
        Save-Shot $app 'app-workbench-ready' -Dark

        Click-Named $a 'Show the media set properties' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-media-set-properties'
        Close-Dialog $a 'Close'
        Click-Named $a 'Show the target properties' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-target-properties'
        Close-Dialog $a 'Close'

        # ---- Information
        Click-Named $a 'Read the target information' Button
        Wait-Running $a -SettleMs 0
        Wait-Idle $a
        Click-Named $a 'TARGET CONFIGURATION' RadioButton
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-information'
        Click-Named $a 'ACTIVITY' RadioButton

        # ---- Upload
        Click-Named $a 'Upload the ticked loads' Button
        Wait-Running $a -SettleMs 5500
        Save-Shot $app 'app-upload-running' -Dark
        Save-Shot $mock 'mock-upload-running'
        if ((Wait-Result $a) -ne 'COMPLETE') { throw 'The upload did not complete' }
        Wait-Idle $a
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-upload-complete' -Dark
        Save-Shot $app 'app-activity-log'
        Click-Named $a 'Collapse the evidence panel' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-upload-receipt' -Dark
        Click-Named $a 'Expand the evidence panel' Button
        Save-Shot $mock 'mock-stored' -Dark

        # ---- Download, media defined
        Click-Named $a 'Download' RadioButton
        Start-Sleep -Milliseconds 500
        Save-Shot $app 'app-download-empty'
        foreach ($name in 'FLIGHT01.LOG', 'MAINT.DAT') {
            Fill-Field $a 'Download file name' $name
            Click-Named $a 'Add the file name to the download' Button
        }
        Save-Shot $app 'app-download-ready' -Dark
        Click-Named $a 'Download from the target' Button
        Wait-Running $a
        if ((Wait-Result $a) -ne 'COMPLETE') { throw 'The media defined download did not complete' }
        Wait-Idle $a
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-download-complete' -Dark
        Click-Named $a 'Collapse the evidence panel' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-download-receipt'
        Click-Named $a 'Expand the evidence panel' Button

        # ---- Download, operator defined
        Click-Named $a 'Operator defined' RadioButton
        Start-Sleep -Milliseconds 500
        Click-Named $a 'Download from the target' Button
        Wait-Control $a -Name 'Download the ticked files' -TimeoutMs 30000 | Out-Null
        Start-Sleep -Milliseconds 800
        Click-Named $a 'Download FAULTS.TXT' CheckBox
        Save-Shot $app 'app-download-choice'
        Click-Named $a 'Download the ticked files' Button
        if ((Wait-Result $a) -ne 'COMPLETE') { throw 'The operator defined download did not complete' }
        Wait-Idle $a
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-download-operator-complete'
        Click-Named $a 'Upload' RadioButton

        # ---- About and Licences
        Click-Named $a 'About the data loader' Button
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-about' -Dark
        Click-Named $a 'Licences' Button
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-licences'
        Close-Dialog $a 'Close'
        Start-Sleep -Milliseconds 400
        if (Test-Control -Window $a -Name 'Close' -Type Button) { Close-Dialog $a 'Close' }
    } catch {
        if ($app) { Save-Failure $app } else { Save-Failure $mock }
        throw
    } finally {
        Stop-AdlApp $app
        Stop-AdlApp $mock
    }
}

# A loader that is started on the settings the first pass left (the workbench, the last target).
function Start-Loader {
    $app = Start-Program 'adl_app'
    $a = Get-AdlWindow $app
    Find-ByIdSuffix $a 'findTargetsButton' -TimeoutMs 30000 | Out-Null
    Start-Sleep -Milliseconds 800
    # The last media set comes back with the workbench, but not the operator's ticks.
    if (Test-Control -Window $a -Name 'Tick every load' -Type Button) {
        Wait-Control $a -Name 'CHECKED' -TimeoutMs 20000 | Out-Null
        Click-Named $a 'Tick every load' Button
    }
    return $app
}

# A picture kept for whoever checks the claims in the guide, not for the documentation.
function Save-Evidence {
    param($Process, [string]$Name)
    Save-WindowPicture $Process (Join-Path $WorkDir "evidence-$Name.png")
}

function Dismiss-ErrorDialog {
    param($Window)
    Click-Named $Window 'OK' Button
    Start-Sleep -Milliseconds 500
}

# Waits for the failure dialog (its OK button) and leaves it open for the picture.
function Wait-ErrorDialog {
    param($Window, [int]$TimeoutMs = 60000)
    Wait-Control $Window -Name 'OK' -TimeoutMs $TimeoutMs | Out-Null
    Start-Sleep -Milliseconds 800
}

function Invoke-FaultsPass {
    param([string]$Generated)
    $app = $null
    $mock = $null
    try {
        $app = Start-Loader
        $a = Get-AdlWindow $app

        # ---- the target refuses the upload
        $mock = Start-MockTarget @{ Refuse = 2 }
        $m = Get-AdlWindow $mock
        Save-Shot $mock 'mock-faults'
        Start-Serving $m
        Click-Named $a 'Upload the ticked loads' Button
        Wait-ErrorDialog $a 40000
        Save-Shot $app 'app-error-refused' -Dark
        Dismiss-ErrorDialog $a
        Click-Named $a 'Collapse the evidence panel' Button
        Start-Sleep -Milliseconds 500
        Save-Shot $app 'app-upload-failed'
        Click-Named $a 'Expand the evidence panel' Button
        Stop-AdlApp $mock
        $mock = $null

        # ---- no target there at all
        Click-Named $a 'Read the target information' Button
        Wait-ErrorDialog $a 90000
        Save-Shot $app 'app-error-no-answer'
        Dismiss-ErrorDialog $a

        # ---- a target that goes silent, and closing the window meanwhile
        $mock = Start-MockTarget @{ Silent = $true }
        Start-Serving (Get-AdlWindow $mock)
        Click-Named $a 'Upload the ticked loads' Button
        Wait-Running $a -SettleMs 2500
        [void]$app.CloseMainWindow()
        Wait-Control $a -Name 'Keep working' -TimeoutMs 10000 | Out-Null
        Start-Sleep -Milliseconds 600
        Save-Shot $app 'app-close-dialog'
        Click-Named $a 'Keep working' Button
        Click-Named $a 'Abort the operation' Button
        Wait-ErrorDialog $a 60000
        Save-Shot $app 'app-error-aborted'
        Dismiss-ErrorDialog $a
        Stop-AdlApp $mock
        $mock = $null

        # ---- the other faults, one at a time, pictured for checking the guide
        $cases = @(
            @{ Name = 'abort';    Faults = @{ AbortAfter = 2 };         Do = 'upload' },
            @{ Name = 'corrupt';  Faults = @{ Corrupt = $true };        Do = 'download' },
            @{ Name = 'checksum'; Faults = @{ WrongChecksum = $true };  Do = 'download' }
        )
        foreach ($case in $cases) {
            $mock = Start-MockTarget $case.Faults
            Start-Serving (Get-AdlWindow $mock)
            Click-Named $a 'Clear the activity log' Button
            switch ($case.Do) {
                'information' { Click-Named $a 'Read the target information' Button }
                'upload'      { Click-Named $a 'Upload the ticked loads' Button }
                'download'    {
                    Click-Named $a 'Download' RadioButton
                    Start-Sleep -Milliseconds 800
                    Fill-Field $a 'Download file name' 'FLIGHT01.LOG'
                    Click-Named $a 'Add the file name to the download' Button   # a second add of a name is refused
                    Click-Named $a 'Download from the target' Button
                }
            }
            Start-Sleep -Seconds 2
            Wait-Idle $a 90000
            Start-Sleep -Milliseconds 1000
            Save-Evidence $app $case.Name
            Save-Evidence $mock "$($case.Name)-mock"
            if (Test-Control -Window $a -Name 'OK' -Type Button) { Dismiss-ErrorDialog $a }
            if ($case.Do -eq 'download') { Click-Named $a 'Upload' RadioButton }
            Stop-AdlApp $mock
            $mock = $null
        }
    } catch {
        Save-Failure $app
        throw
    } finally {
        Stop-AdlApp $app
        Stop-AdlApp $mock
    }
}

function Invoke-SetsPass {
    param([string]$Generated)
    $app = $null
    try {
        # A copy of the generated set with one byte of a data file changed.
        $broken = Join-Path $samplesDir 'broken'
        if (Test-Path $broken) { Remove-Item -Recurse -Force $broken }
        New-Item -ItemType Directory -Force $broken | Out-Null
        Copy-Item -Recurse (Join-Path $Generated $setName) (Join-Path $broken $setName)
        $victim = Get-ChildItem (Join-Path $broken $setName) -Recurse -Filter 'NAVDATA.LUP' | Select-Object -First 1
        $bytes = [System.IO.File]::ReadAllBytes($victim.FullName)
        $bytes[100] = $bytes[100] -bxor 0xFF
        [System.IO.File]::WriteAllBytes($victim.FullName, $bytes)

        # The samples the payload writer makes; the two-member one has a finding.
        $payloads = Join-Path $samplesDir 'payloads'
        if (Test-Path $payloads) { Remove-Item -Recurse -Force $payloads }
        & (Join-Path $BuildDir 'adl_write_payloads.exe') --scenario two-member $payloads | Out-Null

        $app = Start-Loader
        $a = Get-AdlWindow $app
        Choose-MediaSet $a (Join-Path $broken $setName)
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-set-invalid' -Dark
        Dismiss-ErrorDialog $a
        Choose-MediaSet $a (Join-Path $payloads 'two-member')
        Click-Named $a 'Tick every load' Button
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-set-warnings'
        Click-Named $a 'Show the media set properties' Button
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-media-set-properties-two-members'
        Close-Dialog $a 'Close'
    } catch {
        Save-Failure $app
        throw
    } finally {
        Stop-AdlApp $app
    }
}

function Invoke-LanguagePass {
    param([string]$Generated)
    $app = $null
    try {
        $app = Start-Loader
        $a = Get-AdlWindow $app
        Choose-MediaSet $a (Join-Path $Generated $setName)
        Click-Named $a 'Open preferences' Button
        Start-Sleep -Milliseconds 600
        Select-ComboIndex (Find-Control -Window $a -Type ComboBox) 5   # Hebrew
        Start-Sleep -Milliseconds 900
        Send-Raw('{ESC}')
        Start-Sleep -Milliseconds 800
        # The window as it is right after the switch, kept for checking whether it mirrors.
        Save-Evidence $app 'hebrew-live'
        # The language is remembered: a new start of the loader is the window in Hebrew from the start.
        Stop-AdlApp $app
        $app = Start-Loader
        Start-Sleep -Milliseconds 800
        Save-Shot $app 'app-workbench-hebrew'
        # Back to English, so that the next run (and the operator's settings) start in English.
        $a = Get-AdlWindow $app
        Invoke-Control (Find-ByIdSuffix $a 'preferencesButton')
        Start-Sleep -Milliseconds 600
        Select-ComboIndex (Find-Control -Window $a -Type ComboBox) 0
        Send-Raw('{ESC}')
    } catch {
        Save-Failure $app
        throw
    } finally {
        Stop-AdlApp $app
    }
}

# The loader's settings as the demonstration leaves them, for running a later part on its own.
function Initialize-LoaderSettings {
    param([string]$Generated)
    $k = $keys.loader
    $set = Join-Path (Join-Path $Generated $setName) ''
    & reg add "$k\app" /v firstRunComplete /t REG_SZ /d true /f | Out-Null
    & reg add "$k\language" /v code /t REG_SZ /d en /f | Out-Null
    & reg add "$k\mediaSet" /v lastDirectory /t REG_SZ /d (Join-Path $Generated $setName) /f | Out-Null
    & reg add "$k\target" /v lastAddress /t REG_SZ /d 127.0.0.1 /f | Out-Null
    & reg add "$k\target" /v lastThwIdPos /t REG_SZ /d HNPFMS_L /f | Out-Null
    & reg add "$k\loader" /v controlPort /t REG_DWORD /d 5900 /f | Out-Null
    & reg add "$k\download" /v directory /t REG_SZ /d $downloads /f | Out-Null
}

# ---- run -------------------------------------------------------------------------------------

New-Item -ItemType Directory -Force $OutDir, $WorkDir | Out-Null
Backup-AppSettings $keys.loader (Join-Path $WorkDir 'loader-settings.reg')
Backup-AppSettings $keys.gen (Join-Path $WorkDir 'gen-settings.reg')
Backup-AppSettings $keys.mock (Join-Path $WorkDir 'mock-settings.reg')
$oldPlatform = $env:QT_QPA_PLATFORM

try {
    New-SampleFiles
    $passes = switch ($Scheme) { 'light' { @('light') } 'dark' { @('dark') } default { @('light', 'dark') } }
    foreach ($pass in $passes) {
        if ($pass -eq 'dark' -and -not (Get-DesktopIsDark)) {
            Write-Warning 'The desktop is in light mode, so the programs cannot be shown dark: skipping the dark pictures.'
            continue
        }
        $script:Pass = $pass
        $env:QT_QPA_PLATFORM = if ($pass -eq 'light') { 'windows:darkmode=0' } else { 'windows:darkmode=2' }
        Write-Host "== $pass"
        $generated = Join-Path $WorkDir ($pass -eq 'light' ? 'generated' : 'generated-dark')
        if (($Only -contains 'generator') -and (Test-Path $generated)) { Remove-Item -Recurse -Force $generated }
        foreach ($k in $keys.Values) { Clear-AppSettings $k }
        if ($Only -contains 'generator') { Invoke-GeneratorPass $generated }
        if (($Only -notcontains 'demo') -and ($Only | Where-Object { $_ -in 'faults', 'sets', 'language' })) { Initialize-LoaderSettings $generated }
        if ($Only -contains 'demo') { Invoke-DemoPass $generated }
        if ($Only -contains 'faults') { Invoke-FaultsPass $generated }
        if ($Only -contains 'sets') { Invoke-SetsPass $generated }
        if ($Only -contains 'language') { Invoke-LanguagePass $generated }
    }
} catch {
    # What the window looked like when it went wrong, for whoever reads the error.
    if ($script:Current -and -not $script:Current.HasExited) {
        Save-WindowPicture $script:Current (Join-Path $WorkDir 'failure.png')
    }
    throw
} finally {
    $env:QT_QPA_PLATFORM = $oldPlatform
    Restore-Clipboard
    Get-Process adl_app, adl_media_gen, adl_mock_target -ErrorAction SilentlyContinue | Stop-Process -Force
    Restore-AppSettings
}

if (-not $NoOptimise -and $script:Pictures.Count -gt 0) {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        & $python.Source -c 'import PIL' 2>$null
        if ($LASTEXITCODE -eq 0) {
            $files = @($script:Pictures | ForEach-Object { Join-Path $OutDir $_ })
            & $python.Source (Join-Path $PSScriptRoot 'optimise_png.py') @files
        } else {
            Write-Warning 'Pillow is not installed (pip install pillow): the pictures are kept as they are.'
        }
    }
}

Write-Host ("{0} pictures in {1}" -f $script:Pictures.Count, $OutDir)
