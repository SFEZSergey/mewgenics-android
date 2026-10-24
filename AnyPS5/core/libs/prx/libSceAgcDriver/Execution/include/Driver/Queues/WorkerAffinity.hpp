#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WORKERAFFINITY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WORKERAFFINITY_HPP

#include <cstdint>

namespace AgcDriver::DriverDetail {

std::uint64_t WorkerAffinityMask();

void PinWorkerThread(const char* role);
// Display priority only, no affinity: for helper threads the scheduler may place on any fast core.
void PrioritizeWorkerThread(const char* role);

}

#endif
