#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <utility>

namespace {
std::mutex mouseMutex;
std::array<MouseData, MOUSE_MAX_DATA_NUM> history{};
int head = 0;
int count = 0;
bool initialized = false;
bool opened = false;
// Android: the screen's touches drive the pad (touch overlay); a mouse is reported only once a real
// one is used (the video-out input layer connects it), so the game sees no second input device.
#ifdef __ANDROID__
bool connected = false;
#else
bool connected = true;
#endif
std::uint32_t buttons = 0;
// The absolute-position mode: the relative motion the host reports moves a cursor over the
// virtual screen the game picks (deltas arrive in 1920-wide units, the touch layer's scale); the
// wheel accumulates between absolute reads. PPSA37318 picks size 3 and recentres the cursor on
// (1920, 1080) after every read, so 3 is taken as 3840x2160, also the default.
double absoluteX = 1920.0;
double absoluteY = 1080.0;
std::int32_t screenWidth = 3840;
std::int32_t screenHeight = 2160;
std::int32_t absoluteWheel = 0;
// Buttons pressed since the last absolute read: a touch tap presses and releases between two reads
// of the game, which sees only the current state; the press is reported by the next read anyway.
std::uint32_t pressedSinceRead = 0;
std::int32_t absoluteTilt = 0;

void clampAbsolute() {
    absoluteX = std::clamp(absoluteX, 0.0, static_cast<double>(screenWidth - 1));
    absoluteY = std::clamp(absoluteY, 0.0, static_cast<double>(screenHeight - 1));
}

void enqueue(const MouseData& data) {
    if (count == MOUSE_MAX_DATA_NUM) {
        head = (head + 1) % MOUSE_MAX_DATA_NUM;
        --count;
    }
    history[(head + count) % MOUSE_MAX_DATA_NUM] = data;
    ++count;
}
}

namespace Mouse {

int Initialize() {
    std::lock_guard lock(mouseMutex);
    initialized = true;
    return MOUSE_OK;
}

int Open(int userId, std::int32_t type, std::int32_t index, const MouseOpenParam* param) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (userId < 0 || type != 0 || index != 0 ||
        (param != nullptr && (param->behaviorFlag & ~MOUSE_OPEN_PARAM_MERGED) != 0)) {
        return MOUSE_ERROR_INVALID_ARG;
    }
    if (opened) return MOUSE_ERROR_ALREADY_OPENED;
    opened = true;
    buttons = pressedSinceRead = 0;
    head = 0;
    count = 0;
    MouseData initial{};
    initial.timestamp = sceKernelGetProcessTime();
    initial.connected = connected;
    enqueue(initial);
    return MOUSE_HANDLE;
}

int Close(std::int32_t handle) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    opened = false;
    buttons = pressedSinceRead = 0;
    head = 0;
    count = 0;
    return MOUSE_OK;
}

int Read(std::int32_t handle, MouseData* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0 || num > MOUSE_MAX_DATA_NUM) return MOUSE_ERROR_INVALID_ARG;
    const int available = std::min(count, num);
    for (int i = 0; i < available; ++i) {
        data[i] = history[head];
        head = (head + 1) % MOUSE_MAX_DATA_NUM;
    }
    count -= available;
    return available;
}

int CheckHandle(std::int32_t handle) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    return MOUSE_OK;
}

int SetAbsoluteScreen(std::int32_t handle, std::int32_t size) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    // The sizes by index, guessed from the PS5 output resolutions (only 3 is seen in a game).
    constexpr std::int32_t widths[] = {1280, 1920, 2560, 3840};
    constexpr std::int32_t heights[] = {720, 1080, 1440, 2160};
    if (size < 0 || size > 3) return MOUSE_ERROR_INVALID_ARG;
    const auto width = widths[size];
    const auto height = heights[size];
    absoluteX = absoluteX * width / screenWidth;
    absoluteY = absoluteY * height / screenHeight;
    screenWidth = width;
    screenHeight = height;
    clampAbsolute();
    return MOUSE_OK;
}

int ResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    absoluteX = x;
    absoluteY = y;
    clampAbsolute();
    return MOUSE_OK;
}

int ReadAbsolute(std::int32_t handle, MouseAbsoluteData* data, std::int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return MOUSE_ERROR_NOT_INITIALIZED;
    if (handle != MOUSE_HANDLE || !opened) return MOUSE_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0 || num > MOUSE_MAX_DATA_NUM) return MOUSE_ERROR_INVALID_ARG;
    // The current state as one sample (the cursor has no history to replay).
    MouseAbsoluteData sample{};
    sample.timestamp = sceKernelGetProcessTime();
    sample.connected = connected ? 1u : 0u;
    sample.buttons = connected ? (buttons | std::exchange(pressedSinceRead, 0u)) : 0u;
    sample.x = static_cast<std::uint16_t>(absoluteX);
    sample.y = static_cast<std::uint16_t>(absoluteY);
    sample.wheel = std::exchange(absoluteWheel, 0);
    sample.tilt = std::exchange(absoluteTilt, 0);
    data[0] = sample;
    return 1;
}

void Publish(const MouseInputEvent& event) {
    std::lock_guard lock(mouseMutex);
    if (event.connectionChange) {
        if (connected == event.connected) return;
        connected = event.connected;
        if (!connected) buttons = pressedSinceRead = 0;
    } else if (!connected) {
        return;
    }
    if (!opened) return;
    absoluteX += event.x * static_cast<double>(screenWidth) / 1920.0;
    absoluteY += event.y * static_cast<double>(screenHeight) / 1080.0;
    clampAbsolute();
    absoluteWheel += event.wheel;
    absoluteTilt += event.tilt;
    if (event.resetButtons) buttons = pressedSinceRead = 0;
    if (event.button != 0) {
        if (event.pressed) {
            buttons |= event.button;
            pressedSinceRead |= event.button;
        }
        else buttons &= ~event.button;
    }
    MouseData data{};
    data.timestamp = sceKernelGetProcessTime();
    data.connected = connected;
    data.buttons = buttons;
    data.x_axis = event.x;
    data.y_axis = event.y;
    data.wheel = event.wheel;
    data.tilt = event.tilt;
    enqueue(data);
}

}
