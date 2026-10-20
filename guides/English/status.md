# Project status

[🏠 Home](../../README.md) | **English** | [Español](../Spanish/status.md)

- Device: **Samsung Galaxy Z Fold3 — SM-F926B / q2q**.
- Last updated: **2026-10-20**.
- Current driver set: development snapshot; no numbered release yet.
- Current UEFI: development snapshot; functional baseline restored on 2026-10-19.

## Simple status

✅ Working · ⚠️ Partial / in development · ❔ Not verified

| Feature | Notes | Status |
| --- | --- | :---: |
| 🪟 Windows boot | Reaches OOBE / desktop on the development unit | ✅ |
| 🖥️ Display | Basic output works; fold / panel switching not verified | ⚠️ |
| 🎆 GPU | Real command completion; Direct3D / DXGI acceleration still in development | ⚠️ |
| 👆 Main touch | Operator-confirmed working | ✅ |
| 👆 Cover touch | Separate controller; not verified | ❔ |
| 🛜 Wi-Fi | Operator-confirmed connectivity | ✅ |
| ⌨️ Side buttons | Power, volume up and volume down work | ✅ |
| 💾 Storage | Windows boots from UFS; stress and all-LUN native I/O not verified | ⚠️ |
| 🔊 Audio | Windows audio unverified; this unit's internal speakers are physically faulty | ❔ |
| 🔋 Battery | Battery reporting and power management not verified | ❔ |
| 🔌 Charging | Windows charging behaviour not verified | ❔ |
| 🪵 USB | Windows host / OTG and device modes not verified | ❔ |
| 🔵 Bluetooth | Not verified | ❔ |
| 🖊️ Pen | Not verified | ❔ |
| 📷 Cameras | Not verified | ❔ |
| 🧭 Sensors | Hinge, rotation and other sensors not verified | ❔ |
| 🔒 Fingerprint | Not verified | ❔ |
| 💤 Sleep / resume | Not verified | ❔ |

Unknown features are not marked as working merely because a driver package exists.

## Detailed status

### 🎆 Display & GPU

| Feature | Notes | Status |
| --- | --- | :---: |
| Basic display output | Usable Windows display output | ✅ |
| GMU firmware / HFI | Real firmware execution, version replies, table ACKs and core start verified | ✅ |
| GPU command submission / DMA | First owned-buffer CP write completed in **225 µs** on 2026-10-19 | ✅ |
| Hardware Direct3D / DXGI | Not verified; no accelerated WDDM capability claimed | ⚠️ |
| Draw / readback | Not verified | ❔ |
| Render fence / IRQ delivery | Not verified | ❔ |
| Brightness / panel switching | Not verified | ❔ |
| Long-term GPU stability | Not verified | ❔ |

The native CB0 + CB2 / LPAC experiment completed a real `CP_MEM_WRITE`, changing
the token from `0x660bad00` to `0x660c0001`. Hardware RPTR, shadow RPTR and WPTR
all reached 16. This proves command consumption and a private GPU memory write
for that sequence. Rendering still requires synchronization, a real Draw/readback
and hardware DXGI/Direct3D acceptance.

The subsequent original `CP_SET_SECURE_MODE(0)` packet experiment was prepared
and applied, but has no runtime result in the reviewed evidence. The functional
UEFI baseline was later restored with a verified readback. That restoration
does not establish a new GPU result.

### 👆 Touch

| Feature | Notes | Status |
| --- | --- | :---: |
| Main display touch | Physical input confirmed by the operator; ARM64 STM SPI driver included | ✅ |
| Cover display touch | Separate controller; not covered by the main-touch confirmation | ❔ |
| Multitouch limits / gestures | No separate acceptance result recorded | ❔ |

### 🛜 Wi-Fi

| Feature | Notes | Status |
| --- | --- | :---: |
| Connectivity | Operator-confirmed; UEFI PCIe/WLAN bring-up and pinned Qualcomm package included | ✅ |
| 2.4 GHz / 5 GHz individually | No separate per-band acceptance result recorded | ❔ |
| Wi-Fi Direct / Miracast | Not verified | ❔ |
| Throughput / sleep-resume stability | Not verified | ❔ |

### ⌨️ Side buttons

| Feature | Notes | Status |
| --- | --- | :---: |
| Volume up | Operator-confirmed working | ✅ |
| Volume down | Operator-confirmed working | ✅ |
| Power | Operator-confirmed working | ✅ |

The former 10-second UEFI pause is also confirmed removed.

### 🔧 Installation status

UEFI sources, ACPI tables, the main-touch driver, the PEP LMH fix and offline
installation tools are included. The 200% display-scaling setup is included;
reproduction on a clean installation remains pending. Installation and recovery
guides are still incomplete.

Experiment sources and evidence limits: [GPU research](../../research/gpu/README.md).
Page layout based on the [Xiaomi Pad 5 project status](https://github.com/erdilS/Port-Windows-11-Xiaomi-Pad-5/blob/main/guide/English/status.md);
the compatibility results here apply only to the Fold3.
