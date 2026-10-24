import struct, sys, zlib
b = open(sys.argv[1], 'rb').read()
off, = struct.unpack_from('<I', b, 10); w, h = struct.unpack_from('<ii', b, 18); row = (w * 3 + 3) & ~3
raw = bytearray()
for y in range(h):
    src = b[off + (h - 1 - y) * row: off + (h - 1 - y) * row + w * 3]
    raw.append(0); raw += bytes(x for i in range(0, len(src), 3) for x in (src[i + 2], src[i + 1], src[i]))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw), 6)) + chunk(b'IEND', b''))
