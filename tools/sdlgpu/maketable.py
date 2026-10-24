# Maps each PSSL blob the game hands to SDL_CreateGPUShader to SDL_GPU SPIR-V:
# shipped shaders/spirv where its layout is SDL's, otherwise our build (built/).
# Output: <out>/<FNV-1a 64 of the PSSL file, 16 hex>.spv + index.txt (hash size stage name source).
import hashlib, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from spvif import iface
shaders, built, out = sys.argv[1:4]; os.makedirs(out, exist_ok=True)
def fnv(b):
    h = 0xcbf29ce484222325
    for x in b: h = ((h ^ x) * 0x100000001b3) & 0xffffffffffffffff
    return '%016x' % h
SDL_SETS = {'vert': {0, 1}, 'frag': {2, 3}}
def slots(t):
    # Interface locations a type takes (vectors/scalars 1, matrices one per column, structs the sum).
    if t.startswith('Struct{'):
        inner, depth, parts, cur = t[7:-1], 0, [], ''
        for ch in inner:
            depth += ch in '<{'; depth -= ch in '>}'
            if ch == ',' and depth == 0: parts.append(cur); cur = ''
            else: cur += ch
        parts.append(cur)
        return sum(slots(x.split('@')[0]) for x in parts if x)
    if t.startswith('Matrix<'): return int(t.rsplit(',', 1)[1].rstrip('>'))
    return 1

def outputs(path):
    covered = set()
    for sc, loc, t in iface(path)[1]:
        if sc == 'Output': covered.update(range(loc, loc + slots(t)))
    return covered

def candidates(name, stage):
    shipped = os.path.join(shaders, 'spirv', '%s.%s' % (name, stage)); ours = os.path.join(built, '%s.%s' % (name, stage))
    found = []
    # The shipped SPIR-V is the engine's own compile (our GLSL builds do not match its uniforms for
    # the swf shaders); its bitmap_atlas_info storage buffer is handled by the native backend.
    res = iface(shipped)[0] if os.path.exists(shipped) else None
    if res is not None and {s for s, _, _ in res} <= SDL_SETS[stage]: found.append(('shipped', shipped))
    if os.path.exists(ours): found.append(('built', ours))
    return found

# One PSSL blob can serve several programs (a shared vertex shader); the shipped SPIR-V of each
# program dropped the outputs its own fragment shader ignores, so pick a variant whose outputs cover
# every variant's (else our full build).
groups = {}
for f in sorted(os.listdir(os.path.join(shaders, 'pssl'))):
    if not f.endswith(('.vs', '.fs')): continue
    name, stage = f[:-3], 'vert' if f.endswith('.vs') else 'frag'
    blob = open(os.path.join(shaders, 'pssl', f), 'rb').read()
    groups.setdefault(fnv(blob), []).append((name, stage, len(blob)))
def choose(members, forced=()):
    stage = members[0][1]
    cands = [c for name, st, _ in members for c in candidates(name, st)]
    if not cands: return None
    if any(name in forced for name, _, _ in members):
        built_c = [c for c in cands if c[0] == 'built']
        return built_c[0] if built_c else cands[0]
    pick = cands[0]
    if len(members) > 1 and stage == 'vert':
        shipped = [c for c in cands if c[0] == 'shipped']
        need = set().union(*(outputs(p) for _, p in shipped)) if shipped else set()
        full = [c for c in shipped if outputs(c[1]) >= need]
        pick = full[0] if full else next((c for c in cands if c[0] == 'built'), cands[0])
    return pick

# Our builds keep the declared interface locations, the shipped SPIR-V compacted them: a program
# whose vertex shader comes from our build takes our fragment shader too.
forced = set()
for sha, members in groups.items():
    if members[0][1] == 'vert':
        pick = choose(members)
        # Only when our vertex build could not match the shipped fragment inputs; with the shipped
        # SPIR-V preferred everywhere this is empty (b39e40...: our passthrough writes color/texcoord at
        # locations 0/1 like the shipped variants).
        if pick and pick[0] == 'built' and os.environ.get('SDLGPU_FORCE_PAIRS'): forced.update(name for name, _, _ in members)
rows, missing = [], []
for sha, members in sorted(groups.items()):
    stage = members[0][1]
    pick = choose(members, forced)
    if not pick: missing.append(members[0][0]); continue
    if stage == 'frag' and pick[0] != 'built' and any(n in forced for n, _, _ in members):
        print('WARNING: %s keeps shipped %s (no build) with a built vertex shader' % (sha, pick[1]))
    if stage == 'frag' and len({n in forced for n, _, _ in members}) > 1:
        print('WARNING: fragment blob %s shared by programs with built and shipped vertex shaders' % sha)
    shutil.copyfile(pick[1], os.path.join(out, sha + '.spv'))
    rows.append('%s %d %s %s %s %s' % (sha, members[0][2], stage, os.path.basename(pick[1]), pick[0], ','.join(n for n, _, _ in members)))
open(os.path.join(out, 'index.txt'), 'w').write('\n'.join(rows) + '\n')
print(len(rows), 'blobs mapped;', 'missing:', missing)
