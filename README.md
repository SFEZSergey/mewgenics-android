# Mewgenics-android

An Android (ARM64) port of Mewgenics (PS5, PPSA37318) running on top of AnyPS5, box64, and Turnip.  
Tested on Snapdragon 8 Elite (Adreno 830), reaching stable 60 FPS in combat.

> **Note:** Game assets and binaries are NOT included and will never be distributed here. Users must provide their own legitimately dumped game files.  
> AnyPS5 is licensed under GPL-2.0-only; source code and applied patches are published alongside this port.

---

## Releases & Downloads

- **Current Version:** `v0.1.5-beta`
- **Download APK:** [`Mewgenics.apk`](https://github.com/SFEZSergey/mewgenics-android/releases/tag/v0.1.5-Beta) from GitHub Releases

### Key Features in v0.1.5-beta:
* **Native SDL_GPU Vulkan:** Bypasses heavy PS5 AGC translation; renders directly on Qualcomm Adreno GPU via Turnip (Mesa 26.3).
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
