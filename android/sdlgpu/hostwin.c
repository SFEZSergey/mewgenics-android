// SPDX-License-Identifier: Zlib
// Window and Vulkan loader of the APK for the native SDL_GPU backend. The APK's SDL2 owns the Android
// surface (VideoOut creates the window); the AGC presenter may already hold a swapchain on it, so it
// is released first (one swapchain per ANativeWindow).
#include <android/native_window.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.h>
#include "SDL.h"
#include "SDL_syswm.h"

#include "hostwin.h"

static void *sdl2(const char *name)
{
    static void *library;
    if (!library) library = dlopen("libSDL2.so", RTLD_NOW | RTLD_NOLOAD);
    if (!library) library = dlopen("libSDL2.so", RTLD_NOW);
    return library ? dlsym(library, name) : NULL;
}

void *Aps5HostVulkanProc(void)
{
    const char *handle = getenv("APS5_VULKAN_HANDLE");
    void *library = handle ? (void *)strtoull(handle, NULL, 0) : dlopen("libvulkan.so", RTLD_NOW);
    return library ? dlsym(library, "vkGetInstanceProcAddr") : NULL;
}

static SDL_Window *hostWindow(void)
{
    static SDL_Window *window;
    if (window) return window;
    SDL_Window *(*fromId)(Uint32) = sdl2("SDL_GetWindowFromID");
    for (Uint32 id = 1; fromId && id < 16 && !window; ++id) window = fromId(id);
    if (!window) {
        SDL_Window *(*create)(const char *, int, int, int, int, Uint32) = sdl2("SDL_CreateWindow");
        if (create) window = create("Mewgenics", 0, 0, 0, 0, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
        fprintf(stderr, "[sdlgpu] created the SDL2 window: %p\n", (void *)window);
    }
    return window;
}

static ANativeWindow *nativeWindow(void)
{
    SDL_Window *window = hostWindow();
    SDL_bool (*info)(SDL_Window *, SDL_SysWMinfo *) = sdl2("SDL_GetWindowWMInfo");
    if (!window || !info) return NULL;
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    return info(window, &wm) ? wm.info.android.window : NULL;
}

bool Aps5HostWindowSize(int *w, int *h)
{
    ANativeWindow *window = nativeWindow();
    if (!window) return false;
    if (w) *w = ANativeWindow_getWidth(window);
    if (h) *h = ANativeWindow_getHeight(window);
    return true;
}

bool Aps5HostCreateSurface(void *instance, void *instanceProc, const void *allocator, void *surface)
{
    // After the app comes back from the background the Android surface may not exist yet: wait for it
    // (up to 3 s) instead of failing the swapchain recreation.
    ANativeWindow *window = nativeWindow();
    for (int i = 0; !window && i < 300; ++i) {
        usleep(10000);
        window = nativeWindow();
    }
    if (!window || !instanceProc) {
        fprintf(stderr, "[sdlgpu] no Android window for the swapchain\n");
        return false;
    }
    // The AGC presenter (VideoOut attaches the window at its start) gives up its swapchain.
    void *agc = dlopen("libSceAgcDriver.so", RTLD_NOW | RTLD_NOLOAD);
    void (*release)(void *) = agc ? (void (*)(void *))dlsym(agc, "AgcDriverReleaseWindow_nid_postfix") : NULL;
    if (release) release(hostWindow());
    PFN_vkCreateAndroidSurfaceKHR create = (PFN_vkCreateAndroidSurfaceKHR)((PFN_vkGetInstanceProcAddr)instanceProc)((VkInstance)instance, "vkCreateAndroidSurfaceKHR");
    if (!create) return false;
    VkAndroidSurfaceCreateInfoKHR info = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    info.window = window;
    const VkResult result = create((VkInstance)instance, &info, (const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)surface);
    fprintf(stderr, "[sdlgpu] Android surface %dx%d: VkResult %d\n", ANativeWindow_getWidth(window), ANativeWindow_getHeight(window), (int)result);
    return result == VK_SUCCESS;
}
