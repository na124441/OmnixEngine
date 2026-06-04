#include "Timer.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace eng::platform {

    eng::core::Result Timer::Start() {
#ifdef _WIN32
        LARGE_INTEGER freq;
        if (!QueryPerformanceFrequency(&freq)) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }
        frequency_ = freq.QuadPart;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        startTicks_ = now.QuadPart;
        lastTicks_ = startTicks_;
        return eng::core::Result();
#else
        return eng::core::Result(eng::core::ResultCode::Failure);
#endif
    }

    double Timer::Tick() {
#ifdef _WIN32
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        uint64_t current = now.QuadPart;
        double dt = static_cast<double>(current - lastTicks_) / frequency_;
        lastTicks_ = current;
        return dt;
#else
        return 0.0;
#endif
    }

    uint64_t Timer::GetRawTicks() const noexcept {
        return lastTicks_;
    }

    uint64_t Timer::GetFrequency() const noexcept {
        return frequency_;
    }

} // namespace eng::platform
