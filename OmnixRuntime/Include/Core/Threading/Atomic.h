#pragma once

#include <atomic>

namespace eng::core {

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

} // namespace eng::core
