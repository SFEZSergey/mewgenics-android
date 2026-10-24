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
open(dst, 'w').write(s)
print('patched', count)
