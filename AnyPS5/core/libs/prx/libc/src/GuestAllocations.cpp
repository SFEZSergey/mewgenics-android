#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <limits>
#include <iterator>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace GuestAllocations {
namespace {

struct Registry {
    std::mutex mutex;
    std::map<std::uint64_t, std::shared_ptr<const Range>> ranges;
    bool mainImageRegistered = false;
    std::map<std::uintptr_t, std::uintptr_t> registeredImages;
};

Registry& registry() {
    static Registry value;
    return value;
}

std::atomic<std::uint64_t> generation{1};
std::atomic<void (*)(std::uintptr_t, std::size_t)> invalidator{nullptr};
std::atomic<bool (*)(std::uintptr_t, std::size_t)> pinWaiter{nullptr};
std::atomic<void (*)(const Mapped&, std::uint64_t)> gpuMapObserver{nullptr};
std::atomic<std::uint32_t> gpuMapObserverCalls{0};

std::chrono::milliseconds pinWait() {
    static const std::chrono::milliseconds value{[] {
        const char* text = std::getenv("APS5_PIN_WAIT_MS");
        return text != nullptr ? std::strtoull(text, nullptr, 10) : 60000ull;
    }()};
    return value;
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

// A mutation holds the registry lock and remembers the ranges it changed, which are reported to the
// invalidator once the generation has moved on, so a cache that observed the old state meanwhile
// notices either way.
struct MutationState {
    std::unique_lock<std::mutex> lock;
    std::vector<std::pair<std::uintptr_t, std::size_t>> changed;
    std::vector<std::pair<std::uintptr_t, std::size_t>> gpuMapped;
};

void recordChange(void* mutation, const void* pointer, std::size_t bytes) {
    if (mutation != nullptr && bytes != 0) static_cast<MutationState*>(mutation)->changed.emplace_back(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

#ifndef _WIN32
std::vector<std::pair<std::uintptr_t, std::size_t>> fileBackedWritableImage() {
    const auto image = std::filesystem::read_symlink("/proc/self/exe").string();
    std::ifstream maps("/proc/self/maps");
    std::vector<std::pair<std::uintptr_t, std::size_t>> mappings;
    for (std::string line; std::getline(maps, line);) {
        std::istringstream fields(line);
        std::string span, permissions, offset, device, inode, path;
        fields >> span >> permissions >> offset >> device >> inode >> std::ws;
        std::getline(fields, path);
        if (permissions != "rw-p" || path != image) continue;
        const auto dash = span.find('-');
        const auto begin = std::stoull(span.substr(0, dash), nullptr, 16);
        mappings.emplace_back(begin, std::stoull(span.substr(dash + 1), nullptr, 16) - begin);
    }
    return mappings;
}

bool backWritableImageAnonymously() {
    for (const auto& [address, bytes] : fileBackedWritableImage()) {
        auto* image = reinterpret_cast<void*>(address);
        auto* copy = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (copy == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mmap of an anonymous main image copy failed");
        std::memcpy(copy, image, bytes);
        if (mremap(copy, bytes, bytes, MREMAP_MAYMOVE | MREMAP_FIXED, image) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mremap of an anonymous main image copy failed");
    }
    return true;
}

[[maybe_unused]] const bool writableImageAnonymous = backWritableImageAnonymously();
#endif

}

void* GuestAllocationsBegin_nid_postfix() {
    return new MutationState{std::unique_lock<std::mutex>(registry().mutex), {}};
}

void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept {
    auto* state = static_cast<MutationState*>(mutation);
    const auto ended = generation.fetch_add(1, std::memory_order_release) + 1;
    if (const auto callback = invalidator.load(std::memory_order_acquire)) {
        for (const auto& [address, bytes] : state->changed) callback(address, bytes);
    }
    if (!state->gpuMapped.empty()) gpuMapObserverCalls.fetch_add(1);
    const auto observer = state->gpuMapped.empty() ? nullptr : gpuMapObserver.load();
    Lease mapped;
    if (observer != nullptr) {
        const auto& ranges = registry().ranges;
        for (const auto& [address, bytes] : state->gpuMapped) {
            auto it = ranges.upper_bound(address);
            if (it != ranges.begin()) --it;
            for (; it != ranges.end() && it->first < address + bytes; ++it) {
                const auto& range = it->second;
                if (range->gpu && range->readable && range->bytes != 0 && range->address + range->bytes > address) mapped.push_back(range);
            }
        }
        std::sort(mapped.begin(), mapped.end(), [](const auto& left, const auto& right) { return left->address < right->address; });
        mapped.erase(std::unique(mapped.begin(), mapped.end()), mapped.end());
    }
    const Mapped weak(mapped.begin(), mapped.end());
    mapped.clear();
    const bool noted = !state->gpuMapped.empty();
    delete state;
    if (!weak.empty()) observer(weak, ended);
    if (noted) gpuMapObserverCalls.fetch_sub(1);
}

std::uint64_t GuestAllocationsGeneration_nid_postfix() {
    return generation.load(std::memory_order_acquire);
}

const void* GuestAllocationsGenerationCounter_nid_no_patch() {
    static_assert(sizeof(generation) == sizeof(std::uint64_t) && decltype(generation)::is_always_lock_free);
    return &generation;
}

void GuestAllocationsSetInvalidator_nid_postfix(void (*callback)(std::uintptr_t, std::size_t)) {
    invalidator.store(callback, std::memory_order_release);
}

void GuestAllocationsInvalidate_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    generation.fetch_add(1, std::memory_order_release);
    if (const auto callback = invalidator.load(std::memory_order_acquire)) callback(address, bytes);
}

void GuestAllocationsSetGpuMapObserver_nid_postfix(void (*callback)(const Mapped& ranges, std::uint64_t generation)) {
    gpuMapObserver.store(callback);
    while (gpuMapObserverCalls.load() != 0) std::this_thread::yield();
}

void GuestAllocationsNoteGpuMapping_nid_postfix(void* mutation, const void* pointer, std::size_t bytes) {
    if (mutation != nullptr && bytes != 0) static_cast<MutationState*>(mutation)->gpuMapped.emplace_back(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)(std::uintptr_t, std::size_t)) {
    pinWaiter.store(callback, std::memory_order_release);
}

#ifdef _WIN32
namespace {
void registerImage(const void* image) {
    auto& state = registry();
    if (state.registeredImages.contains(reinterpret_cast<std::uintptr_t>(image))) return;
    auto replacement = state.ranges;
    auto cursor = reinterpret_cast<std::uintptr_t>(image);
    bool registered = false;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query a guest image");
        if (memory.AllocationBase != image) break;
        require(memory.Type == MEM_IMAGE && (memory.State == MEM_COMMIT || memory.State == MEM_RESERVE), "unsupported guest image mapping");
        require(reinterpret_cast<std::uintptr_t>(memory.BaseAddress) == cursor && memory.RegionSize != 0 && memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - cursor, "invalid guest image range");
        if (memory.State == MEM_COMMIT) {
            require((memory.Protect & PAGE_GUARD) == 0, "guarded guest image pages are not supported");
            const auto protection = memory.Protect & 0xffu;
            const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            const bool readable = writable || protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ;
            require(readable || protection == PAGE_NOACCESS || protection == PAGE_EXECUTE, "unsupported guest image protection");
            const auto next = replacement.lower_bound(cursor);
            require(next == replacement.end() || cursor + memory.RegionSize <= next->first, "guest image overlaps a registered allocation");
            if (next != replacement.begin()) {
                const auto& previous = *std::prev(next)->second;
                require(previous.address + previous.bytes <= cursor, "guest image overlaps a registered allocation");
            }
            replacement.emplace(cursor, std::make_shared<const Range>(Range{cursor, memory.RegionSize, readable, writable, cursor, memory.RegionSize, false}));
            registered = true;
        }
        cursor += memory.RegionSize;
    }
    require(registered, "guest image has no committed pages");
    state.ranges.swap(replacement);
    state.registeredImages.emplace(reinterpret_cast<std::uintptr_t>(image), cursor);
}
}

void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    const auto image = GetModuleHandleW(nullptr);
    require(image != nullptr, "cannot locate the main guest image");
    registerImage(image);
    state.mainImageRegistered = true;
}

void GuestAllocationsRegisterImage_nid_postfix(void*, const void* image) {
    require(image != nullptr, "cannot register a guest image without a base");
    registerImage(image);
}

void GuestAllocationsUnregisterImage_nid_postfix(void* mutation, const void* image) {
    auto& state = registry();
    const auto found = state.registeredImages.find(reinterpret_cast<std::uintptr_t>(image));
    if (found == state.registeredImages.end()) return;
    const auto [begin, end] = *found;
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, image, end - begin);
    recordChange(mutation, image, end - begin);
    std::erase_if(state.ranges, [&](const auto& entry) { return !entry.second->releasable && entry.second->allocationAddress >= begin && entry.second->allocationAddress < end; });
    state.registeredImages.erase(found);
}
#else
void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    // Сегменты PT_LOAD первого образа как отрезки [start, end) с правами. Раньше образ раскладывался
    // по страницам в std::map: у PvZ это ~40 млн страниц (~160 ГБ адресов) — 10+ с и гигабайты памяти
    // на старте. Теперь: разбиение по границам сегментов и слияние соседних кусков с одинаковыми правами
    // (результат тот же, что у постраничного объединения).
    struct Segment {
        std::uint64_t start;
        std::uint64_t end;
        bool readable;
        bool writable;
    };
    std::vector<Segment> segments;
    const auto pageSize = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
    std::pair<std::vector<Segment>*, std::uint64_t> collection{&segments, pageSize};
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& collected = *static_cast<std::pair<std::vector<Segment>*, std::uint64_t>*>(data);
        for (int index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
            const auto start = (image->dlpi_addr + header.p_vaddr) & ~(collected.second - 1);
            const auto end = (image->dlpi_addr + header.p_vaddr + header.p_memsz + collected.second - 1) & ~(collected.second - 1);
            collected.first->push_back({start, end, (header.p_flags & (PF_R | PF_W)) != 0, (header.p_flags & PF_W) != 0});
        }
        return 1;
    }, &collection);
    require(!segments.empty(), "main guest image has no loadable segments");
    std::vector<std::uint64_t> bounds;
    for (const auto& segment : segments) {
        bounds.push_back(segment.start);
        bounds.push_back(segment.end);
    }
    std::sort(bounds.begin(), bounds.end());
    bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
    struct Piece {
        std::uint64_t start;
        std::uint64_t end;
        bool readable;
        bool writable;
    };
    std::vector<Piece> pieces;
    for (std::size_t index = 0; index + 1 < bounds.size(); ++index) {
        const auto first = bounds[index];
        const auto last = bounds[index + 1];
        bool covered = false, readable = false, writable = false;
        for (const auto& segment : segments) {
            if (segment.start <= first && last <= segment.end) {
                covered = true;
                readable = readable || segment.readable;
                writable = writable || segment.writable;
            }
        }
        if (!covered) continue;
        if (!pieces.empty() && pieces.back().end == first && pieces.back().readable == readable && pieces.back().writable == writable) pieces.back().end = last;
        else pieces.push_back({first, last, readable, writable});
    }
    auto& replacement = state.ranges;
    for (const auto& piece : pieces) {
        const auto address = piece.start;
        const auto bytes = static_cast<std::size_t>(piece.end - piece.start);
        const auto next = replacement.lower_bound(address);
        require(next == replacement.end() || address + bytes <= next->first, "guest image overlaps a registered allocation");
        if (next != replacement.begin()) {
            const auto& previous = *std::prev(next)->second;
            require(previous.address + previous.bytes <= address, "guest image overlaps a registered allocation");
        }
    }
    for (const auto& piece : pieces) {
        const auto bytes = static_cast<std::size_t>(piece.end - piece.start);
        replacement.emplace(piece.start, std::make_shared<const Range>(Range{piece.start, bytes, piece.readable, piece.writable, piece.start, bytes, false}));
    }
    std::fprintf(stderr, "[guestalloc] main image: %zu segments -> %zu ranges", segments.size(), pieces.size());
    for (const auto& piece : pieces) std::fprintf(stderr, " [0x%llx+0x%llx %c%c]", static_cast<unsigned long long>(piece.start), static_cast<unsigned long long>(piece.end - piece.start), piece.readable ? 'r' : '-', piece.writable ? 'w' : '-');
    std::fprintf(stderr, "\n");
    state.mainImageRegistered = true;
}
#endif

void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu) {
    recordChange(mutation, pointer, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    require(!writable || readable, "writable guest allocation must be readable");
    auto& ranges = registry().ranges;
    const auto next = ranges.lower_bound(address);
    require(next == ranges.end() || (next->first != address && address + bytes <= next->first), "overlapping guest allocation");
    if (next != ranges.begin()) {
        const auto& previous = *std::prev(next)->second;
        require(previous.address + previous.bytes <= address, "overlapping guest allocation");
    }
    ranges.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes, true, gpu}));
}

[[noreturn]] void PinnedFailure(std::uintptr_t address, std::size_t bytes, const char* why) {
    char message[160];
    std::snprintf(message, sizeof(message), "guest allocation 0x%llx+0x%llx is still pinned by GPU work or an import (%s)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), why);
    throw std::runtime_error(message);
}

void GuestAllocationsRequireUnpinned_nid_postfix(void* mutation, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest allocation range overflow");
    const auto end = address + bytes;
    auto& ranges = registry().ranges;
    // GPU work leases allocations (a lease copies the shared_ptr) until the work completed, so wait
    // for it rather than failing the guest. The wait releases the registry lock: the driver's waiter
    // finishes that work under the device lock, under which the driver's workers acquire leases from
    // this registry, so waiting with the lock held would deadlock them. The scan restarts after the
    // lock is retaken, since another mutation may have run meanwhile. Without a waiter the lease is
    // dropped by another thread (a synchronous draw or dispatch), so yielding suffices. A waiter
    auto* state = static_cast<MutationState*>(mutation);
    const auto deadline = std::chrono::steady_clock::now() + pinWait();
    int syncedRounds = 0;
    for (;;) {
        bool pinned = false;
        auto it = ranges.upper_bound(address);
        if (it != ranges.begin()) --it;
        for (; it != ranges.end(); ++it) {
            const auto& [base, range] = *it;
            if (base >= end && base != address) break;
            if (((address < base + range->bytes && base < end) || base == address) && range.use_count() != 1) {
                pinned = true;
                break;
            }
        }
        if (!pinned) return;
        if (std::chrono::steady_clock::now() >= deadline) PinnedFailure(address, bytes, ("waited " + std::to_string(pinWait().count()) + " ms for the lease (APS5_PIN_WAIT_MS)").c_str());
        const auto waiter = pinWaiter.load(std::memory_order_acquire);
        bool progressed = false;
        if (state != nullptr && state->lock.owns_lock()) {
            state->lock.unlock();
            if (waiter != nullptr) progressed = waiter(address, bytes);
            else std::this_thread::yield();
            state->lock.lock();
        } else {
            std::this_thread::yield();
        }
        if (progressed) {
            // The lease's work finished, yet the range is pinned again: between the waiter's return
            // and the re-lock a driver worker took a new lease (every address-based build pins every
            // registered range). Rare per round, but each round is a GPU wait, so a repeat is reported.
            if (++syncedRounds == 8) std::fprintf(stderr, "[gpu] leased allocation 0x%llx+0x%llx still pinned after %d recorder syncs\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), syncedRounds);
            continue;
        }
    }
}

namespace {

// Первый элемент реестра, который может пересекать address (начинается не позже него).
std::map<std::uint64_t, std::shared_ptr<const Range>>::const_iterator firstCandidate(std::uintptr_t address) {
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    return it;
}

}

void GuestAllocationsRequireAvailable_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid fixed guest mapping");
    for (auto it = firstCandidate(address); it != registry().ranges.end(); ++it) {
        const auto& [base, range] = *it;
        if (base >= address + bytes) break;
        if (base + range->bytes > address) {
            char message[160];
            std::snprintf(message, sizeof(message), "fixed mapping 0x%llx+0x%llx overlaps a registered guest allocation 0x%llx+0x%llx", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(base), static_cast<unsigned long long>(range->bytes));
            throw std::runtime_error(message);
        }
    }
}

bool GuestAllocationsOverlaps_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    for (auto it = firstCandidate(address); it != registry().ranges.end(); ++it) {
        const auto& [base, range] = *it;
        if (base >= address + bytes) break;
        if (base + range->bytes > address) return true;
    }
    return false;
}

bool GuestAllocationsCovers_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    const auto end = address + bytes;
    auto cursor = address;
    for (auto it = firstCandidate(address); it != registry().ranges.end(); ++it) {
        const auto& [base, range] = *it;
        const auto finish = base + range->bytes;
        if (finish <= cursor) continue;
        if (base > cursor || !range->releasable) return false;
        cursor = finish;
        if (cursor >= end) return true;
    }
    return false;
}

Range GuestAllocationsFind_nid_postfix(void*, const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto exact = registry().ranges.find(address);
    if (exact != registry().ranges.end() && exact->second->allocationAddress == address && exact->second->allocationBytes == exact->second->bytes) {
        const auto& range = *exact->second;
        require(range.releasable, "guest image memory is not a releasable allocation");
        return {address, range.allocationBytes, range.readable, range.writable, address, range.allocationBytes, range.releasable, range.gpu};
    }
    for (auto it = registry().ranges.lower_bound(address); it != registry().ranges.end(); ++it) {
        const auto& range = it->second;
        if (range->allocationAddress > address) break; // выделения не пересекаются: дальше только более поздние
        if (range->allocationAddress == address) {
            require(range->releasable, "guest image memory is not a releasable allocation");
            return {address, range->allocationBytes, range->readable, range->writable, address, range->allocationBytes, range->releasable, range->gpu};
        }
    }
    throw std::runtime_error("guest allocation is not registered");
}

void GuestAllocationsRemove_nid_postfix(void* mutation, const void* pointer) {
    const auto range = GuestAllocationsFind_nid_postfix(mutation, pointer);
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, range.bytes);
    recordChange(mutation, pointer, range.bytes);
    auto& ranges = registry().ranges;
    const auto exact = ranges.find(range.address);
    if (exact != ranges.end() && exact->second->allocationBytes == exact->second->bytes) {
        ranges.erase(exact);
        return;
    }
    // Куски выделения лежат в [address, address + bytes).
    for (auto it = ranges.lower_bound(range.address); it != ranges.end() && it->first < range.address + range.bytes;) {
        if (it->second->allocationAddress == range.address) it = ranges.erase(it);
        else ++it;
    }
}

namespace {

// Правка реестра для Protect/Unmap: удаляемые ключи и вставляемые куски. Считается без копирования
// всего реестра (раньше каждая mprotect/munmap гостя копировала std::map целиком — O(N) на вызов,
// при загрузке игры это минуты под эмуляцией) и применяется только после успешного apply().
struct RangeEdit {
    std::vector<std::uint64_t> erased;
    std::vector<std::pair<std::uint64_t, std::shared_ptr<const Range>>> inserted;

    void Commit() const {
        auto& ranges = registry().ranges;
        for (const auto base : erased) ranges.erase(base);
        for (const auto& [base, entry] : inserted) ranges.emplace(base, entry);
    }
};

RangeEdit replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable, bool gpu, bool keepSame = false) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
    const auto end = address + bytes;
    const auto& ranges = registry().ranges;
    RangeEdit edit;
    auto cursor = address;
    // Первый кандидат — диапазон, начинающийся не позже address (он может его покрывать).
    auto it = ranges.upper_bound(address);
    if (it != ranges.begin()) --it;
    for (; it != ranges.end(); ++it) {
        const auto base = it->first;
        const auto& range = *it->second;
        const auto finish = base + range.bytes;
        if (finish <= address) continue;
        if (base >= end) break;
        require(base <= cursor, "guest protection or unmap range has a hole");
        // Те же права — диапазон не меняется: прежний объект остаётся (драйвер привязывает к нему
        // импорт в GPU; новый объект на каждую mprotect гостя с теми же правами заставлял заново
        // закреплять в GPU всё выделение). APS5_PROTECT_REPLACES_RANGES=1 — как раньше.
        static const bool replaceAlways = std::getenv("APS5_PROTECT_REPLACES_RANGES") != nullptr;
        if (keepSame && !remove && !replaceAlways && range.readable == readable && range.writable == writable && range.gpu == gpu) {
            cursor = std::min(finish, end);
            continue;
        }
        edit.erased.push_back(base);
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite, bool gpuMapped) {
            if (first < last) edit.inserted.emplace_back(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable, gpuMapped}));
        };
        insert(base, std::max(base, address), range.readable, range.writable, range.gpu);
        if (!remove) insert(std::max(base, address), std::min(finish, end), readable, writable, gpu);
        insert(std::min(finish, end), finish, range.readable, range.writable, range.gpu);
        cursor = std::min(finish, end);
    }
    require(cursor == end, "guest protection or unmap range is not registered");
    return edit;
}

// Остаются ли после правки куски выделения allocation (ищем только в его пределах).
bool allocationSurvives(const RangeEdit& edit, const Range& allocation) {
    const auto& ranges = registry().ranges;
    const auto first = allocation.allocationAddress;
    const auto last = first + allocation.allocationBytes;
    for (auto it = ranges.lower_bound(first); it != ranges.end() && it->first < last; ++it) {
        if (it->second->allocationAddress != first) continue;
        if (std::find(edit.erased.begin(), edit.erased.end(), it->first) == edit.erased.end()) return true;
    }
    for (const auto& [base, entry] : edit.inserted) {
        if (entry->allocationAddress == first) return true;
    }
    return false;
}

}

void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto edit = replaceRange(pointer, bytes, false, readable, writable, gpu);
    apply();
    edit.Commit();
}

// Protect для обычной mprotect гостя (страницы те же): куски с прежними правами остаются прежними
// объектами. Перемапливание (новые страницы) идёт через GuestAllocationsProtect — там объект новый.
void GuestAllocationsProtectKeep_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto edit = replaceRange(pointer, bytes, false, readable, writable, gpu, true);
    apply();
    edit.Commit();
}

void GuestAllocationsUnmap_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t, const void*, bool)>& apply) {
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    recordChange(mutation, pointer, bytes);
    const auto end = reinterpret_cast<std::uintptr_t>(pointer) + bytes;
    auto cursor = reinterpret_cast<std::uintptr_t>(pointer);
    bool any = false;
    while (cursor < end) {
        const auto found = registry().ranges.upper_bound(cursor);
        const Range* containing = nullptr;
        if (found != registry().ranges.begin()) {
            const auto& candidate = *std::prev(found)->second;
            if (cursor < candidate.allocationAddress + candidate.allocationBytes) containing = &candidate;
        }
        if (containing == nullptr) {
            if (found == registry().ranges.end() || found->first >= end) break;
            cursor = found->first;
            continue;
        }
        const auto range = *containing;
        require(range.releasable, "guest image memory cannot be unmapped");
        const auto pieceEnd = std::min<std::uint64_t>(end, range.allocationAddress + range.allocationBytes);
        const auto edit = replaceRange(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, true, false, false, false);
        const bool last = !allocationSurvives(edit, range);
        apply(reinterpret_cast<const void*>(cursor), pieceEnd - cursor, reinterpret_cast<const void*>(range.allocationAddress), last);
        edit.Commit();
        any = true;
        cursor = pieceEnd;
    }
    require(any, "unmap address is not registered");
}

std::uint64_t GuestAllocationsReadableEnd_nid_postfix(std::uintptr_t begin, std::uintptr_t end) {
    std::lock_guard lock(registry().mutex);
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(begin);
    if (it == ranges.begin()) return 0;
    const auto& range = *std::prev(it)->second;
    return range.readable && range.bytes != 0 && begin >= range.address && end <= range.address + range.bytes ? range.address + range.bytes : 0;
}

Lease GuestAllocationsAcquire_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    Lease result;
    for (const auto& [address, range] : registry().ranges) {
        if (range->readable && range->bytes != 0) result.push_back(range);
    }
    return result;
}

Lease GuestAllocationsAcquireWindow_nid_postfix(std::uintptr_t begin, std::uintptr_t end) {
    std::lock_guard lock(registry().mutex);
    Lease result;
    if (begin >= end) return result;
    const auto& ranges = registry().ranges;
    auto it = ranges.upper_bound(begin);
    if (it != ranges.begin() && std::prev(it)->second->address + std::prev(it)->second->bytes > begin) --it;
    for (; it != ranges.end() && it->second->address < end; ++it) {
        if (it->second->readable && it->second->bytes != 0) result.push_back(it->second);
    }
    return result;
}

Lease GuestAllocationsQuery_nid_postfix(std::uintptr_t address, bool findNext) {
    std::lock_guard lock(registry().mutex);
    const auto& ranges = registry().ranges;
    Lease result;
    const auto above = ranges.upper_bound(address);
    // Ranges do not overlap: only the last non-empty range starting at or below the address can hold it.
    for (auto it = above; it != ranges.begin();) {
        --it;
        if (it->second->bytes == 0) continue;
        if (address < it->second->address + it->second->bytes) result.push_back(it->second);
        break;
    }
    if (!result.empty() || !findNext) return result;
    for (auto it = above; it != ranges.end(); ++it) {
        if (it->second->bytes != 0) { result.push_back(it->second); break; }
    }
    return result;
}

Lease GuestAllocationsAcquireAll_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    Lease result;
    result.reserve(registry().ranges.size());
    for (const auto& [address, range] : registry().ranges) {
        if (range->bytes != 0) result.push_back(range);
    }
    return result;
}

}
