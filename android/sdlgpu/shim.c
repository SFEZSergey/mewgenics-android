// SPDX-License-Identifier: Zlib
// The few SDL3 internals that SDL_gpu_vulkan.c (plus SDL_hashtable.c / SDL_properties.c) needs, for
// running SDL's Vulkan GPU backend natively under the game's x86 SDL_GPU frontend (see backend.c).
// No SDL3 library is in the process: memory, strings, atomics, locks, errors and logs map to bionic;
// the window is the APK's SDL2 window (hostwin.c), Vulkan is the APK's Turnip loader.
#include "SDL_internal.h"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#include "video/SDL_sysvideo.h"
#include "events/SDL_windowevents_c.h"

#include <android/log.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hostwin.h"

// ---- memory and strings
void *SDL_malloc(size_t size) { return malloc(size ? size : 1); }
void *SDL_calloc(size_t n, size_t size) { return calloc(n ? n : 1, size ? size : 1); }
void *SDL_realloc(void *p, size_t size) { return realloc(p, size ? size : 1); }
void SDL_free(void *p) { free(p); }
char *SDL_strdup(const char *s) { return strdup(s); }
size_t SDL_strlen(const char *s) { return strlen(s); }
int SDL_strcmp(const char *a, const char *b) { return strcmp(a, b); }
int SDL_memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
double SDL_atof(const char *s) { return atof(s); }
long long SDL_strtoll(const char *s, char **end, int base) { return strtoll(s, end, base); }
double SDL_round(double x) { return __builtin_round(x); }
int SDL_snprintf(char *text, size_t maxlen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(text, maxlen, fmt, ap);
    va_end(ap);
    return n;
}
int SDL_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vasprintf(strp, fmt, ap);
    va_end(ap);
    return n;
}
size_t SDL_utf8strlcpy(char *dst, const char *src, size_t dst_bytes)
{
    // Byte copy that does not cut a UTF-8 sequence.
    size_t n = strlen(src);
    if (dst_bytes == 0) return n;
    size_t len = n < dst_bytes - 1 ? n : dst_bytes - 1;
    while (len > 0 && len < n && (src[len] & 0xc0) == 0x80) --len;
    memcpy(dst, src, len);
    dst[len] = 0;
    return len;
}
int SDL_setenv_unsafe(const char *name, const char *value, int overwrite) { return setenv(name, value, overwrite); }
bool SDL_GetStringBoolean(const char *value, bool default_value)
{
    if (!value || !*value) return default_value;
    return !(*value == '0' || SDL_strcasecmp(value, "false") == 0);
}
int SDL_strcasecmp(const char *a, const char *b) { return strcasecmp(a, b); }

// ---- atomics
int SDL_AddAtomicInt(SDL_AtomicInt *a, int v) { return __atomic_fetch_add(&a->value, v, __ATOMIC_SEQ_CST); }
int SDL_GetAtomicInt(SDL_AtomicInt *a) { return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST); }
int SDL_SetAtomicInt(SDL_AtomicInt *a, int v) { return __atomic_exchange_n(&a->value, v, __ATOMIC_SEQ_CST); }
bool SDL_CompareAndSwapAtomicInt(SDL_AtomicInt *a, int oldval, int newval)
{
    return __atomic_compare_exchange_n(&a->value, &oldval, newval, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
Uint32 SDL_GetAtomicU32(SDL_AtomicU32 *a) { return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST); }
bool SDL_CompareAndSwapAtomicU32(SDL_AtomicU32 *a, Uint32 oldval, Uint32 newval)
{
    return __atomic_compare_exchange_n(&a->value, &oldval, newval, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// ---- threads and locks (recursive mutexes, as SDL's)
struct SDL_Mutex { pthread_mutex_t m; };
struct SDL_RWLock { pthread_rwlock_t l; };
SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *m = malloc(sizeof(*m));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m->m, &attr);
    pthread_mutexattr_destroy(&attr);
    return m;
}
void SDL_LockMutex(SDL_Mutex *m) { if (m) pthread_mutex_lock(&m->m); }
void SDL_UnlockMutex(SDL_Mutex *m) { if (m) pthread_mutex_unlock(&m->m); }
void SDL_DestroyMutex(SDL_Mutex *m) { if (m) { pthread_mutex_destroy(&m->m); free(m); } }
SDL_RWLock *SDL_CreateRWLock(void)
{
    SDL_RWLock *l = malloc(sizeof(*l));
    pthread_rwlock_init(&l->l, NULL);
    return l;
}
void SDL_LockRWLockForReading(SDL_RWLock *l) { if (l) pthread_rwlock_rdlock(&l->l); }
void SDL_LockRWLockForWriting(SDL_RWLock *l) { if (l) pthread_rwlock_wrlock(&l->l); }
void SDL_UnlockRWLock(SDL_RWLock *l) { if (l) pthread_rwlock_unlock(&l->l); }
void SDL_DestroyRWLock(SDL_RWLock *l) { if (l) { pthread_rwlock_destroy(&l->l); free(l); } }
SDL_ThreadID SDL_GetCurrentThreadID(void) { return (SDL_ThreadID)gettid(); }
void SDL_Delay(Uint32 ms) { usleep(ms * 1000u); }

bool SDL_ShouldInit(SDL_InitState *state)
{
    while (SDL_GetAtomicInt(&state->status) != SDL_INIT_STATUS_INITIALIZED) {
        if (SDL_CompareAndSwapAtomicInt(&state->status, SDL_INIT_STATUS_UNINITIALIZED, SDL_INIT_STATUS_INITIALIZING)) {
            state->thread = SDL_GetCurrentThreadID();
            return true;
        }
        SDL_Delay(1);
    }
    return false;
}
bool SDL_ShouldQuit(SDL_InitState *state)
{
    while (SDL_GetAtomicInt(&state->status) != SDL_INIT_STATUS_UNINITIALIZED) {
        if (SDL_CompareAndSwapAtomicInt(&state->status, SDL_INIT_STATUS_INITIALIZED, SDL_INIT_STATUS_UNINITIALIZING)) {
            state->thread = SDL_GetCurrentThreadID();
            return true;
        }
        SDL_Delay(1);
    }
    return false;
}
void SDL_SetInitialized(SDL_InitState *state, bool initialized)
{
    SDL_SetAtomicInt(&state->status, initialized ? SDL_INIT_STATUS_INITIALIZED : SDL_INIT_STATUS_UNINITIALIZED);
}

// ---- errors, logs, asserts (to stderr = box64.log, and logcat)
static __thread char error[1024];
bool SDL_SetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, sizeof(error), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[sdlgpu] error: %s\n", error);
    return false;
}
const char *SDL_GetError(void) { return error; }

static void logv(int prio, const char *fmt, va_list ap)
{
    char text[1024];
    vsnprintf(text, sizeof(text), fmt, ap);
    fprintf(stderr, "[sdlgpu] %s\n", text);
    __android_log_print(prio, "aps5sdlgpu", "%s", text);
}
#define LOGFN(name, prio, ...)                       \
    void name(__VA_ARGS__ const char *fmt, ...)      \
    {                                                \
        va_list ap;                                  \
        va_start(ap, fmt);                           \
        logv(prio, fmt, ap);                         \
        va_end(ap);                                  \
    }
LOGFN(SDL_Log, ANDROID_LOG_INFO, )
LOGFN(SDL_LogError, ANDROID_LOG_ERROR, int category, )
LOGFN(SDL_LogWarn, ANDROID_LOG_WARN, int category, )
LOGFN(SDL_LogInfo, ANDROID_LOG_INFO, int category, )
// Debug and verbose lines only with APS5_SDLGPU_VERBOSE.
void SDL_LogDebug(int category, const char *fmt, ...)
{
    static int verbose = -1;
    if (verbose < 0) verbose = getenv("APS5_SDLGPU_VERBOSE") != NULL;
    if (!verbose) return;
    va_list ap;
    va_start(ap, fmt);
    logv(ANDROID_LOG_DEBUG, fmt, ap);
    va_end(ap);
}
void SDL_LogVerbose(int category, const char *fmt, ...)
{
    static int verbose = -1;
    if (verbose < 0) verbose = getenv("APS5_SDLGPU_VERBOSE") != NULL;
    if (!verbose) return;
    va_list ap;
    va_start(ap, fmt);
    logv(ANDROID_LOG_VERBOSE, fmt, ap);
    va_end(ap);
}
SDL_AssertState SDL_ReportAssertion(SDL_AssertData *data, const char *func, const char *file, int line)
{
    fprintf(stderr, "[sdlgpu] assertion '%s' failed at %s:%d (%s)\n", data->condition, file, line, func);
    return SDL_ASSERTION_IGNORE;
}
void SDL_DestroySurface(SDL_Surface *surface) { (void)surface; }

// ---- window: the game's (guest) SDL_Window is only a key; size and surface come from the APK window.
static SDL_PropertiesID windowProps;
SDL_PropertiesID SDL_GetWindowProperties(SDL_Window *window)
{
    (void)window;
    if (!windowProps) windowProps = SDL_CreateProperties();
    return windowProps;
}
SDL_WindowID SDL_GetWindowID(SDL_Window *window) { (void)window; return 1; }
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h)
{
    (void)window;
    return Aps5HostWindowSize(w, h);
}
bool SDL_SyncWindow(SDL_Window *window) { (void)window; return true; }
void SDL_AddWindowEventWatch(SDL_WindowEventWatchPriority priority, SDL_EventFilter filter, void *userdata) { (void)priority; (void)filter; (void)userdata; }
void SDL_RemoveWindowEventWatch(SDL_WindowEventWatchPriority priority, SDL_EventFilter filter, void *userdata) { (void)priority; (void)filter; (void)userdata; }

// ---- Vulkan: the APK's loader (libadrenotools with Turnip, handle in APS5_VULKAN_HANDLE, as VideoOut)
static PFN_vkGetInstanceProcAddr instanceProc;
bool SDL_Vulkan_LoadLibrary(const char *path)
{
    (void)path;
    if (!instanceProc) instanceProc = (PFN_vkGetInstanceProcAddr)Aps5HostVulkanProc();
    return instanceProc ? true : SDL_SetError("no Vulkan loader (APS5_VULKAN_HANDLE)");
}
SDL_FunctionPointer SDL_Vulkan_GetVkGetInstanceProcAddr(void) { return (SDL_FunctionPointer)instanceProc; }
void SDL_Vulkan_UnloadLibrary(void) {}
char const *const *SDL_Vulkan_GetInstanceExtensions(Uint32 *count)
{
    static const char *const extensions[] = {"VK_KHR_surface", "VK_KHR_android_surface"};
    if (count) *count = 2;
    return extensions;
}
bool SDL_Vulkan_GetPresentationSupport(VkInstance instance, VkPhysicalDevice physicalDevice, Uint32 queueFamilyIndex)
{
    (void)instance; (void)physicalDevice; (void)queueFamilyIndex;
    return true;
}

static bool createSurface(SDL_VideoDevice *_this, SDL_Window *window, VkInstance instance, const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    (void)_this; (void)window;
    return Aps5HostCreateSurface((void *)instance, (void *)instanceProc, allocator, (void *)surface);
}
SDL_VideoDevice *SDL_GetVideoDevice(void)
{
    static SDL_VideoDevice device;
    device.Vulkan_CreateSurface = createSurface;
    device.name = "android";
    return &device;
}
