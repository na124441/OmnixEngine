#pragma once
#include <cstdint>
#include "Core/Error/ResultCode.h"

namespace eng::platform {

    class Timer {
    public:
        Timer() = default;
        ~Timer() = default;

        /**
         * @brief Initialize the high-resolution timer.
         * @return ResultCode::Success, or Failure on platforms where the counter cannot be queried.
         */
        eng::core::ResultCode Start() noexcept;

        /**
         * @brief Return the elapsed time (in seconds) since the previous Tick().
         *        The first call after Start() returns the time since Start().
         */
        double Tick() noexcept;

        /** @brief Return the raw tick count (platform-specific units). */
        uint64_t GetRawTicks() const noexcept;

        /** @brief Frequency of the underlying counter (ticks per second). */
        uint64_t GetFrequency() const noexcept;

    private:
        uint64_t startTicks_{ 0 };
        uint64_t lastTicks_{ 0 };
        uint64_t frequency_{ 0 };
    };

} // namespace eng::platform
