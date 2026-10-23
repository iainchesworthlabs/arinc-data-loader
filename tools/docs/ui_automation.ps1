# SPDX-License-Identifier: GPL-3.0-or-later
# Helpers for driving the three Qt applications through Windows UI Automation and keeping a
# picture of their windows. Dot-sourced by capture_screenshots.ps1; nothing here is specific to a
# screen. Windows only: the pictures are taken with PrintWindow, so the window may be partly
# covered but must not be minimised.
#
# The applications name every control (Accessible.name), which is what the helpers look for:
# Find-Control -Window $w -Name 'Upload the ticked loads'.

Set-StrictMode -Version Latest

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms

if (-not ('AdlWin32' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AdlWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool AdjustWindowRectEx(ref RECT r, int style, bool menu, int exStyle);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int index);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder text, int max);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}
'@
}
[void][AdlWin32]::SetProcessDPIAware()

$script:UIA = [System.Windows.Automation.AutomationElement]
$script:Scope = [System.Windows.Automation.TreeScope]

# Starts a program with the environment pinned so the picture does not depend on this machine's
# scaling: one device pixel per logical pixel.
function Start-AdlApp {
    param([string]$Path, [string[]]$Arguments = @())
    $env:QT_ENABLE_HIGHDPI_SCALING = '0'
    $env:QT_FONT_DPI = '96'
    if ($Arguments.Count -gt 0) {
        $proc = Start-Process -FilePath $Path -ArgumentList $Arguments -PassThru
    } else {
        $proc = Start-Process -FilePath $Path -PassThru
    }
    for ($i = 0; $i -lt 300; $i++) {
        Start-Sleep -Milliseconds 200
        $proc.Refresh()
        if ($proc.HasExited) { throw "$Path exited with code $($proc.ExitCode)" }
        if ($proc.MainWindowHandle -ne 0) { break }
    }
    if ($proc.MainWindowHandle -eq 0) { throw "$Path showed no window" }
    return $proc
}

function Stop-AdlApp {
    param($Process)
    if ($null -eq $Process -or $Process.HasExited) { return }
    [void]$Process.CloseMainWindow()
    if (-not $Process.WaitForExit(20000)) { Stop-Process -Id $Process.Id -Force }
}


# Keys are only ever sent while a window of the program under test is in front: SendKeys goes to
# whatever has the focus, and that must never be somebody else's window.
function Get-ForegroundProcessId {
    [uint32]$id = 0
    [void][AdlWin32]::GetWindowThreadProcessId([AdlWin32]::GetForegroundWindow(), [ref]$id)
    return $id
}

function Set-Foreground {
    param($Process)
    $h = [IntPtr]$Process.MainWindowHandle
    for ($i = 0; $i -lt 6; $i++) {
        [AdlWin32]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
        [AdlWin32]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
        [void][AdlWin32]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0003)
        [void][AdlWin32]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, 0x0003)
        [void][AdlWin32]::SetForegroundWindow($h)
        Start-Sleep -Milliseconds 250
        if (Test-AppInFront $Process) { return $true }
    }
    return $false
}

# The front window is the program's own, or a dialog it opened (the system's file dialogs may live
# in another process but are owned by the window that asked for them).
function Test-AppInFront {
    param($Process)
    if ((Get-ForegroundProcessId) -eq $Process.Id) { return $true }
    $root = [AdlWin32]::GetAncestor([AdlWin32]::GetForegroundWindow(), 3)
    return $root -eq [IntPtr]$Process.MainWindowHandle
}

function Assert-Foreground {
    if ($null -eq $script:Target) { throw 'No target program: call Use-AdlApp first' }
    if (-not (Test-AppInFront $script:Target)) {
        if (-not (Set-Foreground $script:Target)) {
            throw 'The program under test is not in front; refusing to send keys to another window'
        }
    }
}

# Keys and clicks go to the program the element belongs to, which is not always the one started
# last (two programs are open at once in the demonstration).
function Use-ElementApp {
    param($Element)
    $id = $Element.Current.ProcessId
    if ($null -eq $script:Target -or $script:Target.Id -ne $id) {
        $script:Target = Get-Process -Id $id
        $script:Target.Refresh()
    }
    if (-not (Test-AppInFront $script:Target)) { [void](Set-Foreground $script:Target) }
}

function Use-AdlApp {
    param($Process)
    $script:Target = $Process
    [void](Set-Foreground $Process)
}

# Puts the client area at x, y with the given size and brings the window forward.
function Set-WindowClientSize {
    param($Process, [int]$X, [int]$Y, [int]$Width, [int]$Height)
    $h = [IntPtr]$Process.MainWindowHandle
    [void][AdlWin32]::ShowWindow($h, 9)
    $rect = New-Object AdlWin32+RECT
    $rect.Left = 0; $rect.Top = 0; $rect.Right = $Width; $rect.Bottom = $Height
    $style = [AdlWin32]::GetWindowLong($h, -16)
    $exStyle = [AdlWin32]::GetWindowLong($h, -20)
    [void][AdlWin32]::AdjustWindowRectEx([ref]$rect, $style, $false, $exStyle)
    [void][AdlWin32]::SetWindowPos($h, [IntPtr]::Zero, $X, $Y, $rect.Right - $rect.Left,
        $rect.Bottom - $rect.Top, 0x0044)
    Start-Sleep -Milliseconds 400
    Use-AdlApp $Process
}

function Get-AdlWindow {
    param($Process)
    $Process.Refresh()
    return $script:UIA::FromHandle([IntPtr]$Process.MainWindowHandle)
}

# The loader's controls used to be named by a phrase that said what they do ("Upload the ticked
# loads"); an accessible name is now the text on the screen ("Upload"), so that a person who says
# what they see is understood. The script still says what a control does: this maps each phrase to
# the objectName the program gives the control (found as the end of its AutomationId) or to the
# text that names it now.
$script:ControlIds = @{
    'Open preferences'                = 'preferencesButton'
    'About the data loader'           = 'aboutButton'
    'Find targets'                    = 'findTargetsButton'
    'Show the target properties'      = 'targetPropertiesButton'
    'Choose the media set folder'     = 'mediaSetChooseButton'
    'Check the media set again'       = 'mediaSetCheckButton'
    'Show the media set properties'   = 'mediaSetPropertiesButton'
    'Tick every load'                 = 'tickAllLoadsButton'
    'Untick every load'               = 'tickNoLoadsButton'
    'Read the target information'     = 'readInformationButton'
    'Upload the ticked loads'         = 'uploadButton'
    'Download from the target'        = 'downloadButton'
    'Abort the operation'             = 'abortButton'
    'Clear the activity log'          = 'clearLogButton'
    'Start the loader'                = 'firstRunStart'
    'Choose none of the files'        = 'chooseNoneButton'
    'Download the ticked files'       = 'confirmDownloadButton'
    'Add the file name to the download' = 'addDownloadButton'
}
$script:ControlNames = @{
    'Show the default settings'       = 'Set up defaults'
    'Hide the default settings'       = 'Hide'
    'Choose the first media set'      = 'Choose a media set...'
    'Collapse the evidence panel'     = 'Hide'
    'Expand the evidence panel'       = 'Show'
    'Open the last download directory' = 'Open folder'
    'Open the download folder'        = 'Open folder'
    'Target hardware identifier and position' = 'THW_ID_POS'
    'Default target address'          = 'Default address'
    'Default target hardware identifier and position' = 'Default THW_ID_POS'
    'Loader control port'             = 'Control port'
    'Activity log capacity'           = 'Activity lines kept'
}

function Resolve-ControlName {
    param([string]$Name)
    if ($script:ControlNames.ContainsKey($Name)) { return $script:ControlNames[$Name] }
    # "Download FAULTS.TXT" and "Upload load <part number>" are check boxes named by their label.
    if ($Name -match '^Download (\S+\.\S+)$') { return $Matches[1] }
    if ($Name -match '^Upload load (.+)$') { return $Matches[1] }
    if ($Name -match '^Remove (.+) from the download$') { return 'Remove' }
    return $Name
}

function Find-Control {
    param(
        $Window,
        [string]$Name,
        [string]$Id,
        [string]$Type,
        [switch]$Optional,
        [switch]$Last,
        [int]$TimeoutMs = 8000
    )
    $suffix = $null
    if ($Name -and -not $Id) {
        if ($script:ControlIds.ContainsKey($Name)) { $suffix = $script:ControlIds[$Name]; $Name = '' }
        else { $Name = Resolve-ControlName $Name }
    }
    if ($suffix) {
        $everything = [System.Windows.Automation.Condition]::TrueCondition
        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        do {
            $matches_ = @($Window.FindAll($script:Scope::Descendants, $everything) | Where-Object {
                $_.Current.AutomationId -like "*.$suffix" -or $_.Current.AutomationId -eq $suffix })
            if ($Type) {
                $ct = [System.Windows.Automation.ControlType]::$Type
                $matches_ = @($matches_ | Where-Object { $_.Current.ControlType -eq $ct })
            }
            if ($matches_.Count -gt 0) { return $(if ($Last) { $matches_[-1] } else { $matches_[0] }) }
            Start-Sleep -Milliseconds 150
        } while ($watch.ElapsedMilliseconds -lt $TimeoutMs)
        if ($Optional) { return $null }
        throw "No control with the id ending in '$suffix' appeared"
    }
    $conditions = @()
    if ($Name) {
        $conditions += New-Object System.Windows.Automation.PropertyCondition($script:UIA::NameProperty, $Name)
    }
    if ($Id) {
        $conditions += New-Object System.Windows.Automation.PropertyCondition($script:UIA::AutomationIdProperty, $Id)
    }
    if ($Type) {
        $ct = [System.Windows.Automation.ControlType]::$Type
        $conditions += New-Object System.Windows.Automation.PropertyCondition($script:UIA::ControlTypeProperty, $ct)
    }
    $condition = if ($conditions.Count -eq 1) { $conditions[0] }
                 else { New-Object System.Windows.Automation.AndCondition($conditions) }
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    do {
        if ($Last) {
            $every = $Window.FindAll($script:Scope::Descendants, $condition)
            $found = if ($every.Count -gt 0) { $every[$every.Count - 1] } else { $null }
        } else {
            $found = $Window.FindFirst($script:Scope::Descendants, $condition)
        }
        if ($found) { return $found }
        Start-Sleep -Milliseconds 150
    } while ($watch.ElapsedMilliseconds -lt $TimeoutMs)
    if ($Optional) { return $null }
    throw "No control named '$Name' (id '$Id', type '$Type') appeared"
}

# A control by the end of its AutomationId (the objectName the program gave it), for the controls
# whose accessible name changes with the language.
function Find-ByIdSuffix {
    param($Window, [string]$Suffix, [int]$TimeoutMs = 8000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    do {
        $all = $Window.FindAll($script:Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($e in $all) { if ($e.Current.AutomationId -like "*.$Suffix") { return $e } }
        Start-Sleep -Milliseconds 300
    } while ($watch.ElapsedMilliseconds -lt $TimeoutMs)
    throw "No control with the id ending in '$Suffix'"
}

# Whether a control is enabled (dimmed controls report it).
function Test-Enabled {
    param($Window, [string]$Name, [string]$Type = 'Button', [int]$WaitMs = 3000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    do {
        $e = Find-Control -Window $Window -Name $Name -Type $Type -Optional -TimeoutMs 300
        if ($e -and $e.Current.IsEnabled) { return $true }
        Start-Sleep -Milliseconds 250
    } while ($watch.ElapsedMilliseconds -lt $WaitMs)
    return $false
}

# Whether a control is there and showing (QML hides by visible: false, which removes it from the tree).
function Test-Control {
    param($Window, [string]$Name, [string]$Id, [string]$Type)
    return $null -ne (Find-Control -Window $Window -Name $Name -Id $Id -Type $Type -Optional -TimeoutMs 400)
}

function Wait-Control {
    param($Window, [string]$Name, [string]$Id, [int]$TimeoutMs = 60000, [switch]$Gone)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt $TimeoutMs) {
        $found = Find-Control -Window $Window -Name $Name -Id $Id -Optional -TimeoutMs 200
        if ($Gone) { if (-not $found) { return } } elseif ($found) { return $found }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out waiting for '$Name'$(if ($Gone) { ' to go' })"
}

# Waits until some control's name contains the text (a status chip, a result line).
function Wait-Text {
    param($Window, [string]$Contains, [int]$TimeoutMs = 60000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt $TimeoutMs) {
        $all = $Window.FindAll($script:Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($e in $all) {
            if ($e.Current.Name -like "*$Contains*") { return $e }
        }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for text '$Contains'"
}

function Get-AllNames {
    param($Window)
    $all = $Window.FindAll($script:Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    return @($all | ForEach-Object { $_.Current.Name } | Where-Object { $_ })
}

# Presses a button (or anything with a pattern for it); falls back to a real click at its centre.
function Invoke-Control {
    param($Element)
    $pattern = $null
    if ($Element.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pattern)) {
        $pattern.Invoke()
    } elseif ($Element.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$pattern)) {
        $pattern.Toggle()
    } elseif ($Element.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern, [ref]$pattern)) {
        $pattern.Select()
    } else {
        Click-Control $Element
    }
    Start-Sleep -Milliseconds 350
}

# Scrolls the thing the element is in (with the mouse wheel, over the element's column) until the
# element is on the screen: an item of a long rail below the fold has no position until then.
function Show-Control {
    param($Element, [int]$MaxSteps = 40)
    Use-ElementApp $Element
    $h = [IntPtr]$script:Target.MainWindowHandle
    $client = New-Object AdlWin32+RECT
    [void][AdlWin32]::GetClientRect($h, [ref]$client)
    $origin = New-Object AdlWin32+POINT
    [void][AdlWin32]::ClientToScreen($h, [ref]$origin)
    $previous = $null
    for ($i = 0; $i -lt $MaxSteps; $i++) {
        $r = $Element.Current.BoundingRectangle
        $inside = (-not $Element.Current.IsOffscreen) -and $r.Y -ge $origin.Y -and
            ($r.Y + $r.Height) -le ($origin.Y + $client.Bottom - 20)
        if ($inside) {
            # The scrolling is animated: it is over when the position stops changing.
            if ($previous -eq $r.Y) { return }
            $previous = $r.Y
            Start-Sleep -Milliseconds 350
            continue
        }
        $previous = $null
        $x = [int][Math]::Min([Math]::Max($r.X + $r.Width / 2, $origin.X + 10), $origin.X + $client.Right - 10)
        $y = $origin.Y + [int]($client.Bottom / 2)
        [void][AdlWin32]::SetCursorPos($x, $y)
        Start-Sleep -Milliseconds 60
        $down = ($r.Y -gt $origin.Y)   # below the fold: wheel down (negative), above: up
        [AdlWin32]::mouse_event(0x0800, 0, 0, [uint32]$(if ($down) { [uint32]::MaxValue - 359 } else { 360 }), [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 200
    }
}

function Click-Control {
    param($Element)
    Show-Control $Element
    $r = $Element.Current.BoundingRectangle
    $x = [int]($r.X + $r.Width / 2)
    $y = [int]($r.Y + $r.Height / 2)
    [void][AdlWin32]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 80
    [AdlWin32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [AdlWin32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 350
}

# Types into a field the way an operator does (the fields react to edits, not to a value being
# set): focus, select all, type.
function Focus-Control {
    param($Element)
    Show-Control $Element
    try { $Element.SetFocus() } catch { Click-Control $Element }
}

function Set-FieldText {
    param($Element, [string]$Text, [switch]$Paste)
    Focus-Control $Element
    Start-Sleep -Milliseconds 150
    Send-Raw('^a')
    Start-Sleep -Milliseconds 80
    if ($Text -eq '') {
        Send-Raw('{DELETE}')
    } elseif ($Paste) {
        # Long text (a path) goes through the clipboard: typing it loses characters now and then.
        if (-not (Test-Path variable:script:ClipboardSaved) -or -not $script:ClipboardSaved) {
            $script:SavedClipboard = Get-Clipboard -Raw -ErrorAction SilentlyContinue
            $script:ClipboardSaved = $true
        }
        Set-Clipboard -Value $Text
        Start-Sleep -Milliseconds 150
        Send-Raw('^v')
    } else {
        Send-Raw((ConvertTo-SendKeys $Text))
    }
    Start-Sleep -Milliseconds 250
}

function ConvertTo-SendKeys {
    param([string]$Text)
    $out = New-Object System.Text.StringBuilder
    foreach ($c in $Text.ToCharArray()) {
        if ('+^%~(){}[]'.Contains([string]$c)) { [void]$out.Append("{$c}") } else { [void]$out.Append($c) }
    }
    return $out.ToString()
}

function Send-Raw {
    param([string]$Keys)
    Assert-Foreground
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
}

function Send-Keys {
    param([string]$Keys)
    Send-Raw($Keys)
    Start-Sleep -Milliseconds 300
}

# Sets a check box or switch to the wanted state. A real click: the applications react to the
# user's toggle, which an automation Toggle call does not raise.
function Set-Toggle {
    param($Element, [bool]$On)
    $pattern = $Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    $isOn = $pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On
    if ($isOn -ne $On) { Click-Control $Element; Start-Sleep -Milliseconds 250 }
}

function Get-ToggleState {
    param($Element)
    $pattern = $Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    return $pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On
}

# Sets a spin box by typing the number.
function Set-SpinValue {
    param($Element, [int]$Value)
    $edit = $Element.FindFirst($script:Scope::Descendants,
        (New-Object System.Windows.Automation.PropertyCondition($script:UIA::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::Edit)))
    $target = if ($edit) { $edit } else { $Element }
    Focus-Control $target
    Start-Sleep -Milliseconds 150
    Send-Raw('^a')
    Send-Raw([string]$Value)
    Send-Raw('{TAB}')
    Start-Sleep -Milliseconds 250
}

# Picks an entry of a combo box by its place in the list. The pop-up's items are not in the
# accessibility tree, so the keys do it: open it with a click, go all the way up, down to the
# entry, and confirm.
function Select-ComboIndex {
    param($Combo, [int]$Index)
    Click-Control $Combo
    Start-Sleep -Milliseconds 300
    Send-Raw('{UP 8}')
    if ($Index -gt 0) { Send-Raw("{DOWN $Index}") }
    Send-Raw('{ENTER}')
    Start-Sleep -Milliseconds 300
}

# The client area of the window as a PNG. PW_CLIENTONLY | PW_RENDERFULLCONTENT.
function Save-WindowPicture {
    param($Process, [string]$Path)
    $h = [IntPtr]$Process.MainWindowHandle
    $rect = New-Object AdlWin32+RECT
    [void][AdlWin32]::GetClientRect($h, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $ht = $rect.Bottom - $rect.Top
    Start-Sleep -Milliseconds 500
    $bmp = New-Object System.Drawing.Bitmap($w, $ht)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [AdlWin32]::PrintWindow($h, $hdc, 3)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    if (-not $ok) { Write-Warning "PrintWindow failed for $Path" }
    Write-Host ("  {0} ({1}x{2})" -f (Split-Path -Leaf $Path), $w, $ht)
}

# Keys for a native file or folder dialog. The dialog may belong to another process, so what
# lets the keys through is its title: the applications name theirs "Choose ...".
function Send-DialogKeys {
    param([string]$Keys, [string]$TitleStart = 'Choose ', [int]$WaitMs = 10000)
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $title = ''
    do {
        $text = New-Object System.Text.StringBuilder 256
        [void][AdlWin32]::GetWindowText([AdlWin32]::GetForegroundWindow(), $text, 256)
        $title = $text.ToString()
        if ($title.StartsWith($TitleStart)) {
            [System.Windows.Forms.SendKeys]::SendWait($Keys)
            return
        }
        Start-Sleep -Milliseconds 300
    } while ($watch.ElapsedMilliseconds -lt $WaitMs)
    throw "The front window is '$title', not a dialog of the program under test"
}

# Opens a file dialog with the named button, goes to the folder and picks the files; tries again
# (closing a dialog left open) until the check says the files arrived. A window of another program
# coming to the front while a dialog is opening is enough to lose the keys, so one attempt is not
# always enough.
function Add-FilesWithDialog {
    param($Window, [string]$Button, [string]$Folder, [string[]]$Names, [scriptblock]$Arrived, [int]$Attempts = 4)
    for ($try = 1; $try -le $Attempts; $try++) {
        try {
            Invoke-Control (Find-Control -Window $Window -Name $Button -Type Button)
            Send-DialogPath $Folder
            Send-DialogFiles $Names
            if (& $Arrived) { return }
        } catch {
            Write-Warning "File dialog attempt $try failed: $($_.Exception.Message)"
        }
        # A dialog may still be open: Escape closes it (only a dialog of ours takes the key).
        try { Send-DialogKeys '{ESC}' -WaitMs 1500 } catch { }
        Start-Sleep -Milliseconds 800
    }
    throw "The files did not arrive through the dialog opened by '$Button'"
}

# Typing a long path into the system's file dialogs loses characters (its auto-complete takes
# some), so the text goes in through the clipboard. The caller's clipboard text is put back by
# Restore-Clipboard.
function Send-DialogText {
    param([string]$Text)
    if (-not (Test-Path variable:script:SavedClipboard)) {
        $script:SavedClipboard = Get-Clipboard -Raw -ErrorAction SilentlyContinue
        $script:ClipboardSaved = $true
    }
    Set-Clipboard -Value $Text
    Start-Sleep -Milliseconds 150
    Send-DialogKeys '^v'
}

function Restore-Clipboard {
    if ((Test-Path variable:script:ClipboardSaved) -and $script:ClipboardSaved) {
        if ($null -ne $script:SavedClipboard) { Set-Clipboard -Value $script:SavedClipboard } else { Set-Clipboard -Value ' ' }
        $script:ClipboardSaved = $false
        Remove-Variable -Scope script SavedClipboard -ErrorAction SilentlyContinue
    }
}

# Types a path into a native file or folder dialog that is open and confirms it. The common
# dialogs start with the focus in the name field. A trailing backslash makes a folder dialog
# open the folder rather than pick it.
function Send-DialogPath {
    param([string]$Path, [switch]$Pick, [int]$WaitMs = 2500)
    Start-Sleep -Milliseconds $WaitMs
    Send-DialogText $Path
    Start-Sleep -Milliseconds 700
    Send-DialogKeys '{ENTER}'
    Start-Sleep -Milliseconds 1500
    if ($Pick) {
        # The first Enter opened the folder; with the name box empty the next one chooses it.
        Send-DialogKeys '{ENTER}'
        Start-Sleep -Milliseconds 1500
    }
}

# Names of files in the folder a file dialog shows: typed in the name field, quoted, then Open.
function Send-DialogFiles {
    param([string[]]$Names)
    $quoted = ($Names | ForEach-Object { '"' + $_ + '"' }) -join ' '
    Send-DialogText $quoted
    Start-Sleep -Milliseconds 600
    Send-DialogKeys '{ENTER}'
    Start-Sleep -Milliseconds 2000
}

# Saves the current settings of an application (a registry key) and puts them back afterwards, so
# a run starts from the first launch and the operator's own settings survive it.
function Backup-AppSettings {
    param([string]$Key, [string]$File)
    if (-not (Test-Path variable:script:BackedUp)) { $script:BackedUp = @{} }
    & reg query $Key *> $null
    $exists = $LASTEXITCODE -eq 0
    if ($exists) { & reg export $Key $File /y | Out-Null }
    $script:BackedUp[$Key] = @{ Exists = $exists; File = $File }
}

function Clear-AppSettings {
    param([string]$Key)
    & reg delete $Key /f 2>$null | Out-Null
}

function Restore-AppSettings {
    foreach ($key in @($script:BackedUp.Keys)) {
        $entry = $script:BackedUp[$key]
        & reg delete $key /f 2>$null | Out-Null
        if ($entry.Exists) { & reg import $entry.File 2>$null | Out-Null }
    }
}
