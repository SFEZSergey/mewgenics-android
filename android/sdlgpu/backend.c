// SPDX-License-Identifier: Zlib
// SDL's Vulkan GPU backend as the SDL_GPU driver of a PS5 title (Mewgenics): box64 replaces the
// title's Agc backend CreateDevice with Aps5SdlGpu_CreateDevice (BOX64 aps5hot, APS5_SDLGPU_VULKAN=1)
// and turns every slot of the returned SDL_GPUDevice into a bridge to the adapters below, so the
// title's x86 SDL_GPU frontend drives native Vulkan directly: no AGC command buffers, no PM4.
// The SDL_GPUDevice layout is the title's SDL (main, OpenXR era); CreateShader swaps the title's
// PSSL blobs for SPIR-V by content hash (<game>/sdlgpu/<fnv1a64>.spv, tools/mew-sdlgpu-shaders.sh).
#include "SDL_internal.h"
#include "gpu/SDL_sysgpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define EXPORT __attribute__((visibility("default")))

extern SDL_GPUBootstrap VulkanDriver;

static SDL_GPUDevice real;
static SDL_GPUTexture *tableTexture;  // bitmap_atlas_info, see noteTableTexture
static SDL_GPUBuffer *tableBuffer;
static Uint32 tableBytes;
static char shaderDir[512] = "sdlgpu";

typedef struct MappedShader
{
    Uint64 hash;
    void *code;
    size_t size;
} MappedShader;
static MappedShader mapped[512];
static int mappedCount;
static SDL_Mutex *mappedLock;

static Uint64 lastShaderHash;
static Uint64 fnv1a(const Uint8 *p, size_t size)
{
    Uint64 h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static void *readFile(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) { free(data); data = NULL; }
    fclose(f);
    *size = data ? (size_t)n : 0;
    return data;
}

// PSSL (the title's only shader format) -> SPIR-V of the same program, by content hash.
static bool Aps5MapShader(SDL_GPUShaderCreateInfo *info)
{
    if (info->format == SDL_GPU_SHADERFORMAT_SPIRV) return true;
    const Uint64 hash = fnv1a(info->code, info->code_size);
    lastShaderHash = hash;
    SDL_LockMutex(mappedLock);
    MappedShader *hit = NULL;
    for (int i = 0; i < mappedCount && !hit; ++i)
        if (mapped[i].hash == hash) hit = &mapped[i];
    if (!hit && mappedCount < (int)SDL_arraysize(mapped)) {
        char path[640];
        snprintf(path, sizeof(path), "%s/%016llx.spv", shaderDir, (unsigned long long)hash);
        size_t size = 0;
        void *code = readFile(path, &size);
        if (code) {
            hit = &mapped[mappedCount++];
            hit->hash = hash;
            hit->code = code;
            hit->size = size;
        }
    }
    SDL_UnlockMutex(mappedLock);
    fprintf(stderr, "[sdlgpu] shader %016llx (%zu bytes, stage %d, format 0x%x, samplers %u, storage textures %u, storage buffers %u, uniform buffers %u): %s\n",
            (unsigned long long)hash, info->code_size, (int)info->stage, (unsigned)info->format, info->num_samplers, info->num_storage_textures,
            info->num_storage_buffers, info->num_uniform_buffers, hit ? "SPIR-V" : "MISSING");
    if (!hit) return SDL_SetError("no SPIR-V for shader %016llx", (unsigned long long)hash);
    // The vertex "storage texture" of the title (its bitmap_atlas_info table) is a storage buffer in
    // the engine's SPIR-V: declared as one here, bound as one by BindVertexStorageTextures.
    if (info->stage == SDL_GPU_SHADERSTAGE_VERTEX && info->num_storage_textures) {
        info->num_storage_buffers += info->num_storage_textures;
        info->num_storage_textures = 0;
    }
    info->code = hit->code;
    info->code_size = hit->size;
    info->format = SDL_GPU_SHADERFORMAT_SPIRV;
    info->entrypoint = "main";
    return true;
}

#include "adapters.inc"

// ---- Texture descriptions, for the logs (first sighting of each bound one).
typedef struct KnownTexture
{
    SDL_GPUTexture *texture;
    SDL_GPUTextureCreateInfo info;
    bool logged;
} KnownTexture;
// Open addressing by pointer (looked up on every sampler bind; a reused pointer overwrites its entry).
#define KNOWN_BITS 13
static KnownTexture known[1u << KNOWN_BITS];
static Uint32 knownSlot(const void *p) { return (Uint32)((((Uint64)(uintptr_t)p >> 4) * 0x9e3779b97f4a7c15ull) >> (64 - KNOWN_BITS)); }
static void rememberTexture(SDL_GPUTexture *texture, const SDL_GPUTextureCreateInfo *info)
{
    if (!texture) return;
    Uint32 i = knownSlot(texture);
    for (Uint32 n = 0; n < SDL_arraysize(known) && known[i].texture && known[i].texture != texture; ++n) i = (i + 1) & (SDL_arraysize(known) - 1);
    known[i].info = *info;
    known[i].logged = false;
    known[i].texture = texture;
}
static const KnownTexture *findTexture(const void *p)
{
    if (!p) return NULL;
    Uint32 i = knownSlot(p);
    for (Uint32 n = 0; n < SDL_arraysize(known) && known[i].texture; ++n, i = (i + 1) & (SDL_arraysize(known) - 1))
        if (known[i].texture == p) return &known[i];
    return NULL;
}
static void logTexture(const char *what, SDL_GPUTexture *texture)
{
    {
        KnownTexture *k = (KnownTexture *)findTexture(texture);
        if (!k) return;
        if (!k->logged) {
            k->logged = true;
            fprintf(stderr, "[sdlgpu] %s %p: type %d format %d usage 0x%x %ux%ux%u levels %u\n", what, (void *)texture, (int)k->info.type, (int)k->info.format,
                    (unsigned)k->info.usage, k->info.width, k->info.height, k->info.layer_count_or_depth, k->info.num_levels);
        }
        return;
    }
}

// ---- Letterbox: the title draws its 16:9 canvas (1920x1080 after tools/mew1080.sh) from the top-left
// corner of whatever it acquires, while the Android swapchain is the whole panel (e.g. 2344x1080). It
// gets a canvas-sized texture instead, scaled into the middle of the real swapchain image at submit.
static Uint32 canvasW = 1920, canvasH = 1080;
static SDL_GPUTexture *canvas;
static SDL_GPUTextureFormat canvasFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
typedef struct PendingPresent
{
    SDL_GPUCommandBuffer *commandBuffer;
    SDL_GPUTexture *swapchain;
    Uint32 w, h;
} PendingPresent;
static PendingPresent pending[16];
static SDL_Mutex *presentLock;

static SDL_GPUTexture *canvasFor(SDL_GPURenderer *renderer, SDL_Window *window)
{
    const SDL_GPUTextureFormat format = real.GetSwapchainTextureFormat(renderer, window);
    if (canvas && format == canvasFormat) return canvas;
    if (canvas) real.ReleaseTexture(renderer, canvas);
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = canvasW;
    info.height = canvasH;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    canvas = real.CreateTexture(renderer, &info);
    canvasFormat = canvas ? format : SDL_GPU_TEXTUREFORMAT_INVALID;
    fprintf(stderr, "[sdlgpu] letterbox canvas %ux%u, format %d: %p\n", canvasW, canvasH, (int)format, (void *)canvas);
    return canvas;
}

static uint64_t acquired(bool ok, uint64_t commandBuffer, uint64_t window, uint64_t texture, uint64_t width, uint64_t height)
{
    SDL_GPUTexture **out = (SDL_GPUTexture **)(uintptr_t)texture;
    if (!ok || !*out) return ok;
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)commandBuffer;
    Uint32 *w = (Uint32 *)(uintptr_t)width, *h = (Uint32 *)(uintptr_t)height;
    SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
    SDL_LockMutex(presentLock);
    SDL_GPUTexture *target = canvasFor(renderer, (SDL_Window *)(uintptr_t)window);
    for (int i = 0; target && i < (int)SDL_arraysize(pending); ++i) {
        if (!pending[i].commandBuffer || pending[i].commandBuffer == cb) {
            pending[i] = (PendingPresent){cb, *out, w ? *w : 0, h ? *h : 0};
            *out = target;
            if (w) *w = canvasW;
            if (h) *h = canvasH;
            break;
        }
    }
    SDL_UnlockMutex(presentLock);
    return 1;
}

static uint64_t L_AcquireSwapchainTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
    return acquired(A_AcquireSwapchainTexture(a0, a1, a2, a3, a4) != 0, a0, a1, a2, a3, a4);
}
static uint64_t L_WaitAndAcquireSwapchainTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
    return acquired(A_WaitAndAcquireSwapchainTexture(a0, a1, a2, a3, a4) != 0, a0, a1, a2, a3, a4);
}

// Before a command buffer holding a present is submitted (or dropped): the canvas goes into the
// middle of the swapchain image at the panel's aspect, black around it.
static bool finishPresent(uint64_t commandBuffer, bool blit)
{
    bool found = false;
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)commandBuffer;
    SDL_LockMutex(presentLock);
    for (int i = 0; i < (int)SDL_arraysize(pending); ++i) {
        if (pending[i].commandBuffer != cb) continue;
        const PendingPresent p = pending[i];
        pending[i].commandBuffer = NULL;
        found = true;
        if (blit && canvas && p.w && p.h) {
            Uint32 w = p.w, h = (Uint32)((Uint64)p.w * canvasH / canvasW);
            if (h > p.h) { h = p.h; w = (Uint32)((Uint64)p.h * canvasW / canvasH); }
            SDL_GPUBlitInfo info = {0};
            info.source = (SDL_GPUBlitRegion){canvas, 0, 0, 0, 0, canvasW, canvasH};
            info.destination = (SDL_GPUBlitRegion){p.swapchain, 0, 0, (p.w - w) / 2, (p.h - h) / 2, w, h};
            info.load_op = SDL_GPU_LOADOP_CLEAR;
            info.clear_color = (SDL_FColor){0, 0, 0, 1};
            info.filter = SDL_GPU_FILTER_LINEAR;
            real.Blit(cb, &info);
        }
        break;
    }
    SDL_UnlockMutex(presentLock);
    return found;
}
// Calls per slot, every 5 s (APS5_SDLGPU_COUNTS=0 turns it off).
static void dumpCounts(void)
{
    static Uint64 last;
    static int enabled = -1;
    if (enabled < 0) { const char *v = getenv("APS5_SDLGPU_COUNTS"); enabled = !(v && *v == '0'); }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const Uint64 now = (Uint64)ts.tv_sec;
    if (!enabled || now < last + 5) return;
    last = now;
    char text[2048] = "";
    for (int i = 0; i < (int)SDL_arraysize(slotCalls); ++i) {
        const unsigned long n = __atomic_exchange_n(&slotCalls[i], 0, __ATOMIC_RELAXED);
        if (!n) continue;
        char one[64];
        snprintf(one, sizeof(one), " %s %lu", slotNames[i], n);
        strncat(text, one, sizeof(text) - strlen(text) - 1);
    }
    fprintf(stderr, "[sdlgpu] calls (5 s):%s\n", text);
}

// ---- Frame rate and frame dumps for the session collector (tools/sesscap.sh), as VideoOut did on the
// Agc path: "[fps] N (frame M)" every 5 s; while the file APS5_DUMP_TRIGGER exists, the canvas of one
// frame (at most one per second) goes to frame_000.bmp next to it, scaled down by APS5_DUMP_SCALE.
static Uint64 presents;
static void countPresent(void)
{
    static Uint64 since, startFrames;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const Uint64 ms = (Uint64)ts.tv_sec * 1000 + (Uint64)ts.tv_nsec / 1000000;
    ++presents;
    if (!since) { since = ms; startFrames = presents; return; }
    if (ms - since >= 5000) {
        fprintf(stderr, "[fps] %.2f (frame %llu)\n", (double)(presents - startFrames) * 1000.0 / (double)(ms - since), (unsigned long long)presents);
        since = ms;
        startFrames = presents;
    }
}

static SDL_GPUTransferBuffer *dumpBuffer;
static bool dumpWanted(void)
{
    static Uint64 last;
    const char *trigger = getenv("APS5_DUMP_TRIGGER");
    if (!trigger || access(trigger, F_OK) != 0 || !canvas) return false;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if ((Uint64)ts.tv_sec < last + 1) return false;
    last = (Uint64)ts.tv_sec;
    return true;
}
// Records the canvas download into the frame's command buffer (before it is submitted).
static bool recordDump(SDL_GPUCommandBuffer *cb)
{
    SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
    if (!dumpBuffer) dumpBuffer = real.CreateTransferBuffer(renderer, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, canvasW * canvasH * 4u, "dump");
    if (!dumpBuffer) return false;
    real.BeginCopyPass(cb);
    const SDL_GPUTextureRegion from = {canvas, 0, 0, 0, 0, 0, canvasW, canvasH, 1};
    const SDL_GPUTextureTransferInfo to = {dumpBuffer, 0, 0, 0};
    real.DownloadFromTexture(cb, &from, &to);
    real.EndCopyPass(cb);
    return true;
}
// After the submit: waits for the GPU and writes a 24-bit BMP.
static void writeDump(SDL_GPURenderer *renderer)
{
    real.Wait(renderer);
    const Uint8 *rgba = real.MapTransferBuffer(renderer, dumpBuffer, false);
    if (!rgba) return;
    const char *trigger = getenv("APS5_DUMP_TRIGGER");
    const char *slash = strrchr(trigger, '/');
    char path[600];
    snprintf(path, sizeof(path), "%.*s/frame_000.bmp", slash ? (int)(slash - trigger) : 1, slash ? trigger : ".");
    const char *scaleText = getenv("APS5_DUMP_SCALE");
    Uint32 scale = scaleText ? (Uint32)atoi(scaleText) : 1;
    if (scale < 1 || scale > 8) scale = 1;
    const Uint32 w = canvasW / scale, h = canvasH / scale, row = (w * 3 + 3) & ~3u;
    FILE *f = fopen(path, "wb");
    if (f) {
        const bool bgra = canvasFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || canvasFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        Uint8 header[54] = {'B', 'M'};
        const Uint32 fileSize = 54 + row * h, offset = 54, infoSize = 40, planesBits = 1 | (24u << 16), imageSize = row * h;
        memcpy(header + 2, &fileSize, 4); memcpy(header + 10, &offset, 4); memcpy(header + 14, &infoSize, 4);
        memcpy(header + 18, &w, 4); memcpy(header + 22, &h, 4); memcpy(header + 26, &planesBits, 4); memcpy(header + 34, &imageSize, 4);
        fwrite(header, 1, 54, f);
        Uint8 *line = calloc(1, row);
        for (Uint32 y = 0; y < h; ++y) {
            const Uint8 *src = rgba + (size_t)(canvasH - 1 - y * scale) * canvasW * 4;
            for (Uint32 x = 0; x < w; ++x) {
                const Uint8 *px = src + (size_t)x * scale * 4;
                line[x * 3 + 0] = bgra ? px[0] : px[2];
                line[x * 3 + 1] = px[1];
                line[x * 3 + 2] = bgra ? px[2] : px[0];
            }
            fwrite(line, 1, row, f);
        }
        free(line);
        fclose(f);
        fprintf(stderr, "[autodump] %s %ux%u\n", path, w, h);
    }
    real.UnmapTransferBuffer(renderer, dumpBuffer);
}

// ---- Texture dump for diagnosis: while <dir of APS5_DUMP_TRIGGER>/texdump exists, levels 0..3 of layer 0
// of the largest uploaded 2D array texture (the title's bitmap atlas) go to tex_L<n>.raw next to it,
// with a tex_L<n>.txt "width height format". One dump per trigger appearance.
static Uint32 blockBytes(SDL_GPUTextureFormat f, Uint32 *block);
static SDL_GPUTexture *atlasTexture;
static SDL_GPUTextureCreateInfo atlasInfo;
static void noteAtlas(SDL_GPUTexture *texture, const SDL_GPUTextureCreateInfo *info)
{
    if (texture && info->type == SDL_GPU_TEXTURETYPE_2D_ARRAY && !(info->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) &&
        (!atlasTexture || (Uint64)info->width * info->height * info->layer_count_or_depth > (Uint64)atlasInfo.width * atlasInfo.height * atlasInfo.layer_count_or_depth)) {
        atlasTexture = texture;
        atlasInfo = *info;
    }
}
static void texDump(SDL_GPUCommandBuffer *cb)
{
    static bool done;
    const char *trigger = getenv("APS5_DUMP_TRIGGER");
    if (!trigger || !atlasTexture) return;
    const char *slash = strrchr(trigger, '/');
    char path[640];
    snprintf(path, sizeof(path), "%.*s/texdump", slash ? (int)(slash - trigger) : 1, slash ? trigger : ".");
    if (access(path, F_OK) != 0) { done = false; return; }
    if (done) return;
    done = true;
    SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
    Uint32 block = 1;
    const Uint32 bytes = blockBytes(atlasInfo.format, &block);
    if (!bytes) return;
    for (Uint32 level = 0; level < 4 && level < atlasInfo.num_levels; ++level) {
        const Uint32 w = atlasInfo.width >> level, h = atlasInfo.height >> level;
        const Uint32 size = ((w + block - 1) / block) * ((h + block - 1) / block) * bytes;
        SDL_GPUTransferBuffer *tb = real.CreateTransferBuffer(renderer, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, size, "texdump");
        SDL_GPUCommandBuffer *own = real.AcquireCommandBuffer(renderer);
        if (!tb || !own) return;
        real.BeginCopyPass(own);
        const SDL_GPUTextureRegion from = {atlasTexture, level, 0, 0, 0, 0, w, h, 1};
        const SDL_GPUTextureTransferInfo to = {tb, 0, 0, 0};
        real.DownloadFromTexture(own, &from, &to);
        real.EndCopyPass(own);
        SDL_GPUFence *fence = real.SubmitAndAcquireFence(own);
        if (fence) { real.WaitForFences(renderer, true, &fence, 1); real.ReleaseFence(renderer, fence); }
        const void *data = real.MapTransferBuffer(renderer, tb, false);
        char name[700];
        snprintf(name, sizeof(name), "%.*s/tex_L%u.raw", slash ? (int)(slash - trigger) : 1, slash ? trigger : ".", level);
        FILE *f = data ? fopen(name, "wb") : NULL;
        if (f) { fwrite(data, 1, size, f); fclose(f); }
        snprintf(name, sizeof(name), "%.*s/tex_L%u.txt", slash ? (int)(slash - trigger) : 1, slash ? trigger : ".", level);
        f = fopen(name, "w");
        if (f) { fprintf(f, "%u %u %d\n", w, h, (int)atlasInfo.format); fclose(f); }
        if (data) real.UnmapTransferBuffer(renderer, tb);
        real.ReleaseTransferBuffer(renderer, tb);
        fprintf(stderr, "[sdlgpu] texdump level %u %ux%u format %d (%u bytes)\n", level, w, h, (int)atlasInfo.format, size);
    }
}

static uint64_t submitFrame(uint64_t a0, uint64_t (*submit)(uint64_t))
{
    texDump((SDL_GPUCommandBuffer *)(uintptr_t)a0);
    dumpCounts();
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
    const bool presenting = finishPresent(a0, true);
    const bool dump = presenting && dumpWanted() && recordDump(cb);
    const uint64_t result = submit(a0);
    if (presenting) countPresent();
    if (dump) writeDump(renderer);
    return result;
}
static uint64_t L_Submit(uint64_t a0)
{
    return submitFrame(a0, A_Submit);
}
static uint64_t L_SubmitAndAcquireFence(uint64_t a0)
{
    return submitFrame(a0, A_SubmitAndAcquireFence);
}
static uint64_t L_Cancel(uint64_t a0)
{
    finishPresent(a0, false);
    return A_Cancel(a0);
}

// Buffers by pointer (what the title binds as vertex storage textures may be one).
typedef struct KnownBuffer
{
    SDL_GPUBuffer *buffer;
    Uint32 usage, size;
} KnownBuffer;
static KnownBuffer knownBuffers[4096];
static int knownBufferNext;
static uint64_t L_CreateBuffer(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    const uint64_t buffer = A_CreateBuffer(a0, a1, a2, a3);
    if (buffer) {
        KnownBuffer *slot = &knownBuffers[__atomic_fetch_add(&knownBufferNext, 1, __ATOMIC_RELAXED) % (int)SDL_arraysize(knownBuffers)];
        *slot = (KnownBuffer){(SDL_GPUBuffer *)(uintptr_t)buffer, (Uint32)a1, (Uint32)a2};
    }
    return buffer;
}
static const KnownBuffer *findBuffer(const void *p)
{
    for (int i = 0; p && i < (int)SDL_arraysize(knownBuffers); ++i)
        if (knownBuffers[i].buffer == p) return &knownBuffers[i];
    return NULL;
}
static bool isTexture(const void *p)
{
    return findTexture(p) != NULL;
}

// Vertex storage textures (see Aps5MapShader): a buffer is bound as the storage buffer of that slot.
static uint64_t L_BindVertexStorageTextures(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    static int reports;
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    void *const *items = (void *const *)(uintptr_t)a2;
    const Uint32 count = (Uint32)a3 < 8 ? (Uint32)a3 : 8;
    SDL_GPUBuffer *buffers[8];
    Uint32 bound = 0;
    for (Uint32 i = 0; i < count; ++i) {
        const KnownBuffer *b = findBuffer(items[i]);
        if (reports < 40) {
            ++reports;
            if (b) fprintf(stderr, "[sdlgpu] vertex table slot %u: buffer %p usage 0x%x size %u\n", (Uint32)a1 + i, items[i], b->usage, b->size);
            else if (isTexture(items[i])) logTexture("vertex table texture", (SDL_GPUTexture *)items[i]);
            else fprintf(stderr, "[sdlgpu] vertex table slot %u: unknown %p\n", (Uint32)a1 + i, items[i]);
        }
        if (b) buffers[bound++] = (SDL_GPUBuffer *)items[i];
    }
    if (bound) real.BindVertexStorageBuffers(cb, (Uint32)a1, buffers, bound);
    else if (tableBuffer) real.BindVertexStorageBuffers(cb, 0, &tableBuffer, 1);
    return 0;
}

// ---- bitmap_atlas_info: the title keeps it in an R32_FLOAT texture one row high (GRAPHICS_STORAGE_READ)
// and never binds it to the vertex shaders (Agc reaches it some other way); the engine's SPIR-V reads
// a storage buffer there. Every upload into that texture is repeated into a mirror buffer, which is
// bound whenever the title binds its vertex storage textures.
static void noteTableTexture(SDL_GPURenderer *renderer, SDL_GPUTexture *texture, const SDL_GPUTextureCreateInfo *info)
{
    if (!texture || info->type != SDL_GPU_TEXTURETYPE_2D || info->height != 1 || info->format != SDL_GPU_TEXTUREFORMAT_R32_FLOAT ||
        !(info->usage & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ))
        return;
    SDL_GPUBuffer *buffer = real.CreateBuffer(renderer, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, info->width * 4u, "bitmap_atlas_info");
    fprintf(stderr, "[sdlgpu] bitmap_atlas_info texture %p (%u floats): mirror buffer %p\n", (void *)texture, info->width, (void *)buffer);
    if (!buffer) return;
    // The previous mirror stays alive (draws already recorded may use it); the table is recreated rarely.
    tableTexture = texture;
    tableBuffer = buffer;
    tableBytes = info->width * 4u;
}

// ---- Uploads into 2D array textures: the title addresses array layers with z/d (as in a 3D
// texture; Agc takes either), Vulkan needs the layer index and z = 0, d = 1 per layer.

// Bytes per texel block and block size (only what a layer split needs; 0 = unknown).
static Uint32 blockBytes(SDL_GPUTextureFormat f, Uint32 *block)
{
    *block = 1;
    switch (f) {
    case SDL_GPU_TEXTUREFORMAT_A8_UNORM: case SDL_GPU_TEXTUREFORMAT_R8_UNORM: return 1;
    case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM: case SDL_GPU_TEXTUREFORMAT_R16_FLOAT: case SDL_GPU_TEXTUREFORMAT_R16_UNORM: return 2;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM: case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM: case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB: case SDL_GPU_TEXTUREFORMAT_R32_FLOAT: case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM: return 4;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT: case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT: return 8;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT: return 16;
    case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM: case SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM: *block = 4; return 8;
    case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM: case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM: case SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM: *block = 4; return 16;
    default: return 0;
    }
}

static __thread SDL_GPUTransferBuffer *uploadRealigned;  // per call, released after the upload is recorded
static void uploadTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    static int reports;
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    SDL_GPUTextureTransferInfo source = *(const SDL_GPUTextureTransferInfo *)(uintptr_t)a1;
    SDL_GPUTextureRegion region = *(const SDL_GPUTextureRegion *)(uintptr_t)a2;
    static unsigned long cycled, uploads;
    const bool cycle = cycleWrites && (a3 & 0xff) != 0;
    ++uploads;
    if ((a3 & 0xff) != 0 && ++cycled <= 8)
        fprintf(stderr, "[sdlgpu] upload with cycle=true #%lu (of %lu uploads): mip %u layer %u at %u,%u size %ux%u%s\n", cycled, uploads, region.mip_level, region.layer,
                region.x, region.y, region.w, region.h, cycleWrites ? "" : " -> in place");
    const KnownTexture *t = findTexture(region.texture);
    static struct { void *texture; Uint32 mip; } seen[1024];
    static int seenCount;
    bool fresh = true;
    for (int i = 0; i < seenCount; ++i)
        if (seen[i].texture == region.texture && seen[i].mip == region.mip_level) { fresh = false; break; }
    if (fresh && seenCount < 1024) { seen[seenCount].texture = region.texture; seen[seenCount].mip = region.mip_level; ++seenCount; }
    if (fresh && reports < 400) {
        ++reports;
        fprintf(stderr, "[sdlgpu] upload: texture type %d format %d %ux%ux%u levels %u <- mip %u layer %u at %u,%u,%u size %ux%ux%u (offset %u, %u px/row, %u rows/layer)\n",
                t ? (int)t->info.type : -1, t ? (int)t->info.format : -1, t ? t->info.width : 0, t ? t->info.height : 0, t ? t->info.layer_count_or_depth : 0,
                t ? t->info.num_levels : 0, region.mip_level, region.layer, region.x, region.y, region.z, region.w, region.h, region.d, source.offset,
                source.pixels_per_row, source.rows_per_layer);
    }
    // Vulkan needs the buffer offset of a texture upload to be a multiple of the texel block size (16 for
    // BC formats, else at least 4); the title's atlas uploads start at offset = 4 (mod 16), which Agc
    // takes and Turnip rounds down (every block shifted: noise on all levels). Such regions are copied
    // from the title's transfer buffer into an aligned one here (the data is already written).
    if (t) {
        Uint32 block = 1, bytes = blockBytes(t->info.format, &block);
        const Uint32 align = bytes > 4 ? bytes : 4;
        if (bytes && source.offset % align) {
            static unsigned long realigned;
            const Uint32 rowPixels = source.pixels_per_row ? source.pixels_per_row : region.w;
            const Uint32 layerRows = source.rows_per_layer ? source.rows_per_layer : region.h;
            const Uint32 rowBytes = (rowPixels + block - 1) / block * bytes;
            const Uint32 rows = (layerRows + block - 1) / block;
            const Uint32 depth = region.d ? region.d : 1;
            const Uint32 size = rowBytes * rows * depth;
            SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
            const Uint8 *from = real.MapTransferBuffer(renderer, source.transfer_buffer, false);
            SDL_GPUTransferBuffer *aligned = real.CreateTransferBuffer(renderer, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size, "aligned upload");
            Uint8 *to = aligned ? real.MapTransferBuffer(renderer, aligned, false) : NULL;
            if (from && to) {
                memcpy(to, from + source.offset, size);
                real.UnmapTransferBuffer(renderer, aligned);
                real.UnmapTransferBuffer(renderer, source.transfer_buffer);
                if (++realigned <= 5) fprintf(stderr, "[sdlgpu] upload offset %u realigned (%u bytes, format %d)\n", source.offset, size, (int)t->info.format);
                source.transfer_buffer = aligned;
                source.offset = 0;
                uploadRealigned = aligned;  // released by L_UploadToTexture once the upload is recorded
            } else {
                if (from) real.UnmapTransferBuffer(renderer, source.transfer_buffer);
                if (aligned) real.ReleaseTransferBuffer(renderer, aligned);
            }
        }
    }
    if (region.texture == tableTexture && tableBuffer && region.mip_level == 0 && region.y == 0 && (Uint64)(region.x + region.w) * 4u <= tableBytes) {
        const SDL_GPUTransferBufferLocation from = {source.transfer_buffer, source.offset};
        const SDL_GPUBufferRegion to = {tableBuffer, region.x * 4u, region.w * 4u};
        real.UploadToBuffer(cb, &from, &to, false);
    }
    if (t && (t->info.type == SDL_GPU_TEXTURETYPE_2D_ARRAY || t->info.type == SDL_GPU_TEXTURETYPE_CUBE || t->info.type == SDL_GPU_TEXTURETYPE_CUBE_ARRAY) &&
        (region.z != 0 || region.d > 1)) {
        const Uint32 layers = region.d ? region.d : 1;
        Uint32 block;
        const Uint32 bytes = blockBytes(t->info.format, &block);
        const Uint32 rowPixels = source.pixels_per_row ? source.pixels_per_row : region.w;
        const Uint32 rows = source.rows_per_layer ? source.rows_per_layer : region.h;
        const Uint64 layerBytes = (Uint64)((rowPixels + block - 1) / block) * ((rows + block - 1) / block) * bytes;
        if (layers > 1 && !bytes) {
            fprintf(stderr, "[sdlgpu] upload: %u layers of format %d not split (unknown size)\n", layers, (int)t->info.format);
        } else {
            const Uint32 first = region.layer + region.z;
            for (Uint32 k = 0; k < layers; ++k) {
                SDL_GPUTextureTransferInfo s = source;
                SDL_GPUTextureRegion r = region;
                s.offset = (Uint32)(source.offset + k * layerBytes);
                r.layer = first + k;
                r.z = 0;
                r.d = 1;
                real.UploadToTexture(cb, &s, &r, cycle && k == 0);
            }
            return;
        }
    }
    real.UploadToTexture(cb, &source, &region, cycle);
    // Experiment only (APS5_SDLGPU_AUTO_MIPS=1): build the chain after a whole level-0 upload. It made
    // the map wrong (gradient atlas rows bleed into each other when downsampled).
    if (t && t->info.num_levels > 1 && region.mip_level == 0 && region.x == 0 && region.y == 0 && region.w == t->info.width && region.h == t->info.height &&
        t->info.type == SDL_GPU_TEXTURETYPE_2D && getenv("APS5_SDLGPU_AUTO_MIPS") != NULL)
        real.GenerateMipmaps(cb, region.texture);
    return;
}

static uint64_t L_UploadToTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    uploadRealigned = NULL;
    uploadTexture(a0, a1, a2, a3);
    if (uploadRealigned) {
        SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
        real.ReleaseTransferBuffer(((CommandBufferCommonHeader *)cb)->device->driverData, uploadRealigned);
        uploadRealigned = NULL;
    }
    return 0;
}

static uint64_t L_CopyTextureToTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
{
    static int reports;
    const SDL_GPUTextureLocation *src = (const SDL_GPUTextureLocation *)(uintptr_t)a1, *dst = (const SDL_GPUTextureLocation *)(uintptr_t)a2;
    if (reports < 20) {
        ++reports;
        const KnownTexture *s = findTexture(src->texture), *d = findTexture(dst->texture);
        fprintf(stderr, "[sdlgpu] copy: type %d %ux%ux%u layer %u z %u -> type %d %ux%ux%u layer %u z %u, %ux%ux%u\n", s ? (int)s->info.type : -1, s ? s->info.width : 0,
                s ? s->info.height : 0, s ? s->info.layer_count_or_depth : 0, src->layer, src->z, d ? (int)d->info.type : -1, d ? d->info.width : 0,
                d ? d->info.height : 0, d ? d->info.layer_count_or_depth : 0, dst->layer, dst->z, (Uint32)a3, (Uint32)a4, (Uint32)a5);
    }
    const uint64_t result = A_CopyTextureToTexture(a0, a1, a2, a3, a4, a5, a6);
    const KnownTexture *target = findTexture(dst->texture);
    if (target && dst->mip_level == 0 && target->info.num_levels > 1 && (target->info.usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) &&
        target->info.type == SDL_GPU_TEXTURETYPE_2D && getenv("APS5_SDLGPU_TARGET_MIPS") != NULL)
        real.GenerateMipmaps((SDL_GPUCommandBuffer *)(uintptr_t)a0, dst->texture);
    return result;
}

// ---- Diagnostics: pipelines (state, shaders by PSSL hash) and the first render passes.
typedef struct ShaderName
{
    void *shader;
    Uint64 hash;
} ShaderName;
static ShaderName shaderNames[512];
static int shaderNameNext;
static Uint64 shaderHash(const void *shader)
{
    for (int i = 0; shader && i < (int)SDL_arraysize(shaderNames); ++i)
        if (shaderNames[i].shader == shader) return shaderNames[i].hash;
    return 0;
}
static uint64_t L_CreateShader(uint64_t a0, uint64_t a1)
{
    const uint64_t shader = A_CreateShader(a0, a1);
    if (shader) shaderNames[__atomic_fetch_add(&shaderNameNext, 1, __ATOMIC_RELAXED) % (int)SDL_arraysize(shaderNames)] = (ShaderName){(void *)(uintptr_t)shader, lastShaderHash};
    return shader;
}
// ---- Per-shader switches for finding a culprit: draws whose fragment shader hash (PSSL FNV, as in the
// logs) is listed in APS5_SDLGPU_SKIP_FS are dropped; in APS5_SDLGPU_NEAREST_FS their fragment samplers
// become point-filtered, level 0 only. Lists: comma-separated 16-digit hex.
static bool listed(const char *env, Uint64 hash)
{
    const char *list = getenv(env);
    char want[17];
    snprintf(want, sizeof(want), "%016llx", (unsigned long long)hash);
    return list && hash && (strstr(list, want) != NULL || strcmp(list, "all") == 0);
}
typedef struct PipelineFlags
{
    void *pipeline;
    int flags;  // 1 skip, 2 nearest
} PipelineFlags;
static PipelineFlags pipelineFlags[1024];
static int pipelineFlagNext;
static int flagsOf(const void *pipeline)
{
    if (!pipelineFlagNext) return 0;
    for (int i = 0; pipeline && i < (int)SDL_arraysize(pipelineFlags); ++i)
        if (pipelineFlags[i].pipeline == pipeline) return pipelineFlags[i].flags;
    return 0;
}
typedef struct BoundFlags
{
    SDL_GPUCommandBuffer *commandBuffer;
    int flags;
} BoundFlags;
static BoundFlags boundFlags[16];
static int currentFlags(SDL_GPUCommandBuffer *cb)
{
    for (int i = 0; i < (int)SDL_arraysize(boundFlags); ++i)
        if (boundFlags[i].commandBuffer == cb) return boundFlags[i].flags;
    return 0;
}
static uint64_t L_BindGraphicsPipeline(uint64_t a0, uint64_t a1)
{
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    const int flags = flagsOf((void *)(uintptr_t)a1);
    BoundFlags *slot = NULL;
    for (int i = 0; i < (int)SDL_arraysize(boundFlags) && !slot; ++i)
        if (boundFlags[i].commandBuffer == cb) slot = &boundFlags[i];
    for (int i = 0; i < (int)SDL_arraysize(boundFlags) && !slot && flags; ++i)
        if (!boundFlags[i].commandBuffer) slot = &boundFlags[i];
    if (slot) { slot->commandBuffer = flags ? cb : NULL; slot->flags = flags; }
    return A_BindGraphicsPipeline(a0, a1);
}
typedef struct SamplerInfo
{
    SDL_GPUSampler *sampler, *nearest;
    SDL_GPUSamplerCreateInfo info;
} SamplerInfo;
static SamplerInfo samplers[1024];
static int samplerNext;
static uint64_t L_CreateSampler(uint64_t a0, uint64_t a1)
{
    const uint64_t sampler = A_CreateSampler(a0, a1);
    if (sampler) {
        SamplerInfo *slot = &samplers[__atomic_fetch_add(&samplerNext, 1, __ATOMIC_RELAXED) % (int)SDL_arraysize(samplers)];
        slot->sampler = (SDL_GPUSampler *)(uintptr_t)sampler;
        slot->nearest = NULL;
        slot->info = *(const SDL_GPUSamplerCreateInfo *)(uintptr_t)a1;
        slot->info.props = 0;
    }
    return sampler;
}
static SDL_GPUSampler *nearestOf(SDL_GPURenderer *renderer, SDL_GPUSampler *sampler)
{
    for (int i = 0; sampler && i < (int)SDL_arraysize(samplers); ++i) {
        SamplerInfo *s = &samplers[i];
        if (s->sampler != sampler) continue;
        if (!s->nearest) {
            // APS5_SDLGPU_NEAREST_MODE: lod0 (default: filter kept, level 0), point (point filter,
            // level 0), mips (point filter, levels kept).
            const char *mode = getenv("APS5_SDLGPU_NEAREST_MODE");
            const bool lodOnly = !mode || strcmp(mode, "lod0") == 0, filterOnly = mode && strcmp(mode, "mips") == 0;
            SDL_GPUSamplerCreateInfo info = s->info;
            if (!lodOnly) {
                info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
                info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
                info.enable_anisotropy = false;
            }
            if (!filterOnly) info.min_lod = info.max_lod = 0;
            s->nearest = real.CreateSampler(renderer, &info);
        }
        return s->nearest ? s->nearest : sampler;
    }
    return sampler;
}
// Uploaded textures (the title's atlases) carry broken lower levels -> sampled at level 0 only;
// render targets keep their sampler (their levels are rebuilt from what was drawn, see L_EndRenderPass).
// Off by default since uploads are realigned (the atlas levels are right then; the PS5 shaders sample
// them with explicit LODs): APS5_SDLGPU_UPLOADED_LOD0=1 turns it on.
static bool uploadedWithLevels(const void *texture)
{
    const KnownTexture *t = findTexture(texture);
    return t && t->info.num_levels > 1 && !(t->info.usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
}
static uint64_t bindSamplers(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t (*bind)(uint64_t, uint64_t, uint64_t, uint64_t))
{
    static int rule = -1;
    if (rule < 0) { const char *v = getenv("APS5_SDLGPU_UPLOADED_LOD0"); rule = v && *v == '1'; }
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    const bool forced = (currentFlags(cb) & 2) != 0;
    if ((!rule && !forced) || (Uint32)a3 > 16) return bind(a0, a1, a2, a3);
    SDL_GPUTextureSamplerBinding bindings[16];
    memcpy(bindings, (const void *)(uintptr_t)a2, (Uint32)a3 * sizeof(*bindings));
    SDL_GPURenderer *renderer = ((CommandBufferCommonHeader *)cb)->device->driverData;
    for (Uint32 i = 0; i < (Uint32)a3; ++i)
        if (forced || uploadedWithLevels(bindings[i].texture)) bindings[i].sampler = nearestOf(renderer, bindings[i].sampler);
    return bind(a0, a1, (uint64_t)(uintptr_t)bindings, a3);
}
static uint64_t L_BindFragmentSamplers(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    return bindSamplers(a0, a1, a2, a3, A_BindFragmentSamplers);
}
static uint64_t L_BindVertexSamplers(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    return bindSamplers(a0, a1, a2, a3, A_BindVertexSamplers);
}
static uint64_t L_DrawIndexedPrimitives(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
    if (currentFlags((SDL_GPUCommandBuffer *)(uintptr_t)a0) & 1) return 0;
    return A_DrawIndexedPrimitives(a0, a1, a2, a3, a4, a5);
}
static uint64_t L_DrawPrimitives(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
    if (currentFlags((SDL_GPUCommandBuffer *)(uintptr_t)a0) & 1) return 0;
    return A_DrawPrimitives(a0, a1, a2, a3, a4);
}

static uint64_t L_CreateGraphicsPipeline(uint64_t a0, uint64_t a1)
{
    const SDL_GPUGraphicsPipelineCreateInfo *p = (const SDL_GPUGraphicsPipelineCreateInfo *)(uintptr_t)a1;
    const SDL_GPUDepthStencilState *d = &p->depth_stencil_state;
    char targets[256] = "";
    for (Uint32 i = 0; i < p->target_info.num_color_targets && i < 4; ++i) {
        const SDL_GPUColorTargetDescription *c = &p->target_info.color_target_descriptions[i];
        const SDL_GPUColorTargetBlendState *b = &c->blend_state;
        char one[64];
        snprintf(one, sizeof(one), " c%u fmt %d blend %d %d/%d/%d %d/%d/%d mask %s0x%x", i, (int)c->format, b->enable_blend, b->src_color_blendfactor, b->dst_color_blendfactor,
                 b->color_blend_op, b->src_alpha_blendfactor, b->dst_alpha_blendfactor, b->alpha_blend_op, b->enable_color_write_mask ? "" : "(off)", b->color_write_mask);
        strncat(targets, one, sizeof(targets) - strlen(targets) - 1);
    }
    const uint64_t pipeline = A_CreateGraphicsPipeline(a0, a1);
    const Uint64 fsHash = shaderHash(p->fragment_shader);
    const int flags = (listed("APS5_SDLGPU_SKIP_FS", fsHash) ? 1 : 0) | (listed("APS5_SDLGPU_NEAREST_FS", fsHash) ? 2 : 0);
    if (pipeline && flags) {
        pipelineFlags[__atomic_fetch_add(&pipelineFlagNext, 1, __ATOMIC_RELAXED) % (int)SDL_arraysize(pipelineFlags)] = (PipelineFlags){(void *)(uintptr_t)pipeline, flags};
        fprintf(stderr, "[sdlgpu] pipeline %p (fs %016llx): %s%s\n", (void *)(uintptr_t)pipeline, (unsigned long long)fsHash, flags & 1 ? "skipped " : "", flags & 2 ? "nearest samplers" : "");
    }
    fprintf(stderr, "[sdlgpu] pipeline %p: vs %016llx fs %016llx prim %d cull %d depth test %d write %d cmp %d | stencil %d cmp %d/%d ops f(%d %d %d) b(%d %d %d) masks %02x/%02x | ds %d fmt %d |%s\n",
            (void *)(uintptr_t)pipeline, (unsigned long long)shaderHash(p->vertex_shader), (unsigned long long)shaderHash(p->fragment_shader), (int)p->primitive_type,
            (int)p->rasterizer_state.cull_mode, d->enable_depth_test, d->enable_depth_write, (int)d->compare_op, d->enable_stencil_test, (int)d->front_stencil_state.compare_op,
            (int)d->back_stencil_state.compare_op, (int)d->front_stencil_state.fail_op, (int)d->front_stencil_state.pass_op, (int)d->front_stencil_state.depth_fail_op,
            (int)d->back_stencil_state.fail_op, (int)d->back_stencil_state.pass_op, (int)d->back_stencil_state.depth_fail_op, d->compare_mask, d->write_mask,
            p->target_info.has_depth_stencil_target, (int)p->target_info.depth_stencil_format, targets);
    return pipeline;
}
// Render targets with mip levels: the title draws (or copies) level 0 and samples lower levels for its
// blurred backgrounds -> the chain is rebuilt after each pass that drew level 0 (APS5_SDLGPU_NO_TARGET_MIPS=1 off).
typedef struct MipTargets
{
    SDL_GPUCommandBuffer *commandBuffer;
    SDL_GPUTexture *textures[4];
    int count;
} MipTargets;
static MipTargets mipTargets[16];
static SDL_Mutex *mipLock;

static uint64_t L_BeginRenderPass(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    {
        const SDL_GPUColorTargetInfo *targets = (const SDL_GPUColorTargetInfo *)(uintptr_t)a1;
        SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
        SDL_LockMutex(mipLock);
        MipTargets *slot = NULL;
        for (int i = 0; i < (int)SDL_arraysize(mipTargets) && !slot; ++i)
            if (mipTargets[i].commandBuffer == cb || !mipTargets[i].commandBuffer) slot = &mipTargets[i];
        if (slot) {
            slot->commandBuffer = cb;
            slot->count = 0;
            for (Uint32 i = 0; i < (Uint32)a2 && i < 4; ++i) {
                const KnownTexture *t = findTexture(targets[i].texture);
                if (t && t->info.num_levels > 1 && targets[i].mip_level == 0 && t->info.type == SDL_GPU_TEXTURETYPE_2D) slot->textures[slot->count++] = targets[i].texture;
            }
            if (!slot->count) slot->commandBuffer = NULL;
        }
        SDL_UnlockMutex(mipLock);
    }
    {
        const SDL_GPUColorTargetInfo *c = (const SDL_GPUColorTargetInfo *)(uintptr_t)a1;
        const SDL_GPUDepthStencilTargetInfo *d = (const SDL_GPUDepthStencilTargetInfo *)(uintptr_t)a3;
        char text[512] = "";
        for (Uint32 i = 0; i < (Uint32)a2 && i < 4; ++i) {
            const KnownTexture *t = findTexture(c[i].texture);
            char one[128];
            snprintf(one, sizeof(one), " c%u %ux%u mip %u load %d store %d clear %.2f,%.2f,%.2f,%.2f cycle %d", i, t ? t->info.width : 0, t ? t->info.height : 0, c[i].mip_level,
                     (int)c[i].load_op, (int)c[i].store_op, c[i].clear_color.r, c[i].clear_color.g, c[i].clear_color.b, c[i].clear_color.a, c[i].cycle);
            strncat(text, one, sizeof(text) - strlen(text) - 1);
        }
        if (d) {
            char one[160];
            snprintf(one, sizeof(one), " | depth load %d store %d clear %.3f stencil load %d store %d clear %u cycle %d", (int)d->load_op, (int)d->store_op, d->clear_depth,
                     (int)d->stencil_load_op, (int)d->stencil_store_op, d->clear_stencil, d->cycle);
            strncat(text, one, sizeof(text) - strlen(text) - 1);
        }
        // One line per distinct pass shape (targets, ops), at most 300.
        static Uint64 seen[300];
        static int seenCount;
        const Uint64 key = fnv1a((const Uint8 *)text, strlen(text));
        bool fresh = true;
        for (int i = 0; i < seenCount; ++i)
            if (seen[i] == key) { fresh = false; break; }
        if (fresh && seenCount < (int)SDL_arraysize(seen)) {
            seen[seenCount++] = key;
            fprintf(stderr, "[sdlgpu] pass%s\n", text);
        }
    }
    // PS5 semantics for "don't care": the memory simply is not touched, so the old contents stay (the
    // title draws parts of targets it opened with DONT_CARE and samples them later); on a tiler like
    // Adreno DONT_CARE leaves garbage tiles. -> LOAD / STORE. APS5_SDLGPU_KEEP_DONT_CARE=1 keeps them.
    static int keep = -1;
    if (keep < 0) keep = getenv("APS5_SDLGPU_KEEP_DONT_CARE") != NULL;
    {
        SDL_GPUColorTargetInfo colors[8];
        const Uint32 n = (Uint32)a2 < 8 ? (Uint32)a2 : 8;
        memcpy(colors, (const void *)(uintptr_t)a1, n * sizeof(*colors));
        for (Uint32 i = 0; i < n; ++i) {
            if (!keep && colors[i].load_op == SDL_GPU_LOADOP_DONT_CARE) colors[i].load_op = SDL_GPU_LOADOP_LOAD;
            if (!keep && colors[i].store_op == SDL_GPU_STOREOP_DONT_CARE) colors[i].store_op = SDL_GPU_STOREOP_STORE;
            if (!cycleWrites) colors[i].cycle = colors[i].cycle_resolve_texture = false;
        }
        SDL_GPUDepthStencilTargetInfo depth, *d = NULL;
        if (a3) {
            depth = *(const SDL_GPUDepthStencilTargetInfo *)(uintptr_t)a3;
            if (!cycleWrites) depth.cycle = false;
            if (!keep && depth.load_op == SDL_GPU_LOADOP_DONT_CARE) depth.load_op = SDL_GPU_LOADOP_LOAD;
            if (!keep && depth.store_op == SDL_GPU_STOREOP_DONT_CARE) depth.store_op = SDL_GPU_STOREOP_STORE;
            if (!keep && depth.stencil_load_op == SDL_GPU_LOADOP_DONT_CARE) depth.stencil_load_op = SDL_GPU_LOADOP_LOAD;
            if (!keep && depth.stencil_store_op == SDL_GPU_STOREOP_DONT_CARE) depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
            d = &depth;
        }
        return A_BeginRenderPass(a0, (uint64_t)(uintptr_t)colors, n, (uint64_t)(uintptr_t)d);
    }
}

// The title maps transfer buffers without cycling and refills them while uploads recorded from the
// previous contents are still pending (Agc copies at once; SDL copies when the command buffer runs):
// mapping always cycles, so a buffer in use is never overwritten. APS5_SDLGPU_CYCLE=1 passes it through.
static uint64_t L_MapTransferBuffer(uint64_t a0, uint64_t a1, uint64_t a2)
{
    static unsigned long maps, plain;
    ++maps;
    if (!(a2 & 0xff) && ++plain <= 5)
        fprintf(stderr, "[sdlgpu] map transfer buffer without cycle #%lu (of %lu maps)%s\n", plain, maps, cycleWrites ? "" : " -> cycled");
    return A_MapTransferBuffer(a0, a1, cycleWrites ? a2 : 1);
}

static uint64_t L_Blit(uint64_t a0, uint64_t a1)
{
    SDL_GPUBlitInfo info = *(const SDL_GPUBlitInfo *)(uintptr_t)a1;
    if (!cycleWrites) info.cycle = false;
    if (info.load_op == SDL_GPU_LOADOP_DONT_CARE && !getenv("APS5_SDLGPU_KEEP_DONT_CARE")) info.load_op = SDL_GPU_LOADOP_LOAD;
    return A_Blit(a0, (uint64_t)(uintptr_t)&info);
}

static uint64_t L_EndRenderPass(uint64_t a0)
{
    const uint64_t result = A_EndRenderPass(a0);
    SDL_GPUCommandBuffer *cb = (SDL_GPUCommandBuffer *)(uintptr_t)a0;
    SDL_GPUTexture *pending[4];
    int count = 0;
    SDL_LockMutex(mipLock);
    for (int i = 0; i < (int)SDL_arraysize(mipTargets); ++i) {
        if (mipTargets[i].commandBuffer != cb) continue;
        count = mipTargets[i].count;
        SDL_memcpy(pending, mipTargets[i].textures, sizeof(pending));
        mipTargets[i].commandBuffer = NULL;
        break;
    }
    SDL_UnlockMutex(mipLock);
    for (int i = 0; i < count; ++i) real.GenerateMipmaps(cb, pending[i]);
    return result;
}

static uint64_t L_SetViewport(uint64_t a0, uint64_t a1)
{
    static int reports;
    const SDL_GPUViewport *v = (const SDL_GPUViewport *)(uintptr_t)a1;
    if (reports < 40) { ++reports; fprintf(stderr, "[sdlgpu] viewport %.1f,%.1f %.1fx%.1f depth %.2f..%.2f\n", v->x, v->y, v->w, v->h, v->min_depth, v->max_depth); }
    return A_SetViewport(a0, a1);
}
static uint64_t L_SetScissor(uint64_t a0, uint64_t a1)
{
    static int reports;
    const SDL_Rect *r = (const SDL_Rect *)(uintptr_t)a1;
    if (reports < 80) { ++reports; fprintf(stderr, "[sdlgpu] scissor %d,%d %dx%d\n", r->x, r->y, r->w, r->h); }
    return A_SetScissor(a0, a1);
}

static uint64_t L_GenerateMipmaps(uint64_t a0, uint64_t a1)
{
    static int reports;
    const KnownTexture *t = findTexture((void *)(uintptr_t)a1);
    if (reports < 60) {
        ++reports;
        fprintf(stderr, "[sdlgpu] mipmaps: %p type %d format %d usage 0x%x %ux%ux%u levels %u\n", (void *)(uintptr_t)a1, t ? (int)t->info.type : -1, t ? (int)t->info.format : -1,
                t ? (unsigned)t->info.usage : 0, t ? t->info.width : 0, t ? t->info.height : 0, t ? t->info.layer_count_or_depth : 0, t ? t->info.num_levels : 0);
    }
    return A_GenerateMipmaps(a0, a1);
}

// Textures the shaders read as storage images (the title's bitmap_atlas_info table): logged once each.
// New textures start as zero, as the PS5's freshly mapped memory does: the title samples texture parts
// and levels it never writes (atlas cells, lower mips), which hold old data in a reused Vulkan
// allocation. Render targets are cleared per level and layer; other textures get zero uploads from a
// shared zero transfer buffer. Depth/stencil and 3D textures are left alone. APS5_SDLGPU_NO_ZERO_INIT=1 off.
static SDL_GPUTransferBuffer *zeroBuffer;
static Uint32 zeroBytes;
static void zeroTexture(SDL_GPURenderer *renderer, SDL_GPUTexture *texture, const SDL_GPUTextureCreateInfo *info)
{
    static int disabled = -1;
    if (disabled < 0) disabled = getenv("APS5_SDLGPU_NO_ZERO_INIT") != NULL;
    if (disabled || !texture || info->type == SDL_GPU_TEXTURETYPE_3D || (info->usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) return;
    const Uint32 layers = info->layer_count_or_depth ? info->layer_count_or_depth : 1;
    const bool target = (info->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) != 0;
    Uint32 block = 1, bytes = 0;
    if (!target) {
        bytes = blockBytes(info->format, &block);
        if (!bytes) {
            fprintf(stderr, "[sdlgpu] zero init: format %d not handled (%ux%u)\n", (int)info->format, info->width, info->height);
            return;
        }
        const Uint32 need = ((info->width + block - 1) / block) * ((info->height + block - 1) / block) * bytes;
        if (need > zeroBytes) {
            if (zeroBuffer) real.ReleaseTransferBuffer(renderer, zeroBuffer);
            zeroBuffer = real.CreateTransferBuffer(renderer, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, need, "zero");
            void *map = zeroBuffer ? real.MapTransferBuffer(renderer, zeroBuffer, false) : NULL;
            if (!map) { zeroBuffer = NULL; zeroBytes = 0; return; }
            memset(map, 0, need);
            real.UnmapTransferBuffer(renderer, zeroBuffer);
            zeroBytes = need;
        }
    }
    SDL_GPUCommandBuffer *cb = real.AcquireCommandBuffer(renderer);
    if (!cb) return;
    if (!target) real.BeginCopyPass(cb);
    for (Uint32 level = 0; level < info->num_levels; ++level) {
        const Uint32 w = info->width >> level ? info->width >> level : 1, h = info->height >> level ? info->height >> level : 1;
        for (Uint32 layer = 0; layer < layers; ++layer) {
            if (target) {
                SDL_GPUColorTargetInfo t = {0};
                t.texture = texture;
                t.mip_level = level;
                t.layer_or_depth_plane = layer;
                t.clear_color = (SDL_FColor){0, 0, 0, 0};
                t.load_op = SDL_GPU_LOADOP_CLEAR;
                t.store_op = SDL_GPU_STOREOP_STORE;
                real.BeginRenderPass(cb, &t, 1, NULL);
                real.EndRenderPass(cb);
            } else {
                const SDL_GPUTextureTransferInfo from = {zeroBuffer, 0, 0, 0};
                const SDL_GPUTextureRegion to = {texture, level, layer, 0, 0, 0, w, h, 1};
                real.UploadToTexture(cb, &from, &to, false);
            }
        }
    }
    if (!target) real.EndCopyPass(cb);
    // Wait until it is done: a texture still in use when the title first writes it with cycle=true
    // would be cycled to a fresh (unzeroed) internal image.
    SDL_GPUFence *fence = real.SubmitAndAcquireFence(cb);
    if (fence) {
        real.WaitForFences(renderer, true, &fence, 1);
        real.ReleaseFence(renderer, fence);
    }
}

static uint64_t L_CreateTexture(uint64_t a0, uint64_t a1)
{
    const SDL_GPUTextureCreateInfo *info = (const SDL_GPUTextureCreateInfo *)(uintptr_t)a1;
    const uint64_t texture = A_CreateTexture(a0, a1);
    rememberTexture((SDL_GPUTexture *)(uintptr_t)texture, info);
    noteAtlas((SDL_GPUTexture *)(uintptr_t)texture, info);
    zeroTexture((SDL_GPURenderer *)(uintptr_t)a0, (SDL_GPUTexture *)(uintptr_t)texture, info);
    if (texture && (info->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
        fprintf(stderr, "[sdlgpu] target %ux%u format %d levels %u layers %u\n", info->width, info->height, (int)info->format, info->num_levels, info->layer_count_or_depth);
    noteTableTexture((SDL_GPURenderer *)(uintptr_t)a0, (SDL_GPUTexture *)(uintptr_t)texture, info);
    static int reports;
    if (reports < 60) {
        ++reports;
        fprintf(stderr, "[sdlgpu] texture %p: type %d format %d usage 0x%x %ux%ux%u levels %u\n", (void *)(uintptr_t)texture, (int)info->type, (int)info->format,
                (unsigned)info->usage, info->width, info->height, info->layer_count_or_depth, info->num_levels);
    }
    if (info->usage & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE))
        fprintf(stderr, "[sdlgpu] storage texture %p: type %d format %d usage 0x%x %ux%ux%u levels %u\n", (void *)(uintptr_t)texture, (int)info->type, (int)info->format,
                (unsigned)info->usage, info->width, info->height, info->layer_count_or_depth, info->num_levels);
    return texture;
}

static void installOverrides(SDL_GPUDevice *device)
{
    const char *size = getenv("APS5_SDLGPU_CANVAS");  // WxH of the title's canvas; 0 = no letterbox
    if (size && sscanf(size, "%ux%u", &canvasW, &canvasH) != 2) canvasW = canvasH = 0;
    if (!presentLock) presentLock = SDL_CreateMutex();
    if (canvasW && canvasH) {
        device->AcquireSwapchainTexture = (void *)L_AcquireSwapchainTexture;
        device->WaitAndAcquireSwapchainTexture = (void *)L_WaitAndAcquireSwapchainTexture;
        device->Submit = (void *)L_Submit;
        device->SubmitAndAcquireFence = (void *)L_SubmitAndAcquireFence;
        device->Cancel = (void *)L_Cancel;
    }
    device->CreateTexture = (void *)L_CreateTexture;
    device->BindVertexStorageTextures = (void *)L_BindVertexStorageTextures;
    device->CreateBuffer = (void *)L_CreateBuffer;
    device->UploadToTexture = (void *)L_UploadToTexture;
    device->CopyTextureToTexture = (void *)L_CopyTextureToTexture;
    device->CreateShader = (void *)L_CreateShader;
    device->CreateGraphicsPipeline = (void *)L_CreateGraphicsPipeline;
    device->BeginRenderPass = (void *)L_BeginRenderPass;
    device->SetViewport = (void *)L_SetViewport;
    device->SetScissor = (void *)L_SetScissor;
    device->GenerateMipmaps = (void *)L_GenerateMipmaps;
    device->Blit = (void *)L_Blit;
    device->MapTransferBuffer = (void *)L_MapTransferBuffer;
    device->BindGraphicsPipeline = (void *)L_BindGraphicsPipeline;
    device->CreateSampler = (void *)L_CreateSampler;
    device->BindFragmentSamplers = (void *)L_BindFragmentSamplers;
    device->DrawIndexedPrimitives = (void *)L_DrawIndexedPrimitives;
    device->DrawPrimitives = (void *)L_DrawPrimitives;
    cycleWrites = getenv("APS5_SDLGPU_CYCLE") != NULL;
    if (!mipLock) mipLock = SDL_CreateMutex();
    if (getenv("APS5_SDLGPU_TARGET_MIPS") != NULL) device->EndRenderPass = (void *)L_EndRenderPass;  // experiment (washes images out)
    device->BindVertexSamplers = (void *)L_BindVertexSamplers;
}

EXPORT void Aps5SdlGpu_SetShaderDir(const char *dir)
{
    if (dir && *dir) snprintf(shaderDir, sizeof(shaderDir), "%s", dir);
}

// The title's CreateDevice(debug_mode, prefer_low_power, props). The props are the title's SDL
// properties (unknown here): the Vulkan backend gets its defaults. kinds/count: slotKinds above,
// for box64 to pick the bridge wrapper of each slot.
EXPORT SDL_GPUDevice *Aps5SdlGpu_CreateDevice(bool debugMode, bool preferLowPower, Uint32 props, const unsigned char **kinds, int *count)
{
    (void)props;
    if (!mappedLock) mappedLock = SDL_CreateMutex();
    fprintf(stderr, "[sdlgpu] creating the native Vulkan SDL_GPU device (debug %d, low power %d), shaders from %s\n", debugMode, preferLowPower, shaderDir);
    // Our own properties: SPIR-V (what CreateShader hands over), everything else at its default.
    const SDL_PropertiesID native = SDL_CreateProperties();
    SDL_SetBooleanProperty(native, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    if (!VulkanDriver.PrepareDriver(SDL_GetVideoDevice(), native)) {
        fprintf(stderr, "[sdlgpu] Vulkan PrepareDriver failed: %s\n", SDL_GetError());
        SDL_DestroyProperties(native);
        return NULL;
    }
    SDL_GPUDevice *device = VulkanDriver.CreateDevice(debugMode, preferLowPower, native);
    SDL_DestroyProperties(native);
    if (!device) {
        fprintf(stderr, "[sdlgpu] Vulkan CreateDevice failed: %s\n", SDL_GetError());
        return NULL;
    }
    real = *device;
    installAdapters(device);
    installOverrides(device);
    // The title asks for PSSL; CreateShader turns it into SPIR-V.
    device->shader_formats = SDL_GPU_SHADERFORMAT_PRIVATE;
    *kinds = slotKinds;
    *count = (int)sizeof(slotKinds);
    fprintf(stderr, "[sdlgpu] device ready: %d slots\n", *count);
    return device;
}
