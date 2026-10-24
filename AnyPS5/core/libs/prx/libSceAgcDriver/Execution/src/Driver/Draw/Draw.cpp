#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

namespace {
// Per-draw containers, reused (cleared, capacity kept) instead of allocated for every draw packet: a
// draw allocated some twenty vectors. One set per nesting level, as draw() re-enters itself after
// recording queued labels; the lease clears its set on the way out so references (results, variants)
// do not outlive the draw. APS5_NO_DRAW_SCRATCH=1: fresh containers per draw.
struct DrawScratch {
    std::vector<DrawProgram> programs;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>> vertexInfos;
    std::vector<std::vector<Graphics::DecodeRead>> decodeReads;
    std::vector<ShaderRecompiler::RecompileResult> results;
    std::vector<Graphics::CompiledShader> stages;
    std::vector<const ShaderRecompiler::RecompileResult*> programResults;
    std::vector<std::shared_ptr<const ShaderRecompiler::RecompileResult>> heldResults;
    std::vector<StageCapture> stageCaptures;
    std::vector<std::shared_ptr<DispatchVariant>> matched;
    std::vector<std::vector<ShaderRecompiler::MemoryRegion>> matchedRegions;
    std::vector<std::shared_ptr<DispatchVariant>> fresh;
    std::vector<bool> recompiled;
    std::vector<std::uint32_t> pushOffsets;
    std::vector<std::size_t> resultIndex;
    std::vector<Graphics::GuestMemorySnapshot> snapshots;
    std::vector<std::shared_ptr<DispatchVariant>> recipeStages;
    std::vector<std::uint64_t> recipeShape;

    void Release() {
        // The per-program vectors are emptied, not destroyed: their capacity serves the next draw
        // (destroying them here, often on the recorder thread, made every draw allocate them again).
        for (auto& reads : decodeReads) reads.clear();
        for (auto& regions : matchedRegions) regions.clear();
        programs.clear(); memory.clear(); linked.clear(); vertexInfos.clear();
        results.clear(); stages.clear(); programResults.clear(); heldResults.clear(); stageCaptures.clear();
        matched.clear(); fresh.clear(); recompiled.clear(); pushOffsets.clear();
        resultIndex.clear(); snapshots.clear(); recipeStages.clear(); recipeShape.clear();
    }
};

// Scratch sets handed to the draw pipeline come back here once the recorder is done with them.
struct ScratchRecycle {
    std::mutex mutex;
    std::vector<std::unique_ptr<DrawScratch>> spare;
    static ScratchRecycle& Get() {
        static ScratchRecycle recycle;
        return recycle;
    }
    std::unique_ptr<DrawScratch> Take() {
        std::lock_guard lock(mutex);
        if (spare.empty()) return std::make_unique<DrawScratch>();
        auto scratch = std::move(spare.back());
        spare.pop_back();
        return scratch;
    }
    void Give(std::unique_ptr<DrawScratch> scratch) {
        scratch->Release();
        std::lock_guard lock(mutex);
        if (spare.size() < 64) spare.push_back(std::move(scratch));
    }
};

struct DrawScratchLease {
    static bool Disabled() {
        static const bool disabled = std::getenv("APS5_NO_DRAW_SCRATCH") != nullptr;
        return disabled;
    }
    DrawScratchLease() {
        if (Disabled()) {
            owned = std::make_unique<DrawScratch>();
            scratch = owned.get();
            return;
        }
        auto& pool = Pool();
        auto& depth = Depth();
        if (pool.size() <= depth) pool.push_back(std::make_unique<DrawScratch>());
        scratch = pool[depth].get();
        ++depth;
    }
    ~DrawScratchLease() {
        if (scratch == nullptr) return;
        scratch->Release();
        if (!owned) --Depth();
    }
    // Hands the set to a pipelined draw: this nesting level gets a fresh one.
    std::unique_ptr<DrawScratch> Detach() {
        std::unique_ptr<DrawScratch> taken;
        if (owned) {
            taken = std::move(owned);
        } else {
            auto& slot = Pool()[Depth() - 1];
            taken = std::move(slot);
            slot = ScratchRecycle::Get().Take();
            --Depth();
        }
        scratch = nullptr;
        return taken;
    }
    DrawScratchLease(const DrawScratchLease&) = delete;
    DrawScratchLease& operator=(const DrawScratchLease&) = delete;
    static std::vector<std::unique_ptr<DrawScratch>>& Pool() {
        thread_local std::vector<std::unique_ptr<DrawScratch>> pool;
        return pool;
    }
    static std::size_t& Depth() {
        thread_local std::size_t depth = 0;
        return depth;
    }
    DrawScratch* scratch = nullptr;
    std::unique_ptr<DrawScratch> owned;
};

// Guest memory a draw may write: storage buffers and its color and depth targets.
void AppendDrawWrites(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> stages, std::vector<DrawPipeline::Range>& writes) {
    for (const auto& stage : stages) {
        if (stage.program != nullptr) Driver::AppendWrittenRanges(*stage.program, writes);
    }
    const auto target = [&](const Graphics::ColorTarget& color) {
        if (color.address != 0 && color.bytes != 0) writes.emplace_back(color.address, color.address + color.bytes);
    };
    if (graphics.hasColorTarget) target(graphics.color);
    for (const auto& color : graphics.colors) target(color);
    if (graphics.depth) {
        const auto bytes = static_cast<std::uint64_t>(graphics.depth->extent.width) * graphics.depth->extent.height * 4u;
        if (graphics.depth->address != 0) writes.emplace_back(graphics.depth->address, graphics.depth->address + bytes);
        if (graphics.depth->stencilAddress != 0) writes.emplace_back(graphics.depth->stencilAddress, graphics.depth->stencilAddress + bytes);
    }
}
}

DrawPipeline* Driver::drawPipeline(std::uint32_t queue) {
    static const bool enabled = [] {
        const char* value = std::getenv("APS5_PIPELINE_DRAWS");
        return value != nullptr && value[0] == '1' && std::getenv("APS5_LOCKED_DRAW_PREPARE") == nullptr && !DrawScratchLease::Disabled();
    }();
    if (!enabled || queue != 0) return nullptr;
    thread_local std::unique_ptr<DrawPipeline> pipeline;
    if (pipeline == nullptr) {
        pipeline = std::make_unique<DrawPipeline>(queue, [queue] {
            onWorkerThread() = true;
            char role[32];
            std::snprintf(role, sizeof role, "draw recorder 0x%x", queue);
            PrioritizeWorkerThread(role);
            GuestMemory::TagGpuLockThread(queue);
            GuestMemory::SetCurrentPacket(0x27, queue);
        });
    }
    return pipeline.get();
}

// Recipes found by the stages' pipeline variant ids when the variants differ in data only
// (DrawRecipeRecord::shape). APS5_NO_SHAPE_RECIPES=1: exact variants only, as before.
bool ShapeRecipes() {
    static const bool off = std::getenv("APS5_NO_SHAPE_RECIPES") != nullptr;
    return !off;
}

bool& Driver::drawQueued() {
    static thread_local bool queued = false;
    return queued;
}

void Driver::AppendWrittenRanges(const ShaderRecompiler::RecompileResult& compiled, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    appendWrittenRanges(compiled, ranges);
}

DrawVerdict Driver::draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::string& rejected) {
    auto* pipeline = drawPipeline(submission.queue);
    // Queued labels belong to this thread and are recorded under the GPU mutex in order with the
    // draws: every earlier draw must be recorded first.
    if (pipeline != nullptr && !deferredLabels().labels.empty()) pipeline->Drain("labels");
    DrawScratchLease lease;
    auto& scratch = *lease.scratch;
    PerformanceTimer timing("Driver.Draw");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::array<double, DrawDriverPhaseCount> phaseMs{};
    std::uint64_t captures = 0;
    auto phaseLap = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    DrawPhaseTiming phaseTiming{profile, phaseMs, phaseLap};
    if (profile && packetStartedAt() != std::chrono::steady_clock::time_point{}) phaseMs[DrawRowPrologue] = std::chrono::duration<double, std::milli>(phaseLap - packetStartedAt()).count();

    const auto drawn = [&] {
        phaseTiming.Phase(DrawRowVectors);
        if (!profile) return DrawVerdict::Drawn;
        auto& pending = pendingDrawPhases();
        pending.phases = true;
        pending.captures = captures;
        pending.ms = phaseMs;
        pending.tailAt = phaseLap;
        return DrawVerdict::Drawn;
    };
    auto drawParameters = Pm4::ResolveDraw(packet, queue);
    bool traceIndirect = false;
    if (const auto verdict = precheckDraw(queue, submission, packet, drawParameters, rejected, traceIndirect)) return *verdict;
    phaseTiming.Phase(DrawRowPrecheck);
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;

    static const std::uint64_t dumpTarget = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SHADERS"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const std::uint64_t dumpSlot1 = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SLOT1"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const bool lockedPrepare = std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr;
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
    std::shared_ptr<VulkanDevice> localDevice;
    if (lockedPrepare) {

        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        gpuLock.lock();
        timing.Mark("gpu_mutex_wait");
        phaseTiming.Phase(DrawRowLockWait);
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        localDevice = device;

        recordLabelsForPacket(localDevice.get(), submission.queue);
        phaseTiming.Phase(DrawRowLabels);
    } else if ((localDevice = device.Load()) == nullptr) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        std::lock_guard createLock(GuestMemory::GpuMutex());
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        localDevice = device;
    }
    timing.Mark("device_setup");
    phaseTiming.Phase(DrawRowVectors);

    const bool useDrawEntries = drawEntries() && !ShaderRecompiler::DebugProbeActive() && dumpTarget == 0 && dumpSlot1 == 0;
    const bool registerKey = useDrawEntries && registerKeyEnabled();
    std::uint64_t drawKey = 0;
    std::shared_ptr<DrawEntry> entry;
    std::shared_ptr<const DrawDecode> decode;
    if (registerKey) {
        const auto keyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        drawKey = drawRegisterKey(queue, *submission.shaders, localDevice->Serial());
        std::lock_guard cacheLock(drawCacheMutex);
        ++drawEntryCounters.lookups;
        ++drawEntryCounters.registerKeyLookups;
        if (profile) drawEntryCounters.keyUs += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - keyStart).count();
        const auto found = drawCache.find(drawKey);
        if (found != drawCache.end()) {
            entry = found->second;
            decode = entry->decode;
        } else {
            ++drawEntryCounters.absent;
        }
    }
    if (skipFailedShaders()) {
        attemptedDrawKey() = drawKey;
        if (skipRefused(drawKey)) return DrawVerdict::Nothing;
    }
    if (decode != nullptr && !DecodeProgramsCurrent(*decode, *submission.shaders)) {
        std::lock_guard cacheLock(drawCacheMutex);
        eraseDrawEntry(drawKey, entry);
        ++drawEntryCounters.absent;
        entry = nullptr;
        decode = nullptr;
    }
    phaseTiming.Phase(DrawRowKeyLookupValidate);

    resolveDrawDecode(queue, submission, decode, registerKey, drawKey, profile);
    const auto& graphics = decode->state;
    const auto& pixel = decode->pixel;
    // Programs are only written for mesh draws (the hidden index-buffer words) and indirect ones (the
    // records' user data); the others read the decode's own vector.
    const bool writesPrograms = graphics.stages.mesh || drawParameters.indirect.has_value();
    if (writesPrograms) scratch.programs = decode->programs;
    auto& programs = writesPrograms ? scratch.programs : const_cast<std::vector<DrawProgram>&>(decode->programs);
    const auto setMeshIndexBuffer = [&](const Pm4::DrawParameters& parameters) {
        if (!graphics.stages.mesh) return;
        auto& words = programs.front().userData;
        require(programs.front().firstUserSgpr == 0 && words.size() >= ShaderRecompiler::MeshIndexBufferUserWord + 4, "mesh program lacks the hidden user words");
        const auto descriptor = Graphics::MeshIndexBufferDescriptor(parameters);
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + ShaderRecompiler::MeshIndexBufferUserWord);
    };
    if (!drawParameters.indirect) setMeshIndexBuffer(drawParameters);
    else if (graphics.stages.mesh) setMeshIndexBuffer(Pm4::DrawParameters{drawParameters.indexAddress, std::max(drawParameters.indexCount, 1u), drawParameters.indexSize, 1, 0, drawParameters.indexed});
    const std::vector<Role>& roles = decode->roles;
    phaseTiming.Phase(DrawRowDecode);

    const auto locate = [&](std::uint32_t location) -> std::optional<std::pair<std::size_t, std::size_t>> {
        if (location == 0x280u) return std::nullopt;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::Fragment || roles[i] == Role::GeometryBack || location < programs[i].userDataBase) continue;
            const auto word = location - programs[i].userDataBase + (8u - programs[i].firstUserSgpr);
            if (word < programs[i].userData.size()) return std::make_pair(i, static_cast<std::size_t>(word));
        }
        return std::nullopt;
    };
    if (drawParameters.indirect) {
        auto& indirect = *drawParameters.indirect;
        const auto sgprOf = [&](std::uint32_t location) -> std::int32_t {
            const auto word = locate(location);
            if (!word || word->first != 0) return -1;
            return static_cast<std::int32_t>(programs.front().firstUserSgpr + word->second);
        };
        indirect.baseVertexSgpr = sgprOf(indirect.baseVertexLocation);
        indirect.startInstanceSgpr = sgprOf(indirect.startInstanceLocation);
        indirect.drawIndexSgpr = sgprOf(indirect.drawIndexLocation);
    }
    auto& memory = scratch.memory;
    auto& linked = scratch.linked;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        for (const auto& region : program.memory) {
            if (!region.bytes.empty()) memory.push_back(region);
        }
        linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
    }
    timing.Mark("prepare");
    phaseTiming.Phase(DrawRowProgramPrepare);

    auto& vertexInfos = scratch.vertexInfos;
    vertexInfos.resize(programs.size());
    auto& decodeReads = scratch.decodeReads;
    decodeReads.resize(programs.size());
    const auto decodeVertexInfo = [&](std::size_t i) {
        const auto& program = programs[i];
        if (program.binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) return;
        decodeReads[i].clear();
        vertexInfos[i] = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, &decodeReads[i]);
    };
    if (!registerKey) {
        for (std::size_t i = 0; i < programs.size(); ++i) decodeVertexInfo(i);
        phaseTiming.Phase(DrawRowDecode);
    }
    ShaderMemory shaderMemory(memory, &queryPendingWrite, &observePendingWrite, hookWaitCounter());
    auto& results = scratch.results;
    auto& stages = scratch.stages;
    // Room for the rect-list pair, a geometry stage and the indirect path's copies of every program:
    // `stages` points into this vector, so it must never reallocate.
    results.reserve(programs.size() * 2u + 3u);
    stages.reserve(programs.size());
    std::uint32_t pushCursorBytes = 0;

    auto& programResults = scratch.programResults;
    programResults.assign(programs.size(), nullptr);
    // Results of the stages captured for this draw, shared with the result memo and the draw cache.
    auto& heldResults = scratch.heldResults;
    heldResults.resize(programs.size());
    std::shared_ptr<const ShaderRecompiler::RecompileResult> geometryHeld;

    auto& stageCaptures = scratch.stageCaptures;
    stageCaptures.resize(programs.size());
    auto& matched = scratch.matched;
    matched.resize(programs.size());
    auto& matchedRegions = scratch.matchedRegions;
    matchedRegions.resize(programs.size());

    auto& fresh = scratch.fresh;
    fresh.resize(programs.size());

    auto& recompiled = scratch.recompiled;
    recompiled.assign(programs.size(), false);
    bool drawHit = false;
    bool verifyHit = false;
    lookupDraw(submission, localDevice, graphics, pixel, programs, roles, vertexInfos, useDrawEntries, registerKey, profile, drawKey, entry, matched, matchedRegions, drawHit, verifyHit, phaseTiming, phaseMs);

    if (registerKey) {

        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (matched[i] != nullptr && drawHit && !verifyDrawRecipe()) {
                if (matched[i]->vertexInfo != nullptr) vertexInfos[i] = *matched[i]->vertexInfo;
                continue;
            }
            decodeVertexInfo(i);
            if (matched[i] != nullptr && verifyDrawRecipe() && (vertexInfos[i].has_value() != (matched[i]->vertexInfo != nullptr) || (vertexInfos[i] && !sameVertexInfo(*vertexInfos[i], *matched[i]->vertexInfo)))) {
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit has a vertex stage info unlike its variant's\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress));
                std::lock_guard cacheLock(drawCacheMutex);
                ++drawEntryCounters.verifyDecodeMismatches;
            }
        }
        phaseTiming.Phase(DrawRowDecode);
    }

    const auto fold = [&](const ShaderRecompiler::RecompileResult& main, Pm4::DrawParameters& parameters) { FoldDrawOffsets(main, programs.front(), parameters); };

    std::optional<Graphics::IndirectDrawPath> indirectCpu;
    auto& pushOffsets = scratch.pushOffsets;
    pushOffsets.assign(programs.size(), 0);
    auto& resultIndex = scratch.resultIndex;
    resultIndex.assign(programs.size(), 0);
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::GeometryBack) continue;
        const auto& program = programs[i];
        pushCursorBytes = Graphics::StagePushOffset(pushCursorBytes, program.binary.stage, localDevice->GraphicsPipelineLibraries());
        pushOffsets[i] = pushCursorBytes;
        if (drawHit) {

            programResults[i] = matched[i]->compiled.get();
            memory.insert(memory.end(), matchedRegions[i].begin(), matchedRegions[i].end());
        } else {
            heldResults[i] = materializeDrawStage(i, pushCursorBytes, queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected);
            if (!rejected.empty()) return DrawVerdict::Rejected;
            programResults[i] = heldResults[i].get();
        }
        const auto& result = *programResults[i];
        if (i == 0 && drawParameters.indirect) {
            indirectCpu = ClassifyIndirectDraw(result, graphics, programs.front(), localDevice, drawParameters, traceIndirect);
        } else if (i == 0) {
            fold(result, drawParameters);
        }
        const auto slotEnd = (pushCursorBytes / Graphics::PipelinePushSlotBytes + 1u) * Graphics::PipelinePushSlotBytes;
        require(result.pushConstants.size() <= slotEnd - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
        stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
        pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
    }

    // The draw's shape: its stages' pipeline variant ids (all known, else none), see DrawRecipeRecord.
    // A miss whose shape already has a recipe needs no fresh variants: they would only push older
    // ones out of the entry and be thrown away in turn (every menu draw differs in its data).
    auto& recipeShape = scratch.recipeShape;
    std::shared_ptr<const DrawRecipe> shapeRecipe;
    if (registerKey && !drawParameters.indirect && Graphics::DrawRecipes() && ShapeRecipes()) {
        for (const auto* result : programResults) {
            const auto id = result != nullptr ? result->PipelineVariantId() : 0;
            if (id == 0) {
                recipeShape.clear();
                break;
            }
            recipeShape.push_back(id);
        }
        if (!drawHit && !recipeShape.empty()) shapeRecipe = findDrawRecipe(drawKey, {}, recipeShape);
    }
    cacheDrawStages(useDrawEntries && shapeRecipe == nullptr, drawHit, drawParameters, indirectCpu, programs, stageCaptures, vertexInfos, decodeReads, verifyHit, matched, fresh, drawKey, registerKey, decode, phaseTiming);
    timing.Mark("shader_compile_and_link");

    for (const auto& reads : decodeReads) {
        for (const auto& read : reads) memory.push_back({read.address, read.bytes()});
    }

    if (recordQueuedLabelsAfterCapture(submission.queue, memory)) return draw(queue, packet, submission, rejected);

    bool rectListBuilt = false;

    std::size_t rectIndex = 0;
    const auto buildRectList = [&] {
        phaseTiming.Phase(DrawRowVectors);
        require(programs.size() == 2 && programResults[0] != nullptr && programResults[1] != nullptr, "rect-list requires vertex and fragment programs");
        auto rectangle = DrawRectangle(*programs[0].snapshot, programs[1].snapshot, programResults[0]->variantId, programResults[1]->variantId, localDevice->Target());
        if (rectListBuilt) {
            results[rectIndex] = std::move(rectangle.control);
            results[rectIndex + 1] = std::move(rectangle.evaluation);
            phaseTiming.Phase(DrawRowRectList);
            return;
        }
        require(stages.size() == 2, "rect-list requires vertex and fragment programs");
        rectIndex = results.size();
        results.push_back(std::move(rectangle.control));
        results.push_back(std::move(rectangle.evaluation));
        stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[rectIndex], 0}, {Stage::TessellationEvaluation, &results[rectIndex + 1], 0}});
        rectListBuilt = true;
        phaseTiming.Phase(DrawRowRectList);
    };
    if (graphics.rectList) buildRectList();
    if (!programs.empty() && programResults.back() != nullptr && programResults.back()->barycentricEmulation.active) {
        require(!graphics.rectList && graphics.stages.path == Graphics::ShaderPath::Vertex && programs.size() == 2 && programResults[0] != nullptr && stages.size() == 2, "a pixel shader that reads barycentrics without VK_KHR_fragment_shader_barycentric needs a vertex shader before it");
        geometryHeld = ShaderRecompiler::BuildBarycentricGeometryShaderShared(*programResults[0], *programResults[1], localDevice->Target(), localDevice->GeometryLimits());
        stages.insert(stages.begin() + 1, Graphics::CompiledShader{Stage::Geometry, geometryHeld.get(), 0});
    }
    auto& snapshots = scratch.snapshots;
    const auto snapshot = [&] {
        snapshots.clear();
        for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
    };
    snapshot();
    timing.Mark("post_compile_prepare");
    const auto lockForDraw = [&] {
        if (gpuLock.owns_lock()) return;
        phaseTiming.Phase(DrawRowVectors);
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        gpuLock.lock();
        timing.Mark("gpu_mutex_wait");
        phaseTiming.Phase(DrawRowLockWait);

        if (auto current = device.Load(); current != nullptr && current != localDevice) {
            static std::atomic<std::uint64_t> replaced{0};
            std::fprintf(stderr, "[draw] device replaced during unlocked preparation (%llu)\n", static_cast<unsigned long long>(++replaced));
            localDevice = std::move(current);
        }

        recordLabelsForPacket(localDevice.get(), submission.queue);
        phaseTiming.Phase(DrawRowLabels);
    };
    if (drawParameters.indirect && indirectCpu) {
        if (pipeline != nullptr) {
            pipeline->Drain("indirect");
            pipeline->NoteSyncDraw("indirect");
        }

        const auto indirect = *drawParameters.indirect;
        // The records patch user data and re-materialize their programs: every program gets its own
        // copy here (the shared results stay as they are).
        {

            for (std::size_t i = 0; i < programs.size(); ++i) {
                if (programResults[i] == nullptr) continue;
                resultIndex[i] = results.size();
                results.push_back(ShaderRecompiler::RecompileResult(*programResults[i]));
                for (auto& stage : stages) {
                    if (stage.program == programResults[i]) stage.program = &results[resultIndex[i]];
                }
                programResults[i] = &results[resultIndex[i]];
            }
        }
        recordQueuedLabelsBeforeRead(submission.queue);
        const auto readStart = std::chrono::steady_clock::now();
        const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
        std::vector<Pm4::DrawArguments> records;
        for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(indirect, record));
        Graphics::CountIndirectDraw(*indirectCpu, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        const auto baseVertexWord = locate(indirect.baseVertexLocation);
        const auto startInstanceWord = locate(indirect.startInstanceLocation);
        const auto drawIndexWord = indirect.drawIndexEnabled ? locate(indirect.drawIndexLocation) : std::nullopt;
        for (std::uint32_t record = 0; record < records.size(); ++record) {
            const auto& arguments = records[record];
            if (traceIndirect) std::fprintf(stderr, "[draw]   record %u: count %u instances %u first %u vertexOffset %u startInstance %u\n", record, arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance);
            if (arguments.count == 0 || arguments.instances == 0) continue;
            std::set<std::size_t> patched;
            const auto patch = [&](const std::optional<std::pair<std::size_t, std::size_t>>& word, std::uint32_t value) {
                if (!word) return;
                programs[word->first].userData[word->second] = value;
                patched.insert(word->first);
            };
            patch(baseVertexWord, indirect.recordBytes == 20 ? arguments.vertexOffset : arguments.firstVertexOrIndex);
            patch(startInstanceWord, arguments.firstInstance);
            patch(drawIndexWord, record);
            Pm4::DrawParameters direct{0, arguments.count, 0, arguments.instances, drawParameters.flags, drawParameters.indexed, 0, 0};
            if (drawParameters.indexed) {

                if (arguments.firstVertexOrIndex >= drawParameters.indexCount) continue;
                direct.indexAddress = drawParameters.indexAddress + static_cast<std::uint64_t>(arguments.firstVertexOrIndex) * drawParameters.indexSize;
                direct.indexCount = std::min(arguments.count, drawParameters.indexCount - arguments.firstVertexOrIndex);
                direct.indexSize = drawParameters.indexSize;
            } else {
                direct.firstVertex = indirect.indxOffset;
            }
            if (graphics.stages.mesh) {
                setMeshIndexBuffer(direct);
                patched.insert(0);
            }
            for (const auto programIndex : patched) {
                auto& result = results[resultIndex[programIndex]];
                const auto pushBytes = result.pushConstants.size();
                decodeVertexInfo(programIndex);
                result = *materializeDrawStage(programIndex, pushOffsets[programIndex], queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected);
                if (!rejected.empty()) return DrawVerdict::Rejected;
                require(result.pushConstants.size() == pushBytes, "patched program changed its push constant layout");
            }
            fold(*programResults[0], direct);
            if (graphics.rectList && patched.contains(0)) buildRectList();
            snapshot();
            if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
                rejected = std::move(*known);
                return DrawVerdict::Rejected;
            }
            lockForDraw();
            noteDrawWriters(stages, submission.queue);
            phaseTiming.Phase(DrawRowVectors);
            localDevice->Draw(graphics, direct, stages, snapshots);
            phaseTiming.Phase(DrawRowGraphics);
        }
        timing.Mark("draw_and_resource_release");
        return drawn();
    }

    auto& recipeStages = scratch.recipeStages;
    if (registerKey && !drawParameters.indirect && Graphics::DrawRecipes()) {
        recipeStages.reserve(programs.size());
        for (std::size_t i = 0; i < programs.size(); ++i) recipeStages.push_back(drawHit ? matched[i] : fresh[i]);
        if (std::all_of(recipeStages.begin(), recipeStages.end(), [](const std::shared_ptr<DispatchVariant>& variant) { return variant == nullptr; })) recipeStages.clear();
    }
    std::shared_ptr<const DrawRecipe> recipe = shapeRecipe;
    if (recipe == nullptr && !recipeStages.empty() && (drawHit || !recipeShape.empty())) {
        recipe = findDrawRecipe(drawKey, recipeStages, recipeShape);
        if (recipe == nullptr) VulkanDevice::NoteDrawRecipeMiss(VulkanDevice::DrawRecipePrecheck::NoRecipe);
    }
    if (recipe == nullptr) {
        if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
            rejected = std::move(*known);
            return DrawVerdict::Rejected;
        }
    }
    if (pipeline != nullptr && !gpuLock.owns_lock()) {
        if (!deferredLabels().labels.empty()) {
            pipeline->Drain("labels");
            pipeline->NoteSyncDraw("labels");
        } else if (pipeline->Overlaps(memory)) {
            // The capture read memory a queued draw writes: capture again once it is recorded.
            pipeline->Drain("overlap");
            pipeline->NoteSyncDraw("overlap");
            return draw(queue, packet, submission, rejected);
        } else {
            DrawPipeline::Job job;
            AppendDrawWrites(graphics, stages, job.writes);
            std::shared_ptr<DrawScratch> owned(lease.Detach().release(), [](DrawScratch* released) { ScratchRecycle::Get().Give(std::unique_ptr<DrawScratch>(released)); });
            job.work = [this, owned = std::move(owned), decode, entry, localDevice, geometryHeld, recipe, drawParameters, drawKey, queueId = submission.queue, epoch = GuestMemory::CollectEpoch()]() mutable {
                auto& work = *owned;
                GuestMemory::AdoptCollectEpoch(epoch);
                const auto& state = decode->state;
                std::unique_lock lock(GuestMemory::GpuMutex(), std::defer_lock);
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
                lock.lock();
                if (auto current = device.Load(); current != nullptr && current != localDevice) localDevice = std::move(current);
                recordLabelsForPacket(localDevice.get(), queueId);
                noteDrawWriters(work.stages, queueId);
                // Counted once recorded; a full batch is submitted here, under the mutex already held.
                const auto recorded = [&] {
                    Graphics::Recorder::CountRecordedWork();
                    if (batchCap() != 0 && Graphics::Recorder::RecordedWorkSinceSubmit() >= batchCap()) localDevice->SubmitRecorded(queueId == 0);
                };
                if (recipe != nullptr) {
                    if (localDevice->DrawFromRecipe(state, drawParameters, work.stages, work.snapshots, recipe) == RecipeOutcome::Recorded) {
                        recorded();
                        return;
                    }
                    VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
                }
                std::shared_ptr<const DrawRecipe> built;
                localDevice->Draw(state, drawParameters, work.stages, work.snapshots, work.recipeStages.empty() ? nullptr : &built);
                recorded();
                lock.unlock();
                if (built != nullptr) attachDrawRecipe(drawKey, work.recipeStages, work.recipeShape, std::move(built));
            };
            pipeline->Enqueue(std::move(job));
            drawQueued() = true;
            timing.Mark("draw_queued");
            return drawn();
        }
    }
    lockForDraw();
    noteDrawWriters(stages, submission.queue);
    phaseTiming.Phase(DrawRowVectors);
    if (recipe != nullptr) {
        if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe) == RecipeOutcome::Recorded) {
            phaseTiming.Phase(DrawRowGraphics);
            timing.Mark("draw_and_resource_release");
            return drawn();
        }

        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
    }
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built);
    phaseTiming.Phase(DrawRowGraphics);
    if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, recipeShape, std::move(built));
    timing.Mark("draw_and_resource_release");
    return drawn();
}

}
