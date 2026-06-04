#pragma once
#include <cstdint>
#include "core/types/Result.h"

namespace eng::platform {

    /**
     * @class Timer
     *
     * Simple high‑resolution timer that works on any supported platform.
     *
     * Usage pattern (typical inside EngineLoop):
     *
     *     static Timer g_Timer;
     *     g_Timer.Start();                 // called once at startup
     *
     *     while (running) {
     *         double dt = g_Timer.Tick(); // returns elapsed seconds since last Tick()
     *         // … use dt for physics, animation, etc.
     *     }
     *
     * Thread‑safety: the timer is **not** thread‑safe; the owner thread must call
     * `Tick()` exclusively.
     */
    class Timer {
    public:
        Timer() = default;
        ~Timer() = default;

        /**
         * @brief Initialise the timer. Must be called before first Tick().
         * @return Result::Success, or Failure on platforms where the high‑resolution
         *         counter cannot be queried.
         */
        eng::core::Result Start();

        /**
         * @brief Return the elapsed time (in seconds) since the previous Tick().
         *        The first call after `Start()` returns the time since Start().
         */
        double Tick();

        /** @brief Return the raw tick count (platform‑specific units). */
        uint64_t GetRawTicks() const noexcept;

        /** @brief Frequency of the underlying counter (ticks per second). */
        uint64_t GetFrequency() const noexcept;

    private:
        uint64_t startTicks_{ 0 };
        uint64_t lastTicks_{ 0 };
        uint64_t frequency_{ 0 };
    };

} // namespace eng::platform
