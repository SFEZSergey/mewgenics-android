// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceAudioOut.prx, переадресующая вызовы
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
    "sceAudioOut2ContextAdvance",
    "sceAudioOut2ContextCreate",
    "sceAudioOut2ContextDestroy",
    "sceAudioOut2ContextGetQueueLevel",
    "sceAudioOut2ContextPush",
    "sceAudioOut2ContextQueryMemory",
    "sceAudioOut2ContextResetParam",
    "sceAudioOut2ContextSetAttributes",
    "sceAudioOut2EnableChat",
    "sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients",
    "sceAudioOut2GetSpeakerArrayMemorySize",
    "sceAudioOut2GetSpeakerInfo",
    "sceAudioOut2GetSystemState",
    "sceAudioOut2Initialize",
    "sceAudioOut2MasteringGetState",
    "sceAudioOut2MasteringInit",
    "sceAudioOut2MasteringSetParam",
    "sceAudioOut2MasteringTerm",
    "sceAudioOut2PortCreate",
    "sceAudioOut2PortDestroy",
    "sceAudioOut2PortGetState",
    "sceAudioOut2PortSetAttributes",
    "sceAudioOut2Set3DLatency",
    "sceAudioOut2SetSystemDebugState",
    "sceAudioOut2SpeakerArrayCreate",
    "sceAudioOut2SpeakerArrayDestroy",
    "sceAudioOut2UserCreate",
    "sceAudioOut2UserDestroy",
    "sceAudioOut2UserGetSupportedAttributes",
    "sceAudioOutClose",
    "sceAudioOutExClose",
    "sceAudioOutExConfigureOutput",
    "sceAudioOutExGetMonitorInfo",
    "sceAudioOutExOpen",
    "sceAudioOutGetLastOutputTime",
    "sceAudioOutGetPortState",
    "sceAudioOutInit",
    "sceAudioOutOpen",
    "sceAudioOutOutput",
    "sceAudioOutOutputs",
    "sceAudioOutSetMixLevelPadSpk",
    "sceAudioOutSetVolume",
    "sceAudioOutSysClose",
    "sceAudioOutSysConfigureOutput",
    "sceAudioOutSysGetHdmiMonitorInfo",
    "sceAudioOutSysOpen",
};
void* functions[46] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceAudioOut.prx", names, functions, 46);
}

} // namespace

extern "C" std::uint64_t sceAudioOut2ContextAdvance(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextAdvance");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextCreate(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextCreate");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextDestroy(std::uint64_t a0) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextDestroy");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextGetQueueLevel(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextGetQueueLevel");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextPush(std::uint64_t a0, std::uint64_t a1) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextPush");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextQueryMemory(std::uint64_t a0, std::uint64_t a1) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextQueryMemory");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextResetParam(std::uint64_t a0) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextResetParam");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2ContextSetAttributes(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2ContextSetAttributes");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2EnableChat() {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2EnableChat");
    return Aps5NativeCall_nid_no_patch(functions[8], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2GetSpeakerArrayMemorySize(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2GetSpeakerArrayMemorySize");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2GetSpeakerInfo(std::uint64_t a0, std::uint64_t a1) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2GetSpeakerInfo");
    return Aps5NativeCall_nid_no_patch(functions[11], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2GetSystemState(std::uint64_t a0) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2GetSystemState");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2Initialize() {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2Initialize");
    return Aps5NativeCall_nid_no_patch(functions[13], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2MasteringGetState(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2MasteringGetState");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2MasteringInit(std::uint64_t a0) {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2MasteringInit");
    return Aps5NativeCall_nid_no_patch(functions[15], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2MasteringSetParam(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2MasteringSetParam");
    return Aps5NativeCall_nid_no_patch(functions[16], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2MasteringTerm() {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2MasteringTerm");
    return Aps5NativeCall_nid_no_patch(functions[17], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2PortCreate(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2PortCreate");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2PortDestroy(std::uint64_t a0) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2PortDestroy");
    return Aps5NativeCall_nid_no_patch(functions[19], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2PortGetState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2PortGetState");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2PortSetAttributes(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2PortSetAttributes");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2Set3DLatency(std::uint64_t a0, std::uint64_t a1) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2Set3DLatency");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2SetSystemDebugState(std::uint64_t a0) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2SetSystemDebugState");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2SpeakerArrayCreate(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2SpeakerArrayCreate");
    return Aps5NativeCall_nid_no_patch(functions[24], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2SpeakerArrayDestroy(std::uint64_t a0) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2SpeakerArrayDestroy");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2UserCreate(std::uint64_t a0, std::uint64_t a1) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2UserCreate");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2UserDestroy(std::uint64_t a0) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2UserDestroy");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOut2UserGetSupportedAttributes() {
    if (functions[28] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOut2UserGetSupportedAttributes");
    return Aps5NativeCall_nid_no_patch(functions[28], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutClose(std::uint64_t a0) {
    if (functions[29] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutClose");
    return Aps5NativeCall_nid_no_patch(functions[29], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutExClose() {
    if (functions[30] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutExClose");
    return Aps5NativeCall_nid_no_patch(functions[30], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutExConfigureOutput() {
    if (functions[31] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutExConfigureOutput");
    return Aps5NativeCall_nid_no_patch(functions[31], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutExGetMonitorInfo() {
    if (functions[32] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutExGetMonitorInfo");
    return Aps5NativeCall_nid_no_patch(functions[32], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutExOpen() {
    if (functions[33] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutExOpen");
    return Aps5NativeCall_nid_no_patch(functions[33], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutGetLastOutputTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[34] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutGetLastOutputTime");
    return Aps5NativeCall_nid_no_patch(functions[34], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutGetPortState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[35] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutGetPortState");
    return Aps5NativeCall_nid_no_patch(functions[35], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutInit() {
    if (functions[36] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutInit");
    return Aps5NativeCall_nid_no_patch(functions[36], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[37] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutOpen");
    return Aps5NativeCall_nid_no_patch(functions[37], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" std::uint64_t sceAudioOutOutput(std::uint64_t a0, std::uint64_t a1) {
    if (functions[38] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutOutput");
    return Aps5NativeCall_nid_no_patch(functions[38], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutOutputs(std::uint64_t a0, std::uint64_t a1) {
    if (functions[39] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutOutputs");
    return Aps5NativeCall_nid_no_patch(functions[39], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSetMixLevelPadSpk(std::uint64_t a0, std::uint64_t a1) {
    if (functions[40] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSetMixLevelPadSpk");
    return Aps5NativeCall_nid_no_patch(functions[40], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSetVolume(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[41] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSetVolume");
    return Aps5NativeCall_nid_no_patch(functions[41], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSysClose() {
    if (functions[42] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSysClose");
    return Aps5NativeCall_nid_no_patch(functions[42], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSysConfigureOutput() {
    if (functions[43] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSysConfigureOutput");
    return Aps5NativeCall_nid_no_patch(functions[43], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSysGetHdmiMonitorInfo() {
    if (functions[44] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSysGetHdmiMonitorInfo");
    return Aps5NativeCall_nid_no_patch(functions[44], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioOutSysOpen() {
    if (functions[45] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioOutSysOpen");
    return Aps5NativeCall_nid_no_patch(functions[45], 0, 0, 0, 0, 0, 0, 0, 0);
}
