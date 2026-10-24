#include <cstdio>
#include <chrono>
#include <atomic>
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <cstdlib>

// APS5_PROFILE_DRAW: фазы вывода кадра (мс на кадр, раз в 10 с) — сколько поток вывода держит
// и ждёт GPU-мьютекс.
namespace PresentPhases {
enum Phase { Acquire, LockWait, Held, Blit, Queue, Setup, SetupHeld, Resizes, Count };
inline std::atomic<std::uint64_t> totals[Count]{};
inline std::atomic<std::uint64_t> frames{0};
inline bool Enabled() {
    static const bool enabled = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return enabled;
}
inline void Add(Phase phase, std::chrono::steady_clock::time_point start) {
    if (!Enabled()) return;
    totals[phase].fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
}
inline void Report() {
    if (!Enabled()) return;
    static std::atomic<std::int64_t> last{0};
    frames.fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = last.load();
    if (previous == 0) { last.compare_exchange_strong(previous, now); return; }
    if (now - previous < 10 || !last.compare_exchange_strong(previous, now)) return;
    const double count = static_cast<double>(std::max<std::uint64_t>(1, frames.exchange(0)));
    const auto ms = [&](Phase phase) { return static_cast<double>(totals[phase].exchange(0)) / 1e6 / count; };
    std::fprintf(stderr, "[present-phases] per frame: setup %.2f ms (held %.3f, resizes %.0f in 10 s), acquire %.2f ms, lock wait %.2f ms, held %.2f ms (blit %.2f, queue present %.2f)\n", ms(Setup), ms(SetupHeld), static_cast<double>(totals[Resizes].exchange(0)), ms(Acquire), ms(LockWait), ms(Held), ms(Blit), ms(Queue));
}
}

namespace AgcDriver::DriverDetail {

void Driver::RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    require(output != nullptr, "null video output");
    std::lock_guard lock(mutex);
    rethrowFailure();
    checkStopping();
    require(outputs.emplace(handle, output).second, "video output already registered");
}

void Driver::UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    std::lock_guard lock(mutex);
    const auto it = outputs.find(handle);
    require(it != outputs.end() && it->second == output, "video output registration mismatch");
    outputs.erase(it);
}

void Driver::AttachWindow(const PresentationWindow& window) {
    CheckFailure();
    std::unique_lock replacing(deviceReplacement);
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
    std::lock_guard lock(GuestMemory::GpuMutex());
    if (device) {
        require(device->Window() == nullptr, "a window is already attached to the device");
        device->PrepareForReplacement();
        replacedDevices.push_back(device);
    }
    device = std::make_shared<VulkanDevice>(&window);
}

void Driver::Present(const PresentationWindow& window, const DisplayBuffer* buffer, bool opaque, void (*gpuReady)(void*), void* context) {

    PerformanceContext timingContext(window.timing.get());
    PerformanceTimer timing("Driver.Present");
    static thread_local bool pinned = false;
    if (!pinned) {
        pinned = true;
        PinWorkerThread("presenter");
        GuestMemory::MarkPresenterThread();
    }
    CheckFailure();
    require(gpuReady != nullptr && context != nullptr, "missing GPU completion callback");
    require(window.getDrawableSize != nullptr, "missing window drawable size query");

    static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
    static const std::size_t inFlight = VulkanDevice::FlipInFlight();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::shared_ptr<VulkanDevice> presenting;
    timing.Mark("validate");
    try {
        bool submitted = false;
        bool presentable = false;
        bool trailing = false;
        double waitedMs = 0;
        {
            // The GPU mutex only guards taking the device; the drawable size is read outside it and
            // the surface is resized under it only when something changed (the surface state is the
            // presenter's own). Every frame used to hold the mutex here, stalling the queue worker.
            // APS5_PRESENT_SETUP_LOCKED=1: the whole setup under the mutex, as before.
            static const bool setupLocked = std::getenv("APS5_PRESENT_SETUP_LOCKED") != nullptr;
            const auto setupStart = std::chrono::steady_clock::now();
            std::unique_lock<GuestMemory::GpuMutexType> lock;
            {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                lock = std::unique_lock<GuestMemory::GpuMutexType>(GuestMemory::GpuMutex());
                const auto heldFrom = std::chrono::steady_clock::now();
                timing.Mark("gpu_mutex_wait");
                require(device != nullptr && device->Window() == window.context, "presentation window is not attached to the device");
                presenting = device;
                if (!setupLocked) {
                    lock.unlock();
                    PresentPhases::Add(PresentPhases::SetupHeld, heldFrom);
                }
            }
            timing.Mark("device_setup");
            std::uint32_t drawableWidth = 0;
            std::uint32_t drawableHeight = 0;
            window.getDrawableSize(window.context, &drawableWidth, &drawableHeight);
            if (setupLocked || presenting->NeedsResize(drawableWidth, drawableHeight)) {
                if (!lock.owns_lock()) {
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                    lock.lock();
                }
                presenting->Resize(drawableWidth, drawableHeight);
                if (PresentPhases::Enabled()) PresentPhases::totals[PresentPhases::Resizes].fetch_add(1, std::memory_order_relaxed);
            }
            if (lock.owns_lock()) lock.unlock();
            timing.Mark("resize");
            presentable = presenting->Presentable();
            if (buffer != nullptr) require(buffer->width == window.width && buffer->height == window.height, "display buffer extent differs from output");
            PresentPhases::Add(PresentPhases::Setup, setupStart);
        }

        if (presentable && !syncFlip) {
            if (inFlight != 0) {

                waitedMs = presenting->RetirePresents(presenting->PresentWaitsForSlots(buffer) ? 0 : inFlight);
                timing.Mark("inflight_wait");
            }
            const auto acquireStart = std::chrono::steady_clock::now();
            presentable = presenting->AcquireImage();
            PresentPhases::Add(PresentPhases::Acquire, acquireStart);
            timing.Mark("acquire_image");
        }
        if (presentable) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
            const auto lockStart = std::chrono::steady_clock::now();
            std::lock_guard lock(GuestMemory::GpuMutex());
            PresentPhases::Add(PresentPhases::LockWait, lockStart);
            const auto heldStart = std::chrono::steady_clock::now();
            struct HeldScope {
                std::chrono::steady_clock::time_point start;
                ~HeldScope() { PresentPhases::Add(PresentPhases::Held, start); }
            } held{heldStart};
            timing.Mark("gpu_mutex_wait");
            if (buffer != nullptr) {
                if (syncFlip) {
                    presenting->WaitIdle();
                    timing.Mark("device_idle_wait");
                }
                const auto blitStart = std::chrono::steady_clock::now();
                submitted = presenting->PresentDisplayBuffer(*buffer);
                PresentPhases::Add(PresentPhases::Blit, blitStart);
                timing.Mark("present_display_buffer");
            } else {
                submitted = presenting->PresentClear(window.width, window.height, opaque);
                timing.Mark("present_clear");
            }
            if (submitted && syncFlip) {
                waitedMs = presenting->FinishPresent();
                timing.Mark("render_fence_wait");
            }
            if (submitted && (syncFlip || inFlight != 0)) {
                const auto queueStart = std::chrono::steady_clock::now();
                presenting->QueuePresent();
                PresentPhases::Add(PresentPhases::Queue, queueStart);
                timing.Mark("queue_present");
                trailing = !syncFlip;
                submitted = false;
            }
        }
        if (trailing) {
            waitedMs += presenting->FinishPresent();
            timing.Mark("render_fence_wait");
        }
        if (submitted) {
            waitedMs = presenting->FinishPresent();
            timing.Mark("render_fence_wait");

            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
            std::lock_guard lock(GuestMemory::GpuMutex());
            presenting->QueuePresent();
            timing.Mark("queue_present");
        }
        gpuReady(context);
        PresentPhases::Report();
        timing.Mark("release_and_callback");
        if (profile) reportPresents(waitedMs, inFlight);
        CheckFailure();
    } catch (const ProcessShutdown&) {
        throw;
    } catch (...) {
        ReportFailure(std::current_exception());
        throw;
    }
}

void Driver::reportPresents(double waitedMs, std::size_t inFlight) {
    static std::vector<double> waits;
    static VulkanDevice::PresentStatistics previous{};
    static std::uint64_t previousFlips = 0, windowSerial = 0, previousUnsignaled = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    waits.push_back(waitedMs);
    if (waits.size() == 1) windowSerial = flipSerial.load();
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    std::sort(waits.begin(), waits.end());
    const auto percentile = [&](double fraction) { return waits.empty() ? 0.0 : waits[std::min(waits.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(waits.size())))]; };
    const auto counts = VulkanDevice::PresentCounts();
    const auto presents = counts.presents - previous.presents;
    const double perPresent = presents != 0 ? 1.0 / static_cast<double>(presents) : 0.0;
    const auto flips = flipsCounted.load() - previousFlips;
    const double perFlip = flips != 0 ? 1.0 / static_cast<double>(flips) : 0.0;
    const auto serial = flipSerial.load();
    AgcDriver::ProfilePrint_nid_no_patch( "[present] %llu presents over 10 s (%zu may trail on the GPU); %s p50 %.2f ms, p90 %.2f ms, max %.2f ms over %zu; per present: GPU busy %.1f ms, idle gaps %.1f ms (%llu batches without a completion record); per flip: %.1f batches recorded (%.1f unsignaled at the flip); per present: %.1f batches ahead of the blit, %.1f after it; after the flip packet: last submit %.1f ms, blit submit %.1f ms; in-place reads overwritten before execution: %llu of %llu checked\n", static_cast<unsigned long long>(presents), inFlight, inFlight != 0 ? "inflight_wait" : "render_fence_wait", percentile(0.5), percentile(0.9), waits.empty() ? 0.0 : waits.back(), waits.size(), (counts.gpuBusyMs - previous.gpuBusyMs) * perPresent, (counts.gpuGapMs - previous.gpuGapMs) * perPresent, static_cast<unsigned long long>(counts.gpuUnread - previous.gpuUnread), static_cast<double>(serial - windowSerial) * perFlip, static_cast<double>(flipBatchesUnsignaled.load() - previousUnsignaled) * perFlip, static_cast<double>(counts.batchesAheadOfBlit - previous.batchesAheadOfBlit) * perPresent, static_cast<double>(counts.batchesAfterFlip - previous.batchesAfterFlip) * perPresent, (counts.lastSubmitAfterFlipMs - previous.lastSubmitAfterFlipMs) * perPresent, (counts.blitSubmitAfterFlipMs - previous.blitSubmitAfterFlipMs) * perPresent, static_cast<unsigned long long>(counts.readsOverwritten - previous.readsOverwritten), static_cast<unsigned long long>(counts.readsChecked - previous.readsChecked));
    previous = counts;
    previousFlips = flipsCounted.load();
    previousUnsignaled = flipBatchesUnsignaled.load();
    waits.clear();
}

void Driver::ReleaseWindow(void* window) {
    std::lock_guard lock(GuestMemory::GpuMutex());
    if (device && device->Window() == window) device.Reset();
}

}
