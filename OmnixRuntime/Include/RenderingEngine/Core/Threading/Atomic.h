#pragma once
#include "Core/Threading/Atomic.h"

namespace eng {

    template <typename T>
    using Atomic = eng::core::Atomic<T>;

    using eng::core::AtomicLoad;
    using eng::core::AtomicStore;

} // namespace eng
