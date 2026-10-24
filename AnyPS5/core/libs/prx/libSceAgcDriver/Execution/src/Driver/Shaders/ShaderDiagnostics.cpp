#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <string_view>

namespace AgcDriver::DriverDetail {

bool Driver::skipFailedShaders() {
    static const bool skip = [] { const char* text = std::getenv("APS5_SKIP_FAILED_SHADERS"); return text != nullptr && text[0] != '\0' && text[0] != '0'; }();
    return skip;
}

namespace {
std::mutex refusedMutex;
std::unordered_map<std::uint64_t, std::uint32_t> refused;
std::atomic<bool> anyRefused{false};
// Every ~10 s with any activity, one line in the log: how much refused work was left out by the memo, how much
// was refused anew and how many repeats were attempted again (the memo works when "left out" dominates).
std::uint64_t leftOut = 0, refusedAnew = 0, retried = 0;
std::chrono::steady_clock::time_point refusalReportAt{};

void reportRefusals() {
    const auto now = std::chrono::steady_clock::now();
    if (refusalReportAt == std::chrono::steady_clock::time_point{}) refusalReportAt = now;
    if (now - refusalReportAt < std::chrono::seconds(10)) return;
    if (leftOut + refusedAnew + retried != 0) {
        std::fprintf(stderr, "[gpu] refused work in %.0f s: %llu left out by the memo, %llu refused anew, %llu tried again; %zu keys\n",
            std::chrono::duration<double>(now - refusalReportAt).count(), static_cast<unsigned long long>(leftOut), static_cast<unsigned long long>(refusedAnew), static_cast<unsigned long long>(retried), refused.size());
    }
    leftOut = refusedAnew = retried = 0;
    refusalReportAt = now;
}
}

bool Driver::skipRefused(std::uint64_t key) {
    if (key == 0 || !anyRefused.load(std::memory_order_relaxed)) return false;
    std::lock_guard lock(refusedMutex);
    const auto found = refused.find(key);
    if (found == refused.end()) return false;
    const bool skip = ++found->second % 256u != 0u;
    ++(skip ? leftOut : retried);
    reportRefusals();
    return skip;
}

void Driver::noteRefused(std::uint64_t key) {
    if (key == 0) return;
    std::lock_guard lock(refusedMutex);
    if (refused.size() >= 4096) refused.clear();
    refused.try_emplace(key, 0u);
    ++refusedAnew;
    reportRefusals();
    anyRefused.store(true, std::memory_order_relaxed);
}

void Driver::forgetRefused(std::uint64_t key) {
    if (key == 0 || !anyRefused.load(std::memory_order_relaxed)) return;
    std::lock_guard lock(refusedMutex);
    refused.erase(key);
}

std::uint64_t& Driver::attemptedDrawKey() {
    thread_local std::uint64_t key = 0;
    return key;
}

void Driver::reportSkip(const char* kind, const std::string& what) {
    static std::mutex reportedMutex;
    static std::set<std::size_t> reported;
    static std::map<std::string, int> requestDumps;

    static const bool traceSkips = std::getenv("APS5_TRACE_SKIPS") != nullptr;
    static const int dumpLimit = [] { const char* text = std::getenv("APS5_SKIP_REQUEST_DUMPS"); return text ? std::atoi(text) : 8; }();
    static constexpr std::string_view marker = "\nRecompileRequest:\n";
    std::lock_guard lock(reportedMutex);
    std::string line;
    static bool announced = false;
    if (!announced) {
        announced = true;
        char text[160];
        std::snprintf(text, sizeof(text), "[gpu] skip reports: one write each, RecompileRequest dumps capped at %d per reason (APS5_SKIP_REQUEST_DUMPS)\n", dumpLimit);
        line = text;
    }
    const std::string prefix = "[gpu] skipped " + std::string(kind);
    if (reported.insert(std::hash<std::string>{}(what)).second) {
        const auto request = what.find(marker);
        if (request == std::string::npos) {
            line += prefix + ": " + what + "\n";
        } else {
            const auto reason = what.substr(0, request);
            const auto suffix = what.find(' ', request + marker.size());
            auto& dumps = requestDumps[reason];
            if (dumpLimit < 0 || dumps < dumpLimit) {
                ++dumps;
                line += prefix + ": " + what + "\n";
            } else {
                if (dumps == dumpLimit) {
                    ++dumps;
                    line += prefix + ": further RecompileRequest dumps for \"" + reason + "\" are left out\n";
                }
                line += prefix + ": " + reason + " (request left out)" + (suffix == std::string::npos ? std::string() : what.substr(suffix)) + "\n";
            }
        }
    } else if (traceSkips) {
        line += prefix + " again: " + what.substr(0, 100) + "\n";
    }
    if (!line.empty()) std::fwrite(line.data(), 1, line.size(), stderr);
}

std::string Driver::dumpRequest(std::uint64_t address, const ShaderRecompiler::RecompileRequest& request) {
    static std::mutex dumpMutex;
    static std::set<std::uint64_t> dumped;
    char name[64];
    std::snprintf(name, sizeof(name), "shader_%llx.req", static_cast<unsigned long long>(address));
    std::lock_guard lock(dumpMutex);
    if (!dumped.insert(address).second) return name;
    try {
        const auto text = ShaderRecompiler::RequestSerializer{}.Serialize(request);
        if (std::FILE* file = std::fopen(name, "wb")) {
            std::fwrite(text.data(), 1, text.size(), file);
            std::fclose(file);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[gpu] could not serialize request for 0x%llx: %s\n", static_cast<unsigned long long>(address), error.what());
    }
    return name;
}

}
