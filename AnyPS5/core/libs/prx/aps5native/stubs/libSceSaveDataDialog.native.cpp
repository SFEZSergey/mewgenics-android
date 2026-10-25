// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceSaveDataDialog.native.prx, переадресующая вызовы
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
    "sceSaveDataDialogClose",
    "sceSaveDataDialogGetResult",
    "sceSaveDataDialogGetStatus",
    "sceSaveDataDialogInitialize",
    "sceSaveDataDialogIsReadyToDisplay",
    "sceSaveDataDialogOpen",
    "sceSaveDataDialogProgressBarInc",
    "sceSaveDataDialogProgressBarSetValue",
    "sceSaveDataDialogTerminate",
    "sceSaveDataDialogUpdateStatus",
};
void* functions[10] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceSaveDataDialog.native.prx", names, functions, 10);
}

} // namespace

extern "C" std::uint64_t sceSaveDataDialogClose(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogClose");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogGetResult(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogGetResult");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogGetStatus() {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogGetStatus");
    return Aps5NativeCall_nid_no_patch(functions[2], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogInitialize() {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogInitialize");
    return Aps5NativeCall_nid_no_patch(functions[3], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogIsReadyToDisplay() {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogIsReadyToDisplay");
    return Aps5NativeCall_nid_no_patch(functions[4], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogOpen(std::uint64_t a0) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogOpen");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogProgressBarInc(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogProgressBarInc");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogProgressBarSetValue(std::uint64_t a0, std::uint64_t a1) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogProgressBarSetValue");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogTerminate() {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogTerminate");
    return Aps5NativeCall_nid_no_patch(functions[8], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDialogUpdateStatus() {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDialogUpdateStatus");
    return Aps5NativeCall_nid_no_patch(functions[9], 0, 0, 0, 0, 0, 0, 0, 0);
}
