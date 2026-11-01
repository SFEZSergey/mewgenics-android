# AI assistant notes

Reply to the user in Russian. This is an Android port of Mewgenics (PS5); see `README.md` and `TOOLS.md`.

Working agreements:
* Do not launch or restart the game in a loop. One launch after a build is allowed if the user agrees.
  The game may be closed to install an APK. Do not tap the screen or take screenshots unless asked;
  `tools/sesscap.sh` can capture screenshots passively while the game is on screen.
* Batch all discovered changes into one pass and one build, then give a brief report: quota is limited.
* Do not merge large upstream updates without asking.
* The goal is to maximize native ARM code (replace x86 code with ARM64 code, preserve game logic bit for bit).
* Do not put game data in the repository.

Where to make changes:
* Native rendering path: `android/sdlgpu/backend.c` (overrides for `L_*` slots),
  `shim.c` (SDL3 runtime replacement), `hostwin.c` (APK window/Vulkan loader), and
  `gen_adapters.py` (generates `adapters.inc` from `SDL_sysgpu.h`).
* box64 bridge: `patches/box64-local.patch` (`src/tools/aps5hot.c`).
* Check hypotheses against logs (`$G/box64.log`, `[sdlgpu]` lines) and collector screenshots, not by eye.
