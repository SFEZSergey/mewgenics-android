#ifndef CORE_LIBS_PRX_APS5NATIVE_APS5NATIVE_HPP
#define CORE_LIBS_PRX_APS5NATIVE_APS5NATIVE_HPP

// Нативная группа AnyPS5 под box64: драйвер GPU и вывод собраны под aarch64 и загружаются
// x86-заглушками через мост box64 libaps5bridge.so. Ядро (libc/libkernel) остаётся x86: нативный
// shim (CoreShim.cpp) вызывает его функции по таблице x86-адресов, которую при загрузке передаёт
// x86-сторона (X86Host.cpp). Порядок элементов — общий контракт обеих сторон.

#include <cstdint>

namespace Aps5Native {

enum X86Function : std::uint32_t {
    EqueueAddEvent,              // int (eq, const KernelEqueueEvent*)
    EqueueDeleteEvent,           // int (eq, ident, filter)
    EqueueTriggerEvent,          // int (eq, ident, filter, data)
    EqueuePinAcquire,            // void* handle (eq): x86 KernelEqueueRef в куче, nullptr если нет
    EqueuePinRelease,            // void (handle)
    GuestArenaRange,             // void (base*, bytes*)
    GuestArenaSharedBacking,     // bool (address, bytes, file*, offset*)
    GuestArenaWriteSharedBacking,// bool (address, source, bytes)
    GuestAllocationsGeneration,  // u64 ()
    LeaseAcquire,                // void* lease (): x86 Lease в куче
    LeaseCount,                  // u64 (lease)
    LeaseAt,                     // const Range* (lease, index)
    LeaseRelease,                // void (lease)
    SetInvalidator,              // void (native fn)
    SetPinWaiter,                // void (native fn)
    SetGpuMapObserver,           // void (native dispatcher, native fn)
    RegisterShutdown,            // void (native fn)
    LibcAwaitExit,               // noreturn ()
    LibcRequestExit,             // void (code)
    LibcRequestShutdown,         // void ()
    NotImplemented,              // void (name)
    ProcessTime,                 // u64 ()
    ProcessTimeCounter,          // u64 ()
    ProcessTimeCounterFrequency, // u64 ()
    HasAppIcon,                  // bool ()
    AppTitle,                    // void (AppTitle* out)
    AppIconData,                 // void (AppIconData* out)
    Mmap,                        // void* (address, length, protection, flags, descriptor, offset)
    ShutdownRequested,           // bool (): токен остановки libc
    EqueueTrampoline,            // void* x86 fn (kind, native fn): x86-трамплин колбэка фильтра очереди
    LeasePinAll,                 // void (lease, const Range** ranges, void** pins): по закреплению на диапазон
    RangeUnpin,                  // void (pin): отпускает закрепление одного диапазона
    EqueuePushPending,           // void (x86 KernelEqueueEvent*, const KernelEvent*)
    RangeUnpinBatch,             // void (void** pins, u64 count): RangeUnpin пачкой (один переход в x86)
    ReadableEnd,                 // u64 (begin, end): конец читаемого диапазона, содержащего [begin, end), или 0
    GuestAllocationsGenerationCounter, // const atomic<u64>* (): счётчик поколения, натив читает его сам
    LeaseAcquireWindow,          // void* lease (begin, end): x86 Lease читаемых диапазонов, пересекающих [begin, end)
    X86FunctionCount
};

// Вызов x86-функции из нативного кода (box64 RunFunction): до 6 целых аргументов.
using X86Caller = std::uint64_t (*)(std::uintptr_t function, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

// Виды колбэков KernelFilter для EqueueTrampoline.
enum EqueueCallback : std::uint32_t { EqueueTrigger, EqueueReset, EqueueDelete, EqueuePoll };

// Нативный диспетчер наблюдателя GPU-отображений: x86 передаёт снимок (Lease) уже захваченных
// диапазонов, нативная сторона строит из него GuestAllocations::Mapped и вызывает обработчик.
using GpuMapDispatcher = void (*)(void* callback, void* lease, std::uint64_t generation);

}

extern "C" void Aps5CoreShimInit(Aps5Native::X86Caller caller, const std::uint64_t* functions, std::uint64_t count);

#endif
