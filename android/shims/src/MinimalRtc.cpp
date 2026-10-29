#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <ctime>

#define APS5_VABI __attribute__((visibility("default")))

namespace {
constexpr std::int64_t TICKS_PER_SECOND = 1000000;
constexpr std::int64_t TICKS_PER_MINUTE = 60 * TICKS_PER_SECOND;
constexpr std::int64_t TICKS_PER_HOUR = 60 * TICKS_PER_MINUTE;
constexpr std::int64_t TICKS_PER_DAY = 24 * TICKS_PER_HOUR;
constexpr std::int64_t UNIX_EPOCH_DAYS = 719162;
constexpr std::uint64_t UNIX_EPOCH_TICK = UNIX_EPOCH_DAYS * TICKS_PER_DAY;
}

struct SceRtcTick {
    std::uint64_t tick;
};

struct SceRtcDateTime {
    std::uint16_t year;
    std::uint16_t month;
    std::uint16_t day;
    std::uint16_t hour;
    std::uint16_t minute;
    std::uint16_t second;
    std::uint32_t microsecond;
};

extern "C" {

APS5_VABI int sceRtcGetCurrentTick(SceRtcTick* tick) {
    if (!tick) return -1;
    const auto now = std::chrono::system_clock::now();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    tick->tick = UNIX_EPOCH_TICK + us;
    return 0;
}

APS5_VABI int sceRtcGetCurrentClock(SceRtcDateTime* time, int tz) {
    if (!time) return -1;
    const auto now = std::time(nullptr);
    std::tm tm_buf{};
    gmtime_r(&now, &tm_buf);
    time->year = tm_buf.tm_year + 1900;
    time->month = tm_buf.tm_mon + 1;
    time->day = tm_buf.tm_mday;
    time->hour = tm_buf.tm_hour;
    time->minute = tm_buf.tm_min;
    time->second = tm_buf.tm_sec;
    time->microsecond = 0;
    return 0;
}

APS5_VABI int sceRtcGetCurrentClockLocalTime(SceRtcDateTime* time) {
    if (!time) return -1;
    const auto now = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&now, &tm_buf);
    time->year = tm_buf.tm_year + 1900;
    time->month = tm_buf.tm_mon + 1;
    time->day = tm_buf.tm_mday;
    time->hour = tm_buf.tm_hour;
    time->minute = tm_buf.tm_min;
    time->second = tm_buf.tm_sec;
    time->microsecond = 0;
    return 0;
}

APS5_VABI int sceRtcConvertUtcToLocalTime(const SceRtcTick* utc, SceRtcTick* local) {
    if (!utc || !local) return -1;
    local->tick = utc->tick;
    return 0;
}

APS5_VABI int sceRtcConvertLocalTimeToUtc(const SceRtcTick* local, SceRtcTick* utc) {
    if (!utc || !local) return -1;
    utc->tick = local->tick;
    return 0;
}

}
