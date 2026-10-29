#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin

ROOT="/mnt/c/Mewgenics"
WORKDIR="/tmp/mewgenics_build"
PLATFORM_JAR="/opt/android/sdk/platforms/android-35/android.jar"
R8_JAR="/opt/android/r8.jar"
SRC_APK="$ROOT/Mewgenics-Optimized.apk"
KEYSTORE="$ROOT/android/debug.keystore"

echo "=== 1. Очистка и подготовка $WORKDIR ==="
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR/gen" "$WORKDIR/classes"

echo "=== 2. Генерация ресурсов (aapt) ==="
aapt package -f -m -J "$WORKDIR/gen" -S "$ROOT/android/res" -M "$ROOT/android/AndroidManifest.xml" -I "$PLATFORM_JAR"

echo "=== 3. Компиляция Java (javac) ==="
find "$ROOT/android/src" "$ROOT/AnyPS5/3rdparty/SDL2/android-project/app/src/main/java" "$WORKDIR/gen" -name "*.java" > "$WORKDIR/sources.list"
javac -source 8 -target 8 -classpath "$PLATFORM_JAR" -d "$WORKDIR/classes" @"$WORKDIR/sources.list"

echo "=== 4. Сборка DEX (d8) ==="
(cd "$WORKDIR/classes" && jar -cf "$WORKDIR/app.jar" .)
java -cp "$R8_JAR" com.android.tools.r8.D8 --lib "$PLATFORM_JAR" --min-api 26 --output "$WORKDIR" "$WORKDIR/app.jar"
ls -lh "$WORKDIR/classes.dex"

echo "=== 5. Замена classes.dex и libaps5sdlgpu.so в APK ==="
python3 - << 'PYEOF'
import zipfile
from pathlib import Path

src = Path("/mnt/c/Mewgenics/Mewgenics-Optimized.apk")
unsigned = Path("/tmp/mewgenics_build/unsigned.apk")
dex = Path("/tmp/mewgenics_build/classes.dex").read_bytes()
sdlgpu_path = Path("/mnt/c/Mewgenics/android/sdlgpu/libaps5sdlgpu.so")
sdlgpu = sdlgpu_path.read_bytes() if sdlgpu_path.exists() else None

with zipfile.ZipFile(src, "r") as zin, zipfile.ZipFile(unsigned, "w", compression=zipfile.ZIP_DEFLATED) as zout:
    for item in zin.infolist():
        if item.filename.startswith("META-INF/"):
            continue
        if item.filename == "classes.dex":
            zout.writestr(item, dex)
        elif item.filename == "lib/arm64-v8a/libaps5sdlgpu.so" and sdlgpu is not None:
            zout.writestr(item, sdlgpu)
            print("[+] Replaced lib/arm64-v8a/libaps5sdlgpu.so")
        else:
            zout.writestr(item, zin.read(item.filename))

print("[+] Replaced classes.dex")
PYEOF

echo "=== 6. Выравнивание (zipalign) ==="
zipalign -f -p 4 "$WORKDIR/unsigned.apk" "$WORKDIR/aligned.apk"

echo "=== 7. Подпись APK (apksigner) ==="
apksigner sign --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true \
  --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
  --ks-key-alias androiddebugkey --out "$SRC_APK" "$WORKDIR/aligned.apk"

apksigner verify "$SRC_APK"
echo "=== УСПЕШНО СОБРАН И ПОДПИСАН: $SRC_APK ==="
