#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace AgcDriver::Graphics {

class Texture;

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target);
std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel);
void ClearDepthSurfaces(VkDevice device);
// A depth or stencil plane of a depth surface at this address with this extent. The extent matters:
// guest memory of a released depth target is reused for other surfaces (a title's blur chain of small
// storage images), and those must not be taken for the depth plane.
bool DepthSurfaceAt(std::uint64_t address, std::uint32_t width, std::uint32_t height);
std::uint64_t HtileDepthClearAddress(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads);
void NoteHtileDepthClear(std::uint64_t htileAddress);
// A buffer fill that covers a whole depth or stencil plane of a depth surface (titles clear them
// so, outside the DB): the surface image never reads guest memory back, so the matching aspect is
// cleared to the fill value. Returns whether any surface took it. APS5_NO_DEPTH_FILL_CLEAR=1 skips it.
bool FillDepthSurfaces(const Context& context, std::uint64_t address, std::size_t bytes, std::span<const std::uint32_t, 4> pattern);
bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture);
std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components);

}

#endif
