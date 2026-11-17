# Automatic Windows → TWRP test cycle

[🏠 Home](../README.md)

Most of the port was developed with unattended test boots: the PC prepares a change, the phone
boots Windows, runs the test, comes back to TWRP by itself, and the PC collects the results over
USB. This page summarises the mechanism; the tooling is part of the development workspace and is
not shipped with the installer.

## Cycle

1. **Prepare (PC, phone in TWRP).** TWRP exposes the UFS LUNs as USB mass storage. The PC edits the
   offline Windows (driver files, registry hives with backups) and, if needed, writes a UEFI image
   to `boot` (size check, then one full SHA-256 readback).
2. **Arm the return.** A one-shot marker is placed on the ESP for the next boot.
3. **Test (Windows).** TWRP → Reboot → System. A test service runs the probe, saves its results on
   the Windows partition and reboots.
4. **Return.** The boot lands in TWRP; the PC re-identifies the phone, collects the results and
   restores the default UEFI and configuration if the test changed them.

## How the phone returns to TWRP

| Generation | Mechanism | Limitation |
| --- | --- | --- |
| 1 | A Windows service waits, validates the disk (model, label, partition layout), writes the TWRP image to `boot` only, verifies it and reboots | Needs Windows to reach the service; after an early crash someone has to press the keys |
| 2 (current) | The Android bootloader of the SM-F926B honours the boot-recovery request in the `misc` BCB, so the next reset boots the `recovery` partition directly | — |
| 2 + ESP loader | A small UEFI application installed as the ESP boot loader writes that request **before** Windows starts (only when the one-shot marker exists) and then chain-loads the Windows boot manager | Covers crashes during early boot: a bug check returned to TWRP on its own in 85 s |

Windows' own recovery loop is disabled in the BCD (`recoveryenabled No`, ignore boot failures),
so a failed boot resets instead of stopping in WinRE.

## Rules that kept the phone safe

- Writes are limited to `boot`, the ESP content and the Windows partition. `recovery` (TWRP), the
  partition table, `vbmeta` and `userdata` are never written.
- The phone's identity, recovery state and the target disk are revalidated before every write;
  drive letters and USB transports are never reused from a previous cycle.
- A verified rollback image of `boot` is always kept, and a write counts only after its readback
  hash matches.
- One variable per test, and every capture is tied to the test and image that produced it.

The BCB return was inspired by [UniXNU-x910](https://github.com/agcarbajo/UniXNU-x910).
