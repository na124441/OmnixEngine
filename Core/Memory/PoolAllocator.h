#pragma once
#include <cstddef>
#include <cstdint>

namespace eng::memory {

    class PoolAllocator {
    public:
        PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment = alignof(std::max_align_t));
        ~PoolAllocator();

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;
        PoolAllocator(PoolAllocator&&) noexcept;
        PoolAllocator& operator=(PoolAllocator&&) noexcept;

        void* Allocate();
        void Free(void* ptr);
        void Reset();

        size_t GetObjectSize() const { return m_ObjectSize; }
        size_t GetCapacity() const { return m_ObjectCount; }
        size_t GetFreeCount() const { return m_FreeCount; }

    private:
        struct FreeNode {
            FreeNode* next;
        };

        uint8_t* m_Base = nullptr;
        size_t m_ObjectSize = 0;
        size_t m_ObjectCount = 0;
        size_t m_Alignment = 0;
        size_t m_BlockSize = 0;
        FreeNode* m_FreeList = nullptr;
        size_t m_FreeCount = 0;
    };

} // namespace eng::memory
