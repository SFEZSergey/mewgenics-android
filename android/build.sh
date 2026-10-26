#!/data/data/com.termux/files/usr/bin/bash
# Сборка APK в Termux. Нативное (arm64, bionic) — в lib/arm64-v8a: box64, SDL2, Turnip + хуки
# adrenotools, нативная группа AnyPS5 (libSceX.prx упакована как libSceX.so). x86-64 библиотеки
# AnyPS5 (заглушки нативной группы + остальные PRX) — в assets/runtime/prx, копируются в game/libs.
# Данные игры в APK не попадают.
set -euo pipefail

APK="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$APK/.." && pwd)"
PS5_ROOT="${PS5_ROOT:-$HOME/ps5-android}"

SDK="${ANDROID_SDK:-$PS5_ROOT/android-sdk/platforms/android-35/android.jar}"
SDL="${SDL_JAVA:-$PS5_ROOT/src/AnyPS5/3rdparty/SDL2/android-project/app/src/main/java}"
OUT="$APK/out"
GEN="$OUT/gen"
CLASSES="$OUT/classes"
BIONIC="${BIONIC_DIR:-$PS5_ROOT/src/AnyPS5/build-bionic}"
ADRENO="${ADRENO_DIR:-$PS5_ROOT/src/libadrenotools-master}"
X86STUBS="${X86STUBS:-$PS5_ROOT/src/AnyPS5/build-x64s/core/libs/libs}"
X86LIBS="${X86LIBS:-$PS5_ROOT/src/AnyPS5/build-x64s/core/libs/libs}"
LIB="$OUT/lib/arm64-v8a"
PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"

rm -rf "$OUT"; mkdir -p "$GEN" "$CLASSES" "$LIB" "$OUT/assets/runtime/prx" "$OUT/assets/runtime/x64lib"
aapt package -f -m -J "$GEN" -S "$APK/res" -M "$APK/AndroidManifest.xml" -I "$SDK" -F "$OUT/resources.ap_"
find "$APK/src" "$SDL" "$GEN" -name '*.java' -print > "$OUT/sources.list"
javac -nowarn -source 8 -target 8 -classpath "$SDK" -d "$CLASSES" $(cat "$OUT/sources.list") > "$OUT/javac.log" 2>&1 || { grep -v "^warning\|^Note" "$OUT/javac.log"; exit 1; }
grep -v "^warning\|^Note" "$OUT/javac.log" || true
d8 --min-api 26 --lib "$SDK" --output "$OUT" $(find "$CLASSES" -name "*.class" -print)

# Нативные библиотеки
cp "${BOX64_LIB:-$PS5_ROOT/src/box64/build-lib/libbox64.so}" "$LIB/"
cp "$PREFIX/lib/libandroid-sysv-semaphore.so" "$PREFIX/lib/libiconv.so" "$PREFIX/lib/libc++_shared.so" "$LIB/"
cp "${RELINKER:-$PS5_ROOT/bin/relinker}" "$LIB/librelinker.so"
cp "$BIONIC/3rdparty/SDL2/libSDL2.so.2.33.0" "$LIB/libSDL2.so"
patchelf --set-soname libSDL2.so "$LIB/libSDL2.so"
cp "${MESA_TURNIP:-$PS5_ROOT/src/mesa-26.3/build-android/src/freedreno/vulkan/libvulkan_freedreno.so}" "$LIB/"
patchelf --replace-needed libz.so.1 libz.so "$LIB/libvulkan_freedreno.so"
cp "$ADRENO"/build/src/hook/lib{hook_impl,main_hook,file_redirect_hook,gsl_alloc_hook}.so "$LIB/"
cp "$BIONIC/core/libs/prx/aps5native/libaps5coreshim.so" "$LIB/"
# Native Vulkan SDL_GPU backend for SDL_GPU titles (Mewgenics, APS5_SDLGPU_VULKAN=1; box64 aps5hot loads it).
bash "${SDLGPU_DIR:-$APK/sdlgpu}/build.sh" "$LIB/libaps5sdlgpu.so"
GROUP="libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut"
for lib in $GROUP; do
  cp "$BIONIC/core/libs/libs/unpatched/$lib.prx" "$LIB/$lib.so"
  patchelf --set-soname "$lib.so" --replace-needed libSDL2.so.0 libSDL2.so "$LIB/$lib.so"
  for dep in $GROUP; do patchelf --replace-needed "$dep.prx" "$dep.so" "$LIB/$lib.so"; done
done
clang --target=aarch64-linux-android30 -fPIC -shared -O2 -Wall -Wextra -I"$ADRENO/include" "$APK/native/sdl_main.c" \
  -L"$LIB" -lbox64 "$ADRENO/build/libadrenotools.a" "$ADRENO/build/lib/linkernsbypass/liblinkernsbypass.a" -lc++_shared -landroid -ldl -llog \
  -Wl,-soname,libmain.so -o "$LIB/libmain.so"
clang --target=aarch64-linux-android26 -fPIC -shared -O2 -Wall -Wextra -isystem "${NDK_SHIM:-$PS5_ROOT/android-sdk/ndk-shim/include}" "$APK/native/entry.c" -o "$LIB/libanyps5_android.so"

# x86-64 часть: все обычные файлы рабочей раскладки (без отладочных хуков) + свежие заглушки.
for f in "$X86LIBS"/*.prx; do [ -L "$f" ] || cp "$f" "$OUT/assets/runtime/prx/"; done
for lib in $GROUP; do cp "$X86STUBS/$lib.prx" "$OUT/assets/runtime/prx/"; done
cp "${X64LIB_DIR:-$PS5_ROOT/x64lib}"/* "$OUT/assets/runtime/x64lib/"
[ -d "$APK/assets" ] && cp -r "$APK/assets/." "$OUT/assets/"

# Компактность: отладочная информация не нужна в APK (таблица символов остаётся — имена в бэктрейсах),
# ассеты сжимаются (лаунчер распаковывает их потоком при импорте).
for f in "$LIB"/*.so "$OUT"/assets/runtime/prx/*.prx; do llvm-strip --strip-debug "$f"; done

cp "$OUT/resources.ap_" "$OUT/AnyPS5-unsigned.apk"
(cd "$OUT" && zip -q -9 -r AnyPS5-unsigned.apk assets && zip -q -r AnyPS5-unsigned.apk classes.dex lib)
KEY="$APK/debug.keystore"
if [ ! -f "$KEY" ]; then keytool -genkeypair -keystore "$KEY" -storepass android -keypass android -alias androiddebugkey -dname 'CN=Android Debug,O=Android,C=US' -keyalg RSA -keysize 2048 -validity 10000 >/dev/null 2>&1; fi
zipalign -f -p 4 "$OUT/AnyPS5-unsigned.apk" "$OUT/AnyPS5-aligned.apk"
apksigner sign --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true --ks "$KEY" --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey --out "$OUT/Mewgenics.apk" "$OUT/AnyPS5-aligned.apk"
apksigner verify "$OUT/Mewgenics.apk"
echo "Built $OUT/Mewgenics.apk"
