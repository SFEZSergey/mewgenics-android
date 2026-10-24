# Resource/IO interface of a SPIR-V module (names ignored): sets/bindings with kind, block member offsets, IO locations with types.
import re, subprocess, sys
def iface(path):
    dis = subprocess.run(['spirv-dis', '--raw-id', path], capture_output=True, text=True).stdout
    defs = {}; dec = {}; mdec = {}
    for l in dis.splitlines():
        m = re.match(r'\s*%(\d+) = (Op\w+)(.*)', l)
        if m: defs[m.group(1)] = (m.group(2), m.group(3).split()); continue
        m = re.match(r'\s*OpDecorate %(\d+) (\w+)\s*(\S*)', l)
        if m: dec.setdefault(m.group(1), {})[m.group(2)] = m.group(3); continue
        m = re.match(r'\s*OpMemberDecorate %(\d+) (\d+) (\w+)\s*(\S*)', l)
        if m: mdec.setdefault(m.group(1), {}).setdefault(int(m.group(2)), {})[m.group(3)] = m.group(4)
    def tname(i, depth=0):
        op, a = defs.get(i, ('?', []))
        if op == 'OpTypePointer': return tname(a[1][1:], depth)
        if op in ('OpTypeVector', 'OpTypeMatrix'): return '%s<%s,%s>' % (op[6:], tname(a[0][1:]), a[1])
        if op == 'OpTypeArray': return 'Array<%s>' % tname(a[0][1:])
        if op == 'OpTypeImage': return 'Image<%s,%s,arr%s>' % (tname(a[0][1:]), a[1], a[3])
        if op == 'OpTypeSampledImage': return 'Sampled<%s>' % tname(a[0][1:])
        if op == 'OpTypeStruct':
            offs = [mdec.get(i, {}).get(k, {}).get('Offset') for k in range(len(a))]
            return 'Struct{%s}' % ','.join('%s@%s' % (tname(x[1:]), o) for x, o in zip(a, offs))
        return op[6:]
    res, io = [], []
    for i, (op, a) in defs.items():
        if op != 'OpVariable': continue
        sc = a[1]; d = dec.get(i, {})
        if 'DescriptorSet' in d: res.append((int(d['DescriptorSet']), int(d['Binding']), tname(a[0][1:])))
        elif sc in ('Input', 'Output') and 'Location' in d: io.append((sc, int(d['Location']), tname(a[0][1:])))
    return sorted(res), sorted(io)
if __name__ == '__main__':
    a, b = iface(sys.argv[1]), iface(sys.argv[2])
    print('SAME' if a == b else 'DIFF\n  shipped %s\n  built   %s' % (a, b))
