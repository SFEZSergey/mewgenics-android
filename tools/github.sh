#!/usr/bin/env bash
# ==============================================================================
# AnyPS5 + Mewgenics (PPSA37318) — патчи, Turnip, APK, установка и env.txt
# Платформа: Snapdragon 8 Elite (Adreno 830) / Termux
# Запуск: bash github.sh
# Опции (перед командой): SKIP_MESA=1 — не пересобирать Turnip; KEEP_ENV=1 — не трогать env.txt;
#                          CLEAN_CACHE=1 — стереть кэш шейдеров; GAME_ID=PPSA29802 — другая игра;
#                          MEW1080=0 — оставить Mewgenics в 4K (по умолчанию 1080p: tools/mew1080.sh).
# ==============================================================================
set -uo pipefail

ok()   { echo -e "\033[32m[OK]\033[0m $*"; }
fail() { echo -e "\033[31m[FAIL]\033[0m $*"; }

# 0. Пути (первый существующий вариант)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
[ -f "$SCRIPT_DIR/README.md" ] && SELF_REPO="$SCRIPT_DIR" || SELF_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

for cand in "${REPO_DIR:-}" "$HOME/mewgenics-android" "$SELF_REPO" "$HOME/ps5-android/pvz-replanted-android" "$HOME/pvz-replanted-android"; do
  if [ -n "$cand" ] && [ -d "$cand" ]; then REPO_DIR="$cand"; break; fi
done
PVZ_DIR="${PVZ_DIR:-$HOME/ps5-android/pvz-replanted-android}"
[ -d "$PVZ_DIR" ] || PVZ_DIR="$HOME/pvz-replanted-android"

A="${ANYPS5_DIR:-$HOME/ps5-android/src/AnyPS5}"
[ -d "$A" ] || A="$HOME/ps5-android/AnyPS5"
[ -d "$A" ] || A="$REPO_DIR/AnyPS5"
MESA="${MESA_DIR:-$HOME/ps5-android/src/mesa-26.3}"
[ -d "$MESA" ] || MESA="$REPO_DIR/mesa"
MESA_BUILD="${MESA_BUILD:-$MESA/build-android}"
APK_DIR="${APK_DIR:-$HOME/ps5-android/apk}"
[ -d "$APK_DIR" ] || APK_DIR="$PVZ_DIR/android"
[ -d "$APK_DIR" ] || APK_DIR="$REPO_DIR/android"

# Исходники каркаса APK (AndroidManifest.xml, res, src, native)
APK_SRC="$REPO_DIR/android"
[ -f "$APK_SRC/AndroidManifest.xml" ] || APK_SRC="$PVZ_DIR/android"

GAME_ID="${GAME_ID:-PPSA37318}"
P="$REPO_DIR/patches"

echo "=== AnyPS5 Master Build ==="
echo "Repo:   $REPO_DIR"; echo "AnyPS5: $A"; echo "Mesa:   $MESA ($MESA_BUILD)"; echo "APK:    $APK_DIR"
for d in "$REPO_DIR" "$A"; do [ -d "$d" ] || { fail "нет папки $d"; exit 1; }; done

# 1. Свежие патчи из репозитория
git -C "$REPO_DIR" pull --ff-only || fail "git pull (продолжаю с тем, что есть)"

# 2. AnyPS5: исходники как в коммите и все 6 патчей по порядку. Папки build-* в .gitignore — clean их не трогает.
echo "=== Патчи AnyPS5 ==="
git -C "$A" checkout -- . && git -C "$A" clean -fdq
for p in anyps5-mouse-absolute anyps5-volume-2darray anyps5-line-on-plane anyps5-skip-failed-shaders anyps5-pass-timing anyps5-host-ftz anyps5-present-letterbox anyps5-mouse-tap-latch anyps5-pass-stats anyps5-watch-maps-once anyps5-import-lease-check anyps5-hub-cpu anyps5-debug-lod0 anyps5-debug-target-trace anyps5-import-change-log anyps5-draw-entry-reaper anyps5-depth-fill-clear anyps5-debug-draw-key anyps5-draw-cpu-trims anyps5-draw-key-reuse anyps5-result-memo-filter anyps5-gpu-map-batch anyps5-direct-memory-punch anyps5-direct-alloc-fast anyps5-direct-page-runs anyps5-driver-alloc-trims anyps5-driver-cpu-trims2 anyps5-window-leases anyps5-lease-mirror anyps5-watch-fast anyps5-direct-phys-index anyps5-depth-reuse anyps5-vquery-point anyps5-agc-packet-inline anyps5-submission-reaper anyps5-exception-string anyps5-fast-vfs; do
  [ -f "$P/$p.patch" ] || { fail "нет $P/$p.patch"; exit 1; }
  if git -C "$A" apply --whitespace=nowarn "$P/$p.patch"; then ok "$p"; else fail "$p — сборка остановлена"; exit 1; fi
done

# 3. Turnip: файлы из патча — как в коммите, патч, пересборка одной библиотеки
if [ "${SKIP_MESA:-0}" != 1 ]; then
  echo "=== Turnip (Mesa) ==="
  if [ -d "$MESA/.git" ] && [ -d "$MESA_BUILD" ]; then
    git -C "$MESA" checkout -- $(grep '^+++ b/' "$P/mesa-26.3-android.patch" | cut -c7- | sort -u)
    if git -C "$MESA" apply --whitespace=nowarn "$P/mesa-26.3-android.patch"; then ok "mesa-26.3-android"; else fail "mesa-26.3-android"; exit 1; fi
    ninja -C "$MESA_BUILD" src/freedreno/vulkan/libvulkan_freedreno.so 2>&1 | tail -n 3
    [ "${PIPESTATUS[0]}" = 0 ] && ok "Turnip собран" || { fail "сборка Turnip"; exit 1; }
  else
    fail "нет $MESA/.git или $MESA_BUILD — Turnip пропущен (укажи MESA_DIR / MESA_BUILD)"
  fi
fi

# 4. Нативная группа AnyPS5 (bionic)
echo "=== Нативная группа AnyPS5 (bionic) ==="
APS5_TARGETS="aps5coreshim libSceAgcDriver libSceVideoOut libScePad libSceMouse libSceKeyboard libSceAudioOut"
cmake --build "$A/build-bionic" --target $APS5_TARGETS -j"$(nproc)" 2>&1 | tail -n 5
[ "${PIPESTATUS[0]}" = 0 ] && ok "AnyPS5 собран" || { fail "сборка AnyPS5"; exit 1; }
# x86-64 libc и libkernel (идут в APK из build-x64s через build.sh)
if [ -d "$A/build-x64s" ]; then
  ninja -C "$A/build-x64s" core/libs/libs/libc.prx core/libs/libs/libkernel.prx core/libs/libs/libSceAgc.prx $APS5_TARGETS 2>&1 | tail -n 5
  [ "${PIPESTATUS[0]}" = 0 ] && ok "x86 libc/libkernel собраны" || { fail "сборка x86 libc/libkernel"; exit 1; }
fi

# 5. APK: в папку сборки — свежие лаунчер и libmain из репозитория (build.sh там свой, с путями Termux)
echo "=== APK ==="
export SDLGPU_DIR="${SDLGPU_DIR:-$REPO_DIR/android/sdlgpu}"
if [ -d "$APK_DIR" ]; then
  [ -f "$APK_SRC/native/sdl_main.c" ] && cp "$APK_SRC/native/sdl_main.c" "$APK_DIR/native/"
  [ -d "$APK_SRC/src" ] && cp -r "$APK_SRC/src/." "$APK_DIR/src/"
  [ -d "$APK_SRC/res" ] && cp -r "$APK_SRC/res/." "$APK_DIR/res/"
  [ -f "$APK_SRC/AndroidManifest.xml" ] && cp "$APK_SRC/AndroidManifest.xml" "$APK_DIR/"
  (cd "$APK_DIR" && bash build.sh) || { fail "build.sh"; exit 1; }
  APK_FILE="$APK_DIR/out/PvZ-Replanted.apk"
else
  (cd "$REPO_DIR/android" && bash build.sh) || { fail "build.sh"; exit 1; }
  APK_FILE="$REPO_DIR/android/out/PvZ-Replanted.apk"
fi
[ -f "$APK_FILE" ] || { fail "нет $APK_FILE"; exit 1; }
su -c "cp '$APK_FILE' /data/local/tmp/pvz.apk && chmod 644 /data/local/tmp/pvz.apk && pm install -r /data/local/tmp/pvz.apk" \
  && ok "APK установлен" || { fail "установка APK"; exit 1; }

# 6. env.txt игры. Всё остальное APK выставляет сам: APS5_HOST_FTZ/NAN/FMA/DPP, quad DPP, конвейер отрисовок.
#    Не дописывай сюда «ВЫКЛЮЧАТЕЛЬ=0» для переменных, которые проверяются по наличию: это их включает.
if [ "${KEEP_ENV:-0}" != 1 ]; then
  echo "=== env.txt ($GAME_ID) ==="
  su -c "
G=/data/data/org.anyps5.android/files/games/$GAME_ID
[ -d \$G ] || { echo 'игра $GAME_ID не импортирована'; exit 0; }
[ -f \$G/env.txt ] && cp \$G/env.txt \$G/env.txt.bak
cat > \$G/env.txt << 'EOF'
APS5_TEXTURE_CACHE_MIB=512
TU_MUTABLE_UBWC=1
APS5_DUMP_TRIGGER=/data/data/org.anyps5.android/files/games/PPSA37318/dumpnow
APS5_DUMP_SCALE=2
APS5_NO_SUBMISSION_REAPER=1
APS5_SDLGPU_VULKAN=1
EOF
[ '${CLEAN_CACHE:-0}' = 1 ] && rm -rf \$G/shader_cache
chown \$(stat -c %u:%g \$G) \$G/env.txt
chcon \$(stat -c %C \$G) \$G/env.txt
cat \$G/env.txt
" && ok "env.txt записан (старый — env.txt.bak)"
fi

# 7. Mewgenics в 1080p: холст движка и экран SDL в app.elf (проверка байтов, копия app.elf.orig1080) и
#    APS5_PRESENT_CROP в env.txt. Откат: su -c 'sh ~/ps5-android/pvz-replanted-android/tools/mew1080.sh undo' (путь Termux).
if [ "$GAME_ID" = PPSA37318 ] && [ "${MEW1080:-1}" = 1 ]; then
  echo "=== Mewgenics 1080p ==="
  su -c "sh '$REPO_DIR/tools/mew1080.sh'" | tail -n 3
fi

# 8. Mewgenics: нативные правки горячего кода app.elf (TLSF: циклы поиска бита -> lzcnt), с проверкой байтов.
#    Только при закрытой игре. Откат: su -c 'sh <репозиторий>/tools/mew-native.sh undo'. MEW_NATIVE=0 — пропустить.
if [ "$GAME_ID" = PPSA37318 ] && [ "${MEW_NATIVE:-1}" = 1 ]; then
  echo "=== Mewgenics: нативные правки app.elf ==="
  su -c "sh '$REPO_DIR/tools/mew-native.sh'" | tail -n 5
fi

# 8b. Mewgenics: патч атласа растровых заливок SWF 4096 (защита от вылета вступительных роликов глав).
#     Только при закрытой игре. Откат: su -c 'sh <репозиторий>/tools/mew-atlas.sh undo'. MEW_ATLAS=0 — пропустить.
if [ "$GAME_ID" = PPSA37318 ] && [ "${MEW_ATLAS:-1}" = 1 ]; then
  echo "=== Mewgenics: фиксация атласа 4096 ==="
  su -c "sh '$REPO_DIR/tools/mew-atlas.sh'" | tail -n 5
fi

# 9. Mewgenics: SPIR-V для нативного Vulkan SDL_GPU (APS5_SDLGPU_VULKAN=1): таблица <игра>/sdlgpu из шейдеров
#    самой игры (нужны Termux python, glslang, spirv-tools). MEW_SDLGPU=0 — пропустить.
if [ "$GAME_ID" = PPSA37318 ] && [ "${MEW_SDLGPU:-1}" = 1 ]; then
  echo "=== Mewgenics: шейдеры SDL_GPU ==="
  bash "$REPO_DIR/tools/mew-sdlgpu-shaders.sh" | tail -n 2
fi

echo "=== ГОТОВО ==="
