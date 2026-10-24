#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

#if defined(__linux__)
// The guest walks its own modules: the main program (no name) and the relinked guest modules
// (*.guest.prx). AnyPS5's own libraries and the host libraries are not PS5 modules; showing them
// cost the game's walkers (PvZ calls one often, parsing every entry) a hundred extra callbacks each.
// APS5_GUEST_SEES_ALL_MODULES=1 shows every loaded image, as before.
struct GuestWalk {
    int (*callback)(dl_phdr_info*, std::size_t, void*);
    void* data;
};

bool GuestModule(const dl_phdr_info* image) {
    const char* name = image->dlpi_name;
    if (name == nullptr || name[0] == '\0') return true;
    constexpr char suffix[] = ".guest.prx";
    const auto length = std::strlen(name);
    return length >= sizeof(suffix) - 1 && std::strcmp(name + length - (sizeof(suffix) - 1), suffix) == 0;
}

int GuestOnly(dl_phdr_info* image, std::size_t size, void* data) {
    const auto* walk = static_cast<const GuestWalk*>(data);
    return GuestModule(image) ? walk->callback(image, size, walk->data) : 0;
}
#endif

}

extern "C" {

int APS5_VABI dl_iterate_phdr_nid_postfix(int (*callback)(dl_phdr_info*, std::size_t, void*), void* data) {
#if defined(__linux__)
    static const bool all = std::getenv("APS5_GUEST_SEES_ALL_MODULES") != nullptr;
    if (all) return dl_iterate_phdr(callback, data);
    GuestWalk walk{callback, data};
    return dl_iterate_phdr(&GuestOnly, &walk);
#elif defined(__APPLE__)
    return dl_iterate_phdr(callback, data);
#else
    (void)callback;
    (void)data;
    throw std::runtime_error("dl_iterate_phdr not available on this platform");
#endif
}

}
