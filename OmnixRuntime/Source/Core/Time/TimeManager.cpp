#include "Core/Time/TimeManager.h"
#include <algorithm>

namespace eng::core {

    void TimeManager::Initialize(float fixedTimestep, float maxDeltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_FixedTimestep = fixedTimestep;
        m_MaxDeltaTime = maxDeltaTime;
        m_RawDeltaTime = 0.0f;
        m_SmoothedDeltaTime = 0.0f;
        m_FixedAccumulator = 0.0f;
        m_TotalTime = 0.0;
        m_TimeScale = 1.0f;
        m_IsPaused = false;
    }

    void TimeManager::Reset() noexcept {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_RawDeltaTime = 0.0f;
        m_SmoothedDeltaTime = 0.0f;
        m_FixedAccumulator = 0.0f;
        m_TotalTime = 0.0;
    }

    void TimeManager::Update(float rawDeltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_RawDeltaTime = rawDeltaTime;

        // Clamp spike values to avoid physics explosions on frame hitches
        float clampedDt = std::min(rawDeltaTime, m_MaxDeltaTime);

        // Exponential moving average to smooth frame pacing spikes
        if (m_SmoothedDeltaTime == 0.0f) {
            m_SmoothedDeltaTime = clampedDt;
        } else {
            m_SmoothedDeltaTime = m_SmoothedDeltaTime + 0.15f * (clampedDt - m_SmoothedDeltaTime);
        }

        if (!m_IsPaused) {
            float scaledDt = m_SmoothedDeltaTime * m_TimeScale;
            m_TotalTime += scaledDt;
            m_FixedAccumulator += scaledDt;

            // Cap fixed accumulator to max 5 steps (prevent spiral of death)
            constexpr float kMaxAccumulator = 5.0f * (1.0f / 60.0f);
            if (m_FixedAccumulator > kMaxAccumulator) {
                m_FixedAccumulator = kMaxAccumulator;
            }
        }
    }

    bool TimeManager::AccumulateFixedTime() noexcept {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_FixedAccumulator >= m_FixedTimestep) {
            m_FixedAccumulator -= m_FixedTimestep;
            return true;
        }
        return false;
    }

} // namespace eng::core
