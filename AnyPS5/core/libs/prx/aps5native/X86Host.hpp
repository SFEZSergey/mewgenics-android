#ifndef CORE_LIBS_PRX_APS5NATIVE_X86HOST_HPP
#define CORE_LIBS_PRX_APS5NATIVE_X86HOST_HPP

// API x86-заглушек нативной группы (реализация в X86Host.cpp, внутри заглушки libSceAgcDriver.prx).
// Суффикс _nid_no_patch сохраняет имена при NID-патчинге.

#include <cstddef>
#include <cstdint>

extern "C" {
// Загружает нативную сборку библиотеки и находит её функции (отсутствующие — nullptr).
void Aps5NativeResolve_nid_no_patch(const char* library, const char* const* names, void** functions, std::size_t count);
// Вызов нативной функции: до 8 целых аргументов (x0..x7).
std::uint64_t Aps5NativeCall_nid_no_patch(void* function, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6, std::uint64_t a7);
// Вызов с вещественными аргументами: 6 целых и 4 вещественных (float — в младших 32 битах).
std::uint64_t Aps5NativeCallFd_nid_no_patch(void* function, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double d0, double d1, double d2, double d3);
[[noreturn]] void Aps5NativeMissing_nid_no_patch(const char* name);
}

#endif
