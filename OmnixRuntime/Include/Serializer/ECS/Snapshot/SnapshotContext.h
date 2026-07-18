#pragma once
//Tells ECS why it is building a snapshot
#include <cstdint>
//Context Includes : {a, SnapshotType ,(Save , Net , Delta) , b , Field Mask (Save_Only , Delta_Only)}
//ECS uses this to filter fields
//ECS never branches on serializer type
// ============================================================================
//1. Context Includes : {a, SnapshotType ,(Save , Net , Delta) , b , Filed Mask (Save_Only , Delta_Only)}
//2. ECS uses this to filtre fields
//3. ECS never branches on serializer type
// ============================================================================
// SNAPSHOT CONTEXT - Describes WHY and WHAT to serialize
// ============================================================================

// 1. SNAPSHOT TYPE - Purpose of snapshot
enum SnapshotType : uint8_t {
    SNAPSHOT_SAVE,      // Saving to disk (complete state)
    SNAPSHOT_NETWORK,   // Sending over network (optimized for bandwidth)
    SNAPSHOT_DELTA      // Incremental (only changed fields)
};

// 2. FIELD INTENT MASK - What fields should be included
enum FieldIntent : uint32_t {
    FIELD_ALWAYS = 1 << 0,  // Always serialized
    FIELD_SAVE_ONLY = 1 << 1,  // Disk save only
    FIELD_NETWORK_ONLY = 1 << 2,  // Network only
    FIELD_DELTA_ONLY = 1 << 3,  // Delta snapshots only

    // Combinations (shortcuts)
    FIELD_SAVE = FIELD_ALWAYS | FIELD_SAVE_ONLY,
    FIELD_NETWORK = FIELD_ALWAYS | FIELD_NETWORK_ONLY,
    FIELD_DELTA = FIELD_ALWAYS | FIELD_DELTA_ONLY
};

// 3. SNAPSHOT CONTEXT - Metadata for serialization
class SnapshotContext {
private:
    SnapshotType m_Type;
    uint32_t m_FieldMask;       // Bitmask of field intents
    bool m_IncludeMetadata;     // Include entity ID, generation, etc.
    float m_DeltaThreshold;     // For delta: only serialize if change > threshold

public:
    SnapshotContext(SnapshotType type = SNAPSHOT_SAVE)
        : m_Type(type),
        m_FieldMask(0),
        m_IncludeMetadata(true),
        m_DeltaThreshold(0.001f) {

        // Set default field mask based on type
        switch (type) {
        case SNAPSHOT_SAVE:
            m_FieldMask = FIELD_SAVE;
            break;
        case SNAPSHOT_NETWORK:
            m_FieldMask = FIELD_NETWORK;
            break;
        case SNAPSHOT_DELTA:
            m_FieldMask = FIELD_DELTA;
            break;
        }
    }

    // 4. GETTERS
    SnapshotType GetType() const { return m_Type; }
    uint32_t GetFieldMask() const { return m_FieldMask; }
    bool ShouldIncludeMetadata() const { return m_IncludeMetadata; }
    float GetDeltaThreshold() const { return m_DeltaThreshold; }

    // 5. SETTERS
    void SetType(SnapshotType type) { m_Type = type; }
    void SetFieldMask(uint32_t mask) { m_FieldMask = mask; }
    void SetIncludeMetadata(bool include) { m_IncludeMetadata = include; }
    void SetDeltaThreshold(float threshold) { m_DeltaThreshold = threshold; }

    // 6. FIELD FILTERING - Core logic
    // Query: Should this field be serialized?
    // Input: Field intent flags
    // Output: true if field matches context
    bool ShouldSerializeField(uint32_t fieldIntent) const {

        // Check if field intent matches our mask
        // For FIELD_ALWAYS: always serialize
        if (fieldIntent & FIELD_ALWAYS) {
            return true;
        }

        // For type-specific flags: check current snapshot type
        switch (m_Type) {
        case SNAPSHOT_SAVE:
            // Include if marked SAVE_ONLY
            return (fieldIntent & FIELD_SAVE_ONLY) != 0;

        case SNAPSHOT_NETWORK:
            // Include if marked NETWORK_ONLY
            return (fieldIntent & FIELD_NETWORK_ONLY) != 0;

        case SNAPSHOT_DELTA:
            // Include if marked DELTA_ONLY
            return (fieldIntent & FIELD_DELTA_ONLY) != 0;

        default:
            return false;
        }
    }

    // 7. CONTEXT TYPE QUERIES
    bool IsSaveSnapshot() const { return m_Type == SNAPSHOT_SAVE; }
    bool IsNetworkSnapshot() const { return m_Type == SNAPSHOT_NETWORK; }
    bool IsDeltaSnapshot() const { return m_Type == SNAPSHOT_DELTA; }

    // 8. DEBUG
    const char* GetTypeName() const {
        switch (m_Type) {
        case SNAPSHOT_SAVE: return "SAVE";
        case SNAPSHOT_NETWORK: return "NETWORK";
        case SNAPSHOT_DELTA: return "DELTA";
        default: return "UNKNOWN";
        }
    }
};