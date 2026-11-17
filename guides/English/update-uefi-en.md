# Updating the UEFI

[🏠 Home](../../README.md)

The UEFI lives in the `boot` partition; TWRP stays in `recovery`, so a bad UEFI never removes
your way back. Only flash `boot`: never touch `recovery`, `vbmeta`, the partition table or
other partitions.

## 1. Build the image

Follow [uefi/README.md](../../uefi/README.md). Use the file padded to the partition size:
`uefi/out/q2q-uefi-<TARGET>-boot-partition.img` (100663296 bytes).

## 2. Boot TWRP

With the phone off and the USB cable connected to the PC, hold **Volume Up + Power** until TWRP
appears. Check that ADB sees the phone in recovery:

```cmd
adb devices -l
```

## 3. Back up the current boot partition

```cmd
adb shell "dd if=/dev/block/by-name/boot of=/tmp/boot-backup.img bs=4M"
adb pull /tmp/boot-backup.img
```

Keep this file: it is your rollback (stock Android boot, or the previous UEFI).

## 4. Flash

```cmd
adb push uefi\out\q2q-uefi-RELEASE-boot-partition.img /tmp/uefi.img
adb shell "blockdev --getsize64 /dev/block/by-name/boot; stat -c %s /tmp/uefi.img"
```

Both numbers must be `100663296`. Then write and sync:

```cmd
adb shell "dd if=/tmp/uefi.img of=/dev/block/by-name/boot bs=4M && sync"
```

## 5. Verify, then boot

```cmd
adb shell "sha256sum /dev/block/by-name/boot"
certutil -hashfile uefi\out\q2q-uefi-RELEASE-boot-partition.img SHA256
```

The two hashes must match. If they don't, flash again (or restore the backup) before rebooting.
Then **Reboot → System** in TWRP starts the UEFI and Windows.

## Going back to Android

Flash the stock `boot.img` of your firmware (from the `AP` package) or your backup the same way
as in step 4. Use the firmware the phone is currently on, never an older bootloader generation.
