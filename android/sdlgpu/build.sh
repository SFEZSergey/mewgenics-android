#!/usr/bin/env bash
# Builds libaps5sdlgpu.so (arm64, bionic, API 30): SDL's Vulkan GPU backend + shim (see backend.c).
# usage: build.sh <output .so> ; SDL_SRC = SDL source at the title's SDL_GPUDevice layout.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/libaps5sdlgpu.so}"
SDL_COMMIT=2747162d9ab6fcb850991434eed9e109e543eebd
SDL_SRC="${SDL_SRC:-$HOME/ps5-android/src/sdl3-gpu-base/SDL-$SDL_COMMIT}"
SDL2_INC="${SDL2_INC:-$HOME/ps5-android/src/AnyPS5/3rdparty/SDL2/include}"
VK_INC="${VK_INC:-$HOME/ps5-android/src/AnyPS5/3rdparty/Vulkan-Headers/include}"
if [ ! -d "$SDL_SRC/src/gpu" ]; then
  mkdir -p "$(dirname "$SDL_SRC")"
  curl -sfL "https://codeload.github.com/libsdl-org/SDL/tar.gz/$SDL_COMMIT" | tar xz -C "$(dirname "$SDL_SRC")"
fi
python3 "$HERE/gen_adapters.py" "$SDL_SRC/src/gpu/SDL_sysgpu.h" "$HERE/adapters.inc" >/dev/null
OBJ="$(mktemp -d)"; trap 'rm -rf "$OBJ"' EXIT
CC="clang --target=aarch64-linux-android30 -fPIC -O2 -fvisibility=hidden -Wno-macro-redefined"
SDLFLAGS="-I$HERE/config -I$SDL_SRC/include -I$SDL_SRC/src -I$SDL_SRC/src/video/khronos -DSDL_GPU_VULKAN=1 -DSDL_VIDEO_VULKAN=1"
python3 "$HERE/patch_sdl.py" "$SDL_SRC/src/gpu/vulkan/SDL_gpu_vulkan.c" "$OBJ/SDL_gpu_vulkan_patched.c" >/dev/null
$CC $SDLFLAGS -I"$SDL_SRC/src/gpu/vulkan" -c "$OBJ/SDL_gpu_vulkan_patched.c" -o "$OBJ/SDL_gpu_vulkan.o"
for f in SDL_hashtable.c SDL_properties.c stdlib/SDL_murmur3.c; do
  $CC $SDLFLAGS -c "$SDL_SRC/src/$f" -o "$OBJ/$(basename "$f" .c).o"
done
$CC $SDLFLAGS -I"$HERE" -c "$HERE/shim.c" -o "$OBJ/shim.o"
$CC $SDLFLAGS -I"$HERE" -c "$HERE/backend.c" -o "$OBJ/backend.o"
$CC -I"$SDL2_INC" -I"$VK_INC" -I"$HERE" -c "$HERE/hostwin.c" -o "$OBJ/hostwin.o"
printf '{ global: Aps5SdlGpu_*; local: *; };\n' > "$OBJ/exports.map"
clang --target=aarch64-linux-android30 -shared -Wl,-Bsymbolic -Wl,--version-script="$OBJ/exports.map" -Wl,--no-undefined \
  -Wl,-soname,libaps5sdlgpu.so "$OBJ"/*.o -landroid -llog -ldl -lm -o "$OUT"
echo "built $OUT"
