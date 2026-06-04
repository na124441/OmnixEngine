#pragma once
#include <cstddef>

namespace eng::core {

    /**
     * @class IAllocator
     * @brief Abstract base class for all memory allocators in the engine.
     */
    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        /**
         * @brief Allocate raw memory.
         * @param bytes Number of bytes to allocate.
         * @param alignment Alignment in bytes.
         * @return Pointer to the allocated memory, or nullptr on failure.
         */
        virtual void* AllocateBytes(std::size_t bytes, std::size_t alignment) noexcept = 0;

        /**
         * @brief Reset the allocator, effectively freeing all allocations.
         * Note: Not all allocators support this.
         */
        virtual void Reset() noexcept = 0;
    };

} // namespace eng::core
