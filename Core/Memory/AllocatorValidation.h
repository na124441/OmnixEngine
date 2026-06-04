#pragma once

namespace eng::memory {

    /**
     * @brief Executes comprehensive stress-tests and validation checks for the Linear, Pool, and Stack allocators.
     * Prints detailed diagnostic reports and logs.
     * @return true if all tests passed successfully, false otherwise.
     */
    bool RunMemoryValidationTests();

} // namespace eng::memory
