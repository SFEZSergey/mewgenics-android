#!/usr/bin/env python3
# Generates adapters.inc from SDL's src/gpu/SDL_sysgpu.h: one native adapter per SDL_GPUDevice slot,
# called by a box64 bridge with the x86 argument registers as 64-bit words.
#   kind 0: slot left NULL (XR: the game's Agc backend has none either)
#   kind 1: up to 6 integer/pointer arguments (rdi, rsi, rdx, rcx, r8, r9)
#   kind 2: SetBlendConstants (SDL_FColor by value: xmm0/xmm1 pairs -> 4 floats)
#   kind 3: 7 integer arguments (the 7th from the x86 stack)
# Narrow arguments are cut to their size (x86 leaves the upper register bits undefined); the
# props of a CreateInfo is dropped (an ID of the game's SDL, unknown to this one), and shader code
# goes through Aps5MapShader (PSSL -> SPIR-V).
import re, sys

src = open(sys.argv[1]).read()
body = src[src.index('struct SDL_GPUDevice\n{'):]
body = body[body.index('{') + 1: body.index('\n};')]
body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
body = re.sub(r'//[^\n]*', '', body)

POINTER = re.compile(r'\*')
out, kinds, names = [], [], []
for decl in body.split(';'):
    d = ' '.join(decl.split())
    if not d:
        continue
    m = re.match(r'(.*?)\(\s*\*\s*(\w+)\s*\)\s*\((.*)\)$', d)
    if not m:
        continue
    ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
    names.append(name)
    params = [] if args in ('', 'void') else [a.strip() for a in args.split(',')]
    if 'XR' in name:
        kinds.append(0)
        continue
    split = []
    for p in params:
        pm = re.match(r'(.*?)(\w+)$', p)
        split.append((pm.group(1).strip(), pm.group(2)))
    if name == 'SetBlendConstants':
        kinds.append(2)
        out.append('static uint64_t A_%s(uint64_t a0, float r, float g, float b, float a)\n{\n    if (countCalls) __atomic_fetch_add(&slotCalls[%d], 1, __ATOMIC_RELAXED);\n'
                   '    fprintf(stderr, "[sdlgpu] >> SetBlendConstants\\n");\n'
                   '    const SDL_FColor color = {r, g, b, a};\n'
                   '    real.%s((SDL_GPUCommandBuffer *)(uintptr_t)a0, color);\n    return 0;\n}\n' % (name, len(names) - 1, name))
        continue
    if len(split) > 7:
        raise SystemExit('%s: %d arguments' % (name, len(split)))
    kinds.append(3 if len(split) == 7 else 1)
    pre, call = [], []
    for i, (t, n) in enumerate(split):
        a = 'a%d' % i
        if POINTER.search(t):
            info = re.match(r'const (SDL_GPU\w+CreateInfo) \*$', t)
            if info:
                pre.append('    %s info%d = *(const %s *)(uintptr_t)%s;\n    info%d.props = 0;\n' % (info.group(1), i, info.group(1), a, i))
                if name == 'CreateShader':
                    pre.append('    if (!Aps5MapShader(&info%d)) return 0;\n' % i)
                call.append('&info%d' % i)
            else:
                call.append('(%s)(uintptr_t)%s' % (t, a))
        elif t == 'bool' and n == 'cycle' and name != 'MapTransferBuffer':
            # PS5 semantics: GPU writes land in place (Agc has no resource cycling); a cycled SDL resource
            # keeps only the part written now (atlas cells, partial targets) -> always in place.
            call.append('(cycleWrites ? (bool)(%s & 0xff) : false)' % a)
        elif t == 'bool':
            call.append('(bool)(%s & 0xff)' % a)
        elif t in ('Uint8', 'Uint16', 'Uint32', 'Uint64', 'size_t', 'SDL_PropertiesID'):
            call.append('(%s)%s' % (t, a))
        elif t in ('Sint32', 'int'):
            call.append('(%s)(int32_t)%s' % (t, a))
        else:  # enums and other 32-bit values
            call.append('(%s)(Uint32)%s' % (t, a))
    sig = ', '.join('uint64_t a%d' % i for i in range(len(split))) or 'void'
    invoke = 'real.%s(%s)' % (name, ', '.join(call))
    guard = '    if (!real.%s) { fprintf(stderr, "[sdlgpu] unbridged slot %s called!\\n"); return 0; }\n' % (name, name)
    log_enter = '    static int enter_%s; if (enter_%s++ < 5) fprintf(stderr, "[sdlgpu] >> %s\\n");\n' % (name, name, name)
    log_exit = '    if (enter_%s <= 5) fprintf(stderr, "[sdlgpu] << %s\\n");\n' % (name, name)
    if name == 'AcquireCommandBuffer':
        tail = guard + log_enter + '    SDL_GPUCommandBuffer *cb = real.AcquireCommandBuffer(%s);\n    if (cb) ((CommandBufferCommonHeader *)cb)->device = (SDL_GPUDevice *)&real;\n' % ', '.join(call) + log_exit + '    return (uint64_t)(uintptr_t)cb;\n'
    elif ret == 'void':
        tail = guard + log_enter + '    %s;\n' % invoke + log_exit + '    return 0;\n'
    elif POINTER.search(ret):
        tail = guard + log_enter + '    uint64_t res = (uint64_t)(uintptr_t)%s;\n' % invoke + log_exit + '    return res;\n'
    elif ret == 'bool':
        tail = guard + log_enter + '    uint64_t res = %s ? 1 : 0;\n' % invoke + log_exit + '    return res;\n'
    else:
        tail = guard + log_enter + '    uint64_t res = (uint64_t)%s;\n' % invoke + log_exit + '    return res;\n'
    out.append('static uint64_t A_%s(%s)\n{\n    if (countCalls) __atomic_fetch_add(&slotCalls[%d], 1, __ATOMIC_RELAXED);\n%s%s}\n' % (name, sig, len(names) - 1, ''.join(pre), tail))

with open(sys.argv[2], 'w') as f:
    f.write('// Generated by gen_adapters.py from SDL_sysgpu.h; do not edit.\n')
    f.write('static unsigned long slotCalls[%d];\n' % len(names))
    f.write('static bool countCalls;  // APS5_SDLGPU_COUNTS=1\n')
    f.write('static bool cycleWrites;  // APS5_SDLGPU_CYCLE=1: pass the title\'s cycle flags through\n')
    f.write('static const char *const slotNames[%d] = {%s};\n' % (len(names), ', '.join('"%s"' % n for n in names)))
    f.write('\n'.join(out))
    f.write('\nstatic const unsigned char slotKinds[%d] = {%s};\n' % (len(kinds), ', '.join(map(str, kinds))))
    f.write('static void installAdapters(SDL_GPUDevice *device)\n{\n')
    for name, kind in zip(names, kinds):
        f.write('    device->%s = %s;\n' % (name, 'NULL' if kind == 0 else '(void *)A_%s' % name))
    f.write('}\n')
print(len(names), 'slots,', kinds.count(0), 'null,', kinds.count(2), 'float,', kinds.count(3), 'seven')
