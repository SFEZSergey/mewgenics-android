#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>

#define APS5_VABI __attribute__((visibility("default")))

struct SystemServiceDisplaySafeAreaInfo {
    float ratio;
};

struct SystemServiceHdrToneMapLuminance {
    float min_luminance;
    float max_luminance;
};

struct SystemServiceHdrCapabilities {
    std::uint32_t supported;
};

extern "C" {

APS5_VABI int sceSystemServiceLoadExec(const char* path, const char* const* args) {
    std::exit(0);
}

APS5_VABI int sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) { return 0; }

APS5_VABI int sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
    if (info) info->ratio = 1.0f;
    return 0;
}

APS5_VABI int sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* lum) {
    if (lum) { lum->min_luminance = 0.0f; lum->max_luminance = 1000.0f; }
    return 0;
}

APS5_VABI int sceSystemServiceGetHdrCapabilities(SystemServiceHdrCapabilities* cap) {
    if (cap) cap->supported = 0;
    return 0;
}

APS5_VABI int sceSystemServiceParamGetInt(int paramId, int* value) {
    if (value) *value = 1; // English US / standard
    return 0;
}

APS5_VABI int sceSystemServiceParamGetString(int paramId, char* buf, size_t bufSize) {
    if (buf && bufSize > 0) buf[0] = '\0';
    return 0;
}

APS5_VABI int sceSystemServiceGetStatus(int* status) {
    if (status) *status = 0;
    return 0;
}

APS5_VABI int sceSystemServiceHideSplashScreen(void) { return 0; }
APS5_VABI int sceSystemServiceReenableSplashScreen(void) { return 0; }
APS5_VABI int sceSystemServiceSetPowerSavingMode(int mode) { return 0; }

}
