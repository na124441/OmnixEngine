#include "Time/FrameTimer.h"
#include <>
// Static helper to query the high resolution clock
TimePoint FrameTimer::QueryClock()
{
    return HighResolutionClock::now();
}

// 1. On Initialization
FrameTimer::FrameTimer()
{
    // Query high resolution clock
    m_lastTimeStamp = QueryClock();

    // set lastTimeStamp = now
    m_currentTimeStamp = m_lastTimeStamp;
    m_rawDeltaTime = Duration::zero();
}

// 2. On BeginFrame()
void FrameTimer::BeginFrame()
{
    // currentTimeStamp = queryClock()
    m_currentTimeStamp = QueryClock();

    // rawDeltTime = current - last
    m_rawDeltaTime = m_currentTimeStamp - m_lastTimeStamp;

    // last = current
    m_lastTimeStamp = m_currentTimeStamp;
}

// 3. Provide API

double FrameTimer::GetRawDeltaTime() const
{
    return std::chrono::duration<double>(m_rawDeltaTime).count();
}

double FrameTimer::GetRawDeltaTimeMillis() const
{
    return std::chrono::duration<double, std::milli>(m_rawDeltaTime).count();
}

double FrameTimer::GetCurrentTimeStamp() const
{
    // Return time since high resolution clock epoch in seconds
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        m_currentTimeStamp.time_since_epoch());
    return std::chrono::duration<double>(nanoseconds).count();
}

uint64_t FrameTimer::GetCurrentTimeStampNanos() const
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        m_currentTimeStamp.time_since_epoch()).count();
}

uint64_t FrameTimer::GetRawDeltaTimeNanos() const
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(m_rawDeltaTime).count();
}