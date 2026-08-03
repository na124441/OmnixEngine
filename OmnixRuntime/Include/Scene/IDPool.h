//============================================================================
// IDPool.h - Entity ID Management System
//
// Singleton class that manages unique EntityID assignment
// Used by Scene to assign IDs to SceneObjects
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <cstdint>
#include <queue>

/**
 * @brief IDPool - Singleton Entity ID Pool
 *
 * Manages unique EntityID assignment for all entities in the engine.
 * Uses a simple incrementing counter with ID recycling support.
 *
 * Thread-safety: Not thread-safe by default.
 * Add mutex if using across multiple threads.
 */
class IDPool {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to IDPool singleton
     */
    static IDPool& Get() {
        static IDPool instance;
        return instance;
    }

    /**
     * @brief Request new unique EntityID
     * @return Unique EntityID (starts from 1, 0 is reserved for invalid)
     */
    uint32_t RequestID() {
        // Check if we have recycled IDs available
        if (!recycledIDs.empty()) {
            uint32_t id = recycledIDs.front();
            recycledIDs.pop();
            return id;
        }

        // Otherwise, return next incrementing ID
        return nextID++;
    }

    /**
     * @brief Return ID to pool for reuse
     * @param id EntityID to recycle
     *
     * Call this when an entity is destroyed to allow ID reuse.
     * This prevents ID exhaustion in long-running games.
     */
    void RecycleID(uint32_t id) {
        if (id == 0) return;  // Never recycle invalid ID (0)
        recycledIDs.push(id);
    }

    /**
     * @brief Reset ID pool (use with caution!)
     *
     * Resets the pool to initial state. Only use when completely
     * clearing all entities (e.g., editor reset, full game restart).
     */
    void Reset() {
        nextID = 1;
        while (!recycledIDs.empty()) {
            recycledIDs.pop();
        }
    }

    /**
     * @brief Get next ID that will be assigned (without assigning it)
     * @return Next ID value
     */
    uint32_t PeekNextID() const {
        if (!recycledIDs.empty()) {
            return recycledIDs.front();
        }
        return nextID;
    }

    /**
     * @brief Get total number of IDs issued (including recycled)
     * @return Total ID count
     */
    uint32_t GetTotalIDsIssued() const {
        return nextID - 1;
    }

private:
    // Private constructor for singleton
    IDPool() : nextID(1) {}

    // Prevent copying
    IDPool(const IDPool&) = delete;
    IDPool& operator=(const IDPool&) = delete;

    // Member variables
    uint32_t nextID;                    // Next ID to assign (starts at 1)
    std::queue<uint32_t> recycledIDs;   // Pool of recycled IDs for reuse
};

//============================================================================
// END OF FILE
//================================================================//
// Created by nayan on 11/25/2025.
//
