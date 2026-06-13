#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace eng::renderer {

    struct RenderPassTiming {
        std::string name;
        float gpuMs = 0.0f;
    };

    struct RenderStats {
        uint32_t drawCallCount = 0;
        uint32_t triangleCount = 0;
        uint32_t visibleMeshCount = 0;
        uint32_t staticMeshCount = 0;
        uint32_t ecsMeshCount = 0;
        uint32_t transparentObjectCount = 0;
        uint32_t materialCount = 0;
        uint32_t textureCount = 0;
        uint32_t lightCount = 0;
        uint32_t shadowCasterCount = 0;
        float cpuFrameTimeMs = 0.0f;
        float gpuFrameTimeMs = 0.0f;
        float gbufferMemoryMB = 0.0f;
        bool renderDocCaptureRequested = false;
        std::vector<RenderPassTiming> passTimings;
    };

} // namespace eng::renderer
