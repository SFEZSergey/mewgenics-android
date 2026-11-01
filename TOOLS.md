# Tools and commands

Environment: Termux on a rooted phone (KernelSU). App package on the phone: `org.anyps5.android`.
Game directory: `/data/data/org.anyps5.android/files/games/PPSA37318` (referred to below as `$G`).
Game log: `$G/box64.log` (overwritten on each launch).

## Working copies on the phone

| Component | Location |
|---|---|
| AnyPS5 | `~/ps5-android/src/AnyPS5` (build: `build-bionic`; x86 part: `build-x64s`) |
| Mesa/Turnip | `~/ps5-android/src/mesa-26.3` (build: `build-android`) |
| box64 | `~/ps5-android/src/box64` (APK uses `build-lib/libbox64.so`) |
| SDL3 for the backend | `~/ps5-android/src/sdl3-gpu-base/SDL-2747162d…` (downloaded automatically) |
| APK | `~/ps5-android/apk` (`build.sh`; output: `out/PvZ-Replanted.apk`) |
| PvZ repository (shared scripts) | `~/ps5-android/pvz-replanted-android` |

## Build and install

```sh
bash tools/github.sh                 # Full pipeline: AnyPS5 patches, Turnip, APK, install, env.txt, shaders
SKIP_MESA=1 KEEP_ENV=1 bash tools/github.sh   # Skip Turnip rebuild and leave env.txt unchanged

# box64 only (changes in src/tools/aps5hot.c or dynarec):
cd ~/ps5-android/src/box64 && cmake -DANDROID=ON build-lib && cmake --build build-lib -j8
#   IMPORTANT: always pass -DANDROID=ON, or CMake resets ANDROID=OFF

# Native backend + APK only:
bash android/sdlgpu/build.sh /tmp/libaps5sdlgpu.so    # Verify the backend build
cd ~/ps5-android/apk && bash build.sh                  # Builds the APK (also calls sdlgpu/build.sh)
su -c 'cp ~/ps5-android/apk/out/PvZ-Replanted.apk /data/local/tmp/pvz.apk && pm install -r /data/local/tmp/pvz.apk'

# Start / stop the game:
su -c 'am start -n org.anyps5.android/.GameActivity'
su -c 'am force-stop org.anyps5.android'
```

## Scripts in `tools/`

| Script | Purpose |
|---|---|
| `github.sh` | Full build pipeline (steps 1–9); options are at the top of the file |
| `mew-sdlgpu-shaders.sh` | Builds `$G/sdlgpu/*.spv` from game shaders (requires Python, glslang, and spirv-tools) |
| `sdlgpu/glsl2spv.py` | Converts game GLSL to SPIR-V in SDL_GPU layout (for programs without ready-made SPIR-V) |
| `sdlgpu/maketable.py` | Builds an FNV PSSL-blob-to-SPIR-V table and `index.txt` |
| `sdlgpu/spvif.py`, `cmpall.sh`, `desc.py` | Compare SPIR-V interfaces and parse `.desc` uniform layouts |
| `mew1080.sh` | Patches `app.elf` to use 1080p instead of 4K (`undo` reverts the patch) |
| `mew-native.sh` | Patches `app.elf` to use native `lzcnt` in the TLSF allocator (`undo` reverts the patch) |
| `mew-atlas.sh` | Fixes the 4096 SWF atlas in `app.elf` to prevent chapter-video crashes (`undo` reverts the patch) |
| `sesscap.sh` | Passive session collector (root; started by `/data/adb/service.d/sesscap-start.sh`): FPS, threads, simpleperf, and screenshots every minute → `/data/local/tmp/sesscap/S<date>/` |
| `jitmap.py` | Maps box64 dynarec blocks in a running process |
| `psdis.sh` | Disassembles PSSL shaders (RDNA2 via `llvm-mc`) |
| `bc3png.py`, `bmp2png.py` | Converts BC3 dumps or BMP files to PNG |
| `fpcr-round-test.c` | Tests `cvt*` rounding in box64 (all four MXCSR modes) |

## `env.txt` options (`$G/env.txt`)

| Variable | Description |
|---|---|
| `APS5_SDLGPU_VULKAN=1` | Use native Vulkan instead of Agc (primary option) |
| `APS5_SDLGPU_CANVAS=WxH` | Game canvas size for letterboxing (`0` disables it; default: 1920x1080) |
| `APS5_SDLGPU_NO_ZERO_INIT=1` | Do not zero-initialize new textures |
| `APS5_SDLGPU_CYCLE=1` | Pass the game's `cycle` flags through unchanged |
| `APS5_SDLGPU_KEEP_DONT_CARE=1` | Do not replace `DONT_CARE` |
| `APS5_SDLGPU_UPLOADED_LOD0=1` | Upload textures at mip level 0 only (experimental) |
| `APS5_SDLGPU_AUTO_MIPS=1`, `APS5_SDLGPU_TARGET_MIPS=1` | Generate mipmaps (experimental; degrades the image) |
| `APS5_SDLGPU_SKIP_FS=<hash,…>` / `APS5_SDLGPU_NEAREST_FS=<hash,…\|all>` | Debug: skip drawing a shader / use unfiltered samplers for a shader |
| `APS5_SDLGPU_COUNTS=0` | Disable call counters in the log |
| `APS5_SDLGPU_VERBOSE=1` | Enable verbose SDL logging |
| `APS5_DUMP_TRIGGER=$G/dumpnow` | `touch $G/dumpnow` dumps a frame to `$G/frame_000.bmp` |
| `APS5_NO_SUBMISSION_REAPER=1` | Disable the Agc-path submission reaper (it was causing heat) |

## Diagnostics

```sh
# Backend output:
su -c "grep -a -E 'sdlgpu|^\\[fps\\]|aps5hot' $G/box64.log | tail -50"
# SDL_GPU call counters, updated every 5 seconds:
su -c "grep -a 'sdlgpu\\] calls' $G/box64.log | tail -2"
# Dump atlas levels 0–3 and view them:
su -c "touch $G/texdump"; sleep 3; su -c "rm $G/texdump"
python3 tools/bc3png.py tex_L0.raw 2048 2048 out.png 1024
# Disassemble a PS5 shader:
tools/psdis.sh $G/shaders/pssl/default_full.shader.fs | grep image_
# Screenshots from the most recent collector session:
su -c 'ls /data/local/tmp/sesscap/$(ls /data/local/tmp/sesscap | grep ^S | sort | tail -1)/shots'
```

## Key `app.elf` addresses (PPSA37318)

* SDL_GPU frontend at approximately `0x114b000`; backend selection at `0x114bb40`; Agc loader at `0x1a6e458`
  (`{"agc", PrepareDriver 0x126e260, CreateDevice 0x126e2d0}`); Agc backend at `0x126c000–0x1298000`.
* `SDL_GPUDevice` is `0x2d0` bytes (SDL main with OpenXR, commit `2747162d`): 87 slots, `driverData` at `0x2b8`.
