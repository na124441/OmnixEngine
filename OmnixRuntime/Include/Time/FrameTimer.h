//This is the lowest-level component
// Responsibility
//	-Measure real , monotonic wall clock time
//	-Compute raw frame delta
//	-Never apply policy or interpretation
// State
//	-lastTimeStamp
//	-currentTimeStamp
//	-rawDeltaTime
// 
// Algorithm
//	1.On Initialization
//		-Query high resolution clock
//		-set lastTimeStamp = now
//	2.On BeginFrame()
//		-currentTimeStamp = queryClock()
//		-rawDeltTime = current - now
//		-last = current
//	3.Provide
//		-GetRawDeltaTime()
//		-GetCurrentTimeStamp()
// 
//
#pragma once

#include <chrono>
#include <cstdint>

// High-resolution clock type for frame timing
using HighResolutionClock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::high_resolution_clock::duration;

class FrameTimer
{
private:
    TimePoint m_lastTimeStamp;
    TimePoint m_currentTimeStamp;
    Duration m_rawDeltaTime;

    // Query high resolution clock
    static TimePoint QueryClock();

public:
    FrameTimer();
    ~FrameTimer() = default;

    // Algorithm: On BeginFrame()
    void BeginFrame();

    // Provide API
    double GetRawDeltaTime() const;  // Delta time in seconds
    double GetRawDeltaTimeMillis() const;  // Delta time in milliseconds
    double GetCurrentTimeStamp() const;  // Current timestamp in seconds

    // Utility methods
    uint64_t GetCurrentTimeStampNanos() const;  // Current timestamp in nanoseconds
    uint64_t GetRawDeltaTimeNanos() const;  // Delta time in nanoseconds
};