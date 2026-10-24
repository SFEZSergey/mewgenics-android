#include <algorithm>
#include <atomic>
#include <cerrno>
#include <utility>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "SDL.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

// Мост геймпада (APS5_PAD_BRIDGE=<файл>): под Termux /dev/input недоступен, поэтому состояние
// пишет помощник tools/padbridge, запущенный через su. Раскладка совпадает с padbridge.c.
struct PadBridgeState {
    std::uint32_t magic;
    std::uint32_t sequence;
    std::uint32_t connected;
    std::uint32_t buttons;
    std::uint8_t sticks[4];
    std::uint8_t l2, r2;
    std::uint8_t reserved[2];
};

const volatile PadBridgeState* padBridge() {
    static const volatile PadBridgeState* const bridge = []() -> const volatile PadBridgeState* {
        const char* path = std::getenv("APS5_PAD_BRIDGE");
        if (path == nullptr) return nullptr;
        const int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            std::fprintf(stderr, "Pad: bridge %s: %s\n", path, std::strerror(errno));
            return nullptr;
        }
        void* map = mmap(nullptr, sizeof(PadBridgeState), PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (map == MAP_FAILED) {
            std::fprintf(stderr, "Pad: bridge mmap %s: %s\n", path, std::strerror(errno));
            return nullptr;
        }
        std::fprintf(stderr, "Pad: reading the gamepad bridge %s\n", path);
        return static_cast<const volatile PadBridgeState*>(map);
    }();
    return bridge;
}

// Снимок состояния моста под seqlock; false, если геймпад не подключён.
bool samplePadBridge(PadInputState& result) {
    const auto* bridge = padBridge();
    if (bridge == nullptr || bridge->magic != 0x44415041u) return false;
    PadBridgeState copy{};
    for (int attempt = 0; attempt < 16; ++attempt) {
        const std::uint32_t before = bridge->sequence;
        std::atomic_thread_fence(std::memory_order_acquire);
        if (before & 1u) continue;
        copy.connected = bridge->connected;
        copy.buttons = bridge->buttons;
        for (int i = 0; i < 4; ++i) copy.sticks[i] = bridge->sticks[i];
        copy.l2 = bridge->l2;
        copy.r2 = bridge->r2;
        std::atomic_thread_fence(std::memory_order_acquire);
        if (bridge->sequence == before) break;
    }
    if (copy.connected == 0) return false;
    static constexpr std::pair<std::uint32_t, Pad::PadButton> map[] = {
        {1u << 0, Pad::PadButton::Cross}, {1u << 1, Pad::PadButton::Circle}, {1u << 2, Pad::PadButton::Square},
        {1u << 3, Pad::PadButton::Triangle}, {1u << 4, Pad::PadButton::L1}, {1u << 5, Pad::PadButton::R1},
        {1u << 6, Pad::PadButton::L2}, {1u << 7, Pad::PadButton::R2}, {1u << 8, Pad::PadButton::TouchPad},
        {1u << 9, Pad::PadButton::Options}, {1u << 11, Pad::PadButton::L3}, {1u << 12, Pad::PadButton::R3},
        {1u << 13, Pad::PadButton::Up}, {1u << 14, Pad::PadButton::Right}, {1u << 15, Pad::PadButton::Down},
        {1u << 16, Pad::PadButton::Left}, {1u << 17, Pad::PadButton::TouchPad},
    };
    result = PadInputState{};
    for (const auto& [bit, button] : map) {
        if (copy.buttons & bit) result.buttons |= static_cast<std::uint32_t>(button);
    }
    result.analogButtonsL2 = copy.l2;
    result.analogButtonsR2 = copy.r2;
    if (copy.l2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (copy.r2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);
    result.sticks = {copy.sticks[0], copy.sticks[1], copy.sticks[2], copy.sticks[3]};
    result.deviceKind = 2;
    // Share (Create) жмёт центр тачпада, как кнопка View у Xbox-пада выше.
    if (copy.buttons & (1u << 8)) result.touch[0] = {true, 960, 471, 0};
    return true;
}

} // namespace

PadInput::PadInput()
    : bindings(Pad::LoadInputMapping()), pressed(bindings.size()), wheelReleaseTimes(bindings.size()) {
    openFirstAvailableController();
}

PadInput::~PadInput() {
    closeController();
}

void PadInput::openFirstAvailableController() {
    if (controller != nullptr) return;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    if ((SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            APS5_LOG_ERR("Pad: SDL game controller init failed: %s", SDL_GetError());
            return;
        }
    }
    for (int deviceIndex = 0; deviceIndex < SDL_NumJoysticks(); ++deviceIndex) {
        if (!SDL_IsGameController(deviceIndex)) continue;
        openController(deviceIndex);
        if (controller != nullptr) return;
    }
}

void PadInput::openController(int deviceIndex) {
    if (controller != nullptr || !SDL_IsGameController(deviceIndex)) return;
    controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        APS5_LOG_ERR("Pad: could not open game controller %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    const char* name = SDL_GameControllerName(controller);
    APS5_LOG_OUT("Pad: connected game controller: %s (type %d, sensors accel=%d gyro=%d, touchpads=%d, led=%d, trigger rumble=%d)",
        name != nullptr ? name : "unknown", static_cast<int>(SDL_GameControllerGetType(controller)),
        SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE, SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE,
        SDL_GameControllerGetNumTouchpads(controller), SDL_GameControllerHasLED(controller) == SDL_TRUE, SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE);
    enableSensors();
    outputPending = true;
}

void PadInput::enableSensors() {
    if (controller == nullptr) return;
    const SDL_bool wanted = outputState.motionEnabled ? SDL_TRUE : SDL_FALSE;
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, wanted);
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, wanted);
}

void PadInput::closeController() {
    if (controller == nullptr) return;
    SDL_GameControllerClose(controller);
    controller = nullptr;
    controllerState = {};
}

void PadInput::applyOutput() {
    PadOutputState fetched;
    if (PadFetchOutput_nid_postfix(&outputSequence, &fetched)) {
        const bool motionChanged = fetched.motionEnabled != outputState.motionEnabled;
        outputState = fetched;
        outputPending = true;
        if (motionChanged) enableSensors();
    }
    if (controller == nullptr) return;
    const auto now = std::chrono::steady_clock::now();
    const bool rumbling = outputState.vibrationLarge != 0 || outputState.vibrationSmall != 0;
    const bool triggerRumble = outputState.trigger[0].fallback != 0 || outputState.trigger[1].fallback != 0;
    const bool isPs5 = SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5;
    if (!outputPending) {
        if ((rumbling || (triggerRumble && !isPs5)) && now >= nextRumbleRefresh) outputPending = true;
        else return;
    }
    outputPending = false;
    nextRumbleRefresh = now + std::chrono::milliseconds(700);
    constexpr Uint32 rumbleMs = 2000;
    SDL_GameControllerRumble(controller, static_cast<Uint16>(outputState.vibrationLarge * 257), static_cast<Uint16>(outputState.vibrationSmall * 257), rumbling ? rumbleMs : 0);
    if (SDL_GameControllerHasLED(controller) == SDL_TRUE) {
        if (outputState.lightBarValid) SDL_GameControllerSetLED(controller, outputState.lightBar[0], outputState.lightBar[1], outputState.lightBar[2]);
        else SDL_GameControllerSetLED(controller, 0, 64, 255);
    }
    if (outputState.triggerTouched) {
        if (isPs5) {
            Uint8 effect[47] = {};
            effect[0] = 0x04 | 0x08;
            std::memcpy(effect + 10, outputState.trigger[1].effect, 11);
            std::memcpy(effect + 21, outputState.trigger[0].effect, 11);
            SDL_GameControllerSendEffect(controller, effect, sizeof(effect));
        } else if (SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE) {
            SDL_GameControllerRumbleTriggers(controller, static_cast<Uint16>(outputState.trigger[0].fallback * 257), static_cast<Uint16>(outputState.trigger[1].fallback * 257), triggerRumble ? rumbleMs : 0);
        }
    }
}

void PadInput::setMouseMode(bool enabled) {
    if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseEnabled = enabled;
    mouseStick = {128, 128};
    nextMousePoll = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::MousePollIntervalMs);
}

PadInputState PadInput::sampleController() const {
    PadInputState result;
    if (controller == nullptr) return result;
    const auto readButton = [this](SDL_GameControllerButton button) {
        return SDL_GameControllerGetButton(controller, button) != 0;
    };
    const auto addButton = [&result, &readButton](SDL_GameControllerButton source, Pad::PadButton button) {
        if (readButton(source)) result.buttons |= static_cast<std::uint32_t>(button);
    };
    addButton(SDL_CONTROLLER_BUTTON_A, Pad::PadButton::Cross);
    addButton(SDL_CONTROLLER_BUTTON_B, Pad::PadButton::Circle);
    addButton(SDL_CONTROLLER_BUTTON_X, Pad::PadButton::Square);
    addButton(SDL_CONTROLLER_BUTTON_Y, Pad::PadButton::Triangle);
    addButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Pad::PadButton::L1);
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Pad::PadButton::R1);
    const auto type = SDL_GameControllerGetType(controller);
    const bool viewPressed = type != SDL_CONTROLLER_TYPE_PS4 && type != SDL_CONTROLLER_TYPE_PS5 && readButton(SDL_CONTROLLER_BUTTON_BACK);
    if (viewPressed) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
    addButton(SDL_CONTROLLER_BUTTON_START, Pad::PadButton::Options);
    addButton(SDL_CONTROLLER_BUTTON_LEFTSTICK, Pad::PadButton::L3);
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, Pad::PadButton::R3);
    addButton(SDL_CONTROLLER_BUTTON_DPAD_UP, Pad::PadButton::Up);
    addButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Pad::PadButton::Right);
    addButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, Pad::PadButton::Down);
    addButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, Pad::PadButton::Left);
    addButton(SDL_CONTROLLER_BUTTON_TOUCHPAD, Pad::PadButton::TouchPad);

    const auto triggerValue = [this](SDL_GameControllerAxis axis) {
        const auto value = std::clamp<int>(SDL_GameControllerGetAxis(controller, axis), 0, 32767);
        return static_cast<std::uint8_t>((value * 255 + 16383) / 32767);
    };
    result.analogButtonsL2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    result.analogButtonsR2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (result.analogButtonsL2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (result.analogButtonsR2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);

    const auto stickValue = [this](SDL_GameControllerAxis axis) {
        const auto value = static_cast<std::int32_t>(SDL_GameControllerGetAxis(controller, axis)) + 32768;
        return static_cast<std::uint8_t>((value * 255 + 32767) / 65535);
    };
    result.sticks = {
        stickValue(SDL_CONTROLLER_AXIS_LEFTX),
        stickValue(SDL_CONTROLLER_AXIS_LEFTY),
        stickValue(SDL_CONTROLLER_AXIS_RIGHTX),
        stickValue(SDL_CONTROLLER_AXIS_RIGHTY)
    };
    switch (SDL_GameControllerGetType(controller)) {
        case SDL_CONTROLLER_TYPE_PS5: result.deviceKind = 1; break;
        case SDL_CONTROLLER_TYPE_PS4: result.deviceKind = 2; break;
        default: result.deviceKind = 3; break;
    }
    if (SDL_GameControllerIsSensorEnabled(controller, SDL_SENSOR_ACCEL) == SDL_TRUE && SDL_GameControllerIsSensorEnabled(controller, SDL_SENSOR_GYRO) == SDL_TRUE) {
        float accel[3];
        float gyro[3];
        if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_ACCEL, accel, 3) == 0 && SDL_GameControllerGetSensorData(controller, SDL_SENSOR_GYRO, gyro, 3) == 0) {
            result.hasMotion = true;
            for (int i = 0; i < 3; ++i) { result.accel[i] = accel[i]; result.gyro[i] = gyro[i]; }
        }
    }
    if (SDL_GameControllerGetNumTouchpads(controller) > 0) {
        for (int finger = 0; finger < 2; ++finger) {
            Uint8 down = 0;
            float x = 0.0f;
            float y = 0.0f;
            float pressure = 0.0f;
            if (SDL_GameControllerGetTouchpadFinger(controller, 0, finger, &down, &x, &y, &pressure) != 0 || down == 0) continue;
            result.touch[finger].active = true;
            result.touch[finger].x = static_cast<std::uint16_t>(std::clamp(x, 0.0f, 1.0f) * 1919.0f);
            result.touch[finger].y = static_cast<std::uint16_t>(std::clamp(y, 0.0f, 1.0f) * 942.0f);
        }
    }
    if (viewPressed && !result.touch[0].active && !result.touch[1].active) {
        result.touch[0] = {true, 960, 471, 0};
    }
    return result;
}

void PadInput::HandleEvent(const SDL_Event& event, DisplayWindow& window) {
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        openController(event.cdevice.which);
        return;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller != nullptr) {
        const auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if (instanceId == event.cdevice.which) {
            closeController();
            openFirstAvailableController();
            publish();
        }
        return;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
        event.type == SDL_CONTROLLERAXISMOTION || event.type == SDL_CONTROLLERDEVICEREMAPPED) {
        publish();
        return;
    }
    if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_CLOSE)) {
        std::fill(pressed.begin(), pressed.end(), false);
        std::fill(wheelReleaseTimes.begin(), wheelReleaseTimes.end(), std::chrono::steady_clock::time_point{});
        if (mouseEnabled) setMouseMode(false);
        publish();
        return;
    }
    // APS5_NO_PAD_INPUT=1 keeps a measurement run from reacting to keys or mouse buttons that reach
    // its window (a stray press advances the title into another stage).
    static const bool ignoreInput = std::getenv("APS5_NO_PAD_INPUT") != nullptr;
    if (ignoreInput && event.type == SDL_MOUSEWHEEL) return;
    if (event.type == SDL_MOUSEWHEEL) {
        int direction = (event.wheel.y > 0) - (event.wheel.y < 0);
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        if (direction == 0) return;
        const auto releaseTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::WheelPressDurationMs);
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            const auto& binding = bindings[index];
            if (binding.wheelDirection == 0) continue;
            pressed[index] = binding.wheelDirection == direction;
            wheelReleaseTimes[index] = pressed[index] ? releaseTime : std::chrono::steady_clock::time_point{};
        }
        publish();
        return;
    }
    const bool keyboard = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP;
    const bool mouse = event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!keyboard && !mouse) return;
    if (ignoreInput) return;
    // Clicks SDL synthesizes from touches (Android) are not mouse buttons: every touch on the screen
    // pressed the left-button binding (Square). Touch input comes from the overlay instead.
    if (mouse && event.button.which == SDL_TOUCH_MOUSEID) return;
    if (keyboard && event.key.repeat != 0) return;
    const bool down = event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const auto& binding = bindings[index];
        const bool matches = keyboard
            ? binding.key != SDL_SCANCODE_UNKNOWN && binding.key == event.key.keysym.scancode
            : binding.mouseButton != Pad::MouseButton::None && binding.mouseButton == static_cast<Pad::MouseButton>(event.button.button);

        if (!matches) continue;
        if (binding.control == Pad::InputControl::ToggleFullscreen) {
            if (keyboard && down && !pressed[index] && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
        }
        if (binding.control == Pad::InputControl::ToggleMouse && down && !pressed[index]) setMouseMode(!mouseEnabled);
        pressed[index] = down;
    }
    publish();
}

namespace {
// libmain's touch overlay state (Android APK); null elsewhere.
struct VirtualPadState {
    std::atomic<std::uint32_t> sequence;
    std::atomic<std::uint32_t> buttons;
    std::atomic<std::uint32_t> sticks;
};

const VirtualPadState* virtualPadState() {
#ifdef __ANDROID__
    static const VirtualPadState* state = [] () -> const VirtualPadState* {
        // libmain passes the address (no linker-namespace lookup needed); dlsym as a fallback.
        if (const char* address = std::getenv("APS5_VIRTUAL_PAD"); address != nullptr && address[0] != '\0') {
            const auto value = std::strtoull(address, nullptr, 16);
            if (value != 0) return reinterpret_cast<const VirtualPadState*>(static_cast<std::uintptr_t>(value));
        }
        void* main = dlopen("libmain.so", RTLD_NOW | RTLD_NOLOAD);
        return main != nullptr ? static_cast<const VirtualPadState*>(dlsym(main, "aps5_virtual_pad")) : nullptr;
    }();
    return state;
#else
    return nullptr;
#endif
}
}

void PadInput::Update() {
    if (const auto* overlay = virtualPadState(); overlay != nullptr) {
        const auto sequence = overlay->sequence.load(std::memory_order_acquire);
        if (sequence != virtualSequence) {
            virtualSequence = sequence;
            virtualButtons = overlay->buttons.load(std::memory_order_relaxed);
            const auto sticks = overlay->sticks.load(std::memory_order_relaxed);
            for (int axis = 0; axis < 4; ++axis) virtualSticks[static_cast<std::size_t>(axis)] = static_cast<std::uint8_t>(sticks >> (axis * 8));
            publish();
        }
    }
    if (controller != nullptr) SDL_GameControllerUpdate();
    applyOutput();
    PadInputState bridged;
    static bool bridgeWasConnected = false;
    if (samplePadBridge(bridged)) {
        controllerState = bridged;
        bridgeWasConnected = true;
        publish();
    } else if (controller != nullptr) {
        controllerState = sampleController();
        publish();
    } else if (bridgeWasConnected) {
        bridgeWasConnected = false;
        controllerState = PadInputState{};
        publish();
    }
    const auto now = std::chrono::steady_clock::now();
    bool released = false;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (bindings[index].wheelDirection == 0 || !pressed[index] || now < wheelReleaseTimes[index]) continue;
        pressed[index] = false;
        wheelReleaseTimes[index] = {};
        released = true;
    }
    if (released) publish();
    if (!mouseEnabled) return;
    if (SDL_GetKeyboardFocus() == nullptr) {
        std::fill(pressed.begin(), pressed.end(), false);
        std::fill(wheelReleaseTimes.begin(), wheelReleaseTimes.end(), std::chrono::steady_clock::time_point{});
        setMouseMode(false);
        publish();
        return;
    }
    if (now < nextMousePoll) return;
    nextMousePoll = now + std::chrono::milliseconds(Pad::MousePollIntervalMs);
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseStick = {128, 128};
    if (deltaX != 0 || deltaY != 0) {
        const double distance = std::hypot(deltaX, deltaY);
        const double scale = std::clamp(distance * Pad::MouseSensitivity + 16.0, 64.0, 128.0) / distance;
        const auto mapAxis = [scale](int delta) { return static_cast<std::uint8_t>(std::clamp(128L + std::lround(delta * scale), 0L, 255L)); };
        mouseStick = {mapAxis(deltaX), mapAxis(deltaY)};
    }
    publish();
}

void PadInput::publish() {
    PadInputState state;
    state.buttons = controllerState.buttons;
    state.sticks = controllerState.sticks;
    state.analogButtonsL2 = controllerState.analogButtonsL2;
    state.analogButtonsR2 = controllerState.analogButtonsR2;
    state.hasMotion = controllerState.hasMotion;
    state.accel = controllerState.accel;
    state.gyro = controllerState.gyro;
    state.touch = controllerState.touch;
    state.deviceKind = controllerState.deviceKind;

    std::array<bool, 4> negative{};
    std::array<bool, 4> positive{};
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (!pressed[index]) continue;
        const auto& binding = bindings[index];
        switch (binding.control) {
            case Pad::InputControl::Button:
                state.buttons |= static_cast<std::uint32_t>(binding.button);
                if (binding.button == Pad::PadButton::L2) state.analogButtonsL2 = 255;
                if (binding.button == Pad::PadButton::R2) state.analogButtonsR2 = 255;
                break;
            case Pad::InputControl::LeftStickLeft: negative[0] = true; break;
            case Pad::InputControl::LeftStickRight: positive[0] = true; break;
            case Pad::InputControl::LeftStickUp: negative[1] = true; break;
            case Pad::InputControl::LeftStickDown: positive[1] = true; break;
            case Pad::InputControl::RightStickLeft: negative[2] = true; break;
            case Pad::InputControl::RightStickRight: positive[2] = true; break;
            case Pad::InputControl::RightStickUp: negative[3] = true; break;
            case Pad::InputControl::RightStickDown: positive[3] = true; break;
            case Pad::InputControl::TouchLeft: state.touchLeft = true; break;
            case Pad::InputControl::TouchRight: state.touchRight = true; break;
            case Pad::InputControl::ToggleMouse: break;
            case Pad::InputControl::ToggleFullscreen: break;
        }
    }
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) {
        if (negative[axis] || positive[axis]) state.sticks[axis] = negative[axis] == positive[axis] ? 128 : negative[axis] ? 0 : 255;
    }
    if (mouseEnabled) {
        state.sticks[2] = mouseStick[0];
        state.sticks[3] = mouseStick[1];
    }
    // Touch overlay: its buttons add to the others, a deflected virtual stick wins over a centred one.
    state.buttons |= virtualButtons;
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) {
        if (virtualSticks[axis] < 112 || virtualSticks[axis] > 144) state.sticks[axis] = virtualSticks[axis];
    }
    if (state.analogButtonsL2 != 0) state.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (state.analogButtonsR2 != 0) state.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);
    PadPublishInput_nid_postfix(state);
}
