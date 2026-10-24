# Decodes a BC3 (DXT5) raw level to PNG (crop: max side), for inspecting dumped atlas levels.
import struct, sys, zlib
raw = open(sys.argv[1], 'rb').read(); w, h = int(sys.argv[2]), int(sys.argv[3]); out = sys.argv[4]
crop = int(sys.argv[5]) if len(sys.argv) > 5 else 1024
cw, ch = min(w, crop), min(h, crop)
px = bytearray(cw * ch * 4)
bw = (w + 3) // 4
for by in range(0, (ch + 3) // 4):
    for bx in range(0, (cw + 3) // 4):
        o = (by * bw + bx) * 16
        a0, a1 = raw[o], raw[o + 1]
        ai = int.from_bytes(raw[o + 2:o + 8], 'little')
        al = [a0, a1] + ([(a0 * (7 - i) + a1 * i) // 7 for i in range(1, 7)] if a0 > a1 else [(a0 * (5 - i) + a1 * i) // 5 for i in range(1, 5)] + [0, 255])
        c0, c1, ci = struct.unpack_from('<HHI', raw, o + 8)
        def rgb(c): return ((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
        p0, p1 = rgb(c0), rgb(c1)
        cols = [p0, p1, tuple((2 * a + b) // 3 for a, b in zip(p0, p1)), tuple((a + 2 * b) // 3 for a, b in zip(p0, p1))]
        for i in range(16):
            x, y = bx * 4 + i % 4, by * 4 + i // 4
            if x >= cw or y >= ch: continue
            c = cols[(ci >> (2 * i)) & 3]; a = al[(ai >> (3 * i)) & 7]
            q = (y * cw + x) * 4
            # composite over grey for viewing
            px[q:q + 4] = bytes((int(c[0] * a / 255 + 128 * (255 - a) / 255), int(c[1] * a / 255 + 128 * (255 - a) / 255), int(c[2] * a / 255 + 128 * (255 - a) / 255), 255))
rows = b''.join(b'\0' + bytes(px[y * cw * 4:(y + 1) * cw * 4]) for y in range(ch))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', cw, ch, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))
print(out, cw, ch)
