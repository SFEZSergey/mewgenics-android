#include "prx/libSceMouse/include/MouseState.hpp"

extern "C" {

int APS5_VABI sceMouseInit() {
    return Mouse::Initialize();
}

int APS5_VABI sceMouseOpen(int userId, std::int32_t type, std::int32_t index, const void* param) {
    return Mouse::Open(userId, type, index, static_cast<const MouseOpenParam*>(param));
}

int APS5_VABI sceMouseClose(std::int32_t handle) {
    return Mouse::Close(handle);
}

int APS5_VABI sceMouseRead(std::int32_t handle, MouseData* data, std::int32_t num) {
    return Mouse::Read(handle, data, num);
}

// Absolute-position mode (PS5). The handle comes first as in the rest of the library; the layout is what
// PPSA37318 reads, unverified on a console (docs/dev/TechnicalDebt.md). The host's relative motion moves a cursor
// over the virtual screen picked here by size index (PPSA37318 passes 3).
int APS5_VABI sceMouseSetAbsoluteVirtualScreenSize(std::int32_t handle, std::int32_t size) {
    return Mouse::SetAbsoluteScreen(handle, size);
}

// x and y are zero-extended 16-bit values (a call site passes the centre of the display), as upstream AnyPS5 found.
int APS5_VABI sceMouseResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y) {
    return Mouse::ResetAbsolutePosition(handle, x, y);
}

int APS5_VABI sceMouseSetAbsoluteHighPrecisionMode(std::int32_t handle, bool enabled) {
    (void)enabled;
    return Mouse::CheckHandle(handle);
}

// One sample with the current cursor, buttons and the wheel since the last read; returns the sample count.
int APS5_VABI sceMouseReadAbsolutePosition(std::int32_t handle, void* data, std::int32_t num) {
    return Mouse::ReadAbsolute(handle, static_cast<MouseAbsoluteData*>(data), num);
}

void MousePublishInput_nid_postfix(const MouseInputEvent& event) {
    Mouse::Publish(event);
}

}
