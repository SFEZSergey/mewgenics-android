#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <string>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <unistd.h>

namespace AgcDriver::DriverDetail {

std::uint64_t Driver::drawRegisterKey(const QueueState& queue, const ShaderRegistry& registry, std::uint64_t deviceSerial) {
    static const bool allUserWords = std::getenv("APS5_DRAW_KEY_ALL_USER_WORDS") != nullptr;
    constexpr std::uint64_t Prime = 0x100000001b3ull;
    const auto userEnd = [&](std::uint32_t base) {
        if (allUserWords) return base + 32u;
        const auto resources = queue.shader.find(base - 1);
        if (resources == queue.shader.end()) return base;
        const auto count = ((resources->second >> 1u) & 0x1fu) | (((resources->second >> 27u) & 1u) << 5u);
        return base + std::min(count, 32u);
    };
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> users{{{0x00cu, userEnd(0x00cu)}, {0x08cu, userEnd(0x08cu)}, {0x10cu, userEnd(0x10cu)}}};
    const auto unread = [&](std::uint32_t offset) {
        return std::any_of(users.begin(), users.end(), [&](const auto& user) { return offset >= user.second && offset < user.first + 32u; });
    };
    // One hash per range, recomputed only when a register word of the range changed since the last
    // key (most draws change a few user words): hashing every range's few hundred registers was ~6%
    // of the worker on UI screens. A range's hash depends on its own registers only (the user counts
    // that decide the unread words lie in the same shader ranges as the words).
    // Debug aid: APS5_TRACE_DRAW_KEY=<file> prints, while the file exists, for up to 600 keys every
    // range whose hash moved with its register words ("[draw-key]"), to tell which registers make
    // keys unique.
    static const char* traceKey = std::getenv("APS5_TRACE_DRAW_KEY");
    static std::atomic<int> traced{0};
    const bool tracing = traceKey != nullptr && traced.load(std::memory_order_relaxed) < 600 && access(traceKey, F_OK) == 0 && traced.fetch_add(1, std::memory_order_relaxed) < 600;
    std::string traceLine;
    auto& cache = queue.drawKeyRanges;
    static_assert(Graphics::DrawKeyRegisters.size() <= std::tuple_size_v<decltype(cache.hashes)>);
    for (std::size_t index = 0; index < Graphics::DrawKeyRegisters.size(); ++index) {
        const auto& range = Graphics::DrawKeyRegisters[index];
        const auto& bank = range.bank == Graphics::RegisterBank::Context ? queue.context : range.bank == Graphics::RegisterBank::Shader ? queue.shader : queue.userConfig;
        const auto end = range.first + range.count;
        if (cache.valid) {
            bool changed = false;
            for (std::size_t word = range.first / 64u; word <= (end - 1u) / 64u; ++word) changed = changed || bank.WordChanged(word);
            if (!changed) continue;
        }
        std::uint64_t hash = (0xcbf29ce484222325ull ^ ((static_cast<std::uint64_t>(range.bank) << 32u) | range.first)) * Prime;
        const bool shader = range.bank == Graphics::RegisterBank::Shader;
        for (auto it = bank.lower_bound(range.first); it != bank.end() && it->first < end; ++it) {
            if (shader && unread(it->first)) continue;
            hash = (hash ^ it->first) * Prime;
            hash = (hash ^ it->second) * Prime;
        }
        if (tracing && hash != cache.hashes[index]) {
            char text[64];
            std::snprintf(text, sizeof(text), " | %c%03x:", range.bank == Graphics::RegisterBank::Context ? 'c' : range.bank == Graphics::RegisterBank::Shader ? 's' : 'u', range.first);
            traceLine += text;
            for (auto it = bank.lower_bound(range.first); it != bank.end() && it->first < end && traceLine.size() < 1500; ++it) {
                if (shader && unread(it->first)) continue;
                std::snprintf(text, sizeof(text), " %x=%08x", it->first, it->second);
                traceLine += text;
            }
        }
        cache.hashes[index] = hash;
    }
    if (tracing) std::fprintf(stderr, "[draw-key]%s\n", traceLine.empty() ? " (no range moved)" : traceLine.c_str());
    cache.valid = true;
    queue.context.ForgetChanges();
    queue.shader.ForgetChanges();
    queue.userConfig.ForgetChanges();

    std::uint64_t key = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint64_t value) {
        key ^= value;
        key *= Prime;
    };
    // The decode key: the same registers without the user words of any stage (the decoders read
    // them only into the programs' user data, which a decode-cache hit reads again), so draws that
    // differ only in per-draw user data (ring-buffer addresses) share one decode.
    std::uint64_t decodeKey = 0x84222325cbf29ce4ull;
    const auto decodeMix = [&](std::uint64_t value) {
        decodeKey ^= value;
        decodeKey *= Prime;
    };
    const auto userWord = [](std::uint32_t offset) { return (offset >= 0x00cu && offset < 0x02cu) || (offset >= 0x08cu && offset < 0x0acu) || (offset >= 0x10cu && offset < 0x12cu); };
    decodeMix(deviceSerial);
    for (std::size_t index = 0; index < Graphics::DrawKeyRegisters.size(); ++index) {
        const auto& range = Graphics::DrawKeyRegisters[index];
        if (range.bank != Graphics::RegisterBank::Shader) {
            decodeMix(cache.hashes[index]);
            continue;
        }
        std::uint64_t hash = (0xcbf29ce484222325ull ^ range.first) * Prime;
        for (auto it = queue.shader.lower_bound(range.first); it != queue.shader.end() && it->first < range.first + range.count; ++it) {
            if (userWord(it->first)) continue;
            hash = (hash ^ it->first) * Prime;
            hash = (hash ^ it->second) * Prime;
        }
        decodeMix(hash);
    }
    mix(deviceSerial);
    for (std::size_t index = 0; index < Graphics::DrawKeyRegisters.size(); ++index) mix(cache.hashes[index]);
    for (const auto base : {0x008u, 0x088u, 0x0c8u, 0x108u, 0x148u}) {
        const auto both = [&](std::uint64_t value) {
            mix(value);
            decodeMix(value);
        };
        const auto low = queue.shader.find(base);
        const auto high = queue.shader.find(base + 1);
        if (low == queue.shader.end() || high == queue.shader.end()) {
            both(0);
            continue;
        }
        const auto address = (static_cast<std::uint64_t>(low->second) << 8u) | (static_cast<std::uint64_t>(high->second & 0xffu) << 40u);
        auto it = registry.upper_bound(address);
        if (it == registry.begin()) {
            both(1);
            continue;
        }
        --it;
        for (const auto& snapshot : it->second) both(reinterpret_cast<std::uintptr_t>(snapshot.get()));
        both(address - it->first);
    }
    cache.decodeKey = decodeKey;
    return key;
}

bool Driver::sameVertexInfo(const ShaderRecompiler::ShaderVertexStageInfo& a, const ShaderRecompiler::ShaderVertexStageInfo& b) {
    if (a.resourcesNum != b.resourcesNum || a.fetchAttribReg != b.fetchAttribReg || a.fetchBufferReg != b.fetchBufferReg || a.fetchEmbedded != b.fetchEmbedded) return false;
    for (std::uint32_t i = 0; i < a.resourcesNum && i < a.resources.size(); ++i) {
        if (a.resources[i].fields != b.resources[i].fields) return false;
        const auto& x = a.resourcesDst[i];
        const auto& y = b.resourcesDst[i];
        if (x.registerStart != y.registerStart || x.registersNum != y.registersNum || x.attrId != y.attrId || x.fetchIndex != y.fetchIndex) return false;
    }
    return true;
}

bool Driver::sameDecode(const DrawDecode& a, const DrawDecode& b) {
    const auto& s = a.state;
    const auto& t = b.state;
    const auto sameColor = [](const Graphics::ColorTarget& x, const Graphics::ColorTarget& y) {
        return x.address == y.address && x.extent.width == y.extent.width && x.extent.height == y.extent.height && x.format == y.format && x.bytes == y.bytes && x.componentMapping == y.componentMapping && x.tileMode == y.tileMode && x.elementBytes == y.elementBytes && x.dccAddress == y.dccAddress && x.dccAlphaOnMsb == y.dccAlphaOnMsb && x.slot == y.slot && x.exportIndex == y.exportIndex;
    };
    const auto sameBlend = [](const VkPipelineColorBlendAttachmentState& x, const VkPipelineColorBlendAttachmentState& y) {
        return x.blendEnable == y.blendEnable && x.srcColorBlendFactor == y.srcColorBlendFactor && x.dstColorBlendFactor == y.dstColorBlendFactor && x.colorBlendOp == y.colorBlendOp && x.srcAlphaBlendFactor == y.srcAlphaBlendFactor && x.dstAlphaBlendFactor == y.dstAlphaBlendFactor && x.alphaBlendOp == y.alphaBlendOp && x.colorWriteMask == y.colorWriteMask;
    };
    const auto sameMesh = [](const std::optional<ShaderRecompiler::MeshConfiguration>& x, const std::optional<ShaderRecompiler::MeshConfiguration>& y) {
        if (x.has_value() != y.has_value()) return false;
        if (!x) return true;
        return x->inputPrimitive == y->inputPrimitive && x->primitivesPerGroup == y->primitivesPerGroup && x->verticesPerGroup == y->verticesPerGroup && x->maxVertices == y->maxVertices && x->maxPrimitives == y->maxPrimitives && x->threadsPerGroup == y->threadsPerGroup && x->ldsSizeDwords == y->ldsSizeDwords && x->provokingVertex == y->provokingVertex && x->esgsItemSize == y->esgsItemSize;
    };
    const auto sameTess = [](const std::optional<ShaderRecompiler::TessellationConfiguration>& x, const std::optional<ShaderRecompiler::TessellationConfiguration>& y) {
        if (x.has_value() != y.has_value()) return false;
        if (!x) return true;
        return x->inputControlPoints == y->inputControlPoints && x->outputControlPoints == y->outputControlPoints && x->domain == y->domain && x->partitioning == y->partitioning && x->outputTopology == y->outputTopology;
    };
    if (s.stages.path != t.stages.path || s.stages.registerValue != t.stages.registerValue || s.stages.vertexWaveSize != t.stages.vertexWaveSize || s.stages.fragmentWaveSize != t.stages.fragmentWaveSize || !sameMesh(s.stages.mesh, t.stages.mesh) || !sameTess(s.stages.tessellation, t.stages.tessellation)) return false;
    if (!sameColor(s.color, t.color) || s.colors.size() != t.colors.size() || s.blends.size() != t.blends.size()) return false;
    for (std::size_t i = 0; i < s.colors.size(); ++i) {
        if (!sameColor(s.colors[i], t.colors[i])) return false;
    }
    for (std::size_t i = 0; i < s.blends.size(); ++i) {
        if (!sameBlend(s.blends[i], t.blends[i])) return false;
    }
    if (s.hasColorTarget != t.hasColorTarget || s.rectList != t.rectList || s.renderExtent.width != t.renderExtent.width || s.renderExtent.height != t.renderExtent.height || s.topology != t.topology || s.negativeOneToOne != t.negativeOneToOne || s.depthClamp != t.depthClamp || s.conservativeRasterization != t.conservativeRasterization || s.cullMode != t.cullMode || s.frontFace != t.frontFace || !sameBlend(s.blend, t.blend) || s.blendConstants != t.blendConstants) return false;
    if (std::memcmp(&s.viewport, &t.viewport, sizeof(VkViewport)) != 0 || std::memcmp(&s.scissor, &t.scissor, sizeof(VkRect2D)) != 0) return false;
    const auto& p = a.pixel;
    const auto& q = b.pixel;
    if (p.interpolatorCount != q.interpolatorCount || p.interpolatorSettings != q.interpolatorSettings || p.wave32 != q.wave32 || p.inputAddr != q.inputAddr || p.hasPerspectiveCenterVgpr != q.hasPerspectiveCenterVgpr || p.perspectiveCentroid != q.perspectiveCentroid || p.posX != q.posX || p.posY != q.posY || p.posZ != q.posZ || p.posW != q.posW || p.frontFace != q.frontFace || p.ancillary != q.ancillary || p.sampleShading != q.sampleShading || p.noPerspective != q.noPerspective || p.linearCentroid != q.linearCentroid || p.pixelKillEnable != q.pixelKillEnable || p.depthExportEnable != q.depthExportEnable || p.sampleMaskExportEnable != q.sampleMaskExportEnable || p.earlyZ != q.earlyZ || p.executeOnNoop != q.executeOnNoop || p.conservativeZExport != q.conservativeZExport || p.orderedPixelShader != q.orderedPixelShader || p.targetOutputMode != q.targetOutputMode || p.targetExportMapping != q.targetExportMapping) return false;
    if (a.roles != b.roles || a.programs.size() != b.programs.size()) return false;
    for (std::size_t i = 0; i < a.programs.size(); ++i) {
        const auto& x = a.programs[i];
        const auto& y = b.programs[i];
        if (x.binary.stage != y.binary.stage || x.binary.codeAddress != y.binary.codeAddress || x.userDataBase != y.userDataBase || x.firstUserSgpr != y.firstUserSgpr || x.userData != y.userData || x.snapshot != y.snapshot || x.codeOffset != y.codeOffset) return false;
    }
    if (p.targetExportPacking != q.targetExportPacking || p.dualSourceBlend != q.dualSourceBlend || p.alphaToCoverage != q.alphaToCoverage) return false;
    return true;
}

}
