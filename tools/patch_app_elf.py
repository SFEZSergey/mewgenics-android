#!/usr/bin/env python3
"""
Оптимизатор app.elf для Mewgenics (PS5 -> Android ARM64):
1. mew-native: замена циклов TLSF-аллокатора на нативные lzcnt/tzcnt (+17% CPU в бою)
2. mew1080: перевод буферов и холста 4K (3840x2160) в 1080p (1920x1080)
3. mew-atlas: фиксация атласа 4096 (устраняет вылет катсцен)
"""

import argparse
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

# LinearAllocator fast path: bypass expensive assertion string building & heap allocs on every alloc/realloc
LINEAR_ALLOC_SITES = [
    (17791361, bytes.fromhex("4189d6488b00488945d085d27404448b7344458d24364531ed443b6338bf200000004889b570ffffff48c745b800000000410f96c5e8456e2c00c5f81005e7b7620048b96c6f636b73697a65488945b049b9200000002d0000004c8d05dd70610048c745c01700000048c745c81f0000004c898d78ffffff4c894580c5f811004889480fc6401700488d05e58a6300c5f857c0c5f811459048c745a00600000048c745a80f00000066c745947274c745904173736548894588488b4588488d3d1b289c00488d55b0488d4d904489ee4889442410c5f8108578ffffffc5f8110424e84995e9ff488b45a84883f8107221488b7d90488d70014881fe00100000720b488b7ff84883c0284889c6e87e6d2c00488b45c848c745a00000000048c745a80f000000c64590004883f8107221488b7db0488d70014881fe00100000720b488b7ff84883c0284889c6e83f6d2c00"),
               bytes.fromhex("4889b570ffffff4189d685d27404448b7344458d2436e935010000") + b"\x90" * 309, "LinearAllocator::alloc fast path"),
    (17792127, bytes.fromhex("418b47444531e4bf300000004889f34401f0413b473848c745b800000000410f96c4e85a6b2c00c5fc10055a6e6100488945b049b939000000360000004c8d05fc6d610048c745c02000000048c745c82f0000004c898d78ffffff4c894580c5fc1100c6402000488d056c806400c5f857c0c5f811459048c745a00600000048c745a80f00000066c745947274c745904173736548894588488b4588488d3d3e259c00488d55b0488d4d904489e64889442410c5f8108578ffffffc5f8110424e86c92e9ff488b45a84883f8107221488b7d90488d70014881fe00100000720b488b7ff84883c0284889c6e8a16a2c00488b45c848c745a00000000048c745a80f000000c64590004883f8107221488b7db0488d70014881fe00100000720b488b7ff84883c0284889c6e8626a2c00"),
               bytes.fromhex("4889f3e927010000") + b"\x90" * 295, "LinearAllocator::Sony_Realloc fast path"),
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

def apply_patches(elf_path: Path, check_only: bool = False):
    if not elf_path.exists():
        print(f"[ERR] File not found: {elf_path}")
        return False

    data = elf_path.read_bytes()
    if (len(data) < 64 or data[:6] != b"\x7fELF\x02\x01"
            or int.from_bytes(data[18:20], "little") != 62):
        print("[ERR] Expected a little-endian ELF64 x86-64 app.elf; no bytes were changed.")
        return False
    all_sites = ATLAS_SITES + NATIVE_SITES + LINEAR_ALLOC_SITES + SITES_1080
    pending = []
    already = 0
    incompatible = []

    for offset, old_bytes, new_bytes, desc in all_sites:
        sz = len(old_bytes)
        current = bytes(data[offset:offset+sz])
        if current == new_bytes:
            already += 1
        elif current == old_bytes:
            pending.append((offset, new_bytes, desc))
        else:
            incompatible.append((offset, desc, current.hex(), old_bytes.hex(), new_bytes.hex()))

    print(f"[INFO] app.elf patch signatures: {already}/{len(all_sites)} already patched, {len(pending)} ready, {len(incompatible)} incompatible")
    if incompatible:
        for offset, desc, current, original, patched in incompatible:
            print(f"  [ERR] {desc} at 0x{offset:x}: found {current}; expected {original} or {patched}")
        print("[ERR] No bytes were changed; this app.elf does not match the complete supported patch set.")
        return False
    if check_only:
        if pending:
            print("[WARN] app.elf is recognized but not fully patched.")
            return False
        print("[OK] All app.elf patch sites are present and verified.")
        return True
    if not pending:
        print("[OK] All app.elf patch sites are already applied.")
        return True

    backup_path = elf_path.with_name(elf_path.name + ".original")
    if not backup_path.exists():
        print(f"[INFO] Creating clean backup: {backup_path}")
        shutil.copy2(elf_path, backup_path)

    mutable = bytearray(data)
    for offset, new_bytes, desc in pending:
        mutable[offset:offset+len(new_bytes)] = new_bytes
        print(f"  [+] Patched {desc} at offset 0x{offset:x}")
    elf_path.write_bytes(mutable)

    verified = elf_path.read_bytes()
    failed = [(offset, desc) for offset, patch, desc in pending
              if verified[offset:offset+len(patch)] != patch]
    if failed:
        for offset, desc in failed:
            print(f"  [ERR] Verification failed for {desc} at 0x{offset:x}")
        return False
    print(f"[OK] Patched {len(pending)} sites, {already} were already patched (total: {len(all_sites)}).")
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check or safely apply the supported Mewgenics app.elf patch set.")
    parser.add_argument("elf", nargs="?", type=Path, default=Path("C:/Mewgenics/PPSA37318/app.elf"))
    parser.add_argument("--check", action="store_true", help="verify all patch sites without writing")
    args = parser.parse_args()
    if not apply_patches(args.elf, check_only=args.check):
        sys.exit(1)
