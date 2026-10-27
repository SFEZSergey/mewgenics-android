#include <cstdint>

// Определение макросов из SceTypes.hpp или аналогичных, чтобы сохранить ABI
#define APS5_VABI __attribute__((visibility("default")))

typedef struct {
    int handle;
    const void* ptr;
} AudioOutOutputParam;

typedef struct {
    std::uint8_t output;
    std::uint8_t channel;
    std::uint8_t rerouteCounter;
    std::uint8_t volume;
    std::uint32_t flag;
    std::uint32_t activeState;
    std::uint32_t reserved[2];
} AudioOutPortState;

extern "C" {

APS5_VABI int sceAudioOutInit() { return 0; }
APS5_VABI int sceAudioOutOpen(int userId, int type, int index, std::uint32_t len, std::uint32_t freq, std::uint32_t param) { return 1; } // Возвращаем handle 1
APS5_VABI int sceAudioOutClose(int handle) { return 0; }
APS5_VABI int sceAudioOutOutput(int handle, const void* ptr) { return 0; }
APS5_VABI int sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num) { return 0; }
APS5_VABI int sceAudioOutSetVolume(int handle, std::uint32_t flag, int* vol) { return 0; }
APS5_VABI int sceAudioOutGetLastOutputTime(int handle, std::uint64_t* outputTime) { if(outputTime) *outputTime = 0; return 0; }
APS5_VABI int sceAudioOutGetPortState(int handle, AudioOutPortState* state) { return 0; }
APS5_VABI int sceAudioOutSysClose() { return 0; }
APS5_VABI int sceAudioOutSysConfigureOutput() { return 0; }
APS5_VABI int sceAudioOutSysGetHdmiMonitorInfo() { return 0; }
APS5_VABI int sceAudioOutSysOpen() { return 0; }
APS5_VABI int sceAudioOutSetMixLevelPadSpk(int handle, int mixLevel) { return 0; }
APS5_VABI int sceAudioOutExClose() { return 0; }
APS5_VABI int sceAudioOutExConfigureOutput() { return 0; }
APS5_VABI int sceAudioOutExGetMonitorInfo() { return 0; }
APS5_VABI int sceAudioOutExOpen() { return 0; }

}
