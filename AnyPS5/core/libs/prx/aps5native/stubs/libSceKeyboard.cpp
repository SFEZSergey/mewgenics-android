// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceKeyboard.prx, переадресующая вызовы
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
    "KeyboardPublishInput_nid_postfix",
    "sceKeyboardClose",
    "sceKeyboardGetKey2Char",
    "sceKeyboardInit",
    "sceKeyboardOpen",
    "sceKeyboardRead",
    "sceKeyboardReadState",
};
void* functions[7] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceKeyboard.prx", names, functions, 7);
}

} // namespace

extern "C" void KeyboardPublishInput_nid_postfix(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("KeyboardPublishInput_nid_postfix");
    Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceKeyboardClose(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardClose");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceKeyboardGetKey2Char(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardGetKey2Char");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, a1, a2, a3, a4, a5, 0, 0);
}

extern "C" std::uint64_t sceKeyboardInit() {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardInit");
    return Aps5NativeCall_nid_no_patch(functions[3], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceKeyboardOpen(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardOpen");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceKeyboardRead(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardRead");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceKeyboardReadState(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceKeyboardReadState");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}
