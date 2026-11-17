# Running Windows on the Samsung Galaxy Z Fold3

Windows 11 ARM64 on the **Samsung Galaxy Z Fold3 5G (SM-F926B, `q2q`, Snapdragon 888 / Adreno 660)**,
booted through a Mu-Silicium UEFI from the `boot` partition while TWRP stays in `recovery`.

The Adreno 660 is **hardware accelerated** with our own WDDM driver and a Mesa-based user-mode stack:
the desktop is composed on the GPU at 120 Hz on both screens, browsers use GPU compositing and WebGL,
and games run through Direct3D 9–12, Vulkan and OpenGL (native ARM64 and emulated x64).

## Language
**English** | [Español (estado)](guides/Spanish/status.md)

## ⚠️ Warning
> [!NOTE]
> - We're not responsible for bricked devices, missing recovery partitions, dead UFS, dead displays, dead hinges, any Samsung shenanigans, or you getting fired because you forgot to boot back into Android for the alarm.
> - This is a development project. Do it at your own risk and follow every step carefully.

> [!WARNING]
> **Only the SM-F926B (`q2q`) is supported.** Other Fold3 variants use different firmware and are untested.

> [!WARNING]
> **DO NOT USE VIDEO GUIDES!** If you do, we will not be able to help you if you mess up.

## Status

✅ Working · ⚠️ Partial · ❌ Not working · ❔ Not tested

| Feature | Status | Notes |
| --- | :---: | --- |
| Boot / UEFI | ✅ | Mu-Silicium UEFI in `boot`, TWRP kept in `recovery` |
| Inner display | ✅¹ | 120 Hz, 60/120 Hz selectable, brightness |
| Cover display | ⚠️¹ | 120 Hz and brightness; 60 Hz fails when it is the only active screen |
| Both screens | ✅¹ | Independent displays (extend, inner only, cover only) |
| GPU (Adreno 660) | ✅¹ | Own WDDM driver, GPU from the first frame, shown in Task Manager |
| Transparency / DWM | ✅¹ | Acrylic / Mica composed on the GPU |
| Browser acceleration | ✅¹ | Edge / Chrome GPU compositing and WebGL |
| Direct3D 11 | ✅¹ | Native, feature level 10_1 |
| Direct3D 9–12 | ✅¹ | DXVK / vkd3d-proton, installed per game |
| Vulkan / OpenGL | ✅¹ | System-wide turnip (Vulkan) and zink (OpenGL 4.6, slower) |
| x64 apps and games | ✅¹ | Emulated x64 uses the same GPU stack (ARM64EC); no 32-bit x86 |
| Main touch | ✅ | |
| Cover touch | ❌ | Controller starts, I2C transfers time out |
| Wi-Fi | ✅ | |
| Bluetooth | ⚠️¹ | Radio and scanning work; pairing not verified |
| Battery | ⚠️¹ | Percentage shown; both packs reported as one combined gauge |
| Charging | ⚠️ | Keeps the firmware defaults; not managed by Windows |
| Buttons | ✅ | Power, volume up, volume down |
| Storage (UFS) | ✅ | |
| Hinge / fold sensor | ❌ | |
| Other sensors | ❔ | Rotation, light, proximity |
| Audio | ❔ | |
| USB | ❔ | |
| Cameras | ❔ | |
| Mobile data | ❔ | |
| Fingerprint / S Pen | ❔ | |
| Sleep / resume | ❔ | Hibernation disabled |

¹ Working on the development unit. The GPU driver package and the UEFI/ACPI changes these items
need are **not released yet**; this repository currently reproduces the base port (boot, main touch,
Wi-Fi, buttons, basic display).

Details for every subsystem: [**Project status**](guides/English/status.md).

## Get Started

- **`📊Project status`** — [guides/English/status.md](guides/English/status.md)
- **`📥Installation`** (guide pending)
- **`🗑️Uninstallation`** (guide pending)

## Miscellaneous

- **`🔄Updating the UEFI`** — [guides/English/update-uefi-en.md](guides/English/update-uefi-en.md)
- **`📦Installing the drivers`** — [drivers/README.md](drivers/README.md)
- **`🎮GPU driver and games`** — [guides/English/gpu-en.md](guides/English/gpu-en.md)
- **`🛟Returning to TWRP`** — [guides/English/twrp-en.md](guides/English/twrp-en.md)

## Repository layout

| Folder | Contents |
| --- | --- |
| [`uefi/`](uefi) | Mu-Silicium patch, ACPI tables and the reproducible UEFI build script |
| [`drivers/`](drivers) | Driver definition, q2q driver sources and packages, offline installer |
| [`guides/`](guides) | Status page and guides |
| [`research/`](research) | Technical notes: GPU / display bring-up, automatic return to TWRP |

## Maintainers
[<img alt="agcarbajo" src="https://images.weserv.nl/?url=https://github.com/agcarbajo.png&w=45&fit=cover&mask=circle&maxage=7d" />](https://github.com/agcarbajo)

## Credits
- [Project Silicium](https://github.com/Project-Silicium) — Mu-Silicium UEFI and Silicium-ACPI
- [Project Mu](https://github.com/microsoft/mu) / [EDK2](https://github.com/tianocore/edk2) — UEFI base
- [WOA-Project](https://github.com/WOA-Project) — Lahaina silicon drivers and DriverUpdater
- [Ost268](https://github.com/Ost268) — Galaxy Z Fold3 driver package (`windows_oem_samsung_q2q`)
- [map220v](https://github.com/map220v) — `fts5cu56a-driver`, base of the touch driver
- [MakrSas/topaz-woa](https://github.com/MakrSas/topaz-woa) — Adreno WDDM driver and Mesa D3D bridge our GPU driver started from
- [Mesa](https://gitlab.freedesktop.org/mesa/mesa) — freedreno, turnip, zink and the D3D10/11 user-mode frontend
- [DXVK](https://github.com/doitsujin/dxvk) and [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) — Direct3D 9–12 over Vulkan
- [Khronos Vulkan-Loader](https://github.com/KhronosGroup/Vulkan-Loader)
- [erdilS/Port-Windows-11-Xiaomi-Pad-5](https://github.com/erdilS/Port-Windows-11-Xiaomi-Pad-5) — inspiration for this repository and its guides

## Other people
***It's impossible to mention everyone who helped with the drivers, UEFI and related software, or who supported the project in any way. Thank you all <3***
