#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include <cstdlib>
#ifdef __ANDROID__
#include <cerrno>
#include <cstdio>
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace AgcDriver::DriverDetail {

std::uint64_t WorkerAffinityMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_WORKER_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
        if (requested != 0) return requested;
#ifndef __linux__
        if (std::getenv("APS5_WORKER_AFFINITY") == nullptr) return std::uint64_t{0};
#endif
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.performant : std::uint64_t{0};
    }();
    return mask;
}

void PinWorkerThread(const char* role) {
    PrioritizeWorkerThread(role);
    const auto mask = WorkerAffinityMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] queue workers and presenter -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(role, nullptr, mask);
}

void PrioritizeWorkerThread(const char* role) {
#ifdef __ANDROID__
    // A readable thread name for profilers (top, simpleperf): "agc-" and the role, 15 characters.
    {
        char name[16];
        std::snprintf(name, sizeof name, "agc-%s", role);
        for (char* c = name; *c != '\0'; ++c) {
            if (*c == ' ') *c = '-';
        }
        pthread_setname_np(pthread_self(), name);
    }
    // The queue workers and the presenter are the frame's critical path; at the default priority the
    // game's own threads preempted them (a presenter preempted under the GPU mutex stalled the queue
    // worker behind it). Display priority, as Android games use (RLIMIT_NICE allows it).
    // APS5_WORKER_NICE sets the value (0: unchanged).
    static const int nice = [] {
        const char* value = std::getenv("APS5_WORKER_NICE");
        return value != nullptr ? std::atoi(value) : -8;
    }();
    if (nice != 0 && setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), nice) != 0) {
        std::fprintf(stderr, "[affinity] %s: priority %d refused (errno %d)\n", role, nice, errno);
    }
#else
    static_cast<void>(role);
#endif
}

}
