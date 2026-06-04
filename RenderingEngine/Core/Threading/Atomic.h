#include <atomic>
namespace eng {

    template <typename T>
    using Atomic = std::atomic<T>;

    template <typename T>
    inline T AtomicLoad(const Atomic<T>& a, std::memory_order order = std::memory_order_relaxed) {
        return a.load(order);
    }
    template <typename T>
    inline void AtomicStore(Atomic<T>& a, T desired, std::memory_order order = std::memory_order_relaxed) {
        a.store(desired, order);
    }

    // Add any extra helpers needed in the future.
}
