// x86-заглушка libSceSystemService.prx, переадресующая вызовы в нативную (aarch64) сборку через мост box64.
#include "prx/aps5native/X86Host.hpp"

#include <cstdint>
#include <cstring>

namespace {

const char* const names[] = {
    "sceSystemServiceDisableMediaPlay",
    "sceSystemServiceDisableMusicPlayer",
    "sceSystemServiceDisableNoticeScreenSkipFlagAutoSet",
    "sceSystemServiceGetAppIdOfRunningBigApp",
    "sceSystemServiceGetDisplaySafeAreaInfo",
    "sceSystemServiceGetHdrToneMapLuminance",
    "sceSystemServiceGetNoticeScreenSkipFlag",
    "sceSystemServiceGetStatus",
    "sceSystemServiceHideSplashScreen",
    "sceSystemServiceInitializePlayerDialogParam",
    "sceSystemServiceKillApp",
    "sceSystemServiceLaunchApp",
    "sceSystemServiceLaunchPlayerDialog",
    "sceSystemServiceLaunchWebBrowser",
    "sceSystemServiceLoadExec",
    "sceSystemServiceOpenChallengeActivity",
    "sceSystemServiceOpenTournamentOccurrence",
    "sceSystemServiceParamGetInt",
    "sceSystemServiceParamGetString",
    "sceSystemServicePowerTick",
    "sceSystemServiceReceiveEvent",
    "sceSystemServiceReenableMediaPlay",
    "sceSystemServiceReenableMusicPlayer",
    "sceSystemServiceReportAbnormalTermination",
    "sceSystemServiceSetNoticeScreenSkipFlag",
    "sceSystemServiceShowControllerSettings",
};
void* functions[26] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceSystemService.prx", names, functions, 26);
}

} // namespace

extern "C" std::uint64_t sceSystemServiceDisableMediaPlay() {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceDisableMediaPlay");
    return Aps5NativeCall_nid_no_patch(functions[0], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceDisableMusicPlayer() {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceDisableMusicPlayer");
    return Aps5NativeCall_nid_no_patch(functions[1], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceDisableNoticeScreenSkipFlagAutoSet() {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceDisableNoticeScreenSkipFlagAutoSet");
    return Aps5NativeCall_nid_no_patch(functions[2], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceGetAppIdOfRunningBigApp() {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceGetAppIdOfRunningBigApp");
    return Aps5NativeCall_nid_no_patch(functions[3], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceGetDisplaySafeAreaInfo(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceGetDisplaySafeAreaInfo");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceGetHdrToneMapLuminance(std::uint64_t a0) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceGetHdrToneMapLuminance");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceGetNoticeScreenSkipFlag(std::uint64_t a0) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceGetNoticeScreenSkipFlag");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceGetStatus(std::uint64_t a0) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceGetStatus");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceHideSplashScreen() {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceHideSplashScreen");
    return Aps5NativeCall_nid_no_patch(functions[8], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceInitializePlayerDialogParam(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceInitializePlayerDialogParam");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceKillApp(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceKillApp");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceLaunchApp(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceLaunchApp");
    return Aps5NativeCall_nid_no_patch(functions[11], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceLaunchPlayerDialog(std::uint64_t a0) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceLaunchPlayerDialog");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceLaunchWebBrowser(std::uint64_t a0, std::uint64_t a1) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceLaunchWebBrowser");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceLoadExec(std::uint64_t a0, std::uint64_t a1) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceLoadExec");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceOpenChallengeActivity() {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceOpenChallengeActivity");
    return Aps5NativeCall_nid_no_patch(functions[15], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceOpenTournamentOccurrence() {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceOpenTournamentOccurrence");
    return Aps5NativeCall_nid_no_patch(functions[16], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceParamGetInt(std::uint64_t a0, std::uint64_t a1) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceParamGetInt");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceParamGetString(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceParamGetString");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServicePowerTick() {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServicePowerTick");
    return Aps5NativeCall_nid_no_patch(functions[19], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceReceiveEvent(std::uint64_t a0) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceReceiveEvent");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceReenableMediaPlay() {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceReenableMediaPlay");
    return Aps5NativeCall_nid_no_patch(functions[21], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceReenableMusicPlayer() {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceReenableMusicPlayer");
    return Aps5NativeCall_nid_no_patch(functions[22], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceReportAbnormalTermination(std::uint64_t a0) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceReportAbnormalTermination");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceSetNoticeScreenSkipFlag() {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceSetNoticeScreenSkipFlag");
    return Aps5NativeCall_nid_no_patch(functions[24], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSystemServiceShowControllerSettings() {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceSystemServiceShowControllerSettings");
    return Aps5NativeCall_nid_no_patch(functions[25], 0, 0, 0, 0, 0, 0, 0, 0);
}
