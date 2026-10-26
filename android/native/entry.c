#include <jni.h>
#include <dlfcn.h>
#include <stdio.h>

JNIEXPORT jstring JNICALL Java_org_anyps5_android_MainActivity_nativeGetStatus(JNIEnv *env, jclass type) {
    (void) type;
    return (*env)->NewStringUTF(env, "Box64 + AnyPS5 Android launcher");
}
JNIEXPORT jstring JNICALL Java_org_anyps5_android_MainActivity_nativeProbeRuntime(JNIEnv *env, jclass type) {
    (void) type;
    void *h = dlopen("libbox64.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) return (*env)->NewStringUTF(env, "Box64: библиотека не загружена");
    void *entry = dlsym(h, "box64_main"); dlclose(h);
    return (*env)->NewStringUTF(env, entry != NULL ? "Box64 Android runtime готов" : "Box64: box64_main не найден");
}
