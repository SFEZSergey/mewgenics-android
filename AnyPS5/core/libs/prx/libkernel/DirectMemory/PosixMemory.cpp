#include "DirectMemory.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#include <io.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI sceKernelMlock_nid_postfix(void* address, std::uint64_t length);
extern "C" int APS5_VABI sceKernelMunlock_nid_postfix(void* address, std::uint64_t length);
extern "C" int APS5_VABI sceKernelVirtualQuery(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size);

namespace {
// FreeBSD/PS5 ABI values, independent of the host's errno and mmap constants.
constexpr int GuestInvalid = 22;
constexpr int GuestNoMemory = 12;
constexpr int GuestNotSupported = 45;
constexpr int GuestShared = 0x1;
constexpr int GuestPrivate = 0x2;
constexpr int GuestProtWrite = 0x2;
constexpr int GuestAnonymous = 0x1000;
constexpr int GuestSyncAsync = 0x1;
constexpr int GuestSyncInvalidate = 0x2;
constexpr int GuestAdviceCore = 9;
constexpr int GuestAdviceProtect = 10;
constexpr std::uintptr_t GuestUserAddressEnd = 0x800000000000;
constexpr int GuestAlignedSuper = 0x1000000;
constexpr std::size_t SuperpageSize = std::size_t{1} << 21;

bool RoundLength(std::size_t length, std::size_t& rounded) {
    constexpr auto mask = PS5_PAGE_SIZE - 1;
    if (length == 0 || length > std::numeric_limits<std::size_t>::max() - mask)
        return false;
    rounded = (length + mask) & ~mask;
    return true;
}

void SetError(int error) {
    *__error_nid_postfix() = error;
}

// Reads [offset, offset + length) of a host descriptor; the tail past the end
// of the file stays zero, as in a mapping of the file's last page.
bool ReadAt(int descriptor, void* buffer, std::size_t length, std::int64_t offset) {
    auto* out = static_cast<unsigned char*>(buffer);
    while (length != 0) {
#ifdef _WIN32
        const auto saved = _lseeki64(descriptor, 0, SEEK_CUR);
        if (saved < 0 || _lseeki64(descriptor, offset, SEEK_SET) < 0) return false;
        const int chunk = length > 0x40000000u ? 0x40000000 : static_cast<int>(length);
        const int n = _read(descriptor, out, static_cast<unsigned>(chunk));
        _lseeki64(descriptor, saved, SEEK_SET);
#else
        const ssize_t n = ::pread(descriptor, out, length, offset);
        if (n < 0 && errno == EINTR) continue;
#endif
        if (n < 0) return false;
        if (n == 0) return true;
        out += n;
        offset += n;
        length -= static_cast<std::size_t>(n);
    }
    return true;
}

bool Mapped(std::uintptr_t address) {
    VirtualQueryInfo info{};
    return sceKernelVirtualQuery(reinterpret_cast<const void*>(address), 0, &info, sizeof(info)) == 0;
}
}

extern "C" {

void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection,
                                int flags, int descriptor, std::int64_t offset) noexcept {
    const auto failed = [](int error) -> void* {
        SetError(error);
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    };
    std::size_t rounded;
    if (!RoundLength(length, rounded) || (protection & ~7) != 0)
        return failed(GuestInvalid);
    if (descriptor != -1 && (flags & GuestAnonymous) == 0) {
        // A file mapping is served as a copy of the file: right for private
        // mappings and for shared ones that are never written. A writable
        // shared mapping would need its stores to reach the file.
        const int sharing = flags & ~GuestAlignedSuper;
        if (sharing != GuestPrivate && sharing != GuestShared) return failed(GuestNotSupported);
        if (sharing == GuestShared && (protection & GuestProtWrite) != 0) return failed(GuestNotSupported);
        if (offset < 0 || (offset & static_cast<std::int64_t>(PS5_PAGE_SIZE - 1)) != 0) return failed(GuestInvalid);
        void* mapped = nullptr;
        try {
            if (DoMapAnon(&mapped, rounded, 3, 0, PS5_PAGE_SIZE) != 0) return failed(GuestNoMemory);
            if (!ReadAt(descriptor, mapped, length, offset)) {
                const int error = errno == EBADF ? 9 : GuestInvalid;
                DoMunmap(mapped, rounded);
                return failed(error);
            }
            if (protection != 3 && DoMprotect(mapped, rounded, protection) != 0) {
                DoMunmap(mapped, rounded);
                return failed(GuestInvalid);
            }
            return mapped;
        } catch (const std::bad_alloc&) {
            return failed(GuestNoMemory);
        } catch (const std::exception&) {
            return failed(GuestNoMemory);
        }
    }
    if ((flags & ~GuestAlignedSuper) != (GuestPrivate | GuestAnonymous))
        return failed(GuestNotSupported);
    if (descriptor != -1 || offset != 0)
        return failed(GuestInvalid);
    // A non-fixed address is a hint; the kernel may choose another address.
    (void)address;
    void* mapped = nullptr;
    try {
        const auto result = DoMapAnon(&mapped, rounded, protection, 0, (flags & GuestAlignedSuper) != 0 ? SuperpageSize : PS5_PAGE_SIZE);
        if (result != 0) return failed(GuestNoMemory);
        return mapped;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestNoMemory);
    }
}

int APS5_VABI munmap_nid_postfix(void* address, std::size_t length) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    std::size_t rounded;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (!RoundLength(length, rounded) || start == 0 ||
        (start & (PS5_PAGE_SIZE - 1)) != 0 ||
        rounded > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMunmap(address, rounded) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        // The allocation tracker rejects foreign, pinned, and noncontiguous ranges.
        return failed(GuestInvalid);
    }
}

int APS5_VABI mprotect_nid_postfix(void* address, std::size_t length, int protection) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (length == 0) return 0;
    if (start == 0 || length > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMprotect(address, length, protection) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestInvalid);
    }
}

int APS5_VABI mlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI munlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMunlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI msync_nid_postfix(void* address, std::size_t length, int flags) {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    constexpr std::uintptr_t mask = PS5_PAGE_SIZE - 1;
    const auto first = reinterpret_cast<std::uintptr_t>(address);
    const auto start = first & ~mask;
    const std::uintptr_t size = (length + (first - start) + mask) & ~mask;
    const auto end = start + size;
    if (end < start) return failed(GuestInvalid);
    if ((flags & (GuestSyncAsync | GuestSyncInvalidate)) == (GuestSyncAsync | GuestSyncInvalidate)) return failed(GuestInvalid);
    if (start == end ? !Mapped(start) : !GuestRangeMapped(start, end)) return failed(GuestNoMemory);
    if ((flags & GuestSyncInvalidate) != 0) throw std::runtime_error("msync: MS_INVALIDATE is not implemented");
    return 0;
}

int APS5_VABI madvise_nid_postfix(void* address, std::size_t length, int advice) {
    if (advice == GuestAdviceProtect) NotImplemented_nid_no_patch("madvise MADV_PROTECT");
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (advice < 0 || advice > GuestAdviceCore || start > GuestUserAddressEnd || length > GuestUserAddressEnd - start) {
        SetError(GuestInvalid);
        return -1;
    }
    return 0;
}

}
