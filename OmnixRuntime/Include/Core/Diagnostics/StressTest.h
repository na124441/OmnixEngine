#pragma once

namespace eng::diagnostics {

    /**
     * @brief Performs unified runtime integration stress tests including 100K ECS entity updates,
     * 10K scheduler task loops, binary serialization validation, and memory leak checks.
     * @return true if all stress tests pass, false otherwise.
     */
    bool RunRuntimeStressTests();

} // namespace eng::diagnostics
