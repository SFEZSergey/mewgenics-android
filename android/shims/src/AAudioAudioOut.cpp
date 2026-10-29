#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <vector>
#include <aaudio/AAudio.h>

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

struct AudioPort {
    bool used = false;
    int type = 0;
    std::uint32_t samplesNum = 0;
    std::uint32_t freq = 48000;
    std::uint32_t channels = 2;
    aaudio_format_t format = AAUDIO_FORMAT_PCM_I16;
    AAudioStream* stream = nullptr;
    std::uint64_t lastOutputTime = 0;
    std::vector<int16_t> downmixBuffer16;
    std::vector<float> downmixBufferFloat;
};

static constexpr int MAX_PORTS = 32;
static AudioPort g_ports[MAX_PORTS];
static std::mutex g_audioMutex;

static std::uint64_t getTimeUs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void paceVirtual(uint32_t samples, uint32_t freq) {
    if (freq == 0) freq = 48000;
    uint64_t ns = (uint64_t)samples * 1000000000ULL / freq;
    struct timespec req;
    req.tv_sec = ns / 1000000000ULL;
    req.tv_nsec = ns % 1000000000ULL;
    nanosleep(&req, nullptr);
}

extern "C" {

APS5_VABI int sceAudioOutInit() {
    std::fprintf(stderr, "[audioout] sceAudioOutInit called\n");
    return 0;
}

APS5_VABI int sceAudioOutOpen(int userId, int type, int index, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
    (void)userId;
    (void)index;
    std::lock_guard<std::mutex> lock(g_audioMutex);

    if (len == 0) len = 512;
    if (freq == 0) freq = 48000;

    int slot = -1;
    for (int i = 0; i < MAX_PORTS; i++) {
        if (!g_ports[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        std::fprintf(stderr, "[audioout] no free ports\n");
        return -1;
    }

    AudioPort& p = g_ports[slot];
    p = AudioPort{};
    p.used = true;
    p.type = type;
    p.samplesNum = len;
    p.freq = freq;

    std::uint32_t formatParam = param & 0xFFu;
    bool isFloat = false;
    std::uint32_t srcChannels = 2;

    switch (formatParam) {
        case 0: srcChannels = 1; isFloat = false; break; // S16Mono
        case 1: srcChannels = 2; isFloat = false; break; // S16Stereo
        case 2: srcChannels = 8; isFloat = false; break; // S16_8Ch
        case 3: srcChannels = 1; isFloat = true;  break; // F32Mono
        case 4: srcChannels = 2; isFloat = true;  break; // F32Stereo
        case 5: srcChannels = 8; isFloat = true;  break; // F32_8Ch
        case 6: srcChannels = 8; isFloat = false; break; // S16_8ChStd
        case 7: srcChannels = 8; isFloat = true;  break; // F32_8ChStd
        default:
            srcChannels = 2;
            isFloat = false;
            break;
    }

    p.channels = srcChannels;
    p.format = isFloat ? AAUDIO_FORMAT_PCM_FLOAT : AAUDIO_FORMAT_PCM_I16;

    // We build AAudio stream with Stereo (2 ch) or Mono (1 ch)
    std::uint32_t outChannels = (srcChannels >= 2) ? 2 : 1;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t res = AAudio_createStreamBuilder(&builder);
    if (res == AAUDIO_OK && builder != nullptr) {
        AAudioStreamBuilder_setSampleRate(builder, freq);
        AAudioStreamBuilder_setChannelCount(builder, outChannels);
        AAudioStreamBuilder_setFormat(builder, p.format);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);

        res = AAudioStreamBuilder_openStream(builder, &p.stream);
        AAudioStreamBuilder_delete(builder);

        if (res == AAUDIO_OK && p.stream != nullptr) {
            AAudioStream_requestStart(p.stream);
            std::fprintf(stderr, "[audioout] port %d opened AAudio: %u Hz, %u ch (in %u ch), format %d\n",
                         slot + 1, freq, outChannels, srcChannels, (int)p.format);
        } else {
            std::fprintf(stderr, "[audioout] AAudio openStream failed: %d, fallback to virtual pacing\n", res);
            p.stream = nullptr;
        }
    } else {
        std::fprintf(stderr, "[audioout] AAudio createStreamBuilder failed: %d\n", res);
        p.stream = nullptr;
    }

    return slot + 1;
}

APS5_VABI int sceAudioOutClose(int handle) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    int slot = handle - 1;
    if (slot < 0 || slot >= MAX_PORTS || !g_ports[slot].used) {
        return -1;
    }
    AudioPort& p = g_ports[slot];
    if (p.stream) {
        AAudioStream_requestStop(p.stream);
        AAudioStream_close(p.stream);
        p.stream = nullptr;
    }
    p.used = false;
    std::fprintf(stderr, "[audioout] port %d closed\n", handle);
    return 0;
}

APS5_VABI int sceAudioOutOutput(int handle, const void* ptr) {
    int slot = handle - 1;
    if (slot < 0 || slot >= MAX_PORTS || !g_ports[slot].used) {
        return 0;
    }

    AudioPort* port = &g_ports[slot];
    port->lastOutputTime = getTimeUs();

    if (ptr == nullptr || port->stream == nullptr) {
        paceVirtual(port->samplesNum, port->freq);
        return (int)port->samplesNum;
    }

    // Check stream state
    aaudio_stream_state_t state = AAudioStream_getState(port->stream);
    if (state == AAUDIO_STREAM_STATE_STOPPED || state == AAUDIO_STREAM_STATE_PAUSED) {
        AAudioStream_requestStart(port->stream);
    } else if (state == AAUDIO_STREAM_STATE_DISCONNECTED) {
        // Stream disconnected, close and pace
        AAudioStream_close(port->stream);
        port->stream = nullptr;
        paceVirtual(port->samplesNum, port->freq);
        return (int)port->samplesNum;
    }

    const void* writeBuf = ptr;
    int32_t framesToWrite = (int32_t)port->samplesNum;

    // Downmix 8 channels to stereo if needed
    if (port->channels == 8) {
        if (port->format == AAUDIO_FORMAT_PCM_FLOAT) {
            port->downmixBufferFloat.resize(port->samplesNum * 2);
            const float* src = (const float*)ptr;
            float* dst = port->downmixBufferFloat.data();
            for (size_t f = 0; f < port->samplesNum; f++) {
                dst[f * 2 + 0] = src[f * 8 + 0]; // L
                dst[f * 2 + 1] = src[f * 8 + 1]; // R
            }
            writeBuf = dst;
        } else {
            port->downmixBuffer16.resize(port->samplesNum * 2);
            const int16_t* src = (const int16_t*)ptr;
            int16_t* dst = port->downmixBuffer16.data();
            for (size_t f = 0; f < port->samplesNum; f++) {
                dst[f * 2 + 0] = src[f * 8 + 0]; // L
                dst[f * 2 + 1] = src[f * 8 + 1]; // R
            }
            writeBuf = dst;
        }
    }

    // Timeout: 100 ms max
    int64_t timeoutNs = 100000000LL;
    aaudio_result_t res = AAudioStream_write(port->stream, writeBuf, framesToWrite, timeoutNs);
    if (res < 0) {
        // Fallback pacing on error
        paceVirtual(port->samplesNum, port->freq);
    }

    return (int)port->samplesNum;
}

APS5_VABI int sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num) {
    if (param == nullptr || num == 0) return 0;
    int ret = 0;
    for (std::uint32_t i = 0; i < num; i++) {
        ret = sceAudioOutOutput(param[i].handle, param[i].ptr);
    }
    return ret;
}

APS5_VABI int sceAudioOutSetVolume(int handle, std::uint32_t flag, int* vol) {
    (void)handle; (void)flag; (void)vol;
    return 0;
}

APS5_VABI int sceAudioOutGetLastOutputTime(int handle, std::uint64_t* outputTime) {
    int slot = handle - 1;
    if (slot >= 0 && slot < MAX_PORTS && g_ports[slot].used) {
        if (outputTime) *outputTime = g_ports[slot].lastOutputTime;
    } else {
        if (outputTime) *outputTime = 0;
    }
    return 0;
}

APS5_VABI int sceAudioOutGetPortState(int handle, AudioOutPortState* state) {
    (void)handle;
    if (state) {
        std::memset(state, 0, sizeof(*state));
        state->activeState = 1;
    }
    return 0;
}

APS5_VABI int sceAudioOutSysClose() { return 0; }
APS5_VABI int sceAudioOutSysConfigureOutput() { return 0; }
APS5_VABI int sceAudioOutSysGetHdmiMonitorInfo() { return 0; }
APS5_VABI int sceAudioOutSysOpen() { return 1; }
APS5_VABI int sceAudioOutSetMixLevelPadSpk(int handle, int mixLevel) { (void)handle; (void)mixLevel; return 0; }
APS5_VABI int sceAudioOutExClose() { return 0; }
APS5_VABI int sceAudioOutExConfigureOutput() { return 0; }
APS5_VABI int sceAudioOutExGetMonitorInfo() { return 0; }
APS5_VABI int sceAudioOutExOpen() { return 1; }

}
