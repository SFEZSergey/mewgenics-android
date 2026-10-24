#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include <algorithm>
#include <cstdlib>
#include <string>

namespace AgcDriver::DriverDetail {
namespace {

// Queued jobs hold their draw's prepared data; past this many the worker waits for the recorder.
constexpr std::size_t MaxQueuedJobs = 32;

// APS5_RECORDER_SPIN_US (default 20, 0 = off): how long the recorder polls for the next job
// before it sleeps. The worker queues a draw every few tens of microseconds in combat, and a
// recorder asleep costs a futex wake on the worker (~3% of it) and a wake-up latency on its own side.
std::chrono::nanoseconds RecorderSpin() {
    static const auto spin = [] {
        const char* value = std::getenv("APS5_RECORDER_SPIN_US");
        return std::chrono::nanoseconds(static_cast<long long>(value != nullptr ? std::strtoul(value, nullptr, 10) : 20ul) * 1000);
    }();
    return spin;
}

inline void SpinPause() {
#if defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#elif defined(__x86_64__)
    __builtin_ia32_pause();
#endif
}

bool Profiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

void Count(std::vector<std::pair<const char*, std::uint64_t>>& rows, const char* reason) {
    for (auto& row : rows) {
        if (row.first == reason) {
            ++row.second;
            return;
        }
    }
    rows.emplace_back(reason, 1);
}

}

DrawPipeline::DrawPipeline(std::uint32_t queue, std::function<void()> setup) : queue(queue), setup(std::move(setup)) {
    thread = std::thread([this] { run(); });
}

DrawPipeline::~DrawPipeline() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    wake.notify_all();
    if (thread.joinable()) thread.join();
}

void DrawPipeline::run() {
    if (setup) setup();
    std::unique_lock lock(mutex);
    for (;;) {
        if (!stopping && jobs.empty() && RecorderSpin().count() != 0) {
            // `pending` counts the queued jobs here (this recorder's last one is retired): the
            // poll reads it without the mutex, the decision below is made under it again.
            lock.unlock();
            const auto until = std::chrono::steady_clock::now() + RecorderSpin();
            for (std::uint32_t i = 1; pending.load(std::memory_order_acquire) == 0; ++i) {
                SpinPause();
                if ((i & 63u) == 0 && std::chrono::steady_clock::now() >= until) break;
            }
            lock.lock();
        }
        if (!stopping && jobs.empty()) {
            recorderWaiting = true;
            wake.wait(lock, [&] { return stopping || !jobs.empty(); });
            recorderWaiting = false;
        }
        if (jobs.empty()) return;
        auto job = std::move(jobs.front());
        jobs.pop_front();
        running = std::move(job.writes);
        lock.unlock();
        const bool timed = Profiled();
        const auto start = timed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        std::exception_ptr error;
        try {
            job.work();
        } catch (...) {
            error = std::current_exception();
        }
        job.work = nullptr;
        if (timed) recordedNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
        lock.lock();
        running.clear();
        if (error != nullptr && failure == nullptr) failure = error;
        pending.fetch_sub(1, std::memory_order_acq_rel);
        if ((waitingForRoom && jobs.size() <= MaxQueuedJobs / 2) || (waitingForDrain && jobs.empty()) || failure != nullptr) done.notify_all();
    }
}

void DrawPipeline::Enqueue(Job job) {
    std::unique_lock lock(mutex);
    if (failure != nullptr) std::rethrow_exception(failure);
    if (jobs.size() >= MaxQueuedJobs) {
        const auto start = Profiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        waitingForRoom = true;
        done.wait(lock, [&] { return jobs.size() <= MaxQueuedJobs / 2 || failure != nullptr; });
        waitingForRoom = false;
        if (failure != nullptr) std::rethrow_exception(failure);
        if (Profiled()) {
            ++fullWaits;
            fullWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    }
    jobs.push_back(std::move(job));
    pending.fetch_add(1, std::memory_order_acq_rel);
    ++enqueued;
    // A sleeping recorder is woken at once: waking it only once four jobs waited (to save the futex
    // wake per draw) left it idle while the worker queued, and a drain then waited for all of them
    // serially (menu 51 -> 47 FPS).
    const bool signal = recorderWaiting;
    lock.unlock();
    if (signal) wake.notify_one();
    if (Profiled()) report();
}

void DrawPipeline::Drain(const char* reason) {
    std::unique_lock lock(mutex);
    ++drains;
    if (pending.load(std::memory_order_acquire) != 0) {
        if (recorderWaiting && !jobs.empty()) wake.notify_one();
        const auto start = Profiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        waitingForDrain = true;
        done.wait(lock, [&] { return pending.load(std::memory_order_acquire) == 0; });
        waitingForDrain = false;
        if (Profiled()) {
            ++drainsWaited;
            drainWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            Count(drainReasons, reason);
        }
    }
    if (failure != nullptr) {
        auto error = failure;
        failure = nullptr;
        std::rethrow_exception(error);
    }
}

bool DrawPipeline::Overlaps(std::span<const ShaderRecompiler::MemoryRegion> regions) {
    if (Idle() || regions.empty()) return false;
    // The draw's regions sorted by start, with the running maximum of their ends: one write range is
    // then a binary search instead of a pass over every region (every queued write x every region
    // per draw was ~4% of the worker on UI screens).
    auto& sorted = overlapScratch;
    sorted.clear();
    for (const auto& region : regions) sorted.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 1; i < sorted.size(); ++i) sorted[i].second = std::max(sorted[i].second, sorted[i - 1].second);
    const auto hit = [&](std::uint64_t begin, std::uint64_t end) {
        // Regions starting before `end`; the latest end among them decides.
        const auto after = std::lower_bound(sorted.begin(), sorted.end(), end, [](const auto& region, std::uint64_t value) { return region.first < value; });
        return after != sorted.begin() && std::prev(after)->second > begin;
    };
    std::lock_guard lock(mutex);
    const auto overlaps = [&](const std::vector<Range>& writes) {
        for (const auto& [begin, end] : writes) {
            if (hit(begin, end)) return true;
        }
        return false;
    };
    if (overlaps(running)) return true;
    return std::any_of(jobs.begin(), jobs.end(), [&](const Job& job) { return overlaps(job.writes); });
}

void DrawPipeline::NoteSyncDraw(const char* reason) {
    if (!Profiled()) return;
    std::lock_guard lock(mutex);
    Count(syncReasons, reason);
}

void DrawPipeline::report() {
    std::lock_guard lock(mutex);
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport < std::chrono::seconds(10)) return;
    const auto seconds = std::chrono::duration<double>(now - lastReport).count();
    lastReport = now;
    std::string drainText, syncText;
    for (const auto& [reason, count] : drainReasons) drainText += std::string(" ") + reason + " " + std::to_string(count);
    for (const auto& [reason, count] : syncReasons) syncText += std::string(" ") + reason + " " + std::to_string(count);
    const auto recorded = static_cast<double>(recordedNs.exchange(0, std::memory_order_relaxed)) / 1e6;
    AgcDriver::ProfilePrint_nid_no_patch("[pipeline] queue 0x%x (%.0f s): %llu draws to the recorder (busy %.0f ms, %.0f%%), %llu drains (%llu waited %.1f ms; by reason:%s), full queue %llu x %.1f ms; draws kept on the worker:%s\n",
        queue, seconds, static_cast<unsigned long long>(enqueued), recorded, recorded / (seconds * 10.0), static_cast<unsigned long long>(drains), static_cast<unsigned long long>(drainsWaited), drainWaitMs, drainText.empty() ? " none" : drainText.c_str(),
        static_cast<unsigned long long>(fullWaits), fullWaitMs, syncText.empty() ? " none" : syncText.c_str());
    enqueued = drains = drainsWaited = fullWaits = 0;
    drainWaitMs = fullWaitMs = 0;
    drainReasons.clear();
    syncReasons.clear();
}

}
