import struct, sys
def desc(path):
    b = open(path, 'rb').read(); n, size, z = struct.unpack_from('<III', b, 0); o = 12; out = []
    for _ in range(n):
        l, = struct.unpack_from('<I', b, o); o += 4; name = b[o:o+l].decode(); o += l
        off, sz = struct.unpack_from('<II', b, o); o += 8; out.append((name, off, sz))
    return size, out
if __name__ == '__main__':
    for p in sys.argv[1:]: print(p.split('/')[-1], desc(p))
