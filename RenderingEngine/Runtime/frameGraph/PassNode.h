#pragma once
#include <functional>
#include <string>
#include <vector>
#include "ResourceNode.h"
#include "rhi/RHI.h"

namespace eng::runtime {

    /**
     * @class PassNode
     *
     * Represents a single *render pass* in the graph.
     *
     * The user supplies a **lambda** (type `PassExecuteFn`) that receives the
     * `FrameContext` and a `RHI::CommandList*`.  The lambda records all commands
     * for that pass – it may also allocate transient resources via the
     * `FrameResources` that are stored in the context.
     *
     * The PassNode stores the **list of resource names** it reads and writes.
     * The compiler uses these lists to build a dependency graph.
     */
    using PassExecuteFn = std::function<void(const FrameContext&, RHI::CommandList*)>;

    class PassNode {
    public:
        PassNode(const std::string& name,
            const std::vector<std::string>& reads,
            const std::vector<std::string>& writes,
            PassExecuteFn exec);
        ~PassNode() = default;

        const std::string& GetName() const noexcept { return m_Name; }
        const std::vector<std::string>& Reads() const noexcept { return m_Reads; }
        const std::vector<std::string>& Writes() const noexcept { return m_Writes; }
        const PassExecuteFn& GetExec() const noexcept { return m_Exec; }

        // Filled by GraphCompiler – execution order index.
        void SetOrder(uint32_t idx) noexcept { m_Order = idx; }
        uint32_t Order() const noexcept { return m_Order; }

    private:
        std::string               m_Name;
        std::vector<std::string>  m_Reads;
        std::vector<std::string>  m_Writes;
        PassExecuteFn            m_Exec;
        uint32_t                 m_Order{ UINT32_MAX };
    };

} // namespace eng::runtime
