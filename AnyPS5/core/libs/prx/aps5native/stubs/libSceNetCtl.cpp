// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceNetCtl.prx, переадресующая вызовы
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
    "sceNetCtlCheckCallback",
    "sceNetCtlGetInfo",
    "sceNetCtlGetNatInfo",
    "sceNetCtlGetResult",
    "sceNetCtlGetState",
    "sceNetCtlGetStateV6",
    "sceNetCtlInit",
    "sceNetCtlRegisterCallback",
    "sceNetCtlTerm",
    "sceNetCtlUnregisterCallback",
};
void* functions[10] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceNetCtl.prx", names, functions, 10);
}

} // namespace

extern "C" std::uint64_t sceNetCtlCheckCallback() {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlCheckCallback");
    return Aps5NativeCall_nid_no_patch(functions[0], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlGetInfo(std::uint64_t a0, std::uint64_t a1) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlGetInfo");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlGetNatInfo(std::uint64_t a0) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlGetNatInfo");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlGetResult(std::uint64_t a0, std::uint64_t a1) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlGetResult");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlGetState(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlGetState");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlGetStateV6(std::uint64_t a0) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlGetStateV6");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlInit() {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlInit");
    return Aps5NativeCall_nid_no_patch(functions[6], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlRegisterCallback(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlRegisterCallback");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" void sceNetCtlTerm() {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlTerm");
    Aps5NativeCall_nid_no_patch(functions[8], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceNetCtlUnregisterCallback(std::uint64_t a0) {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceNetCtlUnregisterCallback");
    return Aps5NativeCall_nid_no_patch(functions[9], a0, 0, 0, 0, 0, 0, 0, 0);
}
