#pragma once
#include <cstdint>
#include <limits>

namespace eng::core {

    /**
     * @brief Simple, type‑safe monotonic identifier.
     *
     * The template parameter `Tag` is an empty struct used only for type safety.
     * Two different tags produce two distinct `ID` types that the compiler
     * will not allow to be mixed.
     *
     * Example tags:
     *   struct EntityTag {};
     *   struct MaterialTag {};
     *
     * Internally the ID stores a single 32‑bit value. The value `Invalid`
     * (0xFFFFFFFF) is reserved as a sentinel for “null/invalid”.
     *
     * IDs are **never recycled** during a program run; the engine simply
     * increments a global counter each time an ID is requested.
     *
     * Because the type is trivially copyable and fits in a register,
     * it can be passed by value with zero overhead.
     */
    template <typename Tag>
    class ID {
    public:
        using Value = uint32_t;

        // --------------------------------------------------------------------
        // Default construction – creates an invalid ID.
        // --------------------------------------------------------------------
        constexpr ID() noexcept : m_Value(Invalid) {}

        // --------------------------------------------------------------------
        // Explicit construction from a raw value (usually done by a manager).
        // --------------------------------------------------------------------
        explicit constexpr ID(Value v) noexcept : m_Value(v) {}

        // --------------------------------------------------------------------
        // Query helpers.
        // --------------------------------------------------------------------
        constexpr bool IsValid() const noexcept { return m_Value != Invalid; }
        constexpr Value Get()   const noexcept { return m_Value; }

        // --------------------------------------------------------------------
        // Equality / inequality operators.
        // --------------------------------------------------------------------
        constexpr bool operator==(ID rhs) const noexcept { return m_Value == rhs.m_Value; }
        constexpr bool operator!=(ID rhs) const noexcept { return !(*this == rhs); }

        // --------------------------------------------------------------------
        // Factory: generate a fresh ID from a monotonically increasing counter.
        // --------------------------------------------------------------------
        static ID Generate(Value next) noexcept { return ID(next); }

    private:
        static constexpr Value Invalid = std::numeric_limits<Value>::max(); // 0xFFFFFFFFu
        Value m_Value;
    };

} // namespace eng::core
