#!/usr/bin/env python3
"""
Обновляет Mewgenics.apk:
Заменяет/добавляет нативные ARM64 библиотеки в lib/arm64-v8a/:
- libaps5sdlgpu.so (свежий Vulkan SDL_GPU бэкенд)
- нативные shims: libSceAudioOut, libSceVideoOut, libSceSysmodule,
  libSceCommonDialog, libSceRandom, libSceRtc, libSceSystemService, libSceUserService
"""

import zipfile
import shutil
from pathlib import Path

def update_apk(src_apk: Path, dst_apk: Path, shims_dir: Path, sdlgpu_so: Path):
    print(f"[+] Чтение исходного APK: {src_apk}")
    replacements = {}

    if sdlgpu_so.exists():
        replacements["lib/arm64-v8a/libaps5sdlgpu.so"] = sdlgpu_so.read_bytes()
        print(f"  [+] Добавлен {sdlgpu_so.name} ({len(replacements['lib/arm64-v8a/libaps5sdlgpu.so'])} байт)")

    for so_file in shims_dir.glob("*.so"):
        target_name = f"lib/arm64-v8a/{so_file.name}"
        replacements[target_name] = so_file.read_bytes()
        print(f"  [+] Добавлен нативный shim {so_file.name} ({len(replacements[target_name])} байт)")

    print(f"[+] Создание оптимизированного APK: {dst_apk}")
    with zipfile.ZipFile(src_apk, "r") as zin:
        with zipfile.ZipFile(dst_apk, "w", compression=zipfile.ZIP_DEFLATED) as zout:
            for item in zin.infolist():
                if item.filename in replacements:
                    print(f"  [~] Замена {item.filename}")
                    data = replacements.pop(item.filename)
                    zout.writestr(item.filename, data)
                else:
                    zout.writestr(item, zin.read(item.filename))

            # Добавляем оставшиеся новые shims, которых раньше не было в APK
            for filename, data in replacements.items():
                print(f"  [+] Новый модуль в APK: {filename}")
                zout.writestr(filename, data)

    print(f"[OK] Готово! Создан {dst_apk} ({dst_apk.stat().st_size} байт)")

if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    src = root / "Mewgenics.apk"
    dst = root / "Mewgenics-Optimized.apk"
    shims = root / "android" / "shims"
    sdlgpu = root / "android" / "sdlgpu" / "libaps5sdlgpu.so"
    update_apk(src, dst, shims, sdlgpu)
