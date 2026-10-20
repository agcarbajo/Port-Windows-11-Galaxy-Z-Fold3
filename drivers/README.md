# Drivers

Driver set for Windows 11 ARM64 on the Galaxy Z Fold3 (`q2q`, SM-F926B).

| Path | Description |
| --- | --- |
| `definitions/q2q.xml` | DriverUpdater definition: every BSP package plus the q2q components (components last) |
| `bsp/` | [Ost268/windows_oem_samsung_q2q](https://github.com/Ost268/windows_oem_samsung_q2q) at `4bb92ee0e551fa0741c937f4995d65383a847bfd` (downloaded by `Fetch-Dependencies.ps1`) |
| `components/Touch/q2q_stm_spi` | Main display touch driver (test-signed). Source: `src/q2q_stm_spi` |
| `components/PEP/q2q_pep_lmh` | Qualcomm PEP with the LMH interrupt mask fix (test-signed). Rebuild with `tools/make_pep_lmh.py` |
| `components/WLAN/qcwlanhsp8380` | Qualcomm Wi-Fi 1.0.4516.4000, WHQL, from the Microsoft Update Catalog (downloaded by `Fetch-Dependencies.ps1`) |
| `tools/Fetch-Dependencies.ps1` | Downloads the BSP, the Wi-Fi package and DriverUpdater, checking pinned hashes/commits |
| `tools/Install-Drivers.ps1` | Installs everything into the phone's offline Windows partition and applies the q2q defaults |
| `tools/make_definition.py` | Regenerates `definitions/q2q.xml` from `bsp/` and `components/` |

## Requirements

- The test-signed components and the BSP only load with test signing enabled in the
  Windows BCD of the phone's ESP (`testsigning on`, `nointegritychecks on`).
- DriverUpdater needs the .NET 8 Desktop Runtime on the PC.

## Installing

From an administrator PowerShell on the PC, with the phone's Windows partition mounted
(for example as `X:`):

```powershell
.\drivers\tools\Fetch-Dependencies.ps1
.\drivers\tools\Install-Drivers.ps1 -WindowsDrive X:
```

Besides the drivers, `Install-Drivers.ps1` applies these defaults to the offline registry:

- `qcppx` (Qualcomm PCIe platform extension) disabled — it crashes Windows on this phone
  (bugcheck 0x9F); PCIe/Wi-Fi is brought up by the UEFI and works without it.
- `qcpep` service pointed at the LMH-fixed PEP binary.
- Display scaling 200% for the built-in panel (logon screen, OOBE and every new user). It is
  written offline for the known panel ID and reapplied at the end of OOBE by
  `Windows\Setup\Scripts\SetupComplete.cmd` for whatever panel ID that unit enumerates.

`-SkipDrivers` applies only the registry defaults.
