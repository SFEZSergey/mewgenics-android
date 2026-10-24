#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cstdio>
#include <mutex>
#include <vector>

#include "SceTypes.hpp"
#include "prx//libc/include/General.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007);
constexpr int PAD_ERROR_DEVICE_NO_HANDLE = static_cast<int>(0x80920008);

bool g_opened = false;

bool ValidPort(int userId, int type, int index) {
    const bool personalPort = type == PAD_PORT_TYPE_STANDARD || type == PAD_PORT_TYPE_SPECIAL;
    const bool systemRemote = userId == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE;
    return index == 0 && (personalPort || systemRemote);
}

// Only the user's standard port is the pad (PAD_HANDLE). Every other port the title opens (the
// special port, the system user's remote-control port) gets a handle of its own with nothing
// connected: all of them returned the one pad before, so the title saw the same input on a second
// device and took a press of Options there for a second player joining.
struct IdlePort {
    int userId;
    int type;
};
constexpr int PAD_HANDLE_IDLE_FIRST = 2;
std::mutex portsMutex;
std::vector<IdlePort> idlePorts;

bool LivePort(int userId, int type) {
    return type == PAD_PORT_TYPE_STANDARD && userId != PAD_USER_ID_SYSTEM;
}

bool IdleHandle(int handle) {
    std::lock_guard lock(portsMutex);
    return handle >= PAD_HANDLE_IDLE_FIRST && static_cast<std::size_t>(handle - PAD_HANDLE_IDLE_FIRST) < idlePorts.size();
}

bool KnownHandle(int handle) {
    return handle == PAD_HANDLE || IdleHandle(handle);
}

int IdleHandleFor(int userId, int type, bool create) {
    std::lock_guard lock(portsMutex);
    for (std::size_t i = 0; i < idlePorts.size(); ++i) {
        if (idlePorts[i].userId == userId && idlePorts[i].type == type) return PAD_HANDLE_IDLE_FIRST + static_cast<int>(i);
    }
    if (!create) return PAD_ERROR_DEVICE_NO_HANDLE;
    idlePorts.push_back({userId, type});
    return PAD_HANDLE_IDLE_FIRST + static_cast<int>(idlePorts.size() - 1);
}

bool RemoteHandle(int handle) {
    std::lock_guard lock(portsMutex);
    const auto index = static_cast<std::size_t>(handle - PAD_HANDLE_IDLE_FIRST);
    return handle >= PAD_HANDLE_IDLE_FIRST && index < idlePorts.size() && idlePorts[index].type == PAD_PORT_TYPE_REMOTE;
}

void LogOpen(int userId, int type, int index, int handle) {
    std::fprintf(stderr, "[pad] open user 0x%x port type %d index %d -> handle %d%s\n", userId, type, index, handle, handle == PAD_HANDLE ? " (the pad)" : handle > 0 ? " (nothing connected)" : "");
}

}

extern "C" {

int APS5_VABI scePadClose_nid_postfix(int handle) {
 if (IdleHandle(handle)) return PAD_OK;
 if (handle != PAD_HANDLE) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 g_opened = false;
 return PAD_OK;
}

int APS5_VABI scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr || class_data == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(class_data, 0, sizeof(*class_data));
 class_data->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 class_data->dataValid = data->connected;
 return PAD_OK;
}

int APS5_VABI scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 if (!KnownHandle(handle)) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 if (info == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::memset(info, 0, sizeof(*info));
 if (handle != PAD_HANDLE) {
  info->connectionType = PAD_CONNECTION_TYPE_LOCAL;
  info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
  return PAD_OK;
 }
 info->touchPadInfo.pixelDensity = 44.86f;
 info->touchPadInfo.resolution.x = 1920;
 info->touchPadInfo.resolution.y = 943;
 info->stickInfo.deadZoneLeft = 2;
 info->stickInfo.deadZoneRight = 2;
 info->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 info->connectedCount = 1;
 info->connected = true;
 info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadGetHandle(int user_id, int type, int index) {
 if (!ValidPort(user_id, type, index)) return PAD_ERROR_DEVICE_NO_HANDLE;
 if (!LivePort(user_id, type)) return IdleHandleFor(user_id, type, false);
 if (!g_opened) return PAD_ERROR_DEVICE_NO_HANDLE;
 return PAD_HANDLE;
}

int APS5_VABI scePadGetInfo_nid_postfix(PadInfo* info) {
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 info->maxConnectCount = 4;
 info->connectedCount[0] = 1;
 info->connectedCount[1] = 0;
 PadInfo::PadTypeInfo& slot = info->padTypeInfo[0];
 slot.connectPort = 0;
 slot.status = 1;
 slot.deviceType = PAD_DEVICE_TYPE_DUAL_SENSE;
 slot.connectType = PAD_CONNECT_TYPE_BLUETOOTH;
 std::memcpy(slot.bluetoothMacAddress, PAD_BD_ADDRESS, sizeof(slot.bluetoothMacAddress));
 return PAD_OK;
}

int APS5_VABI scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 return PAD_OK;
}

int APS5_VABI scePadGetExtControllerInformation(int handle, void* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, 0x2c);
 PadControllerInformation* base = static_cast<PadControllerInformation*>(info);
 if (handle != PAD_HANDLE) {
  base->deviceClass = PAD_DEVICE_CLASS_STANDARD;
  return PAD_OK;
 }
 base->touchPadInfo.pixelDensity = 44.86f;
 base->touchPadInfo.resolution.x = 1920;
 base->touchPadInfo.resolution.y = 943;
 base->stickInfo.deadZoneLeft = 2;
 base->stickInfo.deadZoneRight = 2;
 base->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 base->connectedCount = 1;
 base->connected = true;
 base->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadSetProcessPrivilege(int privilege) {
 (void)privilege;
 return PAD_OK;
}

int APS5_VABI scePadSetParticularMode(bool mode) {
 (void)mode;
 return PAD_OK;
}

int APS5_VABI scePadInit_nid_postfix(void) {
 Pad::Initialize();
 return PAD_OK;
}

int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param) {
 (void)param;
 if (!ValidPort(userId, type, index)) {
  LogOpen(userId, type, index, PAD_ERROR_INVALID_ARG);
  return PAD_ERROR_INVALID_ARG;
 }
 if (!LivePort(userId, type)) {
  const int handle = IdleHandleFor(userId, type, true);
  LogOpen(userId, type, index, handle);
  return handle;
 }
 g_opened = true;
 LogOpen(userId, type, index, PAD_HANDLE);
 return PAD_HANDLE;
}

int APS5_VABI scePadOpenExt(int userId, int type, int index, const void* param) {
 if (!ValidPort(userId, type, index) || param == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 if (type != PAD_PORT_TYPE_SPECIAL) NotImplemented_nid_no_patch(__func__);
 return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

int APS5_VABI scePadReadExt() { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadGetFeatureReport() { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadSetFeatureReport() { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadOutputReport() { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadReadState(int handle, PadData* data);

int APS5_VABI scePadRead_nid_postfix(int handle, PadData* data, int num) {
    // The title drains the queued states; one current state is reported per call.
    if (data == nullptr || num <= 0) APS5_INVALID_ARG_EX;
    const int result = scePadReadState(handle, data);
    if (result != 0) return result;
    return 1;
}

int APS5_VABI scePadReadState(int handle, PadData* data) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr) APS5_INVALID_ARG_EX;
 if (handle != PAD_HANDLE) {
  // Nothing connected on this port: centred sticks, no buttons.
  *data = PadData{};
  data->left_stick_x = data->left_stick_y = data->right_stick_x = data->right_stick_y = 128;
  data->orientation_w = 1.0f;
  data->connected = false;
  data->timestamp = Pad::ReadState().timestamp;
  return 0;
 }

 *data = Pad::ReadState();

 return 0;
}

int APS5_VABI scePadResetLightBar(int handle) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 Pad::SetLightBar(false, 0, 0, 0);
 return PAD_OK;
}

int APS5_VABI scePadResetOrientation(int handle) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 Pad::ResetOrientation();
 return PAD_OK;
}

int APS5_VABI scePadSetAngularVelocityDeadbandState(int handle, bool enable) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (enable) NotImplemented_nid_no_patch(__func__);
 return PAD_OK;
}

int APS5_VABI scePadSetLightBar(int handle, const PadLightBarParam* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 Pad::SetLightBar(true, param->r, param->g, param->b);
 return PAD_OK;
}

int APS5_VABI scePadSetMotionSensorState(int handle, bool enable) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 Pad::SetMotionEnabled(enable);
 return PAD_OK;
}

int APS5_VABI scePadSetTiltCorrectionState(int handle, bool enabled) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 Pad::SetTiltCorrection(enabled);
 return PAD_OK;
}

int APS5_VABI scePadSetTriggerEffect(int handle, const void* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 const std::uint8_t* bytes = static_cast<const std::uint8_t*>(param);
 const std::uint8_t mask = bytes[0];
 if ((mask & ~0x3u) != 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::uint32_t mode[2] = {};
 for (int trigger = 0; trigger < 2; ++trigger) std::memcpy(&mode[trigger], bytes + 8 + 56 * trigger, 4);
 if (mode[0] > 6 || mode[1] > 6) {
  return PAD_ERROR_INVALID_ARG;
 }
 for (int trigger = 0; trigger < 2; ++trigger) {
  if ((mask & (1u << trigger)) != 0) Pad::SetTriggerCommand(trigger, bytes + 8 + 56 * trigger);
 }
 return PAD_OK;
}

int APS5_VABI scePadSetVibration(int handle, const PadVibrationParam* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 Pad::SetVibration(param->large_motor, param->small_motor);
 return PAD_OK;
}

int APS5_VABI scePadSetVibrationMode(int handle, int mode) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (mode < 0 || mode > 2) return PAD_ERROR_INVALID_ARG;
 Pad::SetVibrationMode(mode);
 return PAD_OK;
}

int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled) {
 (void)enabled;
 return PAD_OK;
}

int APS5_VABI scePadVrControllerGetDeviceInformation() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerRead() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerReadState(int handle, void* state) {
 (void)handle;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadIsRemoteController(int handle, bool* remote) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (remote == nullptr) return PAD_ERROR_INVALID_ARG;
 *remote = handle != PAD_HANDLE && RemoteHandle(handle);
 return 0;
}


int APS5_VABI scePadSetAngularVelocityBiasCorrectionState(int handle, bool enabled) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (handle != PAD_HANDLE) return PAD_OK;
 if (enabled) NotImplemented_nid_no_patch(__func__);
 return PAD_OK;
}

}
