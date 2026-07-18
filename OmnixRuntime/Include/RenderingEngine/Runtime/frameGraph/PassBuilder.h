#pragma once
#include <string>
#include <vector>
#include "RenderingEngine/Runtime/frameGraph/PassNode.h"
#include "RenderingEngine/Runtime/frameGraph/ResourceNode.h"

namespace eng::runtime {

    /**
     * @class PassBuilder
     *
     * Helper used by the high‑level `Renderer` to declare a pass.
     *
     * Example usage in a DeferredRenderer:
     *
     * ```cpp
     * PassBuilder pb;
     * pb.Name("GBufferPass")
     *   .Read("Depth")
     *   .Write("GAlbedo")
     *   .Write("GNormal")
     *   .Write("GMaterial")
     *   .Execute([this](const FrameContext& ctx, RHI::CommandList* cmd) {
     *       // Record draw calls using ctx.renderScene, ctx.visibleSet, etc.
     *   });
     * frameGraph.AddPass(pb.Build());
     * ```
     *
     * The builder stores the name, read/write lists, and the lambda,
     * then constructs a `PassNode` when `Build()` is called.
     */
    class PassBuilder {
    public:
        PassBuilder() = default;

        PassBuilder& Name(const std::string& n) { m_Name = n; return *this; }
        PassBuilder& Read(const std::string& r) { m_Reads.push_back(r); return *this; }
        PassBuilder& Write(const std::string& w) { m_Writes.push_back(w); return *this; }
        PassBuilder& Execute(PassExecuteFn fn) { m_Exec = std::move(fn); return *this; }

        std::unique_ptr<PassNode> Build() const {
            return std::make_unique<PassNode>(m_Name, m_Reads, m_Writes, m_Exec);
        }

    private:
        std::string      m_Name;
        std::vector<std::string> m_Reads;
        std::vector<std::string> m_Writes;
        PassExecuteFn    m_Exec;
    };

} // namespace eng::runtime
