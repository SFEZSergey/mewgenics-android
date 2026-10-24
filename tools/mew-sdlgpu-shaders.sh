#!/data/data/com.termux/files/usr/bin/bash
# Mewgenics (PPSA37318): SPIR-V for the native Vulkan SDL_GPU backend (APS5_SDLGPU_VULKAN=1).
# Builds <game>/sdlgpu/<FNV-1a of each PSSL shader>.spv from the game's own shaders: the shipped
# SDL-layout SPIR-V where there is one, the rest compiled from the game's GLSL (tools/sdlgpu).
# Needs Termux python, glslang and spirv-tools; reads the game through su.
set -euo pipefail
GAME_ID="${GAME_ID:-PPSA37318}"
G="/data/data/org.anyps5.android/files/games/$GAME_ID"
HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
su -c "cp -r '$G/shaders' '$W/shaders' && chown -R $(id -u):$(id -g) '$W/shaders'"
python3 -I "$HERE/sdlgpu/glsl2spv.py" "$W/shaders" "$W/built" | tail -n 1
python3 -I "$HERE/sdlgpu/maketable.py" "$W/shaders" "$W/built" "$W/table"
chmod -R a+rX "$W/table"
su -c "rm -rf '$G/sdlgpu' && cp -r '$W/table' '$G/sdlgpu' && chown -R \$(stat -c %u:%g '$G') '$G/sdlgpu' && chcon -R \$(stat -c %C '$G') '$G/sdlgpu' && echo \"installed \$(ls '$G/sdlgpu' | wc -l) files in $G/sdlgpu\""
