<#
.SYNOPSIS
    Installs the Galaxy Z Fold3 (q2q) drivers into an offline Windows partition.

.DESCRIPTION
    Run on the PC, as administrator, with the phone's Windows partition mounted
    (for example through TWRP mass storage). It:
      1. Installs the BSP and the q2q components with DriverUpdater (definitions\q2q.xml).
      2. Applies the q2q system defaults to the offline registry:
         - qcppx (Qualcomm PCIe PEP plugin) disabled: it crashes Windows on this device (0x9F)
           and Wi-Fi works without it.
         - qcpep uses the q2q LMH-fixed binary (prevents the 0x133 DPC watchdog).
         - Display scaling 200% for the built-in panel, for the logon screen, OOBE and
           every user created afterwards.

    It never touches the PC's own Windows installation.

.PARAMETER WindowsDrive
    Drive letter of the phone's Windows partition, e.g. X:
.PARAMETER SkipDrivers
    Only apply the registry defaults.
#>
#Requires -RunAsAdministrator
param(
    [Parameter(Mandatory = $true)][string]$WindowsDrive,
    [switch]$SkipDrivers
)
$ErrorActionPreference = 'Stop'

$Drivers = Split-Path $PSScriptRoot -Parent
$Target = $WindowsDrive.TrimEnd('\').TrimEnd(':') + ':\'
$MonitorId = 'MSNILNOEDID_1414_008D_FFFFFFFF_FFFFFFFF_0^030B4FCE00727AC1593E5B6FD18648D6'
$DpiValue = 4   # steps above the recommended 100% for this panel: 100,125,150,175,200

if (-not (Test-Path (Join-Path $Target 'Windows\System32\config\SYSTEM'))) { throw "$Target does not contain a Windows installation" }
if ($Target.TrimEnd('\') -ieq $env:SystemDrive) { throw "$Target is this PC's Windows; choose the phone's partition" }

if (-not $SkipDrivers) {
    if (-not (Test-Path (Join-Path $Drivers 'bsp\Drivers')) -or -not (Test-Path (Join-Path $Drivers 'components\WLAN\qcwlanhsp8380\qcwlanhsp8380.inf'))) {
        throw 'Missing BSP or Wi-Fi package: run tools\Fetch-Dependencies.ps1 first'
    }
    $arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }
    $updater = Get-ChildItem (Join-Path $PSScriptRoot "DriverUpdater\$arch") -Recurse -Filter DriverUpdater.exe | Select-Object -First 1
    if (-not $updater) { throw 'DriverUpdater not found: run tools\Fetch-Dependencies.ps1 first' }
    & $updater.FullName -d (Join-Path $Drivers 'definitions\q2q.xml') -r $Drivers -p $Target
    if ($LASTEXITCODE) { throw "DriverUpdater failed ($LASTEXITCODE)" }
}

function Invoke-Reg([string[]]$Arguments) {
    & reg.exe @Arguments | Out-Null
    if ($LASTEXITCODE) { throw "reg.exe $($Arguments -join ' ') failed" }
}

function Set-DwordValue([string]$Key, [string]$Name, [int]$Value) {
    # -Force only creates missing parent keys here: the key itself does not exist yet.
    if (-not (Test-Path -LiteralPath $Key)) { New-Item -Path $Key -Force | Out-Null }
    Set-ItemProperty -LiteralPath $Key -Name $Name -Type DWord -Value $Value
}

function Use-OfflineHive([string]$File, [string]$Name, [scriptblock]$Body) {
    Invoke-Reg @('load', "HKLM\$Name", $File)
    try { & $Body "HKLM:\$Name" }
    finally {
        [GC]::Collect(); [GC]::WaitForPendingFinalizers()
        Invoke-Reg @('unload', "HKLM\$Name")
    }
}

Use-OfflineHive (Join-Path $Target 'Windows\System32\config\SYSTEM') 'Q2Q_SYSTEM' {
    param($Root)
    $set = 'ControlSet{0:D3}' -f (Get-ItemProperty "$Root\Select").Current
    $services = "$Root\$set\Services"

    if (Test-Path "$services\qcppx") {
        Set-ItemProperty "$services\qcppx" -Name Start -Type DWord -Value 4
        Write-Host 'qcppx disabled'
    }

    $pep = Get-ChildItem (Join-Path $Target 'Windows\System32\DriverStore\FileRepository') -Directory -Filter 'q2q_pep_lmh.inf_arm64_*' | Select-Object -First 1
    if ($pep -and (Test-Path "$services\qcpep")) {
        $image = "System32\DriverStore\FileRepository\$($pep.Name)\qcpep8350.sys"
        if ((Get-ItemProperty "$services\qcpep").ImagePath -ne $image) {
            Set-ItemProperty "$services\qcpep" -Name ImagePath -Type ExpandString -Value $image
        }
        Write-Host "qcpep -> $image"
    } elseif (-not $SkipDrivers) {
        Write-Warning 'q2q PEP package not found in the DriverStore'
    }

    Set-DwordValue "$Root\$set\Control\GraphicsDrivers\ScaleFactors\$MonitorId" 'DpiValue' $DpiValue
}

Use-OfflineHive (Join-Path $Target 'Users\Default\NTUSER.DAT') 'Q2Q_DEFAULT' {
    param($Root)
    Set-DwordValue "$Root\Control Panel\Desktop\PerMonitorSettings\$MonitorId" 'DpiValue' $DpiValue
}

# The panel ID above can differ between units; SetupComplete (end of OOBE) applies 200% to
# every EDID-less panel Windows actually enumerated.
$scripts = Join-Path $Target 'Windows\Setup\Scripts'
New-Item -ItemType Directory -Path $scripts -Force | Out-Null
$setupPs1 = @"
`$sf = 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\ScaleFactors'
`$ids = @(Get-ChildItem `$sf -ErrorAction SilentlyContinue | Where-Object PSChildName -like 'MSNILNOEDID_*' | ForEach-Object PSChildName)
foreach (`$id in `$ids) { Set-ItemProperty "`$sf\`$id" -Name DpiValue -Type DWord -Value $DpiValue }
reg.exe load HKU\Q2Q_DEFAULT "`$env:SystemDrive\Users\Default\NTUSER.DAT" | Out-Null
try {
    foreach (`$id in `$ids) {
        `$k = "Registry::HKEY_USERS\Q2Q_DEFAULT\Control Panel\Desktop\PerMonitorSettings\`$id"
        if (-not (Test-Path -LiteralPath `$k)) { New-Item -Path `$k -Force | Out-Null }
        Set-ItemProperty -LiteralPath `$k -Name DpiValue -Type DWord -Value $DpiValue
    }
} finally { [GC]::Collect(); reg.exe unload HKU\Q2Q_DEFAULT | Out-Null }
"@
Set-Content -LiteralPath (Join-Path $scripts 'q2q-scaling.ps1') -Value $setupPs1 -Encoding ASCII
$cmd = Join-Path $scripts 'SetupComplete.cmd'
$line = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%WINDIR%\Setup\Scripts\q2q-scaling.ps1"'
if (-not (Test-Path $cmd) -or -not (Select-String -LiteralPath $cmd -SimpleMatch 'q2q-scaling.ps1' -Quiet)) {
    Add-Content -LiteralPath $cmd -Value $line -Encoding ASCII
}
Write-Host 'Display scaling defaults to 200%'
Write-Host 'Done.'
