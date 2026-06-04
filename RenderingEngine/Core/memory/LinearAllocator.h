/**************************************************************************************************
 * @file  LinearAllocator.h
 * @brief Bump‑allocator used for short‑lived, per‑frame data (e.g. RenderScene, VisibleSet, etc.).
 *
 *        The allocator does **not** perform any heap allocation of its own – it operates on a raw
 *        memory block supplied by the caller.  Every allocation returns a pointer that is
 *        correctly aligned for the requested type.  All allocations are freed at once by calling
 *        `Reset()`.
 *
 *        The class is trivially copy‑able, fits in a register and can be passed by value.
 *
 *        Namespace: eng::core
 *
 *        © 2024  Your Engine Project – all rights reserved.
 **************************************************************************************************/

#pragma once

#include <cstddef>      // std::size_t, std::max_align_t
#include <cstdint>      // std::uint8_t
#include <type_traits> // std::is_trivially_destructible
#include <limits>       // std::numeric_limits
#include <exception>
#include "Core/Log/log.h"

 // -----------------------------------------------------------------------------
 // Optional memory‑tracking – compiled in only when ENG_ENABLE_MEMORY_TRACKING is set
 // -----------------------------------------------------------------------------
#if defined(ENG_ENABLE_MEMORY_TRACKING)
#include "MemoryTracker.h"        // <-- your existing tracker header
#define ENG_TRACK_ALLOC(ptr, size, tag) \
        ::eng::core::MemoryTracker::OnAllocate(ptr, size, tag)
#else
#define ENG_TRACK_ALLOC(ptr, size, tag) ((void)0)
#endif

namespace eng::core {

    /**
     * @brief Simple linear (bump) allocator.
     *
     * The allocator owns **no memory** – it merely manages a pointer/offset into a block that
     * the user supplied at construction time.  It is therefore extremely cheap:
     *
     *   * Allocation → pointer arithmetic + optional alignment.
     *   * Reset      → `offset = 0`.
     *
     * Typical usage pattern (per‑frame):
     *
     * ```cpp
     * // 1️⃣ Allocate a raw buffer somewhere (once at program start)
     * constexpr std::size_t FrameArenaSize = 8 * 1024 * 1024; // 8 MiB
     * alignas(std::max_align_t) std::uint8_t frameArena[FrameArenaSize];
     *
     * // 2️⃣ Construct allocator at the beginning of each frame
     * LinearAllocator linAlloc(frameArena, FrameArenaSize);
     *
     * // 3️⃣ Allocate whatever you need for this frame
     * auto* objects = linAlloc.Allocate<RenderObject>(maxObjects);
     *
     * // 4️⃣ When the frame ends
     * linAlloc.Reset();   // all memory is instantly reclaimed
     * ```
     *
     * @tparam Tag  (optional) a dummy type used only for compile‑time distinction.
     *              If you need two independent linear allocators that must not be
     *              interchanged accidentally, give them different Tag types.
     */
    template <typename Tag = void>
    class LinearAllocator {
    public:
        // -------------------------------------------------------------------------
        // Types & constants
        // -------------------------------------------------------------------------
        using Byte = std::uint8_t;

        /** Max alignment guaranteed for any allocation that does not specify a custom alignment. */
        static constexpr std::size_t DefaultAlignment = alignof(std::max_align_t);

        // -------------------------------------------------------------------------
        // Construction / destruction
        // -------------------------------------------------------------------------
        /**
         * @param base       Pointer to the beginning of a pre‑allocated memory block.
         * @param capacity   Size of that block in bytes.
         *
         * @note The constructor does **not** own the memory. The caller must guarantee that
         *       `base` remains valid for the whole lifetime of the allocator (or until a
         *       `Reset` that reuses the same block).
         */
        constexpr LinearAllocator(void* base, std::size_t capacity) noexcept
            : m_Base(reinterpret_cast<Byte*>(base)),
            m_Capacity(capacity),
            m_Offset(0) {
        }

        // The allocator is trivially copyable – defaulted constructors are fine.
        constexpr LinearAllocator(const LinearAllocator&) = default;
        constexpr LinearAllocator(LinearAllocator&&) = default;
        constexpr LinearAllocator& operator=(const LinearAllocator&) = default;
        constexpr LinearAllocator& operator=(LinearAllocator&&) = default;
        ~LinearAllocator() = default;

        // -------------------------------------------------------------------------
        // Public API
        // -------------------------------------------------------------------------

        /** Reset the allocator – all previously allocated memory becomes invalid. */
        constexpr void Reset() noexcept { m_Offset = 0; }

        /**
         * @brief Allocate a raw byte range.
         *
         * @param bytes      Number of bytes required.
         * @param alignment Desired alignment (must be a power of two). Default is
         *                  `alignof(std::max_align_t)`.
         *
         * @return Pointer to the start of the allocation, or `nullptr` if there is
         *         insufficient space.
         *
         * @warning The returned memory is **uninitialized**.  The caller must
         *          construct objects manually (placement‑new) or use `Allocate<T>()`.
         */
        constexpr void* AllocateBytes(std::size_t bytes,
            std::size_t alignment = DefaultAlignment) noexcept
        {
            // Align current offset up to the requested alignment.
            std::size_t alignedOffset = AlignUp(m_Offset, alignment);
            std::size_t newOffset = alignedOffset + bytes;

            if (newOffset > m_Capacity) {
                // Allocation would overflow → fail.
                return nullptr;
            }

            // Record the new offset before returning the pointer (so that a
            // subsequent allocation sees the updated state even if the caller
            // forgets to store the result).
            m_Offset = newOffset;

            void* ptr = static_cast<void*>(m_Base + alignedOffset);
            ENG_TRACK_ALLOC(ptr, bytes, "LinearAllocator");
            return ptr;
        }

        /**
         * @brief Allocate space for `count` objects of type `T`.
         *
         * The returned pointer is suitably aligned for `T`.  The memory is **uninitialized**;
         * you must construct the objects yourself (e.g. `new (ptr) T(args…)`) or use
         * `emplace_back` on a container that takes the pointer.
         *
         * @tparam T        Type to allocate.
         * @param count     Number of objects (default = 1).
         * @return Pointer to the first object, or `nullptr` on failure.
         */
        template <typename T>
        constexpr T* Allocate(std::size_t count = 1) noexcept
        {
            static_assert(std::is_trivially_destructible<T>::value,
                "LinearAllocator can only allocate trivially destructible types. "
                "If you need destruction, allocate the memory elsewhere and "
                "manage the lifetime manually.");

            const std::size_t bytes = sizeof(T) * count;
            void* raw = AllocateBytes(bytes, alignof(T));
            return static_cast<T*>(raw);
        }

        /** @return Number of bytes already allocated (since the last Reset). */
        constexpr std::size_t Used() const noexcept { return m_Offset; }

        /** @return Total capacity of the underlying buffer (bytes). */
        constexpr std::size_t Capacity() const noexcept { return m_Capacity; }

        /** @return Pointer to the start of the managed block (read‑only). */
        constexpr const void* Base() const noexcept { return m_Base; }

    private:
        // -------------------------------------------------------------------------
        // Helpers
        // -------------------------------------------------------------------------
        /**
         * @brief Align `value` up to the next multiple of `alignment`.
         *
         * `alignment` must be a power of two; the function uses the classic bit‑twiddle:
         *
         *   aligned = (value + (alignment-1)) & ~(alignment-1)
         *
         * This works for both signed and unsigned integer types.
         */
        static constexpr std::size_t AlignUp(std::size_t value,
            std::size_t alignment) noexcept
        {
            // Alignment of zero would be undefined – guard against it in debug builds.
            // In Release we rely on the caller to pass a sane value.
#if defined(NDEBUG)
            (void)alignment; // silence unused‑parameter warning if you compile with -Wall
#else
            if (alignment == 0) {
                // In a debug build we deliberately crash the program – it signals a
                // serious programming error.
                ENG_LOG_ERROR("LinearAllocator::AlignUp called with alignment == 0");
                std::terminate();
            }
#endif
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        // -------------------------------------------------------------------------
        // Data members (all trivially copyable)
        // -------------------------------------------------------------------------
        Byte* m_Base;      ///< Start of the memory block supplied by the user.
        std::size_t  m_Capacity; ///< Total size of the block (bytes).
        std::size_t  m_Offset;   ///< Current bump offset (bytes from m_Base).
    };

} // namespace eng::core
