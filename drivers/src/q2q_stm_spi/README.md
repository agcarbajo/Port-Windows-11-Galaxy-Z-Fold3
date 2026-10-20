---
page_type: sample
description: "Demonstrates how to write a HID minidriver using User-Mode Driver Framework (UMDF)."
languages:
- cpp
products:
- windows
- windows-wdk
---

# Galaxy Z Fold3 (q2q) main display touch driver

KMDF HID minidriver for the STMicroelectronics touch controller of the Galaxy Z Fold3
main display (ACPI `STFT180F`, SPI), based on [map220v/fts5cu56a-driver](https://github.com/map220v/fts5cu56a-driver).

- `driver/q2q_stm_spi.c`: SPI transport (full-duplex transfers, scan-on and FIFO reads).
  It never writes controller firmware, calibration or NVM.
- `driver/q2q_stm_protocol.*`: 16-byte event decoding / FIFO framing, with host tests in
  `driver/q2q_stm_protocol_test.c`.
- `driver/kmdf/q2q_stm_spi.inf`: ARM64 INF. Orientation defaults (`XRevert=1`, `YRevert=0`,
  `XYExchange=1`) match the panel mounting.

The prebuilt, test-signed package lives in `drivers/components/Touch/q2q_stm_spi`.
The upstream license (GPL-3.0) and notices are retained.

# HID Minidriver Sample (UMDF version 2)

The *HID minidriver* sample demonstrates how to write a HID minidriver using User-Mode Driver Framework (UMDF).

The sample demonstrates how to communicate with an HID minidriver from an HID client using a custom-feature item in order to control certain features of the HID minidriver. This is needed since other conventional modes for communicating with a driver, like custom IOCTL or WMI, do not work with the HID minidriver. The sample also is useful in testing the correctness of a HID report descriptor without using a physical device.

## Related topics

[Creating WDF HID Minidrivers](https://docs.microsoft.com/windows-hardware/drivers/wdf/creating-umdf-hid-minidrivers)

[Human Input Devices Design Guide](https://docs.microsoft.com/windows-hardware/drivers/hid/)

[Human Input Devices Reference](https://docs.microsoft.com/windows-hardware/drivers/ddi/content/_hid/)

[UMDF HID Minidriver IOCTLs](https://docs.microsoft.com/windows-hardware/drivers/ddi/content/hidport/)
