#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"

#include "SDL_mouse.h"

void MouseInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    MouseInputEvent input{};
    // The mouse SDL synthesizes from touches is not a mouse: on a phone every touch of the touch
    // controls also moved and clicked it, a second input device (player 2) for the game.
    if ((event.type == SDL_MOUSEMOTION && event.motion.which == SDL_TOUCH_MOUSEID) ||
        ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.which == SDL_TOUCH_MOUSEID) ||
        (event.type == SDL_MOUSEWHEEL && event.wheel.which == SDL_TOUCH_MOUSEID))
        return;
    if (!seen && (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL)) {
        seen = true;
        MouseInputEvent connect{};
        connect.connectionChange = true;
        MousePublishInput_nid_postfix(connect);
    }
    switch (event.type) {
        case SDL_MOUSEMOTION:
            if (event.motion.windowID != windowId || !focused) return;
            input.x = event.motion.xrel;
            input.y = event.motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.windowID != windowId || !focused) return;
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: input.button = 1; break;
                case SDL_BUTTON_RIGHT: input.button = 2; break;
                case SDL_BUTTON_MIDDLE: input.button = 4; break;
                default: return;
            }
            input.pressed = event.type == SDL_MOUSEBUTTONDOWN;
            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.windowID != windowId || !focused) return;
            input.wheel = event.wheel.y;
            input.tilt = event.wheel.x;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                input.wheel = -input.wheel;
                input.tilt = -input.tilt;
            }
            if (input.wheel == 0 && input.tilt == 0) return;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.windowID != windowId) return;
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                focused = false;
                input.resetButtons = true;
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                focused = true;
                if (!seen) return;
                input.connectionChange = true;
            } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                focused = false;
                input.connectionChange = true;
                input.connected = false;
            } else {
                return;
            }
            break;
        default:
            return;
    }
    MousePublishInput_nid_postfix(input);
}
