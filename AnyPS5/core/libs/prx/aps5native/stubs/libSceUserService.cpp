// x86-заглушка libSceUserService.prx, переадресующая вызовы в нативную (aarch64) сборку через мост box64.
#include "prx/aps5native/X86Host.hpp"

#include <cstdint>
#include <cstring>

namespace {

const char* const names[] = {
    "sceUserServiceGetAccessibilityChatTranscription",
    "sceUserServiceGetAccessibilityPressAndHoldDelay",
    "sceUserServiceGetAccessibilityTriggerEffect",
    "sceUserServiceGetAccessibilityVibration",
    "sceUserServiceGetAccessibilityZoomEnabled",
    "sceUserServiceGetAccessibilityZoomFollowFocus",
    "sceUserServiceGetAgeLevel",
    "sceUserServiceGetEvent",
    "sceUserServiceGetForegroundUser",
    "sceUserServiceGetGamePresets",
    "sceUserServiceGetInitialUser",
    "sceUserServiceGetLoginUserIdList",
    "sceUserServiceGetNpAccountId",
    "sceUserServiceGetPlatformPrivacyWs1",
    "sceUserServiceGetRegisteredUserIdList",
    "sceUserServiceGetUserColor",
    "sceUserServiceGetUserName",
    "sceUserServiceGetUserNumber",
    "sceUserServiceInitialize",
    "sceUserServiceInitialize2",
    "sceUserServiceTerminate",
};
void* functions[21] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceUserService.prx", names, functions, 21);
}

} // namespace

extern "C" std::uint64_t sceUserServiceGetAccessibilityChatTranscription(std::uint64_t a0, std::uint64_t a1) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityChatTranscription");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAccessibilityPressAndHoldDelay(std::uint64_t a0, std::uint64_t a1) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityPressAndHoldDelay");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAccessibilityTriggerEffect(std::uint64_t a0, std::uint64_t a1) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityTriggerEffect");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAccessibilityVibration(std::uint64_t a0, std::uint64_t a1) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityVibration");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAccessibilityZoomEnabled(std::uint64_t a0, std::uint64_t a1) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityZoomEnabled");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAccessibilityZoomFollowFocus(std::uint64_t a0, std::uint64_t a1) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAccessibilityZoomFollowFocus");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetAgeLevel(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetAgeLevel");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetEvent(std::uint64_t a0) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetEvent");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetForegroundUser(std::uint64_t a0) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetForegroundUser");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetGamePresets(std::uint64_t a0, std::uint64_t a1) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetGamePresets");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetInitialUser(std::uint64_t a0) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetInitialUser");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetLoginUserIdList(std::uint64_t a0) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetLoginUserIdList");
    return Aps5NativeCall_nid_no_patch(functions[11], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetNpAccountId(std::uint64_t a0, std::uint64_t a1) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetNpAccountId");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetPlatformPrivacyWs1(std::uint64_t a0, std::uint64_t a1) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetPlatformPrivacyWs1");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetRegisteredUserIdList(std::uint64_t a0) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetRegisteredUserIdList");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetUserColor(std::uint64_t a0, std::uint64_t a1) {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetUserColor");
    return Aps5NativeCall_nid_no_patch(functions[15], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetUserName(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetUserName");
    return Aps5NativeCall_nid_no_patch(functions[16], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceGetUserNumber(std::uint64_t a0, std::uint64_t a1) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceGetUserNumber");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceInitialize(std::uint64_t a0) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceInitialize");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceInitialize2() {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceInitialize2");
    return Aps5NativeCall_nid_no_patch(functions[19], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceUserServiceTerminate() {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceUserServiceTerminate");
    return Aps5NativeCall_nid_no_patch(functions[20], 0, 0, 0, 0, 0, 0, 0, 0);
}
