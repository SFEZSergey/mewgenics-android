# mewgenics-android

Порт Mewgenics (PS5, PPSA37318) на Android (arm64) поверх AnyPS5 + box64 + Turnip.
Проверено на Snapdragon 8 Elite (Adreno 830), бой 60 FPS.

Данных игры здесь нет и быть не должно: пользователь импортирует свой дамп.
AnyPS5 — GPL-2.0-only, исходники публикуются вместе с портом.

## Как устроено

```
app.elf (x86-64, игра) ──box64──▶ SDL_GPU фронтенд игры (x86)
                                     │  слоты SDL_GPUDevice = мосты box64
                                     ▼
                     libaps5sdlgpu.so (arm64): Vulkan-бэкенд SDL3 + прослойка
                                     ▼
                         Turnip (Mesa, libvulkan_freedreno.so)
```

* Игра рисует через SDL3 SDL_GPU. Единственный бэкенд в app.elf — PS5 `Agc`.
  box64 (`aps5hot.c`) подменяет его `CreateDevice` (указатель в загрузчике по
  `0x1a6e468`, хеш кода `0xfb170c2bbebbdcf3`) на нативный Vulkan-бэкенд SDL3.
  Если нативный не поднялся — игра остаётся на Agc (путь AnyPS5: AGC → PM4 → Vulkan).
* Шейдеры: игра отдаёт PSSL, бэкенд подставляет SPIR-V по FNV-1a хешу блоба
  (`<игра>/sdlgpu/<hash>.spv`, собирает `tools/mew-sdlgpu-shaders.sh`).
* Отличия PS5 от Vulkan, которые закрывает `android/sdlgpu/backend.c`:
  - невыровненные смещения загрузки сжатых текстур (остаток 4 по модулю 16) —
    копия в выровненный буфер (главная причина «шума» на текстурах);
  - новые текстуры обнуляются (память PS5 нулевая);
  - `bitmap_atlas_info` (таблица R32_FLOAT 9100x1) — копия в storage-буфер,
    привязывается сама (игра её не привязывает);
  - `cycle` выключен у записей GPU, `DONT_CARE` → `LOAD/STORE`;
  - letterbox: игра рисует холст 1920x1080, он масштабируется в центр экрана.
* Ошибка SDL: в `Download*` не было барьера `TRANSFER_WRITE → HOST_READ`
  (на Turnip CPU читал нули) — правится при сборке (`android/sdlgpu/patch_sdl.py`).

## Состав

| Папка | Что |
|---|---|
| `AnyPS5/` | эмулятор (дерево с наложенными патчами из `patches/anyps5-*.patch`) |
| `mesa/` | Mesa 26.3 (Turnip) с `patches/mesa-26.3-android.patch` |
| `android/sdlgpu/` | нативный Vulkan SDL_GPU бэкенд (C), сборка `build.sh` |
| `android/apk-build.sh` | сборка APK (копия `~/ps5-android/apk/build.sh`) |
| `patches/` | патчи AnyPS5, Mesa, box64 (`box64-local.patch`) |
| `tools/` | скрипты сборки/установки/диагностики, см. `TOOLS.md` |

Подробно про инструменты и команды — `TOOLS.md`. Для ИИ-ассистента — `AGENTS.md`.
