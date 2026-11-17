# Changelog

## 2026-11-17

- Status: Adreno 660 acceleration with our own WDDM driver and Mesa (DWM / transparency, browsers,
  D3D9–12, Vulkan, OpenGL, ARM64 and x64), 120 Hz and brightness on both panels, independent
  displays, Bluetooth radio and battery reporting — all on the development unit; the GPU package
  is not released yet.
- New guides: GPU driver and games, updating the UEFI, returning to TWRP.
- New research notes: GPU / display bring-up and the automatic return to TWRP.
- Added the battery driver source (`drivers/src/q2qbatt`) and the Bluetooth / battery SSDTs
  (`uefi/acpi/ssdt`).
- The LMH-fixed PEP is no longer redistributed: `Fetch-Dependencies.ps1` generates it from the BSP
  and it is signed locally.
- Removed `STATE.md` and the October GPU research snapshot (superseded).

## 2026-10-20

- First public snapshot: q2q UEFI (Mu-Silicium patch, DSDT, buttons), main touch driver and
  package, PEP LMH fix, pinned dependency fetcher and offline driver installer.
- Main touch, Wi-Fi and power / volume buttons working; 200% scaling by default.
