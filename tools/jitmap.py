#!/usr/bin/env python3
# Map box64's translated blocks of a running AnyPS5 process from outside (root): reads
# /proc/<pid>/maps and /proc/<pid>/mem, finds box64 dynarec chunks by their blocklist_t header and
# walks their blockmarks to the dynablock_t of every translated block. Nothing is injected into the
# game process. Output, one block per line:
#   <native-start> <native-size> <x64-start> <x64-size> <x86 module>+<offset>
# (hex). Layouts follow box64 src/custommem.c (blocklist_t, blockmark_t) and
# src/dynarec/dynablock_private.h (dynablock_t) on arm64.
#
# usage: jitmap.py <pid> <output file>
import struct
import sys

BLOCKLIST = 40      # void* block; size_t maxfree; size_t size; void* first; u32 lowest; u8 type; u8 is32bits (+pad)
BLOCKMARK = 16      # mark_t prev; mark_t next; data aligned to 16
DYNABLOCK = 48      # block, actual_block, in_used(+pad), x64_addr, x64_size, native_size


def mappings(pid):
    result = []
    with open(f"/proc/{pid}/maps") as maps:
        for line in maps:
            parts = line.split(None, 5)
            start, end = (int(x, 16) for x in parts[0].split("-"))
            name = parts[5].strip() if len(parts) > 5 else ""
            result.append((start, end, parts[1], int(parts[2], 16), name))
    return result


def module_of(maps, address):
    for start, end, perms, offset, name in maps:
        if start <= address < end and name and not name.startswith("["):
            base = min(s for s, e, p, o, n in maps if n == name)
            return f"{name.rsplit('/', 1)[-1]}+{address - base:x}"
    return "?"


def main():
    pid = int(sys.argv[1])
    maps = mappings(pid)
    blocks = []
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as mem:
        def read(address, size):
            try:
                mem.seek(address)
                return mem.read(size)
            except OSError:
                return None

        for start, end, perms, offset, name in maps:
            if "x" not in perms or end - start < 4096:
                continue
            header = read(start, BLOCKLIST)
            if not header or len(header) < BLOCKLIST:
                continue
            block, maxfree, size = struct.unpack_from("<QQQ", header)
            if block != start + BLOCKLIST or size == 0 or start + BLOCKLIST + size > end:
                continue
            data = read(block, size)
            if not data:
                continue
            cursor = 0
            while cursor + BLOCKMARK <= len(data):
                prev, nxt = struct.unpack_from("<II", data, cursor)
                offs, fill = nxt & 0x7FFFFFFF, nxt >> 31
                if offs == 0:
                    break
                if fill and cursor + BLOCKMARK + 8 <= len(data):
                    (pointer,) = struct.unpack_from("<Q", data, cursor + BLOCKMARK)
                    record = read(pointer, DYNABLOCK) if pointer else None
                    if record and len(record) == DYNABLOCK:
                        native, _, _, x64, x64_size, native_size = struct.unpack_from("<QQQQQQ", record)
                        if native and x64 and native_size < (1 << 26):
                            blocks.append((native, native_size, x64, x64_size))
                cursor += offs
    with open(sys.argv[2], "w") as out:
        for native, native_size, x64, x64_size in sorted(blocks):
            out.write(f"{native:x} {native_size:x} {x64:x} {x64_size:x} {module_of(maps, x64)}\n")
    print(f"[jitmap] {len(blocks)} blocks", file=sys.stderr)


if __name__ == "__main__":
    main()
