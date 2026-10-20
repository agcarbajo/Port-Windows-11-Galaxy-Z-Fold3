# Project state — 2026-10-20

Target: Samsung Galaxy Z Fold3 **SM-F926B / q2q**, Windows 11 ARM64.

| Feature | Current status | Evidence / limits |
| --- | --- | --- |
| Windows boot | Working | Windows reaches OOBE / desktop on the development unit. This does not establish full hardware support. |
| Main display touch | **Working** | Physical operation confirmed by the operator; custom ARM64 STM SPI driver included. Cover-panel touch is a separate device and is not covered by this confirmation. |
| Wi-Fi | **Working** | Physical operation confirmed by the operator. UEFI PCIe/WLAN bring-up and the pinned Qualcomm WLAN package are included. Throughput, sleep/resume and long-term stability are not certified. |
| Power / volume buttons | **Working** | Physical operation confirmed by the operator; ACPI button definitions included. The former 10-second UEFI pause is also confirmed removed. |
| GPU / display | **Basic display works; Adreno acceleration remains in development** | Real GMU/HFI communication and the first owned-memory GPU command completion are verified. Hardware Direct3D / DXGI, Draw/readback, render-fence completion, IRQ delivery and stability remain unverified. |
| Bluetooth, pen, cameras, sensors, audio and suspend | Not validated here | Do not infer support from an installed package or ACPI declaration. |

Touch, Wi-Fi and button status records the operator's confirmation, reiterated
on 2026-10-20. The GPU experiments below do not independently re-test those
physical functions on every experimental boot.

## Latest GPU progress

On **2026-10-19**, the Windows experiment using native **CB0 + CB2 / LPAC**
global-memory coverage completed its first real `CP_MEM_WRITE` into an owned
buffer. Before submission the sentinel was `0x660bad00`; after one `WPTR=16`
publication the GPU wrote `0x660c0001`. Hardware RPTR, shadow RPTR and WPTR
all reached **16**, in **225 microseconds**, within the unchanged 100 ms guard.
Canary, measured-firmware and fault/drop checks passed. This proves command
consumption and a private GPU memory write for this tested sequence.

Earlier steps verified original GMU firmware execution, HFI version replies,
performance/bandwidth table acknowledgements, core start, GPU OOB handling,
owned memory retention and the original PIL/ZAP path. The successful CP trial
also retained a fresh original ZAP claim and HFI response in the same boot.

The immediately preceding CB0-only trial timed out without the token write.
Adding source-derived native CB2 coverage resolved that tested blockage.
This result is not a Direct3D render, a rendering fence, an IRQ test or a
general stability result. No accelerated WDDM capability is claimed.

A subsequent experiment adds the original `CP_SET_SECURE_MODE(0)` packet
between `ME_INIT` and `CP_MEM_WRITE`: 18 DWORDs, one WPTR publication and a new
token `0x660c0002`. Its build, preparation and application were recorded,
but **no runtime result is available in the reviewed evidence**. Even a packet
completion would not alone prove an observed secure-state transition.

The latest recorded device write, on 2026-10-19, restored the functional UEFI
baseline with SHA-256
`375e799a32d088c6f02d1acbda5765904f81255715d5f2415917f1f8bb8056a0`.
Its single complete post-flash readback matched. No later runtime result is
inferred from that write; the device was not inspected again for this publication.

## Included improvements and remaining work

- Reproducible UEFI sources and patch: header v3, corrected MSI window,
  touch power, PCIe/WLAN bring-up, dynamic MCFG/IORT, multi-LUN Mass Storage
  and power/volume button ACPI.
- Main-touch driver source and package, PEP LMH fix, pinned dependency fetcher
  and offline driver installer.
- 200% display-scaling setup for OOBE and new users. The setup script handles
  enumerated panel IDs; a clean-install reproduction remains pending.
- [GPU research notes and source snapshots](research/gpu/README.md), kept
  separate from the functional installation packages.

Next GPU acceptance target: complete the pending packet experiment, establish
render synchronization, perform a real Draw/readback and verify hardware
DXGI/Direct3D plus stability before exposing accelerated capabilities.

Installation and recovery guides are still incomplete. This repository is a
development snapshot, not a validated end-to-end installation release.

See [Spanish status](guides/Spanish/status.md) and the
[publication changelog](CHANGELOG.md).
