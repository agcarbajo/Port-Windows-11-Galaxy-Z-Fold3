# Returning to TWRP

[🏠 Home](../../README.md)

TWRP is in the `recovery` partition and is never replaced by the port, so it is always reachable.

## From Windows (or a frozen screen)

1. Connect the USB cable.
2. Hold **Volume Down + Power** for about 7 seconds to force a restart.
3. As soon as the screen turns off, switch to **Volume Up + Power** and hold until TWRP appears.

## From TWRP back to Windows

**Reboot → System**. The `boot` partition holds the UEFI, which starts Windows.

## Automatic return (development)

For unattended testing the development setup returns to TWRP by itself. On a test boot, a small
UEFI loader on the ESP writes a boot-recovery request (Android bootloader control block, BCB) to
`misc` before Windows starts, so whatever ends that boot — the test service rebooting, a crash or
a reset — lands in TWRP. The bootloader of the SM-F926B honours that request. Normal boots are
not affected.

How it works and its safety rules: [research/autotest.md](../../research/autotest.md). These
tools are not part of the public installer.
