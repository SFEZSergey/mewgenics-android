#!/data/data/com.termux/files/usr/bin/bash
# Полный цикл сборки и упаковки нативного ARM64 Bionic порта Mewgenics для Android.
set -euo pipefail
export HOME=/data/data/com.termux/files/home
export PATH=/data/data/com.termux/files/usr/bin:$PATH
export PREFIX=/data/data/com.termux/files/usr
REPO_DIR="${REPO_DIR:-$HOME/mewgenics-android}"
export SDLGPU_DIR="${SDLGPU_DIR:-$REPO_DIR/android/sdlgpu}"

echo "=== 1. Сборка нативного Vulkan SDL_GPU бэкенда (libaps5sdlgpu.so) ==="
cd "$SDLGPU_DIR"
bash build.sh /tmp/libaps5sdlgpu.so

echo "=== 2. Сборка нативной ARM64 Bionic группы AnyPS5 ==="
cd /data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-bionic
ninja aps5coreshim libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut libSceUserService libSceSystemService libSceRtc libSceCommonDialog libSceImeDialog libSceRandom libSceWebBrowserDialog libSceNetCtl libSceAudioIn libSceSaveData.native libSceSaveDataDialog.native

if [ -d "/data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-x64s" ]; then
  echo "=== 2b. Сборка x86-64 PRX заглушек нативной группы ==="
  cd /data/data/com.termux/files/home/ps5-android/src/AnyPS5/build-x64s
  ninja libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut libSceUserService libSceSystemService libSceRtc libSceCommonDialog libSceImeDialog libSceRandom libSceWebBrowserDialog libSceNetCtl libSceAudioIn libSceSaveData.native libSceSaveDataDialog.native
fi

echo "=== 3. Сборка и подпись APK ==="
cd "$REPO_DIR/android"
bash build.sh

echo "=== 4. Установка APK в систему ==="
cp "$REPO_DIR/android/out/Mewgenics.apk" /data/local/tmp/mewgenics.apk
pm install -r /data/local/tmp/mewgenics.apk

echo "=== УСПЕШНО ЗАВЕРШЕНО ==="
