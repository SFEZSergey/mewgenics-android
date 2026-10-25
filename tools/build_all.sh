#!/data/data/com.termux/files/usr/bin/bash
# Полный цикл сборки и упаковки нативного ARM64 Bionic порта Mewgenics для Android.
set -euo pipefail
export HOME=/data/data/com.termux/files/home
export PATH=/data/data/com.termux/files/usr/bin:$PATH
export PREFIX=/data/data/com.termux/files/usr
export SDLGPU_DIR=/data/data/com.termux/files/home/mewgenics-android/android/sdlgpu

echo "=== 1. Сборка нативного Vulkan SDL_GPU бэкенда (libaps5sdlgpu.so) ==="
cd "$SDLGPU_DIR"
bash build.sh /tmp/libaps5sdlgpu.so

echo "=== 2. Сборка нативной ARM64 Bionic группы AnyPS5 ==="
cd /data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-bionic
ninja aps5coreshim libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut libSceUserService libSceSystemService libSceRtc libSceCommonDialog libSceImeDialog libSceRandom libSceWebBrowserDialog

if [ -d "/data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-x64s" ]; then
  echo "=== 2b. Сборка x86-64 PRX заглушек нативной группы ==="
  cd /data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-x64s
  ninja libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut libSceUserService libSceSystemService libSceRtc libSceCommonDialog libSceImeDialog libSceRandom libSceWebBrowserDialog
fi

echo "=== 3. Сборка и подпись APK ==="
cd /data/data/com.termux/files/home/ps5-android/apk
bash build.sh

echo "=== 4. Установка APK в систему ==="
cp /data/data/com.termux/files/home/ps5-android/apk/out/PvZ-Replanted.apk /data/local/tmp/pvz.apk
pm install -r /data/local/tmp/pvz.apk

echo "=== УСПЕШНО ЗАВЕРШЕНО ==="
