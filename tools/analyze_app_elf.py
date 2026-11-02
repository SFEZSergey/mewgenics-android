#!/usr/bin/env python3
"""Read-only ELF64 inventory and Box64 perf-map correlation (stdlib only).

Diagnostic strings give candidate names, not proven function identities.
No game bytes are written to the report. Use --output outside the repository.
"""
import argparse
import bisect
import collections
import hashlib
import json
import re
import struct
from pathlib import Path


def analyze(path, profile=None):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64')
    phoff = struct.unpack_from('<Q', data, 32)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 54)
    segments = [struct.unpack_from('<IIQQQQQQ', data, phoff+i*phsize)
                for i in range(phnum)]
    loads = [s for s in segments if s[0] == 1]
    def va(off):
        for _, flags, start, addr, _, size, _, _ in loads:
            if start <= off < start+size:
                return addr+off-start
        return None
    starts = []
    for seg in segments:
        if seg[0] == 0x6474e550:
            off, addr = seg[2:4]
            if data[off:off+4] != b'\x01\x1b\x03\x3b':
                raise ValueError('Unsupported EH_FRAME_HDR encoding')
            count = struct.unpack_from('<I', data, off+8)[0]
            starts = sorted(addr+struct.unpack_from('<i', data, off+12+8*i)[0]
                            for i in range(count))
    def owner(addr):
        i = bisect.bisect_right(starts, addr)-1
        return starts[i] if i >= 0 else None
    strings = {}
    for m in re.finditer(rb'[\x20-\x7e]{8,}\x00', data):
        text = m[0][:-1].decode('ascii')
        if 'glaiel::' in text or 'LZ4FILE::' in text or 'LinearAllocator::' in text:
            addr = va(m.start())
            if addr is not None:
                strings[addr] = text
    names = collections.defaultdict(set)
    references = []
    # RIP-relative LEA byte patterns are candidates; disassemble before patching.
    for _, flags, off, addr, _, size, _, _ in loads:
        if not flags & 1:
            continue
        for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', data[off:off+size]):
            pos = off+m.start()
            if pos+7 > off+size:
                continue
            pc = addr+m.start()
            target = pc+7+struct.unpack_from('<i', data, pos+3)[0]
            if target in strings:
                fn = owner(pc)
                if fn is not None:
                    names[fn].add(strings[target])
                    references.append({'function': hex(fn), 'reference': hex(pc),
                                       'diagnostic': strings[target]})
    hot = []
    if profile:
        maps = []
        for line in (profile/'maps.txt').read_text().splitlines():
            p = line.split(None, 5)
            if len(p) == 6 and re.search(r'/app\.elf-.*\.box64$', p[5]):
                lo, hi = [int(v, 16) for v in p[0].split('-')]
                maps.append((lo, hi, int(p[2], 16)))
        perf = []
        for line in (profile/'perf.map').read_text().splitlines():
            p = line.split(None, 2)
            if len(p) == 3:
                try:
                    perf.append((int(p[0], 16), int(p[1], 16), p[2]))
                except ValueError:
                    pass
        perf.sort()
        pcs = [p[0] for p in perf]
        for line in (profile/'report-self.txt').read_text().splitlines():
            match = re.match(r'([\d.]+)%.*app\.elf-.*\[\+([0-9a-f]+)\]', line)
            if not match:
                continue
            weight, offset = float(match[1]), int(match[2], 16)
            item = {'percent': weight, 'cache_offset': hex(offset)}
            for lo, hi, fileoff in maps:
                if fileoff <= offset < fileoff+hi-lo:
                    pc = lo+offset-fileoff
                    idx = bisect.bisect_right(pcs, pc)-1
                    if idx >= 0 and pc < perf[idx][0]+perf[idx][1]:
                        item['jit_symbol'] = perf[idx][2]
                        guest = re.search(r'/app\.elf \+ 0x([0-9a-f]+)', perf[idx][2])
                        if guest:
                            guest_pc = int(guest[1], 16)
                            fn = owner(guest_pc)
                            item.update(guest_pc=hex(guest_pc), function=hex(fn) if fn is not None else None,
                                        candidate_names=sorted(names.get(fn, [])))
                    break
            hot.append(item)
    return {'sha256': hashlib.sha256(data).hexdigest(), 'file_bytes': len(data),
            'machine': struct.unpack_from('<H', data, 18)[0],
            'unwind_function_entries': len(starts),
            'diagnostic_strings': len(strings), 'named_function_candidates': len(names),
            'limits': ['LEA references are byte-pattern candidates, not verified instructions.',
                       'Diagnostic strings do not prove the containing function name.',
                       'Profile weights cover only rows present in report-self.txt.'],
            'profile_hot_instructions': hot, 'diagnostic_references': references}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fnv', help='print FNV-1a-64 for guest VA:length, e.g. 0x1000:64')
    args = parser.parse_args()
    result = analyze(args.elf, args.profile)
    if args.fnv:
        addr_text, length_text = args.fnv.split(':', 1)
        guest_va, length = int(addr_text, 0), int(length_text, 0)
        phoff = struct.unpack_from('<Q', args.elf.read_bytes(), 32)[0]
        raw = args.elf.read_bytes()
        phsize, phnum = struct.unpack_from('<HH', raw, 54)
        file_off = None
        for i in range(phnum):
            p = struct.unpack_from('<IIQQQQQQ', raw, phoff+i*phsize)
            if p[0] == 1 and p[3] <= guest_va < p[3]+p[6] and guest_va+length <= p[3]+p[6]:
                file_off = p[2]+guest_va-p[3]
                break
        if file_off is None:
            raise ValueError('VA range is outside a PT_LOAD segment')
        h = 0xcbf29ce484222325
        for b in raw[file_off:file_off+length]:
            h = ((h ^ b)*0x100000001b3) & 0xffffffffffffffff
        print(f'FNV1a64({guest_va:#x}:{length})={h:#018x}')
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False)+'\n')
    print(json.dumps({k: v for k, v in result.items()
                      if k not in ('diagnostic_references', 'profile_hot_instructions')}, indent=2))
    print('Profile rows:', len(result['profile_hot_instructions']))
    print('Resolved rows:', sum('guest_pc' in r for r in result['profile_hot_instructions']))


if __name__ == '__main__':
    main()
