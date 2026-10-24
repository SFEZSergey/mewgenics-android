// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceAgcDriver.prx, переадресующая вызовы
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
    "AgcDriverAttachWindow_nid_postfix",
    "AgcDriverBeginShaderPreparation_nid_postfix",
    "AgcDriverCheckGuestMemory_nid_postfix",
    "AgcDriverCommitShaderPreparation_nid_postfix",
    "AgcDriverDisplayBufferSize_nid_postfix",
    "AgcDriverEndShaderPreparation_nid_postfix",
    "AgcDriverLockVulkanLoader_nid_postfix",
    "AgcDriverPresentBuffer_nid_postfix",
    "AgcDriverPresentClear_nid_postfix",
    "AgcDriverRaiseWorkerThreadPriority_nid_postfix",
    "AgcDriverRegisterShader_nid_postfix",
    "AgcDriverRegisterVideoOutput_nid_postfix",
    "AgcDriverReleaseWindow_nid_postfix",
    "AgcDriverResolveGraphicsAbi_nid_postfix",
    "AgcDriverResolveGraphicsStagesAbi_nid_postfix",
    "AgcDriverResolveShaderAbi_nid_postfix",
    "AgcDriverShutdown_nid_postfix",
    "AgcDriverSuspendPoint_nid_postfix",
    "AgcDriverUnlockVulkanLoader_nid_postfix",
    "AgcDriverUnregisterVideoOutput_nid_postfix",
    "AgcDriverWaitIdle_nid_postfix",
    "sceAgcDriverAddEqEvent",
    "sceAgcDriverAgrSubmitDcb",
    "sceAgcDriverAgrSubmitMultiDcbs",
    "sceAgcDriverDeleteEqEvent",
    "sceAgcDriverFindResourcesPublic",
    "sceAgcDriverGetDefaultOwner",
    "sceAgcDriverGetEqContextId",
    "sceAgcDriverGetEqEventType",
    "sceAgcDriverGetOwnerName",
    "sceAgcDriverGetResourceBaseAddressAndSizeInBytes",
    "sceAgcDriverGetResourceName",
    "sceAgcDriverGetResourceRegistrationMaxNameLength",
    "sceAgcDriverGetResourceShaderGuid",
    "sceAgcDriverGetResourceType",
    "sceAgcDriverGetResourceUserData",
    "sceAgcDriverGetShaderDebuggingStatus",
    "sceAgcDriverGetSubmitValidationConfig",
    "sceAgcDriverGetSubmitValidationMode",
    "sceAgcDriverGetTFRing",
    "sceAgcDriverGetWaitRenderingPacketSizeInDwords",
    "sceAgcDriverInitResourceRegistration",
    "sceAgcDriverIsCaptureInProgress",
    "sceAgcDriverIsSubmitValidationEnabled",
    "sceAgcDriverIsTraceInProgress",
    "sceAgcDriverQueryResourceRegistrationUserMemoryRequirements",
    "sceAgcDriverRegisterDefaultOwner",
    "sceAgcDriverRegisterGdsResource",
    "sceAgcDriverRegisterMultipleResources",
    "sceAgcDriverRegisterOwner",
    "sceAgcDriverRegisterResource",
    "sceAgcDriverRegisterWorkloadStream",
    "sceAgcDriverRequestCaptureStart",
    "sceAgcDriverRequestCaptureStop",
    "sceAgcDriverSetHsOffchipParam",
    "sceAgcDriverSetResourceUserData",
    "sceAgcDriverSetSubmitValidationConfig",
    "sceAgcDriverSetSubmitValidationMode",
    "sceAgcDriverSetTFRing",
    "sceAgcDriverSetValidationErrorOutputFrequency",
    "sceAgcDriverSubmitAcb",
    "sceAgcDriverSubmitCommandBuffer",
    "sceAgcDriverSubmitDcb",
    "sceAgcDriverSubmitMultiAcbs",
    "sceAgcDriverSubmitMultiCommandBuffers",
    "sceAgcDriverSubmitMultiDcbs",
    "sceAgcDriverTriggerCapture",
    "sceAgcDriverUnregisterAllResourcesForOwner",
    "sceAgcDriverUnregisterOwnerAndResources",
    "sceAgcDriverUnregisterResource",
    "sceAgcDriverUnregisterWorkloadStream",
    "sceAgcDriverWaitUntilSafeForRendering",
};
void* functions[72] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceAgcDriver.prx", names, functions, 72);
}

} // namespace

extern "C" void AgcDriverAttachWindow_nid_postfix(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverAttachWindow_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t AgcDriverBeginShaderPreparation_nid_postfix() {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverBeginShaderPreparation_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[1], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverCheckGuestMemory_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[2], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" void AgcDriverCommitShaderPreparation_nid_postfix(std::uint64_t a0) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverCommitShaderPreparation_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[3], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t AgcDriverDisplayBufferSize_nid_postfix(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverDisplayBufferSize_nid_postfix");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverEndShaderPreparation_nid_postfix(std::uint64_t a0) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverEndShaderPreparation_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[5], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverLockVulkanLoader_nid_postfix() {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverLockVulkanLoader_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[6], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverPresentBuffer_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[7], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" void AgcDriverPresentClear_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverPresentClear_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[8], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" void AgcDriverRaiseWorkerThreadPriority_nid_postfix(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverRaiseWorkerThreadPriority_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverRegisterShader_nid_postfix(std::uint64_t a0) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverRegisterShader_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[10], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint64_t a0, std::uint64_t a1) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverRegisterVideoOutput_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[11], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(std::uint64_t a0) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverReleaseWindow_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[12], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverResolveGraphicsAbi_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[13], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverResolveGraphicsStagesAbi_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[14], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4) {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverResolveShaderAbi_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[15], a0, a1, a2, a3, a4, 0, 0, 0);
}

extern "C" void AgcDriverShutdown_nid_postfix() {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverShutdown_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[16], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverSuspendPoint_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[17], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix() {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverUnlockVulkanLoader_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[18], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint64_t a0, std::uint64_t a1) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverUnregisterVideoOutput_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[19], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" void AgcDriverWaitIdle_nid_postfix() {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("AgcDriverWaitIdle_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[20], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverAddEqEvent(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverAddEqEvent");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverAgrSubmitDcb(std::uint64_t a0) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverAgrSubmitDcb");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverAgrSubmitMultiDcbs(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverAgrSubmitMultiDcbs");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverDeleteEqEvent(std::uint64_t a0, std::uint64_t a1) {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverDeleteEqEvent");
    return Aps5NativeCall_nid_no_patch(functions[24], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverFindResourcesPublic(std::uint64_t a0, std::uint64_t a1) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverFindResourcesPublic");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetDefaultOwner(std::uint64_t a0) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetDefaultOwner");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetEqContextId(std::uint64_t a0) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetEqContextId");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetEqEventType(std::uint64_t a0) {
    if (functions[28] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetEqEventType");
    return Aps5NativeCall_nid_no_patch(functions[28], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetOwnerName(std::uint64_t a0, std::uint64_t a1) {
    if (functions[29] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetOwnerName");
    return Aps5NativeCall_nid_no_patch(functions[29], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceBaseAddressAndSizeInBytes(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[30] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceBaseAddressAndSizeInBytes");
    return Aps5NativeCall_nid_no_patch(functions[30], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceName(std::uint64_t a0, std::uint64_t a1) {
    if (functions[31] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceName");
    return Aps5NativeCall_nid_no_patch(functions[31], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceRegistrationMaxNameLength(std::uint64_t a0) {
    if (functions[32] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceRegistrationMaxNameLength");
    return Aps5NativeCall_nid_no_patch(functions[32], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceShaderGuid(std::uint64_t a0, std::uint64_t a1) {
    if (functions[33] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceShaderGuid");
    return Aps5NativeCall_nid_no_patch(functions[33], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceType(std::uint64_t a0, std::uint64_t a1) {
    if (functions[34] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceType");
    return Aps5NativeCall_nid_no_patch(functions[34], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetResourceUserData(std::uint64_t a0, std::uint64_t a1) {
    if (functions[35] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetResourceUserData");
    return Aps5NativeCall_nid_no_patch(functions[35], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetShaderDebuggingStatus() {
    if (functions[36] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetShaderDebuggingStatus");
    return Aps5NativeCall_nid_no_patch(functions[36], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetSubmitValidationConfig() {
    if (functions[37] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetSubmitValidationConfig");
    return Aps5NativeCall_nid_no_patch(functions[37], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetSubmitValidationMode() {
    if (functions[38] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetSubmitValidationMode");
    return Aps5NativeCall_nid_no_patch(functions[38], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetTFRing(std::uint64_t a0, std::uint64_t a1) {
    if (functions[39] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetTFRing");
    return Aps5NativeCall_nid_no_patch(functions[39], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverGetWaitRenderingPacketSizeInDwords() {
    if (functions[40] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverGetWaitRenderingPacketSizeInDwords");
    return Aps5NativeCall_nid_no_patch(functions[40], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverInitResourceRegistration() {
    if (functions[41] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverInitResourceRegistration");
    return Aps5NativeCall_nid_no_patch(functions[41], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverIsCaptureInProgress() {
    if (functions[42] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverIsCaptureInProgress");
    return Aps5NativeCall_nid_no_patch(functions[42], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverIsSubmitValidationEnabled() {
    if (functions[43] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverIsSubmitValidationEnabled");
    return Aps5NativeCall_nid_no_patch(functions[43], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverIsTraceInProgress() {
    if (functions[44] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverIsTraceInProgress");
    return Aps5NativeCall_nid_no_patch(functions[44], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(std::uint64_t a0) {
    if (functions[45] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverQueryResourceRegistrationUserMemoryRequirements");
    return Aps5NativeCall_nid_no_patch(functions[45], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterDefaultOwner(std::uint64_t a0) {
    if (functions[46] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterDefaultOwner");
    return Aps5NativeCall_nid_no_patch(functions[46], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterGdsResource(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6) {
    if (functions[47] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterGdsResource");
    return Aps5NativeCall_nid_no_patch(functions[47], a0, a1, a2, a3, a4, a5, a6, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterMultipleResources(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[48] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterMultipleResources");
    return Aps5NativeCall_nid_no_patch(functions[48], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterOwner(std::uint64_t a0, std::uint64_t a1) {
    if (functions[49] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterOwner");
    return Aps5NativeCall_nid_no_patch(functions[49], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterResource(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6) {
    if (functions[50] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterResource");
    return Aps5NativeCall_nid_no_patch(functions[50], a0, a1, a2, a3, a4, a5, a6, 0);
}

extern "C" std::uint64_t sceAgcDriverRegisterWorkloadStream(std::uint64_t a0, std::uint64_t a1) {
    if (functions[51] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRegisterWorkloadStream");
    return Aps5NativeCall_nid_no_patch(functions[51], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRequestCaptureStart(std::uint64_t a0) {
    if (functions[52] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRequestCaptureStart");
    return Aps5NativeCall_nid_no_patch(functions[52], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverRequestCaptureStop() {
    if (functions[53] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverRequestCaptureStop");
    return Aps5NativeCall_nid_no_patch(functions[53], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetHsOffchipParam(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[54] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetHsOffchipParam");
    return Aps5NativeCall_nid_no_patch(functions[54], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetResourceUserData(std::uint64_t a0, std::uint64_t a1) {
    if (functions[55] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetResourceUserData");
    return Aps5NativeCall_nid_no_patch(functions[55], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetSubmitValidationConfig() {
    if (functions[56] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetSubmitValidationConfig");
    return Aps5NativeCall_nid_no_patch(functions[56], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetSubmitValidationMode() {
    if (functions[57] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetSubmitValidationMode");
    return Aps5NativeCall_nid_no_patch(functions[57], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetTFRing(std::uint64_t a0, std::uint64_t a1) {
    if (functions[58] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetTFRing");
    return Aps5NativeCall_nid_no_patch(functions[58], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSetValidationErrorOutputFrequency() {
    if (functions[59] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSetValidationErrorOutputFrequency");
    return Aps5NativeCall_nid_no_patch(functions[59], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitAcb(std::uint64_t a0, std::uint64_t a1) {
    if (functions[60] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitAcb");
    return Aps5NativeCall_nid_no_patch(functions[60], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitCommandBuffer(std::uint64_t a0, std::uint64_t a1) {
    if (functions[61] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitCommandBuffer");
    return Aps5NativeCall_nid_no_patch(functions[61], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitDcb(std::uint64_t a0) {
    if (functions[62] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitDcb");
    return Aps5NativeCall_nid_no_patch(functions[62], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitMultiAcbs(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[63] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitMultiAcbs");
    return Aps5NativeCall_nid_no_patch(functions[63], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitMultiCommandBuffers(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[64] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitMultiCommandBuffers");
    return Aps5NativeCall_nid_no_patch(functions[64], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverSubmitMultiDcbs(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[65] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverSubmitMultiDcbs");
    return Aps5NativeCall_nid_no_patch(functions[65], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverTriggerCapture() {
    if (functions[66] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverTriggerCapture");
    return Aps5NativeCall_nid_no_patch(functions[66], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverUnregisterAllResourcesForOwner(std::uint64_t a0) {
    if (functions[67] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverUnregisterAllResourcesForOwner");
    return Aps5NativeCall_nid_no_patch(functions[67], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverUnregisterOwnerAndResources(std::uint64_t a0) {
    if (functions[68] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverUnregisterOwnerAndResources");
    return Aps5NativeCall_nid_no_patch(functions[68], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverUnregisterResource(std::uint64_t a0) {
    if (functions[69] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverUnregisterResource");
    return Aps5NativeCall_nid_no_patch(functions[69], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverUnregisterWorkloadStream(std::uint64_t a0) {
    if (functions[70] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverUnregisterWorkloadStream");
    return Aps5NativeCall_nid_no_patch(functions[70], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAgcDriverWaitUntilSafeForRendering(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4) {
    if (functions[71] == nullptr) Aps5NativeMissing_nid_no_patch("sceAgcDriverWaitUntilSafeForRendering");
    return Aps5NativeCall_nid_no_patch(functions[71], a0, a1, a2, a3, a4, 0, 0, 0);
}
