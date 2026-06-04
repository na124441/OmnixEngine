#pragma once
#include <chrono>
#include <algorithm>
#include <limits>

class FrameTimer {
public:
    FrameTimer()
    {
        lastTime = Clock::now();
        delta = 0.0;
        minDelta = std::numeric_limits<double>::max();
        maxDelta = 0.0;
        frameCount = 0;
    }

    void tick()
    {
        auto now = Clock::now();
        delta = std::chrono::duration<double, std::milli>(now - lastTime).count(); // ms
        lastTime = now;

        minDelta = std::min(minDelta, delta);
        maxDelta = std::max(maxDelta, delta);
        ++frameCount;
    }

    double   deltaMs()   const { return delta; }
    double   minDeltaMs() const { return minDelta; }
    double   maxDeltaMs() const { return maxDelta; }
    uint64_t frames()    const { return frameCount; }

    void resetStats()
    {
        minDelta = std::numeric_limits<double>::max();
        maxDelta = 0.0;
        frameCount = 0;
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point lastTime;
    double delta;
    double minDelta, maxDelta;
    uint64_t frameCount;
};
