# Adreno 660 and display bring-up

[🏠 Home](../README.md)

Summary of how GPU acceleration and both displays were brought up on the Fold3. The driver
package is not released yet; this page documents the design and the main findings.

## Stack

```
 D3D11 app ──► q2qa660umd (Mesa d3d10umd + freedreno, ARM64X) ─┐
 D3D9–12 game ► DXVK / vkd3d-proton ─► turnip (Vulkan, ARM64X) ─┤
 OpenGL app ──► zink (WGL, ARM64X) ──► turnip ──────────────────┤
                                                                ▼
                     Q2qA660Pnp.sys — WDDM 2.0 miniport (kernel)
                       GPU: GMU / HFI power-up, ZAP, CP ringbuffer, SMMU, fences, DCVS
                       Display: MDP scanout of both panels, vsync, DSI commands
```

- **Kernel driver.** Our own WDDM 2.0 miniport, started from the Adreno 610 driver of
  [topaz-woa](https://github.com/MakrSas/topaz-woa) and rewritten for the A660 and the Fold3
  panels. It exposes an msm-DRM-like interface (buffer objects, submits, fences) to user mode
  through WDDM escapes, so Mesa's freedreno and turnip drivers run with small Windows backends.
- **GPU power-up.** The GMU runs Qualcomm's original firmware from the installed BSP; the driver
  speaks HFI to it (version, performance and bandwidth tables, core start), loads the ZAP shader
  through the original PIL service and brings up the command processor. GPU memory is mapped
  through SMMU context banks owned by the driver.
- **User mode.** Mesa's D3D10/11 frontend (native D3D11, feature level 10_1), turnip (Vulkan) and
  zink (OpenGL 4.6). Each DLL is ARM64X: an ARM64 half for native processes and an ARM64EC half for
  emulated x64 processes. The Khronos loader is installed system-wide and finds the Adreno ICD
  through the adapter's `VulkanDriverName` value.
- **Direct3D 9/10/12.** DXVK and vkd3d-proton per game (the native driver has no D3D9 interface).

## Display

- The UEFI leaves both panels lit on the MDP (one shared framebuffer); the driver takes over its
  pipes instead of re-initialising the display.
- DWM flips are scanned out directly from physically contiguous primary buffers (VIG pipes + CTL
  flush after the buffer's fence); the CPU copy is only a fallback.
- The cover panel is a second VidPN source/target with its own EDID (physical size, so Windows picks
  100–125 % scaling) and its own refresh rate.
- 60/120 Hz: both panels run their 120 Hz mode; the selected Windows mode sets the vsync deadline,
  driven by a 0.5 ms display tick.
- Brightness for each panel goes through DSI DCS commands on its own DSI link.

## Findings that mattered

| Problem | Cause / fix | Effect |
| --- | --- | --- |
| Laggy window drag (16.8 ms/frame) | The D3D UMD flushed on every no-overwrite map (~50 submits per DWM frame); flush only before discard maps | 8.3 ms (120 fps) with both screens |
| Slow Vulkan / DXVK present | WSI copied each frame from write-combined memory; cacheable buffers + one `SetDIBitsToDevice` per frame | 1.5–3.3× faster present |
| Vulkan present not paced to the display | FIFO present waits for the real vblank of the window's screen | Vsync-paced Vulkan / DXVK games |
| Touch keyboard drawing glitches | Missing synchronisation of mapped buffers in the UMD | Fixed |
| GPU only after ~35 s | Early `DriverEntry` failed on a registry flush during boot; volatile key instead | GPU from the first frame |
| Incomplete WebGL canvases in browsers | 4× MSAA support enabled by default in the UMD | WebGL renders correctly (~85–95 fps) |

## Open issues

- Cover display at 60 Hz as the only active screen: Windows keeps turning that source's
  visibility off; under investigation.
- OpenGL through zink is slower than Vulkan in small windows; cause still under investigation.
- No 32-bit Vulkan driver, so no x86 games.
- Signing for clean installs (currently a development certificate with test signing).
