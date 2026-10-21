#requires -Version 5.1
<#
.SYNOPSIS
    Adds, removes or lists the Windows Defender Firewall rules for the ARINC Data Loader programs.

.DESCRIPTION
    The first time an unfamiliar program path listens on the network, Windows asks "Allow access?"
    and holds the foreground until someone answers. The prompt, answered Allow, makes an inbound rule
    for that exact path for TCP and UDP, any port, on the Private and the Public profiles. This script
    makes the narrower rule up front, once per program path, so no prompt appears:

        inbound, UDP only, the Private profile only, remote addresses on the local subnet only.

    There is deliberately no list of local ports. The programs speak TFTP (RFC 1350): the data of a
    transfer travels between ports chosen when the transfer starts (the loader lets the system pick,
    the mock target uses 40000 and up), so port 59 / 1001 / the loader's control port are not the only
    ones that must be open. The program path, UDP, the private profile and the subnet are the scope.

    Nothing here is needed when the loader and the mock target run on one computer and you tick
    "This computer only (127.0.0.1)" in the mock target and use 127.0.0.1 as the target address: those
    sockets never listen on the network, and Windows asks nothing. See docs/user-guide/installing.md.

    Adding and removing rules needs an elevated PowerShell (Run as administrator). Listing does not.
    Windows needs no elevation to open ports below 1024 (59 and 1001 included).

.PARAMETER Action
    Add     Create (or replace) the rules for each program. This is the default.
    Remove  Delete the rules this script made for each program.
    Show    List every inbound rule that names one of the programs, including the broad ones the
            Windows prompt made (their group is "TCP Query User" or "UDP Query User").

.PARAMETER Program
    Full paths of the programs to cover. Default: the three that are in -BinDir.

.PARAMETER BinDir
    The folder that holds adl_app.exe, adl_mock_target.exe and adl_media_gen.exe. Default: the
    first of build\windows-msvc-release\bin under the repository, or the script's own folder, that
    has them.

.PARAMETER Profile
    The firewall profiles the rule applies to. Default: Private. A network that Windows calls "Public"
    is not covered by the default: change the network to Private (Settings, Network and Internet), or
    pass -Profile Private,Public if you accept the exposure.

.PARAMETER RemoteAddress
    Who may send to the program. Default: LocalSubnet. Use Any, or a list of addresses and subnets
    (for example 192.168.10.0/24), to widen or narrow it.

.EXAMPLE
    # From an elevated PowerShell, for the Release build:
    .\tools\firewall\windows\adl_firewall.ps1 -BinDir .\build\windows-msvc-release\bin

.EXAMPLE
    .\tools\firewall\windows\adl_firewall.ps1 -Action Show

.EXAMPLE
    .\tools\firewall\windows\adl_firewall.ps1 -Action Remove -BinDir C:\ARINC\bin

.EXAMPLE
    .\tools\firewall\windows\adl_firewall.ps1 -WhatIf     # says what it would do; changes nothing
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [ValidateSet('Add', 'Remove', 'Show')]
    [string]$Action = 'Add',
    [string[]]$Program,
    [string]$BinDir,
    [ValidateSet('Private', 'Domain', 'Public')]
    [string[]]$Profile = @('Private'),
    [string[]]$RemoteAddress = @('LocalSubnet')
)

$ErrorActionPreference = 'Stop'

$group = 'ARINC Data Loader'
$names = 'adl_app.exe', 'adl_mock_target.exe', 'adl_media_gen.exe'

function Test-Elevated {
    $principal = New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-BinDir {
    $root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
    $candidates = @(
        (Join-Path $root 'build\windows-msvc-release\bin'),
        $PSScriptRoot
    )
    foreach ($candidate in $candidates) {
        foreach ($name in $names) {
            if (Test-Path -LiteralPath (Join-Path $candidate $name)) {
                return $candidate
            }
        }
    }
    return $null
}

function Resolve-Programs {
    if ($Program) {
        return @($Program | ForEach-Object { [System.IO.Path]::GetFullPath($_) })
    }
    $dir = $BinDir
    if (-not $dir) { $dir = Find-BinDir }
    if (-not $dir -or -not (Test-Path -LiteralPath $dir)) {
        throw 'No programs to cover: pass -BinDir <folder with adl_app.exe ...> or -Program <paths>.'
    }
    $found = @()
    foreach ($name in $names) {
        $path = Join-Path $dir $name
        if (Test-Path -LiteralPath $path) { $found += [System.IO.Path]::GetFullPath($path) }
    }
    if ($found.Count -eq 0) {
        throw "None of $($names -join ', ') is in $dir."
    }
    return $found
}

# One rule per program path; the name carries the path so that a second copy of the programs in
# another folder gets a rule of its own, and a rerun replaces the rule instead of adding a twin.
function Get-RuleName([string]$path) {
    return "$group - $path"
}

function Show-Rules([string[]]$paths) {
    $rows = @()
    foreach ($filter in (Get-NetFirewallApplicationFilter)) {
        $program = $filter.Program
        if ($program -eq 'Any' -or $program -eq 'System') { continue }
        $leaf = [System.IO.Path]::GetFileName($program).ToLowerInvariant()
        if ($names -notcontains $leaf) { continue }
        $rule = $filter | Get-NetFirewallRule
        if ($rule.Direction -ne 'Inbound') { continue }
        $protocol = ($rule | Get-NetFirewallPortFilter).Protocol
        $address = ($rule | Get-NetFirewallAddressFilter).RemoteAddress -join ','
        $rows += [pscustomobject]@{
            Rule     = $rule.DisplayName
            Action   = $rule.Action
            Enabled  = $rule.Enabled
            Profile  = $rule.Profile
            Protocol = $protocol
            Remote   = $address
            Program  = $program
            Made_by  = $(if ($rule.Group -eq $group) { 'this script' } else { 'the Windows prompt or another tool' })
        }
    }
    if ($rows.Count -eq 0) {
        Write-Host 'No inbound firewall rule names any ARINC Data Loader program.'
    } else {
        $rows | Sort-Object Program, Protocol | Format-List
    }
}

function Add-Rules([string[]]$paths) {
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path)) {
            Write-Warning "$path does not exist; the rule is made anyway (it matches the path, not the file)."
        }
        $name = Get-RuleName $path
        $existing = Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue
        if ($PSCmdlet.ShouldProcess($path, "allow inbound UDP on profile $($Profile -join ',') from $($RemoteAddress -join ',')")) {
            if ($existing) { $existing | Remove-NetFirewallRule }
            $null = New-NetFirewallRule -DisplayName $name -Group $group -Direction Inbound -Action Allow `
                -Protocol UDP -Program $path -Profile $Profile -RemoteAddress $RemoteAddress `
                -Description 'Made by tools/network-access/windows/adl_firewall.ps1. Inbound UDP for this program only.'
            Write-Host "Allowed: $path"
        }
    }
    $profiles = @(Get-NetConnectionProfile -ErrorAction SilentlyContinue)
    foreach ($p in $profiles) {
        if ($Profile -notcontains [string]$p.NetworkCategory) {
            Write-Warning ("The network '$($p.Name)' is $($p.NetworkCategory), which these rules do not cover. " +
                'Make it Private (Settings, Network and Internet) or add -Profile.')
        }
    }
}

function Remove-Rules([string[]]$paths) {
    foreach ($path in $paths) {
        $name = Get-RuleName $path
        $existing = Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue
        if (-not $existing) {
            Write-Host "No rule from this script for: $path"
            continue
        }
        if ($PSCmdlet.ShouldProcess($path, 'remove the rule this script made')) {
            $existing | Remove-NetFirewallRule
            Write-Host "Removed: $path"
        }
    }
}

if ($Action -eq 'Show') {
    Show-Rules $null
    return
}

$paths = Resolve-Programs
if (-not $WhatIfPreference -and -not (Test-Elevated)) {
    Write-Error ('Changing firewall rules needs an elevated PowerShell: right-click PowerShell, ' +
        'Run as administrator, and run this again. (-WhatIf shows what would happen without elevation.)')
    exit 1
}
if ($Action -eq 'Add') { Add-Rules $paths } else { Remove-Rules $paths }
