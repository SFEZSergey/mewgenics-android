// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceWebBrowserDialog.prx, переадресующая вызовы
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
    "sceWebBrowserDialogClose",
    "sceWebBrowserDialogGetResult",
    "sceWebBrowserDialogGetStatus",
    "sceWebBrowserDialogInitialize",
    "sceWebBrowserDialogOpen",
    "sceWebBrowserDialogOpenForPredeterminedContent",
    "sceWebBrowserDialogResetCookie",
    "sceWebBrowserDialogSetCookie",
    "sceWebBrowserDialogTerminate",
    "sceWebBrowserDialogUpdateStatus",
};
void* functions[10] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceWebBrowserDialog.prx", names, functions, 10);
}

} // namespace

extern "C" std::uint64_t sceWebBrowserDialogClose() {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogClose");
    return Aps5NativeCall_nid_no_patch(functions[0], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogGetResult(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogGetResult");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogGetStatus() {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogGetStatus");
    return Aps5NativeCall_nid_no_patch(functions[2], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogInitialize() {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogInitialize");
    return Aps5NativeCall_nid_no_patch(functions[3], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogOpen(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogOpen");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogOpenForPredeterminedContent() {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogOpenForPredeterminedContent");
    return Aps5NativeCall_nid_no_patch(functions[5], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogResetCookie() {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogResetCookie");
    return Aps5NativeCall_nid_no_patch(functions[6], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogSetCookie() {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogSetCookie");
    return Aps5NativeCall_nid_no_patch(functions[7], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogTerminate() {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogTerminate");
    return Aps5NativeCall_nid_no_patch(functions[8], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceWebBrowserDialogUpdateStatus() {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceWebBrowserDialogUpdateStatus");
    return Aps5NativeCall_nid_no_patch(functions[9], 0, 0, 0, 0, 0, 0, 0, 0);
}
