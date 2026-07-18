//This decides how time feels
//	Responsibility
//		-Control Global Simulation speed
//		-Seperate scaled and unscaled time domains
//		-Enable pause , slowmotion
//	State
//		-globalTimeScale (default = 1.0)
//  Algorithm
//		1.On Initializtion
//			-timescale = 1.0
//		2.On SetTimeScale(value)
//			-clamp value > 0
//			-assign to timescale
//		3.On ApplyScale(unscaledDelta)
//			-scaledDelta = unscaledDelta x timescale
//			-return scaledDelta
//		4.On IsPaused()
//			-return timescale == 0
// 
// 
//
#pragma once

#include <algorithm>
#include <cmath>

class TimeScale
{
private:
    double m_globalTimeScale;

    // Utility helper for clamping
    static constexpr double MIN_TIMESCALE = 0.0;
    static constexpr double MAX_TIMESCALE = 1000.0;  // Reasonable upper bound

public:
    TimeScale();
    ~TimeScale() = default;

    // Algorithm: On SetTimeScale(value)
    void SetTimeScale(double value);

    // Algorithm: On ApplyScale(unscaledDelta)
    double ApplyScale(double unscaledDeltaSeconds) const;

    // Algorithm: On IsPaused()
    bool IsPaused() const;

    // Getter for current time scale
    double GetTimeScale() const;

    // Utility methods
    void Pause();
    void Resume();
    void Reset();
};