#include "TimeScale.h"
#include <stdexcept>

// 1. On Initialization
TimeScale::TimeScale()
    : m_globalTimeScale(1.0)
{
    // timescale = 1.0
}

// 2. On SetTimeScale(value)
void TimeScale::SetTimeScale(double value)
{
    // clamp value >= 0
    if (value < MIN_TIMESCALE)
    {
        throw std::invalid_argument("TimeScale must be >= 0");
    }

    if (value > MAX_TIMESCALE)
    {
        throw std::invalid_argument("TimeScale exceeds maximum value of " + std::to_string(MAX_TIMESCALE));
    }

    // Clamp to valid range (though we've already validated)
    m_globalTimeScale = std::clamp(value, MIN_TIMESCALE, MAX_TIMESCALE);
}

// 3. On ApplyScale(unscaledDelta)
double TimeScale::ApplyScale(double unscaledDeltaSeconds) const
{
    if (unscaledDeltaSeconds < 0.0)
    {
        throw std::invalid_argument("Delta time cannot be negative");
    }

    // scaledDelta = unscaledDelta x timescale
    double scaledDelta = unscaledDeltaSeconds * m_globalTimeScale;

    // return scaledDelta
    return scaledDelta;
}

// 4. On IsPaused()
bool TimeScale::IsPaused() const
{
    // return timescale == 0
    return m_globalTimeScale == 0.0;
}

double TimeScale::GetTimeScale() const
{
    return m_globalTimeScale;
}

void TimeScale::Pause()
{
    SetTimeScale(0.0);
}

void TimeScale::Resume()
{
    SetTimeScale(1.0);
}

void TimeScale::Reset()
{
    SetTimeScale(1.0);
}