# Drivers

Driver set for Windows 11 ARM64 on the Galaxy Z Fold3 (`q2q`, SM-F926B).

| Path | Description |
| --- | --- |
| `definitions/q2q.xml` | DriverUpdater definition: every BSP package plus the q2q components (components last) |
| `bsp/` | [Ost268/windows_oem_samsung_q2q](https://github.com/Ost268/windows_oem_samsung_q2q) at `4bb92ee0e551fa0741c937f4995d65383a847bfd` (downloaded) |
| `components/Touch/q2q_stm_spi` | Main display touch driver (test-signed). Source: `src/q2q_stm_spi` |
| `components/PEP/q2q_pep_lmh` | Qualcomm PEP with the LMH interrupt mask fix. Only the INF is kept here: the binary is generated from the BSP |
| `components/WLAN/qcwlanhsp8380` | Qualcomm Wi-Fi 1.0.4516.4000, WHQL, from the Microsoft Update Catalog (downloaded) |
| `src/q2qbatt` | Battery driver source (development unit; needs `uefi/acpi/ssdt/SSDT-Q2QBATD.asl`) |
| `tools/Fetch-Dependencies.ps1` | Downloads the BSP, the Wi-Fi package and DriverUpdater (pinned hashes / commits) and generates the PEP |
| `tools/make_pep_lmh.py` | Patches the BSP `qcpep8350.sys` (three instructions) into the LMH-fixed PEP |
| `tools/Install-Drivers.ps1` | Installs everything into the phone's offline Windows partition and applies the q2q defaults |
| `tools/make_definition.py` | Regenerates `definitions/q2q.xml` from `bsp/` and `components/` |

The Adreno 660 GPU driver is a separate package (not released yet): see [GPU driver and games](../guides/English/gpu-en.md).

## Requirements

- Test signing enabled in the Windows BCD on the phone's ESP (`testsigning on`, `nointegritychecks on`).
- Python 3 and the .NET 8 Desktop Runtime (for DriverUpdater) on the PC.
- The Windows Driver Kit (`Inf2Cat`, `signtool`) to sign the PEP.

## Installing

From an administrator PowerShell on the PC, with the phone's Windows partition mounted
(for example as `X:` through TWRP mass storage):

```powershell
.\drivers\tools\Fetch-Dependencies.ps1
```

Sign the generated PEP with a code-signing test certificate of your own:

```powershell
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=q2q local test' -CertStoreLocation Cert:\CurrentUser\My
$pep = '.\drivers\components\PEP\q2q_pep_lmh'
signtool sign /fd SHA256 /sha1 $cert.Thumbprint "$pep\qcpep8350.sys"
Inf2Cat /driver:$pep /os:10_GE_ARM64
signtool sign /fd SHA256 /sha1 $cert.Thumbprint "$pep\q2q_pep_lmh.cat"
```

Then install:

```powershell
.\drivers\tools\Install-Drivers.ps1 -WindowsDrive X:
```

Besides the drivers, `Install-Drivers.ps1` applies these defaults to the offline registry:

- `qcppx` (Qualcomm PCIe platform extension) disabled — it crashes Windows on this phone
  (bugcheck 0x9F); PCIe/Wi-Fi is brought up by the UEFI and works without it.
- `qcpep` service pointed at the LMH-fixed PEP (prevents the 0x133 DPC watchdog).
- Display scaling 200% for the built-in panel (logon screen, OOBE and every new user). It is
  written offline for the known panel ID and reapplied at the end of OOBE by
  `Windows\Setup\Scripts\SetupComplete.cmd` for whatever panel ID that unit enumerates.

`-SkipDrivers` applies only the registry defaults.
