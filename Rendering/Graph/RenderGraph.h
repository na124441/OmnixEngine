#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan.h>
#include "RenderResource.h"
#include "RenderPass.h"
#include "Rendering/Core/RenderStats.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;
        RenderGraph(RenderGraph&&) noexcept = default;
        RenderGraph& operator=(RenderGraph&&) noexcept = default;

        void RegisterPass(
            const std::string& name,
            const std::vector<std::string>& inputs,
            const std::vector<std::string>& outputs,
            PassID physicalSlot,
            std::function<void(VkCommandBuffer)> execute,
            bool requiresFramebuffer = false,
            VkFramebuffer framebuffer = VK_NULL_HANDLE,
            bool requiresPipeline = false,
            VkPipeline pipeline = VK_NULL_HANDLE,
            const std::vector<RenderTargetHandle>& inputHandles = {},
            const std::vector<RenderTargetHandle>& outputHandles = {},
            std::function<PassResult(VkCommandBuffer)> executeWithResult = nullptr,
            const std::vector<RenderResourceUsage>& resourceUsages = {}
        );

        void DeclareTexture(const std::string& name, const TextureResourceDesc& desc, bool transient = true);
        void DeclareBuffer(const std::string& name, const BufferResourceDesc& desc, bool transient = true);

        void Compile(EngineResources& resources);
        void Execute(EngineResources& resources, uint32_t frameIndex);
        bool ExecuteWithValidation(EngineResources& resources, uint32_t frameIndex, RenderTargetManager& targetManager, bool& outGraphExecutionFailed);
        bool ValidatePass(const RenderPass& pass, const RenderTargetManager& targetManager);
        void CollectGpuTimings(EngineResources& resources, uint32_t frameIndex);
        const std::vector<RenderPassTiming>& GetLastPassTimings() const { return m_LastPassTimings; }
        float GetLastGpuFrameTimeMs() const { return m_LastGpuFrameTimeMs; }
        const std::vector<bool>& GetLastRecordedSlots() const { return m_LastRecordedSlots; }

        void PrintDebug() const;
        void Clear();
        void Shutdown(EngineResources& resources);

        size_t GetPassCount() const { return m_CompiledPasses.size(); }

        void LogDeviceLostReport(uint32_t frameIndex) const;
        const std::string& GetActivePassName() const { return m_ActivePassName; }
        VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }
        const std::string& GetLastSuccessfulPassName() const { return m_LastSuccessfulPassName; }
        const std::string& GetLastResourceTransitionLog() const { return m_LastResourceTransitionLog; }
        const std::string& GetLastRenderPassStartedLog() const { return m_LastRenderPassStartedLog; }

    private:
        bool TopologicalSort();
        void CalculateResourceLifetimes();
        void EnsureTimestampQueries(EngineResources& resources);

        std::vector<RenderPass> m_RegisteredPasses;
        std::unordered_map<std::string, RenderResource> m_Resources;

        // Crash reporting tracking variables (Phase 5 & 6)
        std::string m_ActivePassName = "None";
        VkCommandBuffer m_ActiveCommandBuffer = VK_NULL_HANDLE;
        std::string m_LastSuccessfulPassName = "None";
        std::string m_LastResourceTransitionLog = "None";
        std::string m_LastRenderPassStartedLog = "None";

        // Compiled execution order
        std::vector<RenderPass> m_CompiledPasses;
        std::vector<std::string> m_LastTimingNames;
        std::vector<RenderPassTiming> m_LastPassTimings;
        std::vector<bool> m_LastRecordedSlots;
        VkQueryPool m_TimestampQueryPool = VK_NULL_HANDLE;
        uint32_t m_TimestampQueryCapacity = 0;
        float m_TimestampPeriodNs = 1.0f;
        float m_LastGpuFrameTimeMs = 0.0f;
    };

} // namespace eng::renderer
