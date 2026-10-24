#!/bin/sh
# Compares resource interfaces of built SPIR-V against the shipped one.
cd "$(dirname "$0")/.."
python3 -I - <<'PY'
import os, sys
sys.path.insert(0, 'tools'); from spvif import iface
same = diff = 0
for f in sorted(os.listdir('shaders/spirv')):
    if not f.endswith(('.vert', '.frag')) or not os.path.exists('built/' + f): continue
    a, b = iface('shaders/spirv/' + f)[0], iface('built/' + f)[0]
    if a == b: same += 1
    else:
        diff += 1; print('DIFF', f, '\n   shipped', a, '\n   built  ', b)
print('resources: same', same, 'diff', diff)
PY
