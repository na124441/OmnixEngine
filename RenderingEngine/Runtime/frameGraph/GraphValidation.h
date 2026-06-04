#pragma once
#include "FrameGraph.h"
#include "core/log/Log.h"

namespace eng::runtime {

    /**
     * @class GraphValidator
     *
     * Debug‑only utility that checks the constructed graph for common mistakes:
     *   - Resource is read before it is created.
     *   - Two passes write the same resource without an intervening barrier.
     *   - Cyclic dependencies.
     *
     * In debug builds `FrameGraph::Compile()` automatically calls `Validate()`
     * and aborts if an error is detected.
     */
    class GraphValidator {
    public:
        explicit GraphValidator(const FrameGraph* graph);
        ~GraphValidator() = default;

        Result Validate() const;
    };

} // namespace eng::runtime
