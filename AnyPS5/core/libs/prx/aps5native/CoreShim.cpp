// Нативный (aarch64) shim ядра AnyPS5 для группы, загружаемой box64 (см. Aps5Native.hpp): определяет
// функции libc/libkernel, нужные драйверу и выводу, и переадресует их в x86-ядро через мост box64.
// C++-объекты x86-стороны (shared_ptr) сюда не попадают: x86 держит их у себя и отдаёт дескрипторы,
// а нативные shared_ptr освобождают их через x86 при последней ссылке.
#include "prx/aps5native/Aps5Native.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <vector>

namespace {

Aps5Native::X86Caller caller = nullptr;
std::array<std::uint64_t, Aps5Native::X86FunctionCount> functions{};
// The x86 libc's generation counter, read in place (see GuestAllocationsGenerationCounter): the driver
// checks the generation several times per draw, each a box64 RunFunction otherwise.
const std::atomic<std::uint64_t>* generationCounter = nullptr;
std::stop_source shutdownSource;

std::uint64_t x86(Aps5Native::X86Function function, std::uint64_t a0 = 0, std::uint64_t a1 = 0, std::uint64_t a2 = 0, std::uint64_t a3 = 0, std::uint64_t a4 = 0, std::uint64_t a5 = 0) {
    if (caller == nullptr || functions[function] == 0) {
        std::fprintf(stderr, "[aps5native] x86 function %u called before Aps5CoreShimInit\n", static_cast<unsigned>(function));
        std::abort();
    }
    return caller(static_cast<std::uintptr_t>(functions[function]), a0, a1, a2, a3, a4, a5);
}

template <class T>
std::uint64_t arg(T value) {
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<std::uintptr_t>(value);
    else return static_cast<std::uint64_t>(value);
}

bool asBool(std::uint64_t value) {
    return (value & 0xffu) != 0;
}

// Отпущенные нативной стороной x86-закрепления копятся и уходят в x86 одним вызовом: переход в x86
// (box64 RunFunction: sigsetjmp + sigprocmask) на каждый диапазон каждого снимка съедал до четверти
// потока драйвера. Очередь сбрасывается при следующем снимке, при переполнении и перед ожиданием
// x86-ядра на закреплённом диапазоне (pin waiter ниже), так что гость не ждёт отложенных.
std::mutex unpinMutex;
std::vector<void*> unpinQueue;
constexpr std::size_t UnpinBatchLimit = 4096;

bool flushUnpins() {
    std::vector<void*> batch;
    {
        std::lock_guard lock(unpinMutex);
        batch.swap(unpinQueue);
    }
    if (batch.empty()) return false;
    x86(Aps5Native::RangeUnpinBatch, arg(batch.data()), batch.size());
    return true;
}

void queueUnpin(void* pin) {
    bool full = false;
    {
        std::lock_guard lock(unpinMutex);
        unpinQueue.push_back(pin);
        full = unpinQueue.size() >= UnpinBatchLimit;
    }
    if (full) flushUnpins();
}

bool (*nativePinWaiter)(std::uintptr_t, std::size_t) = nullptr;

// Снимок x86 (Lease в куче x86) как нативный Lease: у каждого элемента своё x86-закрепление,
// которое отпускается при последней нативной ссылке на этот диапазон; сам снимок отпускается сразу.
GuestAllocations::Lease adoptLease(void* lease) {
    GuestAllocations::Lease result;
    if (lease == nullptr) return result;
    flushUnpins();
    const auto count = x86(Aps5Native::LeaseCount, arg(lease));
    std::vector<const GuestAllocations::Range*> ranges(count);
    std::vector<void*> pins(count);
    if (count != 0) x86(Aps5Native::LeasePinAll, arg(lease), arg(ranges.data()), arg(pins.data()));
    x86(Aps5Native::LeaseRelease, arg(lease));
    result.reserve(count);
    for (std::uint64_t i = 0; i < count; ++i) {
        void* pin = pins[i];
        result.emplace_back(ranges[i], [pin](const GuestAllocations::Range*) { queueUnpin(pin); });
    }
    return result;
}

// Нативное зеркало реестра. Полный снимок x86 на каждый Acquire — ~2000 диапазонов, каждому новое
// x86-закрепление через мост (в хабе Mewgenics до 40% потока игры). Зеркало закрепляет диапазон один
// раз; изменения приходят через инвалидатор (x86 зовёт его под замком реестра, поэтому он только
// записывает окно) и дочитываются окнами при следующем Acquire. Гость, освобождающий закреплённый
// зеркалом диапазон, попадает в pin waiter: тот выбрасывает из зеркала только диапазоны, пересекающие
// освобождаемый кусок (прежний кэш APS5_LEASE_CACHE выбрасывал всё и тянул полный снимок заново:
// титульник 60 -> 20 FPS). APS5_NO_LEASE_MIRROR=1 — снимок x86 на каждый Acquire, как раньше.
bool leaseMirrorEnabled() {
    static const bool enabled = std::getenv("APS5_NO_LEASE_MIRROR") == nullptr;
    return enabled;
}

using RangeRef = std::shared_ptr<const GuestAllocations::Range>;
using Span = std::pair<std::uint64_t, std::uint64_t>; // [begin, end)

constexpr std::size_t MirrorDirtyLimit = 16384; // больше записей — полное перечитывание
constexpr std::size_t MirrorWindowLimit = 64;   // больше окон после слияния — тоже

struct LeaseMirror {
    // Замки: dirtyMutex берётся инвалидатором под замком реестра x86 и не держится ни через что;
    // updateMutex — один обновляющий, держится через вызовы x86; mutex — зеркало и снимок, никогда
    // не держится через вызовы x86.
    std::mutex dirtyMutex;
    std::vector<Span> dirty;
    bool overflow = false;
    int updating = 0;
    std::mutex updateMutex;
    std::mutex mutex;
    std::atomic<bool> built{false};
    std::map<std::uint64_t, RangeRef> ranges;
    GuestAllocations::Lease snapshot;
    bool snapshotValid = false;
    std::atomic<std::uint64_t> acquires{0};
    std::atomic<std::uint64_t> fast{0};
    std::atomic<std::uint64_t> windows{0};
    std::atomic<std::uint64_t> rebuilds{0};
    std::atomic<std::uint64_t> waiterDrops{0};
    std::atomic<std::int64_t> reportedAt{0};
};

LeaseMirror& mirror() {
    static auto& value = *new LeaseMirror();
    return value;
}

std::atomic<void (*)(std::uintptr_t, std::size_t)> driverInvalidator{nullptr};

std::uint64_t spanEnd(std::uint64_t address, std::uint64_t bytes) {
    return bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
}

void noteDirty(std::uint64_t begin, std::uint64_t end) {
    auto& m = mirror();
    std::lock_guard lock(m.dirtyMutex);
    if (m.dirty.size() >= MirrorDirtyLimit) m.overflow = true;
    else m.dirty.emplace_back(begin, end);
}

void mirrorInvalidate(std::uintptr_t address, std::size_t bytes) {
    noteDirty(address, spanEnd(address, bytes));
    if (const auto callback = driverInvalidator.load(std::memory_order_acquire)) callback(address, bytes);
}

void installMirrorInvalidator() {
    static std::once_flag once;
    std::call_once(once, [] { x86(Aps5Native::SetInvalidator, arg(&mirrorInvalidate)); });
}

// Диапазоны зеркала, которые x86 считает пересекающими [address, end) (как в RequireUnpinned).
template <class Visit>
void forOverlapping(std::map<std::uint64_t, RangeRef>& ranges, std::uint64_t address, std::uint64_t end, Visit visit) {
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    while (it != ranges.end() && (it->first < end || it->first == address)) {
        const auto base = it->first;
        const auto next = std::next(it);
        if ((address < spanEnd(base, it->second->bytes) && base < end) || base == address) visit(it);
        it = next;
    }
}

void mirrorRebuild() {
    auto& m = mirror();
    auto lease = adoptLease(reinterpret_cast<void*>(x86(Aps5Native::LeaseAcquire)));
    std::map<std::uint64_t, RangeRef> fresh;
    for (auto& range : lease) fresh.emplace(range->address, std::move(range));
    std::map<std::uint64_t, RangeRef> old;
    GuestAllocations::Lease oldSnapshot;
    {
        std::lock_guard lock(m.mutex);
        old.swap(m.ranges);
        m.ranges.swap(fresh);
        oldSnapshot.swap(m.snapshot);
        m.snapshotValid = false;
    }
    m.built.store(true, std::memory_order_release);
    m.rebuilds.fetch_add(1, std::memory_order_relaxed);
}

// Перечитывает окно: растит его, пока оно не накроет и всё, что вернул x86, и все пересекающиеся
// записи зеркала, затем заменяет записи окна ответом x86. false — окно не сошлось.
bool mirrorRefresh(std::uint64_t begin, std::uint64_t end) {
    auto& m = mirror();
    for (int round = 0; round < 8; ++round) {
        auto lease = adoptLease(reinterpret_cast<void*>(x86(Aps5Native::LeaseAcquireWindow, begin, end)));
        m.windows.fetch_add(1, std::memory_order_relaxed);
        auto grownBegin = begin;
        auto grownEnd = end;
        for (const auto& range : lease) {
            grownBegin = std::min<std::uint64_t>(grownBegin, range->address);
            grownEnd = std::max(grownEnd, spanEnd(range->address, range->bytes));
        }
        std::vector<RangeRef> dropped;
        GuestAllocations::Lease oldSnapshot;
        {
            std::lock_guard lock(m.mutex);
            forOverlapping(m.ranges, grownBegin, grownEnd, [&](auto it) {
                grownBegin = std::min(grownBegin, it->first);
                grownEnd = std::max(grownEnd, spanEnd(it->first, it->second->bytes));
            });
            if (grownBegin != begin || grownEnd != end) {
                begin = grownBegin;
                end = grownEnd;
                continue;
            }
            forOverlapping(m.ranges, begin, end, [&](auto it) {
                dropped.push_back(std::move(it->second));
                m.ranges.erase(it);
            });
            for (auto& range : lease) m.ranges.insert_or_assign(range->address, std::move(range));
            oldSnapshot.swap(m.snapshot);
            m.snapshotValid = false;
        }
        return true;
    }
    return false;
}

void mirrorReport() {
    auto& m = mirror();
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = m.reportedAt.load(std::memory_order_relaxed);
    if (last == 0) {
        m.reportedAt.compare_exchange_strong(last, now);
        return;
    }
    if (now - last < 10 || !m.reportedAt.compare_exchange_strong(last, now)) return;
    std::size_t held = 0;
    {
        std::lock_guard lock(m.mutex);
        held = m.ranges.size();
    }
    std::fprintf(stderr, "[lease-mirror] 10 s: %llu acquires (%llu without changes), %llu windows read, %llu full reads, %llu ranges dropped for the guest; %zu ranges held\n",
        static_cast<unsigned long long>(m.acquires.exchange(0)), static_cast<unsigned long long>(m.fast.exchange(0)), static_cast<unsigned long long>(m.windows.exchange(0)),
        static_cast<unsigned long long>(m.rebuilds.exchange(0)), static_cast<unsigned long long>(m.waiterDrops.exchange(0)), held);
}

GuestAllocations::Lease mirrorSnapshot() {
    auto& m = mirror();
    std::lock_guard lock(m.mutex);
    if (!m.snapshotValid) {
        m.snapshot.clear();
        m.snapshot.reserve(m.ranges.size());
        for (const auto& [address, range] : m.ranges) m.snapshot.push_back(range);
        m.snapshotValid = true;
    }
    return m.snapshot;
}

GuestAllocations::Lease mirrorAcquire() {
    auto& m = mirror();
    m.acquires.fetch_add(1, std::memory_order_relaxed);
    mirrorReport();
    bool current = false;
    if (m.built.load(std::memory_order_acquire)) {
        std::lock_guard lock(m.dirtyMutex);
        current = m.dirty.empty() && !m.overflow && m.updating == 0;
    }
    if (current) {
        m.fast.fetch_add(1, std::memory_order_relaxed);
    } else {
        installMirrorInvalidator();
        std::lock_guard update(m.updateMutex);
        std::vector<Span> dirty;
        bool overflow = false;
        {
            std::lock_guard lock(m.dirtyMutex);
            dirty.swap(m.dirty);
            overflow = m.overflow;
            m.overflow = false;
            ++m.updating;
        }
        try {
            std::sort(dirty.begin(), dirty.end());
            std::vector<Span> merged;
            for (const auto& span : dirty) {
                if (!merged.empty() && span.first <= merged.back().second) merged.back().second = std::max(merged.back().second, span.second);
                else merged.push_back(span);
            }
            bool rebuild = !m.built.load(std::memory_order_acquire) || overflow || merged.size() > MirrorWindowLimit;
            for (std::size_t i = 0; !rebuild && i < merged.size(); ++i) rebuild = !mirrorRefresh(merged[i].first, merged[i].second);
            if (rebuild) mirrorRebuild();
        } catch (...) {
            std::lock_guard lock(m.dirtyMutex);
            --m.updating;
            m.overflow = true;
            throw;
        }
        std::lock_guard lock(m.dirtyMutex);
        --m.updating;
    }
    return mirrorSnapshot();
}

// Pin waiter: выбрасывает из зеркала диапазоны, пересекающие [address, address + bytes); они
// дочитаются при следующем Acquire. true — что-то выброшено; shared — у выброшенного остались другие
// владельцы (снимки у драйвера), то есть закрепление само не уйдёт.
bool mirrorDrop(std::uintptr_t address, std::size_t bytes, bool& shared) {
    auto& m = mirror();
    const auto end = spanEnd(address, bytes);
    std::vector<RangeRef> dropped;
    GuestAllocations::Lease oldSnapshot;
    {
        std::lock_guard lock(m.mutex);
        forOverlapping(m.ranges, address, end, [&](auto it) {
            dropped.push_back(std::move(it->second));
            m.ranges.erase(it);
        });
        if (dropped.empty()) return false;
        // Снимок держит копии выброшенных: без него они остались бы закреплены.
        oldSnapshot.swap(m.snapshot);
        m.snapshotValid = false;
    }
    oldSnapshot.clear();
    for (const auto& range : dropped) {
        noteDirty(range->address, spanEnd(range->address, range->bytes));
        if (range.use_count() != 1) shared = true;
    }
    m.waiterDrops.fetch_add(dropped.size(), std::memory_order_relaxed);
    return true;
}

bool pinWaiterFlushing(std::uintptr_t address, std::size_t bytes) {
    // Зеркало держит закрепления: x86 ждёт освобождения — отпускаем его диапазоны первыми.
    bool shared = false;
    const bool dropped = leaseMirrorEnabled() && mirrorDrop(address, bytes, shared);
    // Сброшенные отложенные закрепления — уже продвижение: x86 перепроверит диапазон сразу. Если
    // выброшенное держат снимки драйвера, продвижения не будет: ждём их работу, как без зеркала
    // (иначе зеркало, дочитав диапазон снова, крутило бы гостя по кругу).
    if ((flushUnpins() || dropped) && !shared) return true;
    return nativePinWaiter != nullptr && nativePinWaiter(address, bytes);
}

void (*registeredShutdown[8])() = {};
int registeredShutdownCount = 0;

// Обработчик остановки libc (x86 вызывает его через мост): останавливает нативный токен и зовёт
// обработчики группы.
void onShutdown() {
    shutdownSource.request_stop();
    for (int i = 0; i < registeredShutdownCount; ++i) {
        try {
            registeredShutdown[i]();
        } catch (...) {
        }
    }
}

void dispatchGpuMap(void* callback, void* lease, std::uint64_t generation) {
    const auto pinned = adoptLease(lease);
    const GuestAllocations::Mapped mapped(pinned.begin(), pinned.end());
    try {
        reinterpret_cast<void (*)(const GuestAllocations::Mapped&, std::uint64_t)>(callback)(mapped, generation);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[aps5native] GPU map observer failed: %s\n", error.what());
    } catch (...) {
    }
}

} // namespace

extern "C" void Aps5CoreShimInit(Aps5Native::X86Caller x86Caller, const std::uint64_t* table, std::uint64_t count) {
    caller = x86Caller;
    if (count != Aps5Native::X86FunctionCount) {
        std::fprintf(stderr, "[aps5native] x86 function table has %llu entries, expected %u\n", static_cast<unsigned long long>(count), static_cast<unsigned>(Aps5Native::X86FunctionCount));
        std::abort();
    }
    for (std::uint64_t i = 0; i < count; ++i) functions[i] = table[i];
    if (std::getenv("APS5_NATIVE_DEBUG") != nullptr) {
        for (std::uint64_t i = 0; i < count; ++i) std::fprintf(stderr, "[aps5native] x86 function %llu = 0x%llx\n", static_cast<unsigned long long>(i), static_cast<unsigned long long>(functions[i]));
    }
    x86(Aps5Native::RegisterShutdown, arg(&onShutdown));
    generationCounter = reinterpret_cast<const std::atomic<std::uint64_t>*>(x86(Aps5Native::GuestAllocationsGenerationCounter));
}

// --- libkernel: очереди событий ---------------------------------------------------------------

extern "C" KernelEqueueRef EqueuePin_nid_postfix(KernelEqueue eq) {
    void* handle = reinterpret_cast<void*>(x86(Aps5Native::EqueuePinAcquire, arg(eq)));
    if (handle == nullptr) return {};
    // Объект очереди нативный код не трогает: указатель лишь отмечает «очередь жива».
    auto* object = *static_cast<KernelEqueuePrivate**>(handle);
    return KernelEqueueRef(object, [handle](KernelEqueuePrivate*) { x86(Aps5Native::EqueuePinRelease, arg(handle)); });
}

extern "C" int EqueueAddEvent_nid_postfix(KernelEqueue eq, const KernelEqueueEvent& event) {
    // Колбэки фильтра вызывает x86-ядро: подменяем их x86-трамплинами. Владельца (нативный
    // shared_ptr) x86 освобождать не может: держим его здесь до конца процесса.
    KernelEqueueEvent copy = event;
    const auto trampoline = [](Aps5Native::EqueueCallback kind, void* native) {
        return reinterpret_cast<void*>(x86(Aps5Native::EqueueTrampoline, kind, arg(native)));
    };
    copy.filter.triggerFunc = reinterpret_cast<EqueueTriggerFunc>(trampoline(Aps5Native::EqueueTrigger, reinterpret_cast<void*>(event.filter.triggerFunc)));
    copy.filter.resetFunc = reinterpret_cast<EqueueResetFunc>(trampoline(Aps5Native::EqueueReset, reinterpret_cast<void*>(event.filter.resetFunc)));
    copy.filter.deleteEventFunc = reinterpret_cast<EqueueDeleteFunc>(trampoline(Aps5Native::EqueueDelete, reinterpret_cast<void*>(event.filter.deleteEventFunc)));
    copy.filter.pollFunc = reinterpret_cast<EqueuePollFunc>(trampoline(Aps5Native::EqueuePoll, reinterpret_cast<void*>(event.filter.pollFunc)));
    if (copy.filter.owner) {
        static std::mutex ownersMutex;
        static std::vector<std::shared_ptr<void>> owners;
        std::lock_guard lock(ownersMutex);
        owners.push_back(std::move(copy.filter.owner));
    }
    return static_cast<int>(x86(Aps5Native::EqueueAddEvent, arg(eq), arg(&copy)));
}

extern "C" int EqueueTriggerEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter, void* triggerData) {
    return static_cast<int>(x86(Aps5Native::EqueueTriggerEvent, arg(eq), ident, static_cast<std::uint64_t>(static_cast<std::int64_t>(filter)), arg(triggerData)));
}

extern "C" void EqueuePushPending_nid_postfix(KernelEqueueEvent* event, const KernelEvent* pending) {
    x86(Aps5Native::EqueuePushPending, arg(event), arg(pending));
}

extern "C" int EqueueDeleteEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter) {
    return static_cast<int>(x86(Aps5Native::EqueueDeleteEvent, arg(eq), ident, static_cast<std::uint64_t>(static_cast<std::int64_t>(filter))));
}

// --- libc: арена и регистр выделений гостя -----------------------------------------------------

namespace GuestArena {
extern "C" void GuestArenaRange_nid_postfix(std::uintptr_t* base, std::size_t* bytes) {
    x86(Aps5Native::GuestArenaRange, arg(base), arg(bytes));
}
extern "C" bool GuestArenaSharedBacking_nid_postfix(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    return asBool(x86(Aps5Native::GuestArenaSharedBacking, address, bytes, arg(file), arg(offset)));
}
extern "C" bool GuestArenaWriteSharedBacking_nid_no_patch(std::uintptr_t address, const void* source, std::size_t bytes) {
    return asBool(x86(Aps5Native::GuestArenaWriteSharedBacking, address, arg(source), bytes));
}
} // namespace GuestArena

namespace GuestAllocations {
extern "C" Lease GuestAllocationsAcquire_nid_postfix() {
    if (leaseMirrorEnabled()) return mirrorAcquire();
    return adoptLease(reinterpret_cast<void*>(x86(Aps5Native::LeaseAcquire)));
}
extern "C" Lease GuestAllocationsAcquireWindow_nid_postfix(std::uintptr_t begin, std::uintptr_t end) {
    return adoptLease(reinterpret_cast<void*>(x86(Aps5Native::LeaseAcquireWindow, begin, end)));
}
extern "C" std::uint64_t GuestAllocationsReadableEnd_nid_postfix(std::uintptr_t begin, std::uintptr_t end) {
    return x86(Aps5Native::ReadableEnd, begin, end);
}
extern "C" std::uint64_t GuestAllocationsGeneration_nid_postfix() {
    if (generationCounter != nullptr) return generationCounter->load(std::memory_order_acquire);
    return x86(Aps5Native::GuestAllocationsGeneration);
}
extern "C" void GuestAllocationsSetInvalidator_nid_postfix(void (*callback)(std::uintptr_t address, std::size_t bytes)) {
    if (!leaseMirrorEnabled()) {
        x86(Aps5Native::SetInvalidator, arg(callback));
        return;
    }
    // Инвалидатор x86 один: зеркало стоит в нём само и передаёт вызовы драйверу.
    driverInvalidator.store(callback, std::memory_order_release);
    installMirrorInvalidator();
}
extern "C" void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)(std::uintptr_t address, std::size_t bytes)) {
    nativePinWaiter = callback;
    x86(Aps5Native::SetPinWaiter, arg(callback != nullptr ? &pinWaiterFlushing : nullptr));
}
extern "C" void GuestAllocationsSetGpuMapObserver_nid_postfix(void (*callback)(const Mapped& ranges, std::uint64_t generation)) {
    x86(Aps5Native::SetGpuMapObserver, arg(&dispatchGpuMap), arg(callback));
}
} // namespace GuestAllocations

// --- libc: отслеживание записей — нативная реализация в WriteWatchNative.cpp ---------------------

// --- libc: завершение процесса -----------------------------------------------------------------

extern "C" void LibcRegisterShutdown_nid_postfix(void (*callback)()) {
    if (registeredShutdownCount < static_cast<int>(std::size(registeredShutdown))) registeredShutdown[registeredShutdownCount++] = callback;
}

extern "C" std::stop_token LibcShutdownToken_nid_postfix() {
    if (!shutdownSource.stop_requested() && caller != nullptr && asBool(x86(Aps5Native::ShutdownRequested))) shutdownSource.request_stop();
    return shutdownSource.get_token();
}

extern "C" void LibcRequestShutdown_nid_postfix() {
    x86(Aps5Native::LibcRequestShutdown);
}

extern "C" void LibcRequestExit_nid_postfix(int code) {
    x86(Aps5Native::LibcRequestExit, static_cast<std::uint64_t>(static_cast<std::int64_t>(code)));
}

extern "C" [[noreturn]] void LibcAwaitExit_nid_postfix() {
    x86(Aps5Native::LibcAwaitExit);
    std::abort();
}

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    x86(Aps5Native::NotImplemented, arg(funcName));
}

// --- libkernel: время и метаданные приложения --------------------------------------------------

namespace {

std::uint64_t monotonicNanos() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

std::uint64_t processStartNanos() {
    static const std::uint64_t start = monotonicNanos();
    return start;
}

} // namespace

extern "C" std::uint64_t sceKernelGetProcessTime() {
    return (monotonicNanos() - processStartNanos()) / 1000ULL;
}

extern "C" std::uint64_t sceKernelGetProcessTimeCounter() {
    return monotonicNanos() - processStartNanos();
}

extern "C" std::uint64_t sceKernelGetProcessTimeCounterFrequency() {
    return 1000000000ULL;
}

extern "C" bool HasAppIcon_nid_postfix() {
    return asBool(x86(Aps5Native::HasAppIcon));
}

extern "C" AppTitle GetAppTitle_nid_postfix() {
    AppTitle title{};
    x86(Aps5Native::AppTitle, arg(&title));
    return title;
}

extern "C" AppIconData GetAppIconData_nid_postfix() {
    AppIconData data{};
    x86(Aps5Native::AppIconData, arg(&data));
    return data;
}

extern "C" void* mmap_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) noexcept {
    return reinterpret_cast<void*>(x86(Aps5Native::Mmap, arg(address), length, static_cast<std::uint64_t>(static_cast<std::int64_t>(protection)), static_cast<std::uint64_t>(static_cast<std::int64_t>(flags)), static_cast<std::uint64_t>(static_cast<std::int64_t>(descriptor)), static_cast<std::uint64_t>(offset)));
}
