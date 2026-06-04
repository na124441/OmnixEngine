#include "TimeManager.h"
#include <stdexcept>
#include <algorithm>

TimeManager::TimeManager()
    : m_frameTimer(nullptr),
    m_timeScale(nullptr),
    m_frameBudget(nullptr),
    m_frameCount(0),
    m_totalUnscaledTime(0.0),
    m_totalScaledTime(0.0),
    m_unscaledDeltaTime(0.0),
    m_scaledDeltaTime(0.0),
    m_fixedAccumulator(0.0)
{
}

// Initialization Algorithm
void TimeManager::Initialize(double targetFrameTimeMs)
{
    // i. Initialize FrameTimer
    m_frameTimer = std::make_unique<FrameTimer>();

    // ii. Initialize TimeScale
    m_timeScale = std::make_unique<TimeScale>();

    // iii. Initialize FrameBudget
    m_frameBudget = std::make_unique<FrameBudget>(targetFrameTimeMs);

    // iv. Reset all counters and accumulators
    m_frameCount = 0;
    m_totalUnscaledTime = 0.0;
    m_totalScaledTime = 0.0;
    m_unscaledDeltaTime = 0.0;
    m_scaledDeltaTime = 0.0;
    m_fixedAccumulator = 0.0;
}

// BeginFrame Algorithm
void TimeManager::BeginFrame()
{
    if (!m_frameTimer || !m_timeScale || !m_frameBudget)
    {
        throw std::runtime_error("TimeManager not initialized. Call Initialize() first.");
    }

    // i. Measure reality
    // call FrameTimer.BeginFrame()
    m_frameTimer->BeginFrame();

    // rawDelta = FrameTimer.GetRawDeltaTime()
    double rawDeltaMs = m_frameTimer->GetRawDeltaTimeMillis();

    // ii. Sanity clamp
    // If rawDelta > maxAllowed -> Clamp to maxAllowed
    if (rawDeltaMs > MAX_ALLOWED_DELTA_MS)
    {
        rawDeltaMs = MAX_ALLOWED_DELTA_MS;
    }

    // Assign to unscaledDeltaTime
    m_unscaledDeltaTime = rawDeltaMs;

    // iii. Accumulate unscaledDeltaTime
    // totalUnscaledTime += unscaledDeltaTime
    m_totalUnscaledTime += m_unscaledDeltaTime;

    // iv. Apply Time scaling
    // scaledDeltaTime = ApplyTimeScaling(unscaledDeltaTime)
    m_scaledDeltaTime = m_timeScale->ApplyScale(m_unscaledDeltaTime);

    // totalScaledTime += scaledDeltaTime
    m_totalScaledTime += m_scaledDeltaTime;

    // Accumulate for fixed timestep
    m_fixedAccumulator += m_scaledDeltaTime;

    // Update frame budget with actual frame time
    m_frameBudget->OnFrameEnd(rawDeltaMs);

    // Increment frame counter
    m_frameCount++;
}

// Immutable time state query
TimeState TimeManager::GetTimeState() const
{
    TimeState state;
    state.frameCount = m_frameCount;
    state.totalUnscaledTime = m_totalUnscaledTime;
    state.totalScaledTime = m_totalScaledTime;
    state.unscaledDeltaTime = m_unscaledDeltaTime;
    state.scaledDeltaTime = m_scaledDeltaTime;
    state.timeScale = m_timeScale->GetTimeScale();

    return state;
}

double TimeManager::GetTimeScale() const
{
    if (!m_timeScale)
    {
        throw std::runtime_error("TimeManager not initialized");
    }

    return m_timeScale->GetTimeScale();
}

// Configuration methods

void TimeManager::SetTimeScale(double scale)
{
    if (!m_timeScale)
    {
        throw std::runtime_error("TimeManager not initialized");
    }

    m_timeScale->SetTimeScale(scale);
}

void TimeManager::SetMaxDeltaTime(double maxDeltaMs)
{
    if (maxDeltaMs <= 0.0)
    {
        throw std::invalid_argument("Max delta time must be positive");
    }

    // Update the internal constant by using a setter pattern if needed
    // For now, we'd need to refactor to make MAX_ALLOWED_DELTA_MS configurable
}

void TimeManager::Pause()
{
    if (!m_timeScale)
    {
        throw std::runtime_error("TimeManager not initialized");
    }

    m_timeScale->Pause();
}

void TimeManager::Resume()
{
    if (!m_timeScale)
    {
        throw std::runtime_error("TimeManager not initialized");
    }

    m_timeScale->Resume();
}

void TimeManager::Reset()
{
    m_frameCount = 0;
    m_totalUnscaledTime = 0.0;
    m_totalScaledTime = 0.0;
    m_unscaledDeltaTime = 0.0;
    m_scaledDeltaTime = 0.0;
    m_fixedAccumulator = 0.0;

    if (m_timeScale)
    {
        m_timeScale->Reset();
    }

    if (m_frameBudget)
    {
        m_frameBudget->Reset();
    }
}

// Fixed timestep support

void TimeManager::AccumulateFixedTime(double fixedTimeStepMs)
{
    if (fixedTimeStepMs <= 0.0)
    {
        throw std::invalid_argument("Fixed timestep must be positive");
    }

    m_fixedAccumulator += fixedTimeStepMs;
}

bool TimeManager::TryConsumeFixedTimeStep(double fixedTimeStepMs)
{
    if (fixedTimeStepMs <= 0.0)
    {
        throw std::invalid_argument("Fixed timestep must be positive");
    }

    if (m_fixedAccumulator >= fixedTimeStepMs)
    {
        m_fixedAccumulator -= fixedTimeStepMs;
        return true;
    }

    return false;
}