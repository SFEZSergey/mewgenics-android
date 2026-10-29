#include <cstdint>

#define APS5_VABI __attribute__((visibility("default")))

extern "C" {

APS5_VABI int sceCommonDialogInitialize(void) {
    return 0;
}

APS5_VABI bool sceCommonDialogIsUsed(void) {
    return false;
}

}
