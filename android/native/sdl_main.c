// SDL_main для APK: открывает Vulkan через libadrenotools со своим Turnip, настраивает окружение
// (как run-pvz.sh) и запускает x86-64 app.elf во встроенном box64 в этом же процессе.
// Аргументы от GameActivity: <путь к app.elf> <nativeLibraryDir> <filesDir>.
#include <adrenotools/driver.h>
#include <android/log.h>
#include <dirent.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <jni.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// Виртуальный геймпад (сенсорный слой GameActivity/TouchControlsView). libSceVideoOut находит этот
// объект через dlsym(libmain.so, "aps5_virtual_pad") и подмешивает его к вводу: sequence растёт на
// каждое изменение, sticks = lx | ly << 8 | rx << 16 | ry << 24.
struct aps5_virtual_pad {
    _Atomic uint32_t sequence;
    _Atomic uint32_t buttons;
    _Atomic uint32_t sticks;
};
__attribute__((visibility("default"))) struct aps5_virtual_pad aps5_virtual_pad = {0, 0, 0x80808080u};

JNIEXPORT void JNICALL Java_org_anyps5_android_GameActivity_nativeVirtualPad(JNIEnv* env, jclass cls, jint buttons, jint lx, jint ly, jint rx, jint ry) {
    (void)env;
    (void)cls;
    const uint32_t sticks = (uint32_t)(lx & 0xff) | (uint32_t)(ly & 0xff) << 8 | (uint32_t)(rx & 0xff) << 16 | (uint32_t)(ry & 0xff) << 24;
    atomic_store(&aps5_virtual_pad.buttons, (uint32_t)buttons);
    atomic_store(&aps5_virtual_pad.sticks, sticks);
    atomic_fetch_add(&aps5_virtual_pad.sequence, 1);
}

// Сенсорная мышь (TouchControlsView, касания мимо кнопок в режиме тачпада): события уходят в libSceMouse
// той же функцией, что и события настоящей мыши из SDL (MouseInput в libSceVideoOut). Структура повторяет
// MouseInputEvent из core/libs/prx/libSceMouse/include/MouseState.hpp (передаётся по ссылке).
struct aps5_mouse_event {
    int32_t x, y, wheel, tilt;
    uint32_t button;
    bool pressed, reset_buttons, connection_change, connected;
};
typedef void (*mouse_publish_fn)(const struct aps5_mouse_event *);
static char mouse_library[PATH_MAX];

// libSceMouse.so грузит загрузчик нативной группы AnyPS5, когда игра стартует: до этого события некуда слать.
static mouse_publish_fn mouse_publish(void) {
    static mouse_publish_fn publish;
    if (publish == NULL && mouse_library[0] != '\0') {
        void *library = dlopen(mouse_library, RTLD_NOW | RTLD_NOLOAD);
        // Имя в форке AnyPS5 и в основном репозитории (там суффикс _nid_no_patch).
        if (library != NULL) publish = (mouse_publish_fn)dlsym(library, "MousePublishInput_nid_postfix");
        if (library != NULL && publish == NULL) publish = (mouse_publish_fn)dlsym(library, "MousePublishInput_nid_no_patch");
    }
    return publish;
}

JNIEXPORT void JNICALL Java_org_anyps5_android_GameActivity_nativeTouchMouse(JNIEnv *env, jclass cls, jint dx, jint dy, jint button, jboolean pressed) {
    (void)env;
    (void)cls;
    const mouse_publish_fn publish = mouse_publish();
    if (publish == NULL) return;
    // На Android мышь сначала не подключена (игре хватает геймпада): первое касание её подключает.
    static bool connected;
    if (!connected) {
        const struct aps5_mouse_event connect = {.connection_change = true, .connected = true};
        publish(&connect);
        connected = true;
    }
    const struct aps5_mouse_event event = {.x = dx, .y = dy, .button = (uint32_t)button, .pressed = pressed, .connected = true};
    publish(&event);
}

typedef void (*mouse_publish_direct_fn)(int32_t, int32_t, uint32_t, bool, uintptr_t);

static mouse_publish_direct_fn mouse_publish_direct(void) {
    static mouse_publish_direct_fn publish_direct;
    if (publish_direct == NULL) {
        void *library = NULL;
        if (mouse_library[0] != '\0') library = dlopen(mouse_library, RTLD_NOW | RTLD_NOLOAD);
        if (library == NULL) library = dlopen("libSceMouse.so", RTLD_NOW | RTLD_NOLOAD);
        if (library != NULL) {
            publish_direct = (mouse_publish_direct_fn)dlsym(library, "MousePublishDirect_nid_postfix");
            if (publish_direct == NULL) publish_direct = (mouse_publish_direct_fn)dlsym(library, "MousePublishDirect");
        }
        if (publish_direct == NULL) publish_direct = (mouse_publish_direct_fn)dlsym(RTLD_DEFAULT, "MousePublishDirect_nid_postfix");
        if (publish_direct == NULL) publish_direct = (mouse_publish_direct_fn)dlsym(RTLD_DEFAULT, "MousePublishDirect");
    }
    return publish_direct;
}

extern uintptr_t Aps5GetAppElfDelta(void) __attribute__((weak));

static uintptr_t get_app_elf_delta_fallback(void) {
    if (Aps5GetAppElfDelta != NULL) {
        uintptr_t d = Aps5GetAppElfDelta();
        if (d != 0) return d;
    }
    typedef uintptr_t (*delta_fn)(void);
    static delta_fn dlsym_delta = NULL;
    if (dlsym_delta == NULL) {
        void *box = dlopen("libbox64.so", RTLD_NOW | RTLD_NOLOAD);
        if (box != NULL) dlsym_delta = (delta_fn)dlsym(box, "Aps5GetAppElfDelta");
        if (dlsym_delta == NULL) dlsym_delta = (delta_fn)dlsym(RTLD_DEFAULT, "Aps5GetAppElfDelta");
    }
    if (dlsym_delta != NULL) {
        uintptr_t d = dlsym_delta();
        if (d != 0) return d;
    }
    // Fallback: считываем базовый адрес app.elf из /proc/self/maps
    static uintptr_t parsed_delta = 0;
    if (parsed_delta != 0) return parsed_delta;
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "app.elf") != NULL) {
                uintptr_t start = 0;
                if (sscanf(line, "%lx-", &start) == 1 && start != 0) {
                    parsed_delta = start;
                    LOG(ANDROID_LOG_INFO, "found app.elf base in maps: 0x%lx", (unsigned long)parsed_delta);
                    break;
                }
            }
        }
        fclose(maps);
    }
    return parsed_delta;
}

JNIEXPORT void JNICALL Java_org_anyps5_android_GameActivity_nativeTouchDirect(JNIEnv *env, jclass cls, jint x, jint y, jint button, jboolean pressed) {
    (void)env;
    (void)cls;
    const uintptr_t delta = get_app_elf_delta_fallback();
    const mouse_publish_direct_fn pub_direct = mouse_publish_direct();
    if (pub_direct != NULL) {
        pub_direct(x, y, (uint32_t)button, pressed, delta);
        return;
    }
    const mouse_publish_fn publish = mouse_publish();
    if (publish == NULL) return;
    static bool connected;
    if (!connected) {
        const struct aps5_mouse_event connect = {.connection_change = true, .connected = true};
        publish(&connect);
        connected = true;
    }
    const struct aps5_mouse_event event = {.x = 0, .y = 0, .button = (uint32_t)button, .pressed = pressed, .connected = true};
    publish(&event);
}

// Ввод текста для игры (sceImeDialog, x86 под box64): libSceImeDialog находит этот объект по адресу
// из APS5_TEXT_INPUT, кладёт начальный текст и ставит state = 1; GameActivity опрашивает его, показывает
// поле ввода с клавиатурой Android и возвращает текст (state 3) или отмену (4). Игра прерывает ввод
// state = 5 (диалог закрывается), после результата игра сама возвращает state в 0.
enum { APS5_TEXT_IDLE = 0, APS5_TEXT_REQUESTED = 1, APS5_TEXT_SHOWN = 2, APS5_TEXT_DONE = 3, APS5_TEXT_CANCELLED = 4, APS5_TEXT_ABORTED = 5 };
#define APS5_TEXT_CAPACITY 1024
struct aps5_text_input {
    _Atomic uint32_t state;
    uint32_t max_length;
    uint32_t type;
    uint32_t reserved;
    uint16_t text[APS5_TEXT_CAPACITY];
};
__attribute__((visibility("default"))) struct aps5_text_input aps5_text_input;

// Запрос игры: начальный текст (и переход в «показан») или null, если запроса нет.
JNIEXPORT jstring JNICALL Java_org_anyps5_android_GameActivity_nativeTextInputRequest(JNIEnv* env, jclass cls) {
    (void)cls;
    uint32_t expected = APS5_TEXT_REQUESTED;
    if (!atomic_compare_exchange_strong(&aps5_text_input.state, &expected, APS5_TEXT_SHOWN)) return NULL;
    jsize length = 0;
    while (length < APS5_TEXT_CAPACITY - 1 && aps5_text_input.text[length] != 0) ++length;
    return (*env)->NewString(env, (const jchar*)aps5_text_input.text, length);
}

JNIEXPORT jint JNICALL Java_org_anyps5_android_GameActivity_nativeTextInputMaxLength(JNIEnv* env, jclass cls) {
    (void)env; (void)cls;
    const uint32_t max = aps5_text_input.max_length;
    return (jint)(max == 0 || max >= APS5_TEXT_CAPACITY ? APS5_TEXT_CAPACITY - 1 : max);
}

// Показанный диалог ещё нужен (игра не прервала ввод).
JNIEXPORT jboolean JNICALL Java_org_anyps5_android_GameActivity_nativeTextInputPending(JNIEnv* env, jclass cls) {
    (void)env; (void)cls;
    return atomic_load(&aps5_text_input.state) == APS5_TEXT_SHOWN ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_org_anyps5_android_GameActivity_nativeTextInputDone(JNIEnv* env, jclass cls, jstring text) {
    (void)cls;
    if (atomic_load(&aps5_text_input.state) != APS5_TEXT_SHOWN) return;
    if (text == NULL) {
        atomic_store(&aps5_text_input.state, APS5_TEXT_CANCELLED);
        return;
    }
    const jsize length = (*env)->GetStringLength(env, text);
    const uint32_t max = (uint32_t)Java_org_anyps5_android_GameActivity_nativeTextInputMaxLength(env, cls);
    const jsize copied = length < (jsize)max ? length : (jsize)max;
    (*env)->GetStringRegion(env, text, 0, copied, (jchar*)aps5_text_input.text);
    aps5_text_input.text[copied] = 0;
    atomic_store(&aps5_text_input.state, APS5_TEXT_DONE);
}

// Автопилот для тестов (APS5_AUTOPILOT=<файл сценария>): нажимает кнопки через тот же виртуальный
// геймпад, что и сенсорное управление, — нажатия видит только игра. Строки сценария:
//   wait <секунды> | press <кнопка> [мс] | stick <x> <y> <мс> (левый стик, -1..1) | mark <метка> | done
// Кнопки: cross circle square triangle l1 r1 l2 r2 l3 r3 options touchpad up down left right.
// Метки и ход пишутся в box64.log строками "[autopilot] ...".
static double autopilot_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void autopilot_send(uint32_t buttons, int lx, int ly) {
    const uint32_t sticks = (uint32_t)(lx & 0xff) | (uint32_t)(ly & 0xff) << 8 | 0x80u << 16 | 0x80u << 24;
    atomic_store(&aps5_virtual_pad.buttons, buttons);
    atomic_store(&aps5_virtual_pad.sticks, sticks);
    atomic_fetch_add(&aps5_virtual_pad.sequence, 1);
}

static void autopilot_sleep(double seconds) {
    if (seconds <= 0) return;
    struct timespec t = {(time_t)seconds, (long)((seconds - (double)(time_t)seconds) * 1e9)};
    while (nanosleep(&t, &t) != 0) {}
}

static uint32_t autopilot_button(const char *name) {
    static const struct { const char *name; uint32_t bit; } names[] = {
        {"cross", 0x4000}, {"circle", 0x2000}, {"square", 0x8000}, {"triangle", 0x1000},
        {"l1", 0x400}, {"r1", 0x800}, {"l2", 0x100}, {"r2", 0x200}, {"l3", 0x2}, {"r3", 0x4},
        {"options", 0x8}, {"touchpad", 0x100000}, {"up", 0x10}, {"right", 0x20}, {"down", 0x40}, {"left", 0x80},
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) if (strcmp(names[i].name, name) == 0) return names[i].bit;
    return 0;
}

static void *autopilot(void *arg) {
    const char *path = (const char *)arg;
    FILE *script = fopen(path, "r");
    if (script == NULL) { fprintf(stderr, "[autopilot] cannot open %s\n", path); return NULL; }
    const double start = autopilot_now();
    char line[256];
    while (fgets(line, sizeof(line), script) != NULL) {
        char command[32] = {0}, word[96] = {0};
        double a = 0, b = 0, c = 0;
        if (sscanf(line, "%31s", command) != 1 || command[0] == '#') continue;
        if (strcmp(command, "wait") == 0 && sscanf(line, "%*s %lf", &a) == 1) {
            autopilot_sleep(a);
        } else if (strcmp(command, "press") == 0 && sscanf(line, "%*s %95s", word) == 1) {
            const uint32_t bit = autopilot_button(word);
            if (sscanf(line, "%*s %*s %lf", &a) != 1) a = 150;
            fprintf(stderr, "[autopilot] %.1f s: press %s\n", autopilot_now() - start, word);
            autopilot_send(bit, 128, 128);
            autopilot_sleep(a / 1000.0);
            autopilot_send(0, 128, 128);
        } else if (strcmp(command, "stick") == 0 && sscanf(line, "%*s %lf %lf %lf", &a, &b, &c) == 3) {
            fprintf(stderr, "[autopilot] %.1f s: stick %.1f %.1f\n", autopilot_now() - start, a, b);
            autopilot_send(0, (int)(128 + a * 127), (int)(128 + b * 127));
            autopilot_sleep(c / 1000.0);
            autopilot_send(0, 128, 128);
        } else if (strcmp(command, "mark") == 0 && sscanf(line, "%*s %95s", word) == 1) {
            fprintf(stderr, "[autopilot] %.1f s: mark %s\n", autopilot_now() - start, word);
        } else if (strcmp(command, "done") == 0) {
            break;
        }
        fflush(stderr);
    }
    fclose(script);
    fprintf(stderr, "[autopilot] %.1f s: done\n", autopilot_now() - start);
    fflush(stderr);
    return NULL;
}

extern int box64_main(int argc, const char **argv, char **env);
extern char **environ;

#define LOG(prio, ...) __android_log_print(prio, "AnyPS5", __VA_ARGS__)

static void setdefault(const char *name, const char *value) { setenv(name, value, 0); }

// Драйвер Vulkan (выбор в лаунчере, launcher.env):
//   APS5_GPU_DRIVER не задан / "builtin" — встроенный Turnip (libvulkan_freedreno.so в nativeLibraryDir);
//   "system" — системный драйвер телефона (libvulkan.so);
//   иначе — каталог пакета драйвера (adrenotools, meta.json), библиотека — APS5_GPU_DRIVER_LIB.
// Не загрузился выбранный — пробуем встроенный Turnip. APS5_SYSTEM_VULKAN=1 — как "system" (старый флаг).
// tmp_dir нужен adrenotools на Android 9 и ниже.
static void *open_vulkan(const char *native_dir, const char *tmp_dir) {
    char hook_dir[PATH_MAX];
    snprintf(hook_dir, sizeof(hook_dir), "%s/", native_dir);
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/", tmp_dir);
    mkdir(tmp_dir, 0700);
    const char *driver = getenv("APS5_GPU_DRIVER");
    if (getenv("APS5_SYSTEM_VULKAN") != NULL || (driver != NULL && strcmp(driver, "system") == 0)) {
        void *system = dlopen("libvulkan.so", RTLD_NOW);
        LOG(ANDROID_LOG_INFO, "Vulkan: system driver %s", system != NULL ? "loaded" : dlerror());
        fprintf(stderr, "[gpu] Vulkan driver: system (%s)\n", system != NULL ? "loaded" : "failed");
        if (system != NULL) return system;
    } else if (driver != NULL && driver[0] == '/' ) {
        const char *library = getenv("APS5_GPU_DRIVER_LIB");
        char custom_dir[PATH_MAX];
        snprintf(custom_dir, sizeof(custom_dir), "%s/", driver);
        void *custom = library != NULL ? adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmp, hook_dir, custom_dir, library, NULL, NULL) : NULL;
        fprintf(stderr, "[gpu] Vulkan driver: custom %s/%s (%s)\n", driver, library != NULL ? library : "?", custom != NULL ? "loaded" : "failed, using built-in Turnip");
        if (custom != NULL) return custom;
        LOG(ANDROID_LOG_ERROR, "custom driver %s failed: %s", driver, dlerror());
    }
    void *handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmp, hook_dir, hook_dir, "libvulkan_freedreno.so", NULL, NULL);
    if (handle == NULL) LOG(ANDROID_LOG_ERROR, "adrenotools_open_libvulkan failed: %s", dlerror());
    else fprintf(stderr, "[gpu] Vulkan driver: built-in Turnip\n");
    return handle;
}


// Сводка для логов ([diag] в box64.log, попадает в zip «Логи»): раз в 9 с — самые загруженные потоки в
// процентах ядра, память процесса и загрузка GPU (если система даёт её прочитать). Без root и Termux.
#define DIAG_THREADS 512
struct diag_sample { int tid; unsigned long ticks; char name[20]; };
static struct diag_sample diag_previous[DIAG_THREADS], diag_current[DIAG_THREADS];
static int diag_previous_count, diag_current_count;

static void diag_note(int tid, const char *name, unsigned long ticks) {
    if (diag_current_count >= DIAG_THREADS) return;
    struct diag_sample *sample = &diag_current[diag_current_count++];
    sample->tid = tid;
    sample->ticks = ticks;
    snprintf(sample->name, sizeof(sample->name), "%s", name);
}

static void diag_report(double seconds) {
    struct { double load; int index; } top[6] = {{0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}};
    const double hz = (double)sysconf(_SC_CLK_TCK);
    for (int i = 0; i < diag_current_count; ++i) {
        unsigned long before = diag_current[i].ticks;
        for (int j = 0; j < diag_previous_count; ++j) {
            if (diag_previous[j].tid == diag_current[i].tid) { before = diag_previous[j].ticks; break; }
        }
        const double load = (double)(diag_current[i].ticks - before) * 100.0 / hz / seconds;
        for (int slot = 0; slot < 6; ++slot) {
            if (load <= top[slot].load) continue;
            for (int move = 5; move > slot; --move) top[move] = top[move - 1];
            top[slot].load = load;
            top[slot].index = i;
            break;
        }
    }
    char line[512];
    int used = snprintf(line, sizeof(line), "[diag] cpu");
    for (int slot = 0; slot < 6 && top[slot].index >= 0 && top[slot].load >= 1.0 && used < (int)sizeof(line); ++slot)
        used += snprintf(line + used, sizeof(line) - (size_t)used, " %s %.0f%%", diag_current[top[slot].index].name, top[slot].load);
    FILE *status = fopen("/proc/self/status", "r");
    if (status != NULL) {
        char row[128];
        while (fgets(row, sizeof(row), status) != NULL) {
            unsigned long kb;
            if (sscanf(row, "VmRSS: %lu", &kb) == 1 && used < (int)sizeof(line)) { used += snprintf(line + used, sizeof(line) - (size_t)used, "; rss %lu MB", kb / 1024); break; }
        }
        fclose(status);
    }
    FILE *gpu = fopen("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", "r");
    if (gpu != NULL) {
        int busy;
        if (fscanf(gpu, "%d", &busy) == 1 && used < (int)sizeof(line)) used += snprintf(line + used, sizeof(line) - (size_t)used, "; gpu busy %d%%", busy);
        fclose(gpu);
    }
    fprintf(stderr, "%s\n", line);
    memcpy(diag_previous, diag_current, sizeof(diag_current[0]) * (size_t)diag_current_count);
    diag_previous_count = diag_current_count;
}

// Сторожевой поток (APS5_WATCHDOG=0 отключает): каждые 3 с пишет в watch.log состояние потоков
// (имя, состояние, utime, stime, wchan) — видно, где процесс встал, даже если его потом заморозят; из тех же
// срезов — сводка [diag] (APS5_DIAG=0 отключает).
static void *watchdog(void *arg) {
    (void)arg;
    FILE *out = fopen("watch.log", "w");
    if (out == NULL) return NULL;
    const char *diag_env = getenv("APS5_DIAG");
    const bool diag = diag_env == NULL || strcmp(diag_env, "0") != 0;
    for (int tick = 0;; ++tick) {
        sleep(3);
        fprintf(out, "== %d s\n", tick * 3 + 3);
        diag_current_count = 0;
        DIR *dir = opendir("/proc/self/task");
        struct dirent *entry;
        while (dir != NULL && (entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char path[128], stat[512] = "", wchan[64] = "";
            snprintf(path, sizeof(path), "/proc/self/task/%s/stat", entry->d_name);
            FILE *f = fopen(path, "r");
            if (f != NULL) { if (fgets(stat, sizeof(stat), f) == NULL) stat[0] = '\0'; fclose(f); }
            snprintf(path, sizeof(path), "/proc/self/task/%s/wchan", entry->d_name);
            f = fopen(path, "r");
            if (f != NULL) { if (fgets(wchan, sizeof(wchan), f) == NULL) wchan[0] = '\0'; fclose(f); }
            char *name = strchr(stat, '('), *close = strrchr(stat, ')');
            if (name == NULL || close == NULL) continue;
            *close = '\0';
            char state = 0; unsigned long utime = 0, stime = 0;
            sscanf(close + 2, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &state, &utime, &stime);
            fprintf(out, "%s %s %c %lu %lu %s\n", entry->d_name, name + 1, state, utime, stime, wchan);
            diag_note(atoi(entry->d_name), name + 1, utime + stime);
        }
        if (dir != NULL) closedir(dir);
        fflush(out);
        if (diag && tick == 0) {
            memcpy(diag_previous, diag_current, sizeof(diag_current[0]) * (size_t)diag_current_count);
            diag_previous_count = diag_current_count;
        } else if (diag && tick % 3 == 0) {
            diag_report(9.0);
        }
    }
    return NULL;
}

// Сэмплер (APS5_SAMPLE=1): каждые 2 мс сигнал главному потоку; обработчик переводит нативный PC в
// x86-адрес начала блока box64 (box64_native_to_x64). Каждые 5 с — топ адресов в sample.log.
// APS5_SAMPLE=<начало имени потока> (например Job.Worker): по очереди все потоки с таким именем,
// список обновляется каждое окно (потоки игры появляются позже).
extern void *box64_native_to_x64(void *pc);
// Точный x86-адрес инструкции (блок box64 может покрывать целую функцию): по нему sample-total.txt
// сводит сэмплы по функциям игры и x86-библиотек.
extern void *box64_native_to_x64_exact(void *pc);
extern void box64_save_dynacache(void);

// Кэш трансляции box64 (DynaCache) пишется только при выходе box64, а Android убивает приложение:
// сохраняем его сами — один раз после старта (ещё меню) и когда игра уходит в фон (GameActivity.onPause).
// Сохранение держит mutex_dyndump, пока пишет весь файл (сотни МБ): трансляция нового кода в это время
// ждёт, поэтому посреди игры (бой, загрузка уровня) по таймеру не сохраняем. APS5_DYNACACHE_SAVE=0 — выкл.
static pthread_mutex_t dynacache_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dynacache_cond = PTHREAD_COND_INITIALIZER;
static bool dynacache_requested, dynacache_enabled;
static void *dynacache_saver(void *arg) {
    (void)arg;
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 40;
    pthread_mutex_lock(&dynacache_mutex);
    // Первое сохранение: через 40 с или раньше, если игру свернули.
    while (!dynacache_requested && pthread_cond_timedwait(&dynacache_cond, &dynacache_mutex, &deadline) == 0) {}
    for (;;) {
        dynacache_requested = false;
        pthread_mutex_unlock(&dynacache_mutex);
        box64_save_dynacache();
        LOG(ANDROID_LOG_INFO, "DynaCache saved");
        pthread_mutex_lock(&dynacache_mutex);
        while (!dynacache_requested) pthread_cond_wait(&dynacache_cond, &dynacache_mutex);
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_org_anyps5_android_GameActivity_nativeSaveDynacache(JNIEnv *env, jclass cls) {
    (void)env;
    (void)cls;
    if (!dynacache_enabled) return;
    pthread_mutex_lock(&dynacache_mutex);
    dynacache_requested = true;
    pthread_cond_signal(&dynacache_cond);
    pthread_mutex_unlock(&dynacache_mutex);
}
// Автозахват кадров (APS5_AUTODUMP=<строка лога>, нужен и APS5_DUMP_TRIGGER=<файл>): поток читает свой
// box64.log, и когда в нём впервые появляется эта строка (например "pass 805000b4" — проходы боя Mewgenics),
// через 5 с создаёт файл-триггер на 0,3 с (VulkanDevice сохраняет кадры, пока файл есть), и так
// APS5_AUTODUMP_SERIES раз (по умолчанию 3) каждые 15 с. Не зависит от Termux и внешних наблюдателей.
static void *autodump(void *arg) {
    const char *pattern = arg;
    const char *trigger = getenv("APS5_DUMP_TRIGGER");
    const char *series_text = getenv("APS5_AUTODUMP_SERIES");
    const int series = series_text != NULL && atoi(series_text) > 0 ? atoi(series_text) : 3;
    FILE *log = fopen("box64.log", "r");
    if (log == NULL || trigger == NULL) return NULL;
    char line[4096];
    for (;;) {
        if (fgets(line, sizeof(line), log) == NULL) {
            clearerr(log);
            sleep(1);
            continue;
        }
        if (strstr(line, pattern) != NULL) break;
    }
    fclose(log);
    fprintf(stderr, "[autodump] '%s' seen: %d series of frames\n", pattern, series);
    for (int i = 0; i < series; ++i) {
        sleep(i == 0 ? 5 : 15);
        FILE *file = fopen(trigger, "w");
        if (file != NULL) fclose(file);
        usleep(300000);
        unlink(trigger);
        fprintf(stderr, "[autodump] series %d done\n", i + 1);
    }
    return NULL;
}
#define SAMPLE_RING 65536
static uintptr_t sample_ring[SAMPLE_RING];
// Адрес возврата (x30) нативного сэмпла: кто вызвал функцию libc (x86-адрес, если вызвал JIT box64).
static uintptr_t sample_caller[SAMPLE_RING];
static volatile unsigned sample_head;
static pid_t sample_tid;
static const char *sample_prefix;

// Потоки процесса, чьё имя начинается с sample_prefix (не больше max).
static int sample_threads(pid_t *tids, int max) {
    DIR *dir = opendir("/proc/self/task");
    if (dir == NULL) return 0;
    int count = 0;
    struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        char path[64], name[32] = "";
        snprintf(path, sizeof(path), "/proc/self/task/%s/comm", entry->d_name);
        FILE *comm = fopen(path, "r");
        if (comm == NULL) continue;
        if (fgets(name, sizeof(name), comm) == NULL) name[0] = '\0';
        fclose(comm);
        if (strncmp(name, sample_prefix, strlen(sample_prefix)) == 0) tids[count++] = (pid_t)atoi(entry->d_name);
    }
    closedir(dir);
    return count;
}

// Поток сейчас на ядре или в очереди на него (состояние R).
static bool thread_running(pid_t tid) {
    char path[64], text[256];
    snprintf(path, sizeof(path), "/proc/self/task/%d/stat", (int)tid);
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const ssize_t got = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (got <= 0) return false;
    text[got] = '\0';
    const char *close_paren = strrchr(text, ')');
    return close_paren != NULL && close_paren[1] == ' ' && close_paren[2] == 'R';
}

// Код libc (bionic): вызывающего ищем за его пределами — обёртки системных вызовов libc сами по себе
// ничего не говорят. Кадры читаются через process_vm_readv (безопасно при битом указателе кадра).
static uintptr_t libc_text_begin, libc_text_end;
static uintptr_t read_word(uintptr_t address) {
    uintptr_t value = 0;
    struct iovec local = {&value, sizeof(value)}, remote = {(void *)address, sizeof(value)};
    return syscall(SYS_process_vm_readv, getpid(), &local, 1, &remote, 1, 0) == (long)sizeof(value) ? value : 0;
}

static void sample_handler(int sig, siginfo_t *info, void *raw) {
    (void)sig; (void)info;
    ucontext_t *uc = raw;
    uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc;
    void *x64 = box64_native_to_x64_exact((void *)pc);
    // x86-адреса помечаем старшим битом 0, нативные — 1 (bit 63).
    uintptr_t value = x64 != NULL ? (uintptr_t)x64 : (pc | (1ULL << 63));
    uintptr_t caller = 0;
    if (x64 == NULL) {
        uintptr_t lr = (uintptr_t)uc->uc_mcontext.regs[30];
        uintptr_t fp = (uintptr_t)uc->uc_mcontext.regs[29];
        for (int depth = 0; depth < 4 && lr >= libc_text_begin && lr < libc_text_end && fp != 0 && (fp & 7) == 0; ++depth) {
            lr = read_word(fp + 8);
            fp = read_word(fp);
        }
        void *lr64 = box64_native_to_x64_exact((void *)lr);
        caller = lr64 != NULL ? (uintptr_t)lr64 : (lr | (1ULL << 63));
    }
    unsigned slot = __atomic_fetch_add(&sample_head, 1, __ATOMIC_RELAXED);
    sample_ring[slot % SAMPLE_RING] = value;
    sample_caller[slot % SAMPLE_RING] = caller;
}

typedef struct { uintptr_t key; unsigned count; } sample_bin;
static int sample_cmp(const void *a, const void *b) { return (int)((const sample_bin *)b)->count - (int)((const sample_bin *)a)->count; }

// Счёт за всю сессию (окна по 5 с слишком коротки для функций по 0,1%): открытая адресация по ключу
// сэмпла. Ключ 0 — пустая ячейка; переполненная таблица просто перестаёт принимать новые ключи.
#define SAMPLE_TOTAL_BITS 17
typedef struct { sample_bin bins[1u << SAMPLE_TOTAL_BITS]; unsigned used; unsigned long long total; } sample_table;
static void sample_table_add(sample_table *table, uintptr_t key) {
    if (key == 0) return;
    const unsigned mask = (1u << SAMPLE_TOTAL_BITS) - 1;
    unsigned slot = (unsigned)((key * 0x9e3779b97f4a7c15ull) >> (64 - SAMPLE_TOTAL_BITS));
    for (unsigned probe = 0; probe < 64; ++probe, slot = (slot + 1) & mask) {
        sample_bin *bin = &table->bins[slot];
        if (bin->key == key) { ++bin->count; ++table->total; return; }
        if (bin->key == 0) {
            if (table->used >= (mask + 1) / 4 * 3) return;
            bin->key = key; bin->count = 1; ++table->used; ++table->total;
            return;
        }
    }
}

// Отображения файлов процесса для перевода адреса в модуль+смещение: смещение считается от самого
// низкого отображения того же файла (так грузятся и x86-модули box64, и нативные библиотеки).
typedef struct { uintptr_t start, end, base; char name[96]; } sample_map;
static int sample_maps(sample_map *maps, int max) {
    FILE *file = fopen("/proc/self/maps", "r");
    if (file == NULL) return 0;
    char line[512];
    int count = 0;
    while (count < max && fgets(line, sizeof(line), file) != NULL) {
        unsigned long start, end;
        int name = 0;
        if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %n", &start, &end, &name) < 2 || name == 0 || line[name] != '/') continue;
        line[strcspn(line, "\n")] = '\0';
        const char *path = line + name;
        const char *slash = strrchr(path, '/');
        sample_map *map = &maps[count];
        map->start = start; map->end = end;
        snprintf(map->name, sizeof(map->name), "%s", slash != NULL ? slash + 1 : path);
        map->base = start;
        ++count;
    }
    fclose(file);
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < i; ++j)
            if (maps[j].start < maps[i].base && strcmp(maps[i].name, maps[j].name) == 0) maps[i].base = maps[j].start;
    return count;
}

static void sample_describe(FILE *out, const sample_map *maps, int mapCount, uintptr_t key) {
    if (key >> 63) {
        Dl_info dl;
        const uintptr_t base = key & ~(1ULL << 63);
        const bool found = base && dladdr((void *)base, &dl) && dl.dli_fbase;
        if (found && dl.dli_sname && dl.dli_saddr == (void *)base) fprintf(out, "native %s:%s", dl.dli_fname, dl.dli_sname);
        else if (found) fprintf(out, "native %s+0x%lx", dl.dli_fname, (unsigned long)(base - (uintptr_t)dl.dli_fbase));
        else fprintf(out, "native ?");
        return;
    }
    for (int i = 0; i < mapCount; ++i) {
        if (key >= maps[i].start && key < maps[i].end) {
            fprintf(out, "x64 %s+0x%lx", maps[i].name, (unsigned long)(key - maps[i].base));
            return;
        }
    }
    fprintf(out, "x64 0x%lx", (unsigned long)key);
}

// sample-total.txt: весь счёт сессии (переписывается целиком), сэмплы и вызывающие нативного кода.
static void sample_write_total(sample_table *samples, sample_table *callers, int window) {
    static sample_map maps[4096];
    static sample_bin sorted[1u << SAMPLE_TOTAL_BITS];
    const int mapCount = sample_maps(maps, 4096);
    FILE *out = fopen("sample-total.txt.tmp", "w");
    if (out == NULL) return;
    fprintf(out, "# %s, %.0f s, %llu samples, %u keys; x64 = exact guest instruction\n", sample_prefix != NULL ? sample_prefix : "main", (window + 1) * 5.0, samples->total, samples->used);
    for (int pass = 0; pass < 2; ++pass) {
        sample_table *table = pass == 0 ? samples : callers;
        unsigned n = 0;
        for (unsigned i = 0; i < (1u << SAMPLE_TOTAL_BITS); ++i) if (table->bins[i].key != 0) sorted[n++] = table->bins[i];
        qsort(sorted, n, sizeof(sorted[0]), sample_cmp);
        fprintf(out, pass == 0 ? "== samples\n" : "== callers of native samples (%llu)\n", table->total);
        for (unsigned i = 0; i < n && i < 4000; ++i) {
            fprintf(out, "%u ", sorted[i].count);
            sample_describe(out, maps, mapCount, sorted[i].key);
            fputc('\n', out);
        }
    }
    fclose(out);
    rename("sample-total.txt.tmp", "sample-total.txt");
}

static void *sampler(void *arg) {
    (void)arg;
    FILE *out = fopen("sample.log", "w");
    if (out == NULL) return NULL;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = sample_handler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGRTMIN + 18, &action, NULL);
    {
        Dl_info dl;
        if (dladdr((void *)&sigprocmask, &dl) && dl.dli_fbase) {
            libc_text_begin = (uintptr_t)dl.dli_fbase;
            libc_text_end = libc_text_begin + 0x200000; // текст bionic libc с запасом
        }
    }
    static sample_bin bins[8192];
    static sample_bin callers[4096];
    static sample_table sessionSamples, sessionCallers;
    sleep(5); // box64 должен успеть инициализироваться
    unsigned taken = __atomic_load_n(&sample_head, __ATOMIC_RELAXED);
    for (int window = 0;; ++window) {
        pid_t tids[64];
        int threads = 1;
        tids[0] = sample_tid;
        if (sample_prefix != NULL) threads = sample_threads(tids, 64);
        for (int i = 0; i < 2500; ++i) {
            // Только работающие потоки (R в /proc/self/task/N/stat): ждущие показывали бы ожидание в
            // libc и прятали поток, который действительно занимает ядро.
            for (int k = 0; k < threads; ++k) {
                const pid_t tid = tids[(i + k) % threads];
                if (threads > 1 && !thread_running(tid)) continue;
                syscall(SYS_tgkill, getpid(), tid, SIGRTMIN + 18);
                break;
            }
            usleep(2000);
        }
        unsigned head = __atomic_load_n(&sample_head, __ATOMIC_RELAXED);
        int used = 0, usedCallers = 0; unsigned total = 0, totalCallers = 0;
        for (; taken != head; ++taken) {
            uintptr_t v = sample_ring[taken % SAMPLE_RING];
            uintptr_t c = sample_caller[taken % SAMPLE_RING];
            if (c != 0) {
                if (c >> 63) {
                    Dl_info dl;
                    const uintptr_t lr = c & ~(1ULL << 63);
                    c = dladdr((void *)lr, &dl) && dl.dli_fbase ? ((lr & ~(uintptr_t)0x3f) | (1ULL << 63)) : (1ULL << 63);
                }
                sample_table_add(&sessionCallers, c);
                int k = 0;
                while (k < usedCallers && callers[k].key != c) ++k;
                if (k == usedCallers && usedCallers < 4096) { callers[usedCallers].key = c; callers[usedCallers].count = 0; ++usedCallers; }
                if (k < usedCallers) { ++callers[k].count; ++totalCallers; }
            }
            // нативные адреса группируем по функции (ближайший символ dladdr, иначе модуль), x86 — по началу блока
            if (v >> 63) {
                Dl_info dl;
                uintptr_t pc = v & ~(1ULL << 63);
                const bool found = dladdr((void *)pc, &dl) && dl.dli_fbase;
                // Без экспортируемого имени (внутренние функции libc вроде __memcpy_aarch64_simd) — по
                // 64 байта кода (обёртки системных вызовов выровнены на 0x40): отчёт пишет смещение в модуле, символ находится по его .symtab.
                v = found ? ((uintptr_t)(dl.dli_saddr ? dl.dli_saddr : (void *)(pc & ~(uintptr_t)0x3f)) | (1ULL << 63)) : (1ULL << 63);
            }
            sample_table_add(&sessionSamples, v);
            int j = 0;
            while (j < used && bins[j].key != v) ++j;
            if (j == used) { if (used == 8192) continue; bins[used].key = v; bins[used].count = 0; ++used; }
            ++bins[j].count; ++total;
        }
        qsort(bins, used, sizeof(bins[0]), sample_cmp);
        fprintf(out, "== window %d (%.0f s), samples %u, threads %d %s\n", window, (window + 1) * 5.0, total, threads, sample_prefix != NULL ? sample_prefix : "main");
        for (int j = 0; j < used && j < 80; ++j) {
            if (bins[j].key >> 63) {
                Dl_info dl;
                uintptr_t base = bins[j].key & ~(1ULL << 63);
                const bool found = base && dladdr((void *)base, &dl);
                const char *name = found ? dl.dli_fname : "?";
                const char *symbol = found && dl.dli_sname && dl.dli_saddr == (void *)base ? dl.dli_sname : "";
                if (symbol[0] == '\0' && found) fprintf(out, "%5.1f%% native %s+0x%lx\n", 100.0 * bins[j].count / total, name, (unsigned long)(base - (uintptr_t)dl.dli_fbase));
                else fprintf(out, "%5.1f%% native %s%s%s\n", 100.0 * bins[j].count / total, name, symbol[0] ? ":" : "", symbol);
            } else {
                fprintf(out, "%5.1f%% x64 0x%lx\n", 100.0 * bins[j].count / total, (unsigned long)bins[j].key);
            }
        }
        // Кто вызывал нативный код (адрес возврата): x86-адрес JIT box64 или модуль+смещение.
        qsort(callers, usedCallers, sizeof(callers[0]), sample_cmp);
        if (totalCallers != 0) fprintf(out, "-- callers of native samples (%u)\n", totalCallers);
        for (int k = 0; k < usedCallers && k < 25; ++k) {
            if (callers[k].key >> 63) {
                Dl_info dl;
                const uintptr_t lr = callers[k].key & ~(1ULL << 63);
                const bool found = lr && dladdr((void *)lr, &dl) && dl.dli_fbase;
                fprintf(out, "%5.1f%% caller %s+0x%lx\n", 100.0 * callers[k].count / totalCallers, found ? dl.dli_fname : "?", found ? (unsigned long)(lr - (uintptr_t)dl.dli_fbase) : 0ul);
            } else {
                fprintf(out, "%5.1f%% caller x64 0x%lx\n", 100.0 * callers[k].count / totalCallers, (unsigned long)callers[k].key);
            }
        }
        fflush(out);
        if (window % 6 == 5) sample_write_total(&sessionSamples, &sessionCallers, window);
    }
    return NULL;
}

// ADPF (Android 12+, APS5_ADPF=0 — выкл.): сессия подсказок производительности над ключевыми потоками
// игры — главный поток (самый загруженный "app.elf"), рендер-поток Unity, очередь и запись драйвера,
// presenter. Каждые 50 мс система получает, сколько процессорного времени съел самый загруженный из
// них (schedstat), при цели 75% окна: частоты и ядра она подбирает сама — поднимает, когда поток не
// успевает, и не держит лишнего, когда запас есть (так делают Unity/Unreal на Android).
typedef void *(*adpf_manager_fn)(void);
typedef void *(*adpf_create_fn)(void *, const int32_t *, size_t, int64_t);
typedef int (*adpf_report_fn)(void *, int64_t);
typedef int (*adpf_target_fn)(void *, int64_t);
typedef int (*adpf_threads_fn)(void *, const pid_t *, size_t);
typedef void (*adpf_close_fn)(void *);

static uint64_t adpf_runtime(pid_t tid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%d/schedstat", tid);
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    unsigned long long runtime = 0;
    if (fscanf(f, "%llu", &runtime) != 1) runtime = 0;
    fclose(f);
    return runtime;
}

// Ключевые потоки: по одному на имя, для "app.elf" — с наибольшим временем (главный поток игры).
static int adpf_threads(pid_t *tids, int max) {
    static const char *const names[] = {"UnityGfxDe", "agc-queue-worke", "agc-draw-record", "agc-presenter"};
    DIR *dir = opendir("/proc/self/task");
    if (dir == NULL) return 0;
    int count = 0;
    pid_t main_tid = 0;
    uint64_t main_runtime = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        const pid_t tid = (pid_t)atoi(entry->d_name);
        char path[64], name[32] = "";
        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
        FILE *comm = fopen(path, "r");
        if (comm == NULL) continue;
        if (fgets(name, sizeof(name), comm) == NULL) name[0] = '\0';
        fclose(comm);
        name[strcspn(name, "\n")] = '\0';
        if (strcmp(name, "app.elf") == 0) {
            const uint64_t runtime = adpf_runtime(tid);
            if (runtime > main_runtime) { main_runtime = runtime; main_tid = tid; }
            continue;
        }
        for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            if (count < max - 1 && strncmp(name, names[i], strlen(names[i])) == 0) { tids[count++] = tid; break; }
        }
    }
    closedir(dir);
    if (main_tid != 0) tids[count++] = main_tid;
    return count;
}

static void *adpf_thread(void *arg) {
    (void)arg;
    void *android = dlopen("libandroid.so", RTLD_NOW);
    adpf_manager_fn get_manager = android ? (adpf_manager_fn)dlsym(android, "APerformanceHint_getManager") : NULL;
    adpf_create_fn create = android ? (adpf_create_fn)dlsym(android, "APerformanceHint_createSession") : NULL;
    adpf_report_fn report = android ? (adpf_report_fn)dlsym(android, "APerformanceHint_reportActualWorkDuration") : NULL;
    adpf_target_fn target = android ? (adpf_target_fn)dlsym(android, "APerformanceHint_updateTargetWorkDuration") : NULL;
    adpf_threads_fn set_threads = android ? (adpf_threads_fn)dlsym(android, "APerformanceHint_setThreads") : NULL;
    adpf_close_fn close_session = android ? (adpf_close_fn)dlsym(android, "APerformanceHint_closeSession") : NULL;
    void *manager = get_manager ? get_manager() : NULL;
    if (manager == NULL || create == NULL || report == NULL || target == NULL) {
        fprintf(stderr, "[adpf] performance hints unavailable\n");
        return NULL;
    }
    const int64_t window_ns = 50000000, target_ns = window_ns * 3 / 4;
    void *session = NULL;
    pid_t tids[8];
    int count = 0;
    uint64_t last[8] = {0};
    sleep(20); // потоки игры и драйвера появляются после старта
    for (unsigned tick = 0;; ++tick) {
        if (tick % 100 == 0) { // раз в 5 с: потоки могли смениться
            pid_t fresh[8];
            const int found = adpf_threads(fresh, 8);
            const int changed = found != count || memcmp(fresh, tids, sizeof(pid_t) * (size_t)found) != 0;
            if (found > 0 && changed) {
                if (session != NULL && set_threads != NULL && set_threads(session, fresh, (size_t)found) == 0) {
                } else {
                    if (session != NULL && close_session != NULL) close_session(session);
                    int32_t ids[8];
                    for (int i = 0; i < found; ++i) ids[i] = fresh[i];
                    session = create(manager, ids, (size_t)found, target_ns);
                }
                memcpy(tids, fresh, sizeof(pid_t) * (size_t)found);
                count = found;
                for (int i = 0; i < count; ++i) last[i] = adpf_runtime(tids[i]);
                fprintf(stderr, "[adpf] session %s over %d threads, target %lld ms per %lld ms\n", session ? "on" : "FAILED", count, (long long)(target_ns / 1000000), (long long)(window_ns / 1000000));
            }
        }
        usleep((useconds_t)(window_ns / 1000));
        if (session == NULL || count == 0) continue;
        int64_t busiest = 0;
        for (int i = 0; i < count; ++i) {
            const uint64_t now = adpf_runtime(tids[i]);
            const int64_t used = now >= last[i] ? (int64_t)(now - last[i]) : 0;
            last[i] = now;
            if (used > busiest) busiest = used;
        }
        if (busiest > 0) report(session, busiest);
    }
    return NULL;
}

int SDL_main(int argc, char **argv) {
    LOG(ANDROID_LOG_INFO, "SDL_main argc=%d", argc);
    if (argc < 4) return 2;
    char game_elf[PATH_MAX];
    if (realpath(argv[1], game_elf) == NULL) { LOG(ANDROID_LOG_ERROR, "no game ELF %s", argv[1]); return 3; }
    const char *native_dir = argv[2];
    snprintf(mouse_library, sizeof(mouse_library), "%s/libSceMouse.so", native_dir);
    char game_dir[PATH_MAX];
    snprintf(game_dir, sizeof(game_dir), "%s", game_elf);
    char *slash = strrchr(game_dir, '/');
    if (slash == NULL) return 4;
    *slash = '\0';
    if (chdir(game_dir) != 0) return 5;
    if (getenv("APS5_LOG_TO_LOGCAT") == NULL) {
        // stdout и stderr — один дескриптор: иначе "w" со своим смещением затирает записи stderr.
        freopen("box64.log", "w", stdout);
        dup2(fileno(stdout), STDERR_FILENO);
        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    // Пользовательские переменные: <game>/env.txt, строки NAME=VALUE (перекрывают умолчания ниже).
    // launcher.env — переключатели хаба (MainActivity), env.txt — ручные (перекрывают хаб).
    static const char *const env_files[] = {"launcher.env", "env.txt"};
    for (size_t file = 0; file < sizeof(env_files) / sizeof(env_files[0]); ++file) {
        FILE *extra = fopen(env_files[file], "r");
        if (extra == NULL) continue;
        char line[1024];
        while (fgets(line, sizeof(line), extra) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            char *eq = strchr(line, '=');
            if (line[0] == '#' || eq == NULL) continue;
            *eq = '\0';
            setenv(line, eq + 1, 1);
        }
        fclose(extra);
    }

    char path[PATH_MAX * 3];
    snprintf(path, sizeof(path), "%s/x64lib:%s/libs", game_dir, game_dir);
    setdefault("BOX64_LD_LIBRARY_PATH", path);
    setdefault("APS5_NATIVE_DIR", native_dir);
    // Адрес состояния сенсорного геймпада для libSceVideoOut (без поиска символа через пространства имён линкера).
    char virtual_pad[32];
    snprintf(virtual_pad, sizeof(virtual_pad), "%llx", (unsigned long long)(uintptr_t)&aps5_virtual_pad);
    setenv("APS5_VIRTUAL_PAD", virtual_pad, 1);
    char text_input[32];
    snprintf(text_input, sizeof(text_input), "%llx", (unsigned long long)(uintptr_t)&aps5_text_input);
    setenv("APS5_TEXT_INPUT", text_input, 1);
    // Кэш шейдеров/конвейеров: по умолчанию рядом с исполняемым файлом (/system/bin — нет доступа).
    char cache[PATH_MAX];
    snprintf(cache, sizeof(cache), "%s/shader_cache", game_dir);
    setdefault("ANYPS5_SHADER_CACHE_DIR", cache);
    setdefault("HOME", argv[3]);
    setdefault("TMPDIR", argv[3]);
    // Циклы ожидания игры (x86 PAUSE) — подсказка YIELD вместо пустого кручения на полной мощности.
    // PAUSE в циклах ожидания (спинлоки потоков Unity) — SEVL+WFE: ядро ждёт события, а не крутится;
    // на одинаковой частоте меню давало больше кадров при ~12% меньшей загрузке ядер.
    setdefault("BOX64_DYNAREC_PAUSE", "3");
    // Кэш ресурсов и рецепты для draw со сгенерированными стадиями (прямоугольники интерфейса).
    setdefault("APS5_GENERATED_STAGE_CACHE", "1");
    // Конвейер draw: подготовка и запись Vulkan в двух потоках (APS5_PIPELINE_DRAWS=0 — по-старому, в одном).
    setdefault("APS5_PIPELINE_DRAWS", "1");
    setdefault("BOX64_LOG", "1");
    setdefault("BOX64_SHOWBT", "1");
    setdefault("BOX64_DYNAREC_BIGBLOCK", "3");
    setdefault("BOX64_DYNAREC_CALLRET", "1");
    setdefault("BOX64_DYNAREC_FASTROUND", "1");
    setdefault("BOX64_DYNAREC_FASTNAN", "1");
    setdefault("APS5_IGNORE_MSAA", "1");
    // Отрисовку или диспетчеризацию, шейдер которой AnyPS5 не поддерживает, пропускать (отчёт в box64.log), а не
    // закрывать игру (патч anyps5-skip-failed-shaders). APS5_SKIP_FAILED_SHADERS=0 — как раньше.
    setdefault("APS5_SKIP_FAILED_SHADERS", "1");
    // Float-семантика Adreno в пересобранных шейдерах вместо побитовой эмуляции AMD (патч anyps5-host-ftz):
    // денормали сбрасывает сам GPU, NaN — его собственный, умножение со сложением склеиваются в mad, DPP
    // читает соседнюю линию без ballot. В Mewgenics это вдвое и больше короче шейдеры. =0 — точная эмуляция.
    setdefault("APS5_HOST_FTZ", "1");
    setdefault("APS5_HOST_NAN", "1");
    setdefault("APS5_HOST_FMA", "1");
    setdefault("APS5_HOST_DPP", "1");
    // Касания — только сенсорное управление (геймпад игрока 1): SDL не делает из них мышь, иначе
    // игра видела мышь вторым устройством ввода (второй игрок) при касании мимо кнопок.
    setdefault("SDL_TOUCH_MOUSE_EVENTS", "0");
    // Turnip: выбор режима прохода (тайлы GMEM или прямо в память) по реальному времени GPU обоих
    // режимов, а не по оценке пропускной способности — та выбирает GMEM для проходов из множества
    // мелких draw (меню), где прямой режим быстрее (Mesa issue 7072). TU_AUTOTUNE_ALGO=bandwidth — как было.
    setdefault("TU_AUTOTUNE_ALGO", "profiled");
    setdefault("APS5_TEXTURE_CHECK_PER_FRAME", "1");
    setdefault("APS5_LOG_FPS", "1");
    // Кэш трансляции box64: в окне заставок половина главного потока — трансляция нового x86-кода.
    // Без сжатия (распаковка zlib стоила ~9% главного потока). BOX64_DYNACACHE=0 — выкл.
    setdefault("BOX64_DYNACACHE", "1");
    setdefault("BOX64_DYNACACHE_COMPRESS", "0");
    // Окно SDL на Android ставит ориентацию сама (по размеру окна / этой подсказке): только альбомная.
    setdefault("SDL_IOS_ORIENTATIONS", "LandscapeLeft LandscapeRight");

    const char *watch = getenv("APS5_WATCHDOG");
    if (watch == NULL || strcmp(watch, "0") != 0) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, watchdog, NULL) == 0) pthread_detach(thread);
    }

    const char *save = getenv("APS5_DYNACACHE_SAVE");
    if (save == NULL || strcmp(save, "0") != 0) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, dynacache_saver, NULL) == 0) {
            pthread_detach(thread);
            dynacache_enabled = true;
        }
    }
    const char *autodump_pattern = getenv("APS5_AUTODUMP");
    if (autodump_pattern != NULL && autodump_pattern[0] != '\0') {
        pthread_t thread;
        if (pthread_create(&thread, NULL, autodump, (void *)autodump_pattern) == 0) pthread_detach(thread);
    }
    const char *autopilot_script = getenv("APS5_AUTOPILOT");
    if (autopilot_script != NULL && autopilot_script[0] != '\0') {
        static char autopilot_path[PATH_MAX];
        snprintf(autopilot_path, sizeof(autopilot_path), "%s", autopilot_script);
        pthread_t thread;
        if (pthread_create(&thread, NULL, autopilot, autopilot_path) == 0) pthread_detach(thread);
    }
    const char *adpf = getenv("APS5_ADPF");
    if (adpf == NULL || strcmp(adpf, "0") != 0) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, adpf_thread, NULL) == 0) pthread_detach(thread);
    }
    const char *sample = getenv("APS5_SAMPLE");
    if (sample != NULL && sample[0] != '\0' && strcmp(sample, "0") != 0) {
        sample_tid = gettid();
        if (strcmp(sample, "1") != 0) sample_prefix = sample;
        pthread_t thread;
        if (pthread_create(&thread, NULL, sampler, NULL) == 0) pthread_detach(thread);
    }

    char adrenotools_tmp[PATH_MAX];
    snprintf(adrenotools_tmp, sizeof(adrenotools_tmp), "%s/adrenotools-tmp", argv[3]);
    void *vulkan = open_vulkan(native_dir, adrenotools_tmp);
    if (vulkan == NULL) return 6;
    // GPU на максимальной частоте (KGSL, без root): работа приходит рывками, и штатный регулятор
    // держал 525 МГц из 1100 — кадр ждал GPU. На титульнике 56-59 -> ровные 60 FPS. APS5_GPU_TURBO=0 — выкл.
    // Выкл. по умолчанию: на батарее максимальная частота греет телефон, и прошивка всё равно режет
    // частоты. Скорость берём уменьшением работы. APS5_GPU_TURBO=1 — включить.
    const char *turbo = getenv("APS5_GPU_TURBO");
    if (turbo != NULL && strcmp(turbo, "1") == 0) {
        adrenotools_set_turbo(true);
        LOG(ANDROID_LOG_INFO, "GPU turbo on");
    }
    char handle[32];
    snprintf(handle, sizeof(handle), "0x%llx", (unsigned long long)(unsigned long)vulkan);
    setenv("APS5_VULKAN_HANDLE", handle, 1);
    LOG(ANDROID_LOG_INFO, "Vulkan handle %s, ELF %s", handle, game_elf);

    // box64 переписывает argv на месте: строки должны лежать одним непрерывным изменяемым блоком.
    size_t len0 = strlen("box64") + 1, len1 = strlen(game_elf) + 1;
    char *block = malloc(len0 + len1);
    memcpy(block, "box64", len0);
    memcpy(block + len0, game_elf, len1);
    const char *box_argv[] = { block, block + len0, NULL };
    int result = box64_main(2, box_argv, environ);
    LOG(ANDROID_LOG_ERROR, "box64_main returned %d", result);
    fflush(stdout);
    _exit(result);
}
