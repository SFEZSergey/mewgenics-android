#include <cstdint>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <time.h>
#include <dlfcn.h>
#include <cmath>

#define APS5_VABI __attribute__((visibility("default")))

extern "C" {

uint64_t __attribute__((weak)) sceKernelGetProcessTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

struct MouseData {
    uint64_t timestamp;
    uint8_t connected;
    uint8_t pad[3];
    uint32_t buttons;
    int32_t x_axis;
    int32_t y_axis;
    int32_t wheel;
    int32_t tilt;
    uint8_t reserved[8];
};

struct MouseAbsoluteData {
    uint64_t timestamp;
    uint8_t connected;
    uint8_t pad[3];
    uint32_t buttons;
    uint16_t x;
    uint16_t y;
    int32_t wheel;
    int32_t tilt;
    uint8_t reserved[12];
};

struct MouseOpenParam {
    uint8_t behaviorFlag;
    uint8_t pad[7];
};

struct MouseInputEvent {
    bool connectionChange;
    bool connected;
    int32_t x;
    int32_t y;
    int32_t wheel;
    int32_t tilt;
    uint32_t button;
    bool pressed;
    bool resetButtons;
};

static std::mutex g_mouseMutex;
static bool g_initialized = false;
static bool g_opened = false;
static bool g_connected = true;
static uint32_t g_buttons = 0;
static uint32_t g_pressedSinceRead = 0;
static double g_absoluteX = 960.0;
static double g_absoluteY = 540.0;
static int32_t g_screenWidth = 1920;
static int32_t g_screenHeight = 1080;
static int32_t g_wheel = 0;
static int32_t g_tilt = 0;

APS5_VABI int sceMouseInit() {
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_initialized = true;
    return 0;
}

APS5_VABI int sceMouseOpen(int userId, int32_t type, int32_t index, const void* param) {
    (void)userId; (void)type; (void)index; (void)param;
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    if (!g_initialized) return -1;
    if (g_opened) return -1;
    g_opened = true;
    g_buttons = g_pressedSinceRead = 0;
    return 1;
}

APS5_VABI int sceMouseClose(int32_t handle) {
    (void)handle;
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_opened = false;
    g_buttons = g_pressedSinceRead = 0;
    return 0;
}

APS5_VABI int sceMouseRead(int32_t handle, MouseData* data, int32_t num) {
    (void)handle; (void)data; (void)num;
    return 0;
}

APS5_VABI int sceMouseSetAbsoluteVirtualScreenSize(int32_t handle, int32_t size) {
    (void)handle;
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    if (size == 0) { g_screenWidth = 1280; g_screenHeight = 720; }
    else if (size == 1) { g_screenWidth = 1920; g_screenHeight = 1080; }
    else if (size == 2) { g_screenWidth = 2560; g_screenHeight = 1440; }
    else { g_screenWidth = 3840; g_screenHeight = 2160; }
    return 0;
}

APS5_VABI int sceMouseResetAbsolutePosition(int32_t handle, uint16_t x, uint16_t y) {
    (void)handle;
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_absoluteX = x;
    g_absoluteY = y;
    return 0;
}

APS5_VABI int sceMouseSetAbsoluteHighPrecisionMode(int32_t handle, bool enabled) {
    (void)handle; (void)enabled;
    return 0;
}

APS5_VABI int sceMouseReadAbsolutePosition(int32_t handle, void* data, int32_t num) {
    (void)handle;
    if (!data || num <= 0) return -1;
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    MouseAbsoluteData* sample = static_cast<MouseAbsoluteData*>(data);
    memset(sample, 0, sizeof(*sample));
    sample->timestamp = sceKernelGetProcessTime();
    sample->connected = 1;
    sample->buttons = g_buttons | g_pressedSinceRead;
    g_pressedSinceRead = 0;
    
    // Prevent integer overflow or wrapping around
    double clampedX = std::clamp(g_absoluteX, -32768.0, 32767.0);
    double clampedY = std::clamp(g_absoluteY, -32768.0, 32767.0);
    sample->x = (uint16_t)(int16_t)std::round(clampedX);
    sample->y = (uint16_t)(int16_t)std::round(clampedY);
    sample->wheel = g_wheel;
    g_wheel = 0;
    sample->tilt = g_tilt;
    g_tilt = 0;
    return 1;
}

APS5_VABI void MousePublishInput_nid_postfix(const MouseInputEvent& event) {
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    if (event.connectionChange) {
        g_connected = event.connected;
        if (!g_connected) g_buttons = g_pressedSinceRead = 0;
    }
    if (event.resetButtons) g_buttons = g_pressedSinceRead = 0;
    if (event.button != 0) {
        if (event.pressed) {
            g_buttons |= event.button;
            g_pressedSinceRead |= event.button;
        } else {
            g_buttons &= ~event.button;
        }
    }
}

APS5_VABI void MousePublishDirect(int32_t targetX, int32_t targetY, uint32_t button, bool pressed, uintptr_t delta) {
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_connected = true;

    targetX = std::clamp(targetX, 0, 1920);
    targetY = std::clamp(targetY, 0, 1080);

    if (delta == 0) {
        typedef uintptr_t (*delta_fn)(void);
        static delta_fn get_delta = nullptr;
        if (!get_delta) {
            void* box = dlopen("libbox64.so", RTLD_NOW | RTLD_NOLOAD);
            if (box) get_delta = (delta_fn)dlsym(box, "Aps5GetAppElfDelta");
            if (!get_delta) get_delta = (delta_fn)dlsym(RTLD_DEFAULT, "Aps5GetAppElfDelta");
        }
        if (get_delta) delta = get_delta();
    }

    if (delta != 0) {
        // App.elf Mewgenics cursor coordinates are at:
        // 0x1ab21c0: int32_t curX
        // 0x1ab21c4: int32_t curY
        int32_t curX = *(const int32_t*)(delta + 0x1ab21c0);
        int32_t curY = *(const int32_t*)(delta + 0x1ab21c4);
        if (curX < 0 || curX > 1920 || curY < 0 || curY > 1080) {
            curX = 960;
            curY = 540;
        }
        const int32_t deltaX = targetX - curX;
        const int32_t deltaY = targetY - curY;
        // Game centers at (960, 540) on reset. We supply 960 + delta so app.elf computes
        // new_cur = cur + (sample - 960) = cur + delta = targetX!
        g_absoluteX = 960.0 + static_cast<double>(deltaX);
        g_absoluteY = 540.0 + static_cast<double>(deltaY);
    } else {
        g_absoluteX = static_cast<double>(targetX);
        g_absoluteY = static_cast<double>(targetY);
    }

    if (button != 0) {
        if (pressed) {
            g_buttons |= button;
            g_pressedSinceRead |= button;
        } else {
            g_buttons &= ~button;
        }
    }
}

APS5_VABI void MousePublishDirect_nid_postfix(int32_t targetX, int32_t targetY, uint32_t button, bool pressed, uintptr_t delta) {
    MousePublishDirect(targetX, targetY, button, pressed, delta);
}

}
