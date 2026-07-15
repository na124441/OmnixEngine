#pragma once

#include <mutex>

namespace eng::runtime {

    /**
     * @class TimeManager
     * @brief Manages smoothed and clamped delta time, scales, pauses, and fixed timestep accumulation (T1.1.18, T1.1.19).
     */
    class TimeManager {
    public:
        TimeManager() = default;
        ~TimeManager() = default;

        TimeManager(const TimeManager&) = delete;
        TimeManager& operator=(const TimeManager&) = delete;

        void Initialize(float fixedTimestep = 1.0f / 60.0f, float maxDeltaTime = 0.1f);
        void Update(float rawDeltaTime);

        [[nodiscard]] float GetDeltaTime() const noexcept { return m_IsPaused ? 0.0f : m_SmoothedDeltaTime * m_TimeScale; }
        [[nodiscard]] float GetRawDeltaTime() const noexcept { return m_RawDeltaTime; }
        [[nodiscard]] float GetTimeScale() const noexcept { return m_TimeScale; }
        [[nodiscard]] bool IsPaused() const noexcept { return m_IsPaused; }

        void SetTimeScale(float scale) noexcept { m_TimeScale = scale; }
        void SetPaused(bool paused) noexcept { m_IsPaused = paused; }
        void TogglePause() noexcept { m_IsPaused = !m_IsPaused; }

        [[nodiscard]] float GetFixedTimestep() const noexcept { return m_FixedTimestep; }
        
        /**
         * @brief Checks if enough time has accumulated to run a fixed tick.
         * Decrements the accumulator if true.
         */
        [[nodiscard]] bool AccumulateFixedTime() noexcept;

    private:
        mutable std::mutex m_Mutex;
        float m_RawDeltaTime = 0.0f;
        float m_SmoothedDeltaTime = 0.0f;
        float m_TimeScale = 1.0f;
        bool m_IsPaused = false;

        float m_FixedTimestep = 1.0f / 60.0f;
        float m_FixedAccumulator = 0.0f;
        float m_MaxDeltaTime = 0.1f;
    };

} // namespace eng::runtime
