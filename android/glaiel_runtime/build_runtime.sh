#!/usr/bin/env bash
# Сборка нативного рантайма GlaielEngine под ARM64 (Android Bionic)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NDK_BIN="${NDK_BIN:-/opt/android/android-ndk-r26d/toolchains/llvm/prebuilt/linux-x86_64/bin}"
export PATH="$NDK_BIN:$PATH"

CXX="clang++ --target=aarch64-linux-android30 -fPIC -O3 -std=c++20"
CC="clang --target=aarch64-linux-android30 -fPIC -O3"

echo "=== Сборка Glaiel Native Engine (ARM64) ==="

OBJ="$(mktemp -d)"
trap 'rm -rf "$OBJ"' EXIT

echo "  [+] Компиляция libtess2 (векторная триангуляция)..."
for f in "$HERE"/libtess2/Source/*.c; do
    $CC -I"$HERE/libtess2/Include" -I"$HERE/libtess2/Source" -c "$f" -o "$OBJ/$(basename "$f" .c).o"
done

echo "  [+] Компиляция GON (Glaiel Object Notation)..."
$CXX -I"$HERE" -c "$HERE/gon.cpp" -o "$OBJ/gon.o"

echo "  [+] Компиляция GlaielEngine..."
$CXX -I"$HERE" -I"$HERE/libtess2/Include" -c "$HERE/GlaielEngine.cpp" -o "$OBJ/GlaielEngine.o"

echo "  [+] Линковка libglaiel_native.so..."
$CXX -shared -Wl,-soname,libglaiel_native.so "$OBJ"/*.o -llog -lm -o "$HERE/libglaiel_native.so"

echo "=== Успешно собрано: ==="
ls -lh "$HERE/libglaiel_native.so"
