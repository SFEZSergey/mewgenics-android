// SPDX-License-Identifier: Zlib
// The APK side of the native SDL_GPU backend: Vulkan loader and the SDL2 window (own header set).
#pragma once
#include <stdbool.h>
#include <stdint.h>

void *Aps5HostVulkanProc(void);
bool Aps5HostWindowSize(int *w, int *h);
bool Aps5HostCreateSurface(void *instance, void *instanceProc, const void *allocator, void *surface);
