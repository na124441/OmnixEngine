#include "Core/Memory/StackAllocator.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Diagnostics/Assert.h"
#include <utility>

namespace eng::memory {

    StackAllocator::StackAllocator(size_t capacity)
        : m_Capacity(capacity), m_Offset(0), m_OwnsMemory(true) {
        m_Base = new uint8_t[capacity];
        AllocationTracker::RegisterAllocation(m_Base, m_Capacity, "StackAllocator");
    }

    StackAllocator::StackAllocator(void* base, size_t capacity)
        : m_Base(static_cast<uint8_t*>(base)), m_Capacity(capacity), m_Offset(0), m_OwnsMemory(false) {
    }

    StackAllocator::~StackAllocator() {
        if (m_OwnsMemory && m_Base) {
            AllocationTracker::RegisterDeallocation(m_Base);
            delete[] m_Base;
            m_Base = nullptr;
        }
    }

    StackAllocator::StackAllocator(StackAllocator&& other) noexcept
        : m_Base(other.m_Base), m_Capacity(other.m_Capacity), m_Offset(other.m_Offset), m_OwnsMemory(other.m_OwnsMemory) {
        other.m_Base = nullptr;
        other.m_Capacity = 0;
        other.m_Offset = 0;
        other.m_OwnsMemory = false;
    }

    StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept {
        if (this != &other) {
            if (m_OwnsMemory && m_Base) {
                AllocationTracker::RegisterDeallocation(m_Base);
                delete[] m_Base;
            }
            m_Base = other.m_Base;
            m_Capacity = other.m_Capacity;
            m_Offset = other.m_Offset;
            m_OwnsMemory = other.m_OwnsMemory;

            other.m_Base = nullptr;
            other.m_Capacity = 0;
            other.m_Offset = 0;
            other.m_OwnsMemory = false;
        }
        return *this;
    }

    void* StackAllocator::Allocate(size_t size, size_t alignment) {
        if (m_Capacity == 0 || !m_Base) return nullptr;

        size_t currentAddress = reinterpret_cast<size_t>(m_Base) + m_Offset;
        size_t padding = 0;
        if (alignment > 0 && (currentAddress % alignment) != 0) {
            padding = alignment - (currentAddress % alignment);
        }

        if (m_Offset + padding + size > m_Capacity) {
            return nullptr; // Out of memory
        }

        m_Offset += padding;
        void* ptr = m_Base + m_Offset;
        m_Offset += size;
        return ptr;
    }

    void StackAllocator::FreeToMarker(Marker marker) {
        OMNIX_ASSERT(marker <= m_Offset,
                     "StackAllocator::FreeToMarker: Attempted to free to an invalid marker (forward in time)!");
        m_Offset = marker;
    }

    void StackAllocator::Reset() {
        m_Offset = 0;
    }

} // namespace eng::memory
