#pragma once
#include <cstddef>
#include <cstdint>
#include "Core/Memory/IAllocator.h"

namespace eng::memory {

    /**
     * @class FreeListAllocator
     * @brief A general-purpose coalescing allocator that manages block allocation from a fixed arena (T1.2.4).
     */
    class FreeListAllocator : public IAllocator {
    public:
        enum class AllocationPolicy {
            FindFirst,
            FindBest
        };

        FreeListAllocator(size_t capacity, AllocationPolicy policy = AllocationPolicy::FindFirst);
        FreeListAllocator(void* base, size_t capacity, AllocationPolicy policy = AllocationPolicy::FindFirst);
        ~FreeListAllocator() override;

        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;
        FreeListAllocator(FreeListAllocator&& other) noexcept;
        FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

        void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
        void Free(void* ptr) override;
        void Reset();

        [[nodiscard]] size_t GetCapacity() const noexcept { return m_Capacity; }
        [[nodiscard]] size_t GetUsedMemory() const noexcept { return m_UsedMemory; }
        [[nodiscard]] size_t GetPeakUsage() const noexcept { return m_PeakUsage; }
        [[nodiscard]] void* GetBase() const noexcept { return m_Base; }

    private:
        struct Node {
            size_t size;
            Node* next;
        };

        struct Header {
            size_t size;
            size_t padding;
        };

        uint8_t* m_Base = nullptr;
        size_t m_Capacity = 0;
        size_t m_UsedMemory = 0;
        size_t m_PeakUsage = 0;
        bool m_OwnsMemory = false;
        Node* m_FreeBlocks = nullptr;
        AllocationPolicy m_Policy;
    };

} // namespace eng::memory
