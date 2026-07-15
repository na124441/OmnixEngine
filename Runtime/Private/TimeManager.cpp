#include "Runtime/Public/TimeManager.h"
#include <algorithm>

namespace eng::runtime {

    void TimeManager::Initialize(float fixedTimestep, float maxDeltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_FixedTimestep = fixedTimestep;
        m_MaxDeltaTime = maxDeltaTime;
        m_RawDeltaTime = 0.0f;
        m_SmoothedDeltaTime = 0.0f;
        m_FixedAccumulator = 0.0f;
    }

    void TimeManager::Update(float rawDeltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_RawDeltaTime = rawDeltaTime;

        // T1.1.19 Clamp spike values to avoid physics explosions on frame hitches
        float clampedDt = std::min(rawDeltaTime, m_MaxDeltaTime);

        // Exponential moving average to smooth frame pacing spikes
        if (m_SmoothedDeltaTime == 0.0f) {
            m_SmoothedDeltaTime = clampedDt;
        } else {
            m_SmoothedDeltaTime = m_SmoothedDeltaTime + 0.15f * (clampedDt - m_SmoothedDeltaTime);
        }

        if (!m_IsPaused) {
            m_FixedAccumulator += m_SmoothedDeltaTime * m_TimeScale;
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

} // namespace eng::runtime
