//This module watches performance without controlling it
// Responsibility
//	-Define target frametime
//	-Detect overruns and pressure
//	-Proivde performance signals to other systems
// State
//	-targetFrameTime(eg.16.6ms)
//  -lastFrameDuration 
//  -movingAverage
//	-worstFrame
//	-budgetExceed(boolean)
// Algorithm
//	1.On Initializtion
//		Load target frame duration
//		reset statics
//	2.On OnFrameEnd(actualFrameTime)
//		lastFrameDuration = actualFrameTime
//		update moving average
//		update worst frame if needed
//		budgetExceed = actualFrameTime > targetFrameTime
//  3.Provide queries : 
//		IsOverBuget()
//		GetFramePressureRatio() = actual/target
//		GetAverageFrameTime()
// 
//
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

class FrameBudget
{
private:
    double m_targetFrameTime;           // Target frame duration in milliseconds
    double m_lastFrameDuration;         // Last frame's actual duration in milliseconds
    double m_movingAverage;             // Exponential moving average of frame times
    double m_worstFrame;                // Worst (longest) frame time observed
    bool m_budgetExceeded;              // True if last frame exceeded budget

    // Configuration
    static constexpr double MOVING_AVERAGE_ALPHA = 0.2;  // Smoothing factor for EMA
    static constexpr double DEFAULT_TARGET_FRAMETIME_MS = 16.666;  // 60 FPS target

public:
    // Constructor with optional target frame time
    explicit FrameBudget(double targetFrameTimeMs = DEFAULT_TARGET_FRAMETIME_MS);
    ~FrameBudget() = default;

    // Algorithm: On OnFrameEnd(actualFrameTime)
    void OnFrameEnd(double actualFrameTimeMs);

    // Algorithm: Provide queries
    bool IsOverBudget() const;
    double GetFramePressureRatio() const;
    double GetAverageFrameTime() const;

    // Additional utility methods
    double GetTargetFrameTime() const;
    double GetLastFrameDuration() const;
    double GetWorstFrameTime() const;
    double GetFrameTimeUndershoot() const;
    double GetFrameTimeOvershoot() const;

    // Configuration
    void SetTargetFrameTime(double targetFrameTimeMs);
    void Reset();
};