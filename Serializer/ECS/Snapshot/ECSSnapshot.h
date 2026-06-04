#pragma once

#include <vector>
#include <cstdint>
#include "EntitySnapshot.h"

// Forward declarations
class ECS;
class ComponentSchemaRegistry;
class SnapshotContext;

// Instead of generic Serializer/Deserializer, we need to declare the concrete classes used
class BinaryWriter;
class BinaryReader;

class ECSSnapshot {
private:
    uint64_t m_SnapshotID;
    std::vector<EntitySnapshot> m_EntitySnapshots;

public:
    ECSSnapshot();
    explicit ECSSnapshot(uint64_t snapshotID);

    void Capture(const ECS& ecs, const ComponentSchemaRegistry& registry, const SnapshotContext& context);

    uint64_t GetSnapshotID() const;
    const std::vector<EntitySnapshot>& GetEntitySnapshots() const;
    std::vector<EntitySnapshot>& GetEntitySnapshots(); // Add non-const overload

    void Serialize(BinaryWriter& writer) const;
    void Deserialize(BinaryReader& reader, const ComponentSchemaRegistry& registry);
};