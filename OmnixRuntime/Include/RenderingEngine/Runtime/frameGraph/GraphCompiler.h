#pragma once
#include "RenderingEngine/Runtime/frameGraph/FrameGraph.h"
#include "core/log/Log.h"

namespace eng::runtime {

    /**
     * @class GraphCompiler
     *
     * Separate class that encapsulates the *hard* work of compilation.
     * `FrameGraph::Compile()` simply forwards to an instance of this class.
     *
     * The compiler does *not* modify any RHI objects – it only computes
     * ordering data that will later be used by `GraphExecutor`.
     */
    class GraphCompiler {
    public:
        explicit GraphCompiler(FrameGraph* graph);
        ~GraphCompiler() = default;

        Result Compile();

    private:
        FrameGraph* m_Graph;

        // Internal helpers
        Result ComputePassDependencies();
        Result ComputeResourceLifetimes();
        Result ResolveAliasing();
        Result InsertImplicitBarriers();
    };

} // namespace eng::runtime
