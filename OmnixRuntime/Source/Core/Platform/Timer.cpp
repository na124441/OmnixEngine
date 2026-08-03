#include "Core/Platform/Timer.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <chrono>
#endif

namespace eng::platform {

    eng::core::ResultCode Timer::Start() noexcept {
#ifdef _WIN32
        LARGE_INTEGER freq;
        if (!QueryPerformanceFrequency(&freq)) {
            return eng::core::ResultCode::Failure;
        }
        frequency_ = freq.QuadPart;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        startTicks_ = now.QuadPart;
        lastTicks_ = startTicks_;
        return eng::core::ResultCode::Success;
#else
        // POSIX standard steady clock fallback
        frequency_ = 1000000000ULL; // Nanoseconds frequency
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        startTicks_ = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        lastTicks_ = startTicks_;
        return eng::core::ResultCode::Success;
#endif
    }

    double Timer::Tick() noexcept {
#ifdef _WIN32
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        uint64_t current = now.QuadPart;
        double dt = static_cast<double>(current - lastTicks_) / frequency_;
        lastTicks_ = current;
        return dt;
#else
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t current = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        double dt = static_cast<double>(current - lastTicks_) / 1000000000.0;
        lastTicks_ = current;
        return dt;
#endif
    }

    uint64_t Timer::GetRawTicks() const noexcept {
        return lastTicks_;
    }

    uint64_t Timer::GetFrequency() const noexcept {
        return frequency_;
    }

} // namespace eng::platform
