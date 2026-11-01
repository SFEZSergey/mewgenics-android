#!/usr/bin/env bash
# Собирает полный комплект нативных ARM64 shims для Mewgenics через Android NDK
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NDK_BIN="${NDK_BIN:-/opt/android/android-ndk-r26d/toolchains/llvm/prebuilt/linux-x86_64/bin}"
export PATH="$NDK_BIN:$PATH"

CXX="clang++ --target=aarch64-linux-android30 -shared -fPIC -O3 -fvisibility=default"

echo "=== Сборка нативных ARM64 shims (замена эмулируемых PS5 PRX) ==="

compile() {
    local src="$1"
    local out="$HERE/$2"
    shift 2
    echo "  [+] Сборка $out ..."
    $CXX "$src" -o "$out" "$@"
}

compile "$HERE/src/AAudioAudioOut.cpp" "libSceAudioOut.so" -laaudio
compile "$HERE/../../AnyPS5/core/libs/prx/libSceVideoOut/src/MinimalVideoOut.cpp" "libSceVideoOut.so"
compile "$HERE/../../AnyPS5/core/libs/prx/libSceSysmodule/src/MinimalSysmodule.cpp" "libSceSysmodule.so"
compile "$HERE/src/MinimalCommonDialog.cpp" "libSceCommonDialog.so"
compile "$HERE/src/MinimalRandom.cpp" "libSceRandom.so"
compile "$HERE/src/MinimalUserService.cpp" "libSceUserService.so"
compile "$HERE/src/MinimalSystemService.cpp" "libSceSystemService.so"
compile "$HERE/src/MinimalRtc.cpp" "libSceRtc.so"
compile "$HERE/src/NativeMouse.cpp" "libSceMouse.so"

echo "=== Готово! Список собранных нативных библиотек: ==="
ls -lh "$HERE"/*.so
