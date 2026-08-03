#pragma once
#include <cstddef>
#include <cstdint>

namespace eng::memory {

    class StackAllocator {
    public:
        using Marker = size_t;

        StackAllocator(size_t capacity);
        StackAllocator(void* base, size_t capacity);
        ~StackAllocator();

        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;
        StackAllocator(StackAllocator&&) noexcept;
        StackAllocator& operator=(StackAllocator&&) noexcept;

        void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
        void FreeToMarker(Marker marker);
        void Reset();

        Marker GetMarker() const { return m_Offset; }
        size_t GetCapacity() const { return m_Capacity; }
        size_t GetOffset() const { return m_Offset; }
        void* GetBase() const { return m_Base; }

    private:
        uint8_t* m_Base = nullptr;
        size_t m_Capacity = 0;
        size_t m_Offset = 0;
        bool m_OwnsMemory = false;
    };

} // namespace eng::memory
