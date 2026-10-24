# Builds SDL_GPU-layout SPIR-V from Mewgenics' GLSL (.shader) files.
# The uniform block is taken from the game's own .desc (names, offsets: what the engine writes), loose
# uniforms in the source are dropped; samplers keep declaration order and unused ones are removed by
# spirv-opt (the shipped SPIR-V was built that way).
# usage: glsl2spv.py <shaders dir> <out dir> [names...]
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from desc import desc

src, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
OPAQUE = re.compile(r'^(i|u)?(sampler|image)')
def expand(path, seen):
    text = open(path).read()
    def inc(m):
        p = os.path.join(os.path.dirname(path), m.group(1))
        return expand(p, seen) if p not in seen and not seen.add(p) else ''
    return re.sub(r'^\s*#\s*include\s+"([^"]+)"', inc, text, flags=re.M)
UNIFORM = re.compile(r'^[ \t]*uniform[ \t]+(\w+)[ \t]+(\w+)[ \t]*(\[[^\]]*\])?[ \t]*;', re.M)
def replace_fetch(text, buf):
    out, i, key = [], 0, re.compile(r'\btexelFetch\s*\(\s*%s\s*,' % buf)
    while True:
        m = key.search(text, i)
        if not m: out.append(text[i:]); return ''.join(out)
        depth, j = 1, m.end()
        while depth:
            depth += {'(': 1, ')': -1}.get(text[j], 0); j += 1
        index = text[m.end():j-1]
        out.append(text[i:m.start()]); out.append('vec4(%s_data[%s])' % (buf, index)); i = j

def rebind(path):
    # SDL_GPU order inside a set: sampled textures, storage textures, storage buffers (declaration order kept).
    dis = subprocess.run(['spirv-dis', '--raw-id', path], capture_output=True, text=True, check=True).stdout
    lines = dis.splitlines(); defs = {}; binds = {}; sets = {}
    for l in lines:
        m = re.match(r'\s*%(\d+) = (Op\w+)(.*)', l)
        if m: defs[m.group(1)] = (m.group(2), m.group(3).split())
        m = re.match(r'\s*OpDecorate %(\d+) (Binding|DescriptorSet) (\d+)', l)
        if m: (binds if m.group(2) == 'Binding' else sets)[m.group(1)] = int(m.group(3))
    def cls(v):
        t = defs[defs[v][1][0][1:]][1][1][1:]
        op = defs[t][0]
        if op == 'OpTypeSampledImage': return 0
        if op == 'OpTypeImage': return 1
        return 2 if defs[v][1][1] in ('StorageBuffer', 'Uniform') and not is_ubo(t) else 3
    def is_ubo(t): return any(re.match(r'\s*OpDecorate %%%s Block$' % t, l) for l in lines) and sets_of_ubo(t)
    def sets_of_ubo(t): return True
    newb = {}
    for s_ in set(sets.values()):
        vs = sorted((v for v in sets if sets[v] == s_), key=lambda v: (cls(v), binds[v]))
        for k, v in enumerate(vs): newb[v] = k
    if all(newb[v] == binds[v] for v in binds): return
    fixed = [re.sub(r'(OpDecorate %(\d+) Binding )\d+', lambda m: m.group(1) + str(newb[m.group(2)]), l) for l in lines]
    asm = path + '.s'; open(asm, 'w').write('\n'.join(fixed) + '\n')
    subprocess.run(['spirv-as', '--preserve-numeric-ids', asm, '-o', path], check=True); os.remove(asm)

names = sys.argv[3:] or sorted(f for f in os.listdir(src) if f.endswith('.shader') and f not in ('std_in.shader', 'swf_lib.shader'))
fails = 0
for name in names:
    body = expand(os.path.join(src, name), set())
    m = re.search(r'^\s*#\s*version\s+(\d+).*$', body, flags=re.M)
    version = '450'  # Vulkan GLSL; the sources are #version 150 core and compile unchanged
    body = re.sub(r'^\s*#\s*version.*$', '', body, count=1, flags=re.M)
    body = re.sub(r'\bgl_VertexID\b', 'gl_VertexIndex', body)
    types = {}
    for t, n, arr in UNIFORM.findall(body):
        if not OPAQUE.match(t): types[n.lower()] = (t, n, arr)
    body = UNIFORM.sub(lambda m: m.group(0) if OPAQUE.match(m.group(1)) else '', body)
    # SDL_GPU has no texel buffers: a read-only float array, as in the shipped SPIR-V (the native
    # backend binds what the PS5 build passes as a vertex storage texture there).
    for buf in re.findall(r'^[ \t]*uniform[ \t]+samplerBuffer[ \t]+(\w+)[ \t]*;', body, flags=re.M):
        body = re.sub(r'^[ \t]*uniform[ \t]+samplerBuffer[ \t]+%s[ \t]*;' % buf, 'layout(std430) readonly buffer %s_ssbo { float %s_data[]; };' % (buf, buf), body, flags=re.M)
        body = replace_fetch(body, buf)
    for stage, define, uset, sset, pssl in (('vert', 'COMPILING_VERTEX_PROGRAM', 1, 0, 'vs'), ('frag', 'COMPILING_FRAGMENT_PROGRAM', 3, 2, 'fs')):
        dpath = os.path.join(src, 'pssl', '%s.%s.desc' % (name, pssl))
        block = ''
        if os.path.exists(dpath):
            size, fields = desc(dpath)
            if fields:
                base = min(o for _, o, _ in fields) // 1024 * 1024
                members = []
                missing = [n for n, _, _ in fields if n.lower() not in types]
                listed = set(n.lower() for n, _, _ in fields)
                if missing:
                    fails += 1; print('NO TYPE', name, stage, missing); continue
                for n, o, sz in fields:
                    t, sname, arr = types[n.lower()]
                    members.append('    layout(offset = %d) %s %s%s;' % (o - base, t, sname, arr))
                block = 'layout(set = %d, binding = 0, std140) uniform DefaultUniforms {\n%s\n};\n' % (uset, '\n'.join(members))
        # Uniforms the engine's compiler found dead (not in .desc) stay plain globals so the source still compiles.
        listed = set(n.lower() for n, _, _ in (desc(dpath)[1] if os.path.exists(dpath) else []))
        block += ''.join('%s %s%s;\n' % (t, n, arr) for k, (t, n, arr) in types.items() if k not in listed)
        text = '#version %s\n#extension GL_ARB_enhanced_layouts : enable\n#extension GL_ARB_shading_language_420pack : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\n#define %s 1\n%s%s\nvoid main() { %s(); }\n' % (version, define, block, body, stage)
        tmp = os.path.join(out, name + '.' + stage + '.glsl'); open(tmp, 'w').write(text)
        raw = os.path.join(out, name + '.' + stage + '.raw')
        r = subprocess.run(['glslangValidator', '-V', '-S', stage, '--amb', '--aml', '--rsb', stage, str(sset), '-o', raw, tmp], capture_output=True, text=True)
        if r.returncode:
            fails += 1; print('FAIL', name, stage, r.stdout.strip().splitlines()[-2:]); continue
        r = subprocess.run(['spirv-opt', '-O', '--eliminate-dead-variables', raw, '-o', os.path.join(out, name + '.' + stage)], capture_output=True, text=True)
        if r.returncode: fails += 1; print('OPT FAIL', name, stage, r.stderr.strip()[-300:]); continue
        rebind(os.path.join(out, name + '.' + stage))
print('failures', fails)
