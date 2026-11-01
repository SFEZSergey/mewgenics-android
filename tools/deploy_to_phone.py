#!/usr/bin/env python3
"""
Скрипт развертывания Mewgenics на Android (ColorOS 17 / AnyPS5):
- Устанавливает подписанный Mewgenics.apk
- Загружает пропатченный дамп игры PPSA37318
- Настраивает env.txt для прямого Vulkan пути SDL_GPU 1080p
"""

import subprocess
import sys
import time
from pathlib import Path

ADB = "C:\\platform-tools\\adb.exe"
PACKAGE = "org.anyps5.android"
REMOTE_GAME_DIR = f"/data/data/{PACKAGE}/files/games/PPSA37318"

def run_adb(cmd, check=True):
    full_cmd = [ADB] + cmd
    res = subprocess.run(full_cmd, capture_output=True, text=True)
    if check and res.returncode != 0:
        print(f"[ERR] ADB command failed: {' '.join(full_cmd)}\n{res.stderr}")
    return res

def wait_for_device():
    print("[*] Ожидание подключения устройства по ADB...")
    while True:
        res = run_adb(["devices"], check=False)
        lines = [line.strip() for line in res.stdout.splitlines() if line.strip() and not line.startswith("List")]
        devices = [line.split()[0] for line in lines if "device" in line]
        if devices:
            dev = devices[0]
            print(f"[+] Найдено устройство: {dev}")
            return dev
        time.sleep(2)

def deploy():
    root = Path(__file__).resolve().parent.parent
    apk_path = root / "Mewgenics.apk"
    game_dir = root / "PPSA37318"

    if not apk_path.exists():
        print(f"[ERR] Не найден {apk_path}")
        return

    wait_for_device()

    print(f"[*] 1. Установка оптимизированного APK ({apk_path.name})...")
    res = run_adb(["install", "-r", str(apk_path)])
    print(res.stdout)

    print("[*] 2. Подготовка каталогов игры на устройстве...")
    run_adb(["shell", "su", "-c", f"mkdir -p {REMOTE_GAME_DIR}"])

    print(f"[*] 3. Загрузка дампа игры ({game_dir})...")
    # Копируем app.elf и основные каталоги
    run_adb(["push", str(game_dir / "app.elf"), f"/data/local/tmp/app.elf"])
    run_adb(["shell", "su", "-c", f"cp /data/local/tmp/app.elf {REMOTE_GAME_DIR}/app.elf && rm /data/local/tmp/app.elf"])

    # Загружаем остальные файлы игры, если их нет
    for sub in ["app0", "audio", "data", "levels", "pswfs", "sce_module", "sce_sys", "shaders", "swfs", "textures"]:
        sub_path = game_dir / sub
        if sub_path.exists():
            print(f"  [+] Синхронизация {sub}...")
            run_adb(["push", str(sub_path), f"/data/local/tmp/"])
            run_adb(["shell", "su", "-c", f"cp -r /data/local/tmp/{sub} {REMOTE_GAME_DIR}/ && rm -rf /data/local/tmp/{sub}"])

    print("[*] 4. Настройка конфигурации env.txt (Vulkan SDL_GPU 1080p + Cool & Low-RAM profile)...")
    env_content = (
        "# Native Vulkan SDL_GPU 1080p\n"
        "APS5_SDLGPU_VULKAN=1\n"
        "APS5_SDLGPU_CANVAS=1920x1080\n"
        "APS5_SDLGPU_COUNTS=0\n"
        "\n"
        "# RAM budget for 4-6 GB devices (prevents OOM / LMK crash)\n"
        "APS5_DIRECT_MEMORY_MB=2048\n"
        "\n"
        "# Box64 Dynarec: Thermal & Energy Efficiency (no CPU overheating)\n"
        "BOX64_DYNAREC_STRONG_MEM=0\n"
        "BOX64_DYNAREC_FASTROUND=1\n"
        "BOX64_DYNAREC_FASTNAN=1\n"
        "BOX64_DYNAREC_X87DOUBLE=0\n"
        "BOX64_DYNAREC_BIGBLOCK=3\n"
        "BOX64_DYNAREC_CALLRET=1\n"
        "BOX64_DYNAREC_PAUSE=3\n"
        "BOX64_DYNACACHE=1\n"
        "BOX64_DYNACACHE_COMPRESS=0\n"
        "\n"
        "# Adreno Fast Shaders (cuts ALU shader cycles by ~50%)\n"
        "APS5_HOST_FTZ=1\n"
        "APS5_HOST_NAN=1\n"
        "APS5_HOST_FMA=1\n"
        "APS5_HOST_DPP=1\n"
    )
    run_adb(["shell", "su", "-c", f"printf '{env_content}' > {REMOTE_GAME_DIR}/env.txt"])

    print("[*] 5. Исправление прав доступа...")
    run_adb(["shell", "su", "-c", f"chown -R $(stat -c %u:%g /data/data/{PACKAGE}) /data/data/{PACKAGE}"])
    run_adb(["shell", "su", "-c", f"chmod -R 755 {REMOTE_GAME_DIR}"])

    print("[OK] Развертывание полностью завершено! Игра готова к первому запуску.")

if __name__ == "__main__":
    deploy()
