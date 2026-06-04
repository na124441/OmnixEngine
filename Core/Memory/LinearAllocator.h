#pragma once
#include <cstddef>
#include <cstdint>

namespace eng::memory {

    class LinearAllocator {
    public:
        LinearAllocator(size_t capacity);
        LinearAllocator(void* base, size_t capacity);
        ~LinearAllocator();

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;
        LinearAllocator(LinearAllocator&&) noexcept;
        LinearAllocator& operator=(LinearAllocator&&) noexcept;

        void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
        void Reset();

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
