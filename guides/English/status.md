# Project status

[🏠 Home](../../README.md) | **English** | [Español](../Spanish/status.md)

- Device: **Samsung Galaxy Z Fold3 — SM-F926B / q2q**
- Last updated: **2026-11-17**
- Releases: none yet (development snapshot)

✅ Working · ⚠️ Partial · ❌ Not working · ❔ Not tested

The summary table is in the [README](../../README.md#status). Results marked ¹ come from the
development unit and need the GPU driver package and UEFI/ACPI changes that are not released yet.

## 🖥️ Displays

| Feature | Status | Notes |
| --- | :---: | --- |
| Inner panel 120 Hz | ✅¹ | 60 / 120 Hz selectable in Settings |
| Cover panel 120 Hz | ✅¹ | Own EDID (physical size), 100–125 % scaling |
| Cover panel 60 Hz | ⚠️¹ | Works with both screens on; fails when the cover is the only screen |
| Extend / single screen | ✅¹ | Inner only, cover only and both, from Settings |
| Brightness | ✅¹ | Both panels |
| Window drag / scrolling | ✅¹ | ~8.3 ms per frame (120 fps) with both screens active |
| Rotation / fold detection | ❌ | Needs the hinge and orientation sensors |

## 🎆 GPU and graphics APIs

| Feature | Status | Notes |
| --- | :---: | --- |
| WDDM driver | ✅¹ | Starts with Windows (normal PnP order), "Adreno 660" with usage in Task Manager |
| DWM / transparency | ✅¹ | Desktop, Acrylic and Mica composed on the GPU |
| Browser | ✅¹ | Edge / Chrome GPU compositing; WebGL ~85–95 fps |
| Direct3D 11 (native) | ✅¹ | Feature level 10_1, ARM64 and x64 |
| Direct3D 9 / 10 / 11 | ✅¹ | DXVK 2.7.1 per game (feature level 11_1) |
| Direct3D 12 | ✅¹ | vkd3d-proton 3.1 per game |
| Direct3D 9 without DXVK | ❌ | The native driver has no D3D9 interface |
| Vulkan | ✅¹ | Mesa turnip, system-wide Khronos loader (ARM64 + x64) |
| OpenGL | ⚠️¹ | OpenGL 4.6 through zink; slower than Vulkan in windowed apps |
| 32-bit x86 apps | ❌ | No 32-bit driver |

Measured on the development unit: SuperTuxKart 1.5 at 1280×720 — OpenGL 98.6 fps (ARM64) /
75.5 fps (x64); Direct3D 9 via DXVK 113 fps (ARM64) / 112 fps (x64).

## 👆 Touch

| Feature | Status | Notes |
| --- | :---: | --- |
| Inner display | ✅ | STM FTS SPI controller, own ARM64 driver |
| Cover display | ❌ | The I2C controller and touch power come up, but every I2C transfer times out |

## 📡 Connectivity

| Feature | Status | Notes |
| --- | :---: | --- |
| Wi-Fi | ✅ | UEFI PCIe bring-up + Qualcomm WLAN driver |
| Bluetooth | ⚠️¹ | Radio on, BLE and classic scanning; pairing not verified |
| Mobile data | ❔ | |
| USB | ❔ | |

## 🔋 Power

| Feature | Status | Notes |
| --- | :---: | --- |
| Battery percentage | ✅¹ | Own battery driver (MAX77705 fuel gauge) |
| Two battery packs | ⚠️¹ | The fuel gauge measures both packs as one; Windows shows a single battery |
| Charging | ⚠️ | Keeps the firmware defaults; Windows does not control it |
| Sleep / resume | ❔ | Hibernation is disabled |

## 🔧 Other hardware

| Feature | Status | Notes |
| --- | :---: | --- |
| Power / volume buttons | ✅ | ACPI Generic Buttons |
| Storage (UFS) | ✅ | Windows boots from UFS |
| Hinge sensor | ❌ | |
| Other sensors | ❔ | |
| Audio | ❔ | |
| Cameras | ❔ | |
| Fingerprint | ❔ | |
| S Pen | ❔ | |

## Known issues

- Cover display at 60 Hz while it is the only active screen stops updating.
- OpenGL (zink) is noticeably slower than Vulkan / DXVK in small windows.
- Cover touch does not work yet.
- Drivers are signed with a development certificate: test signing must stay enabled.

Page layout based on the [Xiaomi Pad 5 project status](https://github.com/erdilS/Port-Windows-11-Xiaomi-Pad-5/blob/main/guide/English/status.md).
