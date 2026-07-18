#include <atomic>
class SpinLock {
public:
    void lock()   noexcept {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Optionally pause instruction on x86:
#if defined(_MSC_VER)
            _mm_pause();
#elif defined(__GNUC__)
            __builtin_ia32_pause();
#endif
        }
    }
    void unlock() noexcept { flag_.clear(std::memory_order_release); }
    bool try_lock() noexcept { return !flag_.test_and_set(std::memory_order_acquire); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};
