#pragma once

#include <mutex>

namespace eng::core {

    /**
     * @class TimeManager
     * @brief Manages smoothed and clamped delta time, frame pacing, game speed scaling, pause states, and fixed timestep accumulation.
     */
    class TimeManager {
    public:
        TimeManager() = default;
        ~TimeManager() = default;

        TimeManager(const TimeManager&) = delete;
        TimeManager& operator=(const TimeManager&) = delete;

        void Initialize(float fixedTimestep = 1.0f / 60.0f, float maxDeltaTime = 0.1f);
        void Update(float rawDeltaTime);
        void Reset() noexcept;

        [[nodiscard]] float GetDeltaTime() const noexcept { return m_IsPaused ? 0.0f : m_SmoothedDeltaTime * m_TimeScale; }
        [[nodiscard]] float GetUnscaledDeltaTime() const noexcept { return m_IsPaused ? 0.0f : m_SmoothedDeltaTime; }
        [[nodiscard]] float GetRawDeltaTime() const noexcept { return m_RawDeltaTime; }
        [[nodiscard]] float GetSmoothedDeltaTime() const noexcept { return m_SmoothedDeltaTime; }
        [[nodiscard]] float GetTimeScale() const noexcept { return m_TimeScale; }
        [[nodiscard]] double GetTotalTime() const noexcept { return m_TotalTime; }
        [[nodiscard]] bool IsPaused() const noexcept { return m_IsPaused; }

        void SetTimeScale(float scale) noexcept { m_TimeScale = (scale < 0.0f) ? 0.0f : scale; }
        void SetPaused(bool paused) noexcept { m_IsPaused = paused; }
        void TogglePause() noexcept { m_IsPaused = !m_IsPaused; }

        [[nodiscard]] float GetFixedTimestep() const noexcept { return m_FixedTimestep; }
        [[nodiscard]] float GetFixedAccumulator() const noexcept { return m_FixedAccumulator; }
        
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
        double m_TotalTime = 0.0;
        bool m_IsPaused = false;

        float m_FixedTimestep = 1.0f / 60.0f;
        float m_FixedAccumulator = 0.0f;
        float m_MaxDeltaTime = 0.1f;
    };

} // namespace eng::core
