#include "Core/Memory/PoolAllocator.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Diagnostics/Assert.h"
#include <utility>

namespace eng::memory {

    PoolAllocator::PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment)
        : m_ObjectSize(objectSize), m_ObjectCount(objectCount), m_Alignment(alignment) {
        
        size_t minSize = sizeof(FreeNode);
        size_t actualSize = objectSize > minSize ? objectSize : minSize;

        m_BlockSize = actualSize;
        if (alignment > 0 && (m_BlockSize % alignment) != 0) {
            m_BlockSize += alignment - (m_BlockSize % alignment);
        }

        size_t totalBytes = m_BlockSize * m_ObjectCount;
        m_Base = new uint8_t[totalBytes];
        AllocationTracker::RegisterAllocation(m_Base, totalBytes, "PoolAllocator");

        Reset();
    }

    PoolAllocator::~PoolAllocator() {
        if (m_Base) {
            size_t totalBytes = m_BlockSize * m_ObjectCount;
            AllocationTracker::RegisterDeallocation(m_Base);
            delete[] m_Base;
            m_Base = nullptr;
        }
    }

    PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
        : m_Base(other.m_Base),
          m_ObjectSize(other.m_ObjectSize),
          m_ObjectCount(other.m_ObjectCount),
          m_Alignment(other.m_Alignment),
          m_BlockSize(other.m_BlockSize),
          m_FreeList(other.m_FreeList),
          m_FreeCount(other.m_FreeCount) {
        other.m_Base = nullptr;
        other.m_ObjectSize = 0;
        other.m_ObjectCount = 0;
        other.m_Alignment = 0;
        other.m_BlockSize = 0;
        other.m_FreeList = nullptr;
        other.m_FreeCount = 0;
    }

    PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            if (m_Base) {
                size_t totalBytes = m_BlockSize * m_ObjectCount;
                AllocationTracker::RegisterDeallocation(m_Base);
                delete[] m_Base;
            }
            m_Base = other.m_Base;
            m_ObjectSize = other.m_ObjectSize;
            m_ObjectCount = other.m_ObjectCount;
            m_Alignment = other.m_Alignment;
            m_BlockSize = other.m_BlockSize;
            m_FreeList = other.m_FreeList;
            m_FreeCount = other.m_FreeCount;

            other.m_Base = nullptr;
            other.m_ObjectSize = 0;
            other.m_ObjectCount = 0;
            other.m_Alignment = 0;
            other.m_BlockSize = 0;
            other.m_FreeList = nullptr;
            other.m_FreeCount = 0;
        }
        return *this;
    }

    void* PoolAllocator::Allocate() {
        if (!m_FreeList) {
            return nullptr; // Pool exhausted
        }
        FreeNode* node = m_FreeList;
        m_FreeList = m_FreeList->next;
        m_FreeCount--;
        return static_cast<void*>(node);
    }

    void PoolAllocator::Free(void* ptr) {
        if (!ptr) return;

        size_t totalBytes = m_BlockSize * m_ObjectCount;
        uint8_t* bytePtr = static_cast<uint8_t*>(ptr);

        // Safety Boundary Check
        OMNIX_ASSERT(bytePtr >= m_Base && bytePtr < (m_Base + totalBytes),
                     "PoolAllocator::Free: Pointer is out of pool boundaries!");

        // Safety Alignment Check
        size_t offset = bytePtr - m_Base;
        OMNIX_ASSERT((offset % m_BlockSize) == 0,
                     "PoolAllocator::Free: Pointer is not aligned to slot boundary!");

        FreeNode* node = static_cast<FreeNode*>(ptr);
        node->next = m_FreeList;
        m_FreeList = node;
        m_FreeCount++;
    }

    void PoolAllocator::Reset() {
        m_FreeList = nullptr;
        m_FreeCount = m_ObjectCount;
        for (size_t i = 0; i < m_ObjectCount; ++i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(m_Base + (i * m_BlockSize));
            node->next = m_FreeList;
            m_FreeList = node;
        }
    }

} // namespace eng::memory
