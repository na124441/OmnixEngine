#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "Serializer/ECS/Snapshot/EntitySnapshot.h"

// Operation types
enum class EntityOperation : uint8_t { Create, Destroy, Modify };
enum class ComponentOperation : uint8_t { Add, Remove, Modify };
enum class FieldChangeType : uint8_t { Set, Add, Sub, BitMask };

// Deltas
struct FieldDelta {
    uint32_t fieldID;
    FieldChangeType changeType;
    std::string oldValue;
    std::string newValue;

    FieldDelta(uint32_t id, FieldChangeType type, const std::string& oldVal, const std::string& newVal)
        : fieldID(id), changeType(type), oldValue(oldVal), newValue(newVal) {}
};

struct ComponentDelta {
    uint32_t componentTypeID;
    ComponentOperation operation;
    std::vector<FieldDelta> fieldDeltas;

    ComponentDelta(uint32_t id, ComponentOperation op)
        : componentTypeID(id), operation(op) {}

    size_t GetFieldDeltaCount() const { return fieldDeltas.size(); }
};

struct EntityDelta {
    uint64_t entityID;
    EntityOperation operation;
    std::vector<ComponentDelta> componentDeltas;

    EntityDelta(uint64_t id, EntityOperation op)
        : entityID(id), operation(op) {}

    size_t GetComponentDeltaCount() const { return componentDeltas.size(); }
};

struct DeltaSnapshot {
    uint64_t baseSnapshotID;
    uint64_t targetSnapshotID;
    std::vector<EntityDelta> entityDeltas;

    DeltaSnapshot(uint64_t baseID, uint64_t targetID)
        : baseSnapshotID(baseID), targetSnapshotID(targetID) {}

    size_t GetEntityDeltaCount() const { return entityDeltas.size(); }
};

// Builder and Applier
class DeltaSnapshot_Builder {
public:
    static DeltaSnapshot BuildDelta(
        uint64_t baseSnapshotID,
        uint64_t targetSnapshotID,
        const std::vector<EntitySnapshot>& snapshotA,
        const std::vector<EntitySnapshot>& snapshotB);
};

class DeltaSnapshot_Applier {
public:
    static std::vector<EntitySnapshot> ApplyDelta(
        const std::vector<EntitySnapshot>& baseSnapshot,
        const DeltaSnapshot& delta);
};
