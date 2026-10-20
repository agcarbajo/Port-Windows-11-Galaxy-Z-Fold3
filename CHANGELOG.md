# Changelog

## 2026-10-20 — Complete project status pages

- Replace the English status pointer with a full hardware overview and detailed
  feature tables, following the Xiaomi Pad 5 guide layout.
- Expand Spanish status with matching tables and direct language navigation.
- Link the README directly to Project status; retain STATE.md as a technical snapshot.

## 2026-10-20 — Initial GitHub snapshot

- Publish the prepared q2q UEFI, ACPI, main-touch sources/packages, PEP LMH
  component, pinned dependency fetcher and offline driver installer.
- Record operator-confirmed working main touch, Wi-Fi and power/volume buttons.
- Document removal of the UEFI 10-second pause and the 200% scaling setup.
- Incorporate GPU research through 2026-10-19: GMU/HFI execution, retained native
  memory domains and the first real CP token write with CB0 + CB2 in 225 µs.
- Explicitly retain the unverified status of hardware DXGI/Direct3D,
  Draw/readback, render fences, IRQ delivery and stability.
- Record the subsequent secure-packet experiment as runtime-unverified and
  the later verified restoration of the functional UEFI baseline.
- Add English and Spanish status documents. Installation guides remain pending.
