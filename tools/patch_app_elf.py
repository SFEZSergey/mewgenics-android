#!/usr/bin/env python3
"""
Оптимизатор app.elf для Mewgenics (PS5 -> Android ARM64):
1. mew-native: замена циклов TLSF-аллокатора на нативные lzcnt/tzcnt (+17% CPU в бою)
2. mew1080: перевод буферов и холста 4K (3840x2160) в 1080p (1920x1080)
3. mew-atlas: фиксация атласа 4096 (устраняет вылет катсцен)
"""

import sys
import shutil
from pathlib import Path

# mew-atlas site
ATLAS_SITES = [
    (15671154, bytes.fromhex("e8b91dfaff"), bytes.fromhex("b800100000"), "SWF BitmapFillAtlas 4096"),
]

# mew-native sites (lzcnt/tzcnt)
NATIVE_SITES = [
    (14832975, bytes.fromhex("b93f00000031d2662e0f1f840000000000480fa3c8721348ffc9ffc283fa4075f0"),
               bytes.fromhex("f3480fbdd0b93f0000004829d183fa4075180f1f8400000000000f1f8000000000"), "TLSF lzcnt site 1"),
    (14833209, bytes.fromhex("b83f0000006690480fa3c1721548ffc841ffc74183ff4075ee"),
               bytes.fromhex("f34c0fbdf9b83f0000004c29f84183ff40750f660f1f440000"), "TLSF lzcnt site 2"),
    (14834747, bytes.fromhex("ba3f000000480fa3d1721a48ffca41ffc74183ff4075ee"),
               bytes.fromhex("f34c0fbdf9ba3f0000004c29fa4183ff4075120f1f4000"), "TLSF lzcnt site 3"),
    (14832723, bytes.fromhex("ba3f00000031c9660f1f440000480fa3d0721448ffcaffc183f94075f0"),
               bytes.fromhex("f3480fbdc8ba3f0000004829ca83f94075150f1f8400000000000f1f00"), "TLSF lzcnt site 4"),
    (14834115, bytes.fromhex("bf3f00000031f6660f1f440000480fa3f8721448ffcfffc683fe4075f0"),
               bytes.fromhex("f3480fbdf0bf3f0000004829f783fe4075150f1f8400000000000f1f00"), "TLSF lzcnt site 5"),
    (14834371, bytes.fromhex("b83f00000031d2660f1f440000480fa3c1721448ffc8ffc283fa4075f0"),
               bytes.fromhex("f3480fbdd1b83f0000004829d083fa4075150f1f8400000000000f1f00"), "TLSF lzcnt site 6"),
    (14834946, bytes.fromhex("ba3f0000004531c0660f1f440000480fa3d1721648ffca41ffc04183f84075ee"),
               bytes.fromhex("f34c0fbdc1ba3f0000004c29c24183f84075170f1f8400000000000f1f440000"), "TLSF lzcnt site 7"),
    (14835245, bytes.fromhex("ba3f0000004531c966662e0f1f840000000000480fa3d1721648ffca41ffc14183f94075ee"),
               bytes.fromhex("f34c0fbdc9ba3f0000004c29ca4183f940751c0f1f8400000000000f1f8400000000006690"), "TLSF lzcnt site 8"),
    (14835587, bytes.fromhex("ba3f00000031c0660f1f440000480fa3d1720f48ffcaffc083f84075f0"),
               bytes.fromhex("f3480fbdc1ba3f0000004829c283f84075100f1f8400000000000f1f00"), "TLSF lzcnt site 9"),
    (14833775, bytes.fromhex("bf3f00000031f64989070f1f8000000000480fa3fa721348ffcfffc683fe4075f0"),
               bytes.fromhex("498907f3480fbdf2bf3f0000004829f783fe4075150f1f8400000000000f1f4000"), "TLSF lzcnt site 10"),
    (14833494, bytes.fromhex("ba3f00000049890631c941c646200166662e0f1f840000000000480fa3d0723048ffcaffc183f94075f0"),
               bytes.fromhex("49890641c6462001f3480fbdc8ba3f0000004829ca83f94075360f1f8400000000000f1f840000000000"), "TLSF lzcnt site 11"),
    (14835635, bytes.fromhex("31c066662e0f1f840000000000480fa3c1720e48ffc04883f84075f1"),
               bytes.fromhex("f3480fbcc1c30f1f8400000000000f1f840000000000660f1f440000"), "TLSF tzcnt site 12"),
    (14833097, bytes.fromhex("ba0100000031c9c4e2f1f7f285c6750e48ffc14883f94075ee"),
               bytes.fromhex("f30fbcc87213eb160f1f8400000000000f1f84000000000090"), "TLSF tzcnt site 13"),
    (14833138, bytes.fromhex("be0100000031c04c8975980f1f00c4e2f9f7fe85d7750e48ffc04883f84075ee"),
               bytes.fromhex("4c897598f30fbcc27216eb190f1f8400000000000f1f8400000000000f1f4000"), "TLSF tzcnt site 14"),
    (14834261, bytes.fromhex("41b80100000031c00f1f00c442f9f7c84185c9750e48ffc04883f84075ed"),
               bytes.fromhex("f30fbcc17218eb1b0f1f8400000000000f1f840000000000660f1f440000"), "TLSF tzcnt site 15"),
    (14834309, bytes.fromhex("41b80100000031c00f1f00c442f9f7c84185c9750e48ffc04883f84075ed"),
               bytes.fromhex("f30fbcc17218eb1b0f1f8400000000000f1f840000000000660f1f440000"), "TLSF tzcnt site 16"),
    (14835881, bytes.fromhex("83faff0f84fb000000"), bytes.fromhex("e9ff0000000f1f4000"), "TLSF site 17"),
    (14837357, bytes.fromhex("0fb7bd2affffff"), bytes.fromhex("0fbfbd2affffff"), "TLSF site 18"),
    (14837364, bytes.fromhex("0fb78528ffffff"), bytes.fromhex("0fbf8528ffffff"), "TLSF site 19"),
]

# mew1080 sites
SITES_1080 = [
    (15222337, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 1"),
    (15224306, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 2"),
    (15231890, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 3"),
    (15232065, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 4"),
    (15234762, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 5"),
    (15235997, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 6"),
    (19424222, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Resolution 1080p site 7"),
    (15235735, bytes.fromhex("70080000"), bytes.fromhex("38040000"), "1080p height 1"),
    (15235822, bytes.fromhex("000f0000"), bytes.fromhex("80070000"), "1080p width 1"),
    (15235828, bytes.fromhex("000f0000"), bytes.fromhex("80070000"), "1080p width 2"),
    (15235892, bytes.fromhex("70080000"), bytes.fromhex("38040000"), "1080p height 2"),
    (15235940, bytes.fromhex("000f0000"), bytes.fromhex("80070000"), "1080p width 3"),
    (15235946, bytes.fromhex("70080000"), bytes.fromhex("38040000"), "1080p height 3"),
    (15222139, bytes.fromhex("000000000000f03f"), bytes.fromhex("000000000000e03f"), "render_scale 1.0 -> 0.5"),
    (19423100, bytes.fromhex("000f0000"), bytes.fromhex("80070000"), "VideoInit width 1080p"),
    (19423169, bytes.fromhex("70080000"), bytes.fromhex("38040000"), "VideoInit height 1080p"),
    (19391694, bytes.fromhex("498b7d10488d735c488d5358e801b8faff"), bytes.fromhex("c7435c80070000c74358380400000f1f00"), "Swapchain SDL_GPU 1080p"),
    (27708552, bytes.fromhex("000f000070080000"), bytes.fromhex("8007000038040000"), "Mouse bounds 1080p"),
    (14837120, bytes.fromhex("03"), bytes.fromhex("01"), "Mouse display 3 -> 1"),
]

def apply_patches(elf_path: Path):
    if not elf_path.exists():
        print(f"[ERR] File not found: {elf_path}")
        return False

    backup_path = elf_path.with_name(elf_path.name + ".original")
    if not backup_path.exists():
        print(f"[INFO] Creating clean backup: {backup_path}")
        shutil.copy2(elf_path, backup_path)

    data = bytearray(elf_path.read_bytes())
    all_sites = ATLAS_SITES + NATIVE_SITES + SITES_1080
    applied = 0
    already = 0

    for offset, old_bytes, new_bytes, desc in all_sites:
        sz = len(old_bytes)
        current = bytes(data[offset:offset+sz])
        if current == new_bytes:
            already += 1
        elif current == old_bytes:
            data[offset:offset+sz] = new_bytes
            applied += 1
            print(f"  [+] Patched {desc} at offset 0x{offset:x}")
        else:
            print(f"  [WARN] Unexpected bytes at offset 0x{offset:x} ({desc}): {current.hex()} != {old_bytes.hex()}")

    elf_path.write_bytes(data)
    print(f"[OK] Patched {applied} sites, {already} were already patched (Total sites: {len(all_sites)})")
    return True

if __name__ == "__main__":
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("C:/Mewgenics/PPSA37318/app.elf")
    apply_patches(target)
