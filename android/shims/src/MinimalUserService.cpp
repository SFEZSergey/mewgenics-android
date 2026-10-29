#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>

#define APS5_VABI __attribute__((visibility("default")))

constexpr int USER_SERVICE_OK = 0;
constexpr int USER_SERVICE_INITIAL_USER_ID = 0x01;
constexpr int USER_SERVICE_USER_ID_INVALID = -1;

struct SceUserServiceEvent {
    std::uint32_t event_type;
    int user_id;
};

struct UserServiceLoginUserIdList {
    int user_id[4];
};

struct UserServiceRegisteredUserIdList {
    int user_id[16];
};

struct UserServiceGamePresets {
    int difficulty;
    int priority;
    int invert_vertical_view_for_1st_person_view;
    int invert_horizontal_view_for_1st_person_view;
    int invert_vertical_view_for_3rd_person_view;
    int invert_horizontal_view_for_3rd_person_view;
    int display_sub_titles;
    int audio_language;
};

extern "C" {

APS5_VABI int sceUserServiceInitialize(const void* params) { return 0; }
APS5_VABI int sceUserServiceInitialize2(void) { return 0; }
APS5_VABI int sceUserServiceTerminate(void) { return 0; }

APS5_VABI int sceUserServiceGetInitialUser(int* user_id) {
    if (user_id) *user_id = USER_SERVICE_INITIAL_USER_ID;
    return 0;
}

APS5_VABI int sceUserServiceGetForegroundUser(int* user_id) {
    if (user_id) *user_id = USER_SERVICE_INITIAL_USER_ID;
    return 0;
}

APS5_VABI int sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* list) {
    if (!list) return -1;
    list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
    list->user_id[1] = USER_SERVICE_USER_ID_INVALID;
    list->user_id[2] = USER_SERVICE_USER_ID_INVALID;
    list->user_id[3] = USER_SERVICE_USER_ID_INVALID;
    return 0;
}

APS5_VABI int sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* list) {
    if (!list) return -1;
    list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
    for (int i = 1; i < 16; ++i) list->user_id[i] = USER_SERVICE_USER_ID_INVALID;
    return 0;
}

APS5_VABI int sceUserServiceGetUserName(int user_id, char* name, size_t size) {
    if (!name || size < 6) return -1;
    std::strncpy(name, "Player", size);
    return 0;
}

APS5_VABI int sceUserServiceGetUserNumber(int user_id, int32_t* number) {
    if (number) *number = 1;
    return 0;
}

APS5_VABI int sceUserServiceGetUserColor(int user_id, int* color) {
    if (color) *color = 0;
    return 0;
}

APS5_VABI int sceUserServiceGetNpAccountId(int user_id, uint64_t* account_id) {
    if (account_id) *account_id = 0;
    return 0;
}

APS5_VABI int sceUserServiceGetEvent(SceUserServiceEvent* event) {
    static std::atomic<bool> loginReported{false};
    if (loginReported.exchange(true) || !event) return 0x80960002; // no event
    event->event_type = 0;
    event->user_id = USER_SERVICE_INITIAL_USER_ID;
    return 0;
}

APS5_VABI int sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
    if (presets) std::memset(presets, 0, sizeof(*presets));
    return 0;
}

APS5_VABI int sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
    if (age_level) *age_level = 0;
    return 0;
}

APS5_VABI int sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetAccessibilityVibration(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetAccessibilityZoomFollowFocus(int user_id, int32_t* v) { if(v) *v=0; return 0; }
APS5_VABI int sceUserServiceGetPlatformPrivacyWs1(int32_t user_id, int32_t* v) { if(v) *v=0; return 0; }

}
