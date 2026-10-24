// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceVideoOut.prx, переадресующая вызовы
// в нативную (aarch64) сборку через мост box64. Не править вручную.
#include "prx/aps5native/X86Host.hpp"

#include <cstdint>
#include <cstring>

namespace {

[[maybe_unused]] double floatBits(float value) {
    // float передаётся в младших 32 битах d-регистра (нативная сторона читает s-регистр).
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

const char* const names[] = {
    "sceVideoOutAddFlipEvent",
    "sceVideoOutAddOutputModeEvent",
    "sceVideoOutAddPreVblankStartEvent",
    "sceVideoOutAddVblankEvent",
    "sceVideoOutAddVrrActiveStatusEvent",
    "sceVideoOutAddVrrStatusFlagsPrivilege",
    "sceVideoOutAdjustColor",
    "sceVideoOutAdjustColor_",
    "sceVideoOutAllowOutputResolutionWqhdDetection",
    "sceVideoOutClose",
    "sceVideoOutColorSettingsSetGamma",
    "sceVideoOutColorSettingsSetGamma_",
    "sceVideoOutConfigureOutput",
    "sceVideoOutDeleteFlipEvent",
    "sceVideoOutDeleteOutputModeEvent",
    "sceVideoOutDeletePreVblankStartEvent",
    "sceVideoOutDeleteVblankEvent",
    "sceVideoOutGetEventCount",
    "sceVideoOutGetEventData",
    "sceVideoOutGetEventId",
    "sceVideoOutGetFlipStatus",
    "sceVideoOutGetOutputStatus",
    "sceVideoOutGetOutputStatusWithVrr",
    "sceVideoOutGetResolutionStatus",
    "sceVideoOutGetVblankStatus",
    "sceVideoOutInitializeOutputOptions",
    "sceVideoOutIsFlipPending",
    "sceVideoOutIsOutputSupported",
    "sceVideoOutLatencyControlWaitBeforeInput",
    "sceVideoOutLatencyMeasureSetStartPoint",
    "sceVideoOutOpen",
    "sceVideoOutRegisterBuffers2",
    "sceVideoOutSetBufferAttribute2",
    "sceVideoOutSetFlipRate",
    "sceVideoOutSetWindowModeMargins",
    "sceVideoOutSubmitChangeBufferAttribute2",
    "sceVideoOutSubmitFlip",
    "sceVideoOutUnregisterBuffers",
    "sceVideoOutVrrPegToFixedRate",
    "sceVideoOutVrrUnpegFromFixedRate",
    "sceVideoOutWaitVblank",
};
void* functions[41] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceVideoOut.prx", names, functions, 41);
}

} // namespace

extern "C" std::uint64_t sceVideoOutAddFlipEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddFlipEvent");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAddOutputModeEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddOutputModeEvent");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAddPreVblankStartEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddPreVblankStartEvent");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAddVblankEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddVblankEvent");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAddVrrActiveStatusEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddVrrActiveStatusEvent");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAddVrrStatusFlagsPrivilege() {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAddVrrStatusFlagsPrivilege");
    return Aps5NativeCall_nid_no_patch(functions[5], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAdjustColor(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAdjustColor");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAdjustColor_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAdjustColor_");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutAllowOutputResolutionWqhdDetection(std::uint64_t a0) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutAllowOutputResolutionWqhdDetection");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutClose(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutClose");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutColorSettingsSetGamma(std::uint64_t a0, float a1) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutColorSettingsSetGamma");
    return Aps5NativeCallFd_nid_no_patch(functions[10], a0, 0, 0, 0, 0, 0, floatBits(a1), 0.0, 0.0, 0.0);
}

extern "C" std::uint64_t sceVideoOutColorSettingsSetGamma_(std::uint64_t a0, float a1, std::uint64_t a2) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutColorSettingsSetGamma_");
    return Aps5NativeCallFd_nid_no_patch(functions[11], a0, a2, 0, 0, 0, 0, floatBits(a1), 0.0, 0.0, 0.0);
}

extern "C" std::uint64_t sceVideoOutConfigureOutput(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutConfigureOutput");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, a1, a2, a3, a4, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutDeleteFlipEvent(std::uint64_t a0, std::uint64_t a1) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutDeleteFlipEvent");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutDeleteOutputModeEvent(std::uint64_t a0, std::uint64_t a1) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutDeleteOutputModeEvent");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutDeletePreVblankStartEvent(std::uint64_t a0, std::uint64_t a1) {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutDeletePreVblankStartEvent");
    return Aps5NativeCall_nid_no_patch(functions[15], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutDeleteVblankEvent(std::uint64_t a0, std::uint64_t a1) {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutDeleteVblankEvent");
    return Aps5NativeCall_nid_no_patch(functions[16], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetEventCount(std::uint64_t a0) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetEventCount");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetEventData(std::uint64_t a0, std::uint64_t a1) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetEventData");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetEventId(std::uint64_t a0) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetEventId");
    return Aps5NativeCall_nid_no_patch(functions[19], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetFlipStatus(std::uint64_t a0, std::uint64_t a1) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetFlipStatus");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetOutputStatus(std::uint64_t a0, std::uint64_t a1) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetOutputStatus");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetOutputStatusWithVrr(std::uint64_t a0, std::uint64_t a1) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetOutputStatusWithVrr");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetResolutionStatus(std::uint64_t a0, std::uint64_t a1) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetResolutionStatus");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutGetVblankStatus(std::uint64_t a0, std::uint64_t a1) {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutGetVblankStatus");
    return Aps5NativeCall_nid_no_patch(functions[24], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutInitializeOutputOptions(std::uint64_t a0) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutInitializeOutputOptions");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutIsFlipPending(std::uint64_t a0) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutIsFlipPending");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutIsOutputSupported(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutIsOutputSupported");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, a1, a2, a3, a4, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutLatencyControlWaitBeforeInput(std::uint64_t a0) {
    if (functions[28] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutLatencyControlWaitBeforeInput");
    return Aps5NativeCall_nid_no_patch(functions[28], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutLatencyMeasureSetStartPoint(std::uint64_t a0, std::uint64_t a1) {
    if (functions[29] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutLatencyMeasureSetStartPoint");
    return Aps5NativeCall_nid_no_patch(functions[29], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[30] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutOpen");
    return Aps5NativeCall_nid_no_patch(functions[30], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutRegisterBuffers2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6, std::uint64_t a7) {
    if (functions[31] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutRegisterBuffers2");
    return Aps5NativeCall_nid_no_patch(functions[31], a0, a1, a2, a3, a4, a5, a6, a7);
}

extern "C" void sceVideoOutSetBufferAttribute2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6, std::uint64_t a7) {
    if (functions[32] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutSetBufferAttribute2");
    Aps5NativeCall_nid_no_patch(functions[32], a0, a1, a2, a3, a4, a5, a6, a7);
}

extern "C" std::uint64_t sceVideoOutSetFlipRate(std::uint64_t a0, std::uint64_t a1) {
    if (functions[33] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutSetFlipRate");
    return Aps5NativeCall_nid_no_patch(functions[33], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutSetWindowModeMargins(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[34] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutSetWindowModeMargins");
    return Aps5NativeCall_nid_no_patch(functions[34], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutSubmitChangeBufferAttribute2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[35] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutSubmitChangeBufferAttribute2");
    return Aps5NativeCall_nid_no_patch(functions[35], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutSubmitFlip(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[36] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutSubmitFlip");
    return Aps5NativeCall_nid_no_patch(functions[36], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutUnregisterBuffers(std::uint64_t a0, std::uint64_t a1) {
    if (functions[37] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutUnregisterBuffers");
    return Aps5NativeCall_nid_no_patch(functions[37], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutVrrPegToFixedRate(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[38] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutVrrPegToFixedRate");
    return Aps5NativeCall_nid_no_patch(functions[38], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutVrrUnpegFromFixedRate(std::uint64_t a0) {
    if (functions[39] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutVrrUnpegFromFixedRate");
    return Aps5NativeCall_nid_no_patch(functions[39], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceVideoOutWaitVblank(std::uint64_t a0) {
    if (functions[40] == nullptr) Aps5NativeMissing_nid_no_patch("sceVideoOutWaitVblank");
    return Aps5NativeCall_nid_no_patch(functions[40], a0, 0, 0, 0, 0, 0, 0, 0);
}
