// Отслеживание записей гостя для нативной группы (Android/bionic + box64, Linux без uffd-wp):
// mprotect + нативный обработчик SIGSEGV — перенос macOS-реализации из libc/src/GuestWriteWatch.cpp.
//
// Обработчик ставится поверх обработчика box64 и обрабатывает только страницы, которые защитил сам;
// остальные сигналы уходят box64. Нативный, а не x86: в память гостя пишут и нативные потоки
// драйвера, о которых box64 не знает. Страницы, которые box64 сам держит без записи (из них
// транслирован x86-код — ловит самомодификацию), не защищаются и считаются записанными всегда.
//
// Нативный драйвер вызывает GuestWriteWatch*_nid_postfix напрямую; x86-ядро (регистрация памяти,
// HostWrite, смена прав) — через Aps5Watch* по мосту box64.

#include "prx/libc/include/GuestWriteWatch.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <dlfcn.h>
#include <link.h>
#include <ucontext.h>
#include <map>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

using PageHasCode = int (*)(std::uintptr_t);
using NativeToX64 = void* (*)(void*);

// APS5_TRACE_WATCH=1: кто пишет в наблюдаемые страницы. Обработчик кладёт (pc, страница) в кольцо,
// Collect раз в 5 с сводит топ писателей (x86-блок box64 или нативный модуль) и страниц в stderr.
struct TraceRing {
    static constexpr unsigned Size = 1u << 16;
    std::uintptr_t pc[Size];
    std::uintptr_t page[Size];
    std::atomic<unsigned> head{0};
};

// Кто вызвал регистрацию/снятие (для APS5_TRACE_SINCE): драйвер (нативно) или x86 (libkernel, libc).
thread_local const char* traceSource = "drv";

class Watch {
public:
    static Watch& Get() {
        static Watch watch;
        return watch;
    }

    bool Available() {
        ensureHandler();
        return _available;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        refreshNativeRanges();
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!Available() || end <= begin) return;
        // Права здесь не читаем (разбор /proc/self/maps на каждую регистрацию стоил процентов главного
        // потока, а память часто ещё без прав): их подставит probe() при первом Collect диапазона.
        std::vector<std::uint8_t> pages((end - begin) / _page, Written);
        const GuardedLock lock(_lock);
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            pages[(page - begin) / _page] = static_cast<std::uint8_t>(Written | (state & ProtectionMask));
        });
        erase(begin, end);
        _ranges.emplace(begin, Range{end, std::move(pages)});
        publish(begin, end, true);
        traceRange("register", begin, end, true);
    }

    // APS5_TRACE_SINCE: регистрации и снятия (первые 400) — чтобы видеть, откуда память текстуры
    // выпадает из покрытия.
    void traceRange(const char* what, std::uintptr_t begin, std::uintptr_t end, bool done) {
        static const bool enabled = std::getenv("APS5_TRACE_SINCE") != nullptr;
        static std::atomic<int> left{4000};
        if (!enabled || !done || end - begin < 0x100000 || left.fetch_sub(1, std::memory_order_relaxed) <= 0) return;
        std::fprintf(stderr, "[watch-%s] %s 0x%llx-0x%llx (%llu KiB)\n", what, traceSource, static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end), static_cast<unsigned long long>((end - begin) >> 10));
    }

    // Интервал покрытия, содержащий адрес (0,0 — нет).
    void Interval(std::uintptr_t address, std::uintptr_t& begin, std::uintptr_t& end) {
        begin = end = 0;
        const auto coverage = std::atomic_load(&_coverage);
        if (!coverage) return;
        auto it = std::upper_bound(coverage->begin(), coverage->end(), address, [](std::uintptr_t value, const auto& interval) { return value < interval.first; });
        if (it == coverage->begin()) return;
        --it;
        if (address < it->second) {
            begin = it->first;
            end = it->second;
        }
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return false;
        const GuardedLock lock(_lock);
        const bool removed = erase(begin, end);
        if (removed) publish(begin, end, false);
        traceRange("unregister", begin, end, removed);
        return removed;
    }

    // Без общей блокировки (и без блокировки сигналов, два системных вызова): драйвер спрашивает
    // покрытие на каждой отрисовке. Читает атомарный снимок интервалов, обновляемый в publish().
    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available) return false;
        const auto coverage = std::atomic_load(&_coverage);
        if (!coverage) return false;
        auto it = std::upper_bound(coverage->begin(), coverage->end(), begin, [](std::uintptr_t value, const auto& interval) { return value < interval.first; });
        if (it == coverage->begin()) return false;
        --it;
        return begin >= it->first && end <= it->second;
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!_available) return false;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        {
            const GuardedLock lock(_lock);
            if (!covers(begin, end)) return false;
            probe(begin, end);
            forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
                if ((state & Written) == 0) return;
                if (!runs.empty() && runs.back().second == page) runs.back().second = page + _page;
                else runs.emplace_back(page, page + _page);
                for (const auto& [first, last] : _hostWrites)
                    if (page >= first && page < last) return;
                // Код (защиту держит box64) и страницы без записи не защищаем: остаются «записанными».
                if ((state & PROT_WRITE) == 0 || hasCode(page)) return;
                state &= static_cast<std::uint8_t>(~Written);
                state |= Protected;
            });
            for (const auto& [first, last] : runs) protect(first, last);
        }
        for (const auto& [first, last] : runs) written(context, first, last);
        if (_trace != nullptr) report();
        return true;
    }

    void report() {
        const auto now = std::chrono::steady_clock::now();
        if (now - _reported < std::chrono::seconds(5)) return;
        _reported = now;
        const unsigned head = _trace->head.load(std::memory_order_relaxed);
        const unsigned count = std::min(head - _traced, TraceRing::Size);
        std::map<std::uintptr_t, unsigned> writers;
        std::map<std::uintptr_t, unsigned> regions;
        for (unsigned i = head - count; i != head; ++i) {
            const auto pc = _trace->pc[i % TraceRing::Size];
            void* x64 = _toX64 != nullptr ? _toX64(reinterpret_cast<void*>(pc)) : nullptr;
            ++writers[x64 != nullptr ? reinterpret_cast<std::uintptr_t>(x64) : (pc | (1ull << 63))];
            ++regions[_trace->page[i % TraceRing::Size] & ~std::uintptr_t{0xfffff}];
        }
        _traced = head;
        std::vector<std::pair<unsigned, std::uintptr_t>> top;
        for (const auto& [key, n] : writers) top.emplace_back(n, key);
        std::sort(top.rbegin(), top.rend());
        std::fprintf(stderr, "[watch] %u write faults in 5 s; writers:", count);
        for (std::size_t i = 0; i < top.size() && i < 12; ++i) {
            const auto key = top[i].second;
            if (key >> 63) {
                Dl_info dl{};
                const auto pc = key & ~(1ull << 63);
                if (dladdr(reinterpret_cast<void*>(pc), &dl) && dl.dli_fname != nullptr) {
                    const char* name = std::strrchr(dl.dli_fname, '/');
                    std::fprintf(stderr, " %u@%s+0x%llx", top[i].first, name ? name + 1 : dl.dli_fname, static_cast<unsigned long long>(pc - reinterpret_cast<std::uintptr_t>(dl.dli_fbase)));
                } else {
                    std::fprintf(stderr, " %u@native:0x%llx", top[i].first, static_cast<unsigned long long>(pc));
                }
            } else {
                std::fprintf(stderr, " %u@x64:0x%llx", top[i].first, static_cast<unsigned long long>(key));
            }
        }
        std::vector<std::pair<unsigned, std::uintptr_t>> hot;
        for (const auto& [key, n] : regions) hot.emplace_back(n, key);
        std::sort(hot.rbegin(), hot.rend());
        std::size_t total = 0, writable = 0, protectedPages = 0, written = 0, code = 0;
        {
            const GuardedLock lock(_lock);
            for (const auto& [begin, range] : _ranges) {
                for (std::size_t i = 0; i < range.pages.size(); ++i) {
                    const auto state = range.pages[i];
                    ++total;
                    if (state & PROT_WRITE) ++writable;
                    if (state & Protected) ++protectedPages;
                    if (state & Written) ++written;
                    if ((i & 255) == 0 && hasCode(begin + i * _page)) ++code;
                }
            }
        }
        std::fprintf(stderr, "; pages %zu (%zu ranges): writable %zu protected %zu written %zu code~%zu", total, _ranges.size(), writable, protectedPages, written, code * 256);
        {
            static int dumped = 0;
            if (dumped++ < 2) {
                const GuardedLock lock(_lock);
                int shown = 0;
                for (const auto& [begin, range] : _ranges) {
                    if (range.pages.size() < 64 || shown++ >= 6) continue;
                    std::vector<std::uint8_t> actual(1, 0);
                    readProtections(begin, begin + _page, actual);
                    std::fprintf(stderr, " | 0x%llx+0x%llx state0=0x%x actual0=0x%x", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(range.end - begin), range.pages[0], actual[0]);
                }
            }
        }
        std::fprintf(stderr, "; MiB regions:");
        for (std::size_t i = 0; i < hot.size() && i < 8; ++i) std::fprintf(stderr, " 0x%llx=%u", static_cast<unsigned long long>(hot[i].second), hot[i].first);
        std::fprintf(stderr, "\n");
    }

    void ProtectionChanged(std::uintptr_t begin, std::uintptr_t end, int protection) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        const GuardedLock lock(_lock);
        forEachPage(begin, end, [&](std::uintptr_t, std::uint8_t& state) {
            state = static_cast<std::uint8_t>(Written | (protection & ProtectionMask));
        });
    }

    void BeginHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        const GuardedLock lock(_lock);
        _hostWrites.emplace_back(begin, end);
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            if ((state & Protected) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            state = static_cast<std::uint8_t>((state & ProtectionMask) | Written);
        });
    }

    void EndHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        const GuardedLock lock(_lock);
        const auto found = std::find(_hostWrites.begin(), _hostWrites.end(), std::pair{begin, end});
        if (found != _hostWrites.end()) _hostWrites.erase(found);
    }

    int Protection(std::uintptr_t address, std::uintptr_t limit, std::uintptr_t* runEnd) {
        const auto page = pageStart(address);
        const GuardedLock lock(_lock);
        // Права диапазона, зарегистрированного раньше, чем память их получила, ещё не прочитаны:
        // без probe() здесь нули, и драйвер считал бы страницы незакоммиченными.
        probe(page, page + _page);
        auto it = _ranges.upper_bound(page);
        if (_available && it != _ranges.begin() && page < std::prev(it)->second.end) {
            --it;
            const auto& pages = it->second.pages;
            const std::uint8_t protection = pages[(page - it->first) / _page] & ProtectionMask;
            auto end = page + _page;
            while (end < it->second.end && end < limit && (pages[(end - it->first) / _page] & ProtectionMask) == protection) end += _page;
            *runEnd = std::min(end, limit);
            return protection;
        }
        *runEnd = it != _ranges.end() ? std::min(it->first, limit) : limit;
        return -1;
    }

private:
    static constexpr std::uint8_t ProtectionMask = PROT_READ | PROT_WRITE | PROT_EXEC;
    static constexpr std::uint8_t Written = 0x10;
    static constexpr std::uint8_t Protected = 0x20;

    struct Range {
        std::uintptr_t end;
        std::vector<std::uint8_t> pages;
        // Права перечитаны при первом Collect: регистрация бывает раньше, чем память получает права.
        bool probed = false;
    };

    // Билетная спин-блокировка: годится и в обработчике сигнала (под ней в память гостя не пишут).
    struct Turns {
        std::atomic<std::uint64_t> next{0};
        std::atomic<std::uint64_t> serving{0};
    };

    struct Lock {
        explicit Lock(Turns& turns) : _turns(turns), _turn(turns.next.fetch_add(1, std::memory_order_relaxed)) {
            while (_turns.serving.load(std::memory_order_acquire) != _turn) ::sched_yield();
        }
        ~Lock() {
            _turns.serving.store(_turn + 1, std::memory_order_release);
        }
        Turns& _turns;
        std::uint64_t _turn;
    };

    // Блокировка вне обработчика ошибок: на время удержания асинхронные сигналы потока заблокированы.
    // Иначе поток, остановленный с блокировкой в руках (остановка потоков сборщиком мусора Unity идёт
    // сигналом), держит её, пока остальные потоки ждут её в обработчике SIGSEGV — взаимная блокировка.
    struct GuardedLock {
        explicit GuardedLock(Turns& turns) : _mask(blockAsync()), _lock(turns) {}
        ~GuardedLock() = default;
        struct Mask {
            sigset_t previous;
            ~Mask() { pthread_sigmask(SIG_SETMASK, &previous, nullptr); }
        };
        static Mask blockAsync() {
            Mask mask;
            sigset_t all;
            sigfillset(&all);
            sigdelset(&all, SIGSEGV);
            sigdelset(&all, SIGBUS);
            sigdelset(&all, SIGILL);
            sigdelset(&all, SIGFPE);
            pthread_sigmask(SIG_BLOCK, &all, &mask.previous);
            return mask;
        }
        Mask _mask;
        Lock _lock;
    };

    // Обработчик ставится при первом использовании: к этому времени box64 уже поставил свой.
    void ensureHandler() {
        if (_installed.exchange(true)) return;
        if (std::getenv("APS5_NO_WRITE_WATCH") != nullptr) return;
        if (void* box64 = dlopen("libbox64.so", RTLD_NOW | RTLD_NOLOAD)) {
            _hasCode = reinterpret_cast<PageHasCode>(dlsym(box64, "box64_page_has_code"));
            _toX64 = reinterpret_cast<NativeToX64>(dlsym(box64, "box64_native_to_x64"));
        }
        if (std::getenv("APS5_TRACE_WATCH") != nullptr) _trace = new TraceRing();
        if (const char* granule = std::getenv("APS5_WATCH_GRANULE")) {
            const long pages = std::strtol(granule, nullptr, 0);
            if (pages >= 1 && pages <= 4096 && (pages & (pages - 1)) == 0) _granule = static_cast<std::uintptr_t>(pages);
        }
        struct sigaction action {};
        action.sa_sigaction = &Watch::fault;
        action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGBUS, &action, &_previousBus) != 0 || sigaction(SIGSEGV, &action, &_previousSegv) != 0) {
            std::fprintf(stderr, "[memory] native write watch: cannot install the fault handler (%s)\n", std::strerror(errno));
            return;
        }
        std::fprintf(stderr, "[memory] native write watch: mprotect + SIGSEGV%s\n", _hasCode ? "" : " (box64 code pages unknown)");
        _available = true;
    }

    bool hasCode(std::uintptr_t page) const {
        return _hasCode != nullptr && _hasCode(page) != 0;
    }

    // Crash report for faults in the native group (driver, VideoOut, AudioOut, coreshim, Turnip, libc++,
    // SDL): box64 dies on them without a backtrace. Ranges are collected in normal context (Register);
    // the handler only compares and formats the first such fault. APS5_NO_CRASH_REPORT=1: off.
    struct NativeRange {
        std::uintptr_t begin = 0;
        std::uintptr_t end = 0;
        char name[48] = {};
    };
    static constexpr int NativeRangeMax = 96;
    NativeRange _native[NativeRangeMax];
    std::atomic<int> _nativeCount{0};
    std::atomic<bool> _nativeDriver{false};

    void refreshNativeRanges() {
        if (_nativeDriver.load(std::memory_order_relaxed)) return;
        // Только для отчёта о падении: обход всех библиотек (dl_iterate_phdr) на каждую регистрацию
        // стоил ~4% потока игры, когда драйвер не находился по имени. Не чаще раза в 2 с.
        {
            static std::atomic<std::int64_t> nextWalk{0};
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            auto due = nextWalk.load(std::memory_order_relaxed);
            if (now < due || !nextWalk.compare_exchange_strong(due, now + 2000, std::memory_order_relaxed)) return;
        }
        static const bool disabled = std::getenv("APS5_NO_CRASH_REPORT") != nullptr;
        if (disabled) {
            _nativeDriver.store(true);
            return;
        }
        struct Walk {
            Watch* watch;
            int count;
            bool driver;
        } walk{this, 0, false};
        dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) -> int {
            auto& walk = *static_cast<Walk*>(data);
            const char* name = image->dlpi_name;
            if (name == nullptr || walk.count >= NativeRangeMax) return 0;
            // Every named object (the native group, box64, libc, the linker, Turnip...).
            if (name[0] == '\0') return 0;
            const char* slash = std::strrchr(name, '/');
            const char* matched = slash != nullptr ? slash + 1 : name;
            std::uintptr_t low = ~std::uintptr_t{0}, high = 0;
            for (int i = 0; i < image->dlpi_phnum; ++i) {
                const auto& header = image->dlpi_phdr[i];
                if (header.p_type != PT_LOAD || (header.p_flags & PF_X) == 0) continue;
                low = std::min<std::uintptr_t>(low, image->dlpi_addr + header.p_vaddr);
                high = std::max<std::uintptr_t>(high, image->dlpi_addr + header.p_vaddr + header.p_memsz);
            }
            if (high == 0) return 0;
            auto& range = walk.watch->_native[walk.count++];
            range.begin = image->dlpi_addr;
            range.end = high;
            std::snprintf(range.name, sizeof(range.name), "%s", matched);
            if (std::strstr(matched, "libSceAgcDriver") != nullptr) walk.driver = true;
            return 0;
        }, &walk);
        _nativeCount.store(walk.count, std::memory_order_release);
        if (walk.driver) _nativeDriver.store(true, std::memory_order_release);
    }

    const NativeRange* nativeRangeOf(std::uintptr_t address) const {
        const int count = _nativeCount.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) {
            if (address >= _native[i].begin && address < _native[i].end) return &_native[i];
        }
        return nullptr;
    }

    void reportNativeFault(int signal, siginfo_t* info, ucontext_t* context) {
        const auto pc = static_cast<std::uintptr_t>(context->uc_mcontext.pc);
        const auto* where = nativeRangeOf(pc);
        // Up to 8 reports, each at a different pc: box64 also handles benign faults of native code (a
        // write into a page it protects for self-modifying code), and those must not hide the crash.
        static std::atomic<int> reports{0};
        static std::atomic<std::uintptr_t> reported[8]{};
        if (where == nullptr) return;
        for (auto& seen : reported) {
            if (seen.load(std::memory_order_relaxed) == pc) return;
        }
        const int slot = reports.fetch_add(1);
        if (slot >= 8) return;
        reported[slot].store(pc, std::memory_order_relaxed);
        char line[256];
        const auto put = [&](int length) { if (length > 0) static_cast<void>(::write(STDERR_FILENO, line, static_cast<std::size_t>(std::min<int>(length, sizeof(line) - 1)))); };
        put(std::snprintf(line, sizeof(line), "[crash] signal %d at %s+0x%llx, fault address %p (tid %d)\n", signal, where->name, static_cast<unsigned long long>(pc - where->begin), info->si_addr, static_cast<int>(gettid())));
        const auto frame = [&](const char* label, std::uintptr_t address) {
            if (const auto* range = nativeRangeOf(address)) put(std::snprintf(line, sizeof(line), "[crash]   %s %s+0x%llx\n", label, range->name, static_cast<unsigned long long>(address - range->begin)));
            else put(std::snprintf(line, sizeof(line), "[crash]   %s 0x%llx\n", label, static_cast<unsigned long long>(address)));
        };
        frame("lr", static_cast<std::uintptr_t>(context->uc_mcontext.regs[30]));
        // Frame-pointer chain (x29): [fp] = previous fp, [fp + 8] = return address; only within the
        // thread's stack neighbourhood and strictly upward.
        auto fp = static_cast<std::uintptr_t>(context->uc_mcontext.regs[29]);
        const auto sp = static_cast<std::uintptr_t>(context->uc_mcontext.sp);
        for (int depth = 0; depth < 24 && fp >= sp && fp - sp < (8u << 20) && (fp & 7u) == 0; ++depth) {
            const auto next = reinterpret_cast<const std::uintptr_t*>(fp)[0];
            const auto ret = reinterpret_cast<const std::uintptr_t*>(fp)[1];
            if (ret == 0) break;
            frame("#", ret);
            if (next <= fp) break;
            fp = next;
        }
        std::fflush(stderr);
    }

    static void fault(int signal, siginfo_t* info, void* context) {
        auto& watch = Get();
        const auto pc = static_cast<std::uintptr_t>(static_cast<ucontext_t*>(context)->uc_mcontext.pc);
        if (watch.record(reinterpret_cast<std::uintptr_t>(info->si_addr), pc)) return;
        watch.reportNativeFault(signal, info, static_cast<ucontext_t*>(context));
        const auto& previous = signal == SIGBUS ? watch._previousBus : watch._previousSegv;
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(signal, info, context);
            return;
        }
        if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
            previous.sa_handler(signal);
            return;
        }
        ::signal(signal, SIG_DFL);
    }

    bool record(std::uintptr_t address, std::uintptr_t pc) {
        const auto page = pageStart(address);
        const Lock lock(_lock);
        auto it = _ranges.upper_bound(page);
        if (it == _ranges.begin()) return false;
        --it;
        if (page >= it->second.end) return false;
        auto& state = it->second.pages[(page - it->first) / _page];
        // Страница стала кодом (box64 защищает её сам): отмечаем запись, разбирается box64.
        if (hasCode(page)) {
            if ((state & Protected) != 0) state = static_cast<std::uint8_t>((state & ProtectionMask) | Written);
            return false;
        }
        if ((state & Protected) != 0) {
            // Адаптивно (как точность KytyPS5 — страница): одиночная запись снимает защиту с одной
            // страницы, чтобы соседние текстуры не считались записанными (их потом сравнивают целиком).
            // Последовательная запись потока (ошибка сразу за предыдущей — загрузка большого буфера)
            // снимает защиту с _granule страниц вперёд, иначе загрузки давали лавину ошибок.
            // По умолчанию — всегда блок: трекер драйвера всё равно метит блоки по 64 КБ, а одиночные
            // страницы давали в 16 раз больше прерываний гостевых потоков (APS5_WATCH_ADAPTIVE=1 — адаптивно).
            static const bool adaptive = std::getenv("APS5_WATCH_ADAPTIVE") != nullptr;
            static thread_local std::uintptr_t lastFault = 0;
            const bool sequential = adaptive && lastFault != 0 && page == lastFault + _page;
            const auto& range = it->second;
            const auto blockBytes = _granule * _page;
            auto first = page;
            auto last = page + _page;
            if (!adaptive) {
                // Выровненный блок, как до адаптивного режима.
                first = std::max(it->first, page & ~(blockBytes - 1));
                last = std::min(range.end, (page & ~(blockBytes - 1)) + blockBytes);
            } else if (sequential) {
                last = std::min(range.end, page + blockBytes);
            }
            lastFault = last - _page;
            for (auto cursor = first; cursor < last;) {
                auto& entry = it->second.pages[(cursor - it->first) / _page];
                if ((entry & Protected) == 0) {
                    cursor += _page;
                    continue;
                }
                const std::uint8_t protection = entry & ProtectionMask;
                auto runEnd = cursor;
                while (runEnd < last) {
                    auto& next = it->second.pages[(runEnd - it->first) / _page];
                    if ((next & Protected) == 0 || (next & ProtectionMask) != protection) break;
                    next = static_cast<std::uint8_t>(protection | Written);
                    runEnd += _page;
                }
                ::mprotect(reinterpret_cast<void*>(cursor), runEnd - cursor, protection);
                cursor = runEnd;
            }
            if (_trace != nullptr) {
                const unsigned slot = _trace->head.fetch_add(1, std::memory_order_relaxed) % TraceRing::Size;
                _trace->pc[slot] = pc;
                _trace->page[slot] = page;
            }
            return true;
        }
        // Другой поток уже снял защиту между ошибкой и блокировкой: повторяем инструкцию.
        return (state & PROT_WRITE) != 0;
    }

    // Текущие права страниц диапазонов из /proc/self/maps (нативное чтение: box64 подменяет только x86).
    // Один проход файла на все диапазоны сразу: ядро генерирует его целиком на каждое открытие, а
    // probe() раньше перечитывал его на каждый новый диапазон (в хабе Mewgenics — большая часть кадра).
    struct ProtectionQuery {
        std::uintptr_t begin;
        std::uintptr_t end;
        std::vector<std::uint8_t>* pages;
    };
    void readProtections(std::vector<ProtectionQuery>& queries) const {
        if (queries.empty()) return;
        std::sort(queries.begin(), queries.end(), [](const ProtectionQuery& a, const ProtectionQuery& b) { return a.begin < b.begin; });
        std::FILE* maps = std::fopen("/proc/self/maps", "re");
        if (maps == nullptr) return;
        const std::uintptr_t lowest = queries.front().begin;
        std::uintptr_t highest = 0;
        for (const auto& query : queries) highest = std::max(highest, query.end);
        char line[512];
        std::size_t first_query = 0;
        while (std::fgets(line, sizeof(line), maps) != nullptr) {
            char* cursor = line;
            const unsigned long long first = std::strtoull(cursor, &cursor, 16);
            if (*cursor != '-') continue;
            const unsigned long long last = std::strtoull(cursor + 1, &cursor, 16);
            if (last <= lowest) continue;
            if (first >= highest) break;
            while (*cursor == ' ') ++cursor;
            if (cursor[0] == '\0' || cursor[1] == '\0' || cursor[2] == '\0') continue;
            const std::uint8_t protection = (cursor[0] == 'r' ? PROT_READ : 0) | (cursor[1] == 'w' ? PROT_WRITE : 0) | (cursor[2] == 'x' ? PROT_EXEC : 0);
            while (first_query < queries.size() && queries[first_query].end <= first) ++first_query;
            for (std::size_t q = first_query; q < queries.size() && queries[q].begin < last; ++q) {
                auto& query = queries[q];
                if (query.end <= first) continue;
                for (auto page = std::max<std::uintptr_t>(query.begin, first); page < std::min<std::uintptr_t>(query.end, last); page += _page) (*query.pages)[(page - query.begin) / _page] = Written | protection;
            }
        }
        std::fclose(maps);
    }

    void readProtections(std::uintptr_t begin, std::uintptr_t end, std::vector<std::uint8_t>& pages) const {
        std::vector<ProtectionQuery> queries{{begin, end, &pages}};
        readProtections(queries);
    }

    std::uintptr_t pageStart(std::uintptr_t address) const {
        return address & ~(_page - 1);
    }

    std::uintptr_t pageEnd(std::uintptr_t address) const {
        return (address + _page - 1) & ~(_page - 1);
    }

    // box64 (ANDROID-сборка) отдаёт права страниц из своей карты отображений: /proc/self/maps под box64
    // огромен, и его разбор на каждый новый импорт стоил ~7% потока игры в хабе Mewgenics.
    // APS5_NO_BOX64_PROTECTIONS=1 — всегда /proc/self/maps.
    using Box64Protections = void (*)(std::uintptr_t begin, std::size_t pages, std::size_t page, std::uint8_t* out);
    static Box64Protections box64Protections() {
        static const Box64Protections function = []() -> Box64Protections {
            if (std::getenv("APS5_NO_BOX64_PROTECTIONS") != nullptr) return nullptr;
            void* box64 = dlopen("libbox64.so", RTLD_NOW | RTLD_NOLOAD);
            auto* found = reinterpret_cast<Box64Protections>(box64 != nullptr ? dlsym(box64, "box64_aps5_page_protections") : dlsym(RTLD_DEFAULT, "box64_aps5_page_protections"));
            std::fprintf(stderr, "[write-watch] page protections: %s\n", found != nullptr ? "box64 map" : "/proc/self/maps");
            return found;
        }();
        return function;
    }

    // Страницам с неизвестными (нулевыми) правами — реальные права (box64 или /proc/self/maps), один раз на диапазон.
    void probe(std::uintptr_t begin, std::uintptr_t end) {
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        std::vector<decltype(it)> pending;
        for (; it != _ranges.end() && it->first < end; ++it) {
            if (it->second.probed || it->second.end <= begin) continue;
            it->second.probed = true;
            pending.push_back(it);
        }
        if (pending.empty()) return;
        std::vector<std::vector<std::uint8_t>> actual(pending.size());
        std::vector<ProtectionQuery> queries;
        queries.reserve(pending.size());
        const auto fromBox64 = box64Protections();
        for (std::size_t i = 0; i < pending.size(); ++i) {
            actual[i].assign(pending[i]->second.pages.size(), 0);
            // Права из карты box64; страницы, которых она не знает, — из /proc/self/maps.
            if (fromBox64 != nullptr) {
                fromBox64(pending[i]->first, actual[i].size(), _page, actual[i].data());
                if (std::find(actual[i].begin(), actual[i].end(), 0) == actual[i].end()) continue;
            }
            queries.push_back({pending[i]->first, pending[i]->second.end, &actual[i]});
        }
        readProtections(queries);
        for (std::size_t r = 0; r < pending.size(); ++r) {
            auto& pages = pending[r]->second.pages;
            for (std::size_t i = 0; i < actual[r].size(); ++i) {
                auto& state = pages[i];
                if ((state & ProtectionMask) == 0 && (state & Protected) == 0) state = static_cast<std::uint8_t>(state | (actual[r][i] & ProtectionMask));
            }
        }
    }

    using Coverage = std::vector<std::pair<std::uintptr_t, std::uintptr_t>>;

    // Покрытие после добавления (added) или вырезания [begin, end): erase() вырезает ровно этот
    // интервал, так что новое покрытие — старое плюс или минус он. Копия вектора вместо обхода всех
    // диапазонов (publish на каждую регистрацию стоил ~6% потока игры в хабе Mewgenics).
    static Coverage editCoverage(const Coverage& old, std::uintptr_t begin, std::uintptr_t end, bool added) {
        Coverage result;
        result.reserve(old.size() + 1);
        auto it = std::lower_bound(old.begin(), old.end(), begin, [](const auto& interval, std::uintptr_t value) { return interval.second < value; });
        result.insert(result.end(), old.begin(), it);
        if (added) {
            auto merged = std::make_pair(begin, end);
            for (; it != old.end() && it->first <= end; ++it) {
                merged.first = std::min(merged.first, it->first);
                merged.second = std::max(merged.second, it->second);
            }
            result.push_back(merged);
        } else {
            for (; it != old.end() && it->first < end; ++it) {
                if (it->second <= begin) {
                    result.push_back(*it);
                    continue;
                }
                if (it->first < begin) result.emplace_back(it->first, begin);
                if (end < it->second) result.emplace_back(end, it->second);
            }
        }
        result.insert(result.end(), it, old.end());
        return result;
    }

    // Снимок покрытых интервалов (соседние слиты) для Covers; вызывается под _lock.
    void publish(std::uintptr_t begin, std::uintptr_t end, bool added) {
        static const bool full = std::getenv("APS5_WATCH_FULL_PUBLISH") != nullptr;
        const auto old = std::atomic_load(&_coverage);
        if (full || old == nullptr) {
            publish();
            return;
        }
        std::atomic_store(&_coverage, std::shared_ptr<const Coverage>(std::make_shared<Coverage>(editCoverage(*old, begin, end, added))));
    }

    void publish() {
        auto coverage = std::make_shared<std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>();
        for (const auto& [begin, range] : _ranges) {
            if (!coverage->empty() && coverage->back().second == begin) coverage->back().second = range.end;
            else coverage->emplace_back(begin, range.end);
        }
        std::atomic_store(&_coverage, std::shared_ptr<const std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>(std::move(coverage)));
    }

    template <typename TFunction>
    void forEachPage(std::uintptr_t begin, std::uintptr_t end, TFunction function) {
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        for (; it != _ranges.end() && it->first < end; ++it) {
            const auto first = std::max(begin, it->first);
            const auto last = std::min(end, it->second.end);
            for (auto page = first; page < last; page += _page) function(page, it->second.pages[(page - it->first) / _page]);
        }
    }

    void protect(std::uintptr_t begin, std::uintptr_t end) {
        std::uintptr_t runStart = 0;
        std::uintptr_t previous = begin;
        int runProtection = -1;
        const auto flush = [&](std::uintptr_t runEnd) {
            if (runProtection >= 0) ::mprotect(reinterpret_cast<void*>(runStart), runEnd - runStart, runProtection);
            runProtection = -1;
        };
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            const int wanted = (state & Protected) != 0 ? (state & ProtectionMask & ~PROT_WRITE) : -1;
            if (wanted != runProtection || page != previous) {
                flush(previous);
                runStart = page;
                runProtection = wanted;
            }
            previous = page + _page;
        });
        flush(previous);
    }

    bool covers(std::uintptr_t begin, std::uintptr_t end) const {
        auto it = _ranges.upper_bound(begin);
        if (it == _ranges.begin()) return false;
        --it;
        auto cursor = begin;
        for (; it != _ranges.end() && it->first <= cursor; ++it) {
            if (it->second.end > cursor) cursor = it->second.end;
            if (cursor >= end) return true;
        }
        return false;
    }

    bool erase(std::uintptr_t begin, std::uintptr_t end) {
        bool removed = false;
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        while (it != _ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second.end;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            removed = true;
            auto pages = std::move(it->second.pages);
            it = _ranges.erase(it);
            const auto cutBegin = std::max(begin, rangeBegin);
            const auto cutEnd = std::min(end, rangeEnd);
            for (auto page = cutBegin; page < cutEnd; page += _page) {
                const auto state = pages[(page - rangeBegin) / _page];
                if ((state & Protected) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            }
            if (rangeBegin < cutBegin) _ranges.emplace(rangeBegin, Range{cutBegin, std::vector<std::uint8_t>(pages.begin(), pages.begin() + (cutBegin - rangeBegin) / _page)});
            if (cutEnd < rangeEnd) it = _ranges.emplace(cutEnd, Range{rangeEnd, std::vector<std::uint8_t>(pages.begin() + (cutEnd - rangeBegin) / _page, pages.end())}).first;
        }
        return removed;
    }

    const std::uintptr_t _page = static_cast<std::uintptr_t>(::getpagesize());
    std::atomic<bool> _installed{false};
    bool _available = false;
    PageHasCode _hasCode = nullptr;
    NativeToX64 _toX64 = nullptr;
    std::uintptr_t _granule = 16; // страниц на одно снятие защиты (степень двойки)
    TraceRing* _trace = nullptr;
    unsigned _traced = 0;
    std::chrono::steady_clock::time_point _reported{};
    Turns _lock;
    std::map<std::uintptr_t, Range> _ranges;
    std::shared_ptr<const std::vector<std::pair<std::uintptr_t, std::uintptr_t>>> _coverage;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _hostWrites;
    struct sigaction _previousBus {};
    struct sigaction _previousSegv {};
};

std::uintptr_t address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

} // namespace

// --- для нативного драйвера (имена ядра) -----------------------------------------------------

namespace GuestWriteWatch {
extern "C" bool GuestWriteWatchAvailable_nid_postfix() {
    return Watch::Get().Available();
}
extern "C" void GuestWriteWatchRegister_nid_postfix(const void* pointer, std::size_t bytes) {
    Watch::Get().Register(address(pointer), address(pointer) + bytes);
}
extern "C" bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes) {
    return Watch::Get().Unregister(address(pointer), address(pointer) + bytes);
}
extern "C" void Aps5WatchTraceSource(const char* source) {
    traceSource = source != nullptr ? source : "drv";
}
extern "C" void Aps5WatchInterval(std::uintptr_t address, std::uintptr_t* begin, std::uintptr_t* end) {
    Watch::Get().Interval(address, *begin, *end);
}
extern "C" bool GuestWriteWatchCovers_nid_postfix(std::uintptr_t begin, std::size_t bytes) {
    return Watch::Get().Covers(begin, begin + bytes);
}
extern "C" bool GuestWriteWatchCollect_nid_postfix(std::uintptr_t begin, std::size_t bytes, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
    if (bytes == 0 || begin + bytes < begin) return false;
    return Watch::Get().Collect(begin, begin + bytes, written, context);
}
extern "C" bool GuestWriteWatchCollectArmed_nid_postfix(std::uintptr_t begin, std::size_t bytes, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
    if (bytes == 0 || begin + bytes < begin) return false;
    return Watch::Get().Collect(begin, begin + bytes, written, context);
}
extern "C" bool GuestWriteWatchCollectFresh_nid_postfix(std::uintptr_t begin, std::size_t bytes, void (*)(void*, std::uintptr_t, std::uintptr_t), void*) {
    // Как на macOS: «свежих» диапазонов нет — первая регистрация уже считает всё записанным.
    return Watch::Get().Available() && bytes != 0 && begin + bytes >= begin;
}
extern "C" void GuestWriteWatchBeginHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
    Watch::Get().BeginHostWrite(address(pointer), address(pointer) + bytes);
}
extern "C" void GuestWriteWatchEndHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
    Watch::Get().EndHostWrite(address(pointer), address(pointer) + bytes);
}
extern "C" void GuestWriteWatchProtectionChanged_nid_postfix(const void* pointer, std::size_t bytes, int protection) {
    Watch::Get().ProtectionChanged(address(pointer), address(pointer) + bytes, protection);
}
extern "C" int GuestWriteWatchProtection_nid_postfix(std::uintptr_t begin, std::uintptr_t limit, std::uintptr_t* runEnd) {
    return Watch::Get().Protection(begin, limit, runEnd);
}
} // namespace GuestWriteWatch

// --- для x86-ядра через мост box64 (aps5b_call: до 8 целых аргументов, результат u64) ---------

extern "C" std::uint64_t Aps5WatchAvailable() {
    return Watch::Get().Available() ? 1 : 0;
}
extern "C" std::uint64_t Aps5WatchRegister(std::uint64_t pointer, std::uint64_t bytes) {
    struct Source { Source() { traceSource = "x86"; } ~Source() { traceSource = "drv"; } } source;
    Watch::Get().Register(pointer, pointer + bytes);
    return 0;
}
extern "C" std::uint64_t Aps5WatchUnregister(std::uint64_t pointer, std::uint64_t bytes) {
    struct Source { Source() { traceSource = "x86"; } ~Source() { traceSource = "drv"; } } source;
    return Watch::Get().Unregister(pointer, pointer + bytes) ? 1 : 0;
}
extern "C" std::uint64_t Aps5WatchBeginHostWrite(std::uint64_t pointer, std::uint64_t bytes) {
    Watch::Get().BeginHostWrite(pointer, pointer + bytes);
    return 0;
}
extern "C" std::uint64_t Aps5WatchEndHostWrite(std::uint64_t pointer, std::uint64_t bytes) {
    Watch::Get().EndHostWrite(pointer, pointer + bytes);
    return 0;
}
extern "C" std::uint64_t Aps5WatchProtectionChanged(std::uint64_t pointer, std::uint64_t bytes, std::uint64_t protection) {
    Watch::Get().ProtectionChanged(pointer, pointer + bytes, static_cast<int>(protection));
    return 0;
}
extern "C" std::uint64_t Aps5WatchProtection(std::uint64_t begin, std::uint64_t limit, std::uint64_t runEnd) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(Watch::Get().Protection(begin, limit, reinterpret_cast<std::uintptr_t*>(runEnd))));
}
