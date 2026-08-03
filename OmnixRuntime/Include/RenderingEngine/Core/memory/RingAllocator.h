#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "Core/memory/Allocator.h"      // IAllocator base class
#include "Core/memory/MemoryTracker.h"  // optional tracking macros

namespace eng::core {

    /**
     * @tparam Tag  (optional) a dummy type used only for type‑distinction.
     *
     * The allocator works on a *pre‑allocated* raw memory block.
     * It is suited for per‑frame or per‑pass data that must survive a few GPU frames
     * (e.g., dynamic uniform buffers). The generation counter guarantees that a
     * pointer from an old generation is never mistakenly reused without the user
     * noticing the mismatch.
     */
    template <typename Tag = void>
    class RingAllocator final : public IAllocator {
    public:
        using Byte = std::uint8_t;
        static constexpr std::size_t DefaultAlignment = alignof(std::max_align_t);
        using Generation = std::uint32_t;

        // -------------------------------------------------------------------------
        // Construction / destruction
        // -------------------------------------------------------------------------
        constexpr RingAllocator(void* base, std::size_t capacity) noexcept
            : m_Base(reinterpret_cast<Byte*>(base)),
            m_Capacity(capacity),
            m_Offset(0),
            m_Generation(0) {
        }

        // The allocator is trivially copyable – defaulted copy/move work fine.
        constexpr RingAllocator(const RingAllocator&) = default;
        constexpr RingAllocator(RingAllocator&&) = default;
        constexpr RingAllocator& operator=(const RingAllocator&) = default;
        constexpr RingAllocator& operator=(RingAllocator&&) = default;
        ~RingAllocator() = default;

        // -------------------------------------------------------------------------
        // IAllocator implementation
        // -------------------------------------------------------------------------
        void* AllocateBytes(std::size_t bytes,
            std::size_t alignment = DefaultAlignment) noexcept override
        {
            const std::size_t alignedOffset = AlignUp(m_Offset, alignment);
            const std::size_t newOffset = alignedOffset + bytes;

            if (newOffset > m_Capacity) {
                return nullptr;
            }

            m_Offset = newOffset;
            void* ptr = static_cast<void*>(m_Base + alignedOffset);
            return ptr;
        }

        void Reset() noexcept override { m_Offset = 0; }

        // -------------------------------------------------------------------------
        // Generation handling
        // -------------------------------------------------------------------------
        void AdvanceGeneration() noexcept
        {
            ++m_Generation;
            m_Offset = 0;
        }

        /** @return The current generation counter. */
        constexpr Generation CurrentGeneration() const noexcept { return m_Generation; }

        template <typename T>
        T* Allocate(std::size_t count = 1) noexcept
        {
            static_assert(std::is_trivially_destructible<T>::value,
                "RingAllocator should only allocate POD/trivial types.");
            const std::size_t bytes = sizeof(T) * count;
            void* raw = AllocateBytes(bytes, alignof(T));
            return static_cast<T*>(raw);
        }

        constexpr std::size_t Used()       const noexcept { return m_Offset; }
        constexpr std::size_t Capacity()   const noexcept { return m_Capacity; }
        constexpr const void* Base()    const noexcept { return m_Base; }

    private:
        static constexpr std::size_t AlignUp(std::size_t value,
            std::size_t alignment) noexcept
        {
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        Byte* m_Base;
        std::size_t    m_Capacity;
        std::size_t    m_Offset;
        Generation     m_Generation;
    };

} // namespace eng::core
