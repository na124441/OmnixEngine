#pragma once

namespace eng::core {

    struct FrameTiming {
        double deltaTime = 0.0;     // Time elapsed since last frame (seconds)
        double frameTime = 0.0;     // Total duration of the last full frame loop (seconds)
        double updateTime = 0.0;    // Duration of simulation update stage (seconds)
        double renderTime = 0.0;    // Duration of rendering tick stage (seconds)
    };

} // namespace eng::core
