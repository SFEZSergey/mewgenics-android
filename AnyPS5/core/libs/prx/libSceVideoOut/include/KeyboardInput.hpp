#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_KEYBOARDINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_KEYBOARDINPUT_HPP

#include "SDL_events.h"

class KeyboardInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);

private:
    bool focused = true;
    // Android: a real device was used (see HandleEvent); until then the game has none.
#ifdef __ANDROID__
    bool seen = false;
#else
    bool seen = true;
#endif
};

#endif
