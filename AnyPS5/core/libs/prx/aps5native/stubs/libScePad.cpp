// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libScePad.prx, переадресующая вызовы
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
    "PadFetchOutput_nid_postfix",
    "PadPublishInput_nid_postfix",
    "scePadClose_nid_postfix",
    "scePadDeviceClassGetExtendedInformation",
    "scePadDeviceClassParseData",
    "scePadGetControllerInformation",
    "scePadGetExtControllerInformation",
    "scePadGetFeatureReport",
    "scePadGetHandle",
    "scePadGetInfo_nid_postfix",
    "scePadGetTriggerEffectState",
    "scePadInit_nid_postfix",
    "scePadIsRemoteController",
    "scePadOpenExt",
    "scePadOpen_nid_postfix",
    "scePadOutputReport",
    "scePadReadExt",
    "scePadReadState",
    "scePadRead_nid_postfix",
    "scePadResetLightBar",
    "scePadResetOrientation",
    "scePadSetAngularVelocityBiasCorrectionState",
    "scePadSetAngularVelocityDeadbandState",
    "scePadSetFeatureReport",
    "scePadSetLightBar",
    "scePadSetMotionSensorState",
    "scePadSetParticularMode",
    "scePadSetProcessPrivilege",
    "scePadSetTiltCorrectionState",
    "scePadSetTriggerEffect",
    "scePadSetVibration",
    "scePadSetVibrationMode",
    "scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse",
    "scePadVrControllerGetDeviceInformation",
    "scePadVrControllerRead",
    "scePadVrControllerReadState",
};
void* functions[36] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libScePad.prx", names, functions, 36);
}

} // namespace

extern "C" std::uint64_t PadFetchOutput_nid_postfix(std::uint64_t a0, std::uint64_t a1) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("PadFetchOutput_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" void PadPublishInput_nid_postfix(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("PadPublishInput_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadClose_nid_postfix(std::uint64_t a0) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("scePadClose_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadDeviceClassGetExtendedInformation(std::uint64_t a0, std::uint64_t a1) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("scePadDeviceClassGetExtendedInformation");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadDeviceClassParseData(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("scePadDeviceClassParseData");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetControllerInformation(std::uint64_t a0, std::uint64_t a1) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetControllerInformation");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetExtControllerInformation(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetExtControllerInformation");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetFeatureReport() {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetFeatureReport");
    return Aps5NativeCall_nid_no_patch(functions[7], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetHandle(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetHandle");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetInfo_nid_postfix(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetInfo_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadGetTriggerEffectState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("scePadGetTriggerEffectState");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadInit_nid_postfix() {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("scePadInit_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[11], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadIsRemoteController(std::uint64_t a0, std::uint64_t a1) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("scePadIsRemoteController");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadOpenExt(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("scePadOpenExt");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadOpen_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("scePadOpen_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadOutputReport() {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("scePadOutputReport");
    return Aps5NativeCall_nid_no_patch(functions[15], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadReadExt() {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("scePadReadExt");
    return Aps5NativeCall_nid_no_patch(functions[16], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadReadState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("scePadReadState");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadRead_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("scePadRead_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadResetLightBar(std::uint64_t a0) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("scePadResetLightBar");
    return Aps5NativeCall_nid_no_patch(functions[19], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadResetOrientation(std::uint64_t a0) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("scePadResetOrientation");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetAngularVelocityBiasCorrectionState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetAngularVelocityBiasCorrectionState");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetAngularVelocityDeadbandState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetAngularVelocityDeadbandState");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetFeatureReport() {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetFeatureReport");
    return Aps5NativeCall_nid_no_patch(functions[23], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetLightBar(std::uint64_t a0, std::uint64_t a1) {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetLightBar");
    return Aps5NativeCall_nid_no_patch(functions[24], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetMotionSensorState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetMotionSensorState");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetParticularMode(std::uint64_t a0) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetParticularMode");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetProcessPrivilege(std::uint64_t a0) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetProcessPrivilege");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetTiltCorrectionState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[28] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetTiltCorrectionState");
    return Aps5NativeCall_nid_no_patch(functions[28], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetTriggerEffect(std::uint64_t a0, std::uint64_t a1) {
    if (functions[29] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetTriggerEffect");
    return Aps5NativeCall_nid_no_patch(functions[29], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetVibration(std::uint64_t a0, std::uint64_t a1) {
    if (functions[30] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetVibration");
    return Aps5NativeCall_nid_no_patch(functions[30], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetVibrationMode(std::uint64_t a0, std::uint64_t a1) {
    if (functions[31] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetVibrationMode");
    return Aps5NativeCall_nid_no_patch(functions[31], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(std::uint64_t a0) {
    if (functions[32] == nullptr) Aps5NativeMissing_nid_no_patch("scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse");
    return Aps5NativeCall_nid_no_patch(functions[32], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadVrControllerGetDeviceInformation() {
    if (functions[33] == nullptr) Aps5NativeMissing_nid_no_patch("scePadVrControllerGetDeviceInformation");
    return Aps5NativeCall_nid_no_patch(functions[33], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadVrControllerRead() {
    if (functions[34] == nullptr) Aps5NativeMissing_nid_no_patch("scePadVrControllerRead");
    return Aps5NativeCall_nid_no_patch(functions[34], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t scePadVrControllerReadState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[35] == nullptr) Aps5NativeMissing_nid_no_patch("scePadVrControllerReadState");
    return Aps5NativeCall_nid_no_patch(functions[35], a0, a1, 0, 0, 0, 0, 0, 0);
}
