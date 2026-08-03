#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "Core/memory/Allocator.h"
#include "Core/memory/MemoryTracker.h"

namespace eng::core {

    /**
     * @tparam T       Type of object stored in the pool (must be at least sizeof(void*)).
     * @tparam Capacity Number of objects the pool can hold.
     */
    template <typename T, std::size_t Capacity>
    class PoolAllocator final : public IAllocator {
    public:
        static_assert(sizeof(T) >= sizeof(void*),
            "PoolAllocator requires sizeof(T) >= sizeof(void*) so it can store the free‑list pointer.");

        constexpr PoolAllocator() noexcept
            : m_FreeList(nullptr),
            m_FreeCount(0)
        {
            for (std::size_t i = 0; i < Capacity; ++i) {
                void* slot = reinterpret_cast<void*>(&m_Storage[i * sizeof(T)]);
                *reinterpret_cast<void**>(slot) = m_FreeList;
                m_FreeList = static_cast<T*>(slot);
            }
            m_FreeCount = Capacity;
        }

        void* AllocateBytes(std::size_t bytes,
            std::size_t alignment = alignof(std::max_align_t)) noexcept override
        {
            if (bytes != sizeof(T) || alignment > alignof(T))
                return nullptr;
            return Allocate();
        }

        void Reset() noexcept override
        {
            m_FreeList = nullptr;
            for (std::size_t i = 0; i < Capacity; ++i) {
                void* slot = reinterpret_cast<void*>(&m_Storage[i * sizeof(T)]);
                *reinterpret_cast<void**>(slot) = m_FreeList;
                m_FreeList = static_cast<T*>(slot);
            }
            m_FreeCount = Capacity;
        }

        T* Allocate() noexcept
        {
            if (!m_FreeList) return nullptr;

            T* result = m_FreeList;
            m_FreeList = *reinterpret_cast<T**>(result);
            --m_FreeCount;

            return result;
        }

        void Free(T* obj) noexcept
        {
            if (!obj) return;
            *reinterpret_cast<T**>(obj) = m_FreeList;
            m_FreeList = obj;
            ++m_FreeCount;
        }

        constexpr std::size_t FreeCount() const noexcept { return m_FreeCount; }
        static constexpr std::size_t TotalCapacity() noexcept { return Capacity; }

    private:
        alignas(alignof(T)) std::uint8_t m_Storage[Capacity * sizeof(T)];
        T* m_FreeList;
        std::size_t m_FreeCount;
    };

} // namespace eng::core
