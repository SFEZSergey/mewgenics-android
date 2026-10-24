// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceMouse.prx, переадресующая вызовы
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
    "MousePublishInput_nid_postfix",
    "sceMouseClose",
    "sceMouseInit",
    "sceMouseOpen",
    "sceMouseRead",
    "sceMouseReadAbsolutePosition",
    "sceMouseResetAbsolutePosition",
    "sceMouseSetAbsoluteHighPrecisionMode",
    "sceMouseSetAbsoluteVirtualScreenSize",
};
void* functions[9] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceMouse.prx", names, functions, 9);
}

} // namespace

extern "C" void MousePublishInput_nid_postfix(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("MousePublishInput_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseClose(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseClose");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseInit() {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseInit");
    return Aps5NativeCall_nid_no_patch(functions[2], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseOpen");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseRead(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseRead");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseReadAbsolutePosition(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseReadAbsolutePosition");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseResetAbsolutePosition(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseResetAbsolutePosition");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseSetAbsoluteHighPrecisionMode(std::uint64_t a0, std::uint64_t a1) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseSetAbsoluteHighPrecisionMode");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceMouseSetAbsoluteVirtualScreenSize(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceMouseSetAbsoluteVirtualScreenSize");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, a1, a2, 0, 0, 0, 0, 0);
}
