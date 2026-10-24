// x86-сторона нативной группы AnyPS5 (входит в x86-заглушку libSceAgcDriver.prx, эмулируется box64): загружает мост box64
// libaps5bridge.so и нативные (aarch64) сборки драйвера и вывода, передаёт нативному shim ядра
// таблицу x86-функций (Aps5Native.hpp) и обслуживает вызовы x86-заглушек (Aps5NativeCall).
// Каталог нативных библиотек: APS5_NATIVE_DIR, по умолчанию <каталог этой библиотеки>/native.
#include "prx/aps5native/Aps5Native.hpp"
#include "prx/aps5native/X86Host.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <unistd.h>

extern "C" void* mmap_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) noexcept;

namespace {

using Load = void* (*)(const char*);
using Symbol = void* (*)(void*, const char*);
using Call = std::uint64_t (*)(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
using CallFd = std::uint64_t (*)(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, double, double, double, double);
using Caller = void* (*)();

struct Bridge {
    Load load = nullptr;
    Symbol symbol = nullptr;
    Call call = nullptr;
    CallFd callFd = nullptr;
    Caller caller = nullptr;
    std::string directory;
};

[[noreturn]] void fail(const char* what, const char* detail) {
    std::fprintf(stderr, "[aps5native] %s: %s\n", what, detail != nullptr ? detail : "");
    std::fflush(stderr);
    std::abort();
}

std::string thisDirectory() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&thisDirectory), &info) == 0 || info.dli_fname == nullptr) return ".";
    std::string path = info.dli_fname;
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

// --- вспомогательные функции для C++-объектов x86-ядра --------------------------------------

void* equeuePinAcquire(KernelEqueue eq) {
    auto ref = EqueuePin_nid_postfix(eq);
    return ref ? new KernelEqueueRef(std::move(ref)) : nullptr;
}

void equeuePinRelease(void* handle) {
    delete static_cast<KernelEqueueRef*>(handle);
}

void* leaseAcquire() {
    return new GuestAllocations::Lease(GuestAllocations::GuestAllocationsAcquire_nid_postfix());
}

void* leaseAcquireWindow(std::uint64_t begin, std::uint64_t end) {
    return new GuestAllocations::Lease(GuestAllocations::GuestAllocationsAcquireWindow_nid_postfix(static_cast<std::uintptr_t>(begin), static_cast<std::uintptr_t>(end)));
}

std::uint64_t leaseCount(void* lease) {
    return static_cast<GuestAllocations::Lease*>(lease)->size();
}

const GuestAllocations::Range* leaseAt(void* lease, std::uint64_t index) {
    return (*static_cast<GuestAllocations::Lease*>(lease))[index].get();
}

void leaseRelease(void* lease) {
    delete static_cast<GuestAllocations::Lease*>(lease);
}

// Отдельное x86-закрепление (shared_ptr в куче) на каждый диапазон снимка: нативный код держит
// диапазоны поштучно, и отпущенный диапазон не должен оставаться закреплённым через соседей.
void leasePinAll(void* lease, const GuestAllocations::Range** ranges, void** pins) {
    const auto& items = *static_cast<GuestAllocations::Lease*>(lease);
    for (std::size_t i = 0; i < items.size(); ++i) {
        ranges[i] = items[i].get();
        pins[i] = new std::shared_ptr<const GuestAllocations::Range>(items[i]);
    }
}

void rangeUnpin(void* pin) {
    delete static_cast<std::shared_ptr<const GuestAllocations::Range>*>(pin);
}

void rangeUnpinBatch(void** pins, std::uint64_t count) {
    for (std::uint64_t i = 0; i < count; ++i) delete static_cast<std::shared_ptr<const GuestAllocations::Range>*>(pins[i]);
}

void* nativeInvalidator = nullptr;
void* nativePinWaiter = nullptr;
void* nativeGpuMapDispatcher = nullptr;
void* nativeGpuMapObserver = nullptr;
void* nativeShutdown = nullptr;

Bridge& bridge();

void setInvalidator(void* callback) {
    nativeInvalidator = callback;
    GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(callback == nullptr ? nullptr : +[](std::uintptr_t address, std::size_t bytes) {
        bridge().call(nativeInvalidator, address, bytes, 0, 0, 0, 0, 0, 0);
    });
}

void setPinWaiter(void* callback) {
    nativePinWaiter = callback;
    GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(callback == nullptr ? nullptr : +[](std::uintptr_t address, std::size_t bytes) {
        return (bridge().call(nativePinWaiter, address, bytes, 0, 0, 0, 0, 0, 0) & 0xffu) != 0;
    });
}

void setGpuMapObserver(void* dispatcher, void* callback) {
    nativeGpuMapDispatcher = dispatcher;
    nativeGpuMapObserver = callback;
    GuestAllocations::GuestAllocationsSetGpuMapObserver_nid_postfix(callback == nullptr ? nullptr : +[](const GuestAllocations::Mapped& ranges, std::uint64_t generation) {
        // Снимок живых диапазонов; нативная сторона отпускает его через LeaseRelease.
        auto* lease = new GuestAllocations::Lease;
        for (const auto& weak : ranges) {
            if (auto range = weak.lock()) lease->push_back(std::move(range));
        }
        bridge().call(nativeGpuMapDispatcher, reinterpret_cast<std::uintptr_t>(nativeGpuMapObserver), reinterpret_cast<std::uintptr_t>(lease), generation, 0, 0, 0, 0, 0);
    });
}

void registerShutdown(void* callback) {
    nativeShutdown = callback;
    LibcRegisterShutdown_nid_postfix(+[] { bridge().call(nativeShutdown, 0, 0, 0, 0, 0, 0, 0, 0); });
}

bool shutdownRequested() {
    return LibcShutdownToken_nid_postfix().stop_requested();
}

void appTitle(AppTitle* out) {
    *out = GetAppTitle_nid_postfix();
}

void appIconData(AppIconData* out) {
    *out = GetAppIconData_nid_postfix();
}

// Трамплины колбэков фильтров очереди событий: x86-ядро вызывает их как обычные x86-функции, а они
// передают вызов нативной функции, привязанной к слоту.
constexpr int TrampolineSlots = 16;
void* trampolineNative[4][TrampolineSlots] = {};
std::mutex trampolineMutex;

template <int I>
void triggerTrampoline(KernelEqueueEvent* event, void* data) {
    bridge().call(trampolineNative[Aps5Native::EqueueTrigger][I], reinterpret_cast<std::uintptr_t>(event), reinterpret_cast<std::uintptr_t>(data), 0, 0, 0, 0, 0, 0);
}
template <int I>
void resetTrampoline(KernelEqueueEvent* event) {
    bridge().call(trampolineNative[Aps5Native::EqueueReset][I], reinterpret_cast<std::uintptr_t>(event), 0, 0, 0, 0, 0, 0, 0);
}
template <int I>
void deleteTrampoline(KernelEqueue eq, KernelEqueueEvent* event) {
    bridge().call(trampolineNative[Aps5Native::EqueueDelete][I], static_cast<std::uint64_t>(eq), reinterpret_cast<std::uintptr_t>(event), 0, 0, 0, 0, 0, 0);
}
template <int I>
bool pollTrampoline(KernelEqueueEvent* event) {
    return (bridge().call(trampolineNative[Aps5Native::EqueuePoll][I], reinterpret_cast<std::uintptr_t>(event), 0, 0, 0, 0, 0, 0, 0) & 0xffu) != 0;
}

template <int... I>
void* trampolineAt(std::uint32_t kind, int slot, std::integer_sequence<int, I...>) {
    void* result = nullptr;
    ((slot == I ? (result = kind == Aps5Native::EqueueTrigger ? reinterpret_cast<void*>(&triggerTrampoline<I>)
                          : kind == Aps5Native::EqueueReset  ? reinterpret_cast<void*>(&resetTrampoline<I>)
                          : kind == Aps5Native::EqueueDelete ? reinterpret_cast<void*>(&deleteTrampoline<I>)
                                                              : reinterpret_cast<void*>(&pollTrampoline<I>), 0) : 0), ...);
    return result;
}

void* equeueTrampoline(std::uint32_t kind, void* native) {
    if (native == nullptr || kind > Aps5Native::EqueuePoll) return nullptr;
    std::lock_guard lock(trampolineMutex);
    int slot = 0;
    while (slot < TrampolineSlots && trampolineNative[kind][slot] != nullptr && trampolineNative[kind][slot] != native) ++slot;
    if (slot == TrampolineSlots) fail("equeue trampolines", "all slots are in use");
    trampolineNative[kind][slot] = native;
    return trampolineAt(kind, slot, std::make_integer_sequence<int, TrampolineSlots>{});
}

// Нативная сторона передаёт свой KernelEqueueEvent: поля до pendingEvents у libc++ и libstdc++
// раскладываются одинаково (std::shared_ptr — два указателя), а std::deque — нет. Собираем x86-копию
// из общего префикса; владельца (нативный shared_ptr) держит нативная сторона.
int addEventFromNative(KernelEqueue eq, const KernelEqueueEvent* native) {
    KernelEqueueEvent event{};
    event.triggered = native->triggered;
    event.deadlineNs = native->deadlineNs;
    event.intervalNs = native->intervalNs;
    event.event = native->event;
    event.filter.data = native->filter.data;
    event.filter.triggerFunc = native->filter.triggerFunc;
    event.filter.resetFunc = native->filter.resetFunc;
    event.filter.deleteEventFunc = native->filter.deleteEventFunc;
    event.filter.pollFunc = native->filter.pollFunc;
    return EqueueAddEvent_nid_postfix(eq, event);
}

template <class F>
std::uint64_t address(F function) {
    return reinterpret_cast<std::uint64_t>(reinterpret_cast<void*>(function));
}

void initializeCoreShim(Bridge& b) {
    std::uint64_t table[Aps5Native::X86FunctionCount]{};
    table[Aps5Native::EqueueAddEvent] = address(&addEventFromNative);
    table[Aps5Native::EqueuePushPending] = address(&EqueuePushPending_nid_postfix);
    table[Aps5Native::EqueueDeleteEvent] = address(&EqueueDeleteEvent_nid_postfix);
    table[Aps5Native::EqueueTriggerEvent] = address(&EqueueTriggerEvent_nid_postfix);
    table[Aps5Native::EqueuePinAcquire] = address(&equeuePinAcquire);
    table[Aps5Native::EqueuePinRelease] = address(&equeuePinRelease);
    table[Aps5Native::GuestArenaRange] = address(&GuestArena::GuestArenaRange_nid_postfix);
    table[Aps5Native::GuestArenaSharedBacking] = address(&GuestArena::GuestArenaSharedBacking_nid_postfix);
    table[Aps5Native::GuestArenaWriteSharedBacking] = address(&GuestArena::GuestArenaWriteSharedBacking_nid_no_patch);
    table[Aps5Native::GuestAllocationsGeneration] = address(&GuestAllocations::GuestAllocationsGeneration_nid_postfix);
    table[Aps5Native::GuestAllocationsGenerationCounter] = address(&GuestAllocations::GuestAllocationsGenerationCounter_nid_no_patch);
    table[Aps5Native::LeaseAcquire] = address(&leaseAcquire);
    table[Aps5Native::LeaseCount] = address(&leaseCount);
    table[Aps5Native::LeaseAt] = address(&leaseAt);
    table[Aps5Native::LeaseRelease] = address(&leaseRelease);
    table[Aps5Native::SetInvalidator] = address(&setInvalidator);
    table[Aps5Native::SetPinWaiter] = address(&setPinWaiter);
    table[Aps5Native::SetGpuMapObserver] = address(&setGpuMapObserver);
    table[Aps5Native::RegisterShutdown] = address(&registerShutdown);
    table[Aps5Native::LibcAwaitExit] = address(&LibcAwaitExit_nid_postfix);
    table[Aps5Native::LibcRequestExit] = address(&LibcRequestExit_nid_postfix);
    table[Aps5Native::LibcRequestShutdown] = address(&LibcRequestShutdown_nid_postfix);
    table[Aps5Native::NotImplemented] = address(&NotImplemented_nid_no_patch);
    table[Aps5Native::ProcessTime] = address(&sceKernelGetProcessTime);
    table[Aps5Native::ProcessTimeCounter] = address(&sceKernelGetProcessTimeCounter);
    table[Aps5Native::ProcessTimeCounterFrequency] = address(&sceKernelGetProcessTimeCounterFrequency);
    table[Aps5Native::HasAppIcon] = address(&HasAppIcon_nid_postfix);
    table[Aps5Native::AppTitle] = address(&appTitle);
    table[Aps5Native::AppIconData] = address(&appIconData);
    table[Aps5Native::Mmap] = address(&mmap_nid_postfix);
    table[Aps5Native::ShutdownRequested] = address(&shutdownRequested);
    table[Aps5Native::EqueueTrampoline] = address(&equeueTrampoline);
    table[Aps5Native::LeasePinAll] = address(&leasePinAll);
    table[Aps5Native::RangeUnpin] = address(&rangeUnpin);
    table[Aps5Native::RangeUnpinBatch] = address(&rangeUnpinBatch);
    table[Aps5Native::ReadableEnd] = address(&GuestAllocations::GuestAllocationsReadableEnd_nid_postfix);
    table[Aps5Native::LeaseAcquireWindow] = address(&leaseAcquireWindow);
    for (std::uint64_t value : table) {
        if (value == 0) fail("x86 function table", "incomplete");
    }

    void* shim = b.load((b.directory + "/libaps5coreshim.so").c_str());
    if (shim == nullptr) fail("load", (b.directory + "/libaps5coreshim.so").c_str());
    void* init = b.symbol(shim, "Aps5CoreShimInit");
    if (init == nullptr) fail("symbol", "Aps5CoreShimInit");
    b.call(init, reinterpret_cast<std::uint64_t>(b.caller()), reinterpret_cast<std::uint64_t>(table), Aps5Native::X86FunctionCount, 0, 0, 0, 0, 0);
}

Bridge& bridge() {
    static Bridge b = [] {
        Bridge result;
        void* library = dlopen("libaps5bridge.so", RTLD_NOW);
        if (library == nullptr) fail("dlopen libaps5bridge.so (box64 with the AnyPS5 bridge is required)", dlerror());
        result.load = reinterpret_cast<Load>(dlsym(library, "aps5b_load"));
        result.symbol = reinterpret_cast<Symbol>(dlsym(library, "aps5b_sym"));
        result.call = reinterpret_cast<Call>(dlsym(library, "aps5b_call"));
        result.callFd = reinterpret_cast<CallFd>(dlsym(library, "aps5b_callfd"));
        result.caller = reinterpret_cast<Caller>(dlsym(library, "aps5b_x86caller"));
        if (!result.load || !result.symbol || !result.call || !result.callFd || !result.caller) fail("libaps5bridge.so", "missing entry points");
        const char* directory = std::getenv("APS5_NATIVE_DIR");
        result.directory = directory != nullptr ? directory : thisDirectory() + "/native";
        return result;
    }();
    return b;
}

std::once_flag coreOnce;

} // namespace

extern "C" void Aps5NativeResolve_nid_no_patch(const char* library, const char* const* names, void** functions, std::size_t count) {
    auto& b = bridge();
    std::call_once(coreOnce, [&] { initializeCoreShim(b); });
    std::string path = b.directory + "/" + library;
    // APK: nativeLibraryDir содержит только lib*.so, поэтому libSceX.prx упакована как libSceX.so.
    const std::string name(library);
    if (access(path.c_str(), F_OK) != 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".prx") == 0) {
        path = b.directory + "/" + name.substr(0, name.size() - 4) + ".so";
    }
    void* handle = b.load(path.c_str());
    if (handle == nullptr) fail("load", path.c_str());
    for (std::size_t i = 0; i < count; ++i) {
        functions[i] = b.symbol(handle, names[i]);
        if (functions[i] == nullptr) std::fprintf(stderr, "[aps5native] %s: %s is missing\n", library, names[i]);
    }
    std::fprintf(stderr, "[aps5native] %s: native build loaded from %s\n", library, path.c_str());
}

extern "C" std::uint64_t Aps5NativeCall_nid_no_patch(void* function, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6, std::uint64_t a7) {
    return bridge().call(function, a0, a1, a2, a3, a4, a5, a6, a7);
}

extern "C" std::uint64_t Aps5NativeCallFd_nid_no_patch(void* function, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double d0, double d1, double d2, double d3) {
    return bridge().callFd(function, a0, a1, a2, a3, a4, a5, d0, d1, d2, d3);
}

extern "C" [[noreturn]] void Aps5NativeMissing_nid_no_patch(const char* name) {
    fail("native function is missing", name);
}
