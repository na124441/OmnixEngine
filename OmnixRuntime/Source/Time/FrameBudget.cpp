#include "Time/FrameBudget.h"
#include <stdexcept>
#include <iostream>

// 1. On Initialization
FrameBudget::FrameBudget(double targetFrameTimeMs)
    : m_targetFrameTime(targetFrameTimeMs),
    m_lastFrameDuration(0.0),
    m_movingAverage(0.0),
    m_worstFrame(0.0),
    m_budgetExceeded(false)
{
    // Load target frame duration
    if (targetFrameTimeMs <= 0.0)
    {
        throw std::invalid_argument("Target frame time must be positive");
    }

    // reset statics
    m_lastFrameDuration = 0.0;
    m_movingAverage = 0.0;
    m_worstFrame = 0.0;
    m_budgetExceeded = false;
}

// 2. On OnFrameEnd(actualFrameTime)
void FrameBudget::OnFrameEnd(double actualFrameTimeMs)
{
    if (actualFrameTimeMs < 0.0)
    {
        throw std::invalid_argument("Frame time cannot be negative");
    }

    // lastFrameDuration = actualFrameTime
    m_lastFrameDuration = actualFrameTimeMs;

    // update moving average
    // Exponential Moving Average: EMA = (value * alpha) + (EMA_prev * (1 - alpha))
    if (m_movingAverage == 0.0)
    {
        // First frame - initialize with actual value
        m_movingAverage = actualFrameTimeMs;
    }
    else
    {
        m_movingAverage = (actualFrameTimeMs * MOVING_AVERAGE_ALPHA) +
            (m_movingAverage * (1.0 - MOVING_AVERAGE_ALPHA));
    }

    // update worst frame if needed
    m_worstFrame = std::max(m_worstFrame, actualFrameTimeMs);

    // budgetExceed = actualFrameTime > targetFrameTime
    m_budgetExceeded = actualFrameTimeMs > m_targetFrameTime;
}

// 3. Provide queries

// IsOverBudget()
bool FrameBudget::IsOverBudget() const
{
    return m_budgetExceeded;
}

// GetFramePressureRatio() = actual/target
double FrameBudget::GetFramePressureRatio() const
{
    if (m_targetFrameTime == 0.0)
    {
        return 0.0;
    }

    return m_lastFrameDuration / m_targetFrameTime;
}

// GetAverageFrameTime()
double FrameBudget::GetAverageFrameTime() const
{
    return m_movingAverage;
}

// Additional utility methods

double FrameBudget::GetTargetFrameTime() const
{
    return m_targetFrameTime;
}

double FrameBudget::GetLastFrameDuration() const
{
    return m_lastFrameDuration;
}

double FrameBudget::GetWorstFrameTime() const
{
    return m_worstFrame;
}

double FrameBudget::GetFrameTimeUndershoot() const
{
    // How much time we saved (if negative, we overran)
    double undershoot = m_targetFrameTime - m_lastFrameDuration;
    return std::max(0.0, undershoot);
}

double FrameBudget::GetFrameTimeOvershoot() const
{
    // How much we exceeded budget (if negative, we were under)
    double overshoot = m_lastFrameDuration - m_targetFrameTime;
    return std::max(0.0, overshoot);
}

// Configuration methods

void FrameBudget::SetTargetFrameTime(double targetFrameTimeMs)
{
    if (targetFrameTimeMs <= 0.0)
    {
        throw std::invalid_argument("Target frame time must be positive");
    }

    m_targetFrameTime = targetFrameTimeMs;
}

void FrameBudget::Reset()
{
    m_lastFrameDuration = 0.0;
    m_movingAverage = 0.0;
    m_worstFrame = 0.0;
    m_budgetExceeded = false;
}