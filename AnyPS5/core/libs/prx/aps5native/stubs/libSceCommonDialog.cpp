// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceCommonDialog.prx, переадресующая вызовы
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
    "sceCommonDialogInitialize",
    "sceCommonDialogIsUsed",
};
void* functions[2] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceCommonDialog.prx", names, functions, 2);
}

} // namespace

extern "C" std::uint64_t sceCommonDialogInitialize() {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceCommonDialogInitialize");
    return Aps5NativeCall_nid_no_patch(functions[0], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceCommonDialogIsUsed() {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceCommonDialogIsUsed");
    return Aps5NativeCall_nid_no_patch(functions[1], 0, 0, 0, 0, 0, 0, 0, 0);
}
