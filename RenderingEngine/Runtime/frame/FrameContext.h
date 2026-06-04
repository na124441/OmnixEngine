/*******************************************************************************************************************
 * @file  FrameContext.h
 * @brief Central data container that flows through the entire frame pipeline.
 *
 *        The FrameContext holds references to all major runtime data structures
 *        needed for a single frame's execution. It acts as a "data bus" that
 *        allows different subsystems to access the current frame's state without
 *        tight coupling.
 *
 *        Contents flow through the pipeline:
 *          1. World (gameplay data)
 *          2. RenderScene (CPU render representation)
 *          3. VisibleSet (culled objects)
 *          4. FrameResources (transient allocations)
 *          5. FrameMetrics (timing/stats)
 *          6. RHI CommandList (GPU commands)
 *
 *        © 2024 Your Engine Project – all rights reserved.
 *******************************************************************************************************************/

#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "core/types/Handle.h"

 // Forward declarations to avoid including heavy headers
namespace eng::runtime {
    class World;
    class RenderScene;
    class VisibleSet;
    class FrameResources;
    class FrameMetrics;
    struct Camera;

    namespace framegraph {
        class FrameGraph;
    }
}

namespace eng::rhi {
    class CommandList;
    using CommandListHandle = eng::core::Handle<struct CommandListTag>;
}

namespace eng::renderer {
    class Renderer;
}

namespace eng::runtime {

    /**
     * @struct FrameContext
     *
     * Central data container for a single frame's execution.
     *
     * This structure is passed through the entire rendering pipeline and contains
     * references to all the data needed for that frame. It is designed to be:
     *   - Lightweight (just pointers/references)
     *   - Reusable (reset each frame rather than recreated)
     *   - Thread-safe for reading (all members are read-only during frame execution)
     *
     * The typical flow through a frame:
     *   1. EngineLoop fills in World, FrameNumber, DeltaTime
     *   2. SceneBuilder creates RenderScene from World
     *   3. VisibilitySystem populates VisibleSet from RenderScene
     *   4. FrameScheduler provides FrameResources and FrameMetrics
     *   5. Renderer/Passes use all data to build FrameGraph
     *   6. FrameGraph executor fills in CommandList
     */
    struct FrameContext {
        // ------------------------------------------------------------------------------------------------
        // Frame identification and timing
        // ------------------------------------------------------------------------------------------------

        /// Monotonically increasing frame number (starts at 0)
        uint64_t frameNumber = 0;

        /// Time elapsed since last frame in seconds
        double deltaTime = 0.0;

        /// Absolute time since engine start (seconds)
        double absoluteTime = 0.0;

        // ------------------------------------------------------------------------------------------------
        // Core runtime data (filled by EngineLoop)
        // ------------------------------------------------------------------------------------------------

        /// Current game world state (ECS data)
        World* world = nullptr;

        /// Active camera/view for this frame (can be multiple in advanced setups)
        const Camera* camera = nullptr;

        // ------------------------------------------------------------------------------------------------
        // Render pipeline data (filled by respective systems)
        // ------------------------------------------------------------------------------------------------

        /// CPU-side render representation (flattened from World)
        RenderScene* renderScene = nullptr;

        /// Objects that passed visibility culling tests
        VisibleSet* visibleSet = nullptr;

        /// Transient per-frame resources (allocators, temp buffers, etc.)
        FrameResources* resources = nullptr;

        /// Frame timing and performance metrics
        FrameMetrics* metrics = nullptr;

        // ------------------------------------------------------------------------------------------------
        // RHI integration (filled by FrameGraph executor)
        // ------------------------------------------------------------------------------------------------

        /// Current command list for recording GPU commands
        eng::rhi::CommandList* commandList = nullptr;

        /// Handle to the command list (for RHI operations)
        eng::rhi::CommandListHandle commandListHandle;

        // ------------------------------------------------------------------------------------------------
        // Optional advanced features
        // ------------------------------------------------------------------------------------------------

        /// Frame graph being built/executed this frame
        framegraph::FrameGraph* frameGraph = nullptr;

        /// Renderer instance (for pass callbacks)
        renderer::Renderer* renderer = nullptr;

        /// User-defined frame data (custom engine extensions)
        void* userData = nullptr;

        // ------------------------------------------------------------------------------------------------
        // Utility methods
        // ------------------------------------------------------------------------------------------------

        /**
         * @brief Reset all pointers to nullptr, preparing for next frame.
         * Does NOT delete the pointed-to objects (they're owned elsewhere).
         */
        void Reset() noexcept
        {
            world = nullptr;
            camera = nullptr;
            renderScene = nullptr;
            visibleSet = nullptr;
            resources = nullptr;
            metrics = nullptr;
            commandList = nullptr;
            commandListHandle = eng::rhi::CommandListHandle();
            frameGraph = nullptr;
            renderer = nullptr;
            userData = nullptr;
            // Note: frameNumber and deltaTime are set by FrameScheduler each frame
        }

        /**
         * @brief Check if this context is valid for use.
         * @return true if essential members are populated
         */
        bool IsValid() const noexcept
        {
            return world != nullptr &&
                camera != nullptr &&
                resources != nullptr &&
                metrics != nullptr;
        }

        /**
         * @brief Check if rendering data is available.
         * @return true if renderScene and visibleSet are populated
         */
        bool HasRenderData() const noexcept
        {
            return renderScene != nullptr && visibleSet != nullptr;
        }

        /**
         * @brief Check if GPU command recording is ready.
         * @return true if commandList is available
         */
        bool CanRecordCommands() const noexcept
        {
            return commandList != nullptr;
        }
    };

    // ------------------------------------------------------------------------------------------------
    // Related structures that work with FrameContext
    // ------------------------------------------------------------------------------------------------

    /**
     * @struct Camera
     *
     * Simple camera representation for the frame context.
     * In a real engine this would be more sophisticated.
     */
    struct Camera {
        glm::vec3 position;
        glm::vec3 forward;
        glm::vec3 up;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec4 viewport; // x, y, width, height
        float nearPlane;
        float farPlane;
        bool isOrthographic;

        Camera() : position(0.0f), forward(0.0f, 0.0f, -1.0f), up(0.0f, 1.0f, 0.0f),
            viewMatrix(1.0f), projectionMatrix(1.0f),
            viewport(0.0f, 0.0f, 1920.0f, 1080.0f),
            nearPlane(0.1f), farPlane(1000.0f), isOrthographic(false) {
        }
    };

    /**
     * @struct FrameMetrics
     *
     * Per-frame performance and timing metrics.
     */
    struct FrameMetrics {
        // CPU timing
        double cpuFrameTimeMs = 0.0;
        double gameUpdateTimeMs = 0.0;
        double renderPrepTimeMs = 0.0;
        double gpuSubmitTimeMs = 0.0;

        // GPU timing (filled by RHI after command execution)
        double gpuTotalTimeMs = 0.0;
        double gpuRenderPassTimeMs = 0.0;
        double gpuPresentTimeMs = 0.0;

        // Resource counts
        uint32_t drawCallCount = 0;
        uint32_t triangleCount = 0;
        uint32_t visibleObjectCount = 0;
        uint32_t culledObjectCount = 0;

        // Memory usage
        size_t frameAllocatorUsedBytes = 0;
        size_t ringAllocatorUsedBytes = 0;

        // Debug information
        struct NamedTimer {
            const char* name;
            double ms;
        };
        std::vector<NamedTimer> namedTimers;

        void Reset() noexcept
        {
            cpuFrameTimeMs = 0.0;
            gameUpdateTimeMs = 0.0;
            renderPrepTimeMs = 0.0;
            gpuSubmitTimeMs = 0.0;
            gpuTotalTimeMs = 0.0;
            gpuRenderPassTimeMs = 0.0;
            gpuPresentTimeMs = 0.0;
            drawCallCount = 0;
            triangleCount = 0;
            visibleObjectCount = 0;
            culledObjectCount = 0;
            frameAllocatorUsedBytes = 0;
            ringAllocatorUsedBytes = 0;
            namedTimers.clear();
        }
    };

} // namespace eng::runtime
