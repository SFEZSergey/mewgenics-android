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
        { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
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

# Force FIFO (VSync) present mode on swapchain
s = s.replace('swapchainCreateInfo.presentMode = SDLToVK_PresentMode[windowData->presentMode];', 'swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;')

# Instrument CreateSwapchain & ClaimWindow
claim_old = 'Uint32 createSwapchainResult = VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);'
claim_new = 'fprintf(stderr, "[sdlgpu] VULKAN_ClaimWindow calling CreateSwapchain\\n");\n        Uint32 createSwapchainResult = VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);\n        fprintf(stderr, "[sdlgpu] VULKAN_INTERNAL_CreateSwapchain result: %u\\n", createSwapchainResult);'
s = s.replace(claim_old, claim_new, 1)

vk_create_old = 'vulkanResult = renderer->vkCreateSwapchainKHR(\n        renderer->logicalDevice,\n        &swapchainCreateInfo,\n        NULL,\n        &windowData->swapchain);'
vk_create_new = 'fprintf(stderr, "[sdlgpu] calling vkCreateSwapchainKHR (%ux%u, format %d, count %u)\\n", windowData->width, windowData->height, (int)windowData->format, requestedImageCount);\n    ' + vk_create_old + '\n    fprintf(stderr, "[sdlgpu] vkCreateSwapchainKHR returned %d\\n", (int)vulkanResult);'
s = s.replace(vk_create_old, vk_create_new, 1)

# Allow Vulkan 1.1+ mobile drivers where VK_KHR_maintenance1 is core and not reported as extension
ext_old = '    return (supports->KHR_swapchain &&\n            supports->KHR_maintenance1);'
ext_new = '    // KHR_swapchain is required for presentation; maintenance1 is core in Vulkan 1.1+\n    return (supports->KHR_swapchain != 0);'
assert ext_old in s, "Could not find CheckDeviceExtensions return check"
s = s.replace(ext_old, ext_new, 1)

# Relax device feature checks in VULKAN_INTERNAL_IsDeviceSuitable for mobile GPUs (Mali, etc.)
feat_old = '''    if ((!deviceFeatures.independentBlend && features->desiredVulkan10DeviceFeatures.independentBlend) ||
        (!deviceFeatures.imageCubeArray && features->desiredVulkan10DeviceFeatures.imageCubeArray) ||
        (!deviceFeatures.depthClamp && features->desiredVulkan10DeviceFeatures.depthClamp) ||
        (!deviceFeatures.shaderClipDistance && features->desiredVulkan10DeviceFeatures.shaderClipDistance) ||
        (!deviceFeatures.drawIndirectFirstInstance && features->desiredVulkan10DeviceFeatures.drawIndirectFirstInstance) ||
        (!deviceFeatures.sampleRateShading && features->desiredVulkan10DeviceFeatures.sampleRateShading) ||
        (!deviceFeatures.samplerAnisotropy && features->desiredVulkan10DeviceFeatures.samplerAnisotropy)) {
        return 0;
    }'''

feat_new = '''    {
        VkPhysicalDeviceProperties devProps;
        renderer->vkGetPhysicalDeviceProperties(physicalDevice, &devProps);
        fprintf(stderr, "[sdlgpu] Evaluating device: %s (API %u.%u.%u, driver %u, type %u)\\n",
                devProps.deviceName,
                VK_API_VERSION_MAJOR(devProps.apiVersion),
                VK_API_VERSION_MINOR(devProps.apiVersion),
                VK_API_VERSION_PATCH(devProps.apiVersion),
                devProps.driverVersion, devProps.deviceType);
        fprintf(stderr, "[sdlgpu] Available features: blend=%d cube=%d depthClamp=%d clip=%d drawIndirect=%d sampleRate=%d aniso=%d\\n",
                deviceFeatures.independentBlend, deviceFeatures.imageCubeArray,
                deviceFeatures.depthClamp, deviceFeatures.shaderClipDistance,
                deviceFeatures.drawIndirectFirstInstance, deviceFeatures.sampleRateShading,
                deviceFeatures.samplerAnisotropy);

        // Mali-G720's first-frame GPU fault can be caused by an out-of-range shader buffer/image
        // access. Enable Vulkan robustness on Mali when supported; APS5_SDLGPU_ROBUST=0 opts out,
        // while =1 enables the same diagnostic mode on other devices.
        const char *robustEnv = getenv("APS5_SDLGPU_ROBUST");
        const bool robustRequested = robustEnv ? strcmp(robustEnv, "0") != 0 : (strstr(devProps.deviceName, "Mali") != NULL);
        VkBool32 robustBufferEnabled = VK_FALSE;
        VkBool32 robustImageEnabled = VK_FALSE;
        if (robustRequested) {
            if (deviceFeatures.robustBufferAccess) {
                features->desiredVulkan10DeviceFeatures.robustBufferAccess = VK_TRUE;
                robustBufferEnabled = VK_TRUE;
            }
            if (devProps.apiVersion >= VK_MAKE_VERSION(1, 3, 0)) {
                VkPhysicalDeviceVulkan13Features supported13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
                VkPhysicalDeviceFeatures2 supported = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &supported13 };
                renderer->vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
                if (supported13.robustImageAccess) {
                    if (!features->usesCustomVulkanOptions) {
                        features->usesCustomVulkanOptions = true;
                        features->desiredApiVersion = VK_MAKE_VERSION(1, 3, 0);
                        SDL_zero(features->desiredVulkan11DeviceFeatures);
                        SDL_zero(features->desiredVulkan12DeviceFeatures);
                        SDL_zero(features->desiredVulkan13DeviceFeatures);
                        features->desiredVulkan11DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
                        features->desiredVulkan12DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                        features->desiredVulkan13DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                    }
                    const Uint32 apiMajor = VK_API_VERSION_MAJOR(features->desiredApiVersion);
                    const Uint32 apiMinor = VK_API_VERSION_MINOR(features->desiredApiVersion);
                    if (apiMajor > 1 || (apiMajor == 1 && apiMinor >= 3)) {
                        features->desiredVulkan13DeviceFeatures.robustImageAccess = VK_TRUE;
                        robustImageEnabled = VK_TRUE;
                    }
                }
            }
        }
        if (robustRequested)
            fprintf(stderr, "[sdlgpu] robustness requested: buffer=%u image=%u (device %s)\\n",
                    robustBufferEnabled, robustImageEnabled, devProps.deviceName);
    }

    // Clamp desired features to what hardware actually supports instead of rejecting device
    if (!deviceFeatures.independentBlend) features->desiredVulkan10DeviceFeatures.independentBlend = VK_FALSE;
    if (!deviceFeatures.imageCubeArray) features->desiredVulkan10DeviceFeatures.imageCubeArray = VK_FALSE;
    if (!deviceFeatures.depthClamp) features->desiredVulkan10DeviceFeatures.depthClamp = VK_FALSE;
    if (!deviceFeatures.shaderClipDistance) features->desiredVulkan10DeviceFeatures.shaderClipDistance = VK_FALSE;
    if (!deviceFeatures.drawIndirectFirstInstance) features->desiredVulkan10DeviceFeatures.drawIndirectFirstInstance = VK_FALSE;
    if (!deviceFeatures.sampleRateShading) features->desiredVulkan10DeviceFeatures.sampleRateShading = VK_FALSE;
    if (!deviceFeatures.samplerAnisotropy) features->desiredVulkan10DeviceFeatures.samplerAnisotropy = VK_FALSE;'''
assert feat_old in s, "Could not find VULKAN_INTERNAL_IsDeviceSuitable feature check"
s = s.replace(feat_old, feat_new, 1)

# Instrument device rejection/acceptance in VULKAN_INTERNAL_IsDeviceSuitable
queue_old = '''    if (*queueFamilyIndex == SDL_MAX_UINT32) {
        // Somehow no graphics queues existed. Compute-only device?
        return 0;
    }

    // FIXME: Need better structure for checking vs storing swapchain support details
    return 1;'''

queue_new = '''    if (*queueFamilyIndex == SDL_MAX_UINT32) {
        fprintf(stderr, "[sdlgpu] Device rejected: no graphics queue family with presentation\\n");
        return 0;
    }

    fprintf(stderr, "[sdlgpu] Device ACCEPTED: queueFamilyIndex=%u\\n", *queueFamilyIndex);
    return 1;'''
assert queue_old in s, "Could not find queueFamilyIndex return check"
s = s.replace(queue_old, queue_new, 1)

# The SDL_GPU bridge uses this query to choose a CPU-decoded upload format when a Vulkan driver
# cannot sample the compressed format supplied by the title.
format_query = '''
__attribute__((visibility("default"))) bool Aps5SdlGpu_VulkanSupportsTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkFormatProperties properties;
    if ((Uint32)format >= SDL_arraysize(SDLToVK_TextureFormat)) {
        return false;
    }
    renderer->vkGetPhysicalDeviceFormatProperties(
        renderer->physicalDevice,
        SDLToVK_TextureFormat[format],
        &properties);
    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

'''
anchor = 'static SDL_GPUTexture *VULKAN_CreateTexture('
assert anchor in s, "Could not find VULKAN_CreateTexture anchor"
s = s.replace(anchor, format_query + anchor, 1)

open(dst, 'w').write(s)
print('patched', count)
