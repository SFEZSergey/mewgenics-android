#ifndef CORE_LIBS_PRX_APS5NATIVE_ATOMICSHAREDPTR_HPP
#define CORE_LIBS_PRX_APS5NATIVE_ATOMICSHAREDPTR_HPP

// std::atomic<std::shared_ptr<T>> (C++20, P0718) для libc++, где он не реализован: Android-сборка
// AnyPS5 (bionic + libc++). Подключается ко всем файлам флагом -include только в этой сборке.
// Реализация на мьютексе: операции без блокировок libstdc++ здесь не нужны, важна семантика.

#ifdef __cplusplus
#include <version>
#endif
#if defined(__cplusplus) && defined(_LIBCPP_VERSION)

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace std {

template <class T>
struct atomic<shared_ptr<T>> {
    using value_type = shared_ptr<T>;
    static constexpr bool is_always_lock_free = false;

    constexpr atomic() noexcept = default;
    atomic(shared_ptr<T> desired) noexcept : value(std::move(desired)) {}
    atomic(const atomic&) = delete;
    void operator=(const atomic&) = delete;

    bool is_lock_free() const noexcept { return false; }

    void store(shared_ptr<T> desired, memory_order = memory_order_seq_cst) noexcept {
        shared_ptr<T> old;
        {
            lock_guard<std::mutex> lock(guard);
            old = std::exchange(value, std::move(desired));
            ++version;
        }
    }
    void operator=(shared_ptr<T> desired) noexcept { store(std::move(desired)); }

    shared_ptr<T> load(memory_order = memory_order_seq_cst) const noexcept {
        lock_guard<std::mutex> lock(guard);
        return value;
    }
    operator shared_ptr<T>() const noexcept { return load(); }

    shared_ptr<T> exchange(shared_ptr<T> desired, memory_order = memory_order_seq_cst) noexcept {
        lock_guard<std::mutex> lock(guard);
        ++version;
        return std::exchange(value, std::move(desired));
    }

    bool compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T> desired, memory_order = memory_order_seq_cst, memory_order = memory_order_seq_cst) noexcept {
        shared_ptr<T> old;
        lock_guard<std::mutex> lock(guard);
        if (value.get() == expected.get() && !value.owner_before(expected) && !expected.owner_before(value)) {
            old = std::exchange(value, std::move(desired));
            ++version;
            return true;
        }
        expected = value;
        return false;
    }
    bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T> desired, memory_order success = memory_order_seq_cst, memory_order failure = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, std::move(desired), success, failure);
    }

    // wait/notify: ждём смены значения (по указателю и владельцу), как в стандарте.
    void wait(shared_ptr<T> old, memory_order = memory_order_seq_cst) const noexcept {
        unique_lock<std::mutex> lock(guard);
        changed.wait(lock, [&] { return value.get() != old.get() || value.owner_before(old) || old.owner_before(value); });
    }
    void notify_one() noexcept { changed.notify_one(); }
    void notify_all() noexcept { changed.notify_all(); }

private:
    mutable std::mutex guard;
    mutable std::condition_variable changed;
    shared_ptr<T> value;
    unsigned long long version = 0;
};

} // namespace std

#endif

#endif
