# Running Windows on the Samsung Galaxy Z Fold3

Updated **2026-10-20**: main-display **touch, Wi-Fi and power/volume buttons
work**, confirmed by the operator. Basic display works; **Adreno hardware
acceleration is still in development**. The GPU completed its first real
owned-memory command write in 225 µs, but Direct3D/DXGI and Draw/readback
are not yet verified. See [STATE.md](STATE.md), [Spanish status](guides/Spanish/status.md)
and [latest changes](CHANGELOG.md).

This is a development snapshot. Installation and recovery guides linked below
are still being written; their presence in this outline does not indicate a
completed installation procedure.

## Language
```Español``` (guide pending) | **```English```**

## ⚠️ Warning
> [!NOTE]
> - We're not responsible for bricked devices, missing recovery partitions, dead UFS, dead PMICs, dead displays, dead hinges, any Samsung shenanigans, dead cats or dogs, nuclear wars or you getting fired because you forgot to boot back into Android for the alarm.
> - This project is in an early stage of development. This is a delicate process, do it under your own risk and follow all the steps carefully.

> [!WARNING]
> **Only the SM-F926B (`q2q`) is supported.** If you're not comfortable with modding your phone or its partition table, or worried about bricking your device, it's okay to take a step back.

> [!WARNING]
> **DO NOT USE VIDEO GUIDES!**
>
> If you do, we will not be able to help you if you mess up.



## Get Started

- **```📖IMPORTANT: READ FIRST!```** (guide pending)

- [**`📊Project status`**](guides/English/status.md)

- **`📥Installation`** (guide pending)

- **`🗑️Uninstallation`** (guide pending)


## Miscellaneous

- **`🔓Bootloader unlocking guide`** (guide pending)

- **`🔀Dualboot setup`** (guide pending)

- **`🔧Troubleshooting`** (guide pending)

- **`🔄Updating drivers / UEFI`** (guide pending)

- **`♻️Reinstallation`** (guide pending)


## Repository layout

| Folder | Contents |
| --- | --- |
| [`uefi/`](uefi) | Mu-Silicium patches, ACPI tables and the reproducible UEFI build script |
| [`drivers/`](drivers) | Driver definition, q2q-specific driver packages and the offline installer |
| [`guides/`](guides) | Installation guides |
| [`research/gpu/`](research/gpu) | GPU experiment results and source excerpts; not an installable acceleration driver |


## Maintainers
[<img alt="ArturoGC06" src="https://images.weserv.nl/?url=https://avatars.githubusercontent.com/u/76574534?v=4&w=45&fit=cover&mask=circle&maxage=7d" />](https://github.com/ArturoGC06)

## Credits
- [Project Silicium](https://github.com/Project-Silicium) — Mu-Silicium UEFI and Silicium-ACPI
- [WOA-Project](https://github.com/WOA-Project) — Lahaina silicon drivers and DriverUpdater
- [Ost268](https://github.com/Ost268) — Galaxy Z Fold3 driver package (`windows_oem_samsung_q2q`)
- [map220v](https://github.com/map220v) — `fts5cu56a-driver`, base of the touch driver

## Other people
***It's impossible to mention everyone who helped with the drivers, UEFI and related software, or who supported the project in any way. Thank you all <3***
