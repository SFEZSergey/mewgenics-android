// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceRandom.prx, переадресующая вызовы
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
    "sceRandomGetRandomNumber",
};
void* functions[1] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceRandom.prx", names, functions, 1);
}

} // namespace

extern "C" std::uint64_t sceRandomGetRandomNumber(std::uint64_t a0, std::uint64_t a1) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceRandomGetRandomNumber");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, a1, 0, 0, 0, 0, 0, 0);
}
