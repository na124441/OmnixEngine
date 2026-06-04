#pragma once
#include <vector>
#include "Pass.h"
#include "Core/Engine/Log.h"

namespace eng::renderer {

/// Very small graph that just stores an ordered list of Passes
/// and executes them sequentially each frame.
class RenderGraph
{
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    /// Add a new pass at the end of the list.
    void addPass(Pass&& p)
    {
        LOG_INFO("RenderGraph: Adding pass \"" + p.name + "\"");
        passes.emplace_back(std::move(p));
    }

    /// Clear the graph (useful when rebuilding after a swap‑chain recreation).
    void clear()
    {
        LOG_INFO("RenderGraph: Clearing all passes");
        passes.clear();
    }

    /// Execute all passes in order.  Typically called once per frame
    /// after the swap‑chain image has been acquired.
    void execute()
    {
        for (size_t i = 0; i < passes.size(); ++i)
        {
            const auto& p = passes[i];
            LOG_DEBUG("RenderGraph: Executing pass [" + std::to_string(i) + "] " + p.name);
            p.execute();      // the callable does its own command‑buffer work
        }
    }

    /// Query the number of passes (for debugging / UI)
    size_t size() const { return passes.size(); }

private:
    std::vector<Pass> passes;
};

} // namespace eng::renderer
