#pragma once
#include <chrono>
#include <algorithm>

namespace eng {

class FrameTimer {
public:
    FrameTimer() {
        start = std::chrono::steady_clock::now();
        lastFrame = start;
    }

    void tick() {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> diff = now - lastFrame;
        delta = diff.count();
        lastFrame = now;

        m_frames++;

        // Statistics
        if (delta > 0) {
            minDelta = std::min(minDelta, delta);
            maxDelta = std::max(maxDelta, delta);
        }
    }

    double deltaMs() const { return delta; }
    
    // Compatibility with baseline renderer
    uint64_t frames()      const { return m_frames; }
    double minDeltaMs()   const { return minDelta; }
    double maxDeltaMs()   const { return maxDelta; }

    // Original methods
    double minMs()   const { return minDelta; }
    double maxMs()   const { return maxDelta; }

    void resetStats() {
        minDelta = 1000000.0;
        maxDelta = 0.0;
        m_frames = 0;
    }

private:
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point lastFrame;
    double delta = 0.0;
    double minDelta = 1000000.0;
    double maxDelta = 0.0;
    uint64_t m_frames = 0;
};

} // namespace eng
