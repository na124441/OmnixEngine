#pragma once
#include <cstdint>
#include <limits>

namespace eng::core {

    /**
     * @brief Opaque, type‑safe handle used to refer to a resource stored in a manager.
     *
     * The template parameter `Tag` is an empty struct that uniquely identifies the
     * resource type (e.g., `struct MeshTag {};`).  Different tags produce
     * completely unrelated handle types, so the compiler will reject accidental
     * mixing.
     *
     * Internally the handle stores two 32‑bit values:
     *   - `index`      – position of the resource inside the manager’s dense array.
     *   - `generation` – a per‑slot counter that increments each time the slot is
     *                     recycled.  Stale handles (pointing to a slot that has
     *                     been reused) fail `IsValid()`.
     *
     * The type is trivially copyable, POD and fits in a single 64‑bit register.
     * No heap allocation, no virtual functions, no RTTI.
     */
    template <typename Tag>
    class Handle {
    public:
        using IndexType = uint32_t;
        using GenType = uint32_t;

        // --------------------------------------------------------------------
        // Constructors & constants
        // --------------------------------------------------------------------
        constexpr Handle() noexcept
            : m_Index(InvalidIndex), m_Generation(InvalidGeneration) {
        }

        constexpr Handle(IndexType idx, GenType gen) noexcept
            : m_Index(idx), m_Generation(gen) {
        }

        // --------------------------------------------------------------------
        // Queries
        // --------------------------------------------------------------------
        /** @return true if the handle encodes a slot that is currently alive. */
        constexpr bool IsValid() const noexcept {
            return m_Index != InvalidIndex && m_Generation != InvalidGeneration;
        }

        constexpr IndexType   Index()      const noexcept { return m_Index; }
        constexpr GenType     Generation() const noexcept { return m_Generation; }

        // --------------------------------------------------------------------
        // Comparison (both fields must match)
        // --------------------------------------------------------------------
        constexpr bool operator==(const Handle& rhs) const noexcept {
            return m_Index == rhs.m_Index && m_Generation == rhs.m_Generation;
        }
        constexpr bool operator!=(const Handle& rhs) const noexcept {
            return !(*this == rhs);
        }

    private:
        // --------------------------------------------------------------------
        // Sentinel values – chosen so that a default‑constructed handle is
        // unmistakably invalid and never collides with a real slot.
        // --------------------------------------------------------------------
        static constexpr IndexType InvalidIndex = std::numeric_limits<IndexType>::max(); // 0xFFFFFFFFu
        static constexpr GenType   InvalidGeneration = std::numeric_limits<GenType>::max();   // 0xFFFFFFFFu

        IndexType m_Index;
        GenType   m_Generation;
    };

} // namespace eng::core
