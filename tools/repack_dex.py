import zipfile
import subprocess
from pathlib import Path

def main():
    root = Path(__file__).resolve().parent.parent
    src_apk = root / "Mewgenics.apk"
    tmp_unsigned = Path("/tmp/build_apk/app-unsigned.apk")
    tmp_aligned = Path("/tmp/build_apk/app-aligned.apk")
    dex_file = Path("/tmp/build_apk/classes.dex")
    keystore = root / "android" / "debug.keystore"

    print(f"Reading dex: {dex_file}")
    dex_bytes = dex_file.read_bytes()

    print(f"Repacking {src_apk} -> {tmp_unsigned}")
    with zipfile.ZipFile(src_apk, "r") as zin:
        with zipfile.ZipFile(tmp_unsigned, "w", compression=zipfile.ZIP_DEFLATED) as zout:
            for item in zin.infolist():
                if item.filename.startswith("META-INF/"):
                    continue
                if item.filename == "classes.dex":
                    zout.writestr(item, dex_bytes)
                else:
                    zout.writestr(item, zin.read(item.filename))

    print(f"Aligning: {tmp_unsigned} -> {tmp_aligned}")
    subprocess.run(["zipalign", "-f", "-p", "4", str(tmp_unsigned), str(tmp_aligned)], check=True)

    print(f"Signing: {src_apk}")
    subprocess.run([
        "apksigner", "sign",
        "--v1-signing-enabled", "true",
        "--v2-signing-enabled", "true",
        "--v3-signing-enabled", "true",
        "--ks", str(keystore),
        "--ks-pass", "pass:android",
        "--key-pass", "pass:android",
        "--ks-key-alias", "androiddebugkey",
        "--out", str(src_apk),
        str(tmp_aligned)
    ], check=True)

    subprocess.run(["apksigner", "verify", str(src_apk)], check=True)
    print("[SUCCESS] APK successfully repacked, aligned and signed!")

if __name__ == "__main__":
    main()
