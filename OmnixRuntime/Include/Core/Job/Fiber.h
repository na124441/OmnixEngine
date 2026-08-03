#pragma once

#include <functional>

// Undefine legacy Windows Yield macro to avoid symbol conflicts
#ifdef Yield
#undef Yield
#endif

namespace eng::core {

    class Fiber {
    public:
        using EntryFn = std::function<void(void*)>;
        explicit Fiber(EntryFn entry, void* userData);
        ~Fiber();

        Fiber(const Fiber&) = delete;
        Fiber& operator=(const Fiber&) = delete;

        void SwitchTo();        // transfer execution to this fiber
        void Yield();           // return to the caller fiber

    private:
        void* m_FiberHandle = nullptr;
        void* m_FiberData = nullptr;
    };

} // namespace eng::core
