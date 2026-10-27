#include <cstdint>

#define APS5_VABI __attribute__((visibility("default")))

extern "C" {

APS5_VABI int sceVideoOutInit() { return 0; }
APS5_VABI int sceVideoOutOpen(int userId, int type, int index, std::uint32_t param) { return 1; }
APS5_VABI int sceVideoOutClose(int handle) { return 0; }
APS5_VABI int sceVideoOutSetWindowSize(int handle, std::uint32_t width, std::uint32_t height) { return 0; }
APS5_VABI int sceVideoOutGetWindowSize(int handle, std::uint32_t* width, std::uint32_t* height) { if(width) *width=1920; if(height) *height=1080; return 0; }
APS5_VABI int sceVideoOutSetFlipRate(int handle, int rate) { return 0; }
APS5_VABI int sceVideoOutGetFlipRate(int handle, int* rate) { if(rate) *rate=60; return 0; }
APS5_VABI int sceVideoOutRegisterCallback(int handle, void* callback, void* arg) { return 0; }
APS5_VABI int sceVideoOutUnregisterCallback(int handle, void* callback, void* arg) { return 0; }
APS5_VABI int sceVideoOutGetMonitorInfo(int handle, void* info) { return 0; }

// Dummy functions for input handling that might be referenced
APS5_VABI int sceVideoOutSetPadMapping(int handle, int pad) { return 0; }
APS5_VABI int sceVideoOutGetPadMapping(int handle) { return 0; }

}
