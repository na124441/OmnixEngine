//This is the single source of temporal truth
// Responsibility
//	-Own the frame lifecycle
//	-Coordinates all time submodules
//	-Publish immutable time state/frame
// State 
//	frameCount
//	totalUnscaledTime
//	totalScaledTime
//	unscaledDeltaTime
//	scaledDeltaTime
//	fixedAccumulator
//	references to : 
//		FrameTimer
//		FrameBudget
//		TimeScale
// Initialization Algorithm 
//	i.Initialize FrameTimer
//	ii.Initialize TimeScale
//	iii.Intialize FrameBudget
//	iv.Reset all counters and accumulators
// 
// BeginFrame()
//	i.Measure reality
//		call FrameTimer.BeginFrame()
//		rawDelta = FrameTimer.GetRawDeltaTime()
//	ii.Sanity clamp 
//		If rawDelta > maxAllowed
//			-> Clamp to maxAllowed
//		Assign to unscaledDeltaTime
//  iii.Accumulate unscaledDeltaTime
//		totalUnscaledTime += unscaledDeltaTime
//	iv.Apply Time scaling
//		scaledDeltaTime = ApplyTimeScaling(unscaledDeltaTime)
//		totalScaledTime += scaledDeltaTime
// 
//
#pragma once

#include <cstdint>
#include <memory>
#include "FrameTimer.h"
#include "TimeScale.h"
#include "FrameBudget.h"

// Immutable time state for the current frame
struct TimeState
{
    uint64_t frameCount;
    double totalUnscaledTime;      // Total elapsed time without scaling
    double totalScaledTime;        // Total elapsed time with time scale applied
    double unscaledDeltaTime;      // Frame delta without scaling
    double scaledDeltaTime;        // Frame delta with scaling applied
    double timeScale;              // Current time scale multiplier

    TimeState()
        : frameCount(0),
        totalUnscaledTime(0.0),
        totalScaledTime(0.0),
        unscaledDeltaTime(0.0),
        scaledDeltaTime(0.0),
        timeScale(1.0)
    {
    }
};

class TimeManager
{
private:
    // Submodule references
    std::unique_ptr<FrameTimer> m_frameTimer;
    std::unique_ptr<TimeScale> m_timeScale;
    std::unique_ptr<FrameBudget> m_frameBudget;

    // State
    uint64_t m_frameCount;
    double m_totalUnscaledTime;
    double m_totalScaledTime;
    double m_unscaledDeltaTime;
    double m_scaledDeltaTime;
    double m_fixedAccumulator;

    // Configuration
    static constexpr double MAX_ALLOWED_DELTA_MS = 100.0;  // Clamp to 100ms max
    static constexpr double TARGET_FRAMETIME_MS = 16.666;  // 60 FPS default

public:
    TimeManager();
    ~TimeManager() = default;

    // Initialization Algorithm
    void Initialize(double targetFrameTimeMs = TARGET_FRAMETIME_MS);

    // BeginFrame Algorithm
    void BeginFrame();

    // Immutable time state query
    TimeState GetTimeState() const;

    // Individual state queries
    uint64_t GetFrameCount() const { return m_frameCount; }
    double GetTotalUnscaledTime() const { return m_totalUnscaledTime; }
    double GetTotalScaledTime() const { return m_totalScaledTime; }
    double GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
    double GetScaledDeltaTime() const { return m_scaledDeltaTime; }
    double GetFixedAccumulator() const { return m_fixedAccumulator; }
    double GetTimeScale() const;

    // Submodule access
    FrameTimer* GetFrameTimer() const { return m_frameTimer.get(); }
    TimeScale* GetTimeScale() const { return m_timeScale.get(); }
    FrameBudget* GetFrameBudget() const { return m_frameBudget.get(); }

    // Configuration
    void SetTimeScale(double scale);
    void SetMaxDeltaTime(double maxDeltaMs);
    void Pause();
    void Resume();
    void Reset();

    // Fixed timestep support
    void AccumulateFixedTime(double fixedTimeStepMs);
    bool TryConsumeFixedTimeStep(double fixedTimeStepMs);
};