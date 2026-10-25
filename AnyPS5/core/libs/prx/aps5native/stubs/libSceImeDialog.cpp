// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceImeDialog.prx, переадресующая вызовы
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
    "sceImeDialogAbort",
    "sceImeDialogGetPanelPositionAndForm",
    "sceImeDialogGetPanelSize",
    "sceImeDialogGetPanelSizeExtended",
    "sceImeDialogGetResult",
    "sceImeDialogGetStatus",
    "sceImeDialogInit",
    "sceImeDialogTerm",
};
void* functions[8] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceImeDialog.prx", names, functions, 8);
}

} // namespace

extern "C" std::uint64_t sceImeDialogAbort() {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogAbort");
    return Aps5NativeCall_nid_no_patch(functions[0], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogGetPanelPositionAndForm(std::uint64_t a0) {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogGetPanelPositionAndForm");
    return Aps5NativeCall_nid_no_patch(functions[1], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogGetPanelSize(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogGetPanelSize");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, a1, a2, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogGetPanelSizeExtended(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogGetPanelSizeExtended");
    return Aps5NativeCall_nid_no_patch(functions[3], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogGetResult(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogGetResult");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogGetStatus() {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogGetStatus");
    return Aps5NativeCall_nid_no_patch(functions[5], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogInit(std::uint64_t a0, std::uint64_t a1) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogInit");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceImeDialogTerm() {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceImeDialogTerm");
    return Aps5NativeCall_nid_no_patch(functions[7], 0, 0, 0, 0, 0, 0, 0, 0);
}
