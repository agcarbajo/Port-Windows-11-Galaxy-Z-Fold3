# GPU driver and games

[🏠 Home](../../README.md)

> [!IMPORTANT]
> The Adreno 660 driver package is **not released yet**. This page describes how it is installed
> and used on the development unit, and will apply to the first release. It also needs a UEFI
> with the GPU ACPI changes, which is not published yet either.

## What the package contains

| Component | Purpose |
| --- | --- |
| `Q2qA660Pnp.sys` | WDDM 2.0 display miniport: GPU, both panels, vsync, brightness, 60/120 Hz |
| `q2qa660umd*.dll` | Direct3D 10/11 user-mode driver (Mesa freedreno), ARM64X for ARM64 and x64 processes |
| `q2qa660vk*.dll` | Vulkan driver (Mesa turnip), ARM64X |
| `q2qa660gl*.dll` | OpenGL 4.6 (Mesa zink over Vulkan), ARM64X |
| `vulkan-1.dll` | Khronos Vulkan loader for ARM64 and x64 processes, copied to `System32` |
| `tools\q2q-game-dx.cmd` | Per-game DXVK / vkd3d-proton installer |

The GPU firmware (GMU, SQE, ZAP) is not part of the package: it is loaded from the Qualcomm
driver set installed by [`drivers/`](../../drivers/README.md).

## Requirements

- Windows installed with the q2q driver set ([drivers/README.md](../../drivers/README.md)).
- Test signing enabled in the phone's BCD (`testsigning on`): the package is signed with a
  development certificate.

## Install

From an administrator terminal **on the phone**:

```cmd
pnputil /add-driver <package folder>\q2qa660gpu.inf /install
```

Or offline from the PC, with the phone's Windows partition mounted as `X:`:

```cmd
dism /Image:X:\ /Add-Driver /Driver:<package folder>\q2qa660gpu.inf
```

After a reboot, Task Manager → Performance shows **GPU 0 – Adreno 660**. If another Vulkan runtime
already installed `System32\vulkan-1.dll`, the package keeps it; check with
`tools\q2q-game-dx.cmd -Game <any exe> -Status`.

**Update:** install the new package the same way; Windows selects the newer driver version.
**Remove:** `pnputil /delete-driver oemNN.inf /uninstall` (find `oemNN.inf` with
`pnputil /enum-drivers`). The three loader files in `System32` are left in place.

## Displays

- Refresh rate: Settings → System → Display → *(screen)* → Advanced display → 60 Hz / 120 Hz.
- Both panels are separate displays: use *Extend*, *Show only on 1* or *Show only on 2*.
- Known issue: the cover display at 60 Hz stops updating when it is the only active screen; keep it at 120 Hz.

## Games

Native Direct3D 11 (feature level 10_1), Vulkan and OpenGL work system-wide. Direct3D 9, 10, 11
(feature level 11_1) and 12 games use DXVK / vkd3d-proton next to the game executable:

```cmd
tools\q2q-game-dx.cmd -Game "D:\Games\Foo\Foo.exe" -Install
tools\q2q-game-dx.cmd -Game "D:\Games\Foo\Foo.exe" -Status
tools\q2q-game-dx.cmd -Game "D:\Games\Foo\Foo.exe" -Uninstall
```

| Option | Values |
| --- | --- |
| `-Api` | `dxvk` (D3D9/10/11), `vkd3d` (D3D12), `both` (default) |
| `-Arch` | `auto` (from the exe header, default), `arm64`, `x64` |
| `-WithLoader` | also put a Vulkan loader next to the game (only if the system loader is missing) |

The tool changes nothing outside the game folder and needs no registry edits. Files it replaces
are kept in `<game>\.q2q-backup\` and restored by `-Uninstall`.

Limits: 32-bit (x86) games are not supported (there is no 32-bit Vulkan driver).
