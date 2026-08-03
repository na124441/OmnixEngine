#pragma once
#include "Core/Diagnostics/Assert.h"
#include <vector>
#include <mutex>

namespace eng::memory {

    /**
     * @struct Handle
     * @brief Represents a resource identifier consisting of an index and a generation identifier (T1.2.9).
     */
    template<typename T>
    struct Handle {
        uint32_t index = 0;
        uint32_t generation = 0;

        [[nodiscard]] bool IsValid() const noexcept { return index != 0; }
        
        bool operator==(const Handle& other) const noexcept {
            return index == other.index && generation == other.generation;
        }
        
        bool operator!=(const Handle& other) const noexcept {
            return !(*this == other);
        }
    };

    /**
     * @class HandleManager
     * @brief Allocates and validates generation-tagged resource handles (T1.2.9).
     */
    template<typename T>
    class HandleManager {
    public:
        explicit HandleManager(size_t maxResources = 1024) {
            m_Slots.resize(maxResources);
            for (uint32_t i = 0; i < maxResources; ++i) {
                m_Slots[i].generation = 1;
                m_FreeSlots.push_back(i);
            }
        }

        /**
         * @brief Register a raw resource pointer. Returns a handle.
         */
        Handle<T> RegisterResource(T* ptr) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_FreeSlots.empty()) {
                OMNIX_FATAL_ASSERT(false, "HandleManager: Resource slot limit reached!");
                return {};
            }

            uint32_t index = m_FreeSlots.back();
            m_FreeSlots.pop_back();

            m_Slots[index].ptr = ptr;
            
            Handle<T> handle{};
            handle.index = index + 1; // 1-based index
            handle.generation = m_Slots[index].generation;
            return handle;
        }

        /**
         * @brief Unregister/Free a resource handle. Validates against double-frees.
         */
        void UnregisterResource(Handle<T> handle) {
            if (!handle.IsValid()) return;
            std::lock_guard<std::mutex> lock(m_Mutex);

            uint32_t index = handle.index - 1;
            OMNIX_ASSERT(index < m_Slots.size(), "HandleManager: Invalid index referenced!");
            OMNIX_ASSERT(m_Slots[index].generation == handle.generation, 
                         "HandleManager: Double-free / Use-after-free detected!");

            m_Slots[index].ptr = nullptr;
            m_Slots[index].generation++; // Invalidate existing handles
            m_FreeSlots.push_back(index);
        }

        /**
         * @brief Resolve a handle back to the raw resource. Validates generation to catch use-after-frees.
         */
        T* Resolve(Handle<T> handle) const {
            if (!handle.IsValid()) return nullptr;
            std::lock_guard<std::mutex> lock(m_Mutex);

            uint32_t index = handle.index - 1;
            if (index >= m_Slots.size()) return nullptr;

            OMNIX_ASSERT(m_Slots[index].generation == handle.generation,
                         "HandleManager: Lifetime assertion failed! Use-after-free detected on generation-tagged handle.");

            return m_Slots[index].ptr;
        }

    private:
        struct Slot {
            T* ptr = nullptr;
            uint32_t generation = 1;
        };

        mutable std::mutex m_Mutex;
        std::vector<Slot> m_Slots;
        std::vector<uint32_t> m_FreeSlots;
    };

} // namespace eng::memory
