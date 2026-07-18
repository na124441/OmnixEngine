#pragma once

#include <cstdint>
#include <string>

namespace eng::renderer {

    // Unique identifier for GPU pipelines and resources
    using PipelineHandle = uint64_t;
    using GpuResourceHandle = uint64_t;

    enum class RendererBackendType : uint8_t {
        Vulkan,
        DirectX12,
        Metal,
        Mock
    };

    struct WindowConfig {
        std::string title = "Omnix Engine";
        uint32_t width = 1920;
        uint32_t height = 1080;
        bool vsync = true;
        bool fullscreen = false;
    };

} // namespace eng::renderer
