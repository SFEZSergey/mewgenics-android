#include <cstdint>

#define APS5_VABI __attribute__((visibility("default")))

typedef struct {
    std::uint32_t st_size;
    char name[256];
    std::uint64_t eh_frame_hdr_addr;
    std::uint64_t eh_frame_addr;
    std::uint32_t eh_frame_size;
    std::uint32_t reserved1;
    std::uint64_t seg0_addr;
    std::uint32_t seg0_size;
    std::uint32_t reserved2;
} ModuleInfoForUnwind;

extern "C" {

APS5_VABI int sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) { return 0; }
APS5_VABI int sceSysmoduleGetModuleHandleInternal(std::uint32_t id, std::int32_t* handle) { if(handle) *handle = 1; return 0; }
APS5_VABI int sceSysmoduleIsLoaded(std::uint16_t id) { return 0; }
APS5_VABI int sceSysmoduleLoadModule(std::uint16_t id) { return 0; }
APS5_VABI int sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) { if(ret) *ret = 0; return 0; }
APS5_VABI int sceSysmoduleUnloadModule(std::uint16_t id) { return 0; }
APS5_VABI int sceSysmoduleLoadModuleInternal(std::uint32_t id) { return 0; }
APS5_VABI int sceSysmoduleUnloadModuleInternal(std::uint32_t id) { return 0; }

}
