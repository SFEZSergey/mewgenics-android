# Инструменты и команды

Среда: Termux на телефоне с root (KernelSU). Пакет игры на телефоне: `org.anyps5.android`.
Папка игры: `/data/data/org.anyps5.android/files/games/PPSA37318` (дальше `$G`).
Лог игры: `$G/box64.log` (перезаписывается при каждом запуске).

## Пути на телефоне (рабочая копия)

| Что | Где |
|---|---|
| AnyPS5 | `~/ps5-android/src/AnyPS5` (сборка `build-bionic`, x86-часть `build-x64s`) |
| Mesa/Turnip | `~/ps5-android/src/mesa-26.3` (сборка `build-android`) |
| box64 | `~/ps5-android/src/box64` (APK берёт `build-lib/libbox64.so`) |
| SDL3 для бэкенда | `~/ps5-android/src/sdl3-gpu-base/SDL-2747162d…` (качается сам) |
| APK | `~/ps5-android/apk` (`build.sh`, результат `out/PvZ-Replanted.apk`) |
| Репозиторий PvZ (общие скрипты) | `~/ps5-android/pvz-replanted-android` |

## Сборка и установка

```sh
bash tools/github.sh                 # полный цикл: патчи AnyPS5, Turnip, APK, установка, env.txt, шейдеры
SKIP_MESA=1 KEEP_ENV=1 bash tools/github.sh   # без пересборки Turnip и без перезаписи env.txt

# только box64 (правки в src/tools/aps5hot.c, dynarec):
cd ~/ps5-android/src/box64 && cmake -DANDROID=ON build-lib && cmake --build build-lib -j8
#   ВАЖНО: всегда -DANDROID=ON, иначе cmake сбрасывает ANDROID=OFF

# только нативный бэкенд + APK:
bash android/sdlgpu/build.sh /tmp/libaps5sdlgpu.so    # проверка сборки
cd ~/ps5-android/apk && bash build.sh                  # APK (сам вызывает sdlgpu/build.sh)
su -c 'cp ~/ps5-android/apk/out/PvZ-Replanted.apk /data/local/tmp/pvz.apk && pm install -r /data/local/tmp/pvz.apk'

# запуск / остановка игры:
su -c 'am start -n org.anyps5.android/.GameActivity'
su -c 'am force-stop org.anyps5.android'
```

## Скрипты `tools/`

| Скрипт | Назначение |
|---|---|
| `github.sh` | мастер-сборка (шаги 1–9), опции в шапке файла |
| `mew-sdlgpu-shaders.sh` | строит `$G/sdlgpu/*.spv` из шейдеров игры (нужны python, glslang, spirv-tools) |
| `sdlgpu/glsl2spv.py` | GLSL игры → SPIR-V в раскладке SDL_GPU (для программ без готового SPIR-V) |
| `sdlgpu/maketable.py` | таблица «FNV PSSL-блоба → SPIR-V» (+ `index.txt`) |
| `sdlgpu/spvif.py`, `cmpall.sh`, `desc.py` | сравнение интерфейсов SPIR-V, разбор `.desc` (раскладка uniform) |
| `mew1080.sh` | патч app.elf: 1080p вместо 4K (`undo` — откат) |
| `mew-native.sh` | патч app.elf: нативные lzcnt в TLSF-аллокаторе (`undo`) |
| `mew-atlas.sh` | патч app.elf: фиксация атласа SWF 4096 (защита от вылета роликов глав, `undo`) |
| `sesscap.sh` | пассивный сборщик сессий (root, стартует из `/data/adb/service.d/sesscap-start.sh`): FPS, потоки, simpleperf, снимки экрана раз в минуту → `/data/local/tmp/sesscap/S<дата>/` |
| `jitmap.py` | карта блоков box64 dynarec живого процесса |
| `psdis.sh` | дизассемблер PSSL-шейдера (RDNA2 через `llvm-mc`) |
| `bc3png.py`, `bmp2png.py` | BC3-дамп / BMP → PNG |
| `fpcr-round-test.c` | тест округления `cvt*` под box64 (все 4 режима MXCSR) |

## Переключатели `env.txt` (`$G/env.txt`)

| Переменная | Смысл |
|---|---|
| `APS5_SDLGPU_VULKAN=1` | нативный Vulkan вместо Agc (главное) |
| `APS5_SDLGPU_CANVAS=WxH` | размер холста игры для letterbox (0 = выкл), по умолчанию 1920x1080 |
| `APS5_SDLGPU_NO_ZERO_INIT=1` | не обнулять новые текстуры |
| `APS5_SDLGPU_CYCLE=1` | пропускать флаги `cycle` игры как есть |
| `APS5_SDLGPU_KEEP_DONT_CARE=1` | не заменять `DONT_CARE` |
| `APS5_SDLGPU_UPLOADED_LOD0=1` | загружаемые текстуры только с уровня 0 (эксперимент) |
| `APS5_SDLGPU_AUTO_MIPS=1`, `APS5_SDLGPU_TARGET_MIPS=1` | строить мипы (эксперимент, портит картинку) |
| `APS5_SDLGPU_SKIP_FS=<hash,…>` / `APS5_SDLGPU_NEAREST_FS=<hash,…\|all>` | отладка: не рисовать / семплеры без фильтра для шейдера |
| `APS5_SDLGPU_COUNTS=0` | выключить счётчики вызовов в логе |
| `APS5_SDLGPU_VERBOSE=1` | подробный лог SDL |
| `APS5_DUMP_TRIGGER=$G/dumpnow` | при `touch $G/dumpnow` — дамп кадра `$G/frame_000.bmp` |
| `APS5_NO_SUBMISSION_REAPER=1` | выключить reaper Agc-пути (он грелся) |

## Диагностика

```sh
# что пишет бэкенд:
su -c "grep -a -E 'sdlgpu|^\[fps\]|aps5hot' $G/box64.log | tail -50"
# счётчики вызовов SDL_GPU раз в 5 с:
su -c "grep -a 'sdlgpu\] calls' $G/box64.log | tail -2"
# дамп атласа (уровни 0..3) и просмотр:
su -c "touch $G/texdump"; sleep 3; su -c "rm $G/texdump"
python3 tools/bc3png.py tex_L0.raw 2048 2048 out.png 1024
# дизассемблировать PS5-шейдер:
tools/psdis.sh $G/shaders/pssl/default_full.shader.fs | grep image_
# снимки сборщика последней сессии:
su -c 'ls /data/local/tmp/sesscap/$(ls /data/local/tmp/sesscap | grep ^S | sort | tail -1)/shots'
```

## Ключевые адреса app.elf (PPSA37318)

* SDL_GPU фронтенд ~`0x114b000`, выбор бэкенда `0x114bb40`; загрузчик Agc `0x1a6e458`
  (`{"agc", PrepareDriver 0x126e260, CreateDevice 0x126e2d0}`); Agc-бэкенд `0x126c000–0x1298000`.
* `SDL_GPUDevice` = 0x2d0 байт (SDL main с OpenXR, коммит 2747162d): 87 слотов, `driverData` 0x2b8.
