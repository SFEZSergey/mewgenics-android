// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceAudioIn.prx, переадресующая вызовы
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
    "sceAudioInAsyncOpen",
    "sceAudioInClose",
    "sceAudioInGetSilentState",
    "sceAudioInHqOpen",
    "sceAudioInInit",
    "sceAudioInInput",
    "sceAudioInOpen",
    "sceAudioInUnknown_X4jdIS75P0",
};
void* functions[8] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceAudioIn.prx", names, functions, 8);
}

} // namespace

extern "C" std::uint64_t sceAudioInAsyncOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInAsyncOpen");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" std::uint64_t sceAudioInClose(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInClose");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioInGetSilentState(std::uint64_t a0) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInGetSilentState");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioInHqOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInHqOpen");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" std::uint64_t sceAudioInInit() {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInInit");
    return Aps5NativeCall_nid_no_patch(functions[4], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioInInput(std::uint64_t a0, std::uint64_t a1) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInInput");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceAudioInOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInOpen");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" std::uint64_t sceAudioInUnknown_X4jdIS75P0() {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceAudioInUnknown_X4jdIS75P0");
    return Aps5NativeCall_nid_no_patch(functions[7], 0, 0, 0, 0, 0, 0, 0, 0);
}
