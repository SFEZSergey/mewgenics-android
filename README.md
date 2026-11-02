# Mewgenics-android

An Android (ARM64) port of Mewgenics (PS5, PPSA37318) running on top of AnyPS5, box64, and Turnip.  
Tested on Snapdragon 8 Elite (Adreno 830), reaching stable 60 FPS in combat.

> **Note:** Game assets and binaries are NOT included and will never be distributed here. Users must provide their own legitimately dumped game files.  
> AnyPS5 is licensed under GPL-2.0-only; source code and applied patches are published alongside this port.

---

## Releases & Downloads

- **Current Version:** `v0.1.10-beta`
- **Download APK:** [`Mewgenics-0.1.10-beta.apk`](https://github.com/SFEZSergey/mewgenics-android/releases/download/v0.1.10-Beta/Mewgenics-0.1.10-beta.apk)

### Changes in v0.1.10-beta
* **Automatic game patching:** The launcher checks and applies the complete supported `app.elf` patch set in the background whenever it opens or resumes, and verifies the result before starting the game.
* **Safer patching:** Patch signatures are checked as a complete set before writing, and the `patch_app_elf.py --check` command verifies a file without modifying it.
* **Native ARM64 LZ4 decoder:** Box64 redirects the signature-checked Mewgenics block decoder to a bounds-checked ARM64 implementation while preserving the game's wrapper and allocation logic.
* **Mali diagnostics:** Added optional Vulkan robust buffer/image access handling for Mali and a slow per-submit fence tracing mode for graphics diagnostics.
* **ELF analysis tools:** Added a read-only app.elf inventory and profile-correlation script, with notes about address and profiling limits.

### Key Features in v0.1.7-beta:
* **MediaTek & ARM Mali GPU Support:** Fixed startup crashes on MediaTek (Dimensity, MT6899, etc.) and ARM Mali GPUs (Mali-G720 / Immortalis). Relaxed strict desktop Vulkan feature requirements (`sampleRateShading`, `depthClamp`, `drawIndirectFirstInstance`, `shaderClipDistance`) and enabled compatibility with core Vulkan 1.1+ drivers without requiring optional extensions (`VK_KHR_maintenance1`).
* **Native SDL_GPU Vulkan:** Bypasses heavy PS5 AGC translation; renders directly on Qualcomm Adreno (via Turnip) and ARM Mali / MediaTek GPUs.
* **Mali texture fallback:** Decodes BC1–BC7 uploads to native Vulkan formats when the device cannot sample the source BC format.
* **Native AAudio Engine:** Low-latency 48 kHz stereo audio output with hardware audio clock synchronization.
* **Refined Touch Controls:**
  * Single tap: Left click.
  * Two-finger tap: Right click.
  * Touch hover / move: Inspect cat stats without accidental dragging.
  * Long press (350 ms with haptic vibration): Grab & move cats/items.
* **Native ARM64 PRX Shims:** Direct ARM64 implementations for `libSceImeDialog`, `libSceSaveData`, `libSceAudioOut`, `libSceMouse`, `libSceVideoOut`, `libSceUserService`, `libSceSystemService`, `libSceRandom`, `libSceRtc`, and more.
* **Custom Optimizations:** Native TLSF allocator bit-scan routines (`lzcnt`/`tzcnt` fast paths), eliminating x86 emulation overhead on memory allocation loops (+17% CPU boost).
* **Glaiel Runtime:** Native ARM64 runtime support (`libglaiel_native.so`) including GON parser and libtess2 polygon tessellation.

---

## How It Works

```
app.elf (x86-64, game binary) ──box64──▶ SDL_GPU game frontend (x86)
                                              │  SDL_GPUDevice slot hooks
                                              ▼
                              libaps5sdlgpu.so (ARM64): SDL3 Vulkan backend + glue layer
                                              ▼
                                   Turnip (Mesa, libvulkan_freedreno.so)
```

1. **Vulkan Rendering Pipeline:**
   * The game renders through SDL3 `SDL_GPU`. Its only backend in `app.elf` is the PS5 `Agc` backend.
   * box64 (`aps5hot.c`) intercepts `CreateDevice` (`0x1a6e468`, code hash `0xfb170c2bbebbdcf3`) and redirects it to the native ARM64 SDL3 Vulkan backend.
   * If native initialization fails, the game stays on Agc (the AnyPS5 path: AGC → PM4 → Vulkan).
   * Shaders: The game submits PSSL blobs; the backend substitutes pre-compiled SPIR-V binaries matched by FNV-1a hash (`<game>/sdlgpu/<hash>.spv`, compiled via `tools/mew-sdlgpu-shaders.sh`).

2. **PS5 to Vulkan Translation Quirks Solved (`android/sdlgpu/backend.c`):**
   * Unaligned compressed texture upload offsets (offset % 16 == 4) are copied into aligned bounce buffers, eliminating texture corruption artifacts.
   * Initialized textures are zero-cleared on creation (mirroring PS5 zero-fill behavior).
   * `bitmap_atlas_info` (R32_FLOAT 9100x1 table) is copied into a storage buffer and bound automatically because the game does not bind it.
   * `cycle` disabled on GPU texture writes; `DONT_CARE` load/store ops mapped cleanly.
   * Letterboxing: the game renders to a 1920x1080 canvas, which is scaled and centered on mobile displays.
   * SDL bug fix: `Download*` lacked a `TRANSFER_WRITE → HOST_READ` barrier (causing the CPU to read zeros on Turnip); patched during build by `android/sdlgpu/patch_sdl.py`.

---

## Repository Structure

| Path | Description |
|---|---|
| `AnyPS5/` | PS5 compatibility layer with applied patches from `patches/` |
| `mesa/` | Mesa 26.3 (Turnip Adreno Vulkan driver) with Android patches |
| `android/` | Android project files, TouchControls, and native ARM64 shims |
| `android/sdlgpu/` | Native Vulkan SDL_GPU backend (`libaps5sdlgpu.so`) |
| `android/shims/` | Native ARM64 implementations replacing emulated PRX modules |
| `patches/` | Complete patch stack for AnyPS5, Mesa, and box64 (`box64-local.patch`) |
| `tools/` | Build, installation, and diagnostic scripts (see `TOOLS.md`) |

---

## Building & Installation

To build the APK from source on Linux/WSL with the Android NDK:

```bash
# 1. Build ARM64 shims
bash android/shims/build_shims.sh

# 2. Package into Mewgenics.apk and sign
python3 tools/update_apk.py
apksigner sign --ks android/debug.keystore --ks-pass pass:android Mewgenics.apk

# 3. Deploy to device via ADB
adb push Mewgenics.apk /data/local/tmp/
adb shell "su -c 'pm install -r /data/local/tmp/Mewgenics.apk; rm /data/local/tmp/Mewgenics.apk'"
```

For more tooling details, refer to [TOOLS.md](file:///c:/Mewgenics/TOOLS.md). For AI assistant guidance, see [AGENTS.md](file:///c:/Mewgenics/AGENTS.md).
