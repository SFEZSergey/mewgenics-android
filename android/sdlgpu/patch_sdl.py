#!/usr/bin/env python3
# Applies our fixes to a copy of SDL's SDL_gpu_vulkan.c (the SDL tree stays untouched):
# downloads into transfer buffers get a TRANSFER_WRITE -> HOST_READ barrier. SDL leaves that "to the
# client", but a fence wait alone does not make GPU writes visible to the CPU on Turnip (they stay in
# the GPU caches; the CPU reads zeros).
import re, sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
barrier = '''
    {
        VkMemoryBarrier hostBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
        renderer->vkCmdPipelineBarrier(vulkanCommandBuffer->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hostBarrier, 0, NULL, 0, NULL);
    }
'''
count = 0
for fn, call in (('VULKAN_DownloadFromTexture', 'vkCmdCopyImageToBuffer'), ('VULKAN_DownloadFromBuffer', 'vkCmdCopyBuffer')):
    start = s.index('static void %s(' % fn)
    at = s.index('renderer->%s(' % call, start)
    end = s.index(');', at) + 2
    s = s[:end] + barrier + s[end:]
    count += 1

safe_funcs = '''
static VkResult SafeGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    uint32_t *pSurfaceFormatCount,
    VkSurfaceFormatKHR *pSurfaceFormats)
{
    (void)physicalDevice; (void)surface;
    static const VkSurfaceFormatKHR formats[] = {
        { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
    };
    const uint32_t count = (uint32_t)(sizeof(formats) / sizeof(formats[0]));
    if (!pSurfaceFormats) {
        *pSurfaceFormatCount = count;
        return VK_SUCCESS;
    }
    uint32_t toCopy = *pSurfaceFormatCount < count ? *pSurfaceFormatCount : count;
    memcpy(pSurfaceFormats, formats, toCopy * sizeof(VkSurfaceFormatKHR));
    *pSurfaceFormatCount = toCopy;
    return VK_SUCCESS;
}

static VkResult SafeGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    uint32_t *pPresentModeCount,
    VkPresentModeKHR *pPresentModes)
{
    (void)physicalDevice; (void)surface;
    static const VkPresentModeKHR modes[] = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR
    };
    const uint32_t count = (uint32_t)(sizeof(modes) / sizeof(modes[0]));
    if (!pPresentModes) {
        *pPresentModeCount = count;
        return VK_SUCCESS;
    }
    uint32_t toCopy = *pPresentModeCount < count ? *pPresentModeCount : count;
    memcpy(pPresentModes, modes, toCopy * sizeof(VkPresentModeKHR));
    *pPresentModeCount = toCopy;
    return VK_SUCCESS;
}
'''
q = 'static bool VULKAN_INTERNAL_QuerySwapchainSupport('
s = s.replace(q, safe_funcs + '\n' + q, 1)

# Replace crashing Turnip Mesa WSI calls with safe implementations
s = s.replace('renderer->vkGetPhysicalDeviceSurfaceFormatsKHR', 'SafeGetPhysicalDeviceSurfaceFormatsKHR')
s = s.replace('renderer->vkGetPhysicalDeviceSurfacePresentModesKHR', 'SafeGetPhysicalDeviceSurfacePresentModesKHR')

# Instrument CreateSwapchain & ClaimWindow
claim_old = 'Uint32 createSwapchainResult = VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);'
claim_new = 'fprintf(stderr, "[sdlgpu] VULKAN_ClaimWindow calling CreateSwapchain\\n");\n        Uint32 createSwapchainResult = VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);\n        fprintf(stderr, "[sdlgpu] VULKAN_INTERNAL_CreateSwapchain result: %u\\n", createSwapchainResult);'
s = s.replace(claim_old, claim_new, 1)

vk_create_old = 'vulkanResult = renderer->vkCreateSwapchainKHR(\n        renderer->logicalDevice,\n        &swapchainCreateInfo,\n        NULL,\n        &windowData->swapchain);'
vk_create_new = 'fprintf(stderr, "[sdlgpu] calling vkCreateSwapchainKHR (%ux%u, format %d, count %u)\\n", windowData->width, windowData->height, (int)windowData->format, requestedImageCount);\n    ' + vk_create_old + '\n    fprintf(stderr, "[sdlgpu] vkCreateSwapchainKHR returned %d\\n", (int)vulkanResult);'
s = s.replace(vk_create_old, vk_create_new, 1)

open(dst, 'w').write(s)
print('patched', count)
