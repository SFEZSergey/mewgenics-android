#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPIPELINE_HPP

#include "Recompiler.hpp"
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

// Draw pipeline (APS5_PIPELINE_DRAWS): the queue worker prepares a draw (decode, shader lookup,
// captures — the part that already runs without the GPU mutex) and hands the rest (GPU mutex,
// Graphics::Draw: resources, descriptors, command recording) to a recorder thread, then goes on with
// the next packets while the previous draw is recorded. Jobs run strictly in order on one thread.
// The worker drains the pipeline before anything that is not a register write or a plain draw, so
// labels, waits, dispatches, copies and flips see every earlier draw recorded, as before.
class DrawPipeline {
public:
    using Range = std::pair<std::uint64_t, std::uint64_t>;
    struct Job {
        std::function<void()> work;
        // Guest memory the draw may write (storage buffers, color and depth targets): a later draw
        // whose captured memory overlaps it waits for the pipeline first.
        std::vector<Range> writes;
    };

    // `setup` runs once on the recorder thread before its first job.
    DrawPipeline(std::uint32_t queue, std::function<void()> setup);
    ~DrawPipeline();
    DrawPipeline(const DrawPipeline&) = delete;
    DrawPipeline& operator=(const DrawPipeline&) = delete;

    void Enqueue(Job job);
    // Waits until every queued job has run; rethrows the first failure of a job.
    void Drain(const char* reason);
    bool Idle() const { return pending.load(std::memory_order_acquire) == 0; }
    bool Overlaps(std::span<const ShaderRecompiler::MemoryRegion> regions);
    void NoteSyncDraw(const char* reason);

private:
    void run();
    void report();

    std::uint32_t queue;
    std::function<void()> setup;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable done;
    std::deque<Job> jobs;
    std::vector<Range> running;
    std::atomic<std::uint64_t> pending{0};
    bool stopping = false;
    // Who sleeps on the condition variables (under `mutex`): a notify is a futex syscall per draw
    // otherwise, a tenth of the recorder's time.
    bool recorderWaiting = false;
    // The worker waits either for room (a full queue: woken once half of it is recorded, not at
    // every job) or for the queue to empty (a drain).
    bool waitingForRoom = false;
    bool waitingForDrain = false;
    std::exception_ptr failure;
    std::thread thread;
    // Overlaps' sorted copy of the draw's regions (the worker is its only caller).
    std::vector<Range> overlapScratch;

    // APS5_PROFILE_DRAW statistics (10 s).
    std::uint64_t enqueued = 0, drains = 0, drainsWaited = 0, fullWaits = 0;
    double drainWaitMs = 0, fullWaitMs = 0;
    std::atomic<std::uint64_t> recordedNs{0};
    std::vector<std::pair<const char*, std::uint64_t>> drainReasons, syncReasons;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

}

#endif
