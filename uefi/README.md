# UEFI

Galaxy Z Fold3 (`q2q`, SM-F926B) UEFI, built from
[Project-Silicium/Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) at commit
`aed54edbb704c62493a1a27bbbc00aa51e5bd179`. The image boots from the phone's `boot`
partition (Android boot image header v3); `recovery` keeps TWRP.

## Contents

| Path | Description |
| --- | --- |
| `build.py` | Clones Mu-Silicium at the pinned commit, applies the q2q changes and builds the image |
| `patches/mu-silicium-q2q.patch` | q2q changes to Mu-Silicium (see below) |
| `acpi/DSDT.asl` | q2q DSDT (UFS, CPUs, GPIO, PEP, SMMU, touch SPI, PCIe/Wi-Fi) |
| `acpi/buttons.asl` | Power / volume buttons (SPMI, PMIC, PMIC GPIO and Generic Buttons device), included by `DSDT.asl` |

The patch contains:

- `Resources/Configs/q2q.toml`: Android boot image header v3 (required by the Fold3 bootloader).
- `PlatformBootManager.c`: board bring-up done before Windows starts — touch panel power
  (S2DOS05), PCIe0/Wi-Fi bring-up (clocks, GDSC, PHY, link), dynamic MCFG/IORT and the SMMU
  stream mapping for the Wi-Fi endpoint. It also publishes some read-only diagnostics as
  volatile UEFI variables.
- `AcpiTableUpdate.c`: ChipInfo output fix (32-bit outputs written into 32-bit variables).
- `MemoryInitPei.c`: publishes the SEC copy of the live translation-entry helper so that DXE
  page-table splits never turn the MMU off.
- `MassStorage.c`: exposes UFS LU0–LU7 in Mass Storage mode.

`build.py` also patches the Lahaina MADT (`APIC.aml`): the GIC MSI frame is reduced to SPI
base `0x320` / count `0x40`, matching the PCIe MSI windows of the device tree.

## Building

Linux or WSL, with Python 3.11+, `iasl` (`acpica-tools`) and Mu-Silicium's dependencies
(`setup_env.sh -p apt` on Debian/Ubuntu: clang, lld, llvm, nasm, mono, gcc-aarch64-linux-gnu,
uuid-dev, python3-venv):

```bash
python3 uefi/build.py                     # DEBUG (default, tested)
python3 uefi/build.py --target RELEASE
python3 uefi/build.py --workdir ~/mu-q2q  # work directory must not contain spaces
```

The first run downloads ~5 GB of sources. Output in `uefi/out/`:

- `q2q-uefi-<TARGET>.img`: boot image.
- `q2q-uefi-<TARGET>-boot-partition.img`: the same image padded to the exact size of the
  `boot` partition (100663296 bytes).
