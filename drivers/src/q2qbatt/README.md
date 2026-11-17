# q2qbatt — battery driver

KMDF battery miniclass for the Fold3. It reports one Windows battery from the MAX77705 fuel gauge,
which measures the main and sub packs together (the hardware has no per-pack state of charge).
Per-pack voltage and current from the two S2ASL01 limiters are published read-only under the
device's `Device Parameters\Q2qLive` volatile key for diagnostics.

The driver only reads registers; charging configuration is left as the firmware set it.

## Device

`ACPI\Q2QC0B01`, defined by [`uefi/acpi/ssdt/SSDT-Q2QBATD.asl`](../../../uefi/acpi/ssdt/SSDT-Q2QBATD.asl).
Connection order in `_CRS`:

| Index | Address | Chip | Bus |
| --- | --- | --- | --- |
| 0 | `0x36` | MAX77705 fuel gauge (required) | `\_SB.I2C8` |
| 1 | `0x69` | MAX77705 charger, status only | `\_SB.I2C8` |
| 2 | `0x38` | S2ASL01 main-pack limiter | `\_SB.I2C2` |
| 3 | `0x39` | S2ASL01 sub-pack limiter | `\_SB.IC10` |

## Building

WDK 10.0.26100, KMDF 1.15, ARM64. Compile `q2qbatt.c` as C with `/kernel /D_ARM64_` and the KM and
KMDF include paths, link with `/DRIVER /SUBSYSTEM:NATIVE /ENTRY:FxDriverEntry` against `ntoskrnl.lib
hal.lib battc.lib ntstrsafe.lib wdfdriverentry.lib wdfldr.lib`, then:

```cmd
infverif /h q2qbatt.inf
inf2cat /driver:<package folder> /os:10_GE_ARM64
signtool sign /fd SHA256 /sha1 <your test certificate thumbprint> q2qbatt.sys q2qbatt.cat
```
