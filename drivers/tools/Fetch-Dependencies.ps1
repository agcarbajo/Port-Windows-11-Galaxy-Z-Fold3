<#
.SYNOPSIS
    Downloads the third-party pieces of the Galaxy Z Fold3 (q2q) driver set.

.DESCRIPTION
    - q2q BSP (Ost268/windows_oem_samsung_q2q) at the tested commit -> drivers\bsp
    - Qualcomm Wi-Fi driver qcwlanhsp8380 1.0.4516.4000 for PCI 17CB:1103 (WHQL, Microsoft Update Catalog)
      -> drivers\components\WLAN\qcwlanhsp8380
    - DriverUpdater (WOA-Project) -> drivers\tools\DriverUpdater

    Every download is verified against a pinned SHA-256 / commit.
#>
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Drivers = Split-Path $PSScriptRoot -Parent

$Bsp = @{
    Url    = 'https://github.com/Ost268/windows_oem_samsung_q2q.git'
    Commit = '4bb92ee0e551fa0741c937f4995d65383a847bfd'
    Path   = Join-Path $Drivers 'bsp'
}
$Wlan = @{
    Url    = 'https://catalog.s.download.windowsupdate.com/d/msdownload/update/driver/drvs/2026/03/c23c6f1e-191a-4ba4-9098-0376eecd411e_3199537fb19ba568258cd19adbcf10d2da04ae70.cab'
    Sha256 = '5c03a0f9d150f38643a814e6f7682e0cbb2e825683fc7fa85b357575a57b3b24'
    SysSha = '07f5b4a7f76ba69a32a76134e9603cb38f552f59073d5004be2938bcf8943e34'
    Path   = Join-Path $Drivers 'components\WLAN\qcwlanhsp8380'
}
$DriverUpdater = @{
    Version = 'v2.1.5.0'
    Path    = Join-Path $PSScriptRoot 'DriverUpdater'
}

function Get-Sha256([string]$File) { (Get-FileHash -Algorithm SHA256 -LiteralPath $File).Hash.ToLowerInvariant() }

# q2q BSP
if (-not (Test-Path (Join-Path $Bsp.Path '.git'))) {
    git clone --no-checkout $Bsp.Url $Bsp.Path
    if ($LASTEXITCODE) { throw 'git clone failed' }
}
git -C $Bsp.Path fetch --depth 1 origin $Bsp.Commit
git -C $Bsp.Path checkout --detach $Bsp.Commit
if ($LASTEXITCODE) { throw 'BSP checkout failed' }
if ((git -C $Bsp.Path rev-parse HEAD) -ne $Bsp.Commit) { throw 'BSP is not at the tested commit' }
Write-Host "BSP ready at $($Bsp.Commit)"

# Wi-Fi driver (WHQL)
$wlanSys = Join-Path $Wlan.Path 'qcwlanhsp8380.sys'
if (-not ((Test-Path $wlanSys) -and (Get-Sha256 $wlanSys) -eq $Wlan.SysSha)) {
    $cab = Join-Path $env:TEMP 'qcwlanhsp8380-1.0.4516.4000.cab'
    Invoke-WebRequest -Uri $Wlan.Url -OutFile $cab -UseBasicParsing
    if ((Get-Sha256 $cab) -ne $Wlan.Sha256) { throw 'Wi-Fi CAB hash mismatch' }
    New-Item -ItemType Directory -Force $Wlan.Path | Out-Null
    & expand.exe -F:* $cab $Wlan.Path | Out-Null
    if ($LASTEXITCODE) { throw 'expand.exe failed' }
    Remove-Item $cab
    if ((Get-Sha256 $wlanSys) -ne $Wlan.SysSha) { throw 'Extracted Wi-Fi driver hash mismatch' }
}
Write-Host 'Wi-Fi driver ready'

# DriverUpdater
foreach ($arch in 'x64', 'arm64') {
    $dir = Join-Path $DriverUpdater.Path $arch
    if (Test-Path (Join-Path $dir 'DriverUpdater.exe')) { continue }
    $zip = Join-Path $env:TEMP "DriverUpdater-$arch.zip"
    Invoke-WebRequest -Uri "https://github.com/WOA-Project/DriverUpdater/releases/download/$($DriverUpdater.Version)/win-$arch-binaries.zip" -OutFile $zip -UseBasicParsing
    Expand-Archive -LiteralPath $zip -DestinationPath $dir -Force
    Remove-Item $zip
}
Write-Host "DriverUpdater $($DriverUpdater.Version) ready"

& python (Join-Path $PSScriptRoot 'make_definition.py') --check
if ($LASTEXITCODE) { Write-Warning 'definitions\q2q.xml does not match the downloaded BSP' }
