#pragma once

namespace eng::renderer {

    class GPUProfiler {
    public:
        static void StartFrame();
        static void EndFrame();
        static float GetLastFrameTimeMs();
    };

} // namespace eng::renderer
