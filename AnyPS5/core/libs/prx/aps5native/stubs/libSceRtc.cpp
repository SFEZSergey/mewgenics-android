// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceRtc.prx, переадресующая вызовы
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
    "sceRtcCheckValid",
    "sceRtcConvertLocalTimeToUtc",
    "sceRtcConvertUtcToLocalTime",
    "sceRtcFormatRFC2822",
    "sceRtcFormatRFC2822LocalTime",
    "sceRtcFormatRFC3339",
    "sceRtcFormatRFC3339LocalTime",
    "sceRtcGetCurrentClock",
    "sceRtcGetCurrentClockLocalTime",
    "sceRtcGetCurrentNetworkTick",
    "sceRtcGetCurrentTick",
    "sceRtcGetDayOfWeek",
    "sceRtcGetDaysInMonth",
    "sceRtcGetDosTime",
    "sceRtcGetTick",
    "sceRtcGetTickResolution",
    "sceRtcGetTime_t",
    "sceRtcGetWin32FileTime",
    "sceRtcIsLeapYear",
    "sceRtcParseDateTime",
    "sceRtcParseRFC3339",
    "sceRtcSetDosTime",
    "sceRtcSetTick",
    "sceRtcSetTime_t",
    "sceRtcSetWin32FileTime",
    "sceRtcTickAddDays",
    "sceRtcTickAddHours",
    "sceRtcTickAddMicroseconds",
    "sceRtcTickAddMinutes",
    "sceRtcTickAddMonths",
    "sceRtcTickAddSeconds",
    "sceRtcTickAddTicks",
    "sceRtcTickAddWeeks",
    "sceRtcTickAddYears",
};
void* functions[34] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceRtc.prx", names, functions, 34);
}

} // namespace

extern "C" std::uint64_t sceRtcCheckValid(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcCheckValid");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcConvertLocalTimeToUtc(std::uint64_t a0, std::uint64_t a1) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcConvertLocalTimeToUtc");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcConvertUtcToLocalTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcConvertUtcToLocalTime");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcFormatRFC2822(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcFormatRFC2822");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcFormatRFC2822LocalTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcFormatRFC2822LocalTime");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcFormatRFC3339(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcFormatRFC3339");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcFormatRFC3339LocalTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcFormatRFC3339LocalTime");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetCurrentClock(std::uint64_t a0, std::uint64_t a1) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetCurrentClock");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetCurrentClockLocalTime(std::uint64_t a0) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetCurrentClockLocalTime");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetCurrentNetworkTick(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetCurrentNetworkTick");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetCurrentTick(std::uint64_t a0) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetCurrentTick");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetDayOfWeek(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetDayOfWeek");
    return Aps5NativeCall_nid_no_patch(functions[11], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetDaysInMonth(std::uint64_t a0, std::uint64_t a1) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetDaysInMonth");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetDosTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetDosTime");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetTick(std::uint64_t a0, std::uint64_t a1) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetTick");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetTickResolution() {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetTickResolution");
    return Aps5NativeCall_nid_no_patch(functions[15], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetTime_t(std::uint64_t a0, std::uint64_t a1) {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetTime_t");
    return Aps5NativeCall_nid_no_patch(functions[16], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcGetWin32FileTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcGetWin32FileTime");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcIsLeapYear(std::uint64_t a0) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcIsLeapYear");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcParseDateTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcParseDateTime");
    return Aps5NativeCall_nid_no_patch(functions[19], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcParseRFC3339(std::uint64_t a0, std::uint64_t a1) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcParseRFC3339");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcSetDosTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcSetDosTime");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcSetTick(std::uint64_t a0, std::uint64_t a1) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcSetTick");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcSetTime_t(std::uint64_t a0, std::uint64_t a1) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcSetTime_t");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcSetWin32FileTime(std::uint64_t a0, std::uint64_t a1) {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcSetWin32FileTime");
    return Aps5NativeCall_nid_no_patch(functions[24], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddDays(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddDays");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddHours(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddHours");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddMicroseconds(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddMicroseconds");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddMinutes(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[28] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddMinutes");
    return Aps5NativeCall_nid_no_patch(functions[28], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddMonths(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[29] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddMonths");
    return Aps5NativeCall_nid_no_patch(functions[29], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddSeconds(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[30] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddSeconds");
    return Aps5NativeCall_nid_no_patch(functions[30], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddTicks(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[31] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddTicks");
    return Aps5NativeCall_nid_no_patch(functions[31], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddWeeks(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[32] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddWeeks");
    return Aps5NativeCall_nid_no_patch(functions[32], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceRtcTickAddYears(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[33] == nullptr) Aps5NativeMissing_nid_no_patch("sceRtcTickAddYears");
    return Aps5NativeCall_nid_no_patch(functions[33], a0, a1, a2, 0, 0, 0, 0, 0);
}
