#!/usr/bin/env python3
import os
import subprocess
import shutil
from pathlib import Path

def main():
    root = Path(__file__).resolve().parent.parent
    classes_dir = Path("/tmp/mew_classes")
    dex_dir = Path("/tmp/mew_dex")
    
    if classes_dir.exists():
        shutil.rmtree(classes_dir)
    if dex_dir.exists():
        shutil.rmtree(dex_dir)
        
    classes_dir.mkdir(parents=True, exist_ok=True)
    dex_dir.mkdir(parents=True, exist_ok=True)
    
    android_jar = "/opt/android/sdk/platforms/android-35/android.jar"
    r8_jar = "/opt/android/r8.jar"
    
    java_files = []
    for d in [root / "android" / "src", root / "AnyPS5" / "3rdparty" / "SDL2" / "android-project" / "app" / "src" / "main" / "java"]:
        for f in d.rglob("*.java"):
            java_files.append(str(f))
            
    print(f"[+] Compiling {len(java_files)} Java files...")
    sources_list = Path("/tmp/sources.list")
    sources_list.write_text("\n".join(java_files))
    
    cmd_javac = [
        "javac", "-nowarn", "-source", "8", "-target", "8",
        "-classpath", android_jar,
        "-d", str(classes_dir),
        f"@{sources_list}"
    ]
    subprocess.check_call(cmd_javac)
    print("[OK] javac finished successfully!")
    
    jar_file = Path("/tmp/mew_app.jar")
    if jar_file.exists():
        jar_file.unlink()
    
    subprocess.check_call(["jar", "cf", str(jar_file), "-C", str(classes_dir), "."])
    print("[OK] Created jar archive!")
    
    cmd_d8 = [
        "java", "-cp", r8_jar, "com.android.tools.r8.D8",
        "--min-api", "26",
        "--lib", android_jar,
        "--output", str(dex_dir),
        str(jar_file)
    ]
    subprocess.check_call(cmd_d8)
    print("[OK] D8 finished successfully!")
    
    out_dex = root / "android" / "classes.dex"
    shutil.copy2(dex_dir / "classes.dex", out_dex)
    print(f"[SUCCESS] Generated {out_dex} ({out_dex.stat().st_size} bytes)")

if __name__ == "__main__":
    main()
