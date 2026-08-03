#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include "RenderingEngine/Runtime/frameGraph/PassNode.h"
#include "RenderingEngine/Runtime/frameGraph/ResourceNode.h"

namespace eng::runtime {

    /**
     * @class FrameGraph
     *
     * Public façade used by the high‑level `Renderer`.
     *
     * Workflow:
     *   1. Renderer creates resources via `CreateTexture`, `CreateBuffer`, etc.
     *   2. Renderer creates passes via `AddPass` (using `PassBuilder`).
     *   3. After all passes have been added, the client calls `Compile()`.
     *      This step:
     *        - Topologically sorts passes based on read/write dependencies.
     *        - Computes lifetimes (`firstPass`, `lastPass`) for each resource.
     *        - Detects and records **resource aliasing** opportunities for transients.
     *        - Inserts implicit **barriers** (image layout transitions, memory barriers).
     *   4. `Execute(ctx)` runs the compiled pass list, records each pass’s lambda
     *      into a fresh `RHI::CommandList`, and finally submits the command list.
     */
    class FrameGraph {
    public:
        explicit FrameGraph(RHI::Device* rhi);
        ~FrameGraph();

        // Resource creation API – returns a handle that passes can reference.
        ResourceNode& CreateTexture(const std::string& name, const RHI::TextureDesc& desc,
            bool transient = true);
        ResourceNode& CreateBuffer(const std::string& name, const RHI::BufferDesc& desc,
            bool transient = true);

        // Pass creation – caller uses PassBuilder.
        PassNode& AddPass(std::unique_ptr<PassNode> pass);

        // Compile the graph – must be called after all passes & resources are added.
        Result Compile();

        // Execute – records and submits the graph.
        Result Execute(const FrameContext& ctx);

    private:
        RHI::Device* m_RHI;
        std::unordered_map<std::string, std::unique_ptr<ResourceNode>> m_Resources;
        std::vector<std::unique_ptr<PassNode>>                         m_Passes;

        // Derived data after compilation
        std::vector<PassNode*>            m_OrderedPasses;
        std::vector<ResourceNode*>          m_OrderedResources;

        // Helpers
        Result InsertBarriers();
        Result TopologicalSort();
    };

} // namespace eng::runtime
